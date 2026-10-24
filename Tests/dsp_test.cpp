// Offline sanity test for the Green Russian Sim DSP core (no GUI needed).
#include "BigMuffDSP.h"
#include <cstdio>

static double goertzel (const std::vector<float>& x, double f, double fs)
{
    const double w = 2.0 * M_PI * f / fs;
    double s1 = 0, s2 = 0, c = 2.0 * std::cos (w);
    for (float v : x) { double s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
    const double p = s1 * s1 + s2 * s2 - c * s1 * s2;
    return 2.0 * std::sqrt (std::max (0.0, p)) / (double) x.size();
}

struct Result { double rms, peak, dc, fund; bool finite; };

static Result run (bigmuff::Params p, double amp, double f0, double fs = 48000.0)
{
    bigmuff::Engine eng;
    eng.prepare (fs);
    const int N = (int) fs, block = 512;
    juce::AudioBuffer<float> buf (2, block);
    std::vector<float> out;
    for (int pos = 0; pos < N; pos += block)
    {
        for (int i = 0; i < block; ++i)
        {
            float v = (float) (amp * std::sin (2.0 * M_PI * f0 * (pos + i) / fs));
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }
        eng.process (buf, p);
        for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
    }
    std::vector<float> tail (out.begin() + N / 2, out.end());
    Result r { 0, 0, 0, 0, true };
    for (float v : tail)
    {
        if (! std::isfinite (v)) r.finite = false;
        r.rms += v * v; r.peak = std::max (r.peak, (double) std::fabs (v)); r.dc += v;
    }
    r.rms = std::sqrt (r.rms / tail.size()); r.dc /= tail.size();
    r.fund = goertzel (tail, f0, fs);
    return r;
}

static void show (const char* name, const Result& r)
{
    std::printf ("%-36s rms=%.3f peak=%.3f dc=%+.4f fund=%.3f %s\n", name, r.rms, r.peak, r.dc, r.fund,
                 r.finite ? "ok" : "NaN/Inf!");
}

int main()
{
    int failures = 0;
    auto check = [&failures] (bool c, const char* m) { if (! c) { std::printf ("FAIL: %s\n", m); ++failures; } };

    bigmuff::Params off; off.on = false;
    auto r0 = run (off, 0.1, 330.0);
    show ("bypass (0.1)", r0);
    check (std::fabs (r0.fund - 0.1) < 0.01, "bypass is transparent");

    bigmuff::Params p;
    for (double a : { 0.02, 0.1, 0.5, 1.0 })
    {
        char n[64]; std::snprintf (n, sizeof n, "default, 330 Hz, in=%.2f", a);
        auto r = run (p, a, 330.0); show (n, r);
        check (r.finite && r.peak < 4.0 && std::fabs (r.dc) < 0.02, "default is stable and DC free");
    }

    // sustain: more sustain = more compression/sustain at low input
    bigmuff::Params lo = p; lo.sustain = 0.1f;
    bigmuff::Params hi = p; hi.sustain = 1.0f;
    auto rl = run (lo, 0.02, 330.0), rh = run (hi, 0.02, 330.0);
    show ("sustain 0.1, quiet in", rl); show ("sustain 1.0, quiet in", rh);
    check (rh.rms > rl.rms * 1.5, "sustain raises the level of quiet notes");

    // compression: output barely changes between a soft and a hard pick at high sustain
    auto rs = run (hi, 0.05, 330.0), rhard = run (hi, 0.5, 330.0);
    show ("sustain 1.0, in=0.05", rs); show ("sustain 1.0, in=0.5", rhard);
    check (rhard.rms < rs.rms * 3.0, "output is heavily compressed (10x input -> <3x output)");

    // tone stack: mid scoop at 0.5 relative to the extremes (use a high-sustain, steady level)
    bigmuff::Params tn = hi;
    double lvl[3];
    int k = 0;
    for (float t : { 0.0f, 0.5f, 1.0f })
    {
        tn.tone = t;
        // low-level tone probes measured on a quiet, mostly linear signal
        auto r = run (tn, 0.2, 880.0);
        char n[64]; std::snprintf (n, sizeof n, "tone=%.1f, 880 Hz", t);
        show (n, r);
        lvl[k++] = r.rms;
    }
    check (lvl[1] < lvl[0] && lvl[1] < lvl[2], "tone at noon scoops the mids (880 Hz lowest)");

    // bass vs treble tilt
    tn.tone = 0.0f; auto bLow = run (tn, 0.2, 100.0), bHigh = run (tn, 0.2, 5000.0);
    show ("tone=0, 100 Hz", bLow); show ("tone=0, 5 kHz", bHigh);
    check (bLow.rms > bHigh.rms, "tone fully down favours bass");
    tn.tone = 1.0f; auto tLow = run (tn, 0.2, 100.0), tHigh = run (tn, 0.2, 5000.0);
    show ("tone=1, 100 Hz", tLow); show ("tone=1, 5 kHz", tHigh);
    check (tHigh.rms > tLow.rms, "tone fully up favours treble");

    for (int d = 0; d < 3; ++d)
    {
        bigmuff::Params q = hi; q.diode = d; q.volume = 1.0f;
        char n[64]; std::snprintf (n, sizeof n, "max everything, diode=%d", d);
        auto r = run (q, 1.0, 110.0); show (n, r);
        check (r.finite && r.peak < 8.0 && std::fabs (r.dc) < 0.1, "max settings stable");
    }

    for (double fs : { 44100.0, 96000.0 })
    {
        auto r = run (p, 0.3, 330.0, fs);
        char n[64]; std::snprintf (n, sizeof n, "default @ %.0f Hz", fs);
        show (n, r);
        check (r.finite, "finite at other sample rates");
    }

    // --- the clipping stage must match a high-precision (bisection) solution of the same equation
    for (int dtype = 0; dtype < 3; ++dtype)
        for (double amp : { 0.0034, 0.05, 0.3, 1.5, 4.0 })
        {
            const auto dm = bigmuff::diodeFor (dtype);
            const double fsOS = 176400.0;
            bigmuff::ClipStage stage; stage.prepare ((float) fsOS, 70.0f, 15.0f);
            bigmuff::OnePole hp; hp.setCutoff (70.0f, (float) fsOS);
            double vPrev = 0.0, maxErr = 0.0;
            for (int i = 0; i < 44100; ++i)
            {
                const float x = (float) (amp * std::sin (2.0 * M_PI * 330.0 * i / fsOS));
                const double I = (double) hp.hp (x) * 15.0 / 1.0e5, Cdt = 440.0e-12 * fsOS;
                double lo = -3.0, hi = 3.0;
                for (int k = 0; k < 200; ++k)
                {
                    const double V = 0.5 * (lo + hi);
                    const double g = (V - vPrev) * Cdt + V / 1.0e5
                                     + dm.Is * (std::exp (std::min (V / dm.nVt, 40.0)) - std::exp (std::min (-V / dm.nVt, 40.0))) - I;
                    if (g > 0.0) hi = V; else lo = V;
                }
                vPrev = 0.5 * (lo + hi);
                maxErr = std::max (maxErr, std::fabs ((double) stage.process (x, dm) - vPrev));
            }
            if (maxErr > 1.0e-3)
            {
                std::printf ("clip stage diode=%d amp=%.4f: max error %.6f V\n", dtype, amp, maxErr);
                check (false, "clipping stage matches the reference solution within 1 mV");
            }
        }

    // --- periodicity: 375 Hz at 48 kHz repeats every 128 samples; noise or limit cycles would break that
    for (int d = 0; d < 3; ++d)
        for (float sus : { 0.1f, 1.0f })
            for (double a : { 0.02, 0.5 })
            {
                bigmuff::Params q; q.diode = d; q.sustain = sus;
                bigmuff::Engine eng; eng.prepare (48000.0);
                const int N = 96000, block = 512;
                juce::AudioBuffer<float> buf (2, block);
                std::vector<float> y;
                for (int pos = 0; pos < N; pos += block)
                {
                    for (int i = 0; i < block; ++i)
                    {
                        const float v = (float) (a * std::sin (2.0 * M_PI * 375.0 * (pos + i) / 48000.0));
                        buf.setSample (0, i, v); buf.setSample (1, i, v);
                    }
                    eng.process (buf, q);
                    for (int i = 0; i < block; ++i) y.push_back (buf.getSample (0, i));
                }
                double e = 0, s = 0;
                for (int i = 64000; i < N; ++i) { const double dd = y[(size_t) i] - y[(size_t) i - 128]; e += dd * dd; s += (double) y[(size_t) i] * y[(size_t) i]; }
                const double res = s > 1e-12 ? std::sqrt (e / s) : 0.0;
                if (res > 1.0e-3)
                {
                    std::printf ("periodicity diode=%d sustain=%.1f amp=%.2f: residual %.2e\n", d, sus, a, res);
                    check (false, "output is periodic for a periodic input (no noise or limit cycles)");
                }
            }

    std::printf (failures == 0 ? "\nALL CHECKS PASSED\n" : "\n%d CHECK(S) FAILED\n", failures);
    return failures;
}
