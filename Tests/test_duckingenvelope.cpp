#include "test_runner.h"
#include "../Source/DSP/DuckingEnvelope.h"

static constexpr double kSampleRate = 48000.0;

int main()
{
    // Below threshold forever: reduction stays at 0.
    {
        DuckingEnvelope env;
        env.prepare (kSampleRate);
        env.setParameters (6.0f, 5.0f, 150.0f, -20.0f);
        float reduction = -1.0f;
        for (int i = 0; i < 1000; ++i)
            reduction = env.process (-40.0f); // well below -20 dB threshold
        CHECK_MSG (reduction == 0.0f, "reduction should stay 0 dB while below threshold");
    }

    // Crossing above threshold ramps 0 -> depth over ~anticipationMs, then holds.
    {
        DuckingEnvelope env;
        env.prepare (kSampleRate);
        const float depthDb = 6.0f, anticipationMs = 5.0f;
        env.setParameters (depthDb, anticipationMs, 150.0f, -20.0f);
        const int anticipationSamples = static_cast<int> (anticipationMs * 0.001 * kSampleRate);

        float reduction = 0.0f;
        for (int i = 0; i < anticipationSamples; ++i)
            reduction = env.process (0.0f); // above threshold
        CHECK_MSG (reduction >= depthDb - 0.1f,
                   "reduction should have reached full depth after anticipationMs");

        for (int i = 0; i < 1000; ++i)
            reduction = env.process (0.0f); // stays above threshold
        CHECK_MSG (reduction == depthDb, "reduction should hold at full depth while above threshold");
    }

    // Falling below threshold ramps depth -> 0 over ~releaseMs.
    {
        DuckingEnvelope env;
        env.prepare (kSampleRate);
        const float depthDb = 6.0f, releaseMs = 150.0f;
        env.setParameters (depthDb, 5.0f, releaseMs, -20.0f);

        for (int i = 0; i < 500; ++i) env.process (0.0f); // drive to full depth
        const int releaseSamples = static_cast<int> (releaseMs * 0.001 * kSampleRate);

        float reduction = depthDb;
        for (int i = 0; i < releaseSamples; ++i)
            reduction = env.process (-40.0f); // below threshold
        CHECK_MSG (reduction <= 0.1f, "reduction should have released to ~0 dB after releaseMs");

        reduction = env.process (-40.0f);
        CHECK_MSG (reduction == 0.0f, "reduction should settle exactly at 0 dB once fully released");
    }

    // Retrigger mid-release: reduction continues increasing from its current
    // (partially-released) level rather than dropping to 0 first.
    {
        DuckingEnvelope env;
        env.prepare (kSampleRate);
        const float depthDb = 6.0f;
        env.setParameters (depthDb, 5.0f, 150.0f, -20.0f);

        for (int i = 0; i < 500; ++i) env.process (0.0f);      // drive to full depth
        for (int i = 0; i < 2000; ++i) env.process (-40.0f);   // release partway
        const float partial = env.process (-40.0f);
        CHECK_MSG (partial > 0.0f && partial < depthDb, "should be partially released before retrigger");

        const float nextSample = env.process (0.0f); // sidechain hits again
        CHECK_MSG (nextSample >= partial,
                   "retrigger should ramp UP from the current level, never drop toward 0 first");
    }

    TEST_SUMMARY();
    return 0;
}
