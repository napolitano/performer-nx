#include "UnitTest.h"

#include "apps/sequencer/engine/CvInput.h"

#include "sim/Simulator.h"

#include <cmath>
#include <memory>

namespace {

class CvInputHarness {
public:
    CvInputHarness() :
        _simulator(makeTarget())
    {
        _simulator.reboot();
    }

    CvInput &cvInput() { return *_cvInput; }

    void setVoltage(int channel, float voltage) {
        _simulator.setAdc(channel, voltage);
    }

private:
    sim::Target makeTarget() {
        sim::Target target;
        target.create = [this] () {
            _adc.reset(new Adc());
            _cvInput.reset(new CvInput(*_adc));
        };
        target.destroy = [this] () {
            _cvInput.reset();
            _adc.reset();
        };
        target.update = [] () {};
        return target;
    }

    std::unique_ptr<Adc> _adc;
    std::unique_ptr<CvInput> _cvInput;
    sim::Simulator _simulator;
};

static float expectedChannel(float voltage) {
    float normalized = std::max(0.f, std::min(1.f, voltage * 0.1f + 0.5f));
    uint16_t raw = uint16_t(std::floor(0xffff - 0xffff * normalized));
    return 5.f - raw / 6553.5f;
}

static void expectClose(float actual, float expected, float epsilon = 0.0002f) {
    expectTrue(std::fabs(actual - expected) <= epsilon);
}

} // namespace

UNIT_TEST("CvInput") {

    CASE("init zeros all exposed channels") {
        CvInputHarness harness;
        auto &cvInput = harness.cvInput();

        cvInput.init();

        for (int channel = 0; channel < CvInput::Channels; ++channel) {
            expectEqual(cvInput.channel(channel), 0.f);
        }
    }

    CASE("update converts ADC voltages for each channel") {
        CvInputHarness harness;
        auto &cvInput = harness.cvInput();

        harness.setVoltage(0, -5.f);
        harness.setVoltage(1, -1.25f);
        harness.setVoltage(2, 0.f);
        harness.setVoltage(3, 2.5f);

        cvInput.update();

        expectClose(cvInput.channel(0), expectedChannel(-5.f));
        expectClose(cvInput.channel(1), expectedChannel(-1.25f));
        expectClose(cvInput.channel(2), expectedChannel(0.f));
        expectClose(cvInput.channel(3), expectedChannel(2.5f));
    }

    CASE("update clamps voltages outside the supported range") {
        CvInputHarness harness;
        auto &cvInput = harness.cvInput();

        harness.setVoltage(0, -12.f);
        harness.setVoltage(1, 12.f);
        cvInput.update();

        expectClose(cvInput.channel(0), expectedChannel(-5.f));
        expectClose(cvInput.channel(1), expectedChannel(5.f));

        cvInput.init();
        expectEqual(cvInput.channel(0), 0.f);
        expectEqual(cvInput.channel(1), 0.f);
    }
}

