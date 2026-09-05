#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Bronze/brown "MMORPG tank" palette, structured the same way as Hex's
// HexLookAndFeel (shared colour constants + LookAndFeel_V4 overrides), but
// re-themed away from Hex's neon purple per the approved design spec.
struct TankCol
{
    static juce::Colour bg()           { return juce::Colour (0xff1c1410); }
    static juce::Colour panel()        { return juce::Colour (0xff2e2218); }
    static juce::Colour panelBorder()  { return juce::Colour (0xff4a3a28); }
    static juce::Colour knobTop()      { return juce::Colour (0xff5a4530); }
    static juce::Colour knobBottom()   { return juce::Colour (0xff1c1410); }
    static juce::Colour knobRim()      { return juce::Colour (0xff6a5340); }
    static juce::Colour valueArc()     { return juce::Colour (0xffcc8833); }
    static juce::Colour valueArcGlow() { return juce::Colour (0xffffaa44); }
    static juce::Colour trackArc()     { return juce::Colour (0xff3a2c1a); }
    static juce::Colour textPrimary()  { return juce::Colour (0xffd8c8a8); }
    static juce::Colour textDim()      { return juce::Colour (0xff7a6a55); }
    static juce::Colour valueText()    { return juce::Colour (0xffd8c8a8); }
    static juce::Colour tbBg()         { return juce::Colour (0xff120c08); }
};

class TankLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TankLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawLabel (juce::Graphics&, juce::Label&) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;
};
