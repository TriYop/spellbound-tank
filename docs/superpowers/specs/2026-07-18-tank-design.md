# Tank — Design Spec

Date: 2026-07-18
Status: Approved (design phase) — implementation not yet started

## Overview

Tank is a JUCE sidechain "pumping" compressor (VST3 / CLAP / Standalone), effect not synth (`IS_SYNTH false`). It ducks a bus (typically bass) whenever a sidechain signal (typically a kick track) hits, carving room for the kick — the classic "pumping" trick, not a ratio-based compressor.

Original spec lives at `AudioPlugins/README.md` (misplaced there instead of `Tank/README.md` — noted in `AudioPlugins/CLAUDE.md`). This document supersedes it with concrete design decisions.

GUI should visually follow Hex's rotary-knob `LookAndFeel` pattern, re-themed to a brownish/bronze "MMORPG tank" palette.

## Core DSP Model: Trigger-Based Ducker

When the sidechain's filtered RMS level crosses `sensitivity`, Tank ramps a **fixed** `depth` dB gain reduction into the main signal (not a level-proportional ratio). This matches the spec's fixed-depth control and is the standard "kick pumping" design (à la Kickstart/LFOTool), as opposed to a classic RMS ratio compressor.

## Bus Architecture

```cpp
BusesProperties()
    .withInput ("Main",      juce::AudioChannelSet::stereo(), true)
    .withInput ("Sidechain",  juce::AudioChannelSet::mono(),   true)
    .withOutput("Main",      juce::AudioChannelSet::stereo(), true)
```

This is new territory for the `AudioPlugins/` workspace — no sibling plugin currently has an external sidechain input bus (Pugilist has multiple *output* buses, not inputs).

If the host doesn't route anything into the Sidechain bus (or, in Standalone, the extra input channel isn't wired), the sidechain buffer reads as silence. RMS never crosses `sensitivity`, so Tank never ducks. This requires no special-case fallback code — it falls out naturally from the detector logic.

## Lookahead / Latency

`anticipation` (1–20 ms, default 5 ms) is implemented as **true lookahead**: the main signal is delayed internally by `anticipation` samples, and this is reported to the host via `setLatencySamples()` for automatic plugin-delay compensation (PDC). The sidechain RMS detector runs on the *undelayed* sidechain signal, so by the time the delayed main signal reaches the point of gain application, the ducking envelope is already ramping — the dip lands in sync with the (delayed) kick transient rather than trailing it. `anticipation` therefore serves double duty: lookahead length *and* attack-ramp length.

**Bypass interaction:** a naive bypass that skips all DSP (including the lookahead delay) would create a latency discontinuity — the plugin's reported latency would no longer match what's actually happening, producing a timing jump/click sized to `anticipation` when toggling bypass. Tank's `bypass` parameter must keep the `LookaheadDelay` stage always running (so `getLatencySamples()` never changes) and only skip the `DuckingEnvelope`'s gain-reduction application. Output during bypass = the untouched main signal, still delayed by the lookahead line.

## DSP Components (`Source/DSP/`)

Four plain (non-`AudioProcessor`) classes, each independently unit-testable — chosen over a single integrated class because clean, decomposed, testable code is preferred whenever the performance cost is limited (it isn't here: none of these classes allocate or use virtual dispatch in the hot path).

```
Source/DSP/
  BandpassFilter.h/.cpp    — 2nd-order bandpass biquad, fixed 50–150 Hz, applied to sidechain only
  RmsDetector.h/.cpp       — windowed RMS envelope follower → level in dB
  LookaheadDelay.h/.cpp    — per-channel circular delay buffer, length = anticipation samples
  DuckingEnvelope.h/.cpp   — state machine: idle → ramp 0→depth over `anticipation` ms once RMS
                             crosses `sensitivity` → hold while above threshold → ramp depth→0
                             over `release` ms once RMS falls back below `sensitivity`
```

### Data flow per block

```
processBlock()
  ├─ Sidechain bus  → BandpassFilter (50–150 Hz) → RmsDetector → level_dB
  ├─ DuckingEnvelope::process(level_dB, sensitivity, anticipation, depth, release, bypass)
  │     → gainReductionDb[n] per sample (0 if bypassed)
  ├─ Main bus       → LookaheadDelay (writes now, reads `anticipation` samples behind;
  │                    always runs, even when bypassed)
  ├─ delayedMain[n] *= dBToGain(gainReductionDb[n])   [applied via juce::SmoothedValue<float>,
  │                                                     matching sibling click-free convention]
  ├─ write current gainReductionDb to std::atomic<float> for the editor's GR meter
  └─ output = delayedMain
```

`PluginProcessor` owns one `BandpassFilter` + `RmsDetector` + `DuckingEnvelope` (mono — sidechain is mono) and a `LookaheadDelay` covering both main channels.

## Source Layout

```
Source/
  PluginProcessor.h/.cpp   — AudioProcessor; APVTS; multi-bus setup; wires DSP chain; latency reporting
  PluginEditor.h/.cpp      — AudioProcessorEditor; 4 knobs + bypass + GR meter; ~30 Hz timer
  DSP/
    BandpassFilter.h/.cpp
    RmsDetector.h/.cpp
    LookaheadDelay.h/.cpp
    DuckingEnvelope.h/.cpp
  UI/
    TankLookAndFeel.h/.cpp  — themed after HexLookAndFeel, brownish/bronze palette
Tests/
    test_bandpassfilter.cpp
    test_rmsdetector.cpp
    test_lookaheaddelay.cpp
    test_duckingenvelope.cpp
    test_runner.h
```

## Parameters (APVTS)

| ID | Range | Default | Description |
|---|---|---|---|
| `bypass` | bool | false | true = pass main signal through untouched but still lookahead-delayed (see Bypass Interaction above) |
| `depth` | 0–12 dB | 6 dB | max gain reduction applied on a duck (continuous knob, not a discrete switch) |
| `anticipation` | 1–20 ms | 5 ms | lookahead time; also the reported plugin latency and the attack-ramp length |
| `release` | 50–500 ms | 150 ms | ramp-back time after sidechain falls below threshold |
| `sensitivity` | −40 to 0 dB | −20 dB | RMS threshold (post bandpass-filter) that triggers a duck |

Non-APVTS: `gr_meter` — current gain reduction in dB, `std::atomic<float>`, read by the editor's timer. Not a plugin parameter, not automatable, not serialized.

## GUI / LookAndFeel

`TankLookAndFeel` (`Source/UI/TankLookAndFeel.h/.cpp`) is structured identically to `HexLookAndFeel` (same rotary-knob drawing approach, label/textbox/combo-box overrides), re-themed:

| Role | Hex (reference) | Tank |
|---|---|---|
| bg | `0xff1a1220` (near-black purple) | dark leather-brown (e.g. `0xff1c1410`) |
| panel | `0xff241a2c` | worn-bronze panel (e.g. `0xff2e2218`) |
| knobTop/knobBottom | purple gradient | bronze/copper gradient |
| valueArc | neon purple `0xffaa44ee` | burnished-orange/amber accent |
| textPrimary | lavender | parchment/tan |

Layout: single row of 4 knobs (Depth, Anticipation, Release, Sensitivity) + bypass toggle + a gain-reduction meter (bar showing current ducking depth, same atomic-read/30 Hz-timer pattern as MaxDPS/TrueSight). No second row — this is a simpler 4-knob utility plugin, unlike Hex's 4-block layout.

## Targets

VST3, CLAP, and **Standalone** (spec only mentioned VST3/CLAP, but Standalone is added to match every sibling plugin's convention and to allow fast manual verification without a DAW; the sidechain bus maps to extra input channels from the audio device in Standalone mode).

## Testing

JUCE-free unit tests under `Tests/`, CTest-registered, following the Outflank/Hex convention documented in `AudioPlugins/CLAUDE.md`:

- `test_bandpassfilter.cpp` — attenuation outside 50–150 Hz; near-unity passband gain
- `test_rmsdetector.cpp` — known sine/silence inputs → expected dB level within tolerance
- `test_lookaheaddelay.cpp` — output at time `n` equals input at `n − delaySamples`; buffer resize on `anticipation` change
- `test_duckingenvelope.cpp` — full state coverage (idle → ramp-down → hold → ramp-up → idle); re-trigger before release completes ramps from current reduction rather than restarting from 0 (no click, no overshoot)

## Edge Cases

- **No sidechain connected**: silence → RMS never crosses threshold → never ducks. No special-case code needed.
- **Sidechain re-triggers before previous release finishes**: envelope ramps from its *current* reduction level back toward full depth rather than restarting from 0 — same "smooth retrigger" principle as Catalyst's ADSR.
- **`anticipation` changed at runtime**: lookahead buffer must resize and `setLatencySamples()` must update. Hosts reset/flush plugin state around a latency change, so continuity isn't guaranteed across that specific change, but it must not crash or glitch.
- **Sample-rate changes** (`prepareToPlay`): all ms-based parameters (anticipation, release) are reconverted to samples.
