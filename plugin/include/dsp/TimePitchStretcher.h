#pragma once
// Phase 23-01 — Native time-stretch + pitch-shift wrapper around RubberBand.
// Header-only. Offline (whole-buffer) API for use by remix/mashup engines.
// Replaces librosa.effects.time_stretch + librosa.effects.pitch_shift used
// by the v3 Python sidecar.

#include <cmath>
#include <memory>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <rubberband/RubberBandStretcher.h>

namespace autoremix::dsp {

class TimePitchStretcher {
public:
    TimePitchStretcher(int sampleRate, int channels)
        : sampleRate_(sampleRate),
          channels_(channels),
          stretcher_(std::make_unique<RubberBand::RubberBandStretcher>(
              (size_t) sampleRate,
              (size_t) channels,
              // R2 (Faster) engine: ~3–5x quicker than R3 (Finer) for
              // creative remix work; quality difference is inaudible
              // after the engines' chop / reverb / normalize stages.
              RubberBand::RubberBandStretcher::OptionEngineFaster |
                  RubberBand::RubberBandStretcher::OptionProcessOffline))
    {}

    /** Time ratio. ratio < 1.0 = output shorter (faster); ratio > 1.0 = output
        longer (slower). Matches RubberBand semantics. */
    void setTimeRatio(double ratio)
    {
        stretcher_->setTimeRatio(ratio);
    }

    /** Pitch scale. 1.0 = no shift; 2.0 = +octave; 0.5 = -octave. */
    void setPitchScale(double scale)
    {
        stretcher_->setPitchScale(scale);
    }

    /** Convenience: shift by semitones. Positive = up, negative = down. */
    void setPitchSemitones(double semitones)
    {
        setPitchScale(std::pow(2.0, semitones / 12.0));
    }

    /** Offline process: feeds the whole input buffer through RubberBand,
        returns the stretched + pitched output. Single-shot. */
    juce::AudioBuffer<float> process(const juce::AudioBuffer<float>& input)
    {
        const int   numInput  = input.getNumSamples();
        const int   ch        = std::min(channels_, input.getNumChannels());
        const size_t blockSize = 1024;

        // Tell RubberBand how much we'll feed it (helps offline planning)
        stretcher_->setExpectedInputDuration((size_t) numInput);

        // ── Study pass ────────────────────────────────────────────────────
        {
            std::vector<const float*> studyPtrs((size_t) ch);
            int pos = 0;
            while (pos < numInput) {
                const int remaining = numInput - pos;
                const int n = std::min((int) blockSize, remaining);
                for (int c = 0; c < ch; ++c)
                    studyPtrs[(size_t) c] = input.getReadPointer(c, pos);
                stretcher_->study(studyPtrs.data(), (size_t) n, n == remaining);
                pos += n;
            }
        }

        // ── Process pass ──────────────────────────────────────────────────
        // Collect output per-channel in std::vector then copy to AudioBuffer.
        std::vector<std::vector<float>> outCh((size_t) ch);

        std::vector<const float*> procPtrs((size_t) ch);
        std::vector<float*>       outPtrs ((size_t) ch);
        std::vector<std::vector<float>> outScratch((size_t) ch);
        for (int c = 0; c < ch; ++c) outScratch[(size_t) c].resize(blockSize * 4);

        int pos = 0;
        // RubberBand offline: available() returns -1 when all output has been
        // retrieved. Condition must be `> 0` not `!= 0` to avoid infinite spin
        // once the stretcher signals EOF with -1.
        while (pos < numInput || stretcher_->available() > 0) {
            // Feed input chunk if anything left
            if (pos < numInput) {
                const int remaining = numInput - pos;
                const int n = std::min((int) blockSize, remaining);
                for (int c = 0; c < ch; ++c)
                    procPtrs[(size_t) c] = input.getReadPointer(c, pos);
                stretcher_->process(procPtrs.data(), (size_t) n, n == remaining);
                pos += n;
            }

            // Pull any available output
            int avail = stretcher_->available();
            while (avail > 0) {
                const int n = std::min(avail, (int) outScratch[0].size());
                for (int c = 0; c < ch; ++c) outPtrs[(size_t) c] = outScratch[(size_t) c].data();
                const size_t got = stretcher_->retrieve(outPtrs.data(), (size_t) n);
                if (got == 0) break;  // retrieve made no progress — stop draining
                for (int c = 0; c < ch; ++c)
                    outCh[(size_t) c].insert(outCh[(size_t) c].end(),
                                             outScratch[(size_t) c].begin(),
                                             outScratch[(size_t) c].begin() + (long) got);
                avail = stretcher_->available();
            }
        }

        // Copy to juce::AudioBuffer<float>
        const int outNumSamples = ch > 0 ? (int) outCh[0].size() : 0;
        juce::AudioBuffer<float> output(channels_, outNumSamples);
        output.clear();
        for (int c = 0; c < ch; ++c)
            output.copyFrom(c, 0, outCh[(size_t) c].data(), outNumSamples);
        return output;
    }

private:
    int sampleRate_;
    int channels_;
    std::unique_ptr<RubberBand::RubberBandStretcher> stretcher_;
};

} // namespace autoremix::dsp
