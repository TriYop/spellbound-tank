#include "PluginEditor.h"

TankAudioProcessorEditor::TankAudioProcessorEditor (TankAudioProcessor& p)
    : AudioProcessorEditor (&p), processor_ (p)
{
    setSize (500, 220);
}

void TankAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void TankAudioProcessorEditor::resized()
{
}
