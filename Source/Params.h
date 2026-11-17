#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// All parameter IDs, choice lists and the parameter layout live here so the
// engine, editor and preset bank agree on names.
namespace Params
{
    // Oscillator waves. "Shape" means something different per wave (see DESIGN.md).
    inline const juce::StringArray oscWaves {
        "Sine", "Triangle", "Saw", "Pulse",
        "WT Analog", "WT Digital", "WT Vocal", "WT Organ",
        "FM 1:1", "FM 2:1", "FM Bell", "Pluck", "Noise" };
    enum Wave { wSine, wTri, wSaw, wPulse, wWtAnalog, wWtDigital, wWtVocal, wWtOrgan,
                wFm11, wFm21, wFmBell, wPluck, wNoise, numWaves };

    inline const juce::StringArray filtModes  { "LP 24", "LP 12", "Band", "High", "Notch", "Vowel" };
    inline const juce::StringArray lfoWaves   { "Sine", "Triangle", "Saw", "Square", "S&H", "Drift" };
    inline const juce::StringArray voiceModes { "Poly", "Mono", "Legato" };
    inline const juce::StringArray subWaves   { "Sine -1", "Square -1", "Sine -2", "Square -2" };
    inline const juce::StringArray trkRanges  { "Guitar", "Bass" };
    inline const juce::StringArray trkModes   { "Retrigger", "Legato" };
    inline const juce::StringArray trkChords  { "Off", "Oct Up", "Oct Down", "Fifth", "Power", "Octaves",
                                                "Major", "Minor", "Sus4", "Maj7", "Min7", "Fourths" };
    inline const juce::StringArray driveTypes { "Warm", "Fuzz", "Crunch", "Bitcrush" };
    inline const juce::StringArray chorusModes{ "Chorus", "Flanger", "Ensemble" };
    inline const juce::StringArray tremShapes { "Sine", "Square", "Ramp" };
    inline const juce::StringArray syncDivs   { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
                                                "1/4T", "1/4", "1/4D", "1/2", "1/2D", "1/1" };
    inline const float syncBeats[] = { 0.125f, 1.0f / 6.0f, 0.25f, 0.375f, 1.0f / 3.0f, 0.5f, 0.75f,
                                       2.0f / 3.0f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f };

    // Parameters that describe the player's rig rather than the sound. Loading a
    // preset leaves these alone so a guitarist's input setup survives preset browsing.
    inline const juce::StringArray rigParams { "inGain", "trkRange", "trkSens", "master" };

    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
    using NR = juce::NormalisableRange<float>;

    inline NR skewed (float lo, float hi, float centre)
    {
        NR r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    inline Layout createLayout()
    {
        Layout layout;
        auto pid = [] (const juce::String& id) { return juce::ParameterID { id, 1 }; };
        auto addF = [&] (const juce::String& id, const juce::String& name, NR range, float def, const char* suffix = "")
        {
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid (id), name, range, def,
                juce::AudioParameterFloatAttributes().withLabel (suffix)));
        };
        auto addI = [&] (const juce::String& id, const juce::String& name, int lo, int hi, int def)
        { layout.add (std::make_unique<juce::AudioParameterInt> (pid (id), name, lo, hi, def)); };
        auto addC = [&] (const juce::String& id, const juce::String& name, const juce::StringArray& items, int def)
        { layout.add (std::make_unique<juce::AudioParameterChoice> (pid (id), name, items, def)); };
        auto addB = [&] (const juce::String& id, const juce::String& name, bool def)
        { layout.add (std::make_unique<juce::AudioParameterBool> (pid (id), name, def)); };

        // ---- Guitar input and pitch tracking
        addF ("inGain",    "Input Gain", NR (-12.0f, 24.0f), 0.0f, "dB");
        addC ("trkRange",  "Tracking Range", trkRanges, 0);
        addF ("trkSens",   "Tracking Threshold", NR (-72.0f, -24.0f), -54.0f, "dB");
        addB ("trkOn",     "Guitar Tracking", true);
        addI ("trkOct",    "Tracking Octave", -2, 2, 0);
        addI ("trkSemi",   "Tracking Transpose", -12, 12, 0);
        addF ("trkQuant",  "Pitch Snap", NR (0.0f, 1.0f), 0.6f);
        addC ("trkMode",   "Tracking Mode", trkModes, 0);
        addC ("trkChord",  "Harmony", trkChords, 0);
        addF ("trkDyn",    "Pick Dynamics", NR (0.0f, 1.0f), 0.7f);
        addF ("ampFollow", "Amp Follows Guitar", NR (0.0f, 1.0f), 0.5f);
        addF ("envCut",    "Guitar Env > Cutoff", NR (-1.0f, 1.0f), 0.0f);
        addF ("dryLvl",    "Dry Guitar", NR (0.0f, 1.0f), 0.0f);
        addF ("synLvl",    "Synth Level", NR (0.0f, 1.0f), 1.0f);

        // ---- Oscillators
        for (auto o : { "A", "B" })
        {
            const juce::String p = juce::String ("osc") + o, n = juce::String ("Osc ") + o + " ";
            const bool a = juce::String (o) == "A";
            addC (p + "Wave",   n + "Wave", oscWaves, 2);
            addI (p + "Oct",    n + "Octave", -3, 3, 0);
            addI (p + "Semi",   n + "Semi", -12, 12, 0);
            addF (p + "Fine",   n + "Fine", NR (-100.0f, 100.0f), 0.0f, "ct");
            addF (p + "Shape",  n + "Shape", NR (0.0f, 1.0f), 0.0f);
            addF (p + "Level",  n + "Level", NR (0.0f, 1.0f), a ? 0.8f : 0.0f);
            addI (p + "Unison", n + "Unison", 1, 5, 1);
            addF (p + "Detune", n + "Detune", NR (0.0f, 1.0f), 0.2f);
        }

        // ---- Mixer (synth sources + direct guitar sources)
        addF ("subLevel",   "Sub Level", NR (0.0f, 1.0f), 0.0f);
        addC ("subWave",    "Sub Wave", subWaves, 0);
        addF ("noiseLevel", "Noise Level", NR (0.0f, 1.0f), 0.0f);
        addF ("ringLevel",  "Ring Mod", NR (0.0f, 1.0f), 0.0f);
        addF ("xmod",       "Cross Mod B>A", NR (0.0f, 1.0f), 0.0f);
        addF ("gtrClean",   "Guitar Direct", NR (0.0f, 1.0f), 0.0f);
        addF ("gtrFuzz",    "Guitar Square", NR (0.0f, 1.0f), 0.0f);
        addF ("gtrOct",     "Guitar Octave Down", NR (0.0f, 1.0f), 0.0f);

        // ---- Filter
        addC ("filtMode",  "Filter Mode", filtModes, 0);
        addF ("cutoff",    "Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 12000.0f, "Hz");
        addF ("reso",      "Resonance", NR (0.0f, 1.0f), 0.1f);
        addF ("filtDrive", "Filter Drive", NR (0.0f, 1.0f), 0.0f);
        addF ("filtEnv",   "Filter Env Amt", NR (-1.0f, 1.0f), 0.0f);
        addF ("keyTrack",  "Key Track", NR (0.0f, 1.0f), 0.3f);
        addF ("filtVel",   "Filter Velocity", NR (0.0f, 1.0f), 0.0f);
        addF ("shpEnv",    "Filter Env > Shape", NR (-1.0f, 1.0f), 0.0f);

        // ---- Envelopes
        auto timeRange = skewed (0.001f, 10.0f, 0.5f);
        addF ("fA", "Filter Attack",  timeRange, 0.005f, "s");
        addF ("fD", "Filter Decay",   timeRange, 0.4f, "s");
        addF ("fS", "Filter Sustain", NR (0.0f, 1.0f), 0.5f);
        addF ("fR", "Filter Release", timeRange, 0.4f, "s");
        addF ("aA", "Amp Attack",  timeRange, 0.003f, "s");
        addF ("aD", "Amp Decay",   timeRange, 0.3f, "s");
        addF ("aS", "Amp Sustain", NR (0.0f, 1.0f), 0.8f);
        addF ("aR", "Amp Release", timeRange, 0.25f, "s");
        addF ("ampVel", "Amp Velocity", NR (0.0f, 1.0f), 0.5f);

        // ---- LFOs
        addC ("lfo1Wave",  "LFO1 Wave", lfoWaves, 0);
        addF ("lfo1Rate",  "LFO1 Rate", skewed (0.02f, 20.0f, 2.0f), 5.0f, "Hz");
        addF ("lfo1Delay", "LFO1 Fade In", NR (0.0f, 3.0f), 0.0f, "s");
        addF ("lfo1Pitch", "LFO1 > Pitch", NR (0.0f, 1.0f), 0.0f);
        addF ("lfo1Cutoff","LFO1 > Cutoff", NR (0.0f, 1.0f), 0.0f);
        addC ("lfo2Wave",  "LFO2 Wave", lfoWaves, 1);
        addF ("lfo2Rate",  "LFO2 Rate", skewed (0.02f, 20.0f, 2.0f), 0.3f, "Hz");
        addF ("lfo2Shape", "LFO2 > Shape", NR (0.0f, 1.0f), 0.0f);
        addF ("lfo2Pan",   "LFO2 > Pan", NR (0.0f, 1.0f), 0.0f);
        addF ("lfo2Amp",   "LFO2 > Amp", NR (0.0f, 1.0f), 0.0f);

        // ---- Voice
        addC ("voiceMode", "Voice Mode", voiceModes, 0);
        addF ("glide",     "Glide", skewed (0.0f, 2.0f, 0.3f), 0.0f, "s");
        addF ("drift",     "Analog Drift", NR (0.0f, 1.0f), 0.1f);
        addF ("spread",    "Stereo Spread", NR (0.0f, 1.0f), 0.5f);
        addI ("bendRange", "Bend Range", 0, 12, 2);
        addF ("level",     "Preset Level", skewed (0.0f, 8.0f, 0.7f), 0.7f);
        addF ("master",    "Master", skewed (0.0f, 2.0f, 1.0f), 1.0f);

        // ---- FX: Drive
        addB ("drvOn",  "Drive On", false);
        addC ("drvType","Drive Type", driveTypes, 0);
        addF ("drvAmt", "Drive Amount", NR (0.0f, 1.0f), 0.3f);
        addF ("drvTone","Drive Tone", NR (0.0f, 1.0f), 0.6f);
        addF ("drvMix", "Drive Mix", NR (0.0f, 1.0f), 1.0f);
        // Phaser
        addB ("phsOn",   "Phaser On", false);
        addF ("phsRate", "Phaser Rate", skewed (0.02f, 10.0f, 0.8f), 0.4f, "Hz");
        addF ("phsDepth","Phaser Depth", NR (0.0f, 1.0f), 0.7f);
        addF ("phsFb",   "Phaser Feedback", NR (0.0f, 0.95f), 0.5f);
        addF ("phsMix",  "Phaser Mix", NR (0.0f, 1.0f), 0.5f);
        // Chorus
        addB ("choOn",   "Chorus On", false);
        addC ("choMode", "Chorus Mode", chorusModes, 0);
        addF ("choRate", "Chorus Rate", skewed (0.02f, 10.0f, 0.8f), 0.5f, "Hz");
        addF ("choDepth","Chorus Depth", NR (0.0f, 1.0f), 0.5f);
        addF ("choFb",   "Chorus Feedback", NR (0.0f, 0.95f), 0.0f);
        addF ("choMix",  "Chorus Mix", NR (0.0f, 1.0f), 0.5f);
        // Tremolo
        addB ("trmOn",   "Tremolo On", false);
        addC ("trmShape","Tremolo Shape", tremShapes, 0);
        addF ("trmRate", "Tremolo Rate", skewed (0.1f, 20.0f, 4.0f), 5.0f, "Hz");
        addF ("trmDepth","Tremolo Depth", NR (0.0f, 1.0f), 0.5f);
        addB ("trmPan",  "Tremolo Auto-Pan", false);
        // Delay
        addB ("dlyOn",   "Delay On", false);
        addB ("dlySync", "Delay Sync", true);
        addC ("dlyDiv",  "Delay Division", syncDivs, 6);
        addF ("dlyMs",   "Delay Time", skewed (10.0f, 2000.0f, 350.0f), 375.0f, "ms");
        addF ("dlyFb",   "Delay Feedback", NR (0.0f, 1.0f), 0.35f);
        addF ("dlyTone", "Delay Tone", NR (0.0f, 1.0f), 0.6f);
        addB ("dlyPing", "Delay Ping-Pong", false);
        addF ("dlyMix",  "Delay Mix", NR (0.0f, 1.0f), 0.25f);
        // Reverb
        addB ("revOn",   "Reverb On", false);
        addF ("revSize", "Reverb Size", NR (0.0f, 1.0f), 0.5f);
        addF ("revDamp", "Reverb Damping", NR (0.0f, 1.0f), 0.5f);
        addF ("revWidth","Reverb Width", NR (0.0f, 1.0f), 1.0f);
        addF ("revMix",  "Reverb Mix", NR (0.0f, 1.0f), 0.25f);

        return layout;
    }
}
