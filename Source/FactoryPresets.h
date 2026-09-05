#pragma once

#include "audioplugins/common/presets/Preset.h"

#include <vector>

// Tank's factory presets -- a new feature this migration adds (the JUCE-era
// plugin never had a presets system at all), authored from scratch rather
// than converted from existing XMLs. Compiled in rather than read from a
// bundled file at runtime, same rationale as Hex's FactoryPresets.h:
// locating a file relative to the plugin binary is brittle across VST3
// (bundle)/CLAP (flat .so)/LV2 (bundle dir) layouts.
inline std::vector<audioplugins::common::presets::Preset> tankFactoryPresets()
{
    using audioplugins::common::presets::Preset;

    std::vector<Preset> presets;

    {
        // Light, fast duck for a bass that only needs to duck out of a
        // kick's way briefly -- short reflex, shallow depth, quick cooldown.
        Preset p;
        p.name = "Subtle Pump";
        p.pluginId = "com.spellbound.tank";
        p.schemaVersion = 1;
        p.parameters = {
            {"bypass", 0.0f}, {"depth", 3.0f}, {"anticipation", 3.0f},
            {"release", 120.0f}, {"sensitivity", -18.0f},
        };
        presets.push_back(std::move(p));
    }
    {
        // Full-block, aggressive duck for an EDM-style pumping effect --
        // deep reduction, higher trigger sensitivity, moderate cooldown.
        Preset p;
        p.name = "Heavy Duck";
        p.pluginId = "com.spellbound.tank";
        p.schemaVersion = 1;
        p.parameters = {
            {"bypass", 0.0f}, {"depth", 10.0f}, {"anticipation", 8.0f},
            {"release", 200.0f}, {"sensitivity", -24.0f},
        };
        presets.push_back(std::move(p));
    }
    {
        // Moderate depth with a long, smooth recovery -- for slower material
        // where the duck should linger rather than snap back.
        Preset p;
        p.name = "Slow Cooldown";
        p.pluginId = "com.spellbound.tank";
        p.schemaVersion = 1;
        p.parameters = {
            {"bypass", 0.0f}, {"depth", 6.0f}, {"anticipation", 5.0f},
            {"release", 400.0f}, {"sensitivity", -20.0f},
        };
        presets.push_back(std::move(p));
    }

    return presets;
}
