# Project File Format

This document precisely describes the binary format of a PERFORMER NX project file as written and read by the firmware and simulator.

**Source of truth:** `src/apps/sequencer/model/` — in particular `Project.cpp`, `ProjectVersion.h`, and all referenced model `.cpp` files.

---

## Table of Contents

- [Overview](#overview)
- [File Layout](#file-layout)
- [FileHeader](#fileheader)
- [Project Body](#project-body)
  - [Scalar fields](#scalar-fields)
  - [ClockSetup](#clocksetup)
  - [Tracks (×8)](#tracks-8)
    - [NoteTrack](#notetrack)
      - [NoteSequence (×16)](#notesequence-16)
        - [NoteSequence Step (×64)](#notesequence-step-64)
    - [CurveTrack](#curvetrack)
      - [CurveSequence (×16)](#curvesequence-16)
        - [CurveSequence Step (×64)](#curvesequence-step-64)
    - [MidiCvTrack](#midicvtrack)
      - [Arpeggiator](#arpeggiator)
  - [CV Output Track mapping (×8)](#cv-output-track-mapping-8)
  - [Gate Output Track mapping (×8)](#gate-output-track-mapping-8)
  - [Song](#song)
    - [Song Slot (×64)](#song-slot-64)
  - [PlayState](#playstate)
    - [TrackState (×8)](#trackstate-8)
  - [Routing](#routing)
    - [Route (×16)](#route-16)
  - [MidiOutput](#midioutput)
    - [Output (×16)](#output-16)
  - [UserScales (×4)](#userscales-4)
  - [Selection state](#selection-state)
  - [Hash](#hash)
- [Serialization Mechanics](#serialization-mechanics)
- [Version History](#version-history)
- [Constants Reference](#constants-reference)

---

## Overview

Project files are stored on the SD card. The file format is a flat binary stream produced by the `VersionedSerializedWriter` / `VersionedSerializedReader` classes in `src/core/io/`.

All multi-byte integers are written in **little-endian** byte order. Floating-point values are written as 32-bit IEEE 754 little-endian unless noted otherwise.

A project file consists of:

1. A **`FileHeader`** (10 bytes, packed)
2. The **project body** (variable-length binary stream)
3. A trailing **CRC-32 hash** (4 bytes)

Integrity is verified by `reader.checkHash()` at the end of `Project::read()`. If the hash does not match the entire project is discarded and re-initialized to defaults.

---

## File Layout

```
Offset   Size   Description
------   ----   -----------
0        1      FileHeader::type      (uint8_t, FileType::Project = 0)
1        1      FileHeader::version   (uint8_t, ProjectVersion::Latest)
2        8      FileHeader::name      (char[8], project name, zero-padded)
10       ...    Project body (see below)
...      4      CRC-32 hash (written by writeHash / checked by checkHash)
```

---

## FileHeader

Defined in `src/apps/sequencer/model/FileDefs.h`.

```
Field    Type      Size   Description
-----    ----      ----   -----------
type     uint8_t   1      FileType::Project = 0
version  uint8_t   1      ProjectVersion::Latest (= 31 as of 2026-04)
name     char[8]   8      Up to 8 chars, zero-padded, NOT null-terminated within the 8 bytes
```

Total: **10 bytes**, `__attribute__((packed))`.

---

## Project Body

Written by `Project::write()` in `src/apps/sequencer/model/Project.cpp`.
All fields are written sequentially with no padding between them.

### Scalar fields

```
Field             Type       Size   Since   Default   Notes
-----             ----       ----   -----   -------   -----
name              char[9]    9      v5      "INIT"    NameLength+1 = 9 bytes (8 chars + NUL)
tempo.base        float      4      —       120.0     BPM, range 1–1000
swing.base        uint8_t    1      —       50        Percent, range 50–75
timeSignature     2 bytes    2      v18     4/4       beats (uint8_t) then note (uint8_t)
syncMeasure       uint8_t    1      —       1         Range 1–128
scale             uint8_t    1      —       0         Index into built-in scale table
rootNote          uint8_t    1      —       0         0–11 (C=0)
monitorMode       uint8_t    1      v30     Always    Types::MonitorMode enum
recordMode        uint8_t    1      —       Overdub   Types::RecordMode enum
midiInputMode     uint8_t    1      v29     All       Types::MidiInputMode enum
midiInputSource   2 bytes    2      v29     Midi/All  MidiSourceConfig (port uint8_t + channel int8_t)
cvGateInput       uint8_t    1      v6      Off       Types::CvGateInput enum
curveCvInput      uint8_t    1      v11     Off       Types::CurveCvInput enum
```

**TimeSignature detail:** `beats` is a uint8_t (range 1–`note*2`), `note` is a uint8_t constrained to `{1,2,3,4,6,8}`.

**MidiSourceConfig detail:** two bytes — `port` (uint8_t, `Types::MidiPort` enum) followed by `channel` (int8_t, -1 = Omni, 0–15 = specific channel).

---

### ClockSetup

Written by `ClockSetup::write()` in `ClockSetup.cpp`.

```
Field               Type      Size   Since   Default   Notes
-----               ----      ----   -----   -------   -----
mode                uint8_t   1      —       Auto      ClockSetup::Mode enum (Auto=0, Master=1, Slave=2)
shiftMode           uint8_t   1      —       Restart   ClockSetup::ShiftMode enum (Restart=0, Pause=1)
clockInputDivisor   uint8_t   1      —       12        Range 1–192; musical subdivision
clockInputMode      uint8_t   1      —       Reset     ClockSetup::ClockInputMode (Reset=0, Run=1, StartStop=2)
clockOutputDivisor  uint8_t   1      —       12        Range 2–192
clockOutputSwing    bool      1      v11     false
clockOutputPulse    uint8_t   1      —       1         Range 1–20 (ms)
clockOutputMode     uint8_t   1      —       Reset     ClockSetup::ClockOutputMode (Reset=0, Run=1)
midiRx              bool      1      —       true
midiTx              bool      1      —       true
usbRx               bool      1      —       false
usbTx               bool      1      —       false
```

Total: **12 bytes**.

---

### Tracks (×8)

Written by `Track::write()` in `Track.cpp`. Repeated 8 times (`CONFIG_TRACK_COUNT = 8`).

```
Field      Type      Size   Since   Notes
-----      ----      ----   -----   -----
trackMode  uint8_t   1      —       Serialized value: Note=0, Curve=1, MidiCv=2
linkTrack  int8_t    1      —       -1 = none, 0–(trackIndex-1) = linked track
<body>     variable  ...    —       Content depends on trackMode (see below)
```

---

#### NoteTrack

Written by `NoteTrack::write()`.

```
Field                         Type      Size   Since   Default   Notes
-----                         ----      ----   -----   -------   -----
playMode                      uint8_t   1      —       Aligned   Types::PlayMode enum
fillMode                      uint8_t   1      —       Gates     NoteTrack::FillMode (None=0,Gates=1,NextPattern=2,Condition=3)
fillMuted                     bool      1      v26     true
cvUpdateMode                  uint8_t   1      v4      Gate      NoteTrack::CvUpdateMode (Gate=0, Always=1)
slideTime.base                uint8_t   1      —       50        Range 0–100 (%)
octave.base                   int8_t    1      —       0         Range -10–+10
transpose.base                int8_t    1      —       0         Range -100–+100 semitones
rotate.base                   int8_t    1      —       0         Range -64–+64
gateProbabilityBias.base      int8_t    1      —       0         Range -8–+8
retriggerProbabilityBias.base int8_t    1      —       0         Range -8–+8
lengthBias.base               int8_t    1      —       0         Range -8–+8
noteProbabilityBias.base      int8_t    1      —       0         Range -8–+8
sequences[]                   ...       ...    —                 16 NoteSequences (see below)
```

---

#### NoteSequence (×16)

Written by `NoteSequence::write()`. Repeated `CONFIG_PATTERN_COUNT = 16` times (plus 1 snapshot slot, but only the 16 regular patterns are written).

```
Field          Type       Size   Since   Default   Notes
-----          ----       ----   -----   -------   -----
scale.base     int8_t     1      —       -1        -1 = use project default
rootNote.base  int8_t     1      —       -1        -1 = use project default
divisor.base   uint16_t   2      —       12        uint8_t before v10; expanded to uint16_t in v10
resetMeasure   uint8_t    1      —       0         0 = Off, 1–128 bars
runMode.base   uint8_t    1      —       Forward   Types::RunMode enum
firstStep.base uint8_t    1      —       0         0-based index (displayed as 1–64)
lastStep.base  uint8_t    1      —       15        0-based index (displayed as 1–64)
steps[]        ...        ...    —                 64 Steps (see below)
```

---

#### NoteSequence Step (×64)

Written by `NoteSequence::Step::write()`. Each step is **8 bytes** (two `uint32_t` words).
Since v27 both words are written as full 32-bit values. Before v27 `_data1` was 16 bits.

**`_data0` — 32 bits (little-endian uint32_t, since —)**

```
Bits   Field                      Since   Width   Range / Notes
----   -----                      -----   -----   -------------
 0     gate                       —       1       0 = off, 1 = on
 1     slide                      —       1       0 = off, 1 = on
 2–4   gateProbability            —       3       0–7 (7 = 100%)
 5–7   length                     —       3       0–7 (4 = 50%)
 8–11  lengthVariationRange       —       4       signed, stored as offset from Min (-8)
12–14  lengthVariationProbability —       3       0–7
15–21  note                       —       7       signed, stored as offset from Min (-64); scale index
22–28  noteVariationRange         —       7       signed, stored as offset from Min (-64)
29–31  noteVariationProbability   —       3       0–7
```

**`_data1` — 16 bits before v27, 32 bits since v27 (little-endian)**

```
Bits   Field                Since   Width   Range / Notes
----   -----                -----   -----   -------------
 0–1   retrigger            —       2       0–3
 2–4   retriggerProbability —       3       0–7
 5–8   gateOffset           v7      4       signed offset from Min; 0 = no offset
 9–15  condition            v12     7       Types::Condition enum (0=Off); 7 bits since v27
16–31  (unused)             v27     16
```

---

#### CurveTrack

Written by `CurveTrack::write()`.

```
Field                     Type      Size   Since   Default    Notes
-----                     ----      ----   -----   -------    -----
playMode                  uint8_t   1      —       Aligned    Types::PlayMode enum
fillMode                  uint8_t   1      —       None       CurveTrack::FillMode (None=0,Variation=1,NextPattern=2,Invert=3)
muteMode                  uint8_t   1      v22     LastValue  CurveTrack::MuteMode (LastValue=0,Zero=1,Min=2,Max=3)
slideTime.base            uint8_t   1      v8      0          Range 0–100 (%)
offset.base               int16_t   2      v28     0          Range -500–+500 (x0.01V = -5V–+5V)
rotate.base               int8_t    1      —       0          Range -64–+64
shapeProbabilityBias.base int8_t    1      v15     0          Range -8–+8
gateProbabilityBias.base  int8_t    1      v15     0          Range -8–+8
sequences[]               ...       ...    —                  16 CurveSequences (see below)
```

---

#### CurveSequence (×16)

Written by `CurveSequence::write()`.

```
Field          Type       Size   Since   Default    Notes
-----          ----       ----   -----   -------    -----
range          uint8_t    1      —       Bipolar5V  Types::VoltageRange enum
divisor.base   uint16_t   2      —       12         uint8_t before v10; expanded to uint16_t in v10
resetMeasure   uint8_t    1      —       0
runMode.base   uint8_t    1      —       Forward
firstStep.base uint8_t    1      —       0
lastStep.base  uint8_t    1      —       15
steps[]        ...        ...    —                  64 Steps (see below)
```

---

#### CurveSequence Step (×64)

Written by `CurveSequence::Step::write()`. Each step is **6 bytes** (`uint32_t` + `uint16_t`).
Since v15 the full packed layout is written. Before v15 only three `uint8_t` fields (shape, min, max) were stored.

**`_data0` — 32 bits (little-endian uint32_t, since v15)**

```
Bits   Field                      Since   Width   Range / Notes
----   -----                      -----   -----   -------------
 0–5   shape                      —       6       0–(Curve::Last-1); curve type index
 6–11  shapeVariation             v15     6       0–(Curve::Last-1)
12–15  shapeVariationProbability  v15     4       0–8
16–23  min                        —       8       0–255 (normalized to output range)
24–31  max                        —       8       0–255 (normalized to output range)
```

**`_data1` — 16 bits (little-endian uint16_t, since v15)**

```
Bits   Field           Since   Width   Range / Notes
----   -----           -----   -----   -------------
 0–3   gate            v15     4       0–15 (gate pattern; 0 = off)
 4–6   gateProbability v15     3       0–7
 7–15  (unused)        v15     9
```

---

#### MidiCvTrack

Written by `MidiCvTrack::write()`.

```
Field           Type      Size   Since   Default      Notes
-----           ----      ----   -----   -------      -----
source          2 bytes   2      —       Midi/Omni    MidiSourceConfig (port uint8_t + channel int8_t)
voices          uint8_t   1      —       1            Range 1–8
voiceConfig     uint8_t   1      —       Pitch        MidiCvTrack::VoiceConfig; uint8_t since v31 (was uint32_t)
notePriority    uint8_t   1      v16     LastNote     MidiCvTrack::NotePriority enum
lowNote         uint8_t   1      v15     0            MIDI note 0–127
highNote        uint8_t   1      v15     127          MIDI note 0–127
pitchBendRange  uint8_t   1      —       2            Semitones 0–48
modulationRange uint8_t   1      —       Unipolar5V   Types::VoltageRange enum
retrigger       bool      1      —       false
slideTime.base  uint8_t   1      v20     0            Range 0–100 (%)
transpose.base  int8_t    1      v21     0            Range -100–+100
arpeggiator     ...       ...    v9                   See Arpeggiator below
```

---

#### Arpeggiator

Written by `Arpeggiator::write()`.

```
Field      Type       Size   Since   Default    Notes
-----      ----       ----   -----   -------    -----
enabled    bool       1      v9      false
hold       bool       1      v9      false
mode       uint8_t    1      v9      PlayOrder  Arpeggiator::Mode; serialized via modeSerialize()
divisor    uint16_t   2      v9      12         uint8_t before v10; expanded to uint16_t in v10
gateLength uint8_t    1      v9      50         Range 0–100 (%)
octaves    int8_t     1      v9      0          uint8_t before v17 (stored as value+1 pre-v17)
```

---

### CV Output Track mapping (×8)

Written as a plain `uint8_t` array of 8 entries (`CONFIG_CHANNEL_COUNT = 8`).
Each byte is a track index (0–7) indicating which track drives that CV output.
Default: identity mapping (output N → track N).

---

### Gate Output Track mapping (×8)

Same structure as CV output mapping. Each byte maps a gate output to a track index.

---

### Song

Written by `Song::write()`.

#### Song Slot (×64)

Repeated `CONFIG_SONG_SLOT_COUNT = 64` times (since v19; was 16 before v19).

```
Field     Type       Size   Since   Default   Notes
-----     ----       ----   -----   -------   -----
patterns  uint32_t   4      —       0         Packed: bits [4n…4n+3] = pattern index for track n (0–15)
mutes     uint8_t    1      v25     0         Bitmask: bit n = mute state of track n
repeats   uint8_t    1      —       1         Range 1–128
```

**After the 64 slots:**

```
Field      Type      Size   Notes
-----      ----      ----   -----
slotCount  uint8_t   1      Number of active song slots (0–64)
```

---

### PlayState

Written by `PlayState::write()`. Contains 8 `TrackState` structs.

#### TrackState (×8)

```
Field       Type      Size   Since   Default   Notes
-----       ----      ----   -----   -------   -----
mute        uint8_t   1      —       0         Mute flag (written as uint8_t, 0 or 1)
pattern     uint8_t   1      —       0         Active pattern index 0–15 (snapshot index NOT saved)
fillAmount  uint8_t   1      v12     100       Range 0–100 (%)
```

---

### Routing

Written by `Routing::write()`. Contains 16 `Route` structs (`CONFIG_ROUTE_COUNT = 16`).

#### Route (×16)

```
Field    Type      Size   Since   Default   Notes
-----    ----      ----   -----   -------   -----
target   uint8_t   1      —       None(0)   Serialized via targetSerialize() (see table below)
tracks   int8_t    1      —       0         Bitmask: bit n = route applies to track n (per-track targets only)
min      float     4      —       0.0       Normalized 0.0–1.0
max      float     4      —       1.0       Normalized 0.0–1.0
source   uint8_t   1      —       None(0)   Routing::Source enum
```

If `source` is a CV source (CvIn1–CvOut8):
```
range    uint8_t   1      —                 Types::VoltageRange enum
```

If `source` is Midi:
```
midiSource.port               uint8_t   1   —    MidiSourceConfig port
midiSource.channel            int8_t    1   —    MidiSourceConfig channel (-1=Omni)
midiSource.event              uint8_t   1   —    Routing::MidiSource::Event enum
midiSource.controlNumberOrNote uint8_t  1   —    CC number or note number 0–127
midiSource.noteRange          uint8_t   1   v13  Range 2–64
```

**Routing Target serialized values:**

| Serialized | Target |
|---|---|
| 0 | None |
| 1 | Play |
| 2 | Record |
| 3 | Tempo |
| 4 | Swing |
| 5 | SlideTime |
| 6 | Octave |
| 7 | Transpose |
| 8 | Rotate |
| 9 | GateProbabilityBias |
| 10 | RetriggerProbabilityBias |
| 11 | LengthBias |
| 12 | NoteProbabilityBias |
| 13 | Divisor |
| 14 | RunMode |
| 15 | FirstStep |
| 16 | LastStep |
| 17 | Mute |
| 18 | Fill |
| 19 | FillAmount |
| 20 | Pattern |
| 21 | TapTempo |
| 22 | ShapeProbabilityBias |
| 23 | Scale |
| 24 | RootNote |
| 25 | Offset |
| 26 | PlayToggle |
| 27 | RecordToggle |

---

### MidiOutput

Written by `MidiOutput::write()`. Contains 16 `Output` structs (`CONFIG_MIDI_OUTPUT_COUNT = 16`; expanded from 8 in v24).

#### Output (×16)

```
Field   Type      Size   Since   Notes
-----   ----      ----   -----   -----
target  2 bytes   2      —       MidiTargetConfig: port (uint8_t) + channel (int8_t, 0–15)
event   uint8_t   1      —       MidiOutput::Output::Event: None=0, Note=1, ControlChange=2
```

If `event == Note`:
```
gateSource     uint8_t   1   —   GateSource enum (0–7 = track index)
noteSource     uint8_t   1   —   NoteSource enum (0–7 = track; 8–135 = fixed note 0–127)
velocitySource uint8_t   1   —   VelocitySource enum (0–7 = track; 8–135 = fixed velocity 0–127)
```

If `event == ControlChange`:
```
controlNumber  uint8_t   1   —   CC number 0–127
controlSource  uint8_t   1   —   ControlSource enum (0–7 = track index)
```

---

### UserScales (×4)

Written by `writeArray(writer, UserScale::userScales)` — calls `UserScale::write()` for each.
`CONFIG_USER_SCALE_COUNT = 4` scales are stored.

**Each UserScale is an independently hashed sub-record (stored since v5):**

```
Field    Type       Size     Since   Default    Notes
-----    ----       ----     -----   -------    -----
name     char[9]    9        v5      "INIT"     NameLength+1 bytes
mode     uint8_t    1        v5      Chromatic  UserScale::Mode (Chromatic=0, Voltage=1)
size     uint8_t    1        v5      1          Number of active items (1–32)
items[]  int16_t    2xsize   v5                 size entries; Chromatic: 0–11; Voltage: -5000–5000 (mV)
hash     uint32_t   4        v5                 CRC-32 of above fields
```

Note: `items` is a variable-length array — only `size` entries are written, not the full 32.

---

### Selection state

Written at the very end of the project body before the hash:

```
Field                 Type      Size   Since   Default   Notes
-----                 ----      ----   -----   -------   -----
selectedTrackIndex    int       4      —       0         sizeof(int) = 4 on all supported targets
selectedPatternIndex  int       4      —       0         sizeof(int) = 4 on all supported targets
```

> **Implementation note:** `writer.write(_selectedTrackIndex)` / `writer.write(_selectedPatternIndex)` use the template `write<T>` which writes `sizeof(int)` bytes. On all supported targets (ARM Cortex-M and x86-64 simulation) `int` is 4 bytes.

---

### Hash

```
Field   Type       Size   Notes
-----   ----       ----   -----
hash    uint32_t   4      CRC-32 of all bytes written since the last writeHash call
```

Written by `writer.writeHash()`. Verified by `reader.checkHash()` in `Project::read()`.
If verification fails the reader returns `false` and the project is re-cleared to defaults.

---

## Serialization Mechanics

The serialization layer is in `src/core/io/VersionedSerializedWriter.h` / `VersionedSerializedReader.h`.

**Writer:**
- `write(T value)` — writes `sizeof(T)` bytes little-endian.
- `writeEnum(E value, Fn serialize)` — writes the result of `serialize(value)` as `uint8_t`.
- `writeHash()` — appends CRC-32 of all bytes written since construction (or previous `writeHash`).

**Reader:**
- `read(T &value)` — reads unconditionally.
- `read(T &value, int minVersion)` — reads only if `dataVersion() >= minVersion`, otherwise leaves `value` at its default.
- `readAs<U>(T &value)` — reads a `U` and casts to `T` (used when a field changed width between versions).
- `readEnum(E &value, Fn deserialize)` — reads a `uint8_t` and maps it via `deserialize`.
- `checkHash()` — reads 4 bytes from the stream and compares to the running CRC-32; returns `true` on match.
- `backupHash()` / `restoreHash()` — used in `NoteTrack::read()` to work around a historical bug (versions < 23 did not update the hash after writing NoteTrack scalar fields).

**Routable fields:**
Parameters that can be controlled by a routing source are stored as a `Routable<T>` struct:
```cpp
struct Routable<T> {
    T base;    // user-set value (always serialized)
    T routed;  // runtime-overridden value (NOT serialized)
};
```
Only the `.base` member is written/read. The `.routed` member is a runtime-only value and is never stored.

---

## Version History

Current latest version: **31** (`ProjectVersion::Latest`).

| Version | Changes |
|---|---|
| 4 | Added `NoteTrack::cvUpdateMode` |
| 5 | Added storing user scales with project; added `Project::name`; added `UserScale::name` |
| 6 | Added `Project::cvGateInput` |
| 7 | Added `NoteSequence::Step::gateOffset` |
| 8 | Added `CurveTrack::slideTime` |
| 9 | Added `MidiCvTrack::arpeggiator` |
| 10 | Expanded `divisor` fields from 8-bit to 16-bit |
| 11 | Added `ClockSetup::clockOutputSwing`; added `Project::curveCvInput` |
| 12 | Added `TrackState::fillAmount`; added `NoteSequence::Step::condition` |
| 13 | Added `Routing::MidiSource::Event::NoteRange` |
| 14 | Swapped `Curve::Type::Low` with `Curve::Type::High` |
| 15 | Added `MidiCvTrack::lowNote/highNote`; changed `CurveSequence::Step` layout; added `CurveTrack::shapeProbabilityBias/gateProbabilityBias` |
| 16 | Added `MidiCvTrack::notePriority` |
| 17 | Changed `Arpeggiator::octaves` (uint8_t → int8_t, offset by −1) |
| 18 | Added `Project::timeSignature`; expanded `Song::slots` read path |
| 19 | Expanded `Song::slots` from 16 to 64 entries |
| 20 | Added `MidiCvTrack::slideTime` |
| 21 | Added `MidiCvTrack::transpose` |
| 22 | Added `CurveTrack::muteMode` |
| 23 | Added routing targets `Scale` and `RootNote`; fixed NoteTrack hash backup/restore bug |
| 24 | Expanded `MidiOutput::outputs` from 8 to 16 entries |
| 25 | Added `Song::Slot::mutes` |
| 26 | Added `NoteTrack::fillMuted` |
| 27 | Expanded `NoteSequence::Step` to 64 bits; expanded `Condition` field to 7 bits |
| 28 | Added `CurveTrack::offset` |
| 29 | Added `Project::midiInput` (midiInputMode + midiInputSource) |
| 30 | Added `Project::monitorMode` |
| 31 | Changed `MidiCvTrack::VoiceConfig` from 32-bit to 8-bit storage |

---

## Constants Reference

From `src/apps/sequencer/Config.h`:

| Constant | Value | Description |
|---|---|---|
| `CONFIG_TRACK_COUNT` | 8 | Number of tracks per project |
| `CONFIG_PATTERN_COUNT` | 16 | Number of patterns per track |
| `CONFIG_SNAPSHOT_COUNT` | 1 | Snapshot slots (not serialized as regular patterns) |
| `CONFIG_STEP_COUNT` | 64 | Steps per sequence |
| `CONFIG_SONG_SLOT_COUNT` | 64 | Song slots |
| `CONFIG_ROUTE_COUNT` | 16 | Routing slots |
| `CONFIG_MIDI_OUTPUT_COUNT` | 16 | MIDI output slots |
| `CONFIG_USER_SCALE_COUNT` | 4 | User scales stored per project |
| `CONFIG_USER_SCALE_SIZE` | 32 | Maximum items per user scale |
| `CONFIG_CHANNEL_COUNT` | 8 | CV/gate output channels |
| `CONFIG_PPQN` | 192 | Internal parts per quarter note |
| `CONFIG_SEQUENCE_PPQN` | 48 | Sequence resolution PPQN |
| `FileHeader::NameLength` | 8 | Name field length (bytes) |


















