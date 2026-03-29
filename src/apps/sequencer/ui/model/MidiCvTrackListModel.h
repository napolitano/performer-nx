#pragma once

#include "Config.h"

#include "RoutableListModel.h"

#include "model/MidiCvTrack.h"

class MidiCvTrackListModel : public RoutableListModel {
public:
    void setTrack(MidiCvTrack &track) {
        _track = &track;
    }

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

    virtual Routing::Target routingTarget(int row) const override {
        switch (Item(row)) {
        case SlideTime:
            return Routing::Target::SlideTime;
        case Transpose:
            return Routing::Target::Transpose;
        default:
            return Routing::Target::None;
        }
    }

private:
    enum Item {
        Source,
        Voices,
        VoiceConfig,
        NotePriority,
        LowNote,
        HighNote,
        PitchBendRange,
        ModulationRange,
        Retrigger,
        SlideTime,
        Transpose,
        ArpeggiatorEnabled,
        ArpeggiatorHold,
        ArpeggiatorMode,
        ArpeggiatorDivisor,
        ArpeggiatorGateLength,
        ArpeggiatorOctaves,
        Last
    };

    static const char *itemName(Item item) {
        switch (item) {
        case Source:                return TXT_LIST_LABEL_SOURCE;
        case Voices:                return TXT_LIST_LABEL_VOICES;
        case VoiceConfig:           return TXT_LIST_LABEL_VOICE_CONFIG;
        case NotePriority:          return TXT_LIST_LABEL_NOTE_PRIORITY;
        case LowNote:               return TXT_LIST_LABEL_LOW_NOTE;
        case HighNote:              return TXT_LIST_LABEL_HIGH_NOTE;
        case PitchBendRange:        return TXT_LIST_LABEL_PITCH_BEND;
        case ModulationRange:       return TXT_LIST_LABEL_MOD_RANGE;
        case Retrigger:             return TXT_LIST_LABEL_RETRIGGER;
        case SlideTime:             return TXT_LIST_LABEL_SLIDE_TIME;
        case Transpose:             return TXT_LIST_LABEL_TRANSPOSE;
        case ArpeggiatorEnabled:    return TXT_LIST_LABEL_ARPEGGIATOR;
        case ArpeggiatorHold:       return TXT_LIST_LABEL_HOLD;
        case ArpeggiatorMode:       return TXT_LIST_LABEL_MODE;
        case ArpeggiatorDivisor:    return TXT_LIST_LABEL_DIVISOR;
        case ArpeggiatorGateLength: return TXT_LIST_LABEL_GATE_LENGTH;
        case ArpeggiatorOctaves:    return TXT_LIST_LABEL_OCTAVE_PLURAL;
        case Last:                  break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, StringBuilder &str) const {
        const auto &arpeggiator = _track->arpeggiator();

        switch (item) {
        case Source:
            _track->source().print(str);
            break;
        case Voices:
            _track->printVoices(str);
            break;
        case VoiceConfig:
            _track->printVoiceConfig(str);
            break;
        case NotePriority:
            _track->printNotePriority(str);
            break;
        case LowNote:
            _track->printLowNote(str);
            break;
        case HighNote:
            _track->printHighNote(str);
            break;
        case PitchBendRange:
            _track->printPitchBendRange(str);
            break;
        case ModulationRange:
            _track->printModulationRange(str);
            break;
        case Retrigger:
            _track->printRetrigger(str);
            break;
        case SlideTime:
            _track->printSlideTime(str);
            break;
        case Transpose:
            _track->printTranspose(str);
            break;
        case ArpeggiatorEnabled:
            arpeggiator.printEnabled(str);
            break;
        case ArpeggiatorHold:
            arpeggiator.printHold(str);
            break;
        case ArpeggiatorMode:
            arpeggiator.printMode(str);
            break;
        case ArpeggiatorDivisor:
            arpeggiator.printDivisor(str);
            break;
        case ArpeggiatorGateLength:
            arpeggiator.printGateLength(str);
            break;
        case ArpeggiatorOctaves:
            arpeggiator.printOctaves(str);
            break;
        case Last:
            break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        auto &arpeggiator = _track->arpeggiator();

        switch (item) {
        case Source:
            _track->source().edit(value, shift);
            break;
        case Voices:
            _track->editVoices(value, shift);
            break;
        case VoiceConfig:
            _track->editVoiceConfig(value, shift);
            break;
        case NotePriority:
            _track->editNotePriority(value, shift);
            break;
        case LowNote:
            _track->editLowNote(value, shift);
            break;
        case HighNote:
            _track->editHighNote(value, shift);
            break;
        case PitchBendRange:
            _track->editPitchBendRange(value, shift);
            break;
        case ModulationRange:
            _track->editModulationRange(value, shift);
            break;
        case Retrigger:
            _track->editRetrigger(value, shift);
            break;
        case SlideTime:
            _track->editSlideTime(value, shift);
            break;
        case Transpose:
            _track->editTranspose(value, shift);
            break;
        case ArpeggiatorEnabled:
            arpeggiator.editEnabled(value, shift);
            break;
        case ArpeggiatorHold:
            arpeggiator.editHold(value, shift);
            break;
        case ArpeggiatorMode:
            arpeggiator.editMode(value, shift);
            break;
        case ArpeggiatorDivisor:
            arpeggiator.editDivisor(value, shift);
            break;
        case ArpeggiatorGateLength:
            arpeggiator.editGateLength(value, shift);
            break;
        case ArpeggiatorOctaves:
            arpeggiator.editOctaves(value, shift);
            break;
        case Last:
            break;
        }
    }

    MidiCvTrack *_track;
};
