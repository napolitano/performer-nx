#include "UnitTest.h"

#include "apps/sequencer/engine/MidiLearn.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

// Build a raw pitch-bend message (status 0xE0 | channel, data 0, 0).
// MidiMessage::makePitchBend() has a known clamping quirk but we only need
// a message whose isPitchBend() returns true; the data bytes do not matter.
static MidiMessage makePitchBend(int channel) {
    return MidiMessage(uint8_t(0xe0 | channel), 0, 0);
}

// Send `count` identical pitch-bend messages.
static void sendPitchBends(MidiLearn &ml, MidiPort port, int channel, int count) {
    for (int i = 0; i < count; ++i) {
        ml.receiveMidi(port, makePitchBend(channel));
    }
}

// Send `count` identical CC messages.
static void sendCCs(MidiLearn &ml, MidiPort port, int channel, int cc, int value, int count) {
    for (int i = 0; i < count; ++i) {
        ml.receiveMidi(port, MidiMessage::makeControlChange(uint8_t(channel), uint8_t(cc), uint8_t(value)));
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
UNIT_TEST("MidiLearn") {

    // ── Lifecycle: start / stop / isActive ──────────────────────────────────

    CASE("isActive reflects start and stop state") {
        MidiLearn ml;
        expectFalse(ml.isActive());

        bool called = false;
        ml.start([&](const MidiLearn::Result &) { called = true; });
        expectTrue(ml.isActive());

        ml.stop();
        expectFalse(ml.isActive());
        (void)called;
    }

    CASE("receiveMidi is a no-op when no callback is set") {
        MidiLearn ml;
        // Normally two NoteOn on the same note would fire a callback,
        // but no start() was called so nothing should happen.
        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60));
        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60));
        expectFalse(ml.isActive());
    }

    CASE("stop clears the callback so further messages produce no result") {
        MidiLearn ml;
        int callCount = 0;

        ml.start([&](const MidiLearn::Result &) { ++callCount; });
        ml.stop();

        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60));
        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60));
        expectEqual(callCount, 0);
    }

    // ── Note detection ───────────────────────────────────────────────────────

    CASE("two NoteOn messages with the same note emit a Note result") {
        MidiLearn ml;
        MidiLearn::Result result{};
        int callCount = 0;

        ml.start([&](const MidiLearn::Result &r) { result = r; ++callCount; });

        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60));
        expectEqual(callCount, 0);   // only 1 event → no emit yet

        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60));
        expectEqual(callCount, 1);
        expectEqual(result.port, MidiPort::Midi);
        expectEqual(int(result.channel), 0);
        expectEqual(result.event, MidiLearn::Event::Note);
        expectEqual(int(result.note), 60);
    }

    CASE("NoteOff counts toward the Note counter just like NoteOn") {
        MidiLearn ml;
        MidiLearn::Result result{};
        int callCount = 0;

        ml.start([&](const MidiLearn::Result &r) { result = r; ++callCount; });

        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 64));
        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOff(0, 64));

        expectEqual(callCount, 1);
        expectEqual(result.event, MidiLearn::Event::Note);
        expectEqual(int(result.note), 64);
    }

    CASE("note change resets the Note counter and requires two more messages on the new note") {
        MidiLearn ml;
        int callCount = 0;

        ml.start([&](const MidiLearn::Result &) { ++callCount; });

        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60)); // counter = 1 (note 60)
        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 62)); // note change → counter reset to 1 (note 62)
        expectEqual(callCount, 0);  // threshold not yet reached

        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 62)); // counter = 2 → emit
        expectEqual(callCount, 1);
    }

    CASE("identical learned result is emitted again while session stays active") {
        MidiLearn ml;
        int callCount = 0;

        ml.start([&](const MidiLearn::Result &) { ++callCount; });

        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60));
        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60));
        expectEqual(callCount, 1);

        // Current behavior: same learned mapping can emit again.
        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60));
        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60));
        expectEqual(callCount, 2);
    }

    CASE("start begins a fresh learn session so same mapping can be emitted again") {
        MidiLearn ml;
        int callCount = 0;

        ml.start([&](const MidiLearn::Result &) { ++callCount; });
        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 61));
        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 61));
        expectEqual(callCount, 1);

        ml.stop();
        ml.start([&](const MidiLearn::Result &) { ++callCount; });
        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 61));
        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 61));
        expectEqual(callCount, 2);
    }

    // ── PitchBend detection ──────────────────────────────────────────────────

    CASE("eight PitchBend messages emit a PitchBend result") {
        MidiLearn ml;
        MidiLearn::Result result{};
        int callCount = 0;

        ml.start([&](const MidiLearn::Result &r) { result = r; ++callCount; });

        sendPitchBends(ml, MidiPort::UsbMidi, 3, 7);
        expectEqual(callCount, 0);  // 7 → still one short

        sendPitchBends(ml, MidiPort::UsbMidi, 3, 1);
        expectEqual(callCount, 1);
        expectEqual(result.port, MidiPort::UsbMidi);
        expectEqual(int(result.channel), 3);
        expectEqual(result.event, MidiLearn::Event::PitchBend);
    }

    // ── ControlAbsolute detection ────────────────────────────────────────────

    CASE("eight CC messages with absolute value emit a ControlAbsolute result") {
        MidiLearn ml;
        MidiLearn::Result result{};
        int callCount = 0;

        ml.start([&](const MidiLearn::Result &r) { result = r; ++callCount; });

        // value=64: not in (0,8) and not in (64,72) → absolute
        sendCCs(ml, MidiPort::Midi, 0, 74, 64, 7);
        expectEqual(callCount, 0);

        sendCCs(ml, MidiPort::Midi, 0, 74, 64, 1);
        expectEqual(callCount, 1);
        expectEqual(result.event, MidiLearn::Event::ControlAbsolute);
        expectEqual(int(result.controlNumber), 74);
    }

    CASE("CC value 0 is treated as absolute (boundary: not > 0)") {
        MidiLearn ml;
        MidiLearn::Result result{};
        int callCount = 0;

        ml.start([&](const MidiLearn::Result &r) { result = r; ++callCount; });

        sendCCs(ml, MidiPort::Midi, 0, 10, 0, 8);
        expectEqual(callCount, 1);
        expectEqual(result.event, MidiLearn::Event::ControlAbsolute);
    }

    CASE("CC value 8 is treated as absolute (boundary: not < 8)") {
        MidiLearn ml;
        MidiLearn::Result result{};
        int callCount = 0;

        ml.start([&](const MidiLearn::Result &r) { result = r; ++callCount; });

        sendCCs(ml, MidiPort::Midi, 0, 10, 8, 8);
        expectEqual(callCount, 1);
        expectEqual(result.event, MidiLearn::Event::ControlAbsolute);
    }

    // ── ControlRelative detection ────────────────────────────────────────────

    CASE("eight CC messages with value 1-7 emit a ControlRelative result") {
        MidiLearn ml;
        MidiLearn::Result result{};
        int callCount = 0;

        ml.start([&](const MidiLearn::Result &r) { result = r; ++callCount; });

        // value=3: in (0,8) → relative
        sendCCs(ml, MidiPort::Midi, 0, 20, 3, 8);
        expectEqual(callCount, 1);
        expectEqual(result.event, MidiLearn::Event::ControlRelative);
        expectEqual(int(result.controlNumber), 20);
    }

    CASE("eight CC messages with value 65-71 emit a ControlRelative result") {
        MidiLearn ml;
        MidiLearn::Result result{};
        int callCount = 0;

        ml.start([&](const MidiLearn::Result &r) { result = r; ++callCount; });

        // value=66: in (64,72) → relative
        sendCCs(ml, MidiPort::Midi, 0, 20, 66, 8);
        expectEqual(callCount, 1);
        expectEqual(result.event, MidiLearn::Event::ControlRelative);
        expectEqual(int(result.controlNumber), 20);
    }

    CASE("result metadata: channel and CC number are forwarded correctly") {
        MidiLearn ml;
        MidiLearn::Result absResult{}, relResult{};

        ml.start([&](const MidiLearn::Result &r) {
            if (r.event == MidiLearn::Event::ControlAbsolute) absResult = r;
            if (r.event == MidiLearn::Event::ControlRelative) relResult = r;
        });

        sendCCs(ml, MidiPort::Midi, 2, 74, 100, 8); // absolute, ch 2
        expectEqual(int(absResult.channel), 2);
        expectEqual(int(absResult.controlNumber), 74);

        // Restart for relative detection
        ml.stop();
        ml.start([&](const MidiLearn::Result &r) {
            if (r.event == MidiLearn::Event::ControlRelative) relResult = r;
        });

        sendCCs(ml, MidiPort::Midi, 1, 20, 5, 8);  // relative, ch 1
        expectEqual(int(relResult.channel), 1);
        expectEqual(int(relResult.controlNumber), 20);
    }

    // ── NRPN filter ──────────────────────────────────────────────────────────

    CASE("NRPN pair (CC 0-31 then CC 32-63) is filtered: only the first CC counts") {
        MidiLearn ml;
        MidiLearn::Result result{};
        int callCount = 0;

        ml.start([&](const MidiLearn::Result &r) { result = r; ++callCount; });

        // 8 pairs of CC#10 (absolute) + CC#42 (NRPN companion, filtered).
        // CC#10 increments the ControlAbsolute counter on each iteration.
        // CC#42 is silently discarded by the NRPN guard.
        for (int i = 0; i < 8; ++i) {
            ml.receiveMidi(MidiPort::Midi, MidiMessage::makeControlChange(0, 10, 64)); // CC#10: not NRPN
            ml.receiveMidi(MidiPort::Midi, MidiMessage::makeControlChange(0, 42, 64)); // CC#42: NRPN → filtered
        }

        expectEqual(callCount, 1);
        expectEqual(result.event, MidiLearn::Event::ControlAbsolute);
        expectEqual(int(result.controlNumber), 10);
    }

    CASE("NRPN guard requires preceding CC to be in 0-31: CC 32-63 pair is not filtered") {
        MidiLearn ml;
        MidiLearn::Result result{};
        int callCount = 0;

        ml.start([&](const MidiLearn::Result &r) { result = r; ++callCount; });

        // CC#50 (not in 0-31) followed by CC#42 → NOT a NRPN pair; both count normally.
        // Both are absolute (value=64). They alternate → each change resets counters.
        // 8 alternating messages never reach 8 on either one.
        for (int i = 0; i < 4; ++i) {
            ml.receiveMidi(MidiPort::Midi, MidiMessage::makeControlChange(0, 50, 64));
            ml.receiveMidi(MidiPort::Midi, MidiMessage::makeControlChange(0, 42, 64));
        }
        expectEqual(callCount, 0); // counters kept resetting due to CC number alternation

        // Now send 8 identical CC#50 to confirm detection still works.
        sendCCs(ml, MidiPort::Midi, 0, 50, 64, 8);
        expectEqual(callCount, 1);
        expectEqual(result.event, MidiLearn::Event::ControlAbsolute);
        expectEqual(int(result.controlNumber), 50);
    }

    // ── Port and channel reset ────────────────────────────────────────────────

    CASE("port change resets state and detection restarts on the new port") {
        MidiLearn ml;
        MidiLearn::Result result{};
        int callCount = 0;

        ml.start([&](const MidiLearn::Result &r) { result = r; ++callCount; });

        // Begin accumulating on Midi port.
        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60)); // Midi, counter=1

        // Switch to UsbMidi → reset, counter starts from scratch.
        ml.receiveMidi(MidiPort::UsbMidi, MidiMessage::makeNoteOn(0, 60)); // reset + counter=1
        expectEqual(callCount, 0);  // not yet at 2

        ml.receiveMidi(MidiPort::UsbMidi, MidiMessage::makeNoteOn(0, 60)); // counter=2 → emit
        expectEqual(callCount, 1);
        expectEqual(result.port, MidiPort::UsbMidi);
    }

    CASE("channel change on the same port resets state") {
        MidiLearn ml;
        int callCount = 0;

        ml.start([&](const MidiLearn::Result &) { ++callCount; });

        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(0, 60)); // ch 0, counter=1
        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(1, 60)); // ch 1 → reset, counter=1
        expectEqual(callCount, 0);

        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeNoteOn(1, 60)); // ch 1, counter=2 → emit
        expectEqual(callCount, 1);
    }

    // ── CC number change resets counters ─────────────────────────────────────

    CASE("CC number change resets both Absolute and Relative counters") {
        MidiLearn ml;
        int callCount = 0;

        ml.start([&](const MidiLearn::Result &) { ++callCount; });

        sendCCs(ml, MidiPort::Midi, 0, 74, 64, 7);  // absolute counter = 7
        // Change CC number → counters reset to 0
        ml.receiveMidi(MidiPort::Midi, MidiMessage::makeControlChange(0, 75, 64));
        expectEqual(callCount, 0);  // threshold not reached

        sendCCs(ml, MidiPort::Midi, 0, 75, 64, 7);  // 1 + 7 = 8 on CC#75 → emit
        expectEqual(callCount, 1);
    }

    // ── Unrecognized message types ────────────────────────────────────────────

    CASE("ProgramChange messages do not increment any counter") {
        MidiLearn ml;
        int callCount = 0;

        ml.start([&](const MidiLearn::Result &) { ++callCount; });

        // ProgramChange has no matching branch in receiveMidi → early return.
        for (int i = 0; i < 100; ++i) {
            ml.receiveMidi(MidiPort::Midi, MidiMessage::makeProgramChange(0, uint8_t(i & 0x7f)));
        }
        expectEqual(callCount, 0);
    }
}

