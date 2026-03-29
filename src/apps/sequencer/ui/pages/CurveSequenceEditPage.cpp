#include "Config.h"
#include "CurveSequenceEditPage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/SequencePainter.h"
#include "ui/painters/WindowPainter.h"

#include "model/Curve.h"

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
    { TXT_MENU_INIT },
    { TXT_MENU_COPY },
    { TXT_MENU_PASTE },
    { TXT_MENU_DUPLICATE },
    { TXT_MENU_GENERATORS },
};

enum class Function {
    Shape   = 0,
    Min     = 1,
    Max     = 2,
    Gate    = 3,
};

static const char *functionNames[] = {
	TXT_MENU_SHAPE,
	TXT_MENU_MINIMUM,
	TXT_MENU_MAXIMUM,
	TXT_MENU_GATES,
	nullptr
};

static const CurveSequenceListModel::Item quickEditItems[8] = {
    CurveSequenceListModel::Item::FirstStep,
    CurveSequenceListModel::Item::LastStep,
    CurveSequenceListModel::Item::RunMode,
    CurveSequenceListModel::Item::Divisor,
    CurveSequenceListModel::Item::ResetMeasure,
    CurveSequenceListModel::Item::Range,
    CurveSequenceListModel::Item::Last,
    CurveSequenceListModel::Item::Last
};

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

static void drawMinMax(Canvas &canvas, int x, int y, int w, int h, float minMax) {
    y += std::round((1.f - minMax) * h);
    canvas.hline(x, y, w);
}

static void drawGatePattern(Canvas &canvas, int x, int y, int w, int h, int gate) {
    int gs = w / 4;
    int gw = w / 8;
    for (int i = 0; i < 4; ++i) {
        canvas.setColor((gate & (1 << i)) ? UI_COLOR_ACTIVE : UI_COLOR_DIM);
        canvas.fillRect(x + i * gs, y, gw, h);
    }
}

CurveSequenceEditPage::CurveSequenceEditPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{
    _stepSelection.setStepCompare([this] (int a, int b) {
        auto layer = _project.selectedCurveSequenceLayer();
        const auto &sequence = _project.selectedCurveSequence();
        return sequence.step(a).layerValue(layer) == sequence.step(b).layerValue(layer);
    });
}

void CurveSequenceEditPage::enter() {
    updateMonitorStep();

    _showDetail = false;
}

void CurveSequenceEditPage::exit() {
}

void CurveSequenceEditPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, TXT_MODE_STEPS);
    WindowPainter::drawActiveFunction(canvas, CurveSequence::layerName(layer()));
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), activeFunctionKey());

    const auto &trackEngine = _engine.selectedTrackEngine().as<CurveTrackEngine>();
    const auto &sequence = _project.selectedCurveSequence();
    bool isActiveSequence = trackEngine.isActiveSequence(sequence);

    canvas.setBlendMode(BlendMode::Add);

    const int stepWidth = Width / StepCount;
    const int stepOffset = this->stepOffset();

    const int loopY = 16;
    const int curveY = 24;
    const int curveHeight = 20;
    const int bottomY = 48;

    bool drawShapeVariation = layer() == Layer::ShapeVariation || layer() == Layer::ShapeVariationProbability;

    // draw loop points
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(UI_COLOR_ACTIVE);
    SequencePainter::drawLoopStart(canvas, (sequence.firstStep() - stepOffset) * stepWidth + 1, loopY, stepWidth - 2);
    SequencePainter::drawLoopEnd(canvas, (sequence.lastStep() - stepOffset) * stepWidth + 1, loopY, stepWidth - 2);

    // draw grid
    if (!drawShapeVariation) {
        canvas.setColor(UI_COLOR_DIM_MORE);
        for (int stepIndex = 1; stepIndex < StepCount; ++stepIndex) {
            int x = stepIndex * stepWidth;
            for (int y = 0; y <= curveHeight; y += 2) {
                canvas.point(x, curveY + y);
            }
        }
    }

    // draw curve
    canvas.setColor(UI_COLOR_ACTIVE);
    float lastY = -1.f;
    float lastYVariation = -1.f;
    for (int i = 0; i < StepCount; ++i) {
        int stepIndex = stepOffset + i;
        const auto &step = sequence.step(stepIndex);
        float min = step.minNormalized();
        float max = step.maxNormalized();

        int x = i * stepWidth;
        int y = 20;

        canvas.setBlendMode(BlendMode::Set);

        // loop
        if (stepIndex > sequence.firstStep() && stepIndex <= sequence.lastStep()) {
            canvas.setColor(UI_COLOR_ACTIVE);
            canvas.point(x, loopY);
        }

        int stepColor = UI_COLOR_ACTIVE;

        if ((stepIndex < sequence.firstStep() || stepIndex > sequence.lastStep()) && stepIndex != trackEngine.currentStep()) {
            stepColor = UI_COLOR_INACTIVE;
        } else if (_stepSelection[stepIndex]) {
            stepColor = UI_COLOR_DIM;
        } else if (stepIndex != trackEngine.currentStep()) {
            stepColor = UI_COLOR_DIM;
        }


        // step index
        {
            canvas.setColor(stepColor);
            FixedStringBuilder<8> str(TXT_INFO_STEP_INDEX, stepIndex + 1);
            canvas.drawText(x + (stepWidth - canvas.textWidth(str) + 1) / 2, y - 2, str);
        }


        if ((stepIndex < sequence.firstStep() || stepIndex > sequence.lastStep()) && stepIndex != trackEngine.currentStep()) {
            stepColor = UI_COLOR_INACTIVE;
        } else if (_stepSelection[stepIndex]) {
            stepColor = UI_COLOR_ACTIVE;
        } else if (stepIndex != trackEngine.currentStep()) {
            stepColor = UI_COLOR_NORMAL;
        }

        // curve
        {
            const auto function = Curve::function(Curve::Type(std::min(Curve::Last - 1, step.shape())));

            canvas.setColor(stepColor);
            canvas.setBlendMode(BlendMode::Add);

            drawCurve(canvas, x, curveY, stepWidth, curveHeight, lastY, function, min, max);
        }

        if (drawShapeVariation) {
            const auto function = Curve::function(Curve::Type(std::min(Curve::Last - 1, step.shapeVariation())));

            canvas.setColor(UI_COLOR_DIM_MORE);
            canvas.setBlendMode(BlendMode::Add);

            drawCurve(canvas, x, curveY, stepWidth, curveHeight, lastYVariation, function, min, max);
        }
        switch (layer()) {
        case Layer::Shape:
            break;
        case Layer::ShapeVariation:
            break;
        case Layer::ShapeVariationProbability:
            SequencePainter::drawProbability(
                canvas,
                stepColor,
                x + 2, bottomY, stepWidth - 4, 2,
                step.shapeVariationProbability(), 8
            );
            break;
        case Layer::Min:
        case Layer::Max: {
            bool functionPressed = globalKeyState()[MatrixMap::fromFunction(activeFunctionKey())];
            canvas.setColor(UI_COLOR_DIM_MORE);
            canvas.setBlendMode(BlendMode::Add);
            if (layer() == Layer::Min || functionPressed) {
                drawMinMax(canvas, x, curveY, stepWidth, curveHeight, min);
            }
            if (layer() == Layer::Max || functionPressed) {
                drawMinMax(canvas, x, curveY, stepWidth, curveHeight, max);
            }
            break;
        }
        case Layer::Gate:
            canvas.setColor(stepColor);
            canvas.setBlendMode(BlendMode::Set);
            drawGatePattern(canvas, x, bottomY, stepWidth, 2, step.gate());
            break;
        case Layer::GateProbability:
            SequencePainter::drawProbability(
                canvas,
                stepColor,
                x + 2, bottomY, stepWidth - 4, 2,
                step.gateProbability() + 1, CurveSequence::GateProbability::Range
            );
            break;
        case Layer::Last:
            break;
        }
    }

    // draw cursor
    if (isActiveSequence) {
        canvas.setColor(UI_COLOR_ACTIVE);
        int x = ((trackEngine.currentStep() - stepOffset) + trackEngine.currentStepFraction()) * stepWidth;
        canvas.vline(x, curveY, curveHeight);
    }

    // handle detail display

    if (_showDetail) {
        if (!(layer() == Layer::ShapeVariationProbability || layer() == Layer::GateProbability) || _stepSelection.none()) {
            _showDetail = false;
        }
        if (_stepSelection.isPersisted() && os::ticks() > _showDetailTicks + os::time::ms(500)) {
            _showDetail = false;
        }
    }

    if (_showDetail) {
        drawDetail(canvas, sequence.step(_stepSelection.first()), _stepSelection.first());
    }
}

void CurveSequenceEditPage::updateLeds(Leds &leds) {
    const auto &trackEngine = _engine.selectedTrackEngine().as<CurveTrackEngine>();
    const auto &sequence = _project.selectedCurveSequence();
    int currentStep = trackEngine.isActiveSequence(sequence) ? trackEngine.currentStep() : -1;

    for (int i = 0; i < 16; ++i) {
        int stepIndex = stepOffset() + i;
        bool red = (stepIndex == currentStep) || _stepSelection[stepIndex];
        bool green = (stepIndex != currentStep) && (sequence.step(stepIndex).gate() > 0 || _stepSelection[stepIndex]);
        leds.set(MatrixMap::fromStep(i), red, green);
    }

    LedPainter::drawSelectedSequenceSection(leds, _section);

    LedPainter::drawSelectedSequenceSection(leds, _section);

    // show quick edit keys
    if (globalKeyState()[Key::Page] && !globalKeyState()[Key::Shift]) {
        for (int i = 0; i < 8; ++i) {
            int index = MatrixMap::fromStep(i + 8);
            leds.unmask(index);
            leds.set(index, false, quickEditItems[i] != CurveSequenceListModel::Item::Last);
            leds.mask(index);
        }
    }
}

void CurveSequenceEditPage::keyDown(KeyEvent &event) {
    _stepSelection.keyDown(event, stepOffset());
    updateMonitorStep();
}

void CurveSequenceEditPage::keyUp(KeyEvent &event) {
    _stepSelection.keyUp(event, stepOffset());
    updateMonitorStep();
}

void CurveSequenceEditPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();
    auto &sequence = _project.selectedCurveSequence();

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

    if (key.isFunction()) {
        switchLayer(key.function(), key.shiftModifier());
        event.consume();
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

void CurveSequenceEditPage::encoder(EncoderEvent &event) {
    auto &sequence = _project.selectedCurveSequence();

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
            case Layer::Shape:
                step.setShape(step.shape() + event.value());
                break;
            case Layer::ShapeVariation:
                step.setShapeVariation(step.shapeVariation() + event.value());
                break;
            case Layer::ShapeVariationProbability:
                step.setShapeVariationProbability(step.shapeVariationProbability() + event.value());
                break;
            case Layer::Min:
            case Layer::Max: {
                bool functionPressed = globalKeyState()[MatrixMap::fromFunction(activeFunctionKey())];
                int offset = event.value() * ((shift || event.pressed()) ? 1 : 8);
                if (functionPressed) {
                    // adjust both min and max
                    offset = clamp(offset, -step.min(), CurveSequence::Max::max() - step.max());
                    step.setMin(step.min() + offset);
                    step.setMax(step.max() + offset);
                } else {
                    // adjust min or max
                    if (layer() == Layer::Min) {
                        step.setMin(step.min() + offset);
                    } else {
                        step.setMax(step.max() + offset);
                    }
                }
                break;
            }
            case Layer::Gate:
                step.setGate(step.gate() + event.value());
                break;
            case Layer::GateProbability:
                step.setGateProbability(step.gateProbability() + event.value());
                break;
            case Layer::Last:
                break;
            }
        }
    }

    event.consume();
}

void CurveSequenceEditPage::switchLayer(int functionKey, bool shift) {
    if (shift) {
        switch (Function(functionKey)) {
        case Function::Shape:
            setLayer(Layer::Shape);
            break;
        case Function::Min:
            setLayer(Layer::Min);
            break;
        case Function::Max:
            setLayer(Layer::Max);
            break;
        case Function::Gate:
            setLayer(Layer::Gate);
            break;
        }
        return;
    }

    switch (Function(functionKey)) {
    case Function::Shape:
        switch (layer()) {
        case Layer::Shape:
            setLayer(Layer::ShapeVariation);
            break;
        case Layer::ShapeVariation:
            setLayer(Layer::ShapeVariationProbability);
            break;
        case Layer::ShapeVariationProbability:
            setLayer(Layer::Shape);
            break;
        default:
            setLayer(Layer::Shape);
            break;
        }
        break;
    case Function::Min:
        setLayer(Layer::Min);
        break;
    case Function::Max:
        setLayer(Layer::Max);
        break;
    case Function::Gate:
        switch (layer()) {
        case Layer::Gate:
            setLayer(Layer::GateProbability);
            break;
        case Layer::GateProbability:
            setLayer(Layer::Gate);
            break;
        default:
            setLayer(Layer::Gate);
            break;
        }
        break;
    }
}

int CurveSequenceEditPage::activeFunctionKey() {
    switch(layer()) {
    case Layer::Shape:
    case Layer::ShapeVariation:
    case Layer::ShapeVariationProbability:
        return 0;
    case Layer::Min:
        return 1;
    case Layer::Max:
        return 2;
    case Layer::Gate:
    case Layer::GateProbability:
        return 3;
    case Layer::Last:
        break;
    }

    return -1;
}

void CurveSequenceEditPage::updateMonitorStep() {
    auto &trackEngine = _engine.selectedTrackEngine().as<CurveTrackEngine>();

    if ((layer() == Layer::Min || layer() == Layer::Max) && !_stepSelection.isPersisted() && _stepSelection.any()) {
        trackEngine.setMonitorStep(_stepSelection.first());
        trackEngine.setMonitorStepLevel(layer() == Layer::Min ? CurveTrackEngine::MonitorLevel::Min : CurveTrackEngine::MonitorLevel::Max);
    } else {
        trackEngine.setMonitorStep(-1);
    }
}

void CurveSequenceEditPage::drawDetail(Canvas &canvas, const CurveSequence::Step &step, int stepIndex) {
    FixedStringBuilder<16> str;

    WindowPainter::drawFrame(canvas, 64, 16, 128, 32);

    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(UI_COLOR_ACTIVE);
    canvas.vline(64 + 32, 16, 32);

    canvas.setFont(Font::Small);
    str(TXT_INFO_STEP_INDEX, _stepSelection.first() + 1);
    if (_stepSelection.count() > 1) {
        str(TXT_INFO_STEP_SELECTION_MARKER);
    }
    canvas.drawTextCentered(64, 16, 32, 32, str);

    canvas.setFont(Font::Tiny);

    const auto &trackEngine = _engine.selectedTrackEngine().as<CurveTrackEngine>();
    const auto &sequence = _project.selectedCurveSequence();

    int baseColor = 0;

    if ((stepIndex < sequence.firstStep() || stepIndex > sequence.lastStep()) && stepIndex != trackEngine.currentStep()) {
        baseColor = UI_COLOR_INACTIVE;
    } else if (_stepSelection[stepIndex]) {
        baseColor = UI_COLOR_ACTIVE;
    } else if (stepIndex != trackEngine.currentStep()) {
        baseColor = UI_COLOR_NORMAL;
    }

    switch (layer()) {
    case Layer::Shape:
    case Layer::ShapeVariation:
        break;
    case Layer::ShapeVariationProbability:
        SequencePainter::drawProbability(
            canvas,
            baseColor,
            64 + 32 + 8, 32 - 4, 64 - 16, 8,
            step.shapeVariationProbability(), 8
        );
        str.reset();
        str(TXT_INFO_SHAPE_VARIATION_PROBABILITY, 100.f * step.shapeVariationProbability() / 8.f);
        canvas.setColor(baseColor);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
        break;
    case Layer::Min:
    case Layer::Max:
    case Layer::Gate:
    case Layer::GateProbability:
        SequencePainter::drawProbability(
            canvas,
            baseColor,
            64 + 32 + 8, 32 - 4, 64 - 16, 8,
            step.gateProbability() + 1, CurveSequence::GateProbability::Range
        );
        str.reset();
        str(TXT_INFO_STEP_PROBABILITY, 100.f * (step.gateProbability() + 1.f) / CurveSequence::GateProbability::Range);
        canvas.setColor(baseColor);
        canvas.drawTextCentered(64 + 32 + 64, 32 - 4, 32, 8, str);
        break;
    case Layer::Last:
        break;
    }
}

void CurveSequenceEditPage::contextShow() {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); }
    ));
}

void CurveSequenceEditPage::contextAction(int index) {
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

bool CurveSequenceEditPage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    case ContextAction::Paste:
        return _model.clipBoard().canPasteCurveSequenceSteps();
    default:
        return true;
    }
}

void CurveSequenceEditPage::initSequence() {
    _project.selectedCurveSequence().clearSteps();
    showMessage(TXT_MESSAGE_STEPS_INITIALIZED);
}

void CurveSequenceEditPage::copySequence() {
    _model.clipBoard().copyCurveSequenceSteps(_project.selectedCurveSequence(), _stepSelection.selected());
    showMessage(TXT_MESSAGE_STEPS_COPIED);
}

void CurveSequenceEditPage::pasteSequence() {
    _model.clipBoard().pasteCurveSequenceSteps(_project.selectedCurveSequence(), _stepSelection.selected());
    showMessage(TXT_MESSAGE_STEPS_PASTED);
}

void CurveSequenceEditPage::duplicateSequence() {
    _project.selectedCurveSequence().duplicateSteps();
    showMessage(TXT_MESSAGE_STEPS_DUPLICATED);
}

void CurveSequenceEditPage::generateSequence() {
    _manager.pages().generatorSelect.show([this] (bool success, Generator::Mode mode) {
        if (success) {
            auto builder = _builderContainer.create<CurveSequenceBuilder>(_project.selectedCurveSequence(), layer());
            auto generator = Generator::execute(mode, *builder);
            if (generator) {
                _manager.pages().generator.show(generator);
            }
        }
    });
}

void CurveSequenceEditPage::quickEdit(int index) {
    _listModel.setSequence(&_project.selectedCurveSequence());
    if (quickEditItems[index] != CurveSequenceListModel::Item::Last) {
        _manager.pages().quickEdit.show(_listModel, int(quickEditItems[index]));
    }
}
