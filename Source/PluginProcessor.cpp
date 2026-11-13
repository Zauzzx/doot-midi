#include "PluginProcessor.h"
#include "PluginEditor.h"

DootMidiProcessor::DootMidiProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void DootMidiProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate.store (sampleRate);
    inputPeak.store (0.0f);
}

bool DootMidiProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;

    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void DootMidiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    float peak = 0.0f;
    for (int ch = 0; ch < numIn; ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, numSamples));

    // Keep the loudest value until the editor reads and decays it.
    float previous = inputPeak.load (std::memory_order_relaxed);
    while (peak > previous && ! inputPeak.compare_exchange_weak (previous, peak, std::memory_order_relaxed)) {}

    // Mono in, stereo out: copy the voice to both sides.
    if (numIn == 1 && numOut > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    for (int ch = numIn; ch < numOut; ++ch)
        if (! (numIn == 1 && ch == 1))
            buffer.clear (ch, 0, numSamples);

    midi.clear();
}

void DootMidiProcessor::decayPeakForEditor (float factor) noexcept
{
    inputPeak.store (inputPeak.load (std::memory_order_relaxed) * factor, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* DootMidiProcessor::createEditor()
{
    return new DootMidiEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DootMidiProcessor();
}
