#include "UnitTest.h"

#include "apps/sequencer/engine/Clock.h"

#include "sim/Simulator.h"

#include <memory>
#include <vector>

namespace {

class RecordingListener : public Clock::Listener {
public:
    void onClockOutput(const Clock::OutputState &state) override {
        outputs.push_back(state);
    }

    void onClockMidi(uint8_t msg) override {
        midi.push_back(msg);
    }

    std::vector<Clock::OutputState> outputs;
    std::vector<uint8_t> midi;
};

class ClockHarness {
public:
    ClockHarness() :
        _simulator(makeTarget()),
        _clock(_timer)
    {
        _clock.init();
        _clock.outputConfigure(12, 1);
        _clock.outputConfigureSwing(0);
        _clock.setListener(&_listener);
    }

    Clock &clock() { return _clock; }
    RecordingListener &listener() { return _listener; }

    void waitMs(int ms) { _simulator.wait(ms); }

    void drainEvents() {
        while (_clock.checkEvent() != Clock::Event(0)) {
        }
    }

    int drainTicks() {
        uint32_t tick = 0;
        int count = 0;
        while (_clock.checkTick(&tick)) {
            ++count;
        }
        return count;
    }

private:
    sim::Target makeTarget() {
        sim::Target target;
        target.create = []() {};
        target.destroy = []() {};
        target.update = []() {};
        return target;
    }

    sim::Simulator _simulator;
    ClockTimer _timer;
    Clock _clock;
    RecordingListener _listener;
};

} // namespace

UNIT_TEST("ClockCore") {

    CASE("setListener accepts nullptr and no output callback is fired") {
        ClockHarness harness;

        harness.clock().setListener(nullptr);
        size_t outputsBefore = harness.listener().outputs.size();

        harness.clock().masterStart();
        harness.waitMs(20);

        expectEqual(harness.listener().outputs.size(), outputsBefore);
    }

    CASE("master clock emits ticks only after pending events are consumed") {
        ClockHarness harness;
        auto &clock = harness.clock();

        clock.masterStart();
        harness.waitMs(30);

        uint32_t tick = 0;
        expectFalse(clock.checkTick(&tick)); // blocked by pending Start/Reset events

        harness.drainEvents();
        expectTrue(clock.checkTick(&tick));
        expectEqual(clock.activeMode(), Clock::Mode::Master);
    }

    CASE("mode switching stops currently running clock source") {
        ClockHarness harness;
        auto &clock = harness.clock();

        clock.slaveConfigure(0, 12, true);
        harness.drainEvents();

        clock.slaveStart(0);
        expectEqual(clock.activeMode(), Clock::Mode::Slave);

        clock.setMode(Clock::Mode::Master);
        expectTrue(clock.isIdle());
        expectEqual(clock.activeMode(), Clock::Mode::Master);

        harness.drainEvents();
        clock.masterStart();
        expectEqual(clock.activeMode(), Clock::Mode::Master);

        clock.setMode(Clock::Mode::Slave);
        expectTrue(clock.isIdle());
        expectEqual(clock.activeMode(), Clock::Mode::Slave);
    }

    CASE("master control calls are ignored when clock mode is slave") {
        ClockHarness harness;
        auto &clock = harness.clock();

        harness.drainEvents();
        clock.setMode(Clock::Mode::Slave);

        clock.masterStart();
        clock.masterContinue();
        clock.masterReset();

        expectTrue(clock.isIdle());
        expectEqual(clock.checkEvent(), Clock::Event(0));
    }

    CASE("master controls are ignored while slave is running") {
        ClockHarness harness;
        auto &clock = harness.clock();

        clock.slaveConfigure(0, 12, true);
        clock.slaveStart(0);
        harness.drainEvents();

        clock.masterStart();
        clock.masterContinue();
        clock.masterReset();

        expectTrue(clock.isRunning());
        expectEqual(clock.activeMode(), Clock::Mode::Slave);
    }

    CASE("slave start continue reset are ignored in explicit master mode") {
        ClockHarness harness;
        auto &clock = harness.clock();

        clock.slaveConfigure(0, 12, true);
        harness.drainEvents();

        clock.setMode(Clock::Mode::Master);

        clock.slaveStart(0);
        clock.slaveContinue(0);
        clock.slaveReset(0);

        expectTrue(clock.isIdle());
        expectEqual(clock.activeMode(), Clock::Mode::Master);
        expectEqual(clock.checkEvent(), Clock::Event(0));
    }

    CASE("slaveHandleMidi routes start tick continue and stop messages") {
        ClockHarness harness;
        auto &clock = harness.clock();

        clock.slaveConfigure(1, 12, true);
        harness.drainEvents();

        clock.slaveHandleMidi(1, MidiMessage::Start);
        expectEqual(clock.activeMode(), Clock::Mode::Slave);

        harness.waitMs(2);
        clock.slaveHandleMidi(1, MidiMessage::Tick);
        harness.waitMs(10);

        harness.drainEvents();
        expectTrue(harness.drainTicks() > 0);

        clock.slaveHandleMidi(1, MidiMessage::Continue);
        expectEqual(clock.activeMode(), Clock::Mode::Slave);

        clock.slaveHandleMidi(1, MidiMessage::Stop);
        expectTrue(clock.isIdle());

        // Non-realtime message should be ignored.
        clock.slaveHandleMidi(1, 0x00);
        expectTrue(clock.isIdle());
    }

    CASE("disabled slave source ignores all slave transport calls") {
        ClockHarness harness;
        auto &clock = harness.clock();

        clock.slaveConfigure(2, 12, false);
        harness.drainEvents();

        clock.slaveStart(2);
        clock.slaveTick(2);
        clock.slaveContinue(2);
        clock.slaveStop(2);
        clock.slaveReset(2);

        expectTrue(clock.isIdle());
        expectEqual(clock.checkEvent(), Clock::Event(0));
    }

    CASE("slave guard paths keep active source unchanged when wrong slave id is used") {
        ClockHarness harness;
        auto &clock = harness.clock();

        clock.slaveConfigure(0, 12, true);
        clock.slaveConfigure(1, 12, true);
        clock.slaveStart(0);
        harness.drainEvents();

        // Running on slave 0: operations for slave 1 must be ignored.
        clock.slaveStart(1);
        clock.slaveStop(1);
        clock.slaveReset(1);

        expectTrue(clock.isRunning());
        expectEqual(clock.activeMode(), Clock::Mode::Slave);

        // Active source can still be stopped normally.
        clock.slaveStop(0);
        expectTrue(clock.isIdle());
    }

    CASE("slave tick is ignored when source is inactive or not active slave") {
        ClockHarness harness;
        auto &clock = harness.clock();

        clock.slaveConfigure(0, 12, true);
        clock.slaveConfigure(1, 12, true);
        harness.drainEvents();

        // Idle clock: ticks should not start anything.
        clock.slaveTick(0);
        expectTrue(clock.isIdle());

        // Running on slave 0: ticks from slave 1 must be ignored.
        clock.slaveStart(0);
        harness.drainEvents();
        uint32_t tickBefore = clock.tick();
        clock.slaveTick(1);
        harness.waitMs(2);
        expectEqual(clock.tick(), tickBefore);
    }

    CASE("slave transport calls are ignored while master clock is running") {
        ClockHarness harness;
        auto &clock = harness.clock();

        clock.slaveConfigure(0, 12, true);
        clock.masterStart();
        harness.drainEvents();

        uint32_t tickBefore = clock.tick();

        clock.slaveStart(0);
        clock.slaveStop(0);
        clock.slaveReset(0);

        harness.waitMs(3);
        expectEqual(clock.activeMode(), Clock::Mode::Master);
        expectTrue(clock.isRunning());
        expectTrue(clock.tick() >= tickBefore);
    }

    CASE("avgSlavePeriod returns zero before any samples") {
        ClockHarness harness;
        auto &clock = harness.clock();

        expectEqual(clock.avgSlavePeriod(), uint32_t(0));
    }

    CASE("slaveStart on already active slave keeps slave running") {
        ClockHarness harness;
        auto &clock = harness.clock();

        clock.slaveConfigure(0, 12, true);
        clock.slaveStart(0);
        harness.drainEvents();

        clock.slaveStart(0);

        expectTrue(clock.isRunning());
        expectEqual(clock.activeMode(), Clock::Mode::Slave);
    }

    CASE("slave period window rollover and averaging path remain stable") {
        ClockHarness harness;
        auto &clock = harness.clock();

        clock.slaveConfigure(0, 12, true);
        clock.slaveStart(0);
        harness.drainEvents();

        for (int i = 0; i < 48; ++i) {
            harness.waitMs(2);
            clock.slaveTick(0);
        }

        expectEqual(clock.activeMode(), Clock::Mode::Slave);
        expectTrue(clock.bpm() > 0.f);
    }

    CASE("non-zero swing path executes during master output scheduling") {
        ClockHarness harness;
        auto &clock = harness.clock();

        clock.outputConfigureSwing(50);
        clock.masterStart();
        harness.drainEvents();
        harness.waitMs(40);

        expectEqual(clock.activeMode(), Clock::Mode::Master);
        expectTrue(harness.listener().midi.size() > 0);
    }
}


