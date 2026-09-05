#pragma once

#include "DistrhoPlugin.hpp"
#include "DSP/BandpassFilter.h"
#include "DSP/RmsDetector.h"
#include "DSP/DuckingEnvelope.h"
#include "DSP/LookaheadDelay.h"
#include "audioplugins/common/io/ParameterSmoother.h"
#include "audioplugins/common/io/MeterTransport.h"

#include <array>

START_NAMESPACE_DISTRHO

/**
   DPF Plugin adapter for Spellbound Tank.

   Thin shim over Tank's existing framework-free DSP chain
   (BandpassFilter -> RmsDetector -> DuckingEnvelope, plus a per-channel
   LookaheadDelay on the main signal): reads host parameters, drives the
   chain once per sample (matching the JUCE-era processBlock()'s per-sample
   loop, since DuckingEnvelope's state machine is inherently per-sample),
   and reports the current gain reduction via a MeterTransport rather than a
   DPF parameter (see DistrhoPluginInfo.h's DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
   comment for why -- same reasoning as Hex's IN/OUT meters).

   Sidechain: declares 3 input ports (index 0/1 = main stereo, index 2 =
   mono sidechain with the CLAP/VST3 sidechain hint set) and 2 output ports
   via initAudioPort() below -- DPF's confirmed, precedented mechanism (see
   the migration design spec's "DPF sidechain support" note). run()'s
   inputs[2] is the sidechain channel directly; readSidechainSample() (see
   DSP/SidechainGuard.h) treats a null/missing pointer as silence, porting
   the JUCE-era hasSidechain guard verbatim.
 */
class TankPluginAdapter : public Plugin
{
public:
    TankPluginAdapter();

protected:
    // -- Information -----------------------------------------------------
    const char* getLabel() const override;
    const char* getDescription() const override;
    const char* getMaker() const override;
    const char* getLicense() const override;
    uint32_t getVersion() const override;

    // -- Init -------------------------------------------------------------
    void initAudioPort(bool input, uint32_t index, AudioPort& port) override;
    void initParameter(uint32_t index, Parameter& parameter) override;

    // -- Internal data ------------------------------------------------------
    float getParameterValue(uint32_t index) const override;
    void setParameterValue(uint32_t index, float value) override;

public:
    // -- Direct UI access (DISTRHO_PLUGIN_WANT_DIRECT_ACCESS) ---------------
    // Read by TankUI via getPluginInstancePointer() + uiIdle(). Public
    // (unlike the DPF overrides above/below) because TankUI reaches this
    // through a raw TankPluginAdapter* obtained from
    // getPluginInstancePointer(), not from within the Plugin class hierarchy.
    float getCurrentReductionDb() const noexcept { return reductionMeter_.read(); }

    static constexpr float kMaxDepthDb = TANK_PARAM_DEPTH_MAX;

protected:
    // -- Process ------------------------------------------------------------
    void activate() override;
    void deactivate() override;
    void run(const float** inputs, float** outputs, uint32_t frames) override;

private:
    bool bypass_ = TANK_PARAM_BYPASS_DEFAULT > 0.5f;
    float depthDb_ = TANK_PARAM_DEPTH_DEFAULT;
    float anticipationMs_ = TANK_PARAM_ANTICIPATION_DEFAULT;
    float releaseMs_ = TANK_PARAM_RELEASE_DEFAULT;
    float sensitivityDb_ = TANK_PARAM_SENSITIVITY_DEFAULT;

    BandpassFilter  sidechainFilter_;
    RmsDetector     sidechainRms_;
    DuckingEnvelope duckEnvelope_;
    std::array<LookaheadDelay, 2> lookaheadDelay_;
    audioplugins::common::io::ParameterSmoother gainSmoother_;

    // Written from run() (audio thread) every block; read by TankUI via
    // getCurrentReductionDb() above, not via any DPF parameter/state.
    audioplugins::common::io::MeterTransport reductionMeter_ { "reduction_db" };

    int reportedLatencySamples_ = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TankPluginAdapter)
};

END_NAMESPACE_DISTRHO
