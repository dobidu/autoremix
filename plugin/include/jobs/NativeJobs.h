#pragma once

// NativeJobs — the editor's heavy operations as free functions with explicit
// inputs (moved out of AutoRemixEditor.cpp in plan 34.5-02).
//
// None of these functions touch a UI object. They run on a JobRunner worker,
// take everything by value or by reference to processor-owned objects, check
// their CancelToken between stages, and report through JobEvents callbacks,
// which the caller makes thread-safe (they post to the message thread).
//
// DSP behaviour is unchanged from the editor versions (34.5-04 fixes DSP).

#include "jobs/JobRunner.h"
#include "PluginTypes.h"
#include "dsp/NativeAnalysis.h"
#include "dsp/NativeAlgorithmicSeparator.h"
#include "dsp/NativeDemucsSeparator.h"
#include "dsp/NativeRemixEngines.h"
#include "dsp/NativeEffectChainEngine.h"
#include "dsp/NativeStructuredRemixEngine.h"
#include "dsp/NativeMashupEngine.h"
#include "dsp/NativePresetTypes.h"
#include "dsp/ModelDownloader.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace autoremix::jobs {

/** Model state changes a job can report (mapped to ModelStatusDot by the UI). */
enum class ModelEvent { checking, downloading, cached, error, gpu_active };

/** Callbacks a job may invoke from its worker thread. The caller supplies
 *  thread-safe implementations (normally post_guarded to the message thread).
 *  Any of them may be empty. */
struct JobEvents {
    std::function<void(const juce::String&)> status;
    std::function<void(ModelEvent)>          model;
    std::function<void(double)>              progress;   // 0..1

    void say(const juce::String& s) const   { if (status) status(s); }
    void set_model(ModelEvent e) const      { if (model) model(e); }
    void report(double f) const             { if (progress) progress(f); }
};

namespace io {

// Load any registered format into a stereo float32 buffer at the file's native
// rate. Returns an empty buffer and sr=0 on failure.
inline std::pair<juce::AudioBuffer<float>, double>
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
    if (chans == 1) buf.copyFrom(1, 0, buf, 0, 0, n);  // mono -> duplicate
    buf.setSize(2, n, true, false, true);
    return { std::move(buf), sr };
}

inline bool write_wav(const juce::File& dest, const juce::AudioBuffer<float>& buf, double sr)
{
    dest.deleteFile();
    auto os = std::unique_ptr<juce::FileOutputStream>(dest.createOutputStream());
    if (!os) return false;
    juce::WavAudioFormat wav;
    auto writer = std::unique_ptr<juce::AudioFormatWriter>(
        wav.createWriterFor(os.get(), sr, static_cast<unsigned>(buf.getNumChannels()), 24, {}, 0));
    if (!writer) return false;
    os.release();   // writer owns the stream now
    return writer->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
}

inline autoremix::StemPaths
write_native_stems(const autoremix::dsp::separators::NativeStems& s,
                   const std::filesystem::path& dir, double sr)
{
    autoremix::StemPaths out;
    std::filesystem::create_directories(dir);
    const auto vp = dir / "vocals.wav";
    const auto dp = dir / "drums.wav";
    const auto bp = dir / "bass.wav";
    const auto op = dir / "other.wav";
    const bool ok = write_wav(juce::File(vp.string()), s.vocals, sr)
                 && write_wav(juce::File(dp.string()), s.drums,  sr)
                 && write_wav(juce::File(bp.string()), s.bass,   sr)
                 && write_wav(juce::File(op.string()), s.other,  sr);
    if (!ok) return out;
    out.vocals = vp; out.drums = dp; out.bass = bp; out.other = op;
    out.valid  = true;
    return out;
}

inline autoremix::dsp::separators::NativeStems
load_native_stems(juce::AudioFormatManager& fmt, const autoremix::StemPaths& paths, double& sr_out)
{
    autoremix::dsp::separators::NativeStems s;
    auto [v, sr_v] = load_audio(fmt, juce::File(paths.vocals.string()));
    auto [d, sr_d] = load_audio(fmt, juce::File(paths.drums.string()));
    auto [b, sr_b] = load_audio(fmt, juce::File(paths.bass.string()));
    auto [o, sr_o] = load_audio(fmt, juce::File(paths.other.string()));
    s.vocals = std::move(v); s.drums = std::move(d); s.bass = std::move(b); s.other = std::move(o);
    sr_out = sr_v;  // all four were written at the same rate
    (void) sr_d; (void) sr_b; (void) sr_o;
    return s;
}

inline juce::AudioBuffer<float> to_mono(const juce::AudioBuffer<float>& in)
{
    juce::AudioBuffer<float> mono(1, in.getNumSamples());
    mono.clear();
    for (int ch = 0; ch < in.getNumChannels(); ++ch)
        mono.addFrom(0, 0, in, ch, 0, in.getNumSamples(),
                     1.0f / static_cast<float>(in.getNumChannels()));
    return mono;
}

} // namespace io

// ── Analysis ────────────────────────────────────────────────────────────────

inline autoremix::FileAnalysis analyze_file(juce::AudioFormatManager& fmt, const juce::String& path)
{
    autoremix::FileAnalysis fa;
    auto [buf, sr] = io::load_audio(fmt, juce::File(path));
    if (buf.getNumSamples() == 0 || sr <= 0.0) return fa;
    auto mono = io::to_mono(buf);
    fa.bpm          = static_cast<float>(autoremix::dsp::analysis::detect_bpm(mono, sr));
    fa.key          = autoremix::dsp::analysis::detect_key(mono, sr);
    fa.duration_sec = static_cast<float>(buf.getNumSamples() / sr);
    return fa;
}

/** Basic analysis plus full structure analysis (cue auto-detection), as the
 *  drop-zone screen needs it. */
struct TrackAnalysis {
    autoremix::FileAnalysis               info;
    autoremix::dsp::analysis::SongStructure structure;
};

inline TrackAnalysis analyze_track(juce::AudioFormatManager& fmt, const juce::String& path,
                                   const CancelToken& cancel)
{
    TrackAnalysis out;
    out.info = analyze_file(fmt, path);
    if (is_cancelled(cancel)) return out;

    std::unique_ptr<juce::AudioFormatReader> reader(fmt.createReaderFor(juce::File(path)));
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
        if (!is_cancelled(cancel))
            out.structure = autoremix::dsp::analysis::analyze_structure(mono, sr);
    }
    return out;
}

// ── Separation ──────────────────────────────────────────────────────────────

inline autoremix::StemPaths separate(juce::AudioFormatManager& fmt,
                                     const std::filesystem::path& in,
                                     const std::filesystem::path& out,
                                     const std::string& sep_id,
                                     const CancelToken& cancel,
                                     const JobEvents& ev)
{
    autoremix::StemPaths empty;
    auto [buf, sr] = io::load_audio(fmt, juce::File(in.string()));
    if (buf.getNumSamples() == 0 || sr <= 0.0) {
        ev.say("Could not read input file");
        return empty;
    }
    if (is_cancelled(cancel)) return empty;

    autoremix::dsp::separators::NativeStems stems;

    if (sep_id == "demucs") {
        ev.say("Demucs: checking model cache...");
        ev.set_model(ModelEvent::downloading);

        auto download = autoremix::dsp::models::ensure_htdemucs([&ev, &cancel](double frac) {
            if (is_cancelled(cancel)) return;
            ev.say("Demucs: downloading model... " + juce::String((int) (frac * 100.0)) + "%");
        });
        if (is_cancelled(cancel)) return empty;
        if (!download.ok) {
            ev.set_model(ModelEvent::error);
            ev.say("Demucs model download failed: " + download.error);
            return empty;
        }
        ev.set_model(ModelEvent::cached);

        if (std::abs(sr - 44100.0) > 0.5) {   // R-11 (resampling) is fixed in 34.5-04
            ev.say("Demucs needs 44.1 kHz, got " + juce::String(sr) + " Hz");
            return empty;
        }
        ev.say("Separating (demucs)...");
        auto res = autoremix::dsp::separators::separate_demucs(
            buf, sr, download.path,
            [&ev, &cancel](double frac) {
                if (is_cancelled(cancel)) return;
                ev.report(frac);
                ev.say("Demucs: separating... " + juce::String((int) (frac * 100.0)) + "%");
            },
            cancel.get());
        if (res.cancelled || is_cancelled(cancel)) return empty;
        if (!res.ok) {
            ev.say("Demucs separation failed: " + res.error);
            return empty;
        }
        if (res.gpu_used) ev.set_model(ModelEvent::gpu_active);
        stems = std::move(res.stems);
    } else {
        ev.say("Separating (algorithmic)...");
        stems = autoremix::dsp::separators::separate_algorithmic(buf, sr);
    }

    if (is_cancelled(cancel)) return empty;
    return io::write_native_stems(stems, out, sr);
}

// ── Remix render ────────────────────────────────────────────────────────────

inline autoremix::ProcessResult
render_remix(juce::AudioFormatManager& fmt,
             const std::vector<autoremix::dsp::presets::NativeRemixPreset>& presets,
             const autoremix::StemPaths& stems,
             const autoremix::RemixParams& params,
             const std::filesystem::path& out_path,
             const CancelToken& cancel)
{
    autoremix::ProcessResult result;
    double sr = 0.0;
    auto native_stems = io::load_native_stems(fmt, stems, sr);
    if (sr <= 0.0) { result.error_message = "Could not read stems"; return result; }
    if (is_cancelled(cancel)) { result.error_message = "cancelled"; return result; }

    // Look up the preset by id (engine_id carries the preset's id), falling
    // back to the engine name in params.engine_id.
    const autoremix::dsp::presets::NativeRemixPreset* preset = nullptr;
    for (auto& p : presets)
        if (p.id == params.engine_id) { preset = &p; break; }

    namespace eng = autoremix::dsp::engines;
    eng::RemixParams ep;
    ep.tempo_factor       = params.tempo_factor;
    ep.pitch_shift_semi   = params.pitch_shift_semi;
    ep.reverb_mix         = params.reverb_mix;
    ep.chop_interval_ms   = params.chop_interval_ms;
    ep.bass_boost_db      = params.bass_boost_db;
    ep.drums_tempo_factor = params.drums_tempo_factor;

    auto apply_gain = [](juce::AudioBuffer<float>& b, float g) { if (g != 1.0f) b.applyGain(g); };
    apply_gain(native_stems.vocals, params.vocals_gain);
    apply_gain(native_stems.drums,  params.drums_gain);
    apply_gain(native_stems.bass,   params.bass_gain);
    apply_gain(native_stems.other,  params.other_gain);

    juce::AudioBuffer<float> out_buf;
    const std::string engine = preset ? preset->engine : params.engine_id;
    try {
        // fmt is processor-owned and only used for createReaderFor(), which is
        // safe to call concurrently once formats are registered.
        if      (engine == "chopped_screwed") out_buf = eng::chopped_and_screwed(native_stems, sr, ep);
        else if (engine == "slowed_reverb")   out_buf = eng::slowed_reverb(native_stems, sr, ep);
        else if (engine == "drum_and_bass")   out_buf = eng::drum_and_bass(native_stems, sr, ep);
        else if (engine == "effect_chain" && preset)
            out_buf = eng::process_effect_chain(native_stems, preset->effects, preset->stem_mix, sr);
        else if (engine == "structured_remix" && preset)
            out_buf = eng::structured_remix(native_stems, sr, ep, *preset, fmt);
        else
            out_buf = eng::chopped_and_screwed(native_stems, sr, ep);
    } catch (const std::exception& e) {
        result.error_message = std::string("engine exception: ") + e.what();
        return result;
    }
    if (is_cancelled(cancel)) { result.error_message = "cancelled"; return result; }

    if (!io::write_wav(juce::File(out_path.string()), out_buf, sr)) {
        result.error_message = "WAV write failed";
        return result;
    }
    result.success     = true;
    result.output_path = out_path;
    return result;
}

// ── Mashup ──────────────────────────────────────────────────────────────────

inline autoremix::MashupResult
run_mashup(juce::AudioFormatManager& fmt,
           const autoremix::StemPaths& stems_a_paths,
           const autoremix::StemPaths& stems_b_paths,
           const autoremix::MashupParams& params,
           const CancelToken& cancel)
{
    autoremix::MashupResult result;
    double sr_a = 0.0, sr_b = 0.0;
    auto stems_a = io::load_native_stems(fmt, stems_a_paths, sr_a);
    auto stems_b = io::load_native_stems(fmt, stems_b_paths, sr_b);
    if (sr_a <= 0.0 || sr_b <= 0.0) {
        result.error_message = "Mashup needs both tracks' stems";
        return result;
    }
    if (is_cancelled(cancel)) { result.error_message = "cancelled"; return result; }

    namespace eng = autoremix::dsp::engines;
    eng::MashupParams mp;
    mp.stem_gains_a             = params.stem_gains_a;
    mp.stem_gains_b             = params.stem_gains_b;
    mp.has_target_bpm           = params.has_target_bpm;
    mp.target_bpm               = params.target_bpm;
    mp.target_key               = params.target_key;
    mp.bpm_modifier             = params.bpm_modifier;
    mp.master_pitch_offset_semi = params.master_pitch_offset_semi;
    mp.master_reverb_mix        = params.master_reverb_mix;
    mp.master_reverb_room       = params.master_reverb_room;
    mp.highpass_b_hz            = params.highpass_b_hz;

    try {
        auto mr = eng::mashup(stems_a, stems_b, sr_a, mp);   // R-10 (B's rate) is 34.5-04
        if (mr.buffer.getNumSamples() == 0) {
            result.error_message = "Mashup produced empty buffer";
            return result;
        }
        if (is_cancelled(cancel)) { result.error_message = "cancelled"; return result; }

        const auto out_dir  = unique_job_dir("mashup", "mix");
        const auto out_name = juce::File(juce::String(params.file_a.string())).getFileNameWithoutExtension()
                            + "_x_"
                            + juce::File(juce::String(params.file_b.string())).getFileNameWithoutExtension()
                            + "_mashup.wav";
        const auto out_path = out_dir.getChildFile(out_name);
        if (!io::write_wav(out_path, mr.buffer, sr_a)) {
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

} // namespace autoremix::jobs
