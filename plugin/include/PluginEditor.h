#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "AutoRemixLookAndFeel.h"
#include "SidecarHealthDot.h"
#include "WaveformDisplay.h"

class AutoRemixAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::ChangeListener {
public:
    explicit AutoRemixAudioProcessorEditor(AutoRemixAudioProcessor&);
    ~AutoRemixAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override { waveform_display_.sourceChanged(); }

private:
    AutoRemixAudioProcessor& audioProcessor;

    AutoRemixLookAndFeel laf_;

    juce::Label    title_lbl;
    juce::ComboBox style_combo_;
    SidecarHealthDot health_dot_{[this]{ return audioProcessor.getBridge().isServerAlive(); }};

    juce::AudioFormatManager  format_manager_;
    juce::AudioThumbnailCache thumbnail_cache_{5};
    juce::AudioThumbnail      thumbnail_{512, format_manager_, thumbnail_cache_};
    WaveformDisplay           waveform_display_{thumbnail_};

    juce::TextButton  loadfile_btn, play_btn, save_btn;
    juce::Label       file_lbl, status_lbl;
    double            progress_   = 0.0;
    juce::ProgressBar progress_bar_{progress_};

    juce::Slider tempo_slider_, pitch_slider_, reverb_slider_, chop_slider_;
    juce::Label  tempo_lbl_,   pitch_lbl_,    reverb_lbl_,    chop_lbl_;

    std::unique_ptr<juce::FileChooser> chooser_;
    juce::String file_path_;
    juce::String output_path_;
    std::vector<autoremix::PresetInfo> presets_;

    void loadFile();
    void loadEngineDefaults(int idx);
    void drawAndConfigComponents();
    void onClick_Play();
    void onClick_Save();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoRemixAudioProcessorEditor)
};
