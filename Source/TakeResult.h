#pragma once

#include "Engine/NoteSegmenter.h"
#include "TakeRecorder.h"
#include <juce_audio_basics/juce_audio_basics.h>

// A finished (or in-progress) take, converted into notes on FL's beat grid.
struct BeatNote
{
    double startBeat = 0.0;
    double lengthBeats = 0.25;
    int midiNote = 60;
    int velocity = 100;
    float exactPitch = 60.0f;
};

struct ResultSettings
{
    float gateDb = -45.0f;
    int quantizeIndex = 2;
    bool useVelocity = false;
    float nudgeMs = 0.0f;
};

struct TakeResult
{
    TakeRecorder::TakeInfo info;
    std::vector<doot::PitchFrame> frames;
    double hopSec = 0.005;

    std::vector<BeatNote> notes;
    double beatsPerSecond = 2.0;
    double offsetBeats = 0.0;  // beat position of take time zero
    double beatsPerBar = 4.0;

    double secondsToBeats (double sec, float nudgeMs) const
    {
        return (sec + nudgeMs / 1000.0) * beatsPerSecond + offsetBeats;
    }
};

juce::StringArray getQuantizeNames();
double getQuantizeBeats (int index);

TakeResult buildResult (std::vector<doot::PitchFrame> frames, const TakeRecorder::TakeInfo& info,
                        double hopSec, const ResultSettings& settings);

juce::MidiFile makeMidiFile (const TakeResult& result);

// FL Studio names middle C (MIDI 60) "C5".
juce::String flNoteName (int midiNote);
