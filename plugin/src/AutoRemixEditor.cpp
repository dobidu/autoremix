#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScreenContext.h"
#include "ScreenBase.h"
#include "ScreenEmpty.h"
#include "ScreenSeparating.h"
#include "ScreenStemsReady.h"
#include "ScreenModeParams.h"
#include "ScreenRender.h"
#include <thread>
#include <filesystem>
#include <algorithm>

namespace {
static const juce::StringArray kAudioExts {
    ".wav", ".aif", ".aiff", ".mp3", ".flac", ".ogg", ".m4a"
};
} // namespace

//==============================================================================
AutoRemixAudioProcessorEditor::AutoRemixAudioProcessorEditor(AutoRemixAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&laf_);
    juce::Component::setSize(960, 600);
    drawAndConfigComponents();

    ctx_.navigate = [this](ScreenId id) { navigateTo(id); };
    ctx_.set_status = [this](const juce::String& msg) {
        juce::MessageManager::callAsync([this, msg] {
            status_lbl.setText(msg, juce::dontSendNotification);
        });
    };
    navigateTo(ScreenId::Empty, false);

    std::thread([this]() {
        auto& bridge = audioProcessor.getBridge();
        auto presets = bridge.getPresets();
        auto seps    = bridge.getAvailableSeparators();
        if (presets.empty() && seps.empty()) return;
        juce::MessageManager::callAsync([this, presets = std::move(presets),
                                               seps    = std::move(seps)]() mutable {
            if (!presets.empty()) {
                presets_      = std::move(presets);
                ctx_.presets  = presets_;
                style_combo_.clear(juce::dontSendNotification);
                for (int i = 0; i < (int)presets_.size(); ++i)
                    style_combo_.addItem(presets_[(size_t)i].name, i + 1);
                style_combo_.setSelectedItemIndex(0, juce::dontSendNotification);
                loadEngineDefaults(0);
            }
            if (!seps.empty()) {
                separators_      = std::move(seps);
                ctx_.separators  = separators_;
                separator_combo_.clear(juce::dontSendNotification);
                for (int i = 0; i < (int)separators_.size(); ++i)
                    separator_combo_.addItem(separators_[(size_t)i].display_name, i + 1);
                separator_combo_.setSelectedItemIndex(0, juce::dontSendNotification);
            }
        });
    }).detach();
}

AutoRemixAudioProcessorEditor::~AutoRemixAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

//==============================================================================
void AutoRemixAudioProcessorEditor::loadFile()
{
#if defined(_WIN32) || defined(__APPLE__)
    constexpr bool useNativeDialog = true;
#else
    constexpr bool useNativeDialog = false;
#endif
    chooser_ = std::make_unique<juce::FileChooser>(
        "Load audio file...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.wav;*.aif;*.aiff;*.mp3;*.flac",
        useNativeDialog);

    chooser_->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            auto results = fc.getResults();
            if (!results.isEmpty()) {
                auto f = results[0];
                ctx_.file_path = f.getFullPathName();
                if (auto* se = dynamic_cast<ScreenEmpty*>(screen_.get()))
                    se->fileWasSelected(f);
            }
        });
}

bool AutoRemixAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    if (files.isEmpty()) return false;
    return kAudioExts.contains(juce::File(files[0]).getFileExtension().toLowerCase());
}

void AutoRemixAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    if (files.isEmpty()) return;
    juce::File f(files[0]);
    if (!f.existsAsFile()) return;
    if (!kAudioExts.contains(f.getFileExtension().toLowerCase())) return;

    ctx_.file_path = f.getFullPathName();
    if (auto* se = dynamic_cast<ScreenEmpty*>(screen_.get()))
        se->fileWasSelected(f);
}

void AutoRemixAudioProcessorEditor::drawAndConfigComponents()
{
    // ── Header: title
    addAndMakeVisible(title_lbl);
    title_lbl.setText("AutoRemix", juce::dontSendNotification);
    title_lbl.setFont(AR::font(AR::FontRole::header));
    title_lbl.setColour(juce::Label::textColourId, juce::Colour(AR::FG));
    title_lbl.setBounds(16, 4, 150, 40);

    // ── Header: remix style combobox
    addAndMakeVisible(style_combo_);
    style_combo_.setBounds(176, 10, 200, 28);
    style_combo_.addItem("Chop & Screw",    1);
    style_combo_.addItem("Slowed + Reverb", 2);
    style_combo_.addItem("Drum & Bass",     3);
    style_combo_.setSelectedItemIndex(0, juce::dontSendNotification);
    style_combo_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(AR::ELEVATED));
    style_combo_.setColour(juce::ComboBox::outlineColourId,    juce::Colour(AR::SURFACE));
    style_combo_.setColour(juce::ComboBox::arrowColourId,      juce::Colour(AR::ACCENT));
    style_combo_.onChange = [this] { loadEngineDefaults(style_combo_.getSelectedItemIndex()); };

    // ── Header: separator combobox
    addAndMakeVisible(separator_combo_);
    separator_combo_.setBounds(384, 10, 140, 28);
    separator_combo_.addItem("Algorithmic FFT", 1);
    separator_combo_.setSelectedItemIndex(0, juce::dontSendNotification);
    separator_combo_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(AR::ELEVATED));
    separator_combo_.setColour(juce::ComboBox::outlineColourId,    juce::Colour(AR::SURFACE));
    separator_combo_.setColour(juce::ComboBox::arrowColourId,      juce::Colour(AR::FG));

    // ── Header: load file button
    addAndMakeVisible(loadfile_btn);
    loadfile_btn.setButtonText("Load");
    loadfile_btn.setComponentID("ghost");
    loadfile_btn.onClick = [this] { loadFile(); };
    loadfile_btn.setBounds(532, 10, 60, 28);

    // ── Header: save preset button
    addAndMakeVisible(save_preset_btn);
    save_preset_btn.setButtonText("Save Preset");
    save_preset_btn.setComponentID("ghost");
    save_preset_btn.onClick = [this] { onClick_SavePreset(); };
    save_preset_btn.setBounds(600, 10, 96, 28);

    // ── Header: save output button
    addAndMakeVisible(save_btn);
    save_btn.setButtonText("Save");
    save_btn.setComponentID("ghost");
    save_btn.setEnabled(false);
    save_btn.onClick = [this] { onClick_Save(); };
    save_btn.setBounds(704, 10, 60, 28);

    // ── Header: sidecar health dot
    addAndMakeVisible(health_dot_);
    health_dot_.setBounds(getWidth() - 24, 20, 8, 8);

    // ── Footer: status label
    addAndMakeVisible(status_lbl);
    status_lbl.setText("Ready", juce::dontSendNotification);
    status_lbl.setFont(AR::font(AR::FontRole::status));
    status_lbl.setJustificationType(juce::Justification::centredLeft);
    status_lbl.setColour(juce::Label::textColourId, juce::Colour(AR::FG));
    status_lbl.setBounds(16, getHeight() - 28, getWidth() - 32, 24);
}

//==============================================================================
void AutoRemixAudioProcessorEditor::loadEngineDefaults(int idx)
{
    if (presets_.empty() || idx < 0 || (size_t)idx >= presets_.size()) return;
    ctx_.selected_preset_idx = idx;
    auto& p = presets_[(size_t)idx].default_params;
    double source_bpm = (ctx_.detected_bpm > 0.0f) ? (double)ctx_.detected_bpm : 120.0;
    double target_bpm = std::clamp(p.tempo_factor * source_bpm, 40.0, 200.0);
    ctx_.target_bpm  = (float)target_bpm;
    ctx_.pitch_semi  = p.pitch_shift_semi;
    ctx_.reverb_mix  = p.reverb_mix;
    double beat_ms   = 60000.0 / source_bpm;
    double beats     = std::clamp(p.chop_interval_ms / beat_ms, 0.25, 16.0);
    ctx_.chop_beats  = (float)beats;
}

void AutoRemixAudioProcessorEditor::onClick_Play()
{
    if (ctx_.file_path.isEmpty()) {
        status_lbl.setText("No file loaded.", juce::dontSendNotification);
        return;
    }
    navigateTo(ScreenId::Separating);
}

void AutoRemixAudioProcessorEditor::onClick_Save()
{
    if (ctx_.output_path.isEmpty()) return;

    chooser_ = std::make_unique<juce::FileChooser>(
        "Save remixed file...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory)
            .getChildFile(juce::File(ctx_.output_path).getFileName()),
        "*.wav",
        false);

    chooser_->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            auto results = fc.getResults();
            if (!results.isEmpty()) {
                juce::File(ctx_.output_path).copyFileTo(results[0]);
                status_lbl.setText("Saved: " + results[0].getFileName(), juce::dontSendNotification);
            }
        });
}

void AutoRemixAudioProcessorEditor::onClick_SavePreset()
{
    juce::AlertWindow aw("Save Preset",
                         "Enter a name for this preset:",
                         juce::MessageBoxIconType::NoIcon);
    aw.addTextEditor("name", "", "Preset name:");
    aw.addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
    aw.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (aw.runModalLoop() != 1) return;

    auto name = aw.getTextEditorContents("name").trim();
    if (name.isEmpty()) return;

    auto idx = static_cast<std::size_t>(style_combo_.getSelectedItemIndex());
    if (presets_.empty() || idx >= presets_.size()) return;
    auto& preset = presets_[idx];

    float src_bpm      = (ctx_.detected_bpm > 0.0f) ? ctx_.detected_bpm : 120.0f;
    float tempo_factor = ctx_.target_bpm / src_bpm;
    float chop_ms      = ctx_.chop_beats * (60000.0f / src_bpm);

    autoremix::RemixParams params;
    params.tempo_factor     = tempo_factor;
    params.pitch_shift_semi = ctx_.pitch_semi;
    params.reverb_mix       = ctx_.reverb_mix;
    params.chop_interval_ms = chop_ms;
    params.engine_id        = preset.id;
    params.separator_id     = "";
    params.vocals_gain      = ctx_.vocals_gain;
    params.drums_gain       = ctx_.drums_gain;
    params.bass_gain        = ctx_.bass_gain;
    params.other_gain       = ctx_.other_gain;

    status_lbl.setText("Saving preset...", juce::dontSendNotification);

    std::string name_str = name.toStdString();
    std::thread([this, name_str, params]() mutable {
        auto& bridge    = audioProcessor.getBridge();
        bool ok         = bridge.savePreset(name_str, params);
        auto newPresets = bridge.getPresets();

        juce::MessageManager::callAsync([this, ok, name_str,
                                               newPresets = std::move(newPresets)]() mutable {
            if (ok && !newPresets.empty()) {
                presets_      = std::move(newPresets);
                ctx_.presets  = presets_;
                style_combo_.clear(juce::dontSendNotification);
                for (int i = 0; i < (int)presets_.size(); ++i)
                    style_combo_.addItem(presets_[(size_t)i].name, i + 1);
                for (int i = 0; i < (int)presets_.size(); ++i) {
                    if (presets_[(size_t)i].name == name_str) {
                        style_combo_.setSelectedItemIndex(i, juce::dontSendNotification);
                        loadEngineDefaults(i);
                        break;
                    }
                }
                status_lbl.setText("Preset saved: " + juce::String(name_str),
                                   juce::dontSendNotification);
            } else {
                status_lbl.setText("Error: preset save failed.",
                                   juce::dontSendNotification);
            }
        });
    }).detach();
}

//==============================================================================
void AutoRemixAudioProcessorEditor::navigateTo(ScreenId id, bool animate)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (screen_) {
        screen_->onExit();
        removeChildComponent(screen_.get());
        screen_.reset();
    }
    installScreen(id, animate);
}

void AutoRemixAudioProcessorEditor::installScreen(ScreenId id, bool animate)
{
    std::unique_ptr<ScreenBase> newScreen;
    switch (id) {
        case ScreenId::Empty: {
            auto* s = new ScreenEmpty(
                ctx_,
                [this](const juce::String& path) {
                    return audioProcessor.getBridge().analyzeFile(path);
                },
                [this](const std::filesystem::path& in,
                       const std::filesystem::path& out,
                       const std::string& sep_id) {
                    return audioProcessor.getBridge().separateStems(in, out, sep_id);
                },
                [this] { loadFile(); }
            );
            newScreen.reset(s);
            break;
        }
        case ScreenId::Separating: {
            auto* s = new ScreenSeparating(
                ctx_,
                [this](const std::filesystem::path& in,
                       const std::filesystem::path& out,
                       const std::string& sep_id) {
                    return audioProcessor.getBridge().separateStems(in, out, sep_id);
                }
            );
            newScreen.reset(s);
            break;
        }
        case ScreenId::StemsReady:
            newScreen = std::make_unique<ScreenStemsReady>(ctx_);
            break;

        case ScreenId::ModeParams:
            newScreen = std::make_unique<ScreenModeParams>(ctx_);
            break;

        case ScreenId::Render: {
            auto* s = new ScreenRender(
                ctx_,
                [this](const autoremix::StemPaths&   stems,
                       const autoremix::RemixParams& params,
                       const std::filesystem::path&  out) {
                    return audioProcessor.getBridge().applyRemix(stems, params, out);
                }
            );
            newScreen.reset(s);
            break;
        }
        default:
            jassertfalse;
            return;
    }

    // content area: 960 wide, below 48px header, above 32px footer
    auto contentBounds = juce::Rectangle<int>(0, 48, 960, 520);
    newScreen->setBounds(contentBounds);
    screen_ = std::move(newScreen);
    addAndMakeVisible(*screen_);

    if (animate) {
        screen_->setAlpha(0.0f);
        screen_animator_.animateComponent(screen_.get(), contentBounds, 1.0f, 150, false, 1.0, 1.0);
    } else {
        screen_->setAlpha(1.0f);
    }
    screen_->onEnter();
}

//==============================================================================
void AutoRemixAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(AR::BG));

    // header zone
    g.setColour(juce::Colour(AR::ELEVATED));
    g.fillRect(0, 0, getWidth(), 48);

    // header separator
    g.setColour(juce::Colour(AR::SURFACE));
    g.fillRect(0, 47, getWidth(), 1);

    // footer zone
    g.setColour(juce::Colour(AR::ELEVATED));
    g.fillRect(0, getHeight() - 32, getWidth(), 32);

    // footer separator
    g.setColour(juce::Colour(AR::SURFACE));
    g.fillRect(0, getHeight() - 33, getWidth(), 1);
}

void AutoRemixAudioProcessorEditor::resized() {}
