# VOX UI

Shared JUCE UI foundation for the Ultima Vox plugin family.

Canonical documents:

- [`docs/UI_DESIGN_SYSTEM.md`](../../docs/UI_DESIGN_SYSTEM.md)
- [`docs/UI_COMPONENT_CATALOG.md`](../../docs/UI_COMPONENT_CATALOG.md)
- [`docs/UI_VISUAL_REFERENCE_SPEC.md`](../../docs/UI_VISUAL_REFERENCE_SPEC.md)
- [`docs/VOX_UI_FAMILY_ARCHITECTURE.md`](../../docs/VOX_UI_FAMILY_ARCHITECTURE.md)
- [`docs/UI_FOUNDATION_CHECKLIST.md`](../../docs/UI_FOUNDATION_CHECKLIST.md)

## Canonical location and ownership

This directory is the only shared VOX design-system implementation in this repository:

```text
libs/vox-ui/   target: vox_ui   namespace: vox::ui
libs/ui/       target: vst_ui   Electronic Engine product UI
apps/*         thin plugin/application shells
```

`Source/UI/` is not a valid path in this repository and must not be created.

## Scope

`vox-ui` contains only reusable family-level presentation primitives:

- design tokens;
- typography factories and canonical text sizes;
- `VoxLookAndFeel`;
- knobs, buttons, toggles, combo boxes and tab primitives;
- panels and section headers;
- generic graph/meter visual language;
- shared interaction and visual-state rules.

Product-specific widgets stay outside this module. In this repository they belong under `libs/ui/` (`vst_ui`). Examples:

- instrument rack and Part header;
- Bass / Acid / Lead / Atmos product panels;
- piano roll;
- step sequencer;
- arpeggiator / phrase editor;
- pattern-generator UI;
- modulation-matrix composition;
- Electronic Engine page composition.

Other products compose the same shared layer rather than copying it:

- Vox Drums Engine: drum pads, sample editor, drum sequencer;
- Mastering Engine: spectrum, loudness/true-peak/GR meters, module chain.

A product widget may move into `vox-ui` only after it has a second real consumer and can be made product-independent without leaking DSP or product state into the shared layer.

## Dependency rule

Allowed:

```text
product UI (vst_ui) -> vox_ui -> JUCE GUI
apps/* -> product UI
```

Forbidden:

```text
vox_ui -> product DSP
vox_ui -> PluginProcessor
vox_ui -> rack / preset / transport / instrument engine
DSP realtime path -> vox_ui
```

The shared module may depend on JUCE GUI/graphics modules only. UI visualisations must consume safe snapshots/atomics/message-thread data, never mutable realtime DSP objects directly.

## JUCE baseline

The repository currently pins:

```text
JUCE 9.0.1
C++20
```

New shared UI code should target that baseline unless the repository-wide JUCE version is intentionally changed.

## Current foundation state

At the foundation checkpoint the module contains:

- `Tokens.h` — canonical palette, spacing, radii and base component sizes;
- `Typography.h` — canonical text sizes (font factories still need to be added);
- `VoxLookAndFeel` — button background, rotary slider and combo-box drawing;
- `VoxPanel`;
- `VoxKnob`;
- `VoxSectionHeader`.

This is a bootstrap, not UI-1 completion. The authoritative gap list and acceptance checklist live in `docs/UI_FOUNDATION_CHECKLIST.md`.

## UI-1 required shared primitives

Before the foundation is considered complete, `vox-ui` must provide at least:

```text
VoxLookAndFeel
VoxPanel
VoxKnob
VoxButton
VoxComboBox
VoxTabBar
VoxSectionHeader
```

and `VoxLookAndFeel` must define the shared visual grammar for:

```text
rotary slider
button
combo box
toggle
linear slider
popup menu
```

The knob contract includes Small/Normal/Large sizes, a 270-degree rotary range, readable label/value presentation and a distinct modulation layer when modulation is present.

## CMake

This directory provides the reusable target:

```cmake
add_subdirectory(libs/vox-ui)
target_link_libraries(vst_ui PUBLIC vox_ui)
```

The parent project must make JUCE targets available before adding this directory.

The top-level project must actually add the subdirectory and link `vst_ui` against `vox_ui`; merely having `libs/vox-ui/CMakeLists.txt` in the tree is not integration.

## Namespace

Reusable code lives under:

```cpp
namespace vox::ui
```

Product-specific UI should use its own namespace and compose `vox::ui` components rather than modifying or forking them locally.

## Change policy

A change to shared colours, spacing, typography, component states or common control behaviour is a design-system change and must be reviewed as such.

Do not fork a shared component inside one plugin simply to obtain a different local style. Extend the shared component when the behaviour is genuinely family-wide; otherwise compose it from the product layer.
