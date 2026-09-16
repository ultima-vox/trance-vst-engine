# VOX UI — Design System v1.0 Freeze Gate

**Status:** DS-0 candidate  
**Purpose:** define the complete design-system contract before additional product panels are implemented.

This document is the gate between foundation work and the UI-2+ product/component implementation roadmap.

---

## 1. Canonical document set

The VOX UI design system is defined by the following documents together:

```text
UI_DESIGN_SYSTEM.md
UI_VISUAL_REFERENCE_SPEC.md
UI_COMPONENT_CATALOG.md
UI_COMPONENT_CATALOG_PRODUCTION_ADDENDUM.md
UI_FOUNDATIONS_TOKENS_SPEC.md
UI_INTERACTION_STATE_SPEC.md
UI_LAYOUT_RESPONSIVE_SPEC.md
UI_ACCESSIBILITY_SPEC.md
UI_ACCEPTANCE_MATRIX.md
UI_PRODUCTION_BACKLOG.md
VOX_UI_FAMILY_ARCHITECTURE.md
```

No single file replaces the others. Together they define foundations, inventory, ownership, interactions, responsive behaviour, accessibility, acceptance and implementation status.

`UI_PRODUCTION_BACKLOG.md` is the canonical status/priority map. It must not redefine component behaviour; behavioural contracts remain in the normative specifications above.

---

## 2. DS-0 goals

DS-0 exists to ensure product implementation is not where the design system is invented.

Before DS-0 closes, the family must already know:

- what shared primitives exist;
- what complex widgets stay product-specific;
- what every common state means;
- how keyboard/focus works;
- how resize/scaling degrades;
- how graphs/meters consume realtime-safe data;
- how modulation is visualised;
- how drag/drop and selection behave;
- how errors/empty/loading states appear;
- how components are accepted as Production-ready;
- which phase owns every known missing capability;
- which items are explicitly Deferred rather than silently absent.

---

## 3. Architecture lock

Canonical dependency direction:

```text
Product UI -> vox_ui -> JUCE GUI
```

Forbidden:

```text
vox_ui -> product DSP
vox_ui -> PluginProcessor
vox_ui -> product preset/rack engines
DSP realtime path -> vox_ui
Source/UI/
```

Product-specific complex widgets remain outside shared `vox-ui` until a second real consumer proves a reusable abstraction.

---

## 4. Foundations lock

Required production foundations:

```text
semantic colours
opacity roles
4 px spacing system
radius system
stroke widths
icon sizes/stroke
control metrics
panel/layout metrics
typography roles
graph metrics
motion durations
logical layer semantics
supported scale factors
```

`libs/vox-ui/Tokens.h` must remain aligned with `UI_FOUNDATIONS_TOKENS_SPEC.md`.

---

## 5. Shared component inventory lock

The master inventory is the existing catalog plus the production addendum. Implementation status and phase ownership are tracked in `UI_PRODUCTION_BACKLOG.md`.

Known shared categories:

```text
layout
text/input
buttons/toggles/selection
knobs/sliders/faders
navigation
overlays/menus/dialogs
feedback/status
lists/tables/tree
graphs/meters/audio visualisation
performance controls
empty/loading/error patterns
```

Not every component must be implemented before DS-0 closes. Every known component must have an explicit status and contract before product release depends on it.

The status vocabulary is:

```text
Concept
Prototype
Implemented
Production
Deferred
```

A status may only advance according to `UI_ACCEPTANCE_MATRIX.md`.

---

## 6. Product workflow lock

Electronic Engine, Drums Engine and Mastering Engine remain separate products.

Electronic sequencing/arp/piano-roll state belongs to the selected Part. Drums workflows remain in Vox Drums Engine. Mastering analysis/module-chain workflows remain in Mastering Engine.

Shared appearance does not imply shared product ownership.

---

## 7. Interaction lock

Family interaction semantics are defined in `UI_INTERACTION_STATE_SPEC.md`.

Product code may add domain actions but may not redefine:

- focus behaviour;
- disabled/read-only/unavailable distinction;
- parameter reset policy;
- state priority;
- drag/drop visual grammar;
- selection grammar;
- menu/dialog baseline;
- modulation/automation distinction.

---

## 8. Responsive lock

`UI_LAYOUT_RESPONSIVE_SPEC.md` is normative for:

- supported scales;
- logical breakpoints;
- Compact vs Normal density;
- degradation priority;
- scrolling policy;
- text resilience;
- graph/editor shrink behaviour;
- minimum-size acceptance.

Pages must use shared breakpoint/layout helpers rather than private thresholds.

---

## 9. Accessibility lock

`UI_ACCESSIBILITY_SPEC.md` is part of production acceptance.

A custom-painted component is incomplete if it loses standard keyboard/accessibility semantics that a JUCE primitive could have provided.

The family remains `JUCE first, custom by necessity`.

---

## 10. Realtime visualisation lock

Audio visualisation must follow:

```text
DSP/audio thread
  -> atomics / lock-free or immutable snapshot
  -> UI bridge/message thread
  -> visual component
```

No paint/timer callback reads mutable realtime DSP objects directly.

Visualisation update budgets and acceptance live in `UI_ACCEPTANCE_MATRIX.md`.

---

## 11. Showcase role

`vox_ui_showcase` evolves into the live Storybook/component laboratory for VOX UI.

It is responsible for demonstrating:

- every implemented shared component;
- all meaningful states;
- scaling;
- long labels/values;
- focus/accessibility presentation;
- graph/meter no-data/error states;
- responsive behaviour.

A shared component should not be declared Production-ready without showcase coverage unless it is nonvisual infrastructure.

The required Storybook sections and their phase ownership are tracked in `UI_PRODUCTION_BACKLOG.md`.

---

## 12. Implementation roadmap after DS-0

Canonical phase order:

```text
UI-1   foundation primitives + Storybook baseline
UI-2   graph base + filter response/reference panel
UI-3   oscillator/waveform primitives
UI-4   envelope/editable graph primitives
UI-5   modulation grammar/components
UI-6   meters/realtime visualization primitives
UI-7   preset/list/search/structured-data primitives
UI-8   timeline/editor grammar primitives
UI-9   drag/drop, routing and matrix primitives
UI-10  shell/page composition acceptance
```

The detailed dependency and priority queue lives in `UI_PRODUCTION_BACKLOG.md`.

Product-specific panels may be composed only from contracts already frozen or explicitly added through a design-system change.

---

## 13. Change control

After DS v1.0 freeze:

- additive backward-compatible component/token additions are DS v1.x;
- breaking API/state/interaction changes require explicit design-system review;
- visual changes affecting family identity require screenshot baseline updates;
- product-specific hacks are not a substitute for a shared DS change;
- newly discovered components must be entered into the production backlog with status, owner and phase before implementation spreads into product code.

---

## 14. DS-0 exit criteria

DS-0 can be marked accepted when:

```text
[ ] foundation/token spec accepted
[ ] production component inventory accepted
[ ] interaction/state spec accepted
[ ] responsive/layout spec accepted
[ ] accessibility spec accepted
[ ] acceptance matrix accepted
[ ] production backlog/status map accepted
[ ] every known item has Concept/Prototype/Implemented/Production/Deferred status
[ ] every non-Deferred missing capability has a phase owner
[ ] UI-1 build is green
[ ] UI-1 full ctest is green
[ ] UI-1 showcase runs
[ ] UI-1 100% screenshot reviewed
[ ] UI-1 125% screenshot reviewed
[ ] UI-1 75/150/200% scale checks completed
[ ] unresolved items are explicit Deferred/roadmap entries
```

The goal is not to pre-code every future widget. The goal is to eliminate undefined family behaviour before that widget is needed in production.
