#include "Config.h"
#include "ContextMenuPage.h"

namespace {
    // TODO: This page currently renders only five slots and maps them to F-keys directly.
    // If hardware key count or menu size changes, update rendering and key mapping together.
    constexpr int kMenuSlotCount = 5;
    constexpr int kBarHeight = 12;
    constexpr int kTextBaselineOffset = 4;
}

/**
 * Creates the modal context menu page and initializes safe defaults.
 */
ContextMenuPage::ContextMenuPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context),
    _contextMenuModel(nullptr),
    _callback()
{}

/**
 * Opens the context menu with the provided model and completion callback.
 * The callback receives the selected item index when the user confirms a choice.
 */
void ContextMenuPage::show(ContextMenuModel &contextMenuModel, ResultCallback callback) {
    _contextMenuModel = &contextMenuModel;
    _callback = callback;
    BasePage::show();
}

/**
 * Draws the bottom context bar and up to five function-key mapped menu items.
 */
void ContextMenuPage::draw(Canvas &canvas) {
    // Defensive guard for rare lifecycle misuse before show() wires the model.
    if (!_contextMenuModel) {
        // TODO: Consider replacing with an assertion if draw-before-show should be impossible.
        return;
    }

    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);

    const int barTop = Height - kBarHeight;
    const int slotWidth = Width / kMenuSlotCount;

    // Paint menu bar background and top separator line.
    canvas.setColor(UI_COLOR_BLACK);
    canvas.fillRect(0, barTop - 1, Width, kBarHeight + 1);

    canvas.setColor(UI_COLOR_ACTIVE);
    canvas.hline(0, barTop, Width);

    // Render one label per function-key slot.
    for (int slotIndex = 0; slotIndex < kMenuSlotCount; ++slotIndex) {
        const int itemIndex = slotIndex;
        if (itemIndex >= _contextMenuModel->itemCount()) {
            continue;
        }

        const auto &item = _contextMenuModel->item(itemIndex);
        if (!item.title) {
            continue;
        }

        const bool enabled = _contextMenuModel->itemEnabled(itemIndex);
        const int slotX = (Width * slotIndex) / kMenuSlotCount;

        canvas.setColor(enabled ? UI_COLOR_ACTIVE : UI_COLOR_DIM);
        const int textX = slotX + (slotWidth - canvas.textWidth(item.title) + 1) / 2;
        canvas.drawText(textX, Height - kTextBaselineOffset, item.title);
    }
}

/**
 * Closes the menu when the user releases the context-menu chord.
 */
void ContextMenuPage::keyUp(KeyEvent &event) {
    const auto &key = event.key();

    if (!key.pageModifier() || !key.shiftModifier()) {
        close();
        event.consume();
    }
}

/**
 * Handles function-key item selection and confirms enabled entries.
 */
void ContextMenuPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isFunction()) {
        const int itemIndex = key.function();

        if (_contextMenuModel && itemIndex >= 0 && itemIndex < _contextMenuModel->itemCount() && _contextMenuModel->itemEnabled(itemIndex)) {
            closeAndCallback(itemIndex);
        }

        event.consume();
    }
}

/**
 * Consumes encoder input because this modal menu is function-key driven.
 */
void ContextMenuPage::encoder(EncoderEvent &event) {
    event.consume();
}

/**
 * Closes the page and dispatches the selected index to the callback.
 */
void ContextMenuPage::closeAndCallback(int index) {
    close();
    if (_callback) {
        _callback(index);
    }
}
