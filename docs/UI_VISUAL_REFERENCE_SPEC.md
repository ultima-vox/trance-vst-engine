# VOX ELECTRONIC ENGINE — Visual Reference Specification v1.0

**Status:** CANON / normative extension of `UI_DESIGN_SYSTEM.md`  
**Purpose:** make the accepted reference screens implementation-ready instead of leaving visual interpretation to an agent.  
**Applies to:** Vox Electronic Engine and, where marked shared, the wider VOX plugin family.

---

## 1. Source of truth

The accepted reference set defines the intended production visual language for these workflows:

- Psy Bass
- Lead
- Acid
- Atmos / Texture
- FX Engine
- Arp / Sequence
- Drums

The references are not pixel-perfect implementation blueprints. They establish **hierarchy, density, component grammar, proportions, visual states, and interaction patterns**. The implementation must preserve those properties while remaining responsive and scale-safe.

Priority of authority:

1. `UI_DESIGN_SYSTEM.md` — global tokens, interaction rules, accessibility, realtime safety.
2. `UI_COMPONENT_CATALOG.md` — semantic component inventory and ownership.
3. `UI_VISUAL_REFERENCE_SPEC.md` — exact visual/layout grammar derived from accepted screens.
4. Product page implementations.

If a page implementation conflicts with this document, the page is wrong unless the design canon is intentionally revised.

---

# 2. Global shell canon

Every Electronic Engine screen uses the same shell.

```text
┌──────────────────────────────────────────────────────────────────────────┐
│ Global Header                                                           │
├───────────────┬──────────────────────────────────────────────────────────┤
│ Instrument    │ Instrument Header                                       │
│ Rack          ├──────────────────────────────────────────────────────────┤
│               │ Main Navigation                                         │
│               ├──────────────────────────────────────────────────────────┤
│               │ Hero / Identity Banner                                  │
│               ├──────────────────────────────────────────────────────────┤
│               │ Active Workspace                                        │
│               ├──────────────────────────────────────────────────────────┤
│               │ Performance Footer / Keyboard                           │
└───────────────┴──────────────────────────────────────────────────────────┘
```

The shell is persistent. Switching instruments must not cause the entire plugin layout to jump.

## 2.1 Global Header

Canonical contents:

```text
Brand | Prev | Next | Project/Preset | Save | Seed | Randomize | MIDI | CPU | Output | Panic | Settings
```

Rules:

- one horizontal row;
- compact, dense, no oversized controls;
- brand occupies the left anchor;
- status indicators are visually quieter than actionable controls;
- `Panic` is the only persistent danger-red action;
- Output uses a compact knob + value rather than a full panel;
- MIDI activity uses a small cyan indicator;
- CPU is a compact status visualization, not a large meter.

## 2.2 Instrument Header

Canonical contents:

```text
Instrument Name + edit icon
Slot | MIDI channel | engine/preset subtitle
Instrument selector | previous | next | favourite | Channel selector
```

The title is the strongest typographic element below the brand.

## 2.3 Main Navigation

Canonical visual grammar:

- full-width horizontal segmented tabs;
- active tab uses cyan fill/edge treatment;
- inactive tabs remain dark;
- tabs are product/workflow level, not individual parameter sections;
- instrument pages may expose different tab labels, but geometry must remain stable.

Examples:

```text
SOUND | PATTERN | ROUTING | ZONES | MACROS | ADVANCED
FX | ROUTING | MODULATION | MACROS | SEQUENCER | ADVANCED
SOUND | PATTERN | MIXER | ROUTING | ZONES | MACROS | ADVANCED
```

---

# 3. Instrument Rack canon

The left rail is a persistent 16-slot rack in Electronic Engine.

Each slot contains:

```text
slot number | identity thumbnail/icon | name      | MIDI channel | power
                                      | subtitle
```

## 3.1 Selected state

Selected slot:

- cyan outline;
- slightly brighter panel fill;
- thumbnail remains visible;
- active power icon cyan;
- name remains primary text;
- selected state must not depend on colour alone: outline + fill + text hierarchy are combined.

## 3.2 Empty state

Empty slots:

- no decorative thumbnail;
- `Empty` + `OFF` only;
- power indicator reduced/muted;
- visual weight significantly lower than occupied slots.

## 3.3 Identity thumbnails

Instrument thumbnails are allowed as small identity assets in the rack and hero banner. They are **not** control surfaces and must not carry essential state.

Suggested product accents visible in accepted references:

- Psy Bass: cyan/blue;
- Acid: green;
- Lead: violet;
- Atmos: ice-blue;
- FX: cyan/blue;
- Arp: cyan;
- Drums: cyan.

These accents are secondary identity cues. The family-level interaction accent remains VOX cyan.

---

# 4. Hero / Identity Banner

Each major instrument may use a shallow banner directly below the workflow tabs.

Purpose:

- rapid instrument identity;
- visual separation between shell and editor;
- short descriptive copy;
- product personality without changing the control language.

Canonical composition:

```text
LEFT: title + 2–3 line tagline
CENTER: low-contrast artwork
RIGHT: 3–4 keyword identity + short description
```

Rules:

- banner is decorative/identifying, never interactive;
- artwork must not reduce readability;
- artwork uses a dark overlay/low luminance behind text;
- banner height remains shallow relative to active workspace;
- different instruments may have different artwork and accent hue;
- banner must be removable/collapsible in future compact layouts without losing functionality.

---

# 5. Panel grammar

Accepted screens use dense modular panels rather than one large undifferentiated surface.

Every functional panel follows:

```text
[power/status icon] SECTION TITLE                 [optional selector/action]
───────────────────────────────────────────────────────────────────────────
visualization / controls
labels
values
```

## 5.1 Panel header

- uppercase or small-caps semantic title;
- optional small icon;
- power/bypass at the left edge where relevant;
- optional selector/actions on the right;
- no oversized header bars;
- controls inside header align to the 4 px grid.

## 5.2 Panel borders

- 1 px subtle blue border;
- radius approximately 4–6 px;
- neighbouring panels separated by narrow consistent gutters;
- raised panels may be slightly lighter than the page background;
- avoid card shadows; use contrast and borders instead.

## 5.3 Density

Reference screens intentionally use professional DAW density:

- multiple logical panels visible without scrolling at 1440×1080 reference size;
- parameter labels are compact;
- graphs share space with controls;
- unnecessary explanatory paragraphs are avoided inside the workspace.

---

# 6. Knob canon

The accepted references refine `VoxKnob`.

Visual anatomy:

```text
      value arc / modulation arc
            ╭──────╮
           │  body │
            ╰──────╯
              ↑ marker
            Label
             Value
```

Rules:

- dark recessed centre;
- thin metallic/dark outer ring;
- cyan value arc;
- bright short position marker;
- no photorealistic knob texture;
- no giant glow;
- value arc is the dominant indication;
- label and numeric value are always readable without hover;
- small knobs are acceptable in dense workflow panels.

Optional instrument-colour overlays may be used for specialised graphs or identity, but ordinary parameter knobs should normally remain cyan to preserve family consistency.

---

# 7. Graph canon

The reference set establishes several graph families.

Shared rules:

- dark near-black graph well;
- subtle grid;
- cyan or instrument-accent curve;
- restrained low-opacity fill;
- data is primary; ornamentation is secondary;
- axes/labels appear only where useful;
- no fake animation.

## 7.1 Waveform / oscillator graph

Used in Psy Bass / Lead / samples.

- line waveform centred vertically;
- optional light glow confined to the curve;
- navigation/selector may sit immediately below or above;
- waveform must correspond to selected/generated content.

## 7.2 Filter response

Canonical axes:

- logarithmic frequency horizontal scale;
- optional dB vertical scale;
- response curve with subtle area fill;
- filter selector in panel header.

## 7.3 Envelope

- editable node points may be shown;
- selected node cyan/bright accent;
- ADSR stage letters appear below relevant controls;
- visual curve and numeric knobs remain synchronized.

## 7.4 Modulation graph

Can represent:

- ENV;
- LFO;
- stepped sequence;
- sample & hold/random.

Tabs above the graph select the modulation source. Do not duplicate a separate unique graph implementation for every source.

---

# 8. Footer / Performance keyboard

The accepted screens use a persistent bottom performance area.

Structure:

```text
Keyboard | Chords | Scale tabs
Pitch wheel | Mod wheel | piano keyboard
right-side context: velocity curve / MIDI Learn / keyboard options
```

Rules:

- keyboard spans nearly the full content width;
- octave `C` labels are visible;
- pressed/active notes use cyan;
- Pitch and Mod controls remain narrow vertical strips;
- footer should be shareable between synth-like instruments;
- instrument-specific footer controls may be added on the right but must not destroy keyboard geometry.

---

# 9. Psy Bass page canon

Layout emphasis:

```text
Row 1: Oscillator | Filter | Amp Envelope
Row 2: Drive/Character | Accent | Performance
Row 3: Modulation | Matrix
Footer: Keyboard
```

Psy Bass should feel compact, technical and low-frequency focused.

Required semantic groups:

- Oscillator;
- Filter;
- Amp Envelope;
- Pitch-related controls where supported;
- Drive / Character;
- Accent;
- Performance / Glide;
- Modulation;
- Mod Matrix;
- Keyboard.

The page must reflect actual engine parameters. Do not add unused synth controls only because they appear in a generic reference.

---

# 10. Lead page canon

Lead uses higher visual complexity than Psy Bass and introduces a violet identity accent in graphs/artwork.

Recommended layout:

```text
Row 1: Oscillator | Filter | Amp Envelope
Row 2: Unison/Voices | Glide/Portamento | Mod Envelope
Row 3: Macros | Arpeggiator/Phrase | Performance
Row 4: Modulation | Matrix
Footer: Keyboard
```

Semantic emphasis:

- supersaw/wavetable-style oscillator identity;
- morph / sync / FM / ring / noise where implemented;
- unison and stereo spread;
- glide / legato;
- modulation envelope;
- macro performance;
- arp/phrase integration;
- modulation matrix.

Violet is a **content accent**, not a replacement for cyan as the global interaction colour.

---

# 11. Acid page canon

Acid is more sequencer-centric than the general synth layout.

Recommended structure:

```text
Top: Oscillator | Filter | Envelope | Drive | Accent | Slide | Output
Middle: Step Sequencer (wide)
Bottom: Modulation | Performance | Play Mode
Footer: Keyboard
```

Canonical Acid step lanes visible in the accepted reference:

- Note;
- Accent;
- Slide;
- Gate;
- Octave.

The current generic sequencer can expose additional supported lanes, but Acid mode should prioritise the 303 workflow.

Acid identity may use green in artwork/waveform-specific cues while active controls remain readable in VOX cyan.

---

# 12. Atmos / Texture page canon

Atmos is layered and visualization-heavy.

Recommended structure:

```text
Row 1: Layer A Sample/Granular | Layer B Texture | Layer Mix
Row 2: Filter | Amp Envelope | Motion Envelope | Spectral Cloud
Row 3: Shimmer/Reverb | Modulation | XY Texture Morph | Mod Matrix
Footer: Keyboard
```

Visual semantics:

- long waveforms are acceptable for layers;
- texture/spectral graphics may be denser than synth waveforms;
- XY morph is a first-class performance control;
- longer time values need enough label width;
- motion/evolution should be represented by real parameter state or safe visualization snapshots.

---

# 13. FX Engine page canon

FX differs from instrument synth pages because signal flow is primary.

## 13.1 Effect Chain

The chain is a visual horizontal signal path:

```text
IN → FILTER → DISTORTION → CHORUS → DELAY → REVERB → + → OUT
```

Rules:

- modules are cards;
- arrows clearly show signal direction;
- selected module has stronger border/identity;
- bypass is visually obvious;
- chain supports serial/parallel mode where implemented;
- add/remove/reorder controls live near the chain, not scattered around parameter panels.

## 13.2 Module editor panels

Below the chain, selected/relevant modules expose dedicated parameter panels.

Examples in reference:

- Reverb with response/decay visualization;
- Delay with timing/feedback controls;
- Distortion with transfer curve.

## 13.3 FX performance

Reference includes:

- XY Performance;
- Modulation;
- Macros;
- Preset snapshots / A-B.

These are valid FX-specific workflow widgets.

---

# 14. Arp / Sequence page canon

This page is editor-first. The sequence grid is the primary focal point.

Top transport/options row:

```text
Play | Stop | Sync | Rate | Swing | Length | Octave | Scale Lock | Chord Mode | Randomize | Mutate
```

Main editor:

- left lane selector;
- central step grid;
- right pattern settings panel.

Canonical lane families shown by the reference:

- Note;
- Gate;
- Velocity;
- Octave;
- Ratchet;
- Tie;
- Probability;
- Mod 1;
- Mod 2;
- Mod 3.

Bottom:

- Modulation;
- Routing;
- Performance;
- Keyboard.

Rules:

- active lane uses cyan selection treatment;
- note lanes use blocks positioned by pitch;
- velocity/probability use vertical bars;
- tie/ratchet use distinct symbolic grammar;
- modulation lanes use curves or stepped lines;
- grid playhead must be synchronized to actual transport.

---

# 15. Drums page canon

Drums is pad/sample/sequencer oriented rather than synth oriented.

Recommended structure:

```text
Row 1: Drum Pad Bank | Sample Waveform | Amp Envelope
Row 2: Filter | Transient | Pitch | Performance
Row 3: Multi-row Drum Step Sequencer
Footer: Keyboard / Drum Map controls
```

## 15.1 Drum Pad Bank

Canonical compact pad grid:

- Kick;
- Snare;
- Clap;
- Closed Hat;
- Open Hat;
- Percussion;
- Rim / FX;
- expandable in the actual product.

Selected pad uses stronger cyan fill/border.

## 15.2 Sample panel

Contains:

- sample selector/name;
- waveform;
- Reverse;
- Normalize;
- One Shot;
- Loop;
- sample duration/playhead where data is available.

## 15.3 Drum sequencer

Rows correspond to drum voices.

Each row may provide:

- Mute;
- Solo;
- step gates;
- velocity;
- probability;
- output route.

Step cells are square/rectangular and visually distinct from the melodic piano-roll grammar.

---

# 16. Accent-colour policy refined

The accepted references use instrument-specific hues selectively.

Canonical rule:

- **VOX cyan** = global interaction, selection, focus, active controls;
- **instrument accent** = identity artwork, waveform/graph content, selected specialised module;
- **danger red** = Panic/destructive/error only;
- **green** must not become a global success colour if it is being used as Acid identity inside that context;
- product accents must never reduce state readability.

Suggested semantic accents:

```text
identity.psyBass     blue/cyan
identity.acid        green
identity.lead        violet
identity.atmos       ice-blue
identity.fx          cyan/blue
identity.arp         cyan
identity.drums       cyan
```

Exact identity hues must be tokenised before implementation.

---

# 17. Layout proportions

At the 1440×1080 reference canvas, use these approximate proportions rather than literal fixed pixels:

```text
Global Header      ~6–7% of height
Instrument Header  ~7–8%
Main Tabs           ~3–4%
Hero Banner         ~12–13%
Footer Keyboard     ~11–13%
Instrument Rack     ~21–22% of width
Main Workspace      remaining width
```

Rules:

- rack width remains stable between instruments;
- footer height remains stable between synth-like instruments;
- panel geometry may vary by workflow;
- no element may depend on the exact 1440×1080 size.

---

# 18. Responsive behaviour

When space decreases:

1. reduce decorative banner height;
2. reduce panel gutters/padding within token limits;
3. switch suitable controls from large to normal/small variants;
4. collapse secondary copy/metadata;
5. allow page-local scrolling only as a last resort;
6. never hide critical parameter values, bypass states, clip indicators, or selected Part identity.

When space increases:

- do not simply enlarge every knob;
- use extra area for graphs, spacing, labels and analyzer detail;
- preserve professional density.

---

# 19. Artwork asset rules

Accepted screens use atmospheric artwork heavily enough that it needs explicit governance.

Artwork may appear in:

- hero banners;
- rack thumbnails;
- empty rack footer/brand area.

Artwork must never be baked into controls.

Requirements:

- separate asset from UI geometry;
- maintain dark overlay for text readability;
- scale/crop with aspect-fill semantics;
- no text embedded in artwork;
- no essential state encoded in imagery;
- fallback UI must remain usable if an artwork asset is missing.

---

# 20. Implementation mapping

Shared components required to reproduce the accepted references:

```text
VoxGlobalHeader
VoxInstrumentRack
VoxInstrumentSlot
VoxInstrumentHeader
VoxMainNavigation
VoxHeroBanner
VoxPanel
VoxSectionHeader
VoxKnob
VoxButton
VoxIconButton
VoxToggle
VoxSegmentedControl
VoxComboBox
VoxValueField
VoxGraph
VoxWaveform
VoxFilterResponse
VoxEnvelopeEditor
VoxTransferCurve
VoxMeter
VoxMacroPanel
VoxModulationTabs
VoxModulationMatrix
VoxXYPad
VoxKeyboard
VoxPitchBend
VoxModWheel
VoxEffectChain
VoxEffectSlot
VoxStepSequencer
VoxPianoRoll
VoxArpeggiator
VoxDrumPadBank
VoxDrumSequencer
VoxSampleEditor
```

Product pages should **compose** these components rather than reimplementing their visual rules.

---

# 21. Screenshot acceptance rules

Every significant UI PR must include screenshots at minimum:

```text
1440×1080 @ 100%
125% scale or equivalent high-DPI validation
one non-default selected instrument/workflow
```

For components with state:

- normal;
- selected/active;
- hover where practical;
- disabled;
- empty/missing-data state where applicable.

A screenshot review must check:

- shell alignment;
- consistent rack width;
- tab height/alignment;
- hero readability;
- panel gutters;
- knob size consistency;
- graph grammar;
- value legibility;
- no clipped labels;
- no default JUCE appearance leaking through.

---

# 22. Anti-patterns revealed by the references

Do not implement:

- giant knobs simply because space is available;
- one unique knob design per instrument;
- freeform panel placement without grid alignment;
- giant hero images that push functional controls below the fold;
- neon glow around every cyan element;
- graph animations unrelated to engine state;
- button-like styling for static labels;
- hidden numeric values that appear only on hover;
- inconsistent footer heights between instruments;
- arbitrary colour replacement of cyan for each product.

---

# 23. Canonical rule

The accepted references define **one product family, multiple workflows**.

A user switching from Psy Bass → Acid → Lead → Atmos → FX → Arp → Drums should feel that the workflow changed, **not that a different vendor's plugin opened**.

Therefore:

> Global shell, interaction states, controls, graph grammar, spacing, typography and feedback remain VOX. Instrument identity is expressed through content, composition, artwork and limited accent colour — not by redesigning the UI system for every page.
