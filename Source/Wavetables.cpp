#include "DSP.h"

namespace dsp
{
namespace
{
using Spec = std::array<float, 513>;

Spec empty() { Spec s{}; s.fill (0.0f); return s; }

// ---- Analog: sine > triangle > soft saw > saw > saw+square > square > pulse 25% > pulse 10%
std::vector<Spec> analog()
{
    std::vector<Spec> v;
    auto saw = [] (float rolloff) { Spec s = empty(); for (int h = 1; h <= 512; ++h) s[(size_t) h] = std::pow (1.0f / h, rolloff); return s; };
    auto pulse = [] (float duty) { Spec s = empty(); for (int h = 1; h <= 512; ++h) s[(size_t) h] = std::abs (std::sin (pi * h * duty)) / h; return s; };
    Spec sine = empty(); sine[1] = 1;
    Spec tri = empty(); for (int h = 1; h <= 512; h += 2) tri[(size_t) h] = 1.0f / (h * h);
    Spec sq = empty(); for (int h = 1; h <= 512; h += 2) sq[(size_t) h] = 1.0f / h;
    Spec sawSq = saw (1.0f); for (int h = 1; h <= 512; h += 2) sawSq[(size_t) h] += 0.7f / h;
    v = { sine, tri, saw (1.6f), saw (1.0f), sawSq, sq, pulse (0.25f), pulse (0.1f) };
    return v;
}

// ---- Digital: resonant harmonic peak sweeping upward (classic 80s wavetable sweep)
std::vector<Spec> digital()
{
    std::vector<Spec> v;
    for (int f = 0; f < 8; ++f)
    {
        Spec s = empty();
        const float centre = std::pow (2.0f, 0.6f + f * 0.72f); // harmonic ~1.5 .. ~50
        const float width = 0.18f + f * 0.02f;
        s[1] = 0.6f;
        for (int h = 1; h <= 512; ++h)
        {
            const float d = std::log2 ((float) h / centre) / width;
            s[(size_t) h] += std::exp (-d * d) * (f % 2 ? 1.0f : 0.8f) + 0.08f / h;
            if (f >= 5 && h % 2 == 0) s[(size_t) h] *= 0.4f; // hollow, square-ish upper frames
        }
        v.push_back (s);
    }
    return v;
}

// ---- Vocal: vowel formant spectra on a 130 Hz fundamental: A E I O U + "oo", "mm", choir "ah"
std::vector<Spec> vocal()
{
    struct F { float f1, f2, f3, b; };
    const F vowels[8] = { { 800, 1150, 2900, 90 }, { 400, 1600, 2700, 80 }, { 300, 2150, 2950, 70 },
                          { 450, 800, 2830, 80 },  { 325, 700, 2530, 60 },  { 280, 600, 2400, 50 },
                          { 250, 1700, 2600, 120 }, { 650, 1080, 2650, 140 } };
    std::vector<Spec> v;
    const float f0 = 130.0f;
    for (auto& w : vowels)
    {
        Spec s = empty();
        for (int h = 1; h <= 512; ++h)
        {
            const float fh = f0 * h;
            auto peak = [&] (float fc, float g) { const float d = (fh - fc) / (w.b + fc * 0.06f); return g / (1.0f + d * d); };
            s[(size_t) h] = (peak (w.f1, 1.0f) + peak (w.f2, 0.5f) + peak (w.f3, 0.25f)) * (1.0f / std::sqrt ((float) h));
        }
        v.push_back (s);
    }
    return v;
}

// ---- Organ: drawbar registrations (8' fundamental; 16' comes from the sub osc)
std::vector<Spec> organ()
{
    // drawbars: 8', 4', 2 2/3', 2', 1 3/5', 1 1/3', 1'  -> harmonics 1,2,3,4,5,6,8
    const int harm[7] = { 1, 2, 3, 4, 5, 6, 8 };
    const float regs[8][7] = {
        { 8, 0, 0, 0, 0, 0, 0 },   // flute 8'
        { 8, 8, 0, 0, 0, 0, 0 },   // 8' + 4'
        { 8, 8, 8, 0, 0, 0, 0 },   // gospel "888"
        { 8, 6, 8, 4, 0, 0, 0 },   // jazz
        { 8, 8, 8, 8, 0, 0, 4 },   // full-ish
        { 6, 8, 4, 8, 3, 2, 8 },   // bright combo
        { 8, 8, 8, 8, 8, 8, 8 },   // full organ
        { 4, 3, 8, 2, 8, 6, 2 } }; // reedy
    std::vector<Spec> v;
    for (auto& r : regs)
    {
        Spec s = empty();
        for (int i = 0; i < 7; ++i) s[(size_t) harm[i]] = r[i] > 0 ? std::pow (10.0f, (r[i] - 8.0f) * 3.0f / 20.0f) : 0.0f;
        v.push_back (s);
    }
    return v;
}

struct Banks
{
    WavetableBank b[4];
    Banks() { b[0].build (analog()); b[1].build (digital()); b[2].build (vocal()); b[3].build (organ()); }
};
} // namespace

const WavetableBank& getWavetable (int bank)
{
    static const Banks banks; // thread-safe static init, built once per process
    return banks.b[std::min (3, std::max (0, bank))];
}
} // namespace dsp
