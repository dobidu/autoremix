cmake_minimum_required(VERSION 3.22)
project(AutoRemix VERSION 4.0.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 20)

# JUCE
add_subdirectory(submodules/JUCE)

# HTTP client (libcpr)
include(FetchContent)
set(LIBCPR_BUILD_SHARED OFF)
set(BUILD_SHARED_LIBS OFF)
# Required so libcpr.a can be linked into VST3 shared library (.so / .dylib)
if(NOT WIN32)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
# On Windows let cpr auto-detect WinHTTP; disable system curl requirement
if(WIN32)
  set(CPR_USE_SYSTEM_CURL OFF CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(
  cpr
  GIT_REPOSITORY https://github.com/libcpr/cpr.git
  GIT_TAG 0817715923c9705e68994eb52ef9df3f6845beba
)
FetchContent_MakeAvailable(cpr)

# JSON
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json
  GIT_TAG 9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03
)
FetchContent_MakeAvailable(json)

# RubberBand — time-stretch + pitch-shift (replaces librosa.effects.* in v4 native).
# Upstream uses Meson; we use its supported "single-file build" path
# (single/RubberBandSingle.cpp) which produces a self-contained TU.
FetchContent_Declare(
  rubberband
  GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git
  GIT_TAG        v3.3.0
)
FetchContent_GetProperties(rubberband)
if(NOT rubberband_POPULATED)
  FetchContent_Populate(rubberband)
endif()
add_library(rubberband STATIC
  ${rubberband_SOURCE_DIR}/single/RubberBandSingle.cpp
)
target_include_directories(rubberband PUBLIC
  ${rubberband_SOURCE_DIR}
  ${rubberband_SOURCE_DIR}/rubberband
)
target_compile_features(rubberband PUBLIC cxx_std_17)
if(NOT WIN32)
  set_target_properties(rubberband PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# ONNX Runtime — pre-built per platform (Phase 26-02). Microsoft ships
# tarballs only; we fetch the right one and import as a SHARED library.
set(ORT_VERSION "1.17.0")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(ORT_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-linux-x64-${ORT_VERSION}.tgz")
  set(ORT_SHA "efc344d54d1969446ff5d3e55b54e205c6579c06333ecf1d34a04215eefae7c6")
  set(ORT_LIB_SUBPATH "lib/libonnxruntime.so.${ORT_VERSION}")
elseif(APPLE)
  set(ORT_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-osx-universal2-${ORT_VERSION}.tgz")
  # SHA pinned on first macOS build — see CMakeLists.txt comment in plan 26-02.
  set(ORT_SHA "")
  set(ORT_LIB_SUBPATH "lib/libonnxruntime.${ORT_VERSION}.dylib")
elseif(WIN32)
  set(ORT_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-win-x64-${ORT_VERSION}.zip")
  # SHA pinned on first Windows build.
  set(ORT_SHA "")
  set(ORT_LIB_SUBPATH "lib/onnxruntime.dll")
endif()

if(ORT_SHA)
  FetchContent_Declare(onnxruntime
    URL      ${ORT_URL}
    URL_HASH SHA256=${ORT_SHA}
  )
else()
  FetchContent_Declare(onnxruntime
    URL ${ORT_URL}
  )
endif()
FetchContent_MakeAvailable(onnxruntime)

add_library(onnxruntime SHARED IMPORTED GLOBAL)
set_target_properties(onnxruntime PROPERTIES
  IMPORTED_LOCATION             "${onnxruntime_SOURCE_DIR}/${ORT_LIB_SUBPATH}"
  INTERFACE_INCLUDE_DIRECTORIES "${onnxruntime_SOURCE_DIR}/include")

# Workaround async file chooser
add_definitions(-DJUCE_MODAL_LOOPS_PERMITTED=1)

# Plugin target
add_subdirectory(plugin)

# Tests
option(AUTOREMIX_BUILD_TESTS "Build tests" ON)
if(AUTOREMIX_BUILD_TESTS)
  enable_testing()
  # add_subdirectory(tests/cpp)  # habilitar na Fase 05
endif()

# Scratch smoke tests (manual, not run by CTest)
option(AUTOREMIX_BUILD_SCRATCH "Build scratch smoke tests under tests/scratch" ON)
if(AUTOREMIX_BUILD_SCRATCH)
  add_subdirectory(tests/scratch)
endif()
