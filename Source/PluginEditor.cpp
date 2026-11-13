#include "PluginEditor.h"
#include "Theme.h"

DootLookAndFeel::DootLookAndFeel()
{
    using namespace Theme;
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::TextButton::buttonColourId, panelLight);
    setColour (juce::TextButton::buttonOnColourId, recordRed);
    setColour (juce::TextButton::textColourOffId, textMain);
    setColour (juce::TextButton::textColourOnId, textMain);
    setColour (juce::ComboBox::backgroundColourId, panelLight);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::textColourId, textMain);
    setColour (juce::ComboBox::arrowColourId, accent);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.3f));
    setColour (juce::PopupMenu::textColourId, textMain);
    setColour (juce::ToggleButton::textColourId, textMain);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::ToggleButton::tickDisabledColourId, textDim);
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::backgroundColourId, panelLight);
    setColour (juce::Slider::thumbColourId, textMain);
    setColour (juce::Slider::textBoxTextColourId, textMain);
    setColour (juce::Slider::textBoxBackgroundColourId, panelLight);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, textDim);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
}

void DootLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg,
                                            bool highlighted, bool down)
{
    auto c = bg;
    if (down) c = c.darker (0.2f);
    else if (highlighted) c = c.brighter (0.12f);
    if (! b.isEnabled()) c = c.withAlpha (0.4f);
    g.setColour (c);
    g.fillRoundedRectangle (b.getLocalBounds().toFloat().reduced (1.0f), 8.0f);
}

//==============================================================================
DootMidiEditor::DootMidiEditor (DootMidiProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    recordButton.onClick = [this]
    {
        if (processor.getRecordState() == TakeRecorder::State::Idle)
            processor.startRecording();
        else
            processor.stopRecording();
        updateRecordButton();
    };
    addAndMakeVisible (recordButton);

    clearButton.onClick = [this] { processor.clearTake(); };
    addAndMakeVisible (clearButton);

    saveButton.onClick = [this] { saveMidi(); };
    addAndMakeVisible (saveButton);

    addAndMakeVisible (noteView);

    dragTile.createFile = [this] { return processor.writeMidiForDrag(); };
    addAndMakeVisible (dragTile);

    quantizeBox.addItemList (getQuantizeNames(), 1);
    addAndMakeVisible (quantizeBox);
    quantizeAttach = std::make_unique<Attach::ComboBoxAttachment> (processor.params, ParamIDs::quantize, quantizeBox);

    addAndMakeVisible (velocityToggle);
    velocityAttach = std::make_unique<Attach::ButtonAttachment> (processor.params, ParamIDs::velocity, velocityToggle);

    syncToggle.setEnabled (! processor.isStandalone());
    addAndMakeVisible (syncToggle);
    syncAttach = std::make_unique<Attach::ButtonAttachment> (processor.params, ParamIDs::sync, syncToggle);

    for (auto* s : { &sensitivitySlider, &nudgeSlider })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 22);
        addAndMakeVisible (*s);
    }
    sensitivityAttach = std::make_unique<Attach::SliderAttachment> (processor.params, ParamIDs::sensitivity, sensitivitySlider);
    nudgeAttach = std::make_unique<Attach::SliderAttachment> (processor.params, ParamIDs::nudge, nudgeSlider);
    sensitivitySlider.setTextValueSuffix (" %");
    sensitivitySlider.setDoubleClickReturnValue (true, 50.0);
    nudgeSlider.setDoubleClickReturnValue (true, 0.0);
    nudgeSlider.setTextValueSuffix (" ms");
    sensitivitySlider.setTooltip ("Higher picks up quieter singing. Lower ignores background noise.");
    nudgeSlider.setTooltip ("Shifts every note earlier (negative) or later (positive).");

    for (auto [label, text] : { std::pair { &quantizeLabel, "Snap to grid" },
                                std::pair { &sensitivityLabel, "Sensitivity" },
                                std::pair { &nudgeLabel, "Timing nudge" } })
    {
        label->setText (text, juce::dontSendNotification);
        label->setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (*label);
    }

    setSize (820, 540);
    updateRecordButton();
    startTimerHz (30);
}

DootMidiEditor::~DootMidiEditor()
{
    setLookAndFeel (nullptr);
}

void DootMidiEditor::updateRecordButton()
{
    using S = TakeRecorder::State;
    const auto state = processor.getRecordState();

    switch (state)
    {
        case S::Idle:
            recordButton.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x8f  Record"));
            recordButton.setColour (juce::TextButton::buttonColourId, Theme::recordRed.withAlpha (0.85f));
            break;
        case S::Armed:
            recordButton.setButtonText ("Cancel");
            recordButton.setColour (juce::TextButton::buttonColourId, Theme::panelLight);
            break;
        case S::Recording:
        case S::Finishing:
            recordButton.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xa0  Stop"));
            recordButton.setColour (juce::TextButton::buttonColourId, Theme::recordRed);
            break;
    }

    clearButton.setEnabled (state == S::Idle);
}

void DootMidiEditor::saveMidi()
{
    auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Doot take.mid");
    chooser = std::make_unique<juce::FileChooser> ("Save MIDI clip", start, "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file == juce::File())
                                  return;
                              if (! file.hasFileExtension ("mid"))
                                  file = file.withFileExtension ("mid");
                              if (! processor.saveMidiTo (file))
                              {
                                  status = "Couldn't save the file. Try another folder.";
                                  statusColour = Theme::recordRed;
                              }
                          });
}

void DootMidiEditor::timerCallback()
{
    using S = TakeRecorder::State;

    // Mic meter
    const float peak = processor.getInputPeak();
    processor.decayPeakForEditor (0.0f);
    levelDb = juce::Decibels::gainToDecibels (peak, -100.0f);
    const float target = juce::jlimit (0.0f, 1.0f, (levelDb + 60.0f) / 60.0f);
    displayLevel = target > displayLevel ? target : displayLevel * 0.85f + target * 0.15f;
    repaint (meterArea);

    const auto state = processor.getRecordState();
    if (state != lastState)
    {
        lastState = state;
        updateRecordButton();
    }

    // Rebuild the notes ~10 times a second while recording, instantly otherwise.
    const bool recording = state == S::Recording || state == S::Finishing;
    if (! recording || ++tick % 3 == 0)
    {
        const auto& result = processor.getResult();
        noteView.setResult (result, processor.getResultSettings(), recording);
        dragTile.setNoteCount (state == S::Idle ? (int) result.notes.size() : 0);
        saveButton.setEnabled (state == S::Idle && ! result.notes.empty());

        juce::String newStatus;
        juce::Colour colour = Theme::textDim;
        const bool waits = ! processor.isStandalone()
                        && processor.params.getRawParameterValue (ParamIDs::sync)->load() > 0.5f;

        if (state == S::Armed)
        {
            newStatus = "Ready. Press Play in FL Studio (Space) and start singing.";
            colour = Theme::accent;
        }
        else if (recording)
        {
            newStatus = waits ? "Recording. Stop FL (or click Stop) when you're done."
                              : "Recording. Click Stop when you're done.";
            colour = Theme::recordRed;
        }
        else if (! result.notes.empty())
        {
            newStatus = juce::String (result.notes.size()) + " notes found. Drag them into the piano roll.";
            colour = Theme::good;
        }
        else if (! result.frames.empty())
        {
            newStatus = "No notes found. Try turning Sensitivity up.";
            colour = Theme::accent;
        }
        else
        {
            newStatus = waits ? "Press Record, then press Play in FL Studio and sing."
                              : "Press Record and sing.";
        }

        if (newStatus != status && ! status.startsWith ("Couldn't"))
        {
            status = newStatus;
            statusColour = colour;
            repaint();
        }
        else if (status.startsWith ("Couldn't") && recording)
        {
            status = newStatus;
            statusColour = colour;
        }
    }
}

void DootMidiEditor::paint (juce::Graphics& g)
{
    using namespace Theme;
    g.fillAll (background);

    auto area = getLocalBounds().reduced (18);
    auto header = area.removeFromTop (36);

    g.setColour (accent);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("Doot MIDI", header.removeFromLeft (160), juce::Justification::centredLeft);

    g.setColour (textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("v" DOOT_VERSION_STRING, header.removeFromLeft (60), juce::Justification::centredLeft);

    // Mic meter in the header
    auto m = meterArea.toFloat();
    g.setColour (textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("MIC", m.removeFromLeft (30.0f), juce::Justification::centredLeft);
    auto bar = m.withSizeKeepingCentre (m.getWidth(), 8.0f);
    g.setColour (panelLight);
    g.fillRoundedRectangle (bar, 4.0f);
    g.setColour (levelDb > -3.0f ? recordRed : good);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * displayLevel), 4.0f);

    // Status line under the record row
    g.setColour (statusColour);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (status, recordButton.getBounds().withX (clearButton.getRight() + 14)
                                                .withRight (getWidth() - 18),
                juce::Justification::centredLeft);
}

void DootMidiEditor::resized()
{
    auto area = getLocalBounds().reduced (18);
    auto header = area.removeFromTop (36);
    meterArea = header.removeFromRight (220).reduced (0, 8);

    area.removeFromTop (10);
    auto row = area.removeFromTop (40);
    recordButton.setBounds (row.removeFromLeft (130));
    row.removeFromLeft (8);
    clearButton.setBounds (row.removeFromLeft (80));

    area.removeFromTop (12);

    // Bottom: controls
    auto controls = area.removeFromBottom (92);
    area.removeFromBottom (12);

    // Middle: preview + drag tile
    auto dragRow = area.removeFromBottom (64);
    area.removeFromBottom (12);
    noteView.setBounds (area);

    saveButton.setBounds (dragRow.removeFromRight (110).reduced (0, 12));
    dragRow.removeFromRight (10);
    dragTile.setBounds (dragRow);

    auto left = controls.removeFromLeft (controls.getWidth() / 2 - 10);
    controls.removeFromLeft (20);
    auto right = controls;

    auto q = left.removeFromTop (44);
    quantizeLabel.setBounds (q.removeFromTop (18));
    quantizeBox.setBounds (q.removeFromLeft (120).reduced (0, 1));
    q.removeFromLeft (14);
    velocityToggle.setBounds (q);

    left.removeFromTop (6);
    syncToggle.setBounds (left.removeFromTop (28));

    auto s = right.removeFromTop (44);
    sensitivityLabel.setBounds (s.removeFromTop (18));
    sensitivitySlider.setBounds (s);
    auto n = right.removeFromTop (44);
    nudgeLabel.setBounds (n.removeFromTop (18));
    nudgeSlider.setBounds (n);
}
