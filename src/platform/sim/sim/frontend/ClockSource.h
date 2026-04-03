#pragma once

#include "sim/Simulator.h"

#include <algorithm>

namespace sim {

class ClockSource {
public:
    ClockSource(Simulator &simulator, std::function<void()> handler) :
        _simulator(simulator),
        _handler(handler)
    {
        _simulator.addUpdateCallback([this] () { update(); });
    }

    void toggle() {
        _active = !_active;
        if (_active) {
            _lastTicks = _simulator.ticks();
        }
    }
    bool active() const {
        return _active;
    }

    int ppqn() const {
        return _ppqn;
    }

    void setPpqn(int ppqn) {
        _ppqn = std::max(1, ppqn);
    }

    double bpm() const {
        return _bpm;
    }

    void setBpm(double bpm) {
        _bpm = std::max(1.0, bpm);
    }

    void update() {
        if (_active) {
            double currentTicks = _simulator.ticks();
            double interval = clockInterval() * 1000.0;
            while (_lastTicks <= currentTicks) { // fire once per interval; first fires immediately on enable
                if (_handler) {
                    _handler();
                }
                _lastTicks += interval;
            }
        }
    }

private:
    double clockInterval() {
        return 60.0 / (_bpm * _ppqn);
    }

    Simulator &_simulator;
    std::function<void()> _handler;

    bool _active = false;
    // Default PPQN must match engine expected external pulse rate.
    // Formula: CONFIG_SEQUENCE_PPQN (48) / clockInputDivisor (default 12) = 4
    // clockInputDivisor=12 (1/16) = 4 ext pulses per QN
    // clockInputDivisor=24 (1/8)  = 2 ext pulses per QN
    // clockInputDivisor=6  (1/32) = 8 ext pulses per QN
    int _ppqn = 4;
    double _bpm = 120.0;

    double _lastTicks;
};

} // namespace sim
