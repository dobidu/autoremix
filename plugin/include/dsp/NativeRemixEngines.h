#pragma once
// Phase 24-01 — Native remix engines.
//
// Header-only port of:
//   sidecar/server/remix/chopped_screwed.py
//   sidecar/server/remix/slowed_reverb.py
//   sidecar/server/remix/drum_and_bass.py
//
// All three consume separators::NativeStems + sr + RemixParams and
// return a single mixed + processed juce::AudioBuffer<float>. The
// caller owns the WAV write (Phase 27 wire-up).
//
// Built on Phase 23 primitives: TimePitchStretcher (RubberBand) +
// NativeAnalysis (BPM, LUFS) + juce::dsp::IIR / Reverb.
//
// RubberBand semantics note: setTimeRatio(R) where R>1 produces
// LONGER output (= slower). Python librosa's effects.time_stretch(rate)
// has rate<1 = slower. So C++ engines invert: setTimeRatio(1.0 / tempo_factor).

#include <algorithm>
#include <cmath>
#include <memory>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "TimePitchStretcher.h"
#include "NativeAnalysis.h"
#include "NativeAlgorithmicSeparator.h"

namespace autoremix::dsp::engines {

struct RemixParams {
    float tempo_factor       = 1.0f;
    float pitch_shift_semi   = 0.0f;
    float reverb_mix         = 0.0f;
    float chop_interval_ms   = 0.0f;
    float bass_boost_db      = 0.0f;
    float drums_tempo_factor = 1.0f;
};

namespace detail {

// Equal-weight mix of the 4 stems, /4.0. Output matches max channel
// count, min sample count across the stems.
inline juce::AudioBuffer<float>
mix_stems_equal(const separators::NativeStems& s)
{
    const std::array<const juce::AudioBuffer<float>*, 4> arr = {
        &s.vocals, &s.drums, &s.bass, &s.other
    };

    int maxCh = 0, minN = INT32_MAX;
    for (auto* b : arr) {
        maxCh = std::max(maxCh, b->getNumChannels());
        if (b->getNumSamples() > 0)
            minN = std::min(minN, b->getNumSamples());
    }
    if (maxCh == 0 || minN == INT32_MAX || minN == 0)
        return juce::AudioBuffer<float>();

    juce::AudioBuffer<float> out(maxCh, minN);
    out.clear();
    constexpr float invN = 1.0f / 4.0f;
    for (auto* b : arr) {
        const int bCh = b->getNumChannels();
        if (bCh == 0) continue;
        for (int c = 0; c < maxCh; ++c) {
            // Mono → broadcast across all output channels
            const int srcC = (bCh == 1) ? 0 : std::min(c, bCh - 1);
            const float* p = b->getReadPointer(srcC);
            float*       q = out.getWritePointer(c);
            for (int i = 0; i < minN; ++i)
                q[i] += p[i] * invN;
        }
    }
    return out;
}

inline juce::AudioBuffer<float>
apply_iir_filter(const juce::AudioBuffer<float>& in,
                 juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>> coefs)
{
    juce::AudioBuffer<float> out;
    out.makeCopyOf(in);
    if (out.getNumSamples() == 0) return out;

    juce::dsp::IIR::Filter<float> filt;
    filt.coefficients = coefs;
    for (int c = 0; c < out.getNumChannels(); ++c) {
        filt.reset();
        auto* d = out.getWritePointer(c);
        for (int i = 0; i < out.getNumSamples(); ++i)
            d[i] = filt.processSample(d[i]);
    }
    return out;
}

inline juce::AudioBuffer<float>
apply_reverb(const juce::AudioBuffer<float>& in, double sr,
             float wetLevel, float roomSize,
             float damping  = 0.5f,
             float dryLevel = 1.0f)
{
    juce::AudioBuffer<float> out;
    out.makeCopyOf(in);
    if (out.getNumSamples() == 0) return out;

    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters params;
    params.roomSize = juce::jlimit(0.0f, 1.0f, roomSize);
    params.damping  = juce::jlimit(0.0f, 1.0f, damping);
    params.wetLevel = juce::jlimit(0.0f, 1.0f, wetLevel);
    params.dryLevel = juce::jlimit(0.0f, 1.0f, dryLevel);
    params.width    = 1.0f;
    reverb.setParameters(params);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = (juce::uint32) out.getNumSamples();
    spec.numChannels      = (juce::uint32) out.getNumChannels();
    reverb.prepare(spec);
    reverb.reset();

    juce::dsp::AudioBlock<float>           block(out);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    reverb.process(ctx);
    return out;
}

inline double
beat_aligned_ms(double interval_ms, double bpm, int min_beats = 1)
{
    if (bpm <= 0.0) return interval_ms;
    const double beat_ms = 60000.0 / bpm;
    const int    n_beats = std::max(min_beats, (int) std::round(interval_ms / beat_ms));
    return (double) n_beats * beat_ms;
}

inline juce::AudioBuffer<float>
stutter_chop(const juce::AudioBuffer<float>& in, double sr,
             double interval_ms, int stutter_every = 4)
{
    const int chunk = (int) std::round(sr * interval_ms / 1000.0);
    const int N     = in.getNumSamples();
    const int ch    = in.getNumChannels();
    if (chunk < 1 || N == 0 || ch == 0) return in;

    // Build the stutter sequence by walking input chunks; every Nth chunk
    // is duplicated. Allocate a temp buffer, then trim to N.
    const int maxOut = N + (N / chunk) / stutter_every * chunk + chunk;
    juce::AudioBuffer<float> tmp(ch, maxOut);
    tmp.clear();

    int writePos = 0;
    int chunkIdx = 0;
    for (int i = 0; i < N && writePos < maxOut; i += chunk) {
        const int copyN = std::min(chunk, N - i);
        for (int c = 0; c < ch; ++c)
            tmp.copyFrom(c, writePos, in, c, i, copyN);
        writePos += copyN;
        ++chunkIdx;
        if (chunkIdx % stutter_every == 0 && writePos + copyN <= maxOut) {
            for (int c = 0; c < ch; ++c)
                tmp.copyFrom(c, writePos, in, c, i, copyN);
            writePos += copyN;
        }
    }

    // Do NOT trim back to N: each stutter legitimately extends the track.
    // Trimming caused every stutter to silently drop a chunk from later in the
    // song, making the beat drift after each repeat. Authentic C&S extends
    // duration by n_stutters * chunk_size.
    juce::AudioBuffer<float> out(ch, writePos);
    for (int c = 0; c < ch; ++c)
        out.copyFrom(c, 0, tmp, c, 0, writePos);
    return out;
}

} // namespace detail

// ───────────────────────────────────────────────────────────────────────────
// Chopped & Screwed
// ───────────────────────────────────────────────────────────────────────────

inline juce::AudioBuffer<float>
chopped_and_screwed(const separators::NativeStems& stems, double sr,
                    const RemixParams& p)
{
    auto buf = detail::mix_stems_equal(stems);
    if (buf.getNumSamples() == 0) return buf;

    const double source_bpm = analysis::detect_bpm(buf, sr);

    // 1. Time-stretch + pitch-shift combined (one RubberBand pass)
    {
        TimePitchStretcher s((int) sr, buf.getNumChannels());
        s.setTimeRatio(1.0 / (double) p.tempo_factor);
        if (std::abs(p.pitch_shift_semi) > 1e-3f)
            s.setPitchSemitones((double) p.pitch_shift_semi);
        buf = s.process(buf);
    }
    // 2. High-pass: pitch-down accumulates sub-mud; scale cutoff with depth
    if (p.pitch_shift_semi < -1.0f) {
        const float cutoff = 60.0f + std::abs(p.pitch_shift_semi) * 4.0f;
        buf = detail::apply_iir_filter(
            buf, juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, cutoff));
    }
    // 4. Beat-aligned stutter chop
    if (p.chop_interval_ms > 0.0f) {
        const double slowed_bpm = source_bpm * (double) p.tempo_factor;
        const double aligned    = detail::beat_aligned_ms(
            (double) p.chop_interval_ms, slowed_bpm);
        buf = detail::stutter_chop(buf, sr, aligned, 4);
    }
    // 5. Light reverb
    if (p.reverb_mix > 0.0f) {
        buf = detail::apply_reverb(buf, sr,
                                   p.reverb_mix, /*roomSize*/ 0.55f,
                                   /*damping*/  0.5f,
                                   /*dryLevel*/ 1.0f - p.reverb_mix);
    }
    // 6. LUFS normalize
    analysis::normalize_lufs(buf, sr, -14.0);
    return buf;
}

// ───────────────────────────────────────────────────────────────────────────
// Slowed Reverb
// ───────────────────────────────────────────────────────────────────────────

inline juce::AudioBuffer<float>
slowed_reverb(const separators::NativeStems& stems, double sr,
              const RemixParams& p)
{
    auto buf = detail::mix_stems_equal(stems);
    if (buf.getNumSamples() == 0) return buf;

    // 1. Time-stretch + pitch-shift combined (one RubberBand pass)
    {
        TimePitchStretcher s((int) sr, buf.getNumChannels());
        s.setTimeRatio(1.0 / (double) p.tempo_factor);
        if (std::abs(p.pitch_shift_semi) > 1e-3f)
            s.setPitchSemitones((double) p.pitch_shift_semi);
        buf = s.process(buf);
    }
    // 2. Restore presence lost to time-stretch (scales with slowdown depth)
    const float presence_db = std::max(0.0f, (1.0f - p.tempo_factor) * 8.0f);
    if (presence_db > 0.1f) {
        const float linearGain = juce::Decibels::decibelsToGain(presence_db);
        const float Q          = 1.0f / std::sqrt(2.0f);
        buf = detail::apply_iir_filter(
            buf,
            juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 5000.0f, Q, linearGain));
    }
    // 4. Reverb wash
    buf = detail::apply_reverb(buf, sr,
                               p.reverb_mix, /*roomSize*/ 0.75f,
                               /*damping*/   0.5f,
                               /*dryLevel*/  1.0f - p.reverb_mix);
    // 5. LUFS normalize
    analysis::normalize_lufs(buf, sr, -14.0);
    return buf;
}

// ───────────────────────────────────────────────────────────────────────────
// Drum and Bass
// ───────────────────────────────────────────────────────────────────────────

inline juce::AudioBuffer<float>
drum_and_bass(const separators::NativeStems& stems, double sr,
              const RemixParams& p)
{
    constexpr double kDnBTarget = 170.0;

    const auto& drumsIn  = stems.drums;
    const auto& bassIn   = stems.bass;
    const auto& vocalsIn = stems.vocals;
    const auto& otherIn  = stems.other;

    if (drumsIn.getNumSamples() == 0) return juce::AudioBuffer<float>();

    // 1. BPM from drums stem (cleaner than full-mix)
    const double source_bpm  = analysis::detect_bpm(drumsIn, sr);
    const double auto_factor = source_bpm > 0.0 ? (kDnBTarget / source_bpm) : 1.0;
    const double drum_factor = auto_factor * (double) p.drums_tempo_factor;
    const double vocal_factor = (std::abs(p.tempo_factor - 1.0f) > 1e-6f)
                                  ? drum_factor * (double) p.tempo_factor
                                  : drum_factor;

    auto stretch = [sr](const juce::AudioBuffer<float>& in, double rate) {
        if (in.getNumSamples() == 0 || std::abs(rate - 1.0) < 1e-3) {
            juce::AudioBuffer<float> copy; copy.makeCopyOf(in); return copy;
        }
        TimePitchStretcher s((int) sr, in.getNumChannels());
        // Convention parity with C&S/SR: ratio>1 = longer
        s.setTimeRatio(1.0 / rate);
        return s.process(in);
    };

    // 2. Drums → DnB grid
    auto drums = stretch(drumsIn, drum_factor);

    // 3. Bass: low-shelf boost @ 250 Hz
    juce::AudioBuffer<float> bass;
    if (std::abs(p.bass_boost_db) > 0.01f) {
        const float linearGain = juce::Decibels::decibelsToGain(p.bass_boost_db);
        const float Q          = 1.0f / std::sqrt(2.0f);
        bass = detail::apply_iir_filter(
            bassIn,
            juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 250.0f, Q, linearGain));
    } else {
        bass.makeCopyOf(bassIn);
    }

    // 4. Vocals: stretch + pitch (combined, single RubberBand pass)
    juce::AudioBuffer<float> vocals;
    if (vocalsIn.getNumSamples() == 0) {
        vocals.makeCopyOf(vocalsIn);
    } else {
        TimePitchStretcher s((int) sr, vocalsIn.getNumChannels());
        s.setTimeRatio(1.0 / vocal_factor);
        if (std::abs(p.pitch_shift_semi) > 1e-3f)
            s.setPitchSemitones((double) p.pitch_shift_semi);
        vocals = s.process(vocalsIn);
    }

    // 5. Other: stretch only
    auto other = stretch(otherIn, vocal_factor);

    // 6. Weighted mix
    const std::array<std::pair<const juce::AudioBuffer<float>*, float>, 4> mix = {{
        { &drums,  1.2f },
        { &bass,   1.0f },
        { &vocals, 0.7f },
        { &other,  0.5f },
    }};
    int maxCh = 0, minN = INT32_MAX;
    for (auto& [b, w] : mix) {
        maxCh = std::max(maxCh, b->getNumChannels());
        if (b->getNumSamples() > 0)
            minN = std::min(minN, b->getNumSamples());
    }
    if (maxCh == 0 || minN == INT32_MAX || minN == 0) return juce::AudioBuffer<float>();

    constexpr float invSum = 1.0f / 3.4f;   // 1.2 + 1.0 + 0.7 + 0.5
    juce::AudioBuffer<float> out(maxCh, minN);
    out.clear();
    for (auto& [b, w] : mix) {
        const int bCh = b->getNumChannels();
        if (bCh == 0) continue;
        for (int c = 0; c < maxCh; ++c) {
            const int srcC = (bCh == 1) ? 0 : std::min(c, bCh - 1);
            const float* pSrc = b->getReadPointer(srcC);
            float*       qDst = out.getWritePointer(c);
            for (int i = 0; i < minN; ++i)
                qDst[i] += pSrc[i] * w * invSum;
        }
    }

    // 7. LUFS normalize
    analysis::normalize_lufs(out, sr, -14.0);
    return out;
}

} // namespace autoremix::dsp::engines
