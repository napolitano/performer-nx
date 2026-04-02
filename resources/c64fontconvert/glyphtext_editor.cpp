#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <ncurses.h>

namespace {

namespace fs = std::filesystem;

// Default/fallback document size when no glyph data exists yet.
constexpr int kDefaultWidth = 8;
constexpr int kDefaultHeight = 8;
// Hard bounds prevent accidental oversized allocations in terminal workflows.
constexpr int kMinGlyphDimension = 1;
constexpr int kMaxGlyphDimension = 128;

constexpr int kGlyphListColumns = 10;

enum ColorPairId {
    kPairEditorBg = 1,
    kPairEditorSet = 2,
    kPairCursorOff = 3,
    kPairCursorOn = 4,
    kPairUndefinedIndex = 5,
};

static bool gUseColors = false;
static short gGrayColor = COLOR_BLACK;

/// Initializes terminal colors used by the editor.
static void initUiColors() {
    if (!has_colors()) {
        gUseColors = false;
        return;
    }

    start_color();
    use_default_colors();
    gUseColors = true;

    // Try a custom gray when terminal allows redefining colors.
    if (COLORS > 8 && can_change_color()) {
        gGrayColor = 8;
        // Dark gray for the editable matrix background.
        init_color(gGrayColor, 140, 140, 140);
    } else {
        // Fallback to black if gray cannot be configured.
        gGrayColor = COLOR_BLACK;
    }

    // For space-based rendering the background color is what becomes visible.
    init_pair(kPairEditorBg, COLOR_BLACK, gGrayColor);
    init_pair(kPairEditorSet, COLOR_BLACK, COLOR_WHITE);
    init_pair(kPairCursorOff, COLOR_BLACK, COLOR_YELLOW);
    init_pair(kPairCursorOn, COLOR_BLACK, COLOR_YELLOW);
    init_pair(kPairUndefinedIndex, gGrayColor, -1);
}

/// Represents one glyph as a packed bitmap with dynamic dimensions.
struct GlyphBitmap {
    int width = kDefaultWidth;
    int height = kDefaultHeight;
    std::vector<uint8_t> bytes;

    GlyphBitmap() : bytes(rowBytes() * static_cast<size_t>(height), 0) {}

    GlyphBitmap(const int w, const int h)
        : width(w), height(h), bytes(static_cast<size_t>((w + 7) / 8) * static_cast<size_t>(h), 0) {}

    /// Number of bytes required per row.
    size_t rowBytes() const {
        return static_cast<size_t>((width + 7) / 8);
    }

    /// Reads one pixel from x/y using MSB-first row packing.
    bool get(const int x, const int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return false;
        }
        const size_t offset = static_cast<size_t>(y) * rowBytes() + static_cast<size_t>(x / 8);
        const int bit = 7 - (x % 8);
        return ((bytes[offset] >> bit) & 0x1u) != 0;
    }

    /// Writes one pixel at x/y using MSB-first row packing.
    void set(const int x, const int y, const bool on) {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return;
        }
        const size_t offset = static_cast<size_t>(y) * rowBytes() + static_cast<size_t>(x / 8);
        const uint8_t mask = static_cast<uint8_t>(1u << (7 - (x % 8)));
        if (on) {
            bytes[offset] |= mask;
        } else {
            bytes[offset] &= static_cast<uint8_t>(~mask);
        }
    }

    /// Toggles one pixel.
    void toggle(const int x, const int y) {
        set(x, y, !get(x, y));
    }

    /// Clears the complete glyph.
    void clear() {
        std::fill(bytes.begin(), bytes.end(), 0);
    }

    /// Mirrors the glyph horizontally.
    void mirrorHorizontal() {
        GlyphBitmap tmp(width, height);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                tmp.set(width - 1 - x, y, get(x, y));
            }
        }
        *this = std::move(tmp);
    }

    /// Mirrors the glyph vertically.
    void mirrorVertical() {
        GlyphBitmap tmp(width, height);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                tmp.set(x, height - 1 - y, get(x, y));
            }
        }
        *this = std::move(tmp);
    }

    /// Rotates the glyph clockwise (only supported for square glyphs).
    bool rotateClockwiseSquareOnly() {
        if (width != height) {
            return false;
        }

        GlyphBitmap tmp(width, height);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                tmp.set(width - 1 - y, x, get(x, y));
            }
        }
        *this = std::move(tmp);
        return true;
    }

    /// Inverts all stored pixels; unused row padding bits are masked back to zero.
    void invert() {
        for (uint8_t &b : bytes) {
            b = static_cast<uint8_t>(~b);
        }

        const int usedBitsInLastByte = width % 8;
        if (usedBitsInLastByte == 0) {
            return;
        }

        const uint8_t keepMask = static_cast<uint8_t>(0xFFu << (8 - usedBitsInLastByte));
        for (int y = 0; y < height; ++y) {
            const size_t last = static_cast<size_t>(y) * rowBytes() + (rowBytes() - 1);
            bytes[last] &= keepMask;
        }
    }
};

/// Indicates which pane currently receives navigation keys.
enum class Focus { Editor, List };

/// Represents a full glyph document with global dimensions and sparse glyph map.
struct Document {
    std::map<int, GlyphBitmap> glyphs;
    std::string path;
    bool dirty = false;
    bool hadExplicitSizeHeader = false;
    int width = kDefaultWidth;
    int height = kDefaultHeight;
    int sourceMaxIndex = 0;

    GlyphBitmap &ensureGlyph(const int index) {
        auto it = glyphs.find(index);
        if (it != glyphs.end()) {
            return it->second;
        }
        return glyphs.emplace(index, GlyphBitmap(width, height)).first->second;
    }

    bool hasGlyph(const int index) const {
        return glyphs.find(index) != glyphs.end();
    }

    const GlyphBitmap *findGlyph(const int index) const {
        const auto it = glyphs.find(index);
        return it != glyphs.end() ? &it->second : nullptr;
    }

    void deleteGlyph(const int index) {
        glyphs.erase(index);
        dirty = true;
    }

    int minIndex() const {
        return glyphs.empty() ? 0 : glyphs.begin()->first;
    }
};

/// Tracks complete runtime state of the ncurses editor.
struct EditorState {
    Document doc;
    int currentGlyph = 0;
    int cursorX = 0;
    int cursorY = 0;
    int listScroll = 0;
    Focus focus = Focus::Editor;
    std::string status = "Ready";
    bool running = true;
};

struct FilePickerEntry {
    fs::path path;
    std::string label;
    bool isDirectory = false;
    bool isParent = false;
};

/// Removes leading/trailing whitespace from a string.
static std::string trim(const std::string &s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

/// Returns true if s begins with prefix.
static bool startsWith(const std::string &s, const std::string &prefix) {
    return s.rfind(prefix, 0) == 0;
}

/// Parses and validates a size declaration "WxH" from a string.
static std::pair<int, int> parseSize(const std::string &value, const std::string &context) {
    const auto xPos = value.find_first_of("xX");
    if (xPos == std::string::npos) {
        throw std::runtime_error(context + ": expected size format WxH, got: " + value);
    }

    const std::string wText = trim(value.substr(0, xPos));
    const std::string hText = trim(value.substr(xPos + 1));
    if (wText.empty() || hText.empty()) {
        throw std::runtime_error(context + ": expected size format WxH, got: " + value);
    }

    int width = 0;
    int height = 0;
    try {
        width = std::stoi(wText);
        height = std::stoi(hText);
    } catch (...) {
        throw std::runtime_error(context + ": invalid numeric size: " + value);
    }

    if (width < kMinGlyphDimension || width > kMaxGlyphDimension ||
        height < kMinGlyphDimension || height > kMaxGlyphDimension) {
        std::ostringstream oss;
        oss << context << ": size out of range [" << kMinGlyphDimension << ".."
            << kMaxGlyphDimension << "], got " << width << "x" << height;
        throw std::runtime_error(oss.str());
    }

    return {width, height};
}

/// Extracts the index from a "Glyph N:" header line.
static int parseGlyphIndex(const std::string &line) {
    if (!startsWith(line, "Glyph ")) {
        throw std::runtime_error("Expected 'Glyph N:' line, got: " + line);
    }

    const auto colon = line.find(':');
    if (colon == std::string::npos) {
        throw std::runtime_error("Missing ':' in glyph header: " + line);
    }

    const std::string num = trim(line.substr(6, colon - 6));
    if (num.empty()) {
        throw std::runtime_error("Missing glyph index in line: " + line);
    }

    try {
        return std::stoi(num);
    } catch (...) {
        throw std::runtime_error("Invalid glyph index in line: " + line);
    }
}

/// Parses one glyph row string into packed row bytes.
static std::vector<uint8_t> parseRow(const std::string &row, const int width, const int glyphIndex, const int rowIndex) {
    if (static_cast<int>(row.size()) != width) {
        std::ostringstream oss;
        oss << "Glyph " << glyphIndex << ", row " << rowIndex
            << ": expected exactly " << width << " characters, got " << row.size();
        throw std::runtime_error(oss.str());
    }

    std::vector<uint8_t> packed(static_cast<size_t>((width + 7) / 8), 0);
    for (int x = 0; x < width; ++x) {
        const char c = row[static_cast<size_t>(x)];
        if (c == '*') {
            const size_t byteIndex = static_cast<size_t>(x / 8);
            const int bitPos = 7 - (x % 8);
            packed[byteIndex] |= static_cast<uint8_t>(1u << bitPos);
        } else if (c != '-') {
            std::ostringstream oss;
            oss << "Glyph " << glyphIndex << ", row " << rowIndex
                << ": invalid character '" << c << "', expected '*' or '-'";
            throw std::runtime_error(oss.str());
        }
    }

    return packed;
}

/// Loads a glyph text document with optional global "Size: WxH" header.
///
/// Parsing behavior:
/// - Empty lines are ignored.
/// - If a size header is present, all glyph rows must match that size.
/// - If no size header is present, size is inferred from the first glyph block.
/// - All subsequent glyphs must match the inferred size.
///
/// @param path Input glyph text file path.
/// @return Parsed document with global dimensions and sparse glyph map.
/// @throws std::runtime_error on malformed headers/rows or inconsistent dimensions.
static Document loadDocument(const std::string &path) {
    Document doc;
    doc.path = path;

    std::ifstream in(path);
    if (!in) {
        return doc;
    }

    std::vector<std::pair<int, std::string>> lines;
    std::string line;
    int lineNumber = 0;
    while (std::getline(in, line)) {
        ++lineNumber;
        lines.emplace_back(lineNumber, line);
    }

    bool sizeKnown = false;
    size_t i = 0;
    while (i < lines.size()) {
        const int ln = lines[i].first;
        const std::string t = trim(lines[i].second);

        if (t.empty()) {
            ++i;
            continue;
        }

        if (startsWith(t, "Size:")) {
            if (sizeKnown || !doc.glyphs.empty()) {
                throw std::runtime_error("Line " + std::to_string(ln) + ": duplicate or misplaced Size header");
            }
            const auto [w, h] = parseSize(trim(t.substr(5)), "Line " + std::to_string(ln));
            doc.width = w;
            doc.height = h;
            doc.hadExplicitSizeHeader = true;
            sizeKnown = true;
            ++i;
            continue;
        }

        if (!startsWith(t, "Glyph ")) {
            throw std::runtime_error("Line " + std::to_string(ln) + ": expected 'Glyph N:' header");
        }

        const int glyphIndex = parseGlyphIndex(t);
        if (glyphIndex < 0) {
            throw std::runtime_error("Line " + std::to_string(ln) + ": negative glyph index is not allowed");
        }

        ++i;
        std::vector<std::string> rawRows;
        while (i < lines.size()) {
            const std::string row = trim(lines[i].second);
            if (row.empty()) {
                ++i;
                continue;
            }
            if (startsWith(row, "Glyph ") || startsWith(row, "Size:")) {
                break;
            }
            rawRows.push_back(row);
            ++i;
        }

        if (rawRows.empty()) {
            throw std::runtime_error("Glyph " + std::to_string(glyphIndex) + ": has no rows");
        }

        if (!sizeKnown) {
            doc.width = static_cast<int>(rawRows.front().size());
            doc.height = static_cast<int>(rawRows.size());
            if (doc.width < kMinGlyphDimension || doc.width > kMaxGlyphDimension ||
                doc.height < kMinGlyphDimension || doc.height > kMaxGlyphDimension) {
                throw std::runtime_error("Inferred glyph size is out of supported range");
            }
            sizeKnown = true;
        }

        if (static_cast<int>(rawRows.size()) != doc.height) {
            std::ostringstream oss;
            oss << "Glyph " << glyphIndex << ": expected " << doc.height
                << " rows, got " << rawRows.size();
            throw std::runtime_error(oss.str());
        }

        GlyphBitmap glyph(doc.width, doc.height);
        const size_t rowStride = glyph.rowBytes();
        for (int row = 0; row < doc.height; ++row) {
            const auto packed = parseRow(rawRows[static_cast<size_t>(row)], doc.width, glyphIndex, row);
            for (size_t b = 0; b < rowStride; ++b) {
                glyph.bytes[static_cast<size_t>(row) * rowStride + b] = packed[b];
            }
        }

        doc.glyphs[glyphIndex] = std::move(glyph);
        doc.sourceMaxIndex = std::max(doc.sourceMaxIndex, glyphIndex);
    }

    doc.dirty = false;
    return doc;
}

/// Applies a newly loaded document to the active editor state.
static void loadIntoState(EditorState &st, const Document &doc, const std::string &statusPrefix) {
    st.doc = doc;
    st.currentGlyph = st.doc.glyphs.empty() ? 0 : st.doc.minIndex();
    st.cursorX = 0;
    st.cursorY = 0;
    st.listScroll = 0;
    st.focus = Focus::Editor;
    st.status = statusPrefix + st.doc.path;
}

/// Returns true if path exists and is a directory.
static bool isDirectoryPath(const std::string &path) {
    std::error_code ec;
    return fs::is_directory(fs::path(path), ec);
}

/// Returns true if path looks like a regular .txt file.
static bool isGlyphTextFilePath(const fs::path &path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec) && path.extension() == ".txt";
}

/// Collects parent directory, sub-directories, and .txt files for the picker.
static std::vector<FilePickerEntry> collectFilePickerEntries(const fs::path &directory) {
    std::vector<FilePickerEntry> directories;
    std::vector<FilePickerEntry> files;

    std::error_code ec;
    const fs::path canonicalDir = fs::weakly_canonical(directory, ec);
    const fs::path actualDir = ec ? directory : canonicalDir;

    const fs::path parent = actualDir.parent_path();
    if (!parent.empty() && parent != actualDir) {
        directories.push_back(FilePickerEntry{parent, "[..]", true, true});
    }

    for (const auto &entry : fs::directory_iterator(actualDir, ec)) {
        if (ec) {
            break;
        }

        const fs::path path = entry.path();
        const std::string name = path.filename().string();
        if (entry.is_directory()) {
            directories.push_back(FilePickerEntry{path, "[" + name + "]", true, false});
        } else if (isGlyphTextFilePath(path)) {
            files.push_back(FilePickerEntry{path, name, false, false});
        }
    }

    auto byLabel = [](const FilePickerEntry &lhs, const FilePickerEntry &rhs) {
        return lhs.label < rhs.label;
    };
    if (directories.size() > 1) {
        std::sort(directories.begin() + 1, directories.end(), byLabel);
    }
    std::sort(files.begin(), files.end(), byLabel);

    std::vector<FilePickerEntry> result;
    result.reserve(directories.size() + files.size());
    result.insert(result.end(), directories.begin(), directories.end());
    result.insert(result.end(), files.begin(), files.end());
    return result;
}

/// Shows a navigable file picker and returns the selected .txt file path, or empty on cancel.
static std::string showFilePicker(const fs::path &startDirectory, const std::string &title) {
    fs::path currentDir = startDirectory;
    int selected = 0;
    int scroll = 0;

    while (true) {
        const auto entries = collectFilePickerEntries(currentDir);
        const int windowHeight = std::min(std::max(8, LINES - 6), 22);
        const int windowWidth = std::min(std::max(40, COLS - 8), 90);
        const int startY = std::max(0, (LINES - windowHeight) / 2);
        const int startX = std::max(0, (COLS - windowWidth) / 2);
        WINDOW *win = newwin(windowHeight, windowWidth, startY, startX);
        if (win == nullptr) {
            return "";
        }

        keypad(win, TRUE);
        const int visibleRows = std::max(1, windowHeight - 4);
        selected = std::clamp(selected, 0, std::max(0, static_cast<int>(entries.size()) - 1));
        scroll = std::clamp(scroll, 0, std::max(0, static_cast<int>(entries.size()) - visibleRows));
        if (selected < scroll) {
            scroll = selected;
        } else if (selected >= scroll + visibleRows) {
            scroll = selected - visibleRows + 1;
        }

        werase(win);
        box(win, 0, 0);
        wattron(win, A_REVERSE);
        mvwprintw(win, 0, 2, " %s ", title.c_str());
        wattroff(win, A_REVERSE);

        std::string dirText = currentDir.string();
        if (static_cast<int>(dirText.size()) > windowWidth - 4) {
            dirText = "..." + dirText.substr(dirText.size() - static_cast<size_t>(windowWidth - 7));
        }
        mvwprintw(win, 1, 2, "%s", dirText.c_str());

        if (entries.empty()) {
            mvwprintw(win, 3, 2, "<no .txt files or subdirectories>");
        } else {
            for (int row = 0; row < visibleRows && scroll + row < static_cast<int>(entries.size()); ++row) {
                const int index = scroll + row;
                if (index == selected) {
                    wattron(win, A_REVERSE);
                }
                mvwprintw(win, 3 + row, 2, "%-*.*s",
                          windowWidth - 4,
                          windowWidth - 4,
                          entries[static_cast<size_t>(index)].label.c_str());
                if (index == selected) {
                    wattroff(win, A_REVERSE);
                }
            }
        }

        mvwprintw(win, windowHeight - 1, 2, "Enter=open/select  Esc=cancel  Backspace=up");
        wrefresh(win);

        const int ch = wgetch(win);
        delwin(win);

        if (ch == 27 || ch == 'q') {
            return "";
        }
        if ((ch == KEY_BACKSPACE || ch == 127 || ch == 8) && currentDir.has_parent_path()) {
            currentDir = currentDir.parent_path();
            selected = 0;
            scroll = 0;
            continue;
        }
        if (entries.empty()) {
            continue;
        }

        switch (ch) {
            case KEY_UP:
            case 'k':
                selected = std::max(0, selected - 1);
                break;
            case KEY_DOWN:
            case 'j':
                selected = std::min(static_cast<int>(entries.size()) - 1, selected + 1);
                break;
            case KEY_PPAGE:
                selected = std::max(0, selected - visibleRows);
                break;
            case KEY_NPAGE:
                selected = std::min(static_cast<int>(entries.size()) - 1, selected + visibleRows);
                break;
            case '\n':
            case KEY_ENTER:
            case 13: {
                const FilePickerEntry &entry = entries[static_cast<size_t>(selected)];
                if (entry.isDirectory) {
                    currentDir = entry.path;
                    selected = 0;
                    scroll = 0;
                } else {
                    return entry.path.string();
                }
                break;
            }
            default:
                break;
        }
    }
}

/// Opens the file picker, loads the selected document, and updates editor state.
static void loadDocumentViaPicker(EditorState &st, const fs::path &startDirectory) {
    const std::string selectedPath = showFilePicker(startDirectory, "Load glyph text");
    if (selectedPath.empty()) {
        st.status = "Load cancelled";
        return;
    }

    try {
        Document doc = loadDocument(selectedPath);
        doc.path = selectedPath;
        loadIntoState(st, doc, "Loaded: ");
    } catch (const std::exception &e) {
        st.status = std::string("Load failed: ") + e.what();
    }
}

/// Saves the current document atomically via temporary file + rename.
///
/// The output always includes a "Size: WxH" header so non-8x8 files remain explicit
/// and stable when edited/saved repeatedly.
///
/// @param doc Document to serialize.
/// @throws std::runtime_error if write/rename fails.
static void saveDocument(const Document &doc) {
    const std::string tmpPath = doc.path + ".tmp";
    std::ofstream out(tmpPath);
    if (!out) {
        throw std::runtime_error("Could not write temporary file: " + tmpPath);
    }

    // Writing a size header preserves non-8x8 layouts explicitly.
    out << "Size: " << doc.width << "x" << doc.height << "\n\n";

    bool first = true;
    for (const auto &[index, glyph] : doc.glyphs) {
        if (!first) {
            out << "\n";
        }
        first = false;

        out << "Glyph " << index << ":\n";
        for (int y = 0; y < doc.height; ++y) {
            for (int x = 0; x < doc.width; ++x) {
                out << (glyph.get(x, y) ? '*' : '-');
            }
            out << "\n";
        }
    }

    out.close();
    if (!out) {
        throw std::runtime_error("Failed while writing: " + tmpPath);
    }

    if (std::rename(tmpPath.c_str(), doc.path.c_str()) != 0) {
        std::remove(tmpPath.c_str());
        throw std::runtime_error("Could not replace output file: " + doc.path);
    }
}

/// Returns the inclusive maximum index shown by the glyph list.
static int listMaxIndex(const Document &doc) {
    return std::max(0, doc.sourceMaxIndex);
}

/// Scrolls the glyph list rows so the selected index stays visible.
static void ensureVisible(EditorState &st, const int visibleRows) {
    const int maxIndex = listMaxIndex(st.doc);
    const int selectedRow = st.currentGlyph / kGlyphListColumns;
    const int totalRows = (maxIndex / kGlyphListColumns) + 1;

    if (selectedRow < st.listScroll) {
        st.listScroll = selectedRow;
    } else if (selectedRow >= st.listScroll + visibleRows) {
        st.listScroll = selectedRow - visibleRows + 1;
    }

    if (st.listScroll < 0) {
        st.listScroll = 0;
    }
    const int maxScroll = std::max(0, totalRows - visibleRows);
    if (st.listScroll > maxScroll) {
        st.listScroll = maxScroll;
    }
}

/// Prompts the user for a one-line text input in the footer line.
///
/// Uses reverse-video footer styling to keep prompt UX consistent with status/help bars.
///
/// @param title Prompt label rendered at the beginning of the footer line.
/// @param outText Receives the entered text (empty when user submits empty input).
static void promptLine(const std::string &title, std::string &outText) {
    move(LINES - 1, 0);
    clrtoeol();
    attron(A_REVERSE);
    mvhline(LINES - 1, 0, ' ', COLS);
    mvprintw(LINES - 1, 0, "%s", title.c_str());
    attroff(A_REVERSE);

    echo();
    curs_set(1);
    char buffer[256] = {};
    const int maxChars = std::min(255, std::max(1, COLS - static_cast<int>(title.size()) - 1));
    move(LINES - 1, std::min(COLS - 1, static_cast<int>(title.size())));
    getnstr(buffer, maxChars);
    noecho();
    curs_set(0);
    outText = buffer;
}

/// Prompts for a yes/no decision in the footer line.
///
/// @param text Prompt question.
/// @return true for 'y'/'Y', false for all other keys.
static bool promptYesNo(const std::string &text) {
    move(LINES - 1, 0);
    clrtoeol();
    attron(A_REVERSE);
    mvhline(LINES - 1, 0, ' ', COLS);
    mvprintw(LINES - 1, 0, "%s [y/N]", text.c_str());
    attroff(A_REVERSE);
    const int ch = getch();
    return ch == 'y' || ch == 'Y';
}

/// Draws an enlarged editor grid for the active glyph.
static void drawZoomedGlyph(const GlyphBitmap &glyph,
                            const int startX,
                            const int startY,
                            const int cellW,
                            const int cellH,
                            const int cursorX,
                            const int cursorY,
                            const bool editorFocused,
                            const int clipLeft,
                            const int clipTop,
                            const int clipRight,
                            const int clipBottom) {
    for (int y = 0; y < glyph.height; ++y) {
        for (int x = 0; x < glyph.width; ++x) {
            const bool on = glyph.get(x, y);
            const bool cursor = (x == cursorX && y == cursorY);

            int attr = 0;
            if (cursor && editorFocused) {
                attr |= A_BOLD;
            }

            for (int yy = 0; yy < cellH; ++yy) {
                for (int xx = 0; xx < cellW; ++xx) {
                    const int drawY = startY + y * cellH + yy;
                    const int drawX = startX + x * cellW + xx;
                    if (drawY < clipTop || drawY > clipBottom || drawX < clipLeft || drawX > clipRight) {
                        continue;
                    }

                    chtype ch = static_cast<chtype>(' ');
                    if (gUseColors) {
                        if (cursor) {
                            ch |= static_cast<chtype>(COLOR_PAIR(on ? kPairCursorOn : kPairCursorOff));
                        } else if (on) {
                            ch |= static_cast<chtype>(COLOR_PAIR(kPairEditorSet));
                        } else {
                            ch |= static_cast<chtype>(COLOR_PAIR(kPairEditorBg));
                        }
                    } else {
                        if (on) {
                            ch |= static_cast<chtype>(A_REVERSE);
                        }
                        if (cursor) {
                            ch |= static_cast<chtype>(A_STANDOUT);
                        }
                    }

                    if (attr) {
                        ch |= static_cast<chtype>(attr);
                    }
                    mvaddch(drawY, drawX, ch);
                }
            }
        }
    }
}

/// Draws a compact glyph preview (1 character per pixel).
static void drawMiniGlyph(const GlyphBitmap &glyph, const int x, const int y) {
    // Clear preview rectangle first so every row is fully deterministic on first paint.
    for (int row = 0; row < glyph.height; ++row) {
        for (int col = 0; col < glyph.width; ++col) {
            const int drawY = y + row;
            const int drawX = x + col;
            if (drawY < 0 || drawY >= LINES || drawX < 0 || drawX >= COLS) {
                continue;
            }
            mvaddch(drawY, drawX, ' ');
        }
    }

    for (int row = 0; row < glyph.height; ++row) {
        for (int col = 0; col < glyph.width; ++col) {
            const int drawY = y + row;
            const int drawX = x + col;
            if (drawY < 0 || drawY >= LINES || drawX < 0 || drawX >= COLS) {
                continue;
            }

            if (glyph.get(col, row)) {
                if (gUseColors) {
                    mvaddch(drawY, drawX, static_cast<chtype>(' ') | COLOR_PAIR(kPairEditorSet));
                } else {
                    mvaddch(drawY, drawX, static_cast<chtype>(' ') | A_REVERSE);
                }
            }
        }
    }
}

/// Executes one selected menu action.
///
/// @param st Mutable editor state.
/// @param selection Index into popup menu entries (excluding cancel handling).
static void applyMenuAction(EditorState &st, const int selection) {
    switch (selection) {
        case 0:  // Load
            loadDocumentViaPicker(st, st.doc.path.empty() ? fs::current_path() : fs::path(st.doc.path).parent_path());
            break;
        case 1:  // Save
            try {
                saveDocument(st.doc);
                st.doc.dirty = false;
                st.status = "Saved";
            } catch (const std::exception &e) {
                st.status = std::string("Save failed: ") + e.what();
            }
            break;
        case 2:  // Clear
            st.doc.ensureGlyph(st.currentGlyph).clear();
            st.doc.dirty = true;
            st.status = "Glyph cleared";
            break;
        case 3:  // Delete
            if (st.doc.hasGlyph(st.currentGlyph)) {
                st.doc.deleteGlyph(st.currentGlyph);
                st.status = "Glyph deleted";
            } else {
                st.status = "Glyph not defined";
            }
            break;
        case 4:  // Mirror H
            st.doc.ensureGlyph(st.currentGlyph).mirrorHorizontal();
            st.doc.dirty = true;
            st.status = "Mirrored horizontally";
            break;
        case 5:  // Mirror V
            st.doc.ensureGlyph(st.currentGlyph).mirrorVertical();
            st.doc.dirty = true;
            st.status = "Mirrored vertically";
            break;
        case 6:  // Rotate
            if (st.doc.ensureGlyph(st.currentGlyph).rotateClockwiseSquareOnly()) {
                st.doc.dirty = true;
                st.status = "Rotated clockwise";
            } else {
                st.status = "Rotate requires square glyphs (e.g. 5x5, 8x8, 12x12)";
            }
            break;
        case 7:  // Invert
            st.doc.ensureGlyph(st.currentGlyph).invert();
            st.doc.dirty = true;
            st.status = "Inverted glyph";
            break;
        case 8: { // Jump
            const int maxIndex = listMaxIndex(st.doc);
            std::string input;
            promptLine("Go to glyph index: ", input);
            if (!input.empty()) {
                try {
                    st.currentGlyph = std::clamp(std::stoi(input), 0, maxIndex);
                    st.cursorX = std::clamp(st.cursorX, 0, st.doc.width - 1);
                    st.cursorY = std::clamp(st.cursorY, 0, st.doc.height - 1);
                    st.status = "Jumped to glyph";
                } catch (...) {
                    st.status = "Invalid glyph index";
                }
            } else {
                st.status = "Jump cancelled";
            }
            break;
        }
        default:
            break;
    }
}

/// Shows a compact ncurses popup menu and applies selected action.
///
/// Navigation:
/// - Up/Down or k/j to change selection
/// - Enter to apply
/// - Esc/q to cancel
///
/// @param st Mutable editor state.
static void openMenu(EditorState &st) {
    static const std::vector<std::string> items = {
        "Load...",
        "Save",
        "Clear glyph",
        "Delete glyph",
        "Mirror horizontal",
        "Mirror vertical",
        "Rotate clockwise",
        "Invert glyph",
        "Jump to glyph",
        "Cancel"};

    const int menuH = static_cast<int>(items.size()) + 4;
    int menuW = 0;
    for (const auto &i : items) {
        menuW = std::max(menuW, static_cast<int>(i.size()));
    }
    menuW += 6;

    const int startY = std::max(0, (LINES - menuH) / 2);
    const int startX = std::max(0, (COLS - menuW) / 2);
    WINDOW *win = newwin(menuH, menuW, startY, startX);
    if (win == nullptr) {
        st.status = "Could not open menu window";
        return;
    }

    keypad(win, TRUE);
    int selected = 0;
    bool done = false;

    while (!done) {
        werase(win);
        box(win, 0, 0);
        wattron(win, A_REVERSE);
        mvwprintw(win, 0, 2, " Menu ");
        wattroff(win, A_REVERSE);

        for (size_t i = 0; i < items.size(); ++i) {
            if (static_cast<int>(i) == selected) {
                wattron(win, A_REVERSE);
            }
            mvwprintw(win, 2 + static_cast<int>(i), 2, "%s", items[i].c_str());
            if (static_cast<int>(i) == selected) {
                wattroff(win, A_REVERSE);
            }
        }
        wrefresh(win);

        const int ch = wgetch(win);
        switch (ch) {
            case KEY_UP:
            case 'k':
                selected = (selected == 0) ? static_cast<int>(items.size()) - 1 : selected - 1;
                break;
            case KEY_DOWN:
            case 'j':
                selected = (selected + 1) % static_cast<int>(items.size());
                break;
            case '\n':
            case KEY_ENTER:
            case 13:
                done = true;
                break;
            case 27:  // ESC
            case 'q':
                selected = static_cast<int>(items.size()) - 1;
                done = true;
                break;
            default:
                break;
        }
    }

    delwin(win);

    // Last entry is cancel.
    if (selected != static_cast<int>(items.size()) - 1) {
        applyMenuAction(st, selected);
    } else {
        st.status = "Menu cancelled";
    }
}

/// Renders complete UI frame.
///
/// Layout:
/// - Inverted header with file, dimensions, glyph, focus
/// - Main split: zoomed editor pane + glyph index list pane
/// - Inverted status line
/// - Inverted help/footer line
///
/// Note: The editor grid area is pre-filled each frame to avoid partial initial draw artifacts.
///
/// @param st Current immutable editor state.
static void render(const EditorState &st) {
    erase();

    if (LINES < 12 || COLS < 80) {
        mvprintw(0, 0, "Terminal too small. Need at least 80x12, got %dx%d", COLS, LINES);
        refresh();
        return;
    }

    const int headerY = 0;
    const int bodyTop = 1;
    const int bodyBottom = LINES - 3;
    const int statusY = LINES - 2;
    const int helpY = LINES - 1;

    const int listCellW = 4;
    const int listBoxW = (kGlyphListColumns * listCellW) + 2;
    const int previewBoxW = std::max(16, st.doc.width + 2);
    const int rightGap = 1;

    const int editorWidth = COLS - (previewBoxW + rightGap + listBoxW + 1);
    if (editorWidth < 20) {
        mvprintw(0, 0, "Terminal too small for 10-column list layout. Need wider terminal.");
        refresh();
        return;
    }

    const int previewX = editorWidth + 1;
    const int listX = previewX + previewBoxW + rightGap;

    const GlyphBitmap fallback(st.doc.width, st.doc.height);
    const GlyphBitmap &glyph = st.doc.findGlyph(st.currentGlyph) ? *st.doc.findGlyph(st.currentGlyph) : fallback;

    // Inverted header.
    attron(A_REVERSE);
    mvhline(headerY, 0, ' ', COLS);
    std::ostringstream header;
    header << " Glyphtext Editor | "
           << st.doc.width << "x" << st.doc.height
           << " | Glyph " << st.currentGlyph
           << " | Focus: " << (st.focus == Focus::Editor ? "Editor" : "List");
    std::string headerText = header.str();

    std::string fileInfo = st.doc.path.empty() ? "<unnamed>" : st.doc.path;
    if (st.doc.dirty) {
        fileInfo += " [modified]";
    }

    mvprintw(headerY, 1, "%s", headerText.c_str());
    mvprintw(headerY, std::max(1, COLS - static_cast<int>(fileInfo.size()) - 1), "%s", fileInfo.c_str());
    attroff(A_REVERSE);

    // Draw left editor pane border.
    const int editorBoxLeft = 0;
    const int editorBoxTop = bodyTop;
    const int editorBoxRight = editorWidth - 1;
    const int editorBoxBottom = bodyBottom;
    mvhline(editorBoxTop, editorBoxLeft + 1, ACS_HLINE, std::max(0, editorBoxRight - editorBoxLeft - 1));
    mvhline(editorBoxBottom, editorBoxLeft + 1, ACS_HLINE, std::max(0, editorBoxRight - editorBoxLeft - 1));
    mvvline(editorBoxTop + 1, editorBoxLeft, ACS_VLINE, std::max(0, editorBoxBottom - editorBoxTop - 1));
    mvvline(editorBoxTop + 1, editorBoxRight, ACS_VLINE, std::max(0, editorBoxBottom - editorBoxTop - 1));
    mvaddch(editorBoxTop, editorBoxLeft, ACS_ULCORNER);
    mvaddch(editorBoxTop, editorBoxRight, ACS_URCORNER);
    mvaddch(editorBoxBottom, editorBoxLeft, ACS_LLCORNER);
    mvaddch(editorBoxBottom, editorBoxRight, ACS_LRCORNER);

    // Draw preview pane (closer to editor).
    const int previewBoxLeft = previewX;
    const int previewBoxTop = bodyTop;
    const int previewBoxRight = previewBoxLeft + previewBoxW - 1;
    const int previewInnerHeightNeeded = st.doc.height + 1;
    const int previewBoxBottom = std::min(bodyBottom, previewBoxTop + previewInnerHeightNeeded);
    mvhline(previewBoxTop, previewBoxLeft + 1, ACS_HLINE, std::max(0, previewBoxRight - previewBoxLeft - 1));
    mvhline(previewBoxBottom, previewBoxLeft + 1, ACS_HLINE, std::max(0, previewBoxRight - previewBoxLeft - 1));
    mvvline(previewBoxTop + 1, previewBoxLeft, ACS_VLINE, std::max(0, previewBoxBottom - previewBoxTop - 1));
    mvvline(previewBoxTop + 1, previewBoxRight, ACS_VLINE, std::max(0, previewBoxBottom - previewBoxTop - 1));
    mvaddch(previewBoxTop, previewBoxLeft, ACS_ULCORNER);
    mvaddch(previewBoxTop, previewBoxRight, ACS_URCORNER);
    mvaddch(previewBoxBottom, previewBoxLeft, ACS_LLCORNER);
    mvaddch(previewBoxBottom, previewBoxRight, ACS_LRCORNER);

    // Draw glyph list pane (rightmost).
    const int listBoxLeft = listX;
    const int listBoxTop = bodyTop;
    const int listBoxRight = COLS - 1;
    const int listBoxBottom = bodyBottom;
    mvhline(listBoxTop, listBoxLeft + 1, ACS_HLINE, std::max(0, listBoxRight - listBoxLeft - 1));
    mvhline(listBoxBottom, listBoxLeft + 1, ACS_HLINE, std::max(0, listBoxRight - listBoxLeft - 1));
    mvvline(listBoxTop + 1, listBoxLeft, ACS_VLINE, std::max(0, listBoxBottom - listBoxTop - 1));
    mvvline(listBoxTop + 1, listBoxRight, ACS_VLINE, std::max(0, listBoxBottom - listBoxTop - 1));
    mvaddch(listBoxTop, listBoxLeft, ACS_ULCORNER);
    mvaddch(listBoxTop, listBoxRight, ACS_URCORNER);
    mvaddch(listBoxBottom, listBoxLeft, ACS_LLCORNER);
    mvaddch(listBoxBottom, listBoxRight, ACS_LRCORNER);

    attron(A_BOLD);
    mvprintw(bodyTop, 2, " Editor ");
    mvprintw(listBoxTop, listBoxLeft + 2, " Glyph list ");
    attroff(A_BOLD);

    // Compute zoom cell size from available panel size.
    const int editorInnerLeft = editorBoxLeft + 1;
    const int editorInnerTop = editorBoxTop + 1;
    const int editorInnerRight = editorBoxRight - 1;
    const int editorInnerBottom = editorBoxBottom - 1;
    const int usableW = std::max(1, editorInnerRight - editorInnerLeft + 1);
    const int usableH = std::max(1, editorInnerBottom - editorInnerTop + 1);
    int cellW = std::max(1, usableW / st.doc.width);
    int cellH = std::max(1, usableH / st.doc.height);
    cellW = std::min(cellW, 4);
    cellH = std::min(cellH, 2);

    const int gridX = editorInnerLeft;
    const int gridY = editorInnerTop;

    // First clear the complete editor pane interior to normal terminal background.
    for (int y = editorInnerTop; y <= editorInnerBottom; ++y) {
        for (int x = editorInnerLeft; x <= editorInnerRight; ++x) {
            const int drawY = y;
            const int drawX = x;
            if (drawY < 0 || drawY >= LINES || drawX < 0 || drawX >= COLS) {
                continue;
            }
            mvaddch(drawY, drawX, ' ');
        }
    }

    // Then fill only the editable pixel matrix area with the matrix background color.
    const int matrixPixelWidth = st.doc.width * cellW;
    const int matrixPixelHeight = st.doc.height * cellH;
    for (int y = gridY; y < gridY + matrixPixelHeight; ++y) {
        for (int x = gridX; x < gridX + matrixPixelWidth; ++x) {
            if (y < editorInnerTop || y > editorInnerBottom || x < editorInnerLeft || x > editorInnerRight) {
                continue;
            }
            if (gUseColors) {
                mvaddch(y, x, static_cast<chtype>(' ') | COLOR_PAIR(kPairEditorBg));
            } else {
                mvaddch(y, x, ' ');
            }
        }
    }
    drawZoomedGlyph(glyph,
                    gridX,
                    gridY,
                    cellW,
                    cellH,
                    st.cursorX,
                    st.cursorY,
                    st.focus == Focus::Editor,
                    editorInnerLeft,
                    editorInnerTop,
                    editorInnerRight,
                    editorInnerBottom);

    // Draw zoomed preview inside preview pane.
    const int miniX = previewBoxLeft + 1;
    const int miniY = previewBoxTop + 1;

    if (previewBoxLeft + 2 < previewBoxRight) {
        mvprintw(previewBoxTop, previewBoxLeft + 2, " Preview ");
        if (miniY - 1 > previewBoxTop && miniX + 14 < previewBoxRight) {
            mvprintw(miniY - 1, miniX, "Zoomed Preview");
        }
    }

    if (miniX + st.doc.width - 1 <= previewBoxRight - 1 &&
        miniY + st.doc.height - 1 <= previewBoxBottom - 1) {
        drawMiniGlyph(glyph, miniX, miniY);
    }


    // Glyph list as fixed 10-column grid from index 0..sourceMaxIndex.
    const int listInnerLeft = listBoxLeft + 1;
    const int listInnerTop = listBoxTop + 1;
    const int listInnerRight = listBoxRight - 1;
    const int listInnerBottom = listBoxBottom - 1;
    const int visibleRows = std::max(1, listInnerBottom - listInnerTop + 1);
    const int maxIndex = listMaxIndex(st.doc);

    for (int row = 0; row < visibleRows; ++row) {
        const int rowIndex = st.listScroll + row;
        for (int col = 0; col < kGlyphListColumns; ++col) {
            const int idx = rowIndex * kGlyphListColumns + col;
            if (idx > maxIndex) {
                continue;
            }

            const int x = listInnerLeft + col * listCellW;
            const int y = listInnerTop + row;
            if (x > listInnerRight || y > listInnerBottom) {
                continue;
            }

            const bool selected = (idx == st.currentGlyph);
            const bool defined = st.doc.hasGlyph(idx);

            if (selected) {
                attron(A_REVERSE);
            } else if (!defined && gUseColors) {
                attron(COLOR_PAIR(kPairUndefinedIndex));
            }

            mvprintw(y, x, "%3d", idx);

            if (selected) {
                attroff(A_REVERSE);
            } else if (!defined && gUseColors) {
                attroff(COLOR_PAIR(kPairUndefinedIndex));
            }
        }
    }

    // Status line (plain; not inverted).
    move(statusY, 0);
    clrtoeol();
    mvhline(statusY, 0, ' ', COLS);
    mvprintw(statusY, 0, "%s", st.status.c_str());

    // Inverted footer help line.
    move(helpY, 0);
    clrtoeol();
    attron(A_REVERSE);
    mvhline(helpY, 0, ' ', COLS);
    mvprintw(helpY,
             0,
             "Arrows/hjkl move | TAB focus | SPACE toggle | n/p glyph | g jump | o load | m menu | s save | q quit");
    attroff(A_REVERSE);

    refresh();
}

/// Handles keys for editor (pixel grid) mode.
///
/// @param st Mutable editor state.
/// @param ch Key code from ncurses getch().
static void handleEditorKey(EditorState &st, const int ch) {
    switch (ch) {
        case KEY_LEFT:
        case 'h':
            st.cursorX = std::max(0, st.cursorX - 1);
            break;
        case KEY_RIGHT:
        case 'l':
            st.cursorX = std::min(st.doc.width - 1, st.cursorX + 1);
            break;
        case KEY_UP:
        case 'k':
            st.cursorY = std::max(0, st.cursorY - 1);
            break;
        case KEY_DOWN:
        case 'j':
            st.cursorY = std::min(st.doc.height - 1, st.cursorY + 1);
            break;
        case ' ':
            st.doc.ensureGlyph(st.currentGlyph).toggle(st.cursorX, st.cursorY);
            st.doc.dirty = true;
            st.status = "Pixel toggled";
            break;
        default:
            break;
    }
}

/// Handles keys for glyph list mode.
///
/// @param st Mutable editor state.
/// @param ch Key code from ncurses getch().
/// @param listHeight Visible list rows used for page-up/page-down movement.
static void handleListKey(EditorState &st, const int ch, const int listHeight) {
    const int maxIndex = listMaxIndex(st.doc);
    switch (ch) {
        case KEY_UP:
        case 'k':
            st.currentGlyph = std::max(0, st.currentGlyph - kGlyphListColumns);
            ensureVisible(st, listHeight);
            break;
        case KEY_DOWN:
        case 'j':
            st.currentGlyph = std::min(maxIndex, st.currentGlyph + kGlyphListColumns);
            ensureVisible(st, listHeight);
            break;
        case KEY_LEFT:
        case 'h':
            st.currentGlyph = std::max(0, st.currentGlyph - 1);
            ensureVisible(st, listHeight);
            break;
        case KEY_RIGHT:
        case 'l':
            st.currentGlyph = std::min(maxIndex, st.currentGlyph + 1);
            ensureVisible(st, listHeight);
            break;
        case KEY_PPAGE:
            st.currentGlyph = std::max(0, st.currentGlyph - listHeight * kGlyphListColumns);
            ensureVisible(st, listHeight);
            break;
        case KEY_NPAGE:
            st.currentGlyph = std::min(maxIndex, st.currentGlyph + listHeight * kGlyphListColumns);
            ensureVisible(st, listHeight);
            break;
        default:
            break;
    }
}

/// Handles global keys independent of focus.
///
/// @param st Mutable editor state.
/// @param ch Key code from ncurses getch().
static void handleGlobalKey(EditorState &st, const int ch) {
    const int maxIndex = listMaxIndex(st.doc);
    switch (ch) {
        case '\t':
            st.focus = (st.focus == Focus::Editor) ? Focus::List : Focus::Editor;
            st.status = (st.focus == Focus::Editor) ? "Focus: editor" : "Focus: glyph list";
            break;
        case 'n':
            st.currentGlyph = std::min(maxIndex, st.currentGlyph + 1);
            st.status = "Next glyph";
            break;
        case 'p':
            st.currentGlyph = std::max(0, st.currentGlyph - 1);
            st.status = "Previous glyph";
            break;
        case 'g': {
            std::string input;
            promptLine("Go to glyph index: ", input);
            if (!input.empty()) {
                try {
                    st.currentGlyph = std::clamp(std::stoi(input), 0, maxIndex);
                    st.cursorX = std::clamp(st.cursorX, 0, st.doc.width - 1);
                    st.cursorY = std::clamp(st.cursorY, 0, st.doc.height - 1);
                    st.status = "Jumped to glyph";
                } catch (...) {
                    st.status = "Invalid glyph index";
                }
            } else {
                st.status = "Jump cancelled";
            }
            break;
        }
        case 'c':
            st.doc.ensureGlyph(st.currentGlyph).clear();
            st.doc.dirty = true;
            st.status = "Glyph cleared";
            break;
        case 'd':
            if (st.doc.hasGlyph(st.currentGlyph)) {
                st.doc.deleteGlyph(st.currentGlyph);
                st.status = "Glyph deleted";
            } else {
                st.status = "Glyph not defined";
            }
            break;
        case 'H':
            st.doc.ensureGlyph(st.currentGlyph).mirrorHorizontal();
            st.doc.dirty = true;
            st.status = "Mirrored horizontally";
            break;
        case 'V':
            st.doc.ensureGlyph(st.currentGlyph).mirrorVertical();
            st.doc.dirty = true;
            st.status = "Mirrored vertically";
            break;
        case 'R':
            if (st.doc.ensureGlyph(st.currentGlyph).rotateClockwiseSquareOnly()) {
                st.doc.dirty = true;
                st.status = "Rotated clockwise";
            } else {
                st.status = "Rotate requires square glyphs (e.g. 5x5, 8x8, 12x12)";
            }
            break;
        case 'I':
            st.doc.ensureGlyph(st.currentGlyph).invert();
            st.doc.dirty = true;
            st.status = "Inverted glyph";
            break;
        case 's':
            try {
                saveDocument(st.doc);
                st.doc.dirty = false;
                st.status = "Saved";
            } catch (const std::exception &e) {
                st.status = std::string("Save failed: ") + e.what();
            }
            break;
        case 'o':
            loadDocumentViaPicker(st, st.doc.path.empty() ? fs::current_path() : fs::path(st.doc.path).parent_path());
            break;
        case 'm':
            openMenu(st);
            break;
        case 'q':
            if (st.doc.dirty) {
                if (promptYesNo("Unsaved changes. Quit anyway?")) {
                    st.running = false;
                } else {
                    st.status = "Quit cancelled";
                }
            } else {
                st.running = false;
            }
            break;
        default:
            break;
    }
}

}  // namespace

/// Program entry point for the interactive ncurses glyph editor.
///
/// @param argc Argument count.
/// @param argv Argument array; expects one positional path to glyph text file.
/// @return 0 on normal exit, 1 on startup/load failure.
int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: glyphtext_editor <file.txt>\n");
        return 1;
    }

    const std::string startupPath = argv[1];
    const bool startupIsDirectory = isDirectoryPath(startupPath);

    EditorState st;
    if (!startupIsDirectory) {
        try {
            st.doc = loadDocument(startupPath);
            st.doc.path = startupPath;
            if (!st.doc.glyphs.empty()) {
                st.currentGlyph = st.doc.minIndex();
            }
        } catch (const std::exception &e) {
            std::fprintf(stderr, "Error loading file: %s\n", e.what());
            return 1;
        }
    }

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    initUiColors();

    if (startupIsDirectory) {
        loadDocumentViaPicker(st, fs::path(startupPath));
        if (st.doc.path.empty()) {
            endwin();
            return 0;
        }
    }

    while (st.running) {
        const int listHeight = std::max(5, LINES - 4);
        ensureVisible(st, listHeight);
        st.cursorX = std::clamp(st.cursorX, 0, st.doc.width - 1);
        st.cursorY = std::clamp(st.cursorY, 0, st.doc.height - 1);

        render(st);
        const int ch = getch();


        handleGlobalKey(st, ch);
        if (!st.running) {
            break;
        }

        if (st.focus == Focus::Editor) {
            handleEditorKey(st, ch);
        } else {
            handleListKey(st, ch, listHeight);
        }
    }

    endwin();
    return 0;
}
