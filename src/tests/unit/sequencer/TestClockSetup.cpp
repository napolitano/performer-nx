#include "UnitTest.h"

#include "../core/io/MemoryReaderWriter.h"

#include "core/io/VersionedSerializedWriter.h"
#include "core/io/VersionedSerializedReader.h"
#include "core/utils/StringBuilder.h"

#include "apps/sequencer/model/ClockSetup.cpp"
#include "apps/sequencer/model/ModelUtils.cpp"

#include <cstring>

static uint8_t buf[128];

static void clearBuf() {
    std::memset(buf, 0, sizeof(buf));
}

static void writeClockSetupV10() {
    MemoryWriter memoryWriter(buf, sizeof(buf));
    VersionedSerializedWriter writer([&memoryWriter] (const void *data, size_t len) { memoryWriter.write(data, len); }, 10);

    writer.write(ClockSetup::Mode::Slave);
    writer.write(ClockSetup::ShiftMode::Pause);
    writer.write(uint8_t(24));
    writer.write(ClockSetup::ClockInputMode::Run);
    writer.write(uint8_t(48));
    writer.write(uint8_t(7));
    writer.write(ClockSetup::ClockOutputMode::Run);
    writer.write(true);
    writer.write(false);
    writer.write(true);
    writer.write(false);
}

static void writeClockSetupV11() {
    MemoryWriter memoryWriter(buf, sizeof(buf));
    VersionedSerializedWriter writer([&memoryWriter] (const void *data, size_t len) { memoryWriter.write(data, len); }, 11);

    writer.write(ClockSetup::Mode::Master);
    writer.write(ClockSetup::ShiftMode::Restart);
    writer.write(uint8_t(12));
    writer.write(ClockSetup::ClockInputMode::StartStop);
    writer.write(uint8_t(24));
    writer.write(true);
    writer.write(uint8_t(5));
    writer.write(ClockSetup::ClockOutputMode::Reset);
    writer.write(false);
    writer.write(true);
    writer.write(false);
    writer.write(true);
}

static void writeClockSetupV11NoSwing() {
    MemoryWriter memoryWriter(buf, sizeof(buf));
    VersionedSerializedWriter writer([&memoryWriter] (const void *data, size_t len) { memoryWriter.write(data, len); }, 11);

    writer.write(ClockSetup::Mode::Master);
    writer.write(ClockSetup::ShiftMode::Restart);
    writer.write(uint8_t(12));
    writer.write(ClockSetup::ClockInputMode::StartStop);
    writer.write(uint8_t(24));
    writer.write(false);
    writer.write(uint8_t(5));
    writer.write(ClockSetup::ClockOutputMode::Reset);
    writer.write(false);
    writer.write(true);
    writer.write(false);
    writer.write(true);
}

UNIT_TEST("ClockSetup") {

    CASE("clear initializes documented defaults") {
        ClockSetup setup;
        setup.clear();

        expectEqual(setup.mode(), ClockSetup::Mode::Auto);
        expectEqual(setup.shiftMode(), ClockSetup::ShiftMode::Restart);
        expectEqual(setup.clockInputDivisor(), 12);
        expectEqual(setup.clockInputMode(), ClockSetup::ClockInputMode::Reset);
        expectEqual(setup.clockOutputDivisor(), 12);
        expectFalse(setup.clockOutputSwing());
        expectEqual(setup.clockOutputPulse(), 1);
        expectEqual(setup.clockOutputMode(), ClockSetup::ClockOutputMode::Reset);
        expectTrue(setup.midiRx());
        expectTrue(setup.midiTx());
        expectFalse(setup.usbRx());
        expectFalse(setup.usbTx());
        expectTrue(setup.isDirty());
    }

    CASE("divisors clamp to current valid ranges") {
        ClockSetup setup;
        setup.clear();

        setup.setClockInputDivisor(-99);
        expectEqual(setup.clockInputDivisor(), 1);

        setup.setClockInputDivisor(999);
        expectEqual(setup.clockInputDivisor(), 192);

        setup.setClockOutputDivisor(-99);
        expectEqual(setup.clockOutputDivisor(), 2);

        setup.setClockOutputDivisor(999);
        expectEqual(setup.clockOutputDivisor(), 192);
    }

    CASE("input divisor string formatting stays stable") {
        ClockSetup setup;
        setup.clear();

        FixedStringBuilder<32> str;
        setup.printClockInputDivisor(str);
        expectEqual((const char *)str, "12 1/16");

        str.reset();
        setup.setClockInputDivisor(16);
        setup.printClockInputDivisor(str);
        expectEqual((const char *)str, "16 1/8T");

        str.reset();
        setup.setClockInputDivisor(17);
        setup.printClockInputDivisor(str);
        expectEqual((const char *)str, "17");
    }

    CASE("reading v10 data keeps clockOutputSwing default") {
        clearBuf();
        writeClockSetupV10();

        ClockSetup setup;
        setup.clear();

        MemoryReader memoryReader(buf, sizeof(buf));
        VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) { memoryReader.read(data, len); }, ProjectVersion::Latest);
        setup.read(reader);

        expectEqual(setup.mode(), ClockSetup::Mode::Slave);
        expectEqual(setup.shiftMode(), ClockSetup::ShiftMode::Pause);
        expectEqual(setup.clockInputDivisor(), 24);
        expectEqual(setup.clockInputMode(), ClockSetup::ClockInputMode::Run);
        expectEqual(setup.clockOutputDivisor(), 48);
        expectFalse(setup.clockOutputSwing());
        expectEqual(setup.clockOutputPulse(), 7);
        expectEqual(setup.clockOutputMode(), ClockSetup::ClockOutputMode::Run);
        expectTrue(setup.midiRx());
        expectFalse(setup.midiTx());
        expectTrue(setup.usbRx());
        expectFalse(setup.usbTx());
    }

    CASE("reading v11 data restores clockOutputSwing") {
        clearBuf();
        writeClockSetupV11();

        ClockSetup setup;
        setup.clear();

        MemoryReader memoryReader(buf, sizeof(buf));
        VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) { memoryReader.read(data, len); }, ProjectVersion::Latest);
        setup.read(reader);

        expectEqual(setup.mode(), ClockSetup::Mode::Master);
        expectEqual(setup.shiftMode(), ClockSetup::ShiftMode::Restart);
        expectEqual(setup.clockInputDivisor(), 12);
        expectEqual(setup.clockInputMode(), ClockSetup::ClockInputMode::StartStop);
        expectEqual(setup.clockOutputDivisor(), 24);
        expectTrue(setup.clockOutputSwing());
        expectEqual(setup.clockOutputPulse(), 5);
        expectEqual(setup.clockOutputMode(), ClockSetup::ClockOutputMode::Reset);
        expectFalse(setup.midiRx());
        expectTrue(setup.midiTx());
        expectFalse(setup.usbRx());
        expectTrue(setup.usbTx());
    }

    CASE("latest write/read roundtrip preserves all clock setup fields") {
        clearBuf();

        ClockSetup writeSetup;
        writeSetup.clear();
        writeSetup.setMode(ClockSetup::Mode::Slave);
        writeSetup.setShiftMode(ClockSetup::ShiftMode::Pause);
        writeSetup.setClockInputDivisor(23);
        writeSetup.setClockInputMode(ClockSetup::ClockInputMode::StartStop);
        writeSetup.setClockOutputDivisor(47);
        writeSetup.setClockOutputSwing(true);
        writeSetup.setClockOutputPulse(9);
        writeSetup.setClockOutputMode(ClockSetup::ClockOutputMode::Run);
        writeSetup.setMidiRx(false);
        writeSetup.setMidiTx(true);
        writeSetup.setUsbRx(true);
        writeSetup.setUsbTx(false);

        MemoryWriter memoryWriter(buf, sizeof(buf));
        VersionedSerializedWriter writer([&memoryWriter] (const void *data, size_t len) { memoryWriter.write(data, len); }, ProjectVersion::Latest);
        writeSetup.write(writer);

        ClockSetup readSetup;
        readSetup.clear();
        MemoryReader memoryReader(buf, sizeof(buf));
        VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) { memoryReader.read(data, len); }, ProjectVersion::Latest);
        readSetup.read(reader);

        expectEqual(readSetup.mode(), ClockSetup::Mode::Slave);
        expectEqual(readSetup.shiftMode(), ClockSetup::ShiftMode::Pause);
        expectEqual(readSetup.clockInputDivisor(), 23);
        expectEqual(readSetup.clockInputMode(), ClockSetup::ClockInputMode::StartStop);
        expectEqual(readSetup.clockOutputDivisor(), 47);
        expectTrue(readSetup.clockOutputSwing());
        expectEqual(readSetup.clockOutputPulse(), 9);
        expectEqual(readSetup.clockOutputMode(), ClockSetup::ClockOutputMode::Run);
        expectFalse(readSetup.midiRx());
        expectTrue(readSetup.midiTx());
        expectTrue(readSetup.usbRx());
        expectFalse(readSetup.usbTx());

        expectEqual(memoryWriter.bytesWritten(), memoryReader.bytesRead());
    }

    CASE("read does not implicitly change dirty flag state") {
        clearBuf();
        writeClockSetupV11();

        ClockSetup setup;
        setup.clear();
        setup.clearDirty();
        expectFalse(setup.isDirty());

        MemoryReader memoryReader(buf, sizeof(buf));
        VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) { memoryReader.read(data, len); }, ProjectVersion::Latest);
        setup.read(reader);

        expectFalse(setup.isDirty());
    }

    CASE("version boundary: v10 ignores swing bit, v11 reads swing bit") {
        ClockSetup v10Setup;
        v10Setup.clear();
        clearBuf();
        writeClockSetupV10();
        MemoryReader v10ReaderMem(buf, sizeof(buf));
        VersionedSerializedReader v10Reader([&v10ReaderMem] (void *data, size_t len) { v10ReaderMem.read(data, len); }, ProjectVersion::Latest);
        v10Setup.read(v10Reader);
        expectFalse(v10Setup.clockOutputSwing());

        ClockSetup v11Setup;
        v11Setup.clear();
        clearBuf();
        writeClockSetupV11NoSwing();
        MemoryReader v11ReaderMem(buf, sizeof(buf));
        VersionedSerializedReader v11Reader([&v11ReaderMem] (void *data, size_t len) { v11ReaderMem.read(data, len); }, ProjectVersion::Latest);
        v11Setup.read(v11Reader);
        expectFalse(v11Setup.clockOutputSwing());

        clearBuf();
        writeClockSetupV11();
        MemoryReader v11ReaderMemSwing(buf, sizeof(buf));
        VersionedSerializedReader v11ReaderSwing([&v11ReaderMemSwing] (void *data, size_t len) { v11ReaderMemSwing.read(data, len); }, ProjectVersion::Latest);
        v11Setup.read(v11ReaderSwing);
        expectTrue(v11Setup.clockOutputSwing());
    }

    CASE("dirty flag only flips on actual value changes") {
        ClockSetup setup;
        setup.clear();
        expectTrue(setup.isDirty());

        setup.clearDirty();
        expectFalse(setup.isDirty());

        setup.setClockInputDivisor(setup.clockInputDivisor());
        expectFalse(setup.isDirty());

        setup.setClockInputDivisor(setup.clockInputDivisor() + 1);
        expectTrue(setup.isDirty());

        setup.clearDirty();
        setup.setClockOutputSwing(setup.clockOutputSwing());
        expectFalse(setup.isDirty());

        setup.setClockOutputSwing(!setup.clockOutputSwing());
        expectTrue(setup.isDirty());

        setup.clearDirty();
        setup.setMode(setup.mode());
        expectFalse(setup.isDirty());

        setup.setMode(ClockSetup::Mode::Slave);
        expectTrue(setup.isDirty());

        setup.clearDirty();
        setup.setMidiRx(setup.midiRx());
        expectFalse(setup.isDirty());

        setup.setMidiRx(!setup.midiRx());
        expectTrue(setup.isDirty());
    }

    CASE("clock output pulse clamps to [1..20]") {
        ClockSetup setup;
        setup.clear();

        setup.setClockOutputPulse(7);
        expectEqual(setup.clockOutputPulse(), 7);

        setup.setClockOutputPulse(-99);
        expectEqual(setup.clockOutputPulse(), 1);

        setup.setClockOutputPulse(999);
        expectEqual(setup.clockOutputPulse(), 20);
    }

    CASE("output divisor and pulse string formatting stays stable") {
        ClockSetup setup;
        setup.clear();

        FixedStringBuilder<32> outDiv;
        setup.setClockOutputDivisor(24);
        setup.printClockOutputDivisor(outDiv);
        expectEqual((const char *)outDiv, "24 1/8");

        FixedStringBuilder<32> pulse;
        setup.setClockOutputPulse(9);
        setup.printClockOutputPulse(pulse);
        expectEqual((const char *)pulse, "9ms");
    }

    CASE("enum setters clamp wrapped/out-of-range values into valid enum domain") {
        ClockSetup setup;
        setup.clear();

        setup.setMode(ClockSetup::Mode(-123));
        expectEqual(setup.mode(), ClockSetup::Mode::Slave);
        setup.setMode(ClockSetup::Mode(123));
        expectEqual(setup.mode(), ClockSetup::Mode::Slave);

        setup.setClockInputMode(ClockSetup::ClockInputMode(-123));
        expectEqual(setup.clockInputMode(), ClockSetup::ClockInputMode::StartStop);
        setup.setClockInputMode(ClockSetup::ClockInputMode(123));
        expectEqual(setup.clockInputMode(), ClockSetup::ClockInputMode::StartStop);

        setup.setClockOutputMode(ClockSetup::ClockOutputMode(-123));
        expectEqual(setup.clockOutputMode(), ClockSetup::ClockOutputMode::Run);
        setup.setClockOutputMode(ClockSetup::ClockOutputMode(123));
        expectEqual(setup.clockOutputMode(), ClockSetup::ClockOutputMode::Run);
    }

}

