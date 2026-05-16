juce_add_binary_data(AutoRemixFonts SOURCES
    ../assets/fonts/Inter-Regular.otf
    ../assets/fonts/Inter-SemiBold.otf
    ../assets/fonts/Inter-Bold.otf
    ../assets/fonts/SpaceGrotesk-Regular.ttf
    ../assets/fonts/SpaceGrotesk-Medium.ttf
    ../assets/fonts/JetBrainsMono-Regular.ttf
    ../assets/fonts/JetBrainsMono-Medium.ttf
)
set_target_properties(AutoRemixFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(AutoRemix
  COMPANY_NAME "LAViD-UFPB"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  PLUGIN_MANUFACTURER_CODE Lavd
  PLUGIN_CODE Armx
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "AutoRemix"
)

target_sources(AutoRemix PRIVATE
  src/AutoRemixProcessor.cpp
  src/AutoRemixEditor.cpp
  src/AudioBridge.cpp
  src/separators/SpleeterSeparator.cpp
  src/separators/AlgorithmicSeparator.cpp
  src/engines/ChoppedAndScrewedEngine.cpp
  src/engines/SlowedReverbEngine.cpp
  src/engines/DrumAndBassEngine.cpp
)

target_include_directories(AutoRemix PRIVATE
  include
  ${CMAKE_SOURCE_DIR}/submodules/JUCE/modules
)

target_compile_definitions(AutoRemix PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_USE_MP3AUDIOFORMAT=1
  AUTOREMIX_SIDECAR_PORT=17432
  AUTOREMIX_SIDECAR_HOST="http://127.0.0.1"
)

target_link_libraries(AutoRemix PRIVATE
  AutoRemixFonts
  juce::juce_audio_utils
  juce::juce_audio_processors
  juce::juce_gui_extra
  cpr::cpr
  nlohmann_json::nlohmann_json
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags
)
