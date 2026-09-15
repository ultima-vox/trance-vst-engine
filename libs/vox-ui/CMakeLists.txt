add_library(vox_ui STATIC
  VoxLookAndFeel.cpp VoxLookAndFeel.h
  VoxComponents.cpp VoxComponents.h
  Tokens.h
  Typography.h)

target_include_directories(vox_ui PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(vox_ui PUBLIC
  juce::juce_gui_basics
  juce::juce_graphics)

target_compile_features(vox_ui PUBLIC cxx_std_20)
