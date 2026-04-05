#include "UnitTest.h"

#include "apps/sequencer/engine/generators/Generator.h"

#include "apps/sequencer/model/NoteSequence.h"

namespace {

static NoteSequence makeSequenceWithEditedLayer(NoteSequence::Layer layer) {
    NoteSequence sequence;
    sequence.clear();
    sequence.step(0).setLayerValue(layer, NoteSequence::layerRange(layer).max);
    sequence.step(1).setLayerValue(layer, NoteSequence::layerRange(layer).min);
    return sequence;
}

} // namespace

UNIT_TEST("Generator") {

    CASE("InitLayer clears the selected layer and returns no generator") {
        auto sequence = makeSequenceWithEditedLayer(NoteSequence::Layer::Note);
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Note);

        Generator *generator = Generator::execute(Generator::Mode::InitLayer, builder);
        expectFalse(generator != nullptr);

        const int expected = NoteSequence::layerDefaultValue(NoteSequence::Layer::Note);
        expectEqual(sequence.step(0).layerValue(NoteSequence::Layer::Note), expected);
        expectEqual(sequence.step(1).layerValue(NoteSequence::Layer::Note), expected);
    }

    CASE("Euclidean mode returns an Euclidean generator instance") {
        NoteSequence sequence;
        sequence.clear();
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Gate);

        Generator *generator = Generator::execute(Generator::Mode::Euclidean, builder);
        expectTrue(generator != nullptr);
        expectEqual(generator->mode(), Generator::Mode::Euclidean);
    }

    CASE("Random mode returns a Random generator instance") {
        NoteSequence sequence;
        sequence.clear();
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Gate);

        Generator *generator = Generator::execute(Generator::Mode::Random, builder);
        expectTrue(generator != nullptr);
        expectEqual(generator->mode(), Generator::Mode::Random);
    }

#ifdef CONFIG_ACID_BASS_GENERATOR
    CASE("AcidBassline mode returns an AcidBassline generator instance") {
        NoteSequence sequence;
        sequence.clear();
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Gate);

        Generator *generator = Generator::execute(Generator::Mode::AcidBassline, builder);
        expectTrue(generator != nullptr);
        expectEqual(generator->mode(), Generator::Mode::AcidBassline);
    }
#endif

    CASE("Last mode falls back to nullptr") {
        NoteSequence sequence;
        sequence.clear();
        NoteSequenceBuilder builder(sequence, NoteSequence::Layer::Gate);

        Generator *generator = Generator::execute(Generator::Mode::Last, builder);
        expectFalse(generator != nullptr);
    }
}

