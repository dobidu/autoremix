#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <BinaryData.h>

namespace AR {
    // ── Background layers ─────────────────────────────────────────────────────
    constexpr uint32_t BG_DEEP       = 0xFF0A0A0B;   // recessed wells
    constexpr uint32_t BG            = 0xFF0F0F10;   // base background
    constexpr uint32_t ELEVATED      = 0xFF1A1A1C;   // cards, panels
    constexpr uint32_t SURFACE       = 0xFF2A2A2D;   // border normal
    constexpr uint32_t BORDER_STRONG = 0xFF3D3D40;   // emphasized border

    // ── Foreground ────────────────────────────────────────────────────────────
    constexpr uint32_t FG      = 0xFFE8E5E0;   // warm cream
    constexpr uint32_t COMMENT = 0xFF5A5A60;   // de-emphasized text

    // ── Accent + semantic ─────────────────────────────────────────────────────
    constexpr uint32_t ACCENT  = 0xFFD4652A;   // burnt orange — primary CTA (Remix)
    constexpr uint32_t MASHUP  = 0xFF2EC4B6;   // tiffany teal — Mashup CTA
    constexpr uint32_t SUCCESS = 0xFF6B8E23;   // olive green
    constexpr uint32_t WARNING = 0xFFC89B3C;   // amber
    constexpr uint32_t ERROR   = 0xFFA0392C;   // muted red

    // ── Stem colors ───────────────────────────────────────────────────────────
    constexpr uint32_t STEM_VOCALS = 0xFFB5C29A;   // sage green
    constexpr uint32_t STEM_DRUMS  = 0xFFC89B3C;   // amber
    constexpr uint32_t STEM_BASS   = 0xFF8E4F2C;   // dark orange-brown
    constexpr uint32_t STEM_OTHER  = 0xFF6E8B8C;   // slate teal

    // ── Geometry ──────────────────────────────────────────────────────────────
    constexpr float RADIUS = 0.0f;   // brutalist: no rounded corners

    // ── Font roles ────────────────────────────────────────────────────────────
    enum class FontRole {
        header,        // Space Grotesk Medium 16px
        section,       // Space Grotesk Medium 11px bold
        label,         // Space Grotesk Regular 11px
        value,         // Space Grotesk Regular 13px
        secondary,     // Space Grotesk Regular 10px
        status,        // Space Grotesk Regular 12px
        button,        // Space Grotesk Medium 12px uppercase
        display_mega,  // Space Grotesk Regular 28px — drag-drop hero
        section_label, // Space Grotesk Medium 10px uppercase
        mono_value,    // JetBrains Mono Regular 13px — numeric readouts
        mono_label,    // JetBrains Mono Medium 11px — units/keys
    };

    inline juce::Font font(FontRole role) {
        switch (role) {
            case FontRole::header:        return juce::Font(16.0f, juce::Font::bold);
            case FontRole::section:       return juce::Font(11.0f, juce::Font::bold);
            case FontRole::label:         return juce::Font(11.0f, juce::Font::plain);
            case FontRole::value:         return juce::Font(13.0f, juce::Font::plain);
            case FontRole::secondary:     return juce::Font(10.0f, juce::Font::plain);
            case FontRole::status:        return juce::Font(12.0f, juce::Font::plain);
            case FontRole::button:        return juce::Font(12.0f, juce::Font::bold);
            case FontRole::display_mega:  return juce::Font(28.0f, juce::Font::plain);
            case FontRole::section_label: return juce::Font(10.0f, juce::Font::bold);
            case FontRole::mono_value:    return juce::Font("JetBrainsMono", 13.0f, juce::Font::plain);
            case FontRole::mono_label:    return juce::Font("JetBrainsMono", 11.0f, juce::Font::bold);
            default:                      return juce::Font(13.0f, juce::Font::plain);
        }
    }
}

class AutoRemixLookAndFeel : public juce::LookAndFeel_V4 {
public:
    AutoRemixLookAndFeel()
    {
        // ── Load typefaces from embedded binary data ───────────────────────────
        sg_regular_  = juce::Typeface::createSystemTypefaceFor(
            BinaryData::SpaceGroteskRegular_ttf,  BinaryData::SpaceGroteskRegular_ttfSize);
        sg_medium_   = juce::Typeface::createSystemTypefaceFor(
            BinaryData::SpaceGroteskMedium_ttf,   BinaryData::SpaceGroteskMedium_ttfSize);
        jbm_regular_ = juce::Typeface::createSystemTypefaceFor(
            BinaryData::JetBrainsMonoRegular_ttf, BinaryData::JetBrainsMonoRegular_ttfSize);
        jbm_medium_  = juce::Typeface::createSystemTypefaceFor(
            BinaryData::JetBrainsMonoMedium_ttf,  BinaryData::JetBrainsMonoMedium_ttfSize);

        // ── Window background ─────────────────────────────────────────────────
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(AR::BG));

        // ── Labels ────────────────────────────────────────────────────────────
        setColour(juce::Label::textColourId,       juce::Colour(AR::FG));
        setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);

        // ── ComboBox ──────────────────────────────────────────────────────────
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(AR::ELEVATED));
        setColour(juce::ComboBox::textColourId,       juce::Colour(AR::FG));
        setColour(juce::ComboBox::arrowColourId,      juce::Colour(AR::ACCENT));
        setColour(juce::ComboBox::outlineColourId,    juce::Colour(AR::SURFACE));

        // ── PopupMenu ─────────────────────────────────────────────────────────
        setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(AR::ELEVATED));
        setColour(juce::PopupMenu::textColourId,                  juce::Colour(AR::FG));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(AR::ACCENT));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(AR::BG));

        // ── ProgressBar ───────────────────────────────────────────────────────
        setColour(juce::ProgressBar::backgroundColourId, juce::Colour(AR::SURFACE));
        setColour(juce::ProgressBar::foregroundColourId, juce::Colour(AR::ACCENT));

        // ── Slider ────────────────────────────────────────────────────────────
        setColour(juce::Slider::backgroundColourId,        juce::Colour(AR::SURFACE));
        setColour(juce::Slider::thumbColourId,             juce::Colour(AR::ACCENT));
        setColour(juce::Slider::trackColourId,             juce::Colour(AR::ACCENT));
        setColour(juce::Slider::textBoxTextColourId,       juce::Colour(AR::FG));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(AR::ELEVATED));
        setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);

        // ── TextButton ────────────────────────────────────────────────────────
        setColour(juce::TextButton::textColourOffId, juce::Colour(AR::FG));
        setColour(juce::TextButton::textColourOnId,  juce::Colour(AR::BG));
        setColour(juce::TextButton::buttonColourId,  juce::Colour(AR::ELEVATED));
    }

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& f) override
    {
        if (f.getTypefaceName().startsWith("JetBrainsMono"))
            return (f.getStyleFlags() & juce::Font::bold) ? jbm_medium_ : jbm_regular_;
        return (f.getStyleFlags() & juce::Font::bold) ? sg_medium_ : sg_regular_;
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button&   button,
                              const juce::Colour& /*backgroundColour*/,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto cid = button.getComponentID();
        bool isPrimary       = (cid == "primary");
        bool isPrimaryMashup = (cid == "primary_mashup");
        float alpha = button.isEnabled() ? 1.0f : 0.4f;

        if (isPrimary || isPrimaryMashup) {
            auto base = isPrimaryMashup ? juce::Colour(AR::MASHUP)
                                        : juce::Colour(AR::ACCENT);
            auto fill = base.withAlpha(alpha);
            if (shouldDrawButtonAsDown)        fill = fill.darker(0.25f);
            if (shouldDrawButtonAsHighlighted) fill = fill.brighter(0.15f);
            g.setColour(fill);
            g.fillRect(bounds);
            // 2px left accent border
            g.setColour(juce::Colour(AR::FG).withAlpha(alpha * 0.6f));
            g.fillRect(bounds.removeFromLeft(2.0f));
        } else {
            auto outline = juce::Colour(AR::SURFACE).withAlpha(alpha);
            if (shouldDrawButtonAsHighlighted) outline = juce::Colour(AR::BORDER_STRONG).withAlpha(alpha);
            if (shouldDrawButtonAsDown) {
                g.setColour(juce::Colour(AR::ELEVATED).withAlpha(alpha));
                g.fillRect(bounds);
            }
            g.setColour(outline);
            g.drawRect(bounds, 1.0f);
        }
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool /*highlighted*/, bool /*down*/) override
    {
        g.setFont(AR::font(AR::FontRole::button));
        float alpha = button.isEnabled() ? 1.0f : 0.4f;
        auto cid = button.getComponentID();
        bool isPrimary = (cid == "primary" || cid == "primary_mashup");
        g.setColour(isPrimary ? juce::Colour(AR::FG).withAlpha(alpha)
                              : juce::Colour(AR::FG).withAlpha(alpha * 0.85f));
        g.drawFittedText(button.getButtonText().toUpperCase(),
                         button.getLocalBounds().reduced(8, 0),
                         juce::Justification::centred, 1);
    }

    void drawLinearSliderThumb(juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, float, float,
                               juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        auto thumbW = 4.0f;
        auto thumbH = (float)height;
        auto thumbX = sliderPos - thumbW * 0.5f;
        g.setColour(juce::Colour(AR::ACCENT).withAlpha(slider.isEnabled() ? 1.0f : 0.4f));
        g.fillRect(thumbX, (float)y, thumbW, thumbH);
    }

    void drawLinearSliderBackground(juce::Graphics& g, int x, int y, int width, int height,
                                    float, float, float,
                                    juce::Slider::SliderStyle, juce::Slider&) override
    {
        auto track = juce::Rectangle<float>((float)x, (float)y + height * 0.5f - 1.0f, (float)width, 2.0f);
        g.setColour(juce::Colour(AR::SURFACE));
        g.fillRect(track);
    }

    juce::Typeface::Ptr sg_regular_;
    juce::Typeface::Ptr sg_medium_;
    juce::Typeface::Ptr jbm_regular_;
    juce::Typeface::Ptr jbm_medium_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoRemixLookAndFeel)
};
