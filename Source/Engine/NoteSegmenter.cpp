#include "NoteSegmenter.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace doot
{
namespace
{
    constexpr float noPitch = std::numeric_limits<float>::quiet_NaN();

    float hzToMidi (float hz) { return 69.0f + 12.0f * std::log2 (hz / 440.0f); }

    float median (std::vector<float> v)
    {
        if (v.empty())
            return noPitch;
        const size_t mid = v.size() / 2;
        std::nth_element (v.begin(), v.begin() + (long) mid, v.end());
        float m = v[mid];
        if (v.size() % 2 == 0)
        {
            const float lower = *std::max_element (v.begin(), v.begin() + (long) mid);
            m = 0.5f * (m + lower);
        }
        return m;
    }

    struct Builder
    {
        const std::vector<PitchFrame>& frames;
        const std::vector<float>& level;
        const std::vector<float>& pitch;
        const SegmenterSettings& s;
        double hopSec;
        std::vector<DetectedNote> notes;

        void emit (int start, int end)
        {
            if (end <= start)
                return;

            std::vector<float> voiced;
            for (int i = start; i < end; ++i)
                if (! std::isnan (pitch[(size_t) i]))
                    voiced.push_back (pitch[(size_t) i]);

            const double startSec = frames[(size_t) start].timeSec;
            const double endSec = frames[(size_t) end - 1].timeSec + hopSec;

            if (voiced.size() < 3 || endSec - startSec < s.minNoteSec)
                return;

            // Ignore the attack scoop and the release when choosing the pitch.
            const size_t from = (size_t) std::floor (voiced.size() * 0.2);
            const size_t to = std::max (from + 1, (size_t) std::ceil (voiced.size() * 0.85));
            const float exact = median (std::vector<float> (voiced.begin() + (long) from,
                                                            voiced.begin() + (long) std::min (to, voiced.size())));

            float peak = -100.0f;
            const int attackFrames = std::max (1, (int) std::lround (0.08 / hopSec));
            for (int i = start; i < std::min (end, start + attackFrames); ++i)
                peak = std::max (peak, level[(size_t) i]);

            DetectedNote note;
            note.startSec = startSec;
            note.endSec = endSec;
            note.exactPitch = exact;
            note.midiNote = std::clamp ((int) std::lround (exact), 0, 127);
            note.strength = std::clamp ((peak - s.gateDb) / (-3.0f - s.gateDb), 0.0f, 1.0f);
            notes.push_back (note);
        }
    };
} // namespace

std::vector<DetectedNote> segmentNotes (const std::vector<PitchFrame>& frames,
                                        const SegmenterSettings& s,
                                        double hopSec)
{
    const int n = (int) frames.size();
    if (n == 0)
        return {};

    // Lightly smoothed level in dB.
    std::vector<float> level ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        float sum = 0.0f;
        int count = 0;
        for (int k = std::max (0, i - 1); k <= std::min (n - 1, i + 1); ++k)
        {
            sum += frames[(size_t) k].levelDb;
            ++count;
        }
        level[(size_t) i] = sum / (float) count;
    }

    // Pitch in MIDI units, NaN where the sound isn't clearly pitched.
    std::vector<float> raw ((size_t) n, noPitch);
    for (int i = 0; i < n; ++i)
    {
        const auto& f = frames[(size_t) i];
        if (f.f0 > 0.0f && f.clarity >= s.minClarity && level[(size_t) i] > s.gateDb - 10.0f)
            raw[(size_t) i] = hzToMidi (f.f0);
    }

    // Fix octave jumps and other single-frame glitches with a local median.
    std::vector<float> pitch = raw;
    for (int i = 0; i < n; ++i)
    {
        if (std::isnan (raw[(size_t) i]))
            continue;
        std::vector<float> around;
        for (int k = std::max (0, i - 3); k <= std::min (n - 1, i + 3); ++k)
            if (! std::isnan (raw[(size_t) k]))
                around.push_back (raw[(size_t) k]);
        const float m = median (around);
        if (std::abs (raw[(size_t) i] - m) > 5.0f)
            pitch[(size_t) i] = m;
    }

    Builder b { frames, level, pitch, s, hopSec, {} };

    const int unvoicedLimit = std::max (4, (int) std::lround (0.05 / hopSec));
    const int stableFrames = std::max (4, (int) std::lround (0.035 / hopSec));

    bool active = false;
    int start = 0;
    float peak = -100.0f;
    bool inDip = false;
    float dipMin = 0.0f;
    int dipIdx = 0;
    int quietRun = 0, unvoicedRun = 0;
    std::vector<std::pair<int, float>> notePitches;

    auto begin = [&] (int at)
    {
        active = true;
        start = at;
        peak = level[(size_t) at];
        inDip = false;
        quietRun = unvoicedRun = 0;
        notePitches.clear();
    };

    auto finish = [&] (int at)
    {
        b.emit (start, at);
        active = false;
    };

    for (int i = 0; i < n; ++i)
    {
        const float lv = level[(size_t) i];
        const bool voiced = ! std::isnan (pitch[(size_t) i]);

        if (! active)
        {
            if (lv > s.gateDb && voiced)
            {
                int good = 0;
                for (int k = i; k < std::min (n, i + 3); ++k)
                    if (level[(size_t) k] > s.gateDb && ! std::isnan (pitch[(size_t) k]))
                        ++good;

                if (good >= 2)
                {
                    // Step back to where the attack began rising.
                    int j = i;
                    while (j > 0 && i - j < 1 && level[(size_t) j - 1] > s.gateDb - 15.0f
                           && level[(size_t) j - 1] < level[(size_t) j])
                        --j;
                    begin (j);
                }
            }
            if (! active)
                continue;
        }

        // End of the note: it went quiet...
        if (lv < s.gateDb - 3.0f)
        {
            if (++quietRun >= 2)
            {
                finish (i - quietRun + 1);
                continue;
            }
        }
        else
            quietRun = 0;

        // ...or it stopped being a pitched sound (breath, noise).
        if (! voiced)
        {
            if (++unvoicedRun >= unvoicedLimit)
            {
                finish (i - unvoicedRun + 1);
                continue;
            }
        }
        else
            unvoicedRun = 0;

        // A new consonant attack without full silence: dip then rise.
        if (! inDip)
        {
            peak = std::max (peak, lv);
            if (lv < peak - s.dipDb)
            {
                inDip = true;
                dipMin = lv;
                dipIdx = i;
            }
        }
        else
        {
            if (lv < dipMin)
            {
                dipMin = lv;
                dipIdx = i;
            }
            else if (lv > dipMin + s.reattackDb && voiced)
            {
                finish (dipIdx);
                begin (dipIdx + 1 <= i ? dipIdx + 1 : i);
                peak = lv;
                continue;
            }
        }

        // A held note that moves to a new pitch.
        if (voiced)
        {
            notePitches.emplace_back (i, pitch[(size_t) i]);

            const int count = (int) notePitches.size();
            if (count >= stableFrames + 4)
            {
                const auto recentBegin = notePitches.end() - stableFrames;
                const bool contiguous = (notePitches.back().first - recentBegin->first) <= stableFrames + 2;

                std::vector<float> recent, earlier;
                for (auto it = recentBegin; it != notePitches.end(); ++it)
                    recent.push_back (it->second);
                for (auto it = notePitches.begin() + std::min (3, count - stableFrames - 1); it != recentBegin; ++it)
                    earlier.push_back (it->second);

                const auto [lo, hi] = std::minmax_element (recent.begin(), recent.end());
                const float recentMedian = median (recent);
                const float earlierMedian = median (earlier);

                if (contiguous && ! earlier.empty() && (*hi - *lo) < 0.7f
                    && std::abs (recentMedian - earlierMedian) >= s.pitchSplitSemitones)
                {
                    // Split halfway through the slide: walk back while the pitch
                    // is still closer to the new note than to the old one.
                    int splitAt = recentBegin->first;
                    for (auto it = recentBegin; it != notePitches.begin(); --it)
                    {
                        auto prev = it - 1;
                        if (std::abs (prev->second - recentMedian) > std::abs (prev->second - earlierMedian))
                            break;
                        splitAt = prev->first;
                    }

                    if (splitAt > start)
                    {
                        // Only the settled part counts as the new note's pitch history,
                        // otherwise the slide itself would trigger a second split.
                        const int settledFrom = recentBegin->first;
                        finish (splitAt);
                        begin (splitAt);
                        for (int k = settledFrom; k <= i; ++k)
                            if (! std::isnan (pitch[(size_t) k]))
                                notePitches.emplace_back (k, pitch[(size_t) k]);
                    }
                }
            }
        }
    }

    if (active)
        finish (n);

    return std::move (b.notes);
}
} // namespace doot
