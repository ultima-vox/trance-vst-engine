# VOX UI Family Architecture

**Status:** CANON  
**Version:** 1.1

## Goal

All Ultima Vox audio products share one visual and interaction language while preserving product-specific workflows.

The shared layer is **VOX UI**. Individual plugins compose it rather than reimplementing their own themes.

## Product family

Initial consumers:

- Vox Electronic Engine;
- Vox Drums Engine;
- Mastering Engine;
- future Ultima Vox VST3/Standalone products.

These are separate products. Sharing VOX UI does **not** mean that product-specific instruments or workspaces are hosted inside another engine.

## Architectural ownership

The UI stack is split into three strict layers:

```text
apps/*
  product shell / plugin editor / standalone host composition
        |
        v
libs/ui/*
  Vox Electronic Engine product UI and workflows
        |
        v
libs/vox-ui/*
  family-wide design system primitives
        |
        v
JUCE GUI
```

Dependency direction is one-way.

Allowed:

```text
apps/* -> libs/ui -> libs/vox-ui -> JUCE GUI
```

Allowed where a shell only needs shared primitives:

```text
apps/* -> libs/vox-ui -> JUCE GUI
```

Forbidden:

```text
libs/vox-ui -> libs/ui
libs/vox-ui -> product DSP
libs/vox-ui -> PluginProcessor
libs/vox-ui -> presets/rack/instrument engines
DSP realtime path -> libs/vox-ui
DSP realtime path -> libs/ui
```

## Canonical source layout

Current production layout:

```text
libs/
├── vox-ui/
│   ├── Tokens.h
│   ├── Typography.h
│   ├── VoxLookAndFeel.h/.cpp
│   ├── VoxComponents.h/.cpp
│   ├── VoxKnob.h/.cpp
│   └── future family-wide primitives
│
└── ui/
    ├── GlobalHeader.*
    ├── MainNavigation.*
    ├── PresetBrowser.*
    ├── BassPanel.*
    ├── StepSequencer.*
    ├── ZoneRangeEditor.*
    ├── common/
    └── future Electronic Engine pages/workflow widgets

apps/
└── */
    plugin/standalone shell composition only
```

### Forbidden legacy layout

The following path is **not canonical and must not be introduced for new UI work**:

```text
Source/UI/
```

Any older documentation or code comments that recommend `Source/UI/Design`, `Source/UI/Components`, `Source/UI/Pages` or similar are superseded by this document.

## Shared layer: `libs/vox-ui`

`libs/vox-ui` owns only family-wide concepts:

- semantic colours;
- spacing, radius, stroke, opacity and size tokens;
- typography scale;
- component state grammar;
- focus rules;
- interaction constants;
- `VoxLookAndFeel`;
- knobs, sliders, buttons, toggles and selectors;
- panels and section headers;
- graph/meter visual language;
- keyboard focus/hover/disabled behavior;
- scaling rules;
- tooltip and fine-adjustment conventions;
- realtime-safety boundary between UI and DSP.

Namespace:

```cpp
vox::ui
```

`libs/vox-ui` must not depend on product semantics.

A component belongs in `libs/vox-ui` only when it can reasonably be reused by another VOX audio product without knowing what `Bass`, `Lead`, `Acid`, `Atmos`, `Mastering`, `Drums`, `Rack` or `Part` means.

## Product layer: `libs/ui`

`libs/ui` owns Electronic Engine semantics and workflows:

- global header composition;
- main navigation;
- instrument/Part rack;
- Part header;
- synth panels;
- Sound page;
- Pattern page;
- Routing page;
- Zones page;
- Macros page;
- Advanced page;
- preset browser workflow;
- piano roll;
- per-Part arpeggiator/phrase editor;
- per-Part step sequencer;
- modulation matrix;
- effect routing and product-specific graph composition.

Product UI may compose shared primitives but must not fork the family palette or redefine their basic interaction semantics.

## Shell layer: `apps/*`

Applications/plugins own:

- top-level editor lifetime;
- VST3/Standalone bootstrap;
- shell composition;
- wiring product UI to processor/application services;
- host-specific integration.

Applications must not become a fourth styling layer. New reusable controls discovered in `apps/*` must move down to `libs/ui` or `libs/vox-ui` according to ownership.

## Product boundary rule

### Vox Electronic Engine

Owns electronic/synth-oriented instruments and workflows such as:

- Psy Bass;
- Acid;
- Lead;
- Atmos / Texture;
- FX-oriented electronic sound modules;
- synth performance and sequencing tools.

Arpeggiator, phrase and sequence editors are capabilities of a selected synth/Part, not standalone rack instruments unless a future product requirement explicitly defines a new instrument type.

### Vox Drums Engine

Is a separate plugin/engine. It owns drum-specific workflows such as:

- drum pads;
- kit/pad bank;
- sample waveform/editor;
- velocity editor;
- choke groups;
- per-pad routing;
- multi-row drum sequencer.

A Drums mock-up may be used as a family-style reference, but it is not an Electronic Engine rack instrument.

### Mastering Engine

Is a separate plugin/engine. It owns:

- spectrum analyzer;
- loudness meter;
- true-peak meter;
- gain-reduction meter;
- stereo/correlation visualization;
- module chain;
- A/B and reference controls.

Mastering UI may be visually calmer and less accent-heavy while still consuming the same family tokens and state grammar.

## Component promotion rule

When implementing a new product widget:

1. If it is generic and family-wide, implement it in `libs/vox-ui`.
2. If it carries Electronic Engine semantics, implement it in `libs/ui`.
3. If it only composes an application shell, keep it in `apps/*`.
4. Never duplicate an existing primitive merely to obtain a visual variation.
5. If a new visual requirement affects a shared primitive, update the design system first.

## Long-term distribution

When a second plugin actively consumes the shared layer, `libs/vox-ui` may move to a dedicated shared repository/library rather than being copied.

Recommended target repository name:

```text
ultima-vox/vox-ui
```

Consumers should pin a known version/tag or commit. Release builds must not depend on an unpinned moving branch.

Suggested versioning:

```text
v1.x  backward-compatible additions and visual refinements
v2.x  breaking component/API or design-language changes
```

## Migration rule

Existing product UI does not require a big-bang rewrite.

Migration order:

1. tokens;
2. typography;
3. LookAndFeel;
4. knob/button/combo/slider primitives;
5. panels/headers;
6. graphs/meters;
7. product widgets;
8. product pages;
9. shell cleanup;
10. remove legacy local styling after parity is confirmed.

Every migration step must preserve plugin state, automation IDs, parameter behavior and DSP behavior.

## Acceptance criteria

A plugin belongs to the VOX UI family when:

- shared visual primitives come from `libs/vox-ui`;
- product workflow components live in the product UI layer;
- application shells do not contain private styling systems;
- no private duplicate palette exists;
- common controls have identical interaction semantics;
- resize/scaling works at supported scale factors;
- specialized widgets visually derive from shared tokens;
- UI changes do not alter realtime DSP safety;
- product-specific deviations are documented rather than silently forked;
- no new `Source/UI` architecture is introduced.
