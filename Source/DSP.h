#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>

// Small, dependency-free DSP building blocks shared by the engine and effects.
// Everything is plain scalar C++ (no SIMD intrinsics) so it runs on any x86-64
// (SSE2 baseline, fine for 2015 Intel Macs) and on arm64.
namespace dsp
{
constexpr float pi = 3.14159265358979f;
constexpr float twoPi = 6.28318530717959f;

inline float midiToHz (float m) { return 440.0f * std::exp2 ((m - 69.0f) / 12.0f); }
inline float hzToMidi (float f) { return 69.0f + 12.0f * std::log2 (std::max (f, 1.0f) / 440.0f); }
inline float clamp (float x, float lo, float hi) { return std::min (hi, std::max (lo, x)); }
inline float softClip (float x) { return std::tanh (x); }
// cheap rational tanh approximation, accurate to ~1e-3 in [-3, 3] and clamped outside
inline float fastTanh (float x)
{
    x = clamp (x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// ---------------------------------------------------------------- sine table
struct SineTable
{
    static constexpr int size = 4096, mask = size - 1;
    float t[size + 1];
    SineTable() { for (int i = 0; i <= size; ++i) t[i] = (float) std::sin (2.0 * 3.141592653589793 * i / size); }
    // phase in cycles (any real value)
    inline float operator() (float ph) const
    {
        ph -= std::floor (ph);
        const float f = ph * size;
        const int i = (int) f;
        const float fr = f - (float) i;
        return t[i] + fr * (t[i + 1] - t[i]);
    }
    static const SineTable& get() { static SineTable s; return s; }
};

// ------------------------------------------------------------------ PolyBLEP
inline float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

// ----------------------------------------------------------------- noise
struct Noise
{
    uint32_t s = 22222;
    void seed (uint32_t x) { s = x ? x : 1; }
    inline float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.0f / 2147483648.0f); }
};

// ------------------------------------------------------- TPT state variable
// Zavalishin topology-preserving SVF; stable under fast modulation.
struct SVF
{
    float ic1 = 0, ic2 = 0, g = 0, k = 2, a1 = 0, a2 = 0, a3 = 0;
    void reset() { ic1 = ic2 = 0; }
    void set (float cutoffHz, float q, float sr)
    {
        cutoffHz = clamp (cutoffHz, 10.0f, sr * 0.45f);
        g = std::tan (pi * cutoffHz / sr);
        k = 1.0f / std::max (q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    // returns low, band, high via refs
    inline void tick (float x, float& lp, float& bp, float& hp)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};

// -------------------------------------------------------------- one poles
struct OnePole
{
    float z = 0, a = 0;
    void setLP (float hz, float sr) { a = std::exp (-twoPi * hz / sr); }
    inline float lp (float x) { z = x + a * (z - x); return z; }
    inline float hp (float x) { return x - lp (x); }
    void reset() { z = 0; }
};

// RBJ biquad (used for tracking pre-filters)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void lowpass (float f, float q, float sr)  { design (f, q, sr, true); }
    void highpass (float f, float q, float sr) { design (f, q, sr, false); }
    void design (float f, float q, float sr, bool low)
    {
        const float w = twoPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0f * q);
        const float a0 = 1.0f + al;
        if (low) { b0 = (1 - c) * 0.5f / a0; b1 = (1 - c) / a0; b2 = b0; }
        else     { b0 = (1 + c) * 0.5f / a0; b1 = -(1 + c) / a0; b2 = b0; }
        a1 = -2.0f * c / a0; a2 = (1.0f - al) / a0;
    }
    inline float tick (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
};

// ------------------------------------------------------------------ ADSR
struct ADSR
{
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Idle;
    float value = 0, sr = 48000;
    float aInc = 0, dCoef = 0, sLevel = 1, rCoef = 0;
    void setSampleRate (float s) { sr = s; }
    void set (float a, float d, float s, float r)
    {
        aInc = 1.0f / std::max (1.0f, a * sr);
        dCoef = std::exp (-4.6f / std::max (1.0f, d * sr));
        sLevel = s;
        rCoef = std::exp (-4.6f / std::max (1.0f, r * sr));
    }
    void noteOn()  { stage = Attack; }
    void noteOff() { if (stage != Idle) stage = Release; }
    void kill()    { stage = Idle; value = 0; }
    bool active() const { return stage != Idle; }
    inline float next()
    {
        switch (stage)
        {
            case Attack:  value += aInc * (1.05f - value * 0.05f); if (value >= 1.0f) { value = 1.0f; stage = Decay; } break;
            case Decay:   value = sLevel + (value - sLevel) * dCoef; if (std::abs (value - sLevel) < 1e-4f) stage = Sustain; break;
            case Sustain: value = sLevel; break;
            case Release: value *= rCoef; if (value < 1e-4f) { value = 0; stage = Idle; } break;
            case Idle:    break;
        }
        return value;
    }
};

// ------------------------------------------------------------------- LFO
struct LFO
{
    float phase = 0, inc = 0, held = 0, driftA = 0, driftB = 0, out = 0;
    Noise rnd;
    int wave = 0;
    void set (int w, float hz, float sr) { wave = w; inc = hz / sr; }
    // advance by n samples, return the current value in [-1, 1]
    inline float advance (int n)
    {
        phase += inc * (float) n;
        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            held = rnd.next();
            driftA = driftB; driftB = rnd.next();
        }
        switch (wave)
        {
            case 0: out = SineTable::get() (phase); break;
            case 1: out = 1.0f - 4.0f * std::abs (phase - 0.5f); break;
            case 2: out = 2.0f * phase - 1.0f; break;
            case 3: out = phase < 0.5f ? 1.0f : -1.0f; break;
            case 4: out = held; break;
            default: { const float s = phase * phase * (3.0f - 2.0f * phase); out = driftA + (driftB - driftA) * s; break; }
        }
        return out;
    }
};

// ------------------------------------------------------------ wavetables
// Four banks of eight morphable frames, each stored as band-limited mip levels
// (level L holds at most 512 >> L harmonics). Built once, shared by all voices.
struct WavetableBank
{
    static constexpr int tableSize = 2048, mask = tableSize - 1, frames = 8, levels = 10;
    // data[frame][level][sample]
    std::vector<float> data;
    inline const float* table (int frame, int level) const
    { return data.data() + ((size_t) frame * levels + (size_t) level) * (tableSize + 1); }

    // amps: harmonic amplitudes for h = 1..512 (index 0 unused); phases are sine
    void build (const std::vector<std::array<float, 513>>& spectra)
    {
        data.assign ((size_t) frames * levels * (tableSize + 1), 0.0f);
        std::vector<float> sinT (tableSize);
        for (int i = 0; i < tableSize; ++i) sinT[(size_t) i] = (float) std::sin (2.0 * 3.141592653589793 * i / tableSize);
        for (int f = 0; f < frames; ++f)
        {
            const auto& amps = spectra[(size_t) f];
            for (int l = 0; l < levels; ++l)
            {
                float* t = data.data() + ((size_t) f * levels + (size_t) l) * (tableSize + 1);
                const int maxH = 512 >> l;
                for (int h = 1; h <= maxH; ++h)
                {
                    const float a = amps[(size_t) h];
                    if (std::abs (a) < 1e-5f) continue;
                    for (int i = 0; i < tableSize; ++i) t[i] += a * sinT[(size_t) ((h * i) & mask)];
                }
            }
            // normalise every level of the frame to the same RMS as level 0 at 0.5,
            // so morphing through a bank keeps a steady loudness
            double sq = 0;
            const float* t0 = table (f, 0);
            for (int i = 0; i < tableSize; ++i) sq += (double) t0[i] * t0[i];
            const float gain = 0.5f / (float) std::sqrt (std::max (sq / tableSize, 1e-12));
            for (int l = 0; l < levels; ++l)
            {
                float* t = data.data() + ((size_t) f * levels + (size_t) l) * (tableSize + 1);
                for (int i = 0; i < tableSize; ++i) t[i] *= gain;
                t[tableSize] = t[0];
            }
        }
    }

    // phase in [0,1), morph in [0,1], inc = cycles per sample
    inline float read (float phase, float morph, float inc) const
    {
        // choose mip level: highest harmonic must stay below ~0.45 * sr
        const float maxHarm = 0.45f / std::max (inc, 1e-7f);
        int level = 0;
        while (level < levels - 1 && (float) (512 >> level) > maxHarm) ++level;
        const float fpos = clamp (morph, 0.0f, 1.0f) * (frames - 1);
        const int f0 = std::min ((int) fpos, frames - 2);
        const float ff = fpos - (float) f0;
        const float x = phase * tableSize;
        const int i = (int) x;
        const float fr = x - (float) i;
        const float* a = table (f0, level);
        const float* b = table (f0 + 1, level);
        const float va = a[i] + fr * (a[i + 1] - a[i]);
        const float vb = b[i] + fr * (b[i + 1] - b[i]);
        return va + ff * (vb - va);
    }
};

const WavetableBank& getWavetable (int bank); // 0 analog, 1 digital, 2 vocal, 3 organ
} // namespace dsp
