#include "Config.h"
#include "SongPainter.h"

void SongPainter::drawArrowDown(Canvas &canvas, int x, int y, int w) {
    x += w / 2;
    canvas.vline(x, y, 2);
    canvas.point(x - 1, y);
    canvas.point(x + 1, y);
}

void SongPainter::drawArrowUp(Canvas &canvas, int x, int y, int w) {
    x += w / 2;
    canvas.vline(x, y, 2);
    canvas.point(x - 1, y + 1);
    canvas.point(x + 1, y + 1);
}

void SongPainter::drawArrowRight(Canvas &canvas, int x, int y, int w, int h) {
    x += w / 2;
    y += h / 2;

    canvas.setFont(Font::Tiny);
    canvas.drawText(x - 2, y + 2, GLYPH_TINY_SMALL_ARROW_RIGHT);

    canvas.setBlendMode(BlendMode::Add);
    canvas.setColor(UI_COLOR_INACTIVE);
    canvas.fillRect(x - 2, y - 3, CONFIG_LCD_WIDTH - x + 2, 7);
}

void SongPainter::drawProgress(Canvas &canvas, int x, int y, int w, int h, float progress) {
    int pw = w * progress;
    canvas.setColor(UI_COLOR_DIM_MORE);
    canvas.fillRect(x + pw, y, w - pw, h);
    canvas.setColor(UI_COLOR_ACTIVE);
    canvas.fillRect(x, y, pw, h);
}
