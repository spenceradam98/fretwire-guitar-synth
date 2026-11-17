#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP.h"
#include "PitchTracker.h"
#include "Params.h"

// ============================================================================
//  Voice engine. Notes come from two places:
//    - MIDI (keyboard play: Poly / Mono / Legato)
//    - the guitar pitch tracker(s): one tracker for a normal mono/stereo input,
//      six (one per string) when the input bus is 6 discrete channels.
//  A tracked note owns 1-4 voices (root + Harmony intervals). Tracked voices
//  also get the raw guitar signal for the "direct" sources (Guitar Direct /
//  Square / Octave Down) and the pick-dynamics follower.
// ============================================================================

struct EngineParams
{
    explicit EngineParams (juce::AudioProcessorValueTreeState&);
    std::atomic<float>* get (const char* id);
    juce::AudioProcessorValueTreeState& s;

    // cached raw pointers
    std::atomic<float> *trkOn, *trkRange, *trkSens, *trkOct, *trkSemi, *trkQuant, *trkMode, *trkChord, *trkDyn,
        *ampFollow, *envCut, *synLvl, *inGain;
    std::atomic<float> *wave[2], *oct[2], *semi[2], *fine[2], *shape[2], *lvl[2], *uni[2], *det[2];
    std::atomic<float> *subLevel, *subWave, *noiseLevel, *ringLevel, *xmod, *gtrClean, *gtrFuzz, *gtrOct;
    std::atomic<float> *filtMode, *cutoff, *reso, *filtDrive, *filtEnv, *keyTrack, *filtVel, *shpEnv;
    std::atomic<float> *fA, *fD, *fS, *fR, *aA, *aD, *aS, *aR, *ampVel;
    std::atomic<float> *lfo1Wave, *lfo1Rate, *lfo1Delay, *lfo1Pitch, *lfo1Cutoff, *lfo2Wave, *lfo2Rate, *lfo2Shape, *lfo2Pan, *lfo2Amp;
    std::atomic<float> *voiceMode, *glide, *drift, *spread, *bendRange;
};

// Per-block snapshot of all parameter values (plain floats)
struct Snapshot
{
    bool trkOn = true; int trkOct = 0, trkSemi = 0, trkMode = 0, trkChord = 0; float trkQuant = 0.5f, trkDyn = 0.7f;
    float ampFollow = 0.5f, envCut = 0, synLvl = 1;
    int wave[2] {}; float pitchOff[2] {}; float shape[2] {}, lvl[2] {}, det[2] {}; int uni[2] { 1, 1 };
    float subLevel = 0; int subWave = 0; float noiseLevel = 0, ringLevel = 0, xmod = 0, gtrClean = 0, gtrFuzz = 0, gtrOct = 0;
    int filtMode = 0; float cutoff = 12000, reso = 0.1f, filtDrive = 0, filtEnv = 0, keyTrack = 0.3f, filtVel = 0, shpEnv = 0;
    float fA, fD, fS, fR, aA, aD, aS, aR, ampVel = 0.5f;
    float lfo1Delay = 0, lfo1Pitch = 0, lfo1Cutoff = 0, lfo2Shape = 0, lfo2Pan = 0, lfo2Amp = 0;
    int voiceMode = 0; float glide = 0, drift = 0.1f, spread = 0.5f; int bendRange = 2;
};

class SynthEngine
{
public:
    static constexpr int maxVoices = 16, maxUnison = 5, maxStrings = 6, chunk = 16;

    explicit SynthEngine (juce::AudioProcessorValueTreeState& s) : params (s) {}

    void prepare (double sampleRate, int maxBlock);
    // out: stereo buffer to ADD the synth into (it is cleared first).
    // guitar: per-string input pointers (1 = mono, 6 = hex), already gain-adjusted.
    void process (juce::AudioBuffer<float>& out, const float* const* guitar, int numStrings, juce::MidiBuffer& midi);
    void allNotesOff();

    // UI feedback
    std::atomic<float> trackedNote { -1.0f }, inputLevel { 0.0f };
    std::atomic<int> activeVoices { 0 };

private:
    struct Voice
    {
        bool active = false;
        int source = -1;      // -1 MIDI, else string index
        int chordIdx = 0;
        int midiKey = -1;
        float interval = 0;
        float velocity = 1;
        float note = 60, target = 60, glideCoef = 0;
        uint64_t age = 0;
        float pan = 0;
        double phase[2][maxUnison] {}, modPhase[2][maxUnison] {};
        float lastOut[2] {};
        float subPhase = 0;
        // Karplus-Strong per osc/unison
        std::vector<float> ks[2][maxUnison];
        int ksPos[2][maxUnison] {};
        float ksLp[2][maxUnison] {};
        // guitar octave divider
        float octLp = 0; bool octState = false, octArmed = true;
        dsp::SVF f[2][3]; // per channel: two cascade stages / three formants
        dsp::ADSR ampEnv, filtEnv;
        float timeOn = 0;
        float driftVal = 0, driftTarget = 0;
        dsp::Noise noise;
    };

    void readParams();
    void handleMidi (const juce::MidiMessage&);
    void handleTracker (int string, const PitchTracker::Event&);
    void startVoice (Voice&, float note, float vel, bool retrigger);
    Voice* allocVoice();
    void renderVoice (Voice&, float* L, float* R, int start, int n, const float* guitar, const float* follow);
    float trackedToNote (float played, float interval) const;
    void exciteKs (Voice&, int osc, int u, float freq);

    EngineParams params;
    Snapshot p;
    double sr = 48000;
    Voice voices[maxVoices];
    uint64_t ageCounter = 0;
    PitchTracker trackers[maxStrings];
    int trackerVoices[maxStrings][4];
    int trackerVoiceCount[maxStrings] {};
    bool lastBass = false;
    std::vector<PitchTracker::Event> events[maxStrings];
    std::vector<float> followBuf[maxStrings];
    dsp::LFO lfo1, lfo2;
    std::vector<float> lfo1Arr, lfo2Arr;
    float lastNote = 60;
    int panFlip = 0;
    float pitchBend = 0, modWheel = 0;
    bool sustain = false;
    std::vector<int> heldKeys;      // mono / legato note stack
    bool sustainedKeys[128] {};
    int maxBlockSize = 512;
};
