#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

// Fixed-length lookahead delay line for one audio channel. Unlike an
// interpolated fractional delay (used elsewhere for continuously modulated
// pitch effects), this is a plain integer-sample delay: Tank's anticipation
// time only changes on parameter edits, not continuously per-sample, so no
// interpolation is needed. The buffer is always sized for the maximum
// anticipation (prepared once in prepareToPlay), so changing the live
// `delaySamples` argument never requires reallocating.
class LookaheadDelay
{
public:
    void prepare (double sampleRate, float maxDelayMs) noexcept
    {
        const int maxDelaySamples = static_cast<int> (std::ceil (maxDelayMs * 0.001 * sampleRate)) + 1;
        buffer_.assign (static_cast<size_t> (std::max (maxDelaySamples, 1)), 0.0f);
        writeIndex_ = 0;
    }

    void reset() noexcept
    {
        std::fill (buffer_.begin(), buffer_.end(), 0.0f);
        writeIndex_ = 0;
    }

    // `delaySamples` is clamped to stay within the buffer prepared via prepare().
    float process (float input, int delaySamples) noexcept
    {
        const int size = static_cast<int> (buffer_.size());
        buffer_[static_cast<size_t> (writeIndex_)] = input;

        const int clampedDelay = std::clamp (delaySamples, 0, size - 1);
        int readIndex = writeIndex_ - clampedDelay;
        if (readIndex < 0) readIndex += size;

        const float out = buffer_[static_cast<size_t> (readIndex)];
        writeIndex_ = (writeIndex_ + 1) % size;
        return out;
    }

private:
    std::vector<float> buffer_;
    int writeIndex_ = 0;
};
