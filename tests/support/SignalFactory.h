#pragma once

// Deterministic synthetic test signals for AutoRemix tests.
// Every function is pure: same arguments -> bit-identical output. Anything
// random takes an explicit seed.

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace autoremix::test {

/** Sample index of click n for a click track at `bpm` and sample rate `sr`. */
inline int64_t click_position(int64_t n, double bpm, double sr)
{
    return (int64_t) std::llround((double) n * 60.0 / bpm * sr);
}

/** Click track: a short exponentially-decaying impulse at every beat.
 *  The first sample of each click is the peak (1.0, or 0.5 on non-downbeats
 *  when accent_downbeat is true, i.e. downbeats are +6 dB). Bars are 4/4. */
inline juce::AudioBuffer<float> click_track(double bpm, double seconds, double sr,
                                            int channels = 2, int click_len = 64,
                                            bool accent_downbeat = true)
{
    const int total = (int) std::llround(seconds * sr);
    juce::AudioBuffer<float> buf(channels, total);
    buf.clear();
    for (int64_t n = 0;; ++n) {
        const int64_t start = click_position(n, bpm, sr);
        if (start >= total) break;
        const float peak = (!accent_downbeat || n % 4 == 0) ? 1.0f : 0.5f;
        for (int i = 0; i < click_len && start + i < total; ++i) {
            const float v = peak * std::exp(-6.0f * (float) i / (float) click_len);
            for (int ch = 0; ch < channels; ++ch)
                buf.setSample(ch, (int) (start + i), v);
        }
    }
    return buf;
}

/** Sum of sines at the given frequencies, normalised so the peak is <= amp. */
inline juce::AudioBuffer<float> sine_chord(const std::vector<double>& freqs_hz, double seconds,
                                           double sr, float amp = 0.5f, int channels = 2)
{
    const int total = (int) std::llround(seconds * sr);
    juce::AudioBuffer<float> buf(channels, total);
    buf.clear();
    if (freqs_hz.empty()) return buf;
    const double scale = (double) amp / (double) freqs_hz.size();
    constexpr double twoPi = 6.283185307179586476925286766559;
    for (int i = 0; i < total; ++i) {
        double v = 0.0;
        for (double f : freqs_hz) v += std::sin(twoPi * f * (double) i / sr);
        for (int ch = 0; ch < channels; ++ch)
            buf.setSample(ch, i, (float) (v * scale));
    }
    return buf;
}

/** All-zero buffer. */
inline juce::AudioBuffer<float> silence(double seconds, double sr, int channels = 2)
{
    juce::AudioBuffer<float> buf(channels, (int) std::llround(seconds * sr));
    buf.clear();
    return buf;
}

/** Concatenate buffers (channel count = max of inputs; missing channels are
 *  zero). Returns the joined buffer and the start sample of every section
 *  after the first (the boundaries). */
inline std::pair<juce::AudioBuffer<float>, std::vector<int>>
splice_sections(const std::vector<juce::AudioBuffer<float>>& parts)
{
    int channels = 0, total = 0;
    for (const auto& p : parts) {
        channels = std::max(channels, p.getNumChannels());
        total += p.getNumSamples();
    }
    juce::AudioBuffer<float> out(std::max(channels, 1), total);
    out.clear();
    std::vector<int> boundaries;
    int pos = 0;
    for (size_t k = 0; k < parts.size(); ++k) {
        if (k > 0) boundaries.push_back(pos);
        const auto& p = parts[k];
        for (int ch = 0; ch < p.getNumChannels(); ++ch)
            out.copyFrom(ch, pos, p, ch, 0, p.getNumSamples());
        pos += p.getNumSamples();
    }
    return { std::move(out), std::move(boundaries) };
}

/** Pink-ish noise (Paul Kellet's economy filter over a seeded LCG). */
inline juce::AudioBuffer<float> pink_noise(double seconds, double sr, uint32_t seed,
                                           float amp = 0.25f, int channels = 2)
{
    const int total = (int) std::llround(seconds * sr);
    juce::AudioBuffer<float> buf(channels, total);
    uint32_t state = seed ? seed : 1u;
    double b0 = 0, b1 = 0, b2 = 0;
    for (int i = 0; i < total; ++i) {
        state = state * 1664525u + 1013904223u;
        const double white = ((double) (state >> 8) / (double) (1u << 24)) * 2.0 - 1.0;
        b0 = 0.99765 * b0 + white * 0.0990460;
        b1 = 0.96300 * b1 + white * 0.2965164;
        b2 = 0.57000 * b2 + white * 1.0526913;
        const float v = (float) ((b0 + b1 + b2 + white * 0.1848) * 0.2 * amp);
        for (int ch = 0; ch < channels; ++ch) buf.setSample(ch, i, v);
    }
    return buf;
}

/** Bytes of an ID3v2.3 tag holding one GEOB frame that ends right after the
 *  (empty) filename terminator, i.e. the frame has no description bytes at
 *  all. Reproduces REVIEW R-05 (description length underflow). */
inline std::vector<uint8_t> truncated_geob_id3()
{
    std::vector<uint8_t> frame_data;
    frame_data.push_back(0x00);                                   // text encoding
    const std::string mime = "application/octet-stream";
    frame_data.insert(frame_data.end(), mime.begin(), mime.end());
    frame_data.push_back(0x00);                                   // MIME terminator
    frame_data.push_back(0x00);                                   // empty filename terminator
    // (description intentionally missing)

    std::vector<uint8_t> frame = { 'G', 'E', 'O', 'B' };
    const uint32_t fs = (uint32_t) frame_data.size();
    frame.push_back((uint8_t) (fs >> 24)); frame.push_back((uint8_t) (fs >> 16));
    frame.push_back((uint8_t) (fs >> 8));  frame.push_back((uint8_t) fs);
    frame.push_back(0x00); frame.push_back(0x00);                 // flags
    frame.insert(frame.end(), frame_data.begin(), frame_data.end());

    const uint32_t ts = (uint32_t) frame.size();                  // tag body size, syncsafe
    std::vector<uint8_t> tag = { 'I', 'D', '3', 0x03, 0x00, 0x00,
                                 (uint8_t) ((ts >> 21) & 0x7f), (uint8_t) ((ts >> 14) & 0x7f),
                                 (uint8_t) ((ts >> 7) & 0x7f),  (uint8_t) (ts & 0x7f) };
    tag.insert(tag.end(), frame.begin(), frame.end());
    return tag;
}

} // namespace autoremix::test
