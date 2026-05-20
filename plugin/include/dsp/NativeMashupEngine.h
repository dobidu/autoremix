#pragma once
// Phase 25-01 — Native port of sidecar/server/remix/mashup.py.
//
// Pairwise mashup: take two NativeStems (already separated), tempo-match
// and key-match track B to track A's anchor (or to user-specified targets),
// apply per-stem gains, sum, optional master pitch + reverb, normalize LUFS.
// Returns a single mixed buffer + diagnostic metadata.

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "TimePitchStretcher.h"
#include "NativeAnalysis.h"
#include "NativeAlgorithmicSeparator.h"
#include "NativeRemixEngines.h"   // engines::detail::apply_iir_filter, apply_reverb

namespace autoremix::dsp::engines {

inline constexpr double kMashupStretchMin = 0.5;
inline constexpr double kMashupStretchMax = 2.0;

inline const std::array<const char*, 4> kMashupStemNames = {
    "vocals", "drums", "bass", "other"
};

struct MashupParams {
    std::unordered_map<std::string, float> stem_gains_a;
    std::unordered_map<std::string, float> stem_gains_b;
    bool        has_target_bpm           = false;
    float       target_bpm               = 0.0f;
    std::string target_key;                                  // empty = anchor to A
    float       bpm_modifier             = 1.0f;
    float       master_pitch_offset_semi = 0.0f;
    float       master_reverb_mix        = 0.0f;
    float       master_reverb_room       = 0.5f;
    float       highpass_b_hz            = 0.0f;
};

struct MashupResult {
    juce::AudioBuffer<float> buffer;
    double      target_bpm        = 0.0;
    std::string target_key;
    double      length_sec        = 0.0;
    double      source_bpm_a      = 0.0;
    double      source_bpm_b      = 0.0;
    std::string source_key_a;
    std::string source_key_b;
    double      stretch_factor_b  = 1.0;
    int         semitone_shift_b  = 0;
};

namespace mashup_detail {

inline juce::AudioBuffer<float>
sum_stems(const separators::NativeStems& s)
{
    std::array<const juce::AudioBuffer<float>*, 4> arr = {
        &s.vocals, &s.drums, &s.bass, &s.other
    };
    int maxCh = 0, minN = INT32_MAX;
    for (auto* b : arr) {
        maxCh = std::max(maxCh, b->getNumChannels());
        if (b->getNumSamples() > 0) minN = std::min(minN, b->getNumSamples());
    }
    if (maxCh == 0 || minN == INT32_MAX || minN == 0)
        return juce::AudioBuffer<float>();

    juce::AudioBuffer<float> out(maxCh, minN);
    out.clear();
    for (auto* b : arr) {
        const int bCh = b->getNumChannels();
        if (bCh == 0) continue;
        for (int c = 0; c < maxCh; ++c) {
            const int srcC = (bCh == 1) ? 0 : std::min(c, bCh - 1);
            const float* pSrc = b->getReadPointer(srcC);
            float*       qDst = out.getWritePointer(c);
            for (int i = 0; i < minN; ++i)
                qDst[i] += pSrc[i];
        }
    }
    return out;
}

inline const juce::AudioBuffer<float>&
get_stem(const separators::NativeStems& s, const std::string& name)
{
    if (name == "vocals") return s.vocals;
    if (name == "drums")  return s.drums;
    if (name == "bass")   return s.bass;
    return s.other;
}

inline float
lookup_gain(const std::unordered_map<std::string, float>& m,
            const std::string& name, float fallback = 1.0f)
{
    auto it = m.find(name);
    return (it != m.end()) ? it->second : fallback;
}

} // namespace mashup_detail

inline MashupResult
mashup(const separators::NativeStems& a,
       const separators::NativeStems& b,
       double sr,
       const MashupParams& p)
{
    MashupResult result;

    // 1. Analyze each track via summed mix
    auto mix_a = mashup_detail::sum_stems(a);
    auto mix_b = mashup_detail::sum_stems(b);
    if (mix_a.getNumSamples() == 0 || mix_b.getNumSamples() == 0)
        return result;

    const double bpm_a = analysis::detect_bpm(mix_a, sr);
    const double bpm_b = analysis::detect_bpm(mix_b, sr);
    const std::string key_a = analysis::detect_key(mix_a, sr);
    const std::string key_b = analysis::detect_key(mix_b, sr);

    // 2. Targets
    double target_bpm = p.has_target_bpm ? (double) p.target_bpm : bpm_a;
    target_bpm *= (double) p.bpm_modifier;
    const std::string target_key = p.target_key.empty() ? key_a : p.target_key;

    // 3. Stretch factors (clamped)
    double stretch_a = (bpm_a > 0.0) ? (target_bpm / bpm_a) : 1.0;
    double stretch_b = (bpm_b > 0.0) ? (target_bpm / bpm_b) : 1.0;
    stretch_a = std::clamp(stretch_a, kMashupStretchMin, kMashupStretchMax);
    stretch_b = std::clamp(stretch_b, kMashupStretchMin, kMashupStretchMax);

    const int delta_b = analysis::semitone_delta(key_b, target_key);

    // 4. Process each stem
    std::vector<juce::AudioBuffer<float>> all_processed;
    all_processed.reserve(8);

    for (const char* nameCstr : kMashupStemNames) {
        const std::string name = nameCstr;

        // ── Track A: stretch only if bpm_modifier shifts off A's bpm
        {
            const auto& src = mashup_detail::get_stem(a, name);
            juce::AudioBuffer<float> ba;
            if (src.getNumSamples() > 0 && std::abs(stretch_a - 1.0) > 1e-3) {
                TimePitchStretcher s((int) sr, src.getNumChannels());
                s.setTimeRatio(stretch_a);
                ba = s.process(src);
            } else {
                ba.makeCopyOf(src);
            }
            const float gain_a = mashup_detail::lookup_gain(p.stem_gains_a, name);
            if (gain_a > 0.0f && ba.getNumSamples() > 0) {
                ba.applyGain(gain_a);
                all_processed.push_back(std::move(ba));
            }
        }

        // ── Track B: optional HPF → stretch → pitch shift
        {
            const auto& src = mashup_detail::get_stem(b, name);
            juce::AudioBuffer<float> bb;
            bb.makeCopyOf(src);
            if (bb.getNumSamples() == 0) continue;

            if (p.highpass_b_hz > 0.0f) {
                bb = engines::detail::apply_iir_filter(
                    bb,
                    juce::dsp::IIR::Coefficients<float>::makeHighPass(
                        sr, (float) p.highpass_b_hz));
            }
            if (std::abs(stretch_b - 1.0) > 1e-3) {
                TimePitchStretcher s((int) sr, bb.getNumChannels());
                s.setTimeRatio(stretch_b);
                bb = s.process(bb);
            }
            if (delta_b != 0) {
                TimePitchStretcher ps((int) sr, bb.getNumChannels());
                ps.setPitchSemitones((double) delta_b);
                bb = ps.process(bb);
            }
            const float gain_b = mashup_detail::lookup_gain(p.stem_gains_b, name);
            if (gain_b > 0.0f) {
                bb.applyGain(gain_b);
                all_processed.push_back(std::move(bb));
            }
        }
    }

    if (all_processed.empty()) return result;

    // 5. Sum to min length, max channel count
    int minN  = all_processed.front().getNumSamples();
    int maxCh = all_processed.front().getNumChannels();
    for (auto& buf : all_processed) {
        if (buf.getNumSamples() > 0)
            minN = std::min(minN, buf.getNumSamples());
        maxCh = std::max(maxCh, buf.getNumChannels());
    }
    if (minN <= 0) return result;

    juce::AudioBuffer<float> mixed(maxCh, minN);
    mixed.clear();
    for (auto& buf : all_processed) {
        const int bCh = buf.getNumChannels();
        if (bCh == 0) continue;
        for (int c = 0; c < maxCh; ++c) {
            const int srcC = (bCh == 1) ? 0 : std::min(c, bCh - 1);
            const float* pSrc = buf.getReadPointer(srcC);
            float*       qDst = mixed.getWritePointer(c);
            for (int i = 0; i < minN; ++i)
                qDst[i] += pSrc[i];
        }
    }

    // 6. Master pitch offset
    if (std::abs(p.master_pitch_offset_semi) > 1e-3f) {
        TimePitchStretcher ps((int) sr, mixed.getNumChannels());
        ps.setPitchSemitones((double) p.master_pitch_offset_semi);
        mixed = ps.process(mixed);
    }

    // 7. Master reverb
    if (p.master_reverb_mix > 0.0f) {
        mixed = engines::detail::apply_reverb(
            mixed, sr,
            p.master_reverb_mix,
            p.master_reverb_room,
            /*damping*/  0.5f,
            /*dryLevel*/ 1.0f);
    }

    // 8. LUFS normalize
    analysis::normalize_lufs(mixed, sr, -14.0);

    // 9. Populate result
    const int final_len = mixed.getNumSamples();
    result.buffer            = std::move(mixed);
    result.target_bpm        = target_bpm;
    result.target_key        = target_key;
    result.length_sec        = (double) final_len / sr;
    result.source_bpm_a      = bpm_a;
    result.source_bpm_b      = bpm_b;
    result.source_key_a      = key_a;
    result.source_key_b      = key_b;
    result.stretch_factor_b  = stretch_b;
    result.semitone_shift_b  = delta_b;
    return result;
}

} // namespace autoremix::dsp::engines
