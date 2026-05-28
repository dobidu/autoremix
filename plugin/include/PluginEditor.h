#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "AutoRemixLookAndFeel.h"
#include "ModelStatusDot.h"
#include "ScreenContext.h"
#include "ScreenBase.h"
#include "dsp/NativePresetTypes.h"

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
    juce::Label    title_lbl;
    juce::ComboBox separator_combo_;
    juce::Label    sidecar_lbl_;
    ModelStatusDot health_dot_;

    // ── Footer ────────────────────────────────────────────────────────────────
    juce::Label status_lbl;

    // ── File dialog ───────────────────────────────────────────────────────────
    std::unique_ptr<juce::FileChooser> chooser_;

    // ── Tooltip support ───────────────────────────────────────────────────────
    juce::TooltipWindow tooltip_window_{ this, 750 };

    // ── Registry data — populated from native loaders at startup ─────────────
    std::vector<autoremix::PresetInfo>    presets_;
    std::vector<autoremix::SeparatorInfo> separators_;
    std::vector<autoremix::dsp::presets::NativeRemixPreset> native_remix_presets_;

    // ── Screen infrastructure ─────────────────────────────────────────────────
    ScreenContext                ctx_;
    std::unique_ptr<ScreenBase>  screen_;
    juce::ComponentAnimator      screen_animator_;

    void loadFile();
    void startMashupFlow();
    void loadEngineDefaults(int idx);
    void drawAndConfigComponents();
    void navigateTo(ScreenId id, bool animate = true);
    void installScreen(ScreenId id, bool animate);

    // Native runtime impls (Phase 27-01). Synchronous; run on the
    // background threads spawned by the screens.
    autoremix::FileAnalysis  analyzeFileNative(const juce::String& path);
    autoremix::StemPaths     separateNative(const std::filesystem::path& in,
                                            const std::filesystem::path& out,
                                            const std::string& sep_id);
    autoremix::ProcessResult renderRemixNative(const autoremix::StemPaths& stems,
                                               const autoremix::RemixParams& params,
                                               const std::filesystem::path& out_path);
    autoremix::MashupResult  runMashupNative(const autoremix::MashupParams& params);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoRemixAudioProcessorEditor)
};
