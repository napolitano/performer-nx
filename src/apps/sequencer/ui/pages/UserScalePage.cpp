#include "Config.h"
#include "UserScalePage.h"

#include "ui/pages/Pages.h"
#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"

enum class ContextAction {
    Init,
    Copy,
    Paste,
    Load,
    Save,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { TXT_MENU_INIT },
    { TXT_MENU_COPY },
    { TXT_MENU_PASTE },
    { TXT_MENU_LOAD },
    { TXT_MENU_SAVE },
};

static const char *functionNames[] = {
    TXT_MENU_USER_SCALE_1,
    TXT_MENU_USER_SCALE_2,
    TXT_MENU_USER_SCALE_3,
    TXT_MENU_USER_SCALE_4,
    nullptr
};

UserScalePage::UserScalePage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{
    setSelectedIndex(0);
}

void UserScalePage::enter() {
}

void UserScalePage::exit() {
}

void UserScalePage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, TXT_MODE_USER_SCALE);
    FixedStringBuilder<8> str(TXT_FUNCTION_USER_SCALE_NUMBER, _selectedIndex + 1);
    WindowPainter::drawActiveFunction(canvas, str);
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), _selectedIndex);

    ListPage::draw(canvas);
}

void UserScalePage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    if (key.isFunction()) {
        if (key.function() < CONFIG_USER_SCALE_COUNT) {
            setSelectedIndex(key.function());
        }
        event.consume();
    }

    if (key.is(Key::Encoder) && selectedRow() == 0) {
        _manager.pages().textInput.show(TXT_ENTER_NAME_PREFIX, _userScale->name(), UserScale::NameLength, [this] (bool result, const char *text) {
            if (result) {
                _userScale->setName(text);
            }
        });

        return;
    }

    ListPage::keyPress(event);
}

void UserScalePage::setSelectedIndex(int index) {
    _selectedIndex = index;
    _userScale = &UserScale::userScales[index];
    _listModel.setUserScale(*_userScale);

    setSelectedRow(0);
    setEdit(false);
}

void UserScalePage::contextShow() {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); }
    ));
}

void UserScalePage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Init:
        initUserScale();
        break;
    case ContextAction::Copy:
        copyUserScale();
        break;
    case ContextAction::Paste:
        pasteUserScale();
        break;
    case ContextAction::Load:
        loadUserScale();
        break;
    case ContextAction::Save:
        saveUserScale();
        break;
    case ContextAction::Last:
        break;
    }
}

bool UserScalePage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    case ContextAction::Paste:
        return _model.clipBoard().canPasteUserScale();
    case ContextAction::Load:
    case ContextAction::Save:
        return FileManager::volumeMounted();
    default:
        return true;
    }
}

void UserScalePage::initUserScale() {
    _userScale->clear();
    showMessage(TXT_MESSAGE_USER_SCALE_INITIALIZED);
}

void UserScalePage::copyUserScale() {
    _model.clipBoard().copyUserScale(*_userScale);
    showMessage(TXT_MESSAGE_USER_SCALE_COPIED);
}

void UserScalePage::pasteUserScale() {
    _model.clipBoard().pasteUserScale(*_userScale);
    showMessage(TXT_MESSAGE_USER_SCALE_PASTED);
}

void UserScalePage::loadUserScale() {
    _manager.pages().fileSelect.show(TXT_FUNCTION_USER_SCLAE_LOAD, FileType::UserScale, 0, false, [this] (bool result, int slot) {
        if (result) {
            _manager.pages().confirmation.show(TXT_ARE_YOU_SURE, [this, slot] (bool result) {
                if (result) {
                    loadUserScaleFromSlot(slot);
                }
            });
        }
    });
}

void UserScalePage::saveUserScale() {
    _manager.pages().fileSelect.show(TXT_FUNCTION_USER_SCALE_SAVE, FileType::UserScale, 0, true, [this] (bool result, int slot) {
        if (result) {
            if (FileManager::slotUsed(FileType::UserScale, slot)) {
                _manager.pages().confirmation.show(TXT_ARE_YOU_SURE, [this, slot] (bool result) {
                    if (result) {
                        saveUserScaleToSlot(slot);
                    }
                });
            } else {
                saveUserScaleToSlot(slot);
            }
        }
    });
}

void UserScalePage::saveUserScaleToSlot(int slot) {
    _engine.suspend();
    _manager.pages().busy.show(TXT_STATUS_SAVING_USER_SCALE);

    FileManager::task([this, slot] () {
        return FileManager::writeUserScale(*_userScale, slot);
    }, [this] (fs::Error result) {
        if (result == fs::OK) {
            showMessage(TXT_MESSAGE_USER_SCALE_SAVED);
        } else {
            showMessage(FixedStringBuilder<32>(TXT_ERROR_FAILED, fs::errorToString(result)));
        }
        // TODO lock ui mutex
        _manager.pages().busy.close();
        _engine.resume();
    });
}

void UserScalePage::loadUserScaleFromSlot(int slot) {
    _engine.suspend();
    _manager.pages().busy.show(TXT_STATUS_LOADING_USER_SCALE);

    FileManager::task([this, slot] () {
        return FileManager::readUserScale(*_userScale, slot);
    }, [this] (fs::Error result) {
        if (result == fs::OK) {
            showMessage(TXT_MESSAGE_USER_SCALE_LOADED);
        } else if (result == fs::INVALID_CHECKSUM) {
            showMessage(TXT_ERROR_INVALID_USER_SCALE_FILE);
        } else {
            showMessage(FixedStringBuilder<32>(TXT_ERROR_FAILED, fs::errorToString(result)));
        }
        // TODO lock ui mutex
        _manager.pages().busy.close();
        _engine.resume();
    });
}
