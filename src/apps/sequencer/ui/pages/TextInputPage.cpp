#include "Config.h"
#include "TextInputPage.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"
#include "core/utils/StringUtils.h"

#include <cstring>

static const char characterSet[] = TXT_CHARACTER_SET;

enum class Function {
    Backspace   = 0,
    Delete      = 1,
    Clear       = 2,
    Cancel      = 3,
    OK          = 4,
};

static const char *functionNames[] = {
    TXT_MENU_BACKSPACE,
    TXT_MENU_DELETE,
    TXT_MENU_CLEAR,
    TXT_MENU_CANCEL,
    TXT_MENU_OK
};


TextInputPage::TextInputPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{
}

void TextInputPage::show(const char *title, const char *text, size_t maxTextLength, ResultCallback callback) {
    _title = title;
    StringUtils::copy(_text, text, sizeof(_text));
    _maxTextLength = std::min(sizeof(_text) - 1, maxTextLength);
    _callback = callback;

    _selectedIndex = 0;
    _cursorIndex = std::strlen(_text);

    BasePage::show();
}

void TextInputPage::enter() {
}

void TextInputPage::exit() {
}

void TextInputPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState());

    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(UI_COLOR_ACTIVE);
    canvas.setFont(Font::Small);

    const int titleX = 28;
    const int titleY = 12;

    const int charsX = 28;
    const int charsY = 20;

    canvas.drawText(titleX, titleY, _title);
    int titleWidth = canvas.textWidth(_title) + 8;

    canvas.drawText(titleX + titleWidth, titleY, _text);

    int offset = 0;
    int width = 0;
    for (int i = 0; i <= _cursorIndex; ++i) {
        const char str[2] = { _text[i] == '\0' ? ' ' : _text[i], '\0' };
        width = canvas.textWidth(str);
        if (i < _cursorIndex) {
            offset += canvas.textWidth(str);
        }
    }

    if (os::ticks() % os::time::ms(300) < os::time::ms(150)) {
        canvas.setColor(UI_COLOR_DIM);
        canvas.fillRect(titleX + titleWidth + offset, titleY - 8, width - 1, 12);
        const char str[2] = { _text[_cursorIndex], '\0' };
        canvas.setBlendMode(BlendMode::Sub);
        canvas.drawText(titleX + titleWidth + offset, titleY, str);
        canvas.setBlendMode(BlendMode::Set);
    }

    canvas.setFont(Font::Tiny);
    canvas.setColor(UI_COLOR_ACTIVE);

    int ix = 0;
    int iy = 0;
    for (int i = 0; i < int(sizeof(characterSet)); ++i) {
        canvas.drawTextCentered(charsX + ix * 10, charsY + iy * 10, 10, 10, FixedStringBuilder<2>("%c", characterSet[i]));
        if (_selectedIndex == i) {
            canvas.setColor(pageKeyState()[Key::Encoder] ? UI_COLOR_ACTIVE : UI_COLOR_DIM);
            canvas.drawRect(charsX + ix * 10, charsY + iy * 10 + 1, 9, 9);
            canvas.setColor(UI_COLOR_ACTIVE);
        }
        ++ix;
        if (ix % 20 == 0) {
            ix = 0;
            ++iy;
        }
    }
}

void TextInputPage::updateLeds(Leds &leds) {
}

void TextInputPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isFunction()) {
        switch (Function(key.function())) {
        case Function::Backspace:
            backspace();
            break;
        case Function::Delete:
            del();
            break;
        case Function::Clear:
            clear();
            break;
        case Function::Cancel:
            closeWithResult(false);
            break;
        case Function::OK:
            closeWithResult(true);
            break;
        }
    }

    if (key.isLeft()) {
        moveLeft();
    }

    if (key.isRight()) {
        moveRight();
    }

    if (key.is(Key::Encoder)) {
        insert(characterSet[_selectedIndex]);
    }

    event.consume();
}

void TextInputPage::encoder(EncoderEvent &event) {
    _selectedIndex = (_selectedIndex + event.value() + sizeof(characterSet)) % sizeof(characterSet);
}

void TextInputPage::closeWithResult(bool result) {
    Page::close();
    if (_callback) {
        _callback(result, _text);
    }
}

void TextInputPage::clear() {
    _cursorIndex = 0;
    _text[0] = '\0';
}

void TextInputPage::insert(char c) {
    if (_cursorIndex < _maxTextLength && int(std::strlen(_text)) < _maxTextLength) {
        for (int i = _maxTextLength; i > _cursorIndex; --i) {
            _text[i] = _text[i - 1];
        }
        _text[_cursorIndex] = c;
        ++_cursorIndex;
    }
}

void TextInputPage::backspace() {
    if (_cursorIndex > 0) {
        --_cursorIndex;
        for (int i = _cursorIndex; i < _maxTextLength; ++i) {
            _text[i] = _text[i + 1];
        }
    }
}

void TextInputPage::del() {
    if (_text[_cursorIndex] != '\0') {
        for (int i = _cursorIndex; i < _maxTextLength; ++i) {
            _text[i] = _text[i + 1];
        }
    }
}

void TextInputPage::moveLeft() {
    _cursorIndex = std::max(0, _cursorIndex - 1);
}

void TextInputPage::moveRight() {
    if (_text[_cursorIndex] != '\0') {
        ++_cursorIndex;
    }
}
