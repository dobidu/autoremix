#pragma once
// Phase 23-03 — Native 4-band algorithmic stem separator.
//
// Port of sidecar/server/separators/algorithmic_sep.py. Always available
// (no ML, no external lib). Low quality by design — fast fallback when
// the user doesn't want to use the ONNX Demucs path.
//
// Band assignments (verbatim from v3):
//   bass   : 0      – 250  Hz
//   drums  : 250    – 2000 Hz
//   vocals : 2000   – 6000 Hz
//   other  : 6000   – sr/2 Hz
//
// Implementation: per-band cascade of 2nd-order Butterworth high-pass +
// 2nd-order Butterworth low-pass (juce::dsp::IIR). Approximates scipy's
// butter(4, [lo, hi], btype='band') closely enough for this separator's
// quality target.

#include <algorithm>
#include <cmath>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace autoremix::dsp::separators {

struct NativeStems {
    juce::AudioBuffer<float> vocals;
    juce::AudioBuffer<float> drums;
    juce::AudioBuffer<float> bass;
    juce::AudioBuffer<float> other;
};

namespace detail {

// 2nd-order HP + 2nd-order LP cascade per channel. Degrades gracefully
// at edges: lo <= 0 skips the HP stage; hi >= sr/2 - 1 skips the LP
// stage; lo >= hi returns a zero buffer.
inline juce::AudioBuffer<float>
bandpass_butter(const juce::AudioBuffer<float>& in,
                double sr, double loHz, double hiHz)
{
    const double nyq = sr * 0.5;

    juce::AudioBuffer<float> out;
    out.makeCopyOf(in);

    if (loHz >= hiHz) { out.clear(); return out; }

    const int nCh = out.getNumChannels();
    const int nS  = out.getNumSamples();
    if (nCh == 0 || nS == 0) return out;

    if (loHz > 0.0) {
        auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, (float) loHz);
        juce::dsp::IIR::Filter<float> filt;
        filt.coefficients = hp;
        for (int c = 0; c < nCh; ++c) {
            filt.reset();
            auto* d = out.getWritePointer(c);
            for (int i = 0; i < nS; ++i)
                d[i] = filt.processSample(d[i]);
        }
    }
    if (hiHz < nyq - 1.0) {
        auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, (float) hiHz);
        juce::dsp::IIR::Filter<float> filt;
        filt.coefficients = lp;
        for (int c = 0; c < nCh; ++c) {
            filt.reset();
            auto* d = out.getWritePointer(c);
            for (int i = 0; i < nS; ++i)
                d[i] = filt.processSample(d[i]);
        }
    }
    return out;
}

} // namespace detail

inline NativeStems
separate_algorithmic(const juce::AudioBuffer<float>& input, double sr)
{
    NativeStems out;
    out.bass   = detail::bandpass_butter(input, sr, 0.0,    250.0);
    out.drums  = detail::bandpass_butter(input, sr, 250.0,  2000.0);
    out.vocals = detail::bandpass_butter(input, sr, 2000.0, 6000.0);
    out.other  = detail::bandpass_butter(input, sr, 6000.0, sr * 0.5);
    return out;
}

} // namespace autoremix::dsp::separators
