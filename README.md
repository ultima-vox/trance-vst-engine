# Vox Trance Engine

Generative VST3 workstation for **dark psytrance, psytrance, forest and related electronic music**.

> The authoritative implementation contract is [issue #11](../../issues/11). The README describes the product and current architecture at a high level; #11 defines checkpoint order, acceptance gates, migration rules and delivery policy.

## Product direction

Vox Trance Engine is one Windows x64 VST3 instance containing a **Kontakt-like internal instrument rack** with explicit, simple multitrack MIDI routing.

The target product is not a collection of hardcoded `Part N = Engine X` paths. Each rack slot hosts a generic Instrument instance and can be assigned independently to MIDI CH1-CH16.

Initial instrument modules:

- Bass
- Acid
- Lead
- Semantic FX
- Atmos

**Kick/Drums are not part of the target Vox Trance Engine product.** Kick moves to the future **Vox Trance Drums** product. Kick/Bass mix analysis and matching belong to **Mastering Engine**.

## Delivery policy

The development goal is an **integrated, installable pre-production plug-in**, not a pile of disconnected PRs.

PRs/checkpoints exist for CI, review and rollback, but the required handoff after the architecture refactor is one working `Vox Trance Engine.vst3` that can be installed and tested in Cubase.

The mandatory pre-production handoff after checkpoints 7A -> 7B.0 -> 7B (and 7C where needed) must include:

- generic Rack and RackSlot model;
- InstrumentProvider / Registry / Factory / Instance architecture;
- Bass migrated through the generic Instrument contract;
- at least one second real instrument module for isolation testing;
- explicit MIDI CH1-CH16 routing;
- deliberate Layer mode;
- key/velocity zones and transpose;
- state/preset round-trip and migration;
- Cubase automation/macros;
- resource budgets and realtime-safe behavior;
- missing/incompatible-module handling;
- installer/package suitable for Cubase validation;
- green Windows CI.

Manual Cubase acceptance is required where host behavior cannot be proven by unit/integration tests.

## Architecture

The host shell must not contain Bass/Acid/Lead-specific routing or DSP branches.

Conceptual dependency direction:

```text
core/shared
    -> instrument contracts
        -> instrument providers/modules
            -> rack + MIDI routing + state
                -> VST3 host shell
```

Conceptual module model:

```text
Vox Trance Engine.vst3
│
├─ Core / Shared
│  ├─ Transport / PPQ sync
│  ├─ Sequence / Pattern primitives
│  ├─ MIDI export
│  ├─ Preset / State / Migration
│  ├─ Shared Arp / Gate / Modulation primitives
│  └─ GenerationContext
│
├─ Rack
│  ├─ RackSlot (stable SlotId)
│  ├─ MIDI Router
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

For 1.0, built-in instruments may remain **statically linked CMake modules inside the VST3**. They are architecturally separated from the core so a future external-module loader can be added at the provider boundary without rewriting Rack, routing, presets, state or the VST3 shell.

## External-module-ready contract

The 1.0 architecture is intentionally prepared for future external instrument packs, without implementing a binary SDK/loader yet.

Required separation:

- `InstrumentId` is globally stable and independent from slot index or provider;
- `SlotId` is stable and independent from display order and MIDI channel;
- provider is a resolution mechanism, not instrument identity;
- missing/incompatible modules remain explicit unresolved slots; there is no silent fallback to another synth;
- state and presets use stable IDs plus explicit schema/version metadata;
- core UI and state logic operate on generic descriptors/capabilities rather than concrete instrument classes;
- future external ABI must avoid passing JUCE/STL-owned objects across DLL boundaries.

Future external loading should therefore add discovery/manifest/ABI/signature/loading infrastructure under the provider boundary rather than refactor the rack.

## Instrument contract

Each instrument module must expose or provide the equivalent of:

- stable InstrumentId, display name and versions;
- DSP lifecycle: prepare/reset/process/reprepare;
- parameter descriptors and state contract;
- Sound preset contract;
- Pattern/Sequence/Arp preset contract where applicable;
- instrument-owned style-aware PatternGenerator;
- Sequence capabilities;
- modulation destinations;
- editor/view-model capabilities;
- latency and tail behavior;
- realtime resource bounds;
- migration support.

Shared mechanisms such as sequence timing, arp/gate primitives, modulation infrastructure and preset/state foundations should be reused. Instrument-specific musical logic remains inside each instrument module.

## Rack and MIDI routing

One Vox Trance Engine instance supports up to **16 rack slots** initially.

Each loaded slot has a visible MIDI input assignment:

- CH1 ... CH16
- OFF

Adding an instrument automatically selects the next free channel.

Normal Cubase workflow:

1. Insert one Vox Trance Engine instance.
2. Load instruments into rack slots.
3. Create Cubase MIDI tracks routed to that VST3 instance.
4. Set each MIDI track to the required channel.
5. CH1-CH16 directly select the corresponding assigned rack slots.

No hidden Port A/B/C/D scheme, MIDI transformer or separate routing utility should be required for ordinary use.

Duplicate-channel assignment must never create accidental silent layering. The UI must require an explicit choice such as SWAP / MOVE / LAYER.

### Layer and zone routing

Channel, key zone, velocity zone, transpose and audio output are independent routing dimensions.

Layered slots may define:

- key range;
- velocity range;
- transpose.

Note ownership is bound to the destination `SlotId` so NoteOff reaches the slot that received NoteOn even if channel or zone routing changes while the note is held.

### GUI audition

The on-screen keyboard auditions the currently selected slot directly. Host MIDI remains channel-routed.

CC120/123 and panic behavior must be scoped correctly; global Panic may reset all slots.

## Automation and parameters

Modular instruments must remain automatable in Cubase without requiring a changing VST3 parameter layout for every future module.

The architecture therefore includes a stable host-facing parameter/automation contract and per-slot macro/automation mapping. Instrument-internal parameter identity remains separate from slot identity and host parameter identity.

Automation behavior must be deterministic, appropriately smoothed and safe across buffer-size/sample-rate changes.

## Pattern generation

Pattern generation belongs to the instrument.

The global generator provides shared `GenerationContext`; each loaded instrument generates role-appropriate musical material using its own grammar and a deterministic sub-seed derived from global seed + stable SlotId + InstrumentId.

Examples:

- Bass: Rolling Psy, Dark Psy, Offbeat, Triplet, Hi-Tech, Progressive
- Acid: Classic 303, Psy Acid, Dark Acid, Forest Acid, Hi-Tech Acid, Hypnotic Acid
- Lead: Dark Psy Lead, Forest Call, Alien Phrase, Metallic Sequence, Hi-Tech Burst, Psy Arp, Hypnotic Lead
- Semantic FX: transition-aware risers, downlifters, lasers, fills and related events
- Atmos: long-form evolving texture/event generation rather than a forced 16-step note model

Generation must remain **musically constrained**. Randomization is never an excuse for unconstrained noise.

## Presets and state

Preset classes are independent:

- Sound
- Pattern / Sequence / Arp
- Modulation
- Internal FX / chain
- Generator / Style
- Full Rack / Scene

Changing a Pattern must not silently change Sound, and vice versa, unless a Full Rack/Scene is loaded.

State is versioned and canonical. Persistent state stores configuration, routing, parameters, patterns, presets and module identity. Runtime-only data such as active voices, transient envelope/LFO phases, scratch buffers and meter history is not serialized unless explicitly required.

Loading must be transactional: parse -> validate -> construct -> prepare -> commit. A bad or missing module must not corrupt the rest of the rack.

## Realtime and resource policy

The audio callback must not perform heap allocation, file I/O, module construction or blocking synchronization.

Every instrument/rack subsystem has bounded resource limits for:

- voices;
- MIDI/events per block;
- modulation routes;
- sequencer/arp/gate activity;
- scratch/buffer resources.

When a limit is reached, behavior must be deterministic and diagnosable, for example deterministic voice stealing or bounded event rejection. Hangs, unbounded allocation and random failure are not acceptable.

## Host synchronization and rendering

Generated playback remains synchronized to host musical position using PPQ where available.

Realtime playback, Cubase export/bounce/freeze and deterministic generation should produce equivalent musical decisions for the same state and seed. Wall-clock randomness must not influence musical output.

Sample rate, block size, device restart, suspend/resume and offline render are part of the instrument lifecycle contract.

## Pre-production diagnostics

A local diagnostic/debug view may expose information useful during Cubase validation, such as:

- Slot / Instrument / MIDI channel;
- last received event/channel;
- active voice count / budget;
- loaded/missing/incompatible module state;
- state schema/module versions;
- latency/tail information;
- resource-budget warnings.

Diagnostics must not add realtime-thread risk.

## Compatibility

Architecture refactors must not accidentally change plug-in identity or break existing Cubase projects.

Compatibility-sensitive identity includes, where applicable:

- VST3 class/plugin identity;
- bundle/company identity;
- manufacturer/plugin codes;
- existing host-visible parameter IDs;
- supported legacy state/preset migration.

Legacy Kick/Match content must be migrated or rejected explicitly. It must never be silently reinterpreted as Acid/Lead/another instrument.

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

After 7B.0 the core architecture is considered frozen for 1.0 unless a measured integration defect justifies a contract change.

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

The Windows CI uses the same configure/build/test sequence and produces the VST3 artifact for validation.

For Cubase, the VST3 bundle is installed in the standard system VST3 location, typically:

```text
C:\Program Files\Common Files\VST3
```

An installer/uninstaller and upgrade path are part of the pre-production pipeline; manual copying is only a development fallback.

## Quality gates

Every new instrument must pass a common compliance/isolation suite plus instrument-specific DSP tests.

Mandatory regression areas include:

- CH1-CH16 isolation and wrong-channel silence;
- actual sounding-engine identity (wrong synth must never render);
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

A small non-shipping `ReferenceInstrument`/compliance fixture should be used where useful to prove generic rack/provider behavior independently of Bass/Acid implementation details.

## Reference Cubase regression session

A repeatable Cubase session should be maintained as a practical regression fixture with multiple slots/channels, automation, generation, layer/zones and scene restore. Significant architecture changes are validated against this reference session rather than by ad-hoc testing only.

## Definition of done

Vox Trance Engine is complete when one installable VST3 instance can host multiple independent synth/FX instrument modules in a rack, each explicitly and visibly assigned to MIDI channels without routing gymnastics, with instrument-specific pattern generation, robust presets/state, deterministic workflows, stable automation, bounded realtime behavior and production-quality Cubase integration.
