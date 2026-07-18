#pragma once
#include <cmath>

// Fixed 50-150 Hz bandpass isolating the kick fundamental range for
// sidechain RMS detection. Zavalishin topology-preserving 2-pole state-
// variable filter (same math as a standard TPT SVF), specialized to only
// expose the band output at a fixed center frequency/Q.
//
// Center frequency is the geometric mean of the band edges
// (sqrt(50*150) ~= 86.6 Hz); Q set so the -3dB points land at
// approximately 50 Hz and 150 Hz for a single 2-pole SVF bandpass response.
//
// Plain C++, no JUCE dependency, so it can be exercised by bare unit tests.
class BandpassFilter
{
public:
    void prepare (double sampleRate) noexcept
    {
        reset();
        const float g = std::tan (kPi * kCenterHz / static_cast<float> (sampleRate));
        const float k = 1.0f / kQ;
        a1_ = 1.0f / (1.0f + g * (g + k));
        a2_ = g * a1_;
        a3_ = g * a2_;
    }

    void reset() noexcept
    {
        ic1eq_ = 0.0f;
        ic2eq_ = 0.0f;
    }

    float process (float input) noexcept
    {
        const float v3 = input - ic2eq_;
        const float v1 = a1_ * ic1eq_ + a2_ * v3;
        const float v2 = ic2eq_ + a2_ * ic1eq_ + a3_ * v3;
        ic1eq_ = 2.0f * v1 - ic1eq_;
        ic2eq_ = 2.0f * v2 - ic2eq_;
        return v1 / kQ; // band output with Q gain normalization
    }

private:
    static constexpr float kPi       = 3.14159265358979323846f;
    static constexpr float kCenterHz = 86.6f;
    static constexpr float kQ        = 0.866f;

    float a1_ = 0.0f, a2_ = 0.0f, a3_ = 0.0f;
    float ic1eq_ = 0.0f, ic2eq_ = 0.0f;
};
