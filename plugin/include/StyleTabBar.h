#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "AutoRemixLookAndFeel.h"
#include <string>
#include <vector>
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

    void setLabels(std::vector<std::string> labels) {
        labels_ = std::move(labels);
        selected_ = 0;
        repaint();
    }

    void paint(juce::Graphics& g) override {
        if (labels_.empty()) return;
        int n    = (int)labels_.size();
        int tabW = getWidth() / n;
        for (int i = 0; i < n; ++i) {
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
            g.drawFittedText(labels_[(size_t)i], r.reduced(4, 0),
                             juce::Justification::centred, 1);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (labels_.empty()) return;
        int n   = (int)labels_.size();
        int idx = e.x / (getWidth() / n);
        setSelectedIndex(juce::jlimit(0, n - 1, idx), true);
    }

private:
    int selected_ = 0;
    std::vector<std::string> labels_ {"Chop & Screw", "Slowed + Reverb", "Drum & Bass"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StyleTabBar)
};
