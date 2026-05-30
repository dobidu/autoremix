#pragma once
// Phase 23-02 — Native audio analysis suite.
//
// Header-only port of sidecar/server/remix/analysis.py + parts of base.py.
// All functions are inline so multiple TUs can include without ODR violations.
//
//   detect_bpm           (replaces librosa.beat.beat_track tempo output)
//   detect_beats         (replaces librosa.beat.beat_track beat output)
//   detect_onsets        (replaces librosa.onset.onset_detect)
//   detect_bars          (every Nth beat — port of analysis.detect_bars)
//   detect_energy_gates  (RMS-threshold gating — port of analysis.detect_energy_gates)
//   detect_key           (Krumhansl-Schmuckler chroma correlation)
//   semitone_delta       (shortest signed semitone path, mode-ignored)
//   normalize_lufs       (EBU R128 ITU-R BS.1770-4; in-place gain)
//
// No external lib. Uses juce::dsp::FFT + WindowingFunction only.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace autoremix::dsp::analysis {

// ───────────────────────────────────────────────────────────────────────────
// Constants
// ───────────────────────────────────────────────────────────────────────────

inline constexpr int kFftSize = 2048;
inline constexpr int kHop     = 512;

inline constexpr std::array<const char*, 12> kNoteNames = {
    "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
};

// Krumhansl-Schmuckler key profiles (verbatim from analysis.py)
inline constexpr std::array<float, 12> kMajorProfile = {
    6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f,
    2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
};
inline constexpr std::array<float, 12> kMinorProfile = {
    6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f,
    2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
};

// EBU R128 K-weighting biquads at 48 kHz (ITU-R BS.1770-4 reference).
// Coefficients: [b0, b1, b2, a1, a2] with a0 normalized to 1.
inline constexpr std::array<double, 5> kKPre = {
    1.53512485958697, -2.69169618940638, 1.19839281085285,
   -1.69065929318241, 0.73248077421585
};
inline constexpr std::array<double, 5> kKRlb = {
    1.0, -2.0, 1.0,
   -1.99004745483398, 0.99007225036621
};

// ───────────────────────────────────────────────────────────────────────────
// SongStructure — Phase 31-01
// Runtime structural description of a song produced by analyze_structure().
// Used by the template engine to map arrangement slots to concrete bar indices.
// ───────────────────────────────────────────────────────────────────────────

struct SongStructure {
    // ── Rhythm ────────────────────────────────────────────────────────────
    double              bpm          = 0.0;
    std::vector<double> beat_times;    // seconds at each detected beat
    std::vector<double> bar_times;     // seconds at each bar (every 4 beats)
    std::vector<double> phrase_times;  // seconds at each phrase (every 4 bars)

    // ── Energy (parallel to bar_times) ────────────────────────────────────
    std::vector<float>  energy_per_bar;  // RMS energy per bar, normalized [0, 1]
    std::vector<int>    energy_peaks;    // indices into bar_times of local maxima

    // ── Tonal ─────────────────────────────────────────────────────────────
    std::string key;

    // ── Timbre (parallel to bar_times, filled in 31-02) ───────────────────
    std::vector<float>  brightness_per_bar;       // spectral centroid, [0, 1]
    std::vector<float>  vocal_presence_per_bar;   // 0=instrumental, 1=vocal

    // ── Sections (filled in 31-02) ────────────────────────────────────────
    std::vector<int>    section_boundaries;  // bar indices of structural transitions

    // ── Meta ──────────────────────────────────────────────────────────────
    double duration_seconds = 0.0;
    int    num_bars         = 0;

    // Helpers
    int bar_index_at(double time_sec) const noexcept
    {
        if (bar_times.empty()) return -1;
        auto it = std::lower_bound(bar_times.begin(), bar_times.end(), time_sec);
        if (it == bar_times.end()) return (int) bar_times.size() - 1;
        return (int) std::distance(bar_times.begin(), it);
    }

    double bar_time_at(int bar_idx) const noexcept
    {
        if (bar_idx < 0 || bar_idx >= (int) bar_times.size()) return -1.0;
        return bar_times[(size_t) bar_idx];
    }
};

// ───────────────────────────────────────────────────────────────────────────
// Private helpers (anonymous-namespace via static inline)
// ───────────────────────────────────────────────────────────────────────────

inline std::vector<float> to_mono(const juce::AudioBuffer<float>& in)
{
    const int n  = in.getNumSamples();
    const int ch = in.getNumChannels();
    std::vector<float> mono((size_t) n, 0.0f);
    if (ch == 0 || n == 0) return mono;
    for (int c = 0; c < ch; ++c) {
        const float* p = in.getReadPointer(c);
        for (int i = 0; i < n; ++i) mono[(size_t) i] += p[i];
    }
    const float inv = 1.0f / (float) ch;
    for (auto& s : mono) s *= inv;
    return mono;
}

// STFT magnitudes. Returns [frames][bins] where bins = fftSize/2 + 1.
inline std::vector<std::vector<float>>
stft_magnitudes(const std::vector<float>& mono, int fftSize, int hop)
{
    const int order   = (int) std::log2((double) fftSize);
    const int n       = (int) mono.size();
    if (n < fftSize) return {};

    juce::dsp::FFT fft(order);
    juce::dsp::WindowingFunction<float> window(
        (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann);

    const int numFrames = 1 + (n - fftSize) / hop;
    const int bins      = fftSize / 2 + 1;

    std::vector<std::vector<float>> out((size_t) numFrames,
                                         std::vector<float>((size_t) bins, 0.0f));
    std::vector<float> buf((size_t) fftSize * 2, 0.0f);

    for (int f = 0; f < numFrames; ++f) {
        const int start = f * hop;
        std::fill(buf.begin(), buf.end(), 0.0f);
        for (int i = 0; i < fftSize; ++i)
            buf[(size_t) i] = mono[(size_t) (start + i)];
        window.multiplyWithWindowingTable(buf.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform(buf.data());
        for (int b = 0; b < bins; ++b)
            out[(size_t) f][(size_t) b] = buf[(size_t) b];
    }
    return out;
}

// Spectral flux onset envelope: sum of positive frame-to-frame
// magnitude differences. Returns one value per frame (first = 0).
inline std::vector<float>
onset_envelope(const std::vector<std::vector<float>>& mag)
{
    const size_t F = mag.size();
    std::vector<float> env(F, 0.0f);
    if (F < 2) return env;
    const size_t B = mag[0].size();
    for (size_t f = 1; f < F; ++f) {
        float flux = 0.0f;
        for (size_t b = 0; b < B; ++b) {
            const float diff = mag[f][b] - mag[f - 1][b];
            if (diff > 0.0f) flux += diff;
        }
        env[f] = flux;
    }
    // Normalize to [0, 1] for downstream peak picking
    const float maxv = *std::max_element(env.begin(), env.end());
    if (maxv > 1e-9f)
        for (auto& v : env) v /= maxv;
    return env;
}

// Single biquad direct-form II transposed
struct Biquad {
    double b0, b1, b2, a1, a2;
    double z1 = 0.0, z2 = 0.0;

    explicit Biquad(const std::array<double, 5>& c)
        : b0(c[0]), b1(c[1]), b2(c[2]), a1(c[3]), a2(c[4]) {}

    inline float process(float x) noexcept {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
};

// ───────────────────────────────────────────────────────────────────────────
// Public API — rhythm
// ───────────────────────────────────────────────────────────────────────────

inline double detect_bpm(const juce::AudioBuffer<float>& audio, double sr)
{
    auto mono = to_mono(audio);
    if ((int) mono.size() < kFftSize * 4) return 120.0;

    auto mag = stft_magnitudes(mono, kFftSize, kHop);
    auto env = onset_envelope(mag);
    if (env.size() < 16) return 120.0;

    // Mean-subtract before autocorrelation
    const float mean = std::accumulate(env.begin(), env.end(), 0.0f) / (float) env.size();
    for (auto& v : env) v -= mean;

    // Lag range corresponds to 60–200 BPM in frame units
    const double framesPerSec = sr / (double) kHop;
    const int minLag = (int) std::floor(framesPerSec * 60.0 / 200.0);  // 200 BPM
    const int maxLag = (int) std::ceil (framesPerSec * 60.0 /  60.0);  // 60  BPM

    double bestLag = (double) minLag;
    double bestVal = -1e30;
    const int F = (int) env.size();
    for (int lag = minLag; lag <= maxLag && lag < F; ++lag) {
        double sum = 0.0;
        for (int i = 0; i + lag < F; ++i)
            sum += (double) env[(size_t) i] * (double) env[(size_t) (i + lag)];
        if (sum > bestVal) { bestVal = sum; bestLag = lag; }
    }

    if (bestLag <= 0.0) return 120.0;
    double bpm = framesPerSec * 60.0 / bestLag;
    if (bpm < 60.0  || std::isnan(bpm)) bpm = 60.0;
    if (bpm > 200.0) bpm = 200.0;
    return bpm;
}

inline std::vector<double>
detect_beats(const juce::AudioBuffer<float>& audio, double sr)
{
    auto mono = to_mono(audio);
    const int N = (int) mono.size();
    if (N < kFftSize * 4) return {0.0};

    auto mag = stft_magnitudes(mono, kFftSize, kHop);
    auto env = onset_envelope(mag);
    if (env.size() < 8) return {0.0};

    const double bpm           = detect_bpm(audio, sr);
    const double secondsPerBeat = 60.0 / bpm;
    const int    framesPerSec  = (int) std::round(sr / (double) kHop);
    const int    beatStride    = std::max(1, (int) std::round(secondsPerBeat * framesPerSec));

    // Phase: pick the strongest peak in the first 2 seconds of envelope
    const int windowEnd = std::min((int) env.size(), framesPerSec * 2);
    int   phase = 0;
    float peak  = -1.0f;
    for (int i = 0; i < windowEnd; ++i)
        if (env[(size_t) i] > peak) { peak = env[(size_t) i]; phase = i; }

    // Snap each predicted beat to nearest local peak within ±50ms
    const int snapRadius = std::max(1, (int) std::round(0.05 * (double) framesPerSec));
    std::vector<double> beats;
    for (int idx = phase; idx < (int) env.size(); idx += beatStride) {
        const int lo = std::max(0, idx - snapRadius);
        const int hi = std::min((int) env.size() - 1, idx + snapRadius);
        int   bestIdx = idx;
        float bestVal = env[(size_t) idx];
        for (int j = lo; j <= hi; ++j)
            if (env[(size_t) j] > bestVal) { bestVal = env[(size_t) j]; bestIdx = j; }
        beats.push_back((double) bestIdx * (double) kHop / sr);
    }
    if (beats.empty()) beats.push_back(0.0);
    return beats;
}

inline std::vector<double>
detect_onsets(const juce::AudioBuffer<float>& audio, double sr,
              double min_gap_ms = 80.0, double threshold = 0.3)
{
    auto mono = to_mono(audio);
    if ((int) mono.size() < kFftSize * 2) return {0.0};

    auto mag = stft_magnitudes(mono, kFftSize, kHop);
    auto env = onset_envelope(mag);
    if (env.size() < 4) return {0.0};

    const double framesPerSec = sr / (double) kHop;
    const int    minGapFrames = std::max(1, (int) std::round(min_gap_ms / 1000.0 * framesPerSec));

    std::vector<double> onsets;
    int  lastIdx = -1000000;
    const int F = (int) env.size();
    for (int i = 1; i < F - 1; ++i) {
        // Local maximum + above threshold (envelope already normalized to [0,1])
        if (env[(size_t) i] >= (float) threshold
            && env[(size_t) i] > env[(size_t) (i - 1)]
            && env[(size_t) i] > env[(size_t) (i + 1)]
            && (i - lastIdx) >= minGapFrames)
        {
            onsets.push_back((double) i * (double) kHop / sr);
            lastIdx = i;
        }
    }
    if (onsets.empty()) onsets.push_back(0.0);
    return onsets;
}

inline std::vector<double>
detect_bars(const juce::AudioBuffer<float>& audio, double sr,
            int beats_per_bar = 4)
{
    auto beats = detect_beats(audio, sr);
    std::vector<double> bars;
    for (size_t i = 0; i < beats.size(); i += (size_t) beats_per_bar)
        bars.push_back(beats[i]);
    return bars;
}

// ───────────────────────────────────────────────────────────────────────────
// Public API — tonal
// ───────────────────────────────────────────────────────────────────────────

inline std::array<float, 12>
chroma_vector(const juce::AudioBuffer<float>& audio, double sr)
{
    auto mono = to_mono(audio);
    std::array<float, 12> chroma{};
    if ((int) mono.size() < kFftSize) return chroma;

    auto mag = stft_magnitudes(mono, kFftSize, kHop);
    if (mag.empty()) return chroma;

    const int  bins      = (int) mag[0].size();
    const auto fft       = (double) kFftSize;
    const auto refA4     = 440.0;

    // Precompute bin → pitch class
    std::vector<int> binToPc((size_t) bins, -1);
    for (int b = 1; b < bins; ++b) {  // skip DC
        const double freq = (double) b * sr / fft;
        if (freq <= 0.0) continue;
        const double midi = 69.0 + 12.0 * std::log2(freq / refA4);
        if (midi < 0.0 || midi > 127.0) continue;
        const int pc = ((int) std::lround(midi) % 12 + 12) % 12;
        binToPc[(size_t) b] = pc;
    }

    for (const auto& frame : mag)
        for (int b = 1; b < bins; ++b) {
            const int pc = binToPc[(size_t) b];
            if (pc >= 0) chroma[(size_t) pc] += frame[(size_t) b];
        }

    // L1 normalize
    float sum = 0.0f;
    for (float v : chroma) sum += v;
    if (sum > 1e-9f)
        for (auto& v : chroma) v /= sum;
    return chroma;
}

inline std::string
detect_key(const juce::AudioBuffer<float>& audio, double sr)
{
    auto chroma = chroma_vector(audio, sr);

    auto correlate = [](const std::array<float, 12>& a, const std::array<float, 12>& b) {
        // Pearson correlation
        float ma = 0.0f, mb = 0.0f;
        for (size_t i = 0; i < 12; ++i) { ma += a[i]; mb += b[i]; }
        ma /= 12.0f; mb /= 12.0f;
        float num = 0.0f, da = 0.0f, db = 0.0f;
        for (size_t i = 0; i < 12; ++i) {
            const float da_i = a[i] - ma;
            const float db_i = b[i] - mb;
            num += da_i * db_i;
            da  += da_i * da_i;
            db  += db_i * db_i;
        }
        const float den = std::sqrt(da * db);
        return den > 1e-9f ? num / den : -1.0f;
    };

    float       bestCorr = -1e9f;
    std::string bestKey  = "C";
    for (int root = 0; root < 12; ++root) {
        std::array<float, 12> shiftedMajor{};
        std::array<float, 12> shiftedMinor{};
        for (int i = 0; i < 12; ++i) {
            shiftedMajor[(size_t) ((i + root) % 12)] = kMajorProfile[(size_t) i];
            shiftedMinor[(size_t) ((i + root) % 12)] = kMinorProfile[(size_t) i];
        }
        const float cM = correlate(chroma, shiftedMajor);
        const float cm = correlate(chroma, shiftedMinor);
        if (cM > bestCorr) { bestCorr = cM; bestKey = kNoteNames[(size_t) root]; }
        if (cm > bestCorr) { bestCorr = cm; bestKey = std::string(kNoteNames[(size_t) root]) + "m"; }
    }
    return bestKey;
}

inline int semitone_delta(const std::string& key_from, const std::string& key_to)
{
    auto rootIndex = [](const std::string& k) -> int {
        std::string s = k;
        if (s.size() > 1 && s.back() == 'm') s.pop_back();
        for (int i = 0; i < 12; ++i)
            if (s == kNoteNames[(size_t) i]) return i;
        return -1;
    };
    const int src = rootIndex(key_from);
    const int dst = rootIndex(key_to);
    if (src < 0 || dst < 0) return 0;
    int raw = ((dst - src) % 12 + 12) % 12;  // 0..11
    if (raw > 6) raw -= 12;
    return raw;
}

// ───────────────────────────────────────────────────────────────────────────
// Public API — loudness / gating
// ───────────────────────────────────────────────────────────────────────────

inline std::vector<uint8_t>
detect_energy_gates(const juce::AudioBuffer<float>& audio, double sr,
                    double threshold_db = -20.0, double hold_ms = 50.0)
{
    auto mono = to_mono(audio);
    const int N = (int) mono.size();
    std::vector<uint8_t> mask((size_t) N, 0u);
    if (N == 0) return mask;

    const int hop = 512;
    const int nBlocks = std::max(1, N / hop);
    const int holdBlocks = std::max(1, (int) std::round(hold_ms / 1000.0 * sr / (double) hop));

    std::vector<uint8_t> frameMask((size_t) nBlocks, 0u);
    for (int f = 0; f < nBlocks; ++f) {
        const int start = f * hop;
        const int end   = std::min(start + hop, N);
        float sumSq = 0.0f;
        for (int i = start; i < end; ++i) sumSq += mono[(size_t) i] * mono[(size_t) i];
        const float rms   = std::sqrt(sumSq / (float) std::max(1, end - start));
        const float rmsDb = 20.0f * std::log10(rms + 1e-9f);
        frameMask[(size_t) f] = (rmsDb >= (float) threshold_db) ? 1u : 0u;
    }

    // Apply hold: set holdBlocks following each true block to true
    std::vector<uint8_t> held = frameMask;
    for (int f = 0; f < nBlocks; ++f) {
        if (frameMask[(size_t) f]) {
            const int upto = std::min(nBlocks, f + holdBlocks);
            for (int g = f; g < upto; ++g) held[(size_t) g] = 1u;
        }
    }

    // Upsample to sample-level mask
    for (int i = 0; i < N; ++i) {
        const int f = std::min(nBlocks - 1, i / hop);
        mask[(size_t) i] = held[(size_t) f];
    }
    return mask;
}

// Compute EBU R128 integrated loudness in LUFS.
// Returns NaN if signal too short or all-zero.
inline double integrated_loudness(const juce::AudioBuffer<float>& audio, double sr)
{
    const int ch = std::min(audio.getNumChannels(), 2);
    const int N  = audio.getNumSamples();
    if (ch == 0 || N == 0) return std::nan("");

    const int windowSamples = (int) std::round(0.400 * sr);
    if (N < windowSamples) return std::nan("");

    // K-weighting filters (one per channel)
    std::vector<Biquad> pre, rlb;
    pre.reserve((size_t) ch);
    rlb.reserve((size_t) ch);
    for (int c = 0; c < ch; ++c) { pre.emplace_back(kKPre); rlb.emplace_back(kKRlb); }

    // Apply K-weighting in place to a working copy
    std::vector<std::vector<float>> w((size_t) ch, std::vector<float>((size_t) N, 0.0f));
    for (int c = 0; c < ch; ++c) {
        const float* p = audio.getReadPointer(c);
        for (int i = 0; i < N; ++i) {
            const float x = pre[(size_t) c].process(p[i]);
            w[(size_t) c][(size_t) i] = rlb[(size_t) c].process(x);
        }
    }

    // 400ms windows, 75% overlap (hop = 100ms)
    const int hop = (int) std::round(0.100 * sr);
    const int nWindows = 1 + (N - windowSamples) / hop;

    // Channel weights: mono/stereo = 1.0; surround channels would have ITU
    // weights, but we only support stereo.
    std::vector<double> blockLoudness;
    blockLoudness.reserve((size_t) nWindows);
    for (int b = 0; b < nWindows; ++b) {
        const int start = b * hop;
        double meanSq = 0.0;
        for (int c = 0; c < ch; ++c) {
            double sumSq = 0.0;
            for (int i = 0; i < windowSamples; ++i) {
                const double x = (double) w[(size_t) c][(size_t) (start + i)];
                sumSq += x * x;
            }
            meanSq += sumSq / (double) windowSamples;
        }
        if (meanSq > 0.0)
            blockLoudness.push_back(-0.691 + 10.0 * std::log10(meanSq));
    }
    if (blockLoudness.empty()) return std::nan("");

    // Absolute gate: keep blocks >= -70 LUFS
    std::vector<double> kept;
    for (double l : blockLoudness) if (l >= -70.0) kept.push_back(l);
    if (kept.empty()) return std::nan("");

    auto meanOf = [](const std::vector<double>& v) {
        double s = 0.0; for (double x : v) s += x; return s / (double) v.size();
    };
    const double absMean = meanOf(kept);

    // Relative gate: keep blocks >= (absMean - 10)
    std::vector<double> kept2;
    const double relThr = absMean - 10.0;
    for (double l : kept) if (l >= relThr) kept2.push_back(l);
    if (kept2.empty()) return absMean;
    return meanOf(kept2);
}

inline void
normalize_lufs(juce::AudioBuffer<float>& audio, double sr,
               double target_lufs = -14.0)
{
    const int N = audio.getNumSamples();
    if (N == 0) return;

    auto peakNorm = [&](float dbTarget) {
        float peak = 0.0f;
        for (int c = 0; c < audio.getNumChannels(); ++c) {
            const float* p = audio.getReadPointer(c);
            for (int i = 0; i < N; ++i) {
                const float a = std::abs(p[i]);
                if (a > peak) peak = a;
            }
        }
        if (peak < 1e-9f) return;
        const float gain = std::pow(10.0f, dbTarget / 20.0f) / peak;
        audio.applyGain(gain);
    };

    const double measured = integrated_loudness(audio, sr);
    if (!std::isfinite(measured)) {
        peakNorm(-1.0f);
        return;
    }

    const double gainDb = target_lufs - measured;
    const double gain   = std::pow(10.0, gainDb / 20.0);
    audio.applyGain((float) gain);

    // Safety: pull back if peak > -0.5 dBFS after gain
    float peak = 0.0f;
    for (int c = 0; c < audio.getNumChannels(); ++c) {
        const float* p = audio.getReadPointer(c);
        for (int i = 0; i < N; ++i) {
            const float a = std::abs(p[i]);
            if (a > peak) peak = a;
        }
    }
    const float peakLimit = std::pow(10.0f, -0.5f / 20.0f);
    if (peak > peakLimit && peak > 1e-9f)
        audio.applyGain(peakLimit / peak);
}

// ───────────────────────────────────────────────────────────────────────────
// Public API — structural analysis (Phase 31-01)
// ───────────────────────────────────────────────────────────────────────────

// RMS energy per bar window, normalized to [0, 1].
inline std::vector<float>
energy_per_bar(const juce::AudioBuffer<float>& audio, double sr,
               const std::vector<double>& bar_times)
{
    const int N = audio.getNumSamples();
    const int ch = audio.getNumChannels();
    std::vector<float> result;
    result.reserve(bar_times.size());
    if (bar_times.empty() || N == 0 || ch == 0) return result;

    auto mono = to_mono(audio);

    for (size_t i = 0; i < bar_times.size(); ++i) {
        const int start = std::min(N - 1, (int) std::round(bar_times[i] * sr));
        const int end   = (i + 1 < bar_times.size())
            ? std::min(N, (int) std::round(bar_times[i + 1] * sr))
            : N;
        const int len = std::max(1, end - start);

        float sumSq = 0.0f;
        for (int s = start; s < end; ++s)
            sumSq += mono[(size_t) s] * mono[(size_t) s];
        result.push_back(std::sqrt(sumSq / (float) len));
    }

    // Normalize to [0, 1]
    const float maxv = *std::max_element(result.begin(), result.end());
    if (maxv > 1e-9f)
        for (auto& v : result) v /= maxv;
    return result;
}

// Local energy maxima separated by at least min_sep_bars.
inline std::vector<int>
energy_peaks(const std::vector<float>& energy, int min_sep_bars = 4)
{
    std::vector<int> peaks;
    const int N = (int) energy.size();
    if (N < 3) return peaks;

    int last = -1000;
    for (int i = 1; i < N - 1; ++i) {
        if (energy[(size_t) i] > energy[(size_t) (i - 1)]
            && energy[(size_t) i] > energy[(size_t) (i + 1)]
            && (i - last) >= min_sep_bars)
        {
            peaks.push_back(i);
            last = i;
        }
    }
    return peaks;
}

// Phrase positions: every phrase_bars bars starting at bar 0.
inline std::vector<double>
phrase_times(const std::vector<double>& bar_times, int phrase_bars = 4)
{
    std::vector<double> phrases;
    if (bar_times.empty() || phrase_bars < 1) return phrases;
    for (size_t i = 0; i < bar_times.size(); i += (size_t) phrase_bars)
        phrases.push_back(bar_times[i]);
    return phrases;
}

// Spectral centroid (brightness) per bar, normalized to [0, 1].
// Center of mass of the FFT spectrum, averaged across frames in each bar.
inline std::vector<float>
spectral_centroid_per_bar(const juce::AudioBuffer<float>& audio, double sr,
                           const std::vector<double>& bar_times)
{
    std::vector<float> result;
    result.reserve(bar_times.size());
    if (bar_times.empty() || audio.getNumSamples() == 0) return result;

    auto mono = to_mono(audio);
    const int N = (int) mono.size();
    const int bins = kFftSize / 2 + 1;

    for (size_t i = 0; i < bar_times.size(); ++i) {
        const int start = std::min(N - 1, (int) std::round(bar_times[i] * sr));
        const int end   = (i + 1 < bar_times.size())
            ? std::min(N, (int) std::round(bar_times[i + 1] * sr))
            : N;

        if (end - start < kFftSize) { result.push_back(0.0f); continue; }

        // Slice mono samples for this bar
        std::vector<float> slice(mono.begin() + start, mono.begin() + end);
        auto mag = stft_magnitudes(slice, kFftSize, kHop);

        if (mag.empty()) { result.push_back(0.0f); continue; }

        float centroid_sum = 0.0f;
        int   frame_count  = 0;
        for (const auto& frame : mag) {
            float num = 0.0f, den = 0.0f;
            for (int b = 1; b < bins; ++b) {
                const float freq = (float) b * (float) sr / (float) kFftSize;
                num += freq * frame[(size_t) b];
                den += frame[(size_t) b];
            }
            if (den > 1e-9f) { centroid_sum += num / den; ++frame_count; }
        }
        result.push_back(frame_count > 0 ? centroid_sum / (float) frame_count : 0.0f);
    }

    // Normalize to [0, 1]
    const float maxv = result.empty() ? 0.0f : *std::max_element(result.begin(), result.end());
    if (maxv > 1e-9f)
        for (auto& v : result) v /= maxv;
    return result;
}

// Vocal presence heuristic per bar: ratio of 1–4 kHz band energy to total energy.
// Higher value = more likely to contain vocals.
inline std::vector<float>
vocal_presence_per_bar(const juce::AudioBuffer<float>& audio, double sr,
                        const std::vector<double>& bar_times)
{
    std::vector<float> result;
    result.reserve(bar_times.size());
    if (bar_times.empty() || audio.getNumSamples() == 0) return result;

    // Bandpass around 1000–4000 Hz: HPF at 1kHz then LPF at 4kHz
    auto apply_iir = [](const std::vector<float>& in,
                        juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>> coefs)
    {
        std::vector<float> out = in;
        juce::dsp::IIR::Filter<float> filt;
        filt.coefficients = coefs;
        filt.reset();
        for (auto& s : out) s = filt.processSample(s);
        return out;
    };

    auto mono = to_mono(audio);
    const auto hpf_coefs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, 1000.0f, 0.7f);
    const auto lpf_coefs = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, 4000.0f, 0.7f);
    auto band = apply_iir(apply_iir(mono, hpf_coefs), lpf_coefs);

    const int N = (int) mono.size();
    for (size_t i = 0; i < bar_times.size(); ++i) {
        const int start = std::min(N - 1, (int) std::round(bar_times[i] * sr));
        const int end   = (i + 1 < bar_times.size())
            ? std::min(N, (int) std::round(bar_times[i + 1] * sr))
            : N;
        const int len = std::max(1, end - start);

        if (len < 512) { result.push_back(0.5f); continue; }

        float full_sq = 0.0f, band_sq = 0.0f;
        for (int s = start; s < end; ++s) {
            full_sq += mono[(size_t) s] * mono[(size_t) s];
            band_sq += band[(size_t) s] * band[(size_t) s];
        }
        const float ratio = full_sq > 1e-12f
            ? std::sqrt(band_sq / full_sq)
            : 0.0f;
        result.push_back(juce::jlimit(0.0f, 1.0f, ratio));
    }

    // Normalize relative to max (keeps proportions, maps to [0,1])
    const float maxv = result.empty() ? 0.0f : *std::max_element(result.begin(), result.end());
    if (maxv > 1e-9f)
        for (auto& v : result) v /= maxv;
    return result;
}

// Section boundaries via simplified novelty function.
// Detects structural transitions as peaks in |Δenergy| + |Δbrightness|.
inline std::vector<int>
section_boundaries(const std::vector<float>& energy,
                   const std::vector<float>& brightness,
                   int min_sep_bars = 4)
{
    const int N = (int) std::min(energy.size(), brightness.size());
    if (N < 3) return {};

    // Novelty = frame-to-frame change in energy + brightness
    std::vector<float> novelty((size_t) N, 0.0f);
    for (int i = 1; i < N; ++i) {
        novelty[(size_t) i] = std::abs(energy[(size_t) i]     - energy[(size_t) (i - 1)])
                            + std::abs(brightness[(size_t) i] - brightness[(size_t) (i - 1)]);
    }

    // 3-point moving average to reduce jitter
    std::vector<float> smooth = novelty;
    for (int i = 1; i < N - 1; ++i)
        smooth[(size_t) i] = (novelty[(size_t) (i - 1)] + novelty[(size_t) i]
                              + novelty[(size_t) (i + 1)]) / 3.0f;

    // Threshold: only peaks above mean * 1.2
    float mean = 0.0f;
    for (float v : smooth) mean += v;
    mean /= (float) N;
    const float threshold = mean * 1.2f;

    // Peak pick with minimum separation
    std::vector<int> bounds;
    int last = -1000;
    for (int i = 1; i < N - 1; ++i) {
        if (smooth[(size_t) i] > threshold
            && smooth[(size_t) i] > smooth[(size_t) (i - 1)]
            && smooth[(size_t) i] > smooth[(size_t) (i + 1)]
            && (i - last) >= min_sep_bars)
        {
            bounds.push_back(i);
            last = i;
        }
    }
    return bounds;
}

// Complete entry point — all SongStructure fields populated.
inline SongStructure
analyze_structure(const juce::AudioBuffer<float>& audio, double sr)
{
    SongStructure s;
    if (audio.getNumSamples() == 0 || sr <= 0.0) return s;

    s.duration_seconds       = audio.getNumSamples() / sr;
    s.bpm                    = detect_bpm(audio, sr);
    s.key                    = detect_key(audio, sr);
    s.beat_times             = detect_beats(audio, sr);
    s.bar_times              = detect_bars(audio, sr, 4);
    s.phrase_times           = phrase_times(s.bar_times, 4);
    s.num_bars               = (int) s.bar_times.size();
    s.energy_per_bar         = energy_per_bar(audio, sr, s.bar_times);
    s.energy_peaks           = energy_peaks(s.energy_per_bar, 4);
    s.brightness_per_bar     = spectral_centroid_per_bar(audio, sr, s.bar_times);
    s.vocal_presence_per_bar = vocal_presence_per_bar(audio, sr, s.bar_times);
    s.section_boundaries     = section_boundaries(s.energy_per_bar,
                                                   s.brightness_per_bar, 4);
    return s;
}

} // namespace autoremix::dsp::analysis
