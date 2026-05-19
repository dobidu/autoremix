#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <thread>
#include <atomic>
#include <filesystem>
#include "ScreenBase.h"
#include "AutoRemixLookAndFeel.h"
#include "PluginTypes.h"

class ScreenRender : public ScreenBase,
                     public juce::Timer,
                     public juce::ChangeListener
{
public:
    using RenderFn = std::function<autoremix::ProcessResult(
        const autoremix::StemPaths&,
        const autoremix::RemixParams&,
        const std::filesystem::path&)>;

    ScreenRender(ScreenContext& ctx, RenderFn render_fn)
        : ScreenBase(ctx),
          render_fn_(std::move(render_fn)),
          thumbnail_cache_(12),
          in_thumb_ (512, format_manager_, thumbnail_cache_),
          b_thumb_  (512, format_manager_, thumbnail_cache_),
          out_thumb_(512, format_manager_, thumbnail_cache_),
          progress_value_(-1.0)
    {
        format_manager_.registerBasicFormats();
        in_thumb_ .addChangeListener(this);
        b_thumb_  .addChangeListener(this);
        out_thumb_.addChangeListener(this);

        addAndMakeVisible(header_lbl_);
        header_lbl_.setText("RENDERING...", juce::dontSendNotification);
        header_lbl_.setFont(AR::font(AR::FontRole::section));
        header_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::FG));
        header_lbl_.setJustificationType(juce::Justification::centred);

        addAndMakeVisible(timer_lbl_);
        timer_lbl_.setFont(AR::font(AR::FontRole::mono_value));
        timer_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));
        timer_lbl_.setJustificationType(juce::Justification::centred);

        addAndMakeVisible(progress_bar_);

        addAndMakeVisible(cancel_btn_);
        cancel_btn_.setButtonText("Cancel");
        cancel_btn_.setComponentID("ghost");
        cancel_btn_.onClick = [this] { requestCancel(); };

        addAndMakeVisible(done_lbl_);
        done_lbl_.setText("DONE", juce::dontSendNotification);
        done_lbl_.setFont(AR::font(AR::FontRole::section));
        done_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::SUCCESS));
        done_lbl_.setJustificationType(juce::Justification::centred);
        done_lbl_.setVisible(false);

        addAndMakeVisible(original_btn_);
        original_btn_.setButtonText(juce::String::fromUTF8("\xE2\x96\xB6 Original"));
        original_btn_.setComponentID("ghost");
        original_btn_.onClick = [this] { handleOriginalPlay(); };
        original_btn_.setVisible(false);

        addAndMakeVisible(remix_btn_);
        remix_btn_.setButtonText(juce::String::fromUTF8("\xE2\x96\xB6 Remix"));
        remix_btn_.setComponentID("ghost");
        remix_btn_.onClick = [this] { handleRemixPlay(); };
        remix_btn_.setVisible(false);

        addAndMakeVisible(save_btn_);
        save_btn_.setButtonText("Save");
        save_btn_.setComponentID("ghost");
        save_btn_.onClick = [this] { saveOutput(); };
        save_btn_.setVisible(false);

        addAndMakeVisible(new_remix_btn_);
        new_remix_btn_.setButtonText("New Remix");
        new_remix_btn_.setComponentID("primary");
        new_remix_btn_.onClick = [this] { ctx_.navigate(ScreenId::ModeParams); };
        new_remix_btn_.setVisible(false);

        addAndMakeVisible(track_b_btn_);
        track_b_btn_.setButtonText(juce::String::fromUTF8("\xE2\x96\xB6 Track B"));
        track_b_btn_.setComponentID("ghost");
        track_b_btn_.onClick = [this] { handleTrackBPlay(); };
        track_b_btn_.setVisible(false);

        addAndMakeVisible(new_mashup_btn_);
        new_mashup_btn_.setButtonText("New Mashup");
        new_mashup_btn_.setComponentID("primary_mashup");
        new_mashup_btn_.onClick = [this] { ctx_.navigate(ScreenId::Mashup); };
        new_mashup_btn_.setVisible(false);

        addAndMakeVisible(new_file_btn_);
        new_file_btn_.setButtonText("New File");
        new_file_btn_.setComponentID("ghost");
        new_file_btn_.onClick = [this] { resetAndGoEmpty(); };
        new_file_btn_.setVisible(false);

        addAndMakeVisible(error_lbl_);
        error_lbl_.setFont(AR::font(AR::FontRole::value));
        error_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::ERROR));
        error_lbl_.setJustificationType(juce::Justification::centred);
        error_lbl_.setVisible(false);

        addAndMakeVisible(retry_btn_);
        retry_btn_.setButtonText("Try Again");
        retry_btn_.setComponentID("ghost");
        retry_btn_.onClick = [this] { ctx_.navigate(ScreenId::ModeParams); };
        retry_btn_.setVisible(false);
    }

    ~ScreenRender() override
    {
        in_thumb_ .removeChangeListener(this);
        b_thumb_  .removeChangeListener(this);
        out_thumb_.removeChangeListener(this);
        stopTimer();
        cancel_requested_.store(true);
    }

    void onEnter() override
    {
        elapsed_secs_ = 0;
        preview_mode_ = PreviewMode::None;
        cancel_requested_.store(false);
        timer_lbl_.setText("0 s", juce::dontSendNotification);

        in_thumb_.setSource(new juce::FileInputSource(juce::File(ctx_.file_path)));

        if (ctx_.render_is_mashup && ctx_.output_path.isNotEmpty()) {
            // Mashup already rendered by ScreenMashup — just show it.
            if (ctx_.file_path_b.isNotEmpty())
                b_thumb_.setSource(new juce::FileInputSource(juce::File(ctx_.file_path_b)));
            else
                b_thumb_.setSource(nullptr);
            out_thumb_.setSource(new juce::FileInputSource(juce::File(ctx_.output_path)));
            state_ = State::Done;
            applyState();
            startTimer(100);   // playback-position polling
            resized();
            repaint();
        } else {
            state_ = State::Rendering;
            applyState();
            startTimer(1000);
            startRender();
            resized();
            repaint();
        }
    }

    void onExit() override
    {
        stopTimer();
        cancel_requested_.store(true);
        if (ctx_.stop_preview) ctx_.stop_preview();
    }

    void timerCallback() override
    {
        if (state_ == State::Rendering) {
            ++elapsed_secs_;
            timer_lbl_.setText(juce::String(elapsed_secs_) + " s", juce::dontSendNotification);
        } else if (state_ == State::Done) {
            bool playing = ctx_.is_preview_playing && ctx_.is_preview_playing();
            if (playing) {
                preview_position_ = ctx_.get_preview_position ? ctx_.get_preview_position() : 0.0;
                repaint();
            } else if (was_preview_playing_) {
                // playback just stopped (EOF or user) — clear cursor, reset buttons
                preview_position_ = 0.0;
                preview_mode_     = PreviewMode::None;
                original_btn_.setButtonText(originalIdleText());
                remix_btn_   .setButtonText(remixIdleText());
                track_b_btn_ .setButtonText(trackBIdleText());
                repaint();
            }
            was_preview_playing_ = playing;
        }
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override { repaint(); }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(AR::BG));

        if (state_ == State::Done) {
            auto fullContent = getLocalBounds()
                .withTrimmedTop(80)
                .withTrimmedBottom(72);

            const bool mashup = ctx_.render_is_mashup;

            if (mashup) {
                // Three sections: TRACK A / TRACK B / MASHUP
                int third = fullContent.getHeight() / 3;
                auto aSection = fullContent.removeFromTop(third).reduced(32, 4);
                auto bSection = fullContent.removeFromTop(third).reduced(32, 4);
                auto mSection = fullContent.reduced(32, 4);

                g.setFont(AR::font(AR::FontRole::section_label));
                g.setColour(juce::Colour(AR::COMMENT));
                g.drawText("TRACK A", aSection.removeFromTop(14), juce::Justification::centredLeft);
                g.drawText("TRACK B", bSection.removeFromTop(14), juce::Justification::centredLeft);
                g.drawText("MASHUP",  mSection.removeFromTop(14), juce::Justification::centredLeft);

                // Track A
                g.setColour(juce::Colour(AR::BG_DEEP));
                g.fillRect(aSection);
                if (in_thumb_.getTotalLength() > 0.0) {
                    g.setColour(juce::Colour(AR::ACCENT).withAlpha(0.6f));
                    in_thumb_.drawChannels(g, aSection, 0.0, in_thumb_.getTotalLength(), 1.0f);
                }
                if (preview_mode_ == PreviewMode::Original && was_preview_playing_) {
                    int cx = aSection.getX() + (int)(preview_position_ * aSection.getWidth());
                    g.setColour(juce::Colour(AR::FG).withAlpha(0.9f));
                    g.fillRect(cx, aSection.getY(), 2, aSection.getHeight());
                }

                // Track B
                g.setColour(juce::Colour(AR::BG_DEEP));
                g.fillRect(bSection);
                if (b_thumb_.getTotalLength() > 0.0) {
                    g.setColour(juce::Colour(AR::STEM_VOCALS).withAlpha(0.75f));
                    b_thumb_.drawChannels(g, bSection, 0.0, b_thumb_.getTotalLength(), 1.0f);
                }
                if (preview_mode_ == PreviewMode::TrackB && was_preview_playing_) {
                    int cx = bSection.getX() + (int)(preview_position_ * bSection.getWidth());
                    g.setColour(juce::Colour(AR::FG).withAlpha(0.9f));
                    g.fillRect(cx, bSection.getY(), 2, bSection.getHeight());
                }

                // Mashup
                g.setColour(juce::Colour(AR::BG_DEEP));
                g.fillRect(mSection);
                if (out_thumb_.getTotalLength() > 0.0) {
                    g.setColour(juce::Colour(AR::MASHUP).withAlpha(0.95f));
                    out_thumb_.drawChannels(g, mSection, 0.0, out_thumb_.getTotalLength(), 1.0f);
                }
                if (preview_mode_ == PreviewMode::Remix && was_preview_playing_) {
                    int cx = mSection.getX() + (int)(preview_position_ * mSection.getWidth());
                    g.setColour(juce::Colour(AR::FG).withAlpha(0.9f));
                    g.fillRect(cx, mSection.getY(), 2, mSection.getHeight());
                }
            } else {
                // Two-section remix layout (unchanged)
                int halfH = fullContent.getHeight() / 2;
                auto origSection  = fullContent.removeFromTop(halfH).reduced(32, 6);
                auto remixSection = fullContent.reduced(32, 6);

                g.setFont(AR::font(AR::FontRole::section_label));
                g.setColour(juce::Colour(AR::COMMENT));
                g.drawText("ORIGINAL", origSection.removeFromTop(14), juce::Justification::centredLeft);
                g.drawText("REMIX",    remixSection.removeFromTop(14), juce::Justification::centredLeft);

                g.setColour(juce::Colour(AR::BG_DEEP));
                g.fillRect(origSection);
                if (in_thumb_.getTotalLength() > 0.0) {
                    g.setColour(juce::Colour(AR::ACCENT).withAlpha(0.6f));
                    in_thumb_.drawChannels(g, origSection, 0.0, in_thumb_.getTotalLength(), 1.0f);
                }
                if (preview_mode_ == PreviewMode::Original && was_preview_playing_) {
                    int cx = origSection.getX() + (int)(preview_position_ * origSection.getWidth());
                    g.setColour(juce::Colour(AR::FG).withAlpha(0.9f));
                    g.fillRect(cx, origSection.getY(), 2, origSection.getHeight());
                }

                g.setColour(juce::Colour(AR::BG_DEEP));
                g.fillRect(remixSection);
                if (out_thumb_.getTotalLength() > 0.0) {
                    g.setColour(juce::Colour(AR::ACCENT).withAlpha(0.85f));
                    out_thumb_.drawChannels(g, remixSection, 0.0, out_thumb_.getTotalLength(), 1.0f);
                }
                if (preview_mode_ == PreviewMode::Remix && was_preview_playing_) {
                    int cx = remixSection.getX() + (int)(preview_position_ * remixSection.getWidth());
                    g.setColour(juce::Colour(AR::FG).withAlpha(0.9f));
                    g.fillRect(cx, remixSection.getY(), 2, remixSection.getHeight());
                }
            }
        }
    }

    void resized() override
    {
        auto b = getLocalBounds();
        auto actionBar = b.removeFromBottom(72).reduced(16, 16);

        auto headerArea = b.removeFromTop(80).reduced(16, 8);
        auto headerFirstLine = headerArea.removeFromTop(32);
        header_lbl_.setBounds(headerFirstLine);
        done_lbl_  .setBounds(headerFirstLine);
        timer_lbl_ .setBounds(headerArea.removeFromTop(24));

        auto midArea = b.withTrimmedBottom(0).reduced(32, 16);
        progress_bar_.setBounds(midArea.removeFromTop(8));

        error_lbl_.setBounds(b.withTrimmedTop(80).withTrimmedBottom(72).reduced(32, 40));

        switch (state_) {
            case State::Rendering:
                cancel_btn_.setBounds(actionBar.withSizeKeepingCentre(120, 36));
                break;
            case State::Done: {
                bool mashup = ctx_.render_is_mashup;
                new_file_btn_ .setBounds(actionBar.removeFromLeft(100));
                actionBar.removeFromLeft(8);
                int playW = mashup ? 100 : 120;
                original_btn_ .setBounds(actionBar.removeFromLeft(playW));
                actionBar.removeFromLeft(6);
                if (mashup) {
                    track_b_btn_ .setBounds(actionBar.removeFromLeft(playW));
                    actionBar.removeFromLeft(6);
                }
                remix_btn_    .setBounds(actionBar.removeFromLeft(playW));
                actionBar.removeFromLeft(6);
                save_btn_     .setBounds(actionBar.removeFromLeft(80));
                if (mashup)
                    new_mashup_btn_.setBounds(actionBar.removeFromRight(140));
                else
                    new_remix_btn_ .setBounds(actionBar.removeFromRight(140));
                break;
            }
            case State::Error:
                new_file_btn_.setBounds(actionBar.removeFromLeft(120));
                retry_btn_   .setBounds(actionBar.removeFromRight(140));
                break;
        }
    }

private:
    enum class State     { Rendering, Done, Error };
    enum class PreviewMode { None, Original, Remix, TrackB };

    void applyState()
    {
        bool rendering = (state_ == State::Rendering);
        bool done      = (state_ == State::Done);
        bool error     = (state_ == State::Error);
        bool mashup    = ctx_.render_is_mashup;

        header_lbl_   .setVisible(rendering);
        timer_lbl_    .setVisible(rendering);
        progress_bar_ .setVisible(rendering);
        cancel_btn_   .setVisible(rendering);

        // Done header — different text/color for mashup
        if (done) {
            done_lbl_.setText(mashup ? "MASHUP DONE" : "DONE", juce::dontSendNotification);
            done_lbl_.setColour(juce::Label::textColourId,
                                juce::Colour(mashup ? AR::MASHUP : AR::SUCCESS));
        }
        done_lbl_      .setVisible(done);

        // Play buttons (renamed in mashup mode)
        original_btn_  .setVisible(done);
        remix_btn_     .setVisible(done);
        track_b_btn_   .setVisible(done && mashup);
        if (done) {
            original_btn_.setButtonText(originalIdleText());
            remix_btn_   .setButtonText(remixIdleText());
            track_b_btn_ .setButtonText(trackBIdleText());
        }

        save_btn_      .setVisible(done);
        new_remix_btn_ .setVisible(done && !mashup);
        new_mashup_btn_.setVisible(done && mashup);

        error_lbl_     .setVisible(error);
        retry_btn_     .setVisible(error);

        new_file_btn_  .setVisible(done || error);
    }

    void startRender()
    {
        autoremix::RemixParams params;
        params.tempo_factor     = (ctx_.detected_bpm > 0.0f)
                                  ? (ctx_.target_bpm / ctx_.detected_bpm)
                                  : 1.0f;
        params.pitch_shift_semi = ctx_.pitch_semi;
        params.reverb_mix       = ctx_.reverb_mix;
        params.chop_interval_ms = (ctx_.target_bpm > 0.0f)
                                  ? (ctx_.chop_beats * 60000.0f / ctx_.target_bpm)
                                  : 0.0f;
        params.vocals_gain      = ctx_.vocals_muted ? 0.0f : ctx_.vocals_gain;
        params.drums_gain       = ctx_.drums_muted  ? 0.0f : ctx_.drums_gain;
        params.bass_gain        = ctx_.bass_muted   ? 0.0f : ctx_.bass_gain;
        params.other_gain       = ctx_.other_muted  ? 0.0f : ctx_.other_gain;

        static const char* kChopModes[] = {"fixed","beat","onset","bar","energy","structural"};
        params.chop_mode = kChopModes[juce::jlimit(0, 5, ctx_.chop_mode_idx)];

        if (!ctx_.presets.empty() && ctx_.selected_preset_idx < (int)ctx_.presets.size()) {
            const auto& preset_params = ctx_.presets[(size_t)ctx_.selected_preset_idx].default_params;
            params.engine_id    = preset_params.engine_id;
            params.separator_id = preset_params.separator_id;
        }

        if (!ctx_.presets.empty() && ctx_.selected_preset_idx < (int)ctx_.presets.size())
            params.engine_id = ctx_.presets[(size_t)ctx_.selected_preset_idx].id;

        juce::File inFile(ctx_.file_path);
        juce::String outName = inFile.getFileNameWithoutExtension() + "_remix.wav";
        std::filesystem::path outputPath =
            juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getChildFile("autoremix")
                .getChildFile("output")
                .getChildFile(outName)
                .getFullPathName().toStdString();
        std::filesystem::create_directories(std::filesystem::path(outputPath).parent_path());

        auto stems  = ctx_.stems;
        auto render = render_fn_;

        std::thread([this, params, stems, outputPath, render]() mutable {
            auto result = render(stems, params, outputPath);
            if (cancel_requested_.load()) return;

            juce::MessageManager::callAsync([this, result, outputPath]() mutable {
                stopTimer();
                if (result.success) {
                    ctx_.output_path = juce::String(outputPath.string());
                    out_thumb_.setSource(new juce::FileInputSource(
                        juce::File(ctx_.output_path)));
                    state_ = State::Done;
                    preview_mode_ = PreviewMode::None;
                    startTimer(100);
                } else {
                    state_ = State::Error;
                    error_lbl_.setText(juce::String(result.error_message),
                                       juce::dontSendNotification);
                }
                applyState();
                resized();
                repaint();
            });
        }).detach();
    }

    void requestCancel()
    {
        cancel_requested_.store(true);
        stopTimer();
        ctx_.navigate(ScreenId::ModeParams);
    }

    void handleOriginalPlay()
    {
        bool playing = ctx_.is_preview_playing && ctx_.is_preview_playing();
        if (preview_mode_ == PreviewMode::Original && playing) {
            ctx_.stop_preview();
            preview_mode_ = PreviewMode::None;
            original_btn_.setButtonText(originalIdleText());
        } else {
            if (ctx_.stop_preview) ctx_.stop_preview();
            preview_mode_ = PreviewMode::Original;
            remix_btn_  .setButtonText(remixIdleText());
            track_b_btn_.setButtonText(trackBIdleText());
            if (ctx_.play_preview) ctx_.play_preview(juce::File(ctx_.file_path));
            original_btn_.setButtonText(originalPlayingText());
        }
    }

    void handleRemixPlay()
    {
        bool playing = ctx_.is_preview_playing && ctx_.is_preview_playing();
        if (preview_mode_ == PreviewMode::Remix && playing) {
            ctx_.stop_preview();
            preview_mode_ = PreviewMode::None;
            remix_btn_.setButtonText(remixIdleText());
        } else {
            if (ctx_.stop_preview) ctx_.stop_preview();
            preview_mode_ = PreviewMode::Remix;
            original_btn_.setButtonText(originalIdleText());
            track_b_btn_ .setButtonText(trackBIdleText());
            if (ctx_.play_preview) ctx_.play_preview(juce::File(ctx_.output_path));
            remix_btn_.setButtonText(remixPlayingText());
        }
    }

    void handleTrackBPlay()
    {
        bool playing = ctx_.is_preview_playing && ctx_.is_preview_playing();
        if (preview_mode_ == PreviewMode::TrackB && playing) {
            ctx_.stop_preview();
            preview_mode_ = PreviewMode::None;
            track_b_btn_.setButtonText(trackBIdleText());
        } else {
            if (ctx_.stop_preview) ctx_.stop_preview();
            preview_mode_ = PreviewMode::TrackB;
            original_btn_.setButtonText(originalIdleText());
            remix_btn_   .setButtonText(remixIdleText());
            if (ctx_.play_preview) ctx_.play_preview(juce::File(ctx_.file_path_b));
            track_b_btn_.setButtonText(trackBPlayingText());
        }
    }

    juce::String originalIdleText() const {
        return juce::String::fromUTF8(ctx_.render_is_mashup ? "\xE2\x96\xB6 Track A"
                                                            : "\xE2\x96\xB6 Original");
    }
    juce::String originalPlayingText() const {
        return juce::String::fromUTF8(ctx_.render_is_mashup ? "\xE2\x96\xA0 Track A"
                                                            : "\xE2\x96\xA0 Original");
    }
    juce::String remixIdleText() const {
        return juce::String::fromUTF8(ctx_.render_is_mashup ? "\xE2\x96\xB6 Mashup"
                                                            : "\xE2\x96\xB6 Remix");
    }
    juce::String remixPlayingText() const {
        return juce::String::fromUTF8(ctx_.render_is_mashup ? "\xE2\x96\xA0 Mashup"
                                                            : "\xE2\x96\xA0 Remix");
    }
    juce::String trackBIdleText() const {
        return juce::String::fromUTF8("\xE2\x96\xB6 Track B");
    }
    juce::String trackBPlayingText() const {
        return juce::String::fromUTF8("\xE2\x96\xA0 Track B");
    }

    void saveOutput()
    {
        chooser_ = std::make_unique<juce::FileChooser>(
            "Save remix as...",
            juce::File::getSpecialLocation(juce::File::userMusicDirectory),
            "*.wav");
        chooser_->launchAsync(
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                auto dest = fc.getResult();
                if (dest.getFullPathName().isEmpty()) return;
                juce::File src(ctx_.output_path);
                if (!src.existsAsFile()) {
                    if (ctx_.set_status) ctx_.set_status("Save failed: source file missing");
                    return;
                }
                if (src.copyFileTo(dest)) {
                    if (ctx_.set_status) ctx_.set_status("Saved: " + dest.getFileName());
                    dest.revealToUser();
                } else {
                    if (ctx_.set_status) ctx_.set_status("Save failed: copy error");
                }
            });
    }

    void resetAndGoEmpty()
    {
        if (ctx_.stop_preview) ctx_.stop_preview();
        ctx_.file_path   = {};
        ctx_.stems       = {};
        ctx_.output_path = {};
        ctx_.navigate(ScreenId::Empty);
    }

    State       state_        = State::Rendering;
    PreviewMode preview_mode_ = PreviewMode::None;
    int         elapsed_secs_ = 0;
    double      preview_position_    = 0.0;
    bool        was_preview_playing_ = false;
    std::atomic<bool> cancel_requested_{false};
    RenderFn          render_fn_;

    juce::AudioFormatManager  format_manager_;
    juce::AudioThumbnailCache thumbnail_cache_;
    juce::AudioThumbnail      in_thumb_;
    juce::AudioThumbnail      b_thumb_;
    juce::AudioThumbnail      out_thumb_;

    std::unique_ptr<juce::FileChooser> chooser_;

    double            progress_value_;
    juce::Label       header_lbl_;
    juce::Label       timer_lbl_;
    juce::ProgressBar progress_bar_{progress_value_};
    juce::TextButton  cancel_btn_;

    juce::Label      done_lbl_;
    juce::TextButton original_btn_;
    juce::TextButton remix_btn_;
    juce::TextButton track_b_btn_;
    juce::TextButton save_btn_;
    juce::TextButton new_remix_btn_;
    juce::TextButton new_mashup_btn_;
    juce::TextButton new_file_btn_;

    juce::Label      error_lbl_;
    juce::TextButton retry_btn_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScreenRender)
};
