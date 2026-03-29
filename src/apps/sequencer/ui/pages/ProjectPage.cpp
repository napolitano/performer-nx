#include "Config.h"
#include "ProjectPage.h"

#include "ui/LedPainter.h"
#include "ui/pages/Pages.h"
#include "ui/painters/WindowPainter.h"

#include "model/FileManager.h"

#include "core/utils/StringBuilder.h"

enum class ContextAction {
    Init,
    Load,
    Save,
    SaveAs,
    Route,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { TXT_MENU_INIT },
    { TXT_MENU_LOAD },
    { TXT_MENU_SAVE },
    { TXT_MENU_SAVE_AS },
    { TXT_MENU_ROUTE }
};

ProjectPage::ProjectPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel),
    _listModel(context.model.project())
{}

void ProjectPage::enter() {
}

void ProjectPage::exit() {
}

void ProjectPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, TXT_MODE_PROJECT);
    WindowPainter::drawFooter(canvas);

    ListPage::draw(canvas);
}

void ProjectPage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}

void ProjectPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    if (key.pageModifier()) {
        // easter egg
        if (key.is(Key::Step15)) {
            _manager.pages().asteroids.show();
        }
        return;
    }

    if (key.is(Key::Encoder) && selectedRow() == 0) {
        _manager.pages().textInput.show(TXT_ENTER_NAME_PREFIX, _project.name(), Project::NameLength, [this] (bool result, const char *text) {
            if (result) {
                _project.setName(text);
            }
        });

        return;
    }

    ListPage::keyPress(event);
}

void ProjectPage::encoder(EncoderEvent &event) {
    ListPage::encoder(event);
}

void ProjectPage::contextShow() {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); }
    ));
}

void ProjectPage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Init:
        initProject();
        break;
    case ContextAction::Load:
        loadProject();
        break;
    case ContextAction::Save:
        saveProject();
        break;
    case ContextAction::SaveAs:
        saveAsProject();
        break;
    case ContextAction::Route:
        initRoute();
        break;
    case ContextAction::Last:
        break;
    }
}

bool ProjectPage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    case ContextAction::Load:
    case ContextAction::Save:
    case ContextAction::SaveAs:
        return FileManager::volumeMounted();
    case ContextAction::Route:
        return _listModel.routingTarget(selectedRow()) != Routing::Target::None;
    default:
        return true;
    }
}

void ProjectPage::initProject() {
    _manager.pages().confirmation.show(TXT_ARE_YOU_SURE, [this] (bool result) {
        if (result) {
            _engine.suspend();
            _project.clear();
            showMessage(TXT_MESSAGE_PROJECT_INITIALIZED);
            _engine.resume();
        }
    });
}

void ProjectPage::loadProject() {
    _manager.pages().fileSelect.show(TXT_FUNCTION_LOAD_PROJECT, FileType::Project, _project.slotAssigned() ? _project.slot() : 0, false, [this] (bool result, int slot) {
        if (result) {
            _manager.pages().confirmation.show(TXT_ARE_YOU_SURE, [this, slot] (bool result) {
                if (result) {
                    loadProjectFromSlot(slot);
                }
            });
        }
    });
}

void ProjectPage::saveProject() {
    if (!_project.slotAssigned() || _project.autoLoaded()) {
        saveAsProject();
        return;
    }

    saveProjectToSlot(_project.slot());
}

void ProjectPage::saveAsProject() {
    _manager.pages().fileSelect.show(TXT_FUNCTION_SAVE_PROJECT, FileType::Project, _project.slotAssigned() ? _project.slot() : 0, true, [this] (bool result, int slot) {
        if (result) {
            if (FileManager::slotUsed(FileType::Project, slot)) {
                _manager.pages().confirmation.show(TXT_ARE_YOU_SURE, [this, slot] (bool result) {
                    if (result) {
                        saveProjectToSlot(slot);
                    }
                });
            } else {
                saveProjectToSlot(slot);
            }
        }
    });
}

void ProjectPage::initRoute() {
    _manager.pages().top.editRoute(_listModel.routingTarget(selectedRow()), 0);
}

void ProjectPage::saveProjectToSlot(int slot) {
    _engine.suspend();
    _manager.pages().busy.show(TXT_STATUS_SAVING_PROJECT);

    FileManager::task([this, slot] () {
        return FileManager::writeProject(_project, slot);
    }, [this] (fs::Error result) {
        if (result == fs::OK) {
            showMessage(TXT_MESSAGE_PROJECT_SAVED);
        } else {
            showMessage(FixedStringBuilder<32>(TXT_ERROR_FAILED, fs::errorToString(result)));
        }
        // TODO lock ui mutex
        _manager.pages().busy.close();
        _engine.resume();
    });
}

void ProjectPage::loadProjectFromSlot(int slot) {
    _engine.suspend();
    _manager.pages().busy.show(TXT_STATUS_LOADING_PROJECT);

    FileManager::task([this, slot] () {
        // TODO this is running in file manager thread but model notification affect ui
        return FileManager::readProject(_project, slot);
    }, [this] (fs::Error result) {
        if (result == fs::OK) {
            showMessage(TXT_MESSAGE_PROJECT_LOADED);
        } else if (result == fs::INVALID_CHECKSUM) {
            showMessage(TXT_ERROR_INVALID_PROJECT_FILE);
        } else {
            showMessage(FixedStringBuilder<32>(TXT_ERROR_FAILED, fs::errorToString(result)));
        }
        // TODO lock ui mutex
        _manager.pages().busy.close();
        _engine.resume();
    });
}
