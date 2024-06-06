/*
Description: This file contains the implementation of the PluginEditor class,
which is responsible for the graphical interface of the plugin. The PluginEditor class 
has a constructor that receives a reference to the AutoRemixAudioProcessor class, which
is the class that implements the audio processing logic of the plugin. 

*/

#include <cpr/cpr.h>

#include "autoremix/PluginProcessor.h"
#include "autoremix/PluginEditor.h"

//TODO: rename classes (orchestrator pad instead of AutoRemix)
//TODO: namespace for classes

//==============================================================================
AutoRemixAudioProcessorEditor::AutoRemixAudioProcessorEditor (AutoRemixAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)

{

    juce::Component::setSize (480, 540);
    drawAndConfigComponents();


}

AutoRemixAudioProcessorEditor::~AutoRemixAudioProcessorEditor()
{

}


using ReplyFunc = std::function<void(const juce::String&)>;


void AutoRemixAudioProcessorEditor::loadFile()
{
    juce::FileChooser chooser("Load file...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*",
        true,
        false);

    if (chooser.browseForFileToOpen())
    { 
        juce::File f;
        f = chooser.getResult();
        file_path = f.getFullPathName();

        int last_bar = file_path.lastIndexOfChar('\\');
        file_lbl.setText(file_path.substring(last_bar+1), juce::dontSendNotification);

    }
}



void AutoRemixAudioProcessorEditor::drawAndConfigComponents() {
    // Load  File
    addAndMakeVisible(file_lbl);
    addAndMakeVisible(loadfile_btn);
    loadfile_btn.setButtonText("Load Audio File");
    loadfile_btn.onClick = [this] { loadFile(); };


    // remix Selector
    addAndMakeVisible(remix_selector_lbl);
    remix_selector_lbl.setText("Remix: ", juce::dontSendNotification);
    remix_selector_lbl.attachToComponent(&remix_selector, true);
    addAndMakeVisible(remix_selector);

    
    addAndMakeVisible(play_btn);
    play_btn.setButtonText("Play");
    play_btn.setColour(juce::TextButton::buttonColourId, juce::Colours::green);

    play_btn.onClick = [this] { onClick_Play(); };

    addAndMakeVisible(stop_btn);
    stop_btn.setButtonText("Stop");
    stop_btn.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    stop_btn.onClick = [this] { onClick_Stop(); };

    // Save Button
    addAndMakeVisible(save_btn);
    save_btn.setButtonText("Save");
    save_btn.setColour(juce::TextButton::buttonColourId, juce::Colours::blue);
    save_btn.onClick = [this] { onClick_Save(); };

    addAndMakeVisible(debug_text);
    debug_text.setMultiLine(true);
    debug_text.setReadOnly(true);

    loadfile_btn.setBounds(10, 20, 100, 30);
    file_lbl.setBounds(120, 20, 140, 30);
    file_lbl.setJustificationType(juce::Justification::topLeft);
    file_lbl.setFont(juce::Font(18.0f, juce::Font::bold));

    remix_selector_lbl.setBounds(10, loadfile_btn.getY() + 50, 100, 20);
    remix_selector_lbl.setJustificationType(juce::Justification::topLeft);
    remix_selector.setBounds(120, loadfile_btn.getY() + 50, (int)(getWidth() * 0.65), 30);

    const int startX = (getWidth() - ((3*60)+20)) / 2;  //3 buttons + 20px spacing

    play_btn.setBounds(startX, remix_selector.getY() + 50, 60, 40);
    stop_btn.setBounds(play_btn.getRight() + 10, remix_selector.getY() + 50, 60, 40);
    save_btn.setBounds(stop_btn.getRight() + 10, remix_selector.getY() + 50, 60, 40);

    debug_text.setBounds(10, play_btn.getY() + 60, getWidth() - 20, 280);

}


void AutoRemixAudioProcessorEditor::onClick_Play()
{

}

void AutoRemixAudioProcessorEditor::onClick_Stop()
{

}


void AutoRemixAudioProcessorEditor::onClick_Save()
{
/*
    juce::FileChooser chooser("Save file...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(output_selector.getText().toStdString()),
        "*.py",
        true,
        false);

    if (chooser.browseForFileToSave(true))
    {
        juce::File f;
        f = chooser.getResult();
        juce::String output_file_path = 
        juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName() +
        juce::File::getSeparatorString() +
        String("orch-pad") + 
        juce::File::getSeparatorString() + 

    }
*/
}

//==============================================================================
void AutoRemixAudioProcessorEditor::paint (juce::Graphics& g)
{

}

void AutoRemixAudioProcessorEditor::resized()
{

}
