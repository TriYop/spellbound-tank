#include "TankPluginAdapter.h"
#include "DSP/SidechainGuard.h"

#include <cmath>
#include <cstring>

START_NAMESPACE_DISTRHO

TankPluginAdapter::TankPluginAdapter()
    : Plugin(kParameterCount, 0, 0) // 5 parameters, 0 programs, 0 states -- the GR meter is direct-access only
{
}

const char* TankPluginAdapter::getLabel() const { return "Tank"; }
const char* TankPluginAdapter::getDescription() const { return "Sidechain pumping compressor"; }
const char* TankPluginAdapter::getMaker() const { return "Spellbound"; }

const char* TankPluginAdapter::getLicense() const
{
    // Must be a URI -- lv2lint's Plugin License test fails a plain word
    // like "Proprietary" (see DistrhoPluginLV2export.cpp's license branch,
    // confirmed against Hex).
    return "https://spellbound.audio/plugins/tank#license";
}

uint32_t TankPluginAdapter::getVersion() const { return d_version(0, 1, 0); }

void TankPluginAdapter::initAudioPort(const bool input, const uint32_t index, AudioPort& port)
{
    if (input)
    {
        if (index < 2)
        {
            // Main stereo input, index 0 = left, index 1 = right.
            port.groupId = kPortGroupStereo;
            port.name    = (index == 0) ? "Main Left" : "Main Right";
            port.symbol  = (index == 0) ? "main_in_left" : "main_in_right";
        }
        else
        {
            // index == 2: mono sidechain input -- the one genuinely novel
            // port shape in this migration (see the design spec's "DPF
            // sidechain support, confirmed real" note).
            port.hints   = kAudioPortIsSidechain;
            port.groupId = kPortGroupMono;
            port.name    = "Sidechain";
            port.symbol  = "sidechain_in";
        }
        return;
    }

    // Main stereo output, index 0 = left, index 1 = right.
    port.groupId = kPortGroupStereo;
    port.name    = (index == 0) ? "Main Left" : "Main Right";
    port.symbol  = (index == 0) ? "main_out_left" : "main_out_right";
}

void TankPluginAdapter::initParameter(const uint32_t index, Parameter& parameter)
{
    switch (index)
    {
    case kParameterBypass:
        parameter.hints  = kParameterIsAutomatable | kParameterIsBoolean;
        parameter.name   = "Bypass";
        parameter.symbol = "bypass";
        parameter.ranges.def = TANK_PARAM_BYPASS_DEFAULT;
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 1.0f;
        break;
    case kParameterDepth:
        parameter.hints  = kParameterIsAutomatable;
        parameter.name   = "Mitigation";
        parameter.symbol = "depth";
        parameter.unit   = "dB";
        parameter.ranges.def = TANK_PARAM_DEPTH_DEFAULT;
        parameter.ranges.min = TANK_PARAM_DEPTH_MIN;
        parameter.ranges.max = TANK_PARAM_DEPTH_MAX;
        break;
    case kParameterAnticipation:
        parameter.hints  = kParameterIsAutomatable;
        parameter.name   = "Reflex";
        parameter.symbol = "anticipation";
        parameter.unit   = "ms";
        parameter.ranges.def = TANK_PARAM_ANTICIPATION_DEFAULT;
        parameter.ranges.min = TANK_PARAM_ANTICIPATION_MIN;
        parameter.ranges.max = TANK_PARAM_ANTICIPATION_MAX;
        break;
    case kParameterRelease:
        parameter.hints  = kParameterIsAutomatable;
        parameter.name   = "Cooldown";
        parameter.symbol = "release";
        parameter.unit   = "ms";
        parameter.ranges.def = TANK_PARAM_RELEASE_DEFAULT;
        parameter.ranges.min = TANK_PARAM_RELEASE_MIN;
        parameter.ranges.max = TANK_PARAM_RELEASE_MAX;
        break;
    case kParameterSensitivity:
        parameter.hints  = kParameterIsAutomatable;
        parameter.name   = "Aggro Trigger";
        parameter.symbol = "sensitivity";
        parameter.unit   = "dB";
        parameter.ranges.def = TANK_PARAM_SENSITIVITY_DEFAULT;
        parameter.ranges.min = TANK_PARAM_SENSITIVITY_MIN;
        parameter.ranges.max = TANK_PARAM_SENSITIVITY_MAX;
        break;
    default:
        break;
    }
}

float TankPluginAdapter::getParameterValue(const uint32_t index) const
{
    switch (index)
    {
    case kParameterBypass:       return bypass_ ? 1.0f : 0.0f;
    case kParameterDepth:        return depthDb_;
    case kParameterAnticipation: return anticipationMs_;
    case kParameterRelease:      return releaseMs_;
    case kParameterSensitivity:  return sensitivityDb_;
    default:                     return 0.0f;
    }
}

void TankPluginAdapter::setParameterValue(const uint32_t index, const float value)
{
    switch (index)
    {
    case kParameterBypass:       bypass_ = value > 0.5f; break;
    case kParameterDepth:        depthDb_ = value; break;
    case kParameterAnticipation: anticipationMs_ = value; break;
    case kParameterRelease:      releaseMs_ = value; break;
    case kParameterSensitivity:  sensitivityDb_ = value; break;
    default: break;
    }
}

void TankPluginAdapter::activate()
{
    const double sampleRate = getSampleRate();

    sidechainFilter_.prepare(sampleRate);
    sidechainRms_.prepare(sampleRate);
    duckEnvelope_.prepare(sampleRate);

    for (auto& delay : lookaheadDelay_)
        delay.prepare(sampleRate, TANK_MAX_ANTICIPATION_MS);

    gainSmoother_.reset(sampleRate, 0.001f, 1.0f);

    reportedLatencySamples_ = static_cast<int>(std::round(anticipationMs_ * 0.001 * sampleRate));
    setLatency(static_cast<uint32_t>(reportedLatencySamples_));
}

void TankPluginAdapter::deactivate()
{
    sidechainFilter_.reset();
    sidechainRms_.reset();
    duckEnvelope_.reset();
    for (auto& delay : lookaheadDelay_)
        delay.reset();
}

void TankPluginAdapter::run(const float** inputs, float** outputs, const uint32_t frames)
{
    const float* mainInL  = inputs[0];
    const float* mainInR  = inputs[1];
    const float* sidechain = inputs[2];

    duckEnvelope_.setParameters(depthDb_, anticipationMs_, releaseMs_, sensitivityDb_);

    const double sampleRate = getSampleRate();
    const int anticipationSamples = static_cast<int>(std::round(anticipationMs_ * 0.001 * sampleRate));
    if (anticipationSamples != reportedLatencySamples_)
    {
        reportedLatencySamples_ = anticipationSamples;
        setLatency(static_cast<uint32_t>(reportedLatencySamples_));
    }

    float reductionDb = 0.0f;
    for (uint32_t n = 0; n < frames; ++n)
    {
        const float scInput    = readSidechainSample(sidechain, n);
        const float scFiltered = sidechainFilter_.process(scInput);
        const float scLevelDb  = sidechainRms_.process(scFiltered);
        reductionDb = duckEnvelope_.process(scLevelDb);

        const float gain = bypass_ ? 1.0f : std::pow(10.0f, -reductionDb / 20.0f);
        gainSmoother_.setTarget(gain);
        const float appliedGain = gainSmoother_.getNextValue();

        const float delayedL = lookaheadDelay_[0].process(mainInL[n], anticipationSamples);
        const float delayedR = lookaheadDelay_[1].process(mainInR[n], anticipationSamples);
        outputs[0][n] = delayedL * appliedGain;
        outputs[1][n] = delayedR * appliedGain;
    }

    reductionMeter_.write(bypass_ ? 0.0f : reductionDb);
}

Plugin* createPlugin()
{
    return new TankPluginAdapter();
}

END_NAMESPACE_DISTRHO
