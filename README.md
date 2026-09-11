# Vox Electronic Engine

Modular generative VST3 workstation for **electronic music production in Cubase**.

Vox Electronic Engine is not trance-only. The core is genre-neutral; musical identity is provided by instrument modules, style/generator profiles and content packs. Trance, psytrance, house, techno, drum & bass, ambient and future electronic styles should be supported without changing the host architecture.

> Product-level source of truth: [issue #10](../../issues/10).  
> Current implementation contract: [issue #11](../../issues/11).

## Product family

The Vox product line is split by domain:

- **Vox Electronic Engine** — synths, musical instrument modules, generative MIDI/patterns, semantic FX, atmospheres and rack-based electronic-music workflow.
- **Vox Drum Engine** — future separate drum/percussion workstation using the same architectural philosophy, with drum modules, kits, grooves and role-aware drum generators.
- **Vox Mastering Engine** — mixing/mastering/analysis workflows, including Kick/Bass low-end analysis.

Kick/drum synthesis and Kick/Bass MATCH are therefore **not part of Vox Electronic Engine current product scope**.

## Product direction

One Windows x64 VST3 instance hosts a **Kontakt-like internal instrument rack** with explicit and simple multitrack MIDI routing.

The product is not built around fixed `Part N = Engine X` assignments. Each slot hosts a generic Instrument instance and can be independently assigned to MIDI CH1–CH16.

Initial instrument families:

- Bass
- Acid
- Lead
- Semantic FX
- Atmos / Texture

Future instruments and style/content packs must be addable without rewriting the VST3 shell, Rack, routing, state or preset infrastructure.

## Delivery policy

The development target is a **working integrated pre-production plug-in**, not a collection of disconnected PRs.

PRs/checkpoints exist for CI, review and rollback. The mandatory handoff after the current architecture refactor is one installable `Vox Electronic Engine.vst3` that can be tested in Cubase.

The first pre-production handoff after `7A -> 7B.0 -> 7B -> 7C` must include at minimum:

- generic Rack / RackSlot model;
- InstrumentProvider / Registry / Factory / Instance architecture;
- Bass migrated through the generic Instrument contract;
- at least one second real or reference instrument for isolation testing;
- explicit MIDI CH1–CH16 routing;
- deliberate Layer mode;
- key/velocity zones and transpose;
- state/preset round-trip and migration;
- stable Cubase automation/macros;
- bounded realtime/resource behavior;
- missing/incompatible-module handling;
- installer/package suitable for Cubase validation;
- green Windows CI.

Manual Cubase acceptance remains mandatory where host behavior cannot be proven automatically.

## Architecture

The VST3 shell must not contain Bass/Acid/Lead-specific DSP or routing branches.

Conceptual dependency direction:

```text
core/shared
    -> instrument contracts
        -> providers / registry / factory
            -> instrument modules
                -> rack + MIDI/audio routing + state
                    -> VST3 shell
```

Conceptual product model:

```text
Vox Electronic Engine.vst3
│
├─ Core / Shared
│  ├─ Transport / PPQ sync
│  ├─ Sequence / Pattern primitives
│  ├─ MIDI export
│  ├─ Preset / State / Migration
│  ├─ Shared Arp / Gate / Modulation primitives
│  ├─ Resource / diagnostics infrastructure
│  └─ GenerationContext
│
├─ Rack
│  ├─ RackSlot (stable SlotId)
│  ├─ MIDI Router
│  ├─ Layer / key / velocity routing
│  ├─ Mixer / Output routing
│  └─ Full Rack / Scene state
│
├─ InstrumentRegistry
│  └─ InstrumentProvider
│      └─ BuiltInInstrumentProvider (1.0)
│          ├─ Bass
│          ├─ Acid
│          ├─ Lead
│          ├─ Semantic FX
│          └─ Atmos
│
└─ future
   └─ ExternalInstrumentProvider
```

For 1.0, built-in instruments may remain **statically linked CMake modules inside the VST3**. They are architecturally separated so future external modules require a provider/loader subsystem rather than a rewrite of the host.

## External-module-ready contract

The 1.0 architecture is intentionally prepared for future external instrument packs.

Core rules:

- `InstrumentId` is globally durable and independent from provider, slot index or MIDI channel;
- `SlotId` is stable and independent from display order and channel;
- provider is a resolution mechanism, not instrument identity;
- missing/incompatible modules remain explicit unresolved slots;
- there is no silent fallback to another synth;
- state/presets use stable IDs plus explicit version/schema metadata;
- the host UI and state logic work through generic descriptors/capabilities;
- a future DLL ABI must avoid JUCE/STL ownership-sensitive objects across the binary boundary;
- future loading should use opaque handles, explicit create/destroy, manifests and versioned compatibility metadata.

External binary loading itself is not required for 1.0.

## Instrument contract

Each instrument module must expose or provide the equivalent of:

- stable InstrumentId and display name;
- instrument / API / state-schema versions;
- DSP lifecycle: prepare / reset / process / reprepare;
- parameter descriptors and state contract;
- Sound preset contract;
- Pattern / Sequence / Arp preset contract where applicable;
- instrument-owned role/style-aware PatternGenerator;
- supported sequence/capability descriptors;
- modulation destinations;
- editor/view-model capabilities;
- latency and tail behavior;
- realtime resource bounds;
- migration support.

Shared mechanisms such as timing, Sequence, Arp/Gate primitives, modulation infrastructure, resources and preset/state foundations should be reused. Instrument-specific musical logic stays inside the instrument module.

## Rack and MIDI routing

One Vox Electronic Engine instance initially supports up to **16 rack slots**.

Each loaded slot has a visible MIDI input assignment:

- CH1 ... CH16
- OFF

Adding an instrument automatically selects the next free channel.

Normal Cubase workflow:

1. Insert one Vox Electronic Engine instance.
2. Load instruments into rack slots.
3. Create multiple Cubase MIDI tracks routed to that VST3 instance.
4. Set each MIDI track to the required channel.
5. Channel N triggers only slots explicitly assigned to N.

No hidden Port A/B/C/D workflow, MIDI transformer or separate routing utility should be required for normal use.

Duplicate-channel assignment must not silently layer. The UI must require an explicit choice such as **SWAP / MOVE / LAYER**.

### Layer and zone routing

MIDI channel, key range, velocity range, transpose and audio output are independent routing dimensions.

Note ownership is bound to the exact destination `SlotId`, so NoteOff returns to the slot that received NoteOn even if routing/zones change while the note is held.

### GUI audition

The on-screen keyboard auditions the selected slot directly. Host MIDI remains channel-routed.

CC120/123 and panic behavior must be scoped correctly; global Panic may reset all slots.

## Automation and parameters

Modular instruments must remain usable with Cubase automation without unpredictable changes to the VST3 parameter topology.

The architecture therefore includes:

- stable global/rack host parameters;
- bounded per-slot automation/macro endpoints;
- persistent macro-to-instrument parameter mapping;
- stable instrument-local parameter identity;
- parameter smoothing/sample-accurate behavior where required.

Changing instruments must not corrupt unrelated automation lanes or project state.

## Pattern generation and genre packs

Pattern generation belongs to the instrument.

The global generator provides shared `GenerationContext`; each instrument generates role-appropriate musical material using its own grammar and a deterministic sub-seed derived from global seed + stable SlotId + InstrumentId.

Genre specialization belongs in **style/content/generator profiles**, not in the host architecture.

Examples:

- Bass: Psy Rolling, House Sub, Techno Bass, DnB Reese and future profiles;
- Acid: Classic 303, Psy Acid, Techno Acid, Hypnotic Acid;
- Lead: arp, pluck, stab, FM, wavetable and genre-specific phrase profiles;
- Semantic FX: transition-aware risers, downlifters, lasers, fills and related events;
- Atmos: long-form evolving texture/event generation rather than a forced 16-step note model.

Generation must remain **musically constrained**. Randomization is never an excuse for unconstrained noise.

Future content may be distributed as packs, for example:

- Trance / Psy Pack
- House Pack
- Techno Pack
- Drum & Bass Pack
- Ambient Pack
- future electronic style packs

A pack may contain presets, patterns, wavetables/resources, generator profiles/grammars, scenes and macro mappings without changing the core architecture.

## Presets and state

Preset classes are independent:

- Sound
- Pattern / Sequence / Arp
- Modulation
- Internal FX / chain
- Generator / Style
- Full Rack / Scene

Changing a Pattern must not silently change Sound, and vice versa, unless a Full Rack/Scene is loaded.

State is versioned and canonical. Persistent state stores configuration, routing, parameters, patterns, presets and module identity. Runtime-only data such as active voices, transient envelope/LFO phases, scratch buffers and meter history is not serialized unless explicitly justified.

Loading must be transactional:

```text
parse -> validate -> resolve -> construct/prepare -> commit
```

A bad, missing or incompatible module must not prevent the rest of the rack from restoring.

## Realtime and resource policy

The audio callback must not perform heap allocation, file I/O, module construction, blocking locks, GUI work or network operations.

Every instrument/rack subsystem has bounded limits for:

- voices;
- MIDI/events per block;
- modulation routes;
- sequencer/arp/gate activity;
- scratch/buffer resources;
- diagnostics counters.

At limits, behavior must remain deterministic and safe: deterministic voice stealing/rejection, bounded event rejection and diagnostics rather than hangs or allocation spikes.

## Host synchronization and deterministic rendering

Generated playback remains synchronized to host musical position using PPQ where available.

Realtime playback and Cubase export/bounce/freeze should make equivalent musical decisions for the same state and seed. Wall-clock time, GUI state and uncontrolled global RNG must not affect musical output.

Sample rate, block size, device restart, suspend/resume and offline render are part of the instrument lifecycle contract.

## Diagnostics and recovery

Pre-production builds may expose local diagnostics such as:

- Slot / Instrument / MIDI channel;
- last received event/channel;
- active voices / budget;
- loaded/missing/incompatible module state;
- state schema/module versions;
- latency/tail information;
- resource-budget warnings.

One bad slot/module must not take down the entire restored rack. Unresolved slots preserve identity, routing and opaque persistent state so they can recover when the correct module becomes available.

## Compatibility

Architecture refactors must not accidentally change plugin identity or break existing Cubase projects.

Compatibility-sensitive identity includes where applicable:

- VST3 class/plugin identity;
- bundle/company identity;
- manufacturer/plugin codes;
- existing host-visible parameter IDs;
- supported legacy state/preset migration.

Legacy Kick/MATCH state must be migrated, isolated or rejected explicitly. It must never be silently reinterpreted as Bass/Acid/Lead/another current instrument.

## Shared Vox platform direction

Vox Drum Engine should eventually reuse the generic platform concepts proven here:

- provider/module contracts;
- state/migration;
- routing primitives;
- automation/macros;
- resource system;
- diagnostics;
- realtime utilities;
- reusable UI foundations.

Do **not** prematurely split a separate shared-core repository while this architecture is still moving. First make the generic core clean and reusable inside Vox Electronic Engine. Extract/share only when there are at least two real product consumers.

## Current checkpoint order

The locked sequence before new synth expansion is:

```text
7A   Product split: remove active Kick/MATCH paths
7B.0 Host parameter + module/ABI-ready contract
7B   Generic Rack / Provider / Registry / MIDI Router
7C   Early installable pre-production pipeline
     -> Cubase pre-production handoff
8    Acid
9    Lead
10   Semantic FX
11   Atmos
12+  Shared modulation, creative FX, global generation, GUI, performance,
     reliability, factory content, packaging and final Cubase acceptance
```

After 7B.0 the core host/module contracts are considered frozen for 1.0 unless a demonstrated host-level defect requires change.

## Build on Windows

Requirements:

- Visual Studio 2022 with Desktop development with C++
- CMake 3.22+
- Git

```powershell
git clone https://github.com/ultima-vox/trance-vst-engine.git
cd trance-vst-engine
cmake -S . -B build -A x64
cmake --build build --config Release --clean-first --parallel
ctest --test-dir build -C Release --output-on-failure
```

The current repository name remains `trance-vst-engine` during the architecture refactor. Repository/target/path renaming should be handled later as a controlled rename pass rather than mixed into the 7B architecture work.

The Windows CI uses the same configure/build/test sequence and produces the VST3 artifact for validation.

For Cubase, the VST3 bundle is installed in the standard system VST3 location, typically:

```text
C:\Program Files\Common Files\VST3
```

Installer/uninstaller and upgrade/rollback behavior are part of the pre-production pipeline; manual copying is only a development fallback.

## Quality gates

Every instrument must pass a common compliance/isolation suite plus instrument-specific DSP tests.

Mandatory regression areas include:

- CH1–CH16 isolation and wrong-channel silence;
- actual sounding-engine identity;
- simultaneous multi-slot playback;
- Layer/key/velocity routing;
- scoped CC120/123/Panic;
- GUI keyboard selected-slot audition;
- preset/state isolation and wrong-engine rejection;
- instrument replacement without stale DSP;
- block-boundary routing;
- generator determinism/isolation;
- mixer/audio isolation;
- missing/incompatible module behavior;
- state/preset round-trip and migration;
- Cubase host-level smoke after rack/routing/factory changes.

A small non-shipping `ReferenceInstrument`/compliance fixture should validate generic rack/provider behavior independently of Bass/Acid implementation details.

## Reference Cubase regression session

Maintain a repeatable Cubase regression session with multiple slots/channels, automation, generation, layering/zones and scene restore. Significant architecture changes should be validated against this reference session rather than only through ad-hoc testing.

## Definition of done

Vox Electronic Engine is complete when one installable VST3 instance can host multiple independent synth/FX instrument modules in a rack, each explicitly and visibly assigned to MIDI channels without routing gymnastics, with instrument-specific pattern generation, genre/content packs, robust presets/state, deterministic workflows, stable automation, bounded realtime behavior and production-quality Cubase integration.
