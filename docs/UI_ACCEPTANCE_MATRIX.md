# VOX UI — Production Acceptance Matrix v1.0

**Status:** CANON candidate for DS-0 freeze  
**Purpose:** prevent product code from becoming the place where missing design-system decisions are discovered.

A component is not `Production Ready` because it compiles. It must satisfy the applicable rows below.

---

## 1. Status vocabulary

```text
Concept       specified only
Prototype     code exists; acceptance incomplete
Implemented   builds and basic behaviour verified
Production    full applicable acceptance passed
Deferred      intentionally outside current product/release scope
```

`Implemented` and `Production` are intentionally different.

---

## 2. Universal component acceptance

Every shared interactive component must be checked for:

| Area | Required |
|---|---|
| canonical tokens only | yes |
| no product DSP dependency | yes |
| no PluginProcessor dependency | yes |
| normal state | yes |
| hover state | if pointer-interactive |
| pressed state | if actionable |
| focused state | if focusable |
| active/selected state | if semantic |
| disabled state | if controllable |
| read-only/unavailable state | if semantic |
| long label/value | yes |
| minimum useful size | yes |
| 75% scale | yes |
| 100% scale | yes |
| 125% scale | yes |
| 150% scale | yes |
| 200% scale | yes |
| keyboard operation | if interactive |
| accessibility role/name/value | where supported |
| state not colour-only | yes |
| tooltip/help for non-obvious action | where needed |
| no paint-time DSP access | yes |
| no realtime allocation introduced | yes |
| showcase example | yes |
| screenshot baseline | production controls |

---

## 3. Shared primitive matrix

The DS v1.0 target inventory includes:

```text
VoxPanel
VoxDivider
VoxLabel
VoxParameterLabel
VoxValueField
VoxTextInput
VoxNumericInput
VoxSearchField
VoxButton
VoxIconButton
VoxToggle
VoxSwitch
VoxCheckBox
VoxRadioGroup
VoxSegmentedControl
VoxComboBox
VoxTabBar
VoxKnob
VoxLinearSlider
VoxRangeSlider
VoxFader
VoxStatusIndicator
VoxBadge
VoxProgress
VoxSpinner
VoxScrollBar
VoxTooltip
VoxPopup/Popover
VoxContextMenu
VoxDialog
VoxConfirmationDialog
VoxToast
VoxBanner
```

A product may ship before every item is implemented only if unused items are explicitly `Deferred`, not silently absent.

---

## 4. Layout primitive matrix

The design system must define and test the following concepts, implemented as Components or shared helpers as appropriate:

```text
VoxStack
VoxRow
VoxGrid
VoxSplitView
VoxScrollArea
VoxDivider
VoxCollapsibleSection
VoxResizablePane
VoxInspectorPane
VoxToolbar
VoxSpacer
```

Acceptance:

- tokenized gaps/padding;
- no clipping at supported scales;
- deterministic resize;
- keyboard focus order remains logical;
- local scrolling only where intended.

---

## 5. Audio primitive matrix

Target shared audio primitives:

```text
VoxGraph
VoxWaveform
VoxSpectrumAnalyzer
VoxMeter / VoxLevelMeter
VoxEnvelopeEditor
VoxFilterResponse
VoxXYPad
VoxModulationGraph
VoxMacroControl
VoxModulationRing
VoxKeyboard
VoxStereoScope
VoxCorrelationMeter
VoxTransferCurve
```

Additional acceptance:

- real data only;
- no fake activity;
- safe UI snapshot contract;
- repaint budget defined;
- static grid/background cached where useful;
- labels degrade gracefully at small sizes;
- stale/no-data state defined;
- editable graphs support explicit selection/focus handles.

---

## 6. Graph-language acceptance

Every specialized graph derives from the shared graph grammar and defines:

```text
plot area
grid major/minor
axis policy
label policy
curve/trace style
fill policy
marker/node style
hover/readout policy
selection policy
no-data state
error/unavailable state
multi-trace policy if supported
```

Filter, waveform, envelope, modulation, analyzer and transfer-curve renderers must not independently invent these basics.

---

## 7. Modulation acceptance

Before a synth product is Production-ready:

- source indication exists;
- destination indication exists;
- bipolar/unipolar semantics are clear;
- positive and negative depth are representable;
- multiple routes are inspectable;
- direct assignment behaviour is specified if supported;
- route remove/edit behaviour is specified;
- knob/range visualization and matrix show the same route state;
- automation and modulation remain visually distinct.

---

## 8. Preset/browser acceptance

Required states/tests:

```text
Factory
User
Clean
Dirty
Loading
Saving
ReadOnly
SearchEmpty
Missing
MigrationRequired
LoadError
SaveError
OverwriteConfirmation
DeleteConfirmation
```

Long names, search/filter ordering and previous/next behaviour require deterministic tests or documented manual acceptance.

---

## 9. Drag/drop and selection acceptance

Applicable widgets must demonstrate:

- source drag state;
- insertion marker;
- valid target;
- invalid target;
- cancel;
- selection distinct from hover;
- multi/range/lasso semantics where supported;
- keyboard alternative for essential operations where practical.

---

## 10. Timeline/editor acceptance

Piano roll, sequencer, automation and sample timeline must define and verify:

```text
ruler
grid/snap
playhead
loop range
selection
move
resize
delete
zoom
scroll
fit
follow playhead
lane header
lane resize if supported
```

Musical time must not be distorted merely to fit a narrow panel.

---

## 11. Realtime/performance acceptance

Baseline UI budgets:

```text
meters             target <= 60 Hz repaint/update
spectrum/scope     normally 30–60 Hz based on cost
slow graphs        normally 15–30 Hz
static graphs      repaint on state/data change
```

Rules:

- no blocking audio-thread lock from UI;
- no mutable realtime structure read from paint;
- no disk I/O in realtime path;
- avoid repeated allocation in hot paint/update paths;
- cache static paths, grids and vector assets where useful;
- no full-editor repaint when only one meter/trace changed.

These are family budgets; a component may run slower when sufficient, not faster by default without evidence.

---

## 12. Page/workspace acceptance

Every release page/workspace requires screenshots at:

```text
1440×1080 @100%
1440×1080 @125%
minimum supported editor size
```

Plus manual/automated checks at 75/150/200%.

Pass criteria:

- hierarchy matches visual reference;
- no clipping/overlap;
- values readable;
- primary actions reachable;
- no default JUCE styling leaks;
- no fake/unsupported controls;
- keyboard focus order sensible;
- empty/error/unavailable states present;
- resize does not change product semantics.

---

## 13. Showcase requirements

`vox_ui_showcase` is the live component laboratory for the design system.

It must eventually provide sections for:

```text
Foundations
Typography
Icons
Buttons
Selection controls
Knobs/sliders/faders
Inputs/value fields
Tabs/navigation
Panels/layout
Menus/popovers/dialogs
Feedback/status
Graphs
Meters
Envelope
Filter response
Waveform
Modulation
Keyboard/performance
Lists/tables/tree
Drag/drop
Accessibility/focus
Scaling/responsive
Error/empty/loading states
```

The showcase may be built incrementally, but missing sections remain visible in the acceptance backlog.

---

## 14. Screenshot naming

Recommended baseline naming:

```text
<component-or-page>__<state>__<scale>.png

vox-knob__states__100.png
vox-filter-response__default__125.png
sound-page-psy-bass__default__100.png
preset-browser__search-empty__100.png
```

---

## 15. DS-0 freeze gate

Design System v1.0 can be declared frozen only when:

- component inventory is complete for known family workflows;
- interaction/state specification is accepted;
- layout/responsive specification is accepted;
- accessibility specification is accepted;
- token gaps are enumerated;
- icon grammar is defined;
- graph/realtime contracts are defined;
- acceptance matrix is accepted;
- unimplemented items have explicit roadmap status rather than undefined behaviour.

Freeze means the contracts are fixed. It does **not** require every component to already be coded.
