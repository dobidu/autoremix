#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <thread>
#include <array>
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
}

AutoRemixAudioProcessorEditor::~AutoRemixAudioProcessorEditor()
{
    thumbnail_.removeChangeListener(this);
    setLookAndFeel(nullptr);
}

//==============================================================================
void AutoRemixAudioProcessorEditor::loadFile()
{
    chooser_ = std::make_unique<juce::FileChooser>(
        "Load audio file...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.wav;*.aif;*.aiff;*.mp3;*.flac",
        false);  // native dialog fails silently on WSL2

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
                repaint();
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

    // ── Header: style selector
    addAndMakeVisible(remix_selector_lbl);
    remix_selector_lbl.setText("Style:", juce::dontSendNotification);
    remix_selector_lbl.setFont(AR::font(AR::FontRole::section));
    remix_selector_lbl.setColour(juce::Label::textColourId, juce::Colour(AR::COMMENT));
    remix_selector_lbl.setBounds(148, 8, 44, 24);

    addAndMakeVisible(remix_selector);
    remix_selector.addItem("Chopped & Screwed", 1);
    remix_selector.addItem("Slowed + Reverb",   2);
    remix_selector.addItem("Drum and Bass",      3);
    remix_selector.setSelectedId(1, juce::dontSendNotification);
    remix_selector.setBounds(196, 7, 280, 26);

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

    struct EngineConfig {
        std::string id;
        autoremix::RemixParams params;
    };
    static const std::array<EngineConfig, 3> engines {{
        {"chopped_screwed", {0.70f, -4.0f, 0.05f, 2000.0f, 0.0f, 1.0f, "chopped_screwed", "algorithmic"}},
        {"slowed_reverb",   {0.75f, -2.0f, 0.60f,    0.0f, 0.0f, 1.0f, "slowed_reverb",   "algorithmic"}},
        {"drum_and_bass",   {1.40f,  2.0f, 0.00f,    0.0f, 6.0f, 2.0f, "drum_and_bass",   "algorithmic"}},
    }};
    int idx = remix_selector.getSelectedId() - 1;
    auto cfg = engines[static_cast<std::size_t>(idx)];

    auto input_file = juce::File(file_path_);
    std::filesystem::path output_path =
        std::filesystem::path("/tmp/autoremix/remix") /
        (input_file.getFileNameWithoutExtension().toStdString() + "_" + cfg.id + ".wav");

    play_btn.setEnabled(false);
    progress_ = -1.0;
    progress_bar_.setVisible(true);
    status_lbl.setText("Separating stems\xe2\x80\xa6", juce::dontSendNotification);

    std::string input_str = file_path_.toStdString();

    std::thread([this, cfg, output_path, input_str]() mutable {
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

        auto result = bridge.applyRemix(stems, cfg.params, output_path);

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

    // waveform zone
    juce::Rectangle<int> waveArea(8, 48, getWidth() - 16, 144);
    g.setColour(juce::Colour(AR::BG_DEEP));
    g.fillRoundedRectangle(waveArea.toFloat(), 4.0f);
    if (thumbnail_.getTotalLength() > 0.0) {
        g.setColour(juce::Colour(AR::PURPLE));
        thumbnail_.drawChannels(g, waveArea, 0.0, thumbnail_.getTotalLength(), 1.0f);
    } else {
        g.setFont(AR::font(AR::FontRole::secondary));
        g.setColour(juce::Colour(AR::COMMENT));
        g.drawFittedText("No file loaded", waveArea, juce::Justification::centred, 1);
    }

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
