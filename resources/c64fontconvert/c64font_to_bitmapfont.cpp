#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

/// Represents one source glyph in row-major MSB-first packed format.
struct GlyphBitmap {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rows;
};

/// Metrics describing how to render and advance a glyph.
struct GlyphMetric {
    int offset = 0;    // Byte offset into packed bitmap
    int width = 0;     // Drawn width
    int height = 0;    // Drawn height
    int xAdvance = 0;  // Cursor advance
    int xOffset = 0;   // X offset for rendering
    int yOffset = 0;   // Y offset for rendering (relative to baseline)
};

/// Command-line options for the converter.
struct Options {
    std::string inputBin;
    std::string outputHeader;
    std::string outputBmp;
    std::string fontName;
    int sourceWidth = 8;
    int sourceHeight = 8;
    int firstChar = 16;
    int lastChar = 125;
    int lineHeight = 10;
    int topPadding = 1;
    int baseline = 8;
    int blankGlyphWidth = 4;
    bool hasExplicitSourceSize = false;
};

/// Packed font data ready for serialization.
struct PackedFontData {
    std::vector<uint8_t> bitmap;
    std::vector<GlyphMetric> metrics;
};

/// Bounding box for a single glyph.
struct GlyphBounds {
    int left = 0;
    int right = -1;

    bool empty() const { return right < left; }
    int width() const { return empty() ? 0 : (right - left + 1); }
};

/// Converts a string to a valid C identifier (lowercase with underscores).
///
/// @param s The input string to convert.
/// @return A string suitable for use as a C variable name (lowercase, underscores for non-alphanumeric).
static std::string toLowerIdent(std::string s) {
    for (char &c : s) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else {
            c = '_';
        }
    }
    return s;
}

/// Converts a string to a valid C identifier (uppercase with underscores).
///
/// @param s The input string to convert.
/// @return A string suitable for use as a C macro name (uppercase, underscores for non-alphanumeric).
static std::string toUpperIdent(std::string s) {
    for (char &c : s) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        } else {
            c = '_';
        }
    }
    return s;
}

/// Parses a size specification string in the format "WxH".
///
/// Accepts either lowercase 'x' or uppercase 'X' as the separator.
/// Currently not used by c64font_to_bitmapfont but provided for future extension.
///
/// @param value   The size string to parse (e.g., "8x8").
/// @param context A descriptive label for error reporting.
/// @return A pair (width, height).
/// @throws std::runtime_error if format is invalid or non-numeric.
[[maybe_unused]]
static std::pair<int, int> parseSizeValue(const std::string& value, const std::string& context) {
    const auto xPos = value.find_first_of("xX");
    if (xPos == std::string::npos) {
        throw std::runtime_error(context + ": expected size format WxH, got: " + value);
    }

    const auto widthStart = value.find_first_not_of(" \t", 0);
    const auto widthEnd = value.find_last_not_of(" \t", xPos - 1);
    const auto heightStart = value.find_first_not_of(" \t", xPos + 1);
    const auto heightEnd = value.find_last_not_of(" \t");

    if (widthStart == std::string::npos || widthEnd == std::string::npos ||
        heightStart == std::string::npos || heightEnd == std::string::npos) {
        throw std::runtime_error(context + ": expected size format WxH, got: " + value);
    }

    const std::string widthText = value.substr(widthStart, widthEnd - widthStart + 1);
    const std::string heightText = value.substr(heightStart, heightEnd - heightStart + 1);

    int width = 0;
    int height = 0;
    try {
        width = std::stoi(widthText);
        height = std::stoi(heightText);
    } catch (...) {
        throw std::runtime_error(context + ": invalid numeric size: " + value);
    }

    return {width, height};
}

/// Number of bytes needed to store one MSB-first packed row.
static int bytesPerRow(const int width) {
    return (width + 7) / 8;
}

/// Best-effort source size inference for BIN files that are not 8x8-packed.
///
/// This is primarily used as a fallback for glyphtext_to_bin output where the dense
/// table usually spans indices 0..126 (127 glyphs) and width <= 8 (one byte per row).
static std::pair<int, int> inferSourceSizeFromBin(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Could not open BIN file for size inference: " + path);
    }

    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (raw.empty()) {
        throw std::runtime_error("BIN file is empty; cannot infer source size.");
    }

    const std::vector<int> glyphCountCandidates = {127, 126, 256, 320};
    for (const int glyphCount : glyphCountCandidates) {
        if ((raw.size() % static_cast<size_t>(glyphCount)) != 0) {
            continue;
        }

        const int stride = static_cast<int>(raw.size() / static_cast<size_t>(glyphCount));
        if (stride < 1 || stride > 128) {
            continue;
        }

        // Compact case: one byte per row, infer width from actual used bits.
        int usedWidth = 0;
        for (const uint8_t b : raw) {
            for (int bit = 0; bit < 8; ++bit) {
                if ((b & static_cast<uint8_t>(1u << (7 - bit))) != 0) {
                    usedWidth = std::max(usedWidth, bit + 1);
                }
            }
        }
        if (usedWidth == 0) {
            usedWidth = 8;
        }
        return {usedWidth, stride};
    }

    throw std::runtime_error(
        "Could not infer source glyph size from BIN. Please pass sourceWidth/sourceHeight.");
}

/// Loads a binary font file containing fixed-size source glyphs.
///
/// @param path Path to the binary font file.
/// @param width Source glyph width in pixels.
/// @param height Source glyph height in pixels.
/// @return A vector of glyphs, one per fixed-size chunk.
/// @throws std::runtime_error if the file cannot be opened or size is invalid.
static std::vector<GlyphBitmap> loadC64Bin(const std::string &path, const int width, const int height) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Could not open BIN file: " + path);
    }

    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const int stride = bytesPerRow(width) * height;
    if (stride <= 0) {
        throw std::runtime_error("Invalid source glyph size.");
    }

    if (raw.empty() || (raw.size() % static_cast<size_t>(stride)) != 0) {
        throw std::runtime_error(
            "BIN file size must be a multiple of source glyph stride. "
            "If your source is not 8x8, pass sourceWidth/sourceHeight as the last arguments.");
    }

    std::vector<GlyphBitmap> glyphs(raw.size() / static_cast<size_t>(stride));
    for (size_t i = 0; i < glyphs.size(); ++i) {
        glyphs[i].width = width;
        glyphs[i].height = height;
        glyphs[i].rows.resize(static_cast<size_t>(stride));
        std::copy(raw.begin() + static_cast<std::ptrdiff_t>(i * static_cast<size_t>(stride)),
                  raw.begin() + static_cast<std::ptrdiff_t>((i + 1) * static_cast<size_t>(stride)),
                  glyphs[i].rows.begin());
    }
    return glyphs;
}

/// Retrieves a single pixel from a source glyph using MSB-first bit ordering.
///
/// @param g The glyph to query.
/// @param x The x-coordinate (0-7), where 0 is leftmost.
/// @param y The y-coordinate (0-7), where 0 is topmost.
/// @return true if the pixel is set, false if clear.
static bool getGlyphPixel(const GlyphBitmap &g, int x, int y) {
    if (x < 0 || y < 0 || x >= g.width || y >= g.height) {
        return false;
    }
    const int rowStride = bytesPerRow(g.width);
    const size_t index = static_cast<size_t>(y * rowStride + (x / 8));
    return ((g.rows[index] >> (7 - (x % 8))) & 0x1u) != 0;
}

/// Sets a single pixel in a source glyph using MSB-first bit ordering.
///
/// @param g   The glyph to modify.
/// @param x   The x-coordinate (0-7), where 0 is leftmost.
/// @param y   The y-coordinate (0-7), where 0 is topmost.
/// @param on  true to set the pixel, false to clear it.
static void setGlyphPixel(GlyphBitmap &g, int x, int y, bool on) {
    if (x < 0 || y < 0 || x >= g.width || y >= g.height) {
        return;
    }
    const int rowStride = bytesPerRow(g.width);
    const size_t index = static_cast<size_t>(y * rowStride + (x / 8));
    const uint8_t mask = static_cast<uint8_t>(1u << (7 - (x % 8)));
    if (on) {
        g.rows[index] |= mask;
    } else {
        g.rows[index] &= static_cast<uint8_t>(~mask);
    }
}

/// Flips a glyph horizontally (mirror left-to-right).
///
/// @param src The source glyph.
/// @return A new glyph with pixels mirrored horizontally.
static GlyphBitmap mirrorHorizontal(const GlyphBitmap &src) {
    GlyphBitmap dst{};
    dst.width = src.width;
    dst.height = src.height;
    dst.rows.assign(static_cast<size_t>(bytesPerRow(dst.width) * dst.height), 0);
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            setGlyphPixel(dst, src.width - 1 - x, y, getGlyphPixel(src, x, y));
        }
    }
    return dst;
}

/// Rotates a glyph 90 degrees clockwise.
///
/// @param src The source glyph.
/// @return A new glyph rotated 90 degrees clockwise.
static GlyphBitmap rotateClockwise(const GlyphBitmap &src) {
    GlyphBitmap dst{};
    dst.width = src.height;
    dst.height = src.width;
    dst.rows.assign(static_cast<size_t>(bytesPerRow(dst.width) * dst.height), 0);
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            setGlyphPixel(dst, src.height - 1 - y, x, getGlyphPixel(src, x, y));
        }
    }
    return dst;
}

/// Computes the horizontal bounds of non-zero pixels in a glyph.
///
/// @param g The glyph to analyze.
/// @return A GlyphBounds struct indicating the leftmost and rightmost set pixels.
///         If no pixels are set, left > right (empty() returns true).
static GlyphBounds computeGlyphBounds(const GlyphBitmap &g) {
    GlyphBounds b;
    b.left = g.width;

    for (int y = 0; y < g.height; ++y) {
        for (int x = 0; x < g.width; ++x) {
            if (getGlyphPixel(g, x, y)) {
                b.left = std::min(b.left, x);
                b.right = std::max(b.right, x);
            }
        }
    }

    return b;
}

/// Computes the horizontal advance (cursor movement) for a glyph.
///
/// For blank glyphs, returns blankGlyphWidth (clamped to 1-8).
/// For non-blank glyphs, returns the used width plus 1 pixel gap (or 8 if already full width).
///
/// @param b                The bounding box of the glyph.
/// @param blankGlyphWidth  The advance to use for blank glyphs.
/// @return The x-advance value in pixels.
static int computeXAdvance(const GlyphBounds &b, int blankGlyphWidth, const int sourceWidth) {
    if (b.empty()) {
        return std::clamp(blankGlyphWidth, 1, sourceWidth);
    }

    const int usedWidth = b.width();
    return usedWidth < sourceWidth ? usedWidth + 1 : sourceWidth;
}

/// Builds a mapping from character tokens to their source glyph indices in the C64 font.
///
/// This encodes the C64 font character layout, which includes ASCII characters,
/// special symbols, and alternate positions for lowercase letters.
///
/// @return An unordered_map from character strings to source glyph indices.
static std::unordered_map<std::string, int> buildSourceMap() {
    std::unordered_map<std::string, int> map;

    map["@"] = 0;
    for (int i = 0; i < 26; ++i) {
        map[std::string(1, static_cast<char>('A' + i))] = 1 + i;
    }
    map["["] = 27;
    map["£"] = 28;
    map["]"] = 29;
    map["↑"] = 30;
    map["←"] = 31;
    map[" "] = 32;
    map["!"] = 33;
    map["\""] = 34;
    map["#"] = 35;
    map["$"] = 36;
    map["%"] = 37;
    map["&"] = 38;
    map["´"] = 39;
    map["("] = 40;
    map[")"] = 41;
    map["*"] = 42;
    map["+"] = 43;
    map[","] = 44;
    map["-"] = 45;
    map["."] = 46;
    map["/"] = 47;
    for (int i = 0; i < 10; ++i) {
        map[std::string(1, static_cast<char>('0' + i))] = 48 + i;
    }
    map[":"] = 58;
    map[";"] = 59;
    map["<"] = 60;
    map["="] = 61;
    map[">"] = 62;
    map["?"] = 63;
    map["_"] = 64;
    for (int i = 0; i < 26; ++i) {
        map[std::string(1, static_cast<char>('a' + i))] = 257 + i;
    }

    return map;
}

/// Maps C64 source glyphs to a target ASCII character range.
///
/// This function creates a vector of target glyphs by mapping ASCII codes (or derived transformations)
/// from the C64 source font. It handles uppercase/lowercase letters, derived glyphs (mirrors/rotations),
/// and blank entries for unmapped characters.
///
/// @param sourceGlyphs The loaded C64 font glyphs.
/// @param opt          The conversion options.
/// @return A vector of target glyphs, one per character in the ASCII range.
/// @throws std::runtime_error if source/target mapping is inconsistent.
static std::vector<GlyphBitmap> buildTargetGlyphs(const std::vector<GlyphBitmap> &sourceGlyphs, const Options &opt) {
    const auto src = buildSourceMap();
    std::vector<GlyphBitmap> target(static_cast<size_t>(opt.lastChar - opt.firstChar + 1));

    auto makeBlankGlyph = [&]() {
        GlyphBitmap blank{};
        blank.width = opt.sourceWidth;
        blank.height = opt.sourceHeight;
        blank.rows.assign(static_cast<size_t>(bytesPerRow(blank.width) * blank.height), 0);
        return blank;
    };
    for (auto &glyph : target) {
        glyph = makeBlankGlyph();
    }

    auto assignFromSource = [&](int code, const std::string &token) {
        const auto it = src.find(token);
        if (it == src.end()) {
            throw std::runtime_error("Internal mapping error for token: " + token);
        }
        const int index = it->second;
        if (index < 0 || index >= static_cast<int>(sourceGlyphs.size())) {
            throw std::runtime_error("Source glyph index out of range for token: " + token);
        }
        target[static_cast<size_t>(code - opt.firstChar)] = sourceGlyphs[static_cast<size_t>(index)];
    };

    auto assignGlyph = [&](int code, const GlyphBitmap &glyph) {
        target[static_cast<size_t>(code - opt.firstChar)] = glyph;
    };

    // For arbitrary ranges, use direct index mapping (glyph code -> same source index).
    // Keep legacy C64 remap only for the default sequencer export range.
    const bool useLegacyC64Remap = (opt.firstChar == 16 && opt.lastChar == 125);
    if (!useLegacyC64Remap) {
        for (int code = opt.firstChar; code <= opt.lastChar; ++code) {
            const int sourceIndex = code;
            if (sourceIndex >= 0 && sourceIndex < static_cast<int>(sourceGlyphs.size())) {
                assignGlyph(code, sourceGlyphs[static_cast<size_t>(sourceIndex)]);
            }
        }
        return target;
    }

    // If the source table does not contain C64 extended indices, fall back to direct mapping.
    constexpr int kMaxC64SourceIndexNeeded = 282;
    if (static_cast<int>(sourceGlyphs.size()) <= kMaxC64SourceIndexNeeded) {
        for (int code = opt.firstChar; code <= opt.lastChar; ++code) {
            const int sourceIndex = code;
            if (sourceIndex >= 0 && sourceIndex < static_cast<int>(sourceGlyphs.size())) {
                assignGlyph(code, sourceGlyphs[static_cast<size_t>(sourceIndex)]);
            }
        }
        return target;
    }

    // Characters 0x10..0x1F are reserved and stay empty.
    assignFromSource(0x20, " ");
    assignFromSource(0x21, "!");
    assignFromSource(0x22, "\"");
    assignFromSource(0x23, "#");
    assignFromSource(0x24, "$");
    assignFromSource(0x25, "%");
    assignFromSource(0x26, "&");
    assignFromSource(0x27, "´");
    assignFromSource(0x28, "(");
    assignFromSource(0x29, ")");
    assignFromSource(0x2A, "*");
    assignFromSource(0x2B, "+");
    assignFromSource(0x2C, ",");
    assignFromSource(0x2D, "-");
    assignFromSource(0x2E, ".");
    assignFromSource(0x2F, "/");

    for (int code = 0x30; code <= 0x39; ++code) {
        assignFromSource(code, std::string(1, static_cast<char>(code)));
    }

    assignFromSource(0x3A, ":");
    assignFromSource(0x3B, ";");
    assignFromSource(0x3C, "<");
    assignFromSource(0x3D, "=");
    assignFromSource(0x3E, ">");
    assignFromSource(0x3F, "?");
    assignFromSource(0x40, "@");

    for (int code = 0x41; code <= 0x5A; ++code) {
        assignFromSource(code, std::string(1, static_cast<char>(code)));
    }

    assignFromSource(0x5B, "[");
    assignGlyph(0x5C, mirrorHorizontal(sourceGlyphs[static_cast<size_t>(src.at("/"))]));
    assignFromSource(0x5D, "]");
    assignGlyph(0x5E, rotateClockwise(sourceGlyphs[static_cast<size_t>(src.at("<"))]));
    assignFromSource(0x5F, "_");
    assignGlyph(0x60, mirrorHorizontal(sourceGlyphs[static_cast<size_t>(src.at("´"))]));

    for (int code = 0x61; code <= 0x7A; ++code) {
        assignFromSource(code, std::string(1, static_cast<char>('a' + (code - 0x61))));
    }

    assignFromSource(0x7B, "(");
    assignFromSource(0x7C, "I");
    assignFromSource(0x7D, ")");

    return target;
}

/// Packs one glyph into LSB-first 1-bit-per-pixel format.
///
/// This packing matches the Canvas::drawBitmap() rendering expectations:
/// pixels are consumed linearly, LSB first inside each byte.
///
/// @param out       Output vector to append packed bytes to.
/// @param glyph     The source glyph to pack.
/// @param bounds    The bounding box of the glyph.
/// @param drawWidth The width in pixels to pack (typically bounds.width()).
/// @param height    The height in pixels to pack.
static void appendPackedGlyph1bpp(std::vector<uint8_t> &out,
                                  const GlyphBitmap &glyph,
                                  const GlyphBounds &bounds,
                                  int drawWidth,
                                  int height) {
    // Pixels are consumed linearly, LSB first inside each byte.
    uint8_t currentByte = 0;
    int shift = 0;

    for (int y = 0; y < height; ++y) {
        for (int dx = 0; dx < drawWidth; ++dx) {
            const int sx = bounds.empty() ? 0 : (bounds.left + dx);
            const bool on = !bounds.empty() && sx >= 0 && sx < glyph.width && getGlyphPixel(glyph, sx, y);

            if (on) {
                currentByte |= static_cast<uint8_t>(1u << shift);
            }
            ++shift;
            if (shift == 8) {
                out.push_back(currentByte);
                currentByte = 0;
                shift = 0;
            }
        }
    }

    if (shift != 0) {
        out.push_back(currentByte);
    }
}

/// Builds packed font data from a vector of 8x8 glyphs.
///
/// Computes bounding boxes, packing offsets, and metrics for each glyph,
/// then serializes the glyphs into LSB-first 1-bit-per-pixel format.
///
/// @param glyphs The glyphs to pack.
/// @param opt    The conversion options (for padding/baseline adjustments).
/// @return A PackedFontData structure containing packed bitmap and metrics.
static PackedFontData buildPackedFont(const std::vector<GlyphBitmap> &glyphs, const Options &opt) {
    PackedFontData out;
    out.metrics.reserve(glyphs.size());

    for (const auto &glyph : glyphs) {
        const GlyphBounds bounds = computeGlyphBounds(glyph);
        const int drawWidth = bounds.empty() ? 1 : bounds.width();
        const int xAdvance = computeXAdvance(bounds, opt.blankGlyphWidth, glyph.width);

        GlyphMetric metric;
        metric.offset = static_cast<int>(out.bitmap.size());
        metric.width = drawWidth;
        metric.height = glyph.height;
        metric.xAdvance = xAdvance;
        metric.xOffset = 0;
        metric.yOffset = opt.topPadding - opt.baseline;
        out.metrics.push_back(metric);

        appendPackedGlyph1bpp(out.bitmap, glyph, bounds, drawWidth, glyph.height);
    }

    return out;
}

/// Builds a preview bitmap atlas showing all glyphs in a grid.
///
/// Creates a bitmap with glyphs arranged in columns (16 glyphs per row by default).
/// Each glyph is displayed in an 8-pixel-wide cell with line-height spacing.
///
/// @param glyphs       The glyphs to render.
/// @param opt          Conversion options for line height and top padding.
/// @param atlasWidth   Output parameter: width of the resulting atlas in pixels.
/// @param atlasHeight  Output parameter: height of the resulting atlas in pixels.
/// @return A bitmap in MSB-first 1-bit-per-pixel format.
static std::vector<uint8_t> buildPreviewAtlas(const std::vector<GlyphBitmap> &glyphs,
                                              const Options &opt,
                                              int &atlasWidth,
                                              int &atlasHeight) {
    const int glyphCount = static_cast<int>(glyphs.size());
    const int columns = 16;
    const int rows = (glyphCount + columns - 1) / columns;

    int cellWidth = opt.sourceWidth;
    int cellHeight = opt.sourceHeight;
    for (const auto &glyph : glyphs) {
        cellWidth = std::max(cellWidth, glyph.width);
        cellHeight = std::max(cellHeight, glyph.height);
    }

    atlasWidth = columns * cellWidth;
    atlasHeight = rows * opt.lineHeight;

    const int stride = (atlasWidth + 7) / 8;
    std::vector<uint8_t> bitmap(static_cast<size_t>(stride * atlasHeight), 0);

    auto setAtlasPixel = [&](int x, int y) {
        if (x < 0 || x >= atlasWidth || y < 0 || y >= atlasHeight) {
            return;
        }
        const int index = y * stride + (x / 8);
        bitmap[static_cast<size_t>(index)] |= static_cast<uint8_t>(1u << (7 - (x % 8)));
    };

    for (int i = 0; i < glyphCount; ++i) {
        const int cellX = (i % columns) * cellWidth;
        const int cellY = (i / columns) * opt.lineHeight + opt.topPadding;

        for (int y = 0; y < glyphs[static_cast<size_t>(i)].height; ++y) {
            for (int x = 0; x < glyphs[static_cast<size_t>(i)].width; ++x) {
                if (getGlyphPixel(glyphs[static_cast<size_t>(i)], x, y)) {
                    setAtlasPixel(cellX + x, cellY + y);
                }
            }
        }
    }

    return bitmap;
}

/// Writes a bitmap to a 24-bit BMP file (RGB, uncompressed).
///
/// The bitmap is expected to be in MSB-first 1-bit-per-pixel format. It is converted
/// to 24-bit RGB (white on black) for the output file. BMP format stores rows bottom-to-top,
/// so the bitmap is vertically flipped during output.
///
/// @param path   Path to the output .bmp file.
/// @param bitmap The source bitmap data (1-bit-per-pixel, MSB-first).
/// @param width  Width of the bitmap in pixels.
/// @param height Height of the bitmap in pixels.
/// @throws std::runtime_error if the file cannot be created or written.
static void writeBmp24(const std::string &path,
                       const std::vector<uint8_t> &bitmap,
                       int width,
                       int height) {
    const int srcStride = (width + 7) / 8;
    const int dstStride = ((width * 3) + 3) & ~3;
    const int pixelDataSize = dstStride * height;
    const int fileSize = 54 + pixelDataSize;

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Could not write BMP file: " + path);
    }

    auto writeLE16 = [&](uint16_t v) {
        out.put(static_cast<char>(v & 0xff));
        out.put(static_cast<char>((v >> 8) & 0xff));
    };
    auto writeLE32 = [&](uint32_t v) {
        out.put(static_cast<char>(v & 0xff));
        out.put(static_cast<char>((v >> 8) & 0xff));
        out.put(static_cast<char>((v >> 16) & 0xff));
        out.put(static_cast<char>((v >> 24) & 0xff));
    };

    out.put('B');
    out.put('M');
    writeLE32(static_cast<uint32_t>(fileSize));
    writeLE16(0);
    writeLE16(0);
    writeLE32(54);

    writeLE32(40);
    writeLE32(static_cast<uint32_t>(width));
    writeLE32(static_cast<uint32_t>(height));
    writeLE16(1);
    writeLE16(24);
    writeLE32(0);
    writeLE32(static_cast<uint32_t>(pixelDataSize));
    writeLE32(2835);
    writeLE32(2835);
    writeLE32(0);
    writeLE32(0);

    std::vector<uint8_t> line(static_cast<size_t>(dstStride), 0);
    for (int y = height - 1; y >= 0; --y) {
        std::fill(line.begin(), line.end(), 0);
        for (int x = 0; x < width; ++x) {
            const int srcByte = y * srcStride + (x / 8);
            const bool on = ((bitmap[static_cast<size_t>(srcByte)] >> (7 - (x % 8))) & 0x1u) != 0;
            const uint8_t v = on ? 255 : 0;
            line[static_cast<size_t>(x * 3 + 0)] = v;
            line[static_cast<size_t>(x * 3 + 1)] = v;
            line[static_cast<size_t>(x * 3 + 2)] = v;
        }
        out.write(reinterpret_cast<const char *>(line.data()), static_cast<std::streamsize>(line.size()));
    }
}

/// Writes a C header file containing font data and metrics.
///
/// Generates a struct containing:
///   - Packed bitmap data as a uint8_t array
///   - Metrics array (offset, width, height, advance, offsets) for each glyph
///   - BitmapFont wrapper with metadata
///
/// @param path      Path to the output .h file.
/// @param fontName  Name of the font (used for variable names and macro guards).
/// @param fontData  The packed font data.
/// @param firstChar The first ASCII code in the target character set.
/// @param lastChar  The last ASCII code in the target character set.
/// @param lineHeight The line height used during rendering.
/// @throws std::runtime_error if the file cannot be created or written.
static void writeHeader(const std::string &path,
                        const std::string &fontName,
                        const PackedFontData &fontData,
                        int firstChar,
                        int lastChar,
                        int lineHeight) {
    const std::string lower = toLowerIdent(fontName);
    const std::string upper = toUpperIdent(fontName);

    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Could not write header file: " + path);
    }

    out << "#ifndef __" << upper << "_H__\n";
    out << "#define __" << upper << "_H__\n\n";
    out << "#include \"BitmapFont.h\"\n\n";

    out << "static uint8_t " << lower << "_bitmap[] = {\n    ";
    for (size_t i = 0; i < fontData.bitmap.size(); ++i) {
        out << "0x" << std::hex << std::nouppercase << std::setfill('0') << std::setw(2)
            << static_cast<int>(fontData.bitmap[i]);
        if (i + 1 != fontData.bitmap.size()) {
            out << ", ";
        }
        if ((i + 1) % 16 == 0 && i + 1 != fontData.bitmap.size()) {
            out << "\n    ";
        }
    }
    out << std::dec << "\n};\n\n";

    out << "static BitmapFontGlyph " << lower << "_glyphs[] = {\n";
    for (const auto &g : fontData.metrics) {
        out << "    { "
            << g.offset << ", "
            << g.width << ", "
            << g.height << ", "
            << g.xAdvance << ", "
            << g.xOffset << ", "
            << g.yOffset
            << " },\n";
    }
    out << "};\n\n";

    out << "static BitmapFont " << lower << " = {\n";
    out << "    1, " << lower << "_bitmap, " << lower << "_glyphs, "
        << firstChar << ", " << lastChar << ", " << lineHeight << "\n";
    out << "};\n\n";

    out << "#endif // __" << upper << "_H__\n";
}

/// Prints CLI usage and help information.
///
/// @param programName The name of the executable.
static void printUsage(const char* programName) {
    std::cout << "Usage:\n"
              << "  " << programName << " <input.bin> <output.h> <output.bmp> <FontName>\n"
              << "                  [options]\n\n"
              << "Named options (recommended):\n"
              << "  -sourceWidth N    Source glyph width in pixels\n"
              << "  -sourceHeight N   Source glyph height in pixels\n"
              << "  -firstChar N      First character code\n"
              << "  -lastChar N       Last character code\n"
              << "  -lineHeight N     Line height for preview atlas\n"
              << "  -topPadding N     Top padding in preview atlas\n"
              << "  -baseline N       Baseline offset\n"
              << "  -blankWidth N     Width of blank/space glyphs\n\n"
              << "Legacy positional optional order:\n"
              << "  [sourceWidth] [sourceHeight] [firstChar] [lastChar]\n"
              << "  [lineHeight] [topPadding] [baseline] [blankWidth]\n\n"
              << "Options:\n"
              << "  input.bin      Path to binary font file (from glyphtext_to_bin)\n"
              << "  output.h       Path to output C header file\n"
              << "  output.bmp     Path to output preview BMP file\n"
              << "  FontName       Name to use for C identifiers\n"
              << "  sourceWidth    Source glyph width in pixels (default: inferred)\n"
              << "  sourceHeight   Source glyph height in pixels (default: inferred)\n"
              << "  firstChar      First ASCII code (default: 16)\n"
              << "  lastChar       Last ASCII code (default: 125)\n"
              << "  lineHeight     Line height for atlas (default: 10, must be >= sourceHeight)\n"
              << "  topPadding     Top padding in pixels (default: 1)\n"
              << "  baseline       Baseline offset (default: 8)\n"
              << "  blankWidth     Width of blank/space glyphs (default: 4)\n\n"
              << "Notes:\n"
              << "  - firstChar must be strictly less than lastChar.\n"
              << "  - Maps C64 character set to ASCII (0x10-0x7D).\n"
              << "  - Generates LSB-first 1-bit-per-pixel packed data.\n";
}

/// Parses and validates command line arguments.
///
/// @param argc Argument count.
/// @param argv Argument array.
/// @return Validated Options structure.
/// @throws std::runtime_error if arguments are invalid or missing.
static Options parseArgs(int argc, char **argv) {
    if (argc < 5) {
        printUsage(argv[0]);
        throw std::runtime_error(
            "Missing required arguments: <input.bin> <output.h> <output.bmp> <FontName>");
    }

    Options opt;
    opt.inputBin = argv[1];
    opt.outputHeader = argv[2];
    opt.outputBmp = argv[3];
    opt.fontName = argv[4];

    auto parseInt = [](const char* text, const std::string& name) {
        try {
            return std::stoi(text);
        } catch (...) {
            throw std::runtime_error("Invalid numeric value for " + name + ": " + text);
        }
    };

    // Parse optional arguments after required positional ones.
    std::vector<std::string> positionalOptional;
    for (int i = 5; i < argc; ++i) {
        const std::string arg = argv[i];
        if (!arg.empty() && arg[0] == '-') {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for option: " + arg);
            }
            const int value = parseInt(argv[++i], arg);

            if (arg == "-sourceWidth" || arg == "--sourceWidth") {
                opt.sourceWidth = value;
                opt.hasExplicitSourceSize = true;
            } else if (arg == "-sourceHeight" || arg == "--sourceHeight") {
                opt.sourceHeight = value;
                opt.hasExplicitSourceSize = true;
            } else if (arg == "-firstChar" || arg == "--firstChar") {
                opt.firstChar = value;
            } else if (arg == "-lastChar" || arg == "--lastChar") {
                opt.lastChar = value;
            } else if (arg == "-lineHeight" || arg == "--lineHeight") {
                opt.lineHeight = value;
            } else if (arg == "-topPadding" || arg == "--topPadding") {
                opt.topPadding = value;
            } else if (arg == "-baseline" || arg == "--baseline") {
                opt.baseline = value;
            } else if (arg == "-blankWidth" || arg == "--blankWidth") {
                opt.blankGlyphWidth = value;
            } else {
                throw std::runtime_error("Unknown option: " + arg);
            }
            continue;
        }
        positionalOptional.push_back(arg);
    }

    // Backward-compatible positional optional arguments.
    if (!positionalOptional.empty()) {
        if (positionalOptional.size() > 8) {
            throw std::runtime_error("Too many positional optional arguments.");
        }
        if (positionalOptional.size() > 0) {
            opt.sourceWidth = parseInt(positionalOptional[0].c_str(), "sourceWidth");
            opt.hasExplicitSourceSize = true;
        }
        if (positionalOptional.size() > 1) {
            opt.sourceHeight = parseInt(positionalOptional[1].c_str(), "sourceHeight");
            opt.hasExplicitSourceSize = true;
        }
        if (positionalOptional.size() > 2) opt.firstChar = parseInt(positionalOptional[2].c_str(), "firstChar");
        if (positionalOptional.size() > 3) opt.lastChar = parseInt(positionalOptional[3].c_str(), "lastChar");
        if (positionalOptional.size() > 4) opt.lineHeight = parseInt(positionalOptional[4].c_str(), "lineHeight");
        if (positionalOptional.size() > 5) opt.topPadding = parseInt(positionalOptional[5].c_str(), "topPadding");
        if (positionalOptional.size() > 6) opt.baseline = parseInt(positionalOptional[6].c_str(), "baseline");
        if (positionalOptional.size() > 7) opt.blankGlyphWidth = parseInt(positionalOptional[7].c_str(), "blankWidth");
    }

    if (opt.firstChar >= opt.lastChar) {
        throw std::runtime_error("firstChar must be strictly less than lastChar.");
    }

    if (opt.sourceWidth < 1 || opt.sourceHeight < 1) {
        throw std::runtime_error("sourceWidth/sourceHeight must be >= 1.");
    }
    return opt;
}

} // namespace

/// Program entry point.
///
/// Orchestrates the conversion pipeline:
///   1. Parse CLI arguments
///   2. Load the binary C64 font
///   3. Build target ASCII character mapping
///   4. Pack glyphs into optimized bitmap format
///   5. Write C header file
///   6. Write preview BMP atlas
///
/// @param argc Argument count (standard main parameter).
/// @param argv Argument array (standard main parameter).
/// @return 0 on success, 1 on any error.
int main(int argc, char **argv) {
    try {
        const Options opt = parseArgs(argc, argv);
        int sourceWidth = opt.sourceWidth;
        int sourceHeight = opt.sourceHeight;

        std::vector<GlyphBitmap> sourceGlyphs;
        if (opt.hasExplicitSourceSize) {
            sourceGlyphs = loadC64Bin(opt.inputBin, sourceWidth, sourceHeight);
        } else {
            const auto inferred = inferSourceSizeFromBin(opt.inputBin);
            sourceWidth = inferred.first;
            sourceHeight = inferred.second;
            std::cout << "Auto-detected source size: " << sourceWidth << "x" << sourceHeight << "\n";
            sourceGlyphs = loadC64Bin(opt.inputBin, sourceWidth, sourceHeight);
        }

        Options effectiveOpt = opt;
        effectiveOpt.sourceWidth = sourceWidth;
        effectiveOpt.sourceHeight = sourceHeight;

        if (effectiveOpt.lineHeight < effectiveOpt.sourceHeight) {
            throw std::runtime_error("lineHeight must be >= sourceHeight.");
        }

        const auto targetGlyphs = buildTargetGlyphs(sourceGlyphs, effectiveOpt);

        const PackedFontData fontData = buildPackedFont(targetGlyphs, effectiveOpt);
        writeHeader(opt.outputHeader, opt.fontName, fontData, opt.firstChar, opt.lastChar, opt.lineHeight);

        int previewWidth = 0;
        int previewHeight = 0;
        const auto previewBitmap = buildPreviewAtlas(targetGlyphs, effectiveOpt, previewWidth, previewHeight);
        writeBmp24(opt.outputBmp, previewBitmap, previewWidth, previewHeight);

        std::cout << "Header:        " << opt.outputHeader << "\n";
        std::cout << "BMP preview:   " << opt.outputBmp << "\n";
        std::cout << "Glyphs:        " << targetGlyphs.size() << "\n";
        std::cout << "Packed bytes:  " << fontData.bitmap.size() << "\n";
        std::cout << "Preview atlas: " << previewWidth << "x" << previewHeight << "\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
