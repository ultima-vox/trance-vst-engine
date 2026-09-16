# VOX UI — Interaction & State Specification v1.0

**Status:** CANON candidate for DS-0 freeze  
**Scope:** family-wide interaction rules for shared VOX UI and product composition  
**Applies to:** Electronic Engine, Drums Engine, Mastering Engine and future VOX products

This document is normative. Product widgets may add domain behaviour, but must not redefine family-wide control semantics locally.

---

## 1. State model

Every interactive control must expose only states that are semantically meaningful for that control. The family state vocabulary is:

```text
Normal
Hover
Pressed
Focused
Selected
Active
Disabled
ReadOnly
Unavailable
Loading
Empty
Error
Warning
Bypassed
Automated
Modulated
Dirty
Dragging
DropTargetValid
DropTargetInvalid
```

### 1.1 Priority when states overlap

Visual priority from highest to lowest:

```text
Error / Unavailable
Pressed / Dragging
Focused
Selected / Active
Warning
Automated / Modulated
Hover
Normal
Disabled lowers the entire control presentation and suppresses interaction states.
```

Automation and modulation never replace the base value indication. They are overlays on the same control.

### 1.2 Disabled vs ReadOnly vs Unavailable

- **Disabled:** feature exists but is currently not actionable because of current state or dependency.
- **ReadOnly:** value is valid and visible, but user editing is intentionally disallowed.
- **Unavailable:** feature/resource cannot currently function (missing module, failed load, unsupported capability, disconnected dependency).

These states must not share identical visuals or tooltips.

---

## 2. Pointer interaction

### 2.1 Hit targets

Visual geometry may be compact, but effective hit targets should be at least 24×24 px for desktop controls and preferably 28×28 px for icon-only actions.

### 2.2 Hover

Hover may increase border/accent emphasis but must not reveal essential information that is otherwise hidden.

### 2.3 Pressed

Pressed state must be visible immediately and remain visible until pointer release/cancel.

### 2.4 Double click

Parameter controls may use double-click reset only when a real default value is known. Never hardcode a family-wide numeric reset.

### 2.5 Right click

Right click opens a context menu when the control supports secondary actions. Parameter menus may expose reset, copy/paste value, MIDI learn, modulation assignment, automation-related actions, or product-specific safe actions.

### 2.6 Mouse wheel

Mouse-wheel editing must be explicitly enabled per control family. Scroll containers take precedence over value changes unless the pointer is clearly over an editable parameter and that component opts into wheel adjustment.

---

## 3. Keyboard interaction

### 3.1 Focus traversal

- `Tab` moves to the next focusable control.
- `Shift+Tab` moves to the previous focusable control.
- focus order follows visual/workflow order.
- hidden, unavailable and disabled controls are skipped where JUCE semantics allow.

### 3.2 Activation

- `Enter` activates default/primary actions where appropriate.
- `Space` toggles focused toggle/button/tab-like controls.
- `Esc` closes transient overlays, popup menus and dialogs before affecting a parent view.

### 3.3 Arrow navigation

Arrow navigation is required for segmented controls, tabs, radio-like groups, lists, trees, piano-roll selection where applicable, and menus.

### 3.4 Fine adjustment

Knob/slider fine adjustment uses one family-wide modifier. Canonical desktop modifier is `Shift` unless a platform conflict requires product-wide revision. The chosen modifier must be documented in tooltips/help and remain consistent.

---

## 4. Parameter control contract

All parameter controls must distinguish:

```text
base value
default value
automation state
modulation state
read-only/host-owned state
```

The UI must not become the authoritative parameter store.

### 4.1 Knobs

Required behaviour:

- explicit drag policy;
- configurable real default reset;
- fine adjustment;
- readable value without hover;
- optional value text editing only when safe;
- modulation range separate from base value;
- automation indication separate from modulation.

### 4.2 Sliders/faders

Required behaviour:

- visible handle/thumb;
- keyboard increment/decrement;
- optional dB/unity markers where semantically relevant;
- clip indication for mixer/meter-coupled variants.

### 4.3 Numeric/value fields

- direct text edit must validate before commit;
- invalid input does not silently coerce to an unrelated value;
- Escape cancels edit;
- Enter commits valid input;
- units are displayed but should not make numeric selection/editing awkward.

---

## 5. Selection grammar

Family selection modes:

```text
Single
Multi
Range
Lasso
PrimarySelected
```

`PrimarySelected` is the active object driving an inspector/editor when multiple items are selected.

Selected state must not depend on colour alone. Use border, fill, marker, handle, icon or geometry in addition to colour.

---

## 6. Drag and drop grammar

All reorderable/editable surfaces use the same concepts:

```text
drag handle or draggable region
dragging ghost/source dimming
insertion marker
valid target highlight
invalid target indication
copy vs move semantic where supported
cancel on Esc
```

No destructive move is committed until drop.

Product examples:

- Instrument Rack reorder;
- FX chain reorder;
- modulation source → destination assignment;
- preset/item movement where supported;
- piano-roll note move/resize;
- sample/asset drop where supported.

---

## 7. Modulation interaction grammar

Modulation is a first-class family behaviour.

### 7.1 Destination visualization

A modulated destination shows:

- base value;
- modulation range/depth;
- optional source indicator(s);
- polarity where relevant.

### 7.2 Assignment

When direct drag assignment is supported:

1. source enters drag state;
2. valid destinations highlight;
3. invalid destinations remain clearly unavailable;
4. drop creates/updates a route;
5. resulting depth is editable without opening a different mental model.

### 7.3 Multiple routes

Multiple modulation sources must not collapse into one ambiguous ring. If a compact control cannot represent all routes, use an aggregate range plus explicit route inspection/popover/matrix.

### 7.4 Automation + modulation

Automation controls the base parameter trajectory; modulation overlays runtime variation. UI must never imply they are the same mechanism.

---

## 8. Menus, popovers and dialogs

### 8.1 Popup/context menu

Must support:

- highlighted row;
- disabled row;
- checked/ticked row;
- submenu indicator;
- optional icon;
- shortcut text;
- separator.

### 8.2 Popover

Use for compact contextual editing where a full dialog would interrupt workflow. Popovers close on outside click or Esc and restore focus appropriately.

### 8.3 Modal/confirmation dialog

Required for destructive or irreversible actions such as delete/overwrite when undo is unavailable.

Buttons use deterministic order throughout the product. Dangerous action uses danger semantics; cancel remains visually secondary.

---

## 9. Preset interaction state machine

Canonical preset states:

```text
Clean
Dirty
Saving
Loading
ReadOnlyFactory
Missing
MigrationRequired
Error
```

Rules:

- switching away from Dirty state must not silently discard edits;
- factory presets are read-only and Save becomes Save As where appropriate;
- overwrite/delete require explicit confirmation when destructive;
- previous/next follow current filtered ordering deterministically;
- failed load leaves the prior valid state intact where possible;
- UI shows migration/version errors explicitly.

---

## 10. Timeline/editor interactions

Shared editor grammar applies to Piano Roll, Pattern, automation lanes and sample timelines.

Canonical concepts:

```text
ruler
grid/snap
playhead
loop range
selection rectangle
object move
object resize
lane selection
zoom
scroll
fit
follow playhead
```

### 10.1 Editing

- click selects;
- modifier-click extends/toggles where multi-select exists;
- drag moves;
- dedicated handles resize when supported;
- Delete removes selected editable objects;
- Esc cancels active drag/creation;
- undo/redo is product-level but must preserve expected editor semantics where implemented.

---

## 11. Empty, loading, warning and error UX

Never replace these states with a blank panel.

Each such surface should provide:

```text
state icon/marker
short title
concise explanation
next valid action when one exists
```

Examples:

- no preset results;
- empty rack slot;
- missing effect/module;
- no MIDI data;
- analyzer waiting for signal;
- failed resource load;
- unsupported state version.

---

## 12. Realtime visualization interaction

Meters/graphs are observers, not authoritative DSP controls unless they explicitly expose editable nodes/handles.

Hover readouts and cursors may inspect safe snapshots. They must never synchronously request mutable realtime DSP state.

---

## 13. Production acceptance

A component is not interaction-complete until the showcase or tests demonstrate applicable states from this specification, including keyboard focus and disabled/unavailable behaviour.
