#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <filesystem>
#include "ScreenBase.h"
#include "AutoRemixLookAndFeel.h"
#include "PluginTypes.h"

class ScreenStemsReady : public ScreenBase,
                         public juce::ChangeListener,
                         public juce::Timer
{
public:
    explicit ScreenStemsReady(ScreenContext& ctx)
        : ScreenBase(ctx),
          thumbnail_cache_(8),
          vocals_thumb_(512, format_manager_, thumbnail_cache_),
          drums_thumb_ (512, format_manager_, thumbnail_cache_),
          bass_thumb_  (512, format_manager_, thumbnail_cache_),
          other_thumb_ (512, format_manager_, thumbnail_cache_)
    {
        format_manager_.registerBasicFormats();
        vocals_thumb_.addChangeListener(this);
        drums_thumb_ .addChangeListener(this);
        bass_thumb_  .addChangeListener(this);
        other_thumb_ .addChangeListener(this);

        initRows();

        addAndMakeVisible(back_btn_);
        back_btn_.setButtonText("< Back");
        back_btn_.setComponentID("ghost");
        back_btn_.onClick = [this] { ctx_.navigate(ScreenId::Empty); };

        addAndMakeVisible(next_btn_);
        next_btn_.setButtonText("Choose Style >");
        next_btn_.setComponentID("primary");
        next_btn_.onClick = [this] { ctx_.navigate(ScreenId::ModeParams); };
    }

    ~ScreenStemsReady() override
    {
        stopTimer();
        vocals_thumb_.removeChangeListener(this);
        drums_thumb_ .removeChangeListener(this);
        bass_thumb_  .removeChangeListener(this);
        other_thumb_ .removeChangeListener(this);
    }

    void onEnter() override
    {
        if (ctx_.stems.valid) {
            auto loadThumb = [](juce::AudioThumbnail& thumb, const std::filesystem::path& p) {
                juce::File f(juce::String(p.string()));
                if (f.existsAsFile())
                    thumb.setSource(new juce::FileInputSource(f));
            };
            loadThumb(vocals_thumb_, ctx_.stems.vocals);
            loadThumb(drums_thumb_,  ctx_.stems.drums);
            loadThumb(bass_thumb_,   ctx_.stems.bass);
            loadThumb(other_thumb_,  ctx_.stems.other);
        }
        startTimer(100);
        resized();
        repaint();
    }

    void onExit() override
    {
        stopTimer();
        if (ctx_.stop_all_stems) ctx_.stop_all_stems();
    }

    void timerCallback() override
    {
        for (int i = 0; i < 4; ++i) {
            bool playing = ctx_.is_stem_playing && ctx_.is_stem_playing(i);
            rows_[(size_t)i].play_btn.setButtonText(
                playing ? juce::String::fromUTF8("\xE2\x96\xA0")   // ■
                        : juce::String::fromUTF8("\xE2\x96\xB6")); // ▶
        }
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override { repaint(); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        drag_stem_idx_  = stemRowAtY(e.y);
        mouse_down_pos_ = e.getPosition();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (drag_stem_idx_ < 0) return;
        if (e.getDistanceFromDragStart() < 8) return;
        auto path = stemPath(drag_stem_idx_);
        if (path.isEmpty() || !juce::File(path).existsAsFile()) return;
        drag_stem_idx_ = -1;
        juce::DragAndDropContainer::performExternalDragDropOfFiles({ path }, false);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(AR::BG));

        auto b = getLocalBounds().withTrimmedBottom(72);

        auto headerRow = b.removeFromTop(40);
        g.setFont(AR::font(AR::FontRole::section_label));
        g.setColour(juce::Colour(AR::COMMENT));
        g.drawText("STEMS", headerRow.reduced(16, 0), juce::Justification::centredLeft);

        for (int i = 0; i < 4; ++i)
            paintRowBackground(g, b.removeFromTop(ROW_H), i);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        auto actionBar = b.removeFromBottom(72);
        b.removeFromTop(40);

        for (int i = 0; i < 4; ++i)
            layoutRow(b.removeFromTop(ROW_H), i);

        auto ab = actionBar.reduced(16, 16);
        back_btn_.setBounds(ab.removeFromLeft(120));
        next_btn_.setBounds(ab.removeFromRight(180));
    }

private:
    static constexpr int ROW_H       = 56;
    static constexpr int LEFT_COL_W  = 156;
    static constexpr int RIGHT_COL_W = 320;
    static constexpr int TOGGLE_SIZE = 24;
    static constexpr int TOGGLE_GAP  = 4;
    static constexpr int SLIDER_W    = 220;

    struct StemRowWidgets {
        juce::String          name;
        uint32_t              color      = 0;
        juce::AudioThumbnail* thumb      = nullptr;
        juce::TextButton      play_btn;
        juce::TextButton      mute_btn;
        juce::TextButton      solo_btn;
        juce::Slider          gain_slider;
        float*                ctx_gain   = nullptr;
        bool*                 ctx_muted  = nullptr;
        bool*                 ctx_solo   = nullptr;

        JUCE_DECLARE_NON_COPYABLE(StemRowWidgets)
        StemRowWidgets() = default;
    };

    std::array<StemRowWidgets, 4> rows_;

    void initRows()
    {
        auto setup = [&](int i, const char* name, uint32_t color,
                         juce::AudioThumbnail& thumb,
                         float& gain, bool& muted, bool& solo) {
            auto& r    = rows_[(size_t)i];
            r.name     = name;
            r.color    = color;
            r.thumb    = &thumb;
            r.ctx_gain  = &gain;
            r.ctx_muted = &muted;
            r.ctx_solo  = &solo;

            addAndMakeVisible(r.play_btn);
            r.play_btn.setButtonText(juce::String::fromUTF8("\xE2\x96\xB6")); // ▶
            r.play_btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(AR::ELEVATED));
            r.play_btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(AR::ACCENT));
            r.play_btn.onClick = [this, i] { handlePlay(i); };

            addAndMakeVisible(r.mute_btn);
            r.mute_btn.setButtonText("M");
            r.mute_btn.setClickingTogglesState(true);
            r.mute_btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(AR::ELEVATED));
            r.mute_btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(AR::ERROR));
            r.mute_btn.onClick = [this, i] { handleMute(i); };

            addAndMakeVisible(r.solo_btn);
            r.solo_btn.setButtonText("S");
            r.solo_btn.setClickingTogglesState(true);
            r.solo_btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(AR::ELEVATED));
            r.solo_btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(AR::WARNING));
            r.solo_btn.onClick = [this, i] { handleSolo(i); };

            addAndMakeVisible(r.gain_slider);
            r.gain_slider.setSliderStyle(juce::Slider::LinearHorizontal);
            r.gain_slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
            r.gain_slider.setRange(0.0, 2.0, 0.01);
            r.gain_slider.setValue(gain, juce::dontSendNotification);
            r.gain_slider.onValueChange = [this, i] {
                *rows_[(size_t)i].ctx_gain = (float)rows_[(size_t)i].gain_slider.getValue();
            };
        };

        setup(0, "Vocals", AR::STEM_VOCALS, vocals_thumb_, ctx_.vocals_gain, ctx_.vocals_muted, ctx_.vocals_solo);
        setup(1, "Drums",  AR::STEM_DRUMS,  drums_thumb_,  ctx_.drums_gain,  ctx_.drums_muted,  ctx_.drums_solo);
        setup(2, "Bass",   AR::STEM_BASS,   bass_thumb_,   ctx_.bass_gain,   ctx_.bass_muted,   ctx_.bass_solo);
        setup(3, "Other",  AR::STEM_OTHER,  other_thumb_,  ctx_.other_gain,  ctx_.other_muted,  ctx_.other_solo);
    }

    void paintRowBackground(juce::Graphics& g, juce::Rectangle<int> row, int i)
    {
        auto& r = rows_[(size_t)i];

        if (i % 2 == 1) {
            g.setColour(juce::Colour(AR::ELEVATED).withAlpha(0.4f));
            g.fillRect(row);
        }

        g.setColour(juce::Colour(AR::SURFACE));
        g.fillRect(row.removeFromBottom(1));

        // Colored dot + stem name
        auto leftCol   = row.removeFromLeft(LEFT_COL_W);
        auto dotCenter = leftCol.getCentre().toFloat();
        g.setColour(juce::Colour(r.color));
        g.fillEllipse(dotCenter.x - 20.0f, dotCenter.y - 6.0f, 12.0f, 12.0f);
        g.setFont(AR::font(AR::FontRole::label));
        g.setColour(juce::Colour(AR::FG));
        g.drawText(r.name, leftCol.withLeft(leftCol.getX() + 8), juce::Justification::centredLeft);

        // Waveform
        auto centerCol = row.withTrimmedRight(RIGHT_COL_W).reduced(0, 8);
        g.setColour(juce::Colour(AR::BG_DEEP));
        g.fillRect(centerCol);
        if (r.thumb && r.thumb->getTotalLength() > 0.0) {
            float alpha = (*r.ctx_muted) ? 0.25f : 0.75f;
            g.setColour(juce::Colour(r.color).withAlpha(alpha));
            r.thumb->drawChannels(g, centerCol, 0.0, r.thumb->getTotalLength(), 0.8f);
        }

        // Unity tick at center of gain slider track (3 buttons: play + mute + solo)
        auto rightCol  = juce::Rectangle<int>(row.getRight() - RIGHT_COL_W, row.getY(), RIGHT_COL_W, row.getHeight());
        int sliderLeft  = rightCol.getX() + TOGGLE_SIZE * 3 + TOGGLE_GAP * 4;
        int sliderRight = rightCol.getRight() - 8;
        int tickX       = sliderLeft + (sliderRight - sliderLeft) / 2;
        g.setColour(juce::Colour(AR::COMMENT));
        g.fillRect(tickX - 1, row.getCentreY() - 6, 2, 12);
    }

    void layoutRow(juce::Rectangle<int> row, int i)
    {
        auto& r = rows_[(size_t)i];
        auto rightCol = row.removeFromRight(RIGHT_COL_W).reduced(4, 12);

        auto sliderBounds = rightCol.removeFromRight(SLIDER_W);
        r.gain_slider.setBounds(sliderBounds);

        rightCol.removeFromRight(TOGGLE_GAP);
        r.solo_btn.setBounds(rightCol.removeFromRight(TOGGLE_SIZE).withSizeKeepingCentre(TOGGLE_SIZE, TOGGLE_SIZE));
        rightCol.removeFromRight(TOGGLE_GAP);
        r.mute_btn.setBounds(rightCol.removeFromRight(TOGGLE_SIZE).withSizeKeepingCentre(TOGGLE_SIZE, TOGGLE_SIZE));
        rightCol.removeFromRight(TOGGLE_GAP);
        r.play_btn.setBounds(rightCol.removeFromRight(TOGGLE_SIZE).withSizeKeepingCentre(TOGGLE_SIZE, TOGGLE_SIZE));
    }

    int stemRowAtY(int y) const
    {
        int rel = y - 40;
        if (rel < 0) return -1;
        int idx = rel / ROW_H;
        return (idx >= 0 && idx < 4) ? idx : -1;
    }

    juce::String stemPath(int idx) const
    {
        if (!ctx_.stems.valid) return {};
        switch (idx) {
            case 0: return juce::String(ctx_.stems.vocals.string());
            case 1: return juce::String(ctx_.stems.drums.string());
            case 2: return juce::String(ctx_.stems.bass.string());
            case 3: return juce::String(ctx_.stems.other.string());
            default: return {};
        }
    }

    void handlePlay(int idx)
    {
        if (!ctx_.is_stem_playing || !ctx_.play_stem || !ctx_.stop_stem) return;
        auto path = stemPath(idx);
        if (path.isEmpty()) return;
        if (ctx_.is_stem_playing(idx)) {
            ctx_.stop_stem(idx);
        } else {
            ctx_.play_stem(idx, juce::File(path));
        }
    }

    void handleMute(int idx)
    {
        *rows_[(size_t)idx].ctx_muted = rows_[(size_t)idx].mute_btn.getToggleState();
        repaint();
    }

    void handleSolo(int idx)
    {
        bool nowSolo = rows_[(size_t)idx].solo_btn.getToggleState();
        *rows_[(size_t)idx].ctx_solo = nowSolo;

        if (nowSolo) {
            for (int i = 0; i < 4; ++i) {
                if (i != idx) {
                    *rows_[(size_t)i].ctx_muted = true;
                    rows_[(size_t)i].mute_btn.setToggleState(true, juce::dontSendNotification);
                    *rows_[(size_t)i].ctx_solo  = false;
                    rows_[(size_t)i].solo_btn.setToggleState(false, juce::dontSendNotification);
                }
            }
            *rows_[(size_t)idx].ctx_muted = false;
            rows_[(size_t)idx].mute_btn.setToggleState(false, juce::dontSendNotification);
        } else {
            for (int i = 0; i < 4; ++i) {
                *rows_[(size_t)i].ctx_solo  = false;
                *rows_[(size_t)i].ctx_muted = false;
                rows_[(size_t)i].solo_btn.setToggleState(false, juce::dontSendNotification);
                rows_[(size_t)i].mute_btn.setToggleState(false, juce::dontSendNotification);
            }
        }
        repaint();
    }

    juce::AudioFormatManager  format_manager_;
    juce::AudioThumbnailCache thumbnail_cache_;
    juce::AudioThumbnail      vocals_thumb_;
    juce::AudioThumbnail      drums_thumb_;
    juce::AudioThumbnail      bass_thumb_;
    juce::AudioThumbnail      other_thumb_;

    int               drag_stem_idx_ = -1;
    juce::Point<int>  mouse_down_pos_;

    juce::TextButton back_btn_;
    juce::TextButton next_btn_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScreenStemsReady)
};
