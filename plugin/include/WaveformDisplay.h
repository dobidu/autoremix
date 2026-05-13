#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "AutoRemixLookAndFeel.h"

class WaveformDisplay : public juce::Component {
public:
    explicit WaveformDisplay(juce::AudioThumbnail& thumb) : thumb_(thumb) {
        setBufferedToImage(true);
    }

    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds();

        g.setColour(juce::Colour(AR::BG_DEEP));
        g.fillRoundedRectangle(area.toFloat(), 4.0f);

        if (thumb_.getTotalLength() > 0.0) {
            double len = thumb_.getTotalLength();

            // area fill: 15% alpha under the waveform shape
            g.setColour(juce::Colour(AR::PURPLE).withAlpha(0.15f));
            thumb_.drawChannels(g, area, 0.0, len, 1.0f);

            // crisp waveform on top
            g.setColour(juce::Colour(AR::PURPLE));
            thumb_.drawChannels(g, area, 0.0, len, 1.0f);
        } else {
            g.setFont(AR::font(AR::FontRole::secondary));
            g.setColour(juce::Colour(AR::COMMENT));
            g.drawFittedText("No file loaded", area, juce::Justification::centred, 1);
        }
    }

    void sourceChanged() { repaint(); }

private:
    juce::AudioThumbnail& thumb_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
