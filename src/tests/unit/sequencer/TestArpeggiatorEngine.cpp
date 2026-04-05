#include "UnitTest.h"

#include "apps/sequencer/engine/ArpeggiatorEngine.h"

#include <initializer_list>
#include <memory>
#include <vector>

namespace {

struct Harness {
    Arpeggiator arpeggiator;
    std::unique_ptr<ArpeggiatorEngine> engine;

    Harness() {
        arpeggiator.clear();
        arpeggiator.setDivisor(1);
        engine.reset(new ArpeggiatorEngine(arpeggiator));
    }
};

static std::vector<ArpeggiatorEngine::Event> collectEvents(ArpeggiatorEngine &engine, uint32_t lastTick, int swing = 50) {
    std::vector<ArpeggiatorEngine::Event> events;
    for (uint32_t tick = 0; tick <= lastTick; ++tick) {
        engine.tick(tick, swing);
        ArpeggiatorEngine::Event event;
        while (engine.getEvent(tick, event)) {
            events.push_back(event);
        }
    }
    return events;
}

static std::vector<int> collectNoteOns(ArpeggiatorEngine &engine, uint32_t lastTick, int swing = 50) {
    std::vector<int> notes;
    for (const auto &event : collectEvents(engine, lastTick, swing)) {
        if (event.action == ArpeggiatorEngine::Event::NoteOn) {
            notes.push_back(event.note);
        }
    }
    return notes;
}

static void expectNotes(const std::vector<int> &actual, std::initializer_list<int> expected) {
    expectEqual(int(actual.size()), int(expected.size()));
    int index = 0;
    for (int note : expected) {
        expectEqual(actual[index], note);
        ++index;
    }
}

static void expectAllNotesInSet(const std::vector<int> &actual, std::initializer_list<int> expected) {
    for (int note : actual) {
        bool found = false;
        for (int expectedNote : expected) {
            if (note == expectedNote) {
                found = true;
                break;
            }
        }
        expectTrue(found);
    }
}

} // namespace

UNIT_TEST("ArpeggiatorEngine") {

    CASE("empty tick emits no events and note off without hold removes notes") {
        Harness harness;
        ArpeggiatorEngine::Event event;

        harness.engine->tick(0, 50);
        expectFalse(harness.engine->getEvent(0, event));

        harness.engine->noteOn(60);
        harness.engine->noteOff(60);
        harness.engine->tick(0, 50);
        expectFalse(harness.engine->getEvent(0, event));
    }

    CASE("play order keeps insertion order, ignores duplicates, caps note count and delays legato note off by one tick") {
        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::PlayOrder);
            harness.arpeggiator.setGateLength(100);

            harness.engine->noteOn(64);
            harness.engine->noteOn(60);
            harness.engine->noteOn(67);
            harness.engine->noteOn(64);
            harness.engine->noteOn(72);
            harness.engine->noteOn(74);
            harness.engine->noteOn(76);
            harness.engine->noteOn(77);
            harness.engine->noteOn(79);
            harness.engine->noteOn(81);

            ArpeggiatorEngine::Event event;
            harness.engine->tick(0, 50);
            expectTrue(harness.engine->getEvent(0, event));
            expectEqual(int(event.action), int(ArpeggiatorEngine::Event::NoteOn));
            expectEqual(int(event.note), 64);
            expectFalse(harness.engine->getEvent(0, event));
        }

        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::PlayOrder);
            harness.arpeggiator.setGateLength(100);

            harness.engine->noteOn(64);
            harness.engine->noteOn(60);
            harness.engine->noteOn(67);
            harness.engine->noteOn(64);
            harness.engine->noteOn(72);
            harness.engine->noteOn(74);
            harness.engine->noteOn(76);
            harness.engine->noteOn(77);
            harness.engine->noteOn(79);
            harness.engine->noteOn(81);

            const auto events = collectEvents(*harness.engine, 31);

            std::vector<int> notes;
            int firstNoteOffTick = -1;
            for (const auto &current : events) {
                if (current.action == ArpeggiatorEngine::Event::NoteOn) {
                    notes.push_back(current.note);
                } else if (firstNoteOffTick == -1) {
                    firstNoteOffTick = int(current.tick);
                }
            }

            expectNotes(notes, { 64, 60, 67, 72, 74, 76, 77, 79 });
            expectEqual(firstNoteOffTick, 5);
        }
    }

    CASE("hold mode keeps released notes until all holds are gone and a new note resets stale state") {
        {
            Harness harness;
            harness.arpeggiator.setHold(true);
            harness.arpeggiator.setMode(Arpeggiator::Mode::Up);

            harness.engine->noteOn(60);
            harness.engine->noteOn(64);
            harness.engine->noteOff(60);

            const auto notes = collectNoteOns(*harness.engine, 4);
            expectNotes(notes, { 60, 64 });
        }

        {
            Harness harness;
            harness.arpeggiator.setHold(true);
            harness.arpeggiator.setMode(Arpeggiator::Mode::Up);

            harness.engine->noteOn(60);
            harness.engine->noteOff(60);
            harness.engine->noteOn(67);

            const auto notes = collectNoteOns(*harness.engine, 0);
            expectNotes(notes, { 67 });
        }
    }

    CASE("note off handles missing notes, repeated hold releases, and middle-note removal") {
        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::Up);

            harness.engine->noteOn(60);
            harness.engine->noteOn(64);
            harness.engine->noteOff(61);

            const auto notes = collectNoteOns(*harness.engine, 8);
            expectNotes(notes, { 60, 64, 60 });
        }

        {
            Harness harness;
            harness.arpeggiator.setHold(true);
            harness.arpeggiator.setMode(Arpeggiator::Mode::Up);

            harness.engine->noteOn(60);
            harness.engine->noteOff(60);
            harness.engine->noteOff(60);

            const auto notes = collectNoteOns(*harness.engine, 8);
            expectNotes(notes, { 60, 60, 60 });
        }

        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::Up);

            harness.engine->noteOn(60);
            harness.engine->noteOn(64);
            harness.engine->noteOn(67);
            harness.engine->noteOff(64);

            const auto notes = collectNoteOns(*harness.engine, 8);
            expectNotes(notes, { 60, 67, 60 });
        }
    }

    CASE("directional and center-based modes emit the expected note order") {
        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::Up);
            harness.engine->noteOn(60);
            harness.engine->noteOn(64);
            harness.engine->noteOn(67);
            expectNotes(collectNoteOns(*harness.engine, 12), { 60, 64, 67, 60 });
        }
        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::Down);
            harness.engine->noteOn(60);
            harness.engine->noteOn(64);
            harness.engine->noteOn(67);
            expectNotes(collectNoteOns(*harness.engine, 12), { 67, 64, 60, 67 });
        }
        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::UpDown);
            harness.engine->noteOn(60);
            harness.engine->noteOn(64);
            harness.engine->noteOn(67);
            expectNotes(collectNoteOns(*harness.engine, 12), { 60, 64, 67, 64 });
        }
        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::DownUp);
            harness.engine->noteOn(60);
            harness.engine->noteOn(64);
            harness.engine->noteOn(67);
            expectNotes(collectNoteOns(*harness.engine, 12), { 67, 64, 60, 64 });
        }
        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::UpDown);
            harness.engine->noteOn(60);
            expectNotes(collectNoteOns(*harness.engine, 8), { 60, 60, 60 });
        }
        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::UpAndDown);
            harness.engine->noteOn(60);
            harness.engine->noteOn(64);
            harness.engine->noteOn(67);
            expectNotes(collectNoteOns(*harness.engine, 20), { 60, 64, 67, 67, 64, 60 });
        }
        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::DownAndUp);
            harness.engine->noteOn(60);
            harness.engine->noteOn(64);
            harness.engine->noteOn(67);
            expectNotes(collectNoteOns(*harness.engine, 20), { 67, 64, 60, 60, 64, 67 });
        }
        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::Converge);
            harness.engine->noteOn(60);
            harness.engine->noteOn(64);
            harness.engine->noteOn(67);
            expectNotes(collectNoteOns(*harness.engine, 8), { 60, 67, 64 });
        }
        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::Diverge);
            harness.engine->noteOn(60);
            harness.engine->noteOn(64);
            harness.engine->noteOn(67);
            expectNotes(collectNoteOns(*harness.engine, 8), { 64, 60, 67 });
        }
        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::Random);
            harness.engine->noteOn(60);
            harness.engine->noteOn(64);
            harness.engine->noteOn(67);
            const auto notes = collectNoteOns(*harness.engine, 16);
            expectEqual(int(notes.size()), 5);
            expectAllNotesInSet(notes, { 60, 64, 67 });
        }
    }

    CASE("octave handling covers zero positive negative and both-direction ranges") {
        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::Up);
            harness.arpeggiator.setOctaves(0);
            harness.engine->noteOn(60);
            expectNotes(collectNoteOns(*harness.engine, 8), { 60, 60, 60 });
        }
        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::Up);
            harness.arpeggiator.setOctaves(1);
            harness.engine->noteOn(60);
            expectNotes(collectNoteOns(*harness.engine, 8), { 60, 72, 60 });
        }
        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::Up);
            harness.arpeggiator.setOctaves(-1);
            harness.engine->noteOn(60);
            expectNotes(collectNoteOns(*harness.engine, 8), { 60, 48, 60 });
        }
        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::Up);
            harness.arpeggiator.setOctaves(7);
            harness.engine->noteOn(60);
            expectNotes(collectNoteOns(*harness.engine, 28), { 60, 72, 84, 84, 72, 60, 60, 72 });
        }
        {
            Harness harness;
            harness.arpeggiator.setMode(Arpeggiator::Mode::Up);
            harness.arpeggiator.setOctaves(-7);
            harness.engine->noteOn(60);
            expectNotes(collectNoteOns(*harness.engine, 28), { 60, 48, 36, 36, 48, 60, 60, 48 });
        }
    }
}




