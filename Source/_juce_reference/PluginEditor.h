#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "PluginProcessor.h"
#include "UI/TankLookAndFeel.h"
#include "UI/TankGrMeter.h"

class TankAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit TankAudioProcessorEditor (TankAudioProcessor&);
    ~TankAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    TankAudioProcessor& processor_;
    TankLookAndFeel lookAndFeel_;

    std::array<Knob, 4> knobs_ { Knob{}, Knob{}, Knob{}, Knob{} };

    juce::ToggleButton bypassButton_ { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment_;

    TankGrMeter grMeter_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TankAudioProcessorEditor)
};
