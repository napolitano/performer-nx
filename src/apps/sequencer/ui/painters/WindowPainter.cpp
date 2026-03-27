#include "WindowPainter.h"

#include "Config.h"

#include "core/utils/StringBuilder.h"

#include <cstring>

#ifdef CONFIG_ENABLE_WINDOW_PAINTER_ENHANCEMENTS
namespace {
    char g_activeFunction[24] = {};
	char g_activeMode[24] = {};
    bool g_hasActiveFunction = false;
}
#endif

static void drawInvertedText(Canvas &canvas, int x, int y, const char *text, bool inverted = true) {
    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);
#ifndef CONFIG_ENABLE_WINDOW_PAINTER_ENHANCEMENTS
    canvas.setColor(0xf);
#endif

    if (inverted) {
        canvas.fillRect(x - 1, y - 5, canvas.textWidth(text) + 1, 7);
        canvas.setBlendMode(BlendMode::Sub);
    }

    canvas.drawText(x, y, text);
}

void WindowPainter::clear(Canvas &canvas) {
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(0);
    canvas.fill();
}

void WindowPainter::drawFrame(Canvas &canvas, int x, int y, int w, int h) {
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(0);
    canvas.fillRect(x, y, w, h);
    canvas.setColor(0xf);
    canvas.drawRect(x, y, w, h);
}

void WindowPainter::drawFunctionKeys(Canvas &canvas, const char *names[], const KeyState &keyState, int highlight) {
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(0x7);
    canvas.hline(0, PageHeight - FooterHeight - 1, PageWidth);

#ifndef CONFIG_ENABLE_WINDOW_PAINTER_ENHANCEMENTS
    for (int i = 0; i < FunctionKeyCount; ++i) {
        if (names[i] || (i + 1 < FunctionKeyCount && names[i + 1])) {
            int x = (PageWidth * (i + 1)) / FunctionKeyCount;
            canvas.vline(x, PageHeight - FooterHeight, FooterHeight);
        }
    }
#endif

    canvas.setFont(Font::Tiny);
    canvas.setColor(0xf);

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
    static const char *clockModeName[] = { "A", "M", "S" };
    const char *name = engine.recording() ? "R" : clockModeName[int(engine.clock().activeMode())];
#ifdef CONFIG_ENABLE_WINDOW_PAINTER_ENHANCEMENTS
    int clockColor = (engine.state().running()) ? 0xf : 0xc;

	canvas.setFont(Font::Tiny);
    canvas.setColor(clockColor);
    canvas.drawText(2, 8-2, FixedStringBuilder<12>("%-1s Í%.1f",name, engine.tempo()));

#else
    drawInvertedText(canvas, 2, 8 - 2, name);

    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(0xf);
    canvas.drawText(11, 8 - 2, FixedStringBuilder<8>("%.1f", engine.tempo()));
#endif
}
#ifdef CONFIG_ENABLE_WINDOW_PAINTER_ENHANCEMENTS
void WindowPainter::drawActiveState(Canvas &canvas, const Engine &engine, int track, int playPattern, int editPattern, bool snapshotActive, bool songActive) {
#else
void WindowPainter::drawActiveState(Canvas &canvas, int track, int playPattern, int editPattern, bool snapshotActive, bool songActive) {
#endif
    canvas.setFont(Font::Tiny);
#ifdef CONFIG_ENABLE_WINDOW_PAINTER_ENHANCEMENTS
	int trackPatternColor = (engine.state().running()) ? 0xf : 0xc;

    canvas.setColor(trackPatternColor);
    canvas.drawText(40, 8-2, FixedStringBuilder<8>("T%d", track + 1));

    if (snapshotActive) {
		canvas.setColor(0xf);
        drawInvertedText(canvas, 55, 8 - 2, "SNAP", true);
    } else {
        // draw active pattern
        drawInvertedText(canvas, 55, 8 - 2, FixedStringBuilder<8>("P%d", playPattern + 1), songActive);

		canvas.setColor(0xf);
        drawInvertedText(canvas, 70, 8 - 2, FixedStringBuilder<8>("E%d", editPattern + 1), playPattern == editPattern);
	}
#else
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(0xf);
    canvas.drawText(40, 8 - 2, FixedStringBuilder<8>("T%d", track + 1));

    if (snapshotActive) {
        drawInvertedText(canvas, 60, 8 - 2, "SNAP", true);
    } else {
        // draw active pattern
        drawInvertedText(canvas, 60, 8 - 2, FixedStringBuilder<8>("P%d", playPattern + 1), songActive);

		canvas.setColor(0xf);
        // draw edit pattern
        drawInvertedText(canvas, 75, 8 - 2, FixedStringBuilder<8>("E%d", editPattern + 1), playPattern == editPattern);
    }
#endif
}

#ifdef CONFIG_ENABLE_WINDOW_PAINTER_ENHANCEMENTS
void WindowPainter::drawActiveMode(Canvas &canvas, const char *mode) {
    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(0xc);
	// Sanitizing active function
	if(strcmp(g_activeMode,mode) != 0) {
		strncpy(g_activeMode, mode, sizeof(g_activeMode) - 1);
        g_activeMode[sizeof(g_activeMode) - 1] = '\0';
		g_hasActiveFunction = false;
	}

	FixedStringBuilder<48> functionMode;
    if (g_hasActiveFunction && g_activeFunction[0]) {
		functionMode = FixedStringBuilder<48>("%-s     %-s", g_activeFunction, mode);
	} else {
		functionMode = FixedStringBuilder<48>("%-s", mode);
	}

    canvas.drawText(PageWidth - canvas.textWidth(functionMode) - 2, 8 - 2, functionMode);
}
#else
void WindowPainter::drawActiveMode(Canvas &canvas, const char *mode) {
    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(0xf);
    canvas.drawText(PageWidth - canvas.textWidth(mode) - 2, 8 - 2, mode);
}
#endif

#ifdef CONFIG_ENABLE_WINDOW_PAINTER_ENHANCEMENTS
void WindowPainter::drawActiveFunction(Canvas &canvas, const char *function) {
    (void)canvas;

    if (function && function[0]) {
        strncpy(g_activeFunction, function, sizeof(g_activeFunction) - 1);
        g_activeFunction[sizeof(g_activeFunction) - 1] = '\0';
        g_hasActiveFunction = true;
    }
}
#else
void WindowPainter::drawActiveFunction(Canvas &canvas, const char *function) {
    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(0xf);
    canvas.drawText(100, 8 - 2, function);
}
#endif

void WindowPainter::drawHeader(Canvas &canvas, const Model &model, const Engine &engine, const char *mode) {
    const auto &project = model.project();
    int track = project.selectedTrackIndex();
    int playPattern = project.playState().trackState(track).pattern();
    int editPattern = project.selectedPatternIndex();
    bool snapshotActive = project.playState().snapshotActive();
    bool songActive = project.playState().songState().playing();

#ifdef CONFIG_ENABLE_WINDOW_PAINTER_ENHANCEMENTS
	canvas.setColor(0x7);
    canvas.hline(0, HeaderHeight, PageWidth);
#endif

    drawClock(canvas, engine);
#ifdef CONFIG_ENABLE_WINDOW_PAINTER_ENHANCEMENTS
    drawActiveState(canvas, engine, track, playPattern, editPattern, snapshotActive, songActive);
#else
    drawActiveState(canvas, track, playPattern, editPattern, snapshotActive, songActive);
#endif
    drawActiveMode(canvas, mode);

    canvas.setBlendMode(BlendMode::Set);

#ifndef CONFIG_ENABLE_WINDOW_PAINTER_ENHANCEMENTS
	canvas.setColor(0x7);
    canvas.hline(0, HeaderHeight, PageWidth);
#endif
}

void WindowPainter::drawFooter(Canvas &canvas) {
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(0x7);
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
    canvas.setColor(0x7);
    canvas.drawRect(x, y, w, h);

    int bh = (visibleRows * h) / totalRows;
    int by = (displayRow * h) / totalRows;
    canvas.setColor(0xf);
    canvas.fillRect(x, y + by, w, bh);
}
