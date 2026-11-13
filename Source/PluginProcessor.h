#pragma once

#include "TakeRecorder.h"
#include "TakeResult.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

namespace ParamIDs
{
    inline constexpr const char* quantize    = "quantize";
    inline constexpr const char* velocity    = "velocity";
    inline constexpr const char* sensitivity = "sensitivity";
    inline constexpr const char* sync        = "syncTransport";
    inline constexpr const char* nudge       = "nudge";
}

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

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Message thread ------------------------------------------------------
    void startRecording();
    void stopRecording()  { recorder.stop(); }
    void clearTake()      { recorder.clear(); }
    TakeRecorder::State getRecordState() const { return recorder.getState(); }
    bool isStandalone() const { return wrapperType == wrapperType_Standalone; }

    // Rebuilds the notes when the take or a setting changed. Cheap otherwise.
    const TakeResult& getResult();
    ResultSettings getResultSettings() const;

    // Writes the current notes to a .mid file in the temp folder, for dragging.
    juce::File writeMidiForDrag();
    bool saveMidiTo (const juce::File& file);

    float getInputPeak() const noexcept { return inputPeak.load (std::memory_order_relaxed); }
    void decayPeakForEditor (float factor) noexcept;
    double getCurrentSampleRate() const noexcept { return currentSampleRate.load (std::memory_order_relaxed); }
    bool isHostPlaying() const noexcept { return hostPlaying.load (std::memory_order_relaxed); }

    juce::AudioProcessorValueTreeState params;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    TakeRecorder recorder;

    std::atomic<float> inputPeak { 0.0f };
    std::atomic<double> currentSampleRate { 0.0 };
    std::atomic<bool> hostPlaying { false };

    // message-thread cache
    TakeResult result;
    int resultRevision = -1;
    ResultSettings resultSettings;
    int dragFileCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DootMidiProcessor)
};
