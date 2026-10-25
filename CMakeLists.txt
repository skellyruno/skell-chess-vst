cmake_minimum_required(VERSION 3.22)
project(Checkmate VERSION 1.0.0 LANGUAGES C CXX)
option(CHECKMATE_CORE_ONLY "Build dependency-free engine tests only" OFF)
include(CTest)
if(BUILD_TESTING)
  add_executable(CheckmateCoreTests tests/CoreTests.cpp)
  target_include_directories(CheckmateCoreTests PRIVATE Source)
  target_compile_features(CheckmateCoreTests PRIVATE cxx_std_17)
  add_test(NAME CheckmateCore COMMAND CheckmateCoreTests)
endif()
if(CHECKMATE_CORE_ONLY)
  return()
endif()
# Based on skell-vst-template-main. Keep the supplied pinned JUCE release.
include(FetchContent)
set(JUCE_PATH "" CACHE PATH "Optional path to a local JUCE 7+ checkout")
if(JUCE_PATH)
  if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE_PATH must point to a JUCE checkout containing CMakeLists.txt")
  endif()
  add_subdirectory("${JUCE_PATH}" JUCE)
else()
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()
juce_add_plugin(Checkmate
  COMPANY_NAME "Skell Audio"
  BUNDLE_ID "audio.skell.checkmate"
  PLUGIN_MANUFACTURER_CODE SkAu
  PLUGIN_CODE CkMt
  FORMATS VST3 Standalone
  PRODUCT_NAME "Checkmate"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT TRUE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)
juce_generate_juce_header(Checkmate)
target_sources(Checkmate PRIVATE
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/Synth.h Source/Chess.h Source/Archive.h)
target_compile_features(Checkmate PRIVATE cxx_std_17)
target_compile_definitions(Checkmate PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_compile_definitions(Checkmate PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(Checkmate PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_extra
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if(BUILD_TESTING)
  add_executable(CheckmatePluginTests tests/PluginTests.cpp)
  target_link_libraries(CheckmatePluginTests PRIVATE Checkmate juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_extra)
  target_compile_definitions(CheckmatePluginTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_include_directories(CheckmatePluginTests PRIVATE Source "${CMAKE_CURRENT_BINARY_DIR}/Checkmate_artefacts/JuceLibraryCode")
  target_compile_features(CheckmatePluginTests PRIVATE cxx_std_17)
  add_test(NAME CheckmatePlugin COMMAND CheckmatePluginTests)
endif()
