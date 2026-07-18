#include "TankLookAndFeel.h"

TankLookAndFeel::TankLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, TankCol::valueText());
    setColour (juce::Slider::textBoxBackgroundColourId, TankCol::tbBg());
    setColour (juce::Slider::textBoxOutlineColourId, TankCol::panelBorder());
    setColour (juce::Label::textColourId, TankCol::textPrimary());
    setColour (juce::ToggleButton::textColourId, TankCol::textPrimary());
}

void TankLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                        float sliderPosProportional,
                                        float rotaryStartAngle, float rotaryEndAngle,
                                        juce::Slider&)
{
    auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                           static_cast<float> (w), static_cast<float> (h)).reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                          rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (TankCol::trackArc());
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                          rotaryStartAngle, angle, true);
    g.setColour (TankCol::valueArc());
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobRadius = radius * 0.72f;
    juce::ColourGradient grad (TankCol::knobTop(), centre.x, centre.y - knobRadius,
                                TankCol::knobBottom(), centre.x, centre.y + knobRadius, false);
    g.setGradientFill (grad);
    g.fillEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);
    g.setColour (TankCol::knobRim());
    g.drawEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.5f);

    juce::Path pointer;
    const float pointerLength = knobRadius * 0.8f;
    const float pointerThickness = 2.5f;
    pointer.addRectangle (-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength * 0.6f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));
    g.setColour (TankCol::valueArcGlow());
    g.fillPath (pointer);
}

void TankLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.setColour (TankCol::textPrimary());
    g.setFont (label.getFont());
    g.drawFittedText (label.getText(), label.getLocalBounds(),
                       label.getJustificationType(), 1);
}

juce::Label* TankLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    l->setColour (juce::Label::textColourId, TankCol::valueText());
    l->setColour (juce::Label::backgroundColourId, TankCol::tbBg());
    l->setColour (juce::Label::outlineColourId, TankCol::panelBorder());
    return l;
}

void TankLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                        bool /*shouldDrawButtonAsHighlighted*/,
                                        bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (button.getToggleState() ? TankCol::valueArc() : TankCol::panel());
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (TankCol::panelBorder());
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    g.setColour (button.getToggleState() ? TankCol::bg() : TankCol::textPrimary());
    g.setFont (juce::Font (juce::FontOptions (14.0f)));
    g.drawFittedText (button.getButtonText(), bounds.toNearestInt(), juce::Justification::centred, 1);
}
