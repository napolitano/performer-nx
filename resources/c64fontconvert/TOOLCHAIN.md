# Font Conversion Toolchain — Master Documentation

This directory contains a **complete toolchain** for converting glyph text files into optimized rendering formats for embedded display systems.

The toolchain consists of four specialized tools that work together:

1. **`glyphtext_to_bin`** — Text → Binary conversion
2. **`c64font_dump_ascii`** — Binary → Text conversion (verification)
3. **`c64font_to_bitmapfont`** — Binary → C header + BMP (rendering format)
4. **`glyphtext_editor`** — Interactive ncurses glyph editor

## Quick Start

### Example Workflow: Convert an 8x8 Font

```bash
# Step 1: Convert human-readable text file to binary
glyphtext_to_bin 8x8font.txt font.bin

# Step 2: Verify the conversion (optional)
c64font_dump_ascii font.bin | head -50

# Step 3: Generate C header and preview image
c64font_to_bitmapfont font.bin font.h font.bmp MyFont

# Step 4: Use in your embedded application
#include "font.h"
canvas_drawString(canvas, x, y, "Hello", &myfont);
```

### Example Workflow: Convert a 5x5 Icon Font

```bash
# Create 5x5 font or use existing one
glyphtext_to_bin --size 5x5 icons.txt icons.bin
c64font_dump_ascii --size 5x5 icons.bin

# Generate rendering data (still works with 5x5 glyphs)
c64font_to_bitmapfont icons.bin icons.h icons.bmp IconFont
```

## Tools Overview

### 1. glyphtext_to_bin

Converts human-readable glyph text files into packed binary format.

**Supports:** Variable dimensions (5x5, 8x8, 12x12, etc.)

**Input:** Text file with optional `Size: WxH` header and `Glyph N:` blocks

**Output:** Dense binary table (one byte per bit, packed left-to-right)

**Key Features:**
- Backward-compatible with 8x8 defaults
- Per-character or file-wide size specification
- Strict validation and clear error messages
- MSB-first bit packing for visual correctness

**Full Documentation:** [`README_GLYPHTEXT_TO_BIN.md`](README_GLYPHTEXT_TO_BIN.md)

---

### 2. c64font_dump_ascii

Converts binary font files back into human-readable text (reverse of glyphtext_to_bin).

**Supports:** Variable dimensions (matches glyphtext_to_bin)

**Input:** Binary font file

**Output:** Text file with `Glyph N:` blocks

**Key Features:**
- Verify binary conversion fidelity
- Debug and inspect font data
- Export for documentation
- MSB-first bit extraction

**Full Documentation:** [`README_C64_FONT_DUMP_ASCII.md`](README_C64_FONT_DUMP_ASCII.md)

---

### 3. c64font_to_bitmapfont

Transforms binary fonts into production-ready C headers and preview images.

**Input:** 8x8 binary font file (standard C64 format)

**Outputs:**
- C header file with packed bitmap and glyph metrics
- 24-bit BMP preview image

**Key Features:**
- Glyph trimming to remove empty columns
- LSB-first 1-bit-per-pixel packing for efficient rendering
- Character set mapping (C64 → ASCII)
- Derived glyphs (rotations/mirrors for special chars)
- Configurable metrics (padding, baseline, advance)

**Full Documentation:** [`README_C64_FONT_TO_BITMAPFONT.md`](README_C64_FONT_TO_BITMAPFONT.md)

---

### 4. glyphtext_editor

Interactive ncurses editor for glyph text files.

**Supports:** Flexible dimensions (`5x5`, `5x8`, `8x8`, `12x12`, ...)

**Input:** Glyph text document (optional `Size: WxH` + `Glyph N:` blocks)

**Output:** Updated glyph text document (atomic save)

**Key Features:**
- Zoomed pixel editor
- Glyph list by index only
- Inverted header and help footer
- Popup menu for save/clear/delete/transform actions
- Navigation and jump between glyph indices

**Full Documentation:** [`README_GLYPHTEXT_EDITOR.md`](README_GLYPHTEXT_EDITOR.md)

---

## Building the Toolchain

All tools require C++17. Compile individually or as a batch:

```bash
# Individual builds
g++ -std=c++17 -O2 -Wall -Wextra -pedantic glyphtext_to_bin.cpp -o glyphtext_to_bin
g++ -std=c++17 -O2 -Wall -Wextra -pedantic c64font_dump_ascii.cpp -o c64font_dump_ascii
g++ -std=c++17 -O2 -Wall -Wextra -pedantic c64font_to_bitmapfont.cpp -o c64font_to_bitmapfont
g++ -std=c++17 -O2 -Wall -Wextra -pedantic glyphtext_editor.cpp -lncurses -o glyphtext_editor

# Or all at once
for f in glyphtext_to_bin.cpp c64font_dump_ascii.cpp c64font_to_bitmapfont.cpp; do
  g++ -std=c++17 -O2 -Wall -Wextra -pedantic "$f" -o "${f%.cpp}"
done
g++ -std=c++17 -O2 -Wall -Wextra -pedantic glyphtext_editor.cpp -lncurses -o glyphtext_editor
```

---

## Detailed Tool Documentation

Each tool has comprehensive documentation in its own README:

| Tool | Documentation |
|------|---------------|
| **glyphtext_to_bin** | [`README_GLYPHTEXT_TO_BIN.md`](README_GLYPHTEXT_TO_BIN.md) |
| **c64font_dump_ascii** | [`README_C64_FONT_DUMP_ASCII.md`](README_C64_FONT_DUMP_ASCII.md) |
| **c64font_to_bitmapfont** | [`README_C64_FONT_TO_BITMAPFONT.md`](README_C64_FONT_TO_BITMAPFONT.md) |
| **glyphtext_editor** | [`README_GLYPHTEXT_EDITOR.md`](README_GLYPHTEXT_EDITOR.md) |

---

## Common Use Cases

### Verify Font Fidelity After Conversion

```bash
glyphtext_to_bin myfont.txt myfont.bin
c64font_dump_ascii myfont.bin > myfont_output.txt
diff myfont.txt myfont_output.txt
```

### Generate Multiple Rendering Formats

```bash
# Convert once to binary
glyphtext_to_bin font.txt font.bin

# Generate multiple output formats (C header with different settings)
c64font_to_bitmapfont font.bin font_tight.h font_tight.bmp TightFont 16 125 8 0 8 2
c64font_to_bitmapfont font.bin font_loose.h font_loose.bmp LooseFont 16 125 12 2 10 4
```

### Preview Before Embedding

```bash
glyphtext_to_bin myfont.txt myfont.bin
c64font_to_bitmapfont myfont.bin myfont.h myfont.bmp MyFont
# Open myfont.bmp to visually inspect the result
# If satisfied, include myfont.h in your project
```

---

## Error Handling and Validation

All tools perform strict validation:

- **File format errors**: Non-existent files, invalid file sizes
- **Dimension errors**: Width/height out of bounds, mismatched row counts
- **Character errors**: Invalid tokens (only `*` and `-` allowed)
- **Index errors**: Negative or duplicate glyph indices
- **Size errors**: Malformed WxH specifications

Errors are reported with context (line number, field, value) and usage help is printed on failure.

---

## Implementation Notes

All tools share common design principles:

- **C++17 standard** with no external dependencies
- **Comprehensive validation** with context-aware error messages
- **Backward compatible** with existing 8x8 font files
- **Consistent bit order** (MSB-first for glyphtext_to_bin / c64font_dump_ascii)
- **Method-level documentation** (Doxygen-style comments)
- **Inline annotations** explaining dense logic

---

## References

- [README_GLYPHTEXT_TO_BIN.md](README_GLYPHTEXT_TO_BIN.md) — Text to binary conversion
- [README_C64_FONT_DUMP_ASCII.md](README_C64_FONT_DUMP_ASCII.md) — Binary to text verification
- [README_C64_FONT_TO_BITMAPFONT.md](README_C64_FONT_TO_BITMAPFONT.md) — Binary to rendering format
- [README_GLYPHTEXT_EDITOR.md](README_GLYPHTEXT_EDITOR.md) — Interactive glyph editing

