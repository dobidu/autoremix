#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoRemixAudioProcessor)

private:
    juce::AudioFormatManager format_manager_;

    std::unique_ptr<juce::AudioFormatReaderSource> preview_reader_;
    juce::AudioTransportSource                     preview_transport_;

    struct StemPlayer {
        std::unique_ptr<juce::AudioFormatReaderSource> reader;
        juce::AudioTransportSource                     transport;
        StemPlayer() = default;
        JUCE_DECLARE_NON_COPYABLE(StemPlayer)
    };
    StemPlayer             stem_players_[4];
    juce::MixerAudioSource mixer_;
};
