#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "AutoRemixLookAndFeel.h"
#include <array>
#include <functional>

class StyleTabBar : public juce::Component {
public:
    std::function<void(int)> onChange;

    StyleTabBar() = default;

    int getSelectedIndex() const noexcept { return selected_; }

    void setSelectedIndex(int i, bool notify = false) {
        if (i == selected_) return;
        selected_ = i;
        repaint();
        if (notify && onChange) onChange(selected_);
    }

    void paint(juce::Graphics& g) override {
        const int tabW = getWidth() / 3;
        for (int i = 0; i < 3; ++i) {
            juce::Rectangle<int> r(i * tabW, 0, tabW, getHeight());
            bool sel = (i == selected_);

            if (sel) {
                g.setColour(juce::Colour(AR::ELEVATED));
                g.fillRoundedRectangle(r.toFloat(), AR::RADIUS);
                g.setColour(juce::Colour(AR::PURPLE));
                g.fillRect(r.getX(), r.getY(), 3, r.getHeight());
            }

            g.setFont(AR::font(sel ? AR::FontRole::button : AR::FontRole::section));
            g.setColour(juce::Colour(sel ? AR::FG : AR::COMMENT));
            g.drawFittedText(labels_[static_cast<size_t>(i)], r.reduced(4, 0),
                             juce::Justification::centred, 1);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        int idx = e.x / (getWidth() / 3);
        setSelectedIndex(juce::jlimit(0, 2, idx), true);
    }

private:
    int selected_ = 0;
    const std::array<const char*, 3> labels_ {{"Chop & Screw", "Slowed + Reverb", "Drum & Bass"}};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StyleTabBar)
};
