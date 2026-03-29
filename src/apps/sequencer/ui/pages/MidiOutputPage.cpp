#include "Config.h"
#include "MidiOutputPage.h"

#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"

enum class Function {
    Prev    = 0,
    Next    = 1,
    Init    = 2,
    Commit  = 4,
};

MidiOutputPage::MidiOutputPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _outputListModel),
    _outputListModel(_editOutput)
{
    showOutput(0);
}

void MidiOutputPage::reset() {
    showOutput(0);
}

void MidiOutputPage::enter() {
    ListPage::enter();
}

void MidiOutputPage::exit() {
    ListPage::exit();
}

void MidiOutputPage::draw(Canvas &canvas) {
    bool showCommit = *_output != _editOutput;
    const char *functionNames[] = {
        TXT_MENU_PREVIOUS,
        TXT_MENU_NEXT,
        TXT_MENU_INIT,
        nullptr,
        showCommit ? TXT_MENU_COMMIT : nullptr
    };

    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, TXT_MODE_MIDI_OUTPUT);
    WindowPainter::drawActiveFunction(canvas, FixedStringBuilder<16>(TXT_FUNCTION_MIDI_OUTPUT_INDEX, _outputIndex + 1));
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState());

    ListPage::draw(canvas);
}

void MidiOutputPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isFunction()) {
        switch (Function(key.function())) {
        case Function::Prev:
            selectOutput(_outputIndex - 1);
            break;
        case Function::Next:
            selectOutput(_outputIndex + 1);
            break;
        case Function::Init:
            _editOutput.clear();
            setSelectedRow(0);
            setEdit(false);
            break;
        case Function::Commit:
            *_output = _editOutput;
            setEdit(false);
            showMessage(TXT_MESSAGE_MIDI_OUTPUT_CHANGED);
            break;
        }
        event.consume();
    }

    ListPage::keyPress(event);
}

void MidiOutputPage::encoder(EncoderEvent &event) {
    if (!edit() && pageKeyState()[Key::Shift]) {
        selectOutput(_outputIndex + event.value());
        event.consume();
        return;
    }

    ListPage::encoder(event);
}

void MidiOutputPage::showOutput(int outputIndex) {
    _output = &_project.midiOutput().output(outputIndex);
    _outputIndex = outputIndex;
    _editOutput = *_output;

    setSelectedRow(0);
    setEdit(false);
}

void MidiOutputPage::selectOutput(int outputIndex) {
    outputIndex = clamp(outputIndex, 0, CONFIG_MIDI_OUTPUT_COUNT - 1);
    if (outputIndex != _outputIndex) {
        showOutput(outputIndex);
    }
}
