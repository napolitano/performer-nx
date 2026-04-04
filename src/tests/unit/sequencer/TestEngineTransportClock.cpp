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

    void setResetInput(bool high) {
        _simulator.setDio(1, high);
    }

    void setClockInput(bool high) {
        _simulator.setDio(0, high);
    }

    void pulseClockInput() {
        // A valid external pulse is a low->high->low edge sequence.
        setClockInput(true);
        setClockInput(false);
    }

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

static void forceMasterClockMode(SequencerApp &app) {
    auto &clockSetup = app.model.project().clockSetup();
    clockSetup.setMode(ClockSetup::Mode::Master);
    app.engine.update();
}

static void configureSlaveClockInputMode(SequencerApp &app, ClockSetup::ClockInputMode mode) {
    auto &clockSetup = app.model.project().clockSetup();
    clockSetup.setMode(ClockSetup::Mode::Slave);
    clockSetup.setClockInputMode(mode);
    // Ensure the updated setup is applied before input edges are simulated.
    app.engine.update();
}

static void configureSlaveClockInputMode(SequencerHarness &harness, ClockSetup::ClockInputMode mode) {
    configureSlaveClockInputMode(harness.app(), mode);
    harness.setResetInput(false);
    harness.setClockInput(false);
}

} // namespace

UNIT_TEST("EngineTransportClock") {

    CASE("clockStart enters running state in master mode") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;

        forceMasterClockMode(harness.app());
        engine.clockStart();
        engine.update();

        expectTrue(engine.clockRunning());
    }

    CASE("clockReset stops running state after explicit start") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;

        forceMasterClockMode(harness.app());
        engine.clockStart();
        engine.update();
        expectTrue(engine.clockRunning());

        engine.clockReset();
        engine.update();

        expectFalse(engine.clockRunning());
    }

    CASE("shift pause toggle alternates between stop and continue") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &clockSetup = harness.app().model.project().clockSetup();

        forceMasterClockMode(harness.app());
        clockSetup.setShiftMode(ClockSetup::ShiftMode::Pause);
        engine.update();

        engine.clockStart();
        engine.update();
        expectTrue(engine.clockRunning());

        engine.togglePlay(true);
        engine.update();
        expectFalse(engine.clockRunning());

        engine.togglePlay(true);
        engine.update();
        expectTrue(engine.clockRunning());
    }

    CASE("clock input divisor change is consumed by engine update") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;
        auto &clockSetup = harness.app().model.project().clockSetup();

        clockSetup.clearDirty();
        expectFalse(clockSetup.isDirty());

        clockSetup.setClockInputDivisor(16);
        expectTrue(clockSetup.isDirty());

        engine.update();
        expectFalse(clockSetup.isDirty());
    }

    CASE("clock input mode Run follows reset input level for stop and continue") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;

        configureSlaveClockInputMode(harness.app(), ClockSetup::ClockInputMode::Run);

        // High reset input in Run mode means continue/running.
        harness.setResetInput(true);
        engine.update();
        expectTrue(engine.clockRunning());

        // Repeating the same high level must not change running state.
        harness.setResetInput(true);
        engine.update();
        expectTrue(engine.clockRunning());

        // Low reset input in Run mode means stop.
        harness.setResetInput(false);
        engine.update();
        expectFalse(engine.clockRunning());

        // Repeating the same low level must keep the engine stopped.
        harness.setResetInput(false);
        engine.update();
        expectFalse(engine.clockRunning());
    }

    CASE("clock input mode StartStop starts on high and stops on low") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;

        configureSlaveClockInputMode(harness.app(), ClockSetup::ClockInputMode::StartStop);

        harness.setResetInput(true);
        engine.update();
        expectTrue(engine.clockRunning());

        // Repeating high in StartStop mode must keep running state stable.
        harness.setResetInput(true);
        engine.update();
        expectTrue(engine.clockRunning());

        harness.setResetInput(false);
        engine.update();
        expectFalse(engine.clockRunning());

        // Repeating low in StartStop mode must keep stopped state stable.
        harness.setResetInput(false);
        engine.update();
        expectFalse(engine.clockRunning());
    }

    CASE("clock input mode Reset starts clock on first pulse while reset is low") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;

        configureSlaveClockInputMode(harness, ClockSetup::ClockInputMode::Reset);
        engine.update();
        expectFalse(engine.clockRunning());

        harness.pulseClockInput();
        engine.update();
        expectTrue(engine.clockRunning());
    }

    CASE("external clock pulse advances sequencer ticks in reset mode") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;

        configureSlaveClockInputMode(harness, ClockSetup::ClockInputMode::Reset);
        uint32_t tickBefore = engine.tick();

        harness.pulseClockInput();
        harness.waitMs(300);

        uint32_t tickAfter = engine.tick();
        expectTrue(tickAfter > tickBefore);
    }

    CASE("run mode resume allows subsequent clock pulses to progress ticks") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;

        configureSlaveClockInputMode(harness, ClockSetup::ClockInputMode::Run);

        harness.setResetInput(true);
        engine.update();
        expectTrue(engine.clockRunning());

        harness.pulseClockInput();
        harness.waitMs(300);
        uint32_t tickWhileRunning = engine.tick();
        expectTrue(tickWhileRunning > 0);

        harness.setResetInput(false);
        engine.update();
        expectFalse(engine.clockRunning());

        harness.setResetInput(true);
        engine.update();
        expectTrue(engine.clockRunning());

        uint32_t tickBeforeResumePulse = engine.tick();
        harness.pulseClockInput();
        harness.waitMs(300);
        expectTrue(engine.tick() > tickBeforeResumePulse);
    }
}


