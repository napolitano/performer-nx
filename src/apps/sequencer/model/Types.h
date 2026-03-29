#pragma once

#include "Config.h"
#include "core/utils/StringBuilder.h"
#include "core/math/Math.h"

#include <array>

#include <cstdint>

class Types {
public:
    // MonitorMode

    enum class MonitorMode : uint8_t {
        Always,
        Stopped,
        Off,
        Last,
    };

    static const char *monitorModeName(MonitorMode monitorMode) {
        switch (monitorMode) {
        case MonitorMode::Always:   return TXT_MODEL_ALWAYS;
        case MonitorMode::Stopped:  return TXT_LIST_LABEL_STOPPED;
        case MonitorMode::Off:      return TXT_MODEL_PRINT_OCTAVE_OFF;
        case MonitorMode::Last:     break;
        }
        return nullptr;
    }

    // RecordMode

    enum class RecordMode : uint8_t {
        Overdub,
        Overwrite,
        StepRecord,
        Last
    };

    static const char *recordModeName(RecordMode recordMode) {
        switch (recordMode) {
        case RecordMode::Overdub:   return TXT_LIST_LABEL_OVERDUB;
        case RecordMode::Overwrite: return TXT_LIST_LABEL_OVERWRITE;
        case RecordMode::StepRecord:return TXT_LIST_LABEL_STEP_RECORD;
        case RecordMode::Last:      break;
        }
        return nullptr;
    }

    // MidiInputMode

    enum class MidiInputMode : uint8_t {
        Off,
        All,
        Source,
        Last
    };

    // CvGateInput

    enum class CvGateInput : uint8_t {
        Off,
        Cv1Cv2,
        Cv3Cv4,
        Last
    };

    static const char *cvGateInputName(CvGateInput cvGateInput) {
        switch (cvGateInput) {
        case CvGateInput::Off:      return TXT_MODEL_PRINT_OCTAVE_OFF;
        case CvGateInput::Cv1Cv2:   return TXT_LIST_LABEL_CV1_CV2;
        case CvGateInput::Cv3Cv4:   return TXT_LIST_LABEL_CV3_CV4;
        case CvGateInput::Last:     break;
        }
        return nullptr;
    }

    enum class CurveCvInput : uint8_t {
        Off,
        Cv1,
        Cv2,
        Cv3,
        Cv4,
        Last
    };

    static const char *curveCvInput(CurveCvInput curveCvInput) {
        switch (curveCvInput) {
        case CurveCvInput::Off:     return TXT_MODEL_PRINT_OCTAVE_OFF;
        case CurveCvInput::Cv1:     return TXT_LIST_LABEL_CV1;
        case CurveCvInput::Cv2:     return TXT_LIST_LABEL_CV2;
        case CurveCvInput::Cv3:     return TXT_LIST_LABEL_CV3;
        case CurveCvInput::Cv4:     return TXT_LIST_LABEL_CV4;
        case CurveCvInput::Last:    break;
        }
        return nullptr;
    }

    // PlayMode

    enum class PlayMode : uint8_t {
        Aligned,
        Free,
        Last
    };

    static const char *playModeName(PlayMode playMode) {
        switch (playMode) {
        case PlayMode::Aligned: return TXT_LIST_LABEL_ALIGNED;
        case PlayMode::Free:    return TXT_LIST_LABEL_FREE;
        case PlayMode::Last:    break;
        }
        return nullptr;
    }

    // RunMode

    enum class RunMode : uint8_t {
        Forward,
        Backward,
        Pendulum,
        PingPong,
        Random,
        RandomWalk,
        Last
    };

    static const char *runModeName(RunMode runMode) {
        switch (runMode) {
        case RunMode::Forward:      return TXT_LIST_LABEL_FORWARD;
        case RunMode::Backward:     return TXT_LIST_LABEL_BACKWARD;
        case RunMode::Pendulum:     return TXT_LIST_LABEL_PENDULUM;
        case RunMode::PingPong:     return TXT_LIST_LABEL_PINGPONG;
        case RunMode::Random:       return TXT_LIST_LABEL_RANDOM;
        case RunMode::RandomWalk:   return TXT_LIST_LABEL_RANDOM_WALK;
        case RunMode::Last:         break;
        }
        return nullptr;
    }

    // Condition

    enum class Condition : uint8_t {
        Off,
        Fill,
        NotFill,
        Pre,
        NotPre,
        First,
        NotFirst,
        Loop,
        Loop2 = Loop,
        Loop3 = Loop2 + 2,
        Loop4 = Loop3 + 3,
        Loop5 = Loop4 + 4,
        Loop6 = Loop5 + 5,
        Loop7 = Loop6 + 6,
        Loop8 = Loop7 + 7,
        NotLoop = Loop8 + 8,
        NotLoop2 = NotLoop,
        NotLoop3 = NotLoop2 + 2,
        NotLoop4 = NotLoop3 + 3,
        NotLoop5 = NotLoop4 + 4,
        NotLoop6 = NotLoop5 + 5,
        NotLoop7 = NotLoop6 + 6,
        NotLoop8 = NotLoop7 + 7,
        Last = NotLoop8 + 8
    };

    struct ConditionInfo {
        const char *name;
        const char *short1;
        const char *short2;
    };

    struct ConditionLoop {
        uint8_t offset;
        uint8_t base;
        uint8_t invert;
    };

    enum class ConditionFormat : uint8_t {
        Long,
        Short1,
        Short2
    };

    static ConditionLoop conditionLoop(Condition condition) {
        static const uint8_t offset[] = { 0, 1,   0, 1, 2,   0, 1, 2, 3,   0, 1, 2, 3, 4,   0, 1, 2, 3, 4, 5,   0, 1, 2, 3, 4, 5, 6,   0, 1, 2, 3, 4, 5, 6, 7 };
        static const uint8_t base[]   = { 2, 2,   3, 3, 3,   4, 4, 4, 4,   5, 5, 5, 5, 5,   6, 6, 6, 6, 6, 6,   7, 7, 7, 7, 7, 7, 7,   8, 8, 8, 8, 8, 8, 8, 8 };
        int index = int(condition);
        if (index >= int(Condition::Loop) && index < int(Condition::NotLoop)) {
            index -= int(Condition::Loop);
            return { offset[index], base[index], 0 };
        } else if (index >= int(Condition::NotLoop) && index < int(Condition::Last)) {
            index -= int(Condition::NotLoop);
            return { offset[index], base[index], 1 };
        } else {
            return { 0, 0, 0 };
        }
    }

    static void printCondition(StringBuilder &str, Condition condition, ConditionFormat format = ConditionFormat::Long) {
        int index = int(condition);
        if (index >= 0 && index < int(Condition::Loop)) {
            const auto &info = conditionInfos[index];
            switch (format) {
            case ConditionFormat::Long: str(info.name); break;
            case ConditionFormat::Short1: str(info.short1); break;
            case ConditionFormat::Short2: str(info.short2); break;
            }
        } else if (index >= int(Condition::Loop) && index < int(Condition::Last)) {
            auto loop = conditionLoop(condition);
            switch (format) {
            case ConditionFormat::Long: str("%s%d:%d", loop.invert ? "!" : "", loop.offset + 1, loop.base); break;
            case ConditionFormat::Short1: str("%s%d", loop.invert ? "!" : "", loop.offset + 1); break;
            case ConditionFormat::Short2: str("%d", loop.base); break;
            }
        }
    }

    // VoltageRange

    enum class VoltageRange : uint8_t {
        Unipolar1V,
        Unipolar2V,
        Unipolar3V,
        Unipolar4V,
        Unipolar5V,
        Bipolar1V,
        Bipolar2V,
        Bipolar3V,
        Bipolar4V,
        Bipolar5V,
        Last
    };

    static const char *voltageRangeName(VoltageRange voltageRange) {
        switch (voltageRange) {
        case VoltageRange::Unipolar1V:  return TXT_LIST_LABEL_1V_UNIPOLAR;
        case VoltageRange::Unipolar2V:  return TXT_LIST_LABEL_2V_UNIPOLAR;
        case VoltageRange::Unipolar3V:  return TXT_LIST_LABEL_3V_UNIPOLAR;
        case VoltageRange::Unipolar4V:  return TXT_LIST_LABEL_4V_UNIPOLAR;
        case VoltageRange::Unipolar5V:  return TXT_LIST_LABEL_5V_UNIPOLAR;
        case VoltageRange::Bipolar1V:   return TXT_LIST_LABEL_1V_BIPOLAR;
        case VoltageRange::Bipolar2V:   return TXT_LIST_LABEL_2V_BIPOLAR;
        case VoltageRange::Bipolar3V:   return TXT_LIST_LABEL_3V_BIPOLAR;
        case VoltageRange::Bipolar4V:   return TXT_LIST_LABEL_4V_BIPOLAR;
        case VoltageRange::Bipolar5V:   return TXT_LIST_LABEL_5V_BIPOLAR;
        case VoltageRange::Last:        break;
        }
        return nullptr;
    }

    struct VoltageRangeInfo {
        float lo;
        float hi;

        float normalize(float value) const {
            return clamp((value - lo) / (hi - lo), 0.f, 1.f);
        }

        float denormalize(float value) const {
            return clamp(value, 0.f, 1.f) * (hi - lo) + lo;
        }
    };

    static const VoltageRangeInfo &voltageRangeInfo(VoltageRange voltageRange) {
        return voltageRangeInfos[int(voltageRange)];
    }

    // MidiPort

    enum class MidiPort : uint8_t {
        Midi,
        UsbMidi,
        Last
    };

    static const char *midiPortName(MidiPort midiPort) {
        switch (midiPort) {
        case MidiPort::Midi:    return TXT_LIST_LABEL_MIDI;
        case MidiPort::UsbMidi: return TXT_LIST_LABEL_USB;
        case MidiPort::Last:    break;
        }
        return nullptr;
    }

    // Misc types

    struct LayerRange {
        int min;
        int max;
    };

    // Utilities
    // TODO maybe move these

    static void printMidiChannel(StringBuilder &str, int midiChannel) {
        if (midiChannel == -1) {
            str(TXT_LIST_LABEL_OMNI);
        } else {
            str(TXT_MODEL_GENERIC_VALUE, midiChannel + 1);
        }
    }

    static void printNote(StringBuilder &str, int note) {
        static const char *names[] = TXT_NOTE_NAMES;
        str(names[note]);
    }

    static void printMidiNote(StringBuilder &str, int midiNote) {
        printNote(str, midiNote % 12);
        int octave = midiNote / 12 - 1;
        str(TXT_MODEL_GENERIC_VALUE, octave);
    }

private:
    static const ConditionInfo conditionInfos[];
    static const VoltageRangeInfo voltageRangeInfos[];

}; // namespace Types
