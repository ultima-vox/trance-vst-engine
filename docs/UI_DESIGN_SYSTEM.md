# VOX ELECTRONIC ENGINE — UI DESIGN SYSTEM v1.1

**Статус:** CANON / обязательная спецификация для UI  
**Назначение:** единая дизайн-система JUCE-интерфейса VST3/Standalone  
**Проект:** `vox-electronic-engine`  
**Версия:** 1.1

---

## 1. Цель

Эта спецификация задаёт единый визуальный язык, структуру компонентов и правила реализации интерфейса VOX Electronic Engine.

Основные цели:

- профессиональный внешний вид уровня коммерческого audio software;
- единообразие всех инструментов и вкладок;
- отсутствие локальных «самодельных» стилей;
- масштабируемость UI;
- повторное использование компонентов;
- разделение shared VOX UI и product UI;
- разделение визуального слоя и DSP;
- отсутствие работы UI в realtime audio thread;
- возможность добавлять новые инструменты без переделки базовой UI-архитектуры.

---

## 2. Архитектурный канон репозитория

Это раздел имеет приоритет над любыми старыми примерами путей.

### 2.1 Канонические директории

```text
libs/
├── vox-ui/                     # shared VOX UI framework
│   ├── CMakeLists.txt          # target: vox_ui
│   ├── Tokens.h
│   ├── Typography.h
│   ├── VoxLookAndFeel.h/.cpp
│   └── components/
│       ├── VoxPanel.h/.cpp
│       ├── VoxKnob.h/.cpp
│       ├── VoxButton.h/.cpp
│       ├── VoxComboBox.h/.cpp
│       ├── VoxGraph.h/.cpp
│       ├── VoxMeter.h/.cpp
│       ├── VoxKeyboard.h/.cpp
│       └── ...
│
└── ui/                         # Vox Electronic Engine product UI
    ├── CMakeLists.txt          # target: vst_ui
    ├── GlobalHeader.h/.cpp
    ├── InstrumentRack.h/.cpp
    ├── MainNavigation.h/.cpp
    ├── PresetBrowser.h/.cpp
    ├── panels/
    │   ├── BassPanel.h/.cpp
    │   ├── AcidPanel.h/.cpp
    │   ├── LeadPanel.h/.cpp
    │   ├── AtmosPanel.h/.cpp
    │   └── FxPanel.h/.cpp
    ├── editors/
    │   ├── StepSequencer.h/.cpp
    │   ├── PianoRoll.h/.cpp
    │   ├── Arpeggiator.h/.cpp
    │   ├── ModulationMatrix.h/.cpp
    │   └── ZoneRangeEditor.h/.cpp
    └── pages/
        ├── SoundPage.h/.cpp
        ├── PatternPage.h/.cpp
        ├── RoutingPage.h/.cpp
        ├── ZonesPage.h/.cpp
        ├── MacrosPage.h/.cpp
        └── AdvancedPage.h/.cpp
```

### 2.2 Запрещённый путь

```text
Source/UI/
```

**`Source/UI` не является допустимым путём в этом репозитории. Агент не должен его создавать.**

Старые примеры `Source/UI/Design`, `Source/UI/Components`, `Source/UI/Pages` и `Source/UI/Panels` считаются отменёнными.

### 2.3 Dependency direction

Разрешено:

```text
apps/* / plugin shell
        ↓
      vst_ui
        ↓
      vox_ui
        ↓
    JUCE GUI
```

Запрещено:

```text
vox_ui -> vst_ui
vox_ui -> product DSP
vox_ui -> PluginProcessor
DSP realtime path -> vox_ui
```

### 2.4 Namespace

Shared UI использует:

```cpp
namespace vox::ui
```

Product UI может использовать собственный namespace проекта, но не должен помещать product-specific классы в `vox::ui`.

### 2.5 Что относится в `libs/vox-ui`

Только элементы, пригодные без продуктовой логики для нескольких VOX-продуктов:

- tokens;
- typography;
- `VoxLookAndFeel`;
- panel;
- knob;
- button/icon button;
- toggle/switch;
- combo box;
- tabs/segmented control;
- labels/value fields;
- graph base;
- waveform base;
- envelope graph/editor primitives;
- filter response;
- meter base;
- XY pad;
- keyboard;
- tooltip/context-menu/dialog visual grammar.

### 2.6 Что относится в `libs/ui`

Electronic Engine product UI:

- Instrument Rack / Part Header;
- Psy Bass / Acid / Lead / Atmos / FX panels;
- Piano Roll;
- Step Sequencer;
- Arpeggiator/Phrase editor;
- Pattern Generator UI;
- Modulation Matrix composition;
- Routing/Zones pages;
- Electronic Engine FX-chain composition.

Правило:

> Не переносить сложный product widget в `vox-ui` только потому, что он потенциально может пригодиться позже. Сначала нужен второй реальный consumer.

---

## 3. Визуальное направление

**Dark futuristic / professional electronic-music workstation**

Ключевые признаки:

- глубокий тёмно-синий/чёрный фон;
- холодный cyan как основной interaction accent;
- высокая информационная плотность;
- тонкие рамки;
- небольшие радиусы;
- минимум декоративного шума;
- glow используется точечно;
- интерфейс не должен выглядеть как game HUD;
- интерфейс не должен выглядеть как generic JUCE demo;
- основной акцент — читаемость, контроль и визуальная иерархия.

---

## 4. Design tokens

Все цвета, размеры, интервалы и радиусы объявляются централизованно в:

```text
libs/vox-ui/Tokens.h
```

### 4.1 Цвета

```text
bg.window          #06121D
bg.panel           #0B1925
bg.panelRaised     #102230
bg.control         #0A1722
bg.graph           #051019

border.default     #1D3B50
border.subtle      #122C3D

accent.primary     #00DDF5
accent.hover       #42EEFF
accent.dim         #087B92

text.primary       #E9F3FA
text.secondary     #9AB2C5
text.muted         #587487

danger             #E4425D
success            #32D296
warning            #E3B341
```

Правила:

- cyan = interaction, selection, focus, active value;
- instrument identity hues допустимы только для artwork/graph content/specialized identity;
- `danger` используется только для destructive/error/Panic semantics;
- новый цвет сначала добавляется в token set, затем используется.

---

## 5. Сетка, интервалы и радиусы

Базовая единица:

```text
4 px
```

Spacing:

```text
space.1 = 4
space.2 = 8
space.3 = 12
space.4 = 16
space.5 = 20
space.6 = 24
space.8 = 32
```

Радиусы:

```text
radius.small   = 4 px
radius.medium  = 6 px
radius.large   = 8 px
```

Основные размеры должны быть кратны 4 px, если нет технической причины для исключения.

---

## 6. Базовый размер и масштабирование

Reference canvas:

```text
1440 × 1080
```

Scale targets:

```text
75%
100%
125%
150%
200%
```

Нельзя строить весь UI набором абсолютных координат. Используются relative layout calculations, reusable helpers, nested rectangles и scale-aware geometry.

---

## 7. Типографика

```text
Instrument Title   24–28 px  SemiBold
Section Title      13–14 px  SemiBold / uppercase
Control Label      11–12 px  Medium
Value/Secondary    10–11 px  Regular
```

Typography definitions находятся в:

```text
libs/vox-ui/Typography.h
```

Не вводить локальные случайные размеры шрифта.

---

## 8. Shared component library

Минимальный shared набор:

```text
VoxLookAndFeel
VoxPanel
VoxKnob
VoxButton
VoxIconButton
VoxToggle
VoxComboBox
VoxTabBar / VoxSegmentedControl
VoxMeter
VoxGraph
VoxWaveform
VoxEnvelopeGraph
VoxFilterResponse
VoxValueField
VoxParameterLabel
VoxSectionHeader
VoxKeyboard
VoxXYPad
VoxTooltip
```

Важно: `VoxInstrumentSlot`, `BassPanel`, `AcidPanel`, `LeadPanel`, `Arpeggiator`, `PianoRoll` и `StepSequencer` **не являются автоматически shared-компонентами**. Они принадлежат `libs/ui`, пока не появится подтверждённый второй consumer.

---

## 9. VoxPanel

```text
background : bg.panel
border     : 1 px border.default
radius     : 6 px
padding    : 12 px
```

Используется как shared container. Семантика секции задаётся product UI.

---

## 10. VoxKnob

Размеры:

```text
Small   36 × 36
Normal  48 × 48
Large   64 × 64
```

Состав:

- inactive arc;
- active value arc;
- dark body;
- position marker;
- label;
- value;
- optional modulation ring.

Диапазон дуги: около `270°`.

Состояния:

```text
Normal
Hover
Pressed
Focused
Active
Disabled
Modulated
```

Ordinary parameter knobs используют cyan; product identity не должна создавать новый knob style.

---

## 11. VoxComboBox

```text
height      32 px
background  bg.control
border      border.default
radius      4 px
padding     8 px
```

Состояния:

```text
Normal   border.default
Hover    accent.dim
Focused  accent.primary
Disabled text.muted / lowered opacity
```

---

## 12. Buttons

Типы:

```text
Primary
Secondary
Toggle
Danger
Icon
```

Стандартная высота: `32 px`.

`Danger` используется для Panic/destructive действий, а не как декоративный accent.

---

## 13. VoxTabBar

Высота: `36 px`.

Active:

```text
background  accent.primary
text        bg.window
```

Inactive:

```text
background  bg.control
text        text.secondary
border      border.default
```

Product-level tabs (`SOUND`, `PATTERN`, `ROUTING`, ...) создаются в `libs/ui`, но визуально используют shared tab grammar.

---

## 14. Graph language

Shared `VoxGraph` задаёт визуальный язык для waveform/filter/envelope/modulation/analyzer primitives:

```text
background  bg.graph
grid        subtle / low-opacity
curve       accent.primary или tokenized content accent
curve width 1.5–2 px
nodes       6–8 px
```

Правила:

- график отображает реальные данные/параметры;
- no fake animation;
- axes используются только где полезны;
- specialized graphs строятся поверх shared graph grammar.

---

## 15. Keyboard

Shared base: `VoxKeyboard`.

Функциональную основу можно строить на `juce::MidiKeyboardComponent`, но внешний вид задаёт VOX UI.

```text
white keys  #EBEEF0
black keys  #081018
pressed     accent.primary
```

Electronic Engine footer composition находится в `libs/ui`, а не в `vox-ui`.

---

## 16. Общие состояния

Каждый интерактивный shared control поддерживает:

```text
Normal
Hover
Pressed
Focused
Active
Disabled
```

Пример:

```text
Normal:            border.default
Hover:             accent.dim
Focused / Active:  accent.primary
Disabled:          opacity ~0.35 + text.muted
```

---

## 17. Product shell

Electronic Engine product UI в `libs/ui` собирает shared primitives в shell:

```text
PluginEditor
├── GlobalHeader
├── InstrumentRack
├── InstrumentHeader
├── MainNavigation
└── ActivePage
```

Sound layout является product composition и может различаться между Psy Bass, Acid, Lead, Atmos и FX при сохранении общей grid/component grammar.

---

## 18. JUCE implementation rules

### 18.1 LookAndFeel

Shared кастомизация идёт через:

```cpp
class VoxLookAndFeel : public juce::LookAndFeel_V4
```

В том числе:

```text
drawRotarySlider
drawButtonBackground
drawComboBox
drawToggleButton
drawLinearSlider
drawPopupMenuItem
```

Specialized product widgets могут переопределять `paint()`, но обязаны использовать VOX tokens.

### 18.2 Parameter bindings

UI использует стандартные attachment-механизмы там, где это применимо, например:

```cpp
juce::AudioProcessorValueTreeState::SliderAttachment
```

UI не хранит независимую копию истины для DSP-параметров.

### 18.3 Realtime safety

UI никогда не должен:

- выполнять тяжёлые вычисления в audio callback;
- брать блокирующие mutex на audio thread;
- выделять память в realtime path;
- читать mutable DSP state небезопасным способом;
- инициировать disk I/O из audio thread.

Для visual state:

```text
DSP/audio thread
    ↓ atomics / lock-free snapshot
UI bridge
    ↓ timer/message thread
visual component
```

---

## 19. Layout rules

Запрещено использовать абсолютные координаты как основной подход.

Допустимо:

- nested rectangles;
- reusable layout helpers;
- flex/grid abstractions;
- proportional allocation;
- scale-aware geometry.

Пример:

```cpp
auto area = getLocalBounds().reduced(12);
auto row = area.removeFromTop(100);
layoutKnob(cutoff,    row.removeFromLeft(72));
layoutKnob(resonance, row.removeFromLeft(72));
```

---

## 20. Запрещённые практики

Агент НЕ ДОЛЖЕН:

- создавать `Source/UI`;
- создавать параллельную вторую UI-архитектуру;
- помещать product-specific классы в `libs/vox-ui` без второго consumer;
- добавлять новые цвета без token;
- использовать разные knob styles на разных страницах;
- использовать emoji вместо UI-icons;
- строить интерфейс целиком на hardcoded coordinates;
- локально менять typography;
- добавлять glow повсеместно;
- копировать default JUCE appearance;
- смешивать DSP-код и отрисовку;
- дублировать parameter state;
- выполнять UI-работу в realtime audio thread.

---

## 21. Иконки

- vector-based;
- SVG / `juce::Drawable`;
- единый stroke приблизительно `1.5–2 px`;
- без emoji;
- без случайных локальных наборов.

---

## 22. Accessibility / usability

Минимум:

- tooltip для controls;
- текстовое значение параметра;
- critical state не кодируется только цветом;
- единая drag sensitivity;
- double-click reset единообразен;
- Shift+drag = fine adjustment;
- disabled state очевиден;
- hit target не меньше визуальной control area.

---

## 23. Definition of Done — shared component

Shared компонент готов, если:

- расположен в `libs/vox-ui`;
- не зависит от product DSP/UI classes;
- использует tokens/typography;
- поддерживает необходимые states;
- scale-safe;
- не содержит локальной темы;
- не нарушает realtime safety;
- реально пригоден более чем одному workflow/product либо является базовым primitive.

---

## 24. Definition of Done — product widget/page

Product widget/page готов, если:

- расположен в `libs/ui`;
- использует `vox_ui` primitives;
- не дублирует shared LookAndFeel/tokens;
- корректно работает при resize/scale;
- controls связаны с реальными параметрами;
- отсутствуют fake graphs/controls;
- state/automation IDs/DSP behaviour не изменены UI-рефакторингом.

---

## 25. Этапы внедрения

### Phase UI-1 — Shared foundation

В `libs/vox-ui`:

```text
Tokens
Typography
VoxLookAndFeel
VoxPanel
VoxKnob
VoxButton
VoxComboBox
VoxTabBar
```

### Phase UI-2 — Product integration

В `libs/ui` подключить `vox_ui` и перевести существующие controls на shared tokens/LookAndFeel без big-bang rewrite.

### Phase UI-3 — Reference panel

Реализовать product `FilterPanel` в `libs/ui` на shared primitives.

### Phase UI-4 — Sound workflows

Psy Bass / Lead / Acid / Atmos / FX.

### Phase UI-5 — Editors

Pattern/Piano Roll/Arpeggiator/Step Sequencer/Modulation Matrix остаются в `libs/ui`.

### Phase UI-6 — Remaining pages

```text
Routing
Zones
Macros
Advanced
```

---

## 26. Инструкция агенту

Перед UI-задачей агент обязан:

1. Прочитать `UI_DESIGN_SYSTEM.md`, `UI_COMPONENT_CATALOG.md`, `UI_VISUAL_REFERENCE_SPEC.md`, `VOX_UI_FAMILY_ARCHITECTURE.md`.
2. Соблюдать канонические пути `libs/vox-ui` и `libs/ui`.
3. Не создавать `Source/UI`.
4. Проверить, существует ли нужный shared primitive до создания нового.
5. Новый reusable primitive сначала добавлять в `vox-ui`; новый product widget — в `libs/ui`.
6. Не переносить product widget в shared library без второго подтверждённого consumer.
7. Не смешивать DSP refactor и UI refactor без необходимости.
8. В PR указывать изменённые shared/product components, tokens, pages и проверенные scale factors.
9. После значимого UI checkpoint прикладывать screenshot/render.

---

## 27. Acceptance criteria v1.1

Design System считается внедрённой, когда:

- существует один `libs/vox-ui` target `vox_ui`;
- существует один product UI target `vst_ui` в `libs/ui`;
- `vst_ui` зависит от `vox_ui`, но не наоборот;
- отсутствует `Source/UI`;
- нет второй palette/theme;
- базовые controls используют `VoxLookAndFeel`;
- product pages используют shared primitives;
- scaling/resize не ломают layout;
- UI не нарушает realtime safety.

---

## 28. Каноническое правило

> `libs/vox-ui` = shared VOX UI framework.  
> `libs/ui` = Vox Electronic Engine product UI.  
> `apps/*` = тонкие plugin/standalone shells.  
> `Source/UI` = запрещённый legacy path.

Новый UI-код должен расширять эту архитектуру, а не создавать параллельную структуру.
