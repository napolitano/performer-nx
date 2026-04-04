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

    void setResetInput(bool high) { _simulator.setDio(1, high); }
    void setClockInput(bool high) { _simulator.setDio(0, high); }
    void waitMs(int ms) { _simulator.wait(ms); }

    void pulseClockInput() {
        setClockInput(true);
        setClockInput(false);
    }

private:
    sim::Target makeTarget() {
        sim::Target target;
        target.create = [this] () { _app.reset(new SequencerApp()); };
        target.destroy = [this] () { _app.reset(); };
        target.update = [this] () { _app->update(); };
        return target;
    }

    std::unique_ptr<SequencerApp> _app;
    sim::Simulator _simulator;
};

static void configureExternalResetMode(SequencerHarness &harness, ClockSetup::Mode mode, int inputDivisor = 12) {
    auto &clockSetup = harness.app().model.project().clockSetup();
    clockSetup.setMode(mode);
    clockSetup.setClockInputMode(ClockSetup::ClockInputMode::Reset);
    clockSetup.setClockInputDivisor(inputDivisor);

    harness.setResetInput(false);
    harness.setClockInput(false);
    harness.app().engine.update();
}

static uint32_t runPulseTrain(SequencerHarness &harness, int pulses, int pulseSpacingMs) {
    auto &engine = harness.app().engine;
    for (int i = 0; i < pulses; ++i) {
        harness.pulseClockInput();
        harness.waitMs(pulseSpacingMs);
    }
    return engine.tick();
}

} // namespace

UNIT_TEST("ClockExternalTiming") {

    CASE("auto mode stops after external pulses time out") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;

        configureExternalResetMode(harness, ClockSetup::Mode::Auto);
        harness.pulseClockInput();
        engine.update();
        expectTrue(engine.clockRunning());

        harness.waitMs(700);
        engine.update();
        expectFalse(engine.clockRunning());
    }

    CASE("auto mode re-locks when external pulses return after timeout") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;

        configureExternalResetMode(harness, ClockSetup::Mode::Auto);
        harness.pulseClockInput();
        harness.waitMs(40);
        expectTrue(engine.clockRunning());

        harness.waitMs(700);
        engine.update();
        expectFalse(engine.clockRunning());

        uint32_t tickBeforeRelock = engine.tick();
        harness.pulseClockInput();
        harness.waitMs(60);
        engine.update();

        expectTrue(engine.clockRunning());
        // Relock may restart the tick base, but it must produce a different tick state.
        expectTrue(engine.tick() != tickBeforeRelock);
    }

    CASE("slave mode keeps running stable with moderate pulse jitter") {
        SequencerHarness harness;
        auto &engine = harness.app().engine;

        configureExternalResetMode(harness, ClockSetup::Mode::Slave);

        const int pulseIntervalsMs[] = { 20, 24, 19, 23, 21, 22, 20, 24 };
        harness.pulseClockInput();
        harness.waitMs(30);

        uint32_t tickBefore = engine.tick();
        for (int interval : pulseIntervalsMs) {
            harness.pulseClockInput();
            harness.waitMs(interval);
            expectTrue(engine.clockRunning());
        }

        uint32_t tickAfter = engine.tick();
        expectTrue(tickAfter > tickBefore);
        expectTrue(engine.tempo() > 30.f);
        expectTrue(engine.tempo() < 300.f);
    }

    CASE("higher clock input divisor yields more internal ticks per pulse train") {
        uint32_t lowDivTicks = 0;
        {
            SequencerHarness lowDivHarness;
            configureExternalResetMode(lowDivHarness, ClockSetup::Mode::Slave, 6);
            lowDivTicks = runPulseTrain(lowDivHarness, 6, 40);
        }

        uint32_t midDivTicks = 0;
        {
            SequencerHarness midDivHarness;
            configureExternalResetMode(midDivHarness, ClockSetup::Mode::Slave, 12);
            midDivTicks = runPulseTrain(midDivHarness, 6, 40);
        }

        uint32_t highDivTicks = 0;
        {
            SequencerHarness highDivHarness;
            configureExternalResetMode(highDivHarness, ClockSetup::Mode::Slave, 24);
            highDivTicks = runPulseTrain(highDivHarness, 6, 40);
        }

        expectTrue(lowDivTicks > 0);
        expectTrue(midDivTicks > lowDivTicks);
        expectTrue(highDivTicks > midDivTicks);
    }
}


