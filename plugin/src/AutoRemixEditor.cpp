#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScreenContext.h"
#include "ScreenBase.h"
#include "ScreenEmpty.h"
#include "ScreenSeparating.h"
#include "ScreenStemsReady.h"
#include "ScreenModeParams.h"
#include "ScreenRender.h"
#include "ScreenMashup.h"
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

    ctx_.play_preview = [this](const juce::File& f) {
        audioProcessor.stopPreview();
        audioProcessor.loadPreviewFile(f);
        audioProcessor.togglePreview();
    };
    ctx_.stop_preview        = [this] { audioProcessor.stopPreview(); };
    ctx_.is_preview_playing  = [this] { return audioProcessor.isPreviewPlaying(); };
    ctx_.play_stem           = [this](int idx, const juce::File& f) { audioProcessor.playStem(idx, f); };
    ctx_.stop_stem           = [this](int idx) { audioProcessor.stopStem(idx); };
    ctx_.is_stem_playing     = [this](int idx) { return audioProcessor.isStemPlaying(idx); };
    ctx_.stop_all_stems      = [this] { audioProcessor.stopAllStems(); };
    ctx_.get_preview_position = [this] { return audioProcessor.getPreviewPosition(); };
    ctx_.get_stem_position    = [this](int idx) { return audioProcessor.getStemPosition(idx); };

    ctx_.run_mashup = [this](autoremix::MashupParams params,
                             std::function<void(autoremix::MashupResult)> on_complete) {
        std::thread([this, params = std::move(params),
                     on_complete = std::move(on_complete)]() mutable {
            auto result = audioProcessor.getBridge().mashup(params);
            juce::MessageManager::callAsync(
                [on_complete = std::move(on_complete),
                 result = std::move(result)]() mutable {
                    on_complete(std::move(result));
                });
        }).detach();
    };

    ctx_.start_mashup_flow = [this] { startMashupFlow(); };

    navigateTo(ScreenId::Empty, false);

    std::thread([this]() {
        auto& bridge       = audioProcessor.getBridge();
        auto presets       = bridge.getPresets();
        auto seps          = bridge.getAvailableSeparators();
        auto mashup_presets = bridge.getMashupPresets();
        if (presets.empty() && seps.empty() && mashup_presets.empty()) return;
        juce::MessageManager::callAsync([this, presets       = std::move(presets),
                                               seps          = std::move(seps),
                                               mashup_presets = std::move(mashup_presets)]() mutable {
            if (!presets.empty()) {
                presets_     = std::move(presets);
                ctx_.presets = presets_;
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
            if (!mashup_presets.empty())
                ctx_.mashup_presets = std::move(mashup_presets);
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

void AutoRemixAudioProcessorEditor::startMashupFlow()
{
#if defined(_WIN32) || defined(__APPLE__)
    constexpr bool useNativeDialog = true;
#else
    constexpr bool useNativeDialog = false;
#endif
    chooser_ = std::make_unique<juce::FileChooser>(
        "Load track B for mashup...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg;*.m4a",
        useNativeDialog);

    chooser_->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            auto results = fc.getResults();
            if (results.isEmpty()) return;
            auto f = results[0];
            if (!f.existsAsFile()) return;

            const juce::String b_path_str = f.getFullPathName();
            ctx_.file_path_b = b_path_str;
            if (ctx_.set_status) ctx_.set_status("Track B: analyzing...");

            std::thread([this, b_path_str]() {
                auto& bridge = audioProcessor.getBridge();
                auto fa = bridge.analyzeFile(b_path_str);

                juce::MessageManager::callAsync([this, fa] {
                    if (fa.bpm > 0.0f) ctx_.detected_bpm_b = fa.bpm;
                    ctx_.detected_key_b = juce::String(fa.key);
                    ctx_.mashup_mode_separating = true;
                    if (ctx_.navigate) ctx_.navigate(ScreenId::Separating);
                });
            }).detach();
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

    // ── Header: separator combobox (global pre-flow setting)
    addAndMakeVisible(separator_combo_);
    separator_combo_.setBounds(176, 10, 160, 28);
    separator_combo_.addItem("Algorithmic FFT", 1);
    separator_combo_.setSelectedItemIndex(0, juce::dontSendNotification);
    separator_combo_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(AR::ELEVATED));
    separator_combo_.setColour(juce::ComboBox::outlineColourId,    juce::Colour(AR::SURFACE));
    separator_combo_.setColour(juce::ComboBox::arrowColourId,      juce::Colour(AR::FG));

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

        case ScreenId::ModeParams: {
            auto* s = new ScreenModeParams(
                ctx_,
                [this](const juce::String& name,
                       const autoremix::RemixParams& params,
                       std::function<void(bool)> on_complete) {
                    std::thread([this, name, params,
                                 on_complete = std::move(on_complete)]() mutable {
                        auto& bridge    = audioProcessor.getBridge();
                        bool ok         = bridge.savePreset(name.toStdString(), params);
                        auto newPresets = bridge.getPresets();
                        juce::MessageManager::callAsync(
                            [this, ok, newPresets = std::move(newPresets),
                             on_complete = std::move(on_complete)]() mutable {
                                if (ok && !newPresets.empty()) {
                                    presets_     = newPresets;
                                    ctx_.presets = presets_;
                                }
                                on_complete(ok);
                            });
                    }).detach();
                }
            );
            newScreen.reset(s);
            break;
        }
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
        case ScreenId::Mashup: {
            newScreen = std::make_unique<ScreenMashup>(ctx_);
            break;
        }
        default:
            jassertfalse;
            return;
    }

    // content area: below 48px header, above 32px footer
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

    g.setColour(juce::Colour(AR::ELEVATED));
    g.fillRect(0, 0, getWidth(), 48);
    g.setColour(juce::Colour(AR::SURFACE));
    g.fillRect(0, 47, getWidth(), 1);

    g.setColour(juce::Colour(AR::ELEVATED));
    g.fillRect(0, getHeight() - 32, getWidth(), 32);
    g.setColour(juce::Colour(AR::SURFACE));
    g.fillRect(0, getHeight() - 33, getWidth(), 1);
}

void AutoRemixAudioProcessorEditor::resized() {}
