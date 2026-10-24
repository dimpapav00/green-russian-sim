#pragma once
// Green Russian Sim - DSP core (Big Muff Pi V7 "Sovtek Green Russian" style).
//
// Signal model, following the classic Big Muff topology:
//   input boost (Q1) -> Sustain (level into clipping) -> clipping stage 1 -> clipping stage 2
//   -> passive tone stack (low-pass / high-pass blend, mid scoop) -> recovery + Volume
//
// Each clipping stage is a gain stage with Rf || Cf || anti-parallel diodes in the feedback path.
// That nonlinear RC network is solved per sample (backward Euler + Newton), so the diodes and the
// feedback capacitor interact the way they do in the circuit.
// Values come from public descriptions of V7 era Muffs; this is a behavioural model, not a
// schematic-exact copy.

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace bigmuff
{
constexpr float kTwoPi = 6.283185307179586f;

struct Params
{
    float volume    = 0.5f;  // 0..1  (0.5 ~ unity-ish)
    float tone      = 0.5f;  // 0..1  0 = bass (low-pass), 1 = treble (high-pass), 0.5 = mid scoop
    float sustain   = 0.6f;  // 0..1  gain into the clipping stages
    float outputDb  = 0.0f;  // host-side trim
    bool  on        = true;
    int   diode     = 0;     // 0 silicon (KD521 / 1N4148 like), 1 germanium, 2 LED
};

struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setCutoff (float fc, float fs) { a = 1.0f - std::exp (-kTwoPi * fc / fs); }
    float lp (float x) { z += a * (x - z); return z; }
    float hp (float x) { return x - lp (x); }
    void reset() { z = 0.0f; }
};

struct DiodeModel
{
    double Is, nVt;
    float norm; // level match against silicon (about 0.6 V)
};

inline DiodeModel diodeFor (int d)
{
    switch (d)
    {
        case 1:  return { 1.0e-6,  0.0336, 0.6f / 0.25f }; // germanium-like, ~0.25 V
        case 2:  return { 2.0e-18, 0.0517, 0.6f / 1.8f };  // LED-like, ~1.8 V
        default: return { 2.5e-9,  0.0452, 1.0f };         // silicon 1N4148-like, ~0.6 V
    }
}

struct Coeffs
{
    float sustainAtt = 1, volGain = 1, outGain = 1, onMix = 1, tone = 0.5f;
    DiodeModel diode { 2.5e-9, 0.0452, 1.0f };
};

// One Big Muff clipping stage. Feedback network: Rf (100k) || Cf (~440 pF) || anti-parallel diodes.
class ClipStage
{
public:
    void prepare (float fsOS, float hpHz, float gain)
    {
        hp.setCutoff (hpHz, fsOS);
        K = (double) gain;
        dt = 1.0 / (double) fsOS;
        reset();
    }
    void reset() { hp.reset(); vPrev = 0.0; }

    float process (float x, const DiodeModel& d)
    {
        const double I = (double) hp.hp (x) * K / Rf;      // input current into the virtual summing node
        const double Cdt = Cf / dt;

        // Implicit equation: g(V) = (V - vPrev)/dt*Cf + V/Rf + 2*Is*sinh(V/nVt) - I = 0.
        // g is monotone and odd apart from the history term, so the root has the sign of
        // Jp = I + vPrev*Cf/dt and only its magnitude W is solved for. On W >= 0 the function is convex, so
        // Newton started from an upper bound of the root converges monotonically. Two upper bounds are known:
        // one that ignores the linear terms, and one that ignores the diodes; the smaller one is used.
        const double Jp = I + vPrev * Cdt;
        const double aJ = std::fabs (Jp);
        const double lin = Cdt + 1.0 / Rf;
        const double nVt = d.nVt;

        double W = std::min (nVt * std::log1p (aJ / d.Is), aJ / lin);
        for (int i = 0; i < 20; ++i)
        {
            const double e1 = std::exp (std::min (W / nVt, 40.0));
            const double e2 = std::exp (-W / nVt);
            const double g = W * lin + d.Is * (e1 - e2) - aJ;
            const double gp = lin + d.Is * (e1 + e2) / nVt;
            const double dW = g / gp;
            W = std::max (0.0, W - dW);
            if (std::fabs (dW) < 1.0e-10) break;
        }
        vPrev = Jp >= 0.0 ? W : -W;
        return (float) vPrev;
    }

private:
    static constexpr double Rf = 100000.0;
    static constexpr double Cf = 440.0e-12; // 390 pF + 53 pF in the V7 Sovtek
    OnePole hp;
    double K = 10.0, dt = 1.0 / 176400.0, vPrev = 0.0;
};

class Channel
{
public:
    void prepare (float fsOS)
    {
        dcIn.setCutoff (10.0f, fsOS);
        stage1.prepare (fsOS, 70.0f, 15.0f);
        stage2.prepare (fsOS, 120.0f, 8.0f);
        toneLP.setCutoff (408.0f, fsOS);   // 39k + 10 nF
        toneHP.setCutoff (1855.0f, fsOS);  // 3.9 nF + 22k
        outHP.setCutoff (15.0f, fsOS);
        reset();
    }

    void reset()
    {
        for (auto* f : { &dcIn, &toneLP, &toneHP, &outHP })
            f->reset();
        stage1.reset();
        stage2.reset();
    }

    float process (float x, const Coeffs& c)
    {
        const float in = dcIn.hp (x);

        float v = 4.0f * in;                                   // Q1 input booster
        v = 4.0f * std::tanh (v * 0.25f);                      // supply-limited
        v *= c.sustainAtt;                                     // Sustain pot
        v = stage1.process (v, c.diode);
        v = stage2.process (v, c.diode) * c.diode.norm;

        // passive tone stack: low-pass and high-pass legs blended by the Tone pot
        const float lo = toneLP.lp (v);
        const float hi = toneHP.hp (v);
        float y = (1.0f - c.tone) * lo + c.tone * hi;

        y *= 3.0f;                                             // recovery stage (Q4)
        y = outHP.hp (y) * c.volGain * c.outGain;

        const float out = in + c.onMix * (y - in);             // footswitch crossfade
        if (! std::isfinite (out))
        {
            reset();
            return 0.0f;
        }
        return std::clamp (out, -8.0f, 8.0f);
    }

private:
    OnePole dcIn, toneLP, toneHP, outHP;
    ClipStage stage1, stage2;
};

class Engine
{
public:
    static constexpr int kChunk = 32;

    void prepare (double sampleRate)
    {
        fsOS = (float) sampleRate * 4.0f;
        os.reset();
        os.initProcessing ((size_t) kChunk);
        for (auto& c : channels)
            c.prepare (fsOS);
        for (auto* s : { &smVol, &smTone, &smSus, &smOut, &smOn })
            s->reset (sampleRate, 0.03);
        snap = true;
    }

    void reset()
    {
        os.reset();
        for (auto& c : channels) c.reset();
        snap = true;
    }

    int getLatencySamples() const { return (int) std::lround (os.getLatencyInSamples()); }

    void process (juce::AudioBuffer<float>& buffer, const Params& p)
    {
        const int numCh = std::min (buffer.getNumChannels(), 2);
        const int n = buffer.getNumSamples();
        if (numCh == 0 || n == 0) return;

        auto set = [this] (juce::SmoothedValue<float>& s, float v)
        {
            if (snap) s.setCurrentAndTargetValue (v); else s.setTargetValue (v);
        };
        set (smVol, p.volume); set (smTone, p.tone); set (smSus, p.sustain);
        set (smOut, p.outputDb); set (smOn, p.on ? 1.0f : 0.0f);
        snap = false;

        const auto diode = diodeFor (p.diode);

        for (int pos = 0; pos < n; pos += kChunk)
        {
            const int len = std::min (kChunk, n - pos);

            Coeffs c;
            const float vol = smVol.skip (len), sus = smSus.skip (len);
            c.tone = smTone.skip (len);
            c.outGain = std::pow (10.0f, smOut.skip (len) / 20.0f);
            c.onMix = smOn.skip (len);
            c.sustainAtt = 0.03f * std::pow (10.0f, 1.52f * sus); // log pot: -30 dB .. 0 dB
            const float v2 = vol * 2.0f;
            c.volGain = v2 * v2;
            c.diode = diode;

            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) pos, (size_t) len);
            auto up = os.processSamplesUp (block);
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = up.getChannelPointer ((size_t) ch);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                    d[i] = channels[(size_t) ch].process (d[i], c);
            }
            os.processSamplesDown (block);
        }
    }

private:
    float fsOS = 176400.0f;
    bool snap = true;
    std::array<Channel, 2> channels;
    juce::dsp::Oversampling<float> os { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    juce::SmoothedValue<float> smVol, smTone, smSus, smOut, smOn;
};
} // namespace bigmuff
