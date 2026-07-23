# Tank

A sidechain "pumping" compressor (VST3 / CLAP / Standalone) that ducks a bus — typically bass — whenever a sidechain signal — typically the kick — hits, carving out room in the low end. Fixed-depth trigger ducking in the vein of Kickstart/LFOTool, not a ratio-based compressor.

![Tank editor](docs/images/tank-editor.png)

## The MMORPG parallel

Tank's controls are named after the MMORPG tanking role it's modeled on: the tank's job is to grab the boss's attention and eat the hit so the rest of the party doesn't have to. Here, the "boss" is the kick, and the instrument being ducked is the party member standing in the blast radius.

| Plugin parameter | APVTS id | MMORPG tank concept | What it actually does |
|---|---|---|---|
| **Aggro Trigger** | `sensitivity` | Aggro threshold — how easily the tank pulls the boss's attention | RMS level (post 50–150 Hz bandpass, sidechain-only) that must be crossed to trigger a duck |
| **Reflex** | `anticipation` | Reaction time before the tank raises its shield | Lookahead (1–20 ms): the main signal is delayed internally so the duck lands in sync with the sidechain hit instead of trailing it; also the reported plugin latency |
| **Mitigation** | `depth` | Damage mitigation — how much of the hit the tank's armor absorbs | Maximum gain reduction applied on a duck (0–12 dB) |
| **Cooldown** | `release` | Ability cooldown — time to recover before the next pull | Ramp-back time (50–500 ms) once the sidechain signal falls back below the Aggro Trigger threshold |
| **Bypass** | `bypass` | Tank drops aggro / leaves the party | Passes the main signal through untouched (still lookahead-delayed, so plugin latency never jumps) |

The GR meter is the "damage taken" bar: it shows how much gain reduction is currently being applied, live, at ~30 Hz.

## Signal flow

```
Sidechain bus (mono)  → 50–150 Hz bandpass → RMS detector → level (dB)
                                                  │
                                                  ▼
                              Ducking envelope (Aggro Trigger / Reflex / Mitigation / Cooldown)
                                                  │
                                                  ▼
Main bus (stereo) → lookahead delay (Reflex) → × gain reduction → Main out (stereo)
```

- **Buses**: stereo Main in, mono Sidechain in, stereo Main out.
- **Detection**: sidechain-only, band-limited to 50–150 Hz (kick fundamental), RMS-based.
- **No sidechain connected** → silence → RMS never crosses the Aggro Trigger threshold → Tank never ducks. No special-case handling needed.
- **Re-trigger before Cooldown finishes** → the envelope ramps from its current reduction level rather than restarting from 0 (no click, no overshoot).

## Repository layout

```
Source/
  PluginProcessor.h/.cpp   — AudioProcessor, APVTS parameter tree, multi-bus setup, DSP wiring, latency reporting
  PluginEditor.h/.cpp      — AudioProcessorEditor: 4 knobs, bypass toggle, GR meter, ~30 Hz UI timer
  DSP/                     — plain (non-JUCE) classes, each independently unit-testable
    BandpassFilter.h       — 2nd-order biquad, fixed 50–150 Hz, sidechain only
    RmsDetector.h          — windowed RMS envelope follower → level in dB
    LookaheadDelay.h        — per-channel circular delay, length = Reflex (anticipation) samples
    DuckingEnvelope.h       — idle → ramp → hold → ramp-back state machine
  UI/
    TankLookAndFeel.h/.cpp — bronze/brown "MMORPG tank" themed LookAndFeel (rotary knobs, labels, toggle)
    TankGrMeter.h          — gain-reduction meter widget
Tests/                     — JUCE-free CTest unit tests (one per DSP class)
scripts/
  install.sh / uninstall.sh — install/remove the packaged release build
docs/
  superpowers/specs/       — design spec
  superpowers/plans/       — implementation plan
  images/tank-editor.png   — screenshot used above
CMakeLists.txt
```

## Building

Prerequisites (Linux): CMake ≥ 3.22, Ninja, and the standard JUCE system deps — see `AudioPlugins/CLAUDE.md` in the workspace root for the full `apt install` line.

```bash
# Configure (first run fetches JUCE 8.0.13 + clap-juce-extensions, ~2 min)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
cmake --build build --parallel

# Run standalone
./build/Tank_artefacts/Debug/Standalone/Tank

# Unit tests
ctest --test-dir build --output-on-failure
```

### Release packaging

```bash
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
cd build-release && cpack
```

Produces a portable `Tank-<version>-linux-x86_64.tar.gz` with `install.sh` / `uninstall.sh` for installing VST3 + CLAP + Standalone to the user's plugin directories.

## Status

Implemented: DSP chain, APVTS parameters, bronze/brown MMORPG-tank themed editor, GR meter, unit tests, release packaging with install/uninstall scripts.
