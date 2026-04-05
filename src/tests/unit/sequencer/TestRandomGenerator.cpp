#include "UnitTest.h"

#include "apps/sequencer/engine/generators/RandomGenerator.h"

#include "apps/sequencer/model/NoteSequence.h"

#include "core/utils/StringBuilder.h"

namespace {

static bool patternsEqual(const GeneratorPattern &a, const GeneratorPattern &b) {
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static bool hasAnyDifference(const GeneratorPattern &a, const GeneratorPattern &b) {
    return !patternsEqual(a, b);
}

} // namespace

UNIT_TEST("RandomGenerator") {

    CASE("constructor update is deterministic for identical params") {
        NoteSequence sequenceA;
        sequenceA.clear();
        RandomGenerator::Params paramsA;
        paramsA.seed = 123;
        paramsA.smooth = 0;
        paramsA.bias = 0;
        paramsA.scale = 10;
        NoteSequenceBuilder builderA(sequenceA, NoteSequence::Layer::Note);
        RandomGenerator genA(builderA, paramsA);

        NoteSequence sequenceB;
        sequenceB.clear();
        RandomGenerator::Params paramsB;
        paramsB.seed = 123;
        paramsB.smooth = 0;
        paramsB.bias = 0;
        paramsB.scale = 10;
        NoteSequenceBuilder builderB(sequenceB, NoteSequence::Layer::Note);
        RandomGenerator genB(builderB, paramsB);

        expectTrue(patternsEqual(genA.pattern(), genB.pattern()));
    }

    CASE("paramName, editParam and printParam cover clamping and formatting") {
        NoteSequence sequence;
        sequence.clear();
        RandomGenerator::Params params;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        RandomGenerator gen(builder, params);

        expectTrue(gen.paramName(int(RandomGenerator::Param::Seed)) != nullptr);
        expectTrue(gen.paramName(int(RandomGenerator::Param::Smooth)) != nullptr);
        expectTrue(gen.paramName(int(RandomGenerator::Param::Bias)) != nullptr);
        expectTrue(gen.paramName(int(RandomGenerator::Param::Scale)) != nullptr);
        expectFalse(gen.paramName(int(RandomGenerator::Param::Last)) != nullptr);

        gen.editParam(int(RandomGenerator::Param::Seed), 5000, false);
        gen.editParam(int(RandomGenerator::Param::Smooth), 100, false);
        gen.editParam(int(RandomGenerator::Param::Bias), -100, false);
        gen.editParam(int(RandomGenerator::Param::Scale), -100, false);

        expectEqual(gen.seed(), 1000);
        expectEqual(gen.smooth(), 10);
        expectEqual(gen.bias(), -10);
        expectEqual(gen.scale(), 0);

        FixedStringBuilder<32> seed;
        FixedStringBuilder<32> smooth;
        FixedStringBuilder<32> bias;
        FixedStringBuilder<32> scale;

        gen.printParam(int(RandomGenerator::Param::Seed), seed);
        gen.printParam(int(RandomGenerator::Param::Smooth), smooth);
        gen.printParam(int(RandomGenerator::Param::Bias), bias);
        gen.printParam(int(RandomGenerator::Param::Scale), scale);

        expectEqual((const char *)seed, "1000");
        expectEqual((const char *)smooth, "10");
        expectEqual((const char *)bias, "-10");
        expectEqual((const char *)scale, "0");

        const int seedBefore = gen.seed();
        const int smoothBefore = gen.smooth();
        const int biasBefore = gen.bias();
        const int scaleBefore = gen.scale();

        gen.editParam(int(RandomGenerator::Param::Last), 123, false);

        expectEqual(gen.seed(), seedBefore);
        expectEqual(gen.smooth(), smoothBefore);
        expectEqual(gen.bias(), biasBefore);
        expectEqual(gen.scale(), scaleBefore);

        FixedStringBuilder<32> untouched("unchanged");
        gen.printParam(int(RandomGenerator::Param::Last), untouched);
        expectEqual((const char *)untouched, "unchanged");
    }

    CASE("init restores default params and recomputes the pattern") {
        NoteSequence sequence;
        sequence.clear();
        RandomGenerator::Params params;
        params.seed = 777;
        params.smooth = 6;
        params.bias = 5;
        params.scale = 30;

        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        RandomGenerator gen(builder, params);

        gen.init();

        expectEqual(gen.seed(), 0);
        expectEqual(gen.smooth(), 0);
        expectEqual(gen.bias(), 0);
        expectEqual(gen.scale(), 10);
    }

    CASE("update applies smoothing and scale transformations") {
        NoteSequence sequence;
        sequence.clear();
        RandomGenerator::Params params;
        params.seed = 345;
        params.smooth = 0;
        params.bias = 0;
        params.scale = 10;

        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        RandomGenerator gen(builder, params);
        GeneratorPattern basePattern = gen.pattern();

        gen.setSmooth(10);
        gen.update();
        GeneratorPattern smoothPattern = gen.pattern();
        expectTrue(hasAnyDifference(basePattern, smoothPattern));

        gen.setSmooth(0);
        gen.setScale(0);
        gen.setBias(0);
        gen.update();

        for (size_t i = 0; i < gen.pattern().size(); ++i) {
            expectEqual(int(gen.pattern()[i]), 127);
        }

        gen.setScale(100);
        gen.setBias(10);
        gen.update();

        for (size_t i = 0; i < gen.pattern().size(); ++i) {
            expectTrue(gen.pattern()[i] <= 255);
        }
    }
}

