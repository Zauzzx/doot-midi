#pragma once

#include "PitchTracker.h"
#include <vector>

namespace doot
{
struct DetectedNote
{
    double startSec = 0.0;
    double endSec = 0.0;
    int midiNote = 60;
    float strength = 0.0f;   // 0..1 from how loud the note's attack was
    float exactPitch = 60.0f; // un-rounded pitch, for display and tuning feedback
};

// Turns the pitch tracker's frames into notes. Built for "doots": each new
// consonant attack (an energy dip and rise) starts a new note, and a held
// note that glides to a new pitch is split where the pitch settles.
struct SegmenterSettings
{
    float gateDb = -45.0f;          // anything quieter than this is silence
    float minClarity = 0.70f;       // frames less pitched than this are ignored
    float minNoteSec = 0.05f;
    float pitchSplitSemitones = 0.75f;
    float dipDb = 8.0f;             // drop from the note's peak that counts as a consonant
    float reattackDb = 5.0f;        // rise out of the dip that starts a new note
};

std::vector<DetectedNote> segmentNotes (const std::vector<PitchFrame>& frames,
                                        const SegmenterSettings& settings,
                                        double hopSec);
} // namespace doot
