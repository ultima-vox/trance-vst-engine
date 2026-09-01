# VST Engine

Generative VST3 instrument for **dark psytrance, psytrance, forest and related electronic music**.

## Status

Early vertical slice (0.1.x). The current target is a reliable VST3 instrument in Cubase before expanding into a larger generative sound-design system.

### Implemented

- JUCE 9.0.1 + CMake project
- VST3 instrument target for Windows
- Host-tempo-aware 16-step rolling bass sequencer
- Deterministic dark-psy pattern generator
- Dedicated psy-bass synth voice with phase reset and fast pitch envelope
- Automatable Drive and Release parameters
- Plug-in state save/restore
- Minimal native JUCE editor
- Pattern generator test
- GitHub Actions Windows build

## Build on Windows

Requirements:

- Visual Studio 2022 with Desktop development with C++
- CMake 3.22+
- Git

```powershell
git clone https://github.com/ultima-vox/vst-engine.git
cd vst-engine
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The VST3 bundle is produced under:

```text
build/VstEngine_artefacts/Release/VST3/
```

For Cubase, copy the resulting .vst3 bundle to the standard system VST3 location, typically:

```text
C:\\Program Files\\Common Files\\VST3
```

## Architecture direction

```text
src/
  dsp/          synthesis and audio DSP
  generator/    musical pattern generation
  Plugin*       VST/JUCE integration and UI
```

Planned engines:

1. Psy bass
2. Kick synthesis
3. Acid / resonant sequence engine
4. FM/wavetable dark leads
5. Forest/alien texture generator
6. Generative sequencer with probability, ratchets, mutation and scale lock
7. FX chain and modulation matrix
8. MIDI-out mode for external instruments

## Design rule

Generation must remain **musically constrained**. Randomization is not allowed to become unconstrained noise: groove, scale, phrase structure and genre-specific rhythm rules are explicit parts of the generator.
