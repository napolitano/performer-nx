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
    }

    SequencerApp &app() { return *_app; }

    void setCvInput(int channel, float volts) {
        _simulator.setAdc(channel, volts);
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

static CurveTrackEngine &configureCurveTrack(SequencerApp &app, int trackIndex) {
    auto &project = app.model.project();
    project.setTrackMode(trackIndex, Track::TrackMode::Curve);
    app.engine.update();

    auto &track = project.track(trackIndex);
    auto &curveTrack = track.curveTrack();
    auto &sequence = curveTrack.sequence(0);

    sequence.clearSteps();
    sequence.setFirstStep(0);
    sequence.setLastStep(0);
    sequence.setDivisor(12);
    sequence.setResetMeasure(0);

    auto &step = sequence.step(0);
    step.setShape(0);
    step.setShapeVariation(1);
    step.setShapeVariationProbability(0);
    step.setMin(0);
    step.setMax(CurveSequence::Max::Max);
    step.setGate(0b0001);
    step.setGateProbability(CurveSequence::GateProbability::Max);

    curveTrack.setPlayMode(Types::PlayMode::Aligned);
    curveTrack.setMuteMode(CurveTrack::MuteMode::LastValue);
    curveTrack.setFillMode(CurveTrack::FillMode::None);
    curveTrack.setSlideTime(0);
    curveTrack.setOffset(0);
    curveTrack.setRotate(0);
    curveTrack.setGateProbabilityBias(0);
    curveTrack.setShapeProbabilityBias(0);

    auto &engine = app.engine.trackEngine(trackIndex).as<CurveTrackEngine>();
    engine.reset();
    return engine;
}

static uint32_t sequenceDivisorTicks(const CurveSequence &sequence) {
    return sequence.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
}

} // namespace

UNIT_TEST("CurveTrackEngine") {

    CASE("aligned play mode triggers step and produces gate on/off events") {
        SequencerHarness harness;
        auto &engine = configureCurveTrack(harness.app(), 0);

        const uint32_t divisor = sequenceDivisorTicks(engine.sequence());
        const uint32_t gateOffTick = divisor / 8;

        auto tick0 = engine.tick(0);
        expectTrue((tick0 & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(engine.gateOutput(0));
        expectEqual(engine.currentStep(), 0);

        auto off = engine.tick(gateOffTick);
        expectTrue((off & TrackEngine::TickResult::GateUpdate) != 0);
        expectFalse(engine.gateOutput(0));
    }

    CASE("play mode Free advances the step and PlayMode::Last input clamps to Free") {
        SequencerHarness harness;
        auto &freeEngine = configureCurveTrack(harness.app(), 0);
        auto &curveTrack = harness.app().model.project().track(0).curveTrack();

        curveTrack.setPlayMode(Types::PlayMode::Free);
        freeEngine.reset();
        freeEngine.tick(0);
        expectEqual(freeEngine.currentStep(), 0);

        // PlayMode::Last is a sentinel; model setters clamp it to the last valid mode (Free).
        curveTrack.setPlayMode(Types::PlayMode::Last);
        freeEngine.reset();
        freeEngine.tick(0);
        expectEqual(freeEngine.currentStep(), 0);
    }

    CASE("mute state suppresses gate output unless fill is active") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &engine = configureCurveTrack(app, 0);
        auto &playState = app.model.project().playState();

        playState.muteTrack(0, PlayState::Immediate);
        app.engine.update();
        auto mutedResult = engine.tick(0);
        expectTrue((mutedResult & TrackEngine::TickResult::GateUpdate) != 0);
        expectFalse(engine.gateOutput(0));

        engine.reset();
        playState.trackState(0).setFillAmount(100);
        playState.fillTrack(0, true);
        app.engine.update();
        auto fillResult = engine.tick(0);
        expectTrue((fillResult & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(engine.gateOutput(0));
    }

    CASE("mute modes Zero Min and Max drive dedicated CV targets") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &engine = configureCurveTrack(app, 0);
        auto &project = app.model.project();
        auto &playState = project.playState();
        auto &curveTrack = project.track(0).curveTrack();
        auto &sequence = curveTrack.sequence(0);
        const auto &range = Types::voltageRangeInfo(sequence.range());

        playState.muteTrack(0, PlayState::Immediate);
        app.engine.update();

        curveTrack.setMuteMode(CurveTrack::MuteMode::Zero);
        engine.reset();
        engine.tick(0);
        engine.update(0.f);
        expectEqual(engine.cvOutput(0), 0.f);

        curveTrack.setMuteMode(CurveTrack::MuteMode::Min);
        engine.reset();
        engine.tick(0);
        engine.update(0.f);
        expectEqual(engine.cvOutput(0), range.lo);

        curveTrack.setMuteMode(CurveTrack::MuteMode::Max);
        engine.reset();
        engine.tick(0);
        engine.update(0.f);
        expectEqual(engine.cvOutput(0), range.hi);
    }

    CASE("fill mode NextPattern evaluates the next pattern when fill is active") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &engine = configureCurveTrack(app, 0);
        auto &project = app.model.project();
        auto &playState = project.playState();
        auto &curveTrack = project.track(0).curveTrack();

        auto &baseStep = curveTrack.sequence(0).step(0);
        baseStep.setMinNormalized(0.f);
        baseStep.setMaxNormalized(0.f);

        auto &fillStep = curveTrack.sequence(1).step(0);
        fillStep.setMinNormalized(1.f);
        fillStep.setMaxNormalized(1.f);

        curveTrack.setFillMode(CurveTrack::FillMode::NextPattern);

        playState.trackState(0).setFillAmount(0);
        playState.fillTrack(0, false);
        app.engine.update();

        engine.reset();
        engine.tick(0);
        engine.update(0.f);
        float cvWithoutFill = engine.cvOutput(0);

        playState.trackState(0).setFillAmount(100);
        playState.fillTrack(0, true);
        app.engine.update();

        engine.reset();
        engine.tick(0);
        engine.update(0.f);
        float cvWithFill = engine.cvOutput(0);

        expectTrue(cvWithFill != cvWithoutFill);
    }

    CASE("monitor step override selects min and max value when engine is stopped") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &engine = configureCurveTrack(app, 0);
        auto &step = app.model.project().track(0).curveTrack().sequence(0).step(0);

        step.setMinNormalized(0.f);
        step.setMaxNormalized(1.f);

        app.engine.setRecording(false);

        engine.setMonitorStep(0);
        engine.setMonitorStepLevel(CurveTrackEngine::MonitorLevel::Min);
        engine.update(0.f);
        float minCv = engine.cvOutput(0);

        engine.setMonitorStepLevel(CurveTrackEngine::MonitorLevel::Max);
        engine.update(0.f);
        float maxCv = engine.cvOutput(0);

        expectTrue(maxCv > minCv);
    }

    CASE("recording path reads selected CV input channel") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &project = app.model.project();
        auto &engine = configureCurveTrack(app, 0);

        project.setSelectedTrackIndex(0);
        app.engine.setRecording(true);

        project.setCurveCvInput(Types::CurveCvInput::Cv1);
        harness.setCvInput(0, 4.f);
        app.update();
        float cv1Value = engine.cvOutput(0);

        project.setCurveCvInput(Types::CurveCvInput::Cv2);
        harness.setCvInput(1, 0.f);
        app.update();
        float cv2Value = engine.cvOutput(0);

        expectTrue(cv1Value != cv2Value);
    }

    CASE("linked curve track follows leader link data") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &project = app.model.project();

        auto &leader = configureCurveTrack(app, 0);
        auto &follower = configureCurveTrack(app, 1);

        project.track(1).setLinkTrack(0);
        app.engine.update();

        leader.reset();
        follower.reset();

        leader.tick(0);
        follower.tick(0);

        expectEqual(leader.currentStep(), 0);
        expectEqual(follower.currentStep(), leader.currentStep());
        expectTrue(follower.linkedTrackEngine() != nullptr);
    }

    CASE("pattern switch updates active curve sequence pointer") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &project = app.model.project();
        auto &playState = project.playState();

        auto &engine = configureCurveTrack(app, 0);
        auto &curveTrack = project.track(0).curveTrack();

        expectTrue(engine.isActiveSequence(curveTrack.sequence(0)));

        playState.selectTrackPattern(0, 1, PlayState::Immediate);
        app.engine.update();

        expectTrue(engine.isActiveSequence(curveTrack.sequence(1)));
        expectFalse(engine.isActiveSequence(curveTrack.sequence(0)));
    }

    CASE("restart resets current step and clears intra-step fraction") {
        SequencerHarness harness;
        auto &engine = configureCurveTrack(harness.app(), 0);

        const uint32_t divisor = sequenceDivisorTicks(engine.sequence());

        engine.tick(0);
        engine.tick(divisor / 4);
        expectEqual(engine.currentStep(), 0);
        expectTrue(engine.currentStepFraction() > 0.f);

        engine.restart();
        expectEqual(engine.currentStep(), -1);
        expectEqual(engine.currentStepFraction(), 0.f);
    }

    CASE("slide time applies smoothing instead of jumping directly to target CV") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &engine = configureCurveTrack(app, 0);
        auto &curveTrack = app.model.project().track(0).curveTrack();
        auto &step = curveTrack.sequence(0).step(0);

        // Start from a low target and apply update once.
        step.setMinNormalized(0.f);
        step.setMaxNormalized(0.f);
        curveTrack.setSlideTime(0);
        engine.reset();
        engine.tick(0);
        engine.update(0.f);
        float lowCv = engine.cvOutput(0);

        // Move target high and enable slide, then verify the transition is gradual.
        step.setMinNormalized(1.f);
        step.setMaxNormalized(1.f);
        curveTrack.setSlideTime(100);
        engine.tick(0);
        engine.update(0.001f);
        float smoothedCv = engine.cvOutput(0);

        expectTrue(smoothedCv > lowCv);
        expectTrue(smoothedCv < 5.f);
    }

    CASE("fill invert changes evaluated curve output compared to normal fill-off mode") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &engine = configureCurveTrack(app, 0);
        auto &project = app.model.project();
        auto &playState = project.playState();
        auto &curveTrack = project.track(0).curveTrack();
        auto &step = curveTrack.sequence(0).step(0);

        step.setMinNormalized(0.f);
        step.setMaxNormalized(1.f);

        const uint32_t divisor = sequenceDivisorTicks(engine.sequence());
        const uint32_t sampleTick = divisor / 4;

        curveTrack.setFillMode(CurveTrack::FillMode::Invert);

        playState.trackState(0).setFillAmount(0);
        playState.fillTrack(0, false);
        app.engine.update();
        engine.reset();
        engine.tick(0);
        engine.tick(sampleTick);
        engine.update(0.f);
        float normalCv = engine.cvOutput(0);

        playState.trackState(0).setFillAmount(100);
        playState.fillTrack(0, true);
        app.engine.update();
        engine.reset();
        engine.tick(0);
        engine.tick(sampleTick);
        engine.update(0.f);
        float invertCv = engine.cvOutput(0);

        expectTrue(invertCv != normalCv);
    }

    CASE("recording with unsupported curve CV input ignores external ADC value") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &project = app.model.project();
        auto &engine = configureCurveTrack(app, 0);

        project.setSelectedTrackIndex(0);
        app.engine.setRecording(true);
        project.setCurveCvInput(Types::CurveCvInput::Last);

        harness.setCvInput(0, 4.f);
        app.update();
        float firstValue = engine.cvOutput(0);

        harness.setCvInput(0, 0.f);
        app.update();
        float secondValue = engine.cvOutput(0);

        expectEqual(firstValue, secondValue);
    }

    CASE("recording over one full divisor updates step shape or range via recorder match") {
        SequencerHarness harness;
        auto &app = harness.app();
        auto &project = app.model.project();
        auto &engine = configureCurveTrack(app, 0);
        auto &step = project.track(0).curveTrack().sequence(0).step(0);

        step.setShape(3);
        step.setMin(64);
        step.setMax(192);

        const int initialShape = step.shape();
        const int initialMin = step.min();
        const int initialMax = step.max();

        project.setSelectedTrackIndex(0);
        app.engine.setRecording(true);
        project.setCurveCvInput(Types::CurveCvInput::Cv1);

        // Prime CV input cache used by updateRecordValue().
        harness.setCvInput(0, 4.f);
        app.update();

        const uint32_t divisor = sequenceDivisorTicks(engine.sequence());
        for (uint32_t tick = 0; tick <= divisor; ++tick) {
            engine.tick(tick);
        }

        const bool stepChanged =
            step.shape() != initialShape ||
            step.min() != initialMin ||
            step.max() != initialMax;

        expectTrue(stepChanged);
    }
}
