#pragma once
#include <algorithm>

// Ducking gain-reduction envelope: RMS crossing `sensitivityDb` ramps gain
// reduction from its current level toward `depthDb` over `anticipationMs`
// (paired with Tank's lookahead delay so the dip lands in sync with the
// delayed sidechain transient), holds while RMS stays above threshold, then
// ramps back to 0 over `releaseMs` once RMS falls back below threshold.
//
// Re-triggering while still releasing does NOT reset the level to 0 -- it
// re-enters the ramp-toward-depth state from wherever the level currently
// is, the same smooth-retrigger principle as Catalyst's AdsrEnvelope
// (Catalyst/Source/DSP/AdsrEnvelope.h): only the state changes on
// noteOn()/threshold-cross, the level itself is left untouched.
class DuckingEnvelope
{
public:
    void prepare (double sampleRate) noexcept
    {
        sampleRate_ = sampleRate;
        reset();
    }

    void reset() noexcept
    {
        state_ = State::Idle;
        reductionDb_ = 0.0f;
    }

    void setParameters (float depthDb, float anticipationMs, float releaseMs, float sensitivityDb) noexcept
    {
        depthDb_       = depthDb;
        sensitivityDb_ = sensitivityDb;
        rampUpRate_    = depthDb / static_cast<float> (std::max (anticipationMs * 0.001 * sampleRate_, 1.0));
        rampDownRate_  = depthDb / static_cast<float> (std::max (releaseMs * 0.001 * sampleRate_, 1.0));
    }

    // `levelDb` is the sidechain's filtered RMS level for this sample.
    // Returns the gain reduction to apply, in dB (0 = no reduction, positive = attenuate).
    float process (float levelDb) noexcept
    {
        const bool above = levelDb > sensitivityDb_;

        if (above && (state_ == State::Idle || state_ == State::Releasing))
            state_ = State::Anticipating;
        else if (! above && (state_ == State::Anticipating || state_ == State::Holding))
            state_ = State::Releasing;

        switch (state_)
        {
            case State::Idle:
                reductionDb_ = 0.0f;
                break;

            case State::Anticipating:
                reductionDb_ = std::min (reductionDb_ + rampUpRate_, depthDb_);
                if (reductionDb_ >= depthDb_)
                    state_ = State::Holding;
                break;

            case State::Holding:
                reductionDb_ = depthDb_;
                break;

            case State::Releasing:
                reductionDb_ = std::max (reductionDb_ - rampDownRate_, 0.0f);
                if (reductionDb_ <= 0.0f)
                    state_ = State::Idle;
                break;
        }

        return reductionDb_;
    }

private:
    enum class State { Idle, Anticipating, Holding, Releasing };

    double sampleRate_    = 48000.0;
    State  state_         = State::Idle;
    float  reductionDb_   = 0.0f;
    float  depthDb_       = 6.0f;
    float  sensitivityDb_ = -20.0f;
    float  rampUpRate_    = 0.0f;
    float  rampDownRate_  = 0.0f;
};
