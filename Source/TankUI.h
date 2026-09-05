#pragma once

#include "DistrhoUI.hpp"

START_NAMESPACE_DISTRHO

class TankUI : public UI
{
public:
    TankUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT) {}

protected:
    void parameterChanged(uint32_t, float) override {}

    void onNanoDisplay() override
    {
        beginPath();
        rect(0.0f, 0.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()));
        fillColor(DGL_NAMESPACE::Color(28, 20, 16, 255)); // TankCol::bg() 0xff1c1410, see Task 6
        fill();
        closePath();
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TankUI)
};

END_NAMESPACE_DISTRHO
