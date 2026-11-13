// Offline tests for the doot detection engine, using synthetic "sung" notes.
// Build: see CMakeLists.txt (DOOT_BUILD_TESTS) or compile directly with g++.

#include "../Source/Engine/NoteSegmenter.h"
#include "../Source/Engine/PitchTracker.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace doot;

namespace
{
constexpr double pi = 3.14159265358979323846;

struct SungNote
{
    double start, length;
    int midi;
    float gainDb = -12.0f;
    // How the note connects to the previous one.
    enum Join { Gap, Dip, Glide } join = Gap;
};

double midiToHz (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

// A rough sung "doot": harmonic voice with an "oo" vowel tilt, vibrato,
// a small pitch scoop at the attack, a noisy "d" burst, and a background hiss.
std::vector<float> synth (const std::vector<SungNote>& notes, double sr, unsigned seed,
                          float hissDb = -60.0f, double vibratoSemis = 0.2)
{
    const double total = notes.back().start + notes.back().length + 0.3;
    std::vector<float> out ((size_t) (total * sr), 0.0f);
    std::mt19937 rng (seed);
    std::normal_distribution<float> noise (0.0f, 1.0f);

    for (auto& s : out)
        s = (float) std::pow (10.0, hissDb / 20.0) * noise (rng); // background hiss

    double phase = 0.0;
    for (size_t n = 0; n < notes.size(); ++n)
    {
        const auto& note = notes[n];
        const double gain = std::pow (10.0, note.gainDb / 20.0);
        const int first = (int) (note.start * sr);
        const int count = (int) (note.length * sr);
        const double prevMidi = n > 0 ? notes[n - 1].midi : note.midi;

        if (note.join == SungNote::Gap)
        {
            // consonant burst
            for (int i = 0; i < (int) (0.008 * sr); ++i)
                out[(size_t) (first + i)] += (float) (gain * 0.3 * noise (rng));
        }

        for (int i = 0; i < count; ++i)
        {
            const double t = i / sr;
            double m = note.midi;

            if (note.join == SungNote::Glide)
                m = prevMidi + (note.midi - prevMidi) * std::min (1.0, t / 0.06);
            else
                m -= 0.8 * std::exp (-t / 0.02); // attack scoop

            m += vibratoSemis * std::sin (2 * pi * 5.5 * t) * std::min (1.0, t / 0.25); // vibrato
            phase += 2 * pi * midiToHz (m) / sr;

            double v = 0.0;
            for (int k = 1; k <= 12; ++k)
            {
                const double f = k * midiToHz (m);
                if (f > sr * 0.45) break;
                const double vowel = 1.0 / (1.0 + std::pow (f / 450.0, 2.0)); // "oo"
                v += vowel / k * std::sin (k * phase);
            }

            double env = 1.0;
            const double attack = note.join == SungNote::Gap ? 0.01 : 0.0;
            if (attack > 0 && t < attack) env = t / attack;
            if (t > note.length - 0.02) env = std::max (0.0, (note.length - t) / 0.02);
            if (note.join == SungNote::Dip && t < 0.04)
                env *= 0.25 + 0.75 * (t / 0.04); // "d" between connected doots

            out[(size_t) (first + i)] += (float) (gain * env * v);
        }
    }
    return out;
}

struct Result
{
    int passed = 0, failed = 0;
};

void runCase (const std::string& name, const std::vector<SungNote>& notes, double sr, Result& r,
              double startTolerance = 0.025, float hissDb = -60.0f, double vibrato = 0.2)
{
    const auto audio = synth (notes, sr, 1234, hissDb, vibrato);

    PitchTracker tracker;
    tracker.prepare (sr);
    std::vector<PitchFrame> frames;
    // feed in host-sized blocks like a real plugin would
    for (size_t i = 0; i < audio.size(); i += 512)
        tracker.push (audio.data() + i, (int) std::min<size_t> (512, audio.size() - i), frames);

    const auto found = segmentNotes (frames, SegmenterSettings {}, tracker.getHopSec());

    bool ok = found.size() == notes.size();
    double worstStart = 0.0, meanSigned = 0.0;
    if (ok)
    {
        for (size_t i = 0; i < notes.size(); ++i)
        {
            if (found[i].midiNote != notes[i].midi)
                ok = false;
            double target = notes[i].start;
            if (notes[i].join == SungNote::Glide)
                target += 0.03; // middle of the slide
            const double err = std::abs (found[i].startSec - target);
            meanSigned += (found[i].startSec - target) / (double) notes.size();
            worstStart = std::max (worstStart, err);
            if (err > startTolerance)
                ok = false;
        }
    }

    std::printf ("%s  %-34s @%6.0f Hz  expected %zu notes, found %zu, worst start error %.1f ms (avg %+.1f)\n",
                 ok ? "PASS" : "FAIL", name.c_str(), sr, notes.size(), found.size(), worstStart * 1000.0, meanSigned * 1000.0);

    if (! ok)
    {
        for (const auto& n : found)
            std::printf ("        found  %.3f-%.3f s  note %d (%.2f)  strength %.2f\n",
                         n.startSec, n.endSec, n.midiNote, n.exactPitch, n.strength);
        for (const auto& n : notes)
            std::printf ("        wanted %.3f-%.3f s  note %d\n", n.start, n.start + n.length, n.midi);
    }

    (ok ? r.passed : r.failed)++;
}
} // namespace

int main()
{
    Result r;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        runCase ("separate doots, mid range", {
            { 0.20, 0.20, 60 }, { 0.50, 0.20, 62 }, { 0.80, 0.20, 64 }, { 1.10, 0.40, 67 } }, sr, r);

        runCase ("fast sixteenths, same pitch", {
            { 0.20, 0.09, 57 }, { 0.325, 0.09, 57 }, { 0.45, 0.09, 57 }, { 0.575, 0.09, 57 } }, sr, r);

        runCase ("low bass doots", {
            { 0.20, 0.25, 40 }, { 0.55, 0.25, 43 }, { 0.90, 0.25, 45 }, { 1.25, 0.25, 38 } }, sr, r);

        runCase ("high doots", {
            { 0.20, 0.15, 76 }, { 0.40, 0.15, 79 }, { 0.60, 0.15, 81 }, { 0.80, 0.30, 84 } }, sr, r);

        SungNote a { 0.20, 0.30, 62 }, b { 0.50, 0.30, 62 }, c { 0.80, 0.30, 65 };
        b.join = SungNote::Dip; c.join = SungNote::Dip;
        runCase ("connected doots (no silence)", { a, b, c }, sr, r);

        SungNote g1 { 0.20, 0.40, 60 }, g2 { 0.60, 0.40, 64 }, g3 { 1.00, 0.40, 59 };
        g2.join = SungNote::Glide; g3.join = SungNote::Glide;
        runCase ("held note sliding to new pitches", { g1, g2, g3 }, sr, r, 0.03);

        SungNote q1 { 0.20, 0.20, 60, -30.0f }, q2 { 0.50, 0.20, 65, -30.0f };
        runCase ("quiet singing", { q1, q2 }, sr, r);

        runCase ("noisy room", {
            { 0.20, 0.20, 55 }, { 0.50, 0.20, 59 }, { 0.80, 0.20, 62 } }, sr, r, 0.025, -38.0f);

        runCase ("wide vibrato", {
            { 0.20, 0.60, 64 }, { 0.90, 0.60, 62 } }, sr, r, 0.025, -60.0f, 0.45);

        SungNote l1 { 0.20, 0.20, 50, -2.0f }, l2 { 0.50, 0.20, 53, -2.0f };
        runCase ("loud singing", { l1, l2 }, sr, r);
    }

    std::printf ("\n%d passed, %d failed\n", r.passed, r.failed);
    return r.failed == 0 ? 0 : 1;
}
