#include "UnitTest.h"

#include "apps/sequencer/Config.h"

#ifdef CONFIG_ACID_BASS_GENERATOR

#include "apps/sequencer/engine/generators/AcidBasslineGenerator.h"
#include "apps/sequencer/engine/generators/SequenceBuilder.h"
#include "apps/sequencer/model/NoteSequence.h"

#include "core/utils/StringBuilder.h"

UNIT_TEST("AcidBasslineGenerator") {

    CASE("mode() returns AcidBassline") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        expectEqual(gen.mode(), Generator::Mode::AcidBassline);
    }

    CASE("paramCount() equals the number of Param enum entries") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        expectEqual(gen.paramCount(), int(AcidBasslineGenerator::Param::Last));
    }

    CASE("paramName returns non-null for every param and nullptr for Last") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        expectTrue(gen.paramName(int(AcidBasslineGenerator::Param::Seed))          != nullptr);
        expectTrue(gen.paramName(int(AcidBasslineGenerator::Param::RootNote))       != nullptr);
        expectTrue(gen.paramName(int(AcidBasslineGenerator::Param::PatternLength))  != nullptr);
        expectTrue(gen.paramName(int(AcidBasslineGenerator::Param::Density))        != nullptr);
        expectTrue(gen.paramName(int(AcidBasslineGenerator::Param::LegatoMix))      != nullptr);
        expectFalse(gen.paramName(int(AcidBasslineGenerator::Param::Last))          != nullptr);
    }

    CASE("same seed produces identical patterns (determinism)") {
        NoteSequence seqA, seqB;
        seqA.clear(); seqB.clear();
        AcidBasslineGenerator::Params paramsA, paramsB;
        paramsA.seed = 42;
        paramsB.seed = 42;
        NoteSequenceBuilder builderA(seqA, NoteSequence::Layer::Note);
        NoteSequenceBuilder builderB(seqB, NoteSequence::Layer::Note);
        AcidBasslineGenerator genA(builderA, paramsA);
        AcidBasslineGenerator genB(builderB, paramsB);

        for (size_t i = 0; i < genA.pattern().size(); ++i) {
            expectEqual(int(genA.pattern()[i]), int(genB.pattern()[i]));
        }
    }

    CASE("different seeds produce different patterns") {
        NoteSequence seqA, seqB;
        seqA.clear(); seqB.clear();
        AcidBasslineGenerator::Params paramsA, paramsB;
        paramsA.seed = 1;
        paramsB.seed = 2;
        NoteSequenceBuilder builderA(seqA, NoteSequence::Layer::Note);
        NoteSequenceBuilder builderB(seqB, NoteSequence::Layer::Note);
        AcidBasslineGenerator genA(builderA, paramsA);
        AcidBasslineGenerator genB(builderB, paramsB);

        bool anyDiff = false;
        for (size_t i = 0; i < genA.pattern().size(); ++i) {
            if (genA.pattern()[i] != genB.pattern()[i]) {
                anyDiff = true;
                break;
            }
        }
        expectTrue(anyDiff);
    }

    CASE("setSeed clamps to [0, 65535]") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        gen.setSeed(-1);
        expectEqual(gen.seed(), 0);

        gen.setSeed(65536);
        expectEqual(gen.seed(), 65535);

        gen.setSeed(1000);
        expectEqual(gen.seed(), 1000);
    }

    CASE("setRootNote clamps to [0, 11]") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        gen.setRootNote(-1);
        expectEqual(gen.rootNote(), 0);

        gen.setRootNote(12);
        expectEqual(gen.rootNote(), 11);

        gen.setRootNote(5);
        expectEqual(gen.rootNote(), 5);
    }

    CASE("setPatternLength clamps to [1, CONFIG_STEP_COUNT]") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        gen.setPatternLength(0);
        expectEqual(gen.patternLength(), 1);

        gen.setPatternLength(CONFIG_STEP_COUNT + 1);
        expectEqual(gen.patternLength(), CONFIG_STEP_COUNT);

        gen.setPatternLength(32);
        expectEqual(gen.patternLength(), 32);
    }

    CASE("setDensity clamps to [0, 100]") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        gen.setDensity(-1);
        expectEqual(gen.density(), 0);

        gen.setDensity(101);
        expectEqual(gen.density(), 100);

        gen.setDensity(50);
        expectEqual(gen.density(), 50);
    }

    CASE("setLegatoMix clamps to [0, 100]") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        gen.setLegatoMix(-1);
        expectEqual(gen.legatoMix(), 0);

        gen.setLegatoMix(101);
        expectEqual(gen.legatoMix(), 100);

        gen.setLegatoMix(70);
        expectEqual(gen.legatoMix(), 70);
    }

    CASE("editParam without shift increments all params by value") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.seed          = 100;
        params.rootNote      = 5;
        params.patternLength = 16;
        params.density       = 50;
        params.legatoMix     = 50;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        gen.editParam(int(AcidBasslineGenerator::Param::Seed), 10, false);
        expectEqual(gen.seed(), 110);

        gen.editParam(int(AcidBasslineGenerator::Param::RootNote), 3, false);
        expectEqual(gen.rootNote(), 8);

        gen.editParam(int(AcidBasslineGenerator::Param::PatternLength), 4, false);
        expectEqual(gen.patternLength(), 20);

        gen.editParam(int(AcidBasslineGenerator::Param::Density), 5, false);
        expectEqual(gen.density(), 55);

        gen.editParam(int(AcidBasslineGenerator::Param::LegatoMix), -10, false);
        expectEqual(gen.legatoMix(), 40);
    }

    CASE("editParam Param::Last is a no-op") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.seed          = 42;
        params.rootNote      = 3;
        params.patternLength = 16;
        params.density       = 50;
        params.legatoMix     = 35;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        gen.editParam(int(AcidBasslineGenerator::Param::Last), 1, false);

        expectEqual(gen.seed(),          42);
        expectEqual(gen.rootNote(),       3);
        expectEqual(gen.patternLength(), 16);
        expectEqual(gen.density(),       50);
        expectEqual(gen.legatoMix(),     35);
    }

    CASE("editParam shift=true on Seed produces a value in [0, 65535]") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.seed = 0;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        gen.editParam(int(AcidBasslineGenerator::Param::Seed), 1, true);

        expectTrue(gen.seed() >= 0);
        expectTrue(gen.seed() <= 65535);
    }

    CASE("editParam shift=true on PatternLength snaps to next/prev multiple of 16") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.patternLength = 16;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        gen.editParam(int(AcidBasslineGenerator::Param::PatternLength), 1, true);
        expectEqual(gen.patternLength(), 32);

        gen.editParam(int(AcidBasslineGenerator::Param::PatternLength), -1, true);
        expectEqual(gen.patternLength(), 16);
    }

    CASE("editParam shift=true on Density snaps to next/prev multiple of 25") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.density = 50;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        gen.editParam(int(AcidBasslineGenerator::Param::Density), 1, true);
        expectEqual(gen.density(), 75);

        gen.editParam(int(AcidBasslineGenerator::Param::Density), -1, true);
        expectEqual(gen.density(), 50);
    }

    CASE("editParam shift=true on LegatoMix snaps to next/prev multiple of 25") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.legatoMix = 25;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        gen.editParam(int(AcidBasslineGenerator::Param::LegatoMix), 1, true);
        expectEqual(gen.legatoMix(), 50);

        gen.editParam(int(AcidBasslineGenerator::Param::LegatoMix), -1, true);
        expectEqual(gen.legatoMix(), 25);
    }

    CASE("printParam formats seed and patternLength as integers, density and legatoMix as percentages") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.seed          = 12345;
        params.rootNote      = 0;
        params.patternLength = 32;
        params.density       = 75;
        params.legatoMix     = 50;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        FixedStringBuilder<64> seed, length, density, legato;
        gen.printParam(int(AcidBasslineGenerator::Param::Seed),          seed);
        gen.printParam(int(AcidBasslineGenerator::Param::PatternLength), length);
        gen.printParam(int(AcidBasslineGenerator::Param::Density),       density);
        gen.printParam(int(AcidBasslineGenerator::Param::LegatoMix),     legato);

        expectEqual((const char *)seed,    "12345");
        expectEqual((const char *)length,  "32");
        expectEqual((const char *)density, "75%");
        expectEqual((const char *)legato,  "50%");
    }

    CASE("printParam Param::Last leaves the string unchanged") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        FixedStringBuilder<32> untouched("unchanged");
        gen.printParam(int(AcidBasslineGenerator::Param::Last), untouched);
        expectEqual((const char *)untouched, "unchanged");
    }

    CASE("init() resets all parameters to their default values") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.seed          = 9999;
        params.rootNote      = 11;
        params.patternLength = 48;
        params.density       = 10;
        params.legatoMix     = 90;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        gen.init();

        expectEqual(gen.seed(),          0);
        expectEqual(gen.rootNote(),       0);
        expectEqual(gen.patternLength(), 16);
        expectEqual(gen.density(),       62);
        expectEqual(gen.legatoMix(),     35);
    }

    CASE("density=100 gates every step in the pattern") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.seed          = 7;
        params.density       = 100;
        params.patternLength = 16;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Gate);
        AcidBasslineGenerator gen(builder, params);

        for (int i = 0; i < 16; ++i) {
            expectTrue(sequence.step(i).gate());
        }
    }

    CASE("step 0 is always gated regardless of seed or density") {
        for (int seed = 0; seed < 8; ++seed) {
            NoteSequence sequence;
            sequence.clear();
            AcidBasslineGenerator::Params params;
            params.seed          = uint16_t(seed);
            params.density       = 62;
            params.patternLength = 16;
            NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Gate);
            AcidBasslineGenerator gen(builder, params);

            expectTrue(sequence.step(0).gate());
        }
    }

    CASE("patternLength sets firstStep=0 and lastStep=length-1 in the sequence") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.seed          = 55;
        params.patternLength = 24;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Gate);
        AcidBasslineGenerator gen(builder, params);

        expectEqual(sequence.firstStep(), 0);
        expectEqual(sequence.lastStep(),  23);
    }

    CASE("steps beyond patternLength have no gate") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.seed          = 123;
        params.density       = 100;
        params.patternLength = 16;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Gate);
        AcidBasslineGenerator gen(builder, params);

        for (int i = 16; i < CONFIG_STEP_COUNT; ++i) {
            expectFalse(sequence.step(i).gate());
        }
    }

    CASE("different rootNote values produce different note outputs") {
        NoteSequence seqRoot0, seqRoot5;
        seqRoot0.clear(); seqRoot5.clear();

        AcidBasslineGenerator::Params params0;
        params0.seed          = 333;
        params0.rootNote      = 0;
        params0.density       = 100;
        params0.patternLength = 16;

        AcidBasslineGenerator::Params params5 = params0;
        params5.rootNote = 5;

        NoteSequenceBuilder builder0(seqRoot0, NoteSequence::Layer::Note);
        NoteSequenceBuilder builder5(seqRoot5, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen0(builder0, params0);
        AcidBasslineGenerator gen5(builder5, params5);

        bool anyDifference = false;
        for (int i = 0; i < 16; ++i) {
            if (seqRoot0.step(i).note() != seqRoot5.step(i).note()) {
                anyDifference = true;
                break;
            }
        }
        expectTrue(anyDifference);
    }

    CASE("update() after seed change regenerates a different pattern") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.seed          = 10;
        params.density       = 100;
        params.patternLength = 16;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);
        GeneratorPattern before = gen.pattern();

        gen.setSeed(9999);
        gen.update();
        GeneratorPattern after = gen.pattern();

        bool anyDiff = false;
        for (size_t i = 0; i < before.size(); ++i) {
            if (before[i] != after[i]) {
                anyDiff = true;
                break;
            }
        }
        expectTrue(anyDiff);
    }

    CASE("note values written to the sequence are within NoteSequence::Note bounds") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.seed          = 9876;
        params.density       = 100;
        params.patternLength = 32;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        for (int i = 0; i < 32; ++i) {
            const int note = sequence.step(i).note();
            expectTrue(note >= NoteSequence::Note::Min);
            expectTrue(note <= NoteSequence::Note::Max);
        }
    }

    CASE("gateProbability values are within valid range for gated steps") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.seed          = 111;
        params.density       = 100;
        params.patternLength = 16;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::GateProbability);
        AcidBasslineGenerator gen(builder, params);

        for (int i = 0; i < 16; ++i) {
            if (sequence.step(i).gate()) {
                const int gp = sequence.step(i).gateProbability();
                expectTrue(gp >= NoteSequence::GateProbability::Min);
                expectTrue(gp <= NoteSequence::GateProbability::Max);
            }
        }
    }

    CASE("higher legatoMix produces greater or equal total step length than legatoMix=0") {
        NoteSequence seqLow, seqHigh;
        seqLow.clear(); seqHigh.clear();

        AcidBasslineGenerator::Params paramsLow;
        paramsLow.seed          = 77;
        paramsLow.density       = 100;
        paramsLow.patternLength = 32;
        paramsLow.legatoMix     = 0;

        AcidBasslineGenerator::Params paramsHigh = paramsLow;
        paramsHigh.legatoMix = 100;

        NoteSequenceBuilder builderLow(seqLow,   NoteSequence::Layer::Length);
        NoteSequenceBuilder builderHigh(seqHigh,  NoteSequence::Layer::Length);
        AcidBasslineGenerator genLow(builderLow,   paramsLow);
        AcidBasslineGenerator genHigh(builderHigh, paramsHigh);

        int totalLow = 0, totalHigh = 0;
        for (int i = 0; i < 32; ++i) {
            totalLow  += seqLow.step(i).length();
            totalHigh += seqHigh.step(i).length();
        }
        expectTrue(totalHigh >= totalLow);
    }

    CASE("printParam formats RootNote as note name") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.rootNote = 0;  // C
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        FixedStringBuilder<32> rootNote;
        gen.printParam(int(AcidBasslineGenerator::Param::RootNote), rootNote);
        expectEqual((const char *)rootNote, "C");
    }

    CASE("gate offset values are within valid bounds for gated steps") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.seed          = 77;
        params.density       = 100;
        params.patternLength = 32;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::GateOffset);
        AcidBasslineGenerator gen(builder, params);

        for (int i = 0; i < 32; ++i) {
            if (sequence.step(i).gate()) {
                const int offset = sequence.step(i).gateOffset();
                expectTrue(offset >= 0);
                expectTrue(offset <= NoteSequence::GateOffset::Max);
            }
        }
    }

    CASE("legatoMix=100 and density=100 produce at least one slide across 32 steps") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.seed          = 100;
        params.density       = 100;
        params.legatoMix     = 100;
        params.patternLength = 32;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Slide);
        AcidBasslineGenerator gen(builder, params);

        bool anySlide = false;
        for (int i = 0; i < 32; ++i) {
            if (sequence.step(i).slide()) {
                anySlide = true;
                break;
            }
        }
        expectTrue(anySlide);
    }

    CASE("applyScale uses sequence-local rootNote when non-negative") {
        AcidBasslineGenerator::Params params;
        params.seed          = 500;
        params.density       = 100;
        params.patternLength = 16;
        params.rootNote      = 0;  // generator root: C

        NoteSequence seqDefault, seqOverride;
        seqDefault.clear();
        seqOverride.clear();

        // seqDefault: sequence rootNote = -1 (use params.rootNote = 0)
        NoteSequenceBuilder builderDefault(seqDefault, NoteSequence::Layer::Note);
        AcidBasslineGenerator genDefault(builderDefault, params);

        // seqOverride: rootNote=5 on the sequence overrides params.rootNote
        seqOverride.setRootNote(5);
        NoteSequenceBuilder builderOverride(seqOverride, NoteSequence::Layer::Note);
        AcidBasslineGenerator genOverride(builderOverride, params);

        bool anyDifference = false;
        for (int i = 0; i < 16; ++i) {
            if (seqDefault.step(i).note() != seqOverride.step(i).note()) {
                anyDifference = true;
                break;
            }
        }
        expectTrue(anyDifference);
    }

    CASE("density=0 produces fewer gated steps than density=100") {
        NoteSequence seqFull, seqSparse;
        seqFull.clear(); seqSparse.clear();

        AcidBasslineGenerator::Params paramsFull;
        paramsFull.seed          = 42;
        paramsFull.density       = 100;
        paramsFull.patternLength = 32;

        AcidBasslineGenerator::Params paramsSparse = paramsFull;
        paramsSparse.density = 0;

        NoteSequenceBuilder builderFull(seqFull,     NoteSequence::Layer::Gate);
        NoteSequenceBuilder builderSparse(seqSparse,  NoteSequence::Layer::Gate);
        AcidBasslineGenerator genFull(builderFull,     paramsFull);
        AcidBasslineGenerator genSparse(builderSparse, paramsSparse);

        int countFull = 0, countSparse = 0;
        for (int i = 0; i < 32; ++i) {
            if (seqFull.step(i).gate())   ++countFull;
            if (seqSparse.step(i).gate()) ++countSparse;
        }
        expectTrue(countSparse < countFull);
    }

    CASE("editParam shift=true on Seed with negative value also stays within [0, 65535]") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.seed = 0;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);
        AcidBasslineGenerator gen(builder, params);

        gen.editParam(int(AcidBasslineGenerator::Param::Seed), -1, true);

        expectTrue(gen.seed() >= 0);
        expectTrue(gen.seed() <= 65535);
    }

    CASE("Gate layer: pattern encodes gated steps as 255 and tail as 0") {
        NoteSequence sequence;
        sequence.clear();
        AcidBasslineGenerator::Params params;
        params.seed          = 7;
        params.density       = 100;
        params.patternLength = 16;
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Gate);
        AcidBasslineGenerator gen(builder, params);

        // All 16 steps gated → pattern must be 255
        for (int i = 0; i < 16; ++i) {
            expectEqual(int(gen.pattern()[i]), 255);
        }
        // Tail beyond patternLength must be 0
        for (int i = 16; i < CONFIG_STEP_COUNT; ++i) {
            expectEqual(int(gen.pattern()[i]), 0);
        }
    }

    CASE("fallback path: CurveSequenceBuilder fills pattern via note-layer renderPreview") {
        CurveSequence curveSeq;
        curveSeq.clear();
        AcidBasslineGenerator::Params params;
        params.seed          = 42;
        params.density       = 100;
        params.patternLength = 16;
        // CurveSequenceBuilder is not a NoteSequenceBuilder → triggers fallback path
        CurveSequenceBuilder builder(curveSeq, CurveSequence::Layer::Gate);
        AcidBasslineGenerator gen(builder, params);

        // The fallback renders the Note layer into _pattern; at least step 0
        // (root note, always gated) must produce a non-zero encoded value.
        bool anyNonZero = false;
        for (int i = 0; i < 16; ++i) {
            if (gen.pattern()[i] != 0) {
                anyNonZero = true;
                break;
            }
        }
        expectTrue(anyNonZero);
    }
}

#endif // CONFIG_ACID_BASS_GENERATOR

