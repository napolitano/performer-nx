#include "Config.h"
#include "Project.h"
#include "ProjectVersion.h"

Project::Project() :
    _playState(*this),
    _routing(*this)
{
    for (size_t i = 0; i < _tracks.size(); ++i) {
        _tracks[i].setTrackIndex(i);
    }

    clear();
}

void Project::writeRouted(Routing::Target target, int intValue, float floatValue) {
    switch (target) {
    case Routing::Target::Tempo:
        setTempo(floatValue, true);
        break;
    case Routing::Target::Swing:
        setSwing(intValue, true);
        break;
    default:
        break;
    }
}

void Project::clear() {
    _slot = uint8_t(-1);
    StringUtils::copy(_name, TXT_MODEL_INIT, sizeof(_name));
    setAutoLoaded(false);
    setTempo(120.f);
    setSwing(50);
    setTimeSignature(TimeSignature());
    setSyncMeasure(1);
    setScale(0);
    setRootNote(0);
    setMonitorMode(Types::MonitorMode::Always);
    setRecordMode(Types::RecordMode::Overdub);
    setMidiInputMode(Types::MidiInputMode::All);
    setCvGateInput(Types::CvGateInput::Off);
    setCurveCvInput(Types::CurveCvInput::Off);

    _clockSetup.clear();

    for (auto &track : _tracks) {
        track.clear();
    }

    for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
        _cvOutputTracks[i] = i;
        _gateOutputTracks[i] = i;
    }

    _song.clear();
    _playState.clear();
    _routing.clear();
    _midiOutput.clear();

    for (auto &userScale : UserScale::userScales) {
        userScale.clear();
    }

    setSelectedTrackIndex(0);
    setSelectedPatternIndex(0);

    // load demo project on simulator
#if PLATFORM_SIM
    // Demo Pattern 1
    noteSequence(0, 0).setLastStep(15);
    noteSequence(0, 0).setGates({ 1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0 }); // Kick
    noteSequence(1, 0).setLastStep(15);
    noteSequence(1, 0).setGates({ 0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0 }); // Snare
    noteSequence(2, 0).setLastStep(15);
    noteSequence(2, 0).setGates({ 0,1,0,0,1,0,0,1,0,0,1,0,0,1,0,0 }); // Rimshot
    noteSequence(3, 0).setLastStep(15);
    noteSequence(3, 0).setGates({ 0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0 }); // Clap
    noteSequence(4, 0).setLastStep(15);
    noteSequence(4, 0).setGates({ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 }); // Closed HiHat
    noteSequence(5, 0).setLastStep(15);
    noteSequence(5, 0).setGates({ 0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0 }); // Open HiHat
    noteSequence(7, 0).setLastStep(15);
    noteSequence(7, 0).setGates({ 1,0,0,1,0,0,1,0,0,1,0,0,1,0,0,1 }); // Synth
    noteSequence(7, 0).setNotes({ 0,0,0,0,12,0,12,1,24,21,22,0,3,6,12,1 });

    // Demo Pattern 2
    noteSequence(0, 1).setLastStep(15);
    noteSequence(0, 1).setGates({ 1,0,0,0,1,0,0,0,1,0,0,0,1,0,1,0 });
    noteSequence(1, 1).setLastStep(15);
    noteSequence(1, 1).setGates({ 0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0 });
    noteSequence(2, 1).setLastStep(15);
    noteSequence(2, 1).setGates({ 0,1,0,0,1,0,0,1,0,0,1,0,0,1,0,0 });
    noteSequence(3, 1).setLastStep(15);
    noteSequence(3, 1).setGates({ 0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0 });
    noteSequence(4, 1).setLastStep(15);
    noteSequence(4, 1).setGates({ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 });
    noteSequence(5, 1).setLastStep(15);
    noteSequence(5, 1).setGates({ 0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0 });
    noteSequence(7, 1).setLastStep(15);
    noteSequence(7, 1).setGates({ 1,0,0,1,0,0,1,0,0,1,0,0,1,0,0,1 });
    noteSequence(7, 1).setNotes({ 0,0,0,0,12,0,12,1,24,21,22,0,3,6,12,1 });

    // Demo Pattern 3
    noteSequence(0, 2).setLastStep(15);
    noteSequence(0, 2).setGates({ 1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0 });
    noteSequence(1, 2).setLastStep(15);
    noteSequence(1, 2).setGates({ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 });
    noteSequence(2, 2).setLastStep(15);
    noteSequence(2, 2).setGates({ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 });
    noteSequence(3, 2).setLastStep(15);
    noteSequence(3, 2).setGates({ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 });
    noteSequence(4, 2).setLastStep(15);
    noteSequence(4, 2).setGates({ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 });
    noteSequence(5, 2).setLastStep(15);
    noteSequence(5, 2).setGates({ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 });
    noteSequence(7, 2).setLastStep(15);
    noteSequence(7, 2).setGates({ 1,0,0,1,0,0,1,0,0,1,0,0,1,0,0,1 });
    noteSequence(7, 2).setNotes({ 0,0,0,0,12,0,12,1,24,21,22,0,3,6,12,1 });

    // Demo Pattern 4
    noteSequence(0, 3).setLastStep(15);
    noteSequence(0, 3).setGates({ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 });
    noteSequence(1, 3).setLastStep(15);
    noteSequence(1, 3).setGates({ 0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1 });
    noteSequence(2, 3).setLastStep(15);
    noteSequence(2, 3).setGates({ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 });
    noteSequence(3, 3).setLastStep(15);
    noteSequence(3, 3).setGates({ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 });
    noteSequence(4, 3).setLastStep(15);
    noteSequence(4, 3).setGates({ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 });
    noteSequence(5, 3).setLastStep(15);
    noteSequence(5, 3).setGates({ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 });
    noteSequence(7, 3).setLastStep(15);
    noteSequence(7, 3).setGates({ 1,0,0,1,0,0,1,0,0,1,0,0,1,0,0,1 });
    noteSequence(7, 3).setNotes({ 0,0,0,0,12,0,12,1,24,21,22,0,3,6,12,1 });

#endif

#if PLATFORM_SIM
    // Demo Song
    _song.clear();

    _song.chainPattern(0);
    _song.chainPattern(1);
    _song.chainPattern(2);
    _song.chainPattern(3);
    _song.chainPattern(4);
    _song.chainPattern(5);
    _song.chainPattern(6);
    _song.chainPattern(7);

    // Slot 0:
    _song.setPattern(0, 0, 0);
    _song.setPattern(0, 1, 4);
    _song.setPattern(0, 2, 4);
    _song.setPattern(0, 3, 4);
    _song.setPattern(0, 4, 4);
    _song.setPattern(0, 5, 4);
    _song.setPattern(0, 6, 4);
    _song.setPattern(0, 7, 4);
    _song.setRepeats(0, 4);

    // Slot 1
    _song.setPattern(1, 0, 0);
    _song.setPattern(1, 1, 4);
    _song.setPattern(1, 2, 4);
    _song.setPattern(1, 3, 4);
    _song.setPattern(1, 4, 0);
    _song.setPattern(1, 5, 0);
    _song.setPattern(1, 6, 4);
    _song.setPattern(1, 7, 4);
    _song.setRepeats(1, 3);

    // Slot 2
    _song.setPattern(2, 0, 1);
    _song.setPattern(2, 1, 4);
    _song.setPattern(2, 2, 4);
    _song.setPattern(2, 3, 4);
    _song.setPattern(2, 4, 0);
    _song.setPattern(2, 5, 0);
    _song.setPattern(2, 6, 4);
    _song.setPattern(2, 7, 4);
    _song.setRepeats(2, 1);

    // Slot 3
    _song.setPattern(3, 0, 0);
    _song.setPattern(3, 1, 4);
    _song.setPattern(3, 2, 4);
    _song.setPattern(3, 3, 4);
    _song.setPattern(3, 4, 0);
    _song.setPattern(3, 5, 0);
    _song.setPattern(3, 6, 4);
    _song.setPattern(3, 7, 0);
    _song.setRepeats(3, 3);

    // Slot 4
    _song.setPattern(4, 0, 0);
    _song.setPattern(4, 1, 3);
    _song.setPattern(4, 2, 4);
    _song.setPattern(4, 3, 4);
    _song.setPattern(4, 4, 0);
    _song.setPattern(4, 5, 0);
    _song.setPattern(4, 6, 4);
    _song.setPattern(4, 7, 0);
    _song.setRepeats(4, 1);

    // Slot 5
    _song.setPattern(5, 0, 0);
    _song.setPattern(5, 1, 0);
    _song.setPattern(5, 2, 0);
    _song.setPattern(5, 3, 0);
    _song.setPattern(5, 4, 0);
    _song.setPattern(5, 5, 0);
    _song.setPattern(5, 6, 0);
    _song.setPattern(5, 7, 0);
    _song.setRepeats(5, 3);

    // Slot 6
    _song.setPattern(6, 0, 1);
    _song.setPattern(6, 1, 0);
    _song.setPattern(6, 2, 0);
    _song.setPattern(6, 3, 0);
    _song.setPattern(6, 4, 0);
    _song.setPattern(6, 5, 0);
    _song.setPattern(6, 6, 0);
    _song.setPattern(6, 7, 0);
    _song.setRepeats(6, 1);

    // Slot 7
    _song.setPattern(7, 0, 1);
    _song.setPattern(7, 1, 3);
    _song.setPattern(7, 2, 0);
    _song.setPattern(7, 3, 0);
    _song.setPattern(7, 4, 0);
    _song.setPattern(7, 5, 0);
    _song.setPattern(7, 6, 0);
    _song.setPattern(7, 7, 0);
    _song.setRepeats(7, 1);

    _song.setMute(6, 0, true);
    _song.setMute(6, 1, true);
    _song.setMute(6, 2, true);
    _song.setMute(6, 3, true);
    _song.setMute(6, 6, true);
    _song.setMute(7, 0, true);
    _song.setMute(7, 2, true);
    _song.setMute(7, 3, true);
    _song.setMute(7, 6, true);
#endif

    _observable.notify(ProjectCleared);
}

void Project::clearPattern(int patternIndex) {
    for (auto &track : _tracks) {
        track.clearPattern(patternIndex);
    }
}

void Project::setTrackMode(int trackIndex, Track::TrackMode trackMode) {
    // TODO make sure engine is synced to this before updating UI
    _playState.revertSnapshot();
    _tracks[trackIndex].setTrackMode(trackMode);
    _observable.notify(TrackModeChanged);
}

void Project::write(VersionedSerializedWriter &writer) const {
    writer.write(_name, NameLength + 1);
    writer.write(_tempo.base);
    writer.write(_swing.base);
    _timeSignature.write(writer);
    writer.write(_syncMeasure);
    writer.write(_scale);
    writer.write(_rootNote);
    writer.write(_monitorMode);
    writer.write(_recordMode);
    writer.write(_midiInputMode);
    _midiInputSource.write(writer);
    writer.write(_cvGateInput);
    writer.write(_curveCvInput);

    _clockSetup.write(writer);

    writeArray(writer, _tracks);
    writeArray(writer, _cvOutputTracks);
    writeArray(writer, _gateOutputTracks);

    _song.write(writer);
    _playState.write(writer);
    _routing.write(writer);
    _midiOutput.write(writer);

    writeArray(writer, UserScale::userScales);

    writer.write(_selectedTrackIndex);
    writer.write(_selectedPatternIndex);

    writer.writeHash();

    _autoLoaded = false;
}

bool Project::read(VersionedSerializedReader &reader) {
    clear();

    reader.read(_name, NameLength + 1, ProjectVersion::Version5);
    reader.read(_tempo.base);
    reader.read(_swing.base);
    if (reader.dataVersion() >= ProjectVersion::Version18) {
        _timeSignature.read(reader);
    }
    reader.read(_syncMeasure);
    reader.read(_scale);
    reader.read(_rootNote);
    reader.read(_monitorMode, ProjectVersion::Version30);
    reader.read(_recordMode);
    if (reader.dataVersion() >= ProjectVersion::Version29) {
        reader.read(_midiInputMode);
        _midiInputSource.read(reader);
    }
    reader.read(_cvGateInput, ProjectVersion::Version6);
    reader.read(_curveCvInput, ProjectVersion::Version11);

    _clockSetup.read(reader);

    readArray(reader, _tracks);
    readArray(reader, _cvOutputTracks);
    readArray(reader, _gateOutputTracks);

    _song.read(reader);
    _playState.read(reader);
    _routing.read(reader);
    _midiOutput.read(reader);

    if (reader.dataVersion() >= ProjectVersion::Version5) {
        readArray(reader, UserScale::userScales);
    }

    reader.read(_selectedTrackIndex);
    reader.read(_selectedPatternIndex);

    bool success = reader.checkHash();
    if (success) {
        _observable.notify(ProjectRead);
    } else {
        clear();
    }

    return success;
}
