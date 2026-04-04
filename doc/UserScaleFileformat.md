# User Scale File Format

This document describes the standalone User Scale file format used by PERFORMER NX.

Source of truth:
- `src/apps/sequencer/model/FileDefs.h`
- `src/apps/sequencer/model/UserScale.h`
- `src/apps/sequencer/model/UserScale.cpp`
- `src/apps/sequencer/model/FileManager.cpp`
- `src/core/io/VersionedSerializedWriter.h`
- `src/core/io/VersionedSerializedReader.h`
- `src/core/hash/FnvHash.h`

## Overview

A User Scale file stores exactly one `UserScale` object:
1. packed `FileHeader` (10 bytes)
2. versioned payload (starts with 32-bit payload version)
3. 32-bit FNV-1a hash of payload bytes

Important: this is **not** CRC-32. The checksum uses `FnvHash` (32-bit FNV-1a style update).

## File Type

From `FileDefs.h`:

```cpp
enum class FileType : uint8_t {
    Project   = 0,
    UserScale = 1,
    Settings  = 255
};
```

For this format:
- `FileHeader.type` must be `FileType::UserScale` (`1`)

## Header Layout

`FileHeader` is packed (`__attribute__((packed))`):

| Offset | Size | Type      | Field     | Notes |
|---|---:|---|---|---|
| 0  | 1 | uint8_t | `type`    | `1` for UserScale |
| 1  | 1 | uint8_t | `version` | currently written as `0` by `FileManager::writeUserScale` |
| 2  | 8 | char[8] | `name`    | zero-padded, may be non-null-terminated inside the 8 bytes |

Total header size: **10 bytes**.

## Payload Layout

Payload is written by `UserScale::write(VersionedSerializedWriter &writer)`.

### 1) Payload version (auto-written by writer)

`VersionedSerializedWriter` writes `writerVersion` first as `uint32_t`.
For UserScale files this value is `ProjectVersion::Latest` (currently 31).

| Relative offset (payload) | Size | Type     | Field |
|---|---:|---|---|
| 0 | 4 | uint32_t | `dataVersion` |

### 2) UserScale body

Then these fields are written in order:

| Field | Size | Type | Since | Notes |
|---|---:|---|---|---|
| `name` | 9 | char[9] | v5 | `NameLength + 1` bytes; absent in pre-v5 payloads |
| `mode` | 1 | uint8_t enum | pre-v4 | `Chromatic=0`, `Voltage=1` |
| `size` | 1 | uint8_t | pre-v4 | number of active entries |
| `items[i]` | `2 * size` | int16_t[] | pre-v4 | written only for `i=0..size-1` |

Finally:

| Field | Size | Type | Notes |
|---|---:|---|---|
| `hash` | 4 | uint32_t | FNV hash of all payload bytes from `dataVersion` through last `items` byte |

## Full Binary Layout (single file)

| Offset | Size | Content |
|---|---:|---|
| 0  | 10 | `FileHeader` |
| 10 | 4  | payload `dataVersion` |
| 14 | 9  | scale name (`char[9]`) |
| 23 | 1  | mode |
| 24 | 1  | size |
| 25 | `2 * size` | items (`int16_t` LE) |
| `25 + 2*size` | 4 | FNV hash |

Total file size = `29 + 2*size` bytes.

## Read Behavior and Compatibility

Reading is done via `FileManager::readUserScale` + `UserScale::read`:
- Reads `FileHeader` first (currently not strictly validated there)
- Creates `VersionedSerializedReader`, which reads payload `dataVersion` (`uint32_t`)
- Reads fields with version guards:
  - `name` only if `dataVersion >= ProjectVersion::Version5`
  - `mode`, `size`, and `items` are always attempted
- Verifies hash using `reader.checkHash()`
- On hash mismatch: `UserScale::clear()` and return failure
- `FileManager` maps this to `fs::INVALID_CHECKSUM` if file I/O itself was otherwise OK

Backward compatibility note:
- Pre-v5 scales cannot contain `name`; loader keeps default name after `clear()`.
- `mode`, `size`, and `items` are part of the legacy format and are read for all versions.

## Version Availability Matrix

The standalone UserScale file format is very stable. Relevant version differences:

| ProjectVersion | UserScale file changes |
|---|---|
| pre-v5 | Payload contains `mode`, `size`, `items[]`; no serialized `name` field |
| v5 | Added serialized `UserScale::name` (`char[9]`) |
| v6-v31 | No further UserScale payload field additions/removals; layout unchanged |

Option availability (`mode` values):
- `Chromatic` mode: available since legacy format (pre-v4)
- `Voltage` mode: available since legacy format (pre-v4)

## Value Semantics

From `UserScale` implementation:

- `mode`:
  - `0` = `Chromatic`
  - `1` = `Voltage`
- `size` constraints in normal runtime API: `1..32` (Chromatic) or `2..32` (Voltage)
- `items` meaning:
  - Chromatic: semitone values (0..11)
  - Voltage: millivolts (-5000..5000)

## Slot Files vs Direct Paths

`FileManager::writeUserScale(const UserScale&, int slot)` stores scales in slot files under the scales directory and extension defined in `Texts.h` (`SCALES` / `SCA`).

`FileManager::writeUserScale(const UserScale&, const char *path)` writes the same binary format to any explicit path.

So format is identical for slot-based and direct-path user scale files.
