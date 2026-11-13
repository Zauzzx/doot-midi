#pragma once

#include <cmath>

namespace doot
{
// Simple RBJ low-pass biquad, used as the anti-aliasing filter before
// the pitch tracker downsamples the voice.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void setLowPass (double sampleRate, double cutoff, double q = 0.7071)
    {
        const double pi = 3.14159265358979323846;
        const double w0 = 2.0 * pi * cutoff / sampleRate;
        const double cosw = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;

        b0 = ((1.0 - cosw) * 0.5) / a0;
        b1 = (1.0 - cosw) / a0;
        b2 = b0;
        a1 = (-2.0 * cosw) / a0;
        a2 = (1.0 - alpha) / a0;
        reset();
    }

    void reset() { z1 = z2 = 0; }

    float process (float x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
};
} // namespace doot
