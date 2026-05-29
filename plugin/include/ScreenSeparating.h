#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <thread>
#include <atomic>
#include <array>
#include <filesystem>
#include "ScreenBase.h"
#include "AutoRemixLookAndFeel.h"
#include "PluginTypes.h"

class ScreenSeparating : public ScreenBase,
                         public juce::Timer
{
public:
    using SeparateFn = std::function<autoremix::StemPaths(
        const std::filesystem::path&,
        const std::filesystem::path&,
        const std::string&)>;

    ScreenSeparating(ScreenContext& ctx, SeparateFn separate_fn)
        : ScreenBase(ctx), separate_fn_(std::move(separate_fn)), progress_value_(-1.0)
    {
        stems_[0] = { "Vocals", AR::STEM_VOCALS, StemStatus::Waiting };
        stems_[1] = { "Drums",  AR::STEM_DRUMS,  StemStatus::Waiting };
        stems_[2] = { "Bass",   AR::STEM_BASS,   StemStatus::Waiting };
        stems_[3] = { "Other",  AR::STEM_OTHER,  StemStatus::Waiting };

        addAndMakeVisible(header_lbl_);
        header_lbl_.setFont(AR::font(AR::FontRole::section));
        header_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::FG));
        header_lbl_.setText("SEPARATING STEMS", juce::dontSendNotification);
        header_lbl_.setJustificationType(juce::Justification::centred);

        addAndMakeVisible(timer_lbl_);
        timer_lbl_.setFont(AR::font(AR::FontRole::mono_value));
        timer_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));
        timer_lbl_.setText("0 s", juce::dontSendNotification);
        timer_lbl_.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(progress_bar_);

        addAndMakeVisible(cancel_btn_);
        cancel_btn_.setButtonText("Cancel");
        cancel_btn_.setComponentID("ghost");
        cancel_btn_.onClick = [this] { requestCancel(); };
    }

    ~ScreenSeparating() override
    {
        stopTimer();
        if (cancel_token_) cancel_token_->store(true);
    }

    void onEnter() override
    {
        elapsed_secs_ = 0;
        cancel_token_ = std::make_shared<std::atomic<bool>>(false);
        ctx_.separation_cancel_token = cancel_token_;  // shared ownership keeps atomic alive
        mashup_mode_  = ctx_.mashup_mode_separating;
        header_lbl_.setText(mashup_mode_ ? "SEPARATING TRACK B" : "SEPARATING STEMS",
                            juce::dontSendNotification);
        for (auto& s : stems_) s.status = StemStatus::Waiting;
        startTimer(1000);
        startSeparation();
        repaint();
    }

    void onExit() override
    {
        stopTimer();
        ctx_.separation_cancel_token.reset();
    }

    void timerCallback() override
    {
        ++elapsed_secs_;
        timer_lbl_.setText(juce::String(elapsed_secs_) + " s", juce::dontSendNotification);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(AR::BG));

        auto rowsArea = getLocalBounds()
            .withTrimmedTop(88)
            .withTrimmedBottom(72);

        for (int i = 0; i < 4; ++i) {
            auto row = rowsArea.removeFromTop(56);
            paintStemRow(g, row, stems_[(size_t)i]);
            g.setColour(juce::Colour(AR::SURFACE));
            g.fillRect(row.removeFromBottom(1));
        }
    }

    void resized() override
    {
        auto b = getLocalBounds();
        auto headerArea = b.removeFromTop(88);
        header_lbl_.setBounds(headerArea.removeFromTop(44).reduced(16, 8));
        timer_lbl_.setBounds(headerArea.removeFromRight(100).reduced(8, 0));

        auto footerArea = b.removeFromBottom(72).reduced(16, 16);
        progress_bar_.setBounds(footerArea.removeFromTop(8));
        footerArea.removeFromTop(8);
        cancel_btn_.setBounds(footerArea.withSizeKeepingCentre(120, 36));
    }

private:
    enum class StemStatus { Waiting, Separating, Done, Failed };

    struct StemRow {
        juce::String name;
        uint32_t     color;
        StemStatus   status;
    };

    void paintStemRow(juce::Graphics& g, juce::Rectangle<int> row, const StemRow& stem)
    {
        auto dotArea = row.removeFromLeft(48);
        auto dotCenter = dotArea.getCentre().toFloat();
        g.setColour(juce::Colour(stem.color));
        g.fillEllipse(dotCenter.x - 6.0f, dotCenter.y - 6.0f, 12.0f, 12.0f);

        auto nameArea = row.removeFromLeft(120);
        g.setFont(AR::font(AR::FontRole::label));
        g.setColour(juce::Colour(AR::FG));
        g.drawText(stem.name, nameArea, juce::Justification::centredLeft);

        juce::String statusText;
        juce::Colour statusColor;
        switch (stem.status) {
            case StemStatus::Waiting:
                statusText  = "Waiting...";
                statusColor = juce::Colour(AR::COMMENT);
                break;
            case StemStatus::Separating:
                statusText  = "Separating...";
                statusColor = juce::Colour(AR::ACCENT);
                break;
            case StemStatus::Done:
                statusText  = "Done";
                statusColor = juce::Colour(AR::SUCCESS);
                break;
            case StemStatus::Failed:
                statusText  = "Failed";
                statusColor = juce::Colour(AR::ERR);
                break;
        }
        g.setFont(AR::font(AR::FontRole::mono_label));
        g.setColour(statusColor);
        g.drawText(statusText, row, juce::Justification::centredLeft);
    }

    void startSeparation()
    {
        const juce::String inputPathStr = mashup_mode_ ? ctx_.file_path_b : ctx_.file_path;
        const char* subdir = mashup_mode_ ? "stems_b" : "stems";

        juce::File inputFile(inputPathStr);
        auto stemsDirJuce = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("autoremix")
                                .getChildFile(subdir)
                                .getChildFile(inputFile.getFileNameWithoutExtension());
        std::filesystem::path outputDir = stemsDirJuce.getFullPathName().toStdString();
        std::filesystem::create_directories(outputDir);

        std::string separator_id = ctx_.separators.empty()
            ? "algorithmic"
            : ctx_.separators[(size_t)ctx_.selected_separator_idx].id;

        std::thread([this,
                     inputPath = inputPathStr.toStdString(),
                     outputDir, separator_id]() mutable {
            juce::MessageManager::callAsync([this] {
                for (auto& s : stems_) s.status = StemStatus::Separating;
                repaint();
            });

            auto result = separate_fn_(
                std::filesystem::path(inputPath),
                outputDir,
                separator_id
            );

            if (cancel_token_ && cancel_token_->load()) return;

            juce::MessageManager::callAsync([this, result]() mutable {
                if (result.valid) {
                    for (auto& s : stems_) s.status = StemStatus::Done;
                    repaint();
                    if (mashup_mode_) {
                        ctx_.stems_b = result;
                        ctx_.mashup_mode_separating = false;
                        if (ctx_.set_status) ctx_.set_status("Track B ready");
                        ctx_.navigate(ScreenId::Mashup);
                    } else {
                        ctx_.stems = result;
                        if (ctx_.set_status) ctx_.set_status("Stems ready");
                        ctx_.navigate(ScreenId::StemsReady);
                    }
                } else {
                    for (auto& s : stems_) s.status = StemStatus::Failed;
                    repaint();
                    ctx_.set_status(mashup_mode_
                        ? juce::String("Track B separation failed.")
                        : juce::String("Stem separation failed."));
                }
            });
        }).detach();
    }

    void requestCancel()
    {
        if (cancel_token_) cancel_token_->store(true);
        if (mashup_mode_) {
            ctx_.stems_b = {};
            ctx_.mashup_mode_separating = false;
            ctx_.navigate(ScreenId::StemsReady);
        } else {
            ctx_.stems = {};
            ctx_.navigate(ScreenId::Empty);
        }
    }

    SeparateFn              separate_fn_;
    std::array<StemRow, 4>  stems_;

    juce::Label       header_lbl_;
    juce::Label       timer_lbl_;
    double            progress_value_;
    juce::ProgressBar progress_bar_{progress_value_};
    juce::TextButton  cancel_btn_;

    int  elapsed_secs_ = 0;
    bool mashup_mode_  = false;
    std::shared_ptr<std::atomic<bool>> cancel_token_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScreenSeparating)
};
