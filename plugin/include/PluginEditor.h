#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include "PluginProcessor.h"

class AutoRemixAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit AutoRemixAudioProcessorEditor(AutoRemixAudioProcessor&);
    ~AutoRemixAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    AutoRemixAudioProcessor& audioProcessor;

    juce::TextButton loadfile_btn, play_btn, stop_btn, save_btn;
    juce::Label file_lbl, remix_selector_lbl;
    juce::ComboBox remix_selector;
    juce::TextEditor debug_text;

    std::unique_ptr<juce::FileChooser> chooser_;
    juce::String file_path_;
    juce::String output_path_;

    void loadFile();
    void drawAndConfigComponents();
    void onClick_Play();
    void onClick_Stop();
    void onClick_Save();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoRemixAudioProcessorEditor)
};
