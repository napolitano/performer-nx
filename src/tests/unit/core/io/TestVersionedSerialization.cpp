#include "UnitTest.h"

#include "MemoryReaderWriter.h"

#include "core/io/VersionedSerializedWriter.h"
#include "core/io/VersionedSerializedReader.h"

#define VERSION(_x_) (_x_)

struct Data1 {
    uint8_t field1 = 123;
    uint16_t field2 = 234;
    uint32_t field3 = 345;
};

struct Data2 {
    uint8_t field1 = 123;
    uint16_t field2 = 234;
    int8_t field4 = -123; // new
    uint32_t field3 = 345;
};

struct Data3 {
    uint8_t field1 = 123;
    uint16_t field2 = 234;
    int8_t field4 = -123;
    int16_t field5 = -234; // new
    uint32_t field3 = 345;
};

struct Data4 {
    uint8_t field1 = 123;
    uint16_t field2 = 234;
    // int8_t field4 = -123; // removed
    int16_t field5 = -234;
    uint32_t field3 = 345;
};

static void writeVersion1(void *buf, size_t len) {
    MemoryWriter memoryWriter(buf, len);
    VersionedSerializedWriter writer([&memoryWriter] (const void *data, size_t len) { memoryWriter.write(data, len); }, 1);
    Data1 data;
    writer.write(data.field1);
    writer.write(data.field2);
    writer.write(data.field3);
    writer.writeHash();
}

static void writeVersion2(void *buf, size_t len) {
    MemoryWriter memoryWriter(buf, len);
    VersionedSerializedWriter writer([&memoryWriter] (const void *data, size_t len) { memoryWriter.write(data, len); }, 2);
    Data2 data;
    writer.write(data.field1);
    writer.write(data.field2);
    writer.write(data.field4);
    writer.write(data.field3);
    writer.writeHash();
}

static void writeVersion3(void *buf, size_t len) {
    MemoryWriter memoryWriter(buf, len);
    VersionedSerializedWriter writer([&memoryWriter] (const void *data, size_t len) { memoryWriter.write(data, len); }, 3);
    Data3 data;
    writer.write(data.field1);
    writer.write(data.field2);
    writer.write(data.field4);
    writer.write(data.field5);
    writer.write(data.field3);
    writer.writeHash();
}

static void writeVersion4(void *buf, size_t len) {
    MemoryWriter memoryWriter(buf, len);
    VersionedSerializedWriter writer([&memoryWriter] (const void *data, size_t len) { memoryWriter.write(data, len); }, 4);
    Data4 data;
    writer.write(data.field1);
    writer.write(data.field2);
    writer.write(data.field5);
    writer.write(data.field3);
    writer.writeHash();
}

static void readVersion1(const void *buf, size_t len) {
    MemoryReader memoryReader(buf, len);
    VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) { memoryReader.read(data, len); }, 1);
    Data1 data {};
    reader.read(data.field1);
    reader.read(data.field2);
    reader.read(data.field3);
    expectTrue(reader.checkHash());
    Data1 expected;
    expectEqual(data.field1, expected.field1);
    expectEqual(data.field2, expected.field2);
    expectEqual(data.field3, expected.field3);
}

static void readVersion2(const void *buf, size_t len) {
    MemoryReader memoryReader(buf, len);
    VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) { memoryReader.read(data, len); }, 2);
    Data2 data {};
    data.field4 = 0;
    reader.read(data.field1);
    reader.read(data.field2);
    reader.read(data.field4, VERSION(2));
    reader.read(data.field3);
    expectTrue(reader.checkHash());
    Data2 expected;
    expectEqual(data.field1, expected.field1);
    expectEqual(data.field2, expected.field2);
    expectEqual(data.field4, reader.dataVersion() >= 2 ? expected.field4 : int8_t(0));
    expectEqual(data.field3, expected.field3);
}

static void readVersion3(const void *buf, size_t len) {
    MemoryReader memoryReader(buf, len);
    VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) { memoryReader.read(data, len); }, 3);
    Data3 data {};
    data.field4 = 0;
    data.field5 = 0;
    reader.read(data.field1);
    reader.read(data.field2);
    reader.read(data.field4, VERSION(2));
    reader.read(data.field5, VERSION(3));
    reader.read(data.field3);
    expectTrue(reader.checkHash());
    Data3 expected;
    expectEqual(data.field1, expected.field1);
    expectEqual(data.field2, expected.field2);
    expectEqual(data.field4, reader.dataVersion() >= 2 ? expected.field4 : int8_t(0));
    expectEqual(data.field5, reader.dataVersion() >= 3 ? expected.field5 : int16_t(0));
    expectEqual(data.field3, expected.field3);
}

static void readVersion4(const void *buf, size_t len) {
    MemoryReader memoryReader(buf, len);
    VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) { memoryReader.read(data, len); }, 4);
    Data4 data {};
    data.field5 = 0;
    reader.read(data.field1);
    reader.read(data.field2);
    reader.skip<int8_t>(VERSION(2), VERSION(4));
    reader.read(data.field5, VERSION(3));
    reader.read(data.field3);
    expectTrue(reader.checkHash());
    Data4 expected;
    expectEqual(data.field1, expected.field1);
    expectEqual(data.field2, expected.field2);
    expectEqual(data.field5, reader.dataVersion() >= 3 ? expected.field5 : int16_t(0));
    expectEqual(data.field3, expected.field3);
}

static uint8_t buf[512];

static void clear() {
    std::memset(buf, 0, sizeof(buf));
}

UNIT_TEST("VersionedSerialization") {

    // Scope:
    // - This test validates generic versioned serialization on an in-memory byte stream (buf).
    // - It does NOT test Project or Settings file formats.
    //
    // Serialized stream layout written by VersionedSerializedWriter:
    // [uint32_t dataVersion][field bytes for that dataVersion][uint32_t checksum]
    //
    // Schema evolution in this synthetic test model:
    // - field4 is introduced in data version 2
    // - field5 is introduced in data version 3
    // - field4 is removed again in data version 4

    CASE("reader v1 decodes data version 1 stream") {
        // Guarantee: the original v1 layout is read correctly and checksum validation passes.
        clear();
        writeVersion1(buf, sizeof(buf));
        readVersion1(buf, sizeof(buf));
    }

    CASE("reader v2 decodes data version 1 and 2 streams") {
        // Guarantee: v2 reader keeps field4 default for v1 data and reads field4 for v2 data.
        clear();
        writeVersion1(buf, sizeof(buf));
        readVersion2(buf, sizeof(buf));

        clear();
        writeVersion2(buf, sizeof(buf));
        readVersion2(buf, sizeof(buf));
    }

    CASE("reader v3 decodes data version 1 through 3 streams") {
        // Guarantee: version-gated reads for field4/field5 behave correctly across v1-v3 inputs.
        clear();
        writeVersion1(buf, sizeof(buf));
        readVersion3(buf, sizeof(buf));

        clear();
        writeVersion2(buf, sizeof(buf));
        readVersion3(buf, sizeof(buf));

        clear();
        writeVersion3(buf, sizeof(buf));
        readVersion3(buf, sizeof(buf));
    }

    CASE("reader v4 decodes data version 1 through 4 streams") {
        // Guarantee: removed field4 is skipped only for versions where it existed, and stream alignment stays correct.
        clear();
        writeVersion1(buf, sizeof(buf));
        readVersion4(buf, sizeof(buf));

        clear();
        writeVersion2(buf, sizeof(buf));
        readVersion4(buf, sizeof(buf));

        clear();
        writeVersion3(buf, sizeof(buf));
        readVersion4(buf, sizeof(buf));

        clear();
        writeVersion4(buf, sizeof(buf));
        readVersion4(buf, sizeof(buf));
    }

    CASE("field4 remains default when data version is below 2") {
        // Guarantee: read(..., addedInVersion=2) does not consume bytes on v1 data and field4 keeps pre-set default.
        clear();
        writeVersion1(buf, sizeof(buf));

        MemoryReader memoryReader(buf, sizeof(buf));
        VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) { memoryReader.read(data, len); }, 2);
        Data2 data {};
        data.field4 = 0;
        reader.read(data.field1);
        reader.read(data.field2);
        reader.read(data.field4, VERSION(2));
        reader.read(data.field3);

        expectEqual(reader.dataVersion(), uint32_t(1));
        expectEqual(data.field4, int8_t(0));
        expectTrue(reader.checkHash());
    }

    CASE("field4 is read from stream when data version is 2") {
        // Guarantee: read(..., addedInVersion=2) consumes field4 bytes on v2 data and restores serialized value.
        clear();
        writeVersion2(buf, sizeof(buf));

        MemoryReader memoryReader(buf, sizeof(buf));
        VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) { memoryReader.read(data, len); }, 2);
        Data2 data {};
        data.field4 = 0;
        reader.read(data.field1);
        reader.read(data.field2);
        reader.read(data.field4, VERSION(2));
        reader.read(data.field3);

        expectEqual(reader.dataVersion(), uint32_t(2));
        expectEqual(data.field4, int8_t(-123));
        expectTrue(reader.checkHash());
    }

    CASE("field5 remains default when data version is below 3") {
        // Guarantee: read(..., addedInVersion=3) does not consume bytes on v2 data and field5 keeps pre-set default.
        clear();
        writeVersion2(buf, sizeof(buf));

        MemoryReader memoryReader(buf, sizeof(buf));
        VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) { memoryReader.read(data, len); }, 3);
        Data3 data {};
        data.field4 = 0;
        data.field5 = 0;
        reader.read(data.field1);
        reader.read(data.field2);
        reader.read(data.field4, VERSION(2));
        reader.read(data.field5, VERSION(3));
        reader.read(data.field3);

        expectEqual(reader.dataVersion(), uint32_t(2));
        expectEqual(data.field5, int16_t(0));
        expectTrue(reader.checkHash());
    }

    CASE("field5 is read from stream when data version is 3") {
        // Guarantee: read(..., addedInVersion=3) consumes field5 bytes on v3 data and restores serialized value.
        clear();
        writeVersion3(buf, sizeof(buf));

        MemoryReader memoryReader(buf, sizeof(buf));
        VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) { memoryReader.read(data, len); }, 3);
        Data3 data {};
        data.field4 = 0;
        data.field5 = 0;
        reader.read(data.field1);
        reader.read(data.field2);
        reader.read(data.field4, VERSION(2));
        reader.read(data.field5, VERSION(3));
        reader.read(data.field3);

        expectEqual(reader.dataVersion(), uint32_t(3));
        expectEqual(data.field5, int16_t(-234));
        expectTrue(reader.checkHash());
    }

    CASE("skip consumes removed field4 bytes for data versions 2 and 3") {
        // Guarantee: skip<int8_t>(2,4) advances stream position for historical layouts that still contain field4.
        clear();
        writeVersion2(buf, sizeof(buf));

        MemoryReader memoryReader(buf, sizeof(buf));
        VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) { memoryReader.read(data, len); }, 4);
        Data4 data {};
        data.field5 = 0;
        reader.read(data.field1);
        reader.read(data.field2);
        reader.skip<int8_t>(VERSION(2), VERSION(4));
        reader.read(data.field5, VERSION(3));
        reader.read(data.field3);

        expectEqual(reader.dataVersion(), uint32_t(2));
        expectEqual(data.field5, int16_t(0));
        expectTrue(reader.checkHash());
    }

    CASE("skip does nothing for removed field4 at data version 4") {
        // Guarantee: skip<int8_t>(2,4) does not consume bytes once field4 is removed from the layout.
        clear();
        writeVersion4(buf, sizeof(buf));

        MemoryReader memoryReader(buf, sizeof(buf));
        VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) { memoryReader.read(data, len); }, 4);
        Data4 data {};
        data.field5 = 0;
        reader.read(data.field1);
        reader.read(data.field2);
        reader.skip<int8_t>(VERSION(2), VERSION(4));
        reader.read(data.field5, VERSION(3));
        reader.read(data.field3);

        expectEqual(reader.dataVersion(), uint32_t(4));
        expectEqual(data.field5, int16_t(-234));
        expectTrue(reader.checkHash());
    }

    CASE("checksum validation passes for unmodified stream") {
        // Guarantee: checkHash() returns true when serialized bytes are read without tampering.
        clear();
        writeVersion3(buf, sizeof(buf));

        MemoryReader memoryReader(buf, sizeof(buf));
        VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) { memoryReader.read(data, len); }, 3);
        Data3 data {};
        data.field4 = 0;
        data.field5 = 0;
        reader.read(data.field1);
        reader.read(data.field2);
        reader.read(data.field4, VERSION(2));
        reader.read(data.field5, VERSION(3));
        reader.read(data.field3);

        expectTrue(reader.checkHash());
    }

    CASE("checksum validation fails after payload corruption") {
        // Guarantee: checkHash() returns false when any payload byte changes after serialization.
        clear();
        writeVersion3(buf, sizeof(buf));
        buf[4] ^= 0x01;

        MemoryReader memoryReader(buf, sizeof(buf));
        VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) { memoryReader.read(data, len); }, 3);
        Data3 data {};
        data.field4 = 0;
        data.field5 = 0;
        reader.read(data.field1);
        reader.read(data.field2);
        reader.read(data.field4, VERSION(2));
        reader.read(data.field5, VERSION(3));
        reader.read(data.field3);

        expectFalse(reader.checkHash());
    }

}
