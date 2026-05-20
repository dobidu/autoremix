#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/TimePitchStretcher.h"          // Phase 23-01 smoke
#include "dsp/NativeAnalysis.h"              // Phase 23-02 smoke
#include "dsp/NativeAlgorithmicSeparator.h"  // Phase 23-03 smoke
#include "dsp/NativeRemixEngines.h"          // Phase 24-01 smoke
#include "dsp/NativeEffectOps.h"             // Phase 24-02 smoke
#include "dsp/NativeEffectChainEngine.h"     // Phase 24-02 smoke
#include <cstdlib>
#include <filesystem>

//==============================================================================
AutoRemixAudioProcessor::AutoRemixAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    if (const char* path = std::getenv("AUTOREMIX_SERVER_PATH"))
        bridge_.startSidecar(std::filesystem::path(path));
    format_manager_.registerBasicFormats();
}

AutoRemixAudioProcessor::~AutoRemixAudioProcessor()
{
    mixer_.removeAllInputs();
    for (auto& sp : stem_players_) {
        sp.transport.stop();
        sp.transport.setSource(nullptr);
    }
    preview_transport_.stop();
    preview_transport_.setSource(nullptr);
    bridge_.stopSidecar();
}

//==============================================================================
void AutoRemixAudioProcessor::loadPreviewFile(const juce::File& f)
{
    preview_transport_.stop();
    preview_transport_.setSource(nullptr);
    preview_reader_.reset();
    if (auto* reader = format_manager_.createReaderFor(f)) {
        preview_reader_ = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
        preview_transport_.setSource(preview_reader_.get(), 0, nullptr, reader->sampleRate);
    }
}

void AutoRemixAudioProcessor::togglePreview()
{
    if (preview_transport_.isPlaying())
        preview_transport_.stop();
    else {
        preview_transport_.setPosition(0.0);
        preview_transport_.start();
    }
}

void AutoRemixAudioProcessor::stopPreview()     { preview_transport_.stop(); }
bool AutoRemixAudioProcessor::isPreviewPlaying() const { return preview_transport_.isPlaying(); }

void AutoRemixAudioProcessor::playStem(int idx, const juce::File& f)
{
    if (idx < 0 || idx >= 4) return;
    auto& sp = stem_players_[idx];
    sp.transport.stop();
    sp.transport.setSource(nullptr);
    sp.reader.reset();
    if (auto* reader = format_manager_.createReaderFor(f)) {
        sp.reader = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
        sp.transport.setSource(sp.reader.get(), 0, nullptr, reader->sampleRate);
        sp.transport.setPosition(0.0);
        sp.transport.start();
    }
}

void AutoRemixAudioProcessor::stopStem(int idx)
{
    if (idx < 0 || idx >= 4) return;
    stem_players_[idx].transport.stop();
}

bool AutoRemixAudioProcessor::isStemPlaying(int idx) const
{
    if (idx < 0 || idx >= 4) return false;
    return stem_players_[idx].transport.isPlaying();
}

void AutoRemixAudioProcessor::stopAllStems()
{
    for (auto& sp : stem_players_)
        sp.transport.stop();
}

double AutoRemixAudioProcessor::getPreviewPosition() const
{
    double len = preview_transport_.getLengthInSeconds();
    return (len > 0.0) ? preview_transport_.getCurrentPosition() / len : 0.0;
}

double AutoRemixAudioProcessor::getStemPosition(int idx) const
{
    if (idx < 0 || idx >= 4) return 0.0;
    double len = stem_players_[idx].transport.getLengthInSeconds();
    return (len > 0.0) ? stem_players_[idx].transport.getCurrentPosition() / len : 0.0;
}

//==============================================================================
const juce::String AutoRemixAudioProcessor::getName() const { return JucePlugin_Name; }

bool AutoRemixAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AutoRemixAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AutoRemixAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AutoRemixAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int    AutoRemixAudioProcessor::getNumPrograms()             { return 1; }
int    AutoRemixAudioProcessor::getCurrentProgram()          { return 0; }
void   AutoRemixAudioProcessor::setCurrentProgram(int)       {}
const juce::String AutoRemixAudioProcessor::getProgramName(int)          { return {}; }
void   AutoRemixAudioProcessor::changeProgramName(int, const juce::String&) {}

//==============================================================================
void AutoRemixAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    for (auto& sp : stem_players_)
        sp.transport.prepareToPlay(samplesPerBlock, sampleRate);
    preview_transport_.prepareToPlay(samplesPerBlock, sampleRate);

    mixer_.removeAllInputs();
    for (auto& sp : stem_players_)
        mixer_.addInputSource(&sp.transport, false);
    mixer_.addInputSource(&preview_transport_, false);
    mixer_.prepareToPlay(samplesPerBlock, sampleRate);
}

void AutoRemixAudioProcessor::releaseResources()
{
    mixer_.removeAllInputs();
    for (auto& sp : stem_players_) {
        sp.transport.stop();
        sp.transport.setSource(nullptr);
        sp.reader.reset();
    }
    preview_transport_.stop();
    preview_transport_.setSource(nullptr);
    preview_reader_.reset();
    mixer_.releaseResources();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AutoRemixAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif
    return true;
  #endif
}
#endif

void AutoRemixAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    bool anyPlaying = preview_transport_.isPlaying();
    for (auto& sp : stem_players_)
        anyPlaying = anyPlaying || sp.transport.isPlaying();

    if (anyPlaying) {
        juce::AudioSourceChannelInfo info(buffer);
        mixer_.getNextAudioBlock(info);
    }
}

//==============================================================================
bool AutoRemixAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* AutoRemixAudioProcessor::createEditor()
{
    return new AutoRemixAudioProcessorEditor(*this);
}

void AutoRemixAudioProcessor::getStateInformation(juce::MemoryBlock&)  {}
void AutoRemixAudioProcessor::setStateInformation(const void*, int)    {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutoRemixAudioProcessor();
}
