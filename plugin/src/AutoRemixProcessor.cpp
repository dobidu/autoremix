#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/TimePitchStretcher.h"          // Phase 23-01 smoke
#include "dsp/NativeAnalysis.h"              // Phase 23-02 smoke
#include "dsp/NativeAlgorithmicSeparator.h"  // Phase 23-03 smoke
#include "dsp/NativeRemixEngines.h"          // Phase 24-01 smoke
#include "dsp/NativeEffectOps.h"             // Phase 24-02 smoke
#include "dsp/NativeEffectChainEngine.h"     // Phase 24-02 smoke
#include "dsp/NativeMashupEngine.h"          // Phase 25-01 smoke
#include "dsp/NativePresetTypes.h"           // Phase 25-02 smoke
#include "dsp/NativePresetLoaders.h"         // Phase 25-02 smoke
#include "dsp/ModelDownloader.h"             // Phase 26-02 smoke
#include "dsp/NativeDemucsSeparator.h"       // Phase 26-02 smoke
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
    // Phase 27-01: Python sidecar lifecycle removed. Native runtime
    // needs no startup work.
    format_manager_.registerBasicFormats();
    gc_timer_.fn = [this] { clips_.collect(); };
    gc_timer_.startTimer(250);
}

AutoRemixAudioProcessor::~AutoRemixAudioProcessor()
{
    // Stop background jobs first: they use format_manager_ (R-01..R-04).
    // Blocks until every job has returned; never proceeds while one runs.
    jobs_.shutdown();
    gc_timer_.stopTimer();
    engine_.release();
}

//==============================================================================
// Playback (plan 34.5-03). Everything here runs on the message thread and
// only touches the engine through atomics; decoding runs on the job runner.

void AutoRemixAudioProcessor::requestClip(int voice, const juce::File& f, bool play_when_ready)
{
    auto& v = engine_.voice(voice);
    const uint64_t gen = ++load_gen_[(size_t) voice];   // a newer request wins
    clips_.assign(voice, nullptr);                        // never play the previous file meanwhile
    if (play_when_ready) v.play_from(0); else v.stop();

    const auto key = autoremix::audio::ClipStore::key_for(f);
    if (auto hit = clips_.cached(key)) { clips_.assign(voice, std::move(hit)); return; }

    using ClipPtr = std::shared_ptr<const autoremix::audio::Clip>;
    jobs_.submit<ClipPtr>(alive_,
        [f](const autoremix::jobs::CancelToken&) { return autoremix::audio::decode_clip(f); },
        [this, voice, gen, key](ClipPtr clip, bool cancelled) {
            if (cancelled || clip == nullptr || gen != load_gen_[(size_t) voice]) return;
            clips_.remember(key, clip);
            clips_.assign(voice, std::move(clip));
        });
}

double AutoRemixAudioProcessor::voiceRatio(int voice) const
{
    const auto* c = clips_.live(voice);
    const int len = c ? c->audio.getNumSamples() : 0;
    return len > 0 ? std::clamp(engine_.voice(voice).position() / (double) len, 0.0, 1.0) : 0.0;
}

void AutoRemixAudioProcessor::loadPreviewFile(const juce::File& f)
{
    requestClip(kPreviewVoice, f, false);
}

void AutoRemixAudioProcessor::togglePreview()
{
    auto& v = engine_.voice(kPreviewVoice);
    if (v.wants_play()) v.stop();
    else                v.play_from(0);
}

void AutoRemixAudioProcessor::stopPreview()            { engine_.voice(kPreviewVoice).stop(); }
bool AutoRemixAudioProcessor::isPreviewPlaying() const { return engine_.voice(kPreviewVoice).wants_play(); }

void AutoRemixAudioProcessor::playStem(int idx, const juce::File& f)
{
    if (idx < 0 || idx >= 4) return;
    requestClip(idx, f, true);
}

void AutoRemixAudioProcessor::stopStem(int idx)
{
    if (idx < 0 || idx >= 4) return;
    engine_.voice(idx).stop();
}

bool AutoRemixAudioProcessor::isStemPlaying(int idx) const
{
    if (idx < 0 || idx >= 4) return false;
    return engine_.voice(idx).wants_play();
}

void AutoRemixAudioProcessor::stopAllStems()
{
    for (int i = 0; i < 4; ++i) engine_.voice(i).stop();
}

double AutoRemixAudioProcessor::getPreviewPosition() const { return voiceRatio(kPreviewVoice); }

double AutoRemixAudioProcessor::getStemPosition(int idx) const
{
    if (idx < 0 || idx >= 4) return 0.0;
    return voiceRatio(idx);
}

void AutoRemixAudioProcessor::seekStem(int idx, double ratio)
{
    if (idx < 0 || idx >= 4) return;
    if (const auto* c = clips_.live(idx))
        engine_.voice(idx).seek((int64_t) (std::clamp(ratio, 0.0, 1.0) * c->audio.getNumSamples()));
}

void AutoRemixAudioProcessor::seekPreview(double ratio)
{
    if (const auto* c = clips_.live(kPreviewVoice))
        engine_.voice(kPreviewVoice).seek((int64_t) (std::clamp(ratio, 0.0, 1.0) * c->audio.getNumSamples()));
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
    engine_.prepare(sampleRate, samplesPerBlock);
}

void AutoRemixAudioProcessor::releaseResources()
{
    // Loaded clips stay loaded (R-18); the engine just stops being "running".
    engine_.release();
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
    // Real-time safe: no allocation, locks, file I/O or logging (R-17).
    juce::ScopedNoDenormals noDenormals;
    const int n    = buffer.getNumSamples();
    const int ins  = getTotalNumInputChannels();
    const int outs = getTotalNumOutputChannels();

    // Pass the input through (R-07, decision D4); clear only output channels
    // that have no matching input.
    for (int ch = ins; ch < outs; ++ch)
        buffer.clear(ch, 0, n);

    // Preview / stems are added on top.
    engine_.render(buffer, outs, n);
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
