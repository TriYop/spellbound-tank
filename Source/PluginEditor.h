#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class TankAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit TankAudioProcessorEditor (TankAudioProcessor&);
    ~TankAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    TankAudioProcessor& processor_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TankAudioProcessorEditor)
};
