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
#include "dsp/NativeAnalysis.h"
#include "dsp/NativeAlgorithmicSeparator.h"
#include "dsp/NativeDemucsSeparator.h"
#include "dsp/NativeRemixEngines.h"
#include "dsp/NativeEffectChainEngine.h"
#include "dsp/NativeStructuredRemixEngine.h"
#include "dsp/NativeMashupEngine.h"
#include "dsp/NativePresetTypes.h"
#include "dsp/NativePresetLoaders.h"
#include "dsp/ModelDownloader.h"
#include "jobs/JobRunner.h"
#include "jobs/NativeJobs.h"
#include <filesystem>
#include <algorithm>
#include <unordered_map>

namespace {
static const juce::StringArray kAudioExts {
    ".wav", ".aif", ".aiff", ".mp3", ".flac", ".ogg", ".m4a"
};

// ── Native ↔ editor type adapters ────────────────────────────────────────────
using NRP = autoremix::dsp::presets::NativeRemixPreset;
using NMP = autoremix::dsp::presets::NativeMashupPreset;

static autoremix::PresetInfo
to_preset_info(const NRP& n)
{
    autoremix::PresetInfo p;
    p.id   = n.id;
    p.name = n.name;
    p.default_params.tempo_factor       = n.params.tempo_factor;
    p.default_params.pitch_shift_semi   = n.params.pitch_shift_semi;
    p.default_params.reverb_mix         = n.params.reverb_mix;
    p.default_params.chop_interval_ms   = n.params.chop_interval_ms;
    p.default_params.bass_boost_db      = n.params.bass_boost_db;
    p.default_params.drums_tempo_factor = n.params.drums_tempo_factor;
    p.default_params.engine_id          = n.engine;
    p.default_params.separator_id       = "algorithmic";
    p.default_params.vocals_gain        = n.stem_mix.vocals;
    p.default_params.drums_gain         = n.stem_mix.drums;
    p.default_params.bass_gain          = n.stem_mix.bass;
    p.default_params.other_gain         = n.stem_mix.other;
    return p;
}

static autoremix::MashupPresetInfo
to_mashup_info(const NMP& n)
{
    autoremix::MashupPresetInfo m;
    m.id                          = n.id;
    m.name                        = n.name;
    m.description                 = n.description;
    m.stem_gains_a                = n.stem_gains_a;
    m.stem_gains_b                = n.stem_gains_b;
    m.target_bpm_mode             = n.target_bpm_mode;
    m.target_bpm_absolute         = static_cast<float>(n.target_bpm_absolute);
    m.target_key_mode             = n.target_key_mode;
    m.target_key_absolute         = n.target_key_absolute;
    m.bpm_modifier                = n.bpm_modifier;
    m.master_pitch_offset_semi    = n.master_pitch_offset_semi;
    m.master_reverb_mix           = n.master_reverb_mix;
    m.master_reverb_room          = n.master_reverb_room;
    m.highpass_b_hz               = n.highpass_b_hz;
    return m;
}

} // namespace

//==============================================================================
AutoRemixAudioProcessorEditor::AutoRemixAudioProcessorEditor(AutoRemixAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&laf_);
    juce::Component::setSize(960, 600);
    drawAndConfigComponents();

    ctx_.jobs     = &audioProcessor.getJobRunner();
    ctx_.navigate = [this](ScreenId id) { navigateTo(id); };
    // Callable from any thread; the post only runs while this editor is alive (R-01).
    ctx_.set_status = [this, alive = autoremix::jobs::Receiver(alive_)](const juce::String& msg) {
        autoremix::jobs::post_guarded(alive, [this, msg] {
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
    ctx_.seek_stem            = [this](int idx, double ratio) { audioProcessor.seekStem(idx, ratio); };
    ctx_.seek_preview         = [this](double ratio) { audioProcessor.seekPreview(ratio); };

    ctx_.run_mashup = [this](autoremix::MashupParams params,
                             std::function<void(autoremix::MashupResult)> on_complete) {
        // Snapshot inputs on the message thread (R-38); the job owns copies only.
        auto work = [fmt = &audioProcessor.getFormatManager(), a = ctx_.stems, b = ctx_.stems_b,
                     params = std::move(params)](const autoremix::jobs::CancelToken& token) {
            return autoremix::jobs::run_mashup(*fmt, a, b, params, token);
        };
        auto done = [on_complete = std::move(on_complete)](autoremix::MashupResult r, bool cancelled) {
            if (cancelled) r.error_message = "cancelled";
            if (on_complete) on_complete(std::move(r));   // caller guards its own lifetime
        };
        trackJob(ctx_.jobs->submit<autoremix::MashupResult>(alive_, std::move(work), std::move(done)));
    };

    ctx_.start_mashup_flow = [this] { startMashupFlow(); };

    // ── Native registries (synchronous; presets are embedded BinaryData) ─────
    {
        auto native_remix  = autoremix::dsp::presets::load_remix_presets();
        auto native_mashup = autoremix::dsp::presets::load_mashup_presets();

        native_remix_presets_ = native_remix;     // keep for engine lookup

        presets_.clear();
        presets_.reserve(native_remix.size());
        for (auto& nr : native_remix) presets_.push_back(to_preset_info(nr));
        ctx_.presets = presets_;

        ctx_.mashup_presets.clear();
        ctx_.mashup_presets.reserve(native_mashup.size());
        for (auto& nm : native_mashup) ctx_.mashup_presets.push_back(to_mashup_info(nm));

        separators_ = {
            { "algorithmic", "Algorithmic FFT" },
            { "demucs",      "Demucs (ML)"     },
        };
        ctx_.separators = separators_;

        separator_combo_.clear(juce::dontSendNotification);
        for (int i = 0; i < (int)separators_.size(); ++i)
            separator_combo_.addItem(separators_[(size_t)i].display_name, i + 1);
        separator_combo_.setSelectedItemIndex(0, juce::dontSendNotification);

        if (!presets_.empty()) loadEngineDefaults(0);
    }

    navigateTo(ScreenId::Empty, false);
}

AutoRemixAudioProcessorEditor::~AutoRemixAudioProcessorEditor()
{
    // The host may close the window mid-job: stop what this editor started.
    // Completions are already guarded by alive_, which dies with the editor.
    // Screens cancel their own jobs in their destructors.
    for (auto& t : jobs_started_) if (t) t->store(true);
    setLookAndFeel(nullptr);
}

void AutoRemixAudioProcessorEditor::trackJob(autoremix::jobs::CancelToken t)
{
    jobs_started_.erase(std::remove_if(jobs_started_.begin(), jobs_started_.end(),
                                       [](const autoremix::jobs::CancelToken& x) { return x.use_count() <= 1; }),
                        jobs_started_.end());
    jobs_started_.push_back(std::move(t));
}

autoremix::jobs::JobEvents AutoRemixAudioProcessorEditor::makeJobEvents()
{
    using autoremix::jobs::ModelEvent;
    autoremix::jobs::Receiver alive = alive_;
    autoremix::jobs::JobEvents ev;
    ev.status = [this, alive](const juce::String& msg) {
        autoremix::jobs::post_guarded(alive, [this, msg] {
            status_lbl.setText(msg, juce::dontSendNotification);
        });
    };
    ev.model = [this, alive](ModelEvent e) {
        autoremix::jobs::post_guarded(alive, [this, e] {
            switch (e) {
                case ModelEvent::checking:    health_dot_.setState(ModelStatusDot::State::checking);    break;
                case ModelEvent::downloading: health_dot_.setState(ModelStatusDot::State::downloading); break;
                case ModelEvent::cached:      health_dot_.setState(ModelStatusDot::State::cached);      break;
                case ModelEvent::error:       health_dot_.setState(ModelStatusDot::State::error);       break;
                case ModelEvent::gpu_active:  health_dot_.setState(ModelStatusDot::State::gpu_active);  break;
            }
        });
    };
    return ev;
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

            auto work = [fmt = &audioProcessor.getFormatManager(), b_path_str]
                        (const autoremix::jobs::CancelToken&) {
                return autoremix::jobs::analyze_file(*fmt, b_path_str);
            };
            auto done = [this](autoremix::FileAnalysis fa, bool cancelled) {   // guarded by alive_
                if (cancelled) return;
                if (fa.bpm > 0.0f) ctx_.detected_bpm_b = fa.bpm;
                ctx_.detected_key_b = juce::String(fa.key);
                ctx_.mashup_mode_separating = true;
                if (ctx_.navigate) ctx_.navigate(ScreenId::Separating);
            };
            trackJob(ctx_.jobs->submit<autoremix::FileAnalysis>(alive_, std::move(work), std::move(done)));
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
    separator_combo_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(AR::ELEVATED));
    separator_combo_.setColour(juce::ComboBox::outlineColourId,    juce::Colour(AR::SURFACE));
    separator_combo_.setColour(juce::ComboBox::arrowColourId,      juce::Colour(AR::FG));
    separator_combo_.setTooltip("Stem separation engine");
    separator_combo_.onChange = [this] {
        ctx_.selected_separator_idx = separator_combo_.getSelectedItemIndex();
    };

    // ── Header: model status label + dot ─────────────────────────────────────
    addAndMakeVisible(sidecar_lbl_);
    sidecar_lbl_.setText("MODEL", juce::dontSendNotification);
    sidecar_lbl_.setFont(AR::font(AR::FontRole::section_label));
    sidecar_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));
    sidecar_lbl_.setJustificationType(juce::Justification::centredRight);
    sidecar_lbl_.setBounds(getWidth() - 92, 12, 60, 24);
    sidecar_lbl_.setTooltip("Demucs ONNX model: green = cached, "
                            "amber = downloading, red = missing");

    addAndMakeVisible(health_dot_);
    health_dot_.setBounds(getWidth() - 24, 20, 8, 8);
    health_dot_.setTooltip("Demucs ONNX model: green = cached, "
                           "amber = downloading, red = missing");

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
                [fmt = &audioProcessor.getFormatManager()](const juce::String& path) {
                    return autoremix::jobs::analyze_file(*fmt, path);
                },
                [this] { loadFile(); }
            );
            newScreen.reset(s);
            break;
        }
        case ScreenId::Separating: {
            auto* s = new ScreenSeparating(
                ctx_,
                [fmt = &audioProcessor.getFormatManager(), ev = makeJobEvents()]
                (const std::filesystem::path& in, const std::filesystem::path& out,
                 const std::string& sep_id, const autoremix::jobs::CancelToken& token) {
                    return autoremix::jobs::separate(*fmt, in, out, sep_id, token, ev);
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
                [](const juce::String& /*name*/,
                   const autoremix::RemixParams& /*params*/,
                   std::function<void(bool)> on_complete) {
                    // Phase 27-01: savePreset is a no-op. Custom-preset
                    // authoring stays out of scope; native loaders only
                    // read. Revisit in v4.1 if users ask for it.
                    if (on_complete) on_complete(false);
                }
            );
            newScreen.reset(s);
            break;
        }
        case ScreenId::Render: {
            auto* s = new ScreenRender(
                ctx_,
                [fmt = &audioProcessor.getFormatManager(), presets = native_remix_presets_]
                (const autoremix::StemPaths& stems, const autoremix::RemixParams& params,
                 const std::filesystem::path& out, const autoremix::jobs::CancelToken& token) {
                    return autoremix::jobs::render_remix(*fmt, presets, stems, params, out, token);
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

