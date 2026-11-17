#pragma once
#include "DSP.h"
#include <vector>

// ============================================================================
//  Monophonic guitar pitch tracker (YIN on a decimated, band-limited signal).
//
//  - The input is high-passed, low-passed and decimated to ~16 kHz (8 kHz in
//    Bass range), so the O(W * tau) difference function stays cheap.
//  - YIN runs every 32 decimated samples (2 ms guitar / 4 ms bass).
//  - A small state machine turns estimates into NoteOn / Pitch / NoteOff
//    events: confident first estimate = immediate note-on, otherwise two
//    agreeing estimates; gradual changes are bends (Pitch events), sudden
//    stable jumps are new notes; octave jumps need three confirmations; a
//    pick attack on the same pitch re-triggers.
//  - It also outputs a per-sample dynamics follower (level relative to the
//    note's pick peak), used for "amp follows guitar" and the envelope filter.
//
//  One instance per string in hex (6-channel) mode.
// ============================================================================
class PitchTracker
{
public:
    struct Event
    {
        enum Type { NoteOn, Pitch, NoteOff } type;
        int offset;       // sample offset in the current block
        float note;       // fractional MIDI note actually played
        float velocity;   // 0..1, from pick strength
    };

    void prepare (double sampleRate, bool bassRange)
    {
        sr = (float) sampleRate;
        bass = bassRange;
        const float targetRate = bass ? 8000.0f : 16000.0f;
        decim = std::max (1, (int) std::lround (sr / targetRate));
        dsr = sr / (float) decim;
        const float minHz = bass ? 28.0f : 60.0f, maxHz = bass ? 700.0f : 1500.0f;
        tauMax = (int) std::ceil (dsr / minHz);
        tauMin = std::max (2, (int) std::floor (dsr / maxHz));
        win = tauMax;
        settleHops = bass ? 12 : 18; // ~40-50 ms after a note-on, jumps are corrections
        buf.assign ((size_t) (win + tauMax + 2), 0.0f);
        diff.assign ((size_t) tauMax + 2, 0.0f);
        hp.highpass (bass ? 25.0f : 55.0f, 0.707f, sr);
        lp1.lowpass (bass ? 700.0f : 1400.0f, 0.707f, sr);
        lp2.lowpass (bass ? 900.0f : 1800.0f, 0.707f, sr);
        envAtt = std::exp (-1.0f / (0.001f * sr));
        envRel = std::exp (-1.0f / (0.060f * sr));
        fastRel = std::exp (-1.0f / (0.015f * sr));
        slowAtt = std::exp (-1.0f / (0.030f * sr));
        slowRel = std::exp (-1.0f / (0.250f * sr));
        reset();
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        hp.reset(); lp1.reset(); lp2.reset();
        env = fastEnv = slowEnv = 0; decimCount = 0; hopCount = 0;
        active = false; gate = false; candidateCount = 0; jumpCount = 0; invalidHops = 0;
        notePeak = 1e-4f; follow = 0; lastOnsetHop = -1000; hopIndex = 0;
    }

    void setThresholdDb (float db) { threshold = std::pow (10.0f, db / 20.0f); }

    bool isActive() const { return active; }
    float getNote() const { return note; }
    float getLevel() const { return env; }

    // Process n samples. Appends events, writes the dynamics follower into followOut (may be null).
    void process (const float* in, int n, std::vector<Event>& events, float* followOut)
    {
        for (int i = 0; i < n; ++i)
        {
            const float x = hp.tick (in[i]);
            const float ax = std::abs (x);
            env = ax > env ? ax + envAtt * (env - ax) : ax + envRel * (env - ax);
            fastEnv = ax > fastEnv ? ax : ax + fastRel * (fastEnv - ax);
            slowEnv = fastEnv > slowEnv ? fastEnv + slowAtt * (slowEnv - fastEnv) : fastEnv + slowRel * (slowEnv - fastEnv);
            if (fastEnv > 2.2f * slowEnv + threshold * 0.5f && fastEnv > threshold * 2.0f) onsetFlag = true;
            if (active) notePeak = std::max (notePeak, env);

            const float y = lp2.tick (lp1.tick (x));
            if (++decimCount >= decim)
            {
                decimCount = 0;
                std::move (buf.begin() + 1, buf.end(), buf.begin());
                buf.back() = y;
                if (++hopCount >= hop) { hopCount = 0; analyse (i, events); }
            }
            const float target = active ? std::min (1.2f, env / notePeak) : 0.0f;
            follow += (target - follow) * 0.004f;
            if (followOut) followOut[i] = follow;
        }
    }

private:
    // YIN over the most recent 2 * tauLim samples (window = tauLim): returns the
    // period in decimated samples (0 if none) and the aperiodicity in ap.
    float yin (float& ap, int tauLim)
    {
        const int w = tauLim;
        const float* x = buf.data() + (buf.size() - (size_t) (w + tauLim));
        float energy = 0;
        for (int j = 0; j < w + tauLim; ++j) energy += x[j] * x[j];
        if (energy < 1e-9f) { ap = 1; return 0; }
        float running = 0;
        int best = -1;
        for (int tau = 1; tau <= tauLim; ++tau)
        {
            float d = 0;
            const float* b = x + tau;
            for (int j = 0; j < w; ++j) { const float e = x[j] - b[j]; d += e * e; }
            running += d;
            diff[(size_t) tau] = running > 0 ? d * (float) tau / running : 1.0f;
        }
        for (int tau = tauMin; tau < tauLim; ++tau)
            if (diff[(size_t) tau] < 0.18f)
            {
                while (tau + 1 < tauLim && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                best = tau; break;
            }
        if (best < 0)
        {
            // no clear dip: take the FIRST local minimum close to the global one (avoids sub-octave picks)
            float m = 10;
            for (int tau = tauMin; tau < tauLim; ++tau) m = std::min (m, diff[(size_t) tau]);
            for (int tau = tauMin + 1; tau < tauLim - 1; ++tau)
                if (diff[(size_t) tau] <= m + 0.08f && diff[(size_t) tau] <= diff[(size_t) tau - 1] && diff[(size_t) tau] <= diff[(size_t) tau + 1])
                { best = tau; break; }
            if (best < 0) { ap = 1; return 0; }
        }
        // octave-up guard: if the dip at twice the period is much deeper, the
        // "period" we found was really the 2nd harmonic
        if (diff[(size_t) best] > 0.04f && 2 * best + 2 < tauLim)
        {
            int b2 = 2 * best;
            for (int k = 2 * best - 2; k <= 2 * best + 2; ++k) if (diff[(size_t) k] < diff[(size_t) b2]) b2 = k;
            if (diff[(size_t) b2] < 0.5f * diff[(size_t) best]) best = b2;
        }
        ap = diff[(size_t) best];
        float t = (float) best;
        if (best > 1 && best < tauLim)
        {
            const float s0 = diff[(size_t) best - 1], s1 = diff[(size_t) best], s2 = diff[(size_t) best + 1];
            const float den = s0 + s2 - 2 * s1;
            if (std::abs (den) > 1e-9f) t += 0.5f * (s0 - s2) / den;
        }
        return t;
    }

    // Multi-resolution: short windows first (fast for high notes), the full window only when needed.
    float detect (float& ap)
    {
        for (int tier = 0; tier < 3; ++tier)
        {
            const int lim = tier == 0 ? tauMax / 4 : (tier == 1 ? tauMax / 2 : tauMax);
            if (lim <= tauMin + 2) continue;
            const float per = yin (ap, lim);
            if (per > 0 && (tier == 2 || ap < 0.1f)) return per;
        }
        return 0;
    }

    float velocityFromLevel (float lvl) const
    {
        const float db = 20.0f * std::log10 (std::max (lvl, 1e-6f));
        return dsp::clamp ((db + 48.0f) / 42.0f, 0.15f, 1.0f); // -48 dBFS soft .. -6 dBFS hard
    }

    void analyse (int offset, std::vector<Event>& events)
    {
        ++hopIndex;
        const bool onset = onsetFlag;
        onsetFlag = false;
        if (env > threshold) gate = true;
        else if (env < threshold * 0.5f) gate = false;

        float ap = 1;
        const float period = gate ? detect (ap) : 0.0f;
        const bool valid = period > 0 && ap < 0.3f;
        const float est = valid ? dsp::hzToMidi (dsr / period) : 0.0f;

        if (! active)
        {
            if (! gate || ! valid) { candidateCount = 0; return; }
            if (ap < 0.08f || (candidateCount > 0 && std::abs (est - candidate) < 0.5f))
            {
                active = true;
                note = est;
                notePeak = std::max (env, 1e-4f);
                follow = 1.0f;
                invalidHops = 0; jumpCount = 0; candidateCount = 0;
                lastOnsetHop = hopIndex;
                events.push_back ({ Event::NoteOn, offset, note, velocityFromLevel (std::max (env, slowEnv)) });
            }
            else { candidate = est; candidateCount = 1; }
            return;
        }

        // active
        if (! gate)
        {
            active = false;
            events.push_back ({ Event::NoteOff, offset, note, 0 });
            return;
        }
        if (! valid)
        {
            if (++invalidHops > (bass ? 30 : 45)) // ~90 ms of no pitch: string muted
            {
                active = false;
                events.push_back ({ Event::NoteOff, offset, note, 0 });
            }
            return;
        }
        invalidHops = 0;
        const float d = est - note;
        const bool octaveJump = std::abs (std::abs (d) - 12.0f) < 0.6f;

        if (std::abs (d) < 0.75f)
        {
            jumpCount = 0;
            note += d * 0.7f;
            if (onset && hopIndex - lastOnsetHop > 25)
            {
                // re-picked the same note: re-trigger
                notePeak = std::max (env, 1e-4f);
                lastOnsetHop = hopIndex;
                events.push_back ({ Event::NoteOn, offset, note, velocityFromLevel (env) });
            }
            else
                events.push_back ({ Event::Pitch, offset, note, 0 });
            return;
        }

        // a jump: confirm before trusting it (octave errors need more evidence)
        if (jumpCount > 0 && std::abs (est - jumpTarget) < 0.6f) ++jumpCount;
        else { jumpTarget = est; jumpCount = 1; }
        const bool settling = hopIndex - lastOnsetHop < settleHops;
        const int needed = settling ? 2 : (octaveJump ? 3 : (onset || ap < 0.1f ? 1 : 2));
        if (jumpCount >= needed && settling)
        {
            // still inside the attack: the first estimate was wrong, correct it without re-triggering
            note = est; jumpCount = 0;
            events.push_back ({ Event::Pitch, offset, note, 0 });
        }
        else if (jumpCount >= needed)
        {
            note = est;
            jumpCount = 0;
            notePeak = std::max (env, 1e-4f);
            lastOnsetHop = hopIndex;
            events.push_back ({ Event::NoteOn, offset, note, velocityFromLevel (std::max (env, fastEnv)) });
        }
    }

    float sr = 48000, dsr = 16000;
    bool bass = false;
    int decim = 3, tauMin = 10, tauMax = 267, win = 267, hop = 32;
    std::vector<float> buf, diff;
    dsp::Biquad hp, lp1, lp2;
    float envAtt = 0, envRel = 0, fastRel = 0, slowAtt = 0, slowRel = 0;
    float env = 0, fastEnv = 0, slowEnv = 0, threshold = 0.002f;
    int decimCount = 0, hopCount = 0;
    bool active = false, gate = false, onsetFlag = false;
    float note = 0, candidate = 0, jumpTarget = 0, notePeak = 1e-4f, follow = 0;
    int settleHops = 15, candidateCount = 0, jumpCount = 0, invalidHops = 0, lastOnsetHop = -1000, hopIndex = 0;
};
