#pragma once
// Phase 24-02 — Native port of sidecar/server/remix/chain_interpreter.py.
//
// Walks a list of EffectStep entries, dispatching each to the right
// native op (NativeEffectOps.h). Stems are kept as a 4-element working
// set ({vocals, drums, bass, other}); ops can target one stem, several,
// or "all" (sum, process, share back across stems).
//
// Final output is a weighted sum across the 4 stems using StemMix.

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "NativeAlgorithmicSeparator.h"   // separators::NativeStems
#include "NativeEffectOps.h"

namespace autoremix::dsp::engines {

struct StemMix {
    float vocals = 1.0f;
    float drums  = 1.0f;
    float bass   = 1.0f;
    float other  = 1.0f;
};

struct EffectStep {
    std::string              op;     // matches NativeEffectOps function names
    std::vector<std::string> stems;  // ["all"] or any subset of {vocals,drums,bass,other}
    ops::OpParams            params;
};

namespace detail {

using OpFn = juce::AudioBuffer<float> (*)(const juce::AudioBuffer<float>&,
                                          double, const ops::OpParams&);

struct OpEntry { const char* name; OpFn fn; };

// Single source of truth for op-name → implementation. Preset loaders use
// find_op() to validate op names at load time; dispatch uses it at run time.
inline const std::array<OpEntry, 23>& op_table()
{
    using namespace ops;
    static const std::array<OpEntry, 23> kOps = {{
        { "time_stretch",   &time_stretch   },
        { "pitch_shift",    &pitch_shift    },
        { "reverb",         &reverb         },
        { "chop",           &chop           },
        { "bass_boost",     &bass_boost     },
        { "eq_highpass",    &eq_highpass    },
        { "chop_beats",     &chop_beats     },
        { "chop_onsets",    &chop_onsets    },
        { "chop_bars",      &chop_bars      },
        { "gate_energy",    &gate_energy    },
        { "structural_cut", &structural_cut },
        // Phase 32-01: distortion + delay ops
        { "saturation",     &saturation     },
        { "bitcrusher",     &bitcrusher     },
        { "delay_pingpong", &delay_pingpong },
        { "delay_tape",     &delay_tape     },
        { "delay_reverse",  &delay_reverse  },
        { "delay_dotted",   &delay_dotted   },
        // Phase 32-02: creative reverbs + filter sweep + vinyl sim + sidechain
        { "reverb_plate",   &reverb_plate   },
        { "reverb_gated",   &reverb_gated   },
        { "reverb_reverse", &reverb_reverse },
        { "filter_sweep",   &filter_sweep   },
        { "vinyl_sim",      &vinyl_sim      },
        { "sidechain",      &sidechain      },
    }};
    return kOps;
}

inline OpFn find_op(const std::string& name)
{
    for (const auto& e : op_table())
        if (name == e.name) return e.fn;
    return nullptr;
}

} // namespace detail

inline bool is_known_op(const std::string& name)
{
    return detail::find_op(name) != nullptr;
}

namespace detail {

inline juce::AudioBuffer<float>
dispatch_op(const std::string& name,
            const juce::AudioBuffer<float>& in, double sr,
            const ops::OpParams& p)
{
    if (OpFn fn = find_op(name)) return fn(in, sr, p);
    juce::AudioBuffer<float> out; out.makeCopyOf(in); return out;
}

// Sum buffers to common min-length / max channel count
inline juce::AudioBuffer<float>
sum_buffers(const std::array<const juce::AudioBuffer<float>*, 4>& arr)
{
    int maxCh = 0, minN = INT32_MAX;
    for (auto* b : arr) {
        maxCh = std::max(maxCh, b->getNumChannels());
        if (b->getNumSamples() > 0)
            minN = std::min(minN, b->getNumSamples());
    }
    if (maxCh == 0 || minN == INT32_MAX) return juce::AudioBuffer<float>();

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

} // namespace detail

inline juce::AudioBuffer<float>
process_effect_chain(const separators::NativeStems& stemsIn,
                     const std::vector<EffectStep>& chain,
                     const StemMix& mix,
                     double sr)
{
    // Working set
    juce::AudioBuffer<float> arrays[4];
    arrays[0].makeCopyOf(stemsIn.vocals);
    arrays[1].makeCopyOf(stemsIn.drums);
    arrays[2].makeCopyOf(stemsIn.bass);
    arrays[3].makeCopyOf(stemsIn.other);

    static const std::array<const char*, 4> kNames = { "vocals", "drums", "bass", "other" };
    auto idxOf = [&](const std::string& n) -> int {
        for (int i = 0; i < 4; ++i)
            if (n == kNames[(size_t) i]) return i;
        return -1;
    };

    // Walk the chain
    for (const auto& step : chain) {
        const bool isAll = std::any_of(step.stems.begin(), step.stems.end(),
                                       [](const std::string& s) { return s == "all"; });
        if (isAll) {
            std::array<const juce::AudioBuffer<float>*, 4> raw = {
                &arrays[0], &arrays[1], &arrays[2], &arrays[3]
            };
            auto summed   = detail::sum_buffers(raw);
            auto processed = detail::dispatch_op(step.op, summed, sr, step.params);
            // Divide by 4 to match v3 chain_interpreter "share" semantics
            processed.applyGain(0.25f);
            for (int i = 0; i < 4; ++i) arrays[i].makeCopyOf(processed);
        } else {
            for (const auto& name : step.stems) {
                const int idx = idxOf(name);
                if (idx < 0) continue;
                arrays[(size_t) idx] = detail::dispatch_op(
                    step.op, arrays[(size_t) idx], sr, step.params);
            }
        }
    }

    // Final weighted mix
    const std::array<float, 4> weights = { mix.vocals, mix.drums, mix.bass, mix.other };

    int maxCh = 0, minN = INT32_MAX;
    for (int i = 0; i < 4; ++i) {
        maxCh = std::max(maxCh, arrays[i].getNumChannels());
        if (arrays[i].getNumSamples() > 0)
            minN = std::min(minN, arrays[i].getNumSamples());
    }
    if (maxCh == 0 || minN == INT32_MAX) return juce::AudioBuffer<float>();

    juce::AudioBuffer<float> out(maxCh, minN);
    out.clear();
    constexpr float invN = 1.0f / 4.0f;  // match v3 divide-by-len(STEM_NAMES)
    for (int i = 0; i < 4; ++i) {
        const float w = weights[(size_t) i] * invN;
        const int bCh = arrays[i].getNumChannels();
        if (bCh == 0) continue;
        for (int c = 0; c < maxCh; ++c) {
            const int srcC = (bCh == 1) ? 0 : std::min(c, bCh - 1);
            const float* pSrc = arrays[i].getReadPointer(srcC);
            float*       qDst = out.getWritePointer(c);
            for (int j = 0; j < minN; ++j)
                qDst[j] += pSrc[j] * w;
        }
    }
    return out;
}

} // namespace autoremix::dsp::engines
