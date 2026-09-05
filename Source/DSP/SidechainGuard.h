#pragma once

// Ports the sidechain-silence guard from the JUCE-era
// PluginProcessor.cpp:115-116 verbatim: a host (or a wrapper that isn't
// multi-bus aware) may declare Tank's sidechain input port present without
// backing it with real channel data. DPF's own run() contract documents
// that a channel pointer "might be null" in exactly this situation (see
// distrho/DistrhoPlugin.hpp). Treat a null pointer as silence rather than
// dereferencing it.
//
// Plain C++, no DPF/JUCE dependency, so it's independently unit-testable --
// TankPluginAdapter::run() (DPF-dependent, not unit-tested itself per this
// workspace's convention) is a thin caller of this function, one call per
// sample.
inline float readSidechainSample (const float* sidechain, unsigned int n) noexcept
{
    return sidechain != nullptr ? sidechain[n] : 0.0f;
}
