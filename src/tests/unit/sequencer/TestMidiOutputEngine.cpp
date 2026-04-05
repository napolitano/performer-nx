#include "UnitTest.h"

#include "apps/sequencer/SequencerApp.h"

#include "sim/Simulator.h"

#include <memory>
#include <vector>

namespace {

class MidiRecorder : public sim::TargetOutputHandler {
public:
    void writeMidiOutput(sim::MidiEvent event) override {
        _events.push_back(event);
    }

    void clear() {
        _events.clear();
    }

    int size() const {
        return int(_events.size());
    }

    const sim::MidiEvent &event(int index) const {
        return _events[index];
    }

private:
    std::vector<sim::MidiEvent> _events;
};

class SequencerHarness {
public:
    SequencerHarness() :
        _simulator(makeTarget())
    {
        _simulator.registerTargetOutputObserver(&_midiRecorder);
        _simulator.reboot();
        _midiRecorder.clear();
    }

    SequencerApp &app() { return *_app; }
    MidiRecorder &midiRecorder() { return _midiRecorder; }

    void waitMs(int ms) {
        _simulator.wait(ms);
    }

private:
    sim::Target makeTarget() {
        sim::Target target;
        target.create = [this] () {
            _app.reset(new SequencerApp());
        };
        target.destroy = [this] () {
            _app.reset();
        };
        target.update = [this] () {
            _app->update();
        };
        return target;
    }

    std::unique_ptr<SequencerApp> _app;
    MidiRecorder _midiRecorder;
    sim::Simulator _simulator;
};

static MidiOutput::Output &configureNoteOutput(SequencerApp &app, Types::MidiPort port = Types::MidiPort::UsbMidi, int channel = 2) {
    auto &output = app.model.project().midiOutput().output(0);
    output.clear();
    output.target().setPort(port);
    output.target().setChannel(channel);
    output.setEvent(MidiOutput::Output::Event::Note);
    output.setGateSource(MidiOutput::Output::GateSource::FirstTrack);
    output.setNoteSource(MidiOutput::Output::NoteSource::FirstTrack);
    output.setVelocitySource(MidiOutput::Output::VelocitySource(int(MidiOutput::Output::VelocitySource::FirstVelocity) + 96));
    return output;
}

static MidiOutput::Output &configureFixedNoteOutput(SequencerApp &app, Types::MidiPort port = Types::MidiPort::Midi, int channel = 1) {
    auto &output = app.model.project().midiOutput().output(0);
    output.clear();
    output.target().setPort(port);
    output.target().setChannel(channel);
    output.setEvent(MidiOutput::Output::Event::Note);
    output.setGateSource(MidiOutput::Output::GateSource::FirstTrack);
    output.setNoteSource(MidiOutput::Output::NoteSource(int(MidiOutput::Output::NoteSource::FirstNote) + 64));
    output.setVelocitySource(MidiOutput::Output::VelocitySource(int(MidiOutput::Output::VelocitySource::FirstVelocity) + 127));
    return output;
}

static MidiOutput::Output &configureControlOutput(SequencerApp &app, Types::MidiPort port = Types::MidiPort::UsbMidi, int channel = 3) {
    auto &output = app.model.project().midiOutput().output(0);
    output.clear();
    output.target().setPort(port);
    output.target().setChannel(channel);
    output.setEvent(MidiOutput::Output::Event::ControlChange);
    output.setControlNumber(74);
    output.setControlSource(MidiOutput::Output::ControlSource::FirstTrack);
    return output;
}

static void expectMessagePort(const sim::MidiEvent &event, int port) {
    expectEqual(event.kind, int(sim::MidiEvent::Message));
    expectEqual(event.port, port);
}

static void expectNoteOn(const sim::MidiEvent &event, int port, int channel, int note, int velocity) {
    expectMessagePort(event, port);
    expectTrue(event.message.isNoteOn());
    expectEqual(int(event.message.channel()), channel);
    expectEqual(int(event.message.note()), note);
    expectEqual(int(event.message.velocity()), velocity);
}

static void expectNoteOff(const sim::MidiEvent &event, int port, int channel, int note) {
    expectMessagePort(event, port);
    expectTrue(event.message.isNoteOff());
    expectEqual(int(event.message.channel()), channel);
    expectEqual(int(event.message.note()), note);
}

static void expectControlChange(const sim::MidiEvent &event, int port, int channel, int controlNumber, int controlValue) {
    expectMessagePort(event, port);
    expectTrue(event.message.isControlChange());
    expectEqual(int(event.message.channel()), channel);
    expectEqual(int(event.message.controlNumber()), controlNumber);
    expectEqual(int(event.message.controlValue()), controlValue);
}

} // namespace

UNIT_TEST("MidiOutputEngine") {

    CASE("fixed note and velocity sources produce note on and note off messages") {
        SequencerHarness harness;
        auto &midiOutputEngine = harness.app().engine.midiOutputEngine();
        configureFixedNoteOutput(harness.app());
        midiOutputEngine.update();
        harness.midiRecorder().clear();

        midiOutputEngine.sendGate(0, true);
        midiOutputEngine.update();

        expectEqual(harness.midiRecorder().size(), 1);
        expectNoteOn(harness.midiRecorder().event(0), 0, 1, 64, 127);

        harness.midiRecorder().clear();
        midiOutputEngine.sendGate(0, false);
        midiOutputEngine.update();

        expectEqual(harness.midiRecorder().size(), 1);
        expectNoteOff(harness.midiRecorder().event(0), 0, 1, 64);
    }

    CASE("track-driven note output suppresses tied retriggers and replaces active notes when pitch changes") {
        SequencerHarness harness;
        auto &midiOutputEngine = harness.app().engine.midiOutputEngine();
        configureNoteOutput(harness.app());
        midiOutputEngine.update();
        harness.midiRecorder().clear();

        midiOutputEngine.sendCv(0, 1.f);
        midiOutputEngine.sendGate(0, true);
        midiOutputEngine.update();

        expectEqual(harness.midiRecorder().size(), 1);
        expectNoteOn(harness.midiRecorder().event(0), 1, 2, 72, 96);

        harness.midiRecorder().clear();
        midiOutputEngine.sendGate(0, false);
        midiOutputEngine.sendGate(0, true);
        midiOutputEngine.update();
        expectEqual(harness.midiRecorder().size(), 0);

        midiOutputEngine.sendCv(0, 2.f);
        midiOutputEngine.sendGate(0, true);
        midiOutputEngine.update();

        expectEqual(harness.midiRecorder().size(), 2);
        expectNoteOn(harness.midiRecorder().event(0), 1, 2, 84, 96);
        expectNoteOff(harness.midiRecorder().event(1), 1, 2, 72);
    }

    CASE("slide requests emit portamento control changes only when the state changes") {
        SequencerHarness harness;
        auto &midiOutputEngine = harness.app().engine.midiOutputEngine();
        configureNoteOutput(harness.app());
        midiOutputEngine.update();
        harness.midiRecorder().clear();

        midiOutputEngine.sendSlide(0, true);
        midiOutputEngine.update();
        expectEqual(harness.midiRecorder().size(), 1);
        expectControlChange(harness.midiRecorder().event(0), 1, 2, 65, 127);

        harness.midiRecorder().clear();
        midiOutputEngine.sendSlide(0, true);
        midiOutputEngine.update();
        expectEqual(harness.midiRecorder().size(), 0);

        midiOutputEngine.sendSlide(0, false);
        midiOutputEngine.update();
        expectEqual(harness.midiRecorder().size(), 1);
        expectControlChange(harness.midiRecorder().event(0), 1, 2, 65, 0);
    }

    CASE("control change output does not send immediately without force") {
        SequencerHarness harness;
        auto &midiOutputEngine = harness.app().engine.midiOutputEngine();
        configureControlOutput(harness.app());
        midiOutputEngine.update();
        harness.midiRecorder().clear();

        midiOutputEngine.sendCv(0, 2.5f);
        midiOutputEngine.update();
        expectEqual(harness.midiRecorder().size(), 0);
    }

    CASE("forced control change flush sends changed values and ignores unchanged values") {
        SequencerHarness harness;
        auto &midiOutputEngine = harness.app().engine.midiOutputEngine();
        configureControlOutput(harness.app());
        midiOutputEngine.update();
        harness.midiRecorder().clear();

        midiOutputEngine.sendCv(0, 2.5f);
        midiOutputEngine.update(true);
        expectEqual(harness.midiRecorder().size(), 1);
        expectControlChange(harness.midiRecorder().event(0), 1, 3, 74, 63);

        harness.midiRecorder().clear();
        midiOutputEngine.sendCv(0, 2.5f);
        midiOutputEngine.update(true);
        expectEqual(harness.midiRecorder().size(), 0);

        midiOutputEngine.sendCv(0, 9.f);
        midiOutputEngine.update(true);
        expectEqual(harness.midiRecorder().size(), 1);
        expectControlChange(harness.midiRecorder().event(0), 1, 3, 74, 127);
    }

    CASE("reset sends note cleanup for active note outputs") {
        SequencerHarness harness;
        auto &midiOutputEngine = harness.app().engine.midiOutputEngine();
        configureNoteOutput(harness.app(), Types::MidiPort::UsbMidi, 4);
        midiOutputEngine.update();
        harness.midiRecorder().clear();

        midiOutputEngine.sendCv(0, 1.f);
        midiOutputEngine.sendSlide(0, true);
        midiOutputEngine.sendGate(0, true);
        midiOutputEngine.update();
        harness.midiRecorder().clear();

        midiOutputEngine.reset();

        expectEqual(harness.midiRecorder().size(), 3);
        expectNoteOff(harness.midiRecorder().event(0), 1, 4, 72);
        expectControlChange(harness.midiRecorder().event(1), 1, 4, 65, 0);
        expectControlChange(harness.midiRecorder().event(2), 1, 4, 120, 0);
    }
}





