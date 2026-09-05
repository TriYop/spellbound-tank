#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <utility>
#include <cmath>
#include "TankLookAndFeel.h"

// Horizontal gain-reduction meter. Polls the processor's current reduction
// (0..maxDepthDb) on a 30Hz timer. Unlike a level VU meter, 0 = empty and
// full depth = fully filled, matching how GR meters are read in DAWs.
// No allocation in paint()/timerCallback(), same convention as Hex's VuMeter.
class TankGrMeter : public juce::Component, private juce::Timer
{
public:
    TankGrMeter (std::function<float()> reductionDbSupplier, float maxDepthDb)
        : reductionDbSupplier_ (std::move (reductionDbSupplier)), maxDepthDb_ (maxDepthDb)
    {
        startTimerHz (30);
    }

    ~TankGrMeter() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (TankCol::tbBg());
        g.fillRoundedRectangle (bounds, 2.0f);
        g.setColour (TankCol::panelBorder());
        g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, 1.0f);

        auto inner = bounds.reduced (2.0f);
        const float w = inner.getWidth();
        const float fillW = juce::jlimit (0.0f, w, displayLevel_ * w);

        if (fillW > 0.0f)
        {
            auto fillRect = inner.removeFromLeft (fillW);
            juce::ColourGradient grad (TankCol::valueArcGlow(), fillRect.getX(), fillRect.getY(),
                                       TankCol::valueArc(), fillRect.getRight(), fillRect.getY(), false);
            g.setGradientFill (grad);
            g.fillRect (fillRect);
        }
    }

private:
    void timerCallback() override
    {
        const float reductionDb = reductionDbSupplier_ ? reductionDbSupplier_() : 0.0f;
        const float level = juce::jlimit (0.0f, 1.0f, reductionDb / juce::jmax (maxDepthDb_, 0.001f));

        if (level > displayLevel_)
            displayLevel_ += (level - displayLevel_) * kAttackCoeff;
        else
            displayLevel_ += (level - displayLevel_) * kReleaseCoeff;

        if (std::abs (displayLevel_ - lastPaintedLevel_) > kRepaintThreshold)
        {
            lastPaintedLevel_ = displayLevel_;
            repaint();
        }
    }

    static constexpr float kAttackCoeff      = 0.6f;
    static constexpr float kReleaseCoeff     = 0.08f;
    static constexpr float kRepaintThreshold = 0.002f;

    std::function<float()> reductionDbSupplier_;
    float maxDepthDb_;
    float displayLevel_ { 0.0f };
    float lastPaintedLevel_ { -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TankGrMeter)
};
