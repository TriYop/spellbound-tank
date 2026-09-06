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
