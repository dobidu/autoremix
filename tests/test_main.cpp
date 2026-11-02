// Catch2 entry point for autoremix_tests.
// JUCE's GUI subsystem is created lazily by tests that need it (see
// TestHelpers.h) and torn down here, before static destruction, so the JUCE
// leak detector and X11 shutdown run in a defined order.

#include <catch2/catch_session.hpp>

#include "TestHelpers.h"

int main(int argc, char* argv[])
{
    const int result = Catch::Session().run(argc, argv);
    autoremix::test::shutdown_gui();
    return result;
}
