#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ScreenContext.h"

// ─────────────────────────────────────────────────────────────────────────────
// ScreenBase — abstract base for all AutoRemix screens.
//
// Each screen is a juce::Component that fills the main content area
// (960×520 px, below the 48px header and above the 32px footer).
//
// Lifecycle:
//   1. navigateTo(id) constructs the ScreenBase-derived object.
//   2. Editor calls addAndMakeVisible(*screen_) and sets its bounds.
//   3. Editor calls screen_->onEnter() — load data, start timers, etc.
//   4. When navigating away: screen_->onExit() → fade out → destroy.
//
// Screens must NOT start background threads in their constructor; use
// onEnter() instead so the component is visible before work begins.
// ─────────────────────────────────────────────────────────────────────────────
class ScreenBase : public juce::Component {
public:
    explicit ScreenBase(ScreenContext& ctx) : ctx_(ctx) {}
    ~ScreenBase() override = default;

    virtual void onEnter() {}
    virtual void onExit()  {}

protected:
    ScreenContext& ctx_;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScreenBase)
};
