#define private public

#include "AcidBasslineGenerator.h"

#include "SequenceBuilder.h"

#include "core/utils/Random.h"

#undef private

#include <cmath>

static Random rng;

namespace {
static constexpr int kPreferredDegrees[] = {
    0,   // root
    7,   // fifth
    12,  // octave
    10,  // minor seventh
    3,   // minor third
    4    // major third
};

static constexpr int kPreferredDegreeWeights[] = {
    40,  // root
    22,  // fifth
    16,  // octave
    10,  // minor seventh
    7,   // minor third
    5    // major third
};

static constexpr int kPreferredDegreeCount =
    sizeof(kPreferredDegrees) / sizeof(kPreferredDegrees[0]);

static const char *kRootNames[] = TXT_NOTE_NAMES;
} // namespace

const AcidBasslineGenerator::TimingFeel AcidBasslineGenerator::kTightFeel = {
    {  0,  1,  -0,  0 },  // anchor
    {  0, 1,  0,  1 },  // even
    { 1,  0,  2,  0 },  // odd
    {  0,  0,  1,  0 }   // slide
};

/**
 * Constructor for AcidBasslineGenerator.
 * Initializes the generator with a sequence builder and parameters, then updates the pattern.
 * @param builder The sequence builder to use for generating the pattern.
 * @param params The parameters for the acid bassline generation.
 */
AcidBasslineGenerator::AcidBasslineGenerator(SequenceBuilder &builder, Params &params) :
    Generator(builder),
    _params(params)
{
    update();
}

/**
 * Returns the number of parameters for this generator.
 * @return The count of parameters.
 */
int AcidBasslineGenerator::paramCount() const {
    return int(Param::Last);
}

/**
 * Returns the name of the parameter at the given index.
 * @param index The parameter index.
 * @return The parameter name string, or nullptr if invalid.
 */
const char *AcidBasslineGenerator::paramName(int index) const {
    switch (Param(index)) {
    case Param::Seed:          return TXT_MENU_SEED;
    case Param::RootNote:      return TXT_MENU_ROOT;
    case Param::PatternLength: return TXT_MENU_LENGTH;
    case Param::Density:       return TXT_MENU_DENSITY;
    case Param::LegatoMix:     return TXT_MENU_LEGATO;
    case Param::Last:          break;
    }
    return nullptr;
}

/**
 * Edits a parameter at the given index by the specified value, with optional shift for coarse adjustments.
 * @param index The parameter index to edit.
 * @param value The value to add to the parameter.
 * @param shift If true, applies coarse adjustments (e.g., multiples of 4 or 25).
 */
void AcidBasslineGenerator::editParam(int index, int value, bool shift) {
    int step = shift ? 4 : 1;

    switch (Param(index)) {
    case Param::Seed:
        if (shift) {
            value = int(rng.next() & 0xffff);
            setSeed(value);
        } else {
            setSeed(seed() + value * step);
        }
        break;
    case Param::RootNote:
        setRootNote(rootNote() + value);
        break;
    case Param::PatternLength:
        if (shift) {
            int len = patternLength();

            if (value > 0) {
                // nächstes Vielfaches von 16
                len = ((len / 16) + 1) * 16;
            } else {
                // vorheriges Vielfaches von 16
                len = ((len - 1) / 16) * 16;
            }

            if (len < 16) {
                len = 16;
            }

            setPatternLength(len);
        } else {
            setPatternLength(patternLength() + value);
        }
        break;
    case Param::Density:
        if (shift) {
            int d = density();

            if (value > 0) {
                d = ((d / 25) + 1) * 25;
            } else {
                d = ((d - 1) / 25) * 25;
            }

            setDensity(d);
        } else {
            setDensity(density() + value * step);
        }
        break;
    case Param::LegatoMix:
        if (shift) {
            int l = legatoMix();

            if (value > 0) {
                l = ((l / 25) + 1) * 25;
            } else {
                l = ((l - 1) / 25) * 25;
            }

            setLegatoMix(l);
        } else {
            setLegatoMix(legatoMix() + value * step);
        }
        break;
    case Param::Last:
        break;
    }
}

/**
 * Prints the value of the parameter at the given index to a string builder.
 * @param index The parameter index.
 * @param str The string builder to append the formatted value to.
 */
void AcidBasslineGenerator::printParam(int index, StringBuilder &str) const {
    switch (Param(index)) {
    case Param::Seed:
        str("%d", seed());
        break;
    case Param::RootNote:
        str("%s", kRootNames[rootNote()]);
        break;
    case Param::PatternLength:
        str("%d", patternLength());
        break;
    case Param::Density:
        str("%d%%", density());
        break;
    case Param::LegatoMix:
        str("%d%%", legatoMix());
        break;
    case Param::Last:
        break;
    }
}

/**
 * Initializes the generator with default parameters and updates the pattern.
 */
void AcidBasslineGenerator::init() {
    _params = Params();
    update();
}

/**
 * Computes a 32-bit hash value for the given input.
 * @param x The input value to hash.
 * @return The hashed value.
 */
uint32_t AcidBasslineGenerator::hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

/**
 * Checks if the given step is an anchor step (every 4th step starting from 0).
 * @param step The step index to check.
 * @return True if the step is an anchor step, false otherwise.
 */
bool AcidBasslineGenerator::isAnchorStep(int step) const {
    int local = step & 0x0f;
    return local == 0 || local == 4 || local == 8 || local == 12;
}

/**
 * Determines if the previous note should be repeated based on the score and step.
 * @param score The score value to compare against the chance.
 * @param step The step index.
 * @return True if the previous note should be repeated, false otherwise.
 */
bool AcidBasslineGenerator::shouldRepeatPrevious(int score, int step) const {
    int chance = 30;
    if ((step & 1) == 1) {
        chance += 12;
    }
    return score < chance;
}

/**
 * Determines if an approach note should be used based on the score and step.
 * @param score The score value to compare against the chance.
 * @param step The step index.
 * @return True if an approach note should be used, false otherwise.
 */
bool AcidBasslineGenerator::shouldUseApproach(int score, int step) const {
    int chance = 10;
    if ((step & 1) == 1) {
        chance += 8;
    }
    return score < chance;
}

/**
 * Chooses a base degree from the preferred degrees based on the pick value.
 * @param pick The pick value used to select the degree.
 * @return The selected degree.
 */
int AcidBasslineGenerator::chooseBaseDegree(int pick) const {
    int totalWeight = 0;
    for (int i = 0; i < kPreferredDegreeCount; ++i) {
        totalWeight += kPreferredDegreeWeights[i];
    }

    int value = pick % totalWeight;

    for (int i = 0; i < kPreferredDegreeCount; ++i) {
        if (value < kPreferredDegreeWeights[i]) {
            return kPreferredDegrees[i];
        }
        value -= kPreferredDegreeWeights[i];
    }

    return 0;
}

/**
 * Chooses an approach degree relative to the target degree based on the pick value.
 * @param pick The pick value used to select the approach.
 * @param targetDegree The target degree to approach.
 * @return The approach degree.
 */
int AcidBasslineGenerator::chooseApproachDegree(int pick, int targetDegree) const {
    switch (pick % 4) {
    case 0:  return targetDegree - 1;
    case 1:  return targetDegree + 1;
    case 2:  return targetDegree - 2;
    default: return targetDegree + 2;
    }
}

/**
 * Chooses an octave offset based on the pick value.
 * @param pick The pick value used to select the octave offset.
 * @return The octave offset in semitones.
 */
int AcidBasslineGenerator::chooseOctaveOffset(int pick) const {
    if (pick < 74) {
        return 0;
    }
    if (pick < 84) {
        return 12;
    }
    if (pick < 94) {
        return 24;
    }
    return -12;
}

/**
 * Returns the density threshold value.
 * @return The density threshold.
 */
int AcidBasslineGenerator::densityThreshold() const {
    return int(_params.density);
}

/**
 * Returns the legato threshold value.
 * @return The legato threshold.
 */
int AcidBasslineGenerator::legatoThreshold() const {
    return int(_params.legatoMix);
}

/**
 * Clears the state of the blueprint and steps arrays.
 */
void AcidBasslineGenerator::clearState() {
    for (auto &step : _blueprint) {
        step = StepBlueprint();
    }
    for (auto &step : _steps) {
        step = StepState();
    }
}

void AcidBasslineGenerator::buildBlueprint() {
    /*
     * Seed-only acid blueprint.
     *
     * This creates the raw phrase identity:
     * - weighted pitch vocabulary
     * - motif repetition
     * - approach notes
     * - dense underlying gate tendency
     *
     * No root transposition, no density masking, no cropping here.
     */
    int previousDegree = 0;
    int cellSize;

    switch (hash32(_params.seed ^ 0x12345678u) % 3) {
    case 0:  cellSize = 2; break;
    case 1:  cellSize = 4; break;
    default: cellSize = 8; break;
    }

    for (int i = 0; i < CONFIG_STEP_COUNT; ++i) {
        StepBlueprint &step = _blueprint[i];

        uint32_t base = hash32(uint32_t(_params.seed) ^ uint32_t(i * 0x9e3779b9u));

        step.gateScore = int(hash32(base ^ 0x100u) % 100);
        step.clusterScore = int(hash32(base ^ 0x200u) % 100);
        step.repeatScore = int(hash32(base ^ 0x300u) % 100);
        step.approachScore = int(hash32(base ^ 0x400u) % 100);
        step.legatoScore = int(hash32(base ^ 0x500u) % 100);
        step.degreePick = int(hash32(base ^ 0x600u) % 1000);
        step.octavePick = int(hash32(base ^ 0x700u) % 100);
        step.motifPick = int(hash32(base ^ 0x800u) % 100);

        int phraseGateThreshold = 82;
        if (isAnchorStep(i)) {
            phraseGateThreshold += 10;
        }
        if ((i & 1) == 1) {
            phraseGateThreshold -= 8;
        }

        step.baseGate = step.gateScore < phraseGateThreshold;

        /*
         * Allow local grouping so the rhythm feels phrase-like, not fully independent.
         */
        if (i > 0 && step.clusterScore > 72) {
            step.baseGate = _blueprint[i - 1].baseGate;
        }

        int degree = 0;
        bool approach = false;

        bool cellRepeat =
            (i >= cellSize) &&
            _blueprint[i - cellSize].baseGate &&
            (step.motifPick < 28);

        if (cellRepeat) {
            degree = _blueprint[i - cellSize].baseDegree;
        } else if (i > 0 && _blueprint[i - 1].baseGate && shouldRepeatPrevious(step.repeatScore, i)) {
            degree = previousDegree;
        } else if (shouldUseApproach(step.approachScore, i)) {
            int targetDegree = chooseBaseDegree(step.degreePick);
            degree = chooseApproachDegree(step.motifPick, targetDegree);
            approach = true;
        } else {
            degree = chooseBaseDegree(step.degreePick);
        }

        step.baseDegree = degree;
        step.baseOctave = chooseOctaveOffset(step.octavePick);
        step.baseApproach = approach;
        step.baseLegato = step.legatoScore < legatoThreshold();

        previousDegree = degree;
    }

    _blueprint[0].baseGate = true;
}

void AcidBasslineGenerator::realizePhrase(int length) {
    /*
     * Apply root, density, length and legato immediately.
     *
     * Density semantics:
     * - 100% means every 1/16 gate is on
     * - lower values create true no-gate clusters
     * - rests exist only via Gate=false
     */
    for (int i = 0; i < length; ++i) {
        const StepBlueprint &src = _blueprint[i];
        StepState &dst = _steps[i];

        dst.rawNote = clamp(src.baseDegree + src.baseOctave, NoteSequence::Note::Min, NoteSequence::Note::Max);
        dst.note = clamp(dst.rawNote + _params.rootNote,
                         NoteSequence::Note::Min,
                         NoteSequence::Note::Max);

        dst.approach = src.baseApproach;
        dst.legato = src.baseLegato;

        if (_params.density >= 100) {
            dst.gate = true;
        } else {
            int keepThreshold = densityThreshold();

            if (isAnchorStep(i)) {
                keepThreshold += 14;
            }
            if ((i & 1) == 1) {
                keepThreshold -= 4;
            }

            if (!src.baseGate) {
                dst.gate = false;
            } else {
                int densityScore = int((hash32(uint32_t(_params.seed) ^ uint32_t(0xABC000u + i * 313u)) % 100));
                dst.gate = densityScore < keepThreshold;
            }
        }
    }

    /*
     * Real rest clusters at lower densities.
     */
    if (_params.density < 80) {
        for (int i = 1; i < length; ++i) {
            int clusterScore = int(hash32(uint32_t(_params.seed) ^ uint32_t(0xCC0000u + i * 131u)) % 100);

            if (clusterScore > _params.density + 10) {
                _steps[i].gate = false;

                if (_params.density < 55 &&
                    i + 1 < length &&
                    clusterScore > 70 &&
                    !isAnchorStep(i + 1)) {
                    _steps[i + 1].gate = false;
                }

                if (_params.density < 30 &&
                    i + 2 < length &&
                    clusterScore > 88 &&
                    !isAnchorStep(i + 2)) {
                    _steps[i + 2].gate = false;
                }
            }
        }
    }

    _steps[0].gate = true;

    for (int i = 0; i < length; ++i) {
        StepState &step = _steps[i];

        if (!step.gate) {
            step.legato = false;
            step.slide = false;
            step.length = 0;
            step.gateProbability = 0;
            step.gateOffset = 0;
            continue;
        }

        bool nextGate = (i + 1 < length) && _steps[i + 1].gate;

        /*
         * Slides only when musically plausible.
         */
        step.slide =
            step.legato &&
            nextGate &&
            !isAnchorStep(i) &&
            (_steps[i + 1].note != step.note);

        if (step.slide) {
            step.length = 6;
        } else if (step.legato) {
            step.length = 4;
        } else {
            step.length = 2;
        }

        if (!nextGate) {
            step.slide = false;
            step.length = 2;
        }

        /*
         * Conservative probability:
         * anchors solid, approaches softer, offbeats slightly softer.
         */
        if (isAnchorStep(i)) {
            step.gateProbability = 7;
        } else if (step.approach) {
            step.gateProbability = 5;
        } else if ((i & 1) == 1) {
            step.gateProbability = 6;
        } else {
            step.gateProbability = 7;
        }

        step.gateOffset = computeGateOffset(i);
    }

    /*
     * Crop the tail explicitly.
     */
    for (int i = length; i < CONFIG_STEP_COUNT; ++i) {
        _steps[i].gate = false;
        _steps[i].legato = false;
        _steps[i].slide = false;
        _steps[i].length = 0;
        _steps[i].gateProbability = 0;
        _steps[i].gateOffset = 0;
        _steps[i].note = 0;
    }
}

/**
 * Applies the generated steps to the note sequence and renders the preview for the current layer.
 * @param sequence The note sequence to apply the steps to.
 * @param currentLayer The current layer being edited.
 * @param length The length of the pattern to apply.
 */
void AcidBasslineGenerator::applyToNoteSequence(NoteSequence &sequence, NoteSequence::Layer currentLayer, int length) {
    sequence.clearSteps();
    sequence.setFirstStep(0);
    sequence.setLastStep(length - 1);

    for (int i = 0; i < CONFIG_STEP_COUNT; ++i) {
        auto &step = sequence.step(i);

        step.setGate(_steps[i].gate);
        step.setGateProbability(_steps[i].gateProbability);
        step.setGateOffset(_steps[i].gateOffset);
        step.setLength(_steps[i].length);
        step.setNote(_steps[i].note);
        step.setSlide(_steps[i].slide);
    }

    renderPreview(currentLayer, length);
}

/**
 * Renders the preview pattern for the specified layer and length.
 * @param currentLayer The layer to render the preview for.
 * @param length The length of the pattern to render.
 */
void AcidBasslineGenerator::renderPreview(NoteSequence::Layer currentLayer, int length) {
    for (int i = 0; i < CONFIG_STEP_COUNT; ++i) {
        if (i < length) {
            _pattern[i] = renderStepValue(currentLayer, i,  _steps[i]);
        } else {
            _pattern[i] = 0;
        }
    }
}

/**
 * Encodes a value into a uint8_t by scaling it from the given min and max range to 0-255.
 * @param value The value to encode.
 * @param minValue The minimum value of the range.
 * @param maxValue The maximum value of the range.
 * @return The encoded uint8_t value.
 */
uint8_t AcidBasslineGenerator::encodeLayerValue(int value, int minValue, int maxValue) {
    if (maxValue <= minValue) {
        return 0;
    }

    value = clamp(value, minValue, maxValue);
    int encoded = std::round((value - minValue) * 255.f / float(maxValue - minValue));
    return uint8_t(clamp(encoded, 0, 255));
}

/**
 * Computes the gate offset for the given step based on timing feel and other factors.
 * @param step The step index to compute the offset for.
 * @return The computed gate offset.
 */
int AcidBasslineGenerator::computeGateOffset(int step) const {
    const StepState &s = _steps[step];

    if (!s.gate) {
        return 0;
    }

    const TimingFeel &feel = kTightFeel;
    int variant = int(hash32(uint32_t(_params.seed) ^ uint32_t(0xD00D0000u + step * 97u)) & 0x3);

    int offset = 0;
    int local = step & 0x0f;

    if (s.slide) {
        offset = feel.slide[variant];
    } else if (local == 0 || local == 4 || local == 8 || local == 12) {
        offset = feel.anchor[variant];
    } else if ((step & 1) == 0) {
        offset = feel.even[variant];
    } else {
        offset = feel.odd[variant];
    }

    if (_params.density >= 80) {
        offset /= 2;
    }

    if (s.length <= 2) {
        if (offset > 0) --offset;
        else if (offset < 0) ++offset;
    }

    return clamp(offset,
                 NoteSequence::GateOffset::Min,
                 NoteSequence::GateOffset::Max);
}

/**
 * Snaps the given note to the nearest note in the specified scale.
 * @param note The note value to snap.
 * @param scale The scale to snap to.
 * @return The snapped note value.
 */
int AcidBasslineGenerator::snapNoteToScale(int note, const Scale &scale) const {
    /*
     * Keep the note in the same integer domain used by NoteSequence::Step::note().
     * We snap by converting to volts and back through the selected scale.
     */
    float volts = scale.noteToVolts(note);
    return scale.noteFromVolts(volts);
}

/**
 * Applies the scale and root note to the generated steps in the sequence.
 * @param sequence The note sequence to apply the scale to.
 * @param length The length of the pattern to apply the scale to.
 */
void AcidBasslineGenerator::applyScale(NoteSequence &sequence, int length) {
    /*
     * Within the current generator context we do not have direct project access.
     * Therefore:
     * - use sequence-local scale/root if explicitly set
     * - otherwise fall back to semitones and generator root parameter
     */
    const Scale &scale =
        sequence.scale() >= 0
            ? sequence.selectedScale(sequence.scale())
            : Scale::get(0);   // Semitones

    int effectiveRoot =
        sequence.rootNote() >= 0
            ? sequence.selectedRootNote(sequence.rootNote())
            : _params.rootNote;

    for (int i = 0; i < length; ++i) {
        StepState &step = _steps[i];

        if (!step.gate) {
            continue;
        }

        int transposed = step.rawNote + effectiveRoot;
        step.note = clamp(
            snapNoteToScale(transposed, scale),
            NoteSequence::Note::Min,
            NoteSequence::Note::Max
        );
    }
}

/**
 * Renders the value for a specific layer at the given step index and step state.
 * @param layer The layer to render the value for.
 * @param stepIndex The index of the step.
 * @param step The step state.
 * @return The rendered uint8_t value for the layer.
 */
uint8_t AcidBasslineGenerator::renderStepValue(NoteSequence::Layer layer, int stepIndex, const StepState &step) const {
    switch (layer) {
    case NoteSequence::Layer::Note:
        return encodeLayerValue(step.note,
                                NoteSequence::Note::Min,
                                NoteSequence::Note::Max);

    case NoteSequence::Layer::Gate:
        return encodeLayerValue(step.gate ? 1 : 0, 0, 1);

    case NoteSequence::Layer::GateProbability:
        return encodeLayerValue(step.gateProbability,
                                NoteSequence::GateProbability::Min,
                                NoteSequence::GateProbability::Max);

    case NoteSequence::Layer::Length:
        return encodeLayerValue(step.length,
                                NoteSequence::Length::Min,
                                NoteSequence::Length::Max);

    case NoteSequence::Layer::Slide:
        return encodeLayerValue(step.slide ? 1 : 0, 0, 1);

    case NoteSequence::Layer::GateOffset:
        return encodeLayerValue(
            computeGateOffset(stepIndex),
            NoteSequence::GateOffset::Min,
            NoteSequence::GateOffset::Max
        );

    default:
        return 0;
    }
}

void AcidBasslineGenerator::update() {
    clearState();
    buildBlueprint();

    int length = clamp<int>(_params.patternLength, 1, CONFIG_STEP_COUNT);
    realizePhrase(length);

    /*
     * If the current builder really is a NoteSequenceBuilder, we write the
     * whole phrase to all relevant note layers in one integrated pass.
     * That is the critical fix.
     */
    if (auto *noteBuilder = dynamic_cast<NoteSequenceBuilder *>(&_builder)) {
        applyScale(noteBuilder->_edit, length);
        applyToNoteSequence(noteBuilder->_edit, noteBuilder->_layer, length);
        return;
    }
    /*
     * Fallback: render only the note layer preview.
     * This path is mainly for safety; the intended use is NoteSequenceBuilder.
     */
    renderPreview(NoteSequence::Layer::Note, length);

    for (size_t i = 0; i < _pattern.size(); ++i) {
        _builder.setValue(i, _pattern[i] * (1.f / 255.f));
    }
}

