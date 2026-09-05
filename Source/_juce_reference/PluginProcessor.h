#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "DSP/BandpassFilter.h"
#include "DSP/RmsDetector.h"
#include "DSP/LookaheadDelay.h"
#include "DSP/DuckingEnvelope.h"

// Sidechain "pumping" compressor: the Sidechain bus feeds a bandpass + RMS
// detector driving a lookahead ducking envelope applied to the Main bus.
// If a host (or JUCE's Standalone wrapper, which isn't multi-bus aware)
// declares the Sidechain bus present but doesn't back it with real channel
// data, processBlock() treats that missing input as silence rather than
// dereferencing a null/absent channel pointer -- see the sidechain-read
// guard in processBlock().
class TankAudioProcessor : public juce::AudioProcessor
{
public:
    TankAudioProcessor();
    ~TankAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Current gain reduction in dB (0 while bypassed), read by the editor's
    // GR meter on a UI timer -- never touched from the audio thread's
    // reader side, only written there.
    float getCurrentReductionDb() const noexcept
    {
        return currentReductionDb_.load (std::memory_order_relaxed);
    }

    static constexpr float kMaxDepthDb = 12.0f;

    juce::AudioProcessorValueTreeState apvts;

private:
    static constexpr float kMaxAnticipationMs = 20.0f;

    double sampleRate_ = 44100.0;

    BandpassFilter   sidechainFilter_;
    RmsDetector      sidechainRms_;
    DuckingEnvelope  duckEnvelope_;
    std::array<LookaheadDelay, 2> lookaheadDelay_;
    juce::SmoothedValue<float> gainSmoothed_;

    std::atomic<float> currentReductionDb_ { 0.0f };
    int reportedLatencySamples_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TankAudioProcessor)
};
