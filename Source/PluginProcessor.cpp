#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout TankAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterBool> ("bypass", "Bypass", false));

    params.push_back (std::make_unique<AudioParameterFloat> ("depth", "Depth",
        NormalisableRange<float> (0.f, 12.f, 0.1f), 6.f,
        AudioParameterFloatAttributes{}.withLabel ("dB")));

    params.push_back (std::make_unique<AudioParameterFloat> ("anticipation", "Anticipation",
        NormalisableRange<float> (1.f, 20.f, 0.1f), 5.f,
        AudioParameterFloatAttributes{}.withLabel ("ms")));

    params.push_back (std::make_unique<AudioParameterFloat> ("release", "Release",
        NormalisableRange<float> (50.f, 500.f, 1.f), 150.f,
        AudioParameterFloatAttributes{}.withLabel ("ms")));

    params.push_back (std::make_unique<AudioParameterFloat> ("sensitivity", "Sensitivity",
        NormalisableRange<float> (-40.f, 0.f, 0.1f), -20.f,
        AudioParameterFloatAttributes{}.withLabel ("dB")));

    return { params.begin(), params.end() };
}

TankAudioProcessor::TankAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Main",      juce::AudioChannelSet::stereo(), true)
          .withInput  ("Sidechain", juce::AudioChannelSet::mono(),   true)
          .withOutput ("Main",      juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Tank", createParameterLayout())
{
}

TankAudioProcessor::~TankAudioProcessor() = default;

const juce::String TankAudioProcessor::getName() const { return JucePlugin_Name; }
bool TankAudioProcessor::acceptsMidi()  const { return false; }
bool TankAudioProcessor::producesMidi() const { return false; }
bool TankAudioProcessor::isMidiEffect() const { return false; }
double TankAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int  TankAudioProcessor::getNumPrograms()              { return 1; }
int  TankAudioProcessor::getCurrentProgram()           { return 0; }
void TankAudioProcessor::setCurrentProgram (int)       {}
const juce::String TankAudioProcessor::getProgramName (int) { return {}; }
void TankAudioProcessor::changeProgramName (int, const juce::String&) {}

void TankAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRate_ = sampleRate;
    juce::ignoreUnused (samplesPerBlock);

    sidechainFilter_.prepare (sampleRate);
    sidechainRms_.prepare (sampleRate);
    duckEnvelope_.prepare (sampleRate);

    for (auto& delay : lookaheadDelay_)
        delay.prepare (sampleRate, kMaxAnticipationMs);

    gainSmoothed_.reset (sampleRate, 0.001);
    gainSmoothed_.setCurrentAndTargetValue (1.0f);

    reportedLatencySamples_ = static_cast<int> (std::round (
        apvts.getRawParameterValue ("anticipation")->load() * 0.001 * sampleRate_));
    setLatencySamples (reportedLatencySamples_);
}

void TankAudioProcessor::releaseResources() {}

bool TankAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getChannelSet (true, 1) != juce::AudioChannelSet::mono())
        return false;
    return true;
}

void TankAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const bool  bypassed        = apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    const float depthDb         = apvts.getRawParameterValue ("depth")->load();
    const float anticipationMs  = apvts.getRawParameterValue ("anticipation")->load();
    const float releaseMs       = apvts.getRawParameterValue ("release")->load();
    const float sensitivityDb   = apvts.getRawParameterValue ("sensitivity")->load();

    duckEnvelope_.setParameters (depthDb, anticipationMs, releaseMs, sensitivityDb);

    const int anticipationSamples = static_cast<int> (std::round (anticipationMs * 0.001 * sampleRate_));
    if (anticipationSamples != reportedLatencySamples_)
    {
        reportedLatencySamples_ = anticipationSamples;
        setLatencySamples (reportedLatencySamples_);
    }

    auto mainIn  = getBusBuffer (buffer, true, 0);
    auto sideIn  = getBusBuffer (buffer, true, 1);
    auto mainOut = getBusBuffer (buffer, false, 0);

    const int numSamples = buffer.getNumSamples();
    // The Sidechain bus may be declared but not actually backed by real channel
    // data -- some hosts (and JUCE's Standalone wrapper, which is not multi-bus
    // aware) report the bus as present while supplying no audio behind it.
    // Treat that as silence rather than dereferencing a missing channel pointer.
    const bool   hasSidechain = sideIn.getNumChannels() > 0 && sideIn.getReadPointer (0) != nullptr;
    const float* sc           = hasSidechain ? sideIn.getReadPointer (0) : nullptr;

    for (int n = 0; n < numSamples; ++n)
    {
        const float scInput      = (sc != nullptr) ? sc[n] : 0.0f;
        const float scFiltered   = sidechainFilter_.process (scInput);
        const float scLevelDb    = sidechainRms_.process (scFiltered);
        const float reductionDb  = duckEnvelope_.process (scLevelDb);

        const float gain = bypassed ? 1.0f : juce::Decibels::decibelsToGain (-reductionDb);
        gainSmoothed_.setTargetValue (gain);
        const float appliedGain = gainSmoothed_.getNextValue();

        for (int ch = 0; ch < mainOut.getNumChannels(); ++ch)
        {
            const float dry     = mainIn.getReadPointer (ch)[n];
            const float delayed = lookaheadDelay_[static_cast<size_t> (ch)].process (dry, anticipationSamples);
            mainOut.getWritePointer (ch)[n] = delayed * appliedGain;
        }

        currentReductionDb_.store (bypassed ? 0.0f : reductionDb, std::memory_order_relaxed);
    }
}

bool TankAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* TankAudioProcessor::createEditor()
{
    return new TankAudioProcessorEditor (*this);
}

void TankAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void TankAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TankAudioProcessor();
}
