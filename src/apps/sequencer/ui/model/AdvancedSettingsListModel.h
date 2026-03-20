#pragma once

#include "ListModel.h"
#include "model/AdvancedSettings.h"

class AdvancedSettingsListModel : public ListModel {
public:
    enum Item {
        EnhancedUi,
        UiLanguage,
        Last
    };

    virtual int rows() const override {
        return Last;
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (column == 0) {
            formatName(Item(row), str);
        } else if (column == 1) {
                str("Yes");
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (column == 1) {

        }
    }

private:

    static const char *itemName(Item item) {
        switch (item) {
            case EnhancedUi:
                return "Enhanced UI";
            case UiLanguage:
                return "Language";
            case Last:
                break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }
};
