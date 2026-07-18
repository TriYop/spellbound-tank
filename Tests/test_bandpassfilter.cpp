#include "test_runner.h"
#include "../Source/DSP/BandpassFilter.h"
#include <cmath>
#include <algorithm>

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kSampleRate = 48000.0;

static float steadyStatePeak (BandpassFilter& f, double freqHz, int numSamples)
{
    float peak = 0.f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float in = static_cast<float> (std::sin (2.0 * kPi * freqHz * i / kSampleRate));
        const float out = f.process (in);
        if (i > numSamples / 2) peak = std::max (peak, std::abs (out));
    }
    return peak;
}

int main()
{
    // Passband: near the ~86.6 Hz center (geometric mean of 50 and 150 Hz),
    // steady-state gain should stay close to unity.
    {
        BandpassFilter f;
        f.prepare (kSampleRate);
        const float peak = steadyStatePeak (f, 86.6, 9600);
        CHECK_MSG (peak > 0.9f && peak < 1.1f,
                   "center frequency should pass at close to unity gain");
    }

    // Stopband below the band: 10 Hz should be heavily attenuated.
    {
        BandpassFilter f;
        f.prepare (kSampleRate);
        const float peak = steadyStatePeak (f, 10.0, 9600);
        CHECK_MSG (peak < 0.3f, "10 Hz should be attenuated well below the band");
    }

    // Stopband above the band: 2000 Hz should be heavily attenuated.
    {
        BandpassFilter f;
        f.prepare (kSampleRate);
        const float peak = steadyStatePeak (f, 2000.0, 9600);
        CHECK_MSG (peak < 0.3f, "2000 Hz should be attenuated well above the band");
    }

    // reset() clears filter state (no residual output after reset with silent input).
    {
        BandpassFilter f;
        f.prepare (kSampleRate);
        steadyStatePeak (f, 86.6, 4800);
        f.reset();
        const float out = f.process (0.0f);
        CHECK_MSG (out == 0.0f, "reset() should clear internal state to exactly zero");
    }

    TEST_SUMMARY();
    return 0;
}
