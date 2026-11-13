#include "PitchTracker.h"

#include <algorithm>
#include <cmath>

namespace doot
{
void PitchTracker::prepare (double sampleRate, Settings s)
{
    settings = s;
    inputRate = sampleRate > 0 ? sampleRate : 44100.0;

    decimation = std::max (1, (int) std::floor (inputRate / 12000.0));
    analysisRate = inputRate / decimation;

    const double cutoff = 0.38 * analysisRate;
    aa1.setLowPass (inputRate, cutoff, 0.5412);  // 4th-order Butterworth pair
    aa2.setLowPass (inputRate, cutoff, 1.3066);

    window = std::max (64, (int) std::lround (settings.windowSec * analysisRate));
    maxTau = (int) std::ceil (analysisRate / settings.minFrequency);
    minTau = std::max (2, (int) std::floor (analysisRate / settings.maxFrequency));
    hop    = std::max (1, (int) std::lround (settings.hopSec * analysisRate));
    levelWindow = std::clamp ((int) std::lround (settings.levelWindowSec * analysisRate), 8, window);

    diff.assign ((size_t) maxTau + 2, 0.0f);
    buffer.reserve ((size_t) (window + maxTau) * 4);
    reset();
}

void PitchTracker::reset()
{
    aa1.reset();
    aa2.reset();
    decimationCounter = 0;
    buffer.clear();
    consumed = 0;
}

void PitchTracker::push (const float* samples, int numSamples, std::vector<PitchFrame>& out)
{
    for (int i = 0; i < numSamples; ++i)
    {
        float x = samples[i];

        if (decimation > 1)
            x = aa2.process (aa1.process (x));

        if (decimationCounter == 0)
            buffer.push_back (x);

        if (++decimationCounter >= decimation)
            decimationCounter = 0;
    }

    const int needed = window + maxTau + 1;
    size_t readPos = 0;

    while (buffer.size() - readPos >= (size_t) needed)
    {
        PitchFrame frame;
        analyseFrame (buffer.data() + readPos, frame);
        frame.timeSec = ((double) consumed + (double) readPos + window * 0.5) / analysisRate;
        out.push_back (frame);
        readPos += (size_t) hop;
    }

    if (readPos > 0)
    {
        buffer.erase (buffer.begin(), buffer.begin() + (long) readPos);
        consumed += (long long) readPos;
    }
}

void PitchTracker::analyseFrame (const float* x, PitchFrame& result) const
{
    // Level from a short window centred on the frame time, so note starts are sharp.
    const int levelStart = (window - levelWindow) / 2;
    double energy = 0.0;
    for (int j = levelStart; j < levelStart + levelWindow; ++j)
        energy += (double) x[j] * x[j];

    const double rms = std::sqrt (energy / levelWindow);
    result.levelDb = rms > 1.0e-5 ? (float) (20.0 * std::log10 (rms)) : -100.0f;

    if (result.levelDb < -75.0f)
        return; // silence: no pitch

    // YIN step 1: difference function
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        double sum = 0.0;
        const float* y = x + tau;
        for (int j = 0; j < window; ++j)
        {
            const double d = (double) x[j] - y[j];
            sum += d * d;
        }
        diff[(size_t) tau] = (float) sum;
    }

    // YIN step 2: cumulative mean normalised difference
    diff[0] = 1.0f;
    double running = 0.0;
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        running += diff[(size_t) tau];
        diff[(size_t) tau] = running > 0.0 ? (float) (diff[(size_t) tau] * tau / running) : 1.0f;
    }

    // YIN step 3: first dip under the threshold, else the global minimum
    int best = -1;
    for (int tau = minTau; tau < maxTau; ++tau)
    {
        if (diff[(size_t) tau] < settings.yinThreshold)
        {
            while (tau + 1 < maxTau && diff[(size_t) tau + 1] < diff[(size_t) tau])
                ++tau;
            best = tau;
            break;
        }
    }

    if (best < 0)
    {
        best = minTau;
        for (int tau = minTau + 1; tau < maxTau; ++tau)
            if (diff[(size_t) tau] < diff[(size_t) best])
                best = tau;
    }

    // YIN step 4: parabolic interpolation for sub-sample accuracy
    double refined = best;
    if (best > 1 && best < maxTau)
    {
        const double a = diff[(size_t) best - 1], b = diff[(size_t) best], c = diff[(size_t) best + 1];
        const double denom = a - 2.0 * b + c;
        if (std::abs (denom) > 1.0e-12)
            refined = best + 0.5 * (a - c) / denom;
    }

    result.clarity = std::clamp (1.0f - diff[(size_t) best], 0.0f, 1.0f);
    result.f0 = refined > 0.0 ? (float) (analysisRate / refined) : 0.0f;
}
} // namespace doot
