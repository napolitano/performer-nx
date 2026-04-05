#include "UnitTest.h"

#include "apps/sequencer/engine/CvOutput.h"

#include "sim/Simulator.h"

#include <memory>

namespace {

class CvOutputHarness {
public:
    CvOutputHarness() :
        _simulator(makeTarget())
    {
        _calibration.clear();
        _simulator.reboot();
    }

    CvOutput &cvOutput() { return *_cvOutput; }
    const Calibration &calibration() const { return _calibration; }
    const sim::TargetState &targetState() const { return _simulator.targetState(); }

private:
    sim::Target makeTarget() {
        sim::Target target;
        target.create = [this] () {
            _dac.reset(new Dac());
            _cvOutput.reset(new CvOutput(*_dac, _calibration));
        };
        target.destroy = [this] () {
            _cvOutput.reset();
            _dac.reset();
        };
        target.update = [] () {};
        return target;
    }

    Calibration _calibration;
    std::unique_ptr<Dac> _dac;
    std::unique_ptr<CvOutput> _cvOutput;
    sim::Simulator _simulator;
};

static uint16_t expectedValue(const Calibration &calibration, int channel, float volts) {
    return calibration.cvOutput(channel).voltsToValue(volts);
}

} // namespace

UNIT_TEST("CvOutput") {

    CASE("init zeros all logical output channels") {
        CvOutputHarness harness;
        auto &cvOutput = harness.cvOutput();

        cvOutput.init();

        for (int channel = 0; channel < CvOutput::Channels; ++channel) {
            expectEqual(cvOutput.channel(channel), 0.f);
        }
    }

    CASE("update writes calibrated DAC values for all zeroed channels") {
        CvOutputHarness harness;
        auto &cvOutput = harness.cvOutput();

        cvOutput.init();
        cvOutput.update();

        for (int channel = 0; channel < CvOutput::Channels; ++channel) {
            expectEqual(harness.targetState().dac.state[channel], expectedValue(harness.calibration(), channel, 0.f));
        }
    }

    CASE("update converts representative voltages and clamps out-of-range values") {
        CvOutputHarness harness;
        auto &cvOutput = harness.cvOutput();

        cvOutput.init();
        cvOutput.setChannel(0, -5.f);
        cvOutput.setChannel(1, -1.5f);
        cvOutput.setChannel(2, 2.25f);
        cvOutput.setChannel(3, 9.f);
        cvOutput.update();

        expectEqual(cvOutput.channel(0), -5.f);
        expectEqual(cvOutput.channel(1), -1.5f);
        expectEqual(cvOutput.channel(2), 2.25f);
        expectEqual(cvOutput.channel(3), 9.f);

        expectEqual(harness.targetState().dac.state[0], expectedValue(harness.calibration(), 0, -5.f));
        expectEqual(harness.targetState().dac.state[1], expectedValue(harness.calibration(), 1, -1.5f));
        expectEqual(harness.targetState().dac.state[2], expectedValue(harness.calibration(), 2, 2.25f));
        expectEqual(harness.targetState().dac.state[3], expectedValue(harness.calibration(), 3, 5.f));
    }
}

