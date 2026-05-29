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
#include "dsp/NativeMashupEngine.h"
#include "dsp/NativePresetTypes.h"
#include "dsp/NativePresetLoaders.h"
#include "dsp/ModelDownloader.h"
#include <thread>
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

// ── WAV I/O helpers ─────────────────────────────────────────────────────────

// Load a WAV (or any registered format) into a stereo float32 buffer at the
// file's native sample rate. Returns empty buffer + sr=0 on failure.
static std::pair<juce::AudioBuffer<float>, double>
load_audio(juce::AudioFormatManager& fmt, const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> reader(fmt.createReaderFor(f));
    if (!reader) return { juce::AudioBuffer<float>{}, 0.0 };

    const int    chans = static_cast<int>(reader->numChannels);
    const int    n     = static_cast<int>(reader->lengthInSamples);
    const double sr    = reader->sampleRate;

    juce::AudioBuffer<float> buf(std::max(chans, 2), n);
    buf.clear();
    reader->read(&buf, 0, n, 0, true, true);
    if (chans == 1) buf.copyFrom(1, 0, buf, 0, 0, n);  // mono → duplicate
    buf.setSize(2, n, true, false, true);
    return { std::move(buf), sr };
}

static bool
write_wav(const juce::File& dest,
          const juce::AudioBuffer<float>& buf,
          double sr)
{
    dest.deleteFile();
    auto os = std::unique_ptr<juce::FileOutputStream>(dest.createOutputStream());
    if (!os) return false;
    juce::WavAudioFormat wav;
    auto writer = std::unique_ptr<juce::AudioFormatWriter>(
        wav.createWriterFor(os.get(), sr,
                            static_cast<unsigned>(buf.getNumChannels()),
                            24, {}, 0));
    if (!writer) return false;
    os.release();   // writer owns now
    return writer->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
}

// Write the 4 native stems to <dir>/{vocals,drums,bass,other}.wav.
static autoremix::StemPaths
write_native_stems(const autoremix::dsp::separators::NativeStems& s,
                   const std::filesystem::path& dir,
                   double sr)
{
    autoremix::StemPaths out;
    std::filesystem::create_directories(dir);
    const auto vp = dir / "vocals.wav";
    const auto dp = dir / "drums.wav";
    const auto bp = dir / "bass.wav";
    const auto op = dir / "other.wav";
    const bool ok =
        write_wav(juce::File(vp.string()), s.vocals, sr) &&
        write_wav(juce::File(dp.string()), s.drums,  sr) &&
        write_wav(juce::File(bp.string()), s.bass,   sr) &&
        write_wav(juce::File(op.string()), s.other,  sr);
    if (!ok) return out;
    out.vocals = vp;
    out.drums  = dp;
    out.bass   = bp;
    out.other  = op;
    out.valid  = true;
    return out;
}

// Load 4 stem WAVs into a NativeStems struct (post-separation reload for
// the remix path).
static autoremix::dsp::separators::NativeStems
load_native_stems(juce::AudioFormatManager& fmt,
                  const autoremix::StemPaths& paths,
                  double& sr_out)
{
    autoremix::dsp::separators::NativeStems s;
    auto [v, sr_v] = load_audio(fmt, juce::File(paths.vocals.string()));
    auto [d, sr_d] = load_audio(fmt, juce::File(paths.drums.string()));
    auto [b, sr_b] = load_audio(fmt, juce::File(paths.bass.string()));
    auto [o, sr_o] = load_audio(fmt, juce::File(paths.other.string()));
    s.vocals = std::move(v);
    s.drums  = std::move(d);
    s.bass   = std::move(b);
    s.other  = std::move(o);
    sr_out   = sr_v;  // all 4 written at the same sr
    (void) sr_d; (void) sr_b; (void) sr_o;
    return s;
}

// Convert a mono mixdown for analysis. Picks channel 0; sums if needed.
static juce::AudioBuffer<float>
to_mono(const juce::AudioBuffer<float>& in)
{
    juce::AudioBuffer<float> mono(1, in.getNumSamples());
    mono.clear();
    for (int ch = 0; ch < in.getNumChannels(); ++ch)
        mono.addFrom(0, 0, in, ch, 0, in.getNumSamples(),
                     1.0f / static_cast<float>(in.getNumChannels()));
    return mono;
}

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
            auto result = runMashupNative(params);
            juce::MessageManager::callAsync(
                [on_complete = std::move(on_complete),
                 result = std::move(result)]() mutable {
                    on_complete(std::move(result));
                });
        }).detach();
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
                auto fa = analyzeFileNative(b_path_str);

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
                [this](const juce::String& path) {
                    return analyzeFileNative(path);
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
                    return separateNative(in, out, sep_id);
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
                [this](const autoremix::StemPaths&   stems,
                       const autoremix::RemixParams& params,
                       const std::filesystem::path&  out) {
                    return renderRemixNative(stems, params, out);
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

//==============================================================================
// Native runtime impls (Phase 27-01)
//==============================================================================

autoremix::FileAnalysis
AutoRemixAudioProcessorEditor::analyzeFileNative(const juce::String& path)
{
    autoremix::FileAnalysis fa;
    auto& fmt = audioProcessor.getFormatManager();
    auto [buf, sr] = load_audio(fmt, juce::File(path));
    if (buf.getNumSamples() == 0 || sr <= 0.0) return fa;

    auto mono = to_mono(buf);
    fa.bpm          = static_cast<float>(autoremix::dsp::analysis::detect_bpm(mono, sr));
    fa.key          = autoremix::dsp::analysis::detect_key(mono, sr);
    fa.duration_sec = static_cast<float>(buf.getNumSamples() / sr);
    return fa;
}

autoremix::StemPaths
AutoRemixAudioProcessorEditor::separateNative(const std::filesystem::path& in,
                                              const std::filesystem::path& out,
                                              const std::string& sep_id)
{
    autoremix::StemPaths empty;
    auto& fmt = audioProcessor.getFormatManager();

    // Load WAV into a stereo float buffer at native sr.
    auto [buf, sr] = load_audio(fmt, juce::File(in.string()));
    if (buf.getNumSamples() == 0 || sr <= 0.0) {
        if (ctx_.set_status)
            juce::MessageManager::callAsync([this] {
                ctx_.set_status("Could not read input file");
            });
        return empty;
    }

    autoremix::dsp::separators::NativeStems stems;

    if (sep_id == "demucs") {
        // 1. Ensure model is cached.
        if (ctx_.set_status)
            juce::MessageManager::callAsync([this] {
                ctx_.set_status("Demucs: checking model cache...");
            });
        health_dot_.setState(ModelStatusDot::State::downloading);

        auto progress_cb = [this](double frac) {
            const int pct = static_cast<int>(frac * 100.0);
            juce::MessageManager::callAsync([this, pct] {
                if (ctx_.set_status)
                    ctx_.set_status("Demucs: downloading model... "
                                    + juce::String(pct) + "%");
            });
        };
        auto download = autoremix::dsp::models::ensure_htdemucs(progress_cb);
        if (!download.ok) {
            health_dot_.setState(ModelStatusDot::State::error);
            juce::MessageManager::callAsync([this, err = download.error] {
                if (ctx_.set_status)
                    ctx_.set_status("Demucs model download failed: " + err);
            });
            return empty;
        }
        health_dot_.setState(ModelStatusDot::State::cached);

        // 2. Run separation (44.1 kHz required by the model).
        if (std::abs(sr - 44100.0) > 0.5) {
            juce::MessageManager::callAsync([this, sr] {
                if (ctx_.set_status)
                    ctx_.set_status("Demucs needs 44.1 kHz, got "
                                    + juce::String(sr) + " Hz");
            });
            return empty;
        }
        juce::MessageManager::callAsync([this] {
            if (ctx_.set_status) ctx_.set_status("Separating (demucs)...");
        });

        auto sep_progress = [this](double frac) {
            const int pct = static_cast<int>(frac * 100.0);
            juce::MessageManager::callAsync([this, pct] {
                if (ctx_.set_status)
                    ctx_.set_status("Demucs: separating... "
                                    + juce::String(pct) + "%");
            });
        };
        auto res = autoremix::dsp::separators::separate_demucs(
            buf, sr, download.path, sep_progress);
        if (!res.ok) {
            juce::MessageManager::callAsync([this, err = res.error] {
                if (ctx_.set_status)
                    ctx_.set_status("Demucs separation failed: " + err);
            });
            return empty;
        }
        if (res.gpu_used)
            health_dot_.setState(ModelStatusDot::State::gpu_active);
        stems = std::move(res.stems);
    }
    else {
        // Algorithmic FFT path.
        juce::MessageManager::callAsync([this] {
            if (ctx_.set_status)
                ctx_.set_status("Separating (algorithmic)...");
        });
        stems = autoremix::dsp::separators::separate_algorithmic(buf, sr);
    }

    return write_native_stems(stems, out, sr);
}

autoremix::ProcessResult
AutoRemixAudioProcessorEditor::renderRemixNative(const autoremix::StemPaths& stems,
                                                 const autoremix::RemixParams& params,
                                                 const std::filesystem::path& out_path)
{
    autoremix::ProcessResult result;
    auto& fmt = audioProcessor.getFormatManager();

    double sr = 0.0;
    auto native_stems = load_native_stems(fmt, stems, sr);
    if (sr <= 0.0) {
        result.error_message = "Could not read stems";
        return result;
    }

    // Look up the preset by engine_id (which carries the preset's id).
    // Fall back to the engine name in params.engine_id.
    const NRP* preset = nullptr;
    for (auto& p : native_remix_presets_) {
        if (p.id == params.engine_id) { preset = &p; break; }
    }

    namespace eng = autoremix::dsp::engines;
    eng::RemixParams ep;
    ep.tempo_factor       = params.tempo_factor;
    ep.pitch_shift_semi   = params.pitch_shift_semi;
    ep.reverb_mix         = params.reverb_mix;
    ep.chop_interval_ms   = params.chop_interval_ms;
    ep.bass_boost_db      = params.bass_boost_db;
    ep.drums_tempo_factor = params.drums_tempo_factor;

    // Apply per-stem gain pre-engine (matches v3 sidecar pre-weighting).
    auto apply_gain = [](juce::AudioBuffer<float>& b, float g) {
        if (g != 1.0f) b.applyGain(g);
    };
    apply_gain(native_stems.vocals, params.vocals_gain);
    apply_gain(native_stems.drums,  params.drums_gain);
    apply_gain(native_stems.bass,   params.bass_gain);
    apply_gain(native_stems.other,  params.other_gain);

    juce::AudioBuffer<float> out_buf;
    const std::string engine = preset ? preset->engine : params.engine_id;

    try {
        if      (engine == "chopped_screwed")
            out_buf = eng::chopped_and_screwed(native_stems, sr, ep);
        else if (engine == "slowed_reverb")
            out_buf = eng::slowed_reverb     (native_stems, sr, ep);
        else if (engine == "drum_and_bass")
            out_buf = eng::drum_and_bass     (native_stems, sr, ep);
        else if (engine == "effect_chain" && preset) {
            out_buf = eng::process_effect_chain(
                native_stems, preset->effects, preset->stem_mix, sr);
        }
        else {
            // Default fallback: equal-mix of stems (algorithmic remix).
            out_buf = eng::chopped_and_screwed(native_stems, sr, ep);
        }
    } catch (const std::exception& e) {
        result.error_message = std::string("engine exception: ") + e.what();
        return result;
    }

    if (!write_wav(juce::File(out_path.string()), out_buf, sr)) {
        result.error_message = "WAV write failed";
        return result;
    }
    result.success     = true;
    result.output_path = out_path;
    return result;
}

autoremix::MashupResult
AutoRemixAudioProcessorEditor::runMashupNative(const autoremix::MashupParams& params)
{
    autoremix::MashupResult result;

    // params.file_a / file_b are the ORIGINAL inputs. The mashup flow
    // already separates both upstream, and ctx_.stems / ctx_.stems_b
    // carry the resulting stem WAV paths. Re-load those instead of
    // re-separating from scratch.
    auto& fmt = audioProcessor.getFormatManager();
    double sr_a = 0.0, sr_b = 0.0;
    auto stems_a = load_native_stems(fmt, ctx_.stems,   sr_a);
    auto stems_b = load_native_stems(fmt, ctx_.stems_b, sr_b);
    if (sr_a <= 0.0 || sr_b <= 0.0) {
        result.error_message = "Mashup needs both tracks' stems";
        return result;
    }

    namespace eng = autoremix::dsp::engines;
    eng::MashupParams mp;
    mp.stem_gains_a              = params.stem_gains_a;
    mp.stem_gains_b              = params.stem_gains_b;
    mp.has_target_bpm            = params.has_target_bpm;
    mp.target_bpm                = params.target_bpm;
    mp.target_key                = params.target_key;
    mp.bpm_modifier              = params.bpm_modifier;
    mp.master_pitch_offset_semi  = params.master_pitch_offset_semi;
    mp.master_reverb_mix         = params.master_reverb_mix;
    mp.master_reverb_room        = params.master_reverb_room;
    mp.highpass_b_hz             = params.highpass_b_hz;

    try {
        auto mr = eng::mashup(stems_a, stems_b, sr_a, mp);
        if (mr.buffer.getNumSamples() == 0) {
            result.error_message = "Mashup produced empty buffer";
            return result;
        }
        // Write output WAV.
        const auto out_dir = juce::File::getSpecialLocation(
                                juce::File::tempDirectory)
                              .getChildFile("autoremix")
                              .getChildFile("mashup");
        const auto out_name =
            juce::File(juce::String(params.file_a.string())).getFileNameWithoutExtension()
            + "_x_"
            + juce::File(juce::String(params.file_b.string())).getFileNameWithoutExtension()
            + "_mashup.wav";
        std::filesystem::create_directories(out_dir.getFullPathName().toStdString());
        const auto out_path = out_dir.getChildFile(out_name);
        if (!write_wav(out_path, mr.buffer, sr_a)) {
            result.error_message = "Mashup WAV write failed";
            return result;
        }
        result.success     = true;
        result.output_path = out_path.getFullPathName().toStdString();
        result.target_bpm  = static_cast<float>(mr.target_bpm);
        result.target_key  = mr.target_key;
        result.length_sec  = static_cast<float>(mr.buffer.getNumSamples() / sr_a);
    } catch (const std::exception& e) {
        result.error_message = std::string("mashup exception: ") + e.what();
    }
    return result;
}
