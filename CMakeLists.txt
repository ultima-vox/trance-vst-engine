cmake_minimum_required(VERSION 3.22)
project(VstEngine VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 9.0.1
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Modular architecture (issue #11).
#
#   lower-level modules -> engine/composition -> VST3 shell
#
# Each subsystem is an independent target with explicit dependencies. The
# shell target (VstEngine) only provides JUCE VST3 integration, bridges the
# APVTS/host transport/MIDI/audio and composes the editor.
# ---------------------------------------------------------------------------
set(VST_LIBS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)

# core: shared low-level identifiers/constants (header-only).
add_library(vst_core INTERFACE)
target_include_directories(vst_core INTERFACE ${VST_LIBS_DIR})

# sequence: canonical Sequence/Step/timing model + serialization.
add_library(vst_sequence STATIC
  libs/sequence/Sequence.cpp libs/sequence/Sequence.h
  libs/sequence/RealtimeSequenceBridge.h)
target_include_directories(vst_sequence PUBLIC ${VST_LIBS_DIR})
target_link_libraries(vst_sequence PUBLIC juce::juce_audio_basics)

# transport: pure PPQ/grid musical-time math (header-only).
add_library(vst_transport INTERFACE)
target_include_directories(vst_transport INTERFACE ${VST_LIBS_DIR})

# midi: source-mode policy, MIDI export, generated-note scheduling.
add_library(vst_midi STATIC
  libs/midi/MidiExport.cpp libs/midi/MidiExport.h
  libs/midi/GeneratedNoteScheduler.cpp libs/midi/GeneratedNoteScheduler.h
  libs/midi/PartMidiRouter.cpp libs/midi/PartMidiRouter.h
  libs/midi/SourceSelector.h)
target_include_directories(vst_midi PUBLIC ${VST_LIBS_DIR})
target_link_libraries(vst_midi
  PUBLIC juce::juce_audio_basics vst_sequence vst_transport)

# bass: psy-bass DSP voice (oscillator/pitch env/amp env/filter/drive/glide).
add_library(vst_bass STATIC
  libs/bass/PsyBassVoice.cpp libs/bass/PsyBassVoice.h)
target_include_directories(vst_bass PUBLIC ${VST_LIBS_DIR})
target_link_libraries(vst_bass PUBLIC
  juce::juce_audio_basics juce::juce_audio_processors)

# kick: synthesized kick DSP voice (pitch sweep, body/tail, click, drive,
# clip, transient, sub layer). Issue #11 PHASE 5.
add_library(vst_kick STATIC
  libs/kick/KickSynth.cpp libs/kick/KickSynth.h)
target_include_directories(vst_kick PUBLIC ${VST_LIBS_DIR})
target_link_libraries(vst_kick PUBLIC juce::juce_audio_basics)

# match: kick/bass low-end analysis + bounded matching adjustments. Issue
# #11 PHASE 6. Message-thread only (editor callback / tests), never RT.
add_library(vst_match STATIC
  libs/match/KickBassMatch.cpp libs/match/KickBassMatch.h)
target_include_directories(vst_match PUBLIC ${VST_LIBS_DIR})
target_link_libraries(vst_match PUBLIC
  juce::juce_audio_basics juce::juce_audio_processors vst_bass vst_kick)

# preset: preset format, filesystem, factory/user distinction, migration.
add_library(vst_preset STATIC
  libs/preset/PresetManager.cpp libs/preset/PresetManager.h)
target_include_directories(vst_preset PUBLIC ${VST_LIBS_DIR})
target_link_libraries(vst_preset
  PUBLIC juce::juce_core juce::juce_data_structures vst_sequence vst_core)

# generator: deterministic seed-based musical pattern generation.
add_library(vst_generator STATIC
  libs/generator/PatternGenerator.cpp libs/generator/PatternGenerator.h)
target_include_directories(vst_generator PUBLIC ${VST_LIBS_DIR})

# ui: reusable JUCE components and tab pages.
add_library(vst_ui STATIC
  libs/ui/common/UiComponents.cpp libs/ui/common/UiComponents.h
  libs/ui/MainNavigation.cpp libs/ui/MainNavigation.h
  libs/ui/GlobalHeader.cpp libs/ui/GlobalHeader.h
  libs/ui/BassPanel.cpp libs/ui/BassPanel.h
  libs/ui/KickPanel.cpp libs/ui/KickPanel.h
  libs/ui/StepSequencer.cpp libs/ui/StepSequencer.h
  libs/ui/MatchPanel.cpp libs/ui/MatchPanel.h
  libs/ui/PresetBrowser.cpp libs/ui/PresetBrowser.h
  libs/ui/SettingsPanel.cpp libs/ui/SettingsPanel.h)
target_include_directories(vst_ui PUBLIC ${VST_LIBS_DIR})
target_link_libraries(vst_ui PUBLIC
  juce::juce_gui_basics juce::juce_audio_utils juce::juce_audio_processors
  vst_sequence vst_match vst_preset)

# ---------------------------------------------------------------------------
# VST3 shell: composition + host integration only.
# ---------------------------------------------------------------------------
juce_add_plugin(VstEngine
  COMPANY_NAME "Ultima Vox"
  BUNDLE_ID "com.ultimavox.vstengine"
  PLUGIN_MANUFACTURER_CODE UlVx
  PLUGIN_CODE DkP1
  FORMATS VST3
  PRODUCT_NAME "VST Engine"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT TRUE
  IS_MIDI_EFFECT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES "Instrument" "Synth")

juce_generate_juce_header(VstEngine)

target_sources(VstEngine PRIVATE
  apps/vst3/PluginProcessor.cpp apps/vst3/PluginProcessor.h
  apps/vst3/PluginEditor.cpp apps/vst3/PluginEditor.h)

target_compile_definitions(VstEngine PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(VstEngine PRIVATE
  juce::juce_audio_utils juce::juce_dsp
  vst_core vst_sequence vst_transport vst_midi
  vst_bass vst_kick vst_match vst_preset vst_generator vst_ui)

if(MSVC)
  target_compile_options(VstEngine PRIVATE /W4 /permissive-)
else()
  target_compile_options(VstEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Tests: one executable per module library, mapped 1:1 to module targets, plus
# integration tests for cross-module state compatibility.
# ---------------------------------------------------------------------------
enable_testing()

add_executable(sequence_tests tests/sequence/SequenceTests.cpp)
target_link_libraries(sequence_tests PRIVATE vst_sequence)
add_test(NAME sequence_tests COMMAND sequence_tests)

add_executable(transport_sync_tests tests/transport/TransportSyncTests.cpp)
target_link_libraries(transport_sync_tests PRIVATE vst_transport)
add_test(NAME transport_sync_tests COMMAND transport_sync_tests)

add_executable(midi_source_tests tests/midi/MidiSourceTests.cpp)
target_link_libraries(midi_source_tests PRIVATE vst_midi)
add_test(NAME midi_source_tests COMMAND midi_source_tests)

add_executable(midi_export_tests tests/midi/MidiExportTests.cpp)
target_link_libraries(midi_export_tests PRIVATE vst_midi)
add_test(NAME midi_export_tests COMMAND midi_export_tests)

add_executable(generated_note_scheduler_tests
  tests/midi/GeneratedNoteSchedulerTests.cpp)
target_link_libraries(generated_note_scheduler_tests PRIVATE vst_midi)
add_test(NAME generated_note_scheduler_tests
         COMMAND generated_note_scheduler_tests)

add_executable(part_midi_router_tests tests/midi/PartMidiRouterTests.cpp)
target_link_libraries(part_midi_router_tests PRIVATE vst_midi vst_kick)
add_test(NAME part_midi_router_tests COMMAND part_midi_router_tests)

add_executable(psy_bass_tests tests/bass/PsyBassTests.cpp)
target_link_libraries(psy_bass_tests PRIVATE vst_bass)
add_test(NAME psy_bass_tests COMMAND psy_bass_tests)

add_executable(kick_tests tests/kick/KickSynthTests.cpp)
target_link_libraries(kick_tests PRIVATE vst_kick)
add_test(NAME kick_tests COMMAND kick_tests)

add_executable(match_tests tests/match/KickBassMatchTests.cpp)
target_link_libraries(match_tests PRIVATE vst_match)
add_test(NAME match_tests COMMAND match_tests)

add_executable(preset_tests tests/preset/PresetTests.cpp)
target_link_libraries(preset_tests PRIVATE vst_preset)
add_test(NAME preset_tests COMMAND preset_tests)

add_executable(generator_tests tests/generator/PatternGeneratorTests.cpp)
target_link_libraries(generator_tests PRIVATE vst_generator)
add_test(NAME generator_tests COMMAND generator_tests)

add_executable(state_round_trip_tests
  tests/integration/StateRoundTripTests.cpp)
target_link_libraries(state_round_trip_tests PRIVATE vst_preset vst_sequence)
add_test(NAME state_round_trip_tests COMMAND state_round_trip_tests)

add_executable(editor_smoke_tests tests/ui/EditorSmokeTests.cpp)
target_include_directories(editor_smoke_tests PRIVATE
  apps/vst3
  ${VST_LIBS_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/VstEngine_artefacts/JuceLibraryCode
  ${juce_SOURCE_DIR}/modules)
target_link_libraries(editor_smoke_tests PRIVATE VstEngine)
add_test(NAME editor_smoke_tests COMMAND editor_smoke_tests)
