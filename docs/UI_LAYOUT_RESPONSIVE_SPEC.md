# VOX UI — Layout & Responsive Specification v1.0

**Status:** CANON candidate for DS-0 freeze  
**Scope:** family-wide layout, scaling, density and adaptive behaviour

---

## 1. Reference geometry

Reference canvas:

```text
1440 × 1080 @ 100%
```

Supported UI scale targets:

```text
75% / 100% / 125% / 150% / 200%
```

Layout must be derived from containers, proportions and shared metrics rather than page-specific absolute coordinates.

---

## 2. Scaling model

VOX distinguishes:

- **plugin window size** — host-provided editor bounds;
- **internal UI scale** — VOX logical scaling factor;
- **OS/display DPI** — platform scale handled by JUCE/platform rendering.

Do not multiply the same scale twice. Internal UI scale affects logical VOX geometry; platform DPI remains the platform/JUCE responsibility.

---

## 3. Minimum usable size

Each product must publish a minimum editor size. For Electronic Engine the production baseline should preserve:

- global header;
- selected Part identity;
- primary workflow navigation;
- current workspace controls;
- access to remaining content via compact layout or scrolling.

The minimum size must be validated by screenshot and keyboard navigation tests before release. A layout that merely clips controls does not pass.

---

## 4. Density modes

Shared components support two semantic density contexts:

```text
Normal
Compact
```

Compact mode may reduce padding, gap and control size only within approved token ranges. It must not invent a different visual style.

Typical adaptation:

```text
Large knob -> Normal -> Small
full text button -> icon/compact label where unambiguous
hero/banner -> collapsed/hidden
secondary labels -> shortened where specified
```

Essential values and primary actions remain visible.

---

## 5. Breakpoint policy

Breakpoints are semantic, not tied to one machine resolution.

Recommended Electronic Engine layout bands:

```text
Wide      >= 1280 logical px
Standard  1000–1279
Compact   800–999
Minimum   product-defined lower bound
```

Exact thresholds may be tuned after real screenshots, but all product pages must use the same breakpoint service/helpers rather than private thresholds.

---

## 6. Adaptive priority

When width/height decreases, degrade in this order:

1. reduce decorative whitespace;
2. collapse/remove decorative hero content;
3. switch eligible controls to Compact variants;
4. reduce nonessential secondary copy;
5. collapse secondary side panels into tabs/popovers;
6. enable local scrolling for editor/detail regions;
7. never hide the only path to a critical function.

Global navigation, current Part context and destructive-state visibility must remain accessible.

---

## 7. Shell geometry

Electronic Engine canonical shell remains structurally stable:

```text
Global Header
Instrument Rack | Main Content
                | Instrument Header
                | Main Navigation
                | optional Hero
                | Active Workspace
                | Performance Footer
```

The rack and footer may compact, but changing instrument must not cause large layout jumps.

---

## 8. Scroll policy

### 8.1 Must not scroll as a whole where avoidable

- global header;
- primary workflow navigation;
- persistent rack identity/select controls.

### 8.2 May scroll

- preset browser lists;
- long routing tables;
- piano roll/timeline;
- modulation matrix;
- advanced/settings content;
- inspector/detail panes.

Nested scrolling should be minimized. Mouse wheel routing must be predictable.

---

## 9. Layout primitives

The design system defines these layout concepts even when implemented as helpers rather than Component subclasses:

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

Product pages compose them; they do not invent new spacing systems.

---

## 10. Grid and spacing

Base unit: `4 px` logical.

All default spacing comes from tokens. Common product panels should use canonical padding/gutters. Exceptions require a documented visual reason.

No negative margins as a routine layout technique.

---

## 11. Text resilience

Controls must be tested with:

- longest English labels expected in product;
- localized/expanded labels where relevant;
- large numeric values/units;
- preset names substantially longer than normal.

Rules:

- essential values must not overlap controls;
- truncation uses ellipsis where the full value can be recovered via tooltip or expanded view;
- destructive actions must not become ambiguous after truncation;
- labels should not force unrelated panels to resize unpredictably.

---

## 12. Graph responsive rules

Graphs preserve plot usefulness before decorative labels.

As graph size shrinks:

1. reduce label density;
2. reduce minor grid density;
3. keep the curve/trace and primary markers;
4. preserve interaction handles if editable;
5. never distort frequency/time mapping simply to fill space.

---

## 13. Editor/timeline responsive rules

Piano roll, sequencer and sample/timeline editors use dedicated zoom/scroll rather than compressing the musical time axis beyond usability.

Persistent lane headers may compact but must remain identifiable.

---

## 14. Dialog/popover sizing

Transient surfaces must remain within editor/display work area.

- popup menus may scroll when too tall;
- popovers may flip alignment near edges;
- dialogs must provide minimum margins and never render confirmation buttons off-screen.

---

## 15. Resize performance

`resized()` must be deterministic and lightweight. Avoid expensive DSP/data recomputation, file I/O, synchronous image generation or rebuilding static resources during continuous resize.

---

## 16. Acceptance matrix requirements

Every major shared component and every production page must be visually checked at:

```text
75%
100%
125%
150%
200%
```

At minimum, page-level screenshot baselines are required at 100% and 125%, plus a minimum-size screenshot.

Pass criteria:

- no clipping;
- no overlap;
- no unreachable action;
- readable primary values;
- stable hierarchy;
- predictable scrolling;
- correct focus order after adaptation.
