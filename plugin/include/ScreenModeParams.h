#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "ScreenBase.h"
#include "AutoRemixLookAndFeel.h"
#include "PluginTypes.h"

class ScreenModeParams : public ScreenBase {
public:
    using SavePresetFn = std::function<void(
        const juce::String& name,
        const autoremix::RemixParams& params,
        std::function<void(bool)> on_complete)>;

    ScreenModeParams(ScreenContext& ctx, SavePresetFn save_fn)
        : ScreenBase(ctx), save_fn_(std::move(save_fn))
    {
        addAndMakeVisible(preset_lbl_);
        preset_lbl_.setText("PRESET", juce::dontSendNotification);
        preset_lbl_.setFont(AR::font(AR::FontRole::section_label));
        preset_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));

        addAndMakeVisible(preset_combo_);
        preset_combo_.onChange = [this] { onPresetChanged(); };

        addAndMakeVisible(save_preset_btn_);
        save_preset_btn_.setButtonText("Save as Preset");
        save_preset_btn_.setComponentID("ghost");
        save_preset_btn_.onClick = [this] { onClickSavePreset(); };

        addAndMakeVisible(source_bpm_lbl_);
        source_bpm_lbl_.setFont(AR::font(AR::FontRole::mono_value));
        source_bpm_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));

        addAndMakeVisible(bpm_section_lbl_);
        bpm_section_lbl_.setText("TARGET BPM", juce::dontSendNotification);
        bpm_section_lbl_.setFont(AR::font(AR::FontRole::section_label));
        bpm_section_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));

        addAndMakeVisible(bpm_slider_);
        bpm_slider_.setSliderStyle(juce::Slider::LinearHorizontal);
        bpm_slider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 24);
        bpm_slider_.setRange(40.0, 200.0, 0.5);
        bpm_slider_.setTextValueSuffix(" BPM");
        bpm_slider_.onValueChange = [this] { ctx_.target_bpm = (float)bpm_slider_.getValue(); };

        addAndMakeVisible(pitch_section_lbl_);
        pitch_section_lbl_.setText("PITCH (SEMITONES)", juce::dontSendNotification);
        pitch_section_lbl_.setFont(AR::font(AR::FontRole::section_label));
        pitch_section_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));

        addAndMakeVisible(pitch_slider_);
        pitch_slider_.setSliderStyle(juce::Slider::LinearHorizontal);
        pitch_slider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 24);
        pitch_slider_.setRange(-12.0, 12.0, 0.5);
        pitch_slider_.setTextValueSuffix(" semi");
        pitch_slider_.onValueChange = [this] { ctx_.pitch_semi = (float)pitch_slider_.getValue(); };

        addAndMakeVisible(reverb_section_lbl_);
        reverb_section_lbl_.setText("REVERB MIX", juce::dontSendNotification);
        reverb_section_lbl_.setFont(AR::font(AR::FontRole::section_label));
        reverb_section_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));

        addAndMakeVisible(reverb_slider_);
        reverb_slider_.setSliderStyle(juce::Slider::LinearHorizontal);
        reverb_slider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 24);
        reverb_slider_.setRange(0.0, 1.0, 0.01);
        reverb_slider_.onValueChange = [this] { ctx_.reverb_mix = (float)reverb_slider_.getValue(); };

        addAndMakeVisible(chop_mode_lbl_);
        chop_mode_lbl_.setText("CHOP MODE", juce::dontSendNotification);
        chop_mode_lbl_.setFont(AR::font(AR::FontRole::section_label));
        chop_mode_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));

        addAndMakeVisible(chop_mode_combo_);
        chop_mode_combo_.addItem("Fixed",      1);
        chop_mode_combo_.addItem("Beat",       2);
        chop_mode_combo_.addItem("Onset",      3);
        chop_mode_combo_.addItem("Bar",        4);
        chop_mode_combo_.addItem("Energy",     5);
        chop_mode_combo_.addItem("Structural", 6);
        chop_mode_combo_.setSelectedItemIndex(0, juce::dontSendNotification);
        chop_mode_combo_.onChange = [this] {
            ctx_.chop_mode_idx = chop_mode_combo_.getSelectedItemIndex();
        };

        addAndMakeVisible(chop_beats_lbl_);
        chop_beats_lbl_.setText("CHOP INTERVAL (BEATS)", juce::dontSendNotification);
        chop_beats_lbl_.setFont(AR::font(AR::FontRole::section_label));
        chop_beats_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));

        addAndMakeVisible(chop_beats_slider_);
        chop_beats_slider_.setSliderStyle(juce::Slider::LinearHorizontal);
        chop_beats_slider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 24);
        chop_beats_slider_.setRange(0.25, 8.0, 0.25);
        chop_beats_slider_.onValueChange = [this] {
            ctx_.chop_beats = (float)chop_beats_slider_.getValue();
        };

        addAndMakeVisible(back_btn_);
        back_btn_.setButtonText("< Back");
        back_btn_.setComponentID("ghost");
        back_btn_.onClick = [this] { ctx_.navigate(ScreenId::StemsReady); };

        addAndMakeVisible(render_btn_);
        render_btn_.setButtonText("Render >");
        render_btn_.setComponentID("primary");
        render_btn_.onClick = [this] {
            ctx_.render_is_mashup = false;
            ctx_.navigate(ScreenId::Render);
        };
    }

    void onEnter() override
    {
        preset_combo_.clear(juce::dontSendNotification);
        for (int i = 0; i < (int)ctx_.presets.size(); ++i)
            preset_combo_.addItem(ctx_.presets[(size_t)i].name, i + 1);
        if (!ctx_.presets.empty())
            preset_combo_.setSelectedItemIndex(ctx_.selected_preset_idx,
                                               juce::dontSendNotification);

        source_bpm_lbl_.setText("Source: " + juce::String(ctx_.detected_bpm, 1) + " BPM",
                                juce::dontSendNotification);
        bpm_slider_       .setValue(ctx_.target_bpm, juce::dontSendNotification);
        pitch_slider_     .setValue(ctx_.pitch_semi,  juce::dontSendNotification);
        reverb_slider_    .setValue(ctx_.reverb_mix,  juce::dontSendNotification);
        chop_beats_slider_.setValue(ctx_.chop_beats,  juce::dontSendNotification);
        chop_mode_combo_  .setSelectedItemIndex(ctx_.chop_mode_idx, juce::dontSendNotification);

        resized();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(AR::BG));
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(32, 0);
        auto actionBar = b.removeFromBottom(72);

        constexpr int LABEL_H     = 20;
        constexpr int CONTROL_H   = 32;
        constexpr int SECTION_GAP = 16;

        b.removeFromTop(16);

        // Preset row: label spans full width; combo + "Save as Preset" button inline
        preset_lbl_.setBounds(b.removeFromTop(LABEL_H));
        b.removeFromTop(4);
        auto comboRow = b.removeFromTop(CONTROL_H);
        save_preset_btn_.setBounds(comboRow.removeFromRight(140));
        comboRow.removeFromRight(8);
        preset_combo_.setBounds(comboRow);
        b.removeFromTop(SECTION_GAP);

        auto bpmRow = b.removeFromTop(LABEL_H);
        bpm_section_lbl_.setBounds(bpmRow.removeFromLeft(bpmRow.getWidth() / 2));
        source_bpm_lbl_ .setBounds(bpmRow);
        b.removeFromTop(4);
        bpm_slider_.setBounds(b.removeFromTop(CONTROL_H));
        b.removeFromTop(SECTION_GAP);

        pitch_section_lbl_.setBounds(b.removeFromTop(LABEL_H));
        b.removeFromTop(4);
        pitch_slider_.setBounds(b.removeFromTop(CONTROL_H));
        b.removeFromTop(SECTION_GAP);

        reverb_section_lbl_.setBounds(b.removeFromTop(LABEL_H));
        b.removeFromTop(4);
        reverb_slider_.setBounds(b.removeFromTop(CONTROL_H));
        b.removeFromTop(SECTION_GAP);

        chop_mode_lbl_.setBounds(b.removeFromTop(LABEL_H));
        b.removeFromTop(4);
        chop_mode_combo_.setBounds(b.removeFromTop(CONTROL_H));
        b.removeFromTop(8);
        chop_beats_lbl_.setBounds(b.removeFromTop(LABEL_H));
        b.removeFromTop(4);
        chop_beats_slider_.setBounds(b.removeFromTop(CONTROL_H));

        actionBar = actionBar.reduced(0, 16);
        back_btn_  .setBounds(actionBar.removeFromLeft(120));
        render_btn_.setBounds(actionBar.removeFromRight(160));
    }

private:
    void onPresetChanged()
    {
        int idx = preset_combo_.getSelectedItemIndex();
        if (idx < 0 || idx >= (int)ctx_.presets.size()) return;
        ctx_.selected_preset_idx = idx;
        const auto& p = ctx_.presets[(size_t)idx].default_params;
        float targetBpm = ctx_.detected_bpm * p.tempo_factor;
        targetBpm = juce::jlimit(40.0f, 200.0f, targetBpm);
        ctx_.target_bpm = targetBpm;
        ctx_.pitch_semi = p.pitch_shift_semi;
        ctx_.reverb_mix = p.reverb_mix;
        bpm_slider_   .setValue(ctx_.target_bpm, juce::dontSendNotification);
        pitch_slider_ .setValue(ctx_.pitch_semi,  juce::dontSendNotification);
        reverb_slider_.setValue(ctx_.reverb_mix,  juce::dontSendNotification);
    }

    void onClickSavePreset()
    {
        juce::AlertWindow aw("Save Preset",
                             "Enter a name for this preset:",
                             juce::MessageBoxIconType::NoIcon);
        aw.addTextEditor("name", "", "Preset name:");
        aw.addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
        aw.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        if (aw.runModalLoop() != 1) return;

        auto name = aw.getTextEditorContents("name").trim();
        if (name.isEmpty()) return;

        float src_bpm      = (ctx_.detected_bpm > 0.0f) ? ctx_.detected_bpm : 120.0f;
        float tempo_factor = ctx_.target_bpm / src_bpm;
        float chop_ms      = ctx_.chop_beats * (60000.0f / src_bpm);

        static const char* kChopModes[] = {"fixed","beat","onset","bar","energy","structural"};

        autoremix::RemixParams params;
        params.tempo_factor     = tempo_factor;
        params.pitch_shift_semi = ctx_.pitch_semi;
        params.reverb_mix       = ctx_.reverb_mix;
        params.chop_interval_ms = chop_ms;
        params.chop_mode        = kChopModes[juce::jlimit(0, 5, ctx_.chop_mode_idx)];
        params.vocals_gain      = ctx_.vocals_gain;
        params.drums_gain       = ctx_.drums_gain;
        params.bass_gain        = ctx_.bass_gain;
        params.other_gain       = ctx_.other_gain;
        if (!ctx_.presets.empty() && ctx_.selected_preset_idx < (int)ctx_.presets.size())
            params.engine_id = ctx_.presets[(size_t)ctx_.selected_preset_idx].id;

        ctx_.set_status("Saving preset...");
        save_preset_btn_.setEnabled(false);

        save_fn_(name, params, [this, name](bool ok) {
            save_preset_btn_.setEnabled(true);
            if (ok) {
                int found = ctx_.selected_preset_idx;
                preset_combo_.clear(juce::dontSendNotification);
                for (int i = 0; i < (int)ctx_.presets.size(); ++i) {
                    preset_combo_.addItem(ctx_.presets[(size_t)i].name, i + 1);
                    if (ctx_.presets[(size_t)i].name == name)
                        found = i;
                }
                preset_combo_.setSelectedItemIndex(found, juce::dontSendNotification);
                ctx_.selected_preset_idx = found;
                ctx_.set_status("Preset saved: " + name);
            } else {
                ctx_.set_status("Error: preset save failed.");
            }
        });
    }

    SavePresetFn   save_fn_;

    juce::Label    preset_lbl_;
    juce::ComboBox preset_combo_;
    juce::TextButton save_preset_btn_;

    juce::Label  bpm_section_lbl_;
    juce::Label  source_bpm_lbl_;
    juce::Slider bpm_slider_;

    juce::Label  pitch_section_lbl_;
    juce::Slider pitch_slider_;

    juce::Label  reverb_section_lbl_;
    juce::Slider reverb_slider_;

    juce::Label    chop_mode_lbl_;
    juce::ComboBox chop_mode_combo_;
    juce::Label    chop_beats_lbl_;
    juce::Slider   chop_beats_slider_;

    juce::TextButton back_btn_;
    juce::TextButton render_btn_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScreenModeParams)
};
