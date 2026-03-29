#pragma once

#include "Config.h"
#include "Serialize.h"
#include "ModelUtils.h"

#include "core/utils/StringBuilder.h"

#include <cstdint>

class Arpeggiator {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    enum class Mode : uint8_t {
        PlayOrder,
        Up,
        Down,
        UpDown,
        DownUp,
        UpAndDown,
        DownAndUp,
        Converge,
        Diverge,
        Random,
        Last
    };

    static const char *modeName(Mode mode) {
        switch (mode) {
        case Mode::PlayOrder:   return TXT_MODEL_MODE_PLAY_ORDER;
        case Mode::Up:          return TXT_MODEL_MODE_UP;
        case Mode::Down:        return TXT_MODEL_MODE_DOWN;
        case Mode::UpDown:      return TXT_MODEL_MODE_UP_DOWN;
        case Mode::DownUp:      return TXT_MODEL_MODE_DOWN_UP;
        case Mode::UpAndDown:   return TXT_MODEL_MODE_UP_AND_DOWN;
        case Mode::DownAndUp:   return TXT_MODEL_MODE_DOWN_AND_UP;
        case Mode::Converge:    return TXT_MODEL_MODE_CONVERGE;
        case Mode::Diverge:     return TXT_MODEL_MODE_DIVERGE;
        case Mode::Random:      return TXT_MODEL_MODE_RANDOM;
        case Mode::Last:        break;
        }
        return nullptr;
    }

    static uint8_t modeSerialize(Mode mode) {
        switch (mode) {
        case Mode::PlayOrder:   return 0;
        case Mode::Up:          return 1;
        case Mode::Down:        return 2;
        case Mode::UpDown:      return 3;
        case Mode::DownUp:      return 4;
        case Mode::UpAndDown:   return 5;
        case Mode::DownAndUp:   return 6;
        case Mode::Converge:    return 7;
        case Mode::Diverge:     return 8;
        case Mode::Random:      return 9;
        case Mode::Last:        break;
        }
        return 0;
    }

    //----------------------------------------
    // Properties
    //----------------------------------------

    // enabled

    bool enabled() const { return _enabled; }
    void setEnabled(bool enabled) {
        _enabled = enabled;
    }

    void editEnabled(int value, bool shift) {
        setEnabled(value > 0);
    }

    void printEnabled(StringBuilder &str) const {
        ModelUtils::printYesNo(str, enabled());
    }

    // hold

    bool hold() const { return _hold; }
    void setHold(bool hold) {
        _hold = hold;
    }

    void editHold(int value, bool shift) {
        setHold(value > 0);
    }

    void printHold(StringBuilder &str) const {
        ModelUtils::printYesNo(str, hold());
    }

    // mode

    Mode mode() const { return _mode; }
    void setMode(Mode mode) {
        _mode = ModelUtils::clampedEnum(mode);
    }

    void editMode(int value, bool shift) {
        setMode(ModelUtils::adjustedEnum(mode(), value));
    }

    void printMode(StringBuilder &str) const {
        str(modeName(mode()));
    }

    // divisor

    int divisor() const { return _divisor; }
    void setDivisor(int divisor) {
        _divisor = ModelUtils::clampDivisor(divisor);
    }

    void editDivisor(int value, bool shift) {
        setDivisor(ModelUtils::adjustedByDivisor(divisor(), value, shift));
    }

    void printDivisor(StringBuilder &str) const {
        ModelUtils::printDivisor(str, divisor());
    }

    // gateLength

    int gateLength() const { return _gateLength; }
    void setGateLength(int gateLength) {
        _gateLength = clamp(gateLength, 1, 100);
    }

    void editGateLength(int value, bool shift) {
        setGateLength(gateLength() + value * (shift ? 10 : 1));
    }

    void printGateLength(StringBuilder &str) const {
        str(TXT_MODEL_GATE_LENGTH, gateLength());
    }

    // octaves

    int octaves() const { return _octaves; }
    void setOctaves(int octaves) {
        _octaves = clamp(octaves, -10, 10);
    }

    void editOctaves(int value, bool shift) {
        setOctaves(octaves() + value);
    }

    void printOctaves(StringBuilder &str) const {
        int value = octaves();
        if (value > 5) {
            str(TXT_MODEL_PRINT_OCTAVE_UP_DOWN, value - 5);
        } else if (value > 0) {
            str(TXT_MODEL_PRINT_OCTAVE_UP, value);
        } else if (value == 0) {
            str(TXT_MODEL_PRINT_OCTAVE_OFF);
        } else if (value >= -5) {
            str(TXT_MODEL_PRINT_OCTAVE_DOWN, -value);
        } else if (value >= -10) {
            str(TXT_MODEL_PRINT_OCTAVE_DOWN_UP, -(value + 5));
        }
    }

    //----------------------------------------
    // Methods
    //----------------------------------------

    void clear();

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    bool _enabled;
    bool _hold;
    Mode _mode;
    uint16_t _divisor;
    uint8_t _gateLength;
    int8_t _octaves;
};
