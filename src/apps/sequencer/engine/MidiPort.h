#pragma once

#include "Config.h"
#include <cstdint>

enum class MidiPort : uint8_t {
    Midi,
    UsbMidi,
    CvGate,
};

static const char *midiPortName(MidiPort port) {
    switch (port) {
    case MidiPort::Midi:    return TXT_LIST_LABEL_MIDI;
    case MidiPort::UsbMidi: return TXT_LIST_LABEL_USB;
    case MidiPort::CvGate:  return TXT_LIST_LABEL_CV_GATE;
    }
    return nullptr;
}
