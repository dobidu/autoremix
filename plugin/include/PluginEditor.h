#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include "PluginProcessor.h"
#include "AutoRemixLookAndFeel.h"

class AutoRemixAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::ChangeListener {
public:
    explicit AutoRemixAudioProcessorEditor(AutoRemixAudioProcessor&);
    ~AutoRemixAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override { repaint(); }

private:
    AutoRemixAudioProcessor& audioProcessor;

    AutoRemixLookAndFeel laf_;

    juce::Label title_lbl;

    juce::AudioFormatManager  format_manager_;
    juce::AudioThumbnailCache thumbnail_cache_{5};
    juce::AudioThumbnail      thumbnail_{512, format_manager_, thumbnail_cache_};

    juce::TextButton  loadfile_btn, play_btn, save_btn;
    juce::Label       file_lbl, remix_selector_lbl, status_lbl;
    juce::ComboBox    remix_selector;
    double            progress_   = 0.0;
    juce::ProgressBar progress_bar_{progress_};

    std::unique_ptr<juce::FileChooser> chooser_;
    juce::String file_path_;
    juce::String output_path_;

    void loadFile();
    void drawAndConfigComponents();
    void onClick_Play();
    void onClick_Save();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoRemixAudioProcessorEditor)
};
