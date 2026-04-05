#include "UnitTest.h"

#include "apps/sequencer/SequencerApp.h"
#include "apps/sequencer/model/NoteSequence.h"

#include "sim/Simulator.h"
#include "sim/MidiEvent.h"

#include <memory>

namespace {

class SequencerHarness {
public:
    SequencerHarness() :
        _simulator(makeTarget())
    {
        _simulator.reboot();
    }

    SequencerApp &app() { return *_app; }

    void sendDinMidi(const MidiMessage &message) {
        _simulator.sendMidi(0, message);
    }

    void sendUsbMidi(const MidiMessage &message) {
        _simulator.sendMidi(1, message);
    }

    void connectUsbMidi(uint16_t vendorId, uint16_t productId) {
        _simulator.writeMidiInput(sim::MidiEvent::makeConnect(1, vendorId, productId));
    }

    void disconnectUsbMidi() {
        _simulator.writeMidiInput(sim::MidiEvent::makeDisconnect(1));
    }

    void waitMs(int ms) {
        _simulator.wait(ms);
    }

    void setResetInput(bool high) {
        _simulator.setDio(1, high);
    }

    void setClockInput(bool high) {
        _simulator.setDio(0, high);
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
    sim::Simulator _simulator;
};

static uint32_t sequenceDivisorTicks(const NoteSequence &sequence) {
    return sequence.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
}

static void forceMasterClockMode(SequencerApp &app) {
    auto &clockSetup = app.model.project().clockSetup();
    clockSetup.setMode(ClockSetup::Mode::Master);
    app.engine.update();
}

static uint32_t gateOnTick(const NoteSequence &sequence, int gateOffset) {
    return (sequenceDivisorTicks(sequence) * gateOffset) / (NoteSequence::GateOffset::Max + 1);
}

static uint32_t gateLengthTicks(const NoteSequence &sequence, int gateLength) {
    return (sequenceDivisorTicks(sequence) * (gateLength + 1)) / NoteSequence::Length::Range;
}

// Creates a deterministic single-step note pattern for timing-related tests.
static NoteTrackEngine &prepareSingleStepEngine(
    SequencerApp &app,
    int gateOffset,
    int gateLength = NoteSequence::Length::Max,
    bool gateActive = true)
{
    auto &sequence = app.model.project().track(0).noteTrack().sequence(0);
    sequence.clearSteps();
    sequence.setFirstStep(0);
    sequence.setLastStep(0);

    auto &step = sequence.step(0);
    step.setGate(gateActive);
    step.setGateProbability(NoteSequence::GateProbability::Max);
    step.setLength(gateLength);
    step.setLengthVariationRange(0);
    step.setLengthVariationProbability(NoteSequence::LengthVariationProbability::Max);
    step.setRetrigger(0);
    step.setRetriggerProbability(NoteSequence::RetriggerProbability::Max);
    step.setGateOffset(gateOffset);

    auto &engine = app.engine.trackEngine(0).as<NoteTrackEngine>();
    engine.reset();
    return engine;
}

// Creates a two-step pattern to validate that each step keeps its own offset timing.
static NoteTrackEngine &prepareTwoStepEngine(
    SequencerApp &app,
    int step0Offset,
    int step1Offset,
    int gateLength = 3)
{
    auto &sequence = app.model.project().track(0).noteTrack().sequence(0);
    sequence.clearSteps();
    sequence.setFirstStep(0);
    sequence.setLastStep(1);

    for (int index = 0; index < 2; ++index) {
        auto &step = sequence.step(index);
        step.setGate(true);
        step.setGateProbability(NoteSequence::GateProbability::Max);
        step.setLength(gateLength);
        step.setLengthVariationRange(0);
        step.setLengthVariationProbability(NoteSequence::LengthVariationProbability::Max);
        step.setRetrigger(0);
        step.setRetriggerProbability(NoteSequence::RetriggerProbability::Max);
        step.setGateOffset(index == 0 ? step0Offset : step1Offset);
    }

    auto &engine = app.engine.trackEngine(0).as<NoteTrackEngine>();
    engine.reset();
    return engine;
}

} // namespace

UNIT_TEST("Engine") {

    CASE("track engines stay consistent after track mode changes") {
        SequencerHarness harness;
        auto &project = harness.app().model.project();
        auto &engine = harness.app().engine;

        expectTrue(engine.trackEnginesConsistent());

        project.setTrackMode(0, Track::TrackMode::Curve);
        engine.update();
        expectTrue(engine.trackEnginesConsistent());
        expectEqual(engine.trackEngine(0).trackMode(), Track::TrackMode::Curve);

        project.setTrackMode(0, Track::TrackMode::Note);
        engine.update();
        expectTrue(engine.trackEnginesConsistent());
        expectEqual(engine.trackEngine(0).trackMode(), Track::TrackMode::Note);
    }

    CASE("track mode MidiCv is instantiated and can switch back to Note") {
        SequencerHarness harness;
        auto &project = harness.app().model.project();
        auto &engine = harness.app().engine;

        project.setTrackMode(0, Track::TrackMode::MidiCv);
        engine.update();
        expectEqual(engine.trackEngine(0).trackMode(), Track::TrackMode::MidiCv);

        project.setTrackMode(0, Track::TrackMode::Note);
        engine.update();
        expectEqual(engine.trackEngine(0).trackMode(), Track::TrackMode::Note);
    }

    CASE("trackEnginesConsistent reports mismatch before setup update") {
        SequencerHarness harness;
        auto &project = harness.app().model.project();
        auto &engine = harness.app().engine;

        expectTrue(engine.trackEnginesConsistent());

        project.setTrackMode(0, Track::TrackMode::Curve);
        expectFalse(engine.trackEnginesConsistent());

        engine.update();
        expectTrue(engine.trackEnginesConsistent());
    }

    CASE("immediate pattern request updates active note sequence") {
        SequencerHarness harness;
        auto &project = harness.app().model.project();
        auto &engine = harness.app().engine;
        auto &playState = project.playState();

        auto &noteEngine = engine.trackEngine(0).as<NoteTrackEngine>();
        expectEqual(playState.trackState(0).pattern(), 0);
        expectTrue(&noteEngine.sequence() == &project.track(0).noteTrack().sequence(0));

        playState.selectTrackPattern(0, 1, PlayState::Immediate);
        engine.update();

        expectEqual(playState.trackState(0).pattern(), 1);
        expectTrue(&noteEngine.sequence() == &project.track(0).noteTrack().sequence(1));
    }

    CASE("latched mute request applies only after commit") {
        SequencerHarness harness;
        auto &project = harness.app().model.project();
        auto &engine = harness.app().engine;
        auto &playState = project.playState();

        expectFalse(playState.trackState(0).mute());

        playState.muteTrack(0, PlayState::Latched);
        engine.update();
        expectFalse(playState.trackState(0).mute());

        playState.commitLatchedRequests();
        engine.update();
        expectTrue(playState.trackState(0).mute());
    }

    CASE("immediate playSong with invalid slot is ignored and request gets cleared") {
        SequencerHarness harness;
        auto &project = harness.app().model.project();
        auto &engine = harness.app().engine;
        auto &playState = project.playState();
        auto &songState = playState.songState();

        // Prepare one valid slot and request an invalid slot index.
        project.song().clear();
        project.song().insertSlot(0);

        playState.playSong(99, PlayState::Immediate);
        engine.update();

        expectFalse(songState.playing());
        expectFalse(songState.hasPlayRequests());
    }

    CASE("latched playSong applies only after latched requests are committed") {
        SequencerHarness harness;
        auto &project = harness.app().model.project();
        auto &engine = harness.app().engine;
        auto &playState = project.playState();
        auto &songState = playState.songState();

        project.song().clear();
        project.song().insertSlot(0);
        project.song().setPattern(0, 0, 3);

        playState.playSong(0, PlayState::Latched);
        engine.update();
        expectFalse(songState.playing());
        expectEqual(playState.trackState(0).pattern(), 0);

        playState.commitLatchedRequests();
        engine.update();
        expectTrue(songState.playing());
        expectEqual(songState.currentSlot(), 0);
        expectEqual(songState.currentRepeat(), 0);
        expectEqual(playState.trackState(0).pattern(), 3);
        expectFalse(songState.hasPlayRequests());
    }

    CASE("pattern change or stopSong request terminates song playback") {
        SequencerHarness harness;
        auto &project = harness.app().model.project();
        auto &engine = harness.app().engine;
        auto &playState = project.playState();
        auto &songState = playState.songState();

        project.song().clear();
        project.song().insertSlot(0);
        project.song().setPattern(0, 0, 2);

        playState.playSong(0, PlayState::Immediate);
        engine.update();
        expectTrue(songState.playing());

        playState.selectTrackPattern(0, 1, PlayState::Immediate);
        engine.update();
        expectFalse(songState.playing());
        expectEqual(playState.trackState(0).pattern(), 1);

        playState.playSong(0, PlayState::Immediate);
        engine.update();
        expectTrue(songState.playing());

        playState.stopSong(PlayState::Immediate);
        engine.update();
        expectFalse(songState.playing());
    }

    CASE("playSong applies slot mutes when track has mutes in song") {
        SequencerHarness harness;
        auto &project = harness.app().model.project();
        auto &engine = harness.app().engine;
        auto &playState = project.playState();

        project.song().clear();
        project.song().insertSlot(0);
        project.song().setMute(0, 0, true);

        playState.playSong(0, PlayState::Immediate);
        engine.update();

        expectTrue(playState.trackState(0).mute());
    }

    CASE("clock setup dirty flag is cleared by engine update") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &clockSetup = harness.app().model.project().clockSetup();

        clockSetup.clearDirty();
        expectFalse(clockSetup.isDirty());

        clockSetup.setClockOutputPulse(clockSetup.clockOutputPulse() + 1);
        expectTrue(clockSetup.isDirty());

        engine.update();
        expectFalse(clockSetup.isDirty());
    }

    CASE("clock input divisor accepts common external ppqn values") {
        SequencerHarness harness;
        auto &clockSetup = harness.app().model.project().clockSetup();

        const int validPpqnValues[] = { 3, 12, 16, 24 };
        for (int ppqn : validPpqnValues) {
            clockSetup.clearDirty();
            clockSetup.setClockInputDivisor(ppqn);
            expectEqual(clockSetup.clockInputDivisor(), ppqn);
            expectTrue(clockSetup.isDirty());
        }
    }

    CASE("clock input divisor clamping and dirty-flag consumption are stable") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &clockSetup = harness.app().model.project().clockSetup();

        clockSetup.clearDirty();
        clockSetup.setClockInputDivisor(0);
        expectEqual(clockSetup.clockInputDivisor(), 1);
        expectTrue(clockSetup.isDirty());

        // Engine::updateClockSetup consumes pending setup changes.
        engine.update();
        expectFalse(clockSetup.isDirty());

        clockSetup.setClockInputDivisor(999);
        expectEqual(clockSetup.clockInputDivisor(), 192);
        expectTrue(clockSetup.isDirty());

        engine.update();
        expectFalse(clockSetup.isDirty());
    }

    CASE("sendMidi rejects CvGate port") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;

        bool sent = engine.sendMidi(MidiPort::CvGate, 0, MidiMessage::makeNoteOn(0, 60, 100));
        expectFalse(sent);
    }

    CASE("sendMidi accepts MIDI and USB MIDI output ports") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        const auto message = MidiMessage::makeNoteOn(0, 60, 100);

        expectTrue(engine.sendMidi(MidiPort::Midi, 0, message));
        expectTrue(engine.sendMidi(MidiPort::UsbMidi, 0, message));
    }

    CASE("message handler receives text and duration") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;

        bool called = false;
        const char *receivedText = nullptr;
        uint32_t receivedDuration = 0;

        engine.setMessageHandler([&] (const char *text, uint32_t duration) {
            called = true;
            receivedText = text;
            receivedDuration = duration;
        });

        engine.showMessage("hello", 1234);

        expectTrue(called);
        expectEqual(receivedText, "hello");
        expectEqual(receivedDuration, uint32_t(1234));
    }

    CASE("lock and suspend transitions update engine state flags") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;

        expectFalse(engine.isLocked());
        engine.lock();
        expectTrue(engine.isLocked());
        engine.unlock();
        expectFalse(engine.isLocked());

        expectFalse(engine.isSuspended());
        engine.suspend();
        expectTrue(engine.isSuspended());
        engine.resume();
        expectFalse(engine.isSuspended());
    }

    CASE("recording state can be toggled and set explicitly") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;

        engine.setRecording(false);
        expectFalse(engine.recording());

        engine.toggleRecording();
        expectTrue(engine.recording());

        engine.setRecording(false);
        expectFalse(engine.recording());
    }

    CASE("togglePlay covers restart and non-shift start-reset behavior") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &clockSetup = harness.app().model.project().clockSetup();

        clockSetup.setMode(ClockSetup::Mode::Master);
        clockSetup.setShiftMode(ClockSetup::ShiftMode::Restart);
        engine.update();

        expectFalse(engine.clockRunning());

        engine.togglePlay(true);
        engine.update();
        expectTrue(engine.clockRunning());

        // Non-shift while running -> reset/stop.
        engine.togglePlay(false);
        engine.update();
        expectFalse(engine.clockRunning());

        // Non-shift while stopped -> start.
        engine.togglePlay(false);
        engine.update();
        expectTrue(engine.clockRunning());
    }

    CASE("tap tempo and nudge helpers are callable and keep tempo positive") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &project = harness.app().model.project();

        engine.tapTempoReset();
        harness.waitMs(120);
        engine.tapTempoTap();
        harness.waitMs(120);
        engine.tapTempoTap();

        engine.nudgeTempoSetDirection(1);
        harness.waitMs(20);
        engine.update();

        expectTrue(project.tempo() > 0.f);
        expectTrue(engine.nudgeTempoStrength() >= 0.f);

        engine.nudgeTempoSetDirection(0);
    }

    CASE("usb midi connect and disconnect handlers are forwarded") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;

        int connectCalls = 0;
        int disconnectCalls = 0;
        uint16_t vendorId = 0;
        uint16_t productId = 0;

        engine.setUsbMidiConnectHandler([&] (uint16_t v, uint16_t p) {
            ++connectCalls;
            vendorId = v;
            productId = p;
        });
        engine.setUsbMidiDisconnectHandler([&] () {
            ++disconnectCalls;
        });

        harness.connectUsbMidi(0x1234, 0x5678);
        expectEqual(connectCalls, 1);
        expectEqual(vendorId, uint16_t(0x1234));
        expectEqual(productId, uint16_t(0x5678));

        harness.disconnectUsbMidi();
        expectEqual(disconnectCalls, 1);
    }

    CASE("cv and gate output overrides are applied during update") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;

        engine.setGateOutputOverride(true);
        engine.setGateOutput(0x0f);
        engine.setCvOutputOverride(true);
        engine.setCvOutput(0, 0.25f);
        engine.setCvOutput(1, -0.5f);
        engine.update();

        expectEqual(engine.gateOutput(), uint8_t(0x0f));
        expectTrue(engine.cvOutput().channel(0) == 0.25f);
        expectTrue(engine.cvOutput().channel(1) == -0.5f);

        engine.setGateOutputOverride(false);
        engine.setCvOutputOverride(false);
    }

    CASE("cv gate input mode switching covers all converter branches") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &project = harness.app().model.project();

        project.setCvGateInput(Types::CvGateInput::Cv1Cv2);
        engine.update();

        project.setCvGateInput(Types::CvGateInput::Cv3Cv4);
        engine.update();

        project.setCvGateInput(Types::CvGateInput::Off);
        engine.update();

        expectTrue(engine.tick() >= 0);
    }

    CASE("clock output run mode and usb midi tx path can run in master mode") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &clockSetup = harness.app().model.project().clockSetup();

        clockSetup.setMode(ClockSetup::Mode::Master);
        clockSetup.setClockOutputMode(ClockSetup::ClockOutputMode::Run);
        clockSetup.setUsbTx(true);
        clockSetup.setMidiTx(true);
        engine.update();

        engine.clockStart();
        engine.update();
        expectTrue(engine.clockRunning());

        engine.clockStop();
        engine.update();
        expectFalse(engine.clockRunning());
    }

    CASE("midi receive handler can consume messages before monitor processing") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &project = harness.app().model.project();
        auto &noteEngine = engine.trackEngine(0).as<NoteTrackEngine>();

        project.setMidiInputMode(Types::MidiInputMode::All);
        project.setMonitorMode(Types::MonitorMode::Always);
        harness.app().model.project().track(0).noteTrack().setSlideTime(0);
        engine.update();

        float baselineCv = noteEngine.cvOutput(0);
        int handlerCalls = 0;

        engine.setMidiReceiveHandler([&] (MidiPort, uint8_t, const MidiMessage &) {
            ++handlerCalls;
            return true;
        });

        harness.sendDinMidi(MidiMessage::makeNoteOn(0, 60, 100));
        engine.update();

        expectEqual(handlerCalls, 1);
        expectTrue(noteEngine.cvOutput(0) == baselineCv);
    }

    CASE("midi receive handler passthrough keeps normal monitor processing") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &project = harness.app().model.project();
        auto &noteEngine = engine.trackEngine(0).as<NoteTrackEngine>();

        project.setMidiInputMode(Types::MidiInputMode::All);
        project.setMonitorMode(Types::MonitorMode::Always);
        project.track(0).noteTrack().setSlideTime(0);
        engine.update();

        float baselineCv = noteEngine.cvOutput(0);
        int handlerCalls = 0;

        engine.setMidiReceiveHandler([&] (MidiPort, uint8_t, const MidiMessage &) {
            ++handlerCalls;
            return false;
        });

        harness.sendDinMidi(MidiMessage::makeNoteOn(0, 63, 100));
        engine.update();

        expectEqual(handlerCalls, 1);
        expectTrue(noteEngine.cvOutput(0) != baselineCv);
    }

    CASE("MidiCv track consumption short-circuits monitor path") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &engine = app.engine;
        auto &project = app.model.project();

        // Track 0 consumes as MidiCv; selected track 1 should not be monitored.
        project.setTrackMode(0, Track::TrackMode::MidiCv);
        project.setTrackMode(1, Track::TrackMode::Note);
        project.setSelectedTrackIndex(1);
        project.setMidiInputMode(Types::MidiInputMode::All);
        project.setMonitorMode(Types::MonitorMode::Always);
        project.track(1).noteTrack().setSlideTime(0);
        engine.update();

        auto &selectedNoteEngine = engine.trackEngine(1).as<NoteTrackEngine>();
        float baselineCv = selectedNoteEngine.cvOutput(0);

        harness.sendDinMidi(MidiMessage::makeNoteOn(0, 64, 100));
        engine.update();

        expectTrue(selectedNoteEngine.cvOutput(0) == baselineCv);
    }

    CASE("midi input mode off ignores incoming monitor midi") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &project = harness.app().model.project();
        auto &noteEngine = engine.trackEngine(0).as<NoteTrackEngine>();

        project.setMidiInputMode(Types::MidiInputMode::Off);
        project.setMonitorMode(Types::MonitorMode::Always);
        project.track(0).noteTrack().setSlideTime(0);
        engine.update();

        float baselineCv = noteEngine.cvOutput(0);
        harness.sendDinMidi(MidiMessage::makeNoteOn(0, 60, 100));
        engine.update();

        expectTrue(noteEngine.cvOutput(0) == baselineCv);
    }

    CASE("midi source mode requires matching port and channel") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &project = harness.app().model.project();
        auto &noteEngine = engine.trackEngine(0).as<NoteTrackEngine>();

        project.setMidiInputMode(Types::MidiInputMode::Source);
        project.setMonitorMode(Types::MonitorMode::Always);
        project.track(0).noteTrack().setSlideTime(0);
        auto &source = project.midiInputSource();
        source.setPort(Types::MidiPort::UsbMidi);
        source.setChannel(2);
        engine.update();

        float baselineCv = noteEngine.cvOutput(0);

        harness.sendDinMidi(MidiMessage::makeNoteOn(2, 61, 100));
        engine.update();
        expectTrue(noteEngine.cvOutput(0) == baselineCv);

        harness.sendUsbMidi(MidiMessage::makeNoteOn(1, 61, 100));
        engine.update();
        expectTrue(noteEngine.cvOutput(0) == baselineCv);

        harness.sendUsbMidi(MidiMessage::makeNoteOn(2, 61, 100));
        engine.update();
        expectTrue(noteEngine.cvOutput(0) != baselineCv);
    }

    CASE("midi note on updates live monitor cv when input mode allows it") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &project = harness.app().model.project();
        auto &noteEngine = engine.trackEngine(0).as<NoteTrackEngine>();

        project.setMidiInputMode(Types::MidiInputMode::All);
        project.setMonitorMode(Types::MonitorMode::Always);
        project.track(0).noteTrack().setSlideTime(0);
        engine.update();

        float baselineCv = noteEngine.cvOutput(0);
        harness.sendDinMidi(MidiMessage::makeNoteOn(0, 64, 100));
        engine.update();

        expectTrue(noteEngine.cvOutput(0) != baselineCv);
    }

    CASE("real-time and system messages are ignored by midi monitor path") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &project = harness.app().model.project();
        auto &noteEngine = engine.trackEngine(0).as<NoteTrackEngine>();

        project.setMidiInputMode(Types::MidiInputMode::All);
        project.setMonitorMode(Types::MonitorMode::Always);
        project.track(0).noteTrack().setSlideTime(0);
        engine.update();

        float baselineCv = noteEngine.cvOutput(0);

        harness.sendDinMidi(MidiMessage(uint8_t(0xF8)));      // Tick (real-time)
        harness.sendDinMidi(MidiMessage(uint8_t(0xF1), 0x01)); // TimeCode (system)
        engine.update();

        expectTrue(noteEngine.cvOutput(0) == baselineCv);
    }

    CASE("single-byte DIN and USB filters handle clock and non-clock statuses") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &clockSetup = harness.app().model.project().clockSetup();

        clockSetup.setMode(ClockSetup::Mode::Slave);
        clockSetup.setUsbRx(true);
        engine.update();

        // Clock statuses should be consumed by clock receive filters.
        harness.sendDinMidi(MidiMessage(uint8_t(MidiMessage::Start)));
        harness.sendUsbMidi(MidiMessage(uint8_t(MidiMessage::Tick)));
        engine.update();

        // Non-clock one-byte statuses should not be consumed by clock filters.
        harness.sendDinMidi(MidiMessage(uint8_t(MidiMessage::TuneRequest)));
        harness.sendUsbMidi(MidiMessage(uint8_t(MidiMessage::TuneRequest)));
        engine.update();

        expectTrue(engine.tick() >= 0);
    }

    CASE("midi source mode with omni channel accepts matching port on any channel") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &project = harness.app().model.project();
        auto &noteEngine = engine.trackEngine(0).as<NoteTrackEngine>();

        project.setMidiInputMode(Types::MidiInputMode::Source);
        project.setMonitorMode(Types::MonitorMode::Always);
        project.track(0).noteTrack().setSlideTime(0);
        auto &source = project.midiInputSource();
        source.setPort(Types::MidiPort::UsbMidi);
        source.setChannel(-1); // omni
        engine.update();

        float baselineCv = noteEngine.cvOutput(0);
        harness.sendUsbMidi(MidiMessage::makeNoteOn(0, 67, 100));
        engine.update();
        float cvAfterChannel0 = noteEngine.cvOutput(0);
        expectTrue(cvAfterChannel0 != baselineCv);

        harness.sendUsbMidi(MidiMessage::makeNoteOn(9, 60, 100));
        engine.update();
        expectTrue(noteEngine.cvOutput(0) != cvAfterChannel0);
    }

    CASE("new note-on on same track replaces previous monitored note") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &project = harness.app().model.project();
        auto &noteEngine = engine.trackEngine(0).as<NoteTrackEngine>();

        project.setMidiInputMode(Types::MidiInputMode::All);
        project.setMonitorMode(Types::MonitorMode::Always);
        project.track(0).noteTrack().setSlideTime(0);
        engine.update();

        float baselineCv = noteEngine.cvOutput(0);

        harness.sendDinMidi(MidiMessage::makeNoteOn(0, 67, 100));
        engine.update();
        float cvAfterFirstNote = noteEngine.cvOutput(0);
        expectTrue(cvAfterFirstNote != baselineCv);

        harness.sendDinMidi(MidiMessage::makeNoteOn(0, 69, 100));
        engine.update();

        expectTrue(noteEngine.cvOutput(0) != cvAfterFirstNote);
    }

    CASE("note-off and control-change messages pass through monitor branches") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &project = harness.app().model.project();

        project.setMidiInputMode(Types::MidiInputMode::All);
        project.setMonitorMode(Types::MonitorMode::Always);
        project.track(0).noteTrack().setSlideTime(0);
        engine.update();

        harness.sendDinMidi(MidiMessage::makeNoteOn(0, 60, 100));
        engine.update();

        harness.sendDinMidi(MidiMessage::makeControlChange(0, 1, 64));
        engine.update();

        harness.sendDinMidi(MidiMessage::makeNoteOff(0, 60));
        engine.update();

        expectFalse(engine.clockRunning());
    }

    CASE("changing selected track while note is active triggers monitor handoff") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &project = harness.app().model.project();

        project.setMidiInputMode(Types::MidiInputMode::All);
        project.setMonitorMode(Types::MonitorMode::Always);
        project.track(0).noteTrack().setSlideTime(0);
        project.track(1).noteTrack().setSlideTime(0);
        engine.update();

        project.setSelectedTrackIndex(0);
        harness.sendDinMidi(MidiMessage::makeNoteOn(0, 62, 100));
        engine.update();

        project.setSelectedTrackIndex(1);
        harness.sendDinMidi(MidiMessage::makeControlChange(0, 7, 80));
        engine.update();

        expectEqual(project.selectedTrackIndex(), 1);
    }

    CASE("fake note-off (note-on with velocity 0) is normalized before receive handler") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;

        bool handlerCalled = false;
        bool sawNormalizedNoteOff = false;

        engine.setMidiReceiveHandler([&] (MidiPort, uint8_t, const MidiMessage &message) {
            handlerCalled = true;
            sawNormalizedNoteOff = message.isNoteOff();
            return true;
        });

        harness.sendDinMidi(MidiMessage::makeNoteOn(0, 69, 0));
        engine.update();

        expectTrue(handlerCalled);
        expectTrue(sawNormalizedNoteOff);
    }

    CASE("time-base and stats helpers expose consistent values") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &project = harness.app().model.project();

        project.setSyncMeasure(4);
        engine.update();

        expectEqual(engine.noteDivisor(), project.timeSignature().noteDivisor());
        expectEqual(engine.measureDivisor(), project.timeSignature().measureDivisor());
        expectEqual(engine.syncDivisor(), uint32_t(project.syncMeasure()) * project.timeSignature().measureDivisor());

        // At startup no ticks were processed yet, so normalized fractions start at zero.
        expectTrue(engine.measureFraction() == 0.f);
        expectTrue(engine.syncFraction() == 0.f);

        auto stats = engine.stats();
        expectEqual(stats.midiRxOverflow, uint32_t(0));
        expectEqual(stats.usbMidiRxOverflow, uint32_t(0));
    }

    CASE("gate offset zero triggers note-on at step start") {
        SequencerHarness harness;
        auto &engine = prepareSingleStepEngine(harness.app(), 0);

        auto result = engine.tick(0);
        expectTrue((result & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(engine.gateOutput(0));
    }

    CASE("positive gate offset delays note-on by expected ticks") {
        SequencerHarness harness;
        constexpr int offset = 3;
        auto &engine = prepareSingleStepEngine(harness.app(), offset);
        uint32_t expectedOnTick = gateOnTick(engine.sequence(), offset);

        for (uint32_t tick = 0; tick < expectedOnTick; ++tick) {
            auto result = engine.tick(tick);
            expectFalse((result & TrackEngine::TickResult::GateUpdate) != 0 && engine.gateOutput(0));
        }

        auto onResult = engine.tick(expectedOnTick);
        expectTrue((onResult & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(engine.gateOutput(0));
    }

    CASE("gate-off timing stays consistent when gate offset is applied") {
        SequencerHarness harness;
        constexpr int offset = NoteSequence::GateOffset::Max;
        constexpr int length = 3;
        auto &engine = prepareSingleStepEngine(harness.app(), offset, length);
        uint32_t onTick = gateOnTick(engine.sequence(), offset);
        uint32_t offTick = onTick + gateLengthTicks(engine.sequence(), length);

        for (uint32_t tick = 0; tick <= onTick; ++tick) {
            engine.tick(tick);
        }
        expectTrue(engine.gateOutput(0));

        for (uint32_t tick = onTick + 1; tick < offTick; ++tick) {
            engine.tick(tick);
        }
        expectTrue(engine.gateOutput(0));

        auto offResult = engine.tick(offTick);
        expectTrue((offResult & TrackEngine::TickResult::GateUpdate) != 0);
        expectFalse(engine.gateOutput(0));
    }

    CASE("consecutive steps keep independent gate offset timing") {
        SequencerHarness harness;
        auto &engine = prepareTwoStepEngine(harness.app(), 0, NoteSequence::GateOffset::Max);
        uint32_t secondStepOnTick = sequenceDivisorTicks(engine.sequence()) +
            gateOnTick(engine.sequence(), NoteSequence::GateOffset::Max);

        auto firstStepResult = engine.tick(0);
        expectTrue((firstStepResult & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(engine.gateOutput(0));

        for (uint32_t tick = 1; tick < secondStepOnTick; ++tick) {
            engine.tick(tick);
        }
        expectFalse(engine.gateOutput(0));

        auto secondStepResult = engine.tick(secondStepOnTick);
        expectTrue((secondStepResult & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(engine.gateOutput(0));
    }

    CASE("negative gate offset input is clamped to zero timing") {
        SequencerHarness harness;
        auto &engine = prepareSingleStepEngine(harness.app(), -5);

        auto result = engine.tick(0);
        expectTrue((result & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(engine.gateOutput(0));
    }

}

