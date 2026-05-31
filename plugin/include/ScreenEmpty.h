#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <thread>
#include <atomic>
#include "ScreenBase.h"
#include "AutoRemixLookAndFeel.h"
#include "PluginTypes.h"

class ScreenEmpty : public ScreenBase,
                    public juce::FileDragAndDropTarget,
                    public juce::ChangeListener
{
public:
    using AnalyzeFn      = std::function<autoremix::FileAnalysis(const juce::String&)>;
    using FileChooserFn  = std::function<void()>;

    ScreenEmpty(ScreenContext&  ctx,
                AnalyzeFn      analyze_fn,
                FileChooserFn  open_chooser_fn)
        : ScreenBase(ctx),
          analyze_fn_(std::move(analyze_fn)),
          open_chooser_fn_(std::move(open_chooser_fn)),
          thumbnail_cache_(5),
          thumbnail_(512, format_manager_, thumbnail_cache_)
    {
        format_manager_.registerBasicFormats();
        thumbnail_.addChangeListener(this);

        addAndMakeVisible(load_btn_);
        load_btn_.setButtonText("Load File");
        load_btn_.setComponentID("ghost");
        load_btn_.onClick = [this] { open_chooser_fn_(); };

        addAndMakeVisible(prepare_btn_);
        prepare_btn_.setButtonText("Prepare Stems >");
        prepare_btn_.setComponentID("primary");
        prepare_btn_.setEnabled(false);
        prepare_btn_.onClick = [this] { ctx_.navigate(ScreenId::Separating); };
    }

    ~ScreenEmpty() override
    {
        thumbnail_.removeChangeListener(this);
        if (cancel_analysis_token_) cancel_analysis_token_->store(true);
    }

    void onEnter() override
    {
        if (ctx_.file_path.isNotEmpty()) {
            juce::File f(ctx_.file_path);
            if (f.existsAsFile())
                thumbnail_.setSource(new juce::FileInputSource(f));
            prepare_btn_.setEnabled(true);
        } else {
            prepare_btn_.setEnabled(false);
        }
        updateAnalysisLabel();
        repaint();
    }

    void onExit() override
    {
        if (cancel_analysis_token_) cancel_analysis_token_->store(true);
    }

    void fileWasSelected(const juce::File& f)
    {
        ctx_.file_path    = f.getFullPathName();
        ctx_.detected_bpm = 120.0f;
        ctx_.detected_key = {};
        thumbnail_.setSource(new juce::FileInputSource(f));
        prepare_btn_.setEnabled(true);
        updateAnalysisLabel();
        repaint();
        runAnalysis();
    }

    // ── FileDragAndDropTarget ─────────────────────────────────────────────────
    bool isInterestedInFileDrag(const juce::StringArray& files) override
    {
        if (files.isEmpty()) return false;
        static const juce::StringArray kExts { ".wav", ".aif", ".aiff", ".mp3", ".flac", ".ogg", ".m4a" };
        return kExts.contains(juce::File(files[0]).getFileExtension().toLowerCase());
    }

    void fileDragEnter(const juce::StringArray&, int, int) override { drag_over_ = true;  repaint(); }
    void fileDragExit(const juce::StringArray&)              override { drag_over_ = false; repaint(); }

    void filesDropped(const juce::StringArray& files, int, int) override
    {
        drag_over_ = false;
        if (files.isEmpty()) return;
        juce::File f(files[0]);
        if (f.existsAsFile() && isInterestedInFileDrag(files))
            fileWasSelected(f);
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override { repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        g.fillAll(juce::Colour(AR::BG));

        if (drag_over_) {
            g.setColour(juce::Colour(AR::ACCENT).withAlpha(0.04f));
            g.fillRect(bounds);
        }

        auto contentArea = bounds.withTrimmedBottom(72);

        if (ctx_.file_path.isEmpty()) {
            // Headline
            g.setFont(AR::font(AR::FontRole::display_mega));
            g.setColour(juce::Colour(AR::COMMENT));
            g.drawText("Drop a song here",
                       contentArea.withTrimmedBottom(140),
                       juce::Justification::centred);

            // Subtitle (one-line intro to the app)
            g.setFont(AR::font(AR::FontRole::status));
            g.setColour(juce::Colour(AR::FG).withAlpha(0.7f));
            g.drawText(juce::String::fromUTF8(
                           "Separate stems  \xC2\xB7  Remix  \xC2\xB7  Mashup two tracks"),
                       juce::Rectangle<int>(0, contentArea.getCentreY() + 12,
                                            getWidth(), 22),
                       juce::Justification::centred);

            // Hint
            g.setFont(AR::font(AR::FontRole::secondary));
            g.setColour(juce::Colour(AR::COMMENT).withAlpha(0.7f));
            g.drawText("or click Load File below",
                       juce::Rectangle<int>(0, contentArea.getCentreY() + 60,
                                            getWidth(), 20),
                       juce::Justification::centred);
        } else {
            auto waveArea = contentArea.reduced(0, 16);
            g.setColour(juce::Colour(AR::BG_DEEP));
            g.fillRect(waveArea);

            if (thumbnail_.getTotalLength() > 0.0) {
                g.setColour(juce::Colour(AR::ACCENT).withAlpha(0.8f));
                thumbnail_.drawChannels(g, waveArea, 0.0, thumbnail_.getTotalLength(), 1.0f);
            }

            if (analysis_text_.isNotEmpty()) {
                auto infoRow = waveArea.removeFromBottom(24);
                g.setFont(AR::font(AR::FontRole::mono_value));
                g.setColour(juce::Colour(AR::FG).withAlpha(0.85f));
                g.drawText(analysis_text_, infoRow.reduced(8, 0),
                           juce::Justification::centredRight);
            }
        }

        if (drag_over_) {
            g.setColour(juce::Colour(AR::ACCENT));
            g.drawRect(bounds.toFloat(), 2.0f);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds();
        auto actionBar = b.removeFromBottom(72).reduced(16, 16);
        load_btn_.setBounds(actionBar.removeFromLeft(120));
        actionBar.removeFromLeft(8);
        prepare_btn_.setBounds(actionBar.removeFromRight(180));
    }

private:
    void updateAnalysisLabel()
    {
        if (ctx_.file_path.isEmpty()) { analysis_text_ = {}; return; }
        juce::String txt = juce::String(ctx_.detected_bpm, 1) + " BPM";
        txt += ctx_.detected_key.isNotEmpty() ? ("  |  " + ctx_.detected_key) : "  |  ?";
        analysis_text_ = txt;
    }

    void runAnalysis()
    {
        cancel_analysis_token_ = std::make_shared<std::atomic<bool>>(false);
        auto cancel = cancel_analysis_token_;
        auto path = ctx_.file_path;
        std::thread([this, cancel, path]() {
            auto info = analyze_fn_(path);
            if (cancel->load()) return;
            float        bpm = info.valid() ? info.bpm  : 120.0f;
            juce::String key = info.valid() ? juce::String(info.key) : "";
            juce::MessageManager::callAsync([this, cancel, bpm, key]() {
                if (cancel->load()) return;
                ctx_.detected_bpm = bpm;
                ctx_.detected_key = key;
                updateAnalysisLabel();
                repaint();
            });
        }).detach();
    }

    AnalyzeFn     analyze_fn_;
    FileChooserFn open_chooser_fn_;

    juce::AudioFormatManager  format_manager_;
    juce::AudioThumbnailCache thumbnail_cache_;
    juce::AudioThumbnail      thumbnail_;

    juce::TextButton  load_btn_;
    juce::TextButton  prepare_btn_;

    juce::String       analysis_text_;
    bool               drag_over_ = false;
    std::shared_ptr<std::atomic<bool>> cancel_analysis_token_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScreenEmpty)
};
