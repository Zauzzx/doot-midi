#include "NoteView.h"
#include "Theme.h"

#include <cmath>

void NoteView::setResult (const TakeResult& r, const ResultSettings& s, bool recording)
{
    result = &r;
    settings = s;
    isRecording = recording;
    repaint();
}

void NoteView::paint (juce::Graphics& g)
{
    using namespace Theme;
    auto bounds = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 10.0f);

    auto area = getLocalBounds().reduced (10);
    const int gutter = 38;
    auto roll = area.withTrimmedLeft (gutter);

    const bool hasFrames = result != nullptr && ! result->frames.empty();
    const bool hasNotes = result != nullptr && ! result->notes.empty();

    if (! hasFrames && ! hasNotes)
    {
        g.setColour (textDim);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText ("Your notes will show up here", area, juce::Justification::centred);
        return;
    }

    const auto& r = *result;

    // Time range in beats
    double endBeat = 0.0;
    for (const auto& n : r.notes)
        endBeat = std::max (endBeat, n.startBeat + n.lengthBeats);
    if (hasFrames)
        endBeat = std::max (endBeat, r.secondsToBeats (r.frames.back().timeSec, settings.nudgeMs));
    const double bar = r.beatsPerBar > 0 ? r.beatsPerBar : 4.0;
    endBeat = std::max (bar, std::ceil (endBeat / bar) * bar);
    const double startBeat = 0.0;

    // Pitch range
    float lo = 200.0f, hi = -200.0f;
    for (const auto& n : r.notes)
    {
        lo = std::min (lo, (float) n.midiNote);
        hi = std::max (hi, (float) n.midiNote);
    }
    for (const auto& f : r.frames)
    {
        if (f.f0 > 0.0f && f.clarity > 0.7f && f.levelDb > settings.gateDb)
        {
            const float m = 69.0f + 12.0f * std::log2 (f.f0 / 440.0f);
            lo = std::min (lo, m);
            hi = std::max (hi, m);
        }
    }
    if (lo > hi)
    {
        lo = 55.0f;
        hi = 67.0f;
    }
    int low = (int) std::floor (lo) - 2, high = (int) std::ceil (hi) + 2;
    if (high - low < 12)
    {
        const int mid = (low + high) / 2;
        low = mid - 6;
        high = mid + 6;
    }

    const float rowH = (float) roll.getHeight() / (float) (high - low + 1);
    auto yFor = [&] (float midi) { return (float) roll.getBottom() - (midi - (float) low + 1.0f) * rowH; };
    auto xFor = [&] (double beat) { return (float) roll.getX() + (float) ((beat - startBeat) / (endBeat - startBeat)) * (float) roll.getWidth(); };

    // Key lanes and note names
    g.setFont (juce::FontOptions (10.5f));
    for (int m = low; m <= high; ++m)
    {
        const int pc = ((m % 12) + 12) % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        const float y = yFor ((float) m);
        g.setColour (black ? laneDark : laneLight);
        g.fillRect (juce::Rectangle<float> ((float) roll.getX(), y, (float) roll.getWidth(), rowH));

        if (pc == 0 || (rowH >= 11.0f && ! black))
        {
            g.setColour (pc == 0 ? textMain : textDim);
            g.drawText (flNoteName (m), juce::Rectangle<float> ((float) area.getX(), y, (float) gutter - 6.0f, rowH),
                        juce::Justification::centredRight);
        }
    }

    // Beat and bar lines
    for (double b = startBeat; b <= endBeat + 1e-6; b += 1.0)
    {
        const bool isBar = std::fmod (b, bar) < 1e-6;
        g.setColour (isBar ? gridBar : gridBeat);
        g.drawVerticalLine ((int) std::round (xFor (b)), (float) roll.getY(), (float) roll.getBottom());
    }

    // Raw pitch trace (what the plugin heard)
    juce::Path trace;
    bool penDown = false;
    double lastT = -1.0;
    for (const auto& f : r.frames)
    {
        const bool voiced = f.f0 > 0.0f && f.clarity > 0.7f && f.levelDb > settings.gateDb;
        if (! voiced) { penDown = false; continue; }

        const float m = 69.0f + 12.0f * std::log2 (f.f0 / 440.0f);
        const float x = xFor (r.secondsToBeats (f.timeSec, settings.nudgeMs));
        const float y = yFor (m) + rowH * 0.5f;
        if (! roll.toFloat().contains (x, y)) { penDown = false; continue; }

        if (! penDown || f.timeSec - lastT > 0.03)
            trace.startNewSubPath (x, y);
        else
            trace.lineTo (x, y);
        penDown = true;
        lastT = f.timeSec;
    }
    g.setColour (traceColour);
    g.strokePath (trace, juce::PathStrokeType (1.5f));

    // Notes
    for (const auto& n : r.notes)
    {
        auto rect = juce::Rectangle<float> (xFor (n.startBeat), yFor ((float) n.midiNote),
                                            xFor (n.startBeat + n.lengthBeats) - xFor (n.startBeat), rowH)
                        .reduced (0.5f, 1.0f);
        const float alpha = settings.useVelocity ? 0.35f + 0.65f * (float) n.velocity / 127.0f : 1.0f;
        g.setColour (accent.withAlpha (alpha));
        g.fillRoundedRectangle (rect, 3.0f);
        g.setColour (accent.brighter (0.4f));
        g.drawRoundedRectangle (rect, 3.0f, 1.0f);

        if (rect.getWidth() > 26.0f && rowH >= 10.0f)
        {
            g.setColour (background);
            g.setFont (juce::FontOptions (juce::jmin (11.0f, rowH - 1.0f), juce::Font::bold));
            g.drawText (flNoteName (n.midiNote), rect.reduced (3.0f, 0.0f), juce::Justification::centredLeft);
        }
    }

    if (isRecording)
    {
        g.setColour (recordRed);
        g.fillEllipse ((float) roll.getRight() - 14.0f, (float) roll.getY() + 4.0f, 9.0f, 9.0f);
    }
}

//==============================================================================
void DragTile::setNoteCount (int count)
{
    if (count != noteCount)
    {
        noteCount = count;
        repaint();
    }
}

void DragTile::paint (juce::Graphics& g)
{
    using namespace Theme;
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const bool enabled = noteCount > 0;

    g.setColour (enabled && hover ? accent.withAlpha (0.18f) : panel);
    g.fillRoundedRectangle (b, 10.0f);

    juce::Path border;
    border.addRoundedRectangle (b, 10.0f);
    juce::Path dashed;
    const float dashes[] = { 6.0f, 4.0f };
    juce::PathStrokeType (1.5f).createDashedStroke (dashed, border, dashes, 2);
    g.setColour (enabled ? accent : textDim.withAlpha (0.5f));
    g.fillPath (dashed);

    auto text = b.reduced (12.0f, 6.0f);
    g.setColour (enabled ? textMain : textDim);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (enabled ? juce::String::fromUTF8 ("\xe2\x87\xb2  Drag into the piano roll") : juce::String ("Nothing to drag yet"),
                text.removeFromTop (text.getHeight() * 0.55f), juce::Justification::centredBottom);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (enabled ? juce::String (noteCount) + (noteCount == 1 ? " note" : " notes") + " as a MIDI clip"
                        : juce::String ("Record a take first"),
                text, juce::Justification::centredTop);
}

void DragTile::mouseDown (const juce::MouseEvent&)
{
    dragStarted = false;
}

void DragTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStarted || noteCount == 0 || e.getDistanceFromDragStart() < 4 || ! createFile)
        return;

    dragStarted = true;
    const auto file = createFile();
    if (file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}
