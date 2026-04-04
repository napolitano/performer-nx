# Song Model and Serialization Reference

This document describes the `Song` model used by the sequencer project, including slot structure, binary serialization, version compatibility, and runtime song-playback semantics.

Source of truth:
- `src/apps/sequencer/model/Song.h`
- `src/apps/sequencer/model/Song.cpp`
- `src/apps/sequencer/model/Project.cpp`
- `src/apps/sequencer/model/ProjectVersion.h`
- `src/apps/sequencer/engine/Engine.cpp`

## Scope

The `Song` object is part of project state and defines:
- per-slot pattern assignments per track
- per-slot mute masks
- per-slot repeat count
- total number of active song slots (`slotCount`)

This document focuses on model + engine behavior, not UI-only editing details.

## 1) Data Structure

`Song` contains:
- `_slots`: `std::array<Slot, CONFIG_SONG_SLOT_COUNT>`
- `_slotCount`: `uint8_t`

Each `Song::Slot` contains:
- `_patterns`: `uint32_t`
- `_mutes`: `uint8_t`
- `_repeats`: `uint8_t`

## 2) Slot Field Semantics

### 2.1 Patterns (`_patterns`)

Pattern assignment is packed in 4-bit nibbles:
- nibble index = track index (`0..7`)
- value range per nibble = `0..15` (pattern index)
- accessor:
  - `pattern(trackIndex) = (_patterns >> (trackIndex << 2)) & 0xf`

So one slot can assign a pattern to each of 8 tracks in one 32-bit word.

### 2.2 Mutes (`_mutes`)

Mute flags are stored as bitmask:
- bit `trackIndex` in `_mutes` corresponds to that track mute state
- accessor:
  - `mute(trackIndex) = (_mutes >> trackIndex) & 0x1`

### 2.3 Repeats (`_repeats`)

Repeat count per slot:
- clamped to `1..128`
- `repeats() const` returns `_repeats`

## 3) Defaults and Clear Behavior

`Song::Slot::clear()`:
- `_patterns = 0` (all tracks use pattern 0)
- `_mutes = 0` (no mutes)
- `_repeats = 1`

`Song::clear()`:
- clears all slots
- sets `_slotCount = 0`

## 4) Editing Semantics (Model)

Key operations in `Song`:
- `chainPattern(pattern)`:
  - if last active slot already contains this pattern on all tracks, increment repeats
  - otherwise append new slot with that pattern on all tracks
- `insertSlot(index)` / `removeSlot(index)` / `duplicateSlot(index)` / `swapSlot(from,to)`
- per-slot edits:
  - `setPattern(slot, pattern)` (all tracks)
  - `setPattern(slot, track, pattern)` (single track)
  - `setMute(slot, track, mute)` / `toggleMute(slot, track)`
  - `setRepeats(slot, repeats)`

Validity helpers:
- `isFull()` checks against `CONFIG_SONG_SLOT_COUNT`
- `isActiveSlot(i)` requires `0 <= i < slotCount`

## 5) Project Serialization Placement

In project serialization (`Project::write`), song data is written after CV/Gate output track maps:

1. tracks
2. cv output track mapping
3. gate output track mapping
4. `song.write(writer)`
5. play state
6. routing
7. midi output

On load (`Project::read`), same order is used.

## 6) Song Binary Format (Inside Project Payload)

`Song::write()` serializes:
1. all `_slots` (always full array size)
2. `_slotCount`

`Song::Slot::write()` serializes each slot as:
1. `_patterns` (`uint32_t`, 4 bytes)
2. `_mutes` (`uint8_t`, 1 byte)
3. `_repeats` (`uint8_t`, 1 byte)

Slot size in current format: **6 bytes**.

With `CONFIG_SONG_SLOT_COUNT = 64`:
- slots block size = `64 * 6 = 384 bytes`
- plus `slotCount` (`1 byte`)
- total serialized `Song` size = **385 bytes**

## 7) Version Compatibility

Relevant project versions:
- `v19`: expanded song slot count from 16 to 64
- `v25`: added per-slot mutes (`_mutes`)

Read behavior (`Song::read` and `Song::Slot::read`):
- if `dataVersion < v18`: read only first 16 slots
- else: read full slot array (64 in current config)
- `_mutes` is read only if `dataVersion >= v25`
- `_repeats` is always read
- `_slotCount` is read after slot array

Implications:
- old projects load with fewer historical slots where applicable
- pre-v25 projects get default mute values (`0`) per slot

## 8) Runtime Playback Semantics (Engine)

Song playback is handled in `Engine::updatePlayState`.

### 8.1 Activating a song slot

When activating a slot:
- pattern for each track is set from slot nibbles
- mute for each track is applied only if that track has any mutes in the song (`song.trackHasMutes(trackIndex)`)

This means:
- if a track has no song mutes anywhere, existing mute state is preserved

### 8.2 Start/Stop requests

When a valid song play request is processed:
- slot is activated
- `currentSlot = requestedSlot`
- `currentRepeat = 0`
- song playing state set true
- clock auto-starts if not running

If pattern changes occur outside song flow or a song stop request appears:
- song playing state is set false

### 8.3 Advance logic

On each measure boundary while song is playing:
- repeat increments until `repeats` reached
- then advances to next slot
- after last active slot, wraps to slot 0
- on slot change, patterns/mutes are applied and all track engines restart

### 8.4 Abort safety

Song mode is stopped if runtime state becomes invalid, for example:
- `currentSlot >= slotCount`
- `currentRepeat >= repeats(currentSlot)`

## 9) Practical Notes

- `slotCount` defines active song length; serialized slots beyond that count are inactive data.
- `_patterns` nibble packing is fixed to 8 tracks x 4 bits.
- Since `CONFIG_PATTERN_COUNT` is 16, 4-bit storage is exact fit.
- `repeats` minimum is 1, so active slot always plays at least one measure cycle.

