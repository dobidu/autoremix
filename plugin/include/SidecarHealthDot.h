#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "AutoRemixLookAndFeel.h"
#include <atomic>
#include <functional>
#include <thread>

class SidecarHealthDot : public juce::Component, private juce::Timer {
public:
    explicit SidecarHealthDot(std::function<bool()> pollFn)
        : poll_fn_(std::move(pollFn))
    {
        startTimer(2000);
    }

    ~SidecarHealthDot() override {
        stopTimer();
        alive_ = false;
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        switch (state_) {
            case State::connected:    g.setColour(juce::Colour(AR::GREEN));  break;
            case State::disconnected: g.setColour(juce::Colour(AR::RED));    break;
            case State::checking:     g.setColour(juce::Colour(AR::ORANGE)); break;
        }
        g.fillEllipse(bounds);
    }

private:
    enum class State { checking, connected, disconnected };

    std::function<bool()> poll_fn_;
    State state_ = State::checking;
    std::atomic<bool> alive_{true};

    void timerCallback() override {
        std::thread([this]() {
            bool ok = poll_fn_();
            if (!alive_) return;
            auto newState = ok ? State::connected : State::disconnected;
            juce::MessageManager::callAsync([this, newState]() {
                if (!alive_) return;
                if (newState != state_) {
                    state_ = newState;
                    repaint();
                }
            });
        }).detach();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SidecarHealthDot)
};
