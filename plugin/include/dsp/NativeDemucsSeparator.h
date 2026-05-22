#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// NativeDemucsSeparator — Phase 26-02
//
// Header-only ONNX Runtime wrapper that runs htdemucs separation on a
// JUCE AudioBuffer. The model file is the one produced by
// tools/export_demucs_onnx.py (a fixed-window htdemucs export, see
// Phase 26-01 SUMMARY).
//
// Constraints:
//   - Input must be 44.1 kHz, mono or stereo float (mono → duplicated).
//     Resampling happens upstream (DAWs are typically 44.1 / 48 kHz;
//     screens convert before calling).
//   - Model window is fixed at 343,980 samples (7.8 s @ 44.1 kHz).
//     Inputs longer than one window are chunked with 25% overlap and
//     stitched with a raised-cosine cross-fade.
//   - CPU execution provider only. CUDA / DirectML deferred to v4.1.
//   - Single Ort::Session is reused across chunks of one call.
//
// Public API:
//   autoremix::dsp::separators::DemucsResult
//   autoremix::dsp::separators::separate_demucs(buf, sr, model_path,
//                                               progress_cb)
// ─────────────────────────────────────────────────────────────────────────────

#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include <onnxruntime_cxx_api.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "NativeAlgorithmicSeparator.h"   // separators::NativeStems

namespace autoremix::dsp::separators {

inline constexpr int   kModelWindowSamples = 343'980;   // 7.8 s @ 44.1 kHz
inline constexpr int   kModelSampleRate    = 44'100;
inline constexpr float kOverlapFraction    = 0.25f;     // matches htdemucs
inline constexpr int   kSources            = 4;         // vocals/drums/bass/other
inline constexpr int   kModelChannels      = 2;

struct DemucsResult {
    bool         ok = false;
    juce::String error;
    NativeStems  stems;
};

namespace detail {

// Pre-compute a raised-cosine fade-in window of length `n`.
// fade_out[i] = fade_in[n-1-i].
inline std::vector<float> raised_cosine_fade_in(int n)
{
    std::vector<float> w(static_cast<size_t>(n));
    if (n <= 1) {
        if (n == 1) w[0] = 1.0f;
        return w;
    }
    for (int i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(n - 1);
        w[static_cast<size_t>(i)] =
            0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * t));
    }
    return w;
}

// Pack a juce::AudioBuffer<float> (mono or stereo) into the fixed
// [1, 2, kModelWindowSamples] layout the ONNX model expects.
// Pads with zeros if the input is shorter than the window.
inline std::vector<float>
pack_chunk(const juce::AudioBuffer<float>& src, int start_sample)
{
    std::vector<float> out(static_cast<size_t>(kModelChannels)
                           * static_cast<size_t>(kModelWindowSamples), 0.0f);
    const int  src_chans  = src.getNumChannels();
    const int  src_total  = src.getNumSamples();
    const int  copy_len   = std::min(kModelWindowSamples, src_total - start_sample);

    for (int c = 0; c < kModelChannels; ++c) {
        const int src_c = (src_chans == 1) ? 0 : std::min(c, src_chans - 1);
        if (copy_len > 0) {
            std::memcpy(
                out.data() + static_cast<size_t>(c) * kModelWindowSamples,
                src.getReadPointer(src_c, start_sample),
                static_cast<size_t>(copy_len) * sizeof(float));
        }
    }
    return out;
}

// Overlap-add a chunk's output into `dst`, applying fade-in / fade-out
// on the leading and trailing overlap regions.
inline void
overlap_add_chunk(juce::AudioBuffer<float>& dst,
                  int dst_start_sample,
                  const float* chunk,            // [kModelChannels, kModelWindowSamples]
                  bool apply_fade_in,
                  bool apply_fade_out,
                  const std::vector<float>& fade)
{
    const int fade_n   = static_cast<int>(fade.size());
    const int dst_chans = dst.getNumChannels();
    const int dst_total = dst.getNumSamples();

    for (int c = 0; c < kModelChannels; ++c) {
        if (c >= dst_chans) break;
        float* w = dst.getWritePointer(c);
        const float* src = chunk + static_cast<size_t>(c) * kModelWindowSamples;

        for (int i = 0; i < kModelWindowSamples; ++i) {
            const int out_idx = dst_start_sample + i;
            if (out_idx < 0 || out_idx >= dst_total) continue;

            float gain = 1.0f;
            if (apply_fade_in  && i < fade_n)
                gain *= fade[static_cast<size_t>(i)];
            if (apply_fade_out && i >= kModelWindowSamples - fade_n) {
                const int fi = kModelWindowSamples - 1 - i;
                gain *= fade[static_cast<size_t>(fi)];
            }
            w[out_idx] += src[i] * gain;
        }
    }
}

} // namespace detail

inline DemucsResult
separate_demucs(const juce::AudioBuffer<float>& input,
                double                          sample_rate,
                const juce::File&               model_path,
                std::function<void(double)>     progress_cb = {})
{
    DemucsResult result;

    if (std::abs(sample_rate - kModelSampleRate) > 0.5) {
        result.error = "NativeDemucsSeparator requires 44.1 kHz input "
                       "(got " + juce::String(sample_rate) + " Hz). "
                       "Resample upstream.";
        return result;
    }
    if (!model_path.existsAsFile()) {
        result.error = "Model file not found: " + model_path.getFullPathName();
        return result;
    }
    if (input.getNumSamples() <= 0) {
        result.error = "Empty input buffer";
        return result;
    }

    const int N = input.getNumSamples();

    // Allocate output stems. Each: 2 channels, N samples, zeroed.
    auto make_stem = [N]() {
        juce::AudioBuffer<float> b(kModelChannels, N);
        b.clear();
        return b;
    };
    juce::AudioBuffer<float> vocals = make_stem();
    juce::AudioBuffer<float> drums  = make_stem();
    juce::AudioBuffer<float> bass   = make_stem();
    juce::AudioBuffer<float> other  = make_stem();
    // ONNX output source order matches demucs.htdemucs.model.sources:
    //   index 0 = drums, 1 = bass, 2 = other, 3 = vocals
    // (Verified empirically — earlier ordering [vocals, drums, bass, other]
    // produced cross-routed stems.)
    std::array<juce::AudioBuffer<float>*, kSources> stems_out =
        { &drums, &bass, &other, &vocals };

    // Chunk schedule.
    const int hop = static_cast<int>(
        kModelWindowSamples * (1.0f - kOverlapFraction));   // 257985
    const int overlap_n = kModelWindowSamples - hop;
    int num_chunks = 1;
    if (N > kModelWindowSamples) {
        num_chunks = 1 + static_cast<int>(
            std::ceil(static_cast<double>(N - kModelWindowSamples)
                      / static_cast<double>(hop)));
    }

    const auto fade = detail::raised_cosine_fade_in(overlap_n);

    try {
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "autoremix-demucs");
        Ort::SessionOptions opts;
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        opts.SetIntraOpNumThreads(0);     // ORT picks a sensible default

        Ort::Session session(
            env, model_path.getFullPathName().toRawUTF8(), opts);

        Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault);

        const std::array<int64_t, 3> input_shape =
            { 1, kModelChannels, kModelWindowSamples };

        // Pre-fetch input/output names (ORT 1.17 returns AllocatedStringPtr).
        Ort::AllocatorWithDefaultOptions allocator;
        Ort::AllocatedStringPtr  in_name_ptr  = session.GetInputNameAllocated (0, allocator);
        Ort::AllocatedStringPtr  out_name_ptr = session.GetOutputNameAllocated(0, allocator);
        const char* in_names [] = { in_name_ptr.get() };
        const char* out_names[] = { out_name_ptr.get() };

        std::vector<float> chunk_buf;  // reusable, [2, kModelWindowSamples]

        for (int idx = 0; idx < num_chunks; ++idx) {
            const int start = idx * hop;

            chunk_buf = detail::pack_chunk(input, start);

            Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
                mem_info,
                chunk_buf.data(),
                chunk_buf.size(),
                input_shape.data(),
                input_shape.size());

            auto outputs = session.Run(
                Ort::RunOptions{nullptr},
                in_names, &input_tensor, 1,
                out_names, 1);

            if (outputs.empty() || !outputs[0].IsTensor()) {
                result.error = "ORT returned no tensor on chunk "
                               + juce::String(idx);
                return result;
            }
            const float* raw = outputs[0].GetTensorData<float>();

            const bool fade_in  = idx > 0;
            const bool fade_out = idx < num_chunks - 1;

            // Output layout: [1, 4, 2, kModelWindowSamples].
            // Stride per source = kModelChannels * kModelWindowSamples.
            const std::size_t per_src =
                static_cast<std::size_t>(kModelChannels)
                * static_cast<std::size_t>(kModelWindowSamples);

            for (std::size_t s = 0; s < static_cast<std::size_t>(kSources); ++s) {
                const float* src = raw + s * per_src;
                detail::overlap_add_chunk(
                    *stems_out[s], start, src, fade_in, fade_out, fade);
            }

            if (progress_cb)
                progress_cb(static_cast<double>(idx + 1)
                            / static_cast<double>(num_chunks));
        }
    }
    catch (const Ort::Exception& e) {
        result.error = juce::String("ORT exception: ") + e.what();
        return result;
    }
    catch (const std::exception& e) {
        result.error = juce::String("std exception: ") + e.what();
        return result;
    }

    result.stems.vocals = std::move(vocals);
    result.stems.drums  = std::move(drums);
    result.stems.bass   = std::move(bass);
    result.stems.other  = std::move(other);
    result.ok = true;
    return result;
}

} // namespace autoremix::dsp::separators
