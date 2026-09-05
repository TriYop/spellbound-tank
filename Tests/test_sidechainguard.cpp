// Tests/test_sidechainguard.cpp
#include "test_runner.h"
#include "../Source/DSP/SidechainGuard.h"

int main()
{
    // A null sidechain pointer (bus declared but not backed by real data,
    // e.g. some hosts and DPF's own Standalone-equivalent) reads as silence
    // for every sample index.
    {
        CHECK (readSidechainSample (nullptr, 0) == 0.0f);
        CHECK (readSidechainSample (nullptr, 511) == 0.0f);
    }

    // A real buffer reads back its own samples, unmodified.
    {
        float buffer[4] = { 0.25f, -0.5f, 1.0f, -1.0f };
        CHECK (readSidechainSample (buffer, 0) == 0.25f);
        CHECK (readSidechainSample (buffer, 1) == -0.5f);
        CHECK (readSidechainSample (buffer, 2) == 1.0f);
        CHECK (readSidechainSample (buffer, 3) == -1.0f);
    }

    TEST_SUMMARY();
    return 0;
}
