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
    setLookAndFeel(&laf_);
    juce::Component::setSize(480, 340);
    drawAndConfigComponents();
}

AutoRemixAudioProcessorEditor::~AutoRemixAudioProcessorEditor()
{
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
    addAndMakeVisible(file_lbl);
    addAndMakeVisible(loadfile_btn);
    loadfile_btn.setButtonText("Load Audio File");
    loadfile_btn.onClick = [this] { loadFile(); };

    addAndMakeVisible(remix_selector_lbl);
    remix_selector_lbl.setText("Remix: ", juce::dontSendNotification);
    remix_selector_lbl.attachToComponent(&remix_selector, true);
    addAndMakeVisible(remix_selector);
    remix_selector.addItem("Chopped & Screwed", 1);
    remix_selector.addItem("Slowed + Reverb",   2);
    remix_selector.addItem("Drum and Bass",      3);
    remix_selector.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(play_btn);
    play_btn.setButtonText("Play");
    play_btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(AR::GREEN));
    play_btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(AR::GREEN));
    play_btn.onClick = [this] { onClick_Play(); };

    addAndMakeVisible(save_btn);
    save_btn.setButtonText("Save");
    save_btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(AR::CYAN));
    save_btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(AR::CYAN));
    save_btn.setEnabled(false);
    save_btn.onClick = [this] { onClick_Save(); };

    addAndMakeVisible(status_lbl);
    status_lbl.setText("Ready", juce::dontSendNotification);
    status_lbl.setJustificationType(juce::Justification::centred);
    status_lbl.setFont(juce::Font(14.0f));

    addAndMakeVisible(progress_bar_);
    progress_bar_.setVisible(false);

    loadfile_btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(AR::COMMENT));
    loadfile_btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(AR::COMMENT));

    file_lbl.setColour(juce::Label::textColourId,   juce::Colour(AR::FG));
    status_lbl.setColour(juce::Label::textColourId, juce::Colour(AR::PURPLE));

    loadfile_btn.setBounds(10, 10, 140, 30);
    file_lbl.setBounds(158, 10, 312, 30);
    file_lbl.setJustificationType(juce::Justification::centredLeft);
    file_lbl.setFont(juce::Font(16.0f));

    remix_selector_lbl.setBounds(10, 52, 60, 24);
    remix_selector.setBounds(80, 50, getWidth() - 90, 30);

    // waveform area: y=90, h=96 — painted in paint(), not a component

    const int btnY   = 200;
    const int startX = (getWidth() - (2 * 80 + 10)) / 2;
    play_btn.setBounds(startX,      btnY, 80, 36);
    save_btn.setBounds(startX + 90, btnY, 80, 36);

    status_lbl.setBounds(10, 248, getWidth() - 20, 24);
    progress_bar_.setBounds(10, 278, getWidth() - 20, 18);
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

    juce::Rectangle<int> waveArea(10, 90, getWidth() - 20, 96);
    g.setColour(juce::Colour(AR::SURFACE));
    g.fillRoundedRectangle(waveArea.toFloat(), 4.0f);

    if (thumbnail_.getTotalLength() > 0.0) {
        g.setColour(juce::Colour(AR::PURPLE));
        thumbnail_.drawChannels(g, waveArea, 0.0, thumbnail_.getTotalLength(), 1.0f);
    } else {
        g.setFont(juce::Font(13.0f));
        g.setColour(juce::Colour(AR::COMMENT));
        g.drawFittedText("No file loaded", waveArea, juce::Justification::centred, 1);
    }

    g.setColour(juce::Colour(AR::SURFACE));
    g.fillRect(0, 194, getWidth(), 1);
}

void AutoRemixAudioProcessorEditor::resized() {}
