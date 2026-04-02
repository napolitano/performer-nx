# glyphtext_to_bin

`glyphtext_to_bin` converts human-readable glyph text files into packed binary font data.

It supports variable glyph dimensions (for example `5x5`, `5x8`, `8x8`, `12x12`) and keeps compatibility with existing `8x8` files.

## What It Does

- Reads glyph blocks from a text file.
- Converts `*` to bit `1` and `-` to bit `0`.
- Packs rows left-to-right in **MSB-first** bit order.
- Writes a dense binary table from glyph index `0` to `maxIndex`.

If an index is missing, its glyph bytes are emitted as zeroes.

## Input Format

### Optional global size header

You can define dimensions in the file:

```text
Size: 5x8
```

If no size is given in file or CLI, default size is `8x8`.

### Glyph blocks

Each glyph starts with a header and is followed by exactly `height` non-empty rows.

```text
Glyph 0:
--------
--****--
-*--*-*-
-*-*-**-
-*-****-
-*------
--****--
--------
```

Rules:

- Header format: `Glyph N:` where `N >= 0`
- Row width must equal glyph width exactly.
- Only `*` and `-` are allowed in rows.
- Empty lines are ignored between blocks/rows.

## Packing Rules

Given:

- `width` in pixels
- `height` in rows

Then:

- `bytesPerRow = ceil(width / 8)`
- `bytesPerGlyph = bytesPerRow * height`

Examples:

- `5x5`: `bytesPerRow = 1`, `bytesPerGlyph = 5`
- `8x8`: `bytesPerRow = 1`, `bytesPerGlyph = 8`
- `12x12`: `bytesPerRow = 2`, `bytesPerGlyph = 24`

For widths not divisible by 8, unused trailing bits in the final row byte are zero-padded.

## Command Line Interface

```text
glyphtext_to_bin [options] <input.txt> <output.bin>
```

Options:

- `--help` show usage
- `-s, --size WxH` set width and height at once
- `-w, --width N` set width
- `-h, --height N` set height

Notes:

- CLI size options override the optional `Size:` header in the input file.
- Accepted size separators: `x` or `X` (for example `12x12` and `12X12`).

## Build and Run

Build with a standalone compiler:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic resources/c64fontconvert/glyphtext_to_bin.cpp -o resources/c64fontconvert/glyphtext_to_bin
```

Convert an existing 8x8 file using defaults:

```bash
resources/c64fontconvert/glyphtext_to_bin resources/c64fontconvert/8x8font.txt /tmp/font8x8.bin
```

Convert a 5x5 file via CLI size:

```bash
resources/c64fontconvert/glyphtext_to_bin --size 5x5 resources/c64fontconvert/5x5symbols.txt /tmp/font5x5.bin
```

## Error Handling

The tool fails early with clear messages for:

- malformed size (`WxH`) declarations
- unsupported size ranges
- unknown CLI options
- missing CLI values
- duplicate `Size:` headers
- invalid glyph headers
- duplicate or negative glyph indices
- wrong row width
- invalid row characters
- incomplete glyph row data
- file I/O failures

## Design Notes

- Glyphs are stored sparsely during parsing (`std::map<int, GlyphBitmap>`), then serialized into a dense output table.
- Dense output simplifies runtime lookup by glyph index.

## TODOs / Future Improvements

- **Memory usage for sparse high-index fonts:** writing dense output can allocate large buffers when `maxIndex` is high but few glyphs are present.
- **Streaming writer mode:** optionally stream row-by-row output to reduce peak RAM.
- **Optional metadata output:** write sidecar metadata (`width`, `height`, `bytesPerRow`, `glyphCount`) to simplify integration in consuming tools.
- **CLI ergonomics:** optionally support `--strict-size` to enforce matching between file `Size:` and CLI size when both are provided.

