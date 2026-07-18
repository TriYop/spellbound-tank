#include "PluginProcessor.h"
#include "PluginEditor.h"

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

void TankAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRate_ = sampleRate;
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
    // Passthrough until Task 6 wires in the sidechain-ducking DSP chain.
    // JUCE hands processBlock() a buffer already containing the input audio
    // in-place, so doing nothing here is a correct, click-free passthrough.
    juce::ignoreUnused (buffer);
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
