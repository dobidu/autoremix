add_executable(demucs_smoke demucs_smoke.cpp)

target_include_directories(demucs_smoke PRIVATE
  ${CMAKE_SOURCE_DIR}/plugin/include
  ${CMAKE_SOURCE_DIR}/submodules/JUCE/modules
)

target_link_libraries(demucs_smoke PRIVATE
  juce::juce_audio_basics
  juce::juce_audio_formats
  juce::juce_core
  juce::juce_dsp
  juce::juce_events
  rubberband
  onnxruntime
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags
)

target_compile_features(demucs_smoke PRIVATE cxx_std_20)

target_compile_definitions(demucs_smoke PRIVATE
  JUCE_USE_MP3AUDIOFORMAT=0
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
  JUCE_STANDALONE_APPLICATION=1
)
