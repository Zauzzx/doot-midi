#pragma once

#include "Engine/PitchTracker.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>

// Records a take: the audio thread copies the voice into a lock-free FIFO and a
// background thread runs the pitch tracker on it, so the audio never glitches.
class TakeRecorder : private juce::Thread
{
public:
    enum class State { Idle, Armed, Recording, Finishing };

    struct TakeInfo
    {
        bool synced = false;     // started while FL's transport was playing
        double startPpq = 0.0;   // FL song position (in beats) when recording began
        double bpm = 120.0;
        int sigNumerator = 4;
        int sigDenominator = 4;
    };

    TakeRecorder();
    ~TakeRecorder() override;

    void setSampleRate (double sr) { sampleRate.store (sr); }

    // Message thread
    void arm (bool waitForTransport);
    void stop();
    void clear();
    void restoreTake (std::vector<doot::PitchFrame> savedFrames, const TakeInfo& savedInfo, double sr);
    State getState() const { return state.load(); }

    // Audio thread
    void processAudio (const juce::AudioBuffer<float>& buffer, int numInputChannels,
                       const juce::Optional<juce::AudioPlayHead::PositionInfo>& position);

    // Any non-audio thread. Returns a number that changes whenever the take changes.
    int copyTake (std::vector<doot::PitchFrame>& framesOut, TakeInfo& infoOut, double& hopSecOut) const;
    int getRevision() const { return revision.load(); }

private:
    void run() override;

    static constexpr int fifoSize = 1 << 19;
    static constexpr size_t maxFrames = 200 * 60 * 15; // 15 minutes

    juce::AbstractFifo fifo { fifoSize };
    std::vector<float> ring;

    std::atomic<State> state { State::Idle };
    std::atomic<bool> waitForTransport { true };
    std::atomic<bool> stopRequested { false };
    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<int> revision { 0 };

    TakeInfo startInfo;              // written by the audio thread before Recording
    bool infoCaptured = false;       // guarded by lock

    juce::CriticalSection lock;
    doot::PitchTracker tracker;
    std::vector<doot::PitchFrame> frames;
    TakeInfo info;
    std::vector<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TakeRecorder)
};
