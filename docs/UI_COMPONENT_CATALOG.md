# VOX UI — Component Catalog v1.0

**Status:** CANON candidate  
**Scope:** conceptual UI elements for the Ultima Vox audio-plugin family  
**Applies to:** Vox Electronic/Trance Engine, Vox Drums Engine, Mastering Engine, future VOX audio products

This catalog defines **types of UI elements**, not every repeated instance of a control.  
For example, `VoxKnob` is specified once; Cutoff, Resonance, Drive and Output are usages of that component.

The visual rules, colors, spacing, typography and interaction conventions are defined in `UI_DESIGN_SYSTEM.md`.

---

## 1. Component levels

Every UI element belongs to one of four levels.

### Level A — primitives

Reusable family-wide controls:

- knob;
- linear slider;
- fader;
- button;
- icon button;
- toggle;
- switch;
- segmented control;
- combo box;
- text input;
- numeric/value field;
- label;
- section header;
- tab;
- badge/status indicator;
- tooltip;
- scrollbar;
- context menu;
- divider;
- panel/container.

These are implemented in shared `vox-ui`.

### Level B — audio primitives

Reusable audio-oriented visualization/control components:

- waveform;
- spectrum;
- meter;
- envelope;
- filter response;
- XY pad;
- modulation curve;
- piano keyboard;
- macro control;
- parameter modulation ring;
- stereo/correlation display;
- vectorscope/goniometer;
- transfer curve.

These are family-wide when their semantics are generic.

### Level C — workflow widgets

Complex controls composed from primitives:

- preset browser;
- instrument rack;
- mixer strip;
- piano roll;
- step sequencer;
- arpeggiator;
- modulation matrix;
- effect chain;
- routing matrix;
- zone editor;
- sample editor;
- drum pad;
- module chain;
- analyzer workspace.

They may be shared when generic, or live in a product-specific layer.

### Level D — pages/workspaces

Complete product workflows:

- Sound;
- Pattern;
- Routing;
- Zones;
- Macros;
- Advanced;
- Mixer;
- FX;
- Mastering chain;
- Analyzer;
- Drum kit;
- Sample edit.

Pages compose existing components and should not introduce private styling.

---

# 2. Global shell

## 2.1 VoxGlobalHeader

Purpose:

- product branding;
- current preset;
- previous/next preset;
- save;
- global output;
- MIDI activity;
- CPU/load status;
- panic;
- settings;
- optional random/seed control.

Must remain visually consistent across VOX products.

## 2.2 VoxMainNavigation

Family navigation container.

Electronic Engine canonical sections:

- Sound
- Pattern
- Routing
- Zones
- Macros
- Advanced

Mastering and Drums may use different page names, but the navigation grammar remains the same.

## 2.3 VoxStatusIndicator

For:

- MIDI input;
- transport;
- clipping;
- bypass;
- sync;
- module missing;
- error/warning;
- CPU status.

Critical states must not rely only on colour.

---

# 3. Presets and content

## 3.1 VoxPresetSelector

Compact current-preset selector for headers.

Supports:

- current name;
- previous;
- next;
- dropdown;
- dirty/modified state.

## 3.2 VoxPresetBrowser

Full browser:

- Factory/User source;
- search;
- category;
- engine/instrument filter;
- preset list;
- sound/full preset type;
- load;
- save;
- save full;
- rename;
- delete;
- refresh;
- read-only state;
- status/error message.

## 3.3 VoxContentBrowser

Generic browser foundation for future:

- samples;
- wavetables;
- impulse responses;
- modules;
- generator profiles.

---

# 4. Instrument and Part system

## 4.1 VoxInstrumentRack

Container for instrument slots / Parts.

Responsibilities:

- selection;
- ordering;
- enable;
- mute;
- solo;
- lock;
- MIDI channel;
- output destination;
- activity;
- missing/incompatible module state.

## 4.2 VoxInstrumentSlot

One rack row/card.

Displays:

- slot/part number;
- instrument identity;
- channel;
- activity;
- enable;
- mute/solo;
- level/pan where appropriate;
- lock;
- selected state.

## 4.3 VoxPartHeader

Header of the currently selected Part:

- instrument name;
- preset;
- MIDI channel;
- mute;
- solo;
- enabled;
- lock;
- level;
- pan;
- output.

---

# 5. Generic parameter controls

## 5.1 VoxKnob

Variants:

- Small
- Normal
- Large

Capabilities:

- current value;
- default value;
- unit;
- modulation amount;
- automation/modulation state;
- reset;
- fine adjustment;
- tooltip.

## 5.2 VoxLinearSlider

For parameters where position is easier to read linearly.

Orientation:

- horizontal;
- vertical.

## 5.3 VoxFader

Dedicated mixer-level control.

Must support:

- dB scale;
- unity marker;
- current value;
- optional integrated meter.

## 5.4 VoxValueField

Text/numeric representation of a parameter.

Must allow direct entry where safe.

## 5.5 VoxChoiceControl

Choice values such as:

- waveform;
- filter type;
- sync division;
- routing destination;
- MIDI mode.

Visual forms:

- ComboBox;
- segmented switch;
- compact popup selector.

---

# 6. Synthesizer elements

These components are used by Bass, Lead, Acid, Atmos and future synth engines.

## 6.1 VoxOscillatorPanel

Contains oscillator-related controls.

Possible controls:

- waveform;
- morph;
- tune;
- octave;
- semitone;
- fine tune;
- phase;
- sync;
- FM;
- ring modulation;
- noise;
- unison;
- voices;
- detune;
- stereo spread;
- oscillator mix.

Only parameters supported by the engine are shown.

## 6.2 VoxWaveformDisplay

A generic oscillator waveform visualization.

Modes:

- generated waveform preview;
- wavetable position preview;
- sample waveform where appropriate.

Must not display fake realtime behaviour.

## 6.3 VoxWaveformSelector

Selection of waveform type.

Common values may include:

- sine;
- triangle;
- saw;
- square/pulse;
- noise;
- engine-specific forms.

It is a semantic selector, not a row of unrelated buttons.

## 6.4 VoxFilterPanel

Contains:

- filter type;
- cutoff;
- resonance;
- drive;
- key tracking;
- envelope amount;
- slope when available.

## 6.5 VoxFilterResponse

Graphical frequency-response representation.

Requirements:

- current cutoff;
- resonance;
- filter type;
- slope;
- updates from actual parameter state.

## 6.6 VoxEnvelopeEditor

Generic envelope editor.

Minimum ADSR:

- attack;
- decay;
- sustain;
- release.

Extended forms may add:

- hold;
- delay;
- curve;
- loop;
- velocity amount.

Used for:

- amplitude;
- filter;
- pitch;
- modulation envelopes.

## 6.7 VoxPitchEnvelope

Specialised compact envelope for transient pitch movement.

Useful for:

- psy bass;
- kick;
- percussion;
- FX.

## 6.8 VoxUnisonPanel

Contains:

- voices;
- detune;
- spread;
- phase/randomisation.

## 6.9 VoxDrivePanel

Generic non-linear stage:

- amount/drive;
- character/type;
- mix;
- output compensation where applicable.

---

# 7. Acid / 303 workflow

## 7.1 VoxAcidPanel

Engine-specific composition of generic components.

Must expose supported Acid controls:

- waveform;
- cutoff;
- resonance;
- envelope amount;
- decay;
- accent;
- slide;
- drive;
- output.

## 7.2 VoxAccentControl

Semantic accent control used by acid/sequencer/percussion workflows.

## 7.3 VoxSlideControl

Displays/enables glide/slide behaviour and slide time where the engine supports it.

---

# 8. Bass workflow

## 8.1 VoxBassPanel

Composed from:

- oscillator;
- amp envelope;
- pitch envelope;
- filter;
- drive;
- output;
- glide where supported.

The UI must represent the actual Psy Bass engine parameters rather than a generic synth mock-up.

---

# 9. Lead synthesizer workflow

## 9.1 VoxLeadPanel

Composed from:

- oscillator/morph;
- sync;
- FM;
- ring modulation;
- noise;
- unison;
- detune;
- filter;
- filter envelope;
- ADSR;
- drive;
- output.

## 9.2 VoxVoiceActivity

Optional polyphony/voice-state visualisation.

Must be low-cost and fed through a safe UI snapshot, never by directly reading mutable realtime voice objects.

---

# 10. Atmos / texture workflow

## 10.1 VoxAtmosPanel

Semantic controls:

- blend;
- brightness;
- motion;
- evolution;
- attack;
- decay;
- sustain;
- release;
- space;
- spread;
- drift;
- resonance;
- texture;
- density;
- harmonics;
- output.

## 10.2 VoxTextureDisplay

Slow-moving visual representation of evolving texture.

It must communicate movement/evolution without becoming decorative noise.

---

# 11. Semantic FX instruments

## 11.1 VoxSemanticFxPanel

Supports event families:

- sweep;
- laser;
- riser;
- downlifter;
- impact;
- whoosh;
- zap;
- noise burst;
- metallic;
- alien.

Controls:

- family;
- tone;
- duration;
- brightness;
- motion;
- texture;
- drive;
- output.

## 11.2 VoxFxEventPreview

Visual shape/time preview for one-shot FX.

The horizontal axis represents event time.  
The display may visualise amplitude/pitch/filter evolution if actual engine data is available.

---

# 12. Sequencing and musical generation

## 12.1 VoxStepSequencer

Canonical grid sequencer.

Current model must support up to 64 steps.

Lanes:

- note;
- gate;
- velocity;
- accent;
- probability;
- ratchet;
- slide;
- gate width when exposed.

Controls:

- sequence length;
- timing/division;
- playhead;
- selection.

Editing operations:

- copy;
- paste;
- rotate left/right;
- reverse;
- shift left/right;
- transpose up/down;
- octave up/down;
- mutate;
- clear.

## 12.2 VoxPianoRoll

Full note editor.

Must provide:

- note blocks;
- note start;
- duration;
- pitch;
- velocity;
- selection;
- move;
- resize;
- delete;
- grid snapping;
- zoom/scroll;
- playhead;
- keyboard gutter.

Optional lower lanes:

- velocity;
- probability;
- accent;
- ratchet;
- slide;
- modulation.

A Part/instrument must edit its own sequence; the piano roll cannot silently edit another Part.

## 12.3 VoxArpeggiator

Canonical arp editor, not a collection of random buttons.

Sections:

**Mode**
- Up
- Down
- Up/Down
- Down/Up
- As Played
- Random
- Chord

**Timing**
- rate/division;
- gate;
- swing;
- sync/free where supported.

**Pitch**
- octave range;
- transpose;
- optional scale/key constraint.

**Pattern**
- pattern length;
- step enable;
- octave offset;
- velocity;
- tie;
- accent;
- probability.

**Playback**
- latch/hold;
- retrigger;
- restart behaviour.

If the DSP engine does not yet support an arp parameter, the UI must not expose a fake control.

## 12.4 VoxPatternGenerator

Generator UI:

- style/profile;
- seed;
- generate/regenerate;
- mutate selected;
- clear selected;
- target Part;
- deterministic status.

Current Trance Engine styles include:

- Dark Psy;
- Psytrance;
- Forest.

## 12.5 VoxMidiSourceSelector

Must represent canonical source modes:

- AUTO
- PIANO ROLL
- GENERATOR
- BOTH

The order must remain aligned with the engine/host parameter schema.

---

# 13. Modulation

## 13.1 VoxMacroPanel

Canonical bank of macro controls.

Per slot/Part:

- 8 macros;
- label;
- value;
- assignment;
- modulation/automation state.

## 13.2 VoxLfoEditor

Contains:

- waveform;
- rate;
- sync/free;
- phase;
- retrigger;
- polarity;
- amount;
- optional shape/skew.

Visual area displays the actual selected LFO shape.

## 13.3 VoxStepModulator

Step-based modulation source.

Controls:

- number of steps;
- values;
- rate;
- smoothing;
- bipolar/unipolar;
- randomise;
- rotate;
- reset.

## 13.4 VoxModulationMatrix

Rows represent routes:

- source;
- transform/polarity;
- destination;
- depth;
- enable/delete.

Supported source families are derived from the engine contract and currently include:

- macro;
- LFO;
- velocity;
- mod wheel;
- aftertouch;
- envelope;
- step modulation;
- sample & hold;
- random.

The matrix must not invent destinations. Destination choices come from the instrument descriptor/registry.

## 13.5 VoxModulationRing

Optional overlay around knobs showing modulation range separately from the base parameter value.

---

# 14. Keyboard and performance

## 14.1 VoxKeyboard

Styled MIDI keyboard.

Supports:

- mouse note input;
- pressed-note state;
- octave labels;
- MIDI activity.

## 14.2 VoxPitchBend

Pitch bend control.

## 14.3 VoxModWheel

Modulation wheel control.

## 14.4 VoxXYPad

Two-dimensional performance/modulation control.

Must expose explicit X and Y destination labels.

---

# 15. Routing and zones

## 15.1 VoxRoutingPanel

Per Part/slot:

- route mode;
- MIDI channel;
- input mode;
- output destination;
- transpose;
- layer/channel routing.

## 15.2 VoxZoneRangeEditor

Graphical editor for:

- key low/high;
- velocity low/high.

Must show handles and active range clearly.

## 15.3 VoxRoutingMatrix

Used when routing grows beyond simple dropdowns.

Rows/columns must have explicit source/destination identities.

---

# 16. Mixer

## 16.1 VoxMixerStrip

Generic channel strip:

- name;
- activity;
- mute;
- solo;
- pan;
- fader;
- meter;
- output destination.

## 16.2 VoxLevelMeter

Variants:

- mono;
- stereo;
- peak;
- RMS/average where relevant.

Must distinguish:

- normal;
- warning;
- clipping.

## 16.3 VoxMasterStrip

Global output strip:

- gain;
- output meter;
- clip status;
- optional limiter state depending on product.

---

# 17. Effects

The Trance Engine currently defines:

- distortion;
- wavefolder;
- phaser;
- flanger;
- chorus;
- bitcrusher;
- delay;
- reverb.

## 17.1 VoxEffectSlot

One effect in a chain:

- effect identity;
- enabled/bypass;
- mix;
- amount;
- rate;
- character;
- drag/reorder when supported.

## 17.2 VoxEffectChain

Ordered effect modules.

Must visually communicate:

- signal order;
- bypass state;
- selected module;
- drag/reorder state if ordering is editable.

## 17.3 VoxDelayDisplay

Optional time/feedback visualisation.

## 17.4 VoxReverbDisplay

Optional decay/space visualisation.

No fake spectrum or waveform animation.

---

# 18. Waveform and sample editing

Required primarily for Vox Drums Engine and sample-based future instruments.

## 18.1 VoxWaveform

Generic non-editing waveform display.

Supports:

- full waveform;
- playhead;
- selection;
- markers.

## 18.2 VoxSampleEditor

Editable waveform workspace:

- start;
- end;
- trim;
- loop start/end;
- fades;
- zoom;
- scroll;
- transient markers;
- playback cursor.

## 18.3 VoxSliceEditor

For slicing workflows:

- slice markers;
- add/remove;
- selected slice;
- optional MIDI mapping.

---

# 19. Vox Drums Engine elements

## 19.1 VoxDrumPad

Displays:

- pad identity;
- instrument/sample name;
- activity/velocity;
- mute;
- solo;
- selected state.

Optional interaction:

- trigger on click;
- drag sample onto pad.

## 19.2 VoxDrumGrid

Container of drum pads.

## 19.3 VoxDrumVoiceEditor

Per-pad editing:

- sample;
- pitch;
- envelope;
- filter;
- drive;
- pan;
- level;
- output;
- choke group.

## 19.4 VoxVelocityEditor

Graphical velocity-lane editor.

## 19.5 VoxChokeGroupEditor

Defines mutual-exclusion groups for percussion.

## 19.6 VoxDrumSequencer

Multi-row sequencer where rows correspond to pads/instruments.

---

# 20. Analysis and metering

## 20.1 VoxSpectrumAnalyzer

Frequency spectrum.

Requirements:

- frequency axis;
- level axis;
- readable grid;
- current trace;
- optional peak/average trace;
- scalable refresh rate.

## 20.2 VoxOscilloscope

Time-domain waveform for diagnostics/synthesis when useful.

## 20.3 VoxStereoScope

Stereo imaging visualization.

Possible forms:

- vectorscope;
- goniometer;
- correlation meter.

## 20.4 VoxTransferCurve

Used for:

- compressor;
- limiter;
- saturation;
- waveshaper.

Displays input/output relationship and current operating point when available.

---

# 21. Mastering Engine elements

## 21.1 VoxModuleChain

Ordered mastering processors.

States:

- selected;
- enabled;
- bypassed;
- unavailable;
- processing/activity.

## 21.2 VoxSpectrumWorkspace

Larger mastering-oriented spectrum/analyzer area.

May contain:

- input trace;
- output trace;
- reference trace;
- EQ response overlay.

## 21.3 VoxLoudnessMeter

Must support only metrics actually implemented by the mastering engine.

Potential visual groups:

- Momentary;
- Short-Term;
- Integrated;
- Loudness Range.

## 21.4 VoxTruePeakMeter

Dedicated true-peak visualization.

## 21.5 VoxGainReductionMeter

For compressor/limiter/dynamics modules.

## 21.6 VoxCorrelationMeter

Displays stereo phase correlation.

## 21.7 VoxReferencePanel

For A/B/reference workflow:

- A/B;
- reference source;
- level matching;
- bypass;
- compare state.

## 21.8 VoxBeforeAfterControl

Global before/after comparison.

---

# 22. Graph language

All graphs share:

- `bg.graph`;
- subtle grid;
- family typography;
- shared cursor/selection style;
- consistent active cyan;
- warning/danger only for semantic warnings;
- no decorative animation unrelated to data.

Generic graph base:

`VoxGraph`

Derived visual components:

- `VoxWaveform`
- `VoxFilterResponse`
- `VoxEnvelopeEditor`
- `VoxSpectrumAnalyzer`
- `VoxTransferCurve`
- `VoxLfoEditor`
- `VoxStereoScope`

---

# 23. Menus, dialogs and overlays

## 23.1 VoxContextMenu

Used for:

- reset;
- MIDI learn;
- assign modulation;
- copy/paste;
- remove;
- advanced parameter actions.

## 23.2 VoxTooltip

Must expose:

- parameter name;
- value/unit;
- concise description where useful.

## 23.3 VoxDialog

For non-realtime actions:

- save preset;
- rename;
- destructive confirmation;
- missing module diagnostics;
- settings.

## 23.4 VoxToast / Notification

Short non-blocking status:

- preset saved;
- copied;
- module loaded;
- error.

---

# 24. Empty, loading and failure states

Complex components must explicitly define:

- empty;
- loading where applicable;
- missing resource;
- missing module;
- incompatible module/version;
- disabled;
- no selection;
- no data.

Do not render a broken normal-state widget when backing data is unavailable.

---

# 25. Shared vs product-specific ownership

## Shared `vox-ui`

Should contain:

- design tokens;
- typography;
- LookAndFeel;
- panels;
- buttons;
- knobs;
- sliders/faders;
- toggles;
- combo boxes;
- tabs;
- value fields;
- labels;
- meters base;
- graph base;
- waveform base;
- envelope base;
- filter-response base;
- keyboard base;
- generic mixer strip;
- generic effect slot/chain visual primitives;
- tooltip/menu/dialog visual grammar.

## Trance/Electronic product layer

- Instrument Rack;
- Part Header;
- Bass Panel;
- Acid Panel;
- Lead Panel;
- Atmos Panel;
- Semantic FX Panel;
- Piano Roll;
- Step Sequencer;
- Arpeggiator;
- Pattern Generator;
- Modulation Matrix;
- Zone editor composition;
- Trance-specific Effect Chain composition.

## Drums product layer

- Drum Pad/Grid;
- Drum Voice Editor;
- Sample Editor composition;
- Slice Editor;
- Choke Group;
- Drum Sequencer.

## Mastering product layer

- Module Chain;
- Loudness workflow;
- True Peak workflow;
- reference/A-B workflow;
- mastering analyzer workspace.

---

# 26. Implementation rule

A complex widget should be **composed**, not painted as one giant monolithic component.

Example:

```text
VoxLeadPanel
├── VoxOscillatorPanel
│   ├── VoxWaveformSelector
│   ├── VoxKnob: Morph
│   ├── VoxToggle: Sync
│   ├── VoxKnob: FM
│   └── VoxUnisonPanel
├── VoxFilterPanel
│   ├── VoxFilterResponse
│   ├── VoxKnob: Cutoff
│   └── VoxKnob: Resonance
├── VoxEnvelopeEditor
└── VoxDrivePanel
```

This rule applies to every product.

---

# 27. Data truth rule

Visual components must reflect actual engine state.

Forbidden:

- fake waveform animation;
- fake filter curves;
- fake spectrum;
- fake meters;
- controls with no backing parameter;
- visual “modulation” not tied to a real route;
- decorative playheads not synchronized to transport.

If the engine does not currently provide required data, the component remains unimplemented or shows an explicit unavailable state.

---

# 28. Realtime boundary

No complex visualization may read mutable DSP structures directly.

Allowed flow:

```text
DSP/audio thread
    ↓ lock-free/atomic snapshot
UI bridge
    ↓ timer/message thread
visual component
```

UI repaint rate is independent from audio block rate.

---

# 29. Definition of Done for a catalog element

An element is complete only when:

1. purpose is defined;
2. ownership is defined: shared vs product-specific;
3. supported states are defined;
4. backing data/parameters are identified;
5. interactions are defined;
6. scaling works;
7. visual tokens come from VOX UI;
8. no realtime-safety violation exists;
9. empty/error states exist where required;
10. it has no fake functionality.

---

# 30. Canonical product map

```text
VOX UI CORE
├── Controls
├── Panels
├── Graphs
├── Meters
├── Keyboard
├── Mixer primitives
└── Interaction grammar

VOX ELECTRONIC ENGINE
├── Instrument Rack
├── Bass
├── Kick
├── Acid
├── Lead
├── Atmos
├── Semantic FX
├── Piano Roll
├── Step Sequencer
├── Arpeggiator
├── Pattern Generator
├── Modulation Matrix
├── Routing / Zones
└── FX Chain

VOX DRUMS ENGINE
├── Drum Grid
├── Drum Pad
├── Sample Editor
├── Drum Voice
├── Velocity Editor
├── Choke Groups
└── Drum Sequencer

VOX MASTERING ENGINE
├── Module Chain
├── Spectrum
├── Loudness
├── True Peak
├── Gain Reduction
├── Stereo / Correlation
├── Transfer Curves
└── A/B Reference
```

---

## Canonical rule

**Do not design a screen first and invent controls inside it.**

The implementation order is:

```text
design tokens
→ primitive
→ audio primitive
→ workflow widget
→ page
→ product
```

If a new conceptual UI element is required, it must be added to this catalog before multiple incompatible local implementations appear.
