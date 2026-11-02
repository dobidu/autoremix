# AutoRemix automated tests (Catch2 v3, BSL-1.0 — test-only, not shipped).
# Every source file is listed explicitly (project rule: no globbing).

add_executable(autoremix_tests
  test_main.cpp
  test_harness_sanity.cpp
)

target_include_directories(autoremix_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/plugin/include
  ${CMAKE_SOURCE_DIR}/tests/support
)

target_link_libraries(autoremix_tests PRIVATE
  Catch2::Catch2
  AutoRemixPresetsData
  nlohmann_json::nlohmann_json
  juce::juce_audio_basics
  juce::juce_audio_formats
  juce::juce_core
  juce::juce_dsp
  juce::juce_events
  juce::juce_graphics
  juce::juce_gui_basics
  rubberband
  onnxruntime
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags
)

target_compile_features(autoremix_tests PRIVATE cxx_std_20)

# Tests compare synthetic signals bit-exactly on purpose.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(autoremix_tests PRIVATE -Wno-float-equal)
endif()

target_compile_definitions(autoremix_tests PRIVATE
  JUCE_STANDALONE_APPLICATION=1
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
  JUCE_USE_MP3AUDIOFORMAT=1
  JUCE_MODAL_LOOPS_PERMITTED=1
)

# ThreadSanitizer on Linux kernels with high mmap ASLR entropy aborts with
# "unexpected memory mapping". Run the test binary (discovery + tests) with
# ASLR disabled instead of requiring a sysctl change on the host.
set(AUTOREMIX_SANITIZER "" CACHE STRING "Sanitizer the tree is built with (address|thread|empty)")
if(AUTOREMIX_SANITIZER STREQUAL "thread" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_program(AUTOREMIX_SETARCH setarch REQUIRED)
  set_property(TARGET autoremix_tests PROPERTY CROSSCOMPILING_EMULATOR
    "${AUTOREMIX_SETARCH};${CMAKE_SYSTEM_PROCESSOR};-R")
endif()

include(Catch)
# PRE_TEST: discover at ctest time, not as a POST_BUILD step — avoids running
# sanitizer-instrumented binaries during the build and is faster on /mnt/e.
catch_discover_tests(autoremix_tests
  DISCOVERY_MODE PRE_TEST
  PROPERTIES TIMEOUT 300
)
