#pragma once
#include <array>
#include <filesystem>
#include <juce_gui_basics/juce_gui_basics.h>
#include "AutoRemixLookAndFeel.h"
#include "PluginTypes.h"
#include "ScreenBase.h"

// ─────────────────────────────────────────────────────────────────────────────
// ScreenMashup — two-column mashup mixer.
//   Track A (left) and Track B (right). Each shows its 4 stems (vocals, drums,
//   bass, other) with an individual volume slider (0–2×). Mashup output =
//   sum of all 8 stems × their gains, with B time-stretched + pitch-shifted
//   to match track A's BPM and key (or user-specified targets).
// ─────────────────────────────────────────────────────────────────────────────
class ScreenMashup : public ScreenBase,
                     private juce::Timer {
public:
    explicit ScreenMashup(ScreenContext& ctx)
        : ScreenBase(ctx)
    {
        addAndMakeVisible(rendering_header_lbl_);
        rendering_header_lbl_.setText("MASHING UP...", juce::dontSendNotification);
        rendering_header_lbl_.setFont(AR::font(AR::FontRole::display_mega));
        rendering_header_lbl_.setColour(juce::Label::textColourId,
                                        juce::Colour(AR::MASHUP));
        rendering_header_lbl_.setJustificationType(juce::Justification::centred);
        rendering_header_lbl_.setVisible(false);

        addAndMakeVisible(rendering_elapsed_lbl_);
        rendering_elapsed_lbl_.setFont(AR::font(AR::FontRole::mono_value));
        rendering_elapsed_lbl_.setColour(juce::Label::textColourId,
                                         juce::Colour(AR::COMMENT));
        rendering_elapsed_lbl_.setJustificationType(juce::Justification::centred);
        rendering_elapsed_lbl_.setVisible(false);

        addAndMakeVisible(rendering_hint_lbl_);
        rendering_hint_lbl_.setText(
            "Separating both tracks, matching tempo + key, mixing 8 stems.",
            juce::dontSendNotification);
        rendering_hint_lbl_.setFont(AR::font(AR::FontRole::status));
        rendering_hint_lbl_.setColour(juce::Label::textColourId,
                                      juce::Colour(AR::COMMENT));
        rendering_hint_lbl_.setJustificationType(juce::Justification::centred);
        rendering_hint_lbl_.setVisible(false);

        addAndMakeVisible(header_lbl_);
        header_lbl_.setText("MASHUP", juce::dontSendNotification);
        header_lbl_.setFont(AR::font(AR::FontRole::section));
        header_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::FG));

        addAndMakeVisible(template_lbl_);
        template_lbl_.setText("TEMPLATE", juce::dontSendNotification);
        template_lbl_.setFont(AR::font(AR::FontRole::section_label));
        template_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));

        addAndMakeVisible(template_combo_);
        template_combo_.onChange = [this] { applyTemplate(template_combo_.getSelectedItemIndex()); };

        addAndMakeVisible(advanced_btn_);
        advanced_btn_.setButtonText(juce::String::fromUTF8("Advanced \xE2\x96\xBE"));
        advanced_btn_.setComponentID("ghost");
        advanced_btn_.onClick = [this] {
            advanced_open_ = !advanced_open_;
            advanced_btn_.setButtonText(juce::String::fromUTF8(
                advanced_open_ ? "Advanced \xE2\x96\xB4" : "Advanced \xE2\x96\xBE"));
            updateAdvancedVisibility();
            resized();
        };

        initAdvancedSlider(tempo_mod_lbl_,     "TEMPO MOD",  tempo_mod_slider_,
                           0.5, 1.5, 0.01, 1.0,
                           [this] { ctx_.mashup_bpm_modifier = (float)tempo_mod_slider_.getValue(); });
        initAdvancedSlider(master_pitch_lbl_,  "MASTER PITCH (semi)", master_pitch_slider_,
                           -12.0, 12.0, 1.0, 0.0,
                           [this] { ctx_.mashup_master_pitch_offset_semi = (float)master_pitch_slider_.getValue(); });
        initAdvancedSlider(master_reverb_lbl_, "REVERB MIX", master_reverb_slider_,
                           0.0, 1.0, 0.01, 0.0,
                           [this] { ctx_.mashup_master_reverb_mix = (float)master_reverb_slider_.getValue(); });
        initAdvancedSlider(reverb_room_lbl_,   "REVERB ROOM", reverb_room_slider_,
                           0.0, 1.0, 0.01, 0.5,
                           [this] { ctx_.mashup_master_reverb_room = (float)reverb_room_slider_.getValue(); });
        initAdvancedSlider(hpf_b_lbl_,         "HPF TRACK B (Hz)", hpf_b_slider_,
                           0.0, 400.0, 1.0, 0.0,
                           [this] { ctx_.mashup_highpass_b_hz = (float)hpf_b_slider_.getValue(); });

        // Track A column header
        addAndMakeVisible(a_header_lbl_);
        a_header_lbl_.setFont(AR::font(AR::FontRole::section_label));
        a_header_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::ACCENT));

        addAndMakeVisible(a_info_lbl_);
        a_info_lbl_.setFont(AR::font(AR::FontRole::mono_value));
        a_info_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));

        // Track B column header
        addAndMakeVisible(b_header_lbl_);
        b_header_lbl_.setFont(AR::font(AR::FontRole::section_label));
        b_header_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::MASHUP));

        addAndMakeVisible(b_info_lbl_);
        b_info_lbl_.setFont(AR::font(AR::FontRole::mono_value));
        b_info_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));

        for (size_t i = 0; i < kStemNames.size(); ++i) {
            initStemRow(rows_a_[i], kStemNames[i], /*is_b=*/false);
            initStemRow(rows_b_[i], kStemNames[i], /*is_b=*/true);
        }

        addAndMakeVisible(target_bpm_lbl_);
        target_bpm_lbl_.setText("TARGET BPM", juce::dontSendNotification);
        target_bpm_lbl_.setFont(AR::font(AR::FontRole::section_label));
        target_bpm_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));

        addAndMakeVisible(target_bpm_slider_);
        target_bpm_slider_.setSliderStyle(juce::Slider::LinearHorizontal);
        target_bpm_slider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 24);
        target_bpm_slider_.setRange(40.0, 200.0, 0.5);
        target_bpm_slider_.onValueChange = [this] {
            ctx_.mashup_target_bpm = (float)target_bpm_slider_.getValue();
        };

        addAndMakeVisible(target_key_lbl_);
        target_key_lbl_.setText("TARGET KEY", juce::dontSendNotification);
        target_key_lbl_.setFont(AR::font(AR::FontRole::section_label));
        target_key_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));

        addAndMakeVisible(target_key_combo_);
        target_key_combo_.addItem("Anchor to A", 1);
        int id = 2;
        for (auto* note : kNotes) target_key_combo_.addItem(note, id++);
        target_key_combo_.setSelectedItemIndex(0, juce::dontSendNotification);
        target_key_combo_.onChange = [this] {
            int idx = target_key_combo_.getSelectedItemIndex();
            if (idx <= 0) ctx_.mashup_target_key = "";
            else          ctx_.mashup_target_key = juce::String(kNotes[(size_t)(idx - 1)]);
        };

        addAndMakeVisible(status_lbl_);
        status_lbl_.setFont(AR::font(AR::FontRole::status));
        status_lbl_.setColour(juce::Label::textColourId, juce::Colour(AR::WARNING));
        status_lbl_.setVisible(false);

        addAndMakeVisible(back_btn_);
        back_btn_.setButtonText("< Back");
        back_btn_.setComponentID("ghost");
        back_btn_.onClick = [this] {
            if (ctx_.navigate) ctx_.navigate(ScreenId::StemsReady);
        };

        addAndMakeVisible(generate_btn_);
        generate_btn_.setButtonText("Generate >");
        generate_btn_.setComponentID("primary_mashup");
        generate_btn_.onClick = [this] { handleGenerate(); };
    }

    void onExit() override
    {
        stopTimer();
    }

    void onEnter() override
    {
        setRendering(false);
        a_header_lbl_.setText("TRACK A | " + filenameOf(ctx_.file_path),
                              juce::dontSendNotification);
        a_info_lbl_.setText("BPM " + juce::String(ctx_.detected_bpm, 1)
                            + "   KEY " + (ctx_.detected_key.isEmpty()
                                           ? juce::String("--") : ctx_.detected_key),
                            juce::dontSendNotification);

        b_header_lbl_.setText("TRACK B | " + filenameOf(ctx_.file_path_b),
                              juce::dontSendNotification);
        b_info_lbl_.setText("BPM " + juce::String(ctx_.detected_bpm_b, 1)
                            + "   KEY " + (ctx_.detected_key_b.isEmpty()
                                           ? juce::String("--") : ctx_.detected_key_b),
                            juce::dontSendNotification);

        target_bpm_slider_.setValue(
            ctx_.mashup_target_bpm > 0.0f ? ctx_.mashup_target_bpm
                                          : ctx_.detected_bpm,
            juce::dontSendNotification);
        ctx_.mashup_target_bpm = (float)target_bpm_slider_.getValue();

        if (ctx_.mashup_target_key.isEmpty())
            target_key_combo_.setSelectedItemIndex(0, juce::dontSendNotification);
        else {
            for (size_t i = 0; i < kNotes.size(); ++i) {
                if (ctx_.mashup_target_key == kNotes[i]) {
                    target_key_combo_.setSelectedItemIndex((int)(i + 1),
                                                           juce::dontSendNotification);
                    break;
                }
            }
        }

        for (size_t i = 0; i < kStemNames.size(); ++i) {
            applyGainToSlider(rows_a_[i], ctx_.mashup_gains_a, kStemNames[i]);
            applyGainToSlider(rows_b_[i], ctx_.mashup_gains_b, kStemNames[i]);
        }

        populateTemplateCombo();
        tempo_mod_slider_    .setValue(ctx_.mashup_bpm_modifier,             juce::dontSendNotification);
        master_pitch_slider_ .setValue(ctx_.mashup_master_pitch_offset_semi, juce::dontSendNotification);
        master_reverb_slider_.setValue(ctx_.mashup_master_reverb_mix,        juce::dontSendNotification);
        reverb_room_slider_  .setValue(ctx_.mashup_master_reverb_room,       juce::dontSendNotification);
        hpf_b_slider_        .setValue(ctx_.mashup_highpass_b_hz,            juce::dontSendNotification);

        status_lbl_.setVisible(false);
        generate_btn_.setEnabled(true);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(AR::BG));

        auto bounds = getLocalBounds();
        const int divX = bounds.getCentreX();
        g.setColour(juce::Colour(AR::SURFACE));
        g.drawLine((float)divX, (float)kHeaderH,
                   (float)divX, (float)(getHeight() - kFooterH), 1.0f);
    }

    void timerCallback() override
    {
        if (!is_rendering_) return;
        int elapsed = (int)((juce::Time::getMillisecondCounterHiRes()
                             - render_start_ms_) / 1000.0);
        rendering_elapsed_lbl_.setText(juce::String(elapsed) + " s",
                                       juce::dontSendNotification);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        const int W = b.getWidth();
        const int halfW = W / 2;

        header_lbl_.setBounds(20, 8, 140, kHeaderH - 8);

        // Template row (top-right of header band)
        template_lbl_  .setBounds(180, 12, 70, 20);
        template_combo_.setBounds(260, 8,  380, 28);
        advanced_btn_  .setBounds(W - 140, 8, 120, 28);

        // Column headers (info)
        int y = kHeaderH;
        a_header_lbl_.setBounds(20,        y,      halfW - 30, 18);
        a_info_lbl_  .setBounds(20,        y + 20, halfW - 30, 18);
        b_header_lbl_.setBounds(halfW + 10, y,      halfW - 30, 18);
        b_info_lbl_  .setBounds(halfW + 10, y + 20, halfW - 30, 18);
        y += 42;

        // 4 stem rows × 2 columns
        const int rowH = 36;
        for (size_t i = 0; i < kStemNames.size(); ++i) {
            int ry = y + (int)i * rowH;
            layoutStemRow(rows_a_[i], juce::Rectangle<int>(20,         ry, halfW - 30, rowH));
            layoutStemRow(rows_b_[i], juce::Rectangle<int>(halfW + 10, ry, halfW - 30, rowH));
        }
        y += rowH * (int)kStemNames.size() + 8;

        // Target BPM + Target Key (side by side)
        target_bpm_lbl_   .setBounds(20,         y,      120, 18);
        target_bpm_slider_.setBounds(20,         y + 20, halfW - 30, 22);
        target_key_lbl_   .setBounds(halfW + 10, y,      120, 18);
        target_key_combo_ .setBounds(halfW + 10, y + 20, 200, 22);
        y += 48;

        // Advanced section (5 sliders in 3 rows: tempo_mod | master_pitch ; reverb_mix | reverb_room ; hpf_b alone)
        if (advanced_open_) {
            const int advRow = 22;
            tempo_mod_lbl_      .setBounds(20,         y,         halfW - 30, 16);
            tempo_mod_slider_   .setBounds(20,         y + 16,    halfW - 30, advRow);
            master_pitch_lbl_   .setBounds(halfW + 10, y,         halfW - 30, 16);
            master_pitch_slider_.setBounds(halfW + 10, y + 16,    halfW - 30, advRow);
            y += 42;
            master_reverb_lbl_   .setBounds(20,         y,         halfW - 30, 16);
            master_reverb_slider_.setBounds(20,         y + 16,    halfW - 30, advRow);
            reverb_room_lbl_     .setBounds(halfW + 10, y,         halfW - 30, 16);
            reverb_room_slider_  .setBounds(halfW + 10, y + 16,    halfW - 30, advRow);
            y += 42;
            hpf_b_lbl_   .setBounds(20, y,      halfW - 30, 16);
            hpf_b_slider_.setBounds(20, y + 16, halfW - 30, advRow);
        }

        // Status + footer
        status_lbl_.setBounds(20, b.getBottom() - kFooterH + 4, W - 40, 20);
        back_btn_    .setBounds(20,      b.getBottom() - 44, 100, 32);
        generate_btn_.setBounds(W - 200, b.getBottom() - 44, 180, 32);

        // Rendering overlay (covers most of the screen)
        auto centerH = b.getHeight() - kHeaderH - kFooterH;
        rendering_header_lbl_ .setBounds(0, kHeaderH + centerH / 2 - 60, W, 60);
        rendering_elapsed_lbl_.setBounds(0, kHeaderH + centerH / 2 + 4,  W, 28);
        rendering_hint_lbl_   .setBounds(20, kHeaderH + centerH / 2 + 40, W - 40, 20);
    }

private:
    static constexpr int kHeaderH = 36;
    static constexpr int kFooterH = 72;

    static inline const std::array<const char*, 4> kStemNames {
        "vocals", "drums", "bass", "other"
    };
    static inline const std::array<const char*, 12> kNotes {
        "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
    };

    struct StemRow {
        const char*  name = "";
        juce::Label  lbl;
        juce::Slider gain;
    };

    void initStemRow(StemRow& row, const char* name, bool is_b)
    {
        row.name = name;

        addAndMakeVisible(row.lbl);
        row.lbl.setText(juce::String(name).toUpperCase(), juce::dontSendNotification);
        row.lbl.setFont(AR::font(AR::FontRole::mono_label));
        row.lbl.setColour(juce::Label::textColourId, juce::Colour(AR::FG));

        addAndMakeVisible(row.gain);
        row.gain.setSliderStyle(juce::Slider::LinearHorizontal);
        row.gain.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 22);
        row.gain.setRange(0.0, 2.0, 0.01);
        row.gain.setValue(1.0, juce::dontSendNotification);
        row.gain.onValueChange = [this, &row, is_b] {
            auto& dst = is_b ? ctx_.mashup_gains_b : ctx_.mashup_gains_a;
            dst[row.name] = (float)row.gain.getValue();
        };
    }

    static void layoutStemRow(StemRow& row, juce::Rectangle<int> r)
    {
        auto lblArea = r.removeFromLeft(70);
        row.lbl.setBounds(lblArea.reduced(0, 8));
        row.gain.setBounds(r.reduced(4, 6));
    }

    void initAdvancedSlider(juce::Label& lbl, const char* text, juce::Slider& slider,
                            double minV, double maxV, double step, double def,
                            std::function<void()> on_change)
    {
        addAndMakeVisible(lbl);
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(AR::font(AR::FontRole::mono_label));
        lbl.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));
        lbl.setVisible(false);

        addAndMakeVisible(slider);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 22);
        slider.setRange(minV, maxV, step);
        slider.setValue(def, juce::dontSendNotification);
        slider.setVisible(false);
        slider.onValueChange = std::move(on_change);
    }

    void populateTemplateCombo()
    {
        template_combo_.clear(juce::dontSendNotification);
        template_combo_.addItem("Custom", 1);
        for (size_t i = 0; i < ctx_.mashup_presets.size(); ++i)
            template_combo_.addItem(ctx_.mashup_presets[i].name, (int)(i + 2));
        template_combo_.setSelectedItemIndex(0, juce::dontSendNotification);
    }

    void applyTemplate(int comboIdx)
    {
        if (comboIdx <= 0 || (size_t)(comboIdx - 1) >= ctx_.mashup_presets.size()) {
            ctx_.selected_mashup_preset_idx = -1;
            return;
        }
        const auto& p = ctx_.mashup_presets[(size_t)(comboIdx - 1)];
        ctx_.selected_mashup_preset_idx = comboIdx - 1;
        ctx_.mashup_gains_a = p.stem_gains_a;
        ctx_.mashup_gains_b = p.stem_gains_b;
        ctx_.mashup_bpm_modifier             = p.bpm_modifier;
        ctx_.mashup_master_pitch_offset_semi = p.master_pitch_offset_semi;
        ctx_.mashup_master_reverb_mix        = p.master_reverb_mix;
        ctx_.mashup_master_reverb_room       = p.master_reverb_room;
        ctx_.mashup_highpass_b_hz            = p.highpass_b_hz;
        if (p.target_bpm_mode == "absolute" && p.target_bpm_absolute > 0.0f)
            ctx_.mashup_target_bpm = p.target_bpm_absolute;
        if (p.target_key_mode == "absolute" && !p.target_key_absolute.empty())
            ctx_.mashup_target_key = juce::String(p.target_key_absolute);

        // Refresh sliders from ctx_
        for (size_t i = 0; i < kStemNames.size(); ++i) {
            applyGainToSlider(rows_a_[i], ctx_.mashup_gains_a, kStemNames[i]);
            applyGainToSlider(rows_b_[i], ctx_.mashup_gains_b, kStemNames[i]);
        }
        tempo_mod_slider_    .setValue(ctx_.mashup_bpm_modifier,             juce::dontSendNotification);
        master_pitch_slider_ .setValue(ctx_.mashup_master_pitch_offset_semi, juce::dontSendNotification);
        master_reverb_slider_.setValue(ctx_.mashup_master_reverb_mix,        juce::dontSendNotification);
        reverb_room_slider_  .setValue(ctx_.mashup_master_reverb_room,       juce::dontSendNotification);
        hpf_b_slider_        .setValue(ctx_.mashup_highpass_b_hz,            juce::dontSendNotification);
        target_bpm_slider_   .setValue(ctx_.mashup_target_bpm,               juce::dontSendNotification);
    }

    void updateAdvancedVisibility()
    {
        const bool on = advanced_open_ && !is_rendering_;
        for (auto* c : advancedComponents()) c->setVisible(on);
    }

    std::array<juce::Component*, 10> advancedComponents()
    {
        return { &tempo_mod_lbl_,    &tempo_mod_slider_,
                 &master_pitch_lbl_, &master_pitch_slider_,
                 &master_reverb_lbl_,&master_reverb_slider_,
                 &reverb_room_lbl_,  &reverb_room_slider_,
                 &hpf_b_lbl_,        &hpf_b_slider_ };
    }

    void setRendering(bool on)
    {
        is_rendering_ = on;
        const bool show_config = !on;

        // Header + analysis
        header_lbl_  .setVisible(show_config);
        a_header_lbl_.setVisible(show_config);
        a_info_lbl_  .setVisible(show_config);
        b_header_lbl_.setVisible(show_config);
        b_info_lbl_  .setVisible(show_config);

        // 8 stem rows
        for (auto& r : rows_a_) { r.lbl.setVisible(show_config); r.gain.setVisible(show_config); }
        for (auto& r : rows_b_) { r.lbl.setVisible(show_config); r.gain.setVisible(show_config); }

        // Target controls
        target_bpm_lbl_   .setVisible(show_config);
        target_bpm_slider_.setVisible(show_config);
        target_key_lbl_   .setVisible(show_config);
        target_key_combo_ .setVisible(show_config);
        status_lbl_       .setVisible(show_config && status_lbl_.getText().isNotEmpty());
        back_btn_         .setVisible(show_config);
        generate_btn_     .setVisible(show_config);

        // Template combo + advanced toggle
        template_lbl_   .setVisible(show_config);
        template_combo_ .setVisible(show_config);
        advanced_btn_   .setVisible(show_config);
        updateAdvancedVisibility();

        // Rendering overlay
        rendering_header_lbl_ .setVisible(on);
        rendering_elapsed_lbl_.setVisible(on);
        rendering_hint_lbl_   .setVisible(on);

        if (on) {
            render_start_ms_ = juce::Time::getMillisecondCounterHiRes();
            rendering_elapsed_lbl_.setText("0 s", juce::dontSendNotification);
            startTimer(500);
        } else {
            stopTimer();
        }
        repaint();
    }

    static juce::String filenameOf(const juce::String& path)
    {
        if (path.isEmpty()) return "(no file)";
        return juce::File(path).getFileName();
    }

    static void applyGainToSlider(StemRow& row,
                                  const std::unordered_map<std::string, float>& gains,
                                  const char* key)
    {
        auto it = gains.find(key);
        float v = (it != gains.end()) ? it->second : 1.0f;
        row.gain.setValue(v, juce::dontSendNotification);
    }

    void handleGenerate()
    {
        if (!ctx_.run_mashup) {
            status_lbl_.setText("run_mashup callback not wired",
                                juce::dontSendNotification);
            status_lbl_.setVisible(true);
            return;
        }
        if (ctx_.file_path.isEmpty() || ctx_.file_path_b.isEmpty()) {
            status_lbl_.setText("Both files A and B must be loaded",
                                juce::dontSendNotification);
            status_lbl_.setVisible(true);
            return;
        }

        generate_btn_.setEnabled(false);
        status_lbl_.setText("", juce::dontSendNotification);
        setRendering(true);

        autoremix::MashupParams params;
        params.file_a = std::filesystem::path(ctx_.file_path.toStdString());
        params.file_b = std::filesystem::path(ctx_.file_path_b.toStdString());
        if (!ctx_.separators.empty()
            && ctx_.selected_separator_idx >= 0
            && (size_t)ctx_.selected_separator_idx < ctx_.separators.size())
            params.separator_id = ctx_.separators[(size_t)ctx_.selected_separator_idx].id;
        params.stem_gains_a            = ctx_.mashup_gains_a;
        params.stem_gains_b            = ctx_.mashup_gains_b;
        params.target_bpm              = ctx_.mashup_target_bpm;
        params.has_target_bpm          = ctx_.mashup_target_bpm > 0.0f;
        params.target_key              = ctx_.mashup_target_key.toStdString();
        params.bpm_modifier            = ctx_.mashup_bpm_modifier;
        params.master_pitch_offset_semi = ctx_.mashup_master_pitch_offset_semi;
        params.master_reverb_mix       = ctx_.mashup_master_reverb_mix;
        params.master_reverb_room      = ctx_.mashup_master_reverb_room;
        params.highpass_b_hz           = ctx_.mashup_highpass_b_hz;

        ctx_.run_mashup(std::move(params),
                        [this](autoremix::MashupResult r) { handleResult(std::move(r)); });
    }

    void handleResult(autoremix::MashupResult r)
    {
        setRendering(false);
        if (!r.success) {
            status_lbl_.setText("Error: " + juce::String(r.error_message),
                                juce::dontSendNotification);
            status_lbl_.setColour(juce::Label::textColourId,
                                  juce::Colour(AR::ERROR));
            status_lbl_.setVisible(true);
            generate_btn_.setEnabled(true);
            return;
        }
        ctx_.mashup_output_path = juce::String(r.output_path.string());
        ctx_.output_path        = ctx_.mashup_output_path;
        ctx_.render_is_mashup   = true;
        if (ctx_.navigate) ctx_.navigate(ScreenId::Render);
    }

    juce::Label header_lbl_;

    juce::Label a_header_lbl_, a_info_lbl_;
    juce::Label b_header_lbl_, b_info_lbl_;

    std::array<StemRow, 4> rows_a_;
    std::array<StemRow, 4> rows_b_;

    juce::Label  target_bpm_lbl_;
    juce::Slider target_bpm_slider_;

    juce::Label    target_key_lbl_;
    juce::ComboBox target_key_combo_;

    juce::Label      status_lbl_;
    juce::TextButton back_btn_;
    juce::TextButton generate_btn_;

    juce::Label rendering_header_lbl_;
    juce::Label rendering_elapsed_lbl_;
    juce::Label rendering_hint_lbl_;
    bool        is_rendering_   = false;
    double      render_start_ms_ = 0.0;

    juce::Label    template_lbl_;
    juce::ComboBox template_combo_;
    juce::TextButton advanced_btn_;
    bool advanced_open_ = false;

    juce::Label  tempo_mod_lbl_;    juce::Slider tempo_mod_slider_;
    juce::Label  master_pitch_lbl_; juce::Slider master_pitch_slider_;
    juce::Label  master_reverb_lbl_;juce::Slider master_reverb_slider_;
    juce::Label  reverb_room_lbl_;  juce::Slider reverb_room_slider_;
    juce::Label  hpf_b_lbl_;        juce::Slider hpf_b_slider_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScreenMashup)
};
