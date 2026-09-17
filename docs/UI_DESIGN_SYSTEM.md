# VOX ELECTRONIC ENGINE — UI DESIGN SYSTEM v1.1

**Status:** CANON / mandatory specification  
**Scope:** VOX family visual foundation + Electronic Engine implementation rules  
**Repository:** `ultima-vox/vox-electronic-engine`  
**Version:** 1.1

This document is authoritative for new UI work in the repository. `VOX_UI_FAMILY_ARCHITECTURE.md` defines ownership boundaries; `UI_COMPONENT_CATALOG.md` defines the component inventory; `UI_VISUAL_REFERENCE_SPEC.md` defines reference-screen visual intent.

---

## 1. Product direction

Visual language:

**Dark futuristic / professional electronic-music workstation**

Required qualities:

- deep dark-blue/black base;
- cold cyan as the primary accent;
- high information density without clutter;
- thin borders and restrained radii;
- precise visual hierarchy;
- minimal decorative noise;
- glow only for meaningful emphasis;
- commercial audio-software appearance;
- deterministic layout and interaction;
- no generic JUCE-demo appearance;
- no game-HUD styling.

The UI must prioritize control readability, parameter state, active workflow and realtime feedback over decoration.

---

## 2. Canonical architecture

The production architecture is:

```text
apps/*
  plugin / standalone shell
        |
        v
libs/ui/*
  Vox Electronic Engine product UI
        |
        v
libs/vox-ui/*
  shared VOX design system
        |
        v
JUCE GUI
```

### `libs/vox-ui`

Owns family-wide primitives only:

- tokens;
- typography;
- `VoxLookAndFeel`;
- primitive controls;
- generic graphs/meters;
- interaction/state grammar;
- shared layout constants.

### `libs/ui`

Owns Electronic Engine semantics:

- instrument/Part rack;
- synth panels;
- product navigation;
- preset workflows;
- Sound/Pattern/Routing/Zones/Macros/Advanced pages;
- sequencers, modulation matrix and other Electronic Engine workflow widgets.

### `apps/*`

Owns shell composition, editor lifetime, plugin/standalone bootstrap and wiring.

### Forbidden legacy architecture

Do not introduce new UI code under:

```text
Source/UI/
```

Any older instruction that recommends `Source/UI/Design`, `Source/UI/Components`, `Source/UI/Pages` or similar is obsolete.

---

## 3. Design-token authority

All reusable visual values must come from:

```text
libs/vox-ui/Tokens.h
libs/vox-ui/Typography.h
```

Local product code must not define a second palette, spacing scale, radius system, focus style or interaction timing system.

A new reusable value must first be promoted into the shared tokens with semantic naming.

Token families:

```text
colour
spacing
radius
stroke
opacity
size
focus
graph
meter
interaction
elevation
```

---

## 4. Colour system

Canonical semantic palette:

```text
background        #06121D
panel             #0B1925
panelRaised       #102230
control           #0A1722
controlHover      #0E2130
graph             #051019
overlay           #020910 @ 80%

border            #1D3B50
borderSubtle      #122C3D
borderStrong      #2B5873

accent            #00DDF5
accentHover       #42EEFF
accentDim         #087B92
accentMuted       #0B4655

text              #E9F3FA
textSecondary     #9AB2C5
textMuted         #587487
textInverse       #06121D

danger            #E4425D
success           #32D296
warning           #E3B341
```

### Accent rules

`accent` is reserved for meaningful state:

- active/selected state;
- current value;
- focus ring;
- active modulation;
- current transport/sequence position;
- selected Part/instrument;
- realtime indicator requiring attention.

Do not use cyan as a generic panel fill or decorative outline on every component.

### Status colour rules

`danger`, `warning`, `success` communicate semantic state. They must not be used as arbitrary decoration.

Critical states must also include shape, label, icon or text indication; colour alone is insufficient.

---

## 5. Spacing and grid

Base spatial unit:

```text
4 px
```

Canonical scale:

```text
4 / 8 / 12 / 16 / 20 / 24 / 32 / 40
```

Semantic defaults:

```text
page padding        16 px
panel padding       12 px
section gap         12 px
control gap          8 px
```

All primary layout geometry should follow the 4 px grid unless there is a technical rendering reason not to.

Do not use arbitrary values such as 13, 17, 23 or 29 px for structural spacing.

---

## 6. Radius system

```text
small      4 px
medium     6 px
large      8 px
```

Default panel radius:

```text
6 px
```

Controls should remain compact and technical rather than pill-shaped.

---

## 7. Stroke system

```text
hairline       1.0 px
control        1.0 px
focus          1.5 px
graph          1.75 px
graphStrong    2.0 px
icon           1.75 px
```

Stroke weight is part of the family identity. Product components must not invent unrelated line weights.

---

## 8. Opacity system

Canonical semantic values:

```text
disabled       0.35
muted          0.60
secondary      0.78
grid           0.42
hover overlay  0.08
pressed        0.14
glow           0.22
full           1.00
```

Disabled state must remain legible but clearly inactive.

---

## 9. Typography

Canonical typography is defined in `libs/vox-ui/Typography.h`.

Primary hierarchy:

```text
Instrument Title    26 px  SemiBold
Section Title       14 px  SemiBold / uppercase where appropriate
Control Label       12 px  Regular/Medium
Value / Secondary   11 px  Regular
```

Rules:

- do not introduce random font sizes;
- do not exceed four primary hierarchy levels without a design-system revision;
- titles use `text`;
- labels generally use `textSecondary`;
- disabled/muted metadata uses `textMuted`;
- numeric values must remain readable at every supported scale.

---

## 10. Reference canvas and scale

Reference canvas:

```text
1440 × 1080
```

Supported scale targets:

```text
75%
100%
125%
150%
200%
```

Layouts must remain valid under resize and UI scaling.

The UI must not be implemented as one global collection of hardcoded `setBounds(x, y, w, h)` calls.

Relative rectangles, reusable layout helpers, nested layout regions or equivalent deterministic layout techniques are required.

---

## 11. Shared size tokens

Canonical baseline geometry:

```text
control height              32 px
compact control height      28 px
tab height                  36 px
top bar height              48 px
Part header height          56 px
keyboard min height        112 px
keyboard preferred height  132 px

knob small                  36 px
knob normal                 48 px
knob large                  64 px

instrument slot             54–58 px
icon small                  12 px
icon normal                 16 px
icon large                  20 px
minimum hit target          28 px
```

A visual control may be smaller than its hit region only if the interaction target still respects the minimum hit-target rule.

---

## 12. Component ownership levels

### Level A — shared primitives (`libs/vox-ui`)

Examples:

```text
VoxPanel
VoxKnob
VoxLinearSlider
VoxFader
VoxButton
VoxIconButton
VoxToggle
VoxSwitch
VoxSegmentedControl
VoxComboBox
VoxTextInput
VoxValueField
VoxLabel
VoxSectionHeader
VoxTabBar
VoxStatusIndicator
VoxTooltip
VoxDivider
```

### Level B — shared audio primitives (`libs/vox-ui` when generic)

```text
VoxWaveform
VoxSpectrumAnalyzer
VoxMeter
VoxEnvelopeEditor
VoxFilterResponse
VoxXYPad
VoxModulationGraph
VoxKeyboard
VoxMacroControl
VoxModulationRing
VoxStereoScope
VoxCorrelationMeter
VoxTransferCurve
```

### Level C — product workflow widgets (`libs/ui`)

```text
instrument rack
preset browser workflow
piano roll
step sequencer
arpeggiator
modulation matrix
routing matrix
zone editor
effect chain
```

### Level D — pages (`libs/ui`)

```text
Sound
Pattern
Routing
Zones
Macros
Advanced
FX
Mixer-like product workspaces where required
```

Pages compose components; they must not invent private primitive styles.

---

## 13. Global component state grammar

Every interactive component supports the subset of these states that applies to it:

```text
Normal
Hover
Pressed
Focused
Active / Selected
Disabled
Modulated
Automated
Error / Warning where semantically required
```

Base state mapping:

```text
Normal
  background = control/panel as appropriate
  border     = border
  text       = text or textSecondary

Hover
  background = controlHover or subtle hover overlay
  border     = accentDim

Pressed
  background = control + pressed overlay
  border     = accentDim or accent

Focused
  visible focus ring = focusRing
  focus must remain distinguishable from hover

Active / Selected
  accent is allowed as primary state indicator
  selected state must remain obvious without hover

Disabled
  opacity = disabled
  text    = textMuted
  no active-looking accent

Modulated
  modulation indication is visually separate from base parameter value

Automated
  automation state must not replace the value indication
```

No product page may redefine these meanings locally.

---

## 14. Interaction tokens

Canonical interaction values:

```text
fine-adjust multiplier      0.10
wheel-step multiplier       0.02
tooltip delay              650 ms
hover transition            90 ms
state transition           120 ms
page transition            140 ms
```

Animation is optional where JUCE/runtime constraints make it undesirable, but state meaning and final appearance are mandatory.

Animations must never run on the audio thread.

---

## 15. Parameter interaction contract

For continuous parameter controls:

```text
Drag             normal adjustment
Shift + Drag     fine adjustment
Double Click     reset to parameter default
Mouse Wheel      optional globally-consistent adjustment
Right Click      context/MIDI Learn only where implemented
```

Rules:

- drag mode/sensitivity is consistent across all knobs;
- reset uses the real parameter default;
- textual value is available;
- parameter state has one source of truth;
- controls bind to processor/model state through safe mechanisms.

Where `AudioProcessorValueTreeState` is used, standard attachments are preferred.

---

## 16. Focus and keyboard contract

Keyboard focus must be visible and distinct from hover.

Canonical focus treatment:

```text
colour     accent
stroke     1.5 px
inset      1 px
gap         1 px
```

Focus must not rely on glow alone.

Popup/select controls should support Escape-to-close and keyboard navigation where technically practical.

---

## 17. Icons

Icons are vector-based and visually uniform.

Preferred implementation:

```text
SVG / juce::Drawable / deterministic JUCE path
```

Canonical icon stroke:

```text
1.75 px
```

Forbidden:

- emoji;
- platform-font glyphs used as arbitrary icons;
- mixed icon families;
- inconsistent stroke weight.

---

## 18. VoxPanel

Standard panel:

```text
background  panel
border      1 px border
radius      6 px
padding     12 px
```

Raised panels may use `panelRaised` only when hierarchy requires it.

Panels must not become decorative cards with heavy shadows.

---

## 19. VoxKnob

Canonical sizes:

```text
Small    36 × 36
Normal   48 × 48
Large    64 × 64
```

Arc geometry:

```text
270° sweep
135° -> 405°
```

Visual layers:

```text
inactive arc
active value arc
central body
indicator
label
value text
optional modulation ring
optional automation indication
```

Colours:

```text
inactive arc   border
active arc     accent
indicator      text
modulation     accentHover / dedicated secondary layer
```

The modulation ring must not obscure the base parameter value.

Do not create instrument-specific knob skins.

---

## 20. Buttons / toggles / selectors

### Button variants

```text
Primary
Secondary
Toggle
Danger
Icon
```

Default height:

```text
32 px
```

Primary actions use accent sparingly. Secondary controls use dark surfaces and normal text hierarchy. Danger is reserved for destructive/critical actions such as Panic or destructive reset.

### ComboBox / selector

```text
height      32 px
background  control
border      border
radius      4 px
padding      8 px
```

### TabBar

```text
height 36 px
```

Active tabs must be obvious without hover. A full cyan fill is allowed when contrast remains appropriate; an accent indicator + raised active surface is also acceptable if globally standardized.

---

## 21. Graph language

Generic graph surface:

```text
background       graph
grid             graphGrid @ grid opacity
curve            graphCurve
curve width      1.75 px
strong curve     2.0 px
node diameter    7 px
node hit area   16 px
```

Rules:

- graphs represent actual state;
- no fake realtime animation;
- decorative gradients must remain subtle;
- graph fill must not reduce curve readability;
- interaction nodes use larger invisible hit regions than their visible dots.

---

## 22. Meter language

Meter semantics use:

```text
low       success
mid       warning
high      danger
track     meterTrack
```

Baseline geometry:

```text
minimum width     6 px
preferred width   8 px
peak hold       900 ms
peak fall      1400 ms
```

Meter ranges and thresholds must follow the actual signal domain; colour thresholds are not substitutes for correct audio metering logic.

---

## 23. Keyboard

Base component:

```text
VoxKeyboard
```

`juce::MidiKeyboardComponent` may be used as a functional foundation, but appearance must conform to VOX UI.

Reference key colours:

```text
white     #EBEEF0
black     #081018
pressed   accent
```

Keyboard height target:

```text
112–132 px at 100% reference scale
```

Octave labels may be shown where useful.

---

## 24. Electronic Engine layout contract

Top-level product composition:

```text
PluginEditor / Standalone shell
|
+-- GlobalHeader
+-- InstrumentRack / Part selector
+-- PartHeader
+-- MainNavigation
+-- ActivePage
`-- optional Keyboard / performance strip
```

Typical Sound page composition:

```text
+----------------+----------------+----------------+
| Oscillator     | Filter         | Envelope       |
+----------------+----------------+----------------+
| Drive          | Accent         | Performance    |
+-------------------------+-------------------------+
| Modulation              | Matrix                  |
+---------------------------------------------------+
| Keyboard / Performance                            |
+---------------------------------------------------+
```

The exact responsive arrangement may change with available width, but section priority and component identity must remain stable.

---

## 25. Responsive degradation order

When space becomes constrained, adapt in this order:

1. reduce flexible whitespace to token minimums;
2. reduce optional metadata density;
3. reflow secondary controls within panels;
4. stack secondary panels;
5. allow deliberate scrolling in a defined content region;
6. hide only explicitly optional secondary information.

Never:

- clip primary parameter controls;
- overlap controls;
- shrink text below readable hierarchy;
- hide active/critical state;
- silently remove essential functionality.

---

## 26. Realtime safety

UI must never:

- perform heavy work in the audio callback;
- take blocking locks on the audio thread;
- allocate memory in realtime paths for rendering purposes;
- perform disk I/O from the audio thread;
- directly read unsafe mutable DSP state.

Use safe bridges such as:

```text
atomics
lock-free snapshots
message-thread updates
timer-driven polling
bounded queues where appropriate
```

Visual state must never compromise audio stability.

---

## 27. Parameter-state authority

UI must not keep an independent duplicate copy of DSP parameter truth.

Graphs and labels should derive from the same canonical model/parameter state as their corresponding controls.

A graph that visually disagrees with the audible parameter state is a defect.

---

## 28. Accessibility and usability

Required:

- every non-obvious control has a tooltip;
- parameter controls expose text values;
- critical states are not colour-only;
- minimum hit target is respected;
- disabled state is visually obvious;
- focus is visible;
- knob drag/fine-adjust/reset semantics are global;
- popup/select behavior is consistent;
- labels do not truncate silently when they communicate critical state.

---

## 29. Elevation and overlays

Elevation levels:

```text
0 base
1 raised
2 overlay
3 modal
```

These are hierarchy levels, not permission for Material-style heavy shadows.

Use elevation primarily through surface value, border strength and controlled overlay treatment.

---

## 30. Forbidden practices

Do not:

- add local hardcoded colours when a semantic token exists;
- create a second palette inside `libs/ui` or `apps/*`;
- create different knob skins for Bass/Lead/Acid/Atmos;
- use emoji as icons;
- build the complete UI from absolute coordinates;
- introduce random font sizes;
- introduce random radii or spacing values;
- use glow everywhere;
- use heavy decorative gradients;
- copy default JUCE appearance unchanged;
- mix DSP algorithms with UI painting code;
- duplicate parameter truth;
- perform GUI work in the realtime thread;
- create shared primitives inside a product page;
- place new canonical UI architecture under `Source/UI`;
- change family visual language in one instrument without a design-system revision.

---

## 31. New-component promotion rule

If a required visual control does not exist:

1. classify it as family primitive, family audio primitive, product widget or page;
2. implement it in the correct ownership layer;
3. add or extend tokens first if new reusable visual semantics are required;
4. document the contract;
5. then consume it in product UI.

New UI code extends the system; it does not bypass the system.

---

## 32. Definition of Done — shared component

A shared component is complete when:

- it uses semantic tokens;
- it has documented geometry;
- applicable states are implemented;
- focus is visible;
- disabled state is correct;
- resize/scale behavior is deterministic;
- no private palette exists;
- interaction follows global contracts;
- parameter components expose readable value text;
- realtime safety is preserved;
- screenshot/deterministic rendering coverage exists where project infrastructure permits.

---

## 33. Definition of Done — product widget/page

A product widget/page is complete when:

- it composes VOX components instead of duplicating them;
- layout follows token spacing;
- it works at 75/100/125/150/200%;
- there is no visual overflow or control overlap;
- all controls bind to real functionality or are explicitly marked unavailable;
- graphs reflect real state;
- active/disabled/focus states are correct;
- no local theme fork exists;
- application/DSP behavior is unchanged unless explicitly part of the task.

---

## 34. UI implementation phases

### DS-1 — Architecture cleanup

Complete when:

- `libs/vox-ui` is the only family design-system source;
- `libs/ui` owns Electronic Engine UI;
- `apps/*` remains shell-only;
- `Source/UI` is forbidden for new work;
- docs agree on dependency direction.

### DS-2 — Foundation tokens

Complete when token families cover:

```text
colour
spacing
radius
stroke
opacity
size
focus
graph
meter
interaction
elevation
```

### DS-3 — Component state matrix

Define exact states for:

```text
VoxKnob
VoxLinearSlider
VoxButton
VoxToggle
VoxComboBox
VoxTabBar
VoxTextInput
VoxIconButton
```

### DS-4 — Layout/grid

Finalize responsive shell, page/panel contracts and degradation behavior.

### DS-5 — Interaction/input

Finalize mouse, wheel, keyboard, focus, context-menu and MIDI-learn conventions.

### DS-6 — Audio visualizations

Finalize graph, analyzer, meter, modulation and envelope language.

### DS-7 — Product patterns

Finalize rack, preset, synth-panel, sequencer, routing and zone patterns.

### DS-8 — Showcase and visual acceptance

Create a deterministic component showcase/reference scene and use it for visual acceptance.

---

## 35. Agent implementation contract

For every UI change, an implementation agent must:

1. read this document and `VOX_UI_FAMILY_ARCHITECTURE.md` first;
2. determine ownership before creating a new component;
3. reuse existing VOX primitives;
4. add new semantic tokens before hardcoding reusable visual values;
5. avoid unrelated DSP refactors;
6. report changed tokens/components/pages in the PR;
7. report tested scale factors and states;
8. provide a screenshot/render for meaningful visual changes;
9. document any deliberate deviation from CANON;
10. never silently fork the visual language.

---

## 36. Foundation acceptance criteria v1.1

DS-1 + DS-2 are complete when:

- canonical architecture is `apps/* -> libs/ui -> libs/vox-ui`;
- `Source/UI` is explicitly non-canonical;
- shared tokens live in one location;
- semantic tokens exist for colour, spacing, radius, stroke, opacity, size, focus, graph, meter, interaction and elevation;
- product UI has no authority to invent a second family theme;
- documentation and code use the same ownership model;
- the next design-system work can proceed through component state contracts rather than architectural cleanup.

---

## 37. Canonical rule

> Shared visual semantics live in `libs/vox-ui`. Electronic Engine workflows live in `libs/ui`. Application shells compose them. New UI must extend this hierarchy, never bypass it.
