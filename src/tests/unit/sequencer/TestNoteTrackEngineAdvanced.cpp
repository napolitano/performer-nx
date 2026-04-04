#include "UnitTest.h"

#include "apps/sequencer/SequencerApp.h"
#include "apps/sequencer/model/NoteSequence.h"

#include "sim/Simulator.h"

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

static NoteTrackEngine &prepareTwoStepSequence(SequencerApp &app) {
    auto &sequence = app.model.project().track(0).noteTrack().sequence(0);
    sequence.clearSteps();
    sequence.setFirstStep(0);
    sequence.setLastStep(1);

    for (int stepIndex = 0; stepIndex <= 1; ++stepIndex) {
        auto &step = sequence.step(stepIndex);
        step.setGate(true);
        step.setGateProbability(NoteSequence::GateProbability::Max);
        step.setLength(NoteSequence::Length::Max / 2);
        step.setLengthVariationRange(0);
        step.setLengthVariationProbability(NoteSequence::LengthVariationProbability::Max);
        step.setRetrigger(0);
        step.setRetriggerProbability(NoteSequence::RetriggerProbability::Max);
        step.setGateOffset(0);
    }

    auto &engine = app.engine.trackEngine(0).as<NoteTrackEngine>();
    engine.reset();
    return engine;
}

static NoteTrackEngine &prepareSingleStepSequence(SequencerApp &app, int trackIndex) {
    auto &sequence = app.model.project().track(trackIndex).noteTrack().sequence(0);
    sequence.clearSteps();
    sequence.setFirstStep(0);
    sequence.setLastStep(0);

    auto &step = sequence.step(0);
    step.setGate(true);
    step.setGateProbability(NoteSequence::GateProbability::Max);
    step.setLength(0);
    step.setLengthVariationRange(0);
    step.setLengthVariationProbability(NoteSequence::LengthVariationProbability::Max);
    step.setRetrigger(0);
    step.setRetriggerProbability(NoteSequence::RetriggerProbability::Max);
    step.setGateOffset(0);

    auto &engine = app.engine.trackEngine(trackIndex).as<NoteTrackEngine>();
    engine.reset();
    return engine;
}

static void tickRange(NoteTrackEngine &engine, uint32_t firstTickInclusive, uint32_t lastTickInclusive) {
    for (uint32_t tick = firstTickInclusive; tick <= lastTickInclusive; ++tick) {
        engine.tick(tick);
    }
}

} // namespace

UNIT_TEST("NoteTrackEngineAdvanced") {

    CASE("restart clears current step after sequence was advanced") {
        SequencerHarness harness;
        auto &engine = prepareTwoStepSequence(harness.app());

        auto result = engine.tick(0);
        expectTrue((result & TrackEngine::TickResult::GateUpdate) != 0);
        expectEqual(engine.currentStep(), 0);

        engine.restart();
        expectEqual(engine.currentStep(), -1);
    }

    CASE("setMonitorStep drives step recorder index only in step record mode") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &project = app.model.project();
        auto &engine = app.engine;
        auto &noteEngine = prepareTwoStepSequence(app);

        project.setSelectedTrackIndex(0);
        project.setRecordMode(Types::RecordMode::StepRecord);
        engine.setRecording(true);

        // update() enables step recorder and initializes to first step.
        noteEngine.update(0.f);
        expectEqual(noteEngine.currentRecordStep(), 0);

        noteEngine.setMonitorStep(1);
        expectEqual(noteEngine.currentRecordStep(), 1);

        // Invalid monitor index must not alter recorder step.
        noteEngine.setMonitorStep(-1);
        expectEqual(noteEngine.currentRecordStep(), 1);

        engine.setRecording(false);
        noteEngine.update(0.f);
        expectEqual(noteEngine.currentRecordStep(), -1);
    }

    CASE("step recorder writes note data and advances to next step on note off") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &project = app.model.project();
        auto &engine = app.engine;
        auto &noteEngine = prepareTwoStepSequence(app);
        auto &sequence = project.track(0).noteTrack().sequence(0);

        project.setSelectedTrackIndex(0);
        project.setRecordMode(Types::RecordMode::StepRecord);
        engine.setRecording(true);
        noteEngine.update(0.f);

        noteEngine.monitorMidi(0, MidiMessage::makeNoteOn(0, 60, 100));
        expectTrue(sequence.step(0).gate());
        expectEqual(sequence.step(0).length(), NoteSequence::Length::Max / 2);

        noteEngine.monitorMidi(1, MidiMessage::makeNoteOff(0, 60));
        expectEqual(noteEngine.currentRecordStep(), 1);

        // Control change #1 tags tie for the pressed step.
        noteEngine.monitorMidi(2, MidiMessage::makeNoteOn(0, 62, 100));
        noteEngine.monitorMidi(3, MidiMessage::makeControlChange(0, 1, 127));
        expectEqual(sequence.step(1).length(), NoteSequence::Length::Max);

        noteEngine.monitorMidi(4, MidiMessage::makeNoteOff(0, 62));
    }

    CASE("overwrite mode clears previous step when no note history exists") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &project = app.model.project();
        auto &engine = app.engine;
        auto &noteEngine = prepareTwoStepSequence(app);
        auto &sequence = project.track(0).noteTrack().sequence(0);

        project.setSelectedTrackIndex(0);
        project.setRecordMode(Types::RecordMode::Overwrite);
        engine.setRecording(true);

        // Prime known non-empty content in step 0.
        sequence.step(0).setGate(true);
        sequence.step(0).setNote(5);

        // First boundary initializes sequence state; second boundary applies overwrite clear
        // to the previous step when no note history was recorded.
        noteEngine.tick(0);
        noteEngine.tick(sequenceDivisorTicks(sequence));

        expectFalse(sequence.step(0).gate());
        expectEqual(sequence.step(0).note(), 0);
    }

    CASE("condition First triggers only on first iteration, NotFirst on later iterations") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &sequence = app.model.project().track(0).noteTrack().sequence(0);
        auto &engine = app.engine.trackEngine(0).as<NoteTrackEngine>();

        sequence.clearSteps();
        sequence.setFirstStep(0);
        sequence.setLastStep(0);
        auto &step = sequence.step(0);
        step.setGate(true);
        step.setGateProbability(NoteSequence::GateProbability::Max);
        step.setLength(0);
        step.setLengthVariationRange(0);
        step.setLengthVariationProbability(NoteSequence::LengthVariationProbability::Max);
        step.setRetrigger(0);
        step.setRetriggerProbability(NoteSequence::RetriggerProbability::Max);
        step.setCondition(Types::Condition::First);
        engine.reset();

        const uint32_t divisor = sequenceDivisorTicks(sequence);

        auto firstResult = engine.tick(0);
        expectTrue((firstResult & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(engine.gateOutput(0));

        tickRange(engine, 1, divisor - 1);
        auto secondResult = engine.tick(divisor);
        expectFalse((secondResult & TrackEngine::TickResult::GateUpdate) != 0 && engine.gateOutput(0));

        step.setCondition(Types::Condition::NotFirst);
        engine.reset();

        auto notFirstStart = engine.tick(0);
        expectFalse((notFirstStart & TrackEngine::TickResult::GateUpdate) != 0 && engine.gateOutput(0));

        tickRange(engine, 1, divisor - 1);
        auto notFirstSecond = engine.tick(divisor);
        expectTrue((notFirstSecond & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(engine.gateOutput(0));
    }

    CASE("condition Pre follows previous condition state in two-step sequence") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &project = app.model.project();
        auto &sequence = project.track(0).noteTrack().sequence(0);
        auto &engine = app.engine.trackEngine(0).as<NoteTrackEngine>();

        sequence.clearSteps();
        sequence.setFirstStep(0);
        sequence.setLastStep(1);

        auto &step0 = sequence.step(0);
        step0.setGate(true);
        step0.setGateProbability(NoteSequence::GateProbability::Max);
        step0.setLength(0);
        step0.setCondition(Types::Condition::Fill);

        auto &step1 = sequence.step(1);
        step1.setGate(true);
        step1.setGateProbability(NoteSequence::GateProbability::Max);
        step1.setLength(0);
        step1.setCondition(Types::Condition::Pre);

        project.track(0).noteTrack().setFillMode(NoteTrack::FillMode::Condition);
        project.playState().trackState(0).setFillAmount(100);
        project.playState().fillTrack(0, true);
        app.engine.update();
        engine.reset();

        const uint32_t divisor = sequenceDivisorTicks(sequence);

        auto step0Result = engine.tick(0);
        expectTrue((step0Result & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(engine.gateOutput(0));

        tickRange(engine, 1, divisor - 1);
        auto step1Result = engine.tick(divisor);
        expectTrue((step1Result & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(engine.gateOutput(0));
    }

    CASE("condition NotPre inverts previous condition state in two-step sequence") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &project = app.model.project();
        auto &sequence = project.track(0).noteTrack().sequence(0);
        auto &engine = app.engine.trackEngine(0).as<NoteTrackEngine>();

        sequence.clearSteps();
        sequence.setFirstStep(0);
        sequence.setLastStep(1);

        auto &step0 = sequence.step(0);
        step0.setGate(true);
        step0.setGateProbability(NoteSequence::GateProbability::Max);
        step0.setLength(0);
        step0.setCondition(Types::Condition::Fill);

        auto &step1 = sequence.step(1);
        step1.setGate(true);
        step1.setGateProbability(NoteSequence::GateProbability::Max);
        step1.setLength(0);
        step1.setCondition(Types::Condition::NotPre);

        project.track(0).noteTrack().setFillMode(NoteTrack::FillMode::Condition);
        project.playState().trackState(0).setFillAmount(100);
        project.playState().fillTrack(0, true);
        app.engine.update();
        engine.reset();

        const uint32_t divisor = sequenceDivisorTicks(sequence);

        auto step0Result = engine.tick(0);
        expectTrue((step0Result & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(engine.gateOutput(0));

        tickRange(engine, 1, divisor - 1);
        auto step1Result = engine.tick(divisor);
        expectFalse((step1Result & TrackEngine::TickResult::GateUpdate) != 0 && engine.gateOutput(0));
    }

    CASE("condition Loop2 triggers on even iterations") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &sequence = app.model.project().track(0).noteTrack().sequence(0);
        auto &engine = app.engine.trackEngine(0).as<NoteTrackEngine>();

        sequence.clearSteps();
        sequence.setFirstStep(0);
        sequence.setLastStep(0);
        auto &step = sequence.step(0);
        step.setGate(true);
        step.setGateProbability(NoteSequence::GateProbability::Max);
        step.setLength(0);
        step.setCondition(Types::Condition::Loop2);
        engine.reset();

        const uint32_t divisor = sequenceDivisorTicks(sequence);

        auto iter0 = engine.tick(0);
        expectTrue((iter0 & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(engine.gateOutput(0));

        tickRange(engine, 1, divisor - 1);
        auto iter1 = engine.tick(divisor);
        expectFalse((iter1 & TrackEngine::TickResult::GateUpdate) != 0 && engine.gateOutput(0));

        tickRange(engine, divisor + 1, (2 * divisor) - 1);
        auto iter2 = engine.tick(2 * divisor);
        expectTrue((iter2 & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(engine.gateOutput(0));
    }

    CASE("condition NotLoop2 triggers on odd iterations") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &sequence = app.model.project().track(0).noteTrack().sequence(0);
        auto &engine = app.engine.trackEngine(0).as<NoteTrackEngine>();

        sequence.clearSteps();
        sequence.setFirstStep(0);
        sequence.setLastStep(0);
        auto &step = sequence.step(0);
        step.setGate(true);
        step.setGateProbability(NoteSequence::GateProbability::Max);
        step.setLength(0);
        step.setCondition(Types::Condition::NotLoop2);
        engine.reset();

        const uint32_t divisor = sequenceDivisorTicks(sequence);

        auto iter0 = engine.tick(0);
        expectFalse((iter0 & TrackEngine::TickResult::GateUpdate) != 0 && engine.gateOutput(0));

        tickRange(engine, 1, divisor - 1);
        auto iter1 = engine.tick(divisor);
        expectTrue((iter1 & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(engine.gateOutput(0));

        tickRange(engine, divisor + 1, (2 * divisor) - 1);
        auto iter2 = engine.tick(2 * divisor);
        expectFalse((iter2 & TrackEngine::TickResult::GateUpdate) != 0 && engine.gateOutput(0));
    }

    CASE("linked track uses source track link data on tick") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &project = app.model.project();

        auto &sourceEngine = prepareSingleStepSequence(app, 0);
        auto &linkedEngine = prepareSingleStepSequence(app, 1);

        project.track(1).setLinkTrack(0);
        app.engine.update(); // apply linked track setup

        auto sourceResult = sourceEngine.tick(0);
        expectTrue((sourceResult & TrackEngine::TickResult::GateUpdate) != 0);

        auto linkedResult = linkedEngine.tick(0);
        expectTrue((linkedResult & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(linkedEngine.gateOutput(0));
    }

    CASE("slide-enabled step updates CV gradually during update") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &project = app.model.project();
        auto &sequence = project.track(0).noteTrack().sequence(0);
        auto &noteTrack = project.track(0).noteTrack();
        auto &engine = app.engine.trackEngine(0).as<NoteTrackEngine>();

        sequence.clearSteps();
        sequence.setFirstStep(0);
        sequence.setLastStep(0);

        auto &step = sequence.step(0);
        step.setGate(true);
        step.setGateProbability(NoteSequence::GateProbability::Max);
        step.setLength(NoteSequence::Length::Max / 2);
        step.setLengthVariationRange(0);
        step.setLengthVariationProbability(NoteSequence::LengthVariationProbability::Max);
        step.setRetrigger(0);
        step.setRetriggerProbability(NoteSequence::RetriggerProbability::Max);
        step.setGateOffset(0);
        step.setSlide(true);
        step.setNote(24);

        noteTrack.setSlideTime(50);
        engine.reset();

        engine.tick(0);
        float cvBefore = engine.cvOutput(0);

        engine.update(0.05f);
        float cvAfterFirstUpdate = engine.cvOutput(0);
        expectTrue(cvAfterFirstUpdate != cvBefore);

        engine.update(0.05f);
        float cvAfterSecondUpdate = engine.cvOutput(0);
        expectTrue(cvAfterSecondUpdate != cvAfterFirstUpdate);
    }

    CASE("overdub mode records note after deterministic boundary progression") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &project = app.model.project();
        auto &appEngine = app.engine;
        auto &engine = prepareTwoStepSequence(app);
        auto &sequence = project.track(0).noteTrack().sequence(0);
        const uint32_t divisor = sequenceDivisorTicks(sequence);

        project.setSelectedTrackIndex(0);
        project.setRecordMode(Types::RecordMode::Overdub);
        appEngine.setRecording(true);

        // Prepare a known baseline so we can verify that recording overwrites it.
        sequence.step(0).setGate(false);
        sequence.step(0).setNote(0);
        sequence.step(0).setLength(0);
        sequence.step(1).setGate(false);
        sequence.step(1).setNote(0);
        sequence.step(1).setLength(0);

        engine.tick(0);
        engine.monitorMidi(1, MidiMessage::makeNoteOn(0, 72, 100));

        // Cross two boundaries to avoid first-boundary sequence-state ambiguity
        // and deterministically execute recordStep() with a valid prevStep.
        engine.tick(divisor);
        engine.tick(2 * divisor);

        bool wroteStep0 = sequence.step(0).gate() && sequence.step(0).length() > 0;
        bool wroteStep1 = sequence.step(1).gate() && sequence.step(1).length() > 0;
        expectTrue(wroteStep0 || wroteStep1);

        engine.monitorMidi((2 * divisor) + 1, MidiMessage::makeNoteOff(0, 72));
    }
}

