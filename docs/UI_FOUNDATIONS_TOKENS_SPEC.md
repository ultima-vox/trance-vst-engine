# VOX UI — Foundations & Tokens Specification v1.0

**Status:** CANON candidate for DS-0 freeze

This document completes the production token contract used by all VOX-family products.

---

## 1. Token rule

Any repeated visual metric or semantic colour used by more than one component belongs in the token system before use.

Product-specific identity artwork may use additional approved palette values, but shared controls must not introduce private palettes.

---

## 2. Colour roles

Existing canonical roles remain:

```text
background
panel
panelRaised
control
graph
border
borderSubtle
accent
accentHover
accentDim
text
textSecondary
textMuted
danger
success
warning
```

Add semantic support roles where needed:

```text
overlay
focus
selection
info
clip
```

`focus` and `selection` may alias `accent` initially; aliases remain semantic so later visual refinement does not require component rewrites.

---

## 3. Opacity roles

Canonical logical alpha factors:

```text
disabled        0.35
subtle          0.60
secondary       0.75
overlayScrim    0.72
graphGridMinor  0.10
graphGridMajor  0.18
graphFill       0.10
hoverFill       0.12
focusGlow       0.20
```

These are defaults, not permission to multiply opacity arbitrarily in each component.

---

## 4. Spacing

Base unit: 4 logical px.

```text
xs    4
sm    8
md   12
lg   16
xl   20
xxl  24
xxxl 32
```

No local 5/7/13 px spacing without documented optical reason.

---

## 5. Radius

```text
small   4
medium  6
large   8
```

VOX does not use pill-shaped surfaces by default. Fully rounded geometry is reserved for circular controls/indicators or explicit segmented semantics.

---

## 6. Stroke

```text
hairline 1.0
normal   1.5
strong   2.0
focus    2.0
```

At non-integer scale factors, drawing code should preserve optical sharpness using JUCE coordinates rather than hardcoding device pixels.

---

## 7. Icon metrics

```text
small   16
normal  20
large   24
stroke  1.5
```

Icon-only button preferred hit target: 28 logical px minimum.

---

## 8. Control metrics

```text
controlHeight      32
controlHeightSmall 28
tabHeight          36
menuRowHeight      28
toolbarHeight      36
sectionHeader      32
scrollbarWidth     10
focusInset          2
hitTargetMin       24
iconHitTargetMin   28
```

Knobs remain:

```text
Small  36
Normal 48
Large  64
```

---

## 9. Panel/layout metrics

```text
panelPaddingCompact  8
panelPadding          12
panelPaddingLarge     16
panelGapCompact        8
panelGap               12
sectionGap            16
```

Product layout helpers consume these roles instead of local literals.

---

## 10. Typography roles

Canonical roles remain:

```text
Instrument Title  26 px semibold intent
Section Title     14 px semibold intent
Control Label     12 px
Value/Secondary   11 px
```

Additional roles for production surfaces:

```text
Menu/Body         12 px
Tooltip           11 px
Micro/Status      10 px
```

Weight intent must use deterministic fallback, not assume a platform `SemiBold` style string exists.

---

## 11. Graph metrics

```text
traceWidth        1.5–2.0
nodeDiameter      6–8
gridMajorWidth    1.0
gridMinorWidth    1.0
axisLabel         Value/Secondary typography
```

Graph fill uses `graphFill` opacity role by default.

---

## 12. Animation/motion

VOX uses restrained motion.

Canonical durations:

```text
fast      80 ms
normal   140 ms
slow     220 ms
```

Use motion for state transitions only where it improves comprehension. Realtime audio visualizers follow data update cadence, not decorative animation durations.

---

## 13. Layer/z-order semantics

Canonical logical layers:

```text
base
content
overlay
popover
modal
tooltip
```

Implementation may use JUCE child ordering rather than numeric z-index, but semantic priority remains consistent.

---

## 14. Reference geometry

```text
referenceWidth  1440
referenceHeight 1080
```

Supported internal UI scales:

```text
0.75
1.00
1.25
1.50
2.00
```

---

## 15. Token acceptance

A new token must:

- have semantic purpose;
- be named by role, not one current component;
- avoid duplicating an equivalent token;
- be demonstrated in showcase when it affects visible family grammar;
- be documented here before production use.
