#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <BinaryData.h>

namespace AR {
    // Background layers
    constexpr uint32_t BG_DEEP  = 0xFF1E1F29;
    constexpr uint32_t BG       = 0xFF282A36;
    constexpr uint32_t ELEVATED = 0xFF343746;
    constexpr uint32_t SURFACE  = 0xFF44475A;

    // Foreground
    constexpr uint32_t FG      = 0xFFF8F8F2;
    constexpr uint32_t COMMENT = 0xFF6272A4;

    // Accents
    constexpr uint32_t GREEN  = 0xFF50FA7B;
    constexpr uint32_t CYAN   = 0xFF8BE9FD;
    constexpr uint32_t PURPLE = 0xFFBD93F9;
    constexpr uint32_t RED    = 0xFFFF5555;
    constexpr uint32_t ORANGE = 0xFFFFB86C;

    constexpr float RADIUS = 6.0f;

    enum class FontRole { header, section, label, value, secondary, status, button };

    inline juce::Font font(FontRole role) {
        switch (role) {
            case FontRole::header:    return juce::Font(16.0f, juce::Font::bold);
            case FontRole::section:   return juce::Font(11.0f, juce::Font::bold);
            case FontRole::label:     return juce::Font(11.0f, juce::Font::plain);
            case FontRole::value:     return juce::Font(13.0f, juce::Font::plain);
            case FontRole::secondary: return juce::Font(10.0f, juce::Font::plain);
            case FontRole::status:    return juce::Font(12.0f, juce::Font::plain);
            case FontRole::button:    return juce::Font(12.0f, juce::Font::bold);
            default:                  return juce::Font(13.0f, juce::Font::plain);
        }
    }
}

class AutoRemixLookAndFeel : public juce::LookAndFeel_V4 {
public:
    AutoRemixLookAndFeel() {
        inter_regular_  = juce::Typeface::createSystemTypefaceFor(
            BinaryData::InterRegular_otf,  BinaryData::InterRegular_otfSize);
        inter_semibold_ = juce::Typeface::createSystemTypefaceFor(
            BinaryData::InterSemiBold_otf, BinaryData::InterSemiBold_otfSize);

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

        setColour(juce::Slider::backgroundColourId,                   juce::Colour(AR::SURFACE));
        setColour(juce::Slider::thumbColourId,                        juce::Colour(AR::FG));
        setColour(juce::Slider::trackColourId,                        juce::Colour(AR::PURPLE));
        setColour(juce::Slider::textBoxTextColourId,                  juce::Colour(AR::FG));
        setColour(juce::Slider::textBoxBackgroundColourId,            juce::Colour(AR::ELEVATED));
        setColour(juce::Slider::textBoxOutlineColourId,               juce::Colours::transparentBlack);

        setColour(juce::TextButton::textColourOffId,                  juce::Colour(AR::FG));
        setColour(juce::TextButton::textColourOnId,                   juce::Colour(AR::BG));
    }

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& f) override {
        return (f.getStyleFlags() & juce::Font::bold) ? inter_semibold_ : inter_regular_;
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
        g.setFont(AR::font(AR::FontRole::button));
        g.setColour(button.isEnabled() ? juce::Colour(AR::FG)
                                       : juce::Colour(AR::FG).withAlpha(0.4f));
        g.drawFittedText(button.getButtonText(),
                         button.getLocalBounds(), juce::Justification::centred, 1);
    }

    juce::Typeface::Ptr inter_regular_;
    juce::Typeface::Ptr inter_semibold_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoRemixLookAndFeel)
};
