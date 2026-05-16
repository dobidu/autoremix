#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <thread>
#include <filesystem>

//==============================================================================
AutoRemixAudioProcessorEditor::AutoRemixAudioProcessorEditor(AutoRemixAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    format_manager_.registerBasicFormats();
    thumbnail_.addChangeListener(this);
    setLookAndFeel(&laf_);
    juce::Component::setSize(600, 400);
    drawAndConfigComponents();

    std::thread([this]() {
        auto& bridge   = audioProcessor.getBridge();
        auto presets   = bridge.getPresets();
        auto seps      = bridge.getAvailableSeparators();
        if (presets.empty() && seps.empty()) return;
        juce::MessageManager::callAsync([this, presets = std::move(presets),
                                               seps    = std::move(seps)]() mutable {
            if (!presets.empty()) {
                presets_ = std::move(presets);
                style_combo_.clear(juce::dontSendNotification);
                for (int i = 0; i < (int)presets_.size(); ++i)
                    style_combo_.addItem(presets_[(size_t)i].name, i + 1);
                style_combo_.setSelectedItemIndex(0, juce::dontSendNotification);
                loadEngineDefaults(0);
            }
            if (!seps.empty()) {
                separators_ = std::move(seps);
                separator_combo_.clear(juce::dontSendNotification);
                for (int i = 0; i < (int)separators_.size(); ++i)
                    separator_combo_.addItem(separators_[(size_t)i].display_name, i + 1);
                separator_combo_.setSelectedItemIndex(0, juce::dontSendNotification);
            }
        });
    }).detach();
}

AutoRemixAudioProcessorEditor::~AutoRemixAudioProcessorEditor()
{
    thumbnail_.removeChangeListener(this);
    setLookAndFeel(nullptr);
}

//==============================================================================
void AutoRemixAudioProcessorEditor::loadFile()
{
#if defined(_WIN32) || defined(__APPLE__)
    constexpr bool useNativeDialog = true;
#else
    constexpr bool useNativeDialog = false;  // WSL2/Linux: native dialog fails silently
#endif
    chooser_ = std::make_unique<juce::FileChooser>(
        "Load audio file...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.wav;*.aif;*.aiff;*.mp3;*.flac",
        useNativeDialog);

    chooser_->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            auto results = fc.getResults();
            if (!results.isEmpty()) {
                auto f = results[0];
                file_path_ = f.getFullPathName();
                file_lbl.setText(f.getFileName(), juce::dontSendNotification);
                output_path_ = {};
                save_btn.setEnabled(false);
                thumbnail_.setSource(new juce::FileInputSource(f));
                waveform_display_.sourceChanged();
            }
        });
}

void AutoRemixAudioProcessorEditor::drawAndConfigComponents()
{
    // ── Header: title
    addAndMakeVisible(title_lbl);
    title_lbl.setText("AutoRemix", juce::dontSendNotification);
    title_lbl.setFont(AR::font(AR::FontRole::header));
    title_lbl.setColour(juce::Label::textColourId, juce::Colour(AR::FG));
    title_lbl.setBounds(16, 0, 120, 40);

    // ── Header: remix style combobox
    addAndMakeVisible(style_combo_);
    style_combo_.setBounds(148, 6, 250, 28);
    style_combo_.addItem("Chop & Screw",    1);
    style_combo_.addItem("Slowed + Reverb", 2);
    style_combo_.addItem("Drum & Bass",     3);
    style_combo_.setSelectedItemIndex(0, juce::dontSendNotification);
    style_combo_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(AR::ELEVATED));
    style_combo_.setColour(juce::ComboBox::outlineColourId,    juce::Colour(AR::SURFACE));
    style_combo_.setColour(juce::ComboBox::arrowColourId,      juce::Colour(AR::PURPLE));
    style_combo_.onChange = [this] { loadEngineDefaults(style_combo_.getSelectedItemIndex()); };

    // ── Header: separator combobox (populated from sidecar health on connect)
    addAndMakeVisible(separator_combo_);
    separator_combo_.setBounds(406, 6, 116, 28);
    separator_combo_.addItem("Algorithmic FFT", 1);
    separator_combo_.setSelectedItemIndex(0, juce::dontSendNotification);
    separator_combo_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(AR::ELEVATED));
    separator_combo_.setColour(juce::ComboBox::outlineColourId,    juce::Colour(AR::SURFACE));
    separator_combo_.setColour(juce::ComboBox::arrowColourId,      juce::Colour(AR::CYAN));

    // ── Header: save preset button
    addAndMakeVisible(save_preset_btn);
    save_preset_btn.setButtonText("Save");
    save_preset_btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(AR::PURPLE));
    save_preset_btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(AR::PURPLE));
    save_preset_btn.setBounds(526, 6, 44, 28);
    save_preset_btn.onClick = [this] { onClick_SavePreset(); };

    // ── Header: sidecar health dot
    addAndMakeVisible(health_dot_);
    health_dot_.setBounds(getWidth() - 24, 16, 8, 8);

    // ── Waveform zone
    addAndMakeVisible(waveform_display_);
    waveform_display_.setBounds(8, 48, getWidth() - 16, 144);

    // ── Controls zone — filename row
    addAndMakeVisible(file_lbl);
    file_lbl.setFont(AR::font(AR::FontRole::value));
    file_lbl.setColour(juce::Label::textColourId, juce::Colour(AR::FG));
    file_lbl.setJustificationType(juce::Justification::centredLeft);
    file_lbl.setBounds(96, 203, 488, 14);

    // ── Controls zone — remix params (left col x=96 w=220) + stem mix (right col x=328 w=256)
    //    rows at y = 218, 246, 274, 302  (spacing 28px)
    auto setupRemixSlider = [this](juce::Slider& s, juce::Label& lbl,
                                    const juce::String& name,
                                    double lo, double hi, double val, int y) {
        addAndMakeVisible(lbl);
        lbl.setText(name, juce::dontSendNotification);
        lbl.setFont(AR::font(AR::FontRole::label));
        lbl.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));
        lbl.setBounds(96, y, 220, 11);

        addAndMakeVisible(s);
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 16);
        s.setRange(lo, hi, 0.0);
        s.setValue(val, juce::dontSendNotification);
        s.setBounds(96, y + 12, 220, 18);
    };

    setupRemixSlider(tempo_slider_,  tempo_lbl_,  "Tempo",   0.3,    2.0,    0.70,  218);
    setupRemixSlider(pitch_slider_,  pitch_lbl_,  "Pitch",  -12.0,  12.0,   -4.0,  246);
    setupRemixSlider(reverb_slider_, reverb_lbl_, "Reverb",  0.0,    1.0,    0.05,  274);
    // ── Chop mode selector (replaces chop_lbl_)
    addAndMakeVisible(chop_mode_combo_);
    chop_mode_combo_.setBounds(96, 302, 220, 20);
    chop_mode_combo_.addItem("Fixed (ms)",      1);
    chop_mode_combo_.addItem("Beat-Aligned",    2);
    chop_mode_combo_.addItem("Onset-Triggered", 3);
    chop_mode_combo_.addItem("Bar-Locked",      4);
    chop_mode_combo_.addItem("Energy Gate",     5);
    chop_mode_combo_.addItem("Structural",      6);
    chop_mode_combo_.setSelectedItemIndex(0, juce::dontSendNotification);
    chop_mode_combo_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(AR::ELEVATED));
    chop_mode_combo_.setColour(juce::ComboBox::outlineColourId,    juce::Colour(AR::SURFACE));
    chop_mode_combo_.setColour(juce::ComboBox::arrowColourId,      juce::Colour(AR::PURPLE));
    chop_mode_combo_.onChange = [this] {
        bool isFixed = (chop_mode_combo_.getSelectedItemIndex() == 0);
        chop_slider_.setEnabled(isFixed);
        chop_slider_.setAlpha(isFixed ? 1.0f : 0.4f);
    };

    // ── Chop ms slider (below combo, enabled only in Fixed mode)
    addAndMakeVisible(chop_slider_);
    chop_slider_.setSliderStyle(juce::Slider::LinearHorizontal);
    chop_slider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 16);
    chop_slider_.setRange(0.0, 4000.0, 0.0);
    chop_slider_.setValue(2000.0, juce::dontSendNotification);
    chop_slider_.setBounds(96, 323, 220, 14);

    auto setupStemSlider = [this](juce::Slider& s, juce::Label& lbl,
                                   const juce::String& name, int y) {
        addAndMakeVisible(lbl);
        lbl.setText(name, juce::dontSendNotification);
        lbl.setFont(AR::font(AR::FontRole::label));
        lbl.setColour(juce::Label::textColourId, juce::Colour(AR::CYAN));
        lbl.setBounds(328, y, 256, 11);

        addAndMakeVisible(s);
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 16);
        s.setRange(0.0, 2.0, 0.0);
        s.setValue(1.0, juce::dontSendNotification);
        s.setBounds(328, y + 12, 256, 18);
    };

    setupStemSlider(vocals_slider_, vocals_lbl_, "Vocals", 218);
    setupStemSlider(drums_slider_,  drums_lbl_,  "Drums",  246);
    setupStemSlider(bass_slider_,   bass_lbl_,   "Bass",   274);
    setupStemSlider(other_slider_,  other_lbl_,  "Other",  302);

    // ── Transport strip: Load / Play / Save (aligned with slider rows)
    addAndMakeVisible(loadfile_btn);
    loadfile_btn.setButtonText("Load");
    loadfile_btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(AR::SURFACE));
    loadfile_btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(AR::SURFACE));
    loadfile_btn.onClick = [this] { loadFile(); };
    loadfile_btn.setBounds(8, 218, 72, 26);

    addAndMakeVisible(play_btn);
    play_btn.setButtonText("Remix");
    play_btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(AR::GREEN));
    play_btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(AR::GREEN));
    play_btn.onClick = [this] { onClick_Play(); };
    play_btn.setBounds(8, 250, 72, 26);

    addAndMakeVisible(save_btn);
    save_btn.setButtonText("Save");
    save_btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(AR::SURFACE));
    save_btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(AR::SURFACE));
    save_btn.setEnabled(false);
    save_btn.onClick = [this] { onClick_Save(); };
    save_btn.setBounds(8, 284, 72, 26);

    addAndMakeVisible(preview_btn);
    preview_btn.setButtonText("Preview");
    preview_btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(AR::ELEVATED));
    preview_btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(AR::ELEVATED));
    preview_btn.setEnabled(false);
    preview_btn.onClick = [this] {
        audioProcessor.togglePreview();
        preview_btn.setButtonText(audioProcessor.isPreviewPlaying() ? "Stop" : "Preview");
    };
    preview_btn.setBounds(8, 312, 72, 20);

    // ── Status zone
    addAndMakeVisible(status_lbl);
    status_lbl.setText("Ready", juce::dontSendNotification);
    status_lbl.setFont(AR::font(AR::FontRole::status));
    status_lbl.setJustificationType(juce::Justification::centredLeft);
    status_lbl.setColour(juce::Label::textColourId, juce::Colour(AR::FG));
    status_lbl.setBounds(16, 344, getWidth() - 32, 20);

    addAndMakeVisible(progress_bar_);
    progress_bar_.setVisible(false);
    progress_bar_.setBounds(16, 368, getWidth() - 32, 12);
}

//==============================================================================
void AutoRemixAudioProcessorEditor::loadEngineDefaults(int idx)
{
    if (presets_.empty() || idx < 0 || (size_t)idx >= presets_.size()) return;
    auto& p = presets_[(size_t)idx].default_params;
    tempo_slider_.setValue(p.tempo_factor,     juce::dontSendNotification);
    pitch_slider_.setValue(p.pitch_shift_semi, juce::dontSendNotification);
    reverb_slider_.setValue(p.reverb_mix,      juce::dontSendNotification);
    chop_slider_.setValue(p.chop_interval_ms,  juce::dontSendNotification);
}

void AutoRemixAudioProcessorEditor::onClick_Play()
{
    if (file_path_.isEmpty()) {
        status_lbl.setText("No file loaded.", juce::dontSendNotification);
        return;
    }

    auto& bridge = audioProcessor.getBridge();
    if (!bridge.isServerAlive()) {
        status_lbl.setText("Sidecar not running on port 17432.", juce::dontSendNotification);
        return;
    }

    auto idx = static_cast<std::size_t>(style_combo_.getSelectedItemIndex());
    if (presets_.empty() || idx >= presets_.size()) {
        status_lbl.setText("Presets not loaded - start sidecar first.", juce::dontSendNotification);
        return;
    }
    auto& preset = presets_[idx];

    auto sep_idx = separator_combo_.getSelectedItemIndex();
    std::string sep_id = (separators_.empty() || sep_idx < 0)
        ? "algorithmic"
        : separators_[(size_t)sep_idx].id;

    static const char* kChopModes[] = {"fixed","beat","onset","bar","energy","structural"};
    int chopIdx = chop_mode_combo_.getSelectedItemIndex();
    std::string chop_mode_str = (chopIdx >= 0 && chopIdx < 6) ? kChopModes[chopIdx] : "fixed";

    autoremix::RemixParams params {
        (float)tempo_slider_.getValue(),
        (float)pitch_slider_.getValue(),
        (float)reverb_slider_.getValue(),
        (float)chop_slider_.getValue(),
        preset.default_params.bass_boost_db,
        preset.default_params.drums_tempo_factor,
        preset.id,
        sep_id,
        (float)vocals_slider_.getValue(),
        (float)drums_slider_.getValue(),
        (float)bass_slider_.getValue(),
        (float)other_slider_.getValue(),
    };
    params.chop_mode = chop_mode_str;

    auto juce_tmp   = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("autoremix");
    auto input_file = juce::File(file_path_);
    std::filesystem::path stems_dir  = juce_tmp.getChildFile("stems")
                                           .getFullPathName().toStdString();
    std::filesystem::path output_path = juce_tmp.getChildFile("remix")
                                            .getFullPathName().toStdString();
    output_path /= input_file.getFileNameWithoutExtension().toStdString()
                   + "_" + preset.id + ".wav";

    play_btn.setEnabled(false);
    progress_ = -1.0;
    progress_bar_.setVisible(true);
    status_lbl.setText("Separating stems...", juce::dontSendNotification);

    std::string input_str = file_path_.toStdString();

    std::thread([this, params, stems_dir, output_path, input_str, sep_id]() mutable {
        auto& bridge = audioProcessor.getBridge();

        auto stems = bridge.separateStems(
            std::filesystem::path(input_str),
            stems_dir,
            sep_id);

        if (!stems.valid) {
            juce::MessageManager::callAsync([this] {
                status_lbl.setText("Error: separation failed.", juce::dontSendNotification);
                progress_bar_.setVisible(false);
                play_btn.setEnabled(true);
            });
            return;
        }

        juce::MessageManager::callAsync([this] {
            status_lbl.setText("Remixing...", juce::dontSendNotification);
        });

        auto result = bridge.applyRemix(stems, params, output_path);

        juce::MessageManager::callAsync([this, result]() mutable {
            progress_bar_.setVisible(false);
            if (result.success) {
                output_path_ = juce::String(result.output_path.string());
                status_lbl.setText("Done - click Save to export.", juce::dontSendNotification);
                save_btn.setEnabled(true);
                audioProcessor.loadPreviewFile(juce::File(output_path_));
                preview_btn.setEnabled(true);
                preview_btn.setButtonText("Preview");
            } else {
                status_lbl.setText("Error: " + juce::String(result.error_message), juce::dontSendNotification);
            }
            play_btn.setEnabled(true);
        });
    }).detach();
}

void AutoRemixAudioProcessorEditor::onClick_Save()
{
    if (output_path_.isEmpty()) return;

    chooser_ = std::make_unique<juce::FileChooser>(
        "Save remixed file...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory)
            .getChildFile(juce::File(output_path_).getFileName()),
        "*.wav",
        false);  // native dialog fails silently on WSL2

    chooser_->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            auto results = fc.getResults();
            if (!results.isEmpty()) {
                juce::File(output_path_).copyFileTo(results[0]);
                status_lbl.setText("Saved: " + results[0].getFileName(), juce::dontSendNotification);
            }
        });
}

void AutoRemixAudioProcessorEditor::onClick_SavePreset()
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

    auto idx = static_cast<std::size_t>(style_combo_.getSelectedItemIndex());
    if (presets_.empty() || idx >= presets_.size()) return;
    auto& preset = presets_[idx];

    autoremix::RemixParams params {
        (float)tempo_slider_.getValue(),
        (float)pitch_slider_.getValue(),
        (float)reverb_slider_.getValue(),
        (float)chop_slider_.getValue(),
        preset.default_params.bass_boost_db,
        preset.default_params.drums_tempo_factor,
        preset.id,
        "",
        (float)vocals_slider_.getValue(),
        (float)drums_slider_.getValue(),
        (float)bass_slider_.getValue(),
        (float)other_slider_.getValue(),
    };

    status_lbl.setText("Saving preset...", juce::dontSendNotification);

    std::string name_str = name.toStdString();
    std::thread([this, name_str, params]() mutable {
        auto& bridge = audioProcessor.getBridge();
        bool ok = bridge.savePreset(name_str, params);
        auto newPresets = bridge.getPresets();

        juce::MessageManager::callAsync([this, ok, name_str,
                                               newPresets = std::move(newPresets)]() mutable {
            if (ok && !newPresets.empty()) {
                presets_ = std::move(newPresets);
                style_combo_.clear(juce::dontSendNotification);
                for (int i = 0; i < (int)presets_.size(); ++i)
                    style_combo_.addItem(presets_[(size_t)i].name, i + 1);
                for (int i = 0; i < (int)presets_.size(); ++i) {
                    if (presets_[(size_t)i].name == name_str) {
                        style_combo_.setSelectedItemIndex(i, juce::dontSendNotification);
                        loadEngineDefaults(i);
                        break;
                    }
                }
                status_lbl.setText("Preset saved: " + juce::String(name_str),
                                   juce::dontSendNotification);
            } else {
                status_lbl.setText("Error: preset save failed.",
                                   juce::dontSendNotification);
            }
        });
    }).detach();
}

//==============================================================================
void AutoRemixAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(AR::BG));

    // header zone
    g.setColour(juce::Colour(AR::ELEVATED));
    g.fillRect(0, 0, getWidth(), 40);

    // transport strip bg
    g.setColour(juce::Colour(AR::ELEVATED));
    g.fillRect(0, 200, 88, 136);

    // separators
    g.setColour(juce::Colour(AR::SURFACE));
    g.fillRect(0, 40,  getWidth(), 1);   // header / waveform
    g.fillRect(0, 200, getWidth(), 1);   // waveform / controls
    g.fillRect(88, 200, 1, 136);         // transport / params vertical
    g.fillRect(0, 336, getWidth(), 1);   // controls / status
}

void AutoRemixAudioProcessorEditor::resized() {}
