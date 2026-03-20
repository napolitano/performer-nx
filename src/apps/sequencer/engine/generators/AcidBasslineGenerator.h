#pragma once

#include "Generator.h"
#include "model/NoteSequence.h"

#include "core/math/Math.h"

#include <array>
#include <cstdint>

class AcidBasslineGenerator : public Generator {
public:
    enum class Param {
        Seed,
        RootNote,
        PatternLength,
        Density,
        LegatoMix,
        Last
    };

    struct Params {
        uint16_t seed = 0;
        uint8_t rootNote = 0;       // 0=C ... 11=B
        uint8_t patternLength = 16; // 1..CONFIG_STEP_COUNT
        uint8_t density = 62;       // 0..100
        uint8_t legatoMix = 35;     // 0..100
    };

    struct TimingFeel {
        int anchor[4];
        int even[4];
        int odd[4];
        int slide[4];
    };

    static const TimingFeel kTightFeel;

    AcidBasslineGenerator(SequenceBuilder &builder, Params &params);

    Mode mode() const override { return Mode::AcidBassline; }

    int paramCount() const override;
    const char *paramName(int index) const override;
    void editParam(int index, int value, bool shift) override;
    void printParam(int index, StringBuilder &str) const override;

    void init() override;
    void update() override;

    int seed() const { return _params.seed; }
    void setSeed(int seed) { _params.seed = clamp(seed, 0, 65535); }

    int rootNote() const { return _params.rootNote; }
    void setRootNote(int rootNote) { _params.rootNote = clamp(rootNote, 0, 11); }

    int patternLength() const { return _params.patternLength; }
    void setPatternLength(int patternLength) { _params.patternLength = clamp(patternLength, 1, CONFIG_STEP_COUNT); }

    int density() const { return _params.density; }
    void setDensity(int density) { _params.density = clamp(density, 0, 100); }

    int legatoMix() const { return _params.legatoMix; }
    void setLegatoMix(int legatoMix) { _params.legatoMix = clamp(legatoMix, 0, 100); }

    const GeneratorPattern &pattern() const { return _pattern; }

private:
    struct StepBlueprint {
        int gateScore = 0;       // 0..99
        int clusterScore = 0;    // 0..99
        int repeatScore = 0;     // 0..99
        int approachScore = 0;   // 0..99
        int legatoScore = 0;     // 0..99
        int degreePick = 0;      // arbitrary bucket
        int octavePick = 0;      // 0..99
        int motifPick = 0;       // 0..99

        bool baseGate = false;
        bool baseLegato = false;
        bool baseApproach = false;
        int baseDegree = 0;
        int baseOctave = 0;
    };

    struct StepState {
        bool gate = false;
        bool legato = false;
        bool slide = false;
        bool approach = false;

        int rawNote = 0;         // note before scale snap
        int note = 0;            // final signed note in NoteSequence domain
        int length = 0;          // NoteSequence::Length range
        int gateProbability = 0; // NoteSequence::GateProbability range
        int gateOffset = 0;
    };

    Params &_params;
    GeneratorPattern _pattern;
    std::array<StepBlueprint, CONFIG_STEP_COUNT> _blueprint;
    std::array<StepState, CONFIG_STEP_COUNT> _steps;

    void clearState();
    void buildBlueprint();
    void realizePhrase(int length);

    void applyToNoteSequence(NoteSequence &sequence, NoteSequence::Layer currentLayer, int length);
    void renderPreview(NoteSequence::Layer currentLayer, int length);

    bool isAnchorStep(int step) const;
    bool shouldRepeatPrevious(int score, int step) const;
    bool shouldUseApproach(int score, int step) const;

    int chooseBaseDegree(int pick) const;
    int chooseApproachDegree(int pick, int targetDegree) const;
    int chooseOctaveOffset(int pick) const;

    int densityThreshold() const;
    int legatoThreshold() const;

    int computeGateOffset(int step) const;

    int snapNoteToScale(int note, const Scale &scale) const;
    void applyScale(NoteSequence &sequence, int length);

    uint8_t renderStepValue(NoteSequence::Layer layer, int stepIndex, const StepState &step) const;
    static uint8_t encodeLayerValue(int value, int minValue, int maxValue);

    static uint32_t hash32(uint32_t x);
};