#pragma once

// ClipPlayer — real-time-safe playback of decoded clips (plan 34.5-03).
//
// Threads:
//  * Message thread: owns clips (ClipStore), issues play/stop/seek, reads
//    positions. Never waits for the audio thread.
//  * Audio thread: PlayerEngine::render() only. No allocation, no locks, no
//    file I/O, no logging, no exceptions; work is bounded by voices x samples.
//
// Hand-off: each Voice publishes its clip as an atomic raw pointer. Clips the
// audio thread may still be reading are released on the message thread only
// after the audio thread is provably past them (ClipStore::collect).
//
// REVIEW R-07, R-16, R-17, R-18, R-36, R-37.

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace autoremix::audio {

/** Immutable decoded audio (1 or 2 channels) at its own sample rate. */
struct Clip {
    juce::AudioBuffer<float> audio;
    double                   sample_rate = 0.0;
    juce::String             source;
};

/** One playback voice. Message-thread API is all atomics; the audio-side
 *  members are touched only from PlayerEngine::render(). */
class Voice
{
public:
    // ── message thread ─────────────────────────────────────────────────────
    void play_from(int64_t clip_sample) { seek_target_.store(clip_sample); want_play_.store(true); }
    void resume()                       { want_play_.store(true); }
    void stop()                         { want_play_.store(false); }
    void seek(int64_t clip_sample)      { seek_target_.store(std::max<int64_t>(0, clip_sample)); }
    bool wants_play() const             { return want_play_.load(); }
    bool is_audible() const             { return audible_.load(); }
    /** Position in clip samples (published once per block). */
    double position() const             { return position_.load(std::memory_order_relaxed); }

private:
    friend class PlayerEngine;
    friend class ClipStore;

    std::atomic<const Clip*> clip_        { nullptr };
    std::atomic<bool>        want_play_   { false };
    std::atomic<int64_t>     seek_target_ { -1 };
    std::atomic<double>      position_    { 0.0 };
    std::atomic<bool>        audible_     { false };

    // audio thread only
    const Clip* cur_          = nullptr;
    double      pos_          = 0.0;
    float       gain_         = 0.0f;
    int64_t     pending_seek_ = -1;
};

class PlayerEngine
{
public:
    static constexpr int kVoices = 5;     // 4 stems + preview

    /** Message thread, while audio is stopped (host contract). */
    void prepare(double device_sr, int /*max_block*/)
    {
        device_sr_.store(device_sr > 0.0 ? device_sr : 44100.0);
        running_.store(true);
    }
    /** Message thread. Clips are not touched (R-18). */
    void release() { running_.store(false); }

    Voice&       voice(int i)       { return voices_[(size_t) i]; }
    const Voice& voice(int i) const { return voices_[(size_t) i]; }

    uint64_t blocks_rendered() const { return blocks_.load(); }
    bool     in_render() const       { return in_render_.load(); }
    bool     running() const         { return running_.load(); }

    /** Audio thread. ADDS all voices into the first `out_channels` channels
     *  of `out` (callers keep their input for pass-through). */
    void render(juce::AudioBuffer<float>& out, int out_channels, int num_samples) noexcept
    {
        in_render_.store(true);
        const double dev_sr = device_sr_.load(std::memory_order_relaxed);
        const float  ramp_step = 1.0f / (float) std::max(1.0, 0.005 * dev_sr);   // 5 ms ramps
        const int    outs = std::min({ out_channels, out.getNumChannels(), 2 });

        if (outs > 0 && num_samples > 0)
            for (auto& v : voices_) render_voice(v, out, outs, num_samples, dev_sr, ramp_step);

        blocks_.fetch_add(1);
        in_render_.store(false);
    }

private:
    static float hermite(float y0, float y1, float y2, float y3, float t) noexcept
    {
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    static float sample_at(const float* d, int64_t len, double pos) noexcept
    {
        const int64_t i = (int64_t) pos;
        const float   t = (float) (pos - (double) i);
        if (t == 0.0f) return (i >= 0 && i < len) ? d[i] : 0.0f;        // exact at integer positions
        auto at = [d, len](int64_t k) { return (k >= 0 && k < len) ? d[k] : 0.0f; };
        return hermite(at(i - 1), at(i), at(i + 1), at(i + 2), t);
    }

    void render_voice(Voice& v, juce::AudioBuffer<float>& out, int outs, int n,
                      double dev_sr, float ramp_step) noexcept
    {
        // seq_cst pairs with ClipStore::assign/collect (store clip, then read in_render).
        const Clip* c = v.clip_.load(std::memory_order_seq_cst);
        if (c != v.cur_) {                       // new clip (or none): start fresh
            v.cur_ = c;
            v.pos_ = 0.0;
            v.gain_ = 0.0f;
            v.pending_seek_ = -1;
        }
        const int64_t seek = v.seek_target_.exchange(-1);
        if (seek >= 0) v.pending_seek_ = seek;

        if (v.cur_ == nullptr || v.cur_->audio.getNumSamples() == 0) {
            v.audible_.store(false);
            return;
        }

        const Clip&   clip = *v.cur_;
        const int64_t len  = clip.audio.getNumSamples();
        const double  ratio = clip.sample_rate > 0.0 ? clip.sample_rate / dev_sr : 1.0;
        const float*  l = clip.audio.getReadPointer(0);
        const float*  r = clip.audio.getNumChannels() > 1 ? clip.audio.getReadPointer(1) : l;
        float* o0 = out.getWritePointer(0);
        float* o1 = outs > 1 ? out.getWritePointer(1) : nullptr;

        bool want = v.want_play_.load();
        for (int s = 0; s < n; ++s) {
            // A pending seek jumps once the voice is silent (ramp out, jump, ramp in).
            if (v.gain_ == 0.0f && v.pending_seek_ >= 0) {
                v.pos_ = (double) std::min(v.pending_seek_, len);
                v.pending_seek_ = -1;
            }
            const float target = (want && v.pending_seek_ < 0) ? 1.0f : 0.0f;
            if      (v.gain_ < target) v.gain_ = std::min(target, v.gain_ + ramp_step);
            else if (v.gain_ > target) v.gain_ = std::max(target, v.gain_ - ramp_step);

            if (v.gain_ == 0.0f) {
                if (!want) break;                // stopped: hold position, output nothing
                continue;                        // ramping out towards a seek
            }
            if (v.pos_ >= (double) len) {        // end of clip: stop and park at the end
                v.gain_ = 0.0f;
                want = false;
                v.want_play_.store(false);
                break;
            }
            const float sl = sample_at(l, len, v.pos_);
            const float sr = (r == l) ? sl : sample_at(r, len, v.pos_);
            if (o1 != nullptr) { o0[s] += sl * v.gain_; o1[s] += sr * v.gain_; }
            else               { o0[s] += 0.5f * (sl + sr) * v.gain_; }     // mono bus (R-37)
            v.pos_ += ratio;
        }
        v.position_.store(v.pos_, std::memory_order_relaxed);
        v.audible_.store(v.gain_ > 0.0f);
    }

    std::array<Voice, kVoices> voices_;
    std::atomic<double>        device_sr_ { 44100.0 };
    std::atomic<uint64_t>      blocks_    { 0 };
    std::atomic<bool>          in_render_ { false };
    std::atomic<bool>          running_   { false };
};

/** Message-thread owner of clip lifetimes plus a small decoded-clip cache. */
class ClipStore
{
public:
    explicit ClipStore(PlayerEngine& engine) : engine_(engine) {}

    /** Publish `clip` on voice `i`; the previous clip is retired, not freed. */
    void assign(int i, std::shared_ptr<const Clip> clip)
    {
        auto& slot = live_[(size_t) i];
        if (slot == clip) return;
        engine_.voice(i).clip_.store(clip.get(), std::memory_order_seq_cst);
        if (slot) retired_.push_back({ std::move(slot), engine_.blocks_rendered() + 2 });
        slot = std::move(clip);
        collect();
    }

    const Clip* live(int i) const { return live_[(size_t) i].get(); }

    /** Free retired clips the audio thread can no longer be reading: either
     *  two blocks have completed since the swap, or no render is in progress
     *  now (any later render loads the new pointer). */
    void collect()
    {
        const bool idle = !engine_.in_render() || !engine_.running();
        const uint64_t done = engine_.blocks_rendered();
        retired_.erase(std::remove_if(retired_.begin(), retired_.end(),
                                      [&](const Retired& r) { return idle || done >= r.after; }),
                       retired_.end());
    }

    // ── cache (keyed by path + mtime + size) ───────────────────────────────
    static juce::String key_for(const juce::File& f)
    {
        return f.getFullPathName() + "|" + juce::String(f.getLastModificationTime().toMilliseconds())
             + "|" + juce::String(f.getSize());
    }
    std::shared_ptr<const Clip> cached(const juce::String& key)
    {
        for (auto& e : cache_)
            if (e.key == key) { e.stamp = ++clock_; return e.clip; }
        return {};
    }
    void remember(const juce::String& key, std::shared_ptr<const Clip> clip)
    {
        if (cached(key)) return;
        if (cache_.size() >= kCacheSize) {
            auto oldest = std::min_element(cache_.begin(), cache_.end(),
                                           [](const Entry& a, const Entry& b) { return a.stamp < b.stamp; });
            cache_.erase(oldest);       // live voices keep their own reference
        }
        cache_.push_back({ key, std::move(clip), ++clock_ });
    }

    size_t retired_count() const { return retired_.size(); }

private:
    static constexpr size_t kCacheSize = 6;
    struct Retired { std::shared_ptr<const Clip> clip; uint64_t after; };
    struct Entry   { juce::String key; std::shared_ptr<const Clip> clip; uint64_t stamp; };

    PlayerEngine&                                           engine_;
    std::array<std::shared_ptr<const Clip>, PlayerEngine::kVoices> live_;
    std::vector<Retired>                                    retired_;
    std::vector<Entry>                                      cache_;
    uint64_t                                                clock_ = 0;
};

/** Decode a file into a Clip (any thread; uses its own format manager).
 *  Keeps at most two channels. Returns null on failure. */
inline std::shared_ptr<const Clip> decode_clip(const juce::File& f)
{
    juce::AudioFormatManager fmt;
    fmt.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fmt.createReaderFor(f));
    if (!reader || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0) return {};
    const int ch = (int) std::min<unsigned>(2u, reader->numChannels);
    const int n  = (int) std::min<juce::int64>(reader->lengthInSamples, std::numeric_limits<int>::max());
    auto clip = std::make_shared<Clip>();
    clip->audio.setSize(ch, n);
    clip->audio.clear();
    reader->read(&clip->audio, 0, n, 0, true, ch > 1);
    clip->sample_rate = reader->sampleRate;
    clip->source = f.getFullPathName();
    return clip;
}

} // namespace autoremix::audio
