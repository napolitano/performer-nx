#include "Config.h"
#include "SequencePainter.h"

// SequencePainter renders compact visual widgets used by step/sequence editors.
// Keep primitives simple and deterministic so painting remains cheap on embedded targets.

void SequencePainter::drawLoopStart(Canvas &canvas, int x, int y, int w) {
    canvas.vline(x, y - 1, 3);
    canvas.point(x + 1, y);

    canvas.setFont(Font::Tiny);
    canvas.drawText(x - 1, y + 2, GLYPH_TINY_SMALL_ARROW_RIGHT);
}

void SequencePainter::drawLoopEnd(Canvas &canvas, int x, int y, int w) {
    x += w - 1;
    canvas.vline(x, y - 1, 3);
    canvas.point(x - 1, y);

    canvas.setFont(Font::Tiny);
    canvas.drawText(x - 1, y + 2, GLYPH_TINY_SMALL_ARROW_LEFT);
}

void SequencePainter::drawOffset(Canvas &canvas, int baseColor, int x, int y, int w, int h, int offset, int minOffset, int maxOffset) {
    // Map value range [minOffset..maxOffset] into the local widget width.
    auto remap = [w, minOffset, maxOffset] (int value) {
        return ((w - 1) * (value - minOffset)) / (maxOffset - minOffset);
    };

    canvas.setBlendMode(BlendMode::Set);

    // Use a dimmed variant of baseColor for the background bar.
    canvas.setColor((baseColor > 9) ? baseColor - 8 : baseColor);
    canvas.fillRect(x, y, w, h);

    canvas.setColor(UI_COLOR_BLACK);
    canvas.vline(x + remap(0), y, h);

    canvas.setColor(baseColor);
    canvas.vline(x + remap(offset), y, h);
}

void SequencePainter::drawRetrigger(Canvas &canvas, int baseColor, int x, int y, int w, int h, int retrigger, int maxRetrigger) {
    canvas.setBlendMode(BlendMode::Set);

    // Split the widget into equal bins and center the active cluster.
    int bw = w / maxRetrigger;
    x += (w - bw * retrigger) / 2;

    canvas.setColor(baseColor);

    for (int i = 0; i < retrigger; ++i) {
        canvas.fillRect(x, y, bw / 2, h);
        x += bw;
    }
}

void SequencePainter::drawProbability(Canvas &canvas, int baseColor, int x, int y, int w, int h, int probability, int maxProbability) {
    canvas.setBlendMode(BlendMode::Set);

    int pw = (w * probability) / maxProbability;

    canvas.setColor(baseColor);
    canvas.fillRect(x, y, pw, h);

    // Fill the inactive remainder with a dimmed tone for contrast.
    canvas.setColor((baseColor > 9) ? baseColor - 8 : baseColor);
    canvas.fillRect(x + pw, y, w - pw, h);
}

void SequencePainter::drawLength(Canvas &canvas, int baseColor, int x, int y, int w, int h, int length, int maxLength) {
    canvas.setBlendMode(BlendMode::Set);

    int gw = ((w - 1) * length) / maxLength;

    canvas.setColor(baseColor);

    canvas.vline(x, y, h);
    canvas.hline(x, y, gw);
    canvas.vline(x + gw, y, h);
    canvas.hline(x + gw, y + h - 1, w - gw);
}

void SequencePainter::drawLengthRange(Canvas &canvas, int baseColor, int x, int y, int w, int h, int length, int range, int maxLength) {
    canvas.setBlendMode(BlendMode::Set);

    // gw: nominal gate length, rw: end of randomized/variation range.
    int gw = ((w - 1) * length) / maxLength;
    int rw = ((w - 1) * std::max(0, std::min(maxLength, length + range))) / maxLength;

    canvas.setColor((baseColor > 9) ? baseColor - 8 : baseColor);

    canvas.vline(x, y, h);
    canvas.hline(x, y, gw);
    canvas.vline(x + gw, y, h);
    canvas.hline(x + gw, y + h - 1, w - gw);

    canvas.setColor(baseColor);

    // Highlight the effective variation span regardless of sign/direction.
    canvas.fillRect(x + std::min(gw, rw), y + 2, std::max(gw, rw) - std::min(gw, rw) + 1, h - 4);
}

void SequencePainter::drawSlide(Canvas &canvas, int baseColor, int x, int y, int w, int h, bool active) {
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(baseColor);

    if (active) {
        canvas.line(x, y + h, x + w, y);
    } else {
        canvas.hline(x, y + h, w);
    }
}

void SequencePainter::drawSequenceProgress(Canvas &canvas, int baseColor, int x, int y, int w, int h, float progress) {
    if (progress < 0.f) {
        return;
    }

    canvas.setBlendMode(BlendMode::Set);
    // Negative progress means "hidden/disabled"; otherwise draw full bar + playhead.
    canvas.setColor((baseColor > 9) ? baseColor - 8 : baseColor);
    canvas.fillRect(x, y, w, h);
    canvas.setColor(baseColor);
    canvas.vline(x + int(std::floor(progress * w)), y, h);
}

