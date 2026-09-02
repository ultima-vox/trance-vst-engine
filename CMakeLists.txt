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
  src/PluginProcessor.cpp src/PluginProcessor.h
  src/PluginEditor.cpp src/PluginEditor.h
  src/dsp/PsyBassVoice.cpp src/dsp/PsyBassVoice.h
  src/generator/PatternGenerator.cpp src/generator/PatternGenerator.h
  src/generator/Sequence.cpp src/generator/Sequence.h)

target_compile_definitions(VstEngine PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(VstEngine PRIVATE juce::juce_audio_utils juce::juce_dsp)

if(MSVC)
  target_compile_options(VstEngine PRIVATE /W4 /permissive-)
else()
  target_compile_options(VstEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_executable(pattern_generator_tests
  tests/PatternGeneratorTests.cpp
  src/generator/PatternGenerator.cpp)
target_include_directories(pattern_generator_tests PRIVATE src)
add_test(NAME pattern_generator_tests COMMAND pattern_generator_tests)

add_executable(sequence_tests
  tests/SequenceTests.cpp
  src/generator/Sequence.cpp)
target_include_directories(sequence_tests PRIVATE src)
add_test(NAME sequence_tests COMMAND sequence_tests)
