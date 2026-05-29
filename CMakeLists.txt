cmake_minimum_required(VERSION 3.22)
project(AutoRemix VERSION 4.2.0)

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
else()
  # NOMINMAX prevents <windows.h> min/max macros from clobbering std::numeric_limits<T>::max()
  # inside RubberBand source files (single-file build includes all TUs in one unit).
  target_compile_definitions(rubberband PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# ONNX Runtime — pre-built per platform (Phase 26-02). Microsoft ships
# tarballs only; we fetch the right one and import as a SHARED library.
# GPU EP support added in Phase 28-01: -DAUTOREMIX_GPU=ON selects GPU packages.
#   Linux:   onnxruntime-linux-x64-gpu (CUDA 11.8 EP)
#   Windows: Microsoft.ML.OnnxRuntime.DirectML NuGet (DirectML EP, DX12)
#   macOS:   CoreML EP deferred to Phase 29 — same CPU tarball used.
option(AUTOREMIX_GPU "Build with GPU EP (CUDA on Linux, DirectML on Windows)" OFF)

set(ORT_VERSION "1.21.0")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  if(AUTOREMIX_GPU)
    set(ORT_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-linux-x64-gpu-${ORT_VERSION}.tgz")
    set(ORT_SHA "ef37a33ba75e457aebfd0d7b342ab20424aa6126bc5f565d247f1201b66996cf")
  else()
    set(ORT_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-linux-x64-${ORT_VERSION}.tgz")
    set(ORT_SHA "7485c7e7aac6501b27e353dcbe068e45c61ab51fbaf598d13970dfae669d20bf")
  endif()
  set(ORT_LIB_SUBPATH "lib/libonnxruntime.so.${ORT_VERSION}")
  set(ORT_INCLUDE_SUBPATH "include")
elseif(APPLE)
  set(ORT_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-osx-universal2-${ORT_VERSION}.tgz")
  set(ORT_SHA "3c3cfc71e538e592192c14d9bad88ec5d5d8d5d698ebc2c6b3119be8c90b4670")
  set(ORT_LIB_SUBPATH "lib/libonnxruntime.${ORT_VERSION}.dylib")
  set(ORT_INCLUDE_SUBPATH "include")
elseif(WIN32)
  if(AUTOREMIX_GPU)
    # DirectML-enabled ORT from NuGet v3 flatcontainer. Headers + DML provider
    # factory header live under build/native/include inside the package.
    # Note: NuGet v3 URL used (v2 API returns 404 for this package).
    set(ORT_URL "https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime.directml/${ORT_VERSION}/microsoft.ml.onnxruntime.directml.${ORT_VERSION}.nupkg")
    set(ORT_SHA "12bed6e1e870f30e0150e039ebb6853da32eba381eda1c373faeeb17168785e6")
    set(ORT_LIB_SUBPATH "runtimes/win-x64/native/onnxruntime.dll")
    set(ORT_IMPLIB_SUBPATH "runtimes/win-x64/native/onnxruntime.lib")
    set(ORT_INCLUDE_SUBPATH "build/native/include")
    # DirectML.dll is a Windows OS component (Win10 1903+); ORT loads it from
    # System32 via LoadLibrary — no bundling needed.
  else()
    set(ORT_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-win-x64-${ORT_VERSION}.zip")
    set(ORT_SHA "5c07bb2805cd666dda75fa9bfa60e75f2f90d478b952298dd9d55c00740d81bf")
    set(ORT_LIB_SUBPATH "lib/onnxruntime.dll")
    set(ORT_IMPLIB_SUBPATH "lib/onnxruntime.lib")
    set(ORT_INCLUDE_SUBPATH "include")
  endif()
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
  INTERFACE_INCLUDE_DIRECTORIES "${onnxruntime_SOURCE_DIR}/${ORT_INCLUDE_SUBPATH}")
if(WIN32)
  # MSVC multi-config generators require IMPORTED_IMPLIB for SHARED IMPORTED targets.
  set_target_properties(onnxruntime PROPERTIES
    IMPORTED_IMPLIB "${onnxruntime_SOURCE_DIR}/${ORT_IMPLIB_SUBPATH}")
endif()

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
