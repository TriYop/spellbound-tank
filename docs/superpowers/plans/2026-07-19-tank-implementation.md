# Tank Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build Tank, a JUCE sidechain "pumping" compressor (VST3/CLAP/Standalone) that ducks a main bus by a fixed dB amount whenever a sidechain signal crosses a threshold, using true lookahead so the dip lands in sync with the sidechain transient.

**Architecture:** Four small, JUCE-free, independently unit-tested DSP classes (`BandpassFilter`, `RmsDetector`, `LookaheadDelay`, `DuckingEnvelope`) wired together in a standard JUCE `AudioProcessor`/`AudioProcessorEditor` pair, following the exact CMake/JUCE/clap-juce-extensions pattern and JUCE-free `Tests/` convention already used by the sibling plugins `Outflank` and `Hex`.

**Tech Stack:** C++20, CMake ≥ 3.22 + Ninja, JUCE 8.0.13 (fetched via `FetchContent`), clap-juce-extensions (`main`), CTest for unit tests (no test framework dependency — plain `CHECK`/`CHECK_MSG` macros, same as siblings).

**Spec:** `docs/superpowers/specs/2026-07-18-tank-design.md` (approved). This plan implements that spec exactly; do not deviate from its parameter ranges, bus layout, or bypass semantics without checking back with the user.

## Global Constraints

- C++20, no exceptions/RTTI assumptions beyond what JUCE itself requires.
- DSP classes under `Source/DSP/` must have **zero JUCE dependency** — plain C++ only (`<cmath>`, `<algorithm>`, `<vector>`, `<array>`), so they can be compiled and unit-tested without pulling in JUCE at all. This matches Outflank's and Hex's `Tests/` convention exactly.
- No dynamic allocation inside `processBlock()` or any DSP class's `process()` method — allocation only happens in `prepare()`, which is called from `prepareToPlay()` (never from the audio thread mid-stream).
- Parameter IDs, ranges, and defaults are fixed by the spec: `bypass` (bool, default false), `depth` (0–12 dB, default 6), `anticipation` (1–20 ms, default 5), `release` (50–500 ms, default 150), `sensitivity` (−40–0 dB, default −20). Do not rename or re-range these.
- Company/branding convention (majority of siblings): `COMPANY_NAME "Spellbound"`, `PLUGIN_MANUFACTURER_CODE Spbd`, `BUNDLE_ID "com.spellbound.tank"`, `PLUGIN_CODE Tank`.
- Every commit must leave `cmake --build build --parallel` succeeding — never commit a build-broken state.

---

## Task 1: Project Scaffold

**Files:**
- Create: `CMakeLists.txt`
- Create: `Source/PluginProcessor.h`
- Create: `Source/PluginProcessor.cpp`
- Create: `Source/PluginEditor.h`
- Create: `Source/PluginEditor.cpp`

**Interfaces:**
- Produces: `TankAudioProcessor` (declared in `Source/PluginProcessor.h`) with a public `juce::AudioProcessorValueTreeState apvts` member and `static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()`. Produces `TankAudioProcessorEditor` (declared in `Source/PluginEditor.h`) taking a `TankAudioProcessor&` in its constructor. Later tasks (6, 8) modify both files further; nothing consumes them yet.

This task stands up a buildable, empty-but-correct plugin skeleton: full parameter layout and the 3-bus layout (Main stereo in/out + mono Sidechain in) from the start, `processBlock()` left as a pure passthrough (JUCE's shared in-place buffer means an empty `processBlock()` body already *is* a correct passthrough — this is not a placeholder, it's the correct minimal implementation until Task 6 wires in the DSP chain).

- [ ] **Step 1: Create `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.22)
project(Tank VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.13
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(clap-juce-extensions)

set(TANK_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND TANK_FORMATS AU)
endif()

juce_add_plugin(Tank
    COMPANY_NAME                "Spellbound"
    PLUGIN_MANUFACTURER_CODE    Spbd
    PLUGIN_CODE                 Tank
    FORMATS                     ${TANK_FORMATS}
    PRODUCT_NAME                "Tank"
    BUNDLE_ID                   "com.spellbound.tank"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    DESCRIPTION                 "Sidechain pumping compressor"
)

target_sources(Tank PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(Tank PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_include_directories(Tank PRIVATE Source/)

target_compile_features(Tank PUBLIC cxx_std_20)

target_link_libraries(Tank
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
        juce::juce_dsp
        clap_juce_extensions
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

clap_juce_extensions_plugin(
    TARGET      Tank
    CLAP_ID     "com.spellbound.tank"
    CLAP_FEATURES audio-effect utility compressor
    CLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES 64
)

# ── Install rules ─────────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(_ARTS "${CMAKE_BINARY_DIR}/Tank_artefacts/${CMAKE_BUILD_TYPE}")

install(DIRECTORY  "${_ARTS}/VST3/Tank.vst3"
        DESTINATION VST3
        COMPONENT   Runtime
        USE_SOURCE_PERMISSIONS)

install(FILES      "${_ARTS}/CLAP/Tank.clap"
        DESTINATION CLAP
        COMPONENT   Runtime
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

install(PROGRAMS   "${_ARTS}/Standalone/Tank"
        DESTINATION bin
        COMPONENT   Runtime)

install(PROGRAMS   "${CMAKE_CURRENT_SOURCE_DIR}/scripts/install.sh"
                   "${CMAKE_CURRENT_SOURCE_DIR}/scripts/uninstall.sh"
        DESTINATION .
        COMPONENT   Runtime)

# ── CPack (TGZ) ───────────────────────────────────────────────────────────────
set(CPACK_GENERATOR                 TGZ)
set(CPACK_PACKAGE_NAME              Tank)
set(CPACK_PACKAGE_VENDOR            Spellbound)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Sidechain pumping compressor")
set(CPACK_PACKAGE_VERSION           ${PROJECT_VERSION})
set(CPACK_SYSTEM_NAME               linux-x86_64)
set(CPACK_PACKAGE_FILE_NAME         "Tank-${PROJECT_VERSION}-linux-x86_64")
set(CPACK_PACKAGING_INSTALL_PREFIX  "")
set(CPACK_STRIP_FILES               TRUE)
set(CPACK_PACKAGE_CHECKSUM          SHA256)
set(CPACK_INSTALL_CMAKE_PROJECTS
    "${CMAKE_BINARY_DIR};${PROJECT_NAME};Runtime;/")
include(CPack)
```

- [ ] **Step 2: Create `Source/PluginProcessor.h`**

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class TankAudioProcessor : public juce::AudioProcessor
{
public:
    TankAudioProcessor();
    ~TankAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

private:
    double sampleRate_ = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TankAudioProcessor)
};
```

- [ ] **Step 3: Create `Source/PluginProcessor.cpp`**

```cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout TankAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterBool> ("bypass", "Bypass", false));

    params.push_back (std::make_unique<AudioParameterFloat> ("depth", "Depth",
        NormalisableRange<float> (0.f, 12.f, 0.1f), 6.f,
        AudioParameterFloatAttributes{}.withLabel ("dB")));

    params.push_back (std::make_unique<AudioParameterFloat> ("anticipation", "Anticipation",
        NormalisableRange<float> (1.f, 20.f, 0.1f), 5.f,
        AudioParameterFloatAttributes{}.withLabel ("ms")));

    params.push_back (std::make_unique<AudioParameterFloat> ("release", "Release",
        NormalisableRange<float> (50.f, 500.f, 1.f), 150.f,
        AudioParameterFloatAttributes{}.withLabel ("ms")));

    params.push_back (std::make_unique<AudioParameterFloat> ("sensitivity", "Sensitivity",
        NormalisableRange<float> (-40.f, 0.f, 0.1f), -20.f,
        AudioParameterFloatAttributes{}.withLabel ("dB")));

    return { params.begin(), params.end() };
}

TankAudioProcessor::TankAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Main",      juce::AudioChannelSet::stereo(), true)
          .withInput  ("Sidechain", juce::AudioChannelSet::mono(),   true)
          .withOutput ("Main",      juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Tank", createParameterLayout())
{
}

TankAudioProcessor::~TankAudioProcessor() = default;

const juce::String TankAudioProcessor::getName() const { return JucePlugin_Name; }
bool TankAudioProcessor::acceptsMidi()  const { return false; }
bool TankAudioProcessor::producesMidi() const { return false; }
bool TankAudioProcessor::isMidiEffect() const { return false; }
double TankAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int  TankAudioProcessor::getNumPrograms()              { return 1; }
int  TankAudioProcessor::getCurrentProgram()           { return 0; }
void TankAudioProcessor::setCurrentProgram (int)       {}
const juce::String TankAudioProcessor::getProgramName (int) { return {}; }
void TankAudioProcessor::changeProgramName (int, const juce::String&) {}

void TankAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRate_ = sampleRate;
}

void TankAudioProcessor::releaseResources() {}

bool TankAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getChannelSet (true, 1) != juce::AudioChannelSet::mono())
        return false;
    return true;
}

void TankAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    // Passthrough until Task 6 wires in the sidechain-ducking DSP chain.
    // JUCE hands processBlock() a buffer already containing the input audio
    // in-place, so doing nothing here is a correct, click-free passthrough.
    juce::ignoreUnused (buffer);
}

bool TankAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* TankAudioProcessor::createEditor()
{
    return new TankAudioProcessorEditor (*this);
}

void TankAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void TankAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TankAudioProcessor();
}
```

- [ ] **Step 4: Create `Source/PluginEditor.h`**

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class TankAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit TankAudioProcessorEditor (TankAudioProcessor&);
    ~TankAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    TankAudioProcessor& processor_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TankAudioProcessorEditor)
};
```

- [ ] **Step 5: Create `Source/PluginEditor.cpp`**

```cpp
#include "PluginEditor.h"

TankAudioProcessorEditor::TankAudioProcessorEditor (TankAudioProcessor& p)
    : AudioProcessorEditor (&p), processor_ (p)
{
    setSize (500, 220);
}

void TankAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void TankAudioProcessorEditor::resized()
{
}
```

- [ ] **Step 6: Configure and build**

Run:
```bash
sudo apt install cmake ninja-build build-essential git \
    libasound2-dev libjack-jackd2-dev \
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
    libxinerama-dev libxrandr-dev libxrender-dev \
    libfreetype-dev libfontconfig1-dev \
    libglu1-mesa-dev libwebkit2gtk-4.1-dev
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel
```
Expected: configure succeeds (first run fetches JUCE + clap-juce-extensions into `build/_deps/`, ~2 min); build succeeds with `Tank_VST3`, `Tank_Standalone`, and the CLAP wrapper all produced under `build/Tank_artefacts/Debug/`.

- [ ] **Step 7: Smoke-test the Standalone**

Run: `timeout 3 ./build/Tank_artefacts/Debug/Standalone/Tank || true`
Expected: window opens (black, empty) and process exits cleanly at the 3s timeout without crashing/segfaulting.

- [ ] **Step 8: Commit**

```bash
git add CMakeLists.txt Source/PluginProcessor.h Source/PluginProcessor.cpp Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "Add Tank project scaffold: bus layout, APVTS params, passthrough processBlock"
```

---

## Task 2: BandpassFilter (50–150 Hz sidechain detection filter)

**Files:**
- Create: `Source/DSP/BandpassFilter.h`
- Create: `Tests/test_runner.h`
- Create: `Tests/test_bandpassfilter.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `class BandpassFilter` with `void prepare(double sampleRate) noexcept`, `void reset() noexcept`, `float process(float input) noexcept`. Consumed by `TankAudioProcessor` in Task 6 as `#include "DSP/BandpassFilter.h"`.

- [ ] **Step 1: Create `Tests/test_runner.h`** (shared by all DSP tests, copied verbatim from Outflank's convention)

```cpp
#pragma once
#include <cstdio>
#include <cstdlib>

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(expr) \
    do { \
        if (expr) { \
            ++g_passed; \
        } else { \
            ++g_failed; \
            std::fprintf(stderr, "FAIL  %s:%d  %s\n", __FILE__, __LINE__, #expr); \
        } \
    } while(0)

#define CHECK_MSG(expr, msg) \
    do { \
        if (expr) { \
            ++g_passed; \
        } else { \
            ++g_failed; \
            std::fprintf(stderr, "FAIL  %s:%d  %s  (%s)\n", __FILE__, __LINE__, #expr, msg); \
        } \
    } while(0)

#define TEST_SUMMARY() \
    do { \
        std::printf("\n%d passed, %d failed\n", g_passed, g_failed); \
        if (g_failed > 0) std::exit(1); \
    } while(0)
```

- [ ] **Step 2: Write the failing test — `Tests/test_bandpassfilter.cpp`**

```cpp
#include "test_runner.h"
#include "../Source/DSP/BandpassFilter.h"
#include <cmath>
#include <algorithm>

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kSampleRate = 48000.0;

static float steadyStatePeak (BandpassFilter& f, double freqHz, int numSamples)
{
    float peak = 0.f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float in = static_cast<float> (std::sin (2.0 * kPi * freqHz * i / kSampleRate));
        const float out = f.process (in);
        if (i > numSamples / 2) peak = std::max (peak, std::abs (out));
    }
    return peak;
}

int main()
{
    // Passband: near the ~86.6 Hz center (geometric mean of 50 and 150 Hz),
    // steady-state gain should stay close to unity.
    {
        BandpassFilter f;
        f.prepare (kSampleRate);
        const float peak = steadyStatePeak (f, 86.6, 9600);
        CHECK_MSG (peak > 0.9f && peak < 1.1f,
                   "center frequency should pass at close to unity gain");
    }

    // Stopband below the band: 10 Hz should be heavily attenuated.
    {
        BandpassFilter f;
        f.prepare (kSampleRate);
        const float peak = steadyStatePeak (f, 10.0, 9600);
        CHECK_MSG (peak < 0.3f, "10 Hz should be attenuated well below the band");
    }

    // Stopband above the band: 2000 Hz should be heavily attenuated.
    {
        BandpassFilter f;
        f.prepare (kSampleRate);
        const float peak = steadyStatePeak (f, 2000.0, 9600);
        CHECK_MSG (peak < 0.3f, "2000 Hz should be attenuated well above the band");
    }

    // reset() clears filter state (no residual output after reset with silent input).
    {
        BandpassFilter f;
        f.prepare (kSampleRate);
        steadyStatePeak (f, 86.6, 4800);
        f.reset();
        const float out = f.process (0.0f);
        CHECK_MSG (out == 0.0f, "reset() should clear internal state to exactly zero");
    }

    TEST_SUMMARY();
    return 0;
}
```

- [ ] **Step 3: Add the CMake test target (part of Modify `CMakeLists.txt`)**

Add this block right after the `clap_juce_extensions_plugin(...)` block (this is the first test, so it also introduces the `enable_testing()` section):

```cmake
# ── Unit tests (no JUCE dependency) ──────────────────────────────────────────
enable_testing()

add_executable(test_bandpassfilter
    Tests/test_bandpassfilter.cpp
)
target_include_directories(test_bandpassfilter PRIVATE Source/ Tests/)
target_compile_features(test_bandpassfilter PRIVATE cxx_std_20)
add_test(NAME BandpassFilter COMMAND test_bandpassfilter)
```

Also add `Source/DSP/BandpassFilter.h` to the existing `target_sources(Tank PRIVATE ...)` list (for IDE visibility — it's header-only so nothing else needs it):

```cmake
target_sources(Tank PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/BandpassFilter.h
)
```

- [ ] **Step 4: Run test to verify it fails**

Run:
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --target test_bandpassfilter
```
Expected: FAIL — compile error, `fatal error: DSP/BandpassFilter.h: No such file or directory` (referenced indirectly via `../Source/DSP/BandpassFilter.h` in the test).

- [ ] **Step 5: Write the minimal implementation — `Source/DSP/BandpassFilter.h`**

```cpp
#pragma once
#include <cmath>

// Fixed 50-150 Hz bandpass isolating the kick fundamental range for
// sidechain RMS detection. Zavalishin topology-preserving 2-pole state-
// variable filter (same math as a standard TPT SVF), specialized to only
// expose the band output at a fixed center frequency/Q.
//
// Center frequency is the geometric mean of the band edges
// (sqrt(50*150) ~= 86.6 Hz); Q set so the -3dB points land at
// approximately 50 Hz and 150 Hz for a single 2-pole SVF bandpass response.
//
// Plain C++, no JUCE dependency, so it can be exercised by bare unit tests.
class BandpassFilter
{
public:
    void prepare (double sampleRate) noexcept
    {
        reset();
        const float g = std::tan (kPi * kCenterHz / static_cast<float> (sampleRate));
        const float k = 1.0f / kQ;
        a1_ = 1.0f / (1.0f + g * (g + k));
        a2_ = g * a1_;
        a3_ = g * a2_;
    }

    void reset() noexcept
    {
        ic1eq_ = 0.0f;
        ic2eq_ = 0.0f;
    }

    float process (float input) noexcept
    {
        const float v3 = input - ic2eq_;
        const float v1 = a1_ * ic1eq_ + a2_ * v3;
        const float v2 = ic2eq_ + a2_ * ic1eq_ + a3_ * v3;
        ic1eq_ = 2.0f * v1 - ic1eq_;
        ic2eq_ = 2.0f * v2 - ic2eq_;
        return v1; // band output
    }

private:
    static constexpr float kPi       = 3.14159265358979323846f;
    static constexpr float kCenterHz = 86.6f;
    static constexpr float kQ        = 0.866f;

    float a1_ = 0.0f, a2_ = 0.0f, a3_ = 0.0f;
    float ic1eq_ = 0.0f, ic2eq_ = 0.0f;
};
```

- [ ] **Step 6: Run test to verify it passes**

Run: `cmake --build build --target test_bandpassfilter && ./build/test_bandpassfilter`
Expected: `PASS` — output ends with `4 passed, 0 failed`.

- [ ] **Step 7: Commit**

```bash
git add Source/DSP/BandpassFilter.h Tests/test_runner.h Tests/test_bandpassfilter.cpp CMakeLists.txt
git commit -m "Add BandpassFilter DSP class with unit tests"
```

---

## Task 3: RmsDetector (sidechain level → dBFS)

**Files:**
- Create: `Source/DSP/RmsDetector.h`
- Create: `Tests/test_rmsdetector.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: nothing from prior tasks (standalone class).
- Produces: `class RmsDetector` with `void prepare(double sampleRate) noexcept`, `void reset() noexcept`, `float process(float input) noexcept` returning a level in dBFS. Consumed by `TankAudioProcessor` in Task 6.

- [ ] **Step 1: Write the failing test — `Tests/test_rmsdetector.cpp`**

```cpp
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
```

- [ ] **Step 2: Add the CMake test target (Modify `CMakeLists.txt`)**

Add after the `test_bandpassfilter` block:

```cmake
add_executable(test_rmsdetector
    Tests/test_rmsdetector.cpp
)
target_include_directories(test_rmsdetector PRIVATE Source/ Tests/)
target_compile_features(test_rmsdetector PRIVATE cxx_std_20)
add_test(NAME RmsDetector COMMAND test_rmsdetector)
```

Add `Source/DSP/RmsDetector.h` to `target_sources(Tank PRIVATE ...)`.

- [ ] **Step 3: Run test to verify it fails**

Run: `cmake --build build --target test_rmsdetector`
Expected: FAIL — `fatal error: DSP/RmsDetector.h: No such file or directory`.

- [ ] **Step 4: Write the minimal implementation — `Source/DSP/RmsDetector.h`**

```cpp
#pragma once
#include <cmath>
#include <algorithm>

// Windowed RMS envelope detector producing the input level in dBFS, used to
// drive Tank's sidechain trigger. Implemented as a one-pole low-pass on the
// squared signal (~10 ms integration time) rather than a sliding-window sum,
// avoiding a buffer while still giving a smooth, RMS-like reading.
class RmsDetector
{
public:
    void prepare (double sampleRate) noexcept
    {
        coeff_ = 1.0f - std::exp (-1.0f / (kWindowMs * 0.001f * static_cast<float> (sampleRate)));
        reset();
    }

    void reset() noexcept { meanSquare_ = 0.0f; }

    // Returns the current level in dBFS (silence floors at kMinDb).
    float process (float input) noexcept
    {
        meanSquare_ += coeff_ * (input * input - meanSquare_);
        const float rms = std::sqrt (std::max (meanSquare_, 0.0f));
        const float db  = 20.0f * std::log10 (std::max (rms, kMinLinear));
        return std::max (db, kMinDb);
    }

private:
    static constexpr float kWindowMs  = 10.0f;
    static constexpr float kMinLinear = 1.0e-6f;   // -120 dBFS floor
    static constexpr float kMinDb     = -120.0f;

    float coeff_      = 0.0f;
    float meanSquare_ = 0.0f;
};
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cmake --build build --target test_rmsdetector && ./build/test_rmsdetector`
Expected: `PASS` — `4 passed, 0 failed`.

- [ ] **Step 6: Commit**

```bash
git add Source/DSP/RmsDetector.h Tests/test_rmsdetector.cpp CMakeLists.txt
git commit -m "Add RmsDetector DSP class with unit tests"
```

---

## Task 4: LookaheadDelay (fixed-length per-channel delay)

**Files:**
- Create: `Source/DSP/LookaheadDelay.h`
- Create: `Tests/test_lookaheaddelay.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `class LookaheadDelay` with `void prepare(double sampleRate, float maxDelayMs) noexcept`, `void reset() noexcept`, `float process(float input, int delaySamples) noexcept`. Consumed by `TankAudioProcessor` in Task 6 as a `std::array<LookaheadDelay, 2>` (one per main-bus channel).

- [ ] **Step 1: Write the failing test — `Tests/test_lookaheaddelay.cpp`**

```cpp
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
```

- [ ] **Step 2: Add the CMake test target (Modify `CMakeLists.txt`)**

Add after the `test_rmsdetector` block:

```cmake
add_executable(test_lookaheaddelay
    Tests/test_lookaheaddelay.cpp
)
target_include_directories(test_lookaheaddelay PRIVATE Source/ Tests/)
target_compile_features(test_lookaheaddelay PRIVATE cxx_std_20)
add_test(NAME LookaheadDelay COMMAND test_lookaheaddelay)
```

Add `Source/DSP/LookaheadDelay.h` to `target_sources(Tank PRIVATE ...)`.

- [ ] **Step 3: Run test to verify it fails**

Run: `cmake --build build --target test_lookaheaddelay`
Expected: FAIL — `fatal error: DSP/LookaheadDelay.h: No such file or directory`.

- [ ] **Step 4: Write the minimal implementation — `Source/DSP/LookaheadDelay.h`**

```cpp
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
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cmake --build build --target test_lookaheaddelay && ./build/test_lookaheaddelay`
Expected: `PASS` — `4 passed, 0 failed`.

- [ ] **Step 6: Commit**

```bash
git add Source/DSP/LookaheadDelay.h Tests/test_lookaheaddelay.cpp CMakeLists.txt
git commit -m "Add LookaheadDelay DSP class with unit tests"
```

---

## Task 5: DuckingEnvelope (the core trigger/ramp state machine)

**Files:**
- Create: `Source/DSP/DuckingEnvelope.h`
- Create: `Tests/test_duckingenvelope.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: nothing from prior tasks (standalone class), but is driven by `RmsDetector`'s dBFS output (Task 3) once wired in Task 6.
- Produces: `class DuckingEnvelope` with `void prepare(double sampleRate) noexcept`, `void reset() noexcept`, `void setParameters(float depthDb, float anticipationMs, float releaseMs, float sensitivityDb) noexcept`, `float process(float levelDb) noexcept` returning the current gain reduction in dB (0 = none, positive = attenuate). Consumed by `TankAudioProcessor` in Task 6.

- [ ] **Step 1: Write the failing test — `Tests/test_duckingenvelope.cpp`**

```cpp
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
```

- [ ] **Step 2: Add the CMake test target (Modify `CMakeLists.txt`)**

Add after the `test_lookaheaddelay` block:

```cmake
add_executable(test_duckingenvelope
    Tests/test_duckingenvelope.cpp
)
target_include_directories(test_duckingenvelope PRIVATE Source/ Tests/)
target_compile_features(test_duckingenvelope PRIVATE cxx_std_20)
add_test(NAME DuckingEnvelope COMMAND test_duckingenvelope)
```

Add `Source/DSP/DuckingEnvelope.h` to `target_sources(Tank PRIVATE ...)`.

- [ ] **Step 3: Run test to verify it fails**

Run: `cmake --build build --target test_duckingenvelope`
Expected: FAIL — `fatal error: DSP/DuckingEnvelope.h: No such file or directory`.

- [ ] **Step 4: Write the minimal implementation — `Source/DSP/DuckingEnvelope.h`**

```cpp
#pragma once
#include <algorithm>

// Ducking gain-reduction envelope: RMS crossing `sensitivityDb` ramps gain
// reduction from its current level toward `depthDb` over `anticipationMs`
// (paired with Tank's lookahead delay so the dip lands in sync with the
// delayed sidechain transient), holds while RMS stays above threshold, then
// ramps back to 0 over `releaseMs` once RMS falls back below threshold.
//
// Re-triggering while still releasing does NOT reset the level to 0 -- it
// re-enters the ramp-toward-depth state from wherever the level currently
// is, the same smooth-retrigger principle as Catalyst's AdsrEnvelope
// (Catalyst/Source/DSP/AdsrEnvelope.h): only the state changes on
// noteOn()/threshold-cross, the level itself is left untouched.
class DuckingEnvelope
{
public:
    void prepare (double sampleRate) noexcept
    {
        sampleRate_ = sampleRate;
        reset();
    }

    void reset() noexcept
    {
        state_ = State::Idle;
        reductionDb_ = 0.0f;
    }

    void setParameters (float depthDb, float anticipationMs, float releaseMs, float sensitivityDb) noexcept
    {
        depthDb_       = depthDb;
        sensitivityDb_ = sensitivityDb;
        rampUpRate_    = depthDb / static_cast<float> (std::max (anticipationMs * 0.001 * sampleRate_, 1.0));
        rampDownRate_  = depthDb / static_cast<float> (std::max (releaseMs * 0.001 * sampleRate_, 1.0));
    }

    // `levelDb` is the sidechain's filtered RMS level for this sample.
    // Returns the gain reduction to apply, in dB (0 = no reduction, positive = attenuate).
    float process (float levelDb) noexcept
    {
        const bool above = levelDb > sensitivityDb_;

        if (above && (state_ == State::Idle || state_ == State::Releasing))
            state_ = State::Anticipating;
        else if (! above && (state_ == State::Anticipating || state_ == State::Holding))
            state_ = State::Releasing;

        switch (state_)
        {
            case State::Idle:
                reductionDb_ = 0.0f;
                break;

            case State::Anticipating:
                reductionDb_ = std::min (reductionDb_ + rampUpRate_, depthDb_);
                if (reductionDb_ >= depthDb_)
                    state_ = State::Holding;
                break;

            case State::Holding:
                reductionDb_ = depthDb_;
                break;

            case State::Releasing:
                reductionDb_ = std::max (reductionDb_ - rampDownRate_, 0.0f);
                if (reductionDb_ <= 0.0f)
                    state_ = State::Idle;
                break;
        }

        return reductionDb_;
    }

private:
    enum class State { Idle, Anticipating, Holding, Releasing };

    double sampleRate_    = 48000.0;
    State  state_         = State::Idle;
    float  reductionDb_   = 0.0f;
    float  depthDb_       = 6.0f;
    float  sensitivityDb_ = -20.0f;
    float  rampUpRate_    = 0.0f;
    float  rampDownRate_  = 0.0f;
};
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cmake --build build --target test_duckingenvelope && ./build/test_duckingenvelope`
Expected: `PASS` — `6 passed, 0 failed`.

- [ ] **Step 6: Commit**

```bash
git add Source/DSP/DuckingEnvelope.h Tests/test_duckingenvelope.cpp CMakeLists.txt
git commit -m "Add DuckingEnvelope DSP class with unit tests"
```

---

## Task 6: Wire the DSP Chain into PluginProcessor

**Files:**
- Modify: `Source/PluginProcessor.h` (replace entire file)
- Modify: `Source/PluginProcessor.cpp` (replace entire file)

**Interfaces:**
- Consumes: `BandpassFilter::{prepare,reset,process}` (Task 2), `RmsDetector::{prepare,reset,process}` (Task 3), `LookaheadDelay::{prepare,reset,process}` (Task 4), `DuckingEnvelope::{prepare,reset,setParameters,process}` (Task 5).
- Produces: `TankAudioProcessor::getCurrentReductionDb() const noexcept -> float`, consumed by the GR meter in Task 8.

- [ ] **Step 1: Replace `Source/PluginProcessor.h`**

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "DSP/BandpassFilter.h"
#include "DSP/RmsDetector.h"
#include "DSP/LookaheadDelay.h"
#include "DSP/DuckingEnvelope.h"

class TankAudioProcessor : public juce::AudioProcessor
{
public:
    TankAudioProcessor();
    ~TankAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Current gain reduction in dB (0 while bypassed), read by the editor's
    // GR meter on a UI timer -- never touched from the audio thread's
    // reader side, only written there.
    float getCurrentReductionDb() const noexcept
    {
        return currentReductionDb_.load (std::memory_order_relaxed);
    }

    static constexpr float kMaxDepthDb = 12.0f;

    juce::AudioProcessorValueTreeState apvts;

private:
    static constexpr float kMaxAnticipationMs = 20.0f;

    double sampleRate_ = 44100.0;

    BandpassFilter   sidechainFilter_;
    RmsDetector      sidechainRms_;
    DuckingEnvelope  duckEnvelope_;
    std::array<LookaheadDelay, 2> lookaheadDelay_;
    juce::SmoothedValue<float> gainSmoothed_;

    std::atomic<float> currentReductionDb_ { 0.0f };
    int reportedLatencySamples_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TankAudioProcessor)
};
```

- [ ] **Step 2: Replace `Source/PluginProcessor.cpp`**

```cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout TankAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterBool> ("bypass", "Bypass", false));

    params.push_back (std::make_unique<AudioParameterFloat> ("depth", "Depth",
        NormalisableRange<float> (0.f, 12.f, 0.1f), 6.f,
        AudioParameterFloatAttributes{}.withLabel ("dB")));

    params.push_back (std::make_unique<AudioParameterFloat> ("anticipation", "Anticipation",
        NormalisableRange<float> (1.f, 20.f, 0.1f), 5.f,
        AudioParameterFloatAttributes{}.withLabel ("ms")));

    params.push_back (std::make_unique<AudioParameterFloat> ("release", "Release",
        NormalisableRange<float> (50.f, 500.f, 1.f), 150.f,
        AudioParameterFloatAttributes{}.withLabel ("ms")));

    params.push_back (std::make_unique<AudioParameterFloat> ("sensitivity", "Sensitivity",
        NormalisableRange<float> (-40.f, 0.f, 0.1f), -20.f,
        AudioParameterFloatAttributes{}.withLabel ("dB")));

    return { params.begin(), params.end() };
}

TankAudioProcessor::TankAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Main",      juce::AudioChannelSet::stereo(), true)
          .withInput  ("Sidechain", juce::AudioChannelSet::mono(),   true)
          .withOutput ("Main",      juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Tank", createParameterLayout())
{
}

TankAudioProcessor::~TankAudioProcessor() = default;

const juce::String TankAudioProcessor::getName() const { return JucePlugin_Name; }
bool TankAudioProcessor::acceptsMidi()  const { return false; }
bool TankAudioProcessor::producesMidi() const { return false; }
bool TankAudioProcessor::isMidiEffect() const { return false; }
double TankAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int  TankAudioProcessor::getNumPrograms()              { return 1; }
int  TankAudioProcessor::getCurrentProgram()           { return 0; }
void TankAudioProcessor::setCurrentProgram (int)       {}
const juce::String TankAudioProcessor::getProgramName (int) { return {}; }
void TankAudioProcessor::changeProgramName (int, const juce::String&) {}

void TankAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRate_ = sampleRate;
    juce::ignoreUnused (samplesPerBlock);

    sidechainFilter_.prepare (sampleRate);
    sidechainRms_.prepare (sampleRate);
    duckEnvelope_.prepare (sampleRate);

    for (auto& delay : lookaheadDelay_)
        delay.prepare (sampleRate, kMaxAnticipationMs);

    gainSmoothed_.reset (sampleRate, 0.001);
    gainSmoothed_.setCurrentAndTargetValue (1.0f);

    reportedLatencySamples_ = static_cast<int> (std::round (
        apvts.getRawParameterValue ("anticipation")->load() * 0.001 * sampleRate_));
    setLatencySamples (reportedLatencySamples_);
}

void TankAudioProcessor::releaseResources() {}

bool TankAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getChannelSet (true, 1) != juce::AudioChannelSet::mono())
        return false;
    return true;
}

void TankAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const bool  bypassed        = apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    const float depthDb         = apvts.getRawParameterValue ("depth")->load();
    const float anticipationMs  = apvts.getRawParameterValue ("anticipation")->load();
    const float releaseMs       = apvts.getRawParameterValue ("release")->load();
    const float sensitivityDb   = apvts.getRawParameterValue ("sensitivity")->load();

    duckEnvelope_.setParameters (depthDb, anticipationMs, releaseMs, sensitivityDb);

    const int anticipationSamples = static_cast<int> (std::round (anticipationMs * 0.001 * sampleRate_));
    if (anticipationSamples != reportedLatencySamples_)
    {
        reportedLatencySamples_ = anticipationSamples;
        setLatencySamples (reportedLatencySamples_);
    }

    auto mainIn  = getBusBuffer (buffer, true, 0);
    auto sideIn  = getBusBuffer (buffer, true, 1);
    auto mainOut = getBusBuffer (buffer, false, 0);

    const int numSamples = buffer.getNumSamples();
    const float* sc = sideIn.getReadPointer (0);

    for (int n = 0; n < numSamples; ++n)
    {
        const float scFiltered  = sidechainFilter_.process (sc[n]);
        const float scLevelDb   = sidechainRms_.process (scFiltered);
        const float reductionDb = duckEnvelope_.process (scLevelDb);

        const float gain = bypassed ? 1.0f : juce::Decibels::decibelsToGain (-reductionDb);
        gainSmoothed_.setTargetValue (gain);
        const float appliedGain = gainSmoothed_.getNextValue();

        for (int ch = 0; ch < mainOut.getNumChannels(); ++ch)
        {
            const float dry     = mainIn.getReadPointer (ch)[n];
            const float delayed = lookaheadDelay_[static_cast<size_t> (ch)].process (dry, anticipationSamples);
            mainOut.getWritePointer (ch)[n] = delayed * appliedGain;
        }

        currentReductionDb_.store (bypassed ? 0.0f : reductionDb, std::memory_order_relaxed);
    }
}

bool TankAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* TankAudioProcessor::createEditor()
{
    return new TankAudioProcessorEditor (*this);
}

void TankAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void TankAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TankAudioProcessor();
}
```

- [ ] **Step 3: Build and re-run all DSP unit tests (regression check)**

Run:
```bash
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```
Expected: build succeeds; all 4 existing tests (`BandpassFilter`, `RmsDetector`, `LookaheadDelay`, `DuckingEnvelope`) still pass — this task doesn't change any DSP class, only how `PluginProcessor` wires them together, so a regression here means a wiring mistake broke a header, not the DSP itself.

- [ ] **Step 4: Smoke-test the Standalone**

Run: `timeout 3 ./build/Tank_artefacts/Debug/Standalone/Tank || true`
Expected: window opens and the process exits cleanly at the timeout, with no crash — confirms `prepareToPlay`/`processBlock` don't segfault even though there's no GUI feedback yet to see the ducking (that's Task 8).

- [ ] **Step 5: Commit**

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "Wire BandpassFilter/RmsDetector/DuckingEnvelope/LookaheadDelay into PluginProcessor"
```

---

## Task 7: TankLookAndFeel + GR Meter Component

**Files:**
- Create: `Source/UI/TankLookAndFeel.h`
- Create: `Source/UI/TankLookAndFeel.cpp`
- Create: `Source/UI/TankGrMeter.h`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `struct TankCol` (color constants), `class TankLookAndFeel : public juce::LookAndFeel_V4`, `class TankGrMeter : public juce::Component` (constructor takes `std::function<float()> reductionDbSupplier, float maxDepthDb`). Consumed by `TankAudioProcessorEditor` in Task 8.

This task is UI-only with no unit tests (matches sibling convention — `HexLookAndFeel`/`VuMeter` aren't unit-tested either, since they're JUCE `Component`/`LookAndFeel` classes, not plain DSP). Verification is visual, done in Task 8 once the editor uses them.

- [ ] **Step 1: Create `Source/UI/TankLookAndFeel.h`**

```cpp
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Bronze/brown "MMORPG tank" palette, structured the same way as Hex's
// HexLookAndFeel (shared colour constants + LookAndFeel_V4 overrides), but
// re-themed away from Hex's neon purple per the approved design spec.
struct TankCol
{
    static juce::Colour bg()           { return juce::Colour (0xff1c1410); }
    static juce::Colour panel()        { return juce::Colour (0xff2e2218); }
    static juce::Colour panelBorder()  { return juce::Colour (0xff4a3a28); }
    static juce::Colour knobTop()      { return juce::Colour (0xff5a4530); }
    static juce::Colour knobBottom()   { return juce::Colour (0xff1c1410); }
    static juce::Colour knobRim()      { return juce::Colour (0xff6a5340); }
    static juce::Colour valueArc()     { return juce::Colour (0xffcc8833); }
    static juce::Colour valueArcGlow() { return juce::Colour (0xffffaa44); }
    static juce::Colour trackArc()     { return juce::Colour (0xff3a2c1a); }
    static juce::Colour textPrimary()  { return juce::Colour (0xffd8c8a8); }
    static juce::Colour textDim()      { return juce::Colour (0xff7a6a55); }
    static juce::Colour valueText()    { return juce::Colour (0xffd8c8a8); }
    static juce::Colour tbBg()         { return juce::Colour (0xff120c08); }
};

class TankLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TankLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawLabel (juce::Graphics&, juce::Label&) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;
};
```

- [ ] **Step 2: Create `Source/UI/TankLookAndFeel.cpp`**

```cpp
#include "TankLookAndFeel.h"

TankLookAndFeel::TankLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, TankCol::valueText());
    setColour (juce::Slider::textBoxBackgroundColourId, TankCol::tbBg());
    setColour (juce::Slider::textBoxOutlineColourId, TankCol::panelBorder());
    setColour (juce::Label::textColourId, TankCol::textPrimary());
    setColour (juce::ToggleButton::textColourId, TankCol::textPrimary());
}

void TankLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                        float sliderPosProportional,
                                        float rotaryStartAngle, float rotaryEndAngle,
                                        juce::Slider&)
{
    auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                           static_cast<float> (w), static_cast<float> (h)).reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                          rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (TankCol::trackArc());
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                          rotaryStartAngle, angle, true);
    g.setColour (TankCol::valueArc());
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobRadius = radius * 0.72f;
    juce::ColourGradient grad (TankCol::knobTop(), centre.x, centre.y - knobRadius,
                                TankCol::knobBottom(), centre.x, centre.y + knobRadius, false);
    g.setGradientFill (grad);
    g.fillEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);
    g.setColour (TankCol::knobRim());
    g.drawEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.5f);

    juce::Path pointer;
    const float pointerLength = knobRadius * 0.8f;
    const float pointerThickness = 2.5f;
    pointer.addRectangle (-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength * 0.6f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));
    g.setColour (TankCol::valueArcGlow());
    g.fillPath (pointer);
}

void TankLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.setColour (TankCol::textPrimary());
    g.setFont (label.getFont());
    g.drawFittedText (label.getText(), label.getLocalBounds(),
                       label.getJustificationType(), 1);
}

juce::Label* TankLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    l->setColour (juce::Label::textColourId, TankCol::valueText());
    l->setColour (juce::Label::backgroundColourId, TankCol::tbBg());
    l->setColour (juce::Label::outlineColourId, TankCol::panelBorder());
    return l;
}

void TankLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                        bool /*shouldDrawButtonAsHighlighted*/,
                                        bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (button.getToggleState() ? TankCol::valueArc() : TankCol::panel());
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (TankCol::panelBorder());
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    g.setColour (button.getToggleState() ? TankCol::bg() : TankCol::textPrimary());
    g.setFont (juce::Font (juce::FontOptions (14.0f)));
    g.drawFittedText (button.getButtonText(), bounds.toNearestInt(), juce::Justification::centred, 1);
}
```

- [ ] **Step 3: Create `Source/UI/TankGrMeter.h`**

```cpp
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <utility>
#include <cmath>
#include "TankLookAndFeel.h"

// Horizontal gain-reduction meter. Polls the processor's current reduction
// (0..maxDepthDb) on a 30Hz timer. Unlike a level VU meter, 0 = empty and
// full depth = fully filled, matching how GR meters are read in DAWs.
// No allocation in paint()/timerCallback(), same convention as Hex's VuMeter.
class TankGrMeter : public juce::Component, private juce::Timer
{
public:
    TankGrMeter (std::function<float()> reductionDbSupplier, float maxDepthDb)
        : reductionDbSupplier_ (std::move (reductionDbSupplier)), maxDepthDb_ (maxDepthDb)
    {
        startTimerHz (30);
    }

    ~TankGrMeter() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (TankCol::tbBg());
        g.fillRoundedRectangle (bounds, 2.0f);
        g.setColour (TankCol::panelBorder());
        g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, 1.0f);

        auto inner = bounds.reduced (2.0f);
        const float w = inner.getWidth();
        const float fillW = juce::jlimit (0.0f, w, displayLevel_ * w);

        if (fillW > 0.0f)
        {
            auto fillRect = inner.removeFromLeft (fillW);
            juce::ColourGradient grad (TankCol::valueArcGlow(), fillRect.getX(), fillRect.getY(),
                                       TankCol::valueArc(), fillRect.getRight(), fillRect.getY(), false);
            g.setGradientFill (grad);
            g.fillRect (fillRect);
        }
    }

private:
    void timerCallback() override
    {
        const float reductionDb = reductionDbSupplier_ ? reductionDbSupplier_() : 0.0f;
        const float level = juce::jlimit (0.0f, 1.0f, reductionDb / juce::jmax (maxDepthDb_, 0.001f));

        if (level > displayLevel_)
            displayLevel_ += (level - displayLevel_) * kAttackCoeff;
        else
            displayLevel_ += (level - displayLevel_) * kReleaseCoeff;

        if (std::abs (displayLevel_ - lastPaintedLevel_) > kRepaintThreshold)
        {
            lastPaintedLevel_ = displayLevel_;
            repaint();
        }
    }

    static constexpr float kAttackCoeff      = 0.6f;
    static constexpr float kReleaseCoeff     = 0.08f;
    static constexpr float kRepaintThreshold = 0.002f;

    std::function<float()> reductionDbSupplier_;
    float maxDepthDb_;
    float displayLevel_ { 0.0f };
    float lastPaintedLevel_ { -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TankGrMeter)
};
```

- [ ] **Step 4: Add new files to `target_sources` (Modify `CMakeLists.txt`)**

```cmake
target_sources(Tank PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/BandpassFilter.h
    Source/DSP/RmsDetector.h
    Source/DSP/LookaheadDelay.h
    Source/DSP/DuckingEnvelope.h
    Source/UI/TankLookAndFeel.h
    Source/UI/TankLookAndFeel.cpp
    Source/UI/TankGrMeter.h
)
```

- [ ] **Step 5: Build**

Run: `cmake --build build --parallel`
Expected: succeeds (these classes aren't used by the editor yet until Task 8, but must compile standalone).

- [ ] **Step 6: Commit**

```bash
git add Source/UI/TankLookAndFeel.h Source/UI/TankLookAndFeel.cpp Source/UI/TankGrMeter.h CMakeLists.txt
git commit -m "Add TankLookAndFeel (bronze theme) and TankGrMeter component"
```

---

## Task 8: PluginEditor — Knobs, Bypass, GR Meter

**Files:**
- Modify: `Source/PluginEditor.h` (replace entire file)
- Modify: `Source/PluginEditor.cpp` (replace entire file)

**Interfaces:**
- Consumes: `TankAudioProcessor::apvts` and `TankAudioProcessor::getCurrentReductionDb()` (Task 6), `TankLookAndFeel`/`TankCol`/`TankGrMeter` (Task 7).

- [ ] **Step 1: Replace `Source/PluginEditor.h`**

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "PluginProcessor.h"
#include "UI/TankLookAndFeel.h"
#include "UI/TankGrMeter.h"

class TankAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit TankAudioProcessorEditor (TankAudioProcessor&);
    ~TankAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    Knob& addKnob (const juce::String& paramId, const juce::String& labelText);

    TankAudioProcessor& processor_;
    TankLookAndFeel lookAndFeel_;

    std::array<Knob, 4> knobs_ { Knob{}, Knob{}, Knob{}, Knob{} };

    juce::ToggleButton bypassButton_ { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment_;

    TankGrMeter grMeter_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TankAudioProcessorEditor)
};
```

- [ ] **Step 2: Replace `Source/PluginEditor.cpp`**

```cpp
#include "PluginEditor.h"

TankAudioProcessorEditor::TankAudioProcessorEditor (TankAudioProcessor& p)
    : AudioProcessorEditor (&p), processor_ (p),
      grMeter_ ([&p] { return p.getCurrentReductionDb(); }, TankAudioProcessor::kMaxDepthDb)
{
    setLookAndFeel (&lookAndFeel_);

    static const std::array<std::pair<const char*, const char*>, 4> kKnobDefs { {
        { "depth",        "Depth" },
        { "anticipation", "Anticipation" },
        { "release",      "Release" },
        { "sensitivity",  "Sensitivity" },
    } };

    for (size_t i = 0; i < knobs_.size(); ++i)
    {
        auto& knob = knobs_[i];
        knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
        addAndMakeVisible (knob.slider);

        knob.label.setText (kKnobDefs[i].second, juce::dontSendNotification);
        knob.label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (knob.label);

        knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor_.apvts, kKnobDefs[i].first, knob.slider);
    }

    addAndMakeVisible (bypassButton_);
    bypassAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor_.apvts, "bypass", bypassButton_);

    addAndMakeVisible (grMeter_);

    setSize (500, 220);
}

TankAudioProcessorEditor::~TankAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void TankAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (TankCol::bg());
}

void TankAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto topRow = area.removeFromTop (30);
    bypassButton_.setBounds (topRow.removeFromLeft (100));

    area.removeFromTop (8);
    auto meterRow = area.removeFromTop (24);
    grMeter_.setBounds (meterRow);

    area.removeFromTop (16);

    const int knobWidth = area.getWidth() / static_cast<int> (knobs_.size());
    for (auto& knob : knobs_)
    {
        auto col = area.removeFromLeft (knobWidth);
        knob.label.setBounds (col.removeFromTop (20));
        knob.slider.setBounds (col.reduced (8));
    }
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build --parallel`
Expected: succeeds — no more `target_sources` changes needed here since `PluginEditor.cpp` was already listed in Task 1.

- [ ] **Step 4: Manual GUI verification**

Run: `./build/Tank_artefacts/Debug/Standalone/Tank`
Expected: a window opens showing 4 knobs (Depth, Anticipation, Release, Sensitivity), a Bypass toggle, and a horizontal GR meter, all in the bronze/amber theme. Turning each knob updates its value label. Toggling Bypass changes color. This is manual — no automated GUI test exists for JUCE editors in any sibling plugin, matching established convention.

- [ ] **Step 5: Commit**

```bash
git add Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "Add Tank editor: 4 knobs, bypass toggle, GR meter"
```

---

## Task 9: Packaging (install/uninstall scripts, release tarball)

**Files:**
- Create: `scripts/install.sh`
- Create: `scripts/uninstall.sh`

**Interfaces:** None — this task only affects packaging, not runtime code.

- [ ] **Step 1: Create `scripts/install.sh`**

```bash
#!/usr/bin/env bash
# Install Tank plugins and standalone app.
# Usage:
#   ./install.sh           — install to user directories (no root needed)
#   ./install.sh --system  — install system-wide to /usr/lib (requires sudo)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SYSTEM=0

for arg in "$@"; do
    case "$arg" in
        --system) SYSTEM=1 ;;
        *) echo "Unknown argument: $arg" >&2; exit 1 ;;
    esac
done

if [[ $SYSTEM -eq 1 ]]; then
    VST3_DIR="/usr/lib/vst3"
    CLAP_DIR="/usr/lib/clap"
    BIN_DIR="/usr/local/bin"
else
    VST3_DIR="${HOME}/.vst3"
    CLAP_DIR="${HOME}/.clap"
    BIN_DIR="${HOME}/.local/bin"
fi

echo "Installing Tank..."

mkdir -p "${VST3_DIR}"
rm -rf   "${VST3_DIR}/Tank.vst3"
cp -r    "${SCRIPT_DIR}/VST3/Tank.vst3" "${VST3_DIR}/"
echo "  VST3  → ${VST3_DIR}/Tank.vst3"

mkdir -p "${CLAP_DIR}"
cp       "${SCRIPT_DIR}/CLAP/Tank.clap" "${CLAP_DIR}/"
chmod    755 "${CLAP_DIR}/Tank.clap"
echo "  CLAP  → ${CLAP_DIR}/Tank.clap"

mkdir -p "${BIN_DIR}"
cp       "${SCRIPT_DIR}/bin/Tank" "${BIN_DIR}/"
chmod    755 "${BIN_DIR}/Tank"
echo "  App   → ${BIN_DIR}/Tank"

echo "Done."
```

- [ ] **Step 2: Create `scripts/uninstall.sh`**

```bash
#!/usr/bin/env bash
# Remove Tank from all known install locations.
set -euo pipefail

removed=0

remove() {
    local path="$1"
    if [[ -e "$path" ]]; then
        rm -rf "$path"
        echo "  Removed: $path"
        removed=1
    fi
}

echo "Uninstalling Tank..."

remove "${HOME}/.vst3/Tank.vst3"
remove "${HOME}/.clap/Tank.clap"
remove "${HOME}/.local/bin/Tank"

if [[ $EUID -eq 0 ]]; then
    remove "/usr/lib/vst3/Tank.vst3"
    remove "/usr/lib/clap/Tank.clap"
    remove "/usr/local/bin/Tank"
else
    for path in "/usr/lib/vst3/Tank.vst3" \
                "/usr/lib/clap/Tank.clap" \
                "/usr/local/bin/Tank"; do
        if [[ -e "$path" ]]; then
            echo "  Skipping $path (re-run with sudo to remove)"
        fi
    done
fi

if [[ $removed -eq 0 ]]; then
    echo "  Nothing to remove."
else
    echo "Done."
fi
```

- [ ] **Step 3: Make scripts executable**

Run: `chmod +x scripts/install.sh scripts/uninstall.sh`

- [ ] **Step 4: Build the release tarball**

Run:
```bash
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
cd build-release && cpack && cd ..
```
Expected: `build-release/Tank-0.1.0-linux-x86_64.tar.gz` and a matching `.sha256` are produced.

- [ ] **Step 5: Verify tarball contents**

Run: `tar -tzf build-release/Tank-0.1.0-linux-x86_64.tar.gz`
Expected: listing includes `install.sh`, `uninstall.sh`, `bin/Tank`, `CLAP/Tank.clap`, `VST3/Tank.vst3/` (directory).

- [ ] **Step 6: Commit**

```bash
git add scripts/install.sh scripts/uninstall.sh
git commit -m "Add install/uninstall scripts and verify release packaging"
```

---

## Final Manual Verification (end-to-end, after all tasks)

Automated coverage stops at the DSP-class level (Tasks 2–5) and build-success checks (Tasks 1, 6–9), matching every sibling plugin's convention — none of them have an automated audio-thread integration test for `PluginProcessor` either. Before considering Tank done, manually verify the actual ducking behavior:

1. Load `Tank.vst3` in a DAW (or use the Standalone with a multi-channel audio interface) on a bass track, with a kick track routed into Tank's Sidechain input.
2. Play the kick against sustained bass and confirm: the bass visibly/audibly ducks in time with the kick, the GR meter moves, and increasing `depth` deepens the duck.
3. Toggle `bypass` while playing — confirm no click or timing jump (this is the scenario the design spec's Bypass Interaction section exists to prevent).
4. Change `anticipation` while playing — confirm no crash and that the host's plugin-delay-compensation updates (may cause a brief host-side resync, which is expected).
