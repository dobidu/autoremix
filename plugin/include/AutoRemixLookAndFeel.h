#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace AR {
    constexpr uint32_t BG      = 0xFF282A36;
    constexpr uint32_t SURFACE = 0xFF44475A;
    constexpr uint32_t FG      = 0xFFF8F8F2;
    constexpr uint32_t COMMENT = 0xFF6272A4;
    constexpr uint32_t GREEN   = 0xFF50FA7B;
    constexpr uint32_t CYAN    = 0xFF8BE9FD;
    constexpr uint32_t PURPLE  = 0xFFBD93F9;
    constexpr float    RADIUS  = 6.0f;
}

class AutoRemixLookAndFeel : public juce::LookAndFeel_V4 {
public:
    AutoRemixLookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId,          juce::Colour(AR::BG));

        setColour(juce::Label::textColourId,                          juce::Colour(AR::FG));
        setColour(juce::Label::backgroundColourId,                    juce::Colours::transparentBlack);

        setColour(juce::ComboBox::backgroundColourId,                 juce::Colour(AR::SURFACE));
        setColour(juce::ComboBox::textColourId,                       juce::Colour(AR::FG));
        setColour(juce::ComboBox::arrowColourId,                      juce::Colour(AR::FG));
        setColour(juce::ComboBox::outlineColourId,                    juce::Colour(AR::COMMENT));

        setColour(juce::PopupMenu::backgroundColourId,                juce::Colour(AR::SURFACE));
        setColour(juce::PopupMenu::textColourId,                      juce::Colour(AR::FG));
        setColour(juce::PopupMenu::highlightedBackgroundColourId,     juce::Colour(AR::PURPLE));
        setColour(juce::PopupMenu::highlightedTextColourId,           juce::Colour(AR::BG));

        setColour(juce::ProgressBar::backgroundColourId,              juce::Colour(AR::SURFACE));
        setColour(juce::ProgressBar::foregroundColourId,              juce::Colour(AR::PURPLE));
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        auto base = backgroundColour
            .withMultipliedSaturation(button.hasKeyboardFocus(true) ? 1.3f : 0.9f)
            .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.4f);

        if (shouldDrawButtonAsDown)        base = base.darker(0.2f);
        if (shouldDrawButtonAsHighlighted) base = base.brighter(0.1f);

        g.setColour(base);
        g.fillRoundedRectangle(bounds, AR::RADIUS);

        g.setColour(base.brighter(0.3f));
        g.drawRoundedRectangle(bounds, AR::RADIUS, 1.0f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool, bool) override
    {
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.setColour(button.isEnabled() ? juce::Colour(AR::FG)
                                       : juce::Colour(AR::FG).withAlpha(0.4f));
        g.drawFittedText(button.getButtonText(),
                         button.getLocalBounds(), juce::Justification::centred, 1);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoRemixLookAndFeel)
};
