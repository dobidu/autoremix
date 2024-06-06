/* Description: This file contains the declaration of the 
AutoRemixAudioProcessorEditor class, which is responsible for the GUI of the 
plugin.
*/

#pragma once

#include <JuceHeader.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "PluginProcessor.h"

class AutoRemixAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    AutoRemixAudioProcessorEditor (AutoRemixAudioProcessor&);
    ~AutoRemixAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AutoRemixAudioProcessor& audioProcessor;

    void loadFile();
    void onClick_Play();
    void onClick_Stop();
    void onClick_Save();
    void drawAndConfigComponents();

    juce::String file_path;

    juce::Label file_lbl;
    juce::Label remix_selector_lbl;
    
    juce::TextButton loadfile_btn;
    juce::TextButton play_btn;
    juce::TextButton stop_btn;
    juce::TextButton save_btn;
    juce::TextButton config_btn;

    juce::TextEditor file_input;

    juce::TextEditor debug_text;

    juce::ComboBox remix_selector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoRemixAudioProcessorEditor)
};
