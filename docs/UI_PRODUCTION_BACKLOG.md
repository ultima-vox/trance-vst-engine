# VOX UI — Production Backlog v1.0

**Status:** DS-0 working backlog  
**Purpose:** track every known VOX design-system capability from contract to Production acceptance.  
**Source of truth for status:** this file + `UI_ACCEPTANCE_MATRIX.md`.

Status vocabulary:

```text
Concept       specified only
Prototype     code exists; acceptance incomplete
Implemented   builds and basic behaviour verified
Production    full applicable acceptance passed
Deferred      intentionally outside current release scope
```

No component becomes `Production` from source-code existence alone.

---

## 1. Current snapshot

Shared VOX UI currently has code for:

```text
Tokens / Typography / VoxLookAndFeel
VoxPanel
VoxKnob
VoxButton
VoxComboBox
VoxTabBar
VoxSectionHeader
vox_ui_showcase
```

Until UI-1 CI + showcase + screenshot acceptance are complete, these remain **Prototype**.

Existing Electronic Engine product widgets under `libs/ui` are also treated as **Prototype** for DS purposes until they are migrated to the frozen contracts and pass page/workspace acceptance. Existing code is not evidence that the family primitive beneath it is complete.

---

## 2. Phase map

```text
DS-0   freeze contracts + inventory + status map
UI-1   foundation primitives + Storybook baseline
UI-2   graph foundation + filter response
UI-3   oscillator / waveform primitives
UI-4   envelope / editable graph primitives
UI-5   modulation interaction system
UI-6   meters + realtime visualisation
UI-7   inputs / lists / preset-browser primitives
UI-8   timeline / musical editor primitives
UI-9   drag/drop / routing / matrix / structured selection
UI-10  shell + page composition + production acceptance
```

Product code may consume a Prototype during development, but release acceptance requires every release-critical dependency to be `Production` or explicitly waived/deferred.

---

## 3. Foundations and infrastructure

| Capability | Ownership | Status | Phase | Required to reach Production |
|---|---|---:|---:|---|
| semantic colour tokens | vox-ui | Prototype | UI-1 | showcase palette + screenshot baseline |
| spacing/radius tokens | vox-ui | Prototype | UI-1 | scale checks 75–200% |
| opacity/stroke/icon/layout/graph/motion tokens | vox-ui | Prototype | DS-0/UI-1 | implementation usage audit |
| Typography factories | vox-ui | Prototype | UI-1 | long text, scale, fallback checks |
| VoxLookAndFeel | vox-ui | Prototype | UI-1 | all canonical draw paths + state screenshots |
| icon grammar | vox-ui | Concept | UI-1 | registry + canonical vector set + states |
| VoxIconRegistry | vox-ui | Concept | UI-1 | production glyph vocabulary |
| responsive breakpoint helpers | vox-ui/helper | Concept | UI-1 | Compact/Normal tests |
| density/scale helpers | vox-ui/helper | Concept | UI-1 | deterministic scale tests |
| safe visual snapshot contracts | vox-ui contract | Concept | UI-2/UI-6 | graph/meter consumers prove contract |
| Storybook/showcase navigation | vox-ui | Prototype | UI-1 | sections for every implemented primitive |
| visual regression baseline process | QA | Concept | UI-1 | named screenshots + review workflow |

### UI-1 exit

UI-1 is Production-ready only when:

```text
build green
full ctest green
showcase launches
1440x1080 @100% reviewed
1440x1080 @125% reviewed
75/150/200% checked
focus/disabled/long-text states demonstrated
no default JUCE visual leaks
```

---

## 4. Core primitives

| Component | Current | Phase | Notes |
|---|---:|---:|---|
| VoxPanel | Prototype | UI-1 | code exists; acceptance pending |
| VoxSectionHeader | Prototype | UI-1 | code exists; action/power/icon states need acceptance |
| VoxKnob | Prototype | UI-1 | code exists; reset/modulation/focus/scales need acceptance |
| VoxButton | Prototype | UI-1 | Primary/Secondary/Toggle/Danger/Icon |
| VoxIconButton | Prototype via VoxButton variant | UI-1 | separate class not required unless API pressure appears |
| VoxComboBox | Prototype | UI-1 | JUCE behaviour + VOX LnF |
| VoxTabBar | Prototype | UI-1 | keyboard/focus/disabled acceptance pending |
| VoxDivider | Concept | UI-1 | can be helper/light component |
| VoxLabel | Concept | UI-1 | canonical text roles |
| VoxParameterLabel | Concept | UI-1 | parameter name/unit grammar |
| VoxValueField | Concept | UI-1 | display/edit policy |
| VoxToggle | Concept | UI-1 | wrapper only if JUCE ToggleButton is insufficient |
| VoxSwitch | Concept | UI-1 | binary compact semantic control |
| VoxCheckBox | Concept | UI-1 | settings/list use |
| VoxRadioGroup | Concept | UI-1 | mutually exclusive choices |
| VoxSegmentedControl | Concept | UI-1 | compact mode choice |
| VoxLinearSlider | Concept | UI-1 | horizontal/vertical grammar |
| VoxRangeSlider | Concept | UI-1 | range/key/velocity use |
| VoxFader | Concept | UI-1/UI-6 | dB scale, unity, optional meter |
| VoxScrollBar | Concept | UI-1 | shared styling, no default JUCE leak |

**Priority:** finish this table before product panels introduce private alternatives.

---

## 5. Text and input primitives

| Component | Current | Phase | Production requirements |
|---|---:|---:|---|
| VoxTextInput | Concept | UI-7 | focus, selection, validation, disabled/read-only |
| VoxNumericInput | Concept | UI-7 | keyboard entry, drag/wheel policy, unit handling |
| VoxSearchField | Concept | UI-7 | clear action, empty/searching/error states |
| VoxChoiceControl grammar | Concept | UI-7 | Combo/segment/popup selection consistency |
| editable parameter value | Concept | UI-7 | commit/cancel/reset/error behaviour |

---

## 6. Navigation, overlays and feedback

| Component | Current | Phase | Notes |
|---|---:|---:|---|
| VoxTooltip | Concept | UI-7 | delay, placement, keyboard/accessibility |
| VoxPopover | Concept | UI-7 | anchored transient surface |
| VoxContextMenu | Prototype via LnF only | UI-7 | common item grammar required |
| VoxMenuItem grammar | Concept | UI-7 | icon/check/submenu/shortcut/destructive |
| VoxDialog | Concept | UI-7 | focus trap + Escape + restore focus |
| VoxConfirmationDialog | Concept | UI-7 | destructive/non-destructive patterns |
| VoxToast | Concept | UI-7 | transient success/error/info |
| VoxBanner | Concept | UI-7 | persistent warning/error/info |
| VoxStatusIndicator | Concept | UI-6/UI-7 | MIDI/CPU/online/activity |
| VoxBadge | Concept | UI-7 | compact semantic status |
| VoxProgress | Concept | UI-7 | determinate/indeterminate |
| VoxSpinner | Concept | UI-7 | loading only; restrained motion |
| VoxEmptyState | Concept | UI-7 | title/message/action |
| VoxLoadingState | Concept | UI-7 | no fake progress |
| VoxErrorState | Concept | UI-7 | retry/action optional |
| VoxUnavailableState | Concept | UI-7 | unavailable != disabled |

---

## 7. Layout system

These may be Components or helpers. The contract matters more than inheritance.

| Concept | Current | Phase | Required |
|---|---:|---:|---|
| VoxStack | Concept | UI-1 | tokenized vertical/horizontal gaps |
| VoxRow | Concept | UI-1 | deterministic resize |
| VoxGrid | Concept | UI-1 | responsive columns |
| VoxSplitView | Concept | UI-9 | splitter/min/max constraints |
| VoxScrollArea | Concept | UI-7 | local scrolling policy |
| VoxCollapsibleSection | Concept | UI-7 | keyboard + persisted product state only where intended |
| VoxResizablePane | Concept | UI-9 | handles + minimums |
| VoxInspectorPane | Concept | UI-9 | structured detail pane |
| VoxToolbar | Concept | UI-7 | compact action grammar |
| VoxSpacer | Concept/helper | UI-1 | layout-only |

---

## 8. Graph and audio-visualisation foundation

### UI-2 — mandatory graph foundation

| Component/capability | Current | Phase | Release priority |
|---|---:|---:|---:|
| VoxGraph | Concept | UI-2a | P0 |
| VoxFilterGraph / VoxFilterResponse | Concept | UI-2a/UI-2b | P0 |
| plot area / major+minor grid | Concept | UI-2a | P0 |
| axes + labels | Concept | UI-2a | P0 |
| trace + filled trace | Concept | UI-2a | P0 |
| markers/nodes | Concept | UI-2/UI-4 | P1 |
| hover cursor/readout | Concept | UI-2 | P1 |
| no-data/unavailable states | Concept | UI-2 | P0 |
| multi-trace legend grammar | Concept | UI-6 | P1 |
| GraphDataSnapshot contract | Concept | UI-2 | P0 |

UI-2a remains intentionally product-independent. `vox_ui` must not depend on PsyBass/APVTS/PluginProcessor.

### UI-3 / UI-4 / UI-6 audio primitives

| Component | Current | Phase | Notes |
|---|---:|---:|---|
| VoxWaveform | Concept | UI-3 | oscillator/sample display base |
| VoxWaveformSelector | Concept/product composition | UI-3 | selector grammar shared; semantics product-specific |
| VoxEnvelopeEditor | Concept | UI-4 | ADSR+optional nodes; keyboard/edit handles |
| VoxModulationGraph | Concept | UI-5 | ENV/LFO/step/random common renderer |
| VoxMeter | Concept | UI-6 | generic meter base |
| VoxLevelMeter | Concept | UI-6 | mono/stereo peak/RMS as configured |
| VoxPeakMeter | Concept | UI-6 | peak/hold/clip grammar |
| VoxGainReductionMeter | Deferred until Mastering consumer | UI-6 | second consumer justification |
| VoxLoudnessMeter | Deferred until Mastering consumer | UI-6 | LUFS-specific semantics |
| VoxSpectrumAnalyzer | Concept | UI-6 | real snapshot only |
| VoxStereoScope | Deferred/Concept | UI-6 | likely Mastering consumer |
| VoxCorrelationMeter | Deferred/Concept | UI-6 | likely Mastering consumer |
| VoxTransferCurve | Concept | UI-6 | distortion/dynamics visual grammar |
| VoxXYPad | Concept | UI-5 | synth/performance/modulation use |
| VoxKeyboard | Concept | UI-6/UI-10 | shared keyboard base |

---

## 9. Modulation system — UI-5

| Capability | Current | Required before Production |
|---|---:|---|
| VoxModulationRing | Prototype inside VoxKnob | extract/standardize visual contract |
| source indicator | Concept | source identity + active/disabled |
| destination indicator | Concept | hover/assigned/multi-route |
| route drag assignment | Concept | valid/invalid destination grammar |
| positive/negative depth | Concept | clear bipolar semantics |
| route editor/popover | Concept | edit/delete/enable |
| automation indicator | Concept | visually distinct from modulation |
| matrix row grammar | Concept | source/transform/destination/depth/delete |
| direct control ↔ matrix consistency | Concept | one route state, two views |

Electronic Engine `ModulationMatrix` remains product-specific; the interaction grammar is shared.

---

## 10. Preset, lists and structured data — UI-7

| Component | Current | Notes |
|---|---:|---|
| VoxList | Concept | virtualisation where needed |
| VoxListRow | Concept | hover/selected/disabled/error |
| VoxTable | Concept | header/sort/selection |
| VoxTableHeader | Concept | resize/sort state |
| VoxTree | Concept | hierarchical data |
| VoxTreeItem | Concept | selected/expanded/disabled |
| VoxDisclosure | Concept | expand/collapse |
| VoxPresetSelector | Product prototype exists | must consume shared primitives |
| VoxPresetBrowser | Product prototype exists | state machine not yet accepted |
| VoxPresetList | Concept | Factory/User grouping |
| VoxPresetSearch | Concept | shared search primitive |
| VoxPresetTagFilter | Concept | filter grammar |
| VoxDirtyIndicator | Concept | unsaved-state grammar |

Preset state acceptance must cover:

```text
Factory / User / Clean / Dirty / Loading / Saving / ReadOnly
SearchEmpty / Missing / MigrationRequired / LoadError / SaveError
OverwriteConfirmation / DeleteConfirmation
```

---

## 11. Timeline and musical-editor grammar — UI-8

The complex editors stay product-specific, but these interaction/visual concepts are shared:

| Primitive/concept | Current | Production requirement |
|---|---:|---|
| TimelineRuler | Concept | musical/time labels + zoom |
| Grid/Snap | Concept | deterministic quantization display |
| Playhead | Concept | actual transport only |
| LoopRange | Concept | handles + active range |
| SelectionRect | Concept | multi-selection grammar |
| NoteBlock | Concept | selected/hover/move/resize |
| ResizeHandle | Concept | explicit hit target |
| VelocityBar | Concept | value + selection |
| AutomationPoint/Curve | Concept | node/segment selection |
| LaneHeader/Divider | Concept | lane actions + resize |
| ZoomControl | Concept | horizontal/vertical where applicable |
| FollowPlayheadIndicator | Concept | on/off state |

Product prototypes already exist for StepSequencer and ZoneRangeEditor, but they must migrate to this grammar before Production.

---

## 12. Drag/drop, selection, routing and matrix — UI-9

| Capability | Current | Applies to |
|---|---:|---|
| DragHandle | Concept | rack/FX/list/editor |
| DragGhost | Concept | all reorder workflows |
| InsertionMarker | Concept | ordered lists/chains |
| DropTargetValid | Concept | routing/modulation/files |
| DropTargetInvalid | Concept | same |
| single selection | Contract | list/editor/rack |
| multi selection | Contract | editor/list |
| range selection | Contract | table/editor |
| lasso | Contract | piano roll/graph if supported |
| keyboard alternative | Contract | essential reorder/actions where practical |
| routing-node grammar | Concept | routing matrix |
| matrix cell grammar | Concept | routing/modulation |

---

## 13. Product shell and Electronic Engine pages — UI-10

Existing product implementations are **Prototype** until migrated and accepted.

| Product widget/page | Current | DS dependency before Production |
|---|---:|---|
| GlobalHeader | Prototype | buttons/icons/status/preset primitives |
| MainNavigation | Prototype | TabBar/segmented grammar |
| InstrumentRack | Prototype/partial | list/selection/drag/drop/status |
| InstrumentSlot | Prototype/partial | selection/status/icon/drag/drop |
| PartHeader | Prototype/partial | common controls + responsive rules |
| PresetBrowser | Prototype | UI-7 complete |
| BassPanel | Prototype | UI-2/3/4/5/6 primitives |
| LeadPanel | Concept/partial | UI-2/3/4/5/8 |
| AcidPanel | Concept/partial | UI-2/4/5/8 |
| AtmosPanel | Concept/partial | UI-3/4/5/6 |
| SemanticFxPanel | Concept/partial | UI-2/5/6/9 |
| StepSequencer | Prototype | UI-8 grammar |
| PianoRoll | Concept | UI-8 grammar |
| Arpeggiator/Phrase | Concept/partial | UI-8 grammar |
| PatternEditor | Concept | UI-8 grammar |
| PatternGenerator UI | Concept/partial | input/list/feedback primitives |
| ZoneRangeEditor | Prototype | range/drag/keyboard grammar |
| RoutingMatrix | Concept | UI-9 |
| EffectChain | Concept/partial | UI-9 drag/drop + selection |
| MixerStrip | Concept/partial | fader/meter/status |
| Keyboard footer | Concept/partial | VoxKeyboard/performance primitives |

A page cannot become Production while any release-critical shared dependency is still Concept/Prototype.

---

## 14. Other family products

These remain explicit so VOX UI does not accidentally optimise only for Electronic Engine.

### Vox Drums Engine

Shared dependencies to preserve in DS:

```text
waveform/sample graph
timeline/grid/playhead
list/tree/preset primitives
drag/drop
meter/fader
selection/focus/accessibility
```

Product-specific widgets remain Deferred in this repository:

```text
DrumGrid
DrumPad
DrumVoiceEditor
DrumSequencer
SampleEditor composition
```

### Mastering Engine

Shared dependencies to preserve:

```text
SpectrumAnalyzer
Level/Peak/GR/Loudness meters
StereoScope
CorrelationMeter
TransferCurve
Module-chain drag/drop
A/B status/selection grammar
```

Mastering product widgets stay outside Electronic Engine.

---

## 15. Showcase / Storybook backlog

The showcase must evolve from one foundation screen into sections:

```text
01 Foundations + tokens
02 Typography + icons
03 Buttons/toggles/selection
04 Knobs/sliders/faders
05 Inputs/value fields
06 Tabs/navigation
07 Panels/layout/responsive
08 Menus/popovers/dialogs
09 Feedback/status/error/empty/loading
10 Graph foundation
11 Filter response
12 Waveform/oscillator
13 Envelope/editable graph
14 Modulation
15 Meters/analyzers
16 Keyboard/performance
17 Lists/tables/tree
18 Drag/drop/selection
19 Timeline/editor grammar
20 Accessibility/focus
21 Scaling 75/100/125/150/200
```

Each implemented component gets at minimum:

```text
normal
hover
pressed where applicable
focused
selected/active where applicable
disabled
long label/value
minimum useful size
100% and 125% screenshot baseline
```

---

## 16. Priority queue

### P0 — blocks current work / architecture

```text
1. UI-1 CI + showcase acceptance
2. DS-0 contract review/freeze
3. Storybook structure
4. shared icon system/registry baseline
5. layout/breakpoint helpers baseline
6. UI-2a VoxGraph + filter graph + DSP helper regression
7. UI-2b FilterResponseModel + real FilterPanel integration
```

### P1 — blocks production synth workflow

```text
8. linear slider/fader/value/input primitives
9. waveform/oscillator graph
10. envelope editor
11. modulation interaction grammar
12. level meter + status primitives
13. keyboard/performance primitives
14. preset/list/search primitives
15. timeline/editor primitives
16. drag/drop + routing/matrix primitives
```

### P2 — production hardening

```text
17. dialogs/popovers/toasts/banners
18. tables/tree/structured views
19. screenshot regression expansion
20. full accessibility audit
21. full 75/100/125/150/200 scale matrix
22. performance/repaint profiling
23. page-level production acceptance
```

### Deferred until a real consumer requires them

```text
specialized Mastering-only meter variants
Drums-only compositions
product-specific workflow components from other repositories
```

Deferred does not mean undefined: shared visual/interaction contracts remain specified.

---

## 17. Production release gate

VOX Electronic Engine UI is not Production-ready until all of the following are true:

```text
[ ] DS-0 contracts accepted
[ ] all release-critical shared components are Production
[ ] no private duplicate palette/style system in product UI
[ ] no default JUCE styling leaks
[ ] no fake or unsupported controls
[ ] all product pages pass 100% + 125% screenshot review
[ ] minimum editor size accepted
[ ] 75/150/200% scale checks pass
[ ] keyboard focus path accepted
[ ] accessibility roles/names/values accepted where supported
[ ] empty/loading/error/unavailable states accepted
[ ] realtime visualizers respect snapshot/performance contract
[ ] full ctest green
[ ] editor smoke test green
[ ] VST3/Standalone smoke acceptance green
```

This backlog is intentionally conservative: existing product code is treated as Prototype until it proves conformance to the frozen design-system contract. That prevents legacy implementation from silently defining the new system.
