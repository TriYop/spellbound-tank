#include "TankPluginAdapter.h"
#include <cstring>

START_NAMESPACE_DISTRHO

Plugin* createPlugin()
{
    return new TankPluginAdapter();
}

END_NAMESPACE_DISTRHO
