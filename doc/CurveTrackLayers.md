# CurveTrack and Layer Reference

This document describes the `CurveTrack` model and all curve-step layers in detail, including defaults, value ranges, serialization, runtime behavior, and version availability.

Source of truth:
- `src/apps/sequencer/model/CurveTrack.h`
- `src/apps/sequencer/model/CurveTrack.cpp`
- `src/apps/sequencer/model/CurveSequence.h`
- `src/apps/sequencer/model/CurveSequence.cpp`
- `src/apps/sequencer/model/ProjectVersion.h`
- `src/apps/sequencer/engine/CurveTrackEngine.cpp`

## Scope

`CurveTrack` contains:
- track-level playback/fill/mute behavior
- routable track parameters (slide, offset, rotate, probability biases)
- per-pattern `CurveSequence` data
- per-step curve layers (shape, variation, min/max, gate pattern)

This doc focuses on model + engine behavior for Curve tracks.

## 1) CurveTrack Structure

`CurveTrack` stores:
- `playMode` (`Types::PlayMode`)
- `fillMode` (`None`, `Variation`, `NextPattern`, `Invert`)
- `muteMode` (`LastValue`, `Zero`, `Min`, `Max`)
- routable track parameters:
  - `slideTime` (0..100)
  - `offset` (-500..500, shown as -5.00V..+5.00V via `offset * 0.01`)
  - `rotate` (-64..64)
  - `shapeProbabilityBias` (-8..8)
  - `gateProbabilityBias` (-8..8)
- sequence array: `CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT`

Serialization writes only `.base` for routable values.

## 2) CurveTrack Serialization Order

From `CurveTrack::write()`:

1. `playMode`
2. `fillMode`
3. `muteMode`
4. `slideTime.base`
5. `offset.base`
6. `rotate.base`
7. `shapeProbabilityBias.base`
8. `gateProbabilityBias.base`
9. `sequences[]`

Versioned read behavior (`CurveTrack::read()`):
- `slideTime` available since v8
- `shapeProbabilityBias` and `gateProbabilityBias` available since v15
- `muteMode` available since v22
- `offset` available since v28

## 3) Sequence-Level Parameters (CurveSequence)

Each `CurveSequence` stores:
- `range` (`Types::VoltageRange`)
- `divisor` (uint16_t; uint8_t before v10)
- `resetMeasure` (0..128)
- `runMode`
- `firstStep` / `lastStep`
- `steps[CONFIG_STEP_COUNT]`

Defaults (`CurveSequence::clear()`):
- `range = Bipolar5V`
- `divisor = 12`
- `resetMeasure = 0`
- `runMode = Forward`
- `firstStep = 0`, `lastStep = 15`

## 4) Step Layers: Overview

Layer enum (`CurveSequence::Layer`):
- `Shape`
- `ShapeVariation`
- `ShapeVariationProbability`
- `Min`
- `Max`
- `Gate`
- `GateProbability`

Layer ranges are defined in `CurveSequence::layerRange()`.

## 5) Layer Matrix (Range, Default, Since)

Defaults come from `CurveSequence::Step::clear()`.

| Layer | Effective range in UI/model | Default | Since | Stored bits |
|---|---|---:|---|---|
| `Shape` | `0..(Curve::Last-1)` (see shape table below) | 0 | legacy | `_data0` bits 0..5 |
| `ShapeVariation` | `0..(Curve::Last-1)` (see shape table below) | 0 | v15 | `_data0` bits 6..11 |
| `ShapeVariationProbability` | 0..8 (implemented) | 0 | v15 | `_data0` bits 12..15 |
| `Min` | 0..255 | 0 | legacy | `_data0` bits 16..23 |
| `Max` | 0..255 | 255 | legacy | `_data0` bits 24..31 |
| `Gate` | 0..15 (4-bit sub-gate mask) | 0 | v15 | `_data1` bits 0..3 |
| `GateProbability` | 0..7 | 7 | v15 | `_data1` bits 4..6 |

Important detail:
- `ShapeVariationProbability` type width is 4 bits, but setter clamps effective values to `0..8`.

## 6) Shape Types

`Shape` and `ShapeVariation` store an index into `Curve::Type` (`src/apps/sequencer/model/Curve.h`).
The enum order is the runtime mapping used by `Curve::function(...)` in `src/apps/sequencer/model/Curve.cpp`.

| Index | `Curve::Type` | Behavior |
|---:|---|---|
| 0 | `Low` | Constant 0 |
| 1 | `High` | Constant 1 |
| 2 | `StepUp` | 0 for first half, 1 for second half |
| 3 | `StepDown` | 1 for first half, 0 for second half |
| 4 | `RampUp` | Linear up (`x`) |
| 5 | `RampDown` | Linear down (`1-x`) |
| 6 | `ExpUp` | Exponential up (`x^2`) |
| 7 | `ExpDown` | Exponential down (`(1-x)^2`) |
| 8 | `LogUp` | Log-like up (`sqrt(x)`) |
| 9 | `LogDown` | Log-like down (`sqrt(1-x)`) |
| 10 | `SmoothUp` | Smoothstep up |
| 11 | `SmoothDown` | Smoothstep down |
| 12 | `Triangle` | Triangle shape |
| 13 | `Bell` | Cosine bell |
| 14 | `ExpDown2x` | 2 repeated exp-down segments per step |
| 15 | `ExpDown3x` | 3 repeated exp-down segments per step |
| 16 | `ExpDown4x` | 4 repeated exp-down segments per step |

Legacy compatibility note:
- For project versions `< v14`, the reader remaps old `Low`/`High` IDs during load.

## 7) Packed Step Format

A `CurveSequence::Step` serializes as:
- `_data0`: 32-bit word
- `_data1`: 16-bit word

Total step size: 6 bytes (v15+ format).

Legacy read path (`CurveSequence::Step::read()`):
- pre-v15 files only had three bytes: `shape`, `min`, `max`
- pre-v14 compatibility fix swaps shape IDs for old Low/High mapping

## 8) Runtime Behavior (Engine)

From `CurveTrackEngine`:

### Step trigger and gates
- A step is selected with sequence rotation (`rotate`), first/last bounds, and run mode.
- `Gate` layer is interpreted as a 4-bit pattern over one step.
- For each of 4 sub-slots (`i=0..3`), if bit is set and probability test passes:
  - gate start at `(divisor * i) / 4`
  - gate length fixed to `divisor / 8`

So gates are quarter-step placed and eighth-step long by design.

### Shape generation
- `shapeVariation` can replace `shape` probabilistically each step.
- Optional fill transforms:
  - `Variation`: forces variation behavior
  - `NextPattern`: evaluates shape from next pattern
  - `Invert`: inverts curve value (`1 - value`)

### CV output path
- Step fraction = `(relativeTick % divisor) / divisor`
- Curve function output is mapped into `[min..max]` (normalized), then denormalized into selected voltage range.
- Track `offset` is added in volts.
- `slideTime > 0` applies slew (`Slide::applySlide`), else immediate value.

### Mute behavior
When muted, `muteMode` controls CV target:
- `LastValue`: keep previous value
- `Zero`: force 0V
- `Min`: force range low value
- `Max`: force range high value

## 9) Coarse Rasterization Notes

Several Curve layers are intentionally coarse/quantized:
- `GateProbability`: 8 levels (`0..7`)
- `ShapeVariationProbability`: 9 effective levels (`0..8`)
- `Gate` pattern: 4 sub-slots only per step
- `Min`/`Max`: 8-bit quantization (`0..255`)

This keeps packed step size compact and deterministic.

## 10) Version Availability Summary (CurveTrack/Layers)

| Version | Added/Changed |
|---|---|
| v8 | `CurveTrack::slideTime` |
| v10 | `CurveSequence::divisor` expanded to 16-bit |
| v14 | Legacy shape ID migration (Low/High swap fix in read path) |
| v15 | New Curve step format (`shapeVariation`, `shapeVariationProbability`, `gate`, `gateProbability`) + track biases |
| v22 | `CurveTrack::muteMode` |
| v28 | `CurveTrack::offset` |

