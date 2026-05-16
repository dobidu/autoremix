#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "AutoRemixLookAndFeel.h"
#include "SidecarHealthDot.h"
#include "ScreenContext.h"
#include "ScreenBase.h"

class AutoRemixAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::FileDragAndDropTarget {
public:
    explicit AutoRemixAudioProcessorEditor(AutoRemixAudioProcessor&);
    ~AutoRemixAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    AutoRemixAudioProcessor& audioProcessor;

    AutoRemixLookAndFeel laf_;

    // ── Header widgets (always visible, owned by editor) ──────────────────────
    juce::Label      title_lbl;
    juce::ComboBox   separator_combo_;
    SidecarHealthDot health_dot_{[this]{ return audioProcessor.getBridge().isServerAlive(); }};

    // ── Footer ────────────────────────────────────────────────────────────────
    juce::Label status_lbl;

    // ── File dialog ───────────────────────────────────────────────────────────
    std::unique_ptr<juce::FileChooser> chooser_;

    // ── Sidecar registry data ─────────────────────────────────────────────────
    std::vector<autoremix::PresetInfo>    presets_;
    std::vector<autoremix::SeparatorInfo> separators_;

    // ── Screen infrastructure ─────────────────────────────────────────────────
    ScreenContext                ctx_;
    std::unique_ptr<ScreenBase>  screen_;
    juce::ComponentAnimator      screen_animator_;

    void loadFile();
    void loadEngineDefaults(int idx);
    void drawAndConfigComponents();
    void navigateTo(ScreenId id, bool animate = true);
    void installScreen(ScreenId id, bool animate);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoRemixAudioProcessorEditor)
};
