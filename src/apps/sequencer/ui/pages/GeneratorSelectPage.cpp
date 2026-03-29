#include "Config.h"
#include "GeneratorSelectPage.h"

#include "ui/painters/WindowPainter.h"

enum class Function {
    Cancel  = 3,
    OK      = 4,
};

static const char *functionNames[] = {
    nullptr,
    nullptr,
    nullptr,
    TXT_MENU_CANCEL,
    TXT_MENU_OK
};


GeneratorSelectPage::GeneratorSelectPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{}

void GeneratorSelectPage::show(ResultCallback callback) {
    _callback = callback;
    ListPage::show();
}

void GeneratorSelectPage::enter() {
}

void GeneratorSelectPage::exit() {
}

void GeneratorSelectPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, TXT_MODE_GENERATOR);
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState());

    ListPage::draw(canvas);
}

void GeneratorSelectPage::updateLeds(Leds &leds) {
}

void GeneratorSelectPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isFunction()) {
        switch (Function(key.function())) {
        case Function::Cancel:
            closeWithResult(false);
            break;
        case Function::OK:
            closeWithResult(true);
            break;
        }
    }

    if (key.is(Key::Encoder)) {
        closeWithResult(true);
        return;
    }

    ListPage::keyPress(event);
}

void GeneratorSelectPage::closeWithResult(bool result) {
    Page::close();
    if (_callback) {
        _callback(result, Generator::Mode(selectedRow()));
    }
}
