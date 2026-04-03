#pragma once

#include "../Widget.h"

#include "nanovg.h"

#include <cstdint>
#include <algorithm>
#include <cmath>

namespace sim {

class Display : public Widget {
public:
    typedef std::shared_ptr<Display> Ptr;

    Display(const Vector2f &pos, const Vector2f &size, const Vector2i &resolution, const Color &color = Color(0.8f, 0.9f, 0.f, 1.f)) :
        Widget(pos, size),
        _resolution(resolution),
        _color(color),
        _frameBuffer(new uint32_t[resolution.prod()])
    {
    }

    const Vector2i &resolution() { return _resolution; }

    const Color &color() const { return _color; }
          Color &color()       { return _color; }

    void draw(const uint8_t *frameBuffer) {
        const uint8_t *src = frameBuffer;
        uint32_t *dst = _frameBuffer.get();
        for (int i = 0; i < _resolution.prod(); ++i) {
            float s = std::min(uint8_t(15), *src++) * (1.f / 15.f);
            *dst++ = Color(s * _color.r(), s * _color.g(), s * _color.b(), 1.f).rgba();
        }
        _frameBufferDirty = true;
    }

    virtual void update() override {
    }

    virtual void render(Renderer &renderer) override {
        auto nvg = renderer.nvg();
        const uint8_t *frameBuffer = reinterpret_cast<uint8_t *>(_frameBuffer.get());

        // Integer-only scaling for pixel-perfect LCD output.
        // This avoids fractional scaling blur and keeps each simulated LCD pixel crisp.
        const float maxScaleX = _size.x() / std::max(1, _resolution.x());
        const float maxScaleY = _size.y() / std::max(1, _resolution.y());
        const int integerScale = std::max(1, static_cast<int>(std::floor(std::min(maxScaleX, maxScaleY))));

        const Vector2f drawSize(_resolution.x() * integerScale, _resolution.y() * integerScale);
        Vector2f drawPos = _pos + (_size - drawSize) * 0.5f;

        // Snap to integer pixel coordinates so the nearest-neighbor sample grid stays stable.
        drawPos = Vector2f(std::round(drawPos.x()), std::round(drawPos.y()));

        // update texture
        if (_image == -1) {
            _image = nvgCreateImageRGBA(nvg, _resolution.x(), _resolution.y(), NVG_IMAGE_NEAREST, frameBuffer);
        } else {
            if (_frameBufferDirty) {
                nvgUpdateImage(nvg, _image, frameBuffer);
                _frameBufferDirty = false;
            }
        }

        _pattern = nvgImagePattern(nvg, drawPos.x(), drawPos.y(), drawSize.x(), drawSize.y(), 0.f, _image, 1.f);

        // Fill the LCD cavity first so centered letterbox margins blend with the panel.
        nvgBeginPath(nvg);
        nvgRect(nvg, _pos.x(), _pos.y(), _size.x(), _size.y());
        nvgFillColor(nvg, nvgRGBA(0, 0, 0, 255));
        nvgFill(nvg);

		nvgBeginPath(nvg);
        nvgRect(nvg, drawPos.x(), drawPos.y(), drawSize.x(), drawSize.y());
        nvgFillPaint(nvg, _pattern);
        nvgFill(nvg);

        renderer.setColor(Color(0.5f, 1.f));
        renderer.drawRect(_pos, _size);
    }

private:
    Vector2i _resolution;
    Color _color;
    std::unique_ptr<uint32_t[]> _frameBuffer;
    bool _frameBufferDirty = true;
    int _image = -1;
    NVGpaint _pattern;
};

} // namespace sim
