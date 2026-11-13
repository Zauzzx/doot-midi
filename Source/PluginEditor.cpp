#include "PluginEditor.h"

namespace
{
    const juce::Colour background { 0xff15171c };
    const juce::Colour panel      { 0xff1f232b };
    const juce::Colour accent     { 0xffffb347 };
    const juce::Colour textMain   { 0xffeceff4 };
    const juce::Colour textDim    { 0xff8a93a3 };
    const juce::Colour good       { 0xff6fd08c };

    constexpr float floorDb = -60.0f;
    constexpr float signalThresholdDb = -50.0f;
}

DootMidiEditor::DootMidiEditor (DootMidiProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (460, 260);
    startTimerHz (30);
}

void DootMidiEditor::timerCallback()
{
    const float peak = processor.getInputPeak();
    processor.decayPeakForEditor (0.0f);

    levelDb = juce::Decibels::gainToDecibels (peak, -100.0f);
    const float target = juce::jlimit (0.0f, 1.0f, (levelDb - floorDb) / -floorDb);

    // Fast attack, slow release so the meter is easy to read.
    displayLevel = target > displayLevel ? target : displayLevel * 0.9f + target * 0.1f;

    if (levelDb > signalThresholdDb)
        framesSinceSignal = 0;
    else if (framesSinceSignal < 1000)
        ++framesSinceSignal;

    repaint();
}

void DootMidiEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    auto area = getLocalBounds().reduced (20);

    // Header
    auto header = area.removeFromTop (40);
    g.setColour (accent);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("Doot MIDI", header, juce::Justification::centredLeft);

    g.setColour (textDim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("v" DOOT_VERSION_STRING " test build", header, juce::Justification::centredRight);

    area.removeFromTop (16);

    // Meter panel
    auto meterPanel = area.removeFromTop (110);
    g.setColour (panel);
    g.fillRoundedRectangle (meterPanel.toFloat(), 10.0f);

    auto inner = meterPanel.reduced (16);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("MIC INPUT", inner.removeFromTop (18), juce::Justification::centredLeft);

    inner.removeFromTop (8);
    auto bar = inner.removeFromTop (22).toFloat();
    g.setColour (background);
    g.fillRoundedRectangle (bar, 6.0f);

    auto filled = bar.withWidth (bar.getWidth() * displayLevel);
    g.setColour (levelDb > -3.0f ? juce::Colours::orangered : accent);
    g.fillRoundedRectangle (filled, 6.0f);

    inner.removeFromTop (10);
    const bool hearing = framesSinceSignal < 45; // about 1.5 seconds
    g.setColour (hearing ? good : textDim);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText (hearing ? "Hearing you!" : "Waiting for sound... try singing a doot",
                inner, juce::Justification::centredLeft);

    // Footer
    area.removeFromTop (14);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (12.0f));
    const auto sr = processor.getCurrentSampleRate();
    const juce::String info = sr > 0.0 ? "Sample rate: " + juce::String (sr, 0) + " Hz"
                                       : juce::String ("Audio not started yet");
    g.drawText (info + "   |   Note detection arrives in the next build",
                area.removeFromTop (20), juce::Justification::centredLeft);
}
