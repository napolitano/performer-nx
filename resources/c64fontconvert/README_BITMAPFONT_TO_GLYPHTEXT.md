# bitmapfont_to_glyphtext

`bitmapfont_to_glyphtext` converts a BitmapFont C header file (like `ati8x8.h`, `tiny5x5.h`) into a human-editable glyph text file compatible with the glyphtext toolchain.

## What It Does

- Parses a BitmapFont C header file containing:
  - A packed bitmap array (1 bit per pixel)
  - BitmapFontGlyph metadata array (offset, width, height, etc.)
  - BitmapFont descriptor (first char, last char, etc.)
- Extracts each glyph's bitmap data from the packed array
- Converts bits to visual format: `*` for set bits, `-` for clear bits
- Centers each source glyph within the output cell (horizontally and/or vertically)
- Outputs a glyph text file in the format used by `glyphtext_editor` and `glyphtext_to_bin`

## Features

- Parses C header files with regex-based extraction
- Supports any output cell size (5x5, 8x8, 8x5, 16x6, etc.)
- Source glyphs are **always centered** in the output cell — no extra flags needed
- Output cell must be >= the largest source glyph (error otherwise)
- Interactive CLI prompts for missing dimensions
- LSB-first linear bit unpacking (compatible with BitmapFont runtime format)
- Comprehensive error messages

## Build

### Linux / WSL

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic -o bitmapfont_to_glyphtext bitmapfont_to_glyphtext.cpp
```

### With development flags

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wshadow -Wconversion -pedantic -o bitmapfont_to_glyphtext bitmapfont_to_glyphtext.cpp
```

## Usage

```bash
./bitmapfont_to_glyphtext [options] <input.h> <output.txt>
```

### Arguments

#### Required

- `input.h` — Path to the BitmapFont C header file
- `output.txt` — Path to the output glyph text file

#### Optional flags

- `-w, --width N` — Set output cell width in pixels (default: max source glyph width)
- `-h, --height N` — Set output cell height in pixels (default: max source glyph height)
- `--help` — Show usage information

Output dimensions must be **greater than or equal to** every source glyph's dimensions.

## Examples

### Convert with source dimensions (exact fit)

```bash
./bitmapfont_to_glyphtext ati8x8.h ati8x8.txt
```

Output size is inferred from the largest glyph in the font. Each glyph is written exactly.

### Convert with wider output cell (horizontal centering)

```bash
./bitmapfont_to_glyphtext -w 10 -h 8 ati8x8.h ati8x8_10x8.txt
```

Source glyphs are horizontally centered in the 10-pixel-wide cell; `-` fills unused columns.

### Convert with larger output cell (full centering)

```bash
./bitmapfont_to_glyphtext -w 10 -h 10 ati8x8.h ati8x8_10x10.txt
```

Source glyphs are centered both horizontally and vertically in the 10×10 cell.

### Convert a non-square source font (8x5)

```bash
./bitmapfont_to_glyphtext font8x5.h font8x5.txt
```

The output keeps rectangular dimensions, for example `Size: 8x5`.

### Interactive prompt for dimensions

```bash
./bitmapfont_to_glyphtext ati8x8.h output.txt
```

If dimensions cannot be inferred (empty font), the tool will prompt for width and height.

## Input Format

The input C header file must follow this structure:

```cpp
#ifndef __FONTNAME_H__
#define __FONTNAME_H__

#include "BitmapFont.h"

static uint8_t fontname_bitmap[] = {
    0x61, 0xbc, 0xff, 0x46, ...,
    ...
};

static BitmapFontGlyph fontname_glyphs[] = {
    { offset, width, height, xAdvance, xOffset, yOffset },
    ...
};

static BitmapFont fontname = {
    bpp, fontname_bitmap, fontname_glyphs, firstChar, lastChar, yAdvance
};

#endif // __FONTNAME_H__
```

### Requirements

- **Bitmap array**: Must be `static uint8_t`, hex-formatted (e.g., `0xAB`)
- **Glyphs array**: Must be `static BitmapFontGlyph` with 6-element tuples
- **Font struct**: Must be `static BitmapFont` with all required fields
- **Bit format**: Assumes 1 bit per pixel (`bpp = 1`), linear LSB-first packing

## Output Format

The output is a glyph text file compatible with `glyphtext_editor` and `glyphtext_to_bin`:

```text
Size: WxH

Glyph 16:
--****--
-*----*-
-*----*-
-*----*-
-*----*-
--****--
--------
--------

Glyph 17:
...
```

## Centering Behavior

Source glyphs are always centered within the output cell. Extra columns are split evenly left and right; extra rows are split evenly above and below. Odd extra pixels go to the right / bottom.

```text
Source glyph (3x3):   Output cell (7x5):
***                   --***--
-*-                   ---*---
***                   --***--
                      -------
                      -------
```

The output cell must be at least as large as the largest source glyph. Use the inferred default (no `-w`/`-h` flags) to produce an exact-fit output with no padding.

## Error Handling

The tool fails with clear error messages on:

- File I/O failures
- Malformed C header structure
- Invalid bitmap offsets
- Output cell smaller than a source glyph
- Unsupported `bpp` values (only `bpp=1` is supported)
- Invalid command-line arguments

## Integration

After conversion, you can:

1. Edit glyphs with `glyphtext_editor`
2. Convert to binary with `glyphtext_to_bin`
3. Use the resulting binary as a font asset

## Design Notes

- Uses regex-based parsing for flexibility
- Stores glyphs sparsely during extraction, then writes sequentially
- Centering is always applied; output must be >= source glyph dimensions
- LSB-first linear extraction matches the original BitmapFont runtime format
- Output is fully compatible with existing glyph text toolchain

## Limitations

- Input must use linear LSB-first bit packing
- Only 1-bit-per-pixel fonts are supported
- Cannot extract kerning or other advanced metrics
- Header file structure must match the documented format closely

## Troubleshooting

### "No bitmap array found"
The regex failed to match your bitmap array. Check that it's declared as `static uint8_t NAME[] = { ... }` with hex values like `0xAB`.

### "Could not parse BitmapFont struct"
The font struct declaration doesn't match the expected format. Ensure it's `static BitmapFont NAME = { ... }` with all 6 fields in order: `bpp, bitmap, glyphs, first, last, yAdvance`.

### "Bitmap offset out of bounds"
The glyph offset or dimensions are incorrect, or the bitmap array is truncated in the header. Verify the source header file is valid.

### "does not fit output size"
The specified output cell is smaller than at least one source glyph. Either increase `-w`/`-h`, or omit them to use the inferred maximum dimensions.

### Glyphs look incorrect
Ensure the input font uses linear LSB-first bit packing. If glyphs appear scrambled, the font may use a different packing layout.

## Future Improvements

- Support for variable bit-per-pixel formats
- Optional alternate unpacking mode for non-BitmapFont layouts
- Metadata preservation (xOffset, yOffset as comments)
- Batch conversion of multiple header files

