/*
 * Spellbound Tank — DPF plugin metadata.
 *
 * Tank is a stereo effect with an extra mono sidechain input: it does not
 * want MIDI, and it declares 3 audio inputs (index 0/1 = main stereo L/R,
 * index 2 = mono sidechain) against 2 audio outputs. See
 * TankPluginAdapter::initAudioPort() for the per-index port declarations
 * (kAudioPortIsSidechain on index 2) — this file only fixes the port
 * *count*, DPF calls initAudioPort() once per index to name/tag each one.
 */

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "Spellbound"
#define DISTRHO_PLUGIN_NAME    "Tank"
#define DISTRHO_PLUGIN_URI     "https://spellbound.audio/plugins/tank"
#define DISTRHO_PLUGIN_CLAP_ID "com.spellbound.tank"

#define DISTRHO_PLUGIN_BRAND_ID  Spbd
#define DISTRHO_PLUGIN_UNIQUE_ID Tank

#define DISTRHO_PLUGIN_HAS_UI      1
#define DISTRHO_PLUGIN_IS_RT_SAFE  1
#define DISTRHO_PLUGIN_IS_SYNTH    0
#define DISTRHO_PLUGIN_NUM_INPUTS  3
#define DISTRHO_PLUGIN_NUM_OUTPUTS 2
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT 0
#define DISTRHO_PLUGIN_WANT_LATENCY    1

/*
 * The gain-reduction meter is never declared as a host parameter (or DPF
 * State) on any format, for the exact reason Hex's IN/OUT meters aren't:
 * clap-validator fails any host-visible, audio-reactive output parameter
 * regardless of hints (see Hex/CLAUDE.md's param-set-events/param-set-
 * no-cookies fix). Instead TankUI reads TankPluginAdapter directly every
 * idle tick via DISTRHO_PLUGIN_WANT_DIRECT_ACCESS +
 * UI::getPluginInstancePointer() — same idiom, same tradeoff (an
 * out-of-process/bridged LV2 host won't show Tank's UI, though DSP/audio
 * still works fine).
 */
#define DISTRHO_PLUGIN_WANT_DIRECT_ACCESS 1
// See Hex's DistrhoPluginInfo.h for why this must be forced to 0: DPF
// defaults it to DISTRHO_PLUGIN_WANT_DIRECT_ACCESS's value when unset, which
// is wrong for a CMake build that produces two separate LV2 modules
// (Tank_dsp.so, Tank_ui.so), not one combined object.
#define DISTRHO_PLUGIN_AND_UI_IN_SINGLE_OBJECT 0

#define DISTRHO_PLUGIN_VST3_CATEGORIES "Fx|Dynamics"
#define DISTRHO_PLUGIN_CLAP_FEATURES   "audio-effect", "utility", "compressor"

#define DISTRHO_UI_USE_NANOVG     1
#define DISTRHO_UI_USER_RESIZABLE 0
#define DISTRHO_UI_DEFAULT_WIDTH  500
#define DISTRHO_UI_DEFAULT_HEIGHT 284
#define DISTRHO_UI_FILE_BROWSER   1

/*
 * Host parameter indices, shared between TankPluginAdapter and TankUI.
 * Declared here (not in TankPluginAdapter.h) so TankUI.cpp can use them
 * without pulling in DistrhoPlugin.hpp — same placement Hex uses.
 */
enum TankParameters {
    kParameterBypass = 0,
    kParameterDepth,
    kParameterAnticipation,
    kParameterRelease,
    kParameterSensitivity,
    kParameterCount   // 5
};

/* Ranges/defaults carried over verbatim from the JUCE-era
   Source/_juce_reference/PluginProcessor.cpp::createParameterLayout(). */
#define TANK_PARAM_BYPASS_DEFAULT 0.0f

#define TANK_PARAM_DEPTH_MIN     0.0f
#define TANK_PARAM_DEPTH_MAX     12.0f
#define TANK_PARAM_DEPTH_DEFAULT 6.0f

#define TANK_PARAM_ANTICIPATION_MIN     1.0f
#define TANK_PARAM_ANTICIPATION_MAX     20.0f
#define TANK_PARAM_ANTICIPATION_DEFAULT 5.0f

#define TANK_PARAM_RELEASE_MIN     50.0f
#define TANK_PARAM_RELEASE_MAX     500.0f
#define TANK_PARAM_RELEASE_DEFAULT 150.0f

#define TANK_PARAM_SENSITIVITY_MIN     -40.0f
#define TANK_PARAM_SENSITIVITY_MAX     0.0f
#define TANK_PARAM_SENSITIVITY_DEFAULT -20.0f

// Mirrors TankAudioProcessor::kMaxAnticipationMs (PluginProcessor.h) --
// the lookahead delay lines are sized once for this ceiling.
#define TANK_MAX_ANTICIPATION_MS 20.0f

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
