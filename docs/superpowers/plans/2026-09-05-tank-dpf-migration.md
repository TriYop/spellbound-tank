# Tank DPF Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate Tank (a sidechain "pumping" compressor, currently JUCE VST3/CLAP/Standalone) off JUCE onto DPF (ISC-licensed), reusing the exact pattern Hex and Pugilist already proved, and add a factory-presets panel it never had.

**Architecture:** Keep Tank's framework-free DSP (`BandpassFilter`, `RmsDetector`, `DuckingEnvelope`, `LookaheadDelay`) unchanged in place; replace the JUCE `AudioProcessor`/`AudioProcessorEditor` pair with a thin DPF `Plugin` adapter (`TankPluginAdapter`) and a DPF/NanoVG `UI` (`TankUI`) that drives `AudioPlugins/Common`'s DGL widgets (`RotaryKnob`, `ToggleSwitch`, `VuMeter` in its new `Horizontal` orientation, `PresetSelector`/`Button`). The JUCE-era `PluginProcessor`/`PluginEditor`/`TankLookAndFeel`/`TankGrMeter` move unchanged to `Source/_juce_reference/` as the porting reference. Tank's 3-bus shape (stereo main + mono sidechain) becomes DPF's `DISTRHO_PLUGIN_NUM_INPUTS 3` with an `initAudioPort` override setting `kAudioPortIsSidechain` on index 2 — a supported, precedented DPF pattern, not something to invent.

**Tech Stack:** C++20, CMake + Ninja, DPF (DISTRHO Plugin Framework, pinned commit, ISC), `AudioPlugins/Common` v0.3.0 (`AudioPluginsCommon::io`/`hui_dgl`/`presets`), pugixml (via Common), CTest for framework-free unit tests.

**Spec:** `/home/yvan/Projects/AudioPlugins/Common/docs/superpowers/specs/2026-09-05-phase2-outflank-tank-dpf-migration-design.md` (see its "Tank migration" section and the "Tank" / "DPF sidechain support, confirmed real" bullets under "Context").

## Global Constraints

- **Hard blocker, verify before Task 1 proceeds:** `AudioPlugins/Common` must be tagged `v0.3.0` (or later) on `https://github.com/TriYop/spellbound-common.git` before `CMakeLists.txt` can pin it. As of 2026-09-05, `git ls-remote --tags https://github.com/TriYop/spellbound-common.git` shows only `v0.2.0`/`v0.2.1`/`v0.2.2` — **`v0.3.0` does not exist yet**. This is a prerequisite from a separate, parallel plan (Common's `VuMeter` `Horizontal` orientation work). If it is still missing when this plan starts execution, stop and resolve that first — do not substitute a workaround (e.g. pinning a commit SHA instead of the tag, or reimplementing horizontal fill locally).
- DPF has no tagged releases; pin the exact commit SHA `4238e1c7f0351bbe488d79f0899c540543ac7583` (same commit Hex/Pugilist use), applying both `dpf-clap-state-chunked-read.patch` and `dpf-clap-activate-latency.patch` via `cmake/apply_patch.cmake` — copied verbatim from Hex's proven `worktree-dpf-stage0` branch (pushed to `origin/worktree-dpf-stage0` on `git@github.com:TriYop/spellbound-hex.git`), not retyped from scratch.
- `CMAKE_POSITION_INDEPENDENT_CODE ON` must be set *before* `FetchContent_MakeAvailable(AudioPluginsCommon)`, and the `AudioPluginsCommon` `FetchContent_Declare`/`MakeAvailable` pair must come *after* `dpf_add_plugin(Tank ...)` (DPF's DGL target is created lazily inside that call) — both ordering constraints are load-bearing, carried over from Hex's `CMakeLists.txt`.
- Windows/macOS CI legs stay `continue-on-error: true`; only the Linux leg gates the workflow (2026-08-31 roadmap scope change — Win/Mac/`.deb` are Phase 6, not an exit criterion here).
- `AudioPluginsCommon::hui_dgl` and `AudioPluginsCommon::presets` link only into the `Tank-ui` static lib DPF creates (never into `Tank`/`Tank-dsp` or the LV2 binary). `AudioPluginsCommon::io` (for `ParameterSmoother`/`MeterTransport`) links into `Tank` itself, since the DSP adapter needs it too.
- "Ticket per task" workspace convention: file one GitHub issue per validator finding or numerical-stability bug discovered in Task 8/Task 10, and reference the issue number in the commit that fixes it (or leave it open and say so if deferred).
- Standing workspace expectation (clean code, hexagonal/DDD, TDD) applies throughout: DSP stays framework-free and unit-tested; DPF/DGL adapter code is a thin shim with no business logic of its own.
- No copyleft license contamination: DPF is ISC-licensed; do not add any GPL/LGPL dependency.

---

## Task 1: Verify prerequisites and create an isolated worktree

**Files:** none (workspace/tooling setup only).

**Interfaces:** N/A — this task only establishes the workspace later tasks run in.

- [ ] **Step 1: Verify the `Common` v0.3.0 blocker is resolved**

```bash
git ls-remote --tags https://github.com/TriYop/spellbound-common.git
```

Expected: a `refs/tags/v0.3.0` line (or a higher version) present. If it is **not** present, stop here — this is a hard blocker per Global Constraints, not something to route around (do not pin a commit SHA instead, do not reimplement `Horizontal` orientation locally). Report the blocker and wait for `Common`'s v0.3.0 release before continuing.

- [ ] **Step 2: Confirm Tank's current git state is clean**

```bash
cd /home/yvan/Projects/AudioPlugins/Tank
git status --short
git remote -v
```

Expected: `origin` is `git@github.com:TriYop/spellbound-tank.git`, branch `main`. If there are uncommitted changes unrelated to this task (e.g. stray image files), leave them — do not clean the working tree as part of this task.

- [ ] **Step 3: Create the isolated worktree via the using-git-worktrees skill**

Invoke `superpowers:using-git-worktrees` (Step 0 detection, then Step 1a/1b) with branch name `worktree-dpf-stage0`, matching Hex's and Pugilist's precedent naming. If a native worktree tool is available, use it; otherwise fall back to:

```bash
git worktree add .worktrees/worktree-dpf-stage0 -b worktree-dpf-stage0
cd .worktrees/worktree-dpf-stage0
```

- [ ] **Step 4: Verify a clean baseline in the new worktree**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Expected: the existing JUCE build configures/builds/tests cleanly (4 DSP tests: BandpassFilter, RmsDetector, LookaheadDelay, DuckingEnvelope, all passing) before any migration changes are made. If this fails, stop and investigate — a dirty baseline makes every later failure ambiguous.

- [ ] **Step 5: Report readiness**

State the worktree's full path, confirm `v0.3.0` exists, and confirm the 4/4 baseline tests pass, before starting Task 2.

---

## Task 2: DPF skeleton — CMake rewrite, plugin metadata, JUCE-reference move, stub adapter/UI, build-only CI

**Files:**
- Create: `cmake/apply_patch.cmake`
- Create: `cmake/patches/dpf-clap-state-chunked-read.patch`
- Create: `cmake/patches/dpf-clap-activate-latency.patch`
- Create: `Source/DistrhoPluginInfo.h`
- Create: `Source/TankPluginAdapter.h` (stub)
- Create: `Source/TankPluginAdapter.cpp` (stub)
- Create: `Source/TankUI.h` (stub)
- Create: `Source/TankUI.cpp` (stub)
- Move: `Source/PluginProcessor.h` → `Source/_juce_reference/PluginProcessor.h`
- Move: `Source/PluginProcessor.cpp` → `Source/_juce_reference/PluginProcessor.cpp`
- Move: `Source/PluginEditor.h` → `Source/_juce_reference/PluginEditor.h`
- Move: `Source/PluginEditor.cpp` → `Source/_juce_reference/PluginEditor.cpp`
- Move: `Source/UI/TankLookAndFeel.h` → `Source/_juce_reference/UI/TankLookAndFeel.h`
- Move: `Source/UI/TankLookAndFeel.cpp` → `Source/_juce_reference/UI/TankLookAndFeel.cpp`
- Move: `Source/UI/TankGrMeter.h` → `Source/_juce_reference/UI/TankGrMeter.h`
- Modify: `CMakeLists.txt` (full rewrite)
- Modify: `scripts/install.sh`, `scripts/uninstall.sh`
- Create: `.github/workflows/ci.yml`

**Interfaces:**
- Produces: `TankPluginAdapter` (stub `Plugin` subclass, `Source/TankPluginAdapter.h`/`.cpp`) and `TankUI` (stub `UI` subclass, `Source/TankUI.h`/`.cpp`) — Task 4 fills in `TankPluginAdapter`'s DSP wiring, Task 6 fills in `TankUI`'s widgets.
- Produces: `Source/DistrhoPluginInfo.h`'s `TankParameters` enum (`kParameterBypass=0, kParameterDepth, kParameterAnticipation, kParameterRelease, kParameterSensitivity, kParameterCount`) and the `TANK_PARAM_*_MIN/MAX/DEFAULT` macros — every later task's parameter code uses these names verbatim.

- [ ] **Step 1: Copy the DPF patch files and apply_patch.cmake from Hex's proven branch**

Hex's DPF migration lives on `worktree-dpf-stage0`, pushed to `origin/worktree-dpf-stage0` on `git@github.com:TriYop/spellbound-hex.git` (not yet merged to Hex's `main`). Fetch it and copy the three files verbatim — they are DPF-bug-fix patches, not Tank-specific, so they must not be edited:

```bash
cd /home/yvan/Projects/AudioPlugins/Hex
git fetch origin worktree-dpf-stage0

cd /home/yvan/Projects/AudioPlugins/Tank/.worktrees/worktree-dpf-stage0   # your Task 1 worktree path
mkdir -p cmake/patches
git -C /home/yvan/Projects/AudioPlugins/Hex show origin/worktree-dpf-stage0:cmake/apply_patch.cmake > cmake/apply_patch.cmake
git -C /home/yvan/Projects/AudioPlugins/Hex show origin/worktree-dpf-stage0:cmake/patches/dpf-clap-state-chunked-read.patch > cmake/patches/dpf-clap-state-chunked-read.patch
git -C /home/yvan/Projects/AudioPlugins/Hex show origin/worktree-dpf-stage0:cmake/patches/dpf-clap-activate-latency.patch > cmake/patches/dpf-clap-activate-latency.patch
```

Verify the three files are non-empty and each patch's `diff --git` header still targets `distrho/src/DistrhoPluginCLAP.cpp`.

- [ ] **Step 2: Move the JUCE-era source to `Source/_juce_reference/`**

```bash
mkdir -p Source/_juce_reference/UI
git mv Source/PluginProcessor.h   Source/_juce_reference/PluginProcessor.h
git mv Source/PluginProcessor.cpp Source/_juce_reference/PluginProcessor.cpp
git mv Source/PluginEditor.h      Source/_juce_reference/PluginEditor.h
git mv Source/PluginEditor.cpp    Source/_juce_reference/PluginEditor.cpp
git mv Source/UI/TankLookAndFeel.h   Source/_juce_reference/UI/TankLookAndFeel.h
git mv Source/UI/TankLookAndFeel.cpp Source/_juce_reference/UI/TankLookAndFeel.cpp
git mv Source/UI/TankGrMeter.h       Source/_juce_reference/UI/TankGrMeter.h
rmdir Source/UI 2>/dev/null || true
```

`Source/DSP/*.h` (`BandpassFilter.h`, `RmsDetector.h`, `LookaheadDelay.h`, `DuckingEnvelope.h`) stay exactly where they are — they are already framework-free and are reused unchanged by `TankPluginAdapter`.

- [ ] **Step 3: Write `Source/DistrhoPluginInfo.h`**

```cpp
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
```

- [ ] **Step 4: Write stub `Source/TankPluginAdapter.h`/`.cpp`**

```cpp
// Source/TankPluginAdapter.h
#pragma once

#include "DistrhoPlugin.hpp"

START_NAMESPACE_DISTRHO

class TankPluginAdapter : public Plugin
{
public:
    TankPluginAdapter() : Plugin(kParameterCount, 0, 0) {}

protected:
    const char* getLabel() const override { return "Tank"; }
    const char* getDescription() const override { return "Sidechain pumping compressor"; }
    const char* getMaker() const override { return "Spellbound"; }
    const char* getLicense() const override { return "https://spellbound.audio/plugins/tank#license"; }
    uint32_t getVersion() const override { return d_version(0, 1, 0); }

    void initParameter(uint32_t, Parameter&) override {}
    float getParameterValue(uint32_t) const override { return 0.0f; }
    void setParameterValue(uint32_t, float) override {}

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        // Stub: pass main L/R through untouched. Task 4 replaces this with
        // the real sidechain-driven ducking chain.
        if (outputs[0] != inputs[0])
            std::memcpy(outputs[0], inputs[0], sizeof(float) * frames);
        if (outputs[1] != inputs[1])
            std::memcpy(outputs[1], inputs[1], sizeof(float) * frames);
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TankPluginAdapter)
};

END_NAMESPACE_DISTRHO
```

```cpp
// Source/TankPluginAdapter.cpp
#include "TankPluginAdapter.h"
#include <cstring>

START_NAMESPACE_DISTRHO

Plugin* createPlugin()
{
    return new TankPluginAdapter();
}

END_NAMESPACE_DISTRHO
```

- [ ] **Step 5: Write stub `Source/TankUI.h`/`.cpp`**

```cpp
// Source/TankUI.h
#pragma once

#include "DistrhoUI.hpp"

START_NAMESPACE_DISTRHO

class TankUI : public UI
{
public:
    TankUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT) {}

protected:
    void parameterChanged(uint32_t, float) override {}

    void onNanoDisplay() override
    {
        beginPath();
        rect(0.0f, 0.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()));
        fillColor(DGL_NAMESPACE::Color(28, 20, 16, 255)); // TankCol::bg() 0xff1c1410, see Task 6
        fill();
        closePath();
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TankUI)
};

END_NAMESPACE_DISTRHO
```

```cpp
// Source/TankUI.cpp
#include "TankUI.h"

START_NAMESPACE_DISTRHO

UI* createUI()
{
    return new TankUI();
}

END_NAMESPACE_DISTRHO
```

- [ ] **Step 6: Rewrite `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.22)
project(Tank VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# DPF has no tagged releases -- pin an exact commit SHA on `main` instead.
# Same commit Hex/Pugilist use (see cmake/patches/*.patch for why each patch
# is needed; neither has been reported upstream yet).
set(AUDIOPLUGINS_DPF_GIT_TAG "4238e1c7f0351bbe488d79f0899c540543ac7583" CACHE STRING "Pinned DPF commit")

# CAVEAT (CMake FetchContent): the patch step only runs on a *fresh*
# population of the source dir. If build/_deps/dpf-src already exists from
# an earlier configure, a plain reconfigure will NOT re-run it -- delete
# build/ (or at least build/_deps/dpf-*) when you need to be sure the
# patches actually took effect. cmake/apply_patch.cmake is idempotent (it
# skips a patch that's already applied), so a re-run is harmless, but that
# is not a substitute for a clean re-fetch.
FetchContent_Declare(dpf
    GIT_REPOSITORY https://github.com/DISTRHO/DPF.git
    GIT_TAG        ${AUDIOPLUGINS_DPF_GIT_TAG}
    GIT_SHALLOW    TRUE
    PATCH_COMMAND  ${CMAKE_COMMAND}
                   -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/dpf-clap-state-chunked-read.patch
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_patch.cmake
            COMMAND ${CMAKE_COMMAND}
                   -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/dpf-clap-activate-latency.patch
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_patch.cmake
)
FetchContent_MakeAvailable(dpf)

set(TANK_DPF_TARGETS vst3 clap lv2)
if(APPLE)
    list(APPEND TANK_DPF_TARGETS au)
endif()

dpf_add_plugin(Tank
    TARGETS ${TANK_DPF_TARGETS}
    UI_TYPE opengl
    USE_FILE_BROWSER TRUE
    FILES_DSP
        Source/TankPluginAdapter.cpp
    FILES_UI
        Source/TankUI.cpp
)

target_include_directories(Tank PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")

# Same DPF WebViewImpl.cpp / GNU `typeof` workaround Hex needs (see Hex's
# CMakeLists.txt for the full explanation) -- unconditionally compiled in by
# a DPF bug regardless of USE_WEB_VIEW, fails under strict ISO C++.
if(TARGET dgl-opengl)
    set_target_properties(dgl-opengl PROPERTIES CXX_EXTENSIONS ON)
endif()

# ── Shared HUI/DSP library ───────────────────────────────────────────────────
# ORDERING (load-bearing): must come after dpf_add_plugin(Tank ...), which is
# what lazily creates DPF's dgl-opengl target that AudioPluginsCommon::hui_dgl
# is guarded on. See Hex's CMakeLists.txt for the full explanation.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

FetchContent_Declare(AudioPluginsCommon
    GIT_REPOSITORY https://github.com/TriYop/spellbound-common.git
    GIT_TAG        v0.3.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(AudioPluginsCommon)

# ParameterSmoother (gain smoothing) and MeterTransport (audio-thread ->
# UI-thread GR value, see TankPluginAdapter.h) both need a real link, not
# just an include path.
target_link_libraries(Tank PUBLIC AudioPluginsCommon::io)

# Linked into `Tank-ui` (not `Tank`): dpf_add_plugin() splits the plugin into
# a common `Tank` static lib plus `Tank-dsp`/`Tank-ui`; TankUI.cpp is the
# only consumer of the widgets and lives in `Tank-ui`. Same reasoning as Hex.
target_link_libraries(Tank-ui PRIVATE AudioPluginsCommon::hui_dgl AudioPluginsCommon::presets)

# ── Unit tests (no JUCE/DPF dependency) ──────────────────────────────────────
enable_testing()

add_executable(test_bandpassfilter Tests/test_bandpassfilter.cpp)
target_include_directories(test_bandpassfilter PRIVATE Source/ Tests/)
target_compile_features(test_bandpassfilter PRIVATE cxx_std_20)
add_test(NAME BandpassFilter COMMAND test_bandpassfilter)

add_executable(test_rmsdetector Tests/test_rmsdetector.cpp)
target_include_directories(test_rmsdetector PRIVATE Source/ Tests/)
target_compile_features(test_rmsdetector PRIVATE cxx_std_20)
add_test(NAME RmsDetector COMMAND test_rmsdetector)

add_executable(test_lookaheaddelay Tests/test_lookaheaddelay.cpp)
target_include_directories(test_lookaheaddelay PRIVATE Source/ Tests/)
target_compile_features(test_lookaheaddelay PRIVATE cxx_std_20)
add_test(NAME LookaheadDelay COMMAND test_lookaheaddelay)

add_executable(test_duckingenvelope Tests/test_duckingenvelope.cpp)
target_include_directories(test_duckingenvelope PRIVATE Source/ Tests/)
target_compile_features(test_duckingenvelope PRIVATE cxx_std_20)
add_test(NAME DuckingEnvelope COMMAND test_duckingenvelope)

# Task 3 (SidechainGuard) and Task 7 (FactoryPresets) each append their own
# add_executable/add_test block here once they create their test files --
# deliberately not included yet, so this task's own build-verification step
# (Step 9 below) has a CMakeLists.txt that only references files that exist
# at this point in the plan.

# ── Install rules ─────────────────────────────────────────────────────────────
# DPF writes all format outputs under a flat bin/ dir -- no Standalone
# target: Tank is an effect, and this workspace only requires Standalone for
# instruments (see AudioPlugins/CLAUDE.md).
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(_BIN "${CMAKE_BINARY_DIR}/bin")

install(DIRECTORY  "${_BIN}/Tank.vst3"
        DESTINATION VST3
        COMPONENT   Runtime
        USE_SOURCE_PERMISSIONS)

# CLAP is a flat shared-library file on Linux/Windows but a bundle
# *directory* on macOS (like VST3/AU) -- see Hex's CMakeLists.txt.
if(APPLE)
    install(DIRECTORY  "${_BIN}/Tank.clap"
            DESTINATION CLAP
            COMPONENT   Runtime
            USE_SOURCE_PERMISSIONS)
else()
    install(FILES      "${_BIN}/Tank.clap"
            DESTINATION CLAP
            COMPONENT   Runtime
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                        GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()

install(DIRECTORY  "${_BIN}/Tank.lv2"
        DESTINATION LV2
        COMPONENT   Runtime
        USE_SOURCE_PERMISSIONS)

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

- [ ] **Step 7: Update `scripts/install.sh` and `scripts/uninstall.sh` for the DPF layout (no Standalone, add LV2)**

```bash
#!/usr/bin/env bash
# Install Tank plugins (VST3/CLAP/LV2). Tank is an effect -- no standalone app.
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
    LV2_DIR="/usr/lib/lv2"
else
    VST3_DIR="${HOME}/.vst3"
    CLAP_DIR="${HOME}/.clap"
    LV2_DIR="${HOME}/.lv2"
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

mkdir -p "${LV2_DIR}"
rm -rf   "${LV2_DIR}/Tank.lv2"
cp -r    "${SCRIPT_DIR}/LV2/Tank.lv2" "${LV2_DIR}/"
echo "  LV2   → ${LV2_DIR}/Tank.lv2"

echo "Done."
```

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
remove "${HOME}/.lv2/Tank.lv2"

if [[ $EUID -eq 0 ]]; then
    remove "/usr/lib/vst3/Tank.vst3"
    remove "/usr/lib/clap/Tank.clap"
    remove "/usr/lib/lv2/Tank.lv2"
else
    for path in "/usr/lib/vst3/Tank.vst3" \
                "/usr/lib/clap/Tank.clap" \
                "/usr/lib/lv2/Tank.lv2"; do
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

- [ ] **Step 8: Write build-only CI at `.github/workflows/ci.yml`**

```yaml
name: CI

on:
  push:
    branches: [ "main", "master" ]
    tags: [ "v*" ]
  pull_request:
    branches: [ "main", "master" ]

jobs:
  build-and-test:
    # Windows/macOS are unverified for the same reasons documented in Hex's
    # ci.yml: nobody has run a DPF/DGL build there yet, and the dgl-opengl
    # CXX_EXTENSIONS ON workaround above is a documented no-op on MSVC.
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
        build_type: [Release]

    runs-on: ${{ matrix.os }}
    continue-on-error: ${{ matrix.os != 'ubuntu-latest' }}

    steps:
      - uses: actions/checkout@v4

      - name: Install Linux build dependencies
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            libasound2-dev libjack-jackd2-dev \
            libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
            libxinerama-dev libxrandr-dev libxrender-dev \
            libfreetype-dev libfontconfig1-dev \
            libglu1-mesa-dev libwebkit2gtk-4.1-dev \
            xvfb

      - name: Configure Common repo access
        # github.com/TriYop/spellbound-common (FetchContent'd below) is
        # private -- same COMMON_REPO_TOKEN fine-grained PAT secret Hex/
        # Outflank already use. This secret must exist in the
        # spellbound-tank repo's own Settings > Secrets and variables >
        # Actions before this step can succeed -- that's a manual,
        # non-scriptable step; verify it's present before relying on green CI.
        run: git config --global url."https://x-access-token:${{ secrets.COMMON_REPO_TOKEN }}@github.com/TriYop/spellbound-common".insteadOf "https://github.com/TriYop/spellbound-common"

      - name: Configure
        run: cmake -B build -DCMAKE_BUILD_TYPE=${{ matrix.build_type }}

      - name: Build
        run: cmake --build build --config ${{ matrix.build_type }} --parallel

      - name: Test
        working-directory: build
        run: ctest --build-config ${{ matrix.build_type }} --output-on-failure
```

Do **not** add the pluginval/clap-validator/lv2lint steps yet — Task 8 adds those once there is real DSP/UI to validate.

- [ ] **Step 9: Configure and build to verify the skeleton compiles**

```bash
rm -rf build   # force a fresh FetchContent population so the patches apply
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel
```

Expected: configures (fetching DPF + the pinned Common tag) and builds `Tank.vst3`/`Tank.clap`/`Tank.lv2` under `build/bin/`, plus the 4 pre-existing DSP test executables. The stub `TankPluginAdapter`/`TankUI` compile and link.

- [ ] **Step 10: Run the existing DSP tests to confirm nothing broke in the move**

```bash
ctest --test-dir build --output-on-failure
```

Expected: 4/4 pass (BandpassFilter, RmsDetector, LookaheadDelay, DuckingEnvelope) — unchanged from Task 1's baseline.

- [ ] **Step 11: Commit**

```bash
git add cmake/ Source/ scripts/ CMakeLists.txt .github/workflows/ci.yml
git commit -m "Migrate Tank's build off JUCE onto DPF (skeleton)

Rewrite CMakeLists.txt to fetch DPF (pinned commit + two upstream-bug
patches) and AudioPlugins/Common v0.3.0 instead of JUCE + clap-juce-
extensions. Move the JUCE-era PluginProcessor/PluginEditor/
TankLookAndFeel/TankGrMeter to Source/_juce_reference/ as the porting
reference; stub TankPluginAdapter/TankUI as pass-through placeholders.
Build-only CI (Linux gating, Windows/macOS continue-on-error), matching
Hex/Pugilist's proven pattern."
```

---

## Task 3: Sidechain-silence guard (framework-free, TDD)

Ports the `hasSidechain`/null-pointer guard from `Source/_juce_reference/PluginProcessor.cpp:115-116` as a tiny, independently unit-tested helper — the only piece of DPF-adjacent logic in this migration that gets a real unit test, per this workspace's convention that DSP/domain-level unit tests are for framework-free classes, not adapter glue. DPF's own contract already documents that an audio-port channel pointer "might be null" (`distrho/DistrhoPlugin.hpp`'s `run()` doc comment) — this guard is what makes Tank robust to that, exactly like the JUCE Standalone case the original comment described.

**Files:**
- Create: `Source/DSP/SidechainGuard.h`
- Create: `Tests/test_sidechainguard.cpp`
- Modify: `CMakeLists.txt` (append the `test_sidechainguard` `add_executable`/`add_test` block, per Task 2 Step 6's note)

**Interfaces:**
- Produces: `readSidechainSample(const float* sidechain, uint32_t n) noexcept -> float`, used by `TankPluginAdapter::run()` in Task 4.

- [ ] **Step 1: Write the failing test**

```cpp
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
```

- [ ] **Step 2: Run the test to verify it fails to compile (no header yet)**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

Expected: FAIL — `CMakeLists.txt` doesn't yet reference `test_sidechainguard.cpp`, and `Source/DSP/SidechainGuard.h` doesn't exist.

- [ ] **Step 3: Write `Source/DSP/SidechainGuard.h`**

```cpp
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
```

- [ ] **Step 4: Append the test target to `CMakeLists.txt`**

Add this block next to the other `add_executable`/`add_test` pairs (after `test_duckingenvelope`):

```cmake
add_executable(test_sidechainguard Tests/test_sidechainguard.cpp)
target_include_directories(test_sidechainguard PRIVATE Source/ Tests/)
target_compile_features(test_sidechainguard PRIVATE cxx_std_20)
add_test(NAME SidechainGuard COMMAND test_sidechainguard)
```

- [ ] **Step 5: Build and run the test**

```bash
cmake --build build --target test_sidechainguard
ctest --test-dir build -R SidechainGuard --output-on-failure
```

Expected: PASS, 6/6 checks (2 null-pointer checks + 4 real-buffer checks).

- [ ] **Step 6: Commit**

```bash
git add Source/DSP/SidechainGuard.h Tests/test_sidechainguard.cpp CMakeLists.txt
git commit -m "Add unit-tested sidechain-silence guard, porting the JUCE-era null-pointer check"
```

---

## Task 4: Wire the DSP chain and 3-port sidechain declaration into TankPluginAdapter

**Files:**
- Modify: `Source/TankPluginAdapter.h` (replace the Task 2 stub)
- Modify: `Source/TankPluginAdapter.cpp` (replace the Task 2 stub)

**Interfaces:**
- Consumes: `readSidechainSample()` (Task 3), `BandpassFilter`/`RmsDetector`/`DuckingEnvelope`/`LookaheadDelay` (`Source/DSP/*.h`, unchanged), `audioplugins::common::io::ParameterSmoother` and `audioplugins::common::io::MeterTransport` (`Common`, already linked via `AudioPluginsCommon::io` in Task 2), `TankParameters`/`TANK_PARAM_*` (`Source/DistrhoPluginInfo.h`, Task 2).
- Produces: `TankPluginAdapter::getCurrentReductionDb() const noexcept -> float` and `static constexpr float TankPluginAdapter::kMaxDepthDb = 12.0f`, both consumed by `TankUI` in Task 6.

- [ ] **Step 1: Write `Source/TankPluginAdapter.h`**

```cpp
#pragma once

#include "DistrhoPlugin.hpp"
#include "DSP/BandpassFilter.h"
#include "DSP/RmsDetector.h"
#include "DSP/DuckingEnvelope.h"
#include "DSP/LookaheadDelay.h"
#include "audioplugins/common/io/ParameterSmoother.h"
#include "audioplugins/common/io/MeterTransport.h"

#include <array>

START_NAMESPACE_DISTRHO

/**
   DPF Plugin adapter for Spellbound Tank.

   Thin shim over Tank's existing framework-free DSP chain
   (BandpassFilter -> RmsDetector -> DuckingEnvelope, plus a per-channel
   LookaheadDelay on the main signal): reads host parameters, drives the
   chain once per sample (matching the JUCE-era processBlock()'s per-sample
   loop, since DuckingEnvelope's state machine is inherently per-sample),
   and reports the current gain reduction via a MeterTransport rather than a
   DPF parameter (see DistrhoPluginInfo.h's DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
   comment for why -- same reasoning as Hex's IN/OUT meters).

   Sidechain: declares 3 input ports (index 0/1 = main stereo, index 2 =
   mono sidechain with the CLAP/VST3 sidechain hint set) and 2 output ports
   via initAudioPort() below -- DPF's confirmed, precedented mechanism (see
   the migration design spec's "DPF sidechain support" note). run()'s
   inputs[2] is the sidechain channel directly; readSidechainSample() (see
   DSP/SidechainGuard.h) treats a null/missing pointer as silence, porting
   the JUCE-era hasSidechain guard verbatim.
 */
class TankPluginAdapter : public Plugin
{
public:
    TankPluginAdapter();

protected:
    // -- Information -----------------------------------------------------
    const char* getLabel() const override;
    const char* getDescription() const override;
    const char* getMaker() const override;
    const char* getLicense() const override;
    uint32_t getVersion() const override;

    // -- Init -------------------------------------------------------------
    void initAudioPort(bool input, uint32_t index, AudioPort& port) override;
    void initParameter(uint32_t index, Parameter& parameter) override;

    // -- Internal data ------------------------------------------------------
    float getParameterValue(uint32_t index) const override;
    void setParameterValue(uint32_t index, float value) override;

public:
    // -- Direct UI access (DISTRHO_PLUGIN_WANT_DIRECT_ACCESS) ---------------
    // Read by TankUI via getPluginInstancePointer() + uiIdle(). Public
    // (unlike the DPF overrides above/below) because TankUI reaches this
    // through a raw TankPluginAdapter* obtained from
    // getPluginInstancePointer(), not from within the Plugin class hierarchy.
    float getCurrentReductionDb() const noexcept { return reductionMeter_.read(); }

    static constexpr float kMaxDepthDb = TANK_PARAM_DEPTH_MAX;

protected:
    // -- Process ------------------------------------------------------------
    void activate() override;
    void deactivate() override;
    void run(const float** inputs, float** outputs, uint32_t frames) override;

private:
    bool bypass_ = TANK_PARAM_BYPASS_DEFAULT > 0.5f;
    float depthDb_ = TANK_PARAM_DEPTH_DEFAULT;
    float anticipationMs_ = TANK_PARAM_ANTICIPATION_DEFAULT;
    float releaseMs_ = TANK_PARAM_RELEASE_DEFAULT;
    float sensitivityDb_ = TANK_PARAM_SENSITIVITY_DEFAULT;

    BandpassFilter  sidechainFilter_;
    RmsDetector     sidechainRms_;
    DuckingEnvelope duckEnvelope_;
    std::array<LookaheadDelay, 2> lookaheadDelay_;
    audioplugins::common::io::ParameterSmoother gainSmoother_;

    // Written from run() (audio thread) every block; read by TankUI via
    // getCurrentReductionDb() above, not via any DPF parameter/state.
    audioplugins::common::io::MeterTransport reductionMeter_ { "reduction_db" };

    int reportedLatencySamples_ = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TankPluginAdapter)
};

END_NAMESPACE_DISTRHO
```

- [ ] **Step 2: Write `Source/TankPluginAdapter.cpp`**

```cpp
#include "TankPluginAdapter.h"
#include "DSP/SidechainGuard.h"

#include <cmath>
#include <cstring>

START_NAMESPACE_DISTRHO

TankPluginAdapter::TankPluginAdapter()
    : Plugin(kParameterCount, 0, 0) // 5 parameters, 0 programs, 0 states -- the GR meter is direct-access only
{
}

const char* TankPluginAdapter::getLabel() const { return "Tank"; }
const char* TankPluginAdapter::getDescription() const { return "Sidechain pumping compressor"; }
const char* TankPluginAdapter::getMaker() const { return "Spellbound"; }

const char* TankPluginAdapter::getLicense() const
{
    // Must be a URI -- lv2lint's Plugin License test fails a plain word
    // like "Proprietary" (see DistrhoPluginLV2export.cpp's license branch,
    // confirmed against Hex).
    return "https://spellbound.audio/plugins/tank#license";
}

uint32_t TankPluginAdapter::getVersion() const { return d_version(0, 1, 0); }

void TankPluginAdapter::initAudioPort(const bool input, const uint32_t index, AudioPort& port)
{
    if (input)
    {
        if (index < 2)
        {
            // Main stereo input, index 0 = left, index 1 = right.
            port.groupId = kPortGroupStereo;
            port.name    = (index == 0) ? "Main Left" : "Main Right";
            port.symbol  = (index == 0) ? "main_in_left" : "main_in_right";
        }
        else
        {
            // index == 2: mono sidechain input -- the one genuinely novel
            // port shape in this migration (see the design spec's "DPF
            // sidechain support, confirmed real" note).
            port.hints   = kAudioPortIsSidechain;
            port.groupId = kPortGroupMono;
            port.name    = "Sidechain";
            port.symbol  = "sidechain_in";
        }
        return;
    }

    // Main stereo output, index 0 = left, index 1 = right.
    port.groupId = kPortGroupStereo;
    port.name    = (index == 0) ? "Main Left" : "Main Right";
    port.symbol  = (index == 0) ? "main_out_left" : "main_out_right";
}

void TankPluginAdapter::initParameter(const uint32_t index, Parameter& parameter)
{
    switch (index)
    {
    case kParameterBypass:
        parameter.hints  = kParameterIsAutomatable | kParameterIsBoolean;
        parameter.name   = "Bypass";
        parameter.symbol = "bypass";
        parameter.ranges.def = TANK_PARAM_BYPASS_DEFAULT;
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 1.0f;
        break;
    case kParameterDepth:
        parameter.hints  = kParameterIsAutomatable;
        parameter.name   = "Mitigation";
        parameter.symbol = "depth";
        parameter.unit   = "dB";
        parameter.ranges.def = TANK_PARAM_DEPTH_DEFAULT;
        parameter.ranges.min = TANK_PARAM_DEPTH_MIN;
        parameter.ranges.max = TANK_PARAM_DEPTH_MAX;
        break;
    case kParameterAnticipation:
        parameter.hints  = kParameterIsAutomatable;
        parameter.name   = "Reflex";
        parameter.symbol = "anticipation";
        parameter.unit   = "ms";
        parameter.ranges.def = TANK_PARAM_ANTICIPATION_DEFAULT;
        parameter.ranges.min = TANK_PARAM_ANTICIPATION_MIN;
        parameter.ranges.max = TANK_PARAM_ANTICIPATION_MAX;
        break;
    case kParameterRelease:
        parameter.hints  = kParameterIsAutomatable;
        parameter.name   = "Cooldown";
        parameter.symbol = "release";
        parameter.unit   = "ms";
        parameter.ranges.def = TANK_PARAM_RELEASE_DEFAULT;
        parameter.ranges.min = TANK_PARAM_RELEASE_MIN;
        parameter.ranges.max = TANK_PARAM_RELEASE_MAX;
        break;
    case kParameterSensitivity:
        parameter.hints  = kParameterIsAutomatable;
        parameter.name   = "Aggro Trigger";
        parameter.symbol = "sensitivity";
        parameter.unit   = "dB";
        parameter.ranges.def = TANK_PARAM_SENSITIVITY_DEFAULT;
        parameter.ranges.min = TANK_PARAM_SENSITIVITY_MIN;
        parameter.ranges.max = TANK_PARAM_SENSITIVITY_MAX;
        break;
    default:
        break;
    }
}

float TankPluginAdapter::getParameterValue(const uint32_t index) const
{
    switch (index)
    {
    case kParameterBypass:       return bypass_ ? 1.0f : 0.0f;
    case kParameterDepth:        return depthDb_;
    case kParameterAnticipation: return anticipationMs_;
    case kParameterRelease:      return releaseMs_;
    case kParameterSensitivity:  return sensitivityDb_;
    default:                     return 0.0f;
    }
}

void TankPluginAdapter::setParameterValue(const uint32_t index, const float value)
{
    switch (index)
    {
    case kParameterBypass:       bypass_ = value > 0.5f; break;
    case kParameterDepth:        depthDb_ = value; break;
    case kParameterAnticipation: anticipationMs_ = value; break;
    case kParameterRelease:      releaseMs_ = value; break;
    case kParameterSensitivity:  sensitivityDb_ = value; break;
    default: break;
    }
}

void TankPluginAdapter::activate()
{
    const double sampleRate = getSampleRate();

    sidechainFilter_.prepare(sampleRate);
    sidechainRms_.prepare(sampleRate);
    duckEnvelope_.prepare(sampleRate);

    for (auto& delay : lookaheadDelay_)
        delay.prepare(sampleRate, TANK_MAX_ANTICIPATION_MS);

    gainSmoother_.reset(sampleRate, 0.001f, 1.0f);

    reportedLatencySamples_ = static_cast<int>(std::round(anticipationMs_ * 0.001 * sampleRate));
    setLatency(static_cast<uint32_t>(reportedLatencySamples_));
}

void TankPluginAdapter::deactivate()
{
    sidechainFilter_.reset();
    sidechainRms_.reset();
    duckEnvelope_.reset();
    for (auto& delay : lookaheadDelay_)
        delay.reset();
}

void TankPluginAdapter::run(const float** inputs, float** outputs, const uint32_t frames)
{
    const float* mainInL  = inputs[0];
    const float* mainInR  = inputs[1];
    const float* sidechain = inputs[2];

    duckEnvelope_.setParameters(depthDb_, anticipationMs_, releaseMs_, sensitivityDb_);

    const double sampleRate = getSampleRate();
    const int anticipationSamples = static_cast<int>(std::round(anticipationMs_ * 0.001 * sampleRate));
    if (anticipationSamples != reportedLatencySamples_)
    {
        reportedLatencySamples_ = anticipationSamples;
        setLatency(static_cast<uint32_t>(reportedLatencySamples_));
    }

    float reductionDb = 0.0f;
    for (uint32_t n = 0; n < frames; ++n)
    {
        const float scInput    = readSidechainSample(sidechain, n);
        const float scFiltered = sidechainFilter_.process(scInput);
        const float scLevelDb  = sidechainRms_.process(scFiltered);
        reductionDb = duckEnvelope_.process(scLevelDb);

        const float gain = bypass_ ? 1.0f : std::pow(10.0f, -reductionDb / 20.0f);
        gainSmoother_.setTarget(gain);
        const float appliedGain = gainSmoother_.getNextValue();

        const float delayedL = lookaheadDelay_[0].process(mainInL[n], anticipationSamples);
        const float delayedR = lookaheadDelay_[1].process(mainInR[n], anticipationSamples);
        outputs[0][n] = delayedL * appliedGain;
        outputs[1][n] = delayedR * appliedGain;
    }

    reductionMeter_.write(bypass_ ? 0.0f : reductionDb);
}

Plugin* createPlugin()
{
    return new TankPluginAdapter();
}

END_NAMESPACE_DISTRHO
```

- [ ] **Step 3: Build**

```bash
cmake --build build --parallel
```

Expected: `TankPluginAdapter.cpp` compiles and links into `Tank.vst3`/`Tank.clap`/`Tank.lv2`.

- [ ] **Step 4: Run the full DSP test suite to confirm the unchanged DSP classes still pass**

```bash
ctest --test-dir build --output-on-failure
```

Expected: 5/5 pass (the 4 pre-existing DSP tests + SidechainGuard from Task 3).

- [ ] **Step 5: Manual smoke test in the Standalone-equivalent — load in a test host**

DPF has no Standalone target for Tank (see Task 2 Step 6's note), so verify the build loads at all via a lightweight host. If `carla` is installed locally:

```bash
carla-single vst3 build/bin/Tank.vst3 &
```

Expected: Carla loads the plugin without crashing and shows 5 parameters (Bypass, Mitigation, Reflex, Cooldown, Aggro Trigger) even though the UI is still the Task 2 stub (plain background, no controls yet — that's Task 6). If `carla` isn't installed, skip this step and rely on Task 5's manual verification instead, which covers the same ground plus the sidechain-specific check.

- [ ] **Step 6: Commit**

```bash
git add Source/TankPluginAdapter.h Source/TankPluginAdapter.cpp
git commit -m "Wire Tank's DSP chain and 3-port sidechain declaration into TankPluginAdapter

Declares 5 host parameters (bypass, depth, anticipation, release,
sensitivity) and 3 input ports (stereo main + mono sidechain, the
latter tagged kAudioPortIsSidechain via initAudioPort) against 2
output ports. run() drives the existing BandpassFilter -> RmsDetector
-> DuckingEnvelope chain per-sample, applies the resulting gain
reduction to a per-channel LookaheadDelay'd main signal, and reports
the current reduction via a MeterTransport (no DPF output parameter --
same clap-validator constraint Hex's IN/OUT meters already worked
around). Ports the JUCE-era sidechain-silence guard via
DSP/SidechainGuard.h's readSidechainSample()."
```

---

## Task 5: Manual sidechain-wiring verification (documented process, ticketed)

DPF's `run()` has no unit-test framework in this workspace's convention (DSP unit tests are for framework-free classes; Task 3 already covers the one piece of adapter logic that *is* independently testable). Verifying that a real host actually connects Tank's 3rd input port to a sidechain source — and that Tank's `initAudioPort` hint is enough for the host to offer that routing at all — can only be confirmed by loading the built plugin in a real host and wiring it up. This task documents that process and files a tracking issue, rather than skipping verification because it isn't unit-testable.

**Files:** none (manual verification + a GitHub issue).

**Interfaces:** N/A.

- [ ] **Step 1: Load Tank in Carla (or another host with sidechain routing support) as VST3 or CLAP**

```bash
carla &
```

In Carla: `Plugins > Add Plugin...`, scan/select `Tank` (VST3 or CLAP), add it to a rack/patchbay slot on an audio track.

- [ ] **Step 2: Confirm the host exposes 3 audio inputs**

In Carla's patchbay view (or via the plugin's context menu > "Show custom UI" is not needed for this check — use the patchbay/connections graph), confirm Tank shows 3 input ports: 2 for "Main Left"/"Main Right" and a 3rd labeled "Sidechain". This confirms `initAudioPort`'s `kAudioPortIsSidechain` hint reached the host and DPF's port declaration is visible.

- [ ] **Step 3: Route a second audio source into the Sidechain port and confirm ducking behavior**

Add a second track (e.g. a kick drum loop or a simple sine burst at ~80 Hz, inside the `BandpassFilter`'s 50-150 Hz passband) and connect its output to Tank's "Sidechain" input in Carla's patchbay. Feed a sustained tone (e.g. a bass note) into Tank's main stereo input. Set `Aggro Trigger` (sensitivity) to around -20 dB and `Mitigation` (depth) to a clearly audible value like 8-10 dB.

Expected: every time the kick/sine-burst sidechain source fires, the main signal audibly ducks by roughly the `Mitigation` amount, recovering over the `Cooldown` (release) time — confirming `inputs[2]` in `run()` really is receiving the sidechain host connects, not silence.

- [ ] **Step 4: Confirm the silence-guard path with no sidechain connected**

Remove the sidechain connection in Carla's patchbay (leave the main input connected). Expected: the main signal passes through unducked (no gain reduction at all) — confirming `readSidechainSample()`'s null/missing-data guard (Task 3) correctly treats a disconnected sidechain as silence rather than crashing or reading garbage.

- [ ] **Step 5: File a tracking issue for this manual verification**

```bash
gh issue create --repo TriYop/spellbound-tank \
  --title "Manual verification: sidechain routing and silence-guard in a real host" \
  --body "$(cat <<'EOF'
Tracks the manual host-verification pass for Tank's DPF sidechain port
(index 2, kAudioPortIsSidechain), per the migration plan's Task 5
(docs/superpowers/plans/2026-09-05-tank-dpf-migration.md).

Verification steps (Carla, VST3/CLAP):
- [ ] Carla's patchbay shows 3 audio inputs on Tank (Main Left, Main
      Right, Sidechain)
- [ ] Routing a kick/sine-burst source into Sidechain produces audible
      ducking on the main signal, recovering over the Cooldown time
- [ ] Disconnecting the sidechain input leaves the main signal
      unducked (silence-guard confirmed, not a crash or garbage read)

This is DPF adapter-glue behavior with no unit-test coverage by this
workspace's convention (DSP unit tests are for framework-free classes
only) -- see the plan's Task 3/Task 5 rationale.
EOF
)"
```

Record the resulting issue number; close it once Steps 1-4 above are confirmed (either in this task if a host is available now, or by whoever next has access to a GUI host/Carla, per this workspace's "ticket per task" convention — do not silently skip filing the issue just because a host isn't available in this environment).

- [ ] **Step 6: If a host was available and verification passed, close the issue; otherwise leave it open**

```bash
gh issue close <issue-number> --repo TriYop/spellbound-tank --comment "Verified: 3 inputs visible, ducking confirmed with sidechain connected, silence-guard confirmed with it disconnected."
```

If no GUI host was available in this environment (e.g. a headless CI-like session), leave the issue open and say so explicitly in the task report — do not claim verification happened without having actually run it.

---

## Task 6: Port the UI — bronze theme, 4 knobs, bypass toggle, horizontal GR meter

**Files:**
- Modify: `Source/TankUI.h` (replace the Task 2 stub)
- Modify: `Source/TankUI.cpp` (replace the Task 2 stub)

**Interfaces:**
- Consumes: `TankPluginAdapter::getCurrentReductionDb()`/`kMaxDepthDb` (Task 4); `TankParameters`/`TANK_PARAM_*` (`Source/DistrhoPluginInfo.h`, Task 2); `audioplugins::common::hui::dgl::RotaryKnob`/`RotaryKnobPalette`, `ToggleSwitch`/`ToggleSwitchPalette`, `VuMeter`/`VuMeterPalette`/`Orientation::Horizontal` (Common `v0.3.0`).
- Produces: nothing consumed by a later task directly, but Task 7 extends this same file with the presets panel.

- [ ] **Step 1: Confirm `Common`'s `VuMeter` `Horizontal` orientation API matches what this task assumes**

```bash
grep -n "Orientation\|setOrientation" build/_deps/audiopluginscommon-src/include/audioplugins/common/hui/dgl/VuMeter.h
```

Expected: an `enum class Orientation { Vertical, Horizontal }` and `void setOrientation(Orientation)` (default `Vertical`). If the API differs from this (e.g. different enum name), adjust Step 3 below to match the real signature rather than guessing — this plan was written against the design spec's description of the *planned* API, not a merged implementation seen firsthand.

- [ ] **Step 2: Write the bronze palette constants and layout constants in an anonymous namespace at the top of `Source/TankUI.cpp`**

```cpp
namespace {

namespace hui = audioplugins::common::hui;

// Bronze/brown "MMORPG tank" palette, ported verbatim from the JUCE-era
// TankCol (see Source/_juce_reference/UI/TankLookAndFeel.h).
const hui::dgl::RotaryKnobPalette kTankKnobPalette = {
    /* track        */ {0x3a, 0x2c, 0x1a, 0xff}, // TankCol::trackArc()     0xff3a2c1a
    /* valueArc     */ {0xcc, 0x88, 0x33, 0xff}, // TankCol::valueArc()     0xffcc8833
    /* valueArcGlow */ {0xff, 0xaa, 0x44, 0xff}, // TankCol::valueArcGlow() 0xffffaa44
    /* knobTop      */ {0x5a, 0x45, 0x30, 0xff}, // TankCol::knobTop()      0xff5a4530
    /* knobBottom   */ {0x1c, 0x14, 0x10, 0xff}, // TankCol::knobBottom()   0xff1c1410
    /* knobRim      */ {0x6a, 0x53, 0x40, 0xff}, // TankCol::knobRim()      0xff6a5340
};

const hui::dgl::ToggleSwitchPalette kTankTogglePalette = {
    /* bezel      */ {0x1c, 0x14, 0x10, 0xff}, // TankCol::knobBottom()
    /* track      */ {0x3a, 0x2c, 0x1a, 0xff}, // TankCol::trackArc()
    /* activeGlow */ {0xcc, 0x88, 0x33, 0xff}, // TankCol::valueArc()
    /* thumbTop   */ {0x5a, 0x45, 0x30, 0xff}, // TankCol::knobTop()
    /* thumbBottom*/ {0x1c, 0x14, 0x10, 0xff}, // TankCol::knobBottom()
    /* thumbRim   */ {0x6a, 0x53, 0x40, 0xff}, // TankCol::knobRim()
};

const hui::dgl::VuMeterPalette kTankGrMeterPalette = {
    /* background */ {0x12, 0x0c, 0x08, 0xff}, // TankCol::tbBg()        0xff120c08
    /* border     */ {0x4a, 0x3a, 0x28, 0xff}, // TankCol::panelBorder() 0xff4a3a28
    /* fillLow    */ {0xcc, 0x88, 0x33, 0xff}, // TankCol::valueArc()
    /* fillHigh   */ {0xff, 0xaa, 0x44, 0xff}, // TankCol::valueArcGlow()
    /* peakLine   */ {0xff, 0xff, 0xff, 0xff}, // white (VuMeterPalette default)
};

constexpr hui::Colour kBackgroundColor{0x1c, 0x14, 0x10, 0xff};  // TankCol::bg()          0xff1c1410
constexpr hui::Colour kTextPrimaryColor{0xd8, 0xc8, 0xa8, 0xff}; // TankCol::textPrimary() 0xffd8c8a8

// DPF's idle callback runs at ~62.5Hz (16ms, both CLAP/VST3 timer
// intervals -- see Hex's HexUI.cpp derivation). VuMeterModel's peak-hold
// decays at kPeakDecayPerSecond=0.3/s from a max pushed level of 1.0 (Tank
// pushes a normalized 0..1 reduction ratio, never above 1.0, unlike a raw
// audio level that can exceed 1.0): 1.0 / 0.3 = 3.33s of decay, so
// 3.33 * 62.5 = ~208 ticks. Reusing Hex's more conservative 320-tick
// constant (derived for kMaxLevel=1.5) is safe here too -- it only means
// the GR meter keeps refreshing slightly longer than strictly necessary
// after gain reduction returns to 0, never less.
constexpr int kMeterIdleTicks = 320;

inline DGL_NAMESPACE::Color toDglColor(const hui::Colour& c) noexcept
{
    return DGL_NAMESPACE::Color(static_cast<int>(c.r), static_cast<int>(c.g),
                                 static_cast<int>(c.b), static_cast<float>(c.a) / 255.f);
}

// Canvas is DISTRHO_UI_DEFAULT_WIDTH x DISTRHO_UI_DEFAULT_HEIGHT (500x284).
// Layout ported from the JUCE-era resized() (Source/_juce_reference/
// PluginEditor.cpp), stacked top-to-bottom: preset bar (new, see Task 7),
// bypass toggle, GR meter (full width, horizontal), then the 4-knob row --
// same vertical order the JUCE editor used (top row bypass, then meter row,
// then knob row).
constexpr float kPresetBarX = 16.0f;
constexpr float kPresetBarY = 8.0f;
constexpr uint  kPresetBarRowH = 24;
constexpr uint  kPresetSelectorW = 200;
constexpr uint  kPresetButtonW = 56;
constexpr float kPresetButtonGap = 6.0f;
constexpr float kSaveButtonX = kPresetBarX + static_cast<float>(kPresetSelectorW) + 8.0f;
constexpr float kDeleteButtonX = kSaveButtonX + static_cast<float>(kPresetButtonW) + kPresetButtonGap;

constexpr int   kBypassX = 16;
constexpr int   kBypassY = 44;
constexpr uint  kBypassSwitchW = 30;
constexpr uint  kBypassSwitchH = 40;
constexpr float kBypassLabelCenterY = 96.0f;
constexpr float kLabelFontSize = 11.0f;

constexpr float kMeterLabelCenterY = 118.0f;
constexpr int   kMeterX = 16;
constexpr int   kMeterY = 126;
constexpr uint  kMeterW = 468; // 500 - 16 margin each side
constexpr uint  kMeterH = 22;

constexpr int kKnobY = 168;
constexpr uint kKnobSize = 84;
constexpr int kKnobGap = 20;
constexpr int kLabelY = kKnobY + static_cast<int>(kKnobSize) + 6; // 258
constexpr int kLabelH = 16;

// 4 knobs, centered in the 468px usable width (x=16..484).
constexpr int kKnobRowContentW = 4 * static_cast<int>(kKnobSize) + 3 * kKnobGap; // 396
constexpr int kKnobRowX0 = 16 + (static_cast<int>(kMeterW) - kKnobRowContentW) / 2; // 52

constexpr int kDepthX        = kKnobRowX0;                                            // 52
constexpr int kAnticipationX = kDepthX + static_cast<int>(kKnobSize) + kKnobGap;      // 156
constexpr int kReleaseX      = kAnticipationX + static_cast<int>(kKnobSize) + kKnobGap; // 260
constexpr int kSensitivityX  = kReleaseX + static_cast<int>(kKnobSize) + kKnobGap;      // 364

struct KnobSpec
{
    uint32_t parameterIndex;
    int x;
    float rangeMin, rangeMax, defaultValue;
    const char* label;
};

constexpr KnobSpec kDepthSpec        { kParameterDepth,        kDepthX,        TANK_PARAM_DEPTH_MIN,        TANK_PARAM_DEPTH_MAX,        TANK_PARAM_DEPTH_DEFAULT,        "Mitigation" };
constexpr KnobSpec kAnticipationSpec { kParameterAnticipation, kAnticipationX, TANK_PARAM_ANTICIPATION_MIN, TANK_PARAM_ANTICIPATION_MAX, TANK_PARAM_ANTICIPATION_DEFAULT, "Reflex" };
constexpr KnobSpec kReleaseSpec      { kParameterRelease,      kReleaseX,      TANK_PARAM_RELEASE_MIN,      TANK_PARAM_RELEASE_MAX,      TANK_PARAM_RELEASE_DEFAULT,      "Cooldown" };
constexpr KnobSpec kSensitivitySpec  { kParameterSensitivity,  kSensitivityX,  TANK_PARAM_SENSITIVITY_MIN,  TANK_PARAM_SENSITIVITY_MAX,  TANK_PARAM_SENSITIVITY_DEFAULT,  "Aggro Trigger" };

constexpr const KnobSpec* kKnobSpecs[4] = { &kDepthSpec, &kAnticipationSpec, &kReleaseSpec, &kSensitivitySpec };

} // namespace
```

This anonymous namespace is reopened in Step 4 below to add the widget-construction helper functions (`makeKnob()`, `makeBypassSwitch()`, `makeGrMeter()`) — C++ allows the same anonymous namespace to be reopened multiple times in one translation unit, so Step 4 simply writes another `namespace { ... }` block further down the same file; the two are not one continuous block.

- [ ] **Step 3: Write `Source/TankUI.h`**

```cpp
#pragma once

#include "DistrhoUI.hpp"
#include "TankPluginAdapter.h"

#include "audioplugins/common/hui/dgl/RotaryKnob.h"
#include "audioplugins/common/hui/dgl/ToggleSwitch.h"
#include "audioplugins/common/hui/dgl/VuMeter.h"

#include <array>
#include <memory>

START_NAMESPACE_DISTRHO

/**
   DPF UI adapter for Spellbound Tank.

   Ports the JUCE-era bronze TankLookAndFeel palette (see
   Source/_juce_reference/UI/TankLookAndFeel.h) onto DGL/NanoVG: 4 rotary
   knobs (Mitigation/Reflex/Cooldown/Aggro Trigger), a ToggleSwitch for
   Bypass, and the gain-reduction meter via Common's VuMeter in its
   Horizontal orientation (v0.3.0), polled every uiIdle() tick straight off
   the DSP instance via fPluginPtr (DISTRHO_PLUGIN_WANT_DIRECT_ACCESS) --
   same direct-access idiom Hex's IN/OUT meters use, since a live gain-
   reduction value can't be a DPF output parameter (clap-validator rejects
   those).

   The presets panel (PresetSelector + SAVE/DELETE buttons) is added in
   Task 7.
 */
class TankUI : public UI
{
public:
    TankUI();

protected:
    // -- DSP/Plugin Callbacks ---------------------------------------------
    void parameterChanged(uint32_t index, float value) override;
    void uiIdle() override;

    // -- Widget Callbacks ---------------------------------------------------
    void onNanoDisplay() override;

private:
    // Raw, non-owning: TankUI does not own the DSP instance's lifetime, DPF
    // does. Captured once in the constructor via getPluginInstancePointer().
    TankPluginAdapter* const fPluginPtr;

    // Consecutive-quiet-tick countdown for uiIdle()'s meter-push gate --
    // same pattern as Hex's HexUI (see kMeterIdleTicks in TankUI.cpp).
    int fMeterIdleCountdown = 0;

    std::unique_ptr<audioplugins::common::hui::dgl::RotaryKnob> fDepthKnob;
    std::unique_ptr<audioplugins::common::hui::dgl::RotaryKnob> fAnticipationKnob;
    std::unique_ptr<audioplugins::common::hui::dgl::RotaryKnob> fReleaseKnob;
    std::unique_ptr<audioplugins::common::hui::dgl::RotaryKnob> fSensitivityKnob;
    std::unique_ptr<audioplugins::common::hui::dgl::ToggleSwitch> fBypassSwitch;
    std::unique_ptr<audioplugins::common::hui::dgl::VuMeter> fGrMeter;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TankUI)
};

END_NAMESPACE_DISTRHO
```

- [ ] **Step 4: Append the widget-construction helpers, constructor, and callbacks to `Source/TankUI.cpp`**

Right after the anonymous namespace from Step 2, reopen a new anonymous namespace block and add:

```cpp
namespace {

std::unique_ptr<hui::dgl::RotaryKnob> makeKnob(TankUI& ui, const KnobSpec& spec)
{
    std::unique_ptr<hui::dgl::RotaryKnob> knob(new hui::dgl::RotaryKnob(&ui));
    knob->setSize(kKnobSize, kKnobSize);
    knob->setAbsolutePos(spec.x, kKnobY);
    knob->setPalette(kTankKnobPalette);
    knob->setRange(spec.rangeMin, spec.rangeMax);
    knob->setDefaultValue(spec.defaultValue);
    knob->setValue(spec.defaultValue);

    const uint32_t paramIndex = spec.parameterIndex;
    hui::dgl::RotaryKnob* const rawKnob = knob.get();
    rawKnob->onDragStateChanged = [&ui, paramIndex](const bool started) { ui.editParameter(paramIndex, started); };
    rawKnob->onValueChanged = [&ui, paramIndex](const float value) { ui.setParameterValue(paramIndex, value); };

    return knob;
}

std::unique_ptr<hui::dgl::ToggleSwitch> makeBypassSwitch(TankUI& ui)
{
    std::unique_ptr<hui::dgl::ToggleSwitch> sw(new hui::dgl::ToggleSwitch(&ui));
    sw->setSize(kBypassSwitchW, kBypassSwitchH);
    sw->setAbsolutePos(kBypassX, kBypassY);
    sw->setPalette(kTankTogglePalette);
    sw->setPosition(static_cast<int>(TANK_PARAM_BYPASS_DEFAULT));

    hui::dgl::ToggleSwitch* const rawSwitch = sw.get();
    rawSwitch->onDragStateChanged = [&ui](const bool started) { ui.editParameter(kParameterBypass, started); };
    rawSwitch->onPositionChanged = [&ui](const int position) { ui.setParameterValue(kParameterBypass, static_cast<float>(position)); };

    return sw;
}

std::unique_ptr<hui::dgl::VuMeter> makeGrMeter(TankUI& ui)
{
    std::unique_ptr<hui::dgl::VuMeter> meter(new hui::dgl::VuMeter(&ui));
    meter->setSize(kMeterW, kMeterH);
    meter->setAbsolutePos(kMeterX, kMeterY);
    meter->setPalette(kTankGrMeterPalette);
    meter->setOrientation(hui::dgl::VuMeter::Orientation::Horizontal);
    return meter;
}

} // namespace

TankUI::TankUI()
    : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT),
      fPluginPtr(static_cast<TankPluginAdapter*>(getPluginInstancePointer())),
      fDepthKnob(makeKnob(*this, kDepthSpec)),
      fAnticipationKnob(makeKnob(*this, kAnticipationSpec)),
      fReleaseKnob(makeKnob(*this, kReleaseSpec)),
      fSensitivityKnob(makeKnob(*this, kSensitivitySpec)),
      fBypassSwitch(makeBypassSwitch(*this)),
      fGrMeter(makeGrMeter(*this))
{
    loadSharedResources();
}

void TankUI::parameterChanged(const uint32_t index, const float value)
{
    // Programmatic path: setValue()/setPosition() deliberately don't fire
    // onValueChanged/onPositionChanged, so host automation / preset recall
    // can't loop back out to the host.
    switch (index)
    {
    case kParameterDepth:        fDepthKnob->setValue(value); break;
    case kParameterAnticipation: fAnticipationKnob->setValue(value); break;
    case kParameterRelease:      fReleaseKnob->setValue(value); break;
    case kParameterSensitivity:  fSensitivityKnob->setValue(value); break;
    case kParameterBypass:       fBypassSwitch->setPosition(static_cast<int>(value + 0.5f)); break;
    default:
        break;
    }
}

void TankUI::uiIdle()
{
    if (fPluginPtr == nullptr)
        return;

    const float reductionDb = fPluginPtr->getCurrentReductionDb();
    const float normalized = reductionDb / (TankPluginAdapter::kMaxDepthDb > 0.001f ? TankPluginAdapter::kMaxDepthDb : 0.001f);

    if (normalized > 0.0f)
        fMeterIdleCountdown = kMeterIdleTicks;
    else if (fMeterIdleCountdown > 0)
        --fMeterIdleCountdown;
    else
        return;

    fGrMeter->pushLevel(normalized);
}

void TankUI::onNanoDisplay()
{
    // 1. Background -----------------------------------------------------
    beginPath();
    rect(0.0f, 0.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()));
    fillColor(toDglColor(kBackgroundColor));
    fill();
    closePath();

    // 2. Bypass label -----------------------------------------------------
    fontFace(NANOVG_DEJAVU_SANS_TTF);
    fontSize(kLabelFontSize);
    fillColor(toDglColor(kTextPrimaryColor));
    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
    text(static_cast<float>(kBypassX) + static_cast<float>(kBypassSwitchW) * 0.5f, kBypassLabelCenterY, "BYPASS", nullptr);

    // 3. GR meter label ---------------------------------------------------
    textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
    text(static_cast<float>(kMeterX), kMeterLabelCenterY, "GAIN REDUCTION", nullptr);

    // 4. Per-knob labels ----------------------------------------------------
    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
    for (int i = 0; i < 4; ++i)
    {
        const float cx = static_cast<float>(kKnobSpecs[i]->x) + static_cast<float>(kKnobSize) * 0.5f;
        text(cx, static_cast<float>(kLabelY) + static_cast<float>(kLabelH) * 0.5f, kKnobSpecs[i]->label, nullptr);
    }
}

UI* createUI()
{
    return new TankUI();
}
```

- [ ] **Step 5: Build**

```bash
cmake --build build --parallel
```

Expected: `TankUI.cpp` compiles against `AudioPluginsCommon::hui_dgl`, and `Tank.vst3`/`Tank.clap`/`Tank.lv2` link. If Step 1's API-shape check found a different real signature for `VuMeter`'s orientation setter, this is where a mismatch would surface as a compile error — fix `makeGrMeter()` to match the real API, not this plan's guess.

- [ ] **Step 6: Manual visual check**

```bash
carla-single vst3 build/bin/Tank.vst3 &
```

Expected: the editor shows the bronze background, a vertical bypass rocker switch labeled "BYPASS", a horizontal empty gain-reduction bar labeled "GAIN REDUCTION", and 4 rotary knobs labeled "Mitigation", "Reflex", "Cooldown", "Aggro Trigger" in bronze tones matching the original JUCE editor's palette. Turning the sensitivity/depth knobs and feeding audio into a connected sidechain (as in Task 5) should visibly fill the GR meter left-to-right.

- [ ] **Step 7: Commit**

```bash
git add Source/TankUI.h Source/TankUI.cpp
git commit -m "Port Tank's editor onto DPF: bronze theme, 4 knobs, bypass toggle, horizontal GR meter

Ports TankLookAndFeel's bronze palette into Common::hui::dgl widget
palettes (RotaryKnobPalette/ToggleSwitchPalette/VuMeterPalette). The
GR meter uses Common's new VuMeter Horizontal orientation (v0.3.0),
polled every uiIdle() tick via DISTRHO_PLUGIN_WANT_DIRECT_ACCESS --
same reasoning as Hex's IN/OUT meters (clap-validator rejects
host-visible audio-reactive output parameters)."
```

---

## Task 7: Author and wire factory presets (new feature)

Tank never had a presets system in JUCE. Per the migration design spec's decision 4, this is new work, authored from scratch, not a port of existing XMLs.

**Files:**
- Create: `Source/FactoryPresets.h`
- Create: `Tests/test_factorypresets.cpp`
- Modify: `Source/TankUI.h`, `Source/TankUI.cpp` (add the presets panel)

**Interfaces:**
- Consumes: `audioplugins::common::presets::Preset`/`ParameterValue` (`Common`), `audioplugins::common::presets::PresetBrowser` (constructor `PresetBrowser(std::vector<Preset>, std::string userPresetsDir, std::string pluginId)`), `audioplugins::common::hui::dgl::PresetSelector`/`Button` (`Common`).
- Produces: `tankFactoryPresets() -> std::vector<audioplugins::common::presets::Preset>`, consumed by `TankUI`'s constructor.

- [ ] **Step 1: Write the failing test for the factory presets' shape**

```cpp
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
```

- [ ] **Step 2: Run the test to verify it fails (no header yet)**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

Expected: FAIL — `Source/FactoryPresets.h` doesn't exist and `CMakeLists.txt` doesn't reference the test yet.

- [ ] **Step 3: Write `Source/FactoryPresets.h`**

```cpp
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
```

- [ ] **Step 4: Append the test target to `CMakeLists.txt`**

```cmake
add_executable(test_factorypresets Tests/test_factorypresets.cpp)
target_include_directories(test_factorypresets PRIVATE Source/ Tests/)
target_link_libraries(test_factorypresets PRIVATE AudioPluginsCommon::presets)
target_compile_features(test_factorypresets PRIVATE cxx_std_20)
add_test(NAME FactoryPresets COMMAND test_factorypresets)
```

- [ ] **Step 5: Build and run the test**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target test_factorypresets
ctest --test-dir build -R FactoryPresets --output-on-failure
```

Expected: PASS.

- [ ] **Step 6: Wire the presets panel into `TankUI`**

Add to `Source/TankUI.h`'s includes:

```cpp
#include "audioplugins/common/hui/dgl/PresetSelector.h"
#include "audioplugins/common/hui/dgl/Button.h"
#include "audioplugins/common/presets/PresetBrowser.h"

#include <vector>
```

Add to `TankUI`'s protected section:

```cpp
    void uiFileBrowserSelected(const char* filename) override;
```

Add to `TankUI`'s private section:

```cpp
    void applyPreset(const audioplugins::common::presets::Preset& preset);
    std::vector<audioplugins::common::presets::ParameterValue> captureCurrentParameters() const;
    void refreshPresetControls();

    audioplugins::common::presets::PresetBrowser fPresetBrowser;
    std::unique_ptr<audioplugins::common::hui::dgl::PresetSelector> fPresetSelector;
    std::unique_ptr<audioplugins::common::hui::dgl::Button> fSaveButton;
    std::unique_ptr<audioplugins::common::hui::dgl::Button> fDeleteButton;
```

In `Source/TankUI.cpp`, add to the includes: `#include "FactoryPresets.h"`, `#include <cstdlib>`, `#include <string>`.

Add the button/selector palettes and the user-presets-directory helper to the anonymous namespace from Step 2 of Task 6:

```cpp
const hui::dgl::ButtonPalette kTankButtonPalette = {
    /* background         */ {0x3a, 0x2c, 0x1a, 0xff}, // TankCol::trackArc()
    /* backgroundDisabled */ {0x1c, 0x14, 0x10, 0xff}, // TankCol::knobBottom()
    /* border             */ {0x6a, 0x53, 0x40, 0xff}, // TankCol::knobRim()
    /* text               */ {0xd8, 0xc8, 0xa8, 0xff}, // TankCol::textPrimary()
    /* textDisabled       */ {0x7a, 0x6a, 0x55, 0xff}, // TankCol::textDim()
};

const hui::dgl::PresetSelectorPalette kTankPresetSelectorPalette = {
    /* closedBackground */ {0x3a, 0x2c, 0x1a, 0xff}, // TankCol::trackArc()
    /* listBackground   */ {0x1c, 0x14, 0x10, 0xff}, // TankCol::bg()
    /* border           */ {0x6a, 0x53, 0x40, 0xff}, // TankCol::knobRim()
    /* text             */ {0xd8, 0xc8, 0xa8, 0xff}, // TankCol::textPrimary()
    /* textFactory      */ {0x7a, 0x6a, 0x55, 0xff}, // TankCol::textDim()
    /* rowHighlight     */ {0xcc, 0x88, 0x33, 0x40}, // TankCol::valueArc() at low alpha
};

std::unique_ptr<hui::dgl::PresetSelector> makePresetSelector(TankUI& ui)
{
    std::unique_ptr<hui::dgl::PresetSelector> selector(new hui::dgl::PresetSelector(&ui));
    selector->setPalette(kTankPresetSelectorPalette);
    selector->setClosedSize(kPresetSelectorW, kPresetBarRowH);
    selector->setAbsolutePos(static_cast<int>(kPresetBarX), static_cast<int>(kPresetBarY));
    return selector;
}

std::unique_ptr<hui::dgl::Button> makeButton(TankUI& ui, const char* label, float x)
{
    std::unique_ptr<hui::dgl::Button> button(new hui::dgl::Button(&ui));
    button->setPalette(kTankButtonPalette);
    button->setLabel(label);
    button->setSize(kPresetButtonW, kPresetBarRowH);
    button->setAbsolutePos(static_cast<int>(x), static_cast<int>(kPresetBarY));
    return button;
}

// Linux-only for now, matching Hex's precedent -- Tank's Windows/macOS
// builds aren't verified yet either (see Task 8's CI note).
std::string tankUserPresetsDirectory()
{
    const char* home = std::getenv("HOME");
    if (home == nullptr)
        return "/tmp/Tank/presets";
    return std::string(home) + "/.config/Tank/presets";
}
```

Update `TankUI`'s constructor member-init list (add after `fGrMeter(makeGrMeter(*this))`):

```cpp
      fPresetBrowser(tankFactoryPresets(), tankUserPresetsDirectory(), "com.spellbound.tank"),
      fPresetSelector(makePresetSelector(*this)),
      fSaveButton(makeButton(*this, "SAVE", kSaveButtonX)),
      fDeleteButton(makeButton(*this, "DELETE", kDeleteButtonX))
```

and its body (replacing the single `loadSharedResources();` line):

```cpp
{
    loadSharedResources();

    fPresetSelector->onIndexSelected = [this](const int index)
    {
        if (const auto* preset = fPresetBrowser.selectIndex(index))
        {
            applyPreset(*preset);
            refreshPresetControls();
        }
    };

    fDeleteButton->onClick = [this]()
    {
        if (fPresetBrowser.deleteCurrent())
            refreshPresetControls();
    };

    fSaveButton->onClick = [this]()
    {
        const std::string startDir = tankUserPresetsDirectory();
        FileBrowserOptions options;
        options.saving = true;
        options.defaultName = "New Preset.xml";
        options.title = "Save Tank Preset";
        options.startDir = startDir.c_str();
        openFileBrowser(options);
    };

    refreshPresetControls();
}
```

Add the new method bodies (after `uiIdle()`, before `onNanoDisplay()`):

```cpp
void TankUI::uiFileBrowserSelected(const char* filename)
{
    if (filename == nullptr)
        return; // user cancelled the dialog

    std::string path(filename);
    const size_t slash = path.find_last_of("/\\");
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos)
        base = base.substr(0, dot);

    if (fPresetBrowser.saveAs(base, captureCurrentParameters()))
        refreshPresetControls();
}

void TankUI::applyPreset(const audioplugins::common::presets::Preset& preset)
{
    for (const auto& pv : preset.parameters)
    {
        uint32_t paramIndex;

        if (pv.id == "bypass")            { fBypassSwitch->setPosition(static_cast<int>(pv.value + 0.5f)); paramIndex = kParameterBypass; }
        else if (pv.id == "depth")        { fDepthKnob->setValue(pv.value);                                paramIndex = kParameterDepth; }
        else if (pv.id == "anticipation") { fAnticipationKnob->setValue(pv.value);                         paramIndex = kParameterAnticipation; }
        else if (pv.id == "release")      { fReleaseKnob->setValue(pv.value);                               paramIndex = kParameterRelease; }
        else if (pv.id == "sensitivity")  { fSensitivityKnob->setValue(pv.value);                           paramIndex = kParameterSensitivity; }
        else continue; // unknown id (forward-compatible with a future schema addition) -- ignore

        editParameter(paramIndex, true);
        setParameterValue(paramIndex, pv.value);
        editParameter(paramIndex, false);
    }
}

std::vector<audioplugins::common::presets::ParameterValue> TankUI::captureCurrentParameters() const
{
    return {
        {"bypass", static_cast<float>(fBypassSwitch->getPosition())},
        {"depth", fDepthKnob->getValue()},
        {"anticipation", fAnticipationKnob->getValue()},
        {"release", fReleaseKnob->getValue()},
        {"sensitivity", fSensitivityKnob->getValue()},
    };
}

void TankUI::refreshPresetControls()
{
    fPresetSelector->setEntries(fPresetBrowser.getEntries());
    fPresetSelector->setCurrentIndex(fPresetBrowser.getCurrentIndex());

    const auto entries = fPresetBrowser.getEntries();
    const int idx = fPresetBrowser.getCurrentIndex();
    const bool isFactory = (idx >= 0 && static_cast<size_t>(idx) < entries.size()) ? entries[static_cast<size_t>(idx)].isFactory : true;
    fDeleteButton->setEnabled(!isFactory);
}
```

- [ ] **Step 7: Build**

```bash
cmake --build build --parallel
```

Expected: links successfully against `AudioPluginsCommon::presets` (already linked into `Tank-ui` since Task 2).

- [ ] **Step 8: Run the full test suite**

```bash
ctest --test-dir build --output-on-failure
```

Expected: 6/6 pass (BandpassFilter, RmsDetector, LookaheadDelay, DuckingEnvelope, SidechainGuard, FactoryPresets).

- [ ] **Step 9: Manual visual check**

```bash
carla-single vst3 build/bin/Tank.vst3 &
```

Expected: a preset bar above the bypass row shows a dropdown defaulting to "Subtle Pump" plus SAVE/DELETE buttons (DELETE disabled on the factory presets). Selecting "Heavy Duck" or "Slow Cooldown" moves all 4 knobs and the bypass switch to match Step 3's values.

- [ ] **Step 10: Commit**

```bash
git add Source/FactoryPresets.h Tests/test_factorypresets.cpp Source/TankUI.h Source/TankUI.cpp CMakeLists.txt
git commit -m "Add Tank's first presets panel: 3 factory presets, PresetBrowser/PresetSelector wiring

New feature -- the JUCE-era plugin never had a presets system.
Authors 3 factory presets from scratch (Subtle Pump, Heavy Duck, Slow
Cooldown) into Source/FactoryPresets.h and wires Common's
PresetBrowser/PresetSelector/Button, same mechanical approach as
Hex's presets panel."
```

---

## Task 8: Add validator CI legs (pluginval, clap-validator, lv2lint)

**Files:**
- Modify: `.github/workflows/ci.yml`

**Interfaces:** N/A — CI-only change.

- [ ] **Step 1: Append the validator steps to the Linux leg of `.github/workflows/ci.yml`**

Add after the existing `Test` step (still inside the `build-and-test` job, guarded `if: runner.os == 'Linux'` throughout, same structure as Hex's proven `ci.yml`):

```yaml
      # --- Plugin validators (Linux-only for now) ---
      #
      # Windows/macOS validator support is explicitly deferred: those legs
      # don't yet have a verified build (continue-on-error above), so
      # there's nothing meaningful to validate there yet.

      - name: Download pluginval
        if: runner.os == 'Linux'
        run: |
          curl -sL -o pluginval_Linux.zip \
            https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Linux.zip
          unzip -o pluginval_Linux.zip
          chmod +x pluginval

      - name: Validate VST3 with pluginval
        if: runner.os == 'Linux'
        run: |
          xvfb-run -a ./pluginval --strictness-level 5 --validate build/bin/Tank.vst3

      - name: Download clap-validator
        if: runner.os == 'Linux'
        run: |
          curl -sL -o clap-validator.zip \
            https://github.com/free-audio/clap-validator/releases/download/0.4.1/clap-validator-0.4.1-127-g152b982-ubuntu-22.04.zip
          unzip -o clap-validator.zip
          tar -xzf clap-validator-*-ubuntu-22.04.tar.gz
          chmod +x clap-validator

      - name: Validate CLAP with clap-validator
        if: runner.os == 'Linux'
        run: |
          xvfb-run -a ./clap-validator validate build/bin/Tank.clap

      - name: Install lv2lint build dependencies
        if: runner.os == 'Linux'
        run: |
          # libelf-dev is required by -Delf-tests=enabled below (the ELF
          # symbol-visibility test) -- baked in from day one here, unlike
          # Hex's original CI pass which discovered this dependency was
          # missing only after a failed build; see Hex/CLAUDE.md's CI
          # findings for the original discovery.
          sudo apt-get install -y liblilv-dev lv2-dev libelf-dev meson ninja-build

      - name: Build lv2lint
        if: runner.os == 'Linux'
        run: |
          git clone --depth 1 https://github.com/sfztools/lv2lint /tmp/lv2lint
          meson setup -Donline-tests=disabled -Delf-tests=enabled -Dx11-tests=disabled /tmp/lv2lint/build /tmp/lv2lint
          ninja -C /tmp/lv2lint/build

      - name: Validate LV2 with lv2lint
        if: runner.os == 'Linux'
        # Same DPF-wide Plugin Class finding Hex's lv2lint run hits (DPF's
        # own LV2 TTL generator emits doap:Project alongside lv2:Plugin) --
        # non-blocking until/unless that's fixed upstream in DPF itself.
        continue-on-error: true
        run: |
          export LV2_PATH="/usr/lib/lv2:${GITHUB_WORKSPACE}/build/bin"
          export LD_PRELOAD="/tmp/lv2lint/build/lv2lint.so"
          /tmp/lv2lint/build/lv2lint.bin -s lv2_generate_ttl "https://spellbound.audio/plugins/tank"
```

- [ ] **Step 2: Run pluginval locally to catch findings before pushing**

```bash
curl -sL -o pluginval_Linux.zip https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Linux.zip
unzip -o pluginval_Linux.zip && chmod +x pluginval
xvfb-run -a ./pluginval --strictness-level 5 --validate build/bin/Tank.vst3
```

If this reports failures, do not skip them: for each distinct finding, file a GitHub issue (`gh issue create --repo TriYop/spellbound-tank --title "pluginval: <finding>" --body "<pluginval output excerpt + root-cause investigation>"`), fix the root cause in `TankPluginAdapter`/`TankUI`/the DSP files as appropriate, verify the fix locally, and close the issue referencing the fixing commit. Do not guess in advance what these findings will be — investigate the actual local run's output.

- [ ] **Step 3: Run clap-validator locally**

```bash
curl -sL -o clap-validator.zip https://github.com/free-audio/clap-validator/releases/download/0.4.1/clap-validator-0.4.1-127-g152b982-ubuntu-22.04.zip
unzip -o clap-validator.zip
tar -xzf clap-validator-*-ubuntu-22.04.tar.gz
chmod +x clap-validator
xvfb-run -a ./clap-validator validate build/bin/Tank.clap
```

Same "ticket per finding, fix root cause, verify, close" process as Step 2 for any failures. Watch specifically for the two failure modes Hex already root-caused upstream in DPF (both already patched by Task 2's `cmake/patches/*.patch`, so they should NOT recur here): the `transport-null`/`transport-fuzz*` latency-reporting-during-activate failures, and the chunked-state-read parameter-loss failure. If either reappears, that means the patches didn't apply — re-check Task 2 Step 9's "fresh `build/_deps` population" caveat before assuming it's a new bug.

- [ ] **Step 4: Run lv2lint locally**

```bash
git clone --depth 1 https://github.com/sfztools/lv2lint /tmp/lv2lint
meson setup -Donline-tests=disabled -Delf-tests=enabled -Dx11-tests=disabled /tmp/lv2lint/build /tmp/lv2lint
ninja -C /tmp/lv2lint/build
export LV2_PATH="/usr/lib/lv2:$(pwd)/build/bin"
export LD_PRELOAD="/tmp/lv2lint/build/lv2lint.so"
/tmp/lv2lint/build/lv2lint.bin -s lv2_generate_ttl "https://spellbound.audio/plugins/tank"
```

Same ticket-per-finding process, except the known DPF-wide "Plugin Class" finding (doap:Project alongside lv2:Plugin) which is already accepted as non-blocking (Step 1's `continue-on-error: true`) — don't re-file that one.

- [ ] **Step 5: Commit the CI change (and any DSP/adapter fixes from Steps 2-4 as separate commits)**

```bash
git add .github/workflows/ci.yml
git commit -m "Add pluginval/clap-validator/lv2lint Linux CI legs, with libelf-dev baked in from day one"
```

Any fixes made in Steps 2-4 get their own commits, each referencing the GitHub issue it closes, per this workspace's "ticket per task" convention:

```bash
git commit -m "Fix <finding>, closes #<issue-number>"
```

- [ ] **Step 6: Push and verify with `gh run list`**

```bash
git push -u origin worktree-dpf-stage0
gh run list --repo TriYop/spellbound-tank --branch worktree-dpf-stage0 --limit 5
```

Expected: the Linux leg of the newest run is green (or, if a `COMMON_REPO_TOKEN` secret is missing from the `spellbound-tank` repo, it fails at "Configure Common repo access" — resolve that manually via `gh secret set COMMON_REPO_TOKEN --repo TriYop/spellbound-tank` with the same PAT already used for Hex/Outflank before re-running). Do not report CI as passing without actually checking `gh run list`'s output.

---

## Task 9: Write `CLAUDE.md` from scratch

**Files:**
- Create: `CLAUDE.md`

**Interfaces:** N/A — documentation only.

- [ ] **Step 1: Write `CLAUDE.md`**

```markdown
# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Spellbound Tank is a DPF audio **effect** plugin (VST3 / CLAP / LV2) implementing a sidechain "pumping" compressor: it ducks a bus -- typically bass -- whenever a sidechain signal -- typically a kick -- hits, carving out low-end room. Fixed-depth trigger ducking in the vein of Kickstart/LFOTool, not a ratio-based compressor. Uses MMORPG "tank" vocabulary for its parameters (a bass "tanking" damage from a kick "boss") -- see README.md for the full flavor-text mapping.

**Current state: migrated off JUCE onto DPF.** The framework-free DSP chain (`BandpassFilter`, `RmsDetector`, `DuckingEnvelope`, `LookaheadDelay` in `Source/DSP/`) is unchanged from the JUCE era and unit-tested via CTest. `Source/TankPluginAdapter.{h,cpp}` wires all 5 host parameters (`bypass`, `depth`, `anticipation`, `release`, `sensitivity`) and the DSP chain, including Tank's 3-input sidechain bus shape (stereo main + mono sidechain, declared via `initAudioPort()` with the CLAP/VST3 sidechain hint on index 2). `Source/TankUI.{h,cpp}` has the ported bronze-themed editor (4 rotary knobs, a bypass toggle switch, a horizontal gain-reduction meter) plus a **new** factory-presets panel (Tank never had one in JUCE) backed by `Source/FactoryPresets.h`'s 3 presets and `AudioPlugins/Common`'s `PresetBrowser`/`PresetSelector`/`Button` widgets. The JUCE-era `PluginProcessor`/`PluginEditor`/`TankLookAndFeel`/`TankGrMeter` are preserved unchanged under `Source/_juce_reference/` as the porting reference this was migrated from. See `docs/superpowers/specs/2026-09-05-phase2-outflank-tank-dpf-migration-design.md` (in the `Common` repo) for the full migration design and `docs/superpowers/plans/2026-09-05-tank-dpf-migration.md` for the task-by-task implementation record.

## Build Commands

### Linux prerequisites (one-time)

```bash
sudo apt install cmake ninja-build build-essential git \
    libasound2-dev libjack-jackd2-dev \
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
    libxinerama-dev libxrandr-dev libxrender-dev \
    libfreetype-dev libfontconfig1-dev \
    libglu1-mesa-dev libwebkit2gtk-4.1-dev
```

(freetype/fontconfig/webkit2gtk are required by DPF's DGL/NanoVG UI backend and its WebView-leak workaround, not JUCE leftovers.)

### Configure / build / run

```bash
# Configure (first run fetches DPF + AudioPlugins/Common into build/_deps/)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
cmake --build build --parallel

# Build outputs (DPF layout, under build/bin/ -- no Standalone target: Tank
# is an effect, not an instrument; this workspace only requires Standalone
# for instruments)
#   build/bin/Tank.vst3/
#   build/bin/Tank.clap
#   build/bin/Tank.lv2/

# Unit tests (DSP + presets, no DPF/JUCE dependency)
ctest --test-dir build --output-on-failure

# Install plugins (Linux, dev build)
cp -r build/bin/Tank.vst3 ~/.vst3/
cp    build/bin/Tank.clap ~/.clap/
cp -r build/bin/Tank.lv2  ~/.lv2/
```

### Release packaging

```bash
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
cd build-release && cpack
```

Produces a portable `Tank-<version>-linux-x86_64.tar.gz` with `install.sh`/`uninstall.sh` for installing VST3/CLAP/LV2 to the user's plugin directories.

## Architecture

### Signal flow (fixed serial chain)

```
Sidechain bus (mono)  → 50–150 Hz bandpass → RMS detector → level (dB)
                                                  │
                                                  ▼
                              Ducking envelope (Aggro Trigger / Reflex / Mitigation / Cooldown)
                                                  │
                                                  ▼
Main bus (stereo) → lookahead delay (Reflex) → × gain reduction → Main out (stereo)
```

- **Ports**: 3 audio inputs (index 0/1 = main stereo L/R, index 2 = mono sidechain, `kAudioPortIsSidechain` hint), 2 audio outputs -- declared in `TankPluginAdapter::initAudioPort()`.
- **Detection**: sidechain-only, band-limited to 50-150 Hz (kick fundamental), RMS-based.
- **No sidechain connected** → `DSP/SidechainGuard.h`'s `readSidechainSample()` treats a null/missing channel pointer as silence → RMS never crosses the Aggro Trigger threshold → Tank never ducks.
- **Re-trigger before Cooldown finishes** → `DuckingEnvelope` ramps from its current reduction level rather than restarting from 0 (no click, no overshoot) -- unchanged DSP behavior from the JUCE era.

### Parameters

| Parameter | Host symbol | Range | Default |
|---|---|---|---|
| Aggro Trigger | `sensitivity` | -40 to 0 dB | -20 dB |
| Reflex | `anticipation` | 1-20 ms | 5 ms |
| Mitigation | `depth` | 0-12 dB | 6 dB |
| Cooldown | `release` | 50-500 ms | 150 ms |
| Bypass | `bypass` | on/off | off |

`Reflex` (anticipation) also drives reported plugin latency via `setLatency()` -- the lookahead delay's length in samples.

### UI

Bronze/brown "MMORPG tank" theme, ported from the JUCE-era `TankLookAndFeel` into `Common::hui::dgl` widget palettes (see `Source/TankUI.cpp`'s anonymous-namespace palette constants). 4 `RotaryKnob`s (Mitigation/Reflex/Cooldown/Aggro Trigger), one `ToggleSwitch` for Bypass, one `VuMeter` in `Horizontal` orientation for the gain-reduction meter (polled every `uiIdle()` tick via `DISTRHO_PLUGIN_WANT_DIRECT_ACCESS`, not a DPF parameter -- clap-validator rejects host-visible audio-reactive output parameters, same constraint Hex's IN/OUT meters already worked around). A presets panel (`PresetSelector` + SAVE/DELETE `Button`s) is a **new** feature added in this migration; Tank's 3 factory presets (`Subtle Pump`, `Heavy Duck`, `Slow Cooldown`) live in `Source/FactoryPresets.h`, authored from scratch since the JUCE version had no presets system to convert.

## Key design constraints

- **Effect, not synth**: `DISTRHO_PLUGIN_IS_SYNTH 0`; no MIDI. `TankPluginAdapter` declares all 5 parameters via DPF's `Parameter`/`initParameter()` API.
- **The one non-standard port shape in this workspace's DPF migrations**: 3 inputs (stereo main + mono sidechain) against 2 outputs, unlike every sibling effect plugin's plain stereo-in/stereo-out. See `initAudioPort()`'s `kAudioPortIsSidechain` hint and `DSP/SidechainGuard.h`'s null-pointer guard.
- **Real-time safety**: `run()` is allocation-free; parameter changes are smoothed via `AudioPluginsCommon::io::ParameterSmoother` (the applied gain) to avoid zipper noise; `DuckingEnvelope`'s own ramp rates handle the audible duck/release shape.
- **DSP is framework-free and unit-tested independently of DPF** (`Source/DSP/*.h` + `Tests/*.cpp`, CTest) -- `TankPluginAdapter`/`TankUI` are thin adapters with no DSP logic of their own, per this workspace's hexagonal-architecture convention.
```

- [ ] **Step 2: Commit**

```bash
git add CLAUDE.md
git commit -m "Add CLAUDE.md documenting Tank's DPF migration, architecture, and build commands"
```

---

## Task 10: Final manual host verification pass and wrap-up

Per the migration design spec's "Verification approach": a manual pass in a real host confirming the presets panel and the GR meter actually work, filed as a tracked issue, is required before Tank is considered migrated -- same as Hex's tracked verification issue.

**Files:** none (manual verification + a GitHub issue + PR).

**Interfaces:** N/A.

- [ ] **Step 1: Full local verification pass**

```bash
rm -rf build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Expected: clean configure/build, 6/6 tests passing (BandpassFilter, RmsDetector, LookaheadDelay, DuckingEnvelope, SidechainGuard, FactoryPresets).

- [ ] **Step 2: Manual host pass in Carla covering presets + GR meter together**

```bash
carla &
```

Load `build/bin/Tank.vst3` (or the CLAP), route a kick/sine-burst source into its Sidechain input (as in Task 5), and:
1. Select each of the 3 factory presets in turn and confirm all 4 knobs + the bypass switch visibly jump to that preset's values.
2. With "Heavy Duck" selected and the sidechain firing, confirm the GR meter's horizontal bar fills and empties in sync with the ducking, matching the `Cooldown` timing.
3. Save a modified set of knob values as a new user preset via the SAVE button's native file dialog; confirm it appears in the dropdown with DELETE enabled (unlike the factory presets), and that selecting a factory preset again re-disables DELETE.
4. Delete the user preset just created and confirm it disappears from the dropdown.

- [ ] **Step 3: File the tracked verification issue**

```bash
gh issue create --repo TriYop/spellbound-tank \
  --title "Manual host verification: presets panel + GR meter (DPF migration)" \
  --body "$(cat <<'EOF'
Tracks the final manual verification pass for Tank's DPF migration, per
the migration plan's Task 10
(docs/superpowers/plans/2026-09-05-tank-dpf-migration.md), matching the
migration design spec's "Verification approach" section.

- [ ] All 3 factory presets (Subtle Pump, Heavy Duck, Slow Cooldown)
      recall correctly in a real host (Carla)
- [ ] GR meter visibly tracks gain reduction during active ducking
- [ ] Saving a user preset works via the native file dialog and
      appears in the dropdown with DELETE enabled
- [ ] Deleting a user preset removes it from the dropdown
- [ ] Factory presets keep DELETE disabled

See also #<Task 5's sidechain-verification issue number> for the
sidechain-routing-specific verification.
EOF
)"
```

Close it once Step 2's checklist is confirmed, same "verify before claiming done" discipline as Task 5.

- [ ] **Step 4: Open the pull request**

```bash
git push -u origin worktree-dpf-stage0
gh pr create --repo TriYop/spellbound-tank \
  --title "Migrate Tank off JUCE onto DPF, add factory presets panel" \
  --body "$(cat <<'EOF'
## Summary
- Rewrites the build off JUCE + clap-juce-extensions onto DPF (pinned
  commit + 2 upstream patches) and AudioPlugins/Common v0.3.0, following
  Hex/Pugilist's proven migration pattern.
- Ports Tank's 3-bus shape (stereo main + mono sidechain) onto DPF's
  initAudioPort/kAudioPortIsSidechain mechanism -- the one genuinely novel
  piece versus prior migrations in this workspace.
- Ports the bronze-themed editor (4 knobs, bypass toggle, horizontal GR
  meter) onto Common's DGL widgets, including Common's new VuMeter
  Horizontal orientation.
- Adds a factory-presets panel -- a new feature, since the JUCE-era plugin
  never had one.
- Adds pluginval/clap-validator/lv2lint Linux CI legs.

## Test plan
- [x] `ctest --test-dir build --output-on-failure` -- 6/6 passing
- [x] Manual host verification in Carla (presets, GR meter, sidechain
      routing) -- see linked issues
- [ ] CI green on this PR (`gh run list --repo TriYop/spellbound-tank`)
EOF
)"
```

- [ ] **Step 5: Verify CI on the PR before considering this plan complete**

```bash
gh run list --repo TriYop/spellbound-tank --branch worktree-dpf-stage0 --limit 5
```

Do not report the migration as complete without this coming back green on the Linux leg (Windows/macOS remain `continue-on-error` and are not a completion criterion, per Global Constraints).
