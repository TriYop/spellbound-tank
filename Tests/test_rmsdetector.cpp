#include "test_runner.h"
#include "../Source/DSP/RmsDetector.h"
#include <cmath>

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kSampleRate = 48000.0;

int main()
{
    // Full-scale sine settles near -3.01 dBFS (RMS of a unit sine = 1/sqrt(2)).
    {
        RmsDetector d;
        d.prepare (kSampleRate);
        float level = -200.0f;
        for (int i = 0; i < 9600; ++i)
        {
            const float in = static_cast<float> (std::sin (2.0 * kPi * 100.0 * i / kSampleRate));
            level = d.process (in);
        }
        CHECK_MSG (level > -4.0f && level < -2.0f,
                   "full-scale sine RMS should settle near -3.01 dBFS");
    }

    // Silence floors at the detector's minimum reported level.
    {
        RmsDetector d;
        d.prepare (kSampleRate);
        float level = 0.0f;
        for (int i = 0; i < 9600; ++i)
            level = d.process (0.0f);
        CHECK_MSG (level <= -119.0f, "silence should floor at the minimum dB level");
    }

    // Full-scale DC settles at 0 dBFS.
    {
        RmsDetector d;
        d.prepare (kSampleRate);
        float level = -200.0f;
        for (int i = 0; i < 9600; ++i)
            level = d.process (1.0f);
        CHECK_MSG (level > -0.5f && level < 0.5f, "full-scale DC RMS should settle near 0 dBFS");
    }

    // reset() drops the level back toward the floor on subsequent silence.
    {
        RmsDetector d;
        d.prepare (kSampleRate);
        for (int i = 0; i < 9600; ++i) d.process (1.0f);
        d.reset();
        const float level = d.process (0.0f);
        CHECK_MSG (level <= -119.0f, "reset() should clear accumulated level");
    }

    TEST_SUMMARY();
    return 0;
}
