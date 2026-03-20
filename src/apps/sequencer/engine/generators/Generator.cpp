#include "Generator.h"

#include "EuclideanGenerator.h"
#include "RandomGenerator.h"
#ifdef CONFIG_ACID_BASS_GENERATOR
#include "AcidBasslineGenerator.h"
#endif

#include "core/utils/Container.h"
#ifdef CONFIG_ACID_BASS_GENERATOR
static Container<EuclideanGenerator, RandomGenerator, AcidBasslineGenerator> generatorContainer;
#else
static Container<EuclideanGenerator, RandomGenerator> generatorContainer;
#endif
static EuclideanGenerator::Params euclideanParams;
static RandomGenerator::Params randomParams;
#ifdef CONFIG_ACID_BASS_GENERATOR
static AcidBasslineGenerator::Params acidBasslineParams;
#endif
static void initLayer(SequenceBuilder &builder) {
    builder.clearLayer();
}

Generator *Generator::execute(Generator::Mode mode, SequenceBuilder &builder) {
    switch (mode) {
    case Mode::InitLayer:
        initLayer(builder);
        return nullptr;
    case Mode::Euclidean:
        return generatorContainer.create<EuclideanGenerator>(builder, euclideanParams);
    case Mode::Random:
        return generatorContainer.create<RandomGenerator>(builder, randomParams);
#ifdef CONFIG_ACID_BASS_GENERATOR
    case Mode::AcidBassline:
        return generatorContainer.create<AcidBasslineGenerator>(builder, acidBasslineParams);
#endif
    case Mode::Last:
        break;
    }

    return nullptr;
}
