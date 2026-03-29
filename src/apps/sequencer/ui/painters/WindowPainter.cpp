#include "WindowPainter.h"

#include "Config.h"

#include "core/utils/StringBuilder.h"

#include <cstring>

namespace {
    char g_activeFunction[24] = {};
	char g_activeMode[24] = {};
    bool g_hasActiveFunction = false;
}

static void drawInvertedText(Canvas &canvas, int x, int y, const char *text, bool inverted = true) {
    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);

    if (inverted) {
        canvas.fillRect(x - 1, y - 5, canvas.textWidth(text) + 1, 7);
        canvas.setBlendMode(BlendMode::Sub);
    }

    canvas.drawText(x, y, text);
}

void WindowPainter::clear(Canvas &canvas) {
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(UI_COLOR_BLACK);
    canvas.fill();
}

void WindowPainter::drawFrame(Canvas &canvas, int x, int y, int w, int h) {
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(UI_COLOR_BLACK);
    canvas.fillRect(x, y, w, h);
    canvas.setColor(UI_COLOR_ACTIVE);
    canvas.drawRect(x, y, w, h);
}

void WindowPainter::drawFunctionKeys(Canvas &canvas, const char *names[], const KeyState &keyState, int highlight) {
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(UI_COLOR_DIM);
    canvas.hline(0, PageHeight - FooterHeight - 1, PageWidth);

    canvas.setFont(Font::Tiny);
    canvas.setColor(UI_COLOR_ACTIVE);

    for (int i = 0; i < FunctionKeyCount; ++i) {
        if (names[i]) {
            bool pressed = keyState[Key::F0 + i];

            if (highlight >= 0) {
                pressed = i == highlight;
            }

            int x0 = (PageWidth * i) / FunctionKeyCount;
            int x1 = (PageWidth * (i + 1)) / FunctionKeyCount;
            int w = x1 - x0 + 1;

            canvas.setBlendMode(BlendMode::Set);

            if (pressed) {
                canvas.fillRect(x0, PageHeight - FooterHeight, w, FooterHeight);
                canvas.setBlendMode(BlendMode::Sub);
            }

            canvas.drawText(x0 + (w - canvas.textWidth(names[i])) / 2, PageHeight - 3, names[i]);
        }
    }
}

void WindowPainter::drawClock(Canvas &canvas, const Engine &engine) {
    static const char *clockModeName[] = {
        TXT_INFO_CLOCK_MODE_AUTO,
        TXT_INFO_CLOCK_MODE_MANUAL,
        TXT_INFO_CLOCK_MODE_SLAVE
    };

    const char *name = engine.recording() ? TXT_INFO_RECORD_MODE : clockModeName[int(engine.clock().activeMode())];
    int clockColor = (engine.state().running()) ? UI_COLOR_ACTIVE : UI_COLOR_NORMAL;

	canvas.setFont(Font::Tiny);
    canvas.setColor(clockColor);
    canvas.drawText(2, 8-2, FixedStringBuilder<12>(TXT_INFO_TEMPO,name, engine.tempo()));
}

void WindowPainter::drawActiveState(Canvas &canvas, const Engine &engine, int track, int playPattern, int editPattern, bool snapshotActive, bool songActive) {
    canvas.setFont(Font::Tiny);

    int trackPatternColor = (engine.state().running()) ? UI_COLOR_ACTIVE : UI_COLOR_NORMAL;

    canvas.setColor(trackPatternColor);
    canvas.drawText(40, 8-2, FixedStringBuilder<8>(TXT_INFO_TRACK, track + 1));

    if (snapshotActive) {
		canvas.setColor(UI_COLOR_ACTIVE);
        drawInvertedText(canvas, 55, 8 - 2, TXT_INFO_SNAP_MODE, true);
    } else {
        // draw active pattern
        drawInvertedText(canvas, 55, 8 - 2, FixedStringBuilder<8>(TXT_INFO_PATTERN, playPattern + 1), songActive);

		canvas.setColor(UI_COLOR_ACTIVE);
        drawInvertedText(canvas, 70, 8 - 2, FixedStringBuilder<8>(TXT_INFO_EDIT_PATTERN, editPattern + 1), playPattern == editPattern);
	}
}

void WindowPainter::drawActiveMode(Canvas &canvas, const char *mode) {
    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(UI_COLOR_NORMAL);
	// Sanitizing active function
	if(strcmp(g_activeMode,mode) != 0) {
		strncpy(g_activeMode, mode, sizeof(g_activeMode) - 1);
        g_activeMode[sizeof(g_activeMode) - 1] = '\0';
		g_hasActiveFunction = false;
	}

	FixedStringBuilder<48> functionMode;
    if (g_hasActiveFunction && g_activeFunction[0]) {
		functionMode = FixedStringBuilder<48>(TXT_INFO_CURRENT_MODE_AND_FUNCTION, g_activeFunction, mode);
	} else {
		functionMode = FixedStringBuilder<48>(TXT_INFO_CURRENT_MODE, mode);
	}

    canvas.drawText(PageWidth - canvas.textWidth(functionMode) - 2, 8 - 2, functionMode);
}

void WindowPainter::drawActiveFunction(Canvas &canvas, const char *function) {
    (void)canvas;

    if (function && function[0]) {
        strncpy(g_activeFunction, function, sizeof(g_activeFunction) - 1);
        g_activeFunction[sizeof(g_activeFunction) - 1] = '\0';
        g_hasActiveFunction = true;
    }
}

void WindowPainter::drawHeader(Canvas &canvas, const Model &model, const Engine &engine, const char *mode) {
    const auto &project = model.project();
    int track = project.selectedTrackIndex();
    int playPattern = project.playState().trackState(track).pattern();
    int editPattern = project.selectedPatternIndex();
    bool snapshotActive = project.playState().snapshotActive();
    bool songActive = project.playState().songState().playing();

	canvas.setColor(UI_COLOR_DIM);
    canvas.hline(0, HeaderHeight, PageWidth);

    drawClock(canvas, engine);
    drawActiveState(canvas, engine, track, playPattern, editPattern, snapshotActive, songActive);
    drawActiveMode(canvas, mode);

    canvas.setBlendMode(BlendMode::Set);
}

void WindowPainter::drawFooter(Canvas &canvas) {
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(UI_COLOR_DIM);
    canvas.hline(0, PageHeight - FooterHeight - 1, PageWidth);
}

void WindowPainter::drawFooter(Canvas &canvas, const char *names[], const KeyState &keyState, int highlight) {
    drawFunctionKeys(canvas, names, keyState, highlight);
}

void WindowPainter::drawScrollbar(Canvas &canvas, int x, int y, int w, int h, int totalRows, int visibleRows, int displayRow) {
    if (visibleRows >= totalRows) {
        return;
    }

    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(UI_COLOR_DIM);
    canvas.drawRect(x, y, w, h);

    int bh = (visibleRows * h) / totalRows;
    int by = (displayRow * h) / totalRows;
    canvas.setColor(UI_COLOR_ACTIVE);
    canvas.fillRect(x, y + by, w, bh);
}
