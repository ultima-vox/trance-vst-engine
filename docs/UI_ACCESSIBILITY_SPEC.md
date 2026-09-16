# VOX UI — Accessibility Specification v1.0

**Status:** CANON candidate for DS-0 freeze  
**Scope:** keyboard, focus, semantic accessibility and non-colour state communication for VOX UI

---

## 1. Principle

A production VOX control must remain operable and understandable without relying exclusively on mouse precision, colour perception or undocumented gestures.

Accessibility is part of component acceptance, not a later polish phase.

---

## 2. Semantic roles

Every focusable shared component must expose an appropriate JUCE accessibility role/handler where the framework supports it.

Examples:

```text
VoxButton            -> button
VoxToggle/Switch     -> toggle/checkable control
VoxTabBar item       -> tab
VoxComboBox          -> combo box
VoxKnob/Slider       -> adjustable value control
VoxTextInput         -> editable text
VoxList/ListRow      -> list/list item
VoxTree              -> tree/tree item
VoxDialog            -> dialog
VoxMeter             -> read-only value/status where meaningful
```

Custom components must not become anonymous painted rectangles when a semantic role exists.

---

## 3. Accessible name, value and description

Interactive controls provide:

- concise accessible name;
- current value/state where meaningful;
- units where needed;
- additional description only when the label alone is insufficient.

Do not expose decorative artwork as essential interactive content.

---

## 4. Keyboard-only operation

All primary workflows must be operable without a mouse where practical.

Required family conventions:

```text
Tab / Shift+Tab    focus traversal
Enter              activate/default action
Space              toggle/activate button-like control
Arrow keys         tabs, segmented controls, menus, lists and adjustable controls
Esc                cancel/close transient UI
Delete             delete selected editable item where safe and expected
```

Product editors may add domain shortcuts, but they must not break the family baseline.

---

## 5. Focus visibility

Keyboard focus must be visible at all times.

Canonical focus treatment:

- tokenized accent focus ring or equivalent high-contrast outline;
- not represented only by subtle colour shift;
- drawn outside or inside geometry without clipping;
- distinguishable from selected/active state.

Focus indication must survive 75–200% UI scaling.

---

## 6. Focus order

Focus traversal follows visual/workflow order:

```text
header -> navigation -> current workspace -> footer / secondary actions
```

Hidden, disabled and unavailable controls must not create dead stops. Opening a dialog/popover moves focus into it; closing restores focus to the invoking control where possible.

---

## 7. Colour independence

No critical state may rely on colour alone.

Examples:

- selected rack slot: border/marker + colour;
- bypassed module: icon/state text + colour/opacity;
- error: icon/label + danger colour;
- modulation: ring/range geometry + accent;
- mute/solo: text/icon state + fill/colour;
- clip: explicit marker/icon or retained peak state, not red alone.

---

## 8. Contrast and legibility

The design system must maintain strong contrast between primary text and background and sufficient distinction for focus/selection/error states.

Muted text may be lower contrast but must remain readable for information that is still required to operate the plugin.

Disabled content may reduce opacity, but labels describing why a feature is unavailable must remain legible.

---

## 9. Hit targets

Desktop visual controls may be dense, but hit targets should meet the family minimum:

```text
24×24 logical px minimum
28×28 preferred for icon-only actions
```

Tiny visual icons may have larger transparent hit regions.

---

## 10. Adjustable controls

Knobs, sliders and faders must expose:

- accessible name;
- current value;
- unit;
- keyboard increment/decrement;
- default/reset semantics where provided;
- read-only/disabled state.

Fine adjustment is a secondary interaction and never the only way to reach a value.

---

## 11. Dynamic/realtime visuals

Meters, scopes and analyzers are not required to expose every frame to accessibility APIs. Provide stable semantic summaries where useful, such as current peak, clipping state, or status text.

Do not flood accessibility events at meter repaint frequency.

---

## 12. Graph/editor accessibility

Editable graph nodes, notes, steps and matrix rows should expose keyboard-operable selection/editing where reasonable.

For complex editors, provide at minimum:

- keyboard selection movement;
- delete/cancel;
- a way to inspect selected item's primary values;
- visible focus/selection distinct from hover.

A visual-only editor may exist only when equivalent product operation is available elsewhere and documented.

---

## 13. Motion and animation

Animation is restrained by default. No essential state depends on flashing/pulsing. Realtime meters may animate because they represent signal, but decorative continuous animation is discouraged.

---

## 14. Tooltips/help

Tooltips supplement labels; they do not replace required labels for primary controls.

Tooltips should explain non-obvious icon actions, modifiers and unavailable states.

---

## 15. Error messaging

Error and validation messages must identify:

- what failed;
- the affected object/control;
- the next valid action where one exists.

Do not communicate failure only via red border.

---

## 16. Acceptance

A shared component cannot be marked production-ready until applicable checks pass:

```text
keyboard focus reachable
keyboard action works
focus visible
accessible role/name/value present where supported
state not colour-only
75/100/125/150/200% scaling preserves focus and labels
long labels do not destroy operation
```
