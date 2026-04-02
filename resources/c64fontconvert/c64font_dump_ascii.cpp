#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Default dimensions for backward compatibility with existing 8x8 binary files.
constexpr int kDefaultWidth = 8;
constexpr int kDefaultHeight = 8;
constexpr int kMinGlyphDimension = 1;
constexpr int kMaxGlyphDimension = 128;

struct CliOptions {
    std::string inputPath;
    int width = kDefaultWidth;
    int height = kDefaultHeight;
    bool hasExplicitSize = false;
};

/// Computes the minimum number of bytes required to encode one bitmap row.
///
/// For example, a 5-pixel wide row requires (5 + 7) / 8 = 1 byte (8 bits).
/// A 12-pixel wide row requires (12 + 7) / 8 = 2 bytes (16 bits).
///
/// @param width The width of the glyph in pixels.
/// @return Number of bytes needed per row.
static size_t bytesPerRow(const int width) {
    return static_cast<size_t>((width + 7) / 8);
}

/// Validates that glyph dimensions are within acceptable parser limits.
///
/// This ensures that both width and height are within safe bounds for an embedded
/// system with limited memory.
///
/// @param width   The glyph width in pixels.
/// @param height  The glyph height in pixels.
/// @param context A descriptive string (e.g., "CLI size") for error reporting.
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
/// @param context A descriptive label for error reporting (e.g., "--size").
/// @return A pair (width, height) with both values already validated.
/// @throws std::runtime_error if format is invalid, non-numeric, or dimensions out of range.
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

    validateDimensions(width, height, context);
    return {width, height};
}

/// Prints CLI usage and help information to stdout.
///
/// Displays the complete command-line interface documentation, including available
/// options and usage examples.
///
/// @param programName The name of the executable (typically argv[0]) to display in usage line.
static void printUsage(const char* programName) {
    std::cout << "Usage:\n"
              << "  " << programName << " [options] <font.bin>\n\n"
              << "Options:\n"
              << "  -s, --size WxH      Set glyph size (example: 5x5, 8x8, 12x12)\n"
              << "  -w, --width N       Set glyph width in pixels\n"
              << "  -h, --height N      Set glyph height in pixels\n"
              << "  --help              Show this help\n\n"
              << "Notes:\n"
              << "  - Default size is 8x8 (backward compatible with legacy .bin files).\n"
              << "  - Binary file format: dense table from glyph 0 to maxIndex,\n"
              << "    with each glyph occupying (width+7)/8 * height bytes.\n"
              << "  - Glyphs are displayed in MSB-first bit order (left-to-right).\n";
}

/// Parses command line arguments and validates the resulting options.
///
/// Processes all arguments, extracting the positional input file path and optional
/// size-related flags. Supports both long-form (--flag) and short-form (-f) syntax.
/// If --help is provided, prints usage and exits immediately.
///
/// Recognized flags:
///   - --help              Prints help and exits
///   - -s, --size WxH      Sets both width and height
///   - -w, --width N       Sets width only
///   - -h, --height N      Sets height only
///
/// Positional argument:
///   - First positional: input binary file path
///
/// @param argc   Argument count (typically from main()).
/// @param argv   Array of argument pointers (typically from main()).
/// @return CliOptions with validated path and dimensions.
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
        } else {
            throw std::runtime_error("Too many positional arguments.");
        }
    }

    validateDimensions(options.width, options.height, "CLI size");

    if (options.inputPath.empty()) {
        throw std::runtime_error("Missing required positional argument: <font.bin>");
    }

    return options;
}

/// Reads a binary font file into memory.
///
/// Opens and reads the entire binary file into a vector. The file should contain
/// packed glyph data generated by glyphtext_to_bin.
///
/// @param path Path to the binary font file.
/// @return A vector of bytes containing the entire file contents.
/// @throws std::runtime_error if the file cannot be opened or read.
static std::vector<uint8_t> readBinaryFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Could not open input file: " + path);
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    return data;
}

/// Validates that the binary file size is compatible with the specified glyph dimensions.
///
/// Checks that the total file size is a multiple of glyphStride. If not, the file is
/// likely corrupted or was generated with different dimensions.
///
/// @param fileSize     Total size of the binary file in bytes.
/// @param width        Glyph width in pixels.
/// @param height       Glyph height in pixels.
/// @throws std::runtime_error if file size is incompatible with the dimensions.
static void validateFileSize(const size_t fileSize, const int width, const int height) {
    const size_t rowStride = bytesPerRow(width);
    const size_t glyphStride = rowStride * static_cast<size_t>(height);

    if (fileSize % glyphStride != 0) {
        std::ostringstream oss;
        oss << "File size (" << fileSize << " bytes) is not a multiple of glyphStride ("
            << glyphStride << " bytes = " << rowStride << " bytes/row * " << height << " rows)";
        throw std::runtime_error(oss.str());
    }
}

/// Renders one glyph from binary data and prints it as ASCII art.
///
/// Extracts the glyph at the given index from the binary data and prints each row
/// as a line of '*' (set bits) and '-' (clear bits).
///
/// @param glyphIndex  The index of the glyph to render (0-based).
/// @param data        The complete binary font file data.
/// @param width       Glyph width in pixels.
/// @param height      Glyph height in pixels.
static void dumpGlyph(const size_t glyphIndex, const std::vector<uint8_t>& data,
                      const int width, const int height) {
    const size_t rowStride = bytesPerRow(width);
    const size_t glyphStride = rowStride * static_cast<size_t>(height);
    const size_t glyphOffset = glyphIndex * glyphStride;

    std::cout << "Glyph " << glyphIndex << ":\n";

    // Iterate through each row of the glyph.
    for (int row = 0; row < height; ++row) {
        const size_t rowOffset = glyphOffset + static_cast<size_t>(row) * rowStride;

        // Iterate through each pixel in the row.
        for (int pixel = 0; pixel < width; ++pixel) {
            const size_t byteIndex = rowOffset + static_cast<size_t>(pixel / 8);
            const int bitPos = 7 - (pixel % 8);

            // Extract bit at MSB-first position.
            const bool set = (data[byteIndex] >> bitPos) & 1;
            std::cout << (set ? '*' : '-');
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

} // namespace

/// Program entry point.
///
/// Orchestrates the three main steps:
///   1. Parse CLI arguments and extract user options
///   2. Read the binary font file into memory
///   3. Validate and dump each glyph as ASCII art
///
/// On success, returns 0. On failure, prints an error message and usage help, returning 1.
///
/// @param argc Argument count (standard main parameter).
/// @param argv Argument array (standard main parameter).
/// @return 0 on successful dump, 1 on any error.
int main(int argc, char** argv) {
    try {
        const CliOptions options = parseArgs(argc, argv);
        const std::vector<uint8_t> data = readBinaryFile(options.inputPath);

        validateFileSize(data.size(), options.width, options.height);

        const size_t rowStride = bytesPerRow(options.width);
        const size_t glyphStride = rowStride * static_cast<size_t>(options.height);
        const size_t glyphCount = data.size() / glyphStride;

        std::cout << "Binary file: " << options.inputPath << "\n";
        std::cout << "Glyph size: " << options.width << "x" << options.height << "\n";
        std::cout << "Bytes per row: " << rowStride << "\n";
        std::cout << "Bytes per glyph: " << glyphStride << "\n";
        std::cout << "Total glyphs: " << glyphCount << "\n\n";

        for (size_t g = 0; g < glyphCount; ++g) {
            dumpGlyph(g, data, options.width, options.height);
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n\n";
        printUsage(argv[0]);
        return 1;
    }
}