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
- Synthesized kick engine (issue #11 Phase 5): pitch sweep, body/tail,
  click with tone control, drive, clip, transient shaping, sub layer, tune,
  phase reset and output level - no samples, fully generated
- Kick factory presets: Psytrance, Dark Psy, Progressive Psy, Hi-Tech,
  Classic Trance
- Synthetic kick/bass matching (issue #11 Phase 6): bounded analysis of
  kick tail/dominant region vs bass onset/dominant region, spectral overlap
  and phase interaction, with APPLY respecting hard bounds (tail x0.60..x1.00,
  phase snap 0|180 deg, bass level +/-6 dB, bass timing 0..16 ms)
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

Modular architecture per issue #11 (independent CMake targets):

```text
apps/vst3/      PluginProcessor (thin VST3/APVTS host adapter), PluginEditor (composition only)
libs/core/      shared stable IDs/constants          -> vst_core
libs/sequence/  canonical Sequence/Step/timing model -> vst_sequence
libs/transport/ pure PPQ/grid musical-time math      -> vst_transport
libs/midi/      source-mode policy, MIDI export, generated-note scheduler -> vst_midi
libs/bass/      psy-bass DSP voice                   -> vst_bass
libs/preset/    preset format/filesystem/migration   -> vst_preset
libs/generator/ seed-based pattern generation        -> vst_generator
libs/ui/        reusable JUCE components             -> vst_ui
tests/          one test executable per module + tests/integration/
```

Dependency rule: lower-level modules -> shell. The shell (PluginProcessor/PluginEditor)
links the module libraries; lower modules never depend on the shell.

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


## MIDI / Cubase workflow

VST Engine supports four MIDI source modes:

- **AUTO** — if the host sends note events, the piano-roll MIDI is used; otherwise the internal generator is used.
- **PIANO ROLL** — only incoming host MIDI is used.
- **GENERATOR** — only VST Engine's generated MIDI is used.
- **BOTH** — incoming host MIDI and generated MIDI are combined.

The generated stream can be assigned to MIDI channels 1-16. This is the routing foundation for future multitimbral Parts.

### Recording generated MIDI into Cubase

A standard VST3 plug-in does not directly modify an existing Cubase piano-roll part. Generated notes are exposed as VST3 MIDI output so the host can route/record them to a MIDI track. Once recorded, switch VST Engine to **PIANO ROLL** or **AUTO** and edit the notes normally in Cubase.

Planned next step: drag-and-drop MIDI clip export from the plug-in UI into the Cubase project.


### Drag generated MIDI into Cubase

The editor exposes **DRAG MIDI TO CUBASE**. Dragging it creates a Standard MIDI File from the current 16-step generator pattern and starts an external file drag operation.

The exported clip uses:
- the current generator pattern
- the selected root note
- the selected generator MIDI channel
- 960 PPQ resolution
- 16th-note step spacing

Cubase can then place/import the MIDI clip into the project for normal piano-roll editing.

## Synthesized kick engine (issue #11 Phase 5)

The kick is a fully synthesized one-shot voice (`libs/kick`, no samples):

- **Pitch Start / Pitch End / Pitch Decay / Pitch Curve** - exponential pitch
  sweep from the attack pitch down to the tuned fundamental.
- **Body Decay / Tail** - body envelope decay plus a slower sub-layer tail.
- **Click / Click Tone** - deterministic attack transient; Tone morphs it from
  bright noise to a tonal thump.
- **Drive / Clip** - gain-compensated saturation then an optional hard-clip
  ceiling.
- **Transient** - attack softness (0 = instant, 1 = soft ramp).
- **Sub** - sub-octave layer amount.
- **Tune / Phase / Output Level** - fundamental as a MIDI note, deterministic
  oscillator start phase, output level.

Routing: the kick listens on its own MIDI channel (**Kick MIDI Channel**,
default 2 per the fixed Part 2 / CH 2 mapping). Note events on that channel
play the kick and never reach the bass engine; the piano roll transposes
relative to C4 while Tune stays the anchor. CC 120/123 fast-fade the tail so
no note can hang. As a one-shot, the kick ignores ordinary note-off; only
CC 120/123 received on the configured Kick channel, UI Panic, transport stop,
or plugin reset use the safety release path. Factory kick presets: Psytrance, Dark Psy, Progressive Psy,
Hi-Tech, Classic Trance.

## Kick/bass matching (issue #11 Phase 6)

The **MATCH** panel analyzes the real synthesized kick and a real synthesized
bass note (same DSP the plugin uses) and measures kick tail duration,
kick/low-end dominant region, bass onset, spectral overlap, phase interaction
and peak relationship. **APPLY** writes bounded adjustments through APVTS:

- kick tail is trimmed by at most 40% (`x0.60..x1.00`)
- kick phase snaps to 0 or 180 degrees
- bass output level is corrected within +/-6 dB
- Bass Part 1 / MIDI CH 1 note events are delayed by at most 16 ms using a
  fixed-capacity, sample-accurate cross-block queue; other channels are unchanged

No random parameter movement, no preset switching and no fake "matched"
state: the exit gate is automated - the unit test material must show a
measurable reduction in low-end spectral overlap without muting either
source.

## Editor and presets

Editor uses compact BASS, KICK, SEQ, MATCH, PRESETS and SETTINGS tabs at
1180x760, resizable from 1040x680 to 1600x1000. PRESETS reads one typed catalog
from `PresetManager`: factory Bass/Kick presets are read-only; user Bass/Kick
Sound presets and Full presets support save, load, rename, delete and refresh.
Files live under JUCE user application data in `UltimaVox/VST-Engine/Presets/User`.
Preset schema remains v1 and legacy v1 Sound presets remain loadable.

## Host synchronization

Generated playback is synchronized to the host's musical position:

- **PPQ sync (primary).** When the host provides an `AudioPlayHead::PositionInfo`
  PPQ position (Cubase does), step onsets are scheduled from the absolute
  quarter-note position against the canonical `stepsPerQuarterNote()` grid.
  This gives bar-accurate alignment at any start locator, immediate realign on
  seek/jump, correct wrap on cycle/loop, and musical alignment that survives
  BPM changes. The alignment math lives in `libs/transport/PpqSync.h` and is
  unit-tested (`transport_sync_tests`).
- **Fallback.** If a host does not provide PPQ, playback falls back to the
  historical BPM-derived free-running step timer (start at step 0, no locator
  alignment). The timing basis (canonical step durations) is covered by the
  sync tests; the fallback itself requires a Cubase manual check.

Probability still advances exactly once per sequence step and reseeds on every
transport restart, so a given project state restarted from the same point
reproduces the same decisions as MIDI export.

## MIDI source isolation

The generator decision (`libs/midi/SourceSelector.h`, unit-tested by
`midi_source_tests`) only ever observes **external host notes** and **GUI
keyboard notes**:

- **AUTO** — host/GUI input wins; the generator runs only when neither is present.
- **PIANO ROLL** — host/GUI input only.
- **GENERATOR** — only generated material (incoming host MIDI is dropped).
- **BOTH** — host/GUI input and generated material are combined.

Internally generated MIDI is never counted as user input. The virtual keyboard
state deliberately sees only what the user plays on it, so "generated notes
would highlight the keyboard" can never suppress generator playback in AUTO.
Consequence: the on-screen keyboard currently highlights only GUI-played keys,
not Cubase incoming or generated notes; Cubase piano-roll input still plays
normally.

Host/Cubase-dependent behavior (actual PPQ alignment, locator start, seek,
loop wrap, AUTO source switching in a real session) must be verified in Cubase
per the acceptance checklist.
