#include "test_runner.h"
#include "../Source/DSP/LookaheadDelay.h"

int main()
{
    // Output at time n equals input at n - delaySamples, once the buffer has filled.
    {
        LookaheadDelay d;
        d.prepare (48000.0, 20.0f); // max 20ms @ 48kHz -> buffer >= 960 samples
        const int delaySamples = 100;
        float lastOut = 0.0f;
        for (int n = 0; n < 500; ++n)
        {
            const float in = static_cast<float> (n);
            lastOut = d.process (in, delaySamples);
        }
        // At n=499 (0-indexed, 500th call), output should equal input at n=399.
        CHECK_MSG (lastOut == 399.0f, "output should equal input delaySamples samples ago");
    }

    // Before the buffer has been filled with delaySamples worth of input,
    // reads before the start return the (silent) initial buffer contents.
    {
        LookaheadDelay d;
        d.prepare (48000.0, 20.0f);
        const int delaySamples = 50;
        const float out = d.process (1.0f, delaySamples); // 1st sample, nothing written 50 samples ago
        CHECK_MSG (out == 0.0f, "reads before any real input was written should return silence");
    }

    // reset() clears the buffer.
    {
        LookaheadDelay d;
        d.prepare (48000.0, 20.0f);
        for (int n = 0; n < 200; ++n) d.process (1.0f, 10);
        d.reset();
        const float out = d.process (0.0f, 10);
        CHECK_MSG (out == 0.0f, "reset() should clear the delay buffer");
    }

    // delaySamples larger than the prepared max is clamped to (bufferSize - 1),
    // not a crash and not garbage: once the buffer has wrapped enough times,
    // the output settles into the fixed, computable delay the clamp implies.
    {
        LookaheadDelay d;
        d.prepare (48000.0, 5.0f); // max 5ms @ 48kHz -> buffer size = ceil(240)+1 = 241
        const int bufferSize    = 241;
        const int clampedDelay  = bufferSize - 1; // 240
        const int totalCalls    = 2000;
        float out = 0.0f;
        for (int n = 0; n < totalCalls; ++n)
            out = d.process (static_cast<float> (n), 100000); // way past buffer size
        const float expected = static_cast<float> (totalCalls - 1 - clampedDelay);
        CHECK_MSG (out == expected,
                   "an out-of-range delay should clamp to (bufferSize - 1), not crash or return garbage");
    }

    TEST_SUMMARY();
    return 0;
}
