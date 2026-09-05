#pragma once

#include "DistrhoUI.hpp"
#include "TankPluginAdapter.h"

#include "audioplugins/common/hui/dgl/RotaryKnob.h"
#include "audioplugins/common/hui/dgl/ToggleSwitch.h"
#include "audioplugins/common/hui/dgl/VuMeter.h"

#include <array>
#include <memory>

START_NAMESPACE_DISTRHO

/**
   DPF UI adapter for Spellbound Tank.

   Ports the JUCE-era bronze TankLookAndFeel palette (see
   Source/_juce_reference/UI/TankLookAndFeel.h) onto DGL/NanoVG: 4 rotary
   knobs (Mitigation/Reflex/Cooldown/Aggro Trigger), a ToggleSwitch for
   Bypass, and the gain-reduction meter via Common's VuMeter in its
   Horizontal orientation (v0.3.0), polled every uiIdle() tick straight off
   the DSP instance via fPluginPtr (DISTRHO_PLUGIN_WANT_DIRECT_ACCESS) --
   same direct-access idiom Hex's IN/OUT meters use, since a live gain-
   reduction value can't be a DPF output parameter (clap-validator rejects
   those).

   The presets panel (PresetSelector + SAVE/DELETE buttons) is added in
   Task 7.
 */
class TankUI : public UI
{
public:
    TankUI();

protected:
    // -- DSP/Plugin Callbacks ---------------------------------------------
    void parameterChanged(uint32_t index, float value) override;
    void uiIdle() override;

    // -- Widget Callbacks ---------------------------------------------------
    void onNanoDisplay() override;

private:
    // Raw, non-owning: TankUI does not own the DSP instance's lifetime, DPF
    // does. Captured once in the constructor via getPluginInstancePointer().
    TankPluginAdapter* const fPluginPtr;

    // Consecutive-quiet-tick countdown for uiIdle()'s meter-push gate --
    // same pattern as Hex's HexUI (see kMeterIdleTicks in TankUI.cpp).
    int fMeterIdleCountdown = 0;

    std::unique_ptr<audioplugins::common::hui::dgl::RotaryKnob> fDepthKnob;
    std::unique_ptr<audioplugins::common::hui::dgl::RotaryKnob> fAnticipationKnob;
    std::unique_ptr<audioplugins::common::hui::dgl::RotaryKnob> fReleaseKnob;
    std::unique_ptr<audioplugins::common::hui::dgl::RotaryKnob> fSensitivityKnob;
    std::unique_ptr<audioplugins::common::hui::dgl::ToggleSwitch> fBypassSwitch;
    std::unique_ptr<audioplugins::common::hui::dgl::VuMeter> fGrMeter;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TankUI)
};

END_NAMESPACE_DISTRHO
