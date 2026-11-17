# Fretwire: guitar synthesiser (design plan, v0.1)

Working name **Fretwire** (to be confirmed). A synth you play *with a guitar*: plug the guitar
in, Fretwire tracks pitch and pick dynamics and drives synth voices, blended with the dry guitar
and an FX rack. It also takes MIDI, so it works as a normal keyboard synth, and it includes a
physical string model so it can also *sound like* a guitar from a keyboard.

Stack: JUCE 8.0.9, C++17, CMake; VST3 + Standalone (AU is one word in `FORMATS`). macOS 11+
universal (no AVX/AVX2, plain SSE2 baseline), Windows with static runtime. Same layout as Paisley.

## Signal flow

Guitar in (mono / stereo summed / **6-channel hex pickup = one tracker per string**)
→ pitch tracker → note + bend + dynamics events → 16-voice synth (+ MIDI notes)
→ dry guitar blend → Drive > Phaser > Chorus/Flanger/Ensemble > Tremolo > Delay > Reverb → out.

## Pitch tracker (`Source/PitchTracker.h`)

- YIN on a band-limited signal decimated to 16 kHz (8 kHz in Bass range); analysis every 2 ms.
- Multi-resolution windows: high notes are found from a short window, low notes fall back to
  the full window. Octave-up guard, sub-octave guard, first-guess correction during the attack.
- Note logic: gradual changes are bends (pitch follows, no re-trigger); stable jumps are new
  notes; re-picking the same note re-triggers; octave jumps need 3 confirmations.
- Pitch Snap (0 = follow bends exactly, 1 = semitones), Octave/Transpose, Retrigger/Legato,
  Harmony (Oct up/down, Fifth, Power, Octaves, Major, Minor, Sus4, Maj7, Min7, Fourths; parallel,
  not key-aware yet), Pick Dynamics → velocity, Amp Follows Guitar, Guitar Env → Filter.
- Measured on a synthetic plucked string (`FretwireRender track`): every note E1–E6 correct,
  one note-on per pluck, bends stay one note; detection ~8–16 ms above A3, ~30 ms on low E
  (≈2.5 string periods), mean 22 ms.

## Synth voice (`Source/SynthEngine.*`)

- 2 oscillators (wave, octave, semi, fine, shape, level, unison 1–5 + detune, stereo spread):
  Sine/Triangle (shape = fold), Saw (shape = hollow), Pulse (PWM), 4 morphing wavetable banks
  (Analog, Digital sweep, Vocal vowels, Organ drawbars), FM 1:1 / 2:1 / Bell (shape = index),
  **Pluck** (Karplus-Strong string, shape = brightness), Noise.
- Mixer: sub (sine/square, -1/-2 oct), noise, ring mod, cross-mod B>A.
- **Guitar Direct** sources (zero tracking latency): Clean guitar into the synth filter,
  Square (fuzz-squared guitar), Octave Down (analog flip-flop divider).
- Filter: LP24, LP12, Band, High, Notch, **Vowel** (cutoff sweeps U-O-A-E-I); drive, env,
  key track, velocity, LFO, guitar envelope. Filter env can also drive oscillator Shape.
- ADSR × 2, LFO × 2 (vibrato/cutoff with fade-in; shape/pan/amp), Poly/Mono/Legato + glide
  for keys, analogue drift, pitch bend, mod wheel vibrato, sustain pedal.

## Presets (`tools/gen_presets.py` → `Source/FactoryPresets.cpp`)

316 factory presets in 14 categories: Leads 28, Basses 24, Pads 26, Strings & Brass 22,
Keys & Organs 24, Plucks & Mallets 24, FM & Bells 20, Guitar Layers 24 (synth under the dry
guitar), Envelope Filters 22 and Fuzz & Octave 22 (guitar-direct sounds, guitar only),
Harmony & Chords 22, Vocal & Formant 18, Textures & FX 24, Physical Strings 16.

- Written as a Python bank: each preset is a hand-set core + FX snippets. The generator checks
  every id/range and refuses near-duplicates in parameter space.
- `FretwireRender render` renders every preset twice (guitar phrase + keyboard chord/phrase),
  flags NaN / clipping / level, and lists the closest-sounding pairs by audio fingerprint.
- Loudness-matched: `FretwireRender calibrate` writes `FactoryPresetLevels.h` (gated loudness of
  the guitar phrase at -17 dB, guitar peaks under -1 dB, keyboard chords capped at -9 dB).
  Result: 314/316 within ±3 dB; 2 very short percussive sounds sit a little lower by design.
- Rig parameters (input gain, tracking range/threshold, master) are never changed by presets.
- User presets: `Documents/Fretwire/Presets/<folder>/<name>.fretwire`.

## Themes (`Source/Themes.h`)

Stage Black (tolex pedalboard), Tweed '59, Cherry Sunburst, Blackface, Neon Pedal, Surf Green.
UI scales 50–150% at a fixed aspect ratio; theme is remembered.

## Known limits / next steps

- Polyphonic tracking needs a hex pickup (6-channel input). Polyphony from a normal pickup
  is not attempted in v0.1.
- Built as an effect with MIDI input (`Fx|Instrument`). Some DAWs (e.g. Ableton) don't send
  MIDI to effects; a second "instrument" build target is a small CMake addition.
- Key-aware (diatonic) harmony, a mod matrix, MPE out of the tracker, tracker CPU optimisation
  (FFT difference function) for hex mode, real-guitar test recordings.
