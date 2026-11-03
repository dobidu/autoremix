#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "jobs/JobRunner.h"
#include "audio/ClipPlayer.h"
#include <array>
#include <functional>

class AutoRemixAudioProcessorEditor;

class AutoRemixAudioProcessor : public juce::AudioProcessor {
public:
    AutoRemixAudioProcessor();
    ~AutoRemixAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioFormatManager&  getFormatManager() { return format_manager_; }

    // Background work (analysis, separation, render, mashup). Owned here so it
    // outlives any editor; the destructor cancels and joins it before the
    // format manager goes away.
    autoremix::jobs::JobRunner& getJobRunner() { return jobs_; }

    // Preview playback — original or remix, exclusive
    void loadPreviewFile(const juce::File& f);
    void togglePreview();
    void stopPreview();
    bool isPreviewPlaying() const;

    // Stem playback — 4 simultaneous, mixed
    void playStem(int idx, const juce::File& f);
    void stopStem(int idx);
    bool isStemPlaying(int idx) const;
    void stopAllStems();

    // Playback positions (0.0–1.0)
    double getPreviewPosition() const;
    double getStemPosition(int idx) const;

    // Seek (ratio 0.0–1.0)
    void seekStem(int idx, double ratio_0_to_1);
    void seekPreview(double ratio_0_to_1);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoRemixAudioProcessor)

private:
    juce::AudioFormatManager format_manager_;

    // ── Preview / stem playback (plan 34.5-03) ────────────────────────────────
    // Lock-free engine for the audio thread; clips decoded on the job runner;
    // lifetimes owned on the message thread. Voices 0..3 = stems, 4 = preview.
    static constexpr int kPreviewVoice = 4;
    void requestClip(int voice, const juce::File& f, bool play_when_ready);
    double voiceRatio(int voice) const;

    struct GcTimer : juce::Timer {
        std::function<void()> fn;
        void timerCallback() override { if (fn) fn(); }
    };

    autoremix::audio::PlayerEngine engine_;
    autoremix::audio::ClipStore    clips_ { engine_ };
    std::array<uint64_t, autoremix::audio::PlayerEngine::kVoices> load_gen_ {};
    GcTimer                        gc_timer_;
    autoremix::jobs::Lifetime      alive_ = autoremix::jobs::make_lifetime();

    // Declared last: destroyed first, before anything a job may reference.
    autoremix::jobs::JobRunner jobs_ { 2 };
};
