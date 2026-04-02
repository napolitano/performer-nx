# c64font_to_bitmapfont

`c64font_to_bitmapfont` is a command-line utility that converts a classic C64-style 8x8 binary font into a packed bitmap font atlas plus glyph metrics suitable for embedded UI rendering.

The tool is designed for workflows where:

- the source font is stored as **8 bytes per glyph**
- each glyph is an **8x8 monochrome bitmap**
- the target renderer uses a **bitmap atlas + glyph metadata**
- glyphs should become **proportional** while preserving the original 8-pixel source format
- a **BMP preview image** is needed for visual inspection

The output format matches the structure shown below:

- `static uint8_t <fontname>_bitmap[]`
- `static BitmapFontGlyph <fontname>_glyphs[]`
- `static BitmapFont <fontname>`

## Features

- Converts **C64-style 8x8 font binaries**
- Computes a **proportional glyph width** for every glyph
- Applies a **1-pixel right-side blank column** when possible
- Preserves 8x8 source glyphs while exposing proportional width through metadata
- Packs all glyphs into a **single bitmap atlas**
- Emits a **C/C++ header file**
- Emits a **BMP preview file** for validation
- Supports configurable:
  - character range
  - line height
  - top padding
  - baseline
  - fallback width for empty glyphs

## How proportional width is computed

Each source glyph is analyzed row by row.

For every glyph, the tool:

1. scans all set bits in the 8x8 bitmap
2. finds the **rightmost used pixel**
3. sets the glyph width to `rightmost + 1`
4. adds **one blank pixel on the right** if the width is still below 8
5. clamps the final width to the range `1..8`

This turns a fixed-width 8-pixel source font into a practical proportional font without modifying the original glyph bitmaps.

### Example

If the rightmost set pixel of a glyph is at column 4:

- used width = `5`
- plus one blank pixel = `6`

If the rightmost set pixel is already at column 7:

- used width = `8`
- no extra blank pixel is added

If a glyph is completely empty:

- the width falls back to the configured `blankWidth` value

## Input format

The input file is expected to be a raw binary font file where:

- every glyph occupies exactly **8 bytes**
- each byte represents one **row**
- each glyph is therefore an **8x8 bitmap**

### Assumed bit layout

The current implementation assumes:

- **Bit 7 = leftmost pixel**
- **Bit 0 = rightmost pixel**

If your font data is mirrored horizontally, the bit extraction logic must be adapted in the source.

## Output files

The tool produces two files:

### 1. Header file

The generated header contains:

- a packed bitmap atlas
- glyph metric entries
- a `BitmapFont` descriptor instance

Typical structure:

```cpp
#ifndef __FONTNAME_H__
#define __FONTNAME_H__

#include "BitmapFont.h"

static uint8_t fontname_bitmap[] = {
    ...
};

static BitmapFontGlyph fontname_glyphs[] = {
    { x, y, w, h, xOffset, yOffset },
    ...
};

static BitmapFont fontname = {
    1, fontname_bitmap, fontname_glyphs, 16, 126, 10
};

#endif // __FONTNAME_H__
```

### 2. BMP preview

The BMP output is a visual representation of the generated bitmap atlas.

Use it to verify:

- glyph order
- spacing
- overall atlas packing
- whether proportional widths look correct

## Build

### Linux

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic -o c64font_to_bitmapfont c64font_to_bitmapfont.cpp
```

### Recommended compiler flags

For development builds:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -pedantic -o c64font_to_bitmapfont c64font_to_bitmapfont.cpp
```

## Usage

```bash
./c64font_to_bitmapfont <input.bin> <output.h> <output.bmp> <FontName> [firstChar] [lastChar] [lineHeight] [topPadding] [baseline] [blankWidth]
```

## Arguments

### Required arguments

#### `input.bin`
Path to the source C64-style binary font.

#### `output.h`
Path to the generated header file.

#### `output.bmp`
Path to the generated BMP preview.

#### `FontName`
Font name used for generated symbols.

Example:

- `Silkscreen8x8`
- `Performer8x8`
- `MainFont`

The tool derives symbol names from this value.

## Optional arguments

### `firstChar`
First glyph index / character code to export.

Default:
```text
16
```

### `lastChar`
Last glyph index / character code to export.

Default:
```text
126
```

### `lineHeight`
Line height stored in the generated `BitmapFont`.

Default:
```text
10
```

### `topPadding`
Vertical atlas offset used when packing glyphs.

Default:
```text
1
```

### `baseline`
Baseline reference used to derive glyph `yOffset`.

Default:
```text
8
```

### `blankWidth`
Fallback width for completely empty glyphs.

Default:
```text
4
```

## Example commands

### Minimal invocation

```bash
./c64font_to_bitmapfont c64font.bin Performer8x8.h Performer8x8.bmp Performer8x8
```

### Full invocation

```bash
./c64font_to_bitmapfont c64font.bin Performer8x8.h Performer8x8.bmp Performer8x8 16 126 10 1 8 4
```

## Generated glyph metrics

Every glyph is written as:

```cpp
{ x, y, w, h, xOffset, yOffset }
```

Typical meaning:

- `x` = x-position in the atlas
- `y` = y-position in the atlas
- `w` = proportional width computed from used bits
- `h` = glyph height
- `xOffset` = horizontal draw offset
- `yOffset` = vertical draw offset relative to baseline

The visual data itself is stored in the atlas bitmap array.

## Integration notes

### Rendering model

The intended rendering flow is:

1. select glyph by character code
2. read its `BitmapFontGlyph`
3. fetch the corresponding sub-rectangle from the atlas
4. draw it using `xOffset` and `yOffset`
5. advance the cursor by the glyph width or your renderer’s chosen spacing rule

### Why the bitmap stays 8x8

The original source format remains 8x8 because:

- conversion stays deterministic
- source data remains easy to inspect
- the proportional behavior is controlled exclusively through metadata

This keeps conversion simple and rendering flexible.

## Best practices

### Keep source fonts unmodified
Do not manually trim source glyph bitmaps unless there is a proven problem in the source font. Let the converter derive width automatically.

### Verify every conversion with the BMP
Always inspect the generated BMP preview before integrating a new font build.

### Keep naming stable
Use a stable `FontName` so generated symbols remain predictable across builds.

### Version generated assets
Check both the source font and generated header into version control if reproducibility matters for firmware releases.

### Document bit order
If your project uses multiple font sources, document whether each one uses MSB-left or LSB-left bit order.

## Limitations

- Input is assumed to be **8x8 monochrome only**
- Input is assumed to be **8 bytes per glyph**
- The current implementation assumes a specific bit order
- No kerning is generated
- Width is based only on the **rightmost used pixel**
- Left-side trimming is intentionally not performed

## Troubleshooting

### Output looks mirrored
Your input bit order likely differs from the current assumption. Adjust the pixel extraction logic in the source.

### Glyph spacing looks too wide
Reduce `blankWidth` for empty glyphs, or review whether your renderer adds additional tracking on top of glyph width.

### Glyph spacing looks too tight
Check whether your source glyphs already use the rightmost column. In that case, no extra blank pixel can be added.

### Wrong character range
Verify `firstChar` and `lastChar`. C64-derived assets often contain control characters or non-printing entries in the lower range.

### BMP preview does not match expectations
Check:
- source file ordering
- bit order
- glyph range
- baseline / padding settings

## Exit behavior

The program returns:

- `0` on success
- non-zero on failure

Failures typically include:

- input file cannot be opened
- output file cannot be written
- input size is not a multiple of 8 bytes
- invalid argument values

## Suggested workflow

1. prepare or export the C64-style font binary
2. run the converter
3. inspect the BMP preview
4. integrate the generated header
5. test rendering on target hardware
6. commit source and generated assets together

## File overview

- `c64font_to_bitmapfont.cpp` — converter source
- `c64font_to_bitmapfont` — compiled executable
- generated `*.h` — font atlas and metrics
- generated `*.bmp` — visual atlas preview

## License

Add the license that matches your project or repository. The converter itself does not embed a license automatically.

## Maintenance notes

When extending the tool, keep these invariants stable unless there is a deliberate format migration:

- 8 bytes per source glyph
- deterministic glyph ordering
- bitmap atlas + glyph metrics output model
- proportional width derived from actual set bits
- optional right-side blank pixel when width is below 8
