# Green Russian Sim (VST3)

Unofficial emulation of a Sovtek-era "Green Russian" Big Muff Pi (V7 style) fuzz. It is a behavioural model
built from public descriptions of the Big Muff topology and V7 component values, **not** a schematic-exact copy,
and is not affiliated with any manufacturer.

## Signal flow
```
in -> input booster -> Sustain -> clipping stage 1 -> clipping stage 2 -> tone stack -> recovery + Volume -> out
```
Each clipping stage models Rf (100k) || Cf (~440 pF) || anti-parallel diodes in the feedback path, solved per sample,
so the diodes and the capacitor interact like in the real circuit. The tone stack is the classic low-pass / high-pass
blend (about 408 Hz and 1.86 kHz corners) with the mid scoop at noon.

## Controls
| Control | What it does |
|---|---|
| Volume | Output level (centre is about unity) |
| Tone | Left = bass (low-pass), right = treble (high-pass), centre = scooped mids |
| Sustain | Level driven into the clipping stages (about -30 dB to 0 dB, log taper) |
| Diodes | Silicon (stock), Germanium (softer), LED (harder). Level-matched |
| ON / OFF | Footswitch with click-free crossfade |
| Output | Host-side trim, -24 to +12 dB |

Runs at 4x oversampling (the plugin reports the oversampler latency to the host).

## Build
CMake 3.22+ and a C++17 compiler. JUCE 8.0.15 is downloaded automatically.
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Result: `build/GreenRussianSim_artefacts/Release/VST3/Green Russian Sim.vst3`

Install: Windows `C:\Program Files\Common Files\VST3\`, macOS `~/Library/Audio/Plug-Ins/VST3/`, Linux `~/.vst3/`.

No compiler? Push this folder to GitHub: `.github/workflows/build.yml` builds Windows, macOS and Linux VST3s as downloadable artifacts.

Offline test: `cmake -S . -B build -DGRS_BUILD_TESTS=ON && cmake --build build --target dsp_test && ./build/dsp_test`

JUCE is used under AGPLv3/commercial licensing: if you distribute the plugin, AGPL terms apply.
