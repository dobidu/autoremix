#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>
#include <thread>
#include <atomic>
#include "ScreenBase.h"
#include "AutoRemixLookAndFeel.h"
#include "PluginTypes.h"
#include "dsp/NativeCueReader.h"
#include "dsp/NativeCueSidecar.h"

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

                // Cue point overlays
                const double dur = thumbnail_.getTotalLength();
                for (const auto& cue : ctx_.cue_points) {
                    const float x = (float)(cue.position_sec / dur) * waveArea.getWidth()
                                    + waveArea.getX();
                    const float alpha = (cue.source == "auto") ? 0.45f : 1.0f;
                    g.setColour(juce::Colour((juce::uint32)cue.color_rgb).withAlpha(alpha));
                    g.drawLine(x, (float)waveArea.getY(), x, (float)waveArea.getBottom(), 1.0f);
                    g.setFont(AR::font(AR::FontRole::secondary));
                    g.drawText(juce::String(cue.name), (int)x + 2, waveArea.getY(), 60, 12,
                               juce::Justification::left, true);
                }
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

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (ctx_.file_path.isEmpty() || thumbnail_.getTotalLength() <= 0.0) return;

        namespace an = autoremix::dsp::analysis;
        auto bounds      = getLocalBounds();
        auto contentArea = bounds.withTrimmedBottom(72);
        auto waveArea    = contentArea.reduced(0, 16);

        // Ctrl+click → place new cue
        if (e.mods.isCtrlDown() && waveArea.contains(e.getPosition())) {
            const double ratio = std::clamp(
                (e.x - waveArea.getX()) / (double)waveArea.getWidth(), 0.0, 1.0);
            const double pos = ratio * thumbnail_.getTotalLength();
            const int n = (int)std::count_if(ctx_.cue_points.begin(), ctx_.cue_points.end(),
                              [](const an::CuePoint& c){ return c.source == "user"; }) + 1;

            juce::AlertWindow aw("Place Cue", "Name:", juce::MessageBoxIconType::NoIcon);
            aw.addTextEditor("name", "cue_" + juce::String(n), "Cue name:");
            aw.addButton("OK",     1, juce::KeyPress(juce::KeyPress::returnKey));
            aw.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
            if (aw.runModalLoop() != 1) return;

            const auto name = aw.getTextEditorContents("name").trim();
            if (name.isEmpty()) return;

            an::CuePoint c;
            c.name         = name.toStdString();
            c.position_sec = pos;
            c.color_rgb    = 0xFFD4652Au;
            c.source       = "user";
            ctx_.cue_points.push_back(c);
            std::sort(ctx_.cue_points.begin(), ctx_.cue_points.end(),
                [](const an::CuePoint& a, const an::CuePoint& b){
                    return a.position_sec < b.position_sec; });
            save_user_cues(juce::File(ctx_.file_path.toStdString()), ctx_.cue_points);
            if (ctx_.on_cues_changed) ctx_.on_cues_changed();
            repaint();
            return;
        }

        // Right-click near existing cue marker → rename / delete
        if (e.mods.isRightButtonDown()) {
            const double dur = thumbnail_.getTotalLength();
            for (size_t i = 0; i < ctx_.cue_points.size(); ++i) {
                const float cx = (float)(ctx_.cue_points[i].position_sec / dur)
                                 * waveArea.getWidth() + waveArea.getX();
                if (std::abs((float)e.x - cx) <= 6.0f) {
                    juce::PopupMenu menu;
                    menu.addItem(1, "Rename...");
                    menu.addItem(2, "Delete");
                    menu.showMenuAsync(juce::PopupMenu::Options{},
                        [this, i](int result) {
                            if (result == 1) {
                                if (i >= ctx_.cue_points.size()) return;
                                juce::AlertWindow aw2("Rename Cue", "New name:",
                                                      juce::MessageBoxIconType::NoIcon);
                                aw2.addTextEditor("name",
                                    juce::String(ctx_.cue_points[i].name), "Name:");
                                aw2.addButton("OK",     1, juce::KeyPress(juce::KeyPress::returnKey));
                                aw2.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
                                if (aw2.runModalLoop() == 1) {
                                    const auto nm = aw2.getTextEditorContents("name").trim();
                                    if (nm.isNotEmpty()) {
                                        ctx_.cue_points[i].name = nm.toStdString();
                                        save_user_cues(juce::File(ctx_.file_path.toStdString()),
                                                       ctx_.cue_points);
                                        if (ctx_.on_cues_changed) ctx_.on_cues_changed();
                                        repaint();
                                    }
                                }
                            } else if (result == 2) {
                                if (i >= ctx_.cue_points.size()) return;
                                ctx_.cue_points.erase(ctx_.cue_points.begin() + (ptrdiff_t)i);
                                save_user_cues(juce::File(ctx_.file_path.toStdString()),
                                               ctx_.cue_points);
                                if (ctx_.on_cues_changed) ctx_.on_cues_changed();
                                repaint();
                            }
                        });
                    return;
                }
            }
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

    void loadAndMergeCues()
    {
        namespace an = autoremix::dsp::analysis;
        const juce::File f(ctx_.file_path.toStdString());
        auto file_cues = load_cues_for_file(f);
        auto user_cues = load_user_cues(f);

        ctx_.cue_points = ctx_.song_structure.cue_points;

        auto mergeCue = [this](an::CuePoint c) {
            auto it = std::find_if(ctx_.cue_points.begin(), ctx_.cue_points.end(),
                [&](const an::CuePoint& x){
                    return std::abs(x.position_sec - c.position_sec) < 0.1; });
            if (it != ctx_.cue_points.end()) *it = std::move(c);
            else ctx_.cue_points.push_back(std::move(c));
        };
        for (auto& c : file_cues) mergeCue(std::move(c));
        for (auto& c : user_cues) mergeCue(std::move(c));

        std::sort(ctx_.cue_points.begin(), ctx_.cue_points.end(),
            [](const an::CuePoint& a, const an::CuePoint& b){
                return a.position_sec < b.position_sec; });
        if (ctx_.on_cues_changed) ctx_.on_cues_changed();
    }

    void runAnalysis()
    {
        cancel_analysis_token_ = std::make_shared<std::atomic<bool>>(false);
        auto cancel = cancel_analysis_token_;
        auto path = ctx_.file_path;
        std::thread([this, cancel, path]() {
            auto info = analyze_fn_(path);
            if (cancel->load()) return;

            // Full structure analysis for cue point auto-detection
            namespace an = autoremix::dsp::analysis;
            an::SongStructure song_struct;
            {
                std::unique_ptr<juce::AudioFormatReader> reader(
                    format_manager_.createReaderFor(juce::File(path)));
                if (reader && reader->lengthInSamples > 0) {
                    const int    n  = static_cast<int>(reader->lengthInSamples);
                    const double sr = reader->sampleRate;
                    const int    ch = static_cast<int>(reader->numChannels);
                    juce::AudioBuffer<float> buf(std::max(ch, 2), n);
                    buf.clear();
                    reader->read(&buf, 0, n, 0, true, true);
                    juce::AudioBuffer<float> mono(1, n);
                    mono.clear();
                    const float gain = 1.0f / static_cast<float>(buf.getNumChannels());
                    for (int c = 0; c < buf.getNumChannels(); ++c)
                        mono.addFrom(0, 0, buf, c, 0, n, gain);
                    if (!cancel->load())
                        song_struct = an::analyze_structure(mono, sr);
                }
            }
            if (cancel->load()) return;

            float        bpm = info.valid() ? info.bpm  : 120.0f;
            juce::String key = info.valid() ? juce::String(info.key) : "";
            juce::MessageManager::callAsync(
                [this, cancel, bpm, key, ss = std::move(song_struct)]() mutable {
                    if (cancel->load()) return;
                    ctx_.detected_bpm   = bpm;
                    ctx_.detected_key   = key;
                    ctx_.song_structure = std::move(ss);
                    loadAndMergeCues();
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
