#include "PluginProcessor.h"
#include "PluginEditor.h"

DootMidiProcessor::DootMidiProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "DootMidi", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout DootMidiProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::quantize, 1 }, "Quantize",
                                                        getQuantizeNames(), 2));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::velocity, 1 }, "Velocity from voice", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::sensitivity, 1 }, "Sensitivity",
                                                       NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::sync, 1 }, "Start with FL play", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::nudge, 1 }, "Timing nudge",
                                                       NormalisableRange<float> (-150.0f, 150.0f, 1.0f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("ms")));
    return layout;
}

void DootMidiProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate.store (sampleRate);
    recorder.setSampleRate (sampleRate);
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

    float previous = inputPeak.load (std::memory_order_relaxed);
    while (peak > previous && ! inputPeak.compare_exchange_weak (previous, peak, std::memory_order_relaxed)) {}

    juce::Optional<juce::AudioPlayHead::PositionInfo> position;
    if (auto* head = getPlayHead())
        position = head->getPosition();
    hostPlaying.store (position.hasValue() && position->getIsPlaying(), std::memory_order_relaxed);

    recorder.processAudio (buffer, numIn, position);

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

void DootMidiProcessor::startRecording()
{
    const bool wait = ! isStandalone() && params.getRawParameterValue (ParamIDs::sync)->load() > 0.5f;
    recorder.arm (wait);
}

ResultSettings DootMidiProcessor::getResultSettings() const
{
    ResultSettings s;
    s.gateDb = -20.0f - 0.5f * params.getRawParameterValue (ParamIDs::sensitivity)->load();
    s.quantizeIndex = (int) params.getRawParameterValue (ParamIDs::quantize)->load();
    s.useVelocity = params.getRawParameterValue (ParamIDs::velocity)->load() > 0.5f;
    s.nudgeMs = params.getRawParameterValue (ParamIDs::nudge)->load();
    return s;
}

const TakeResult& DootMidiProcessor::getResult()
{
    const auto settings = getResultSettings();
    const bool settingsChanged = settings.gateDb != resultSettings.gateDb
                              || settings.quantizeIndex != resultSettings.quantizeIndex
                              || settings.useVelocity != resultSettings.useVelocity
                              || settings.nudgeMs != resultSettings.nudgeMs;

    if (recorder.getRevision() != resultRevision || settingsChanged)
    {
        std::vector<doot::PitchFrame> frames;
        TakeRecorder::TakeInfo info;
        double hopSec = 0.005;
        resultRevision = recorder.copyTake (frames, info, hopSec);
        resultSettings = settings;
        result = buildResult (std::move (frames), info, hopSec, settings);
    }
    return result;
}

bool DootMidiProcessor::saveMidiTo (const juce::File& file)
{
    const auto& r = getResult();
    if (r.notes.empty())
        return false;

    auto midiFile = makeMidiFile (r);
    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk())
        return false;
    return midiFile.writeTo (out, 1);
}

juce::File DootMidiProcessor::writeMidiForDrag()
{
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Doot MIDI");
    folder.createDirectory();

    // A fresh name each time, so FL never reuses an older drag.
    auto file = folder.getChildFile ("Doot take " + juce::Time::getCurrentTime().formatted ("%H-%M-%S")
                                     + "-" + juce::String (++dragFileCounter) + ".mid");
    return saveMidiTo (file) ? file : juce::File();
}

void DootMidiProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = params.copyState();
    auto xml = state.createXml();

    // Keep the last take with the FL project, so it's still there next time.
    std::vector<doot::PitchFrame> frames;
    TakeRecorder::TakeInfo info;
    double hopSec = 0.005;
    recorder.copyTake (frames, info, hopSec);

    if (! frames.empty() && recorder.getState() == TakeRecorder::State::Idle)
    {
        juce::MemoryOutputStream data;
        for (const auto& f : frames)
        {
            data.writeDouble (f.timeSec);
            data.writeFloat (f.f0);
            data.writeFloat (f.clarity);
            data.writeFloat (f.levelDb);
        }

        auto* take = xml->createNewChildElement ("Take");
        take->setAttribute ("synced", info.synced);
        take->setAttribute ("startPpq", info.startPpq);
        take->setAttribute ("bpm", info.bpm);
        take->setAttribute ("sigNum", info.sigNumerator);
        take->setAttribute ("sigDen", info.sigDenominator);
        take->setAttribute ("sampleRate", getCurrentSampleRate());
        take->addTextElement (data.getMemoryBlock().toBase64Encoding());
    }

    copyXmlToBinary (*xml, destData);
}

void DootMidiProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    if (auto* take = xml->getChildByName ("Take"))
    {
        TakeRecorder::TakeInfo info;
        info.synced = take->getBoolAttribute ("synced");
        info.startPpq = take->getDoubleAttribute ("startPpq");
        info.bpm = take->getDoubleAttribute ("bpm", 120.0);
        info.sigNumerator = take->getIntAttribute ("sigNum", 4);
        info.sigDenominator = take->getIntAttribute ("sigDen", 4);

        juce::MemoryBlock block;
        if (block.fromBase64Encoding (take->getAllSubText().trim()))
        {
            std::vector<doot::PitchFrame> frames;
            juce::MemoryInputStream in (block, false);
            while (in.getNumBytesRemaining() >= 20)
            {
                doot::PitchFrame f;
                f.timeSec = in.readDouble();
                f.f0 = in.readFloat();
                f.clarity = in.readFloat();
                f.levelDb = in.readFloat();
                frames.push_back (f);
            }
            recorder.restoreTake (std::move (frames), info, take->getDoubleAttribute ("sampleRate", 0.0));
        }
        xml->removeChildElement (take, true);
    }

    if (xml->hasTagName (params.state.getType()))
        params.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* DootMidiProcessor::createEditor()
{
    return new DootMidiEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DootMidiProcessor();
}
