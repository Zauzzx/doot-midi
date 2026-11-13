#pragma once

#include "Biquad.h"
#include <vector>

namespace doot
{
// One analysis frame (every ~5 ms of audio).
struct PitchFrame
{
    double timeSec = 0.0;   // centre of the analysis window, from the start of the take
    float f0 = 0.0f;        // detected frequency in Hz, 0 when no clear pitch
    float clarity = 0.0f;   // 0..1, how periodic (pitched) the sound is
    float levelDb = -100.0f;
};

// Monophonic pitch tracker based on the YIN algorithm.
// Feed it audio of any length with push(); it appends finished frames to the
// vector you pass in. Works at any host sample rate: the voice is low-passed
// and downsampled to roughly 12-16 kHz first, which is plenty for singing and
// keeps the CPU cost tiny.
class PitchTracker
{
public:
    struct Settings
    {
        float minFrequency = 60.0f;    // a bit below a low male voice
        float maxFrequency = 1400.0f;  // above a high whistle-like doot
        float yinThreshold = 0.20f;
        float windowSec = 0.032f;      // pitch window
        float levelWindowSec = 0.014f; // shorter window for sharp note starts
        float hopSec = 0.005f;
    };

    void prepare (double sampleRate, Settings settings);
    void prepare (double sampleRate) { prepare (sampleRate, Settings{}); }
    void reset();

    void push (const float* samples, int numSamples, std::vector<PitchFrame>& out);

    double getAnalysisRate() const { return analysisRate; }
    double getHopSec() const { return hop / analysisRate; }

private:
    void analyseFrame (const float* frame, PitchFrame& result) const;

    Settings settings;
    double inputRate = 44100.0;
    double analysisRate = 14700.0;
    int decimation = 3;
    int decimationCounter = 0;
    Biquad aa1, aa2;

    int window = 0, maxTau = 0, minTau = 0, hop = 0, levelWindow = 0;

    std::vector<float> buffer;      // decimated samples waiting to be analysed
    long long consumed = 0;         // decimated samples already dropped from the buffer
    mutable std::vector<float> diff;
};
} // namespace doot
