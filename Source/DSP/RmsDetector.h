#pragma once
#include <cmath>
#include <algorithm>

// Windowed RMS envelope detector producing the input level in dBFS, used to
// drive Tank's sidechain trigger. Implemented as a one-pole low-pass on the
// squared signal (~10 ms integration time) rather than a sliding-window sum,
// avoiding a buffer while still giving a smooth, RMS-like reading.
class RmsDetector
{
public:
    void prepare (double sampleRate) noexcept
    {
        coeff_ = 1.0f - std::exp (-1.0f / (kWindowMs * 0.001f * static_cast<float> (sampleRate)));
        reset();
    }

    void reset() noexcept { meanSquare_ = 0.0f; }

    // Returns the current level in dBFS (silence floors at kMinDb).
    float process (float input) noexcept
    {
        meanSquare_ += coeff_ * (input * input - meanSquare_);
        const float rms = std::sqrt (std::max (meanSquare_, 0.0f));
        const float db  = 20.0f * std::log10 (std::max (rms, kMinLinear));
        return std::max (db, kMinDb);
    }

private:
    static constexpr float kWindowMs  = 10.0f;
    static constexpr float kMinLinear = 1.0e-6f;   // -120 dBFS floor
    static constexpr float kMinDb     = -120.0f;

    float coeff_      = 0.0f;
    float meanSquare_ = 0.0f;
};
