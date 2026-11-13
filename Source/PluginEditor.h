#pragma once

#include "NoteView.h"
#include "PluginProcessor.h"

class DootLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DootLookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override;
};

class DootMidiEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit DootMidiEditor (DootMidiProcessor&);
    ~DootMidiEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateRecordButton();
    void saveMidi();

    DootMidiProcessor& processor;
    DootLookAndFeel lookAndFeel;

    juce::TextButton recordButton, clearButton { "Clear" }, saveButton { "Save .mid" };
    NoteView noteView;
    DragTile dragTile;

    juce::ComboBox quantizeBox;
    juce::ToggleButton velocityToggle { "Velocity from voice" };
    juce::ToggleButton syncToggle { "Start/stop with FL play" };
    juce::Slider sensitivitySlider, nudgeSlider;
    juce::Label quantizeLabel, sensitivityLabel, nudgeLabel;

    using Attach = juce::AudioProcessorValueTreeState;
    std::unique_ptr<Attach::ComboBoxAttachment> quantizeAttach;
    std::unique_ptr<Attach::ButtonAttachment> velocityAttach, syncAttach;
    std::unique_ptr<Attach::SliderAttachment> sensitivityAttach, nudgeAttach;

    std::unique_ptr<juce::FileChooser> chooser;

    juce::String status;
    juce::Colour statusColour;
    float displayLevel = 0.0f;
    float levelDb = -100.0f;
    int lastRevisionShown = -1;
    int tick = 0;
    TakeRecorder::State lastState = TakeRecorder::State::Idle;

    juce::Rectangle<int> meterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DootMidiEditor)
};
