#include "TakeResult.h"

#include <algorithm>
#include <cmath>

juce::StringArray getQuantizeNames()
{
    return { "Off", "1/32", "1/16", "1/8", "1/4" };
}

double getQuantizeBeats (int index)
{
    static const double grids[] = { 0.0, 0.125, 0.25, 0.5, 1.0 };
    return grids[juce::jlimit (0, 4, index)];
}

juce::String flNoteName (int midiNote)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[((midiNote % 12) + 12) % 12]) + juce::String (midiNote / 12);
}

TakeResult buildResult (std::vector<doot::PitchFrame> frames, const TakeRecorder::TakeInfo& info,
                        double hopSec, const ResultSettings& settings)
{
    TakeResult result;
    result.info = info;
    result.hopSec = hopSec;
    result.frames = std::move (frames);
    result.beatsPerSecond = info.bpm / 60.0;
    result.beatsPerBar = info.sigNumerator * 4.0 / juce::jmax (1, info.sigDenominator);

    doot::SegmenterSettings seg;
    seg.gateDb = settings.gateDb;
    const auto detected = doot::segmentNotes (result.frames, seg, hopSec);

    if (info.synced)
    {
        // Line the take up with the bar it was recorded in.
        const double barStart = std::floor (info.startPpq / result.beatsPerBar) * result.beatsPerBar;
        result.offsetBeats = info.startPpq - barStart;
    }
    else if (! detected.empty())
    {
        // No transport: the first note starts the clip.
        result.offsetBeats = -(detected.front().startSec + settings.nudgeMs / 1000.0) * result.beatsPerSecond;
    }

    const double grid = getQuantizeBeats (settings.quantizeIndex);

    std::vector<BeatNote> notes;
    for (const auto& d : detected)
    {
        BeatNote n;
        double start = result.secondsToBeats (d.startSec, settings.nudgeMs);
        double end = result.secondsToBeats (d.endSec, settings.nudgeMs);

        if (grid > 0.0)
        {
            start = std::round (start / grid) * grid;
            end = std::max (start + grid, std::round (end / grid) * grid);
        }

        start = std::max (0.0, start);
        if (end <= start)
            continue;

        n.startBeat = start;
        n.lengthBeats = end - start;
        n.midiNote = d.midiNote;
        n.exactPitch = d.exactPitch;
        n.velocity = settings.useVelocity ? juce::jlimit (1, 127, (int) std::lround (20.0f + d.strength * 107.0f)) : 100;
        notes.push_back (n);
    }

    // Quantizing can make notes collide: one note per slot, no overlaps.
    std::vector<BeatNote> cleaned;
    for (const auto& n : notes)
    {
        if (! cleaned.empty())
        {
            auto& prev = cleaned.back();
            if (n.startBeat <= prev.startBeat + 1.0e-6)
                continue;
            if (prev.startBeat + prev.lengthBeats > n.startBeat)
                prev.lengthBeats = n.startBeat - prev.startBeat;
        }
        cleaned.push_back (n);
    }

    result.notes = std::move (cleaned);
    return result;
}

juce::MidiFile makeMidiFile (const TakeResult& result)
{
    constexpr int ticksPerBeat = 960;

    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::textMetaEvent (3, "Doot MIDI"), 0);
    track.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / result.info.bpm)), 0);
    track.addEvent (juce::MidiMessage::timeSignatureMetaEvent (result.info.sigNumerator, result.info.sigDenominator), 0);

    double lastTick = 0.0;
    for (const auto& n : result.notes)
    {
        const double on = n.startBeat * ticksPerBeat;
        const double off = (n.startBeat + n.lengthBeats) * ticksPerBeat;
        track.addEvent (juce::MidiMessage::noteOn (1, n.midiNote, (juce::uint8) n.velocity), on);
        track.addEvent (juce::MidiMessage::noteOff (1, n.midiNote), off);
        lastTick = std::max (lastTick, off);
    }

    track.addEvent (juce::MidiMessage::endOfTrack(), lastTick);
    track.updateMatchedPairs();

    juce::MidiFile file;
    file.setTicksPerQuarterNote (ticksPerBeat);
    file.addTrack (track);
    return file;
}
