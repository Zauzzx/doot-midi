#include "TakeRecorder.h"

TakeRecorder::TakeRecorder() : juce::Thread ("Doot MIDI analysis")
{
    ring.assign ((size_t) fifoSize, 0.0f);
    scratch.assign (4096, 0.0f);
    tracker.prepare (44100.0);
    startThread (juce::Thread::Priority::normal);
}

TakeRecorder::~TakeRecorder()
{
    stopThread (2000);
}

void TakeRecorder::arm (bool shouldWaitForTransport)
{
    if (state.load() != State::Idle)
        return;

    {
        const juce::ScopedLock sl (lock);
        tracker.prepare (sampleRate.load());
        fifo.reset(); // the audio thread doesn't write while Idle
        frames.clear();
        frames.reserve (200 * 60);
        infoCaptured = false;
        info = {};
    }

    stopRequested.store (false);
    waitForTransport.store (shouldWaitForTransport);
    revision.fetch_add (1);
    state.store (State::Armed);
}

void TakeRecorder::stop()
{
    auto expected = State::Armed;
    if (state.compare_exchange_strong (expected, State::Idle))
    {
        revision.fetch_add (1);
        return;
    }

    // Finish straight away rather than waiting for the next audio block, in case
    // the host has stopped sending audio.
    expected = State::Recording;
    state.compare_exchange_strong (expected, State::Finishing);
}

void TakeRecorder::clear()
{
    if (state.load() != State::Idle)
        return;

    const juce::ScopedLock sl (lock);
    frames.clear();
    infoCaptured = false;
    info = {};
    revision.fetch_add (1);
}

void TakeRecorder::restoreTake (std::vector<doot::PitchFrame> savedFrames, const TakeInfo& savedInfo, double sr)
{
    if (state.load() != State::Idle)
        return;

    const juce::ScopedLock sl (lock);
    tracker.prepare (sr > 0.0 ? sr : sampleRate.load()); // so the hop size matches the saved frames
    frames = std::move (savedFrames);
    info = savedInfo;
    infoCaptured = true;
    revision.fetch_add (1);
}

void TakeRecorder::processAudio (const juce::AudioBuffer<float>& buffer, int numInputChannels,
                                 const juce::Optional<juce::AudioPlayHead::PositionInfo>& position)
{
    auto current = state.load();
    const bool playing = position.hasValue() && position->getIsPlaying();

    if (current == State::Armed && (! waitForTransport.load() || playing))
    {
        TakeInfo ti;
        ti.synced = playing && position->getPpqPosition().hasValue();
        if (ti.synced)
            ti.startPpq = *position->getPpqPosition();
        if (position.hasValue())
        {
            if (auto bpm = position->getBpm(); bpm.hasValue() && *bpm > 0.0)
                ti.bpm = *bpm;
            if (auto sig = position->getTimeSignature(); sig.hasValue() && sig->numerator > 0)
            {
                ti.sigNumerator = sig->numerator;
                ti.sigDenominator = sig->denominator;
            }
        }
        startInfo = ti;
        state.store (State::Recording, std::memory_order_release);
        current = State::Recording;
    }

    if (current != State::Recording)
        return;

    // Stop when asked, or when FL's transport stops on a synced take.
    if (stopRequested.load() || (waitForTransport.load() && startInfo.synced && ! playing))
    {
        auto expected = State::Recording;
        state.compare_exchange_strong (expected, State::Finishing);
        return;
    }

    const int numSamples = buffer.getNumSamples();
    const int channels = juce::jmax (1, juce::jmin (numInputChannels, buffer.getNumChannels()));
    const float scale = 1.0f / (float) channels;

    int start1, size1, start2, size2;
    fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

    auto writeRange = [&] (int ringStart, int count, int sourceOffset)
    {
        for (int i = 0; i < count; ++i)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
                sum += buffer.getSample (ch, sourceOffset + i);
            ring[(size_t) (ringStart + i)] = sum * scale;
        }
    };

    writeRange (start1, size1, 0);
    writeRange (start2, size2, size1);
    fifo.finishedWrite (size1 + size2);
}

int TakeRecorder::copyTake (std::vector<doot::PitchFrame>& framesOut, TakeInfo& infoOut, double& hopSecOut) const
{
    const juce::ScopedLock sl (lock);
    framesOut = frames;
    infoOut = info;
    hopSecOut = tracker.getHopSec();
    return revision.load();
}

void TakeRecorder::run()
{
    while (! threadShouldExit())
    {
        wait (4);

        const juce::ScopedLock sl (lock);
        const auto current = state.load (std::memory_order_acquire);

        if ((current == State::Recording || current == State::Finishing) && ! infoCaptured)
        {
            info = startInfo;
            infoCaptured = true;
            revision.fetch_add (1);
        }

        bool added = false;
        while (fifo.getNumReady() > 0)
        {
            int start1, size1, start2, size2;
            const int wanted = juce::jmin (fifo.getNumReady(), (int) scratch.size());
            fifo.prepareToRead (wanted, start1, size1, start2, size2);
            std::copy_n (ring.begin() + start1, size1, scratch.begin());
            std::copy_n (ring.begin() + start2, size2, scratch.begin() + size1);
            fifo.finishedRead (size1 + size2);

            if (frames.size() < maxFrames)
                tracker.push (scratch.data(), size1 + size2, frames);
            else
                stopRequested.store (true);

            added = true;
        }

        if (added)
            revision.fetch_add (1);

        if (current == State::Finishing && fifo.getNumReady() == 0)
        {
            state.store (State::Idle);
            revision.fetch_add (1);
        }
    }
}
