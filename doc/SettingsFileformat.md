# Settings File Format

This document describes the binary format of the settings backup file (`SETTINGS.DAT`) and the raw settings payload written to flash.

Source of truth:
- `src/apps/sequencer/model/Settings.h`
- `src/apps/sequencer/model/Settings.cpp`
- `src/apps/sequencer/model/Calibration.h`
- `src/apps/sequencer/model/Calibration.cpp`
- `src/apps/sequencer/model/AdvancedSettings.h`
- `src/apps/sequencer/model/AdvancedSettings.cpp`
- `src/apps/sequencer/model/FileDefs.h`
- `src/apps/sequencer/model/FileManager.cpp`
- `src/core/io/VersionedSerializedWriter.h`
- `src/core/io/VersionedSerializedReader.h`
- `src/core/hash/FnvHash.h`

## Overview

There are two wire-compatible settings payload contexts:

1. **Settings file** (`SETTINGS.DAT`) via `FileManager::writeSettings/readSettings`
2. **Flash payload** via `Settings::writeToFlash/readFromFlash`

Both use the same `Settings::write/read` payload format (versioned stream + trailing hash).
The file variant adds a `FileHeader` in front of that payload.

Checksum algorithm is **32-bit FNV hash** (`FnvHash`), not CRC-32.

## File Type and Name

From `FileDefs.h`:

```cpp
enum class FileType : uint8_t {
    Project   = 0,
    UserScale = 1,
    Settings  = 255
};
```

File-specific constants:
- Default filename: `Settings::Filename = "SETTINGS.DAT"`
- `FileHeader.name` for settings files: `"SETTINGS"` (`TXT_MODEL_FILE_HEADER_SETTINGS`)

## Settings File Layout (`SETTINGS.DAT`)

| Offset | Size | Content |
|---|---:|---|
| 0  | 10 | `FileHeader` |
| 10 | 4  | payload `dataVersion` (`uint32_t`) |
| 14 | ... | settings body (depends on settings version) |
| ... | 4 | trailing FNV hash |

### `FileHeader` (10 bytes, packed)

| Offset | Size | Type | Field | Value for settings |
|---|---:|---|---|---|
| 0 | 1 | uint8_t | `type` | `FileType::Settings` (`255`) |
| 1 | 1 | uint8_t | `version` | written as `0` |
| 2 | 8 | char[8] | `name` | `"SETTINGS"` (zero-padded) |

## Raw Flash Layout (no FileHeader)

`Settings::writeToFlash()` writes only the payload stream:

| Offset | Size | Content |
|---|---:|---|
| 0 | 4 | payload `dataVersion` (`uint32_t`) |
| 4 | ... | settings body |
| ... | 4 | trailing FNV hash |

## Payload Format

Payload is produced by `Settings::write(VersionedSerializedWriter&)`.

Order:
1. `dataVersion` (`uint32_t`) auto-written by `VersionedSerializedWriter`
2. `Calibration`
3. optional `AdvancedSettings` (only if `CONFIG_ADVANCED_SETTINGS` enabled in this build)
4. trailing hash via `writer.writeHash()`

## Calibration Block

`Calibration` contains `CONFIG_CV_OUTPUT_CHANNELS` outputs.

Each `Calibration::CvOutput` stores `ItemCount` samples where:
- `MinVoltage = -5`
- `MaxVoltage = +5`
- `ItemsPerVolt = 1`
- `ItemCount = (MaxVoltage - MinVoltage) * ItemsPerVolt + 1 = 11`

Each item is `uint16_t`.

So per output: `11 * 2 = 22` bytes.

With current config (`CONFIG_CV_OUTPUT_CHANNELS = 8`):
- Calibration block size = `8 * 22 = 176` bytes.

## AdvancedSettings Block (version >= 2 builds)

Only present when `CONFIG_ADVANCED_SETTINGS` is compiled in.

Field order:
1. `_flags` (`uint32_t`, 4 bytes)
2. `_language` (`uint8_t`, 1 byte)

Block size: 5 bytes.

Language values:
- `0` = English
- `1` = German

Invalid language values are clamped back to English when reading.

## Settings Versioning

`Settings::Version` is compile-time dependent:

- `Version = 1` if `CONFIG_ADVANCED_SETTINGS` is **not** defined
- `Version = 2` if `CONFIG_ADVANCED_SETTINGS` **is** defined

`FileManager::writeSettings` and `Settings::writeToFlash` both use `Settings::Version` as stream writer version.

### Version Matrix

| Settings stream version | Availability |
|---|---|
| v1 | `Calibration` only |
| v2 | `Calibration` + `AdvancedSettings` (`flags`, `language`) |

Compatibility behavior:
- Reader always starts by reading `dataVersion` from stream.
- In builds with `CONFIG_ADVANCED_SETTINGS`, `AdvancedSettings::read()` is called and reads fields unconditionally.
- Therefore, a **v1 payload** read by a **v2 build** is expected to fail hash verification (missing advanced fields in payload stream).

## Hash / Integrity

`VersionedSerializedWriter` hashes every payload byte written through `write(...)`.
That includes:
- `dataVersion`
- body fields

`writeHash()` then appends `uint32_t hash` to stream (hash value itself is not hashed).

`VersionedSerializedReader::checkHash()` reads that trailing `uint32_t` and compares with computed hash.

On mismatch in settings file path:
- `Settings::read()` clears settings and returns `false`
- `FileManager::readSettings()` maps success=false to `fs::INVALID_CHECKSUM`

## Size Formulas

Let:
- `N = CONFIG_CV_OUTPUT_CHANNELS`
- `C = 11` (`Calibration::CvOutput::ItemCount`)
- `A = 5` if `CONFIG_ADVANCED_SETTINGS`, else `0`

Then:
- Calibration bytes = `N * C * 2`
- Payload bytes (without trailing hash) = `4 + (N * C * 2) + A`
- Payload bytes (with hash) = `8 + (N * C * 2) + A`
- File bytes (`SETTINGS.DAT`) = `10 + 8 + (N * C * 2) + A`

With current config (`N=8`, advanced enabled in this repo):
- Payload without hash: `4 + 176 + 5 = 185`
- Payload with hash: `189`
- Full file size: `10 + 189 = 199` bytes

