#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/PitchTracker.h"

// ============================================================================
//  FretwireRender: offline QA tool.
//    FretwireRender track                      pitch-tracker accuracy/latency test
//    FretwireRender render <outDir> [--wav]    render every preset (keys + guitar), report problems
//    FretwireRender calibrate <levels.h>       write per-preset loudness levels
//    FretwireRender snap <outDir>              editor screenshot in every theme
// ============================================================================

namespace
{
const double sr = 48000.0;
const int block = 256;

// --------------------------------------------------------- synthetic guitar
// Karplus-Strong string with pick-position comb, string damping and a simple
// body filter; supports bends. Good enough to exercise the tracker.
struct GNote { double start, dur; float midi, bendTo; double bendAt; float amp; };

std::vector<float> synthGuitar (const std::vector<GNote>& notes, double totalSec, uint32_t seed = 7)
{
    const int total = (int) (totalSec * sr);
    std::vector<float> out ((size_t) total, 0.0f);
    juce::Random rnd ((juce::int64) seed);
    for (auto& nt : notes)
    {
        const int s0 = (int) (nt.start * sr), len = (int) (nt.dur * sr);
        std::vector<float> line (8192, 0.0f);
        const float f0 = dsp::midiToHz (nt.midi);
        const int L0 = (int) (sr / f0);
        // excitation: noise burst, low-passed, with pick-position comb (1/5 of the string)
        float lp = 0;
        std::vector<float> exc ((size_t) L0);
        for (int i = 0; i < L0; ++i) { lp += 0.5f * ((rnd.nextFloat() * 2 - 1) - lp); exc[(size_t) i] = lp; }
        for (int i = 0; i < L0; ++i) line[(size_t) i] = exc[(size_t) i] - (i >= L0 / 5 ? exc[(size_t) (i - L0 / 5)] : 0.0f);
        int w = L0;
        float prev = 0, hpZ = 0, bodyZ = 0;
        for (int i = 0; i < len && s0 + i < total; ++i)
        {
            const double t = (double) i / sr;
            float m = nt.midi;
            if (nt.bendTo != nt.midi && t > nt.bendAt) m += (nt.bendTo - nt.midi) * (float) std::min (1.0, (t - nt.bendAt) / 0.18);
            const float Ld = (float) sr / dsp::midiToHz (m) - 0.5f;
            float rp = (float) w - Ld;
            int ri = (int) std::floor (rp); const float fr = rp - (float) ri;
            const float y = line[(size_t) (ri & 8191)] * (1 - fr) + line[(size_t) ((ri + 1) & 8191)] * fr;
            const float fbv = 0.5f * (y + prev) * 0.9985f;
            prev = y;
            line[(size_t) (w & 8191)] = fbv;
            ++w;
            float o = y;
            hpZ += 0.01f * (o - hpZ); o -= hpZ;               // ~75 Hz high-pass
            bodyZ += 0.45f * (o - bodyZ); o = bodyZ;          // pickup/body roll-off
            const float fadeOut = std::min (1.0f, (float) (len - i) / (0.015f * (float) sr));
            out[(size_t) (s0 + i)] += o * nt.amp * fadeOut;
        }
    }
    for (auto& x : out) x += (float) (rnd.nextFloat() * 2 - 1) * 3e-5f; // -90 dB hiss
    return out;
}

void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b)
{
    f.getParentDirectory().createDirectory(); f.deleteFile();
    juce::WavAudioFormat wav;
    if (auto os = f.createOutputStream())
        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), sr, (unsigned) b.getNumChannels(), 16, {}, 0)))
        { os.release(); w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); }
}

// ------------------------------------------------------------ tracker test
int trackTest()
{
    int fails = 0;
    double latSum = 0; int latN = 0;
    std::cout << "note  expect  got     latency  noteOns\n";
    for (int bassRange = 0; bassRange < 2; ++bassRange)
    {
        const int lo = bassRange ? 28 : 40, hi = bassRange ? 60 : 88;
        for (int midi = lo; midi <= hi; midi += 3)
        {
            auto sig = synthGuitar ({ { 0.1, 1.0, (float) midi, (float) midi, 0, 0.25f } }, 1.3, (uint32_t) midi);
            PitchTracker t;
            t.prepare (sr, bassRange == 1);
            t.setThresholdDb (-54.0f);
            std::vector<PitchTracker::Event> ev;
            std::vector<float> fol ((size_t) block);
            float got = -1; double lat = -1; int ons = 0; float lastPitch = -1;
            for (int pos = 0; pos + block <= (int) sig.size(); pos += block)
            {
                ev.clear();
                t.process (sig.data() + pos, block, ev, fol.data());
                for (auto& e : ev)
                {
                    if (e.type == PitchTracker::Event::NoteOn)
                    {
                        ++ons;
                        if (got < 0) { got = e.note; lat = ((pos + e.offset) / sr - 0.1) * 1000.0; }
                    }
                    if (e.type != PitchTracker::Event::NoteOff) lastPitch = e.note;
                }
            }
            const bool ok = got >= 0 && ons == 1 && std::abs (lastPitch - (float) midi) < 0.5f;
            const bool corrected = ok && std::abs (got - (float) midi) >= 0.5f;
            if (! ok) ++fails;
            if (lat >= 0) { latSum += lat; ++latN; }
            std::cout << (bassRange ? "B " : "G ") << juce::String (midi).paddedLeft (' ', 3) << "  " << juce::String (dsp::midiToHz ((float) midi), 1).paddedLeft (' ', 6)
                      << "  " << juce::String (got, 2).paddedLeft (' ', 6) << "  " << juce::String (lat, 1).paddedLeft (' ', 6) << " ms  "
                      << ons << (ok ? (corrected ? "   (first guess corrected within attack)" : "") : "   <-- FAIL") << "\n";
        }
    }
    // bend test: A3 bent up a whole tone must stay ONE note with pitch events following
    {
        auto sig = synthGuitar ({ { 0.1, 1.2, 57, 59, 0.4, 0.25f } }, 1.5);
        PitchTracker t; t.prepare (sr, false); t.setThresholdDb (-54.0f);
        std::vector<PitchTracker::Event> ev; std::vector<float> fol ((size_t) block);
        int ons = 0; float last = 0;
        for (int pos = 0; pos + block <= (int) sig.size(); pos += block)
        {
            ev.clear(); t.process (sig.data() + pos, block, ev, fol.data());
            for (auto& e : ev) { if (e.type == PitchTracker::Event::NoteOn) ++ons; if (e.type != PitchTracker::Event::NoteOff) last = e.note; }
        }
        const bool ok = ons == 1 && std::abs (last - 59.0f) < 0.4f;
        std::cout << "bend A3->B3: noteOns " << ons << ", final pitch " << last << (ok ? "  ok" : "  <-- FAIL") << "\n";
        if (! ok) ++fails;
    }
    // phrase test: 5 consecutive notes, each must produce exactly one note-on at the right pitch
    {
        const float ph[] = { 52, 55, 57, 59, 64 };
        std::vector<GNote> ns;
        for (int i = 0; i < 5; ++i) ns.push_back ({ 0.1 + i * 0.25, 0.25, ph[i], ph[i], 0, 0.25f });
        auto sig = synthGuitar (ns, 1.6);
        PitchTracker t; t.prepare (sr, false); t.setThresholdDb (-54.0f);
        std::vector<PitchTracker::Event> ev; std::vector<float> fol ((size_t) block);
        std::vector<float> got;
        for (int pos = 0; pos + block <= (int) sig.size(); pos += block)
        {
            ev.clear(); t.process (sig.data() + pos, block, ev, fol.data());
            for (auto& e : ev) if (e.type == PitchTracker::Event::NoteOn) got.push_back (std::round (e.note));
        }
        bool ok = got.size() == 5;
        for (size_t i = 0; ok && i < 5; ++i) ok = std::abs (got[i] - ph[i]) < 0.5f;
        std::cout << "phrase E3 G3 A3 B3 E4 (250 ms each): got";
        for (auto g : got) std::cout << " " << g;
        std::cout << (ok ? "  ok" : "  <-- FAIL") << "\n";
        if (! ok) ++fails;
    }
    std::cout << "mean detection latency " << juce::String (latSum / std::max (1, latN), 1) << " ms, failures: " << fails << std::endl;
    return fails == 0 ? 0 : 1;
}

// ------------------------------------------------------------ preset render
struct Render { juce::AudioBuffer<float> audio; float rms = 0, peak = 0; bool nan = false; };

void setParam (FretwireProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

// mode 0: MIDI keys (chord for poly presets, phrase for mono), mode 1: synthetic guitar phrase
Render renderPreset (FretwireProcessor& proc, int mode, const std::vector<float>& guitar)
{
    proc.prepareToPlay (sr, block);
    const int total = (int) (sr * 6.0);
    Render r;
    r.audio.setSize (2, total);
    r.audio.clear();
    const bool mono = proc.apvts.getRawParameterValue ("voiceMode")->load() > 0.5f;
    const int notes[] = { 48, 52, 55, 59 };
    const int phrase[] = { 48, 55, 60, 58, 55 };
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        juce::AudioBuffer<float> buf (2, n);
        buf.clear();
        juce::MidiBuffer midi;
        auto at = [&] (double sec) { int s = (int) (sec * sr); return s >= pos && s < pos + n ? s - pos : -1; };
        if (mode == 1)
            for (int c = 0; c < 2; ++c) buf.copyFrom (c, 0, guitar.data() + pos, n);
        else if (mono)
            for (int k = 0; k < 5; ++k)
            {
                if (int t = at (k * 0.5); t >= 0) midi.addEvent (juce::MidiMessage::noteOn (1, phrase[k], 0.8f), t);
                if (int t = at (k * 0.5 + 0.45); t >= 0) midi.addEvent (juce::MidiMessage::noteOff (1, phrase[k]), t);
            }
        else
            for (int nn : notes)
            {
                if (int t = at (0.0); t >= 0) midi.addEvent (juce::MidiMessage::noteOn (1, nn, 0.8f), t);
                if (int t = at (2.5); t >= 0) midi.addEvent (juce::MidiMessage::noteOff (1, nn), t);
            }
        proc.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) r.audio.copyFrom (c, pos, buf, c, 0, n);
    }
    r.peak = r.audio.getMagnitude (0, total);
    // gated loudness (LUFS-like, unweighted): 100 ms windows, absolute gate -60 dB,
    // relative gate 10 dB under the mean, so short plucks and long pads compare fairly
    {
        const int win = (int) (sr * 0.1);
        std::vector<double> pw;
        for (int s0 = 0; s0 + win <= total; s0 += win)
        {
            double acc = 0;
            for (int c = 0; c < 2; ++c) { const float* d = r.audio.getReadPointer (c, s0); for (int i = 0; i < win; ++i) acc += (double) d[i] * d[i]; }
            pw.push_back (acc / (2.0 * win));
        }
        double m1 = 0; int n1 = 0;
        for (double v : pw) if (v > 1e-6) { m1 += v; ++n1; }
        m1 = n1 > 0 ? m1 / n1 : 0;
        double m2 = 0; int n2 = 0;
        for (double v : pw) if (v > 1e-6 && v > m1 * 0.1) { m2 += v; ++n2; }
        r.rms = n2 > 0 ? (float) std::sqrt (m2 / n2) : 0.0f;
    }
    for (int c = 0; c < 2; ++c) for (int i = 0; i < total; ++i) if (! std::isfinite (r.audio.getSample (c, i))) r.nan = true;
    return r;
}

// Guitar phrase used for renders and calibration: E3, G3, A3 bent to B3, low E2 ringing.
std::vector<float> guitarPhrase()
{
    return synthGuitar ({ { 0.05, 0.9, 52, 52, 0, 0.22f }, { 1.0, 0.9, 55, 55, 0, 0.2f },
                          { 2.0, 1.4, 57, 59, 0.5, 0.22f }, { 3.5, 2.2, 40, 40, 0, 0.3f } }, 6.0);
}

// coarse audio fingerprint for the duplicate check: per-band energy over 3 time slices + width
std::vector<float> fingerprint (const juce::AudioBuffer<float>& a)
{
    std::vector<float> f;
    const int n = a.getNumSamples();
    const float bands[] = { 150, 400, 1000, 2500, 6000 };
    for (int slice = 0; slice < 3; ++slice)
    {
        const int s0 = slice * n / 4, s1 = s0 + n / 4;
        dsp::OnePole lp[5];
        for (int b = 0; b < 5; ++b) lp[b].setLP (bands[b], (float) sr);
        float e[6] {}, prevLp = 0;
        float tot = 1e-9f;
        for (int i = s0; i < s1; ++i)
        {
            const float x = 0.5f * (a.getSample (0, i) + a.getSample (1, i));
            prevLp = 0;
            for (int b = 0; b < 5; ++b) { const float y = lp[b].lp (x); e[b] += (y - prevLp) * (y - prevLp); prevLp = y; }
            e[5] += (x - prevLp) * (x - prevLp);
        }
        for (float v : e) tot += v;
        for (float v : e) f.push_back (std::sqrt (v / tot));
        f.push_back (0.25f * std::log10 (tot / (float) (s1 - s0) + 1e-9f));
    }
    float side = 0, mid = 1e-9f;
    for (int i = 0; i < n; ++i) { const float l = a.getSample (0, i), r = a.getSample (1, i); side += (l - r) * (l - r); mid += (l + r) * (l + r); }
    f.push_back (std::sqrt (side / mid));
    return f;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String cmd = argc > 1 ? juce::String (argv[1]) : "render";
    if (cmd == "track") return trackTest();

    FretwireProcessor proc;
    proc.enableAllBuses();
    proc.setPlayConfigDetails (2, 2, sr, block);
    auto& pm = proc.presets;

    if (cmd == "snap")
    {
        juce::File out (argc > 2 ? juce::String (argv[2]) : "snaps");
        out.createDirectory();
        pm.load (0);
        for (int t = 0; t < (int) getThemes().size(); ++t)
        {
            proc.setThemeIndex (t);
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            ed->setSize (FretwireEditor::designW, FretwireEditor::designH);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto f = out.getChildFile ("theme-" + juce::String (t + 1) + "-" + juce::File::createLegalFileName (getThemes()[(size_t) t].name).replace (" ", "-") + ".png");
            f.deleteFile();
            juce::PNGImageFormat png;
            if (auto os = f.createOutputStream()) png.writeImageToStream (img, *os);
        }
        proc.setThemeIndex (0);
        return 0;
    }

    const bool calibrate = cmd == "calibrate";
    const bool wav = argc > 3 && juce::String (argv[3]) == "--wav";
    juce::File out (argc > 2 ? juce::String (argv[2]) : "render");
    const auto guitar = guitarPhrase();
    if (wav) { juce::AudioBuffer<float> g (1, (int) guitar.size()); g.copyFrom (0, 0, guitar.data(), (int) guitar.size()); writeWav (out.getChildFile ("_guitar-input.wav"), g); }

    juce::String report, levels;
    int problems = 0;
    std::vector<std::vector<float>> prints;
    std::vector<juce::String> names;
    const float targetDb = -17.0f; // guitar-phrase RMS target

    for (int p = 0; p < pm.getEntries().size(); ++p)
    {
        const auto e = pm.getEntries()[p];
        if (e.factoryIndex < 0) continue;
        pm.load (p);
        if (calibrate) setParam (proc, "level", 0.5f);
        auto g = renderPreset (proc, 1, guitar);
        auto k = renderPreset (proc, 0, guitar);
        if (calibrate)
        {
            // level so the guitar phrase hits the target RMS; also keep MIDI-chord peaks under -1 dBFS
            const float tgt = juce::Decibels::decibelsToGain (targetDb);
            float m = 0.5f * tgt / juce::jmax (g.rms, 1e-5f);
            // keyboard chords may be louder than single guitar notes, but not much louder than -9 dB
            if (k.rms > 1e-5f) m = juce::jmin (m, 0.5f * juce::Decibels::decibelsToGain (-9.0f) / k.rms);
            m = juce::jmin (m, 0.5f * 0.9f / juce::jmax (g.peak, 1e-5f)); // no clipper on guitar peaks
            m = juce::jlimit (0.05f, 8.0f, m);
            levels << "    { \"" << e.name << "\", " << juce::String (m, 3) << "f },\n";
            continue;
        }
        juce::String flag;
        if (g.nan || k.nan) flag << " NAN";
                { const float db = juce::Decibels::gainToDecibels (g.rms); if (db > targetDb + 3.0f || (db < targetDb - 3.0f && g.peak < 0.85f)) flag << " LEVEL"; }
        if (g.peak > 0.97f) flag << " HOT";
        if (flag.isNotEmpty()) ++problems;
        report << juce::String (p).paddedLeft ('0', 3) << " " << e.category.paddedRight (' ', 14) << e.name.paddedRight (' ', 26)
               << " gtr " << juce::String (juce::Decibels::gainToDecibels (g.rms), 1) << " dB pk " << juce::String (g.peak, 2)
               << " | keys " << juce::String (juce::Decibels::gainToDecibels (k.rms), 1) << " dB pk " << juce::String (k.peak, 2) << flag << "\n";
        auto fp = fingerprint (g.audio);
        auto fk = fingerprint (k.audio);
        fp.insert (fp.end(), fk.begin(), fk.end());
        prints.push_back (fp);
        names.push_back (e.category + "/" + e.name);
        if (wav)
        {
            writeWav (out.getChildFile ("guitar").getChildFile (e.category + " - " + e.name + ".wav"), g.audio);
            writeWav (out.getChildFile ("keys").getChildFile (e.category + " - " + e.name + ".wav"), k.audio);
        }
    }

    if (calibrate)
    {
        juce::File (out).replaceWithText ("// Generated by FretwireRender calibrate: per-preset output level for even loudness.\n#pragma once\n#include <map>\n#include <string>\ninline const std::map<std::string, float> factoryPresetLevels {\n" + levels + "};\n");
        std::cout << "wrote " << out.getFullPathName() << std::endl;
        return 0;
    }

    // ---- near-duplicate check on the audio fingerprints
    struct Pair { float d; int a, b; };
    std::vector<Pair> pairs;
    for (size_t a = 0; a < prints.size(); ++a)
        for (size_t b = a + 1; b < prints.size(); ++b)
        {
            float d = 0;
            for (size_t i = 0; i < prints[a].size(); ++i) d += (prints[a][i] - prints[b][i]) * (prints[a][i] - prints[b][i]);
            pairs.push_back ({ std::sqrt (d), (int) a, (int) b });
        }
    std::sort (pairs.begin(), pairs.end(), [] (auto& x, auto& y) { return x.d < y.d; });
    report << "\nClosest-sounding preset pairs (audio fingerprint distance):\n";
    for (size_t i = 0; i < std::min<size_t> (15, pairs.size()); ++i)
        report << "  " << juce::String (pairs[i].d, 3) << "  " << names[(size_t) pairs[i].a] << "  ~  " << names[(size_t) pairs[i].b] << "\n";
    int dupes = 0;
    for (auto& pr : pairs) if (pr.d < 0.02f) ++dupes;
    report << "near-identical pairs (< 0.02): " << dupes << "\n";
    report << "presets: " << (int) prints.size() << ", with problems: " << problems << "\n";
    std::cout << report;
    out.createDirectory();
    out.getChildFile ("report.txt").replaceWithText (report);
    return problems == 0 && dupes == 0 ? 0 : 1;
}
