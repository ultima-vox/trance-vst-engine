# VOX UI

Shared JUCE UI foundation for the Ultima Vox plugin family.

Canonical visual specification: [`docs/UI_DESIGN_SYSTEM.md`](../../docs/UI_DESIGN_SYSTEM.md).

## Scope

This module contains only reusable presentation-layer primitives:

- design tokens;
- typography sizes;
- `VoxLookAndFeel`;
- base panels, knobs and section headers;
- common interaction and visual-state rules.

Product-specific widgets stay in the product repository. Examples:

- Trance Engine: instrument rack, piano roll, sequencer, modulation matrix;
- Vox Drums Engine: drum pads, sample waveform, velocity editor;
- Mastering Engine: spectrum, loudness/true-peak/GR meters, module chain.

## Dependency rule

`vox-ui` may depend on JUCE GUI modules, but must not depend on any product DSP, processor, preset, rack, transport or instrument implementation.

DSP and host integration must depend on UI only at the shell/composition layer, never from realtime code.

## CMake

The directory provides a reusable `vox_ui` target. In a host project:

```cmake
add_subdirectory(path/to/vox-ui)
target_link_libraries(MyPlugin PRIVATE vox_ui)
```

The parent project must make JUCE targets available before adding this directory.

## Namespace

Reusable code lives under:

```cpp
namespace vox::ui
```

Product-specific UI should use its own namespace and compose `vox::ui` components rather than modifying them locally.

## Change policy

A change to shared colors, spacing, component states or common control behavior is a design-system change and should be reviewed as such. Do not fork a shared component inside one plugin simply to obtain a different local style.
