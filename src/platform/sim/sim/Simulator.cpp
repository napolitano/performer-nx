#include "Simulator.h"

#include "libs/stb/stb_image_write.h"

#include "os/os.h"

#include "core/midi/MidiMessage.h"

#include <memory>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>

#include <cmath>

namespace sim {

static Simulator *g_instance;

Simulator::Simulator(Target target) :
    _target(target),
    _targetStateTracker(_targetState)
{
    g_instance = this;

    registerTargetInputObserver(&_targetStateTracker);
    registerTargetOutputObserver(&_targetStateTracker);
}

Simulator::~Simulator() {
    if (_targetCreated) {
       _target.destroy();
    }
}

void Simulator::wait(int ms) {
    while (ms--) {
        step();
    }
}

void Simulator::setButton(int index, bool pressed) {
    writeButton(index, pressed);
}

void Simulator::setEncoder(bool pressed) {
    writeEncoder(pressed ? EncoderEvent::Down : EncoderEvent::Up);
}

void Simulator::rotateEncoder(int direction) {
    if (direction > 0) {
        writeEncoder(EncoderEvent::Right);
    } else if (direction < 0) {
        writeEncoder(EncoderEvent::Left);
    }
}

void Simulator::setAdc(int channel, float voltage) {
    float normalized = std::max(0.f, std::min(1.f, voltage * 0.1f + 0.5f));
    writeAdc(channel, uint16_t(std::floor(0xffff - 0xffff * normalized)));
}

void Simulator::setDio(int pin, bool state) {
    writeDigitalInput(pin, state);
}

void Simulator::sendMidi(int port, const MidiMessage &message) {
    writeMidiInput(MidiEvent::makeMessage(port, message));
}

void Simulator::reboot() {
    const auto buttonState = _targetState.button;
    const auto adcState = _targetState.adc;
    const auto digitalInputState = _targetState.digitalInput;

    if (_targetCreated) {
        _target.destroy();
        _targetCreated = false;
    }

    _tick = 0;
    _targetState = {};

    // Reset visible outputs immediately so the frontend does not keep stale state
    // until the recreated firmware produces fresh output.
    for (int index = 0; index < LedState::Count; ++index) {
        writeLed(index, false, false);
    }
    for (int channel = 0; channel < GateOutputState::Count; ++channel) {
        writeGateOutput(channel, false);
    }
    for (int channel = 0; channel < DacState::Count; ++channel) {
        writeDac(channel, 0);
    }
    for (int pin = 0; pin < DigitalOutputState::Count; ++pin) {
        writeDigitalOutput(pin, false);
    }
    FrameBuffer frameBuffer;
    frameBuffer.fill(0);
    writeLcd(frameBuffer);

    _target.create();
    _targetCreated = true;

    // Reapply the current simulated external inputs so the recreated firmware sees
    // the same CV and digital input environment as before the reboot.
    for (int index = 0; index < ButtonState::Count; ++index) {
        writeButton(index, buttonState.state.test(index));
    }
    for (int channel = 0; channel < AdcState::Count; ++channel) {
        writeAdc(channel, adcState.state[channel]);
    }
    for (int pin = 0; pin < DigitalInputState::Count; ++pin) {
        writeDigitalInput(pin, digitalInputState.state.test(pin));
    }
}

void Simulator::screenshot(const std::string &filename) {
    std::unique_ptr<uint8_t[]> pixelBuffer(new uint8_t[CONFIG_LCD_WIDTH * CONFIG_LCD_HEIGHT]);

    const uint8_t *src = targetState().lcd.state.data();
    uint8_t *dst = pixelBuffer.get();

    for (int i = 0; i < CONFIG_LCD_WIDTH * CONFIG_LCD_HEIGHT; ++i) {
        float s = std::min(uint8_t(15), *src++) * (1.f / 15.f);
        *dst++ = s * 0xff;
    }

    stbi_write_png(filename.c_str(), CONFIG_LCD_WIDTH, CONFIG_LCD_HEIGHT, 1, pixelBuffer.get(), CONFIG_LCD_WIDTH);
}

double Simulator::ticks() {
    return _tick;
}

void Simulator::addUpdateCallback(UpdateCallback callback) {
    _updateCallbacks.emplace_back(callback);
}

void Simulator::registerTargetTickObserver(TargetTickHandler *observer) {
    _targetTickObservers.emplace_back(observer);
}

void Simulator::registerTargetInputObserver(TargetInputHandler *observer) {
    _targetInputObservers.emplace_back(observer);
}

void Simulator::registerTargetOutputObserver(TargetOutputHandler *observer) {
    _targetOutputObservers.emplace_back(observer);
}

// TargetInputHandler

void Simulator::writeButton(int index, bool pressed) {
    for (auto observer : _targetInputObservers) {
        observer->writeButton(index, pressed);
    }
}

void Simulator::writeEncoder(EncoderEvent event) {
    for (auto observer : _targetInputObservers) {
        observer->writeEncoder(event);
    }
}

void Simulator::writeAdc(int channel, uint16_t value) {
    for (auto observer : _targetInputObservers) {
        observer->writeAdc(channel, value);
    }
}

void Simulator::writeDigitalInput(int pin, bool value) {
    for (auto observer : _targetInputObservers) {
        observer->writeDigitalInput(pin, value);
    }
}

void Simulator::writeMidiInput(MidiEvent event) {
    for (auto observer : _targetInputObservers) {
        observer->writeMidiInput(event);
    }
}

// TargetOutputHandler

void Simulator::writeLed(int index, bool red, bool green) {
    for (auto observer : _targetOutputObservers) {
        observer->writeLed(index, red, green);
    }
}

void Simulator::writeGateOutput(int channel, bool value) {
    for (auto observer : _targetOutputObservers) {
        observer->writeGateOutput(channel, value);
    }
}

void Simulator::writeDac(int channel, uint16_t value) {
    for (auto observer : _targetOutputObservers) {
        observer->writeDac(channel, value);
    }
}

void Simulator::writeDigitalOutput(int pin, bool value) {
    for (auto observer : _targetOutputObservers) {
        observer->writeDigitalOutput(pin, value);
    }
}

void Simulator::writeLcd(const FrameBuffer &frameBuffer) {
    for (auto observer : _targetOutputObservers) {
        observer->writeLcd(frameBuffer);
    }
}

void Simulator::writeMidiOutput(MidiEvent event) {
    for (auto observer : _targetOutputObservers) {
        observer->writeMidiOutput(event);
    }
}

Simulator &Simulator::instance() {
    return *g_instance;
}

void Simulator::step() {
    if (!_targetCreated) {
        _target.create();
        _targetCreated = true;
    }

    for (auto observer : _targetTickObservers) {
        observer->setTick(_tick);
    }

    for (const auto &callback : os::updateCallbacks()) {
        callback();
    }

    for (const auto &callback : _updateCallbacks) {
        callback();
    }

    _target.update();

    _tick += 1;
}

} // namespace sim
