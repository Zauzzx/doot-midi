#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

// Step 1 "hello world": passes audio through and measures the input level,
// so we can confirm FL Studio loads the plugin and that it hears the mic.
class DootMidiProcessor : public juce::AudioProcessor
{
public:
    DootMidiProcessor();
    ~DootMidiProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    // Read by the editor on the message thread.
    float getInputPeak() const noexcept { return inputPeak.load (std::memory_order_relaxed); }
    void decayPeakForEditor (float factor) noexcept;
    double getCurrentSampleRate() const noexcept { return currentSampleRate.load (std::memory_order_relaxed); }

private:
    std::atomic<float> inputPeak { 0.0f };
    std::atomic<double> currentSampleRate { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DootMidiProcessor)
};
