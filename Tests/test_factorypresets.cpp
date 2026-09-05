// Tests/test_factorypresets.cpp
#include "test_runner.h"
#include "../Source/FactoryPresets.h"

int main()
{
    const auto presets = tankFactoryPresets();

    CHECK_MSG (presets.size() == 3, "expected exactly 3 factory presets");

    for (const auto& p : presets)
    {
        CHECK_MSG (p.pluginId == "com.spellbound.tank", "every preset must stamp Tank's plugin id");
        CHECK_MSG (p.schemaVersion == 1, "schema version must be 1");
        CHECK_MSG (p.parameters.size() == 5, "every preset must set all 5 host parameters");

        bool hasBypass = false, hasDepth = false, hasAnticipation = false, hasRelease = false, hasSensitivity = false;
        for (const auto& pv : p.parameters)
        {
            if (pv.id == "bypass") hasBypass = true;
            else if (pv.id == "depth") { hasDepth = true; CHECK_MSG (pv.value >= 0.0f && pv.value <= 12.0f, "depth out of range"); }
            else if (pv.id == "anticipation") { hasAnticipation = true; CHECK_MSG (pv.value >= 1.0f && pv.value <= 20.0f, "anticipation out of range"); }
            else if (pv.id == "release") { hasRelease = true; CHECK_MSG (pv.value >= 50.0f && pv.value <= 500.0f, "release out of range"); }
            else if (pv.id == "sensitivity") { hasSensitivity = true; CHECK_MSG (pv.value >= -40.0f && pv.value <= 0.0f, "sensitivity out of range"); }
        }
        CHECK (hasBypass);
        CHECK (hasDepth);
        CHECK (hasAnticipation);
        CHECK (hasRelease);
        CHECK (hasSensitivity);
    }

    // Names are distinct and non-empty.
    CHECK (!presets[0].name.empty());
    CHECK (!presets[1].name.empty());
    CHECK (!presets[2].name.empty());
    CHECK (presets[0].name != presets[1].name);
    CHECK (presets[1].name != presets[2].name);
    CHECK (presets[0].name != presets[2].name);

    TEST_SUMMARY();
    return 0;
}
