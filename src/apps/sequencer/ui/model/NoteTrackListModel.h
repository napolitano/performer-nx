#pragma once

#include "Config.h"

#include "RoutableListModel.h"

#include "model/NoteTrack.h"

class NoteTrackListModel : public RoutableListModel {
public:
    void setTrack(NoteTrack &track) {
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
        case Octave:
            return Routing::Target::Octave;
        case Transpose:
            return Routing::Target::Transpose;
        case Rotate:
            return Routing::Target::Rotate;
        case GateProbabilityBias:
            return Routing::Target::GateProbabilityBias;
        case RetriggerProbabilityBias:
            return Routing::Target::RetriggerProbabilityBias;
        case LengthBias:
            return Routing::Target::LengthBias;
        case NoteProbabilityBias:
            return Routing::Target::NoteProbabilityBias;
        default:
            return Routing::Target::None;
        }
    }

private:
    enum Item {
        PlayMode,
        FillMode,
        FillMuted,
        CvUpdateMode,
        SlideTime,
        Octave,
        Transpose,
        Rotate,
        GateProbabilityBias,
        RetriggerProbabilityBias,
        LengthBias,
        NoteProbabilityBias,
        Last
    };

    static const char *itemName(Item item) {
        switch (item) {
        case PlayMode:  return TXT_LIST_LABEL_PLAY_MODE;
        case FillMode:  return TXT_LIST_LABEL_FILL_MODE;
        case FillMuted: return TXT_LIST_LABEL_FILL_MUTED;
        case CvUpdateMode:  return TXT_LIST_LABEL_CV_UPDATE_MODE;
        case SlideTime: return TXT_LIST_LABEL_SLIDE_TIME;
        case Octave:    return TXT_LIST_LABEL_OCTAVE;
        case Transpose: return TXT_LIST_LABEL_TRANSPOSE;
        case Rotate:    return TXT_LIST_LABEL_ROTATE;
        case GateProbabilityBias: return TXT_LIST_LABEL_GATE_PROBABILITY_BIAS;
        case RetriggerProbabilityBias: return TXT_LIST_LABEL_GATE_RETRIGGER_PROBABILITY_BIAS;
        case LengthBias: return TXT_LIST_LABEL_GATE_LENGTH_BIAS;
        case NoteProbabilityBias: return TXT_LIST_LABEL_GATE_NOTE_PROBABILITY_BIAS;
        case Last:      break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, StringBuilder &str) const {
        switch (item) {
        case PlayMode:
            _track->printPlayMode(str);
            break;
        case FillMode:
            _track->printFillMode(str);
            break;
        case FillMuted:
            _track->printFillMuted(str);
            break;
        case CvUpdateMode:
            _track->printCvUpdateMode(str);
            break;
        case SlideTime:
            _track->printSlideTime(str);
            break;
        case Octave:
            _track->printOctave(str);
            break;
        case Transpose:
            _track->printTranspose(str);
            break;
        case Rotate:
            _track->printRotate(str);
            break;
        case GateProbabilityBias:
            _track->printGateProbabilityBias(str);
            break;
        case RetriggerProbabilityBias:
            _track->printRetriggerProbabilityBias(str);
            break;
        case LengthBias:
            _track->printLengthBias(str);
            break;
        case NoteProbabilityBias:
            _track->printNoteProbabilityBias(str);
            break;
        case Last:
            break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        switch (item) {
        case PlayMode:
            _track->editPlayMode(value, shift);
            break;
        case FillMode:
            _track->editFillMode(value, shift);
            break;
        case FillMuted:
            _track->editFillMuted(value, shift);
            break;
        case CvUpdateMode:
            _track->editCvUpdateMode(value, shift);
            break;
        case SlideTime:
            _track->editSlideTime(value, shift);
            break;
        case Octave:
            _track->editOctave(value, shift);
            break;
        case Transpose:
            _track->editTranspose(value, shift);
            break;
        case Rotate:
            _track->editRotate(value, shift);
            break;
        case GateProbabilityBias:
            _track->editGateProbabilityBias(value, shift);
            break;
        case RetriggerProbabilityBias:
            _track->editRetriggerProbabilityBias(value, shift);
            break;
        case LengthBias:
            _track->editLengthBias(value, shift);
            break;
        case NoteProbabilityBias:
            _track->editNoteProbabilityBias(value, shift);
            break;
        case Last:
            break;
        }
    }

    NoteTrack *_track;
};
