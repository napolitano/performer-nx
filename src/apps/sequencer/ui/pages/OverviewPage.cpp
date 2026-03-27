#include "OverviewPage.h"

#include "model/NoteTrack.h"

#include "ui/painters/WindowPainter.h"

#include "model/Scale.h"

#ifdef CONFIG_OVERVIEW_ENHANCEMENTS
enum class ContextAction {
    Maps,
    Gates,
    Notes,
    Voltages,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { "MAPS" },
    { "GATES" },
    { "NOTES" },
    { "VOLTAGES" },
    { },
};

static bool showMaps = true;
static bool showVoltages = true;
static bool showNotes = true;
static bool showGates = true;
#endif

static void drawNoteTrack(Canvas &canvas, int trackIndex, const NoteTrackEngine &trackEngine, const NoteSequence &sequence) {
    canvas.setBlendMode(BlendMode::Set);

    int stepOffset = (std::max(0, trackEngine.currentStep()) / 16) * 16;
    int y = trackIndex * 8;

#ifdef CONFIG_OVERVIEW_ENHANCEMENTS
    if (showMaps) {
        // Draw header showing the number of 16-step slices and the active slice
        for (int i= 0; i < 4; ++i) {
            //int stepIndex = stepOffset + i;
            //const auto &step = sequence.step(stepIndex);
            int x = 34  + i * 5;

            int stepColor = BasePage::UI_COLOR_ACTIVE;


            if (sequence.firstStep() / 16 > i || i > sequence.lastStep() / 16) {
                stepColor = BasePage::UI_COLOR_INACTIVE;
            } else if (trackEngine.currentStep() / 16 == i) {
                stepColor = BasePage::UI_COLOR_ACTIVE;
            } else if (trackEngine.currentStep() / 16 != i) {
                stepColor = BasePage::UI_COLOR_NORMAL;
            }

            canvas.setColor(stepColor);
            canvas.fillRect(x + 1, y + 2, 4, 4);

        }
    }
#endif

    for (int i = 0; i < 16; ++i) {
        int stepIndex = stepOffset + i;
        const auto &step = sequence.step(stepIndex);

        int x = 64 + i * 8;

#ifdef CONFIG_OVERVIEW_ENHANCEMENTS
        // Draw the steps of the current 16-step slice
        int stepColor = BasePage::UI_COLOR_ACTIVE;

        if (stepIndex < sequence.firstStep() || stepIndex > sequence.lastStep()) {
            stepColor = BasePage::UI_COLOR_INACTIVE;
        } else if (trackEngine.currentStep() == stepIndex && step.gate()) {
            stepColor = BasePage::UI_COLOR_ACTIVE;
        } else if (trackEngine.currentStep() == stepIndex && !step.gate()) {
            stepColor = BasePage::UI_COLOR_ACTIVE;
        } else if (trackEngine.currentStep() != stepIndex && step.gate()) {
            stepColor = BasePage::UI_COLOR_NORMAL;
        } else if (trackEngine.currentStep() != stepIndex && !step.gate()) {
            stepColor = BasePage::UI_COLOR_DIM_MORE;
        }

        canvas.setColor(stepColor);
        canvas.fillRect(x + 1, y + 1, 6, 6);
#else
        if (trackEngine.currentStep() == stepIndex) {
            canvas.setColor(step.gate() ? 0xf : 0xa);
            canvas.fillRect(x + 1, y + 1, 6, 6);
        } else {
            canvas.setColor(step.gate () ? 0x7 : 0x3);
            canvas.fillRect(x + 1, y + 1, 6, 6);
        }
#endif
        // if (trackEngine.currentStep() == stepIndex) {
        //     canvas.setColor(0xf);
        //     canvas.drawRect(x + 1, y + 1, 6, 6);
        // }
    }
}

static void drawCurve(Canvas &canvas, int x, int y, int w, int h, float &lastY, const Curve::Function function, float min, float max) {
    const int Step = 1;

    auto eval = [=] (float x) {
        return (1.f - (function(x) * (max - min) + min)) * h;
    };

    float fy0 = y + eval(0.f);

    if (lastY >= 0.f && lastY != fy0) {
        canvas.line(x, lastY, x, fy0);
    }

    for (int i = 0; i < w; i += Step) {
        float fy1 = y + eval((float(i) + Step) / w);
        canvas.line(x + i, fy0, x + i + Step, fy1);
        fy0 = fy1;
    }

    lastY = fy0;
}

static void drawCurveTrack(Canvas &canvas, int trackIndex, const CurveTrackEngine &trackEngine, const CurveSequence &sequence) {
    canvas.setBlendMode(BlendMode::Add);
    canvas.setColor(0xa);

    int stepOffset = (std::max(0, trackEngine.currentStep()) / 16) * 16;
    int y = trackIndex * 8;

#ifdef CONFIG_OVERVIEW_ENHANCEMENTS
    if (showMaps) {
        // Draw header showing the number of 16-step slices and the active slice
        for (int i= 0; i < 4; ++i) {
            //int stepIndex = stepOffset + i;
            //const auto &step = sequence.step(stepIndex);
            int x = 34  + i * 5;

            int stepColor = BasePage::UI_COLOR_ACTIVE;


            if (sequence.firstStep() / 16 > i || i > sequence.lastStep() / 16) {
                stepColor = BasePage::UI_COLOR_INACTIVE;
            } else if (trackEngine.currentStep() / 16 == i) {
                stepColor = BasePage::UI_COLOR_ACTIVE;
            } else if (trackEngine.currentStep() / 16 != i) {
                stepColor = BasePage::UI_COLOR_NORMAL;
            }

            canvas.setColor(stepColor);
            canvas.fillRect(x + 1, y + 2, 4, 4);

        }
    }
#endif

    float lastY = -1.f;

    for (int i = 0; i < 16; ++i) {
        int stepIndex = stepOffset + i;
        const auto &step = sequence.step(stepIndex);
        float min = step.minNormalized();
        float max = step.maxNormalized();
        const auto function = Curve::function(Curve::Type(std::min(Curve::Last - 1, step.shape())));

        int x = 64 + i * 8;

#ifdef CONFIG_OVERVIEW_ENHANCEMENTS
        int stepColor = BasePage::UI_COLOR_ACTIVE;

        if (sequence.firstStep() > i || i > sequence.lastStep()) {
            stepColor = BasePage::UI_COLOR_INACTIVE;
        } else if (trackEngine.currentStep() / 16 == i) {
            stepColor = BasePage::UI_COLOR_ACTIVE;
        } else if (trackEngine.currentStep() / 16 != i) {
            stepColor = BasePage::UI_COLOR_NORMAL;
        }

        canvas.setColor(stepColor);
#endif

        drawCurve(canvas, x, y + 1, 8, 6, lastY, function, min, max);
    }

    if (trackEngine.currentStep() >= 0) {
        int x = 64 + ((trackEngine.currentStep() - stepOffset) + trackEngine.currentStepFraction()) * 8;
        canvas.setBlendMode(BlendMode::Set);
        canvas.setColor(0xf);
        canvas.vline(x, y + 1, 7);
    }
}


OverviewPage::OverviewPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{}

void OverviewPage::enter() {
}

void OverviewPage::exit() {
}

void OverviewPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);

    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(0x7);

    canvas.vline(64 - 3, 0, 64);
    canvas.vline(64 - 2, 0, 64);
    canvas.vline(192 + 1, 0, 64);
    canvas.vline(192 + 2, 0, 64);

    for (int trackIndex = 0; trackIndex < 8; trackIndex++) {
        const auto &track = _project.track(trackIndex);
        const auto &trackState = _project.playState().trackState(trackIndex);
        const auto &trackEngine = _engine.trackEngine(trackIndex);

        canvas.setBlendMode(BlendMode::Set);
        canvas.setColor(0x7);

        int y = 5 + trackIndex * 8;

        // track number / pattern number
        canvas.setColor(trackState.mute() ? 0x7 : 0xf);
        canvas.drawText(2, y, FixedStringBuilder<8>("T%d", trackIndex + 1));
        canvas.drawText(18, y, FixedStringBuilder<8>("P%d", trackState.pattern() + 1));
#ifdef CONFIG_OVERVIEW_ENHANCEMENTS
        int gatesX = 256 - 60 + 1;
        int notesX = 256 - 51;
        int voltagesX = 256 - 27;

        if (showVoltages && !showNotes) {
            voltagesX = 256 - 32;
            gatesX = 256 - 48 +1;
        } else if (showNotes && ! showVoltages) {
            notesX = 256 - 32;
            gatesX = 256 - 48 + 1;
        } else if (!showNotes && !showVoltages) {
            gatesX = 256 - 32;
        }

#endif


#ifdef CONFIG_OVERVIEW_ENHANCEMENTS
        if (showGates) {
            bool gate = _engine.gateOutput() & (1 << trackIndex);
            canvas.setColor(gate ? 0xf : 0x7);
            canvas.fillRect(gatesX, trackIndex * 8 + 1, 6, 6);
        }
#else
        // gate output
        bool gate = _engine.gateOutput() & (1 << trackIndex);
        canvas.setColor(gate ? 0xf : 0x7);
        canvas.fillRect(256 - 48 + 1, trackIndex * 8 + 1, 6, 6);
#endif
        // cv output
        canvas.setColor(0xf);
#ifdef CONFIG_OVERVIEW_ENHANCEMENTS
        if (showNotes) {
            if (track.trackMode() == Track::TrackMode::Note) {
                auto &sequence = track.noteTrack().sequence(trackState.pattern());
                const auto &scale = sequence.selectedScale(_project.scale());
                int rootNote = sequence.selectedRootNote(_model.project().rootNote());
                int note = scale.noteFromVolts(_engine.cvOutput().channel(trackIndex));
                FixedStringBuilder<16> noteName;
                scale.noteName(noteName,  note, rootNote, Scale:: Long);

                canvas.drawText(notesX, y, FixedStringBuilder<8>("%-16s", (const char *)(noteName)));
            }
        }

        if (showVoltages) {
            canvas.drawText(voltagesX, y, FixedStringBuilder<8>("%.2fV", _engine.cvOutput().channel(trackIndex)));
        }
#else
        canvas.drawText(256 - 32, y, FixedStringBuilder<8>("%.2fV", _engine.cvOutput().channel(trackIndex)));
#endif

        switch (track.trackMode()) {
        case Track::TrackMode::Note:
            drawNoteTrack(canvas, trackIndex, trackEngine.as<NoteTrackEngine>(), track.noteTrack().sequence(trackState.pattern()));
            break;
        case Track::TrackMode::Curve:
            drawCurveTrack(canvas, trackIndex, trackEngine.as<CurveTrackEngine>(), track.curveTrack().sequence(trackState.pattern()));
            break;
        case Track::TrackMode::MidiCv:
            break;
        case Track::TrackMode::Last:
            break;
        }
    }
}

void OverviewPage::updateLeds(Leds &leds) {
}

void OverviewPage::keyDown(KeyEvent &event) {
    const auto &key = event.key();

    if (key.isGlobal()) {
        return;
    }

    // event.consume();
}

void OverviewPage::keyUp(KeyEvent &event) {
    const auto &key = event.key();

    if (key.isGlobal()) {
        return;
    }

    // event.consume();
}

void OverviewPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

#ifdef CONFIG_OVERVIEW_ENHANCEMENTS
    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }
#endif

    if (key.isGlobal()) {
        return;
    }

    // event.consume();
}

void OverviewPage::encoder(EncoderEvent &event) {
    // event.consume();
}

#ifdef CONFIG_OVERVIEW_ENHANCEMENTS
void OverviewPage::contextShow() {
    showContextMenu(ContextMenu(
    contextMenuItems,
    int(ContextAction::Last),
    [&] (int index) { contextAction(index); },
    [&] (int index) { return contextActionEnabled(index); }
));
}

void OverviewPage::contextAction(int index) {
    switch (ContextAction(index)) {
        case ContextAction::Maps:
            showMaps = !showMaps;
            break;
        case ContextAction::Notes:
            showNotes = !showNotes;
            break;
        case ContextAction::Voltages:
            showVoltages = !showVoltages;
            break;
        case ContextAction::Gates:
            showGates = !showGates;
            break;
        case ContextAction::Last:
            break;
    }
}

bool OverviewPage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {

        default:
            return true;
    }
}
#endif