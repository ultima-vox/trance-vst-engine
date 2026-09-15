# VOX UI Foundation Checklist

**Status:** CANON implementation checklist for UI-1  
**Repository:** `ultima-vox/vox-electronic-engine`  
**Shared module:** `libs/vox-ui/`  
**Product UI module:** `libs/ui/`  
**JUCE baseline:** 9.0.1  
**C++ baseline:** C++20

This document records the verified implementation state of the shared VOX UI foundation and the work required before UI-1 can be considered complete.

It complements, but does not replace:

- `UI_DESIGN_SYSTEM.md` — visual and interaction rules;
- `UI_COMPONENT_CATALOG.md` — semantic component inventory;
- `UI_VISUAL_REFERENCE_SPEC.md` — accepted visual/layout grammar;
- `VOX_UI_FAMILY_ARCHITECTURE.md` — shared/product ownership and dependency direction.

---

## 1. Canonical repository structure

The following paths are authoritative:

```text
libs/vox-ui/   target vox_ui   namespace vox::ui   shared VOX UI primitives
libs/ui/       target vst_ui   Electronic Engine product UI
apps/*         thin plugin/application shells
```

`Source/UI/` is not a valid path in this repository. Do not create it.

Shared `vox-ui` must not contain product-specific components such as:

- BassPanel;
- AcidPanel;
- LeadPanel;
- AtmosPanel;
- InstrumentRack;
- PartHeader;
- PianoRoll;
- StepSequencer;
- Arpeggiator / Phrase editor;
- PatternGenerator UI;
- Electronic Engine page composition.

These belong to `libs/ui/` until there is a second real consumer and a product-independent abstraction is justified.

---

## 2. Verified repository baseline

The root CMake currently pins:

```text
JUCE 9.0.1
C++20
```

Therefore JUCE 9 APIs may be used deliberately. Typography should use `juce::FontOptions`/`juce::Font` rather than treating bare float sizes as the complete typography contract.

Do not depend on a platform-specific font style string such as `"SemiBold"` being available in the default typeface. `SemiBold` is the design intent; implementation must have a deterministic fallback (for example an available bold/medium weight or a bundled/explicit family later if the project adopts one).

---

## 3. Verified current implementation state

### 3.1 `Tokens.h` — PASS for bootstrap

Current implementation already defines:

- background/panel/control/graph colours;
- default/subtle borders;
- primary/hover/dim accents;
- primary/secondary/muted text;
- danger/success/warning roles;
- 4 px spacing scale;
- radii;
- control/tab heights;
- Small/Normal/Large knob diameters;
- 1440x1080 reference size.

No duplicate local palette should be introduced.

### 3.2 `Typography.h` — PARTIAL

Current implementation contains only numeric heights:

```text
instrumentTitle 26
sectionTitle    14
controlLabel    12
valueText       11
```

UI-1 requires:

- keep canonical numeric size constants;
- add `juce::Font` factory helpers;
- encode weight intent deterministically;
- preserve separate `controlLabel` and `valueText` roles;
- avoid local font-size literals in components.

### 3.3 `VoxComponents` — PARTIAL

Currently present:

- `VoxPanel`;
- `VoxKnob`;
- `VoxSectionHeader`.

Missing UI-1 primitives:

- `VoxButton`;
- `VoxComboBox`;
- `VoxTabBar`.

As the component count grows, split implementation into `components/*.h/.cpp` and retain `VoxComponents.h` as an umbrella compatibility header.

### 3.4 `VoxLookAndFeel` — PARTIAL

Currently implemented:

- button background;
- rotary slider;
- combo box.

UI-1 still requires shared styling for:

- toggle button;
- linear slider/fader baseline;
- popup menu item.

### 3.5 `vox_ui` CMake target — EXISTS BUT NOT YET INTEGRATED

`libs/vox-ui/CMakeLists.txt` defines a `vox_ui` static library.

However, the top-level CMake currently defines `vst_ui` directly and does not yet add `libs/vox-ui` or link `vst_ui` to `vox_ui`.

This is a blocker because code inside `libs/vox-ui` is not guaranteed to compile as part of normal CI until the parent build actually includes the target.

Required integration:

```cmake
add_subdirectory(libs/vox-ui)
...
target_link_libraries(vst_ui PUBLIC
  vox_ui
  juce::juce_gui_basics
  juce::juce_audio_utils
  juce::juce_audio_processors
  vst_sequence vst_parts vst_preset)
```

Exact placement may follow the existing target ordering, but dependency direction must remain:

```text
vst_ui -> vox_ui -> JUCE GUI
```

---

## 4. Knob contract for UI-1

The current `VoxKnob` is only a bootstrap wrapper around `juce::Slider`.

UI-1 requires the following explicit contract.

### 4.1 Sizes

```text
Small  = 36 px
Normal = 48 px
Large  = 64 px
```

Knob rendering must scale proportionally with diameter. Do not hardcode a single stroke/inset/pointer geometry that only looks correct at one size.

### 4.2 Rotary range

Canonical range:

```text
270 degrees
135 degrees -> 405 degrees
```

Set this explicitly on the slider; do not rely on JUCE defaults.

### 4.3 Visual anatomy

Required layers:

1. inactive arc;
2. optional modulation range;
3. active value arc;
4. dark recessed body;
5. subtle outer/body ring;
6. position marker;
7. label;
8. readable numeric value.

The modulation range must be visually distinct from the base parameter value.

### 4.4 Interaction

All VOX knobs must use one family-wide policy for:

- drag mode;
- double-click reset;
- fine adjustment modifier;
- mouse wheel enable/disable;
- tooltip/value formatting.

Do not rely on accidental JUCE defaults. Configure the policy explicitly and apply it consistently.

### 4.5 Rendering ownership decision

Canonical decision for UI-1:

- `VoxKnob` owns semantic state and interaction configuration;
- `VoxLookAndFeel` owns shared slider rendering;
- do **not** discover modulation state by `dynamic_cast`-ing the slider parent component.

If renderer-specific state is required, use a dedicated shared slider subtype or another explicit shared-state contract inside `vox-ui`. This keeps the LookAndFeel independent of parent hierarchy and makes later reuse safer.

---

## 5. `VoxSectionHeader` contract

The current component is text-only and therefore incomplete.

Canonical section header grammar:

```text
[optional power/status/icon] SECTION TITLE        [optional action/selector]
```

UI-1 requirements:

- uppercase/small-caps title role;
- optional left icon or power/bypass control;
- optional right action slot;
- no product-specific semantics inside the shared component;
- geometry based on design tokens;
- predictable disabled/bypassed state.

The shared component provides layout/visual grammar; product code supplies the actual action component and behaviour.

---

## 6. `VoxButton` contract

Required semantic variants:

```text
Primary
Secondary
Toggle
Danger
Icon
```

Required states:

```text
Normal
Hover
Pressed
Focused
Active
Disabled
```

Rules:

- height uses the canonical control-height token;
- `Danger` uses the danger semantic role only for destructive/critical actions;
- Toggle active state must be obvious without hover;
- Icon buttons use vector/drawable icons, never emoji;
- colours/radii come only from tokens.

---

## 7. `VoxComboBox` contract

UI-1 requirements:

- canonical control height;
- tokenised background, border and radius;
- readable selected text;
- hover/focus/disabled state;
- popup menu rendered through VOX visual grammar;
- no default JUCE appearance leaking through.

A thin wrapper around `juce::ComboBox` is sufficient if behaviour is standard and all styling is centralised.

---

## 8. `VoxTabBar` contract

Use a dedicated VOX component rather than relying on default JUCE tab visuals.

Canonical geometry:

```text
height = 36 px
```

Active:

```text
background = accent.primary
text       = background/window role
```

Inactive:

```text
background = control
text       = text.secondary
border     = border.default
```

The implementation may internally use JUCE primitives, but it must expose a simple product-facing API and exact VOX states without local restyling.

A custom VOX tab component is preferred over forcing product pages to customize `juce::TabbedButtonBar` individually.

---

## 9. Component file layout

Once UI-1 components are added, prefer:

```text
libs/vox-ui/
  CMakeLists.txt
  README.md
  Tokens.h
  Typography.h
  VoxLookAndFeel.h/.cpp
  VoxComponents.h            # umbrella header
  components/
    VoxPanel.h/.cpp
    VoxKnob.h/.cpp
    VoxButton.h/.cpp
    VoxComboBox.h/.cpp
    VoxTabBar.h/.cpp
    VoxSectionHeader.h/.cpp
```

`VoxComponents.h` remains as the stable aggregate include while implementation files are split by responsibility.

Do not create `Source/UI/`.

---

## 10. Showcase requirement

UI-1 is not complete without a component showcase.

Recommended structure:

```text
libs/vox-ui/showcase/
  CMakeLists.txt
  Main.cpp
```

Recommended CMake option:

```text
VOX_UI_BUILD_SHOWCASE
```

Library default should be `OFF` so consumers are not forced to build the showcase. The root development build/CI may explicitly enable it for UI validation.

Showcase acceptance:

- 1440x1080 reference window;
- all UI-1 primitives visible together;
- normal/active/disabled examples;
- Small/Normal/Large knobs;
- modulation example;
- resize validation;
- scale checks at 75/100/125/150/200%;
- no product DSP dependency.

---

## 11. UI-1 checkpoint sequence

### UI-1a — build integration + typography + LookAndFeel

Required:

- wire `add_subdirectory(libs/vox-ui)` into the normal project build;
- link `vst_ui` against `vox_ui`;
- convert typography from bare sizes to size constants + font factories;
- make rotary drawing scale with diameter;
- add missing toggle/linear-slider/popup-menu styling;
- preserve current product UI behaviour.

Acceptance:

- `vox_ui` compiles in normal CI;
- no product/DSP dependency introduced;
- existing tests still pass.

### UI-1b — shared components

Required:

- complete `VoxKnob` contract;
- add `VoxButton`;
- add `VoxComboBox`;
- add `VoxTabBar`;
- complete `VoxSectionHeader`;
- split component files while retaining umbrella include;
- update `libs/vox-ui/CMakeLists.txt`.

Acceptance:

- all required visual states implemented;
- all dimensions come from tokens;
- no product-specific component enters `vox_ui`.

### UI-1c — showcase + visual acceptance

Required:

- add optional showcase app;
- validate 100% and 125% screenshots at minimum;
- validate remaining family scale factors;
- document any deliberate deviation from the accepted references.

Acceptance:

- screenshot review passes;
- resize works;
- no default JUCE visual leakage;
- UI-1 checklist is fully checked off.

---

## 12. Build / CI acceptance

Windows reference build:

```text
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

When the showcase is enabled, it must build in the same configuration without changing the plugin DSP or host automation schema.

UI work must not alter:

- parameter IDs;
- automation compatibility;
- preset/state schema;
- realtime DSP behaviour;
- instrument ABI/contract.

---

## 13. Definition of Done — UI-1 foundation

UI-1 is complete only when all of the following are true:

- [ ] `vox_ui` is part of the normal top-level CMake graph;
- [ ] `vst_ui` depends on `vox_ui`;
- [ ] no `Source/UI/` path exists;
- [ ] typography has reusable font factories + canonical sizes;
- [ ] `VoxPanel` is tokenised;
- [ ] `VoxKnob` supports Small/Normal/Large;
- [ ] `VoxKnob` uses explicit 270-degree range;
- [ ] modulation range is visually distinct;
- [ ] `VoxButton` exists with canonical variants/states;
- [ ] `VoxComboBox` exists with VOX popup styling;
- [ ] `VoxTabBar` exists;
- [ ] `VoxSectionHeader` supports left status/icon + title + right action slot;
- [ ] toggle, linear slider and popup menu visual grammar is implemented;
- [ ] shared components do not depend on product DSP/state;
- [ ] showcase builds when enabled;
- [ ] reference screenshots are reviewed;
- [ ] resize and supported scale factors pass;
- [ ] tests remain green.

---

## 14. Agent guardrails

Before changing shared UI code, an agent must read the canonical UI documents and this checklist.

The agent must not:

- create `Source/UI/`;
- duplicate `Tokens.h` in `libs/ui`;
- put Bass/Acid/Lead/Atmos pages into `vox-ui`;
- move PianoRoll/StepSequencer/Arpeggiator into `vox-ui` during UI-1;
- use local hardcoded colours/radii/fonts when a token exists;
- mix DSP refactors into a shared UI checkpoint;
- add fake meters/graphs/modulation states;
- read mutable realtime DSP structures directly from paint/timer code.

If a new family-level primitive is required, add it deliberately to `vox-ui` and update the design-system documentation instead of creating a private local clone.
