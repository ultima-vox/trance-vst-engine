# VOX UI — Component Catalog Production Addendum v1.0

**Status:** CANON candidate for DS-0 freeze  
**Extends:** `UI_COMPONENT_CATALOG.md`

This addendum closes production-level gaps in the existing component catalog without changing the established ownership model. When the catalog is next consolidated, these entries should be folded into the master file.

---

## 1. Missing shared primitives to add to the master catalog

### Inputs and value controls

```text
VoxNumericInput
VoxSearchField
VoxRangeSlider
VoxCheckBox
VoxRadioGroup
```

### Feedback and status

```text
VoxBadge
VoxProgress
VoxSpinner
VoxToast
VoxBanner
```

### Overlay/transient surfaces

```text
VoxPopover
VoxDialog
VoxConfirmationDialog
VoxMenuItem
```

### Structured data/navigation

```text
VoxList
VoxListRow
VoxTable
VoxTableHeader
VoxTree
VoxTreeItem
VoxDisclosure
```

These are family-level visual/interaction primitives. Product semantics remain outside `vox-ui`.

---

## 2. Layout concepts to formalize

These are design-system concepts and may be implemented as shared helpers rather than individual `Component` subclasses:

```text
VoxStack
VoxRow
VoxGrid
VoxSplitView
VoxScrollArea
VoxCollapsibleSection
VoxResizablePane
VoxInspectorPane
VoxToolbar
VoxSpacer
```

All use design tokens and the responsive specification.

---

## 3. Icon system

Add a shared icon contract and registry concept:

```text
VoxIcon
VoxIconRegistry
```

Canonical icon grids:

```text
16 px
20 px
24 px
```

Rules:

- SVG/Drawable/vector only for shared UI icons;
- no emoji;
- semantic colour comes from VOX tokens, not arbitrary embedded SVG colours;
- icon-only controls meet family hit-target requirements;
- active/hover/disabled states are derived from component state, not separate random icon assets;
- one canonical glyph per semantic action unless a product identity asset is intentionally different.

Minimum shared icon vocabulary should include:

```text
power
bypass
close
add
remove/delete
edit
save
search
settings
previous/next
play/stop
lock
mute
solo
favourite
warning/error/info
expand/collapse
menu/more
reset
copy/paste
link/routing
MIDI
```

---

## 4. Graph foundation expansion

`VoxGraph` is the common primitive. Specialized graphs must derive visual grammar from it.

Required shared graph concepts:

```text
PlotArea
MajorGrid
MinorGrid
Axis
AxisLabel
Trace
FilledTrace
Marker
Node
Selection
HoverCursor
HoverReadout
WarningRegion
NoDataState
UnavailableState
MultiTraceLegend
```

These do not all need to be public classes; they are required behaviours/visual contracts.

### Shared snapshot concepts

Product/UI bridges should converge on immutable/safe snapshot contracts such as:

```text
GraphDataSnapshot
WaveformSnapshot
SpectrumSnapshot
MeterSnapshot
```

Exact C++ types may differ by data shape, but the ownership rule is constant: visual components consume stable UI-facing data, never mutable realtime DSP internals.

---

## 5. Meter family expansion

Shared meter grammar should cover:

```text
VoxMeter
VoxLevelMeter
VoxPeakMeter
VoxGainReductionMeter
VoxLoudnessMeter
VoxCorrelationMeter
VoxStereoScope
```

Not every product needs every meter. Mastering Engine is the likely second consumer that justifies promoting specialized meter primitives into shared `vox-ui`.

Required states:

```text
normal
warning
clip/over
hold/peak-hold
no-signal
unavailable
```

---

## 6. Realtime visualization contract

All visualizers declare:

- expected update frequency;
- data ownership/snapshot source;
- no-data behaviour;
- stale-data behaviour if relevant;
- interpolation/smoothing policy;
- repaint scope;
- whether editing is allowed.

A graph that displays a target parameter response must not be described as exact instantaneous audio output when DSP smoothing/polyphony can differ.

---

## 7. Modulation component grammar

The catalog should distinguish:

```text
VoxModulationSourceIndicator
VoxModulationDestinationIndicator
VoxModulationRing
VoxModulationRouteEditor / Popover
VoxModulationMatrix   # product workflow composition
```

Shared layer owns visual/interaction semantics. Product layer owns actual source/destination descriptors and routing state.

---

## 8. Preset/file workflow component grammar

Add family concepts:

```text
VoxPresetSelector
VoxPresetBrowser
VoxPresetList
VoxPresetSearch
VoxPresetTagFilter
VoxDirtyIndicator
```

State model is defined in `UI_INTERACTION_STATE_SPEC.md` and includes Factory/User, Dirty, Saving, Loading, ReadOnly, Missing, MigrationRequired and Error.

---

## 9. Timeline/editor visual grammar

The following are shared design concepts even when the complex editor itself remains product-specific:

```text
TimelineRuler
Grid
SnapControl
Playhead
LoopRange
SelectionRect
NoteBlock
ResizeHandle
VelocityBar
AutomationPoint
AutomationCurve
LaneHeader
LaneDivider
ZoomControl
FollowPlayheadIndicator
```

Electronic Engine `VoxPianoRoll` / `VoxPatternEditor`, Drums sequencer and future sample editors should all derive from this grammar rather than inventing unrelated timelines.

---

## 10. Drag/drop primitives

Design-system grammar includes:

```text
DragHandle
DragGhost
InsertionMarker
DropTargetValid
DropTargetInvalid
```

Applicable to rack, FX chain, modulation, editors, preset/list workflows and future sample workflows.

---

## 11. Empty/loading/error primitives

Shared patterns:

```text
VoxEmptyState
VoxLoadingState
VoxErrorState
VoxUnavailableState
```

They may be lightweight compositions rather than classes, but must use one family grammar: icon/marker, title, concise description, next valid action when available.

---

## 12. Production component status rule

The master catalog should ultimately use four implementation states:

```text
Concept
Prototype
Implemented
Production
```

Definitions are normative in `UI_ACCEPTANCE_MATRIX.md`.

No component should be labelled `Production` based solely on source-code existence.

---

## 13. Known product-specific components that must remain outside shared vox-ui

### Electronic Engine

```text
InstrumentRack
InstrumentSlot
PartHeader
BassPanel
LeadPanel
AcidPanel
AtmosPanel
SemanticFxPanel
PatternEditor
PianoRoll
StepSequencer
Arpeggiator
PatternGenerator
RoutingMatrix composition
Zone editor composition
EffectChain composition
```

### Drums Engine

```text
DrumGrid
DrumPad
DrumVoiceEditor
DrumSequencer
SampleEditor composition
```

### Mastering Engine

```text
MasteringModuleChain
AnalyzerWorkspace
Reference/A-B workflow
module-specific mastering panels
```

A product widget moves into shared only after a second real consumer proves a product-independent abstraction.
