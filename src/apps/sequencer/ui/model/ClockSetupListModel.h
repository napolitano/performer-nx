#pragma once

#include "Config.h"

#include "ListModel.h"

#include "model/ClockSetup.h"

class ClockSetupListModel : public ListModel {
public:
    ClockSetupListModel(ClockSetup &clockSetup) :
        _clockSetup(clockSetup)
    {}

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
            formatValue(Item(row), str);
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (column == 1) {
            editValue(Item(row), value, shift);
        }
    }

private:
    enum Item {
        Mode,
        ShiftMode,
        ClockInputDivisor,
        ClockInputMode,
        ClockOutputDivisor,
        ClockOutputSwing,
        ClockOutputPulse,
        ClockOutputMode,
        MidiRx,
        MidiTx,
        UsbRx,
        UsbTx,
        Last
    };

    static const char *itemName(Item item) {
        switch (item) {
        case Mode:              return TXT_LIST_LABEL_MODE;
        case ShiftMode:         return TXT_LIST_LABEL_SHIFT_MODE;
        case ClockInputDivisor: return TXT_LIST_LABEL_INPUT_DIVISOR;
        case ClockInputMode:    return TXT_LIST_LABEL_INPUT_MODE;
        case ClockOutputDivisor:return TXT_LIST_LABEL_OUTPUT_DIVISOR;
        case ClockOutputSwing:  return TXT_LIST_LABEL_OUTPUT_SWING;
        case ClockOutputPulse:  return TXT_LIST_LABEL_OUTPUT_PULSE;
        case ClockOutputMode:   return TXT_LIST_LABEL_OUTPUT_MODE;
        case MidiRx:            return TXT_LIST_LABEL_MIDI_RX;
        case MidiTx:            return TXT_LIST_LABEL_MIDI_TX;
        case UsbRx:             return TXT_LIST_LABEL_USB_RX;
        case UsbTx:             return TXT_LIST_LABEL_USB_TX;
        case Last:              break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, StringBuilder &str) const {
        switch (item) {
        case Mode:
            _clockSetup.printMode(str);
            break;
        case ShiftMode:
            _clockSetup.printShiftMode(str);
            break;
        case ClockInputDivisor:
            _clockSetup.printClockInputDivisor(str);
            break;
        case ClockInputMode:
            _clockSetup.printClockInputMode(str);
            break;
        case ClockOutputDivisor:
            _clockSetup.printClockOutputDivisor(str);
            break;
        case ClockOutputSwing:
            _clockSetup.printClockOutputSwing(str);
            break;
        case ClockOutputPulse:
            _clockSetup.printClockOutputPulse(str);
            break;
        case ClockOutputMode:
            _clockSetup.printClockOutputMode(str);
            break;
        case MidiRx:
            _clockSetup.printMidiRx(str);
            break;
        case MidiTx:
            _clockSetup.printMidiTx(str);
            break;
        case UsbRx:
            _clockSetup.printUsbRx(str);
            break;
        case UsbTx:
            _clockSetup.printUsbTx(str);
            break;
        case Last:
            break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        switch (item) {
        case Mode:
            _clockSetup.editMode(value, shift);
            break;
        case ShiftMode:
            _clockSetup.editShiftMode(value, shift);
            break;
        case ClockInputDivisor:
            _clockSetup.editClockInputDivisor(value, shift);
            break;
        case ClockInputMode:
            _clockSetup.editClockInputMode(value, shift);
            break;
        case ClockOutputDivisor:
            _clockSetup.editClockOutputDivisor(value, shift);
            break;
        case ClockOutputSwing:
            _clockSetup.editClockOutputSwing(value, shift);
            break;
        case ClockOutputPulse:
            _clockSetup.editClockOutputPulse(value, shift);
            break;
        case ClockOutputMode:
            _clockSetup.editClockOutputMode(value, shift);
            break;
        case MidiRx:
            _clockSetup.editMidiRx(value, shift);
            break;
        case MidiTx:
            _clockSetup.editMidiTx(value, shift);
            break;
        case UsbRx:
            _clockSetup.editUsbRx(value, shift);
            break;
        case UsbTx:
            _clockSetup.editUsbTx(value, shift);
            break;
        case Last:
            break;
        }
    }

    ClockSetup &_clockSetup;
};
