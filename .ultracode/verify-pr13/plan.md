# Verification Plan: PR #13 vs Issue #11 Phases 0-4

## Objective
Independently verify that PR #13 truthfully satisfies Phases 0-4 of issue #11. Do not reinterpret, simplify, skip acceptance criteria, or close the issue.

## Scope
- Issue #11 Phases 0-4 only (no Phase 5, no Kick, no future phases)
- All 10 mandatory fixes from the original task
- All 3 second-review blockers
- Build, tests, and code quality checks

## Verification Packets

### Packet A: Realtime Safety Audit
- Verify no heap allocation in processBlock/renderNextBlock
- Verify no IIR coefficient object construction in per-sample path
- Verify no locks, no file I/O in audio callback
- Verify scratch buffer is preallocated

### Packet B: Ownership & Lifetime Audit
- Verify presetManager_ is unique_ptr
- Verify no raw owned pointer leaks
- Verify no manual delete needed

### Packet C: Timing Mode Verification
- Verify canonical stepsPerQuarterNote helper exists
- Verify realtime playback uses it
- Verify MIDI export uses it
- Verify tests cover all 4 timing modes

### Packet D: Ratchet Verification
- Verify realtime ratchet scheduling
- Verify MIDI export ratchet events
- Verify no stuck notes
- Verify tests exist

### Packet E: Slide Verification
- Verify internal synth legato glide
- Verify MIDI export pitch-bend ramp
- Verify deterministic behavior
- Verify tests exist

### Packet F: RNG Audit
- Verify no std::rand in realtime path
- Verify instance-owned deterministic RNG
- Verify seed saved/restored

### Packet G: Preset Semantics Audit
- Verify Sound vs Full preset separation
- Verify factory presets are Sound presets
- Verify exact factory preset names

### Packet H: Schema Versioning Audit
- Verify explicit version field
- Verify version dispatch in loader
- Verify fail-safe corrupt data handling
- Verify round-trip and malformed-data tests

### Packet I: Build & Test Verification
- Clean Release configure/build
- All ctest targets pass
- VST3 artifact produced

### Packet J: Code Quality Audit
- No TODO/FIXME in completed paths
- No placeholder implementations
- No dead controls
- No duplicated unsynchronized state

## Acceptance
All packets must pass. Any failure is a blocker.
