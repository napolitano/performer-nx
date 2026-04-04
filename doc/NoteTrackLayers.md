# NoteTrack and Layer Reference

This document describes the `NoteTrack` model and all note-step layers in detail, including defaults, value ranges, serialization, and version availability.

Source of truth:
- `src/apps/sequencer/model/NoteTrack.h`
- `src/apps/sequencer/model/NoteTrack.cpp`
- `src/apps/sequencer/model/NoteSequence.h`
- `src/apps/sequencer/model/NoteSequence.cpp`
- `src/apps/sequencer/model/ProjectVersion.h`
- `src/apps/sequencer/engine/NoteTrackEngine.cpp`
- `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp`

## Scope

`NoteTrack` contains:
- track-level parameters (play/fill behavior, routing-aware biases, transpose etc.)
- per-pattern `NoteSequence` data
- per-step layer values (gate, probability, retrigger, note, condition, microtiming)

This doc is focused on model/runtime behavior of Note tracks.

## 1) NoteTrack Structure

`NoteTrack` stores:
- `playMode` (`Types::PlayMode`)
- `fillMode` (`None`, `Gates`, `NextPattern`, `Condition`)
- `fillMuted` (bool, added in v26)
- `cvUpdateMode` (`Gate`, `Always`, added in v4)
- routable track parameters:
  - `slideTime` (0..100)
  - `octave` (-10..10)
  - `transpose` (-100..100)
  - `rotate` (-64..64)
  - `gateProbabilityBias` (-8..8)
  - `retriggerProbabilityBias` (-8..8)
  - `lengthBias` (-8..8)
  - `noteProbabilityBias` (-8..8)
- sequence array: `CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT`

Serialization writes only `.base` for routable values.

## 2) NoteTrack Serialization Order

From `NoteTrack::write()`:

1. `playMode`
2. `fillMode`
3. `fillMuted`
4. `cvUpdateMode`
5. `slideTime.base`
6. `octave.base`
7. `transpose.base`
8. `rotate.base`
9. `gateProbabilityBias.base`
10. `retriggerProbabilityBias.base`
11. `lengthBias.base`
12. `noteProbabilityBias.base`
13. `sequences[]`

Versioned read behavior (`NoteTrack::read()`):
- `fillMuted` available since v26
- `cvUpdateMode` available since v4
- hash workaround for dataVersion < v23 (`backupHash`/`restoreHash`)

## 3) Sequence-Level Parameters (NoteSequence)

Each `NoteSequence` stores:
- `scale` (-1..Scale::Count-1, -1 means project default)
- `rootNote` (-1..11, -1 means project default)
- `divisor` (uint16_t; uint8_t before v10)
- `resetMeasure` (0..128)
- `runMode`
- `firstStep` / `lastStep`
- `steps[CONFIG_STEP_COUNT]`

## 4) Step Layers: Overview

Layer enum (`NoteSequence::Layer`):
- `Gate`
- `GateProbability`
- `GateOffset` (microtiming / gate delay)
- `Slide`
- `Retrigger`
- `RetriggerProbability`
- `Length`
- `LengthVariationRange`
- `LengthVariationProbability`
- `Note`
- `NoteVariationRange`
- `NoteVariationProbability`
- `Condition`

Layer ranges are defined in `NoteSequence::layerRange()`.

## 5) Layer Matrix (Range, Default, Since)

Defaults come from `NoteSequence::Step::clear()`.

| Layer | Effective range in UI/model | Default | Since | Stored bits |
|---|---|---:|---|---|
| `Gate` | 0..1 | 0 | legacy | `_data0` bit 0 |
| `GateProbability` | 0..7 | 7 | legacy | `_data0` bits 2..4 |
| `GateOffset` | 0..7 (implemented; 8 coarse steps) | 0 | v7 | `_data1` bits 5..8 |
| `Slide` | 0..1 | 0 | legacy | `_data0` bit 1 |
| `Retrigger` | 0..3 | 0 | legacy | `_data1` bits 0..1 |
| `RetriggerProbability` | 0..7 | 7 | legacy | `_data1` bits 2..4 |
| `Length` | 0..7 | 4 | legacy | `_data0` bits 5..7 |
| `LengthVariationRange` | -8..7 | 0 | legacy | `_data0` bits 8..11 |
| `LengthVariationProbability` | 0..7 | 7 | legacy | `_data0` bits 12..14 |
| `Note` | -64..63 | 0 | legacy | `_data0` bits 15..21 |
| `NoteVariationRange` | -64..63 | 0 | legacy | `_data0` bits 22..28 |
| `NoteVariationProbability` | 0..7 | 7 | legacy | `_data0` bits 29..31 |
| `Condition` | enum value (0..127 storable) | Off (0) | v12 (7-bit since v27) | `_data1` bits 9..15 |

Note on `Condition`:
- introduced in v12
- storage expanded with v27 step-format migration (`_data1` became 32-bit; condition is now 7-bit)

## 6) Packed Step Format

A step serializes as:
- v27+: 8 bytes (`uint32_t _data0` + `uint32_t _data1`)
- pre-v27: `_data0` 32-bit + `_data1` read as 16-bit

Migration behavior in `Step::read()`:
- pre-v5: `_data1` masked (`_data1.raw &= 0x1f`)
- pre-v7: `gateOffset` forced to `0`
- pre-v12: `condition` forced to `Off`

## 7) MicroTiming (GateOffset) Status and Limitation

In UX terms, microtiming corresponds to `Layer::GateOffset` (`TXT_MODEL_NOTE_GATE_OFFSET`, shown as percent in the editor).

### Current behavior (corrected)

Only positive gate delay is currently implemented.

- Effective value range in runtime/UI is `0..7`.
- Negative offsets are currently not reachable, even though the declared value type is signed.

### Why negative values are not active yet

Evidence from implementation:

1. Declared type is signed (`SignedValue<4>`), so the theoretical domain is `-8..+7`.
2. `setGateOffset()` explicitly clamps to non-negative values:
   - `std::max(0, GateOffset::clamp(gateOffset))`
3. `layerRange(Layer::GateOffset)` returns `0..GateOffset::Max`.
4. `NoteTrackEngine` treats offset as unsigned delay and schedules at `tick + gateOffset`:
   - `uint32_t gateOffset = (divisor * step.gateOffset()) / (GateOffset::Max + 1)`
   - gate/CV events are enqueued at `tick + gateOffset`
5. Code contains TODO note in model:
   - `// TODO: allow negative gate delay in the future`

- The data type already keeps headroom for future negative timing.
- The current model + scheduler path is still "delay-only" (post-step) and therefore clamps to `>= 0`.
- Supporting negative microtiming would require pre-step event scheduling semantics, not just a wider numeric type.

### Why gate offset is currently coarse

`GateOffset` uses only 8 implemented levels (`0..7`), then converts to ticks via:

`gateOffsetTicks = (divisor * gateOffset) / 8`

Implications:
- Resolution is quantized to 1/8 of the active step length (coarse by design).
- Integer arithmetic causes truncation, so timing increments are not always perfectly even in tick space.
- Example with `divisor = 12`: possible tick delays are `{0,1,3,4,6,7,9,10}`.

So the current implementation provides positive, quantized micro-delay rather than fine bipolar microtiming.

## 8) Practical Layer Notes

- `GateProbability`, `RetriggerProbability`, `LengthVariationProbability`, `NoteVariationProbability` use 3-bit quantization (8 discrete states).
- `Length` default is centered (`Length::Max / 2`), not min.
- `CvUpdateMode::Always` causes CV updates even when gate does not fire.
- `FillMode::Condition` re-evaluates condition context during fill usage.

## 9) Version Availability Summary (NoteTrack/Layers)

| Version | Added/Changed |
|---|---|
| v4 | `NoteTrack::cvUpdateMode` |
| v7 | `NoteSequence::Step::gateOffset` (microtiming delay) |
| v12 | `NoteSequence::Step::condition` |
| v26 | `NoteTrack::fillMuted` |
| v27 | Step storage expanded to 64-bit total (`_data0+_data1`), `condition` widened to 7 bits |

