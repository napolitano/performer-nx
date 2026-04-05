#include "UnitTest.h"

#include "apps/sequencer/SequencerApp.h"
#include "apps/sequencer/model/NoteSequence.h"
#include "engine/Groove.h"
#include "model/ProjectVersion.h"

#include "sim/Simulator.h"

#include "../core/io/MemoryReaderWriter.h"
#include "core/io/VersionedSerializedWriter.h"
#include "core/io/VersionedSerializedReader.h"

#include <cstring>
#include <memory>

// ─── Harness ──────────────────────────────────────────────────────────────────

namespace {

class SequencerHarness {
public:
    SequencerHarness() : _simulator(makeTarget()) { _simulator.reboot(); }
    SequencerApp &app() { return *_app; }

private:
    sim::Target makeTarget() {
        sim::Target t;
        t.create  = [this] () { _app.reset(new SequencerApp()); };
        t.destroy = [this] () { _app.reset(); };
        t.update  = [this] () { _app->update(); };
        return t;
    }
    std::unique_ptr<SequencerApp> _app;
    sim::Simulator _simulator;
};

// Configure track 0 as a single-step sequence.
// gateActive/length/gateOffset are fully controllable.
// All probability fields are set to Max so evaluation is deterministic.
static NoteTrackEngine &prepareSingleStepEngine(
        SequencerApp &app,
        int  gateOffset,
        int  length     = NoteSequence::Length::Max,
        bool gateActive = true,
        int  retrigger  = 0)
{
    auto &seq = app.model.project().track(0).noteTrack().sequence(0);
    seq.clearSteps();
    seq.setFirstStep(0);
    seq.setLastStep(0);

    auto &step = seq.step(0);
    step.setGate(gateActive);
    step.setGateProbability(NoteSequence::GateProbability::Max);
    step.setLength(length);
    step.setLengthVariationRange(0);
    step.setLengthVariationProbability(NoteSequence::LengthVariationProbability::Max);
    step.setRetrigger(retrigger);
    step.setRetriggerProbability(NoteSequence::RetriggerProbability::Max);
    step.setGateOffset(gateOffset);

    auto &eng = app.engine.trackEngine(0).as<NoteTrackEngine>();
    eng.reset();
    return eng;
}

// Tick at which gate-on fires for a given gateOffset value (no swing).
static int32_t gateOnTick(const NoteSequence &seq, int gateOffset) {
    int32_t divisor = seq.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
    return (divisor * gateOffset) / (NoteSequence::GateOffset::Max + 1);
}

// Tick duration of the gate (length-based, no variation, no swing).
static uint32_t gateLength(const NoteSequence &seq, int length) {
    uint32_t divisor = seq.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
    return (divisor * (length + 1)) / NoteSequence::Length::Range;
}

// Two-step sequence: both steps have gate active.
// step 0 gets offset0, step 1 gets offset1, both use the same length.
static NoteTrackEngine &prepareTwoStepEngine(SequencerApp &app, int offset0, int offset1,
                                              int length = 3) {
    auto &seq = app.model.project().track(0).noteTrack().sequence(0);
    seq.clearSteps();
    seq.setFirstStep(0);
    seq.setLastStep(1);

    for (int i = 0; i <= 1; ++i) {
        auto &step = seq.step(i);
        step.setGate(true);
        step.setGateProbability(NoteSequence::GateProbability::Max);
        step.setLength(length);
        step.setLengthVariationRange(0);
        step.setLengthVariationProbability(NoteSequence::LengthVariationProbability::Max);
        step.setRetrigger(0);
        step.setRetriggerProbability(NoteSequence::RetriggerProbability::Max);
        step.setGateOffset(i == 0 ? offset0 : offset1);
    }

    auto &eng = app.engine.trackEngine(0).as<NoteTrackEngine>();
    eng.reset();
    return eng;
}

} // namespace

// ─── Tests ────────────────────────────────────────────────────────────────────

UNIT_TEST("NoteTrackEngineGateOffset") {

    // ── 1. Model: setGateOffset input clamping ─────────────────────────────

    CASE("Step::setGateOffset stores negative values down to GateOffset::Min") {
        NoteSequence::Step step;
        step.clear();

        step.setGateOffset(-1);
        expectEqual(step.gateOffset(), -1);

        step.setGateOffset(NoteSequence::GateOffset::Min); // -7
        expectEqual(step.gateOffset(), NoteSequence::GateOffset::Min);

        step.setGateOffset(-99);
        expectEqual(step.gateOffset(), NoteSequence::GateOffset::Min);
    }

    CASE("Step::setGateOffset stores every value in [Min..Max] without loss") {
        NoteSequence::Step step;
        step.clear();
        for (int v = NoteSequence::GateOffset::Min; v <= NoteSequence::GateOffset::Max; ++v) {
            step.setGateOffset(v);
            expectEqual(step.gateOffset(), v);
        }
    }

    CASE("Step::setGateOffset clamps positive overflow to Max") {
        NoteSequence::Step step;
        step.clear();

        step.setGateOffset(NoteSequence::GateOffset::Max + 1);
        expectEqual(step.gateOffset(), NoteSequence::GateOffset::Max);

        step.setGateOffset(99);
        expectEqual(step.gateOffset(), NoteSequence::GateOffset::Max);
    }

    CASE("Step::clear resets gateOffset to 0") {
        NoteSequence::Step step;
        step.setGateOffset(NoteSequence::GateOffset::Max);
        step.clear();
        expectEqual(step.gateOffset(), 0);
    }

    // ── 2. Model: layer range / default ────────────────────────────────────

    CASE("layerRange GateOffset min equals GateOffset::Min") {
        auto range = NoteSequence::layerRange(NoteSequence::Layer::GateOffset);
        expectEqual(range.min, NoteSequence::GateOffset::Min);
    }

    CASE("layerRange GateOffset max equals GateOffset::Max") {
        auto range = NoteSequence::layerRange(NoteSequence::Layer::GateOffset);
        expectEqual(range.max, NoteSequence::GateOffset::Max);
    }

    CASE("layerDefaultValue GateOffset is 0") {
        expectEqual(NoteSequence::layerDefaultValue(NoteSequence::Layer::GateOffset), 0);
    }

    CASE("setLayerValue / layerValue round-trip for all valid GateOffset values") {
        NoteSequence::Step step;
        step.clear();
        for (int v = NoteSequence::GateOffset::Min; v <= NoteSequence::GateOffset::Max; ++v) {
            step.setLayerValue(NoteSequence::Layer::GateOffset, v);
            expectEqual(step.layerValue(NoteSequence::Layer::GateOffset), v);
        }
    }

    // ── 3. Engine: gate-on timing for every offset value ──────────────────

    CASE("gateOffset 0: GateUpdate fires at tick 0, gate is high") {
        SequencerHarness h;
        auto &eng = prepareSingleStepEngine(h.app(), 0);

        auto r = eng.tick(0);
        expectTrue((r & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(eng.gateOutput(0));
    }

    CASE("every gateOffset value fires GateUpdate exactly at its expected tick") {
        for (int offset = 0; offset <= NoteSequence::GateOffset::Max; ++offset) {
            SequencerHarness h;
            auto &eng    = prepareSingleStepEngine(h.app(), offset);
            uint32_t on  = gateOnTick(eng.sequence(), offset);

            // No gate-on fires before the expected tick
            for (uint32_t t = 0; t < on; ++t) {
                auto r = eng.tick(t);
                expectFalse((r & TrackEngine::TickResult::GateUpdate) != 0 && eng.gateOutput(0));
            }

            // Gate-on fires exactly at the expected tick
            auto r = eng.tick(on);
            expectTrue((r & TrackEngine::TickResult::GateUpdate) != 0);
            expectTrue(eng.gateOutput(0));
        }
    }

    // ── 4. Engine: negative input is silently clamped ─────────────────────

    CASE("negative gateOffset: model stores the signed value") {
        NoteSequence::Step step;
        step.clear();
        step.setGateOffset(-5);
        expectEqual(step.gateOffset(), -5);
    }

    CASE("negative gateOffset on first step starts at tick 0") {
        SequencerHarness h;
        auto &eng = prepareSingleStepEngine(h.app(), -5);

        auto r = eng.tick(0);
        expectTrue((r & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(eng.gateOutput(0));
    }

    CASE("negative gateOffset pre-triggers only on subsequent loop passes") {
        SequencerHarness h;
        const int offset = -4;
        auto &eng = prepareSingleStepEngine(h.app(), offset, 0);
        const uint32_t divisor = eng.sequence().divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
        const uint32_t lookAheadOn = divisor + gateOnTick(eng.sequence(), offset);

        // Initial start cannot pre-trigger before tick 0.
        auto r = eng.tick(0);
        expectTrue((r & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(eng.gateOutput(0));

        for (uint32_t t = 1; t < lookAheadOn; ++t) {
            eng.tick(t);
        }

        // Next cycle starts with a proper negative-offset pre-trigger.
        r = eng.tick(lookAheadOn);
        expectTrue((r & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(eng.gateOutput(0));

        // No additional gate-on should be emitted at the cycle boundary itself.
        r = eng.tick(divisor);
        expectFalse((r & TrackEngine::TickResult::GateUpdate) != 0 && eng.gateOutput(0));
    }

    // ── 5. Engine: CV co-arrives with gate-on ─────────────────────────────

    CASE("CV update and GateUpdate arrive on the same tick for gateOffset 0") {
        SequencerHarness h;
        auto &eng = prepareSingleStepEngine(h.app(), 0);

        auto r = eng.tick(0);
        expectTrue((r & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue((r & TrackEngine::TickResult::CvUpdate)   != 0);
    }

    CASE("CV update is absent before gate-on tick for gateOffset Max") {
        SequencerHarness h;
        auto &eng = prepareSingleStepEngine(h.app(), NoteSequence::GateOffset::Max);
        uint32_t on = gateOnTick(eng.sequence(), NoteSequence::GateOffset::Max);

        for (uint32_t t = 0; t < on; ++t) {
            auto r = eng.tick(t);
            expectFalse((r & TrackEngine::TickResult::CvUpdate) != 0);
        }

        auto r = eng.tick(on);
        expectTrue((r & TrackEngine::TickResult::CvUpdate) != 0);
    }

    CASE("no CV update when gate is inactive") {
        SequencerHarness h;
        auto &eng = prepareSingleStepEngine(h.app(), 0, NoteSequence::Length::Max, /*gateActive=*/false);

        auto r = eng.tick(0);
        expectFalse((r & TrackEngine::TickResult::CvUpdate) != 0);
    }

    // ── 6. Engine: gate-off timing ─────────────────────────────────────────
    // Use length=3 (stepLen = divisor*4/8 = 24 ticks) so gate-off lands
    // well before the next step boundary at tick 48 — avoids step-boundary
    // overlap that would cause the next step's gate-on to swallow the test.

    CASE("gate-off fires exactly at gateOnTick + stepLength for gateOffset 0") {
        SequencerHarness h;
        const int  testLen = 3; // stepLen = 24
        auto &eng = prepareSingleStepEngine(h.app(), 0, testLen);
        uint32_t len = gateLength(eng.sequence(), testLen); // 24

        eng.tick(0);
        expectTrue(eng.gateOutput(0));

        for (uint32_t t = 1; t < len; ++t) { eng.tick(t); }
        expectTrue(eng.gateOutput(0)); // still high at tick 23

        auto r = eng.tick(len);        // tick 24
        expectTrue((r & TrackEngine::TickResult::GateUpdate) != 0);
        expectFalse(eng.gateOutput(0));
    }

    CASE("gate-off fires at gateOnTick + stepLength for gateOffset Max") {
        SequencerHarness h;
        const int testLen = 3;
        auto &eng = prepareSingleStepEngine(h.app(), NoteSequence::GateOffset::Max, testLen);
        uint32_t on  = gateOnTick(eng.sequence(), NoteSequence::GateOffset::Max); // 42
        uint32_t len = gateLength(eng.sequence(), testLen);                        // 24
        uint32_t off = on + len;                                                   // 66

        for (uint32_t t = 0; t <= on; ++t) eng.tick(t);
        expectTrue(eng.gateOutput(0));

        for (uint32_t t = on + 1; t < off; ++t) { eng.tick(t); }
        expectTrue(eng.gateOutput(0)); // still high

        auto r = eng.tick(off);
        expectTrue((r & TrackEngine::TickResult::GateUpdate) != 0);
        expectFalse(eng.gateOutput(0));
    }

    CASE("minimum gate length (length=0) still fires a gate-off event") {
        SequencerHarness h;
        auto &eng = prepareSingleStepEngine(h.app(), 0, 0); // stepLen = 6 ticks
        uint32_t len = gateLength(eng.sequence(), 0);       // 6

        eng.tick(0);
        expectTrue(eng.gateOutput(0));

        for (uint32_t t = 1; t < len; ++t) { eng.tick(t); }

        auto r = eng.tick(len);
        expectTrue((r & TrackEngine::TickResult::GateUpdate) != 0);
        expectFalse(eng.gateOutput(0));
    }

    // ── 7. Engine: inactive gate ───────────────────────────────────────────

    CASE("inactive gate step: no GateUpdate, no CvUpdate, no gate output for a full divisor window") {
        SequencerHarness h;
        auto &eng = prepareSingleStepEngine(h.app(), 0, NoteSequence::Length::Max, /*gateActive=*/false);
        const uint32_t divisor = eng.sequence().divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);

        for (uint32_t t = 0; t <= divisor; ++t) {
            auto r = eng.tick(t);
            expectFalse((r & TrackEngine::TickResult::GateUpdate) != 0);
            expectFalse((r & TrackEngine::TickResult::CvUpdate)   != 0);
            expectFalse(eng.gateOutput(0));
        }
    }

    // ── 8. Serialization roundtrip ─────────────────────────────────────────

    CASE("Step: gateOffset Min..Max survives serialize/deserialize at latest project version") {
        uint8_t buf[32];

        for (int v = NoteSequence::GateOffset::Min; v <= NoteSequence::GateOffset::Max; ++v) {
            NoteSequence::Step writeStep;
            writeStep.clear();
            writeStep.setGateOffset(v);

            std::memset(buf, 0, sizeof(buf));
            MemoryWriter memWriter(buf, sizeof(buf));
            VersionedSerializedWriter writer(
                [&memWriter](const void *d, size_t len) { memWriter.write(d, len); },
                ProjectVersion::Latest);
            writeStep.write(writer);

            NoteSequence::Step readStep;
            readStep.clear();
            MemoryReader memReader(buf, sizeof(buf));
            VersionedSerializedReader reader(
                [&memReader](void *d, size_t len) { memReader.read(d, len); },
                ProjectVersion::Latest);
            readStep.read(reader);

            expectEqual(readStep.gateOffset(), v);
        }
    }

    // ── 9. Engine reset ────────────────────────────────────────────────────

    CASE("engine reset immediately clears gate output, activity, and CV output") {
        SequencerHarness h;
        auto &eng = prepareSingleStepEngine(h.app(), 0);

        eng.tick(0);                  // gate fires → gate output high
        expectTrue(eng.gateOutput(0));
        expectTrue(eng.activity());

        eng.reset();
        expectFalse(eng.gateOutput(0));
        expectFalse(eng.activity());
        expectEqual(eng.cvOutput(0), 0.0f);
    }

    // ── 10. activity() lifecycle ───────────────────────────────────────────

    CASE("activity() mirrors gateOutput() throughout gate on/off lifecycle") {
        SequencerHarness h;
        const int testLen = 3;
        auto &eng = prepareSingleStepEngine(h.app(), 0, testLen);
        uint32_t len = gateLength(eng.sequence(), testLen); // 24

        eng.tick(0);
        expectTrue(eng.gateOutput(0));
        expectTrue(eng.activity());

        for (uint32_t t = 1; t < len; ++t) eng.tick(t);
        expectTrue(eng.activity()); // still on at tick 23

        eng.tick(len); // gate-off at tick 24
        expectFalse(eng.gateOutput(0));
        expectFalse(eng.activity());
    }

    // ── 11. Multi-step: offsets are per-step, not global ──────────────────

    CASE("two-step sequence: each step fires at its own gateOffset relative to its boundary") {
        SequencerHarness h;
        // Step 0: offset=0  → gate-on at tick 0
        // Step 1: offset=Max → gate-on at tick 48 + 42 = 90
        auto &eng = prepareTwoStepEngine(h.app(), 0, NoteSequence::GateOffset::Max);
        const uint32_t divisor = eng.sequence().divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
        uint32_t step1On = divisor + gateOnTick(eng.sequence(), NoteSequence::GateOffset::Max);

        // Step 0 gate fires at tick 0
        auto r0 = eng.tick(0);
        expectTrue((r0 & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(eng.gateOutput(0));

        // Advance to one tick before step 1's gate-on; gate must be low
        // (step 0's gate-off already fired at its stepLen, no new gate-on until step1On)
        for (uint32_t t = 1; t < step1On; ++t) eng.tick(t);
        expectFalse(eng.gateOutput(0));

        // Step 1 gate fires exactly at step1On
        auto r1 = eng.tick(step1On);
        expectTrue((r1 & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(eng.gateOutput(0));
    }

    // ── 12. Divisor scaling ────────────────────────────────────────────────

    CASE("gate-on timing scales proportionally when sequence divisor is doubled") {
        SequencerHarness h;
        // Default divisor=12 → 48 ticks/step. Double to 24 → 96 ticks/step.
        h.app().model.project().track(0).noteTrack().sequence(0).setDivisor(24);
        auto &eng = prepareSingleStepEngine(h.app(), 3); // offset=3 → (96*3)/8 = 36
        uint32_t on = gateOnTick(eng.sequence(), 3);     // must equal 36

        for (uint32_t t = 0; t < on; ++t) {
            auto r = eng.tick(t);
            expectFalse((r & TrackEngine::TickResult::GateUpdate) != 0 && eng.gateOutput(0));
        }
        auto r = eng.tick(on);
        expectTrue((r & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(eng.gateOutput(0));
    }

    // ── 13. CvUpdateMode::Always interaction ───────────────────────────────

    CASE("CvUpdateMode::Always: CV updates at gateOffset tick even when gate is inactive") {
        SequencerHarness h;
        auto &project = h.app().model.project();
        project.track(0).noteTrack().setCvUpdateMode(NoteTrack::CvUpdateMode::Always);

        auto &eng = prepareSingleStepEngine(h.app(), NoteSequence::GateOffset::Max, NoteSequence::Length::Max, /*gateActive=*/false);
        uint32_t on = gateOnTick(eng.sequence(), NoteSequence::GateOffset::Max);

        for (uint32_t t = 0; t < on; ++t) {
            auto r = eng.tick(t);
            expectFalse((r & TrackEngine::TickResult::CvUpdate) != 0);
            expectFalse((r & TrackEngine::TickResult::GateUpdate) != 0);
        }

        auto r = eng.tick(on);
        expectTrue((r & TrackEngine::TickResult::CvUpdate) != 0);
        expectFalse((r & TrackEngine::TickResult::GateUpdate) != 0);
        expectFalse(eng.gateOutput(0));
    }

    // ── 14. Retrigger interaction ──────────────────────────────────────────

    CASE("retrigger with gateOffset: first and second pulses occur at expected delayed ticks") {
        SequencerHarness h;
        auto &eng = prepareSingleStepEngine(h.app(), 3, NoteSequence::Length::Max, /*gateActive=*/true, /*retrigger=*/1);
        uint32_t divisor = eng.sequence().divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
        uint32_t on0 = gateOnTick(eng.sequence(), 3);
        uint32_t on1 = on0 + divisor / 2;

        for (uint32_t t = 0; t < on0; ++t) {
            auto r = eng.tick(t);
            expectFalse((r & TrackEngine::TickResult::GateUpdate) != 0 && eng.gateOutput(0));
        }

        auto r = eng.tick(on0);
        expectTrue((r & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(eng.gateOutput(0));

        for (uint32_t t = on0 + 1; t < on1; ++t) {
            eng.tick(t);
        }

        r = eng.tick(on1);
        expectTrue((r & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(eng.gateOutput(0));
    }

    // ── 15. Swing interaction ──────────────────────────────────────────────

    CASE("swing shifts gate event timing from raw gateOffset tick") {
        SequencerHarness h;
        auto &project = h.app().model.project();
        project.setSwing(75);

        auto &eng = prepareSingleStepEngine(h.app(), NoteSequence::GateOffset::Max);
        uint32_t rawOn = gateOnTick(eng.sequence(), NoteSequence::GateOffset::Max);
        uint32_t swungOn = Groove::applySwing(rawOn, project.swing());

        for (uint32_t t = 0; t < swungOn; ++t) {
            auto r = eng.tick(t);
            if (t == rawOn) {
                expectFalse((r & TrackEngine::TickResult::GateUpdate) != 0);
            }
        }

        auto r = eng.tick(swungOn);
        expectTrue((r & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(eng.gateOutput(0));
    }

    // ── 16. Fill mode interaction ──────────────────────────────────────────

    CASE("fill mode Gates injects gate at gateOffset tick even when step gate is off") {
        SequencerHarness h;
        auto &project = h.app().model.project();
        project.playState().fillTrack(0, true);

        auto &eng = prepareSingleStepEngine(h.app(), NoteSequence::GateOffset::Max, NoteSequence::Length::Max, /*gateActive=*/false);
        uint32_t on = gateOnTick(eng.sequence(), NoteSequence::GateOffset::Max);

        for (uint32_t t = 0; t < on; ++t) {
            auto r = eng.tick(t);
            expectFalse((r & TrackEngine::TickResult::GateUpdate) != 0 && eng.gateOutput(0));
        }

        auto r = eng.tick(on);
        expectTrue((r & TrackEngine::TickResult::GateUpdate) != 0);
        expectTrue(eng.gateOutput(0));
    }

    // ── 17. Mute interaction ───────────────────────────────────────────────

    CASE("muted track suppresses gate/cv outputs while gate event still gets processed") {
        SequencerHarness h;
        auto &project = h.app().model.project();
        auto &eng = prepareSingleStepEngine(h.app(), 0);

        project.playState().muteTrack(0);
        // Engine update applies immediate mute requests to track state.
        h.app().engine.update();
        expectTrue(project.playState().trackState(0).mute());

        auto r = eng.tick(0);
        expectTrue((r & TrackEngine::TickResult::GateUpdate) != 0);
        expectFalse((r & TrackEngine::TickResult::CvUpdate) != 0);
        expectFalse(eng.gateOutput(0));
    }

}







