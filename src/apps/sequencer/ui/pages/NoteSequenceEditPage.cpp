#include "NoteSequenceEditPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/SequencePainter.h"
#include "ui/painters/WindowPainter.h"

#include "model/Scale.h"

#include "os/os.h"

#include "core/utils/StringBuilder.h"

enum class ContextAction {
    Init,
    Copy,
    Paste,
    Duplicate,
    Generate,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INIT" },
    { "COPY" },
    { "PASTE" },
    { "DUPL" },
    { "GEN" },
};

enum class Function {
    Gate        = 0,
    Retrigger   = 1,
    Length      = 2,
    Note        = 3,
    Condition     = 4,
};

static const char *functionNames[] = { "GATE", "RETRIG", "LENGTH", "NOTE", "COND" };

static const NoteSequenceListModel::Item quickEditItems[8] = {
    NoteSequenceListModel::Item::FirstStep,
    NoteSequenceListModel::Item::LastStep,
    NoteSequenceListModel::Item::RunMode,
    NoteSequenceListModel::Item::Divisor,
    NoteSequenceListModel::Item::ResetMeasure,
    NoteSequenceListModel::Item::Scale,
    NoteSequenceListModel::Item::RootNote,
    NoteSequenceListModel::Item::Last
};

NoteSequenceEditPage::NoteSequenceEditPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{
    _stepSelection.setStepCompare([this] (int a, int b) {
        auto layer = _project.selectedNoteSequenceLayer();
        const auto &sequence = _project.selectedNoteSequence();
        return sequence.step(a).layerValue(layer) == sequence.step(b).layerValue(layer);
    });
}

void NoteSequenceEditPage::enter() {
    updateMonitorStep();

    _showDetail = false;
}

void NoteSequenceEditPage::exit() {
    _engine.selectedTrackEngine().as<NoteTrackEngine>().setMonitorStep(-1);
}

void NoteSequenceEditPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "STEPS");
    WindowPainter::drawActiveFunction(canvas, NoteSequence::layerName(layer()));
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), activeFunctionKey());

    const auto &trackEngine = _engine.selectedTrackEngine().as<NoteTrackEngine>();
    const auto &sequence = _project.selectedNoteSequence();
    const auto &scale = sequence.selectedScale(_project.scale());
    int currentStep = trackEngine.isActiveSequence(sequence) ? trackEngine.currentStep() : -1;
    int currentRecordStep = trackEngine.isActiveSequence(sequence) ? trackEngine.currentRecordStep() : -1;

    const int stepWidth = Width / StepCount;
    const int stepOffset = this->stepOffset();

    const int loopY = 16;

    // draw loop points
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(0xf);
    SequencePainter::drawLoopStart(canvas, (sequence.firstStep() - stepOffset) * stepWidth + 1, loopY, stepWidth - 2);
    SequencePainter::drawLoopEnd(canvas, (sequence.lastStep() - stepOffset) * stepWidth + 1, loopY, stepWidth - 2);

    for (int i = 0; i < StepCount; ++i) {
        int stepIndex = stepOffset + i;
        const auto &step = sequence.step(stepIndex);

        int x = i * stepWidth;
        int y = 20;

        // loop
        if (stepIndex > sequence.firstStep() && stepIndex <= sequence.lastStep()) {
            canvas.setColor(0xf);
            canvas.point(x, loopY);
        }

#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
        int stepColor = BasePage::UI_COLOR_ACTIVE;

        if ((stepIndex < sequence.firstStep() || stepIndex > sequence.lastStep()) && stepIndex != currentStep) {
            stepColor = UiColor::UI_COLOR_INACTIVE;
        } else if (_stepSelection[stepIndex] || stepIndex != currentStep) {
            if (stepIndex % 4 == 0) {
                stepColor = UiColor::UI_COLOR_NORMAL;
            } else {
                stepColor = UiColor::UI_COLOR_DIM;
            }
        }
#endif
        // step index
        {
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
            canvas.setColor(stepColor);
#else
            canvas.setColor(stepIndex == currentStep ? 0xf : 0x7);
#endif
            FixedStringBuilder<8> str("%d", stepIndex + 1);
            canvas.drawText(x + (stepWidth - canvas.textWidth(str) + 1) / 2, y - 2, str);
        }

        // step gate
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
        canvas.setColor(stepColor);
#else
        canvas.setColor(stepIndex == currentStep ? 0xf : 0x7);
#endif
        canvas.drawRect(x + 2, y + 2, stepWidth - 4, stepWidth - 4);
        if (step.gate()) {
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
            // Dim filled gates outside the active loop range.
            canvas.setColor(stepIndex >= sequence.firstStep() && stepIndex <= sequence.lastStep() ? BasePage::UI_COLOR_ACTIVE : BasePage::UI_COLOR_INACTIVE);
#else
            canvas.setColor(0xf);
#endif
            canvas.fillRect(x + 4, y + 4, stepWidth - 8, stepWidth - 8);
        }

        // record step
        if (stepIndex == currentRecordStep) {
            // draw circle
            canvas.setColor(step.gate() ? 0x0 : 0xf);
            canvas.fillRect(x + 6, y + 6, stepWidth - 12, stepWidth - 12);
            canvas.setColor(0x7);
            canvas.hline(x + 7, y + 5, 2);
            canvas.hline(x + 7, y + 10, 2);
            canvas.vline(x + 5, y + 7, 2);
            canvas.vline(x + 10, y + 7, 2);
        }

        int baseColor = (stepIndex >= sequence.firstStep() && stepIndex <= sequence.lastStep()) ? BasePage::UI_COLOR_NORMAL : BasePage::UI_COLOR_INACTIVE;;

        switch (layer()) {
        case Layer::Gate:
            break;
        case Layer::GateProbability:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
            SequencePainter::drawProbability(
                canvas,
                baseColor,
                x + 2, y + 18, stepWidth - 4, 2,
                step.gateProbability() + 1, NoteSequence::GateProbability::Range
            );
#else
            SequencePainter::drawProbability(
                canvas,
                x + 2, y + 18, stepWidth - 4, 2,
                step.gateProbability() + 1, NoteSequence::GateProbability::Range
            );
#endif
            break;
        case Layer::GateOffset:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
            SequencePainter::drawOffset(
                canvas,
                baseColor,
                x + 2, y + 18, stepWidth - 4, 2,
                step.gateOffset(), NoteSequence::GateOffset::Min - 1, NoteSequence::GateOffset::Max + 1
            );
#else
            SequencePainter::drawOffset(
                canvas,
                x + 2, y + 18, stepWidth - 4, 2,
                step.gateOffset(), NoteSequence::GateOffset::Min - 1, NoteSequence::GateOffset::Max + 1
            );
#endif
            break;
        case Layer::Retrigger:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
            SequencePainter::drawRetrigger(
                canvas,
                baseColor,
                x, y + 18, stepWidth, 2,
                step.retrigger() + 1, NoteSequence::Retrigger::Range
            );
#else
            SequencePainter::drawRetrigger(
                canvas,
                x, y + 18, stepWidth, 2,
                step.retrigger() + 1, NoteSequence::Retrigger::Range
            );
#endif;
            break;
        case Layer::RetriggerProbability:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
            SequencePainter::drawProbability(
                canvas,
                baseColor,
                x + 2, y + 18, stepWidth - 4, 2,
                step.retriggerProbability() + 1, NoteSequence::RetriggerProbability::Range
            );
#else
            SequencePainter::drawProbability(
                canvas,
                x + 2, y + 18, stepWidth - 4, 2,
                step.retriggerProbability() + 1, NoteSequence::RetriggerProbability::Range
            );
#endif
            break;
        case Layer::Length:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
            SequencePainter::drawLength(
                canvas,
                baseColor,
                x + 2, y + 18, stepWidth - 4, 6,
                step.length() + 1, NoteSequence::Length::Range
            );
#else
            SequencePainter::drawLength(
                canvas,
                x + 2, y + 18, stepWidth - 4, 6,
                step.length() + 1, NoteSequence::Length::Range
            );
#endif
            break;
        case Layer::LengthVariationRange:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
            SequencePainter::drawLengthRange(
                canvas,
                baseColor,
                x + 2, y + 18, stepWidth - 4, 6,
                step.length() + 1, step.lengthVariationRange(), NoteSequence::Length::Range
            );
#else
            SequencePainter::drawLengthRange(
                canvas,
                x + 2, y + 18, stepWidth - 4, 6,
                step.length() + 1, step.lengthVariationRange(), NoteSequence::Length::Range
            );
#endif
            break;
        case Layer::LengthVariationProbability:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
            SequencePainter::drawProbability(
                canvas,
                baseColor,
                x + 2, y + 18, stepWidth - 4, 2,
                step.lengthVariationProbability() + 1, NoteSequence::LengthVariationProbability::Range
            );
#else
            SequencePainter::drawProbability(
                canvas,
                x + 2, y + 18, stepWidth - 4, 2,
                step.lengthVariationProbability() + 1, NoteSequence::LengthVariationProbability::Range
            );
#endif
            break;
        case Layer::Note: {
            int rootNote = sequence.selectedRootNote(_model.project().rootNote());
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
            canvas.setColor(baseColor);
#else
            canvas.setColor(0xf);
#endif
            FixedStringBuilder<8> str;
            scale.noteName(str, step.note(), rootNote, Scale::Short1);
            canvas.drawText(x + (stepWidth - canvas.textWidth(str) + 1) / 2, y + 20, str);
            str.reset();
            scale.noteName(str, step.note(), rootNote, Scale::Short2);
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
            int octaveColor = baseColor;
            int octave = roundDownDivide(step.note(), scale.notesPerOctave());
            if (octave == 0) {
                octaveColor = UiColor::UI_COLOR_INACTIVE;
            } else {
                octaveColor = UiColor::UI_COLOR_DIM;
            }
            canvas.setColor(octaveColor);
#endif
            canvas.drawText(x + (stepWidth - canvas.textWidth(str) + 1) / 2, y + 27, str);
            break;
        }
        case Layer::NoteVariationRange: {
            canvas.setColor(0xf);
            FixedStringBuilder<8> str("%d", step.noteVariationRange());
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
            canvas.setColor(baseColor);
#endif
            canvas.drawText(x + (stepWidth - canvas.textWidth(str) + 1) / 2, y + 20, str);
            break;
        }
        case Layer::NoteVariationProbability:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
            SequencePainter::drawProbability(
                canvas,
                baseColor,
                x + 2, y + 18, stepWidth - 4, 2,
                step.noteVariationProbability() + 1, NoteSequence::NoteVariationProbability::Range
            );
#else
            SequencePainter::drawProbability(
                canvas,
                x + 2, y + 18, stepWidth - 4, 2,
                step.noteVariationProbability() + 1, NoteSequence::NoteVariationProbability::Range
            );
#endif
            break;
        case Layer::Slide:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
            SequencePainter::drawSlide(
                canvas,
                baseColor,
                x + 4, y + 18, stepWidth - 8, 4,
                step.slide()
            );
#else
            SequencePainter::drawSlide(
                canvas,
                x + 4, y + 18, stepWidth - 8, 4,
                step.slide()
            );
#endif
            break;
        case Layer::Condition: {
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
            canvas.setColor(baseColor);
#else
            canvas.setColor(0xf);
#endif
            FixedStringBuilder<8> str;
            Types::printCondition(str, step.condition(), Types::ConditionFormat::Short1);
            canvas.drawText(x + (stepWidth - canvas.textWidth(str) + 1) / 2, y + 20, str);
            str.reset();
            Types::printCondition(str, step.condition(), Types::ConditionFormat::Short2);
            canvas.drawText(x + (stepWidth - canvas.textWidth(str) + 1) / 2, y + 27, str);
            break;
        }
        case Layer::Last:
            break;
        }
    }

    // handle detail display

    if (_showDetail) {
        if (layer() == Layer::Gate || layer() == Layer::Slide || _stepSelection.none()) {
            _showDetail = false;
        }
        if (_stepSelection.isPersisted() && os::ticks() > _showDetailTicks + os::time::ms(500)) {
            _showDetail = false;
        }
    }

    if (_showDetail) {
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
        drawDetail(canvas, sequence.step(_stepSelection.first()), _stepSelection.first());
#else
        drawDetail(canvas, sequence.step(_stepSelection.first()));
#endif
    }
}

void NoteSequenceEditPage::updateLeds(Leds &leds) {
    const auto &trackEngine = _engine.selectedTrackEngine().as<NoteTrackEngine>();
    const auto &sequence = _project.selectedNoteSequence();
    int currentStep = trackEngine.isActiveSequence(sequence) ? trackEngine.currentStep() : -1;

    for (int i = 0; i < 16; ++i) {
        int stepIndex = stepOffset() + i;
        bool red = (stepIndex == currentStep) || _stepSelection[stepIndex];
        bool green = (stepIndex != currentStep) && (sequence.step(stepIndex).gate() || _stepSelection[stepIndex]);
        leds.set(MatrixMap::fromStep(i), red, green);
    }

    LedPainter::drawSelectedSequenceSection(leds, _section);

    // show quick edit keys
    if (globalKeyState()[Key::Page] && !globalKeyState()[Key::Shift]) {
        for (int i = 0; i < 8; ++i) {
            int index = MatrixMap::fromStep(i + 8);
            leds.unmask(index);
            leds.set(index, false, quickEditItems[i] != NoteSequenceListModel::Item::Last);
            leds.mask(index);
        }
    }
}

void NoteSequenceEditPage::keyDown(KeyEvent &event) {
    _stepSelection.keyDown(event, stepOffset());
    updateMonitorStep();

#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
    /*
    // Quick edit of first step and last step
    const auto &key = event.key();
    if (globalKeyState()[Key::Page] && !globalKeyState()[Key::Shift] && globalKeyState()[Key::Encoder]) {
        quickEdit(NoteSequenceListModel::Item::FirstStep); // First Step
        event.consume();
        return;
    }

    if (globalKeyState()[Key::Shift] && !globalKeyState()[Key::Page] && globalKeyState()[Key::Encoder]) {
        quickEdit(NoteSequenceListModel::Item::LastStep); // Last Step
        event.consume();
        return;
    }
*/
#endif
}

void NoteSequenceEditPage::keyUp(KeyEvent &event) {
    _stepSelection.keyUp(event, stepOffset());
    updateMonitorStep();
}

void NoteSequenceEditPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();
    auto &sequence = _project.selectedNoteSequence();

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    if (key.isQuickEdit()) {
        quickEdit(key.quickEdit());
        event.consume();
        return;
    }

    if (key.pageModifier()) {
        return;
    }

    _stepSelection.keyPress(event, stepOffset());
    updateMonitorStep();

    if (!key.shiftModifier() && key.isStep()) {
        int stepIndex = stepOffset() + key.step();
        switch (layer()) {
        case Layer::Gate:
            sequence.step(stepIndex).toggleGate();
            event.consume();
            break;
        default:
            break;
        }
    }

    if (key.isFunction()) {
        switchLayer(key.function(), key.shiftModifier());
        event.consume();
    }

    if (key.isEncoder()) {
        if (!_showDetail && _stepSelection.any() && allSelectedStepsActive()) {
            setSelectedStepsGate(false);
        } else {
            setSelectedStepsGate(true);
        }
    }

    if (key.isLeft()) {
        if (key.shiftModifier()) {
            sequence.shiftSteps(_stepSelection.selected(), -1);
        } else {
            _section = std::max(0, _section - 1);
        }
        event.consume();
    }
    if (key.isRight()) {
        if (key.shiftModifier()) {
            sequence.shiftSteps(_stepSelection.selected(), 1);
        } else {
            _section = std::min(3, _section + 1);
        }
        event.consume();
    }
}

void NoteSequenceEditPage::encoder(EncoderEvent &event) {
    auto &sequence = _project.selectedNoteSequence();
    const auto &scale = sequence.selectedScale(_project.scale());

    if (_stepSelection.any()) {
        _showDetail = true;
        _showDetailTicks = os::ticks();
    } else {
        return;
    }

    for (size_t stepIndex = 0; stepIndex < sequence.steps().size(); ++stepIndex) {
        if (_stepSelection[stepIndex]) {
            auto &step = sequence.step(stepIndex);
            bool shift = globalKeyState()[Key::Shift];
            switch (layer()) {
            case Layer::Gate:
                step.setGate(event.value() > 0);
                break;
            case Layer::GateProbability:
                step.setGateProbability(step.gateProbability() + event.value());
                break;
            case Layer::GateOffset:
                step.setGateOffset(step.gateOffset() + event.value());
                break;
            case Layer::Retrigger:
                step.setRetrigger(step.retrigger() + event.value());
                break;
            case Layer::RetriggerProbability:
                step.setRetriggerProbability(step.retriggerProbability() + event.value());
                break;
            case Layer::Length:
                step.setLength(step.length() + event.value());
                break;
            case Layer::LengthVariationRange:
                step.setLengthVariationRange(step.lengthVariationRange() + event.value());
                break;
            case Layer::LengthVariationProbability:
                step.setLengthVariationProbability(step.lengthVariationProbability() + event.value());
                break;
            case Layer::Note:
                step.setNote(step.note() + event.value() * ((shift && scale.isChromatic()) ? scale.notesPerOctave() : 1));
                updateMonitorStep();
                break;
            case Layer::NoteVariationRange:
                step.setNoteVariationRange(step.noteVariationRange() + event.value() * ((shift && scale.isChromatic()) ? scale.notesPerOctave() : 1));
                updateMonitorStep();
                break;
            case Layer::NoteVariationProbability:
                step.setNoteVariationProbability(step.noteVariationProbability() + event.value());
                break;
            case Layer::Slide:
                step.setSlide(event.value() > 0);
                break;
            case Layer::Condition:
                step.setCondition(ModelUtils::adjustedEnum(step.condition(), event.value()));
                break;
            case Layer::Last:
                break;
            }
        }
    }

    event.consume();
}

void NoteSequenceEditPage::midi(MidiEvent &event) {
    if (!_engine.recording() && layer() == Layer::Note && _stepSelection.any()) {
        auto &trackEngine = _engine.selectedTrackEngine().as<NoteTrackEngine>();
        auto &sequence = _project.selectedNoteSequence();
        const auto &scale = sequence.selectedScale(_project.scale());
        const auto &message = event.message();

        if (message.isNoteOn()) {
            float volts = (message.note() - 60) * (1.f / 12.f);
            int note = scale.noteFromVolts(volts);

            for (size_t stepIndex = 0; stepIndex < sequence.steps().size(); ++stepIndex) {
                if (_stepSelection[stepIndex]) {
                    auto &step = sequence.step(stepIndex);
                    step.setNote(note);
                    step.setGate(true);
                }
            }

            trackEngine.setMonitorStep(_stepSelection.first());
            updateMonitorStep();
        }
    }
}

void NoteSequenceEditPage::switchLayer(int functionKey, bool shift) {
    if (shift) {
        switch (Function(functionKey)) {
        case Function::Gate:
            setLayer(Layer::Gate);
            break;
        case Function::Retrigger:
            setLayer(Layer::Retrigger);
            break;
        case Function::Length:
            setLayer(Layer::Length);
            break;
        case Function::Note:
            setLayer(Layer::Note);
            break;
        case Function::Condition:
            setLayer(Layer::Condition);
            break;
        }
        return;
    }

    switch (Function(functionKey)) {
    case Function::Gate:
        switch (layer()) {
        case Layer::Gate:
            setLayer(Layer::GateProbability);
            break;
        case Layer::GateProbability:
            setLayer(Layer::GateOffset);
            break;
        case Layer::GateOffset:
            setLayer(Layer::Slide);
            break;
        default:
            setLayer(Layer::Gate);
            break;
        }
        break;
    case Function::Retrigger:
        switch (layer()) {
        case Layer::Retrigger:
            setLayer(Layer::RetriggerProbability);
            break;
        default:
            setLayer(Layer::Retrigger);
            break;
        }
        break;
    case Function::Length:
        switch (layer()) {
        case Layer::Length:
            setLayer(Layer::LengthVariationRange);
            break;
        case Layer::LengthVariationRange:
            setLayer(Layer::LengthVariationProbability);
            break;
        default:
            setLayer(Layer::Length);
            break;
        }
        break;
    case Function::Note:
        switch (layer()) {
        case Layer::Note:
            setLayer(Layer::NoteVariationRange);
            break;
        case Layer::NoteVariationRange:
            setLayer(Layer::NoteVariationProbability);
            break;
        default:
            setLayer(Layer::Note);
            break;
        }
        break;
    case Function::Condition:
        setLayer(Layer::Condition);
        break;
    }
}

int NoteSequenceEditPage::activeFunctionKey() {
    switch (layer()) {
    case Layer::Gate:
    case Layer::GateProbability:
    case Layer::GateOffset:
    case Layer::Slide:
        return 0;
    case Layer::Retrigger:
    case Layer::RetriggerProbability:
        return 1;
    case Layer::Length:
    case Layer::LengthVariationRange:
    case Layer::LengthVariationProbability:
        return 2;
    case Layer::Note:
    case Layer::NoteVariationRange:
    case Layer::NoteVariationProbability:
        return 3;
    case Layer::Condition:
        return 4;
    case Layer::Last:
        break;
    }

    return -1;
}

void NoteSequenceEditPage::updateMonitorStep() {
    auto &trackEngine = _engine.selectedTrackEngine().as<NoteTrackEngine>();

    // TODO should we monitor an all layers not just note?
    if (layer() == Layer::Note && !_stepSelection.isPersisted() && _stepSelection.any()) {
        trackEngine.setMonitorStep(_stepSelection.first());
    } else {
        trackEngine.setMonitorStep(-1);
    }
}
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
void NoteSequenceEditPage::drawDetail(Canvas &canvas, const NoteSequence::Step &step, int stepIndex) {
#else
void NoteSequenceEditPage::drawDetail(Canvas &canvas, const NoteSequence::Step &step) {
#endif

    const auto &sequence = _project.selectedNoteSequence();
    const auto &scale = sequence.selectedScale(_project.scale());

    FixedStringBuilder<16> str;

    WindowPainter::drawFrame(canvas, 64, 16, 128, 32);

    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(0xf);
    canvas.vline(64 + 32, 16, 32);

    canvas.setFont(Font::Small);
    str("%d", _stepSelection.first() + 1);
    if (_stepSelection.count() > 1) {
        str("*");
    }
    canvas.drawTextCentered(64, 16, 32, 32, str);

    canvas.setFont(Font::Tiny);

    int baseColor = (stepIndex >= sequence.firstStep() && stepIndex <= sequence.lastStep()) ? BasePage::UI_COLOR_ACTIVE : BasePage::UI_COLOR_INACTIVE;

    switch (layer()) {
    case Layer::Gate:
    case Layer::Slide:
        break;
    case Layer::GateProbability:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
        SequencePainter::drawProbability(
            canvas,
            baseColor,
            64 + 32 + 8, 32 - 4, 64 - 16, 8,
            step.gateProbability() + 1, NoteSequence::GateProbability::Range
        );
        str.reset();
        str("%.1f%%", 100.f * (step.gateProbability() + 1.f) / NoteSequence::GateProbability::Range);
        canvas.setColor(baseColor);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
#else
        SequencePainter::drawProbability(
            canvas,
            64 + 32 + 8, 32 - 4, 64 - 16, 8,
            step.gateProbability() + 1, NoteSequence::GateProbability::Range
        );
        str.reset();
        str("%.1f%%", 100.f * (step.gateProbability() + 1.f) / NoteSequence::GateProbability::Range);
        canvas.setColor(0xf);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
#endif;
        break;
    case Layer::GateOffset:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
        SequencePainter::drawOffset(
            canvas,
            baseColor,
            64 + 32 + 8, 32 - 4, 64 - 16, 8,
            step.gateOffset(), NoteSequence::GateOffset::Min - 1, NoteSequence::GateOffset::Max + 1
        );
        str.reset();
        str("%.1f%%", 100.f * step.gateOffset() / float(NoteSequence::GateOffset::Max + 1));
        canvas.setColor(baseColor);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
#else
        SequencePainter::drawOffset(
            canvas,
            64 + 32 + 8, 32 - 4, 64 - 16, 8,
            step.gateOffset(), NoteSequence::GateOffset::Min - 1, NoteSequence::GateOffset::Max + 1
        );
        str.reset();
        str("%.1f%%", 100.f * step.gateOffset() / float(NoteSequence::GateOffset::Max + 1));
        canvas.setColor(0xf);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
#endif
        break;
    case Layer::Retrigger:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
        SequencePainter::drawRetrigger(
            canvas,
            baseColor,
            64+ 32 + 8, 32 - 4, 64 - 16, 8,
            step.retrigger() + 1, NoteSequence::Retrigger::Range
        );
        str.reset();
        str("%d", step.retrigger() + 1);
        canvas.setColor(baseColor);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
#else
        SequencePainter::drawRetrigger(
            canvas,
            64+ 32 + 8, 32 - 4, 64 - 16, 8,
            step.retrigger() + 1, NoteSequence::Retrigger::Range
        );
        str.reset();
        str("%d", step.retrigger() + 1);
        canvas.setColor(0xf);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
#endif
        break;
    case Layer::RetriggerProbability:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
        SequencePainter::drawProbability(
            canvas,
            baseColor,
            64 + 32 + 8, 32 - 4, 64 - 16, 8,
            step.retriggerProbability() + 1, NoteSequence::RetriggerProbability::Range
        );
        str.reset();
        str("%.1f%%", 100.f * (step.retriggerProbability() + 1.f) / NoteSequence::RetriggerProbability::Range);
        canvas.setColor(baseColor);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
#else
        SequencePainter::drawProbability(
            canvas,
            64 + 32 + 8, 32 - 4, 64 - 16, 8,
            step.retriggerProbability() + 1, NoteSequence::RetriggerProbability::Range
        );
        str.reset();
        str("%.1f%%", 100.f * (step.retriggerProbability() + 1.f) / NoteSequence::RetriggerProbability::Range);
        canvas.setColor(0xf);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
#endif
        break;
    case Layer::Length:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
        SequencePainter::drawLength(
            canvas,
            baseColor,
            64 + 32 + 8, 32 - 4, 64 - 16, 8,
            step.length() + 1, NoteSequence::Length::Range
        );
        str.reset();
        str("%.1f%%", 100.f * (step.length() + 1.f) / NoteSequence::Length::Range);
        canvas.setColor(baseColor);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
#else
        SequencePainter::drawLength(
            canvas,
            64 + 32 + 8, 32 - 4, 64 - 16, 8,
            step.length() + 1, NoteSequence::Length::Range
        );
        str.reset();
        str("%.1f%%", 100.f * (step.length() + 1.f) / NoteSequence::Length::Range);
        canvas.setColor(0xf);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
#endif
        break;
    case Layer::LengthVariationRange:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
        SequencePainter::drawLengthRange(
            canvas,
            baseColor,
            64 + 32 + 8, 32 - 4, 64 - 16, 8,
            step.length() + 1, step.lengthVariationRange(), NoteSequence::Length::Range
        );
        str.reset();
        str("%.1f%%", 100.f * (step.lengthVariationRange()) / NoteSequence::Length::Range);
        canvas.setColor(baseColor);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
#else
        SequencePainter::drawLengthRange(
            canvas,
            64 + 32 + 8, 32 - 4, 64 - 16, 8,
            step.length() + 1, step.lengthVariationRange(), NoteSequence::Length::Range
        );
        str.reset();
        str("%.1f%%", 100.f * (step.lengthVariationRange()) / NoteSequence::Length::Range);
        canvas.setColor(0xf);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
#endif
        break;
    case Layer::LengthVariationProbability:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
        SequencePainter::drawProbability(
            canvas,
            baseColor,
            64 + 32 + 8, 32 - 4, 64 - 16, 8,
            step.lengthVariationProbability() + 1, NoteSequence::LengthVariationProbability::Range
        );
        str.reset();
        str("%.1f%%", 100.f * (step.lengthVariationProbability() + 1.f) / NoteSequence::LengthVariationProbability::Range);
        canvas.setColor(baseColor);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
#else
        SequencePainter::drawProbability(
            canvas,
            64 + 32 + 8, 32 - 4, 64 - 16, 8,
            step.lengthVariationProbability() + 1, NoteSequence::LengthVariationProbability::Range
        );
        str.reset();
        str("%.1f%%", 100.f * (step.lengthVariationProbability() + 1.f) / NoteSequence::LengthVariationProbability::Range);
        canvas.setColor(0xf);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
#endif
        break;
    case Layer::Note:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
        canvas.setColor(baseColor);
#endif
        str.reset();
        scale.noteName(str, step.note(), sequence.selectedRootNote(_model.project().rootNote()), Scale::Long);
        canvas.setFont(Font::Small);
        canvas.drawTextCentered(64 + 32, 16, 64, 32, str);
        break;
    case Layer::NoteVariationRange:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
        canvas.setColor(baseColor);
#endif
        str.reset();
        str("%d", step.noteVariationRange());
        canvas.setFont(Font::Small);
        canvas.drawTextCentered(64 + 32, 16, 64, 32, str);
        break;
    case Layer::NoteVariationProbability:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
        SequencePainter::drawProbability(
            canvas,
            baseColor,
            64 + 32 + 8, 32 - 4, 64 - 16, 8,
            step.noteVariationProbability() + 1, NoteSequence::NoteVariationProbability::Range
        );
        str.reset();
        str("%.1f%%", 100.f * (step.noteVariationProbability() + 1.f) / NoteSequence::NoteVariationProbability::Range);
        canvas.setColor(baseColor);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
#else
        SequencePainter::drawProbability(
            canvas,
            64 + 32 + 8, 32 - 4, 64 - 16, 8,
            step.noteVariationProbability() + 1, NoteSequence::NoteVariationProbability::Range
        );
        str.reset();
        str("%.1f%%", 100.f * (step.noteVariationProbability() + 1.f) / NoteSequence::NoteVariationProbability::Range);
        canvas.setColor(0xf);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
#endif
        break;
    case Layer::Condition:
#ifdef CONFIG_ENABLE_NOTE_EDIT_ENHANCEMENTS
        canvas.setColor(baseColor);
#endif;
        str.reset();
        Types::printCondition(str, step.condition(), Types::ConditionFormat::Long);
        canvas.setFont(Font::Small);
        canvas.drawTextCentered(64 + 32, 16, 96, 32, str);
        break;
    case Layer::Last:
        break;
    }
}

void NoteSequenceEditPage::contextShow() {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); }
    ));
}

void NoteSequenceEditPage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Init:
        initSequence();
        break;
    case ContextAction::Copy:
        copySequence();
        break;
    case ContextAction::Paste:
        pasteSequence();
        break;
    case ContextAction::Duplicate:
        duplicateSequence();
        break;
    case ContextAction::Generate:
        generateSequence();
        break;
    case ContextAction::Last:
        break;
    }
}

bool NoteSequenceEditPage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    case ContextAction::Paste:
        return _model.clipBoard().canPasteNoteSequenceSteps();
    default:
        return true;
    }
}

void NoteSequenceEditPage::initSequence() {
    _project.selectedNoteSequence().clearSteps();
    showMessage("STEPS INITIALIZED");
}

void NoteSequenceEditPage::copySequence() {
    _model.clipBoard().copyNoteSequenceSteps(_project.selectedNoteSequence(), _stepSelection.selected());
    showMessage("STEPS COPIED");
}

void NoteSequenceEditPage::pasteSequence() {
    _model.clipBoard().pasteNoteSequenceSteps(_project.selectedNoteSequence(), _stepSelection.selected());
    showMessage("STEPS PASTED");
}

void NoteSequenceEditPage::duplicateSequence() {
    _project.selectedNoteSequence().duplicateSteps();
    showMessage("STEPS DUPLICATED");
}

void NoteSequenceEditPage::generateSequence() {
    _manager.pages().generatorSelect.show([this] (bool success, Generator::Mode mode) {
        if (success) {
            auto builder = _builderContainer.create<NoteSequenceBuilder>(_project.selectedNoteSequence(), layer());
            auto generator = Generator::execute(mode, *builder);
            if (generator) {
                _manager.pages().generator.show(generator);
            }
        }
    });
}

void NoteSequenceEditPage::quickEdit(int index) {
    _listModel.setSequence(&_project.selectedNoteSequence());
    if (quickEditItems[index] != NoteSequenceListModel::Item::Last) {
        _manager.pages().quickEdit.show(_listModel, int(quickEditItems[index]));
    }
}

bool NoteSequenceEditPage::allSelectedStepsActive() const {
    const auto &sequence = _project.selectedNoteSequence();
    for (size_t stepIndex = 0; stepIndex < _stepSelection.size(); ++stepIndex) {
        if (_stepSelection[stepIndex] && !sequence.step(stepIndex).gate()) {
            return false;
        }
    }
    return true;
}

void NoteSequenceEditPage::setSelectedStepsGate(bool gate) {
    auto &sequence = _project.selectedNoteSequence();
    for (size_t stepIndex = 0; stepIndex < _stepSelection.size(); ++stepIndex) {
        if (_stepSelection[stepIndex]) {
            sequence.step(stepIndex).setGate(gate);
        }
    }
}
