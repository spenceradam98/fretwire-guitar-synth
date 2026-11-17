#include "SynthEngine.h"

using namespace dsp;

// ------------------------------------------------------------------ params
EngineParams::EngineParams (juce::AudioProcessorValueTreeState& st) : s (st)
{
    trkOn = get ("trkOn"); trkRange = get ("trkRange"); trkSens = get ("trkSens"); trkOct = get ("trkOct");
    trkSemi = get ("trkSemi"); trkQuant = get ("trkQuant"); trkMode = get ("trkMode"); trkChord = get ("trkChord");
    trkDyn = get ("trkDyn"); ampFollow = get ("ampFollow"); envCut = get ("envCut"); synLvl = get ("synLvl"); inGain = get ("inGain");
    const char* o[2] = { "oscA", "oscB" };
    for (int i = 0; i < 2; ++i)
    {
        auto g = [&] (const char* suffix) { return get ((juce::String (o[i]) + suffix).toRawUTF8()); };
        wave[i] = g ("Wave"); oct[i] = g ("Oct"); semi[i] = g ("Semi"); fine[i] = g ("Fine");
        shape[i] = g ("Shape"); lvl[i] = g ("Level"); uni[i] = g ("Unison"); det[i] = g ("Detune");
    }
    subLevel = get ("subLevel"); subWave = get ("subWave"); noiseLevel = get ("noiseLevel"); ringLevel = get ("ringLevel");
    xmod = get ("xmod"); gtrClean = get ("gtrClean"); gtrFuzz = get ("gtrFuzz"); gtrOct = get ("gtrOct");
    filtMode = get ("filtMode"); cutoff = get ("cutoff"); reso = get ("reso"); filtDrive = get ("filtDrive");
    filtEnv = get ("filtEnv"); keyTrack = get ("keyTrack"); filtVel = get ("filtVel"); shpEnv = get ("shpEnv");
    fA = get ("fA"); fD = get ("fD"); fS = get ("fS"); fR = get ("fR");
    aA = get ("aA"); aD = get ("aD"); aS = get ("aS"); aR = get ("aR"); ampVel = get ("ampVel");
    lfo1Wave = get ("lfo1Wave"); lfo1Rate = get ("lfo1Rate"); lfo1Delay = get ("lfo1Delay"); lfo1Pitch = get ("lfo1Pitch");
    lfo1Cutoff = get ("lfo1Cutoff"); lfo2Wave = get ("lfo2Wave"); lfo2Rate = get ("lfo2Rate"); lfo2Shape = get ("lfo2Shape");
    lfo2Pan = get ("lfo2Pan"); lfo2Amp = get ("lfo2Amp");
    voiceMode = get ("voiceMode"); glide = get ("glide"); drift = get ("drift"); spread = get ("spread"); bendRange = get ("bendRange");
}

std::atomic<float>* EngineParams::get (const char* id)
{
    auto* v = s.getRawParameterValue (id);
    jassert (v != nullptr);
    return v;
}

// ------------------------------------------------------------------ setup
void SynthEngine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    maxBlockSize = maxBlock;
    lfo1Arr.assign ((size_t) (std::max (maxBlock, 4096) / chunk + 2), 0.0f);
    lfo2Arr = lfo1Arr;
    getWavetable (0); // build tables now, not on the audio thread
    const int ksLen = juce::nextPowerOfTwo ((int) (sr / 15.0) + 8);
    for (auto& v : voices)
    {
        v = Voice();
        for (int o = 0; o < 2; ++o)
            for (int u = 0; u < maxUnison; ++u) v.ks[o][u].assign ((size_t) ksLen, 0.0f);
        v.ampEnv.setSampleRate ((float) sr);
        v.filtEnv.setSampleRate ((float) sr);
    }
    for (int i = 0; i < maxVoices; ++i) voices[i].noise.seed ((uint32_t) (1234567 + i * 7919));
    lastBass = params.trkRange->load() > 0.5f;
    for (int s = 0; s < maxStrings; ++s)
    {
        trackers[s].prepare (sr, ! lastBass); // reserve capacity for both ranges
        trackers[s].prepare (sr, lastBass);
        events[s].clear();
        events[s].reserve (256);
        followBuf[s].assign ((size_t) std::max (maxBlock, 4096), 0.0f);
        trackerVoiceCount[s] = 0;
    }
    heldKeys.clear();
    heldKeys.reserve (128);
    pitchBend = 0; modWheel = 0; sustain = false;
    std::fill (std::begin (sustainedKeys), std::end (sustainedKeys), false);
}

void SynthEngine::allNotesOff()
{
    for (auto& v : voices) { v.ampEnv.noteOff(); v.filtEnv.noteOff(); }
    heldKeys.clear();
}

void SynthEngine::readParams()
{
    auto L = [] (std::atomic<float>* a) { return a->load(); };
    auto I = [] (std::atomic<float>* a) { return (int) std::lround (a->load()); };
    p.trkOn = L (params.trkOn) > 0.5f; p.trkOct = I (params.trkOct); p.trkSemi = I (params.trkSemi);
    p.trkMode = I (params.trkMode); p.trkChord = I (params.trkChord); p.trkQuant = L (params.trkQuant); p.trkDyn = L (params.trkDyn);
    p.ampFollow = L (params.ampFollow); p.envCut = L (params.envCut); p.synLvl = L (params.synLvl);
    for (int o = 0; o < 2; ++o)
    {
        p.wave[o] = I (params.wave[o]);
        p.pitchOff[o] = 12.0f * (float) I (params.oct[o]) + (float) I (params.semi[o]) + L (params.fine[o]) * 0.01f;
        p.shape[o] = L (params.shape[o]); p.lvl[o] = L (params.lvl[o]);
        p.uni[o] = juce::jlimit (1, maxUnison, I (params.uni[o])); p.det[o] = L (params.det[o]);
    }
    p.subLevel = L (params.subLevel); p.subWave = I (params.subWave); p.noiseLevel = L (params.noiseLevel);
    p.ringLevel = L (params.ringLevel); p.xmod = L (params.xmod);
    p.gtrClean = L (params.gtrClean); p.gtrFuzz = L (params.gtrFuzz); p.gtrOct = L (params.gtrOct);
    p.filtMode = I (params.filtMode); p.cutoff = L (params.cutoff); p.reso = L (params.reso); p.filtDrive = L (params.filtDrive);
    p.filtEnv = L (params.filtEnv); p.keyTrack = L (params.keyTrack); p.filtVel = L (params.filtVel); p.shpEnv = L (params.shpEnv);
    p.fA = L (params.fA); p.fD = L (params.fD); p.fS = L (params.fS); p.fR = L (params.fR);
    p.aA = L (params.aA); p.aD = L (params.aD); p.aS = L (params.aS); p.aR = L (params.aR); p.ampVel = L (params.ampVel);
    p.lfo1Delay = L (params.lfo1Delay); p.lfo1Pitch = L (params.lfo1Pitch); p.lfo1Cutoff = L (params.lfo1Cutoff);
    p.lfo2Shape = L (params.lfo2Shape); p.lfo2Pan = L (params.lfo2Pan); p.lfo2Amp = L (params.lfo2Amp);
    p.voiceMode = I (params.voiceMode); p.glide = L (params.glide); p.drift = L (params.drift);
    p.spread = L (params.spread); p.bendRange = I (params.bendRange);
    lfo1.set (I (params.lfo1Wave), L (params.lfo1Rate), (float) sr);
    lfo2.set (I (params.lfo2Wave), L (params.lfo2Rate), (float) sr);
}

// ------------------------------------------------------------- voices
SynthEngine::Voice* SynthEngine::allocVoice()
{
    Voice* best = nullptr;
    for (auto& v : voices) if (! v.active) return &v;
    // steal: prefer released voices, then the oldest
    for (auto& v : voices)
        if (v.ampEnv.stage == ADSR::Release && (best == nullptr || v.age < best->age)) best = &v;
    if (best) return best;
    for (auto& v : voices) if (best == nullptr || v.age < best->age) best = &v;
    return best;
}

void SynthEngine::exciteKs (Voice& v, int o, int u, float freq)
{
    auto& buf = v.ks[o][u];
    const int size = (int) buf.size(), mask = size - 1;
    const int len = std::min (size - 4, (int) (sr / std::max (freq, 16.0f)) + 4);
    const float bright = 0.15f + 0.85f * p.shape[o];
    float lp = 0;
    int pos = v.ksPos[o][u];
    for (int i = 0; i < len; ++i)
    {
        lp += bright * (v.noise.next() - lp);
        buf[(size_t) ((pos - len + i) & mask)] = lp * (1.6f - 0.6f * bright) * 1.3f;
    }
    v.ksLp[o][u] = 0;
}

void SynthEngine::startVoice (Voice& v, float note, float vel, bool retrigger)
{
    const bool fresh = ! v.active;
    v.target = note;
    if (fresh) v.note = note;
    v.velocity = vel;
    v.age = ++ageCounter;
    if (fresh || retrigger)
    {
        v.active = true;
        v.ampEnv.noteOn();
        v.filtEnv.noteOn();
        v.timeOn = 0;
        if (fresh)
        {
            for (int o = 0; o < 2; ++o)
                for (int u = 0; u < maxUnison; ++u)
                {
                    v.phase[o][u] = u == 0 ? 0.0 : (double) (v.noise.next() * 0.5f + 0.5f);
                    v.modPhase[o][u] = 0;
                }
            v.subPhase = 0;
            for (auto& ch : v.f) for (auto& f : ch) f.reset();
            v.driftVal = v.noise.next() * 0.5f;
        }
        for (int o = 0; o < 2; ++o)
            if (p.wave[o] == Params::wPluck && p.lvl[o] > 0.0f)
                for (int u = 0; u < p.uni[o]; ++u) exciteKs (v, o, u, midiToHz (note + p.pitchOff[o]));
    }
}

float SynthEngine::trackedToNote (float played, float interval) const
{
    float n = played + 12.0f * (float) p.trkOct + (float) p.trkSemi;
    const float r = std::round (n);
    n += (r - n) * p.trkQuant;
    return n + interval;
}

static const std::vector<float>& chordIntervals (int chord)
{
    static const std::vector<float> table[] = {
        { 0 }, { 0, 12 }, { 0, -12 }, { 0, 7 }, { 0, 7, 12 }, { 0, -12, 12 },
        { 0, 4, 7 }, { 0, 3, 7 }, { 0, 5, 7 }, { 0, 4, 7, 11 }, { 0, 3, 7, 10 }, { 0, 5, 10 } };
    return table[juce::jlimit (0, 11, chord)];
}

void SynthEngine::handleTracker (int s, const PitchTracker::Event& e)
{
    auto owned = [&] (int k) -> Voice*
    {
        if (k >= trackerVoiceCount[s]) return nullptr;
        auto& v = voices[trackerVoices[s][k]];
        return (v.active && v.source == s && v.chordIdx == k) ? &v : nullptr;
    };
    const float pitchGlide = std::max (p.glide, 0.004f);

    if (e.type == PitchTracker::Event::NoteOn)
    {
        const auto& iv = chordIntervals (p.trkChord);
        const int count = (int) std::min<size_t> (4, iv.size());
        // release voices of a previous, larger chord
        for (int k = count; k < trackerVoiceCount[s]; ++k)
            if (auto* v = owned (k)) { v->ampEnv.noteOff(); v->filtEnv.noteOff(); }
        const float vel = 0.8f + (e.velocity - 0.8f) * p.trkDyn;
        for (int k = 0; k < count; ++k)
        {
            const float n = trackedToNote (e.note, iv[(size_t) k]);
            Voice* v = owned (k);
            const bool legato = p.trkMode == 1 && v != nullptr && v->ampEnv.stage != ADSR::Release;
            if (v == nullptr) v = allocVoice();
            v->source = s; v->chordIdx = k; v->interval = iv[(size_t) k]; v->midiKey = -1;
            v->glideCoef = std::exp (-(float) chunk / (pitchGlide * (float) sr));
            if (! v->active) v->note = n;
            v->pan = (k == 0 ? 0.0f : (k % 2 ? -0.6f : 0.6f)) * p.spread;
            startVoice (*v, n, vel, ! legato);
            trackerVoices[s][k] = (int) (v - voices);
        }
        trackerVoiceCount[s] = count;
    }
    else if (e.type == PitchTracker::Event::Pitch)
    {
        for (int k = 0; k < trackerVoiceCount[s]; ++k)
            if (auto* v = owned (k))
            {
                v->target = trackedToNote (e.note, v->interval);
                v->glideCoef = std::exp (-(float) chunk / (0.004f * (float) sr));
            }
    }
    else
    {
        for (int k = 0; k < trackerVoiceCount[s]; ++k)
            if (auto* v = owned (k)) { v->ampEnv.noteOff(); v->filtEnv.noteOff(); }
    }
}

void SynthEngine::handleMidi (const juce::MidiMessage& m)
{
    const float glideCoef = p.glide > 0.0005f ? std::exp (-(float) chunk / (p.glide * (float) sr)) : 0.0f;
    auto findMono = [&]() -> Voice* { for (auto& v : voices) if (v.active && v.source == -1 && v.ampEnv.stage != ADSR::Release) return &v; return nullptr; };

    if (m.isNoteOn())
    {
        const int key = m.getNoteNumber();
        const float vel = m.getFloatVelocity();
        sustainedKeys[key] = false;
        if (p.voiceMode == 0)
        {
            Voice* v = allocVoice();
            const bool wasActive = v->active;
            v->source = -1; v->chordIdx = 0; v->midiKey = key; v->interval = 0;
            v->glideCoef = glideCoef;
            v->pan = ((panFlip++ % 2) ? -0.35f : 0.35f) * p.spread;
            startVoice (*v, (float) key, vel, true);
            if (! wasActive && glideCoef > 0) v->note = lastNote;
        }
        else
        {
            heldKeys.erase (std::remove (heldKeys.begin(), heldKeys.end(), key), heldKeys.end());
            heldKeys.push_back (key);
            Voice* v = findMono();
            const bool legato = p.voiceMode == 2 && v != nullptr;
            if (v == nullptr) v = allocVoice();
            const bool wasActive = v->active;
            v->source = -1; v->chordIdx = 0; v->midiKey = key; v->interval = 0; v->pan = 0;
            v->glideCoef = glideCoef;
            startVoice (*v, (float) key, vel, ! legato);
            if (! wasActive && glideCoef > 0) v->note = lastNote;
        }
        lastNote = (float) key;
    }
    else if (m.isNoteOff())
    {
        const int key = m.getNoteNumber();
        if (p.voiceMode == 0)
        {
            for (auto& v : voices)
                if (v.active && v.source == -1 && v.midiKey == key && v.ampEnv.stage != ADSR::Release)
                {
                    if (sustain) sustainedKeys[key] = true;
                    else { v.ampEnv.noteOff(); v.filtEnv.noteOff(); }
                }
        }
        else
        {
            heldKeys.erase (std::remove (heldKeys.begin(), heldKeys.end(), key), heldKeys.end());
            if (auto* v = findMono())
            {
                if (! heldKeys.empty()) { v->target = (float) heldKeys.back(); v->midiKey = heldKeys.back(); }
                else if (v->midiKey == key) { v->ampEnv.noteOff(); v->filtEnv.noteOff(); }
            }
        }
    }
    else if (m.isSustainPedalOn()) sustain = true;
    else if (m.isSustainPedalOff())
    {
        sustain = false;
        for (int k = 0; k < 128; ++k)
            if (sustainedKeys[k])
            {
                sustainedKeys[k] = false;
                for (auto& v : voices)
                    if (v.active && v.source == -1 && v.midiKey == k) { v.ampEnv.noteOff(); v.filtEnv.noteOff(); }
            }
    }
    else if (m.isPitchWheel()) pitchBend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
    else if (m.isControllerOfType (1)) modWheel = (float) m.getControllerValue() / 127.0f;
    else if (m.isAllNotesOff() || m.isAllSoundOff()) allNotesOff();
}

// ------------------------------------------------------------- rendering
void SynthEngine::process (juce::AudioBuffer<float>& out, const float* const* guitar, int numStrings, juce::MidiBuffer& midi)
{
    const int n = out.getNumSamples();
    out.clear();
    readParams();

    for (auto& v : voices)
    {
        v.ampEnv.set (p.aA, p.aD, p.aS, p.aR);
        v.filtEnv.set (p.fA, p.fD, p.fS, p.fR);
    }

    // ---- trackers
    if ((int) followBuf[0].size() < n) for (auto& f : followBuf) f.assign ((size_t) n, 0.0f); // oversized host block
    numStrings = juce::jlimit (0, maxStrings, numStrings);
    const bool bass = params.trkRange->load() > 0.5f;
    if (bass != lastBass)
    {
        lastBass = bass;
        for (auto& t : trackers) t.prepare (sr, bass);
    }
    const float thr = params.trkSens->load();
    float level = 0;
    for (int s = 0; s < maxStrings; ++s)
    {
        events[s].clear();
        if (s >= numStrings || guitar == nullptr || guitar[s] == nullptr) continue;
        trackers[s].setThresholdDb (thr);
        trackers[s].process (guitar[s], n, events[s], followBuf[s].data());
        level = std::max (level, trackers[s].getLevel());
        if (! p.trkOn) events[s].clear();
    }
    inputLevel.store (level);
    if (! p.trkOn)
        for (int s = 0; s < maxStrings; ++s)
            for (int k = 0; k < trackerVoiceCount[s]; ++k)
            {
                auto& v = voices[trackerVoices[s][k]];
                if (v.active && v.source == s) { v.ampEnv.noteOff(); v.filtEnv.noteOff(); }
            }

    // ---- global LFOs, one value per 16-sample chunk of the block
    const int numChunks = (n + chunk - 1) / chunk;
    if ((int) lfo1Arr.size() < numChunks + 1) { lfo1Arr.resize ((size_t) numChunks + 1); lfo2Arr.resize ((size_t) numChunks + 1); }
    for (int c = 0; c < numChunks; ++c)
    {
        const int len = std::min (chunk, n - c * chunk);
        lfo1Arr[(size_t) c] = lfo1.advance (len);
        lfo2Arr[(size_t) c] = lfo2.advance (len);
    }

    // ---- merge MIDI + tracker events in time order, render between them
    auto mit = midi.cbegin();
    size_t eidx[maxStrings] {};
    int pos = 0;
    float* L = out.getWritePointer (0);
    float* R = out.getWritePointer (1);

    auto renderTo = [&] (int end)
    {
        if (end <= pos) return;
        for (auto& v : voices)
            if (v.active)
            {
                const int s = v.source;
                const bool hasGtr = s >= 0 && s < numStrings && guitar != nullptr && guitar[s] != nullptr;
                renderVoice (v, L, R, pos, end - pos, hasGtr && v.chordIdx == 0 ? guitar[s] : nullptr,
                             hasGtr ? followBuf[s].data() : nullptr);
            }
        pos = end;
    };

    for (;;)
    {
        int next = n, which = -2; // -1 = midi, >= 0 = tracker string
        if (mit != midi.cend() && (*mit).samplePosition < next) { next = (*mit).samplePosition; which = -1; }
        for (int s = 0; s < maxStrings; ++s)
            if (eidx[s] < events[s].size() && events[s][eidx[s]].offset < next) { next = events[s][eidx[s]].offset; which = s; }
        renderTo (juce::jlimit (pos, n, next));
        if (which == -2) break;
        if (which == -1) { handleMidi ((*mit).getMessage()); ++mit; }
        else handleTracker (which, events[which][eidx[which]++]);
    }

    // ---- UI feedback
    int count = 0;
    for (auto& v : voices) count += v.active ? 1 : 0;
    activeVoices.store (count);
    trackedNote.store (numStrings > 0 && trackers[0].isActive() ? trackers[0].getNote() : -1.0f);

    out.applyGain (p.synLvl);
}

void SynthEngine::renderVoice (Voice& v, float* L, float* R, int start, int n, const float* gtr, const float* follow)
{
    const auto& sine = SineTable::get();
    const float fsr = (float) sr;
    const float invSr = 1.0f / fsr;
    const bool tracked = follow != nullptr;

    for (int c0 = start; c0 < start + n; c0 += chunk)
    {
        const int cn = std::min (chunk, start + n - c0);
        // ---- per-chunk modulation
        const int ci = std::min ((int) lfo1Arr.size() - 1, c0 / chunk);
        const float lfo1Val = lfo1Arr[(size_t) ci], lfo2Val = lfo2Arr[(size_t) ci];
        if (v.glideCoef > 0) v.note = v.target + (v.note - v.target) * v.glideCoef; else v.note = v.target;
        if (std::abs (v.note - v.target) < 1e-4f) v.note = v.target;
        v.timeOn += (float) cn * invSr;
        if (v.noise.next() > 0.97f) v.driftTarget = v.noise.next();
        v.driftVal += (v.driftTarget - v.driftVal) * 0.01f;
        const float fade = p.lfo1Delay > 0.001f ? std::min (1.0f, v.timeOn / p.lfo1Delay) : 1.0f;
        const float vib = lfo1Val * (p.lfo1Pitch * p.lfo1Pitch * 2.0f * fade + modWheel * 0.4f);
        const float pitchMod = pitchBend * (float) p.bendRange + vib + v.driftVal * p.drift * 0.25f;
        const float fEnvNow = v.filtEnv.value;
        const float followNow = tracked ? follow[c0] : v.ampEnv.value;

        float inc[2][maxUnison], gl[2][maxUnison], gr[2][maxUnison], shapeNow[2];
        for (int o = 0; o < 2; ++o)
        {
            const int U = p.uni[o];
            const float base = v.note + p.pitchOff[o] + pitchMod;
            for (int u = 0; u < U; ++u)
            {
                const float pos = U > 1 ? (2.0f * (float) u / (float) (U - 1) - 1.0f) : 0.0f;
                inc[o][u] = std::min (0.45f, midiToHz (base + pos * p.det[o] * 0.5f) * invSr);
                const float pan = clamp (v.pan + pos * p.spread + lfo2Val * p.lfo2Pan, -1.0f, 1.0f);
                const float norm = p.lvl[o] / std::sqrt ((float) U);
                gl[o][u] = norm * std::sqrt (0.5f * (1.0f - pan));
                gr[o][u] = norm * std::sqrt (0.5f * (1.0f + pan));
            }
            shapeNow[o] = clamp (p.shape[o] + p.shpEnv * fEnvNow + p.lfo2Shape * lfo2Val * 0.5f, 0.0f, 1.0f);
        }
        const float subInc = midiToHz (v.note + p.pitchOff[0] + pitchMod - (p.subWave >= 2 ? 24.0f : 12.0f)) * invSr;
        const float monoPanL = std::sqrt (0.5f * (1.0f - clamp (v.pan + lfo2Val * p.lfo2Pan, -1.f, 1.f)));
        const float monoPanR = std::sqrt (0.5f * (1.0f + clamp (v.pan + lfo2Val * p.lfo2Pan, -1.f, 1.f)));

        // ---- filter coefficients
        float octs = p.filtEnv * fEnvNow * 7.0f + p.keyTrack * (v.note - 60.0f) / 12.0f
                   + p.filtVel * (v.velocity - 0.7f) * 3.0f + p.lfo1Cutoff * lfo1Val * 2.0f * fade
                   + p.envCut * followNow * v.velocity * 5.0f;
        const float fc = clamp (p.cutoff * std::exp2 (octs), 20.0f, 20000.0f);
        const float q = 0.707f + p.reso * p.reso * 16.0f;
        const int mode = p.filtMode;
        float formantGain[3] { 1, 0.6f, 0.35f };
        if (mode == 5)
        {
            // cutoff sweeps the vowel: U O A E I
            static const float fm[5][3] = { { 325, 700, 2530 }, { 450, 800, 2830 }, { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 300, 2150, 2950 } };
            const float pos = clamp ((std::log2 (fc) - std::log2 (20.0f)) / 10.0f, 0.0f, 0.999f) * 4.0f;
            const int a = (int) pos; const float fr = pos - (float) a;
            const float fq = 4.0f + p.reso * 14.0f;
            for (int ch = 0; ch < 2; ++ch)
                for (int k = 0; k < 3; ++k)
                    v.f[ch][k].set (fm[a][k] + fr * (fm[std::min (4, a + 1)][k] - fm[a][k]), fq, fsr);
        }
        else
            for (int ch = 0; ch < 2; ++ch)
            {
                v.f[ch][0].set (fc, mode == 0 ? 0.54f : q, fsr);
                v.f[ch][1].set (fc, q, fsr);
            }
        // narrow band-passes pass less energy: make up roughly by sqrt(Q)
        const float bpComp = 1.6f * std::sqrt (q);
        const float vowelComp = 2.2f * std::sqrt (4.0f + p.reso * 14.0f) * 0.5f;
        const float driveGain = 1.0f + p.filtDrive * 6.0f;
        const float driveNorm = 1.0f / std::sqrt (driveGain);
        const float velGain = 1.0f - p.ampVel + p.ampVel * v.velocity;
        const float lfoAmp = 1.0f - p.lfo2Amp * 0.5f * (1.0f + lfo2Val);
        const float af = tracked ? p.ampFollow : 0.0f;

        for (int i = c0; i < c0 + cn; ++i)
        {
            float xl = 0, xr = 0, monoA = 0, monoB = 0;
            for (int o = 1; o >= 0; --o)
            {
                const int w = p.wave[o];
                if (p.lvl[o] <= 0.0f && ! (o == 1 && (p.xmod > 0 || p.ringLevel > 0))) continue;
                const float sh = shapeNow[o];
                const float pmIn = (o == 0 ? p.xmod * 0.5f * v.lastOut[1] : 0.0f);
                float sum = 0;
                for (int u = 0; u < p.uni[o]; ++u)
                {
                    double& ph = v.phase[o][u];
                    const float dt = inc[o][u];
                    float ph1 = (float) ph + pmIn;
                    ph1 -= std::floor (ph1);
                    float s = 0;
                    switch (w)
                    {
                        case Params::wSine:
                        { s = sine (ph1); if (sh > 0) s = sine (0.25f * s * (1.0f + sh * 4.0f)); break; }
                        case Params::wTri:
                        { s = 1.0f - 4.0f * std::abs (ph1 - 0.5f); if (sh > 0) s = sine (0.25f * s * (1.0f + sh * 4.0f)); break; }
                        case Params::wSaw:
                        {
                            s = 2.0f * ph1 - 1.0f - polyBlep (ph1, dt);
                            if (sh > 0) { float p2 = ph1 + 0.5f; p2 -= std::floor (p2); s = (s + sh * (2.0f * p2 - 1.0f - polyBlep (p2, dt))) / (1.0f + sh * 0.3f); }
                            break;
                        }
                        case Params::wPulse:
                        {
                            const float pw = 0.5f - 0.45f * sh;
                            float p2 = ph1 + (1.0f - pw); p2 -= std::floor (p2);
                            s = (2.0f * ph1 - 1.0f - polyBlep (ph1, dt)) - (2.0f * p2 - 1.0f - polyBlep (p2, dt));
                            s += 2.0f * pw - 1.0f;
                            break;
                        }
                        case Params::wWtAnalog: case Params::wWtDigital: case Params::wWtVocal: case Params::wWtOrgan:
                            s = getWavetable (w - Params::wWtAnalog).read (ph1, sh, dt); break;
                        case Params::wFm11: case Params::wFm21: case Params::wFmBell:
                        {
                            const float ratio = w == Params::wFm11 ? 1.0f : (w == Params::wFm21 ? 2.0f : 3.5f);
                            double& mp = v.modPhase[o][u];
                            const float idx = sh * sh * 1.6f; // in cycles (~0..10 rad)
                            s = sine (ph1 + idx * sine ((float) mp));
                            mp += dt * ratio; if (mp >= 1.0) mp -= 1.0;
                            break;
                        }
                        case Params::wPluck:
                        {
                            auto& buf = v.ks[o][u];
                            const int size = (int) buf.size(), mask = size - 1;
                            const float damp = 0.3f + 0.65f * sh;
                            const float delay = clamp (1.0f / dt - (1.0f - damp) / damp - 0.5f, 2.0f, (float) size - 4);
                            int& wp = v.ksPos[o][u];
                            const float rp = (float) wp - delay;
                            const int ri = (int) std::floor (rp);
                            const float fr = rp - (float) ri;
                            const float y = buf[(size_t) (ri & mask)] * (1 - fr) + buf[(size_t) ((ri + 1) & mask)] * fr;
                            float& lp = v.ksLp[o][u];
                            lp += damp * (y - lp);
                            buf[(size_t) wp] = lp * (0.9965f + 0.003f * sh);
                            wp = (wp + 1) & mask;
                            s = y;
                            break;
                        }
                        default: s = v.noise.next(); break;
                    }
                    ph += dt; if (ph >= 1.0) ph -= 1.0;
                    sum += s;
                    xl += s * gl[o][u];
                    xr += s * gr[o][u];
                }
                const float m = sum / (float) p.uni[o];
                if (o == 1) { monoB = m; v.lastOut[1] = m; }
                else monoA = m;
            }

            float mono = 0;
            if (p.ringLevel > 0) mono += monoA * monoB * p.ringLevel;
            if (p.subLevel > 0)
            {
                v.subPhase += subInc; if (v.subPhase >= 1.0f) v.subPhase -= 1.0f;
                const float sq = (p.subWave % 2 == 1)
                    ? ((v.subPhase < 0.5f ? 1.0f : -1.0f) + polyBlep (v.subPhase, subInc)
                       - polyBlep (std::fmod (v.subPhase + 0.5f, 1.0f), subInc)) * 0.7f
                    : sine (v.subPhase);
                mono += sq * p.subLevel;
            }
            if (p.noiseLevel > 0) mono += v.noise.next() * p.noiseLevel * 0.5f;
            if (gtr != nullptr)
            {
                const float g = gtr[i];
                if (p.gtrClean > 0) mono += g * p.gtrClean * 7.0f;
                if (p.gtrFuzz > 0) mono += fastTanh (g * 80.0f) * p.gtrFuzz * 0.6f;
                if (p.gtrOct > 0)
                {
                    v.octLp += 0.08f * (g - v.octLp);
                    if (v.octArmed && v.octLp > 0.0005f) { v.octState = ! v.octState; v.octArmed = false; }
                    else if (v.octLp < -0.0005f) v.octArmed = true;
                    mono += (v.octState ? 0.5f : -0.5f) * p.gtrOct;
                }
            }
            xl += mono * monoPanL;
            xr += mono * monoPanR;

            // ---- filter
            if (p.filtDrive > 0) { xl = fastTanh (xl * driveGain) * driveNorm * 1.4f; xr = fastTanh (xr * driveGain) * driveNorm * 1.4f; }
            float yl = 0, yr = 0;
            for (int ch = 0; ch < 2; ++ch)
            {
                const float x = ch == 0 ? xl : xr;
                float y = 0, lp, bp, hp;
                auto& f = v.f[ch];
                switch (mode)
                {
                    case 0: f[0].tick (x, lp, bp, hp); f[1].tick (lp, y, bp, hp); break;
                    case 1: f[1].tick (x, y, bp, hp); break;
                    case 2: f[1].tick (x, lp, bp, hp); y = bp * f[1].k * bpComp; break;
                    case 3: f[1].tick (x, lp, bp, hp); y = hp; break;
                    case 4: f[1].tick (x, lp, bp, hp); y = lp + hp; break;
                    default:
                        for (int k = 0; k < 3; ++k) { f[k].tick (x, lp, bp, hp); y += bp * f[k].k * formantGain[k] * vowelComp; }
                        break;
                }
                (ch == 0 ? yl : yr) = y;
            }

            // ---- amp
            const float env = v.ampEnv.next();
            v.filtEnv.next();
            const float fol = tracked ? follow[i] : 1.0f;
            const float gain = env * velGain * lfoAmp * ((1.0f - af) + af * fol) * 0.3f;
            L[i] += yl * gain;
            R[i] += yr * gain;
        }
        if (! v.ampEnv.active()) { v.active = false; break; }
    }
}
