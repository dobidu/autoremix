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
        auto presets = audioProcessor.getBridge().getPresets();
        if (presets.empty()) return;
        juce::MessageManager::callAsync([this, presets = std::move(presets)]() mutable {
            presets_ = std::move(presets);
            std::vector<std::string> names;
            names.reserve(presets_.size());
            for (auto& p : presets_) names.push_back(p.name);
            style_tab_.setLabels(std::move(names));
            loadEngineDefaults(0);
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

    // ── Header: style tab bar (narrowed to leave room for health dot)
    addAndMakeVisible(style_tab_);
    style_tab_.setBounds(148, 4, 400, 32);
    style_tab_.onChange = [this](int idx) { loadEngineDefaults(idx); };

    // ── Header: sidecar health dot
    addAndMakeVisible(health_dot_);
    health_dot_.setBounds(getWidth() - 24, 16, 8, 8);

    // ── Waveform zone
    addAndMakeVisible(waveform_display_);
    waveform_display_.setBounds(8, 48, getWidth() - 16, 144);

    // ── Params zone: 4 sliders (x=96..584, y=200..336)
    auto setupSlider = [this](juce::Slider& s, juce::Label& lbl,
                               const juce::String& name,
                               double lo, double hi, double val, int y) {
        addAndMakeVisible(lbl);
        lbl.setText(name, juce::dontSendNotification);
        lbl.setFont(AR::font(AR::FontRole::label));
        lbl.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));
        lbl.setBounds(96, y, 488, 11);

        addAndMakeVisible(s);
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 16);
        s.setRange(lo, hi, 0.0);
        s.setValue(val, juce::dontSendNotification);
        s.setBounds(96, y + 12, 488, 18);
    };

    setupSlider(tempo_slider_,  tempo_lbl_,  "Tempo",   0.3,    2.0,    0.70,   204);
    setupSlider(pitch_slider_,  pitch_lbl_,  "Pitch",  -12.0,  12.0,   -4.0,   237);
    setupSlider(reverb_slider_, reverb_lbl_, "Reverb",  0.0,    1.0,    0.05,   270);
    setupSlider(chop_slider_,   chop_lbl_,   "Chop ms", 0.0, 4000.0, 2000.0,   303);

    // ── Controls zone right: filename label
    addAndMakeVisible(file_lbl);
    file_lbl.setFont(AR::font(AR::FontRole::value));
    file_lbl.setColour(juce::Label::textColourId, juce::Colour(AR::FG));
    file_lbl.setJustificationType(juce::Justification::centredLeft);
    file_lbl.setBounds(100, 204, 492, 22);

    // ── Transport strip: Load / Play / Save
    addAndMakeVisible(loadfile_btn);
    loadfile_btn.setButtonText("Load");
    loadfile_btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(AR::SURFACE));
    loadfile_btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(AR::SURFACE));
    loadfile_btn.onClick = [this] { loadFile(); };
    loadfile_btn.setBounds(8, 212, 72, 26);

    addAndMakeVisible(play_btn);
    play_btn.setButtonText("Play");
    play_btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(AR::GREEN));
    play_btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(AR::GREEN));
    play_btn.onClick = [this] { onClick_Play(); };
    play_btn.setBounds(8, 246, 72, 26);

    addAndMakeVisible(save_btn);
    save_btn.setButtonText("Save");
    save_btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(AR::SURFACE));
    save_btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(AR::SURFACE));
    save_btn.setEnabled(false);
    save_btn.onClick = [this] { onClick_Save(); };
    save_btn.setBounds(8, 280, 72, 26);

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

    auto idx = static_cast<std::size_t>(style_tab_.getSelectedIndex());
    if (presets_.empty() || idx >= presets_.size()) {
        status_lbl.setText("Presets not loaded \xe2\x80\x94 start sidecar first.", juce::dontSendNotification);
        return;
    }
    auto& preset = presets_[idx];
    autoremix::RemixParams params {
        (float)tempo_slider_.getValue(),
        (float)pitch_slider_.getValue(),
        (float)reverb_slider_.getValue(),
        (float)chop_slider_.getValue(),
        preset.default_params.bass_boost_db,
        preset.default_params.drums_tempo_factor,
        preset.id,
        "algorithmic"
    };

    auto input_file = juce::File(file_path_);
    std::filesystem::path output_path =
        std::filesystem::path("/tmp/autoremix/remix") /
        (input_file.getFileNameWithoutExtension().toStdString() + "_" + preset.id + ".wav");

    play_btn.setEnabled(false);
    progress_ = -1.0;
    progress_bar_.setVisible(true);
    status_lbl.setText("Separating stems\xe2\x80\xa6", juce::dontSendNotification);

    std::string input_str = file_path_.toStdString();

    std::thread([this, params, output_path, input_str]() mutable {
        auto& bridge = audioProcessor.getBridge();

        auto stems = bridge.separateStems(
            std::filesystem::path(input_str),
            std::filesystem::path("/tmp/autoremix/stems"),
            "algorithmic");

        if (!stems.valid) {
            juce::MessageManager::callAsync([this] {
                status_lbl.setText("Error: separation failed.", juce::dontSendNotification);
                progress_bar_.setVisible(false);
                play_btn.setEnabled(true);
            });
            return;
        }

        juce::MessageManager::callAsync([this] {
            status_lbl.setText("Remixing\xe2\x80\xa6", juce::dontSendNotification);
        });

        auto result = bridge.applyRemix(stems, params, output_path);

        juce::MessageManager::callAsync([this, result]() mutable {
            progress_bar_.setVisible(false);
            if (result.success) {
                output_path_ = juce::String(result.output_path.string());
                status_lbl.setText("Done \xe2\x80\x94 click Save to export.", juce::dontSendNotification);
                save_btn.setEnabled(true);
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
