#include "UnitTest.h"

#include "apps/sequencer/SequencerApp.h"

#include "sim/Simulator.h"

#include <memory>

namespace {

class SequencerHarness {
public:
    SequencerHarness() :
        _simulator(makeTarget())
    {
        _simulator.reboot();
        _simulator.wait(1);
    }

    SequencerApp &app() { return *_app; }

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
    sim::Simulator _simulator;
};

static MidiCvTrackEngine &configureMidiCvTrack(SequencerApp &app, int trackIndex) {
    auto &project = app.model.project();
    project.setTrackMode(trackIndex, Track::TrackMode::MidiCv);
    app.engine.update();

    auto &track = project.track(trackIndex);
    auto &midiCvTrack = track.midiCvTrack();
    midiCvTrack.clear();
    midiCvTrack.source().setPort(Types::MidiPort::Midi);
    midiCvTrack.source().setChannel(-1);
    midiCvTrack.setVoices(1);
    midiCvTrack.setVoiceConfig(MidiCvTrack::VoiceConfig::Pitch);
    midiCvTrack.setNotePriority(MidiCvTrack::NotePriority::LastNote);
    midiCvTrack.setLowNote(0);
    midiCvTrack.setHighNote(127);
    midiCvTrack.setPitchBendRange(12);
    midiCvTrack.setModulationRange(Types::VoltageRange::Unipolar5V);
    midiCvTrack.setRetrigger(false);
    midiCvTrack.setSlideTime(0);
    midiCvTrack.setTranspose(0);
    midiCvTrack.arpeggiator().clear();

    auto &engine = app.engine.trackEngine(trackIndex).as<MidiCvTrackEngine>();
    engine.reset();
    return engine;
}

static float noteToCv(int note) {
    return (note - 60) * (1.f / 12.f);
}

static float valueToCv(int value, Types::VoltageRange range = Types::VoltageRange::Unipolar5V) {
    return Types::voltageRangeInfo(range).denormalize(value * (1.f / 127.f));
}

static float pitchBendToCv(int value, int bendRange) {
    return value * bendRange * (1.f / (12 * 8192));
}

} // namespace

UNIT_TEST("MidiCvTrackEngine") {

    CASE("reset restart tick and basic note handling keep engine state coherent") {
        SequencerHarness harness;
        auto &engine = configureMidiCvTrack(harness.app(), 0);
        auto &track = harness.app().model.project().track(0).midiCvTrack();

        expectEqual(engine.tick(0), TrackEngine::TickResult::NoUpdate);
        expectFalse(engine.activity());
        expectFalse(engine.gateOutput(0));
        expectEqual(engine.cvOutput(0), 0.f);

        track.setLowNote(60);
        track.setHighNote(72);

        expectFalse(engine.receiveMidi(MidiPort::UsbMidi, MidiMessage::makeNoteOn(0, 60, 100)));
        expectFalse(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 59, 100)));
        expectFalse(engine.activity());

        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60, 100)));
        expectTrue(engine.activity());
        expectTrue(engine.gateOutput(0));
        expectEqual(engine.cvOutput(0), noteToCv(60));

        // restart() is intentionally a no-op for MIDI/CV tracks.
        engine.restart();
        expectTrue(engine.activity());
        expectTrue(engine.gateOutput(0));

        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOff(0, 60, 0)));
        expectFalse(engine.activity());
        expectFalse(engine.gateOutput(0));

        engine.reset();
        expectFalse(engine.activity());
        expectEqual(engine.cvOutput(0), 0.f);
    }

    CASE("pitch velocity pressure channel pressure and pitch bend drive configured CV outputs") {
        SequencerHarness harness;
        auto &engine = configureMidiCvTrack(harness.app(), 0);
        auto &track = harness.app().model.project().track(0).midiCvTrack();
        const MidiMessage maxPitchBend(uint8_t(MidiMessage::PitchBend | 0), 0x7f, 0x7f);

        track.setVoiceConfig(MidiCvTrack::VoiceConfig::PitchVelocityPressure);
        track.setPitchBendRange(12);
        track.setModulationRange(Types::VoltageRange::Unipolar5V);

        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 72, 64)));
        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeKeyPressure(0, 72, 32)));
        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeChannelPressure(0, 16)));
        expectTrue(engine.receiveMidi(MidiPort::Midi, maxPitchBend));
        engine.update(0.f);

        const float expectedPitch = noteToCv(72) + pitchBendToCv(8191, 12);
        const float expectedVelocity = valueToCv(64);
        const float expectedPressure = valueToCv(32) + valueToCv(16);

        expectEqual(engine.cvOutput(0), expectedPitch);
        expectEqual(engine.cvOutput(1), expectedVelocity);
        expectEqual(engine.cvOutput(2), expectedPressure);

        // Key pressure on an inactive note is still consumed but must not change activity.
        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeKeyPressure(0, 71, 40)));
        expectTrue(engine.activity());

        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOff(0, 72, 0)));
        expectFalse(engine.activity());
    }

    CASE("note priority LastNote keeps the newest note on the monophonic output") {
        SequencerHarness harness;
        auto &engine = configureMidiCvTrack(harness.app(), 0);

        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60, 100)));
        harness.waitMs(1);
        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 64, 100)));
        engine.update(0.f);

        expectEqual(engine.cvOutput(0), noteToCv(64));
    }

    CASE("note priority FirstNote keeps the oldest note on the monophonic output") {
        SequencerHarness harness;
        auto &engine = configureMidiCvTrack(harness.app(), 0);
        auto &track = harness.app().model.project().track(0).midiCvTrack();
        track.setNotePriority(MidiCvTrack::NotePriority::FirstNote);

        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60, 100)));
        harness.waitMs(1);
        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 64, 100)));
        engine.update(0.f);

        expectEqual(engine.cvOutput(0), noteToCv(60));
    }

    CASE("note priority LowestNote and HighestNote pick the expected monophonic note") {
        {
            SequencerHarness lowHarness;
            auto &lowEngine = configureMidiCvTrack(lowHarness.app(), 0);
            auto &lowTrack = lowHarness.app().model.project().track(0).midiCvTrack();
            lowTrack.setNotePriority(MidiCvTrack::NotePriority::LowestNote);
            expectTrue(lowEngine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 67, 100)));
            expectTrue(lowEngine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60, 100)));
            lowEngine.update(0.f);
            expectEqual(lowEngine.cvOutput(0), noteToCv(60));
        }

        {
            SequencerHarness highHarness;
            auto &highEngine = configureMidiCvTrack(highHarness.app(), 0);
            auto &highTrack = highHarness.app().model.project().track(0).midiCvTrack();
            highTrack.setNotePriority(MidiCvTrack::NotePriority::HighestNote);
            expectTrue(highEngine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60, 100)));
            expectTrue(highEngine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 67, 100)));
            highEngine.update(0.f);
            expectEqual(highEngine.cvOutput(0), noteToCv(67));
        }
    }

    CASE("retrigger delays gate reactivation briefly after note-on") {
        SequencerHarness harness;
        auto &engine = configureMidiCvTrack(harness.app(), 0);
        auto &track = harness.app().model.project().track(0).midiCvTrack();
        track.setRetrigger(true);

        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60, 100)));
        expectFalse(engine.gateOutput(0));

        harness.waitMs(5);
        expectTrue(engine.gateOutput(0));
    }

    CASE("monophonic slide applies smoothing when a new note arrives while another voice is active") {
        SequencerHarness harness;
        auto &engine = configureMidiCvTrack(harness.app(), 0);
        auto &track = harness.app().model.project().track(0).midiCvTrack();

        track.setSlideTime(100);
        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60, 100)));
        engine.update(0.f);
        float startCv = engine.cvOutput(0);

        harness.waitMs(1);
        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 72, 100)));
        engine.update(0.001f);
        float smoothedCv = engine.cvOutput(0);

        expectTrue(smoothedCv > startCv);
        expectTrue(smoothedCv < noteToCv(72));

        engine.update(1.f);
        expectTrue(engine.cvOutput(0) > smoothedCv);
    }

    CASE("polyphonic outputs expose pitch velocity and pressure signals per allocated voice") {
        SequencerHarness harness;
        auto &engine = configureMidiCvTrack(harness.app(), 0);
        auto &track = harness.app().model.project().track(0).midiCvTrack();

        track.setVoices(2);
        track.setVoiceConfig(MidiCvTrack::VoiceConfig::PitchVelocityPressure);
        track.setModulationRange(Types::VoltageRange::Unipolar5V);

        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60, 32)));
        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeKeyPressure(0, 60, 10)));
        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 67, 96)));
        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeKeyPressure(0, 67, 20)));

        bool foundLowPitch = false;
        bool foundHighPitch = false;
        bool foundLowVelocity = false;
        bool foundHighVelocity = false;
        bool foundLowPressure = false;
        bool foundHighPressure = false;

        for (int index = 0; index < 6; ++index) {
            float value = engine.cvOutput(index);
            if (value == noteToCv(60)) foundLowPitch = true;
            if (value == noteToCv(67)) foundHighPitch = true;
            if (value == valueToCv(32)) foundLowVelocity = true;
            if (value == valueToCv(96)) foundHighVelocity = true;
            if (value == valueToCv(10)) foundLowPressure = true;
            if (value == valueToCv(20)) foundHighPressure = true;
        }

        expectTrue(foundLowPitch);
        expectTrue(foundHighPitch);
        expectTrue(foundLowVelocity);
        expectTrue(foundHighVelocity);
        expectTrue(foundLowPressure);
        expectTrue(foundHighPressure);
    }

    CASE("adding more than eight active notes replaces the lowest-priority internal voice") {
        SequencerHarness harness;
        auto &engine = configureMidiCvTrack(harness.app(), 0);
        auto &track = harness.app().model.project().track(0).midiCvTrack();

        track.setVoices(8);
        track.setVoiceConfig(MidiCvTrack::VoiceConfig::Pitch);
        track.setNotePriority(MidiCvTrack::NotePriority::LastNote);

        for (int note = 60; note < 68; ++note) {
            expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, note, 100)));
            harness.waitMs(1);
        }

        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 72, 100)));

        bool foundOldestNote = false;
        bool foundNewestNote = false;
        for (int index = 0; index < 8; ++index) {
            float value = engine.cvOutput(index);
            if (value == noteToCv(60)) foundOldestNote = true;
            if (value == noteToCv(72)) foundNewestNote = true;
        }

        expectFalse(foundOldestNote);
        expectTrue(foundNewestNote);
    }

    CASE("arpeggiator tick path turns held notes into active output voices and disable resets them") {
        SequencerHarness harness;
        auto &engine = configureMidiCvTrack(harness.app(), 0);
        auto &track = harness.app().model.project().track(0).midiCvTrack();

        track.arpeggiator().setEnabled(true);
        track.arpeggiator().setMode(Arpeggiator::Mode::PlayOrder);
        track.arpeggiator().setDivisor(12);
        track.arpeggiator().setGateLength(50);
        engine.update(0.f);

        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60, 100)));
        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 64, 100)));
        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOff(0, 64, 0)));
        expectFalse(engine.activity());

        expectEqual(engine.tick(0), TrackEngine::TickResult::NoUpdate);
        expectTrue(engine.activity());
        expectTrue(engine.gateOutput(0));

        track.arpeggiator().setEnabled(false);
        engine.update(0.f);
        expectFalse(engine.activity());
        expectFalse(engine.gateOutput(0));
    }

    CASE("arpeggiator queued note-off event clears activity after the configured gate length") {
        SequencerHarness harness;
        auto &engine = configureMidiCvTrack(harness.app(), 0);
        auto &track = harness.app().model.project().track(0).midiCvTrack();

        track.arpeggiator().setEnabled(true);
        track.arpeggiator().setMode(Arpeggiator::Mode::PlayOrder);
        track.arpeggiator().setDivisor(12);
        track.arpeggiator().setGateLength(50);
        engine.update(0.f);

        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60, 100)));
        expectEqual(engine.tick(0), TrackEngine::TickResult::NoUpdate);
        expectTrue(engine.activity());
        expectTrue(engine.gateOutput(0));

        expectEqual(engine.tick(24), TrackEngine::TickResult::NoUpdate);
        expectFalse(engine.activity());
        expectFalse(engine.gateOutput(0));
    }

    CASE("arpeggiator update advances while clock is stopped") {
        SequencerHarness harness;
        auto &engine = configureMidiCvTrack(harness.app(), 0);
        auto &track = harness.app().model.project().track(0).midiCvTrack();
        const float tickDuration = harness.app().engine.clock().tickDuration();

        track.arpeggiator().setEnabled(true);
        track.arpeggiator().setMode(Arpeggiator::Mode::PlayOrder);
        track.arpeggiator().setDivisor(12);
        engine.update(0.f);

        expectTrue(engine.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 72, 100)));
        expectFalse(engine.activity());

        engine.update(tickDuration * 2.f);
        expectTrue(engine.activity());
        expectEqual(engine.cvOutput(0), noteToCv(72));
    }
}

