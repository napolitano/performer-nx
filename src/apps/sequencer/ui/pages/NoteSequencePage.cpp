#include "Config.h"
#include "NoteSequencePage.h"

#include "Pages.h"

#include "ui/LedPainter.h"
#include "ui/painters/WindowPainter.h"

#include "core/utils/StringBuilder.h"

enum class ContextAction {
    Init,
    Copy,
    Paste,
    Duplicate,
    Route,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { TXT_MENU_INIT },
    { TXT_MENU_COPY },
    { TXT_MENU_PASTE },
    { TXT_MENU_DUPLICATE },
    { TXT_MENU_ROUTE },
};


NoteSequencePage::NoteSequencePage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel)
{}

void NoteSequencePage::enter() {
    _listModel.setSequence(&_project.selectedNoteSequence());
}

void NoteSequencePage::exit() {
    _listModel.setSequence(nullptr);
}

void NoteSequencePage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, TXT_MODE_SEQUENCE);
    WindowPainter::drawActiveFunction(canvas, Track::trackModeName(_project.selectedTrack().trackMode()));
    WindowPainter::drawFooter(canvas);

    ListPage::draw(canvas);
}

void NoteSequencePage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}

void NoteSequencePage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    if (key.pageModifier()) {
        return;
    }

    if (!event.consumed()) {
        ListPage::keyPress(event);
    }
}

void NoteSequencePage::contextShow() {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); }
    ));
}

void NoteSequencePage::contextAction(int index) {
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
    case ContextAction::Route:
        initRoute();
        break;
    case ContextAction::Last:
        break;
    }
}

bool NoteSequencePage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    case ContextAction::Paste:
        return _model.clipBoard().canPasteNoteSequence();
    case ContextAction::Route:
        return _listModel.routingTarget(selectedRow()) != Routing::Target::None;
    default:
        return true;
    }
}

void NoteSequencePage::initSequence() {
    _project.selectedNoteSequence().clear();
    showMessage(TXT_MESSAGE_SEQUENCE_INITIALIZED);
}

void NoteSequencePage::copySequence() {
    _model.clipBoard().copyNoteSequence(_project.selectedNoteSequence());
    showMessage(TXT_MESSAGE_SEQUENCE_COPIED);
}

void NoteSequencePage::pasteSequence() {
    _model.clipBoard().pasteNoteSequence(_project.selectedNoteSequence());
    showMessage(TXT_MESSAGE_SEQUENCE_PASTED);
}

void NoteSequencePage::duplicateSequence() {
    if (_project.selectedTrack().duplicatePattern(_project.selectedPatternIndex())) {
        showMessage(TXT_MESSAGE_SEQUENCE_DUPLICATED);
    }
}

void NoteSequencePage::initRoute() {
    _manager.pages().top.editRoute(_listModel.routingTarget(selectedRow()), _project.selectedTrackIndex());
}
