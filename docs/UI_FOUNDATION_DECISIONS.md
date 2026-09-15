# VOX UI Foundation — Locked Implementation Decisions

**Status:** CANON for UI-1 implementation  
**Repository:** `ultima-vox/vox-electronic-engine`  
**Applies to:** `libs/vox-ui/` shared foundation

This document closes the remaining implementation choices for UI-1. It complements `UI_FOUNDATION_CHECKLIST.md`, `UI_DESIGN_SYSTEM.md`, `UI_COMPONENT_CATALOG.md`, and `UI_VISUAL_REFERENCE_SPEC.md`.

## 1. VoxKnob layout

Canonical layout:

```text
LABEL
 KNOB
VALUE
```

Rules:

- label is always above the rotary control;
- numeric value is always below it;
- value is always visible and must never be hover-only;
- Small / Normal / Large rotary diameters remain 36 / 48 / 64 px;
- rotary travel is explicitly 270 degrees, 135 -> 405 degrees;
- the centre contains only the rotary body and position marker, not the value label;
- label/value row geometry must use shared tokens rather than page-local literal dimensions.

API naming must not hide `juce::Component::setSize(int, int)`. Prefer `setKnobSize(Size)` (or expose the base overload explicitly) instead of a single-argument `setSize(Size)` API.

## 2. VoxKnob rendering ownership

`VoxKnob` owns:

- semantic knob size/style;
- interaction configuration;
- modulation visualization state supplied by the UI bridge;
- specialised VOX knob drawing.

The internal `juce::Slider` owns:

- value/range;
- mouse/keyboard interaction;
- host/APVTS attachment compatibility.

`VoxLookAndFeel::drawRotarySlider()` remains a fallback for ordinary JUCE sliders.

Do not use parent-hierarchy `dynamic_cast` from LookAndFeel to discover `VoxKnob` state.

## 3. Modulation visualization

Modulation visibility is data-driven, not style-driven.

Do not define a `Style::Modulated` mode. If a modulation range/depth exists, the ring is displayed automatically.

The implementation must not confuse a bipolar parameter with bipolar modulation. `Style::Bipolar` describes the base parameter/value grammar. Modulation needs an explicit signed/range contract of its own.

Preferred UI-facing representation:

```text
base normalized value: 0..1
modulation min/max normalized range: 0..1, clamped
```

or an equivalent signed-depth contract that can represent both positive and negative modulation. A single unsigned `0..1` amount is insufficient for all modulation routes.

The UI stores only the visualization snapshot/range; it does not become the authoritative DSP modulation state.

## 4. Reset and interaction policy

Do not hardcode double-click reset to `0.0` for every knob. The reset value must come from the parameter/default-value contract.

All knobs must explicitly define a family-wide policy for:

- normal drag;
- fine adjustment modifier;
- double-click reset to the real default;
- mouse-wheel policy;
- tooltip/value formatting.

Do not rely on accidental JUCE defaults for these behaviours.

## 5. VoxSectionHeader

Canonical structure:

```text
[leading]  SECTION TITLE  [action]
```

Leading slot is mutually exclusive:

```cpp
enum class LeadingType { None, Power, Icon };
```

Required API semantics:

- `setPowerVisible(true)` -> `LeadingType::Power`, clears any icon;
- `setPowerVisible(false)` -> `None` if Power was active;
- `setIcon(std::unique_ptr<juce::Drawable>)` -> `LeadingType::Icon`, disables Power leading;
- `clearIcon()` -> `None` if Icon was active;
- `setActionComponent(juce::Component*)` sets the optional non-owned trailing component.

The header owns the Drawable. It does not own the trailing action component.

When replacing the trailing action component, the previous child must be detached before the pointer is replaced. The implementation must not leave stale visible children.

Icons are vector-based. A shared icon must obey VOX token colours/stroke grammar; arbitrary colours embedded in an SVG/Drawable must not silently bypass the design system.

## 6. VoxButton icon variant

`VoxButton::Type::Icon` is not complete unless the component has an actual vector-icon API. A textless button with no icon renderer is not an Icon button.

The shared component should support an owned/copy-safe Drawable or another central vector-icon contract and apply the same hover/focus/disabled rules as other button variants.

## 7. VoxTabBar

Use a dedicated VOX component rather than default `juce::TabbedButtonBar` visuals.

However, the implementation must preserve standard control semantics:

- keyboard focus;
- keyboard navigation where applicable;
- disabled state;
- pressed state;
- accessibility semantics;
- predictable click targets.

A custom-painted container that only hit-tests in `mouseDown()` is not sufficient for production acceptance if it discards those behaviours. Prefer composing dedicated tab buttons/components while keeping the public `VoxTabBar` API product-independent.

Canonical height remains 36 px.

## 8. LookAndFeel completeness

UI-1 LookAndFeel must provide VOX grammar for:

- button background;
- ordinary rotary-slider fallback;
- combo box;
- toggle button;
- linear slider;
- popup menu item.

Linear-slider styling must include a visible thumb/handle where the slider semantics require one. A track-only implementation is not sufficient for a normal editable slider/fader.

Popup-menu styling must render supported tick/icon/submenu information rather than merely reserving blank space for it.

## 9. Typography

JUCE baseline is 9.0.1, so `juce::FontOptions` may be used.

Keep the four canonical roles:

- Instrument Title: 26 px, semibold intent;
- Section Title: 14 px, semibold intent;
- Control Label: 12 px;
- Value / Secondary: 11 px.

Do not assume the platform default typeface contains a style literally named `SemiBold`. The implementation must use a deterministic fallback (for example an available bold/medium weight) until the product adopts an explicit font family.

Preserve numeric size constants for layout and provide font factories separately so existing code is not forced to treat function names as constants.

## 10. CMake and dependency boundary

Canonical dependency direction remains:

```text
vst_ui -> vox_ui -> JUCE GUI
```

`vox_ui` must be added to the normal top-level CMake graph before UI-1 is considered implemented. Merely having `libs/vox-ui/CMakeLists.txt` is not sufficient.

Do not replace the existing `add_library(vox_ui STATIC ...)` with an unrelated `juce_add_module(...)` pattern. The repository currently uses an ordinary CMake target for `vox_ui` and should keep that model unless a separate architecture decision changes it.

Shared library dependencies should remain minimal (`juce_gui_basics` / required graphics modules). `juce_gui_extra` is appropriate for the showcase app if needed, not automatically for the shared library.

## 11. Showcase policy

`VOX_UI_BUILD_SHOWCASE` defaults to **OFF** in the reusable library so downstream consumers are not forced to build a development app.

Development/CI validation may explicitly enable it.

Showcase acceptance includes:

- 1440x1080 reference window;
- Small / Normal / Large knobs;
- modulation example;
- primary/secondary/toggle/danger/icon buttons;
- combo box and popup;
- tabs;
- section headers with None/Power/Icon leading variants;
- normal/active/disabled/focused states;
- resize validation;
- supported scale factors;
- screenshots at 100% and 125% minimum.

The showcase must install/reset any global LookAndFeel safely with a lifetime that outlives all child components.

## 12. Canonical UI-1 PR sequence

### UI-1a — build integration + typography + LookAndFeel

- integrate `vox_ui` into the top-level CMake graph;
- link `vst_ui` -> `vox_ui`;
- add JUCE 9 font factories while preserving canonical numeric sizes;
- complete LookAndFeel fallback/states;
- keep existing product UI behaviour unchanged.

### UI-1b — shared components

- split components into `libs/vox-ui/components/`;
- retain `VoxComponents.h` as umbrella include;
- complete `VoxKnob`, `VoxButton`, `VoxComboBox`, `VoxTabBar`, `VoxSectionHeader`;
- keep product-specific editors/panels out of `vox_ui`.

### UI-1c — showcase + visual acceptance

- optional showcase app (`VOX_UI_BUILD_SHOWCASE=OFF` by default);
- scale/resize/state validation;
- screenshot acceptance;
- only after acceptance mark UI-1 as implemented in the component catalog.

## 13. Status rule

Do **not** mark UI-1 as `Implemented` in `UI_COMPONENT_CATALOG.md` merely because code has been drafted. Mark it implemented only after:

- normal CI compiles `vox_ui`;
- product build/tests are green;
- showcase builds when enabled;
- screenshot/state/scale acceptance passes.
