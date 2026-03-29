#include "Config.h"
#include "MonitorPage.h"

#include "ui/painters/WindowPainter.h"

#include "engine/CvInput.h"
#include "engine/CvOutput.h"

#include "core/utils/StringBuilder.h"

enum class Function {
    CvIn    = 0,
    CvOut   = 1,
    Midi    = 2,
    Stats   = 3,
};

static const char *functionNames[] = {
    TXT_FUNCTION_MONITOR_CV_IN,
    TXT_FUNCTION_MONITOR_CV_OUT,
    TXT_FUNCTION_MONITOR_MIDI,
    TXT_FUNCTION_MONITOR_STATS,
    nullptr
};

static void formatMidiMessage(StringBuilder &eventStr, StringBuilder &dataStr, const MidiMessage &msg) {
    if (msg.isChannelMessage()) {
        int channel = msg.channel() + 1;
        switch (msg.channelMessage()) {
        case MidiMessage::NoteOff:
            eventStr(TXT_EVENT_NOTE_OFF);
            dataStr(TXT_INFO_MONITOR_DATA_NOTE_OFF, channel, msg.note(), msg.velocity());
            return;
        case MidiMessage::NoteOn:
            eventStr(TXT_EVENT_NOTE_ON);
            dataStr(TXT_INFO_MONITOR_DATA_NOTE_ON, channel, msg.note(), msg.velocity());
            return;
        case MidiMessage::KeyPressure:
            eventStr(TXT_EVENT_KEY_PRESSURE);
            dataStr(TXT_INFO_MONITOR_DATA_KEY_PRESSURE, channel, msg.note(), msg.keyPressure());
            return;
        case MidiMessage::ControlChange:
            eventStr(TXT_EVENT_CONTROL_CHANGE);
            dataStr(TXT_INFO_MONITOR_DATA_CONTROL_CHANGE, channel, msg.controlNumber(), msg.controlValue());
            return;
        case MidiMessage::ProgramChange:
            eventStr(TXT_EVENT_PROGRAM_CHANGE);
            dataStr(TXT_INFO_MONITOR_DATA_PROGRAM_CHANGE, channel, msg.programNumber());
            return;
        case MidiMessage::ChannelPressure:
            eventStr(TXT_EVENT_CHANNEL_PRESSURE);
            dataStr(TXT_INFO_MONITOR_DATA_CHANNEL_PRESSURE, channel, msg.channelPressure());
            return;
        case MidiMessage::PitchBend:
            eventStr(TXT_EVENT_PITCH_BEND);
            dataStr(TXT_INFO_MONITOR_DATA_PITCH_BEND, channel, msg.pitchBend());
            return;
        }
    } else if (msg.isSystemMessage()) {
        switch (msg.systemMessage()) {
        case MidiMessage::SystemExclusive:
            eventStr(TXT_EVENT_SYSEX);
            return;
        case MidiMessage::TimeCode:
            eventStr(TXT_EVENT_TIME_CODE);
            dataStr(TXT_INFO_MONITOR_DATA_TIME_CODE, msg.data0());
            return;
        case MidiMessage::SongPosition:
            eventStr(TXT_EVENT_SONG_POSITION);
            dataStr(TXT_INFO_MONITOR_DATA_SONG_POSITION, msg.songPosition());
            return;
        case MidiMessage::SongSelect:
            eventStr(TXT_EVENT_SONG_SELECT);
            dataStr(TXT_INFO_MONITOR_DATA_SONG_NUMBER, msg.songNumber());
            return;
        case MidiMessage::TuneRequest:
            eventStr(TXT_EVENT_TUNE_REQUEST);
            return;
        default: break;
        }
    }
}

MonitorPage::MonitorPage(PageManager &manager, PageContext &context) :
    BasePage(manager, context)
{}

void MonitorPage::enter() {
}

void MonitorPage::exit() {
}

void MonitorPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, TXT_MODE_MONITOR);
    WindowPainter::drawActiveFunction(canvas, functionNames[int(_mode)]);
    WindowPainter::drawFooter(canvas, functionNames, pageKeyState(), int(_mode));

    canvas.setBlendMode(BlendMode::Set);
    canvas.setFont(Font::Tiny);
    canvas.setColor(UI_COLOR_ACTIVE);

    switch (_mode) {
    case Mode::CvIn:
        drawCvIn(canvas);
        break;
    case Mode::CvOut:
        drawCvOut(canvas);
        break;
    case Mode::Midi:
        drawMidi(canvas);
        break;
    case Mode::Stats:
        drawStats(canvas);
        break;
    }
}

void MonitorPage::updateLeds(Leds &leds) {
}

void MonitorPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.pageModifier()) {
        return;
    }

    if (key.isFunction()) {
        switch (Function(key.function())) {
        case Function::CvIn:
            _mode = Mode::CvIn;
            break;
        case Function::CvOut:
            _mode = Mode::CvOut;
            break;
        case Function::Midi:
            _mode = Mode::Midi;
            break;
        case Function::Stats:
            _mode = Mode::Stats;
            break;
        }
    }
}

void MonitorPage::encoder(EncoderEvent &event) {
}

void MonitorPage::midi(MidiEvent &event) {
    _lastMidiMessage = event.message();
    _lastMidiMessagePort = event.port();
    _lastMidiMessageTicks = os::ticks();
}

void MonitorPage::drawCvIn(Canvas &canvas) {
    FixedStringBuilder<16> str;

    int w = Width / 4;
    int h = 8;

    for (size_t i = 0; i < CvInput::Channels; ++i) {
        int x = i * w;
        int y = 32;

        str.reset();
        str(TXT_INFO_CV_VALUE, i + 1);
        canvas.drawTextCentered(x, y - h, w, h, str);

        str.reset();
        str(TXT_INFO_VOLTAGE, _engine.cvInput().channel(i));
        canvas.drawTextCentered(x, y, w, h, str);
    }
}

void MonitorPage::drawCvOut(Canvas &canvas) {
    FixedStringBuilder<16> str;

    int w = Width / 4;
    int h = 8;

    for (size_t i = 0; i < CvOutput::Channels; ++i) {
        int x = (i % 4) * w;
        int y = 20 + (i / 4) * 20;

        str.reset();
        str(TXT_INFO_CV_VALUE, i + 1);
        canvas.drawTextCentered(x, y - h, w, h, str);

        str.reset();
        str(TXT_INFO_VOLTAGE, _engine.cvOutput().channel(i));
        canvas.drawTextCentered(x, y, w, h, str);
    }
}

void MonitorPage::drawMidi(Canvas &canvas) {

    if (os::ticks() - _lastMidiMessageTicks < os::time::ms(1000)) {
        FixedStringBuilder<32> eventStr;
        FixedStringBuilder<32> dataStr;
        formatMidiMessage(eventStr, dataStr, _lastMidiMessage);
        canvas.drawTextCentered(0, 24 - 8, Width, 16, midiPortName(_lastMidiMessagePort));
        canvas.drawTextCentered(0, 32 - 8, Width, 16, eventStr);
        canvas.drawTextCentered(0, 40 - 8, Width, 16, dataStr);
    }
}

void MonitorPage::drawStats(Canvas &canvas) {
    auto stats = _engine.stats();

    auto drawValue = [&] (int index, const char *name, const char *value) {
        canvas.drawText(10, 20 + index * 10, name);
        canvas.drawText(100, 20 + index * 10, value);
    };

    {
        int seconds = stats.uptime;
        int minutes = seconds / 60;
        int hours = minutes / 60;
        FixedStringBuilder<16> str(TXT_INFO_MONITOR_DATA_UPTIME, hours, minutes % 60, seconds % 60);
        drawValue(0, TXT_STAT_UPTIME, str);
    }

    {
        FixedStringBuilder<16> str(TXT_INFO_MONITOR_DATA_MIDI_OVF, stats.midiRxOverflow);
        drawValue(1, TXT_STAT_MIDI_OVF, str);
    }

    {
        FixedStringBuilder<16> str(TXT_INFO_MONITOR_DATA_USBMIDI_OVF, stats.usbMidiRxOverflow);
        drawValue(2, TXT_STAT_USBMIDI_OVF, str);
    }

}
