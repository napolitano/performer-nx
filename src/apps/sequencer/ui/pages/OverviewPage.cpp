#include "Config.h"
#include "OverviewPage.h"

#include "model/NoteTrack.h"

#include "ui/painters/WindowPainter.h"

#include "model/Scale.h"

enum class ContextAction {
    Maps,
    Gates,
    Notes,
    Voltages,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { TXT_MENU_MAPS },
    { TXT_MENU_GATES },
    { TXT_MENU_NOTES },
    { TXT_MENU_VOLTAGES },
    { },
};

static bool showMaps = true;
static bool showVoltages = true;
static bool showNotes = true;
static bool showGates = true;

static void drawNoteTrack(Canvas &canvas, int trackIndex, const NoteTrackEngine &trackEngine, const NoteSequence &sequence) {
    canvas.setBlendMode(BlendMode::Set);

    int stepOffset = (std::max(0, trackEngine.currentStep()) / 16) * 16;
    int y = trackIndex * 8;

    if (showMaps) {
        // Draw header showing the number of 16-step slices and the active slice
        for (int i= 0; i < 4; ++i) {
            //int stepIndex = stepOffset + i;
            //const auto &step = sequence.step(stepIndex);
            int x = 34  + i * 5;

            int stepColor = UI_COLOR_ACTIVE;


            if (sequence.firstStep() / 16 > i || i > sequence.lastStep() / 16) {
                stepColor = UI_COLOR_INACTIVE;
            } else if (trackEngine.currentStep() / 16 == i) {
                stepColor = UI_COLOR_ACTIVE;
            } else if (trackEngine.currentStep() / 16 != i) {
                stepColor = UI_COLOR_NORMAL;
            }

            canvas.setColor(stepColor);
            canvas.fillRect(x + 1, y + 2, 4, 4);

        }
    }

    for (int i = 0; i < 16; ++i) {
        int stepIndex = stepOffset + i;
        const auto &step = sequence.step(stepIndex);

        int x = 64 + i * 8;

        // Draw the steps of the current 16-step slice
        int stepColor = UI_COLOR_ACTIVE;

        if (stepIndex < sequence.firstStep() || stepIndex > sequence.lastStep()) {
            stepColor = UI_COLOR_INACTIVE;
        } else if (trackEngine.currentStep() == stepIndex && step.gate()) {
            stepColor = UI_COLOR_ACTIVE;
        } else if (trackEngine.currentStep() == stepIndex && !step.gate()) {
            stepColor = UI_COLOR_ACTIVE;
        } else if (trackEngine.currentStep() != stepIndex && step.gate()) {
            stepColor = UI_COLOR_NORMAL;
        } else if (trackEngine.currentStep() != stepIndex && !step.gate()) {
            stepColor = UI_COLOR_DIM_MORE;
        }

        canvas.setColor(stepColor);
        canvas.fillRect(x + 1, y + 1, 6, 6);

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
    canvas.setColor(UI_COLOR_NORMAL);

    int stepOffset = (std::max(0, trackEngine.currentStep()) / 16) * 16;
    int y = trackIndex * 8;

    if (showMaps) {
        // Draw header showing the number of 16-step slices and the active slice
        for (int i= 0; i < 4; ++i) {
            //int stepIndex = stepOffset + i;
            //const auto &step = sequence.step(stepIndex);
            int x = 34  + i * 5;

            int stepColor = UI_COLOR_ACTIVE;


            if (sequence.firstStep() / 16 > i || i > sequence.lastStep() / 16) {
                stepColor = UI_COLOR_INACTIVE;
            } else if (trackEngine.currentStep() / 16 == i) {
                stepColor = UI_COLOR_ACTIVE;
            } else if (trackEngine.currentStep() / 16 != i) {
                stepColor = UI_COLOR_NORMAL;
            }

            canvas.setColor(stepColor);
            canvas.fillRect(x + 1, y + 2, 4, 4);

        }
    }

    float lastY = -1.f;

    for (int i = 0; i < 16; ++i) {
        int stepIndex = stepOffset + i;
        const auto &step = sequence.step(stepIndex);
        float min = step.minNormalized();
        float max = step.maxNormalized();
        const auto function = Curve::function(Curve::Type(std::min(Curve::Last - 1, step.shape())));

        int x = 64 + i * 8;

        int stepColor = UI_COLOR_ACTIVE;

        if (sequence.firstStep() > i || i > sequence.lastStep()) {
            stepColor = UI_COLOR_INACTIVE;
        } else if (trackEngine.currentStep() / 16 == i) {
            stepColor = UI_COLOR_ACTIVE;
        } else if (trackEngine.currentStep() / 16 != i) {
            stepColor = UI_COLOR_NORMAL;
        }

        canvas.setColor(stepColor);

        drawCurve(canvas, x, y + 1, 8, 6, lastY, function, min, max);
    }

    if (trackEngine.currentStep() >= 0) {
        int x = 64 + ((trackEngine.currentStep() - stepOffset) + trackEngine.currentStepFraction()) * 8;
        canvas.setBlendMode(BlendMode::Set);
        canvas.setColor(UI_COLOR_ACTIVE);
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
    canvas.setColor(UI_COLOR_DIM);

    canvas.vline(64 - 3, 0, 64);
    canvas.vline(64 - 2, 0, 64);
    canvas.vline(192 + 1, 0, 64);
    canvas.vline(192 + 2, 0, 64);

    for (int trackIndex = 0; trackIndex < 8; trackIndex++) {
        const auto &track = _project.track(trackIndex);
        const auto &trackState = _project.playState().trackState(trackIndex);
        const auto &trackEngine = _engine.trackEngine(trackIndex);

        canvas.setBlendMode(BlendMode::Set);
        canvas.setColor(UI_COLOR_DIM);

        int y = 5 + trackIndex * 8;

        // track number / pattern number
        canvas.setColor(trackState.mute() ? UI_COLOR_DIM : UI_COLOR_ACTIVE);
        canvas.drawText(2, y, FixedStringBuilder<8>(TXT_INFO_TRACK, trackIndex + 1));
        canvas.drawText(18, y, FixedStringBuilder<8>(TXT_INFO_PATTERN, trackState.pattern() + 1));

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

        if (showGates) {
            bool gate = _engine.gateOutput() & (1 << trackIndex);
            canvas.setColor(gate ? UI_COLOR_ACTIVE : UI_COLOR_DIM);
            canvas.fillRect(gatesX, trackIndex * 8 + 1, 6, 6);
        }

        // cv output
        canvas.setColor(UI_COLOR_ACTIVE);

        if (showNotes) {
            if (track.trackMode() == Track::TrackMode::Note) {
                auto &sequence = track.noteTrack().sequence(trackState.pattern());
                const auto &scale = sequence.selectedScale(_project.scale());
                int rootNote = sequence.selectedRootNote(_model.project().rootNote());
                int note = scale.noteFromVolts(_engine.cvOutput().channel(trackIndex));
                FixedStringBuilder<16> noteName;
                scale.noteName(noteName,  note, rootNote, Scale:: Long);

                canvas.drawText(notesX, y, FixedStringBuilder<8>(TXT_INFO_NOTE, (const char *)(noteName)));
            }
        }

        if (showVoltages) {
            canvas.drawText(voltagesX, y, FixedStringBuilder<8>(TXT_INFO_VOLTAGE, _engine.cvOutput().channel(trackIndex)));
        }

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


    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    if (key.isGlobal()) {
        return;
    }

    // event.consume();
}

void OverviewPage::encoder(EncoderEvent &event) {
    // event.consume();
}

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
