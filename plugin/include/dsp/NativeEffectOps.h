#pragma once
// Phase 24-02 — Native ports of the v3 effect-chain ops (sidecar/server/remix/ops.py).
//
// 11 inline functions under namespace autoremix::dsp::ops, one per
// OP_REGISTRY key:
//   time_stretch, pitch_shift, reverb, chop, bass_boost, eq_highpass,
//   chop_beats, chop_onsets, chop_bars, gate_energy, structural_cut
//
// All take a juce::AudioBuffer<float>, sample rate, and an OpParams
// struct (single-struct shim for v3's dict params; extra fields ignored).
// All return a new juce::AudioBuffer<float>.

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "TimePitchStretcher.h"
#include "NativeAnalysis.h"
#include "NativeRemixEngines.h"   // engines::detail::apply_iir_filter, apply_reverb

namespace autoremix::dsp::ops {

struct OpParams {
    // numeric
    double factor        = 1.0;     // time_stretch
    double semitones     = 0.0;     // pitch_shift
    double mix           = 0.0;     // reverb
    double room_size     = 0.6;     // reverb
    double interval_ms   = 0.0;     // chop
    double db            = 0.0;     // bass_boost
    double cutoff_hz     = 0.0;     // eq_highpass
    double division      = 1.0;     // chop_beats
    int    offset_beats  = 0;       // chop_beats
    double min_gap_ms    = 80.0;    // chop_onsets
    double threshold     = 0.3;     // chop_onsets
    int    beats_per_bar = 4;       // chop_bars
    double threshold_db  = -20.0;   // gate_energy
    double hold_ms       = 50.0;    // gate_energy
    int    n_segments    = 8;       // structural_cut
    std::string mode     = "repeat_first";  // structural_cut
    int    repeat        = 2;       // chop_*
    // Phase 32-01 additions
    double drive         = 0.5;    // saturation: 0=clean, 1=heavy clip
    int    bit_depth     = 8;      // bitcrusher: target bit depth (4-16)
    double delay_ms      = 250.0;  // delay ops: delay time in ms
    double feedback      = 0.35;   // delay ops: feedback ratio (0-0.95)
    double stereo_width  = 1.0;    // delay_pingpong: L/R alternation depth
    double wow_depth     = 0.003;  // delay_tape: wow modulation depth (fraction of sr)
    double flutter_rate  = 6.0;    // delay_tape: flutter LFO rate (Hz)
    double delay_division = 0.375; // delay_dotted: note division (3/8 = dotted 1/8)
};

// ───────────────────────────────────────────────────────────────────────────
// Private helper
// ───────────────────────────────────────────────────────────────────────────

namespace detail {

inline juce::AudioBuffer<float>
chop_at_boundaries(const juce::AudioBuffer<float>& in,
                   std::vector<int> boundaries, int repeat)
{
    const int N  = in.getNumSamples();
    const int ch = in.getNumChannels();
    if (N == 0 || ch == 0) {
        juce::AudioBuffer<float> out; out.makeCopyOf(in); return out;
    }
    // Insert 0 and N, sort + dedupe, clamp to [0, N]
    boundaries.push_back(0);
    boundaries.push_back(N);
    for (auto& b : boundaries) b = std::clamp(b, 0, N);
    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end()),
                     boundaries.end());

    const int rep = std::max(1, repeat);

    // Build chunk list (each chunk = pair of indices repeated `rep` times)
    std::vector<std::pair<int,int>> chunks;
    chunks.reserve(boundaries.size() * (size_t) rep);
    for (size_t i = 0; i + 1 < boundaries.size(); ++i) {
        const int s = boundaries[i];
        const int e = boundaries[i + 1];
        if (e <= s) continue;
        for (int k = 0; k < rep; ++k)
            chunks.emplace_back(s, e);
    }
    if (chunks.empty()) {
        juce::AudioBuffer<float> out; out.makeCopyOf(in); return out;
    }

    // Total samples
    int total = 0;
    for (auto& [s, e] : chunks) total += (e - s);

    juce::AudioBuffer<float> tmp(ch, total);
    int pos = 0;
    for (auto& [s, e] : chunks) {
        const int n = e - s;
        for (int c = 0; c < ch; ++c)
            tmp.copyFrom(c, pos, in, c, s, n);
        pos += n;
    }

    // Trim or zero-pad to original length N (port matches ops.py)
    juce::AudioBuffer<float> out(ch, N);
    out.clear();
    const int copyN = std::min(N, tmp.getNumSamples());
    for (int c = 0; c < ch; ++c)
        out.copyFrom(c, 0, tmp, c, 0, copyN);
    return out;
}

} // namespace detail

// ───────────────────────────────────────────────────────────────────────────
// 6 basic ops
// ───────────────────────────────────────────────────────────────────────────

inline juce::AudioBuffer<float>
time_stretch(const juce::AudioBuffer<float>& in, double sr, const OpParams& p)
{
    if (std::abs(p.factor - 1.0) < 1e-3) {
        juce::AudioBuffer<float> out; out.makeCopyOf(in); return out;
    }
    TimePitchStretcher s((int) sr, in.getNumChannels());
    // RubberBand convention: ratio>1 = longer. v3 librosa convention:
    // rate<1 = slower (= longer). Invert to match v3 semantics.
    s.setTimeRatio(1.0 / p.factor);
    return s.process(in);
}

inline juce::AudioBuffer<float>
pitch_shift(const juce::AudioBuffer<float>& in, double sr, const OpParams& p)
{
    if (std::abs(p.semitones) < 1e-3) {
        juce::AudioBuffer<float> out; out.makeCopyOf(in); return out;
    }
    TimePitchStretcher s((int) sr, in.getNumChannels());
    s.setPitchSemitones(p.semitones);
    return s.process(in);
}

inline juce::AudioBuffer<float>
reverb(const juce::AudioBuffer<float>& in, double sr, const OpParams& p)
{
    return engines::detail::apply_reverb(
        in, sr,
        (float) p.mix,
        (float) p.room_size,
        /*damping*/  0.5f,
        /*dryLevel*/ 1.0f);
}

inline juce::AudioBuffer<float>
chop(const juce::AudioBuffer<float>& in, double sr, const OpParams& p)
{
    const int interval = (int) std::round(sr * p.interval_ms / 1000.0);
    const int N  = in.getNumSamples();
    const int ch = in.getNumChannels();
    if (interval <= 0 || N == 0 || ch == 0) {
        juce::AudioBuffer<float> out; out.makeCopyOf(in); return out;
    }

    juce::AudioBuffer<float> out(ch, N);
    out.clear();

    // Track most recent "current" chunk so we can stamp it on every other
    // interval (matches ops.py condition: i % (2*interval) >= interval).
    int prevStart = -1, prevLen = 0;

    int i = 0;
    while (i < N) {
        const int chunkEnd = std::min(i + interval, N);
        const int chunkLen = chunkEnd - i;
        const bool isAlt = (prevStart >= 0) && ((i % (2 * interval)) >= interval);
        if (isAlt) {
            const int srcLen = std::min(chunkLen, prevLen);
            for (int c = 0; c < ch; ++c)
                out.copyFrom(c, i, in, c, prevStart, srcLen);
        } else {
            for (int c = 0; c < ch; ++c)
                out.copyFrom(c, i, in, c, i, chunkLen);
            prevStart = i;
            prevLen   = chunkLen;
        }
        i = chunkEnd;
    }
    return out;
}

inline juce::AudioBuffer<float>
bass_boost(const juce::AudioBuffer<float>& in, double sr, const OpParams& p)
{
    const float linearGain = juce::Decibels::decibelsToGain((float) p.db);
    const float Q          = 1.0f / std::sqrt(2.0f);
    return engines::detail::apply_iir_filter(
        in,
        juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 200.0f, Q, linearGain));
}

inline juce::AudioBuffer<float>
eq_highpass(const juce::AudioBuffer<float>& in, double sr, const OpParams& p)
{
    return engines::detail::apply_iir_filter(
        in,
        juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, (float) p.cutoff_hz));
}

// ───────────────────────────────────────────────────────────────────────────
// Musical-chop ops + structural_cut
// ───────────────────────────────────────────────────────────────────────────

inline juce::AudioBuffer<float>
chop_beats(const juce::AudioBuffer<float>& in, double sr, const OpParams& p)
{
    auto beats = analysis::detect_beats(in, sr);
    if (p.division != 1.0 && p.division > 0.0) {
        const int steps = std::max(1, (int) std::round(1.0 / p.division));
        std::vector<double> extra;
        for (size_t i = 0; i + 1 < beats.size(); ++i) {
            const double interval = beats[i + 1] - beats[i];
            for (int s = 1; s < steps; ++s)
                extra.push_back(beats[i] + s * interval / steps);
        }
        beats.insert(beats.end(), extra.begin(), extra.end());
        std::sort(beats.begin(), beats.end());
    }
    if (p.offset_beats > 0 && (size_t) p.offset_beats < beats.size())
        beats.erase(beats.begin(), beats.begin() + p.offset_beats);

    std::vector<int> boundaries;
    boundaries.reserve(beats.size());
    for (double t : beats)
        boundaries.push_back(std::clamp((int) (t * sr), 0, in.getNumSamples()));
    return detail::chop_at_boundaries(in, std::move(boundaries), p.repeat);
}

inline juce::AudioBuffer<float>
chop_onsets(const juce::AudioBuffer<float>& in, double sr, const OpParams& p)
{
    auto onsets = analysis::detect_onsets(in, sr, p.min_gap_ms, p.threshold);
    std::vector<int> boundaries;
    boundaries.reserve(onsets.size());
    for (double t : onsets)
        boundaries.push_back(std::clamp((int) (t * sr), 0, in.getNumSamples()));
    return detail::chop_at_boundaries(in, std::move(boundaries), p.repeat);
}

inline juce::AudioBuffer<float>
chop_bars(const juce::AudioBuffer<float>& in, double sr, const OpParams& p)
{
    auto bars = analysis::detect_bars(in, sr, p.beats_per_bar);
    std::vector<int> boundaries;
    boundaries.reserve(bars.size());
    for (double t : bars)
        boundaries.push_back(std::clamp((int) (t * sr), 0, in.getNumSamples()));
    return detail::chop_at_boundaries(in, std::move(boundaries), p.repeat);
}

inline juce::AudioBuffer<float>
gate_energy(const juce::AudioBuffer<float>& in, double sr, const OpParams& p)
{
    auto mask = analysis::detect_energy_gates(in, sr, p.threshold_db, p.hold_ms);
    juce::AudioBuffer<float> out;
    out.makeCopyOf(in);
    const int N = std::min((int) mask.size(), out.getNumSamples());
    for (int c = 0; c < out.getNumChannels(); ++c) {
        auto* d = out.getWritePointer(c);
        for (int i = 0; i < N; ++i)
            if (!mask[(size_t) i]) d[i] = 0.0f;
    }
    return out;
}

inline juce::AudioBuffer<float>
structural_cut(const juce::AudioBuffer<float>& in, double /*sr*/, const OpParams& p)
{
    // Native fallback: uniform N-segment split (no MFCC clustering yet).
    const int N    = in.getNumSamples();
    const int ch   = in.getNumChannels();
    const int nSeg = std::max(2, p.n_segments);
    const int segLen = N / nSeg;
    if (segLen <= 0 || ch == 0) {
        juce::AudioBuffer<float> out; out.makeCopyOf(in); return out;
    }

    std::vector<std::pair<int,int>> segs;
    segs.reserve((size_t) nSeg);
    for (int i = 0; i < nSeg; ++i) {
        const int s = i * segLen;
        const int e = (i == nSeg - 1) ? N : std::min(N, s + segLen);
        segs.emplace_back(s, e);
    }

    if (p.mode == "reverse") {
        std::reverse(segs.begin(), segs.end());
    } else if (p.mode == "shuffle") {
        std::mt19937 rng(42);
        std::shuffle(segs.begin(), segs.end(), rng);
    } else if (p.mode == "repeat_first" && segs.size() > 1) {
        std::vector<std::pair<int,int>> woven;
        woven.reserve(segs.size() * 2);
        woven.push_back(segs[0]);
        for (size_t i = 1; i < segs.size(); ++i) {
            woven.push_back(segs[i]);
            woven.push_back(segs[0]);
        }
        segs = std::move(woven);
    }

    int total = 0;
    for (auto& [s, e] : segs) total += (e - s);

    juce::AudioBuffer<float> out(ch, total);
    int pos = 0;
    for (auto& [s, e] : segs) {
        const int n = e - s;
        for (int c = 0; c < ch; ++c)
            out.copyFrom(c, pos, in, c, s, n);
        pos += n;
    }
    return out;
}

// ───────────────────────────────────────────────────────────────────────────
// Phase 32-01: distortion + delay ops
// ───────────────────────────────────────────────────────────────────────────

// Tube saturation via tanh soft-clip with drive. Output normalized to [-1, 1].
inline juce::AudioBuffer<float>
saturation(const juce::AudioBuffer<float>& in, double sr, const OpParams& p)
{
    juce::AudioBuffer<float> out;
    out.makeCopyOf(in);
    if (p.drive < 0.01) return out;

    const double drive_gain = std::pow(10.0, p.drive * 2.0);
    const double norm       = std::tanh(drive_gain);
    if (norm < 1e-9) return out;

    for (int c = 0; c < out.getNumChannels(); ++c) {
        float* d = out.getWritePointer(c);
        for (int i = 0; i < out.getNumSamples(); ++i)
            d[i] = (float) (std::tanh((double)d[i] * drive_gain) / norm);
    }
    analysis::normalize_lufs(out, sr, -14.0);
    return out;
}

// Bit-depth reduction (quantization distortion). bit_depth clamped to [1, 16].
inline juce::AudioBuffer<float>
bitcrusher(const juce::AudioBuffer<float>& in, double /*sr*/, const OpParams& p)
{
    juce::AudioBuffer<float> out;
    out.makeCopyOf(in);
    const double levels = std::pow(2.0, (double) std::clamp(p.bit_depth, 1, 16)) - 1.0;
    if (levels < 1.0) return out;

    for (int c = 0; c < out.getNumChannels(); ++c) {
        float* d = out.getWritePointer(c);
        for (int i = 0; i < out.getNumSamples(); ++i)
            d[i] = (float) (std::round((double)d[i] * levels) / levels);
    }
    return out;
}

// Stereo ping-pong delay: L echoes on R and R echoes on L alternately.
inline juce::AudioBuffer<float>
delay_pingpong(const juce::AudioBuffer<float>& in, double sr, const OpParams& p)
{
    const int N    = in.getNumSamples();
    const int dly  = std::max(1, std::min((int)std::round(p.delay_ms / 1000.0 * sr), (int)(sr * 2.0)));
    const float fb = (float) std::clamp(p.feedback, 0.0, 0.95);
    const float sw = (float) std::clamp(p.stereo_width, 0.0, 1.0);

    // Ensure stereo
    juce::AudioBuffer<float> src(std::max(2, in.getNumChannels()), N);
    src.clear();
    for (int c = 0; c < in.getNumChannels(); ++c)
        src.copyFrom(c, 0, in, c, 0, N);
    if (in.getNumChannels() == 1)
        src.copyFrom(1, 0, src, 0, 0, N);

    juce::AudioBuffer<float> out(2, N);
    std::vector<float> delL(static_cast<size_t>(dly), 0.0f);
    std::vector<float> delR(static_cast<size_t>(dly), 0.0f);

    for (int i = 0; i < N; ++i) {
        const int idx = i % dly;
        const float dL = delL[static_cast<size_t>(idx)];
        const float dR = delR[static_cast<size_t>(idx)];
        const float inL = src.getSample(0, i);
        const float inR = src.getSample(1, i);

        out.setSample(0, i, juce::jlimit(-1.0f, 1.0f, inL + sw * dR));
        out.setSample(1, i, juce::jlimit(-1.0f, 1.0f, inR + sw * dL));

        delL[static_cast<size_t>(idx)] = inL + fb * dL;
        delR[static_cast<size_t>(idx)] = inR + fb * dR;
    }
    return out;
}

// Tape delay with wow/flutter LFO modulation.
inline juce::AudioBuffer<float>
delay_tape(const juce::AudioBuffer<float>& in, double sr, const OpParams& p)
{
    const int N    = in.getNumSamples();
    const int ch   = in.getNumChannels();
    const int base = std::max(1, std::min((int)std::round(p.delay_ms / 1000.0 * sr), (int)(sr * 2.0)));
    const int buf_size = base * 2 + 4;
    const float fb  = (float) std::clamp(p.feedback, 0.0, 0.95);
    const double wow_amp = p.wow_depth * sr;
    const double flutter_hz = p.flutter_rate;

    juce::AudioBuffer<float> out;
    out.makeCopyOf(in);

    for (int c = 0; c < ch; ++c) {
        std::vector<float> buf(static_cast<size_t>(buf_size), 0.0f);
        int write_pos = 0;
        float* d = out.getWritePointer(c);

        for (int i = 0; i < N; ++i) {
            const double mod = wow_amp * std::sin(2.0 * juce::MathConstants<double>::pi * flutter_hz * i / sr);
            const int read_offset = base + (int)mod;
            const int read_pos    = ((write_pos - read_offset) % buf_size + buf_size) % buf_size;

            const float delayed = buf[static_cast<size_t>(read_pos)];
            buf[static_cast<size_t>(write_pos)] = d[i] + fb * delayed;
            d[i] = juce::jlimit(-1.0f, 1.0f, d[i] + delayed);
            write_pos = (write_pos + 1) % buf_size;
        }
    }
    return out;
}

// Reverse delay: reverses windows then applies feedback echo.
inline juce::AudioBuffer<float>
delay_reverse(const juce::AudioBuffer<float>& in, double /*sr*/, const OpParams& p)
{
    juce::AudioBuffer<float> out;
    out.makeCopyOf(in);
    const int N  = out.getNumSamples();
    const int ch = out.getNumChannels();
    // window_samples: use delay_ms rounded to input samples (no sr conversion needed if 0)
    const int win = std::max(1, std::min((int)(p.delay_ms * 44.1), N));  // rough: delay_ms * samples_per_ms
    const float fb = (float) std::clamp(p.feedback, 0.0, 0.95);

    for (int c = 0; c < ch; ++c) {
        float* d = out.getWritePointer(c);
        std::vector<float> prev(static_cast<size_t>(win), 0.0f);
        for (int s = 0; s < N; s += win) {
            const int end = std::min(s + win, N);
            const int len = end - s;
            std::vector<float> chunk(d + s, d + end);
            std::reverse(chunk.begin(), chunk.end());
            for (int k = 0; k < len; ++k)
                d[s + k] = juce::jlimit(-1.0f, 1.0f, chunk[static_cast<size_t>(k)] + fb * prev[static_cast<size_t>(k)]);
            prev.assign(d + s, d + end);
            prev.resize(static_cast<size_t>(win), 0.0f);
        }
    }
    return out;
}

// Dotted-note delay: delay time = beat * delay_division (default 3/8 = dotted 1/8).
inline juce::AudioBuffer<float>
delay_dotted(const juce::AudioBuffer<float>& in, double sr, const OpParams& p)
{
    const int N  = in.getNumSamples();
    const int ch = in.getNumChannels();
    double bpm = analysis::detect_bpm(in, sr);
    if (bpm < 20.0) bpm = 120.0;
    const double beat_samples = sr * 60.0 / bpm;
    const int dly = std::max(1, std::min((int)(beat_samples * p.delay_division), (int)(sr * 2.0)));
    const float fb = (float) std::clamp(p.feedback, 0.0, 0.95);

    juce::AudioBuffer<float> out;
    out.makeCopyOf(in);
    for (int c = 0; c < ch; ++c) {
        std::vector<float> buf(static_cast<size_t>(dly), 0.0f);
        float* d = out.getWritePointer(c);
        for (int i = 0; i < N; ++i) {
            const int idx = i % dly;
            const float delayed = buf[static_cast<size_t>(idx)];
            buf[static_cast<size_t>(idx)] = d[i] + fb * delayed;
            d[i] = juce::jlimit(-1.0f, 1.0f, d[i] + delayed);
        }
    }
    return out;
}

} // namespace autoremix::dsp::ops
