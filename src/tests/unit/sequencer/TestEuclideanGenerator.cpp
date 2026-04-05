#include "UnitTest.h"

#include "apps/sequencer/engine/generators/EuclideanGenerator.h"

#include "apps/sequencer/model/NoteSequence.h"

#include "core/utils/StringBuilder.h"

namespace {

static bool patternEquals(const Rhythm::Pattern &a, const Rhythm::Pattern &b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

} // namespace

UNIT_TEST("EuclideanGenerator") {

    CASE("constructor update builds expected shifted euclidean pattern and sets sequence length") {
        NoteSequence sequence;
        sequence.clear();
        EuclideanGenerator::Params params;
        params.steps = 13;
        params.beats = 5;
        params.offset = 3;

        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Gate);
        EuclideanGenerator generator(builder, params);

        const auto expected = Rhythm::euclidean(params.beats, params.steps).shifted(params.offset);

        expectTrue(patternEquals(generator.pattern(), expected));
        expectEqual(builder.length(), int(params.steps));

        for (int i = 0; i < CONFIG_STEP_COUNT; ++i) {
            const float expectedValue = expected[i % expected.size()] ? 1.f : 0.f;
            expectEqual(builder.value(i), expectedValue);
        }
    }

    CASE("paramName, editParam and printParam cover clamping formatting and Param::Last no-op") {
        NoteSequence sequence;
        sequence.clear();
        EuclideanGenerator::Params params;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Gate);
        EuclideanGenerator generator(builder, params);

        expectTrue(generator.paramName(int(EuclideanGenerator::Param::Steps)) != nullptr);
        expectTrue(generator.paramName(int(EuclideanGenerator::Param::Beats)) != nullptr);
        expectTrue(generator.paramName(int(EuclideanGenerator::Param::Offset)) != nullptr);
        expectFalse(generator.paramName(int(EuclideanGenerator::Param::Last)) != nullptr);

        generator.editParam(int(EuclideanGenerator::Param::Steps), 1000, false);
        generator.editParam(int(EuclideanGenerator::Param::Beats), 1000, false);
        generator.editParam(int(EuclideanGenerator::Param::Offset), 1000, false);

        expectEqual(generator.steps(), CONFIG_STEP_COUNT);
        expectEqual(generator.beats(), CONFIG_STEP_COUNT);
        expectEqual(generator.offset(), CONFIG_STEP_COUNT - 1);

        FixedStringBuilder<32> steps;
        FixedStringBuilder<32> beats;
        FixedStringBuilder<32> offset;

        generator.printParam(int(EuclideanGenerator::Param::Steps), steps);
        generator.printParam(int(EuclideanGenerator::Param::Beats), beats);
        generator.printParam(int(EuclideanGenerator::Param::Offset), offset);

        expectEqual((const char *)steps, "64");
        expectEqual((const char *)beats, "64");
        expectEqual((const char *)offset, "63");

        const int stepsBefore = generator.steps();
        const int beatsBefore = generator.beats();
        const int offsetBefore = generator.offset();

        generator.editParam(int(EuclideanGenerator::Param::Last), 123, false);

        expectEqual(generator.steps(), stepsBefore);
        expectEqual(generator.beats(), beatsBefore);
        expectEqual(generator.offset(), offsetBefore);

        FixedStringBuilder<32> untouched("unchanged");
        generator.printParam(int(EuclideanGenerator::Param::Last), untouched);
        expectEqual((const char *)untouched, "unchanged");
    }

    CASE("init restores default params and recomputes pattern") {
        NoteSequence sequence;
        sequence.clear();
        EuclideanGenerator::Params params;
        params.steps = 31;
        params.beats = 17;
        params.offset = 9;

        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Gate);
        EuclideanGenerator generator(builder, params);

        generator.init();

        expectEqual(generator.steps(), 16);
        expectEqual(generator.beats(), 4);
        expectEqual(generator.offset(), 0);
        expectEqual(builder.length(), 16);

        const auto expected = Rhythm::euclidean(4, 16).shifted(0);
        expectTrue(patternEquals(generator.pattern(), expected));
    }

    CASE("update handles beats larger than steps by delegating clamped rhythm generation") {
        NoteSequence sequence;
        sequence.clear();
        EuclideanGenerator::Params params;
        params.steps = 4;
        params.beats = 64;
        params.offset = 1;

        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Gate);
        EuclideanGenerator generator(builder, params);

        const auto expected = Rhythm::euclidean(params.beats, params.steps).shifted(params.offset);
        expectTrue(patternEquals(generator.pattern(), expected));

        for (int i = 0; i < 12; ++i) {
            expectEqual(builder.value(i), expected[i % expected.size()] ? 1.f : 0.f);
        }
    }
}


