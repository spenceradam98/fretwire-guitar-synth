#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::PropertiesFile::Options settingsOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "Fretwire";
    o.filenameSuffix = ".settings";
    o.folderName = "Fretwire";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}

FretwireProcessor::FretwireProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Guitar In", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FRETWIRE", Params::createLayout()),
      presets (apvts),
      engine (apvts),
      fx (apvts)
{
    level = apvts.getRawParameterValue ("level");
    master = apvts.getRawParameterValue ("master");
    inGain = apvts.getRawParameterValue ("inGain");
    dryLvl = apvts.getRawParameterValue ("dryLvl");
    settings = std::make_unique<juce::PropertiesFile> (settingsOptions());
    themeIndex = settings->getIntValue ("theme", 0);
}

void FretwireProcessor::setThemeIndex (int i)
{
    themeIndex = i;
    settings->setValue ("theme", i);
    settings->saveIfNeeded();
}

bool FretwireProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo()
        || in == juce::AudioChannelSet::discreteChannels (6) || in.isDisabled();
}

void FretwireProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int maxBlock = juce::jmax (samplesPerBlock, 512);
    engine.prepare (sampleRate, maxBlock);
    fx.prepare (sampleRate, maxBlock);
    guitarBuf.setSize (SynthEngine::maxStrings, maxBlock);
    synthBuf.setSize (2, maxBlock);
    outGain.reset (sampleRate, 0.05);
    outGain.setCurrentAndTargetValue (level->load() * master->load());
    dryGain.reset (sampleRate, 0.05);
    dryGain.setCurrentAndTargetValue (dryLvl->load());
}

void FretwireProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                fx.setBpm (*bpm);

    if (guitarBuf.getNumSamples() < n) guitarBuf.setSize (SynthEngine::maxStrings, n, false, false, true);
    if (synthBuf.getNumSamples() < n) synthBuf.setSize (2, n, false, false, true);

    // ---- gather the guitar: hex = one channel per string, otherwise sum to mono
    const float inG = juce::Decibels::decibelsToGain (inGain->load());
    const int strings = numIn == 6 ? 6 : 1;
    numStrings.store (strings);
    if (numIn <= 0)
        guitarBuf.clear();
    else if (strings == 6)
        for (int s = 0; s < 6; ++s) guitarBuf.copyFromWithRamp (s, 0, buffer.getReadPointer (s), n, inG, inG);
    else
    {
        { const float g0 = numIn > 1 ? inG * 0.5f : inG; guitarBuf.copyFromWithRamp (0, 0, buffer.getReadPointer (0), n, g0, g0); }
        for (int c = 1; c < numIn; ++c) guitarBuf.addFrom (0, 0, buffer, c, 0, n, inG * 0.5f);
    }
    const float* gtr[SynthEngine::maxStrings] {};
    for (int s = 0; s < strings; ++s) gtr[s] = guitarBuf.getReadPointer (s);

    // ---- synth
    juce::AudioBuffer<float> synth (synthBuf.getArrayOfWritePointers(), 2, n);
    engine.process (synth, gtr, numIn > 0 ? strings : 0, midi);

    // ---- dry guitar (mono, centred) blended in before the FX
    dryGain.setTargetValue (dryLvl->load());
    float* L = synth.getWritePointer (0);
    float* R = synth.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        float d = 0;
        for (int s = 0; s < strings && numIn > 0; ++s) d += gtr[s][i];
        d *= dryGain.getNextValue() * (strings == 6 ? 0.5f : 1.0f);
        L[i] += d; R[i] += d;
    }

    fx.process (synth);

    // ---- output: preset level x master, safety clipper
    outGain.setTargetValue (level->load() * master->load());
    for (int c = 0; c < buffer.getNumChannels(); ++c) if (c >= 2) buffer.clear (c, 0, n);
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue();
        for (int c = 0; c < juce::jmin (2, buffer.getNumChannels()); ++c)
        {
            float x = synth.getSample (c, i) * g;
            if (! std::isfinite (x)) x = 0.0f;
            buffer.setSample (c, i, std::abs (x) < 0.8f ? x : std::copysign (0.8f + 0.2f * std::tanh ((std::abs (x) - 0.8f) / 0.2f), x));
        }
    }
}

int FretwireProcessor::getNumPrograms() { return juce::jmax (1, (int) getFactoryPresets().size()); }
int FretwireProcessor::getCurrentProgram() { return juce::jmax (0, presets.getCurrentIndex()); }
void FretwireProcessor::setCurrentProgram (int i) { if (i < (int) getFactoryPresets().size()) presets.load (i); }
const juce::String FretwireProcessor::getProgramName (int i)
{
    const auto& fp = getFactoryPresets();
    return juce::isPositiveAndBelow (i, (int) fp.size()) ? juce::String (fp[(size_t) i].name) : juce::String ("Init");
}

void FretwireProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presets.getCurrentName(), nullptr);
    state.setProperty ("theme", themeIndex, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void FretwireProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.hasType (apvts.state.getType()))
        {
            apvts.replaceState (state);
            presets.setCurrentByName (state.getProperty ("presetName").toString());
            if (state.hasProperty ("theme")) themeIndex = (int) state.getProperty ("theme");
        }
    }
}

juce::AudioProcessorEditor* FretwireProcessor::createEditor() { return new FretwireEditor (*this); }

#if ! FRETWIRE_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FretwireProcessor(); }
#endif
