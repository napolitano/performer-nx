#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

// Default behavior remains backward compatible with existing 8x8 assets.
constexpr int kDefaultWidth = 8;
constexpr int kDefaultHeight = 8;
constexpr int kMinGlyphDimension = 1;
constexpr int kMaxGlyphDimension = 128;

struct CliOptions {
    std::string inputPath;
    std::string outputPath;
    int width = kDefaultWidth;
    int height = kDefaultHeight;
    bool hasExplicitSize = false;
};

struct GlyphBitmap {
    // Packed row-major bitmap data.
    // Row size is bytesPerRow(width), and each bit maps left-to-right in MSB-first order.
    std::vector<uint8_t> bytes;
};

struct ParsedFont {
    std::map<int, GlyphBitmap> glyphs;
    int width = kDefaultWidth;
    int height = kDefaultHeight;
};

/// Removes leading and trailing whitespace characters from a string.
///
/// @param s Input string to trim. May contain spaces, tabs, carriage returns, or newlines.
/// @return A new string with leading/trailing whitespace removed. If the entire string
///         is whitespace, returns an empty string.
static std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

/// Checks whether a string starts with a given prefix.
///
/// @param s          The string to check.
/// @param prefix     The prefix to search for at the beginning of s.
/// @return true if s begins with prefix; false otherwise.
static bool startsWith(const std::string& s, const std::string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

/// Computes the minimum number of bytes required to encode one bitmap row.
///
/// For example, a 5-pixel wide row requires (5 + 7) / 8 = 1 byte (8 bits).
/// A 12-pixel wide row requires (12 + 7) / 8 = 2 bytes (16 bits).
/// The unused bits in the final byte are zero-padded.
///
/// @param width The width of the glyph in pixels.
/// @return Number of bytes needed per row.
static size_t bytesPerRow(const int width) {
    return static_cast<size_t>((width + 7) / 8);
}

/// Validates that glyph dimensions are within acceptable parser limits.
///
/// This function checks both width and height against minimum and maximum constraints.
/// An embedded target with limited memory should not accept extremely large glyphs.
///
/// @param width   The glyph width in pixels.
/// @param height  The glyph height in pixels.
/// @param context A descriptive string (e.g., "CLI size" or "Line 42") for error reporting.
/// @throws std::runtime_error if width or height is out of range.
static void validateDimensions(const int width, const int height, const std::string& context) {
    if (width < kMinGlyphDimension || width > kMaxGlyphDimension) {
        throw std::runtime_error(context + ": width out of range [" +
                                 std::to_string(kMinGlyphDimension) + ", " +
                                 std::to_string(kMaxGlyphDimension) + "]: " +
                                 std::to_string(width));
    }
    if (height < kMinGlyphDimension || height > kMaxGlyphDimension) {
        throw std::runtime_error(context + ": height out of range [" +
                                 std::to_string(kMinGlyphDimension) + ", " +
                                 std::to_string(kMaxGlyphDimension) + "]: " +
                                 std::to_string(height));
    }
}

/// Parses a size specification string in the format "WxH" and validates the result.
///
/// Accepts either lowercase 'x' or uppercase 'X' as the separator.
/// Example inputs: "5x5", "8X8", "12x16".
///
/// @param value   The size string to parse (e.g., "12x12").
/// @param context A descriptive label for error reporting (e.g., "Line 5" or "--size").
/// @return A pair (width, height) with both values already validated.
/// @throws std::runtime_error if format is invalid, non-numeric, or dimensions out of range.
static std::pair<int, int> parseSizeValue(const std::string& value, const std::string& context) {
    const auto xPos = value.find_first_of("xX");
    if (xPos == std::string::npos) {
        throw std::runtime_error(context + ": expected size format WxH, got: " + value);
    }

    const std::string widthText = trim(value.substr(0, xPos));
    const std::string heightText = trim(value.substr(xPos + 1));
    if (widthText.empty() || heightText.empty()) {
        throw std::runtime_error(context + ": expected size format WxH, got: " + value);
    }

    int width = 0;
    int height = 0;
    try {
        width = std::stoi(widthText);
        height = std::stoi(heightText);
    } catch (...) {
        throw std::runtime_error(context + ": invalid numeric size: " + value);
    }

    validateDimensions(width, height, context);
    return {width, height};
}

/// Extracts and validates the glyph index from a "Glyph N:" line.
///
/// Expected format: "Glyph 123:" where 123 is a non-negative integer.
///
/// @param line A trimmed input line to parse (e.g., "Glyph 5:").
/// @return The glyph index N as an integer.
/// @throws std::runtime_error if the line is malformed, missing ':', or index is non-numeric.
static int parseGlyphIndex(const std::string& line) {
    if (!startsWith(line, "Glyph ")) {
        throw std::runtime_error("Expected 'Glyph N:' line, got: " + line);
    }

    const auto colon = line.find(':');
    if (colon == std::string::npos) {
        throw std::runtime_error("Missing ':' in glyph header: " + line);
    }

    const std::string number = trim(line.substr(6, colon - 6));
    if (number.empty()) {
        throw std::runtime_error("Missing glyph index in line: " + line);
    }

    try {
        return std::stoi(number);
    } catch (...) {
        throw std::runtime_error("Invalid glyph index in line: " + line);
    }
}

/// Converts one visual bitmap row (sequence of '*' and '-') into packed bytes.
///
/// Each '*' becomes a set bit (1), and each '-' becomes a clear bit (0).
/// Bits are packed MSB-first (left-to-right) within each byte.
/// Unused bits in the final byte are zero-padded.
///
/// @param row        A visual row string with exactly 'width' characters.
/// @param width      The expected number of pixels/characters in this row.
/// @param glyphIndex The glyph index being parsed (for error messages).
/// @param rowIndex   The row number within this glyph (for error messages).
/// @return A vector of packed bytes, size = bytesPerRow(width).
/// @throws std::runtime_error if row size mismatches width or contains invalid characters.
static std::vector<uint8_t> parseRow(
    const std::string& row,
    const int width,
    const int glyphIndex,
    const int rowIndex) {
    if (static_cast<int>(row.size()) != width) {
        std::ostringstream oss;
        oss << "Glyph " << glyphIndex << ", row " << rowIndex
            << ": expected exactly " << width << " characters, got " << row.size();
        throw std::runtime_error(oss.str());
    }

    std::vector<uint8_t> packed(bytesPerRow(width), 0);
    for (int x = 0; x < width; ++x) {
        const char c = row[static_cast<size_t>(x)];
        if (c == '*') {
            // MSB-first layout keeps visual left-to-right order in the byte stream.
            const size_t byteIndex = static_cast<size_t>(x / 8);
            const int bitPos = 7 - (x % 8);
            packed[byteIndex] |= static_cast<uint8_t>(1u << bitPos);
        } else if (c != '-') {
            std::ostringstream oss;
            oss << "Glyph " << glyphIndex << ", row " << rowIndex
                << ": invalid character '" << c << "', only '*' and '-' are allowed";
            throw std::runtime_error(oss.str());
        }
    }

    return packed;
}

/// Parses an input glyph text file into a sparse glyph map and determines dimensions.
///
/// This function reads all glyph blocks from a text file, validates their structure,
/// and stores packed bitmap data for each glyph index. It supports an optional
/// "Size: WxH" file header that can be overridden by CLI options.
///
/// File format:
///   - Optional header: "Size: WxH"
///   - Glyph blocks: "Glyph N:" followed by height non-empty rows of exactly width characters
///   - Empty lines are skipped
///
/// @param path    Path to the input glyph text file.
/// @param options CLI options that may override file size (if options.hasExplicitSize is true).
/// @return ParsedFont structure containing the sparse glyph map and final dimensions.
/// @throws std::runtime_error on file I/O error, malformed glyph data, or validation failure.
static ParsedFont parseGlyphFile(const std::string& path, const CliOptions& options) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Could not open input file: " + path);
    }

    ParsedFont parsed{};
    parsed.width = options.width;
    parsed.height = options.height;

    std::string line;
    int lineNumber = 0;
    bool sizeHeaderSeen = false;

    while (std::getline(in, line)) {
        ++lineNumber;
        const std::string t = trim(line);
        if (t.empty()) {
            continue;
        }

        if (startsWith(t, "Size:")) {
            if (sizeHeaderSeen) {
                throw std::runtime_error("Line " + std::to_string(lineNumber) +
                                         ": duplicate Size header.");
            }
            sizeHeaderSeen = true;

            if (!options.hasExplicitSize) {
                const auto [fileWidth, fileHeight] =
                    parseSizeValue(trim(t.substr(5)), "Line " + std::to_string(lineNumber));
                parsed.width = fileWidth;
                parsed.height = fileHeight;
            }
            continue;
        }

        if (!startsWith(t, "Glyph ")) {
            std::ostringstream oss;
            oss << "Line " << lineNumber << ": expected 'Glyph N:' header, got: " << t;
            throw std::runtime_error(oss.str());
        }

        const int glyphIndex = parseGlyphIndex(t);
        if (glyphIndex < 0) {
            throw std::runtime_error("Negative glyph index is not allowed.");
        }
        if (parsed.glyphs.find(glyphIndex) != parsed.glyphs.end()) {
            throw std::runtime_error("Duplicate glyph index is not allowed: " +
                                     std::to_string(glyphIndex));
        }

        GlyphBitmap glyph{};
        const size_t rowStride = bytesPerRow(parsed.width);
        glyph.bytes.reserve(rowStride * static_cast<size_t>(parsed.height));

        int rowsRead = 0;
        while (rowsRead < parsed.height && std::getline(in, line)) {
            ++lineNumber;
            const std::string row = trim(line);
            if (row.empty()) {
                continue;
            }

            const auto packedRow = parseRow(row, parsed.width, glyphIndex, rowsRead);
            glyph.bytes.insert(glyph.bytes.end(), packedRow.begin(), packedRow.end());
            ++rowsRead;
        }

        if (rowsRead != parsed.height) {
            std::ostringstream oss;
            oss << "Glyph " << glyphIndex << ": expected " << parsed.height
                << " bitmap rows, got " << rowsRead;
            throw std::runtime_error(oss.str());
        }

        parsed.glyphs[glyphIndex] = std::move(glyph);
    }

    return parsed;
}

/// Writes sparse glyphs into a dense fixed-stride binary table.
///
/// This function takes a sparse map of glyphs (not all indices may be present)
/// and outputs a contiguous binary table from index 0 through maxIndex.
/// Missing indices are filled with zero bytes.
///
/// Output layout:
///   - Glyph 0 at offset 0 * glyphStride, size glyphStride bytes
///   - Glyph 1 at offset 1 * glyphStride, size glyphStride bytes
///   - ...
///   - Glyph maxIndex at offset maxIndex * glyphStride, size glyphStride bytes
///
/// @param path Path to the output binary file to create.
/// @param font ParsedFont structure containing glyphs and dimensions.
/// @throws std::runtime_error on file I/O error or internal consistency failure.
/// @note TODO: Consider streaming output for very sparse/high-index fonts to avoid large allocations.
static void writeBin(const std::string& path, const ParsedFont& font) {
    if (font.glyphs.empty()) {
        throw std::runtime_error("No glyphs found.");
    }

    const int maxIndex = font.glyphs.rbegin()->first;
    const size_t rowStride = bytesPerRow(font.width);
    const size_t glyphStride = rowStride * static_cast<size_t>(font.height);

    // Allocate dense output table; missing glyphs will be zero-filled.
    std::vector<uint8_t> bin(static_cast<size_t>(maxIndex + 1) * glyphStride, 0);

    for (const auto& [index, glyph] : font.glyphs) {
        const size_t base = static_cast<size_t>(index) * glyphStride;
        if (glyph.bytes.size() != glyphStride) {
            throw std::runtime_error("Internal error: glyph byte size mismatch.");
        }
        std::copy(glyph.bytes.begin(), glyph.bytes.end(), bin.begin() + static_cast<std::ptrdiff_t>(base));
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Could not open output BIN file: " + path);
    }

    out.write(reinterpret_cast<const char*>(bin.data()), static_cast<std::streamsize>(bin.size()));
    if (!out) {
        throw std::runtime_error("Failed while writing output BIN file: " + path);
    }
}

/// Prints formatted CLI usage and help information to stdout.
///
/// This function displays the complete command-line interface documentation,
/// including available options, input format, and usage examples.
///
/// @param programName The name of the executable (typically argv[0]) to display in usage line.
static void printUsage(const char* programName) {
    std::cout << "Usage:\n"
              << "  " << programName << " [options] <input.txt> <output.bin>\n\n"
              << "Options:\n"
              << "  -s, --size WxH      Set glyph size (example: 5x5, 12x12)\n"
              << "  -w, --width N       Set glyph width in pixels\n"
              << "  -h, --height N      Set glyph height in pixels\n"
              << "  --help              Show this help\n\n"
              << "Input format:\n"
              << "  Optional header: Size: WxH\n"
              << "  Glyph blocks: Glyph N: followed by H rows of '-' and '*'\n\n"
              << "Notes:\n"
              << "  - CLI size overrides optional Size header in file.\n"
              << "  - Row bytes are packed MSB-first; final partial byte is zero-padded.\n";
}

/// Parses command line arguments and validates the resulting options.
///
/// This function processes all arguments, extracting positional file paths and
/// optional size-related flags. It supports both long-form (--flag) and short-form (-f)
/// option syntax. If --help is provided, the function prints usage and exits immediately.
///
/// Recognized flags:
///   - --help                    Prints help and exits
///   - -s, --size WxH            Sets both width and height
///   - -w, --width N             Sets width only
///   - -h, --height N            Sets height only
///
/// Positional arguments:
///   - First positional:  input file path
///   - Second positional: output file path
///
/// @param argc   Argument count (typically from main()).
/// @param argv   Array of argument pointers (typically from main()).
/// @return CliOptions with validated paths and dimensions.
/// @throws std::runtime_error on missing/invalid arguments, unknown options, or dimension validation failure.
static CliOptions parseArgs(const int argc, char** argv) {
    CliOptions options{};

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        }
        if (arg == "--size" || arg == "-s") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for --size");
            }
            const auto [w, h] = parseSizeValue(argv[++i], "--size");
            options.width = w;
            options.height = h;
            options.hasExplicitSize = true;
            continue;
        }
        if (arg == "--width" || arg == "-w") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for --width");
            }
            try {
                options.width = std::stoi(argv[++i]);
            } catch (...) {
                throw std::runtime_error("Invalid numeric value for --width");
            }
            options.hasExplicitSize = true;
            continue;
        }
        if (arg == "--height" || arg == "-h") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for --height");
            }
            try {
                options.height = std::stoi(argv[++i]);
            } catch (...) {
                throw std::runtime_error("Invalid numeric value for --height");
            }
            options.hasExplicitSize = true;
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            throw std::runtime_error("Unknown option: " + arg);
        }

        if (options.inputPath.empty()) {
            options.inputPath = arg;
        } else if (options.outputPath.empty()) {
            options.outputPath = arg;
        } else {
            throw std::runtime_error("Too many positional arguments.");
        }
    }

    validateDimensions(options.width, options.height, "CLI size");

    if (options.inputPath.empty() || options.outputPath.empty()) {
        throw std::runtime_error("Missing required positional arguments: <input.txt> <output.bin>");
    }

    return options;
}

} // namespace

/// Program entry point.
///
/// Orchestrates the three main steps of conversion:
///   1. Parse CLI arguments and extract user options
///   2. Parse the input glyph text file
///   3. Write the binary output
///
/// On success, prints summary information and returns 0.
/// On failure, prints an error message and usage help, returning 1.
///
/// @param argc Argument count (standard main parameter).
/// @param argv Argument array (standard main parameter).
/// @return 0 on successful conversion, 1 on any error.
int main(int argc, char** argv) {
    try {
        const CliOptions options = parseArgs(argc, argv);
        const ParsedFont font = parseGlyphFile(options.inputPath, options);
        writeBin(options.outputPath, font);

        std::cout << "Parsed glyphs: " << font.glyphs.size() << "\n";
        std::cout << "Glyph size: " << font.width << "x" << font.height << "\n";
        std::cout << "Max glyph index: " << font.glyphs.rbegin()->first << "\n";
        std::cout << "Bytes per row: " << bytesPerRow(font.width) << "\n";
        std::cout << "Bytes per glyph: " << bytesPerRow(font.width) * static_cast<size_t>(font.height)
                  << "\n";
        std::cout << "Wrote BIN: " << options.outputPath << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n\n";
        printUsage(argv[0]);
        return 1;
    }
}
