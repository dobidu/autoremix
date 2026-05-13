#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <thread>
#include <array>
#include <filesystem>

//==============================================================================
AutoRemixAudioProcessorEditor::AutoRemixAudioProcessorEditor(AutoRemixAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    juce::Component::setSize(480, 540);
    drawAndConfigComponents();
}

AutoRemixAudioProcessorEditor::~AutoRemixAudioProcessorEditor() {}

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
    play_btn.setColour(juce::TextButton::buttonColourId, juce::Colours::green);
    play_btn.onClick = [this] { onClick_Play(); };

    addAndMakeVisible(stop_btn);
    stop_btn.setButtonText("Stop");
    stop_btn.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    stop_btn.onClick = [this] { onClick_Stop(); };

    addAndMakeVisible(save_btn);
    save_btn.setButtonText("Save");
    save_btn.setColour(juce::TextButton::buttonColourId, juce::Colours::blue);
    save_btn.setEnabled(false);
    save_btn.onClick = [this] { onClick_Save(); };

    addAndMakeVisible(debug_text);
    debug_text.setMultiLine(true);
    debug_text.setReadOnly(true);

    loadfile_btn.setBounds(10, 20, 130, 30);
    file_lbl.setBounds(150, 20, 310, 30);
    file_lbl.setJustificationType(juce::Justification::centredLeft);
    file_lbl.setFont(juce::Font(16.0f));

    remix_selector_lbl.setBounds(10, 70, 60, 20);
    remix_selector.setBounds(80, 70, (int)(getWidth() * 0.75), 30);

    const int startX = (getWidth() - ((3 * 70) + 20)) / 2;
    play_btn.setBounds(startX,       120, 70, 36);
    stop_btn.setBounds(startX + 80,  120, 70, 36);
    save_btn.setBounds(startX + 160, 120, 70, 36);

    debug_text.setBounds(10, 170, getWidth() - 20, 350);
}

//==============================================================================
void AutoRemixAudioProcessorEditor::onClick_Play()
{
    if (file_path_.isEmpty()) {
        debug_text.insertTextAtCaret("No file loaded.\n");
        return;
    }

    auto& bridge = audioProcessor.getBridge();
    if (!bridge.isServerAlive()) {
        debug_text.insertTextAtCaret("Sidecar not running. Start python server on port 17432.\n");
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
    debug_text.clear();
    debug_text.insertTextAtCaret("Separating...\n");

    std::string input_str = file_path_.toStdString();

    std::thread([this, cfg, output_path, input_str]() mutable {
        auto& bridge = audioProcessor.getBridge();

        auto stems = bridge.separateStems(
            std::filesystem::path(input_str),
            std::filesystem::path("/tmp/autoremix/stems"),
            "algorithmic");

        if (!stems.valid) {
            juce::MessageManager::callAsync([this] {
                debug_text.insertTextAtCaret("Error: stem separation failed.\n");
                play_btn.setEnabled(true);
            });
            return;
        }

        juce::MessageManager::callAsync([this] {
            debug_text.insertTextAtCaret("Remixing...\n");
        });

        auto result = bridge.applyRemix(stems, cfg.params, output_path);

        juce::MessageManager::callAsync([this, result]() mutable {
            if (result.success) {
                output_path_ = juce::String(result.output_path.string());
                debug_text.insertTextAtCaret("Done: " + output_path_ + "\n");
                save_btn.setEnabled(true);
            } else {
                debug_text.insertTextAtCaret("Error: " + juce::String(result.error_message) + "\n");
            }
            play_btn.setEnabled(true);
        });
    }).detach();
}

void AutoRemixAudioProcessorEditor::onClick_Stop()
{
    // Stop mid-processing deferred to Phase 05.
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
                debug_text.insertTextAtCaret("Saved to: " + results[0].getFullPathName() + "\n");
            }
        });
}

//==============================================================================
void AutoRemixAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void AutoRemixAudioProcessorEditor::resized() {}
