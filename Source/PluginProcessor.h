#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Params.h"
#include "SynthEngine.h"
#include "Effects.h"
#include "PresetManager.h"

// Guitar synthesiser. Audio in = guitar (mono, stereo summed, or 6 discrete
// channels from a hex pickup = one tracker per string). MIDI in plays the
// same synth from a keyboard. Out = synth + optional dry guitar through the FX rack.
class FretwireProcessor : public juce::AudioProcessor
{
public:
    FretwireProcessor();
    ~FretwireProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Fretwire"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;
    juce::MidiKeyboardState keyboardState;
    SynthEngine engine;

    int getThemeIndex() const { return themeIndex; }
    void setThemeIndex (int i);
    int getNumStrings() const { return numStrings.load(); }

private:
    FxRack fx;
    std::atomic<float> *level = nullptr, *master = nullptr, *inGain = nullptr, *dryLvl = nullptr;
    juce::SmoothedValue<float> outGain, dryGain;
    juce::AudioBuffer<float> guitarBuf, synthBuf;
    std::atomic<int> numStrings { 1 };
    int themeIndex = 0;
    std::unique_ptr<juce::PropertiesFile> settings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FretwireProcessor)
};
