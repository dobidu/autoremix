#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// ModelStatusDot — Phase 27-01
//
// Replaces the v3 SidecarHealthDot. Indicates whether the htdemucs ONNX
// model is cached locally:
//   green  → cached + size plausible
//   amber  → checking (during a download)
//   red    → not cached (will download on first demucs selection)
//
// Cache check is a cheap file-stat + size sanity (no SHA at paint time), done
// on the message thread from the timer: no worker thread, so nothing can
// outlive the component (REVIEW R-03). setState() may be called from any
// thread; off the message thread it posts through a SafePointer.
// ─────────────────────────────────────────────────────────────────────────────

#include <juce_gui_basics/juce_gui_basics.h>
#include "AutoRemixLookAndFeel.h"
#include "dsp/ModelDownloader.h"


class ModelStatusDot : public juce::Component,
                       public juce::SettableTooltipClient,
                       private juce::Timer {
public:
    enum class State { checking, cached, missing, downloading, error, gpu_active };

    ModelStatusDot()
    {
        startTimer(3000);
        refresh_();
    }

    ~ModelStatusDot() override
    {
        stopTimer();
    }

    void setState(State s)
    {
        if (juce::MessageManager::getInstance()->isThisTheMessageThread()) {
            state_ = s;
            repaint();
            return;
        }
        juce::MessageManager::callAsync([safe = juce::Component::SafePointer<ModelStatusDot>(this), s] {
            if (safe == nullptr) return;
            safe->state_ = s;
            safe->repaint();
        });
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        switch (state_) {
            case State::cached:      g.setColour(juce::Colour(AR::SUCCESS)); break;
            case State::missing:     g.setColour(juce::Colour(AR::ERR));   break;
            case State::downloading: g.setColour(juce::Colour(AR::WARNING)); break;
            case State::error:       g.setColour(juce::Colour(AR::ERR));   break;
            case State::checking:    g.setColour(juce::Colour(AR::WARNING)); break;
            case State::gpu_active:  g.setColour(juce::Colour(0xFF4A9EDB)); break;
        }
        g.fillEllipse(bounds);
    }

private:
    void timerCallback() override { refresh_(); }

    void refresh_()
    {
        const auto p = autoremix::dsp::models::htdemucs_cache_path();
        const bool exists =
            p.existsAsFile()
            && p.getSize() >= autoremix::dsp::models::kHtdemucsMinBytes
            && p.getSize() <= autoremix::dsp::models::kHtdemucsMaxBytes;
        // Don't clobber an in-flight downloading / error / gpu_active state.
        if (state_ == State::downloading || state_ == State::error
            || state_ == State::gpu_active) return;
        const auto next = exists ? State::cached : State::missing;
        if (next != state_) { state_ = next; repaint(); }
    }

    State state_ = State::checking;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModelStatusDot)
};
