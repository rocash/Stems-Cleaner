cmake_minimum_required(VERSION 3.22)
project(StemCleanerPro VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 7.0.12
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(StemCleanerPro
  COMPANY_NAME "TuEstudio"
  BUNDLE_ID com.tuestudio.stemcleaner
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Tstu
  PLUGIN_CODE Stc1
  FORMATS VST3 Standalone
  PRODUCT_NAME "Stem Cleaner Pro"
  VST3_CATEGORIES Fx Tools
)

juce_generate_juce_header(StemCleanerPro)

target_sources(StemCleanerPro PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
)

target_compile_definitions(StemCleanerPro PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(StemCleanerPro PRIVATE
  juce::juce_audio_utils
  juce::juce_dsp
  juce::juce_gui_extra
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags
)
