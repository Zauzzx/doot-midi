#pragma once

#include "PluginProcessor.h"

class DootMidiEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit DootMidiEditor (DootMidiProcessor&);
    ~DootMidiEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    void timerCallback() override;

    DootMidiProcessor& processor;
    float displayLevel = 0.0f;   // smoothed 0..1 for drawing
    float levelDb = -100.0f;
    int framesSinceSignal = 1000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DootMidiEditor)
};
