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
// Cache check is a cheap file-stat + size sanity (no SHA at paint time).
// External code can call setState() during a download flow.
// ─────────────────────────────────────────────────────────────────────────────

#include <juce_gui_basics/juce_gui_basics.h>
#include "AutoRemixLookAndFeel.h"
#include "dsp/ModelDownloader.h"

#include <atomic>
#include <thread>

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
        alive_ = false;
    }

    void setState(State s)
    {
        // Always write state_ on the message thread — called from background threads.
        juce::MessageManager::callAsync([this, s] { state_ = s; repaint(); });
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
        // Cheap stat-only check on a background thread; SHA comparison is
        // skipped at paint time (ModelDownloader::ensure_htdemucs runs the
        // full SHA check before use).
        std::thread([this]() {
            const auto p = autoremix::dsp::models::htdemucs_cache_path();
            const bool exists =
                p.existsAsFile()
                && p.getSize() >= autoremix::dsp::models::kHtdemucsMinBytes
                && p.getSize() <= autoremix::dsp::models::kHtdemucsMaxBytes;
            if (!alive_) return;
            juce::MessageManager::callAsync([this, exists]() {
                if (!alive_) return;
                // Don't clobber an in-flight downloading / error / gpu_active state.
                if (state_ == State::downloading || state_ == State::error
                    || state_ == State::gpu_active) return;
                const auto next = exists ? State::cached : State::missing;
                if (next != state_) { state_ = next; repaint(); }
            });
        }).detach();
    }

    std::atomic<bool> alive_{true};
    State state_ = State::checking;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModelStatusDot)
};
