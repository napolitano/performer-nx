#include "UnitTest.h"

#include "apps/sequencer/engine/generators/Rhythm.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

static int countBeats(const Rhythm::Pattern &p) {
    int n = 0;
    for (size_t i = 0; i < p.size(); ++i) {
        if (p[i]) ++n;
    }
    return n;
}

static bool patternMatches(const Rhythm::Pattern &p, const bool *expected, int len) {
    if (int(p.size()) != len) {
        return false;
    }
    for (int i = 0; i < len; ++i) {
        if (p[i] != expected[i]) {
            return false;
        }
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
UNIT_TEST("Rhythm") {

    // ---- specific-pattern correctness -----------------------------------------------

    CASE("euclidean(3,8) returns classic tresillo [1,0,0,1,0,0,1,0]") {
        // Path: loop starts with xCount(3) < yCount(5), then xCount(3) >= yCount(2);
        // exits because yCount reaches 1.
        const bool expected[] = {1,0,0,1,0,0,1,0};
        auto p = Rhythm::euclidean(3, 8);
        expectEqual(int(p.size()), 8);
        expectEqual(countBeats(p), 3);
        expectTrue(patternMatches(p, expected, 8));
    }

    CASE("euclidean(5,8) returns [1,0,1,1,0,1,1,0]") {
        // Path: loop starts with xCount(5) >= yCount(3);
        // exercises the if-branch in the first iteration.
        const bool expected[] = {1,0,1,1,0,1,1,0};
        auto p = Rhythm::euclidean(5, 8);
        expectEqual(int(p.size()), 8);
        expectEqual(countBeats(p), 5);
        expectTrue(patternMatches(p, expected, 8));
    }

    CASE("euclidean(4,16) returns four evenly-spaced beats") {
        // Path: xCount(4) < yCount(12) for two iterations, then xCount >= yCount;
        // covers the else-branch being taken multiple times before the if-branch.
        const bool expected[] = {1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0};
        auto p = Rhythm::euclidean(4, 16);
        expectEqual(int(p.size()), 16);
        expectEqual(countBeats(p), 4);
        expectTrue(patternMatches(p, expected, 16));
    }

    CASE("euclidean(7,8) returns [1,0,1,1,1,1,1,1]") {
        // Path: xCount(7) >= yCount(1) → loop exits immediately because
        // xCount becomes 1 (not > 1) after a single iteration.
        const bool expected[] = {1,0,1,1,1,1,1,1};
        auto p = Rhythm::euclidean(7, 8);
        expectEqual(int(p.size()), 8);
        expectEqual(countBeats(p), 7);
        expectTrue(patternMatches(p, expected, 8));
    }

    CASE("euclidean(1,8) returns [1,0,0,0,0,0,0,0]") {
        // Path: xCount(1) < yCount(7) → loop exits immediately because
        // xCount is 1 (not > 1) after a single iteration.
        const bool expected[] = {1,0,0,0,0,0,0,0};
        auto p = Rhythm::euclidean(1, 8);
        expectEqual(int(p.size()), 8);
        expectEqual(countBeats(p), 1);
        expectTrue(patternMatches(p, expected, 8));
    }

    CASE("euclidean(5,13) multi-step mixed path") {
        // Path: else-branch (5<8), if-branch (5>3), if-branch (3>2);
        // exercises both branches across multiple iterations.
        const bool expected[] = {1,0,0,1,0,1,0,0,1,0,1,0,0};
        auto p = Rhythm::euclidean(5, 13);
        expectEqual(int(p.size()), 13);
        expectEqual(countBeats(p), 5);
        expectTrue(patternMatches(p, expected, 13));
    }

    // ---- boundary / edge cases -------------------------------------------------------

    CASE("euclidean(0,8) returns all-silent pattern of size 8") {
        // Path: beats clamped to 0; xCount=0 forces loop to exit immediately
        // (0 > 1 is false) after one body execution.
        auto p = Rhythm::euclidean(0, 8);
        expectEqual(int(p.size()), 8);
        expectEqual(countBeats(p), 0);
        for (int i = 0; i < 8; ++i) {
            expectFalse(p[i]);
        }
    }

    CASE("euclidean(8,8) returns all-active pattern of size 8") {
        // Path: yCount=0 from the start; xCount becomes 0 after one body
        // execution (yCountNew = xCount - yCount = 8 - 0 = 8, xCount = 0).
        auto p = Rhythm::euclidean(8, 8);
        expectEqual(int(p.size()), 8);
        expectEqual(countBeats(p), 8);
        for (int i = 0; i < 8; ++i) {
            expectTrue(p[i]);
        }
    }

    CASE("beats > steps is clamped to steps (euclidean(12,8) == euclidean(8,8))") {
        // Exercises the `beats = std::min(beats, steps)` guard.
        auto p = Rhythm::euclidean(12, 8);
        expectEqual(int(p.size()), 8);
        expectEqual(countBeats(p), 8);
        for (int i = 0; i < 8; ++i) {
            expectTrue(p[i]);
        }
    }

    CASE("euclidean(1,1) single-step pattern has exactly one beat") {
        auto p = Rhythm::euclidean(1, 1);
        expectEqual(int(p.size()), 1);
        expectEqual(countBeats(p), 1);
        expectTrue(p[0]);
    }

    // ---- structural properties -------------------------------------------------------

    CASE("pattern.size() equals steps for a range of inputs") {
        struct Row { int beats; int steps; };
        const Row table[] = {
            {1,  1},
            {3,  8},
            {5, 13},
            {7, 16},
            {4,  4},
            {0, 12},
            {12,12},
        };
        for (const auto &r : table) {
            auto p = Rhythm::euclidean(r.beats, r.steps);
            expectEqual(int(p.size()), r.steps);
        }
    }

    CASE("beat count equals min(beats,steps) for a range of inputs") {
        struct Row { int beats; int steps; int expectedBeats; };
        const Row table[] = {
            {3,  8,  3},
            {5,  8,  5},
            {4, 16,  4},
            {0,  8,  0},
            {8,  8,  8},
            {12, 8,  8},   // clamped
            {1,  8,  1},
            {7,  8,  7},
        };
        for (const auto &r : table) {
            auto p = Rhythm::euclidean(r.beats, r.steps);
            expectEqual(countBeats(p), r.expectedBeats);
        }
    }
}

