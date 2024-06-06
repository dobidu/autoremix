# To verify your version run
# $ cmake --version
cmake_minimum_required(VERSION 3.22)

# Sets a few variables, like PROJECT_NAME
project(AutoRemix VERSION 0.1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Always use the newest C++ standard on green-field projects if possible.
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(submodules/JUCE)

# Fetch libcpr
include(FetchContent)

set(LIBCPR_BUILD_SHARED OFF)
set(BUILD_SHARED_LIBS OFF)
set(SENTRY_BUILD_SHARED_LIBS OFF)

FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 0817715923c9705e68994eb52ef9df3f6845beba) # The commit hash for 1.10.x. Replace with the latest from: https://github.com/libcpr/cpr/releases
FetchContent_MakeAvailable(cpr)

FetchContent_Declare(json GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG 9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03) # The commit hash for 0.6.4. Replace with the latest from: https://github.com/getsentry/sentry-native/releases
FetchContent_MakeAvailable(json)

#CAIO: workaround for non async file chooser
add_definitions(-DJUCE_MODAL_LOOPS_PERMITTED=1)
# Adds all the targets configured in the "plugin" folder.
add_subdirectory(plugin)
