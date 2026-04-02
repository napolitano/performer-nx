#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Glyph metadata structure (matches BitmapFont.h)
struct BitmapFontGlyph {
    uint16_t offset;
    uint8_t width, height;
    uint8_t xAdvance;
    int8_t xOffset, yOffset;
};

// Font descriptor structure (matches BitmapFont.h)
struct BitmapFont {
    uint8_t bpp;
    // bitmap and glyphs pointers are not included in this struct for parsing
    uint8_t firstChar;
    uint8_t lastChar;
    uint8_t yAdvance;
};

struct GlyphTextLine {
    int glyphIndex;
    std::vector<std::string> rows;  // Visual representation: rows of '*' and '-'
};

struct ParsedHeader {
    std::vector<uint8_t> bitmap;
    std::vector<BitmapFontGlyph> glyphs;
    BitmapFont font;
};

struct CliOptions {
    std::string inputPath;
    std::string outputPath;
    int targetWidth = -1;  // -1 means infer from max source glyph width
    int targetHeight = -1; // -1 means infer from max source glyph height
};

/// Trims whitespace from both ends of a string.
static std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

/// Extracts all C array declarations and their values from a header file.
/// Returns pairs of (array_name, array_data_string)
static std::vector<std::pair<std::string, std::string>> extractCArrays(const std::string& content) {
    std::vector<std::pair<std::string, std::string>> arrays;

    // Match: static uint8_t name[] = { ... };
    std::regex arrayPattern(R"(static\s+uint8_t\s+(\w+)\s*\[\s*\]\s*=\s*\{([^}]+)\})");
    std::sregex_iterator it(content.begin(), content.end(), arrayPattern);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        const std::string name = (*it)[1].str();
        const std::string data = (*it)[2].str();
        arrays.emplace_back(name, data);
    }

    return arrays;
}

/// Parses a hex string (comma-separated 0xNN values) into a uint8_t vector.
static std::vector<uint8_t> parseHexArray(const std::string& hexStr) {
    std::vector<uint8_t> result;
    std::istringstream iss(hexStr);
    std::string token;

    while (std::getline(iss, token, ',')) {
        const std::string trimmed = trim(token);
        if (trimmed.empty()) {
            continue;
        }
        try {
            result.push_back(static_cast<uint8_t>(std::stoul(trimmed, nullptr, 0)));
        } catch (...) {
            throw std::runtime_error("Failed to parse hex value: " + trimmed);
        }
    }

    return result;
}

/// Extracts glyph array metadata from a header file.
/// Looks for: static BitmapFontGlyph name[] = { { ... }, ... };
static std::vector<BitmapFontGlyph> extractGlyphArray(const std::string& content) {
    std::vector<BitmapFontGlyph> glyphs;

    // Match: { offset, width, height, xAdvance, xOffset, yOffset }
    std::regex glyphPattern(R"(\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\})");
    std::sregex_iterator it(content.begin(), content.end(), glyphPattern);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        BitmapFontGlyph glyph{};
        glyph.offset = static_cast<uint16_t>(std::stoul((*it)[1].str()));
        glyph.width = static_cast<uint8_t>(std::stoul((*it)[2].str()));
        glyph.height = static_cast<uint8_t>(std::stoul((*it)[3].str()));
        glyph.xAdvance = static_cast<uint8_t>(std::stoul((*it)[4].str()));
        glyph.xOffset = static_cast<int8_t>(std::stoi((*it)[5].str()));
        glyph.yOffset = static_cast<int8_t>(std::stoi((*it)[6].str()));
        glyphs.push_back(glyph);
    }

    return glyphs;
}

/// Extracts font descriptor values from the BitmapFont struct.
/// Looks for: static BitmapFont name = { bpp, ..., glyphs, first, last, yAdvance };
static BitmapFont extractFontStruct(const std::string& content) {
    BitmapFont font{};

    // Match: { bpp, bitmap, glyphs, first, last, yAdvance }
    std::regex fontPattern(R"(static\s+BitmapFont\s+\w+\s*=\s*\{\s*(\d+)\s*,\s*\w+\s*,\s*\w+\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\})");
    std::smatch match;

    if (std::regex_search(content, match, fontPattern)) {
        font.bpp = static_cast<uint8_t>(std::stoul(match[1].str()));
        font.firstChar = static_cast<uint8_t>(std::stoul(match[2].str()));
        font.lastChar = static_cast<uint8_t>(std::stoul(match[3].str()));
        font.yAdvance = static_cast<uint8_t>(std::stoul(match[4].str()));
    } else {
        throw std::runtime_error("Could not parse BitmapFont struct");
    }

    return font;
}

/// Reads and parses a C header file containing BitmapFont data.
static ParsedHeader parseHeaderFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Could not open input file: " + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string content = buffer.str();

    ParsedHeader parsed{};

    // Extract bitmap array
    const auto arrays = extractCArrays(content);
    if (arrays.empty()) {
        throw std::runtime_error("No bitmap array found in header file");
    }
    parsed.bitmap = parseHexArray(arrays[0].second);

    // Extract glyph array
    parsed.glyphs = extractGlyphArray(content);
    if (parsed.glyphs.empty()) {
        throw std::runtime_error("No glyph array found in header file");
    }

    // Extract font struct
    parsed.font = extractFontStruct(content);

    // Validate
    if (parsed.font.bpp != 1) {
        throw std::runtime_error("Only 1-bit-per-pixel fonts are supported, got: " +
                                 std::to_string(parsed.font.bpp));
    }

    return parsed;
}

/// Converts a glyph bitmap to visual representation.
/// BitmapFont glyphs are packed as a linear LSB-first bitstream starting at glyph.offset bytes.
static std::vector<std::string> extractGlyphVisual(
    const std::vector<uint8_t>& bitmap,
    const BitmapFontGlyph& glyph) {

    std::vector<std::string> rows;
    if (glyph.width == 0 || glyph.height == 0) {
        return rows;
    }

    const size_t startBit = static_cast<size_t>(glyph.offset) * 8u;
    const size_t glyphBits = static_cast<size_t>(glyph.width) * static_cast<size_t>(glyph.height);
    const size_t lastBit = startBit + glyphBits - 1u;
    if (lastBit / 8u >= bitmap.size()) {
        throw std::runtime_error("Bitmap offset out of bounds");
    }

    for (int y = 0; y < glyph.height; ++y) {
        std::string row;
        for (int x = 0; x < glyph.width; ++x) {
            const size_t bitIndex = startBit +
                                    static_cast<size_t>(y) * static_cast<size_t>(glyph.width) +
                                    static_cast<size_t>(x);
            const size_t byteIndex = bitIndex / 8u;
            const int bitPos = static_cast<int>(bitIndex % 8u);
            const uint8_t bit = (bitmap[byteIndex] >> bitPos) & 1;
            row += (bit ? '*' : '-');
        }
        rows.push_back(row);
    }

    return rows;
}

/// Centers a glyph row within a target width.
static std::string centerRow(const std::string& row, int targetWidth) {
    if (static_cast<int>(row.length()) >= targetWidth) {
        return row;
    }

    const int padding = targetWidth - static_cast<int>(row.length());
    const int leftPad = padding / 2;
    const int rightPad = padding - leftPad;

    return std::string(leftPad, '-') + row + std::string(rightPad, '-');
}

/// Centers and pads a glyph within target dimensions using '-' for empty pixels.
/// The source glyph rows are distributed evenly: extra rows split above/below,
/// extra columns split left/right. The output must be >= source dimensions.
static std::vector<std::string> padGlyph(
    const std::vector<std::string>& rows,
    int targetWidth,
    int targetHeight) {

    std::vector<std::string> result;

    // Vertical centering: extra rows go above (floor-half) and below (ceil-half).
    const int topPad = (targetHeight - static_cast<int>(rows.size())) / 2;
    for (int i = 0; i < topPad; ++i) {
        result.push_back(std::string(targetWidth, '-'));
    }

    // Horizontal centering applied to each source row.
    for (const auto& row : rows) {
        result.push_back(centerRow(row, targetWidth));
    }

    // Fill any remaining rows at the bottom.
    while (static_cast<int>(result.size()) < targetHeight) {
        result.push_back(std::string(targetWidth, '-'));
    }

    return result;
}

/// Prompts user for input and returns the result.
static std::string promptUser(const std::string& prompt) {
    std::cout << prompt;
    std::cout.flush();
    std::string input;
    std::getline(std::cin, input);
    return input;
}

/// Prompts user for integer input with validation.
static int promptInteger(const std::string& prompt, int minVal = 1, int maxVal = 128) {
    while (true) {
        const std::string input = promptUser(prompt);
        try {
            const int value = std::stoi(input);
            if (value < minVal || value > maxVal) {
                std::cerr << "Error: Value must be between " << minVal << " and " << maxVal << "\n";
                continue;
            }
            return value;
        } catch (...) {
            std::cerr << "Error: Invalid integer input\n";
        }
    }
}

/// Writes the glyphtext file.
static void writeGlyphTextFile(
    const std::string& path,
    const std::vector<GlyphTextLine>& glyphs,
    int width,
    int height) {

    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Could not open output file: " + path);
    }

    out << "Size: " << width << "x" << height << "\n\n";

    for (const auto& glyph : glyphs) {
        out << "Glyph " << glyph.glyphIndex << ":\n";
        for (const auto& row : glyph.rows) {
            out << row << "\n";
        }
        out << "\n";
    }
}

/// Prints usage information.
static void printUsage(const char* programName) {
    std::cout << "Usage:\n"
              << "  " << programName << " [options] <input.h> <output.txt>\n\n"
              << "Options:\n"
              << "  -w, --width N       Set output glyph width (default: inferred max source width)\n"
              << "  -h, --height N      Set output glyph height (default: inferred max source height)\n"
              << "  --help              Show this help\n\n"
              << "Description:\n"
              << "  Converts a BitmapFont C header file to human-editable glyph text format.\n"
              << "  Source glyphs are centered in the output cell. Output size must be >= any source glyph.\n";
}

/// Parses command-line arguments.
static CliOptions parseArgs(const int argc, char** argv) {
    CliOptions options{};

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        }
        if (arg == "--width" || arg == "-w") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for --width");
            }
            try {
                options.targetWidth = std::stoi(argv[++i]);
            } catch (...) {
                throw std::runtime_error("Invalid numeric value for --width");
            }
            continue;
        }
        if (arg == "--height" || arg == "-h") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for --height");
            }
            try {
                options.targetHeight = std::stoi(argv[++i]);
            } catch (...) {
                throw std::runtime_error("Invalid numeric value for --height");
            }
            continue;
        }
        if (arg == "--center" || arg == "-c") {
            // Centering is now always applied; this flag is accepted but has no effect.
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

    if (options.inputPath.empty() || options.outputPath.empty()) {
        throw std::runtime_error("Missing required arguments: <input.h> <output.txt>");
    }

    return options;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const CliOptions options = parseArgs(argc, argv);

        std::cout << "Parsing header file: " << options.inputPath << "\n";
        const ParsedHeader parsed = parseHeaderFile(options.inputPath);

        std::cout << "Found " << parsed.glyphs.size() << " glyphs\n";
        std::cout << "Font: first=" << static_cast<int>(parsed.font.firstChar)
                  << ", last=" << static_cast<int>(parsed.font.lastChar) << "\n";

        // Determine output dimensions
        int outWidth = options.targetWidth;
        int outHeight = options.targetHeight;

        // Use max source glyph dimensions as defaults if needed.
        if (outWidth == -1 || outHeight == -1) {
            if (!parsed.glyphs.empty()) {
                int maxWidth = 0;
                int maxHeight = 0;
                for (const auto& glyph : parsed.glyphs) {
                    maxWidth = std::max(maxWidth, static_cast<int>(glyph.width));
                    maxHeight = std::max(maxHeight, static_cast<int>(glyph.height));
                }
                if (outWidth == -1) {
                    outWidth = maxWidth;
                }
                if (outHeight == -1) {
                    outHeight = maxHeight;
                }
            }
        }

        // Prompt user if still not determined
        if (outWidth == -1) {
            outWidth = promptInteger("Enter output glyph width [1-128]: ");
        }
        if (outHeight == -1) {
            outHeight = promptInteger("Enter output glyph height [1-128]: ");
        }

        if (outWidth < 1 || outWidth > 128 || outHeight < 1 || outHeight > 128) {
            throw std::runtime_error("Output dimensions must be in range [1, 128]");
        }

        std::cout << "Output dimensions: " << outWidth << "x" << outHeight << "\n";

        // Extract and convert glyphs
        std::vector<GlyphTextLine> glyphLines;

        for (size_t i = 0; i < parsed.glyphs.size(); ++i) {
            const int charCode = static_cast<int>(parsed.font.firstChar) + static_cast<int>(i);
            const auto& glyph = parsed.glyphs[i];

            if (glyph.width > outWidth || glyph.height > outHeight) {
                throw std::runtime_error(
                    "Glyph " + std::to_string(charCode) +
                    " does not fit output size " + std::to_string(outWidth) + "x" + std::to_string(outHeight) +
                    " (glyph is " + std::to_string(glyph.width) + "x" + std::to_string(glyph.height) + ")");
            }

            auto visual = extractGlyphVisual(parsed.bitmap, glyph);
            auto padded = padGlyph(visual, outWidth, outHeight);

            glyphLines.push_back({charCode, padded});
        }

        std::cout << "Converted " << glyphLines.size() << " glyphs\n";

        // Write output
        writeGlyphTextFile(options.outputPath, glyphLines, outWidth, outHeight);
        std::cout << "Wrote: " << options.outputPath << "\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n\n";
        printUsage(argc > 0 ? argv[0] : "bitmapfont_to_glyphtext");
        return 1;
    }
}

