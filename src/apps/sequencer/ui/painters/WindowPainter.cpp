#include "WindowPainter.h"

#include "Config.h"

#include "core/utils/StringBuilder.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace {
    // Sticky mode/function labels shown in the header.
    char g_activeFunction[24] = {};
    char g_activeMode[24] = {};
    bool g_hasActiveFunction = false;

    // Glyph sequence used by the transport activity indicator in the header.
    // Values are font-specific glyph codes from the tiny font.
    static constexpr std::array<char, 4> kClockAnimationFrames = {
        static_cast<char>(5),
        static_cast<char>(6),
        static_cast<char>(7),
        static_cast<char>(6),
    };

    struct ClockAnimationState {
        int frameIndex = 0;
        uint32_t lastStep = 0;
        bool hasLastStep = false;
    };

    static ClockAnimationState g_clockAnimation;

    // Advances the indicator once per sequencer step (not once per redraw).
    // This keeps animation speed tied to musical time and independent from UI FPS.
    // When transport stops, state resets to frame 0 and a blank character is returned.
    static char updateClockAnimation(const Engine &engine, const bool isRunning) {
        if (!isRunning) {
            g_clockAnimation.frameIndex = 0;
            g_clockAnimation.hasLastStep = false;
            return ' ';
        }

        const uint32_t stepDivisor = std::max(1u, engine.noteDivisor());
        const uint32_t currentStep = engine.tick() / stepDivisor;

        if (!g_clockAnimation.hasLastStep) {
            g_clockAnimation.lastStep = currentStep;
            g_clockAnimation.hasLastStep = true;
        } else if (currentStep != g_clockAnimation.lastStep) {
            const int frameCount = static_cast<int>(kClockAnimationFrames.size());
            g_clockAnimation.frameIndex = (g_clockAnimation.frameIndex + 1) % frameCount;
            g_clockAnimation.lastStep = currentStep;
        }

        return kClockAnimationFrames[static_cast<size_t>(g_clockAnimation.frameIndex)];
    }
}

/**
 * Draws text on the canvas, optionally inverting the background for highlighting.
 * @param canvas The canvas to draw on.
 * @param x The x-coordinate of the text.
 * @param y The y-coordinate of the text.
 * @param text The text string to draw.
 * @param inverted Whether to invert the text (draw background and use subtractive blend mode).
 */
static void drawInvertedText(Canvas &canvas, int x, int y, const char *text, bool inverted = true) {
    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);

    if (inverted) {
        canvas.fillRect(x - 1, y - 5, canvas.textWidth(text) + 1, 7);
        canvas.setBlendMode(BlendMode::Sub);
    }

    canvas.drawText(x, y, text);
}

/**
 * Clears the entire canvas by filling it with black color.
 * @param canvas The canvas to clear.
 */
void WindowPainter::clear(Canvas &canvas) {
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(UI_COLOR_BLACK);
    canvas.fill();
}

/**
 * Draws a rectangular frame on the canvas: fills the area with black and draws an active-colored border.
 * @param canvas The canvas to draw on.
 * @param x The x-coordinate of the frame.
 * @param y The y-coordinate of the frame.
 * @param w The width of the frame.
 * @param h The height of the frame.
 */
void WindowPainter::drawFrame(Canvas &canvas, int x, int y, int w, int h) {
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(UI_COLOR_BLACK);
    canvas.fillRect(x, y, w, h);
    canvas.setColor(UI_COLOR_ACTIVE);
    canvas.drawRect(x, y, w, h);
}

/**
 * Draws the function keys in the footer area, displaying their names and handling pressed/highlight states.
 * @param canvas The canvas to draw on.
 * @param names Array of strings for the function key labels.
 * @param keyState The current state of the keys.
 * @param highlight The index of the key to highlight (-1 for none).
 */
void WindowPainter::drawFunctionKeys(Canvas &canvas, const char *names[], const KeyState &keyState, int highlight) {
    const int footerTop = PageHeight - FooterHeight;
    const int footerTextY = PageHeight - 3;

    canvas.setBlendMode(BlendMode::Add);
    canvas.setColor(UI_COLOR_FOOTER_BACK);
    canvas.fillRect(0, PageHeight - FooterHeight, PageWidth, FooterHeight);

    canvas.setFont(Font::Tiny);
    canvas.setColor(UI_COLOR_ACTIVE);

    for (int i = 0; i < FunctionKeyCount; ++i) {
        const char *label = names[i];
        if (!label) {
            continue;
        }

        bool isPressed = keyState[Key::F0 + i];
        if (highlight >= 0) {
            isPressed = i == highlight;
        }

        const int left = (PageWidth * i) / FunctionKeyCount;
        const int right = (PageWidth * (i + 1)) / FunctionKeyCount;
        const int width = right - left + 1;

        canvas.setBlendMode(BlendMode::Add);
        if (isPressed) {
            canvas.fillRect(left, footerTop, width, FooterHeight);
            canvas.setBlendMode(BlendMode::Sub);
        }

        const int textX = left + (width - canvas.textWidth(label)) / 2;
        canvas.drawText(textX, footerTextY, label);
    }
}

/**
 * Draws the clock information, including tempo and mode, in the header.
 * @param canvas The canvas to draw on.
 * @param engine The engine providing clock and tempo data.
 */
void WindowPainter::drawClock(Canvas &canvas, const Engine &engine) {
    static const char *clockModeName[] = {
        TXT_INFO_CLOCK_MODE_AUTO,
        TXT_INFO_CLOCK_MODE_MANUAL,
        TXT_INFO_CLOCK_MODE_SLAVE
    };

    const bool isRunning = engine.state().running();

    const char *name = engine.recording() ? TXT_INFO_RECORD_MODE : clockModeName[int(engine.clock().activeMode())];
    const char animationChar = updateClockAnimation(engine, isRunning);
    const char animationText[2] = { animationChar, '\0' };

    const int clockColor = isRunning ? UI_COLOR_ACTIVE : UI_COLOR_NORMAL;

    canvas.setFont(Font::Tiny);
    canvas.setColor(clockColor);
    canvas.drawText(2, 8 - 2, FixedStringBuilder<24>(TXT_INFO_TEMPO, name, engine.tempo()));
    canvas.drawText(24, 8 - 2, animationText);
}

/**
 * Draws the active state information, such as track, pattern, and snapshot status.
 * @param canvas The canvas to draw on.
 * @param engine The engine providing state data.
 * @param track The current track index.
 * @param playPattern The playing pattern index.
 * @param editPattern The editing pattern index.
 * @param snapshotActive Whether snapshot mode is active.
 * @param songActive Whether song mode is active.
 */
void WindowPainter::drawActiveState(Canvas &canvas, const Engine &engine, int track, int playPattern, int editPattern, bool snapshotActive, bool songActive) {
    canvas.setFont(Font::Tiny);

    const bool isRunning = engine.state().running();
    const int stateColor = isRunning ? UI_COLOR_ACTIVE : UI_COLOR_NORMAL;

    const int y = 8 - 2;
    const int trackX = 68;
    const int patternX = 83;
    const int editPatternX = 98;

    canvas.setColor(stateColor);
    canvas.drawText(trackX, y, FixedStringBuilder<8>(TXT_INFO_TRACK, track + 1));

    if (snapshotActive) {
        canvas.setColor(UI_COLOR_ACTIVE);
        drawInvertedText(canvas, patternX, y, TXT_INFO_SNAP_MODE, true);
        return;
    }

    drawInvertedText(canvas, patternX, y, FixedStringBuilder<8>(TXT_INFO_PATTERN, playPattern + 1), songActive);

    const bool isEditPatternActive = playPattern == editPattern;
    canvas.setColor(UI_COLOR_ACTIVE);
    drawInvertedText(canvas, editPatternX, y, FixedStringBuilder<8>(TXT_INFO_EDIT_PATTERN, editPattern + 1), isEditPatternActive);
}

/**
 * Draws the current active mode in the header, optionally including the active function.
 * @param canvas The canvas to draw on.
 * @param mode The mode string to display.
 */
void WindowPainter::drawActiveMode(Canvas &canvas, const char *mode) {
    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(UI_COLOR_NORMAL);

    // Clear the sticky function label when the mode changes.
    const bool modeChanged = strcmp(g_activeMode, mode) != 0;
    if (modeChanged) {
        strncpy(g_activeMode, mode, sizeof(g_activeMode) - 1);
        g_activeMode[sizeof(g_activeMode) - 1] = '\0';
        g_hasActiveFunction = false;
    }

    const bool showFunction = g_hasActiveFunction && g_activeFunction[0];
    FixedStringBuilder<48> modeText = showFunction
        ? FixedStringBuilder<48>(TXT_INFO_CURRENT_MODE_AND_FUNCTION, g_activeFunction, mode)
        : FixedStringBuilder<48>(TXT_INFO_CURRENT_MODE, mode);

    const int textX = PageWidth - canvas.textWidth(modeText) - 2;
    const int textY = 8 - 2;
    canvas.drawText(textX, textY, modeText);
}

/**
 * Sets the active function for display in the mode area.
 * @param canvas Unused parameter.
 * @param function The function string to set.
 */
void WindowPainter::drawActiveFunction(Canvas &canvas, const char *function) {
    (void)canvas;

    if (function && function[0]) {
        strncpy(g_activeFunction, function, sizeof(g_activeFunction) - 1);
        g_activeFunction[sizeof(g_activeFunction) - 1] = '\0';
        g_hasActiveFunction = true;
    }
}

/**
 * Draws the header section, including clock, active state, and mode.
 * @param canvas The canvas to draw on.
 * @param model The model providing project data.
 * @param engine The engine providing state and clock data.
 * @param mode The current mode string.
 */
void WindowPainter::drawHeader(Canvas &canvas, const Model &model, const Engine &engine, const char *mode) {
    // Extract necessary data from model and engine
    const auto &project = model.project();
    const int track = project.selectedTrackIndex();
    const int playPattern = project.playState().trackState(track).pattern();
    const int editPattern = project.selectedPatternIndex();
    const bool snapshotActive = project.playState().snapshotActive();
    const bool songActive = project.playState().songState().playing();

    // Draw the header separator line
    canvas.setColor(UI_COLOR_DIM);
    canvas.hline(0, HeaderHeight, PageWidth);

    // Draw header components
    drawClock(canvas, engine);
    drawActiveState(canvas, engine, track, playPattern, editPattern, snapshotActive, songActive);
    drawActiveMode(canvas, mode);

    // Reset blend mode
    canvas.setBlendMode(BlendMode::Set);
}

/**
 * Draws a simple footer line.
 * @param canvas The canvas to draw on.
 */
void WindowPainter::drawFooter(Canvas &canvas) {
    canvas.setBlendMode(BlendMode::Add);
    canvas.setColor(UI_COLOR_FOOTER_BACK);
    canvas.fillRect(0, PageHeight - FooterHeight, PageWidth, FooterHeight);

    /* canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(UI_COLOR_DIM);
    canvas.hline(0, PageHeight - FooterHeight - 1, PageWidth);*/
}

/**
 * Draws the footer with function keys, delegating to drawFunctionKeys.
 * @param canvas The canvas to draw on.
 * @param names Array of strings representing the names of the function keys.
 * @param keyState The current state of the keys.
 * @param highlight The index of the function key to highlight (-1 for none).
 */
void WindowPainter::drawFooter(Canvas &canvas, const char *names[], const KeyState &keyState, int highlight) {
    drawFunctionKeys(canvas, names, keyState, highlight);
}

/**
 * Draws a scrollbar on the given canvas to represent the visible portion of a list or content area.
 * The scrollbar consists of a track (background) and a thumb (indicator) that shows the current view position.
 *
 * @param canvas The canvas to draw on.
 * @param totalRows The total number of rows in the content.
 * @param visibleRows The number of rows currently visible in the view.
 * @param displayRow The index of the first visible row in the content (0-based).
 */
void WindowPainter::drawScrollbar(Canvas &canvas, int totalRows, int visibleRows, int displayRow) {
    if (visibleRows >= totalRows) {
        return;
    }

    const int scrollbarWidth = 2;
    const int scrollbarTop = HeaderHeight + 1;
    const int scrollbarHeight = PageHeight - (scrollbarTop * 2);
    const int scrollbarX = PageWidth - scrollbarWidth;

    const int thumbHeight = (visibleRows * scrollbarHeight) / totalRows;
    const int thumbY = (displayRow * scrollbarHeight) / totalRows;

    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(UI_COLOR_DIM_MORE);
    canvas.fillRect(scrollbarX, scrollbarTop, scrollbarWidth, scrollbarHeight);

    canvas.setColor(UI_COLOR_ACTIVE);
    canvas.fillRect(scrollbarX, scrollbarTop + thumbY, scrollbarWidth, thumbHeight);
}
