#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP.h"
#include "Params.h"

// ============================================================================
//  Post FX rack, fixed order:
//  Drive > Phaser > Chorus/Flanger/Ensemble > Tremolo > Delay > Reverb
//  Every block has a power switch; switched-off blocks cost nothing.
// ============================================================================

class FxRack
{
public:
    explicit FxRack (juce::AudioProcessorValueTreeState& s) : apvts (s)
    {
        for (auto* id : { "drvOn", "drvType", "drvAmt", "drvTone", "drvMix",
                          "phsOn", "phsRate", "phsDepth", "phsFb", "phsMix",
                          "choOn", "choMode", "choRate", "choDepth", "choFb", "choMix",
                          "trmOn", "trmShape", "trmRate", "trmDepth", "trmPan",
                          "dlyOn", "dlySync", "dlyDiv", "dlyMs", "dlyFb", "dlyTone", "dlyPing", "dlyMix",
                          "revOn", "revSize", "revDamp", "revWidth", "revMix" })
            p[id] = apvts.getRawParameterValue (id);
    }

    void prepare (double sampleRate, int)
    {
        sr = (float) sampleRate;
        chorusBuf.setSize (2, (int) (sr * 0.06f) + 4); chorusBuf.clear(); chorusPos = 0;
        delayBuf.setSize (2, (int) (sr * 4.2f) + 4); delayBuf.clear(); delayPos = 0;
        reverb.setSampleRate (sampleRate);
        reverb.reset();
        for (auto& ch : phs) for (auto& z : ch) z = 0;
        phsFbState[0] = phsFbState[1] = 0;
        drvLp[0].reset(); drvLp[1].reset(); dlyLp[0].reset(); dlyLp[1].reset(); dlyHp[0].reset(); dlyHp[1].reset();
        dlySmooth = -1;
    }

    void setBpm (double b) { bpm = (float) juce::jlimit (30.0, 300.0, b); }

    void process (juce::AudioBuffer<float>& buf)
    {
        const int n = buf.getNumSamples();
        float* L = buf.getWritePointer (0);
        float* R = buf.getWritePointer (1);
        if (on ("drvOn")) drive (L, R, n);
        if (on ("phsOn")) phaser (L, R, n);
        if (on ("choOn")) chorus (L, R, n);
        if (on ("trmOn")) tremolo (L, R, n);
        if (on ("dlyOn")) { delay (L, R, n); dlyDirty = true; }
        else if (dlyDirty) { delayBuf.clear(); dlyDirty = false; dlySmooth = -1; }
        if (on ("revOn")) rev (L, R, n);
    }

private:
    float v (const char* id) const { return p.at (id)->load(); }
    bool on (const char* id) const { return v (id) > 0.5f; }

    // ------------------------------------------------------------- drive
    void drive (float* L, float* R, int n)
    {
        const int type = (int) v ("drvType");
        const float amt = v ("drvAmt"), mix = v ("drvMix");
        const float tone = 800.0f * std::pow (20.0f, v ("drvTone"));
        drvLp[0].setLP (tone, sr); drvLp[1].setLP (tone, sr);
        const float g = 1.0f + amt * (type == 1 ? 60.0f : type == 2 ? 20.0f : 8.0f);
        const float comp = 1.0f / (type == 1 ? 1.4f : 1.0f + amt * 0.6f);
        const float bits = std::exp2 (14.0f - amt * 11.0f);
        const int hold = 1 + (int) (amt * amt * 12.0f);
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float x = ch[c][i];
                float y;
                switch (type)
                {
                    case 0: y = dsp::fastTanh (x * g) / std::sqrt (g) * 1.6f; break;                         // warm
                    case 1: { const float a = x * g; y = (a > 0 ? std::tanh (a) : std::tanh (a * 0.6f) * 1.2f) * 0.7f; break; } // fuzz
                    case 2: { const float a = x * g; y = (a / (1.0f + std::abs (a))) * comp * 1.2f; break; }  // crunch
                    default:
                    {
                        if (++crushCount[c] >= hold) { crushCount[c] = 0; crushHeld[c] = std::round (x * bits) / bits; }
                        y = crushHeld[c]; break;
                    }
                }
                y = drvLp[c].lp (y);
                ch[c][i] = x + (y - x) * mix;
            }
    }

    // ------------------------------------------------------------ phaser
    void phaser (float* L, float* R, int n)
    {
        const float rate = v ("phsRate"), depth = v ("phsDepth"), fb = v ("phsFb"), mix = v ("phsMix");
        for (int i = 0; i < n; ++i)
        {
            phsLfo += rate / sr; if (phsLfo >= 1) phsLfo -= 1;
            for (int c = 0; c < 2; ++c)
            {
                float ph = phsLfo + (c ? 0.25f : 0.0f); ph -= std::floor (ph);
                const float lfo = 0.5f + 0.5f * dsp::SineTable::get() (ph);
                const float f = 200.0f * std::pow (2.0f, lfo * depth * 4.5f);
                const float t = std::tan (dsp::pi * std::min (f, sr * 0.45f) / sr);
                const float a = (t - 1) / (t + 1);
                float* io = c ? R : L;
                float x = io[i] + phsFbState[c] * fb;
                for (int s = 0; s < 6; ++s) { const float y = a * x + phs[c][s]; phs[c][s] = x - a * y; x = y; }
                phsFbState[c] = x;
                io[i] = io[i] * (1 - mix * 0.5f) + x * mix * 0.7f;
            }
        }
    }

    // ------------------------------------------------------------ chorus
    float readChorus (int c, float delaySamples)
    {
        const int size = chorusBuf.getNumSamples();
        float rp = (float) chorusPos - delaySamples;
        while (rp < 0) rp += (float) size;
        const int i0 = (int) rp; const float fr = rp - (float) i0;
        const float* d = chorusBuf.getReadPointer (c);
        return d[i0 % size] * (1 - fr) + d[(i0 + 1) % size] * fr;
    }

    void chorus (float* L, float* R, int n)
    {
        const int mode = (int) v ("choMode");
        const float rate = v ("choRate"), depth = v ("choDepth"), fb = v ("choFb"), mix = v ("choMix");
        const int size = chorusBuf.getNumSamples();
        const float baseMs = mode == 1 ? 1.5f : (mode == 2 ? 9.0f : 12.0f);
        const float modMs = mode == 1 ? 6.0f * depth + 0.2f : (mode == 2 ? 3.0f * depth : 8.0f * depth);
        for (int i = 0; i < n; ++i)
        {
            choLfo += rate / sr; if (choLfo >= 1) choLfo -= 1;
            choLfo2 += rate * 6.3f / sr; if (choLfo2 >= 1) choLfo2 -= 1;
            float out[2];
            for (int c = 0; c < 2; ++c)
            {
                float wet = 0;
                if (mode == 2)
                {
                    // ensemble: three taps 120 degrees apart, plus a fast shimmer
                    for (int k = 0; k < 3; ++k)
                    {
                        float ph = choLfo + (float) k / 3.0f + (c ? 0.17f : 0.0f); ph -= std::floor (ph);
                        const float m = dsp::SineTable::get() (ph) + 0.15f * dsp::SineTable::get() (choLfo2 + (float) k / 3.0f);
                        wet += readChorus (c, (baseMs + modMs * (0.5f + 0.5f * m)) * 0.001f * sr) * 0.45f;
                    }
                }
                else
                {
                    float ph = choLfo + (c ? 0.25f : 0.0f); ph -= std::floor (ph);
                    const float m = 0.5f + 0.5f * dsp::SineTable::get() (ph);
                    wet = readChorus (c, (baseMs + modMs * m) * 0.001f * sr);
                }
                out[c] = wet;
            }
            for (int c = 0; c < 2; ++c)
            {
                float* io = c ? R : L;
                chorusBuf.setSample (c, chorusPos, io[i] + out[c] * fb);
                io[i] = io[i] * (1.0f - mix * 0.4f) + out[c] * mix;
            }
            chorusPos = (chorusPos + 1) % size;
        }
    }

    // ------------------------------------------------------------ tremolo
    void tremolo (float* L, float* R, int n)
    {
        const int shape = (int) v ("trmShape");
        const float rate = v ("trmRate"), depth = v ("trmDepth");
        const bool pan = on ("trmPan");
        for (int i = 0; i < n; ++i)
        {
            trmPh += rate / sr; if (trmPh >= 1) trmPh -= 1;
            float m = shape == 0 ? dsp::SineTable::get() (trmPh)
                    : shape == 1 ? std::tanh (6.0f * dsp::SineTable::get() (trmPh))
                                 : 1.0f - 2.0f * trmPh;
            if (pan)
            {
                L[i] *= 1.0f - depth * 0.5f * (1.0f + m);
                R[i] *= 1.0f - depth * 0.5f * (1.0f - m);
            }
            else
            {
                const float g = 1.0f - depth * 0.5f * (1.0f + m);
                L[i] *= g; R[i] *= g;
            }
        }
    }

    // ------------------------------------------------------------ delay
    void delay (float* L, float* R, int n)
    {
        const bool sync = on ("dlySync");
        const int div = juce::jlimit (0, 12, (int) v ("dlyDiv"));
        const float timeS = sync ? Params::syncBeats[div] * 60.0f / bpm : v ("dlyMs") * 0.001f;
        const float target = juce::jlimit (1.0f, (float) delayBuf.getNumSamples() - 4, timeS * sr);
        if (dlySmooth < 0) dlySmooth = target;
        const float fb = v ("dlyFb") * 1.02f, mix = v ("dlyMix");
        const bool ping = on ("dlyPing");
        const float tone = 1200.0f * std::pow (12.0f, v ("dlyTone"));
        dlyLp[0].setLP (tone, sr); dlyLp[1].setLP (tone, sr);
        dlyHp[0].setLP (90.0f, sr); dlyHp[1].setLP (90.0f, sr);
        const int size = delayBuf.getNumSamples();
        float* d0 = delayBuf.getWritePointer (0);
        float* d1 = delayBuf.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            dlySmooth += (target - dlySmooth) * 0.0005f; // tape-style glide when the time changes
            float rp = (float) delayPos - dlySmooth;
            if (rp < 0) rp += (float) size;
            const int i0 = (int) rp; const float fr = rp - (float) i0;
            const int i1 = (i0 + 1) % size;
            float wl = d0[i0] * (1 - fr) + d0[i1] * fr;
            float wr = d1[i0] * (1 - fr) + d1[i1] * fr;
            wl = dlyHp[0].hp (dlyLp[0].lp (wl));
            wr = dlyHp[1].hp (dlyLp[1].lp (wr));
            float inL = L[i], inR = R[i];
            if (ping)
            {
                const float mono = 0.5f * (inL + inR);
                d0[delayPos] = dsp::fastTanh (mono + wr * fb);
                d1[delayPos] = dsp::fastTanh (wl * fb);
            }
            else
            {
                d0[delayPos] = dsp::fastTanh (inL + wl * fb);
                d1[delayPos] = dsp::fastTanh (inR + wr * fb);
            }
            L[i] = inL + wl * mix;
            R[i] = inR + wr * mix;
            delayPos = (delayPos + 1) % size;
        }
    }

    // ------------------------------------------------------------ reverb
    void rev (float* L, float* R, int n)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.3f + 0.69f * v ("revSize");
        rp.damping = v ("revDamp");
        rp.width = v ("revWidth");
        rp.wetLevel = v ("revMix") * 0.9f;
        rp.dryLevel = 1.0f - v ("revMix") * 0.5f;
        rp.freezeMode = 0;
        reverb.setParameters (rp);
        reverb.processStereo (L, R, n);
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::map<std::string, std::atomic<float>*> p;
    float sr = 48000, bpm = 120;
    // drive
    dsp::OnePole drvLp[2];
    float crushHeld[2] {}; int crushCount[2] {};
    // phaser
    float phs[2][6] {}, phsFbState[2] {}, phsLfo = 0;
    // chorus
    juce::AudioBuffer<float> chorusBuf; int chorusPos = 0; float choLfo = 0, choLfo2 = 0;
    // tremolo
    float trmPh = 0;
    // delay
    juce::AudioBuffer<float> delayBuf; int delayPos = 0; float dlySmooth = -1; bool dlyDirty = false;
    dsp::OnePole dlyLp[2], dlyHp[2];
    // reverb
    juce::Reverb reverb;
};
