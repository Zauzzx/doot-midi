#pragma once

#include "TakeResult.h"
#include <juce_gui_basics/juce_gui_basics.h>

// A small piano-roll preview of the detected notes, with the raw pitch trace.
class NoteView : public juce::Component
{
public:
    void setResult (const TakeResult& r, const ResultSettings& s, bool recording);
    void paint (juce::Graphics&) override;

private:
    const TakeResult* result = nullptr;
    ResultSettings settings;
    bool isRecording = false;
};

// The tile you drag from to drop a MIDI clip into FL Studio.
class DragTile : public juce::Component
{
public:
    std::function<juce::File()> createFile;
    void setNoteCount (int count);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

private:
    int noteCount = 0;
    bool hover = false;
    bool dragStarted = false;
};
