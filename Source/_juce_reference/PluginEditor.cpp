#include "PluginEditor.h"

TankAudioProcessorEditor::TankAudioProcessorEditor (TankAudioProcessor& p)
    : AudioProcessorEditor (&p), processor_ (p),
      grMeter_ ([&p] { return p.getCurrentReductionDb(); }, TankAudioProcessor::kMaxDepthDb)
{
    setLookAndFeel (&lookAndFeel_);

    static const std::array<std::pair<const char*, const char*>, 4> kKnobDefs { {
        { "depth",        "Mitigation" },
        { "anticipation", "Reflex" },
        { "release",      "Cooldown" },
        { "sensitivity",  "Aggro Trigger" },
    } };

    for (size_t i = 0; i < knobs_.size(); ++i)
    {
        auto& knob = knobs_[i];
        knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
        addAndMakeVisible (knob.slider);

        knob.label.setText (kKnobDefs[i].second, juce::dontSendNotification);
        knob.label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (knob.label);

        knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor_.apvts, kKnobDefs[i].first, knob.slider);
    }

    addAndMakeVisible (bypassButton_);
    bypassAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor_.apvts, "bypass", bypassButton_);

    addAndMakeVisible (grMeter_);

    setSize (500, 220);
}

TankAudioProcessorEditor::~TankAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void TankAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (TankCol::bg());
}

void TankAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto topRow = area.removeFromTop (30);
    bypassButton_.setBounds (topRow.removeFromLeft (100));

    area.removeFromTop (8);
    auto meterRow = area.removeFromTop (24);
    grMeter_.setBounds (meterRow);

    area.removeFromTop (16);

    const int knobWidth = area.getWidth() / static_cast<int> (knobs_.size());
    for (auto& knob : knobs_)
    {
        auto col = area.removeFromLeft (knobWidth);
        knob.label.setBounds (col.removeFromTop (20));
        knob.slider.setBounds (col.reduced (8));
    }
}
