#include "Config.h"
#include "StartupPage.h"

#include "ui/pages/Pages.h"

#include "model/FileManager.h"

#include "core/utils/StringBuilder.h"

#include "os/os.h"

StartupPage::StartupPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{
    _startTicks = os::ticks();
}

void StartupPage::draw(Canvas &canvas) {
    if (_state == State::Initial) {
        _state = State::Loading;
        _engine.suspend();
        FileManager::task([this] () {
            return FileManager::readLastProject(_model.project());
        }, [this] (fs::Error result) {
            _engine.resume();
            _state = State::Ready;
        });
    }

    if (relTime() > 1.f && _state == State::Ready) {
        close();
    }

    canvas.setColor(0);
    canvas.fill();

    canvas.setColor(UI_COLOR_ACTIVE);
    canvas.setFont(Font::Small);
    canvas.drawTextCentered(0, 2, Width, 32, CONFIG_VERSION_NAME);

    int yStart = 33;
    int minWidth = 6;
    int maxWidth = 9;
    int distance = 2;
    int indicatorCount = 5;
    int throttle = 4;
    bool filled = false;
    bool modulateColor = true;
    IndicatorDirection direction = IndicatorDirection::CenterOut;

    drawActivityIndicator(canvas, yStart, minWidth, maxWidth, distance, indicatorCount, throttle, filled, modulateColor, direction);

    canvas.setColor(UI_COLOR_DIM_MORE);
    canvas.setFont(Font::Tiny);
    FixedStringBuilder<32> versionString;
    versionString(TXT_INFO_VERSION_NUMBER, CONFIG_VERSION_MAJOR, CONFIG_VERSION_MINOR, CONFIG_VERSION_REVISION);
    canvas.drawTextCentered(0, 35, Width, 40, versionString);
}

void StartupPage::updateLeds(Leds &leds) {
    leds.clear();

    int progress = std::floor(relTime() * 8.f);
    for (int i = 0; i < 8; ++i) {
        bool active = i <= progress;
        leds.set(MatrixMap::fromTrack(i), active, active);
        leds.set(MatrixMap::fromStep(i), active, active);
        leds.set(MatrixMap::fromStep(i + 8), active, active);
    }
}

float StartupPage::time() const {
    return (os::ticks() - _startTicks) / float(os::time::ms(1000));
}

void StartupPage::drawActivityIndicator(Canvas &canvas, int yStart, int minWidth, int maxWidth, int distance, int indicatorCount, int throttle,bool filled, bool modulateColor, IndicatorDirection direction) {
   if (indicatorCount <= 0 || minWidth <= 0 || maxWidth < minWidth) {
        return;
    }

    if (throttle < 1) {
        throttle = 1;
    }

    static int frameDivider = 0;
    static float phase = 0.f;

    if (++frameDivider >= throttle) {
        frameDivider = 0;

        phase += 0.04f;
        if (phase >= 1.f) {
            phase -= 1.f;
        }
    }

    const int totalWidth = indicatorCount * minWidth + (indicatorCount - 1) * distance;
    int x0 = (256 - totalWidth) / 2;
    if (x0 < 0) {
        x0 = 0;
    }

    const float phaseStep = 1.f / float(indicatorCount);

    for (int i = 0; i < indicatorCount; ++i) {
        float p = 0.f;

        switch (direction) {

        case IndicatorDirection::RightToLeft:
            p = phase + i * phaseStep;
            break;

        case IndicatorDirection::LeftToRight:
            p = phase + (indicatorCount - 1 - i) * phaseStep;
            break;

        case IndicatorDirection::CenterOut: {
            float center = (indicatorCount - 1) * 0.5f;
            float dist = std::abs(i - center);

            float maxDist = center;
            float norm = maxDist > 0.f ? dist / maxDist : 0.f;

            norm = 1.f - norm;

            p = phase + norm;
            break;
        }

        }

        // wrap
        while (p >= 1.f) {
            p -= 1.f;
        }

        float pulse;
        if (p < 0.5f) {
            float t = p / 0.5f;
            pulse = t * t;          // accelerate
        } else {
            float t = (p - 0.5f) / 0.5f;
            float u = 1.f - t;
            pulse = u * u;          // decelerate
        }

        int size = minWidth + int((maxWidth - minWidth) * pulse + 0.5f);

        int baseX = x0 + i * (minWidth + distance);

        int drawX = baseX - (size - minWidth) / 2;
        int drawY = yStart - (size - minWidth) / 2;

        int color = UI_COLOR_ACTIVE;

        if (modulateColor) {
            const int minColor = UI_COLOR_INACTIVE;
            const int maxColor = 0xf;

            float brightness = pulse * pulse;
            brightness *= brightness;   // optional, for stronger contrast

            color = minColor + int((maxColor - minColor) * brightness + 0.5f);
            color = clamp(color, 0x0, 0xf);
        }

        canvas.setColor(color);
        if (filled) {
            canvas.fillRect(drawX, drawY, size, size);
        } else {
            canvas.drawRect(drawX, drawY, size, size);
        }
    }
}