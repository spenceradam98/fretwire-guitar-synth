#!/usr/bin/env python3
"""Generates Source/FactoryPresets.cpp from the bank below.

Each preset = a hand-written core (oscillators, filter, envelopes, tracking
behaviour) + FX "pedalboard" snippets. Values are in real parameter units.
The script validates every id and range, and refuses to write the bank if two
presets are too close in parameter space (so the bank stays genuinely varied).

Run:  python3 tools/gen_presets.py
"""
import itertools, math, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "Source")

# ---- wave / mode indices (see Params.h)
SINE, TRI, SAW, PULSE, WTA, WTD, WTV, WTO, FM11, FM21, BELL, PLUCK, NOISE = range(13)
LP24, LP12, BP, HP, NOTCH, VOWEL = range(6)
L_SINE, L_TRI, L_SAW, L_SQ, L_SH, L_DRIFT = range(6)
POLY, MONO, LEGATO = range(3)
C_OFF, C_OCTUP, C_OCTDN, C_FIFTH, C_POWER, C_OCTAVES, C_MAJ, C_MIN, C_SUS4, C_MAJ7, C_MIN7, C_FOURTHS = range(12)
WARM, FUZZ, CRUNCH, CRUSH = range(4)

# ---- FX snippets
def fx(**kw): return kw
room      = fx(revOn=1, revSize=0.4, revDamp=0.5, revMix=0.18)
hall      = fx(revOn=1, revSize=0.78, revDamp=0.4, revMix=0.3)
plate     = fx(revOn=1, revSize=0.6, revDamp=0.15, revMix=0.26)
cathedral = fx(revOn=1, revSize=1.0, revDamp=0.25, revMix=0.45)
slap      = fx(dlyOn=1, dlySync=0, dlyMs=110, dlyFb=0.12, dlyTone=0.6, dlyMix=0.22)
echo8d    = fx(dlyOn=1, dlyDiv=6, dlyFb=0.35, dlyTone=0.55, dlyMix=0.24)
echo4     = fx(dlyOn=1, dlyDiv=8, dlyFb=0.4, dlyTone=0.5, dlyMix=0.22)
ping      = fx(dlyOn=1, dlyDiv=5, dlyFb=0.45, dlyPing=1, dlyTone=0.6, dlyMix=0.26)
tapeEcho  = fx(dlyOn=1, dlyDiv=9, dlyFb=0.55, dlyTone=0.3, dlyMix=0.28)
longEcho  = fx(dlyOn=1, dlyDiv=10, dlyFb=0.6, dlyPing=1, dlyTone=0.45, dlyMix=0.3)
chorus    = fx(choOn=1, choMode=0, choRate=0.5, choDepth=0.5, choMix=0.45)
ensemble  = fx(choOn=1, choMode=2, choRate=0.6, choDepth=0.7, choMix=0.6)
flanger   = fx(choOn=1, choMode=1, choRate=0.15, choDepth=0.85, choFb=0.7, choMix=0.5)
phaseSlow = fx(phsOn=1, phsRate=0.15, phsDepth=0.85, phsFb=0.6, phsMix=0.55)
phaseFast = fx(phsOn=1, phsRate=2.5, phsDepth=0.6, phsFb=0.4, phsMix=0.45)
trem      = fx(trmOn=1, trmShape=0, trmRate=5.5, trmDepth=0.45)
chop      = fx(trmOn=1, trmShape=1, trmRate=7.0, trmDepth=0.8)
autopan   = fx(trmOn=1, trmShape=0, trmRate=0.8, trmDepth=0.6, trmPan=1)
warm      = fx(drvOn=1, drvType=WARM, drvAmt=0.35, drvTone=0.6)
fuzz      = fx(drvOn=1, drvType=FUZZ, drvAmt=0.5, drvTone=0.45)
crunch    = fx(drvOn=1, drvType=CRUNCH, drvAmt=0.45, drvTone=0.55)
crush     = fx(drvOn=1, drvType=CRUSH, drvAmt=0.55, drvTone=0.8, drvMix=0.8)
vib       = fx(lfo1Wave=L_SINE, lfo1Rate=5.5, lfo1Delay=0.5, lfo1Pitch=0.22)

BANK = []
def P(cat, name, core, *snips):
    d = {}
    for s in snips: d.update(s)
    d.update(core)
    BANK.append((name, cat, d))

# behaviour presets per family, merged under each core
LEADB  = dict(ampFollow=0.7, trkQuant=0.4, voiceMode=LEGATO, glide=0.03)
BASSB  = dict(ampFollow=0.6, trkQuant=0.85, trkOct=-1, voiceMode=MONO)
PADB   = dict(ampFollow=0.15, trkQuant=0.95, trkMode=1)
KEYB   = dict(ampFollow=0.35, trkQuant=1.0)
PLUCKB = dict(ampFollow=0.0, trkQuant=1.0)

# ============================================================== LEADS
C = "Leads"
P(C, "Fuzz Saw Lead", dict(LEADB, oscAWave=SAW, oscBWave=SAW, oscBFine=9, oscBLevel=0.6, cutoff=2600, reso=0.3, filtEnv=0.3, fD=0.5), warm, echo8d, room)
P(C, "Square Hero", dict(LEADB, oscAWave=PULSE, oscAShape=0.15, oscBWave=PULSE, oscBOct=1, oscBLevel=0.3, cutoff=3200, reso=0.2), echo4, plate)
P(C, "Hot Wire", dict(LEADB, oscAWave=SAW, filtDrive=0.65, reso=0.55, cutoff=1700, filtEnv=0.4, fD=0.3, fS=0.4), fuzz, slap)
P(C, "Sine Whistle", dict(LEADB, oscAWave=SINE, trkQuant=0.9, cutoff=8000, aA=0.02), vib, hall)
P(C, "Prog Mono", dict(LEADB, oscAWave=SAW, oscBWave=PULSE, oscBOct=-1, oscBLevel=0.55, glide=0.12, cutoff=1100, filtEnv=0.55, fD=0.55, fS=0.3, reso=0.38), echo8d)
P(C, "Digital Edge", dict(LEADB, oscAWave=WTD, oscAShape=0.55, lfo2Rate=0.4, lfo2Shape=0.4, cutoff=7000), ping, room)
P(C, "Glass Lead", dict(LEADB, oscAWave=FM11, oscAShape=0.35, oscBWave=SINE, oscBOct=1, oscBLevel=0.35, cutoff=9000, shpEnv=0.3, fD=0.6), plate, echo4)
P(C, "Vowel Lead", dict(LEADB, oscAWave=WTV, oscAShape=0.25, lfo2Rate=0.7, lfo2Shape=0.5, cutoff=6000), vib, room)
P(C, "Anthem Supersaw", dict(LEADB, oscAWave=SAW, oscAUnison=5, oscADetune=0.35, oscBWave=SAW, oscBUnison=3, oscBOct=1, oscBLevel=0.35, cutoff=6500, spread=0.9), chorus, hall)
P(C, "Thin Pulse", dict(LEADB, oscAWave=PULSE, oscAShape=0.8, filtMode=HP, cutoff=450, reso=0.25), slap, room)
P(C, "Wah Lead", dict(LEADB, oscAWave=SAW, filtMode=BP, cutoff=500, reso=0.6, envCut=0.75), warm, echo8d)
P(C, "Octave Lead", dict(LEADB, oscAWave=SAW, oscBWave=SAW, oscBOct=1, oscBLevel=0.5, cutoff=4500, reso=0.15), phaseFast, echo4)
P(C, "Theremin", dict(LEADB, oscAWave=SINE, oscAShape=0.1, trkQuant=0.0, glide=0.25, aA=0.06, aR=0.4, lfo1Rate=6.2, lfo1Pitch=0.3), hall)
P(C, "Tube Lead", dict(LEADB, oscAWave=TRI, filtDrive=0.75, cutoff=3500, reso=0.2), dict(drvOn=1, drvType=WARM, drvAmt=0.7, drvTone=0.5), echo8d, room)
P(C, "Laser Zap", dict(LEADB, oscAWave=SAW, cutoff=300, reso=0.72, filtEnv=0.9, fA=0.001, fD=0.09, fS=0.0), ping)
P(C, "Fifth Stack Lead", dict(LEADB, oscAWave=SAW, oscBWave=SAW, oscBSemi=7, oscBLevel=0.55, cutoff=3000, reso=0.2), crunch, echo4)
P(C, "Chip Hero", dict(LEADB, oscAWave=PULSE, oscAShape=0.5, trkQuant=1.0, glide=0.0, cutoff=12000), crush, slap)
P(C, "Brass Lead", dict(LEADB, oscAWave=SAW, cutoff=800, filtEnv=0.6, fA=0.04, fD=0.35, fS=0.45, aA=0.02), vib, hall)
P(C, "Ring Lead", dict(LEADB, oscAWave=SINE, oscALevel=0.4, oscBWave=SINE, oscBSemi=7, oscBFine=12, ringLevel=0.8, cutoff=6000), echo8d, room)
P(C, "Breath Flute Lead", dict(LEADB, oscAWave=SINE, oscBWave=TRI, oscBOct=1, oscBLevel=0.2, noiseLevel=0.18, cutoff=3500, aA=0.05), vib, plate)
P(C, "Fusion Smooth", dict(LEADB, oscAWave=TRI, oscBWave=SAW, oscBLevel=0.3, filtMode=LP12, cutoff=2200, reso=0.2), chorus, echo8d, room)
P(C, "Detuned Pulse Lead", dict(LEADB, oscAWave=PULSE, oscAShape=0.3, oscAUnison=3, oscADetune=0.25, cutoff=4000, lfo2Rate=0.5, lfo2Shape=0.3), echo4, hall)
P(C, "Bit Lead", dict(LEADB, oscAWave=WTD, oscAShape=0.9, cutoff=9000), crush, ping)
P(C, "Glide Mono", dict(LEADB, oscAWave=SAW, oscBWave=PULSE, oscBLevel=0.4, oscBFine=-7, glide=0.28, trkMode=1, cutoff=2400, reso=0.3), echo4, room)
P(C, "Hollow Square", dict(LEADB, oscAWave=WTA, oscAShape=0.72, cutoff=5000, lfo1Rate=4.8, lfo1Pitch=0.12), phaseSlow, room)
P(C, "Infinite Sustainer", dict(LEADB, oscAWave=SAW, oscBWave=SINE, oscBOct=1, oscBLevel=0.4, ampFollow=0.0, aS=1.0, aR=0.8, cutoff=3000, reso=0.25), echo4, hall)
P(C, "Acid Line", dict(LEADB, oscAWave=SAW, cutoff=320, reso=0.82, filtEnv=0.7, fD=0.16, fS=0.05, voiceMode=MONO, glide=0.05), dict(drvOn=1, drvType=CRUNCH, drvAmt=0.3), slap)
P(C, "Triangle Sweet", dict(LEADB, oscAWave=TRI, oscBWave=TRI, oscBOct=1, oscBFine=5, oscBLevel=0.3, cutoff=6000, aA=0.01), vib, echo8d, plate)

# ============================================================== BASSES
C = "Basses"
P(C, "Octave Sub Bass", dict(BASSB, oscAWave=SINE, oscALevel=0.9, subLevel=0.4, cutoff=900), )
P(C, "Moog-ish Saw Bass", dict(BASSB, oscAWave=SAW, oscBWave=SAW, oscBOct=-1, oscBLevel=0.5, cutoff=420, reso=0.35, filtEnv=0.5, fD=0.25, fS=0.2))
P(C, "Square Thump", dict(BASSB, oscAWave=PULSE, subLevel=0.5, subWave=1, cutoff=600, filtEnv=0.4, fD=0.18, fS=0.1, aD=0.5, aS=0.6))
P(C, "Rubber Bass", dict(BASSB, oscAWave=TRI, filtDrive=0.4, cutoff=380, reso=0.6, filtEnv=0.55, fD=0.2, fS=0.0))
P(C, "FM Slap Bass", dict(BASSB, oscAWave=FM11, oscAShape=0.4, shpEnv=0.5, fD=0.15, fS=0.0, cutoff=5000, aD=0.6, aS=0.4))
P(C, "Reese", dict(BASSB, oscAWave=SAW, oscAUnison=2, oscADetune=0.25, oscBWave=SAW, oscBFine=-14, oscBLevel=0.7, cutoff=700, reso=0.2), chorus)
P(C, "Acid Bass", dict(BASSB, oscAWave=SAW, cutoff=260, reso=0.85, filtEnv=0.75, fD=0.2, fS=0.0, glide=0.06), dict(drvOn=1, drvType=WARM, drvAmt=0.4))
P(C, "Funk Wah Bass", dict(BASSB, oscAWave=SAW, filtMode=BP, cutoff=350, reso=0.55, envCut=0.85, subLevel=0.4))
P(C, "Fuzz Bass", dict(BASSB, oscAWave=PULSE, oscAShape=0.35, subLevel=0.35, cutoff=1500), fuzz)
P(C, "Deep Sine Sub", dict(BASSB, trkOct=-2, oscAWave=SINE, oscBWave=TRI, oscBOct=1, oscBLevel=0.2, cutoff=500))
P(C, "Pluck Bass", dict(BASSB, oscAWave=PLUCK, oscAShape=0.45, subLevel=0.3, ampFollow=0.0, aD=1.2, aS=0.0, aR=0.3, cutoff=2500))
P(C, "Growl Bass", dict(BASSB, oscAWave=WTD, oscAShape=0.3, lfo2Rate=3.0, lfo2Shape=0.6, cutoff=900, reso=0.4, filtDrive=0.5))
P(C, "Synth Pop Bass", dict(BASSB, oscAWave=SAW, oscBWave=PULSE, oscBLevel=0.5, cutoff=1200, filtEnv=0.35, fD=0.12, fS=0.3, aD=0.25, aS=0.7))
P(C, "Tuba-ish", dict(BASSB, oscAWave=SAW, cutoff=350, filtEnv=0.45, fA=0.05, fD=0.4, aA=0.03, filtMode=LP12), room)
P(C, "Wobble Bass", dict(BASSB, oscAWave=SAW, oscBWave=PULSE, oscBOct=-1, oscBLevel=0.5, cutoff=400, reso=0.5, lfo1Wave=L_SINE, lfo1Rate=3.0, lfo1Cutoff=0.8))
P(C, "Organ Pedal Bass", dict(BASSB, oscAWave=WTO, oscAShape=0.14, subLevel=0.4, cutoff=3000, aA=0.005, aS=1.0, ampFollow=0.3))
P(C, "Bit Bass", dict(BASSB, oscAWave=PULSE, oscAShape=0.5, cutoff=4000), crush)
P(C, "Ring Bass", dict(BASSB, oscAWave=SINE, oscALevel=0.4, oscBWave=SAW, oscBSemi=-5, ringLevel=0.7, cutoff=1400, subLevel=0.3))
P(C, "Sub + Grit", dict(BASSB, oscAWave=SINE, oscBWave=SAW, oscBOct=1, oscBLevel=0.25, cutoff=1000, filtDrive=0.8))
P(C, "Smooth Fretless", dict(BASSB, oscAWave=TRI, oscBWave=SINE, oscBLevel=0.5, trkQuant=0.2, glide=0.08, cutoff=1200, aA=0.02), chorus, room)
P(C, "Distorted Saw Bass", dict(BASSB, oscAWave=SAW, oscAUnison=3, oscADetune=0.15, cutoff=2000, reso=0.25), crunch)
P(C, "Hollow Bass", dict(BASSB, oscAWave=WTA, oscAShape=0.65, cutoff=1500, filtEnv=0.3, fD=0.2))
P(C, "Guitar Octave Bass", dict(BASSB, oscALevel=0.0, gtrOct=0.9, gtrClean=0.3, cutoff=700, reso=0.2, trkOct=0, ampFollow=0.8))
P(C, "Talking Bass", dict(BASSB, oscAWave=SAW, filtMode=VOWEL, cutoff=300, reso=0.5, envCut=0.7, subLevel=0.35))

# ============================================================== PADS
C = "Pads"
P(C, "Warm Analog Pad", dict(PADB, oscAWave=SAW, oscAUnison=3, oscADetune=0.3, oscBWave=SAW, oscBOct=-1, oscBLevel=0.4, cutoff=1500, aA=0.6, aR=1.5, fA=0.8, filtEnv=0.25), chorus, hall)
P(C, "Glass Pad", dict(PADB, oscAWave=FM11, oscAShape=0.25, oscBWave=SINE, oscBOct=1, oscBLevel=0.3, aA=0.4, aR=2.0, cutoff=8000), plate, longEcho)
P(C, "Choir Pad", dict(PADB, oscAWave=WTV, oscAShape=0.6, oscAUnison=3, oscADetune=0.2, aA=0.5, aR=1.8, cutoff=5000, lfo2Rate=0.15, lfo2Shape=0.3), ensemble, cathedral)
P(C, "Sweep Pad", dict(PADB, oscAWave=SAW, oscAUnison=4, oscADetune=0.3, cutoff=500, reso=0.4, lfo1Wave=L_TRI, lfo1Rate=0.08, lfo1Cutoff=0.9, aA=0.7, aR=2.0), phaseSlow, hall)
P(C, "Digital Shimmer", dict(PADB, oscAWave=WTD, oscAShape=0.4, lfo2Rate=0.12, lfo2Shape=0.6, oscBWave=SINE, oscBOct=2, oscBLevel=0.15, aA=0.8, aR=2.5), ping, cathedral)
P(C, "Dark Drone Pad", dict(PADB, oscAWave=SAW, oscBWave=PULSE, oscBOct=-1, oscBLevel=0.6, cutoff=380, reso=0.3, aA=1.2, aR=3.0, drift=0.4), tapeEcho, hall)
P(C, "Breathing Strings Pad", dict(PADB, oscAWave=SAW, oscAUnison=5, oscADetune=0.4, cutoff=2500, lfo2Rate=0.25, lfo2Amp=0.35, aA=0.5, aR=1.5), ensemble, hall)
P(C, "Pulse Wash", dict(PADB, oscAWave=PULSE, oscAUnison=3, oscADetune=0.2, lfo2Rate=0.3, lfo2Shape=0.8, cutoff=3000, aA=0.4, aR=1.6), chorus, longEcho)
P(C, "Fifths Pad", dict(PADB, oscAWave=SAW, oscBWave=SAW, oscBSemi=7, oscBLevel=0.6, cutoff=1800, aA=0.5, aR=1.5, trkChord=C_OCTAVES), chorus, hall)
P(C, "Organ Haze", dict(PADB, oscAWave=WTO, oscAShape=0.4, aA=0.3, aR=1.2, cutoff=4000, drift=0.3), phaseSlow, tapeEcho, hall)
P(C, "Ice Bloom", dict(PADB, oscAWave=TRI, oscAUnison=4, oscADetune=0.15, oscBWave=BELL, oscBShape=0.2, oscBOct=1, oscBLevel=0.2, aA=0.9, aR=2.5, cutoff=7000), cathedral, ping)
P(C, "Vowel Morph Pad", dict(PADB, oscAWave=SAW, oscAUnison=3, filtMode=VOWEL, cutoff=200, reso=0.4, lfo1Rate=0.1, lfo1Cutoff=1.0, aA=0.6, aR=1.8), hall)
P(C, "Noise Swell", dict(PADB, oscAWave=SAW, oscALevel=0.5, noiseLevel=0.6, filtMode=BP, cutoff=1500, reso=0.5, aA=1.5, aR=2.5, lfo1Rate=0.15, lfo1Cutoff=0.6), cathedral)
P(C, "80s Poly Pad", dict(PADB, oscAWave=PULSE, oscAShape=0.25, oscBWave=SAW, oscBFine=8, oscBLevel=0.7, cutoff=2200, filtEnv=0.3, fA=0.4, fD=1.0, aA=0.25, aR=1.0), chorus, plate)
P(C, "Tape Pad", dict(PADB, oscAWave=TRI, oscBWave=SAW, oscBLevel=0.3, cutoff=2000, drift=0.7, aA=0.4, aR=1.2), dict(choOn=1, choMode=0, choRate=0.3, choDepth=0.9, choMix=0.5), tapeEcho)
P(C, "Hex Cloud", dict(PADB, oscAWave=WTA, oscAShape=0.3, oscAUnison=5, oscADetune=0.5, spread=1.0, aA=1.0, aR=3.0, cutoff=3000, trkChord=C_FIFTH), longEcho, cathedral)
P(C, "Sub Pad", dict(PADB, oscAWave=SINE, oscBWave=TRI, oscBOct=1, oscBLevel=0.4, subLevel=0.5, aA=0.5, aR=1.5, cutoff=1500), room)
P(C, "Bell Pad", dict(PADB, oscAWave=BELL, oscAShape=0.3, oscBWave=SAW, oscBLevel=0.25, cutoff=2500, aA=0.3, aR=2.0, shpEnv=-0.2, fA=1.0), hall, echo4)
P(C, "Flanged Pad", dict(PADB, oscAWave=SAW, oscAUnison=3, oscADetune=0.2, cutoff=3500, aA=0.6, aR=1.4), flanger, hall)
P(C, "Minor Pad", dict(PADB, oscAWave=SAW, oscAUnison=2, cutoff=1300, aA=0.5, aR=1.5, trkChord=C_MIN), chorus, hall)
P(C, "Major Pad", dict(PADB, oscAWave=TRI, oscBWave=SAW, oscBLevel=0.35, cutoff=2500, aA=0.5, aR=1.5, trkChord=C_MAJ), ensemble, plate)
P(C, "Grainy Pad", dict(PADB, oscAWave=WTD, oscAShape=0.7, lfo2Wave=L_SH, lfo2Rate=8.0, lfo2Shape=0.25, aA=0.7, aR=2.0, cutoff=4000), dict(drvOn=1, drvType=CRUSH, drvAmt=0.3, drvMix=0.5), hall)
P(C, "Warm Sine Swell", dict(PADB, oscAWave=SINE, oscAUnison=3, oscADetune=0.12, aA=1.2, aR=2.0, cutoff=4000), autopan, hall)
P(C, "Underwater Pad", dict(PADB, oscAWave=SAW, cutoff=450, reso=0.5, lfo1Wave=L_SINE, lfo1Rate=0.3, lfo1Cutoff=0.4, aA=0.8, aR=2.0), phaseSlow, chorus, cathedral)
P(C, "Sus4 Air", dict(PADB, oscAWave=WTV, oscAShape=0.85, cutoff=6000, aA=0.7, aR=2.0, trkChord=C_SUS4), longEcho, cathedral)
P(C, "Endless Swell", dict(PADB, oscAWave=SAW, oscBWave=TRI, oscBOct=1, oscBLevel=0.3, ampFollow=0.0, aA=2.5, aS=1.0, aR=3.5, cutoff=2000), hall, tapeEcho)

# ============================================================== STRINGS & BRASS
C = "Strings & Brass"
SB = dict(ampFollow=0.3, trkQuant=0.85, trkMode=1)
P(C, "String Machine", dict(SB, oscAWave=SAW, oscAUnison=3, oscADetune=0.2, oscBWave=SAW, oscBOct=1, oscBLevel=0.4, cutoff=3500, aA=0.15, aR=0.8), ensemble, hall)
P(C, "Cello Section", dict(SB, oscAWave=SAW, oscAUnison=3, oscADetune=0.15, trkOct=-1, cutoff=1400, filtEnv=0.2, fA=0.2, aA=0.12, aR=0.6), vib, hall)
P(C, "Violin Solo", dict(SB, oscAWave=SAW, filtMode=LP12, cutoff=3000, reso=0.25, aA=0.08, aR=0.4, trkQuant=0.3), dict(lfo1Rate=6.0, lfo1Delay=0.3, lfo1Pitch=0.25), plate)
P(C, "Pizzicato", dict(PLUCKB, oscAWave=SAW, oscAUnison=2, cutoff=1500, filtEnv=0.4, fD=0.15, fS=0.0, aD=0.25, aS=0.0, aR=0.2), room)
P(C, "Tremolo Strings", dict(SB, oscAWave=SAW, oscAUnison=4, oscADetune=0.25, cutoff=2800, aA=0.2, aR=0.8, lfo2Wave=L_SQ, lfo2Rate=9.0, lfo2Amp=0.6), hall)
P(C, "Brass Section", dict(SB, oscAWave=SAW, oscBWave=SAW, oscBFine=6, oscBLevel=0.8, cutoff=700, filtEnv=0.6, fA=0.06, fD=0.4, fS=0.5, aA=0.04), room)
P(C, "Synth Brass Stab", dict(KEYB, oscAWave=SAW, oscAUnison=3, oscADetune=0.2, cutoff=600, filtEnv=0.75, fA=0.01, fD=0.25, fS=0.25, aD=0.4, aS=0.5, aR=0.2), plate)
P(C, "French Horn", dict(SB, oscAWave=TRI, oscBWave=SAW, oscBLevel=0.35, cutoff=600, filtEnv=0.4, fA=0.1, aA=0.08, trkOct=-1), hall)
P(C, "Trumpet Lead", dict(LEADB, oscAWave=SAW, cutoff=1000, filtEnv=0.65, fA=0.03, fD=0.2, fS=0.6, aA=0.02, reso=0.15), vib, room)
P(C, "Muted Trumpet", dict(LEADB, oscAWave=SAW, filtMode=BP, cutoff=1400, reso=0.45, filtEnv=0.3, fA=0.03), vib, room)
P(C, "Sax Growl", dict(LEADB, oscAWave=PULSE, oscAShape=0.3, filtMode=VOWEL, cutoff=900, reso=0.35, envCut=0.3, noiseLevel=0.08), warm, room)
P(C, "Oboe Reed", dict(LEADB, oscAWave=PULSE, oscAShape=0.7, filtMode=BP, cutoff=1800, reso=0.35, aA=0.04), vib, plate)
P(C, "Clarinet", dict(LEADB, oscAWave=WTA, oscAShape=0.71, cutoff=2500, aA=0.05), vib, room)
P(C, "Orchestra Hit", dict(PLUCKB, oscAWave=SAW, oscAUnison=5, oscADetune=0.3, oscBWave=NOISE, oscBLevel=0.2, trkChord=C_OCTAVES, cutoff=3000, filtEnv=0.5, fD=0.3, fS=0.0, aD=0.6, aS=0.0, aR=0.6), cathedral)
P(C, "Analog Strings Slow", dict(SB, oscAWave=PULSE, oscAShape=0.3, oscAUnison=3, oscADetune=0.25, cutoff=2000, aA=0.8, aR=1.5, lfo2Rate=0.3, lfo2Shape=0.4), ensemble, cathedral)
P(C, "Octave Strings", dict(SB, oscAWave=SAW, oscAUnison=3, trkChord=C_OCTUP, cutoff=4000, aA=0.25, aR=1.0), ensemble, hall)
P(C, "Spiccato", dict(PLUCKB, oscAWave=SAW, oscAUnison=3, cutoff=2500, aA=0.005, aD=0.12, aS=0.0, aR=0.1, filtEnv=0.2, fD=0.1), room)
P(C, "Glass Harmonica", dict(SB, oscAWave=SINE, oscBWave=SINE, oscBOct=2, oscBLevel=0.25, aA=0.3, aR=1.5, lfo2Rate=4.5, lfo2Amp=0.2), cathedral)
P(C, "Brass Swell", dict(SB, oscAWave=SAW, oscAUnison=2, cutoff=400, filtEnv=0.7, fA=0.8, fD=1.0, fS=0.7, aA=0.6), hall)
P(C, "Power Brass", dict(KEYB, oscAWave=SAW, oscBWave=PULSE, oscBLevel=0.6, trkChord=C_POWER, cutoff=900, filtEnv=0.55, fA=0.02, fD=0.3, fS=0.4), crunch, room)
P(C, "Viola Ensemble", dict(SB, oscAWave=SAW, oscAUnison=4, oscADetune=0.3, filtMode=LP12, cutoff=2200, aA=0.2), dict(lfo1Rate=5.2, lfo1Delay=0.4, lfo1Pitch=0.15), ensemble, hall)
P(C, "Bowed Bass", dict(SB, oscAWave=SAW, trkOct=-1, oscBWave=TRI, oscBOct=-1, oscBLevel=0.5, cutoff=900, aA=0.15, noiseLevel=0.05), hall)

# ============================================================== KEYS & ORGANS
C = "Keys & Organs"
P(C, "Gospel Drawbars", dict(KEYB, oscAWave=WTO, oscAShape=2/7, aA=0.003, aS=1.0, aR=0.05, ampVel=0.0), dict(choOn=1, choMode=0, choRate=6.0, choDepth=0.25, choMix=0.4), room)
P(C, "Jazz Organ", dict(KEYB, oscAWave=WTO, oscAShape=3/7, aS=1.0, aR=0.04, ampVel=0.0, cutoff=7000), dict(trmOn=1, trmRate=6.5, trmDepth=0.25, trmPan=1), room)
P(C, "Full Organ", dict(KEYB, oscAWave=WTO, oscAShape=1.0, subLevel=0.4, aS=1.0, aR=0.06, ampVel=0.0), warm, hall)
P(C, "Combo Organ", dict(KEYB, oscAWave=WTO, oscAShape=5/7, oscBWave=PULSE, oscBLevel=0.2, aS=1.0, aR=0.05, ampVel=0.0, cutoff=5500), vib, room)
P(C, "Church Organ", dict(KEYB, oscAWave=WTO, oscAShape=0.95, oscBWave=WTO, oscBShape=0.1, oscBOct=-1, oscBLevel=0.5, aA=0.08, aS=1.0, aR=0.5, ampVel=0.0), cathedral)
P(C, "Reed Organ", dict(KEYB, oscAWave=WTO, oscAShape=1.0, filtMode=LP12, cutoff=2500, aA=0.05, aS=1.0, aR=0.15, ampVel=0.0), dict(lfo1Rate=4.0, lfo1Pitch=0.05), room)
P(C, "Tine Electric", dict(KEYB, oscAWave=FM11, oscAShape=0.45, shpEnv=0.4, fD=0.5, fS=0.0, aD=2.0, aS=0.3, aR=0.4, oscBWave=SINE, oscBLevel=0.5), trem, room)
P(C, "Reed Electric", dict(KEYB, oscAWave=TRI, oscBWave=FM21, oscBShape=0.35, oscBLevel=0.4, shpEnv=0.3, fD=0.3, fS=0.0, aD=1.5, aS=0.35, cutoff=4000), warm, autopan)
P(C, "Clav Funk", dict(PLUCKB, oscAWave=PULSE, oscAShape=0.6, filtMode=BP, cutoff=1800, reso=0.4, envCut=0.4, aD=0.4, aS=0.2, aR=0.08), phaseFast)
P(C, "Digital Piano", dict(KEYB, oscAWave=FM21, oscAShape=0.3, oscBWave=TRI, oscBLevel=0.5, shpEnv=0.3, fD=0.8, fS=0.0, aD=2.5, aS=0.2, aR=0.5), chorus, room)
P(C, "Toy Piano", dict(PLUCKB, oscAWave=BELL, oscAShape=0.3, oscBWave=SINE, oscBOct=1, oscBLevel=0.3, aD=0.8, aS=0.0, aR=0.3, cutoff=8000), room)
P(C, "Harpsichord", dict(PLUCKB, oscAWave=PLUCK, oscAShape=0.9, oscBWave=PLUCK, oscBShape=0.8, oscBOct=1, oscBLevel=0.4, aD=1.5, aS=0.0, aR=0.15, cutoff=9000), room)
P(C, "Wurly Grit", dict(KEYB, oscAWave=TRI, oscAShape=0.3, filtDrive=0.4, cutoff=2500, aD=1.2, aS=0.4), crunch, trem)
P(C, "Vibes", dict(PLUCKB, oscAWave=SINE, oscBWave=BELL, oscBShape=0.15, oscBOct=2, oscBLevel=0.2, aD=2.0, aS=0.0, aR=1.0), dict(trmOn=1, trmRate=4.5, trmDepth=0.4), hall)
P(C, "Stab Organ", dict(KEYB, oscAWave=WTO, oscAShape=4/7, aD=0.25, aS=0.0, aR=0.1, ampVel=0.3, trkChord=C_MIN7), room)
P(C, "Rotary Drawbars", dict(KEYB, oscAWave=WTO, oscAShape=3/7, aS=1.0, aR=0.05, ampVel=0.0, lfo2Rate=6.5, lfo2Pan=0.6, lfo2Amp=0.2), warm, room)
P(C, "Fairground Organ", dict(KEYB, oscAWave=WTO, oscAShape=6/7, oscBWave=PULSE, oscBOct=1, oscBLevel=0.25, aS=1.0, aR=0.1, ampVel=0.0), dict(lfo1Rate=7.0, lfo1Pitch=0.18), room)
P(C, "Lo-Fi Keys", dict(KEYB, oscAWave=TRI, oscBWave=SINE, oscBOct=1, oscBLevel=0.3, drift=0.6, cutoff=2000, aD=1.5, aS=0.3), dict(choOn=1, choRate=0.4, choDepth=0.8, choMix=0.4), crush, room)
P(C, "Synth Piano", dict(KEYB, oscAWave=SAW, oscBWave=TRI, oscBOct=1, oscBLevel=0.4, cutoff=1500, filtEnv=0.5, fD=0.6, fS=0.1, aD=2.0, aS=0.15), chorus, plate)
P(C, "Accordion", dict(KEYB, oscAWave=WTO, oscAShape=1.0, oscBWave=WTO, oscBShape=0.9, oscBFine=14, oscBLevel=0.6, filtMode=LP12, cutoff=3500, aA=0.03, aS=1.0, aR=0.1, ampVel=0.0), room)
P(C, "Percussive Organ", dict(KEYB, oscAWave=WTO, oscAShape=1/7, oscBWave=SINE, oscBSemi=7, oscBOct=1, oscBLevel=0.6, shpEnv=0.0, aS=0.8, aR=0.05, ampVel=0.0, lfo2Rate=0.1), room)
P(C, "Dark Electric", dict(KEYB, oscAWave=FM11, oscAShape=0.25, cutoff=1200, aD=1.8, aS=0.3, shpEnv=0.2, fD=0.4, fS=0.0), phaseSlow, room)
P(C, "Celesta", dict(PLUCKB, oscAWave=SINE, oscBWave=SINE, oscBOct=2, oscBSemi=0, oscBLevel=0.4, aD=1.2, aS=0.0, aR=0.6, trkOct=1), hall)
P(C, "Steel Pan", dict(PLUCKB, oscAWave=FM21, oscAShape=0.35, shpEnv=0.25, fD=0.2, fS=0.0, aD=0.9, aS=0.0, aR=0.3, cutoff=5000), room)

# ============================================================== PLUCKS & MALLETS
C = "Plucks & Mallets"
P(C, "Synth Pluck", dict(PLUCKB, oscAWave=SAW, cutoff=600, filtEnv=0.65, fD=0.18, fS=0.0, aD=0.5, aS=0.0, aR=0.3), ping, room)
P(C, "Marimba", dict(PLUCKB, oscAWave=SINE, oscBWave=SINE, oscBOct=2, oscBLevel=0.25, aD=0.5, aS=0.0, aR=0.25, cutoff=5000), room)
P(C, "Kalimba", dict(PLUCKB, oscAWave=FM11, oscAShape=0.2, shpEnv=0.4, fD=0.05, fS=0.0, aD=0.7, aS=0.0, aR=0.4, trkOct=1), plate)
P(C, "Harp", dict(PLUCKB, oscAWave=PLUCK, oscAShape=0.55, aS=1.0, aR=1.2, cutoff=6000), hall)
P(C, "Koto", dict(PLUCKB, oscAWave=PLUCK, oscAShape=0.8, oscBWave=PLUCK, oscBShape=0.6, oscBFine=6, oscBLevel=0.5, aS=1.0, aR=0.8, trkQuant=0.5), room)
P(C, "Dub Pluck", dict(PLUCKB, oscAWave=PULSE, oscAShape=0.4, cutoff=800, reso=0.3, filtEnv=0.5, fD=0.12, fS=0.0, aD=0.3, aS=0.0, aR=0.2), tapeEcho)
P(C, "Glass Pluck", dict(PLUCKB, oscAWave=BELL, oscAShape=0.25, oscBWave=TRI, oscBLevel=0.4, aD=1.0, aS=0.0, aR=0.6), ping, plate)
P(C, "Nylon Model", dict(PLUCKB, oscAWave=PLUCK, oscAShape=0.35, aS=1.0, aR=0.4, cutoff=4000), room)
P(C, "Muted Pluck", dict(PLUCKB, oscAWave=SAW, cutoff=400, filtEnv=0.4, fD=0.06, fS=0.0, aD=0.12, aS=0.0, aR=0.1), slap)
P(C, "Xylophone", dict(PLUCKB, oscAWave=TRI, oscBWave=SINE, oscBSemi=7, oscBOct=2, oscBLevel=0.2, aD=0.35, aS=0.0, aR=0.2, trkOct=1, cutoff=9000), room)
P(C, "Arp Pluck", dict(PLUCKB, oscAWave=SAW, oscBWave=PULSE, oscBOct=1, oscBLevel=0.3, cutoff=1500, reso=0.35, filtEnv=0.5, fD=0.1, fS=0.0, aD=0.25, aS=0.0, aR=0.15), echo8d, room)
P(C, "Bell Pluck", dict(PLUCKB, oscAWave=BELL, oscAShape=0.45, aD=1.5, aS=0.0, aR=1.0, cutoff=7000), hall)
P(C, "Wood Block", dict(PLUCKB, oscAWave=SINE, oscBWave=NOISE, oscBLevel=0.15, filtMode=BP, cutoff=1500, reso=0.5, aD=0.08, aS=0.0, aR=0.05, keyTrack=1.0), room)
P(C, "Plastic Pluck", dict(PLUCKB, oscAWave=PULSE, oscAShape=0.7, filtMode=LP12, cutoff=2000, filtEnv=0.4, fD=0.15, fS=0.0, aD=0.35, aS=0.0), chorus, room)
P(C, "Sitar-ish", dict(PLUCKB, oscAWave=PLUCK, oscAShape=0.95, oscBWave=SAW, oscBLevel=0.2, filtMode=BP, cutoff=2500, reso=0.3, aS=1.0, aR=0.8, trkQuant=0.3), dict(lfo2Rate=0.2, lfo2Shape=0.1), room)
P(C, "Music Box", dict(PLUCKB, oscAWave=SINE, oscBWave=BELL, oscBShape=0.1, oscBOct=1, oscBLevel=0.35, aD=1.6, aS=0.0, aR=1.2, trkOct=2), cathedral)
P(C, "Steel Drum Pluck", dict(PLUCKB, oscAWave=FM21, oscAShape=0.5, shpEnv=0.3, fD=0.08, fS=0.0, aD=0.6, aS=0.0), echo8d)
P(C, "Hammered Dulcimer", dict(PLUCKB, oscAWave=PLUCK, oscAShape=0.85, oscAUnison=2, oscADetune=0.1, aS=1.0, aR=1.0), plate)
P(C, "Square Pluck", dict(PLUCKB, oscAWave=PULSE, cutoff=1000, filtEnv=0.6, fD=0.2, fS=0.0, aD=0.6, aS=0.0, aR=0.3), ping)
P(C, "Banjo-ish", dict(PLUCKB, oscAWave=PLUCK, oscAShape=1.0, filtMode=HP, cutoff=250, aS=1.0, aR=0.3), room)
P(C, "Resonant Pluck", dict(PLUCKB, oscAWave=SAW, cutoff=700, reso=0.75, filtEnv=0.5, fD=0.25, fS=0.0, aD=0.7, aS=0.0), echo4)
P(C, "Mallet Pad", dict(PLUCKB, oscAWave=SINE, oscBWave=TRI, oscBOct=1, oscBLevel=0.3, aD=1.0, aS=0.3, aR=1.5), chorus, hall)
P(C, "Digital Pluck", dict(PLUCKB, oscAWave=WTD, oscAShape=0.7, shpEnv=-0.6, fD=0.25, fS=0.0, aD=0.6, aS=0.0), ping, room)
P(C, "Pizz Octaves", dict(PLUCKB, oscAWave=SAW, trkChord=C_OCTUP, cutoff=1800, filtEnv=0.3, fD=0.1, fS=0.0, aD=0.3, aS=0.0), room)

# ============================================================== FM & BELLS
C = "FM & Bells"
P(C, "Tubular Bell", dict(PLUCKB, oscAWave=BELL, oscAShape=0.55, aD=3.0, aS=0.0, aR=2.0), cathedral)
P(C, "FM Electric Piano", dict(KEYB, oscAWave=FM11, oscAShape=0.5, shpEnv=0.5, fD=0.7, fS=0.1, aD=2.5, aS=0.25, oscBWave=FM21, oscBShape=0.2, oscBLevel=0.3), chorus)
P(C, "Metal Mallet", dict(PLUCKB, oscAWave=BELL, oscAShape=0.8, shpEnv=0.2, fD=0.05, fS=0.0, aD=0.6, aS=0.0), plate)
P(C, "FM Brass", dict(LEADB, oscAWave=FM11, oscAShape=0.2, shpEnv=0.5, fA=0.06, fD=0.3, fS=0.6, aA=0.03), room)
P(C, "FM Bass Lead", dict(LEADB, oscAWave=FM21, oscAShape=0.55, cutoff=3000, trkOct=-1), warm)
P(C, "Gong", dict(PADB, oscAWave=BELL, oscAShape=0.9, oscBWave=NOISE, oscBLevel=0.08, aD=4.0, aS=0.0, aR=4.0, trkOct=-1), cathedral)
P(C, "Crystal", dict(PLUCKB, oscAWave=BELL, oscAShape=0.25, oscBWave=SINE, oscBOct=2, oscBLevel=0.3, aD=1.5, aS=0.0, aR=1.5, trkOct=1), longEcho, cathedral)
P(C, "FM Clang", dict(PLUCKB, oscAWave=FM21, oscAShape=0.95, shpEnv=-0.5, fD=0.3, fS=0.0, aD=0.8, aS=0.0), room)
P(C, "Wooden FM", dict(PLUCKB, oscAWave=FM11, oscAShape=0.7, shpEnv=0.3, fD=0.03, fS=0.0, aD=0.2, aS=0.0, aR=0.1, cutoff=3000), room)
P(C, "Cross-Mod Bell", dict(PLUCKB, oscAWave=SINE, oscBWave=SINE, oscBSemi=5, oscBOct=1, oscBLevel=0.0, xmod=0.6, aD=1.5, aS=0.0, aR=1.0), hall)
P(C, "FM Lead Pure", dict(LEADB, oscAWave=FM11, oscAShape=0.3, lfo2Rate=0.3, lfo2Shape=0.3), echo8d, plate)
P(C, "FM Pad Motion", dict(PADB, oscAWave=FM21, oscAShape=0.3, lfo2Wave=L_TRI, lfo2Rate=0.2, lfo2Shape=0.6, aA=0.6, aR=2.0), chorus, hall)
P(C, "Digital Bells", dict(PLUCKB, oscAWave=BELL, oscAShape=0.4, oscBWave=BELL, oscBShape=0.3, oscBSemi=7, oscBLevel=0.5, aD=1.8, aS=0.0, aR=1.5), ping, hall)
P(C, "Church Bells", dict(PLUCKB, oscAWave=BELL, oscAShape=0.65, trkChord=C_OCTDN, aD=4.0, aS=0.0, aR=3.0), cathedral)
P(C, "Ring Bell", dict(PLUCKB, oscAWave=SINE, oscALevel=0.3, oscBWave=TRI, oscBSemi=6, oscBOct=1, ringLevel=0.9, aD=1.4, aS=0.0, aR=1.0), hall)
P(C, "FM Growl", dict(LEADB, oscAWave=FM21, oscAShape=0.7, lfo1Wave=L_SQ, lfo1Rate=6.0, lfo1Cutoff=0.0, lfo2Wave=L_SQ, lfo2Rate=6.0, lfo2Shape=0.5, cutoff=4000), crunch)
P(C, "Hand Bells", dict(PLUCKB, oscAWave=BELL, oscAShape=0.2, aD=1.2, aS=0.0, aR=0.8, trkOct=1, trkChord=C_FIFTH), room)
P(C, "FM Sitar", dict(PLUCKB, oscAWave=FM21, oscAShape=0.8, shpEnv=-0.3, fD=1.0, fS=0.0, aD=1.5, aS=0.2, filtMode=BP, cutoff=2000, reso=0.3), room)
P(C, "Ice Tines", dict(KEYB, oscAWave=FM11, oscAShape=0.6, oscBWave=SINE, oscBOct=2, oscBLevel=0.25, shpEnv=0.3, fD=0.2, fS=0.0, aD=1.5, aS=0.2), ping, plate)
P(C, "FM Organ", dict(KEYB, oscAWave=FM21, oscAShape=0.15, oscBWave=FM11, oscBOct=1, oscBShape=0.15, oscBLevel=0.4, aS=1.0, aR=0.05, ampVel=0.0), room)

# ============================================================== GUITAR LAYERS (synth + dry guitar)
C = "Guitar Layers"
GL = dict(dryLvl=0.8, ampFollow=0.6, trkQuant=0.8)
P(C, "Octave Pad Under", dict(GL, oscAWave=SAW, oscAUnison=3, oscADetune=0.2, trkOct=-1, cutoff=1200, aA=0.3, aR=1.2, synLvl=0.6), chorus, room)
P(C, "Shimmer Layer", dict(GL, oscAWave=SINE, oscBWave=TRI, oscBLevel=0.4, trkOct=1, aA=0.4, aR=2.0, synLvl=0.5), cathedral, longEcho)
P(C, "12-String Sim", dict(GL, oscAWave=PLUCK, oscAShape=0.8, trkOct=1, ampFollow=0.0, aS=1.0, aR=0.5, synLvl=0.5), chorus, room)
P(C, "Organ Double", dict(GL, oscAWave=WTO, oscAShape=3/7, trkQuant=1.0, aS=1.0, aR=0.1, ampVel=0.0, synLvl=0.55), dict(trmOn=1, trmRate=6.0, trmDepth=0.2, trmPan=1), room)
P(C, "Strings Behind", dict(GL, oscAWave=SAW, oscAUnison=4, oscADetune=0.25, cutoff=2500, aA=0.5, aR=1.5, synLvl=0.55, trkMode=1), ensemble, hall)
P(C, "Sub Doubler", dict(GL, oscAWave=SINE, trkOct=-1, cutoff=600, synLvl=0.7, trkQuant=1.0))
P(C, "Fifth Layer", dict(GL, oscAWave=TRI, trkSemi=7, cutoff=4000, synLvl=0.5), room)
P(C, "Bell Doubler", dict(GL, oscAWave=BELL, oscAShape=0.3, trkOct=1, ampFollow=0.0, aD=1.5, aS=0.0, aR=1.0, synLvl=0.45), plate)
P(C, "Brass Doubler", dict(GL, oscAWave=SAW, cutoff=800, filtEnv=0.5, fA=0.04, fD=0.3, synLvl=0.6), room)
P(C, "Choir Behind", dict(GL, oscAWave=WTV, oscAShape=0.7, oscAUnison=3, aA=0.5, aR=1.5, synLvl=0.5, trkMode=1), cathedral)
P(C, "Synth Clean Blend", dict(GL, oscAWave=PULSE, oscAShape=0.3, cutoff=2000, filtEnv=0.3, fD=0.3, synLvl=0.5), chorus, echo8d)
P(C, "Fuzz Synth Blend", dict(GL, dryLvl=0.5, oscAWave=SAW, gtrFuzz=0.5, cutoff=2500, reso=0.3), fuzz, room)
P(C, "Octave Up Sparkle", dict(GL, oscAWave=TRI, oscBWave=SINE, oscBOct=1, oscBLevel=0.3, trkOct=1, cutoff=6000, synLvl=0.4), ping, plate)
P(C, "Ambient Swell Layer", dict(GL, oscAWave=SAW, oscAUnison=3, ampFollow=0.0, aA=1.5, aS=1.0, aR=3.0, cutoff=1800, synLvl=0.5), longEcho, cathedral)
P(C, "Harmonised Thirds", dict(GL, oscAWave=PULSE, oscAShape=0.25, trkSemi=4, cutoff=3500, synLvl=0.55), chorus, room)
P(C, "Clav Doubler", dict(GL, oscAWave=PULSE, oscAShape=0.6, filtMode=BP, cutoff=1500, reso=0.4, envCut=0.4, synLvl=0.5), phaseFast)
P(C, "Piano Doubler", dict(GL, oscAWave=FM21, oscAShape=0.3, shpEnv=0.3, fD=0.8, fS=0.0, ampFollow=0.0, aD=2.0, aS=0.2, synLvl=0.55), room)
P(C, "Wide Stereo Synth", dict(GL, oscAWave=SAW, oscAUnison=5, oscADetune=0.3, spread=1.0, cutoff=3500, synLvl=0.45), chorus, room)
P(C, "Mallet Doubler", dict(GL, oscAWave=SINE, oscBWave=SINE, oscBOct=2, oscBLevel=0.25, ampFollow=0.0, aD=0.6, aS=0.0, synLvl=0.5), room)
P(C, "Drone Root", dict(GL, oscAWave=SAW, trkOct=-1, trkQuant=1.0, trkMode=1, ampFollow=0.0, aA=0.8, aS=1.0, aR=4.0, cutoff=700, synLvl=0.45), hall)
P(C, "Flute Doubler", dict(GL, oscAWave=SINE, noiseLevel=0.12, trkOct=1, aA=0.04, synLvl=0.45), vib, room)
P(C, "Talkbox Blend", dict(GL, dryLvl=0.4, oscAWave=SAW, filtMode=VOWEL, cutoff=400, reso=0.45, envCut=0.6, synLvl=0.7), room)
P(C, "Sitar Layer", dict(GL, oscAWave=PLUCK, oscAShape=0.95, filtMode=BP, cutoff=2200, reso=0.3, ampFollow=0.0, aS=1.0, aR=0.6, synLvl=0.5), room)
P(C, "Reverse-ish Swell", dict(GL, dryLvl=0.6, oscAWave=TRI, oscBWave=SAW, oscBLevel=0.3, aA=0.35, aD=0.1, aS=1.0, aR=0.4, ampFollow=0.8, cutoff=3000, synLvl=0.6), tapeEcho, hall)

# ============================================================== ENVELOPE FILTERS (guitar direct through the filter)
C = "Envelope Filters"
EF = dict(oscALevel=0.0, ampFollow=1.0, aA=0.002, aS=1.0, aR=0.15, ampVel=0.0, gtrClean=0.8)
P(C, "Classic Auto-Wah", dict(EF, filtMode=BP, cutoff=300, reso=0.55, envCut=0.8))
P(C, "Funk Quack", dict(EF, filtMode=LP24, cutoff=250, reso=0.7, envCut=0.9), room)
P(C, "Down Sweep Wah", dict(EF, filtMode=LP24, cutoff=3500, reso=0.6, envCut=-0.7))
P(C, "Synth Squelch", dict(EF, gtrClean=0.3, gtrFuzz=0.6, filtMode=LP24, cutoff=200, reso=0.8, envCut=0.85), slap)
P(C, "Vowel Wah", dict(EF, filtMode=VOWEL, cutoff=150, reso=0.5, envCut=0.75), room)
P(C, "Mu Tron Style", dict(EF, filtMode=BP, cutoff=400, reso=0.75, envCut=0.65), phaseFast)
P(C, "LFO Wah", dict(EF, filtMode=BP, cutoff=700, reso=0.6, lfo1Wave=L_TRI, lfo1Rate=1.5, lfo1Cutoff=0.8, envCut=0.2), room)
P(C, "Step Filter", dict(EF, filtMode=LP24, cutoff=900, reso=0.65, lfo1Wave=L_SH, lfo1Rate=8.0, lfo1Cutoff=0.9), echo8d)
P(C, "Notch Sweep", dict(EF, filtMode=NOTCH, cutoff=600, reso=0.3, lfo1Wave=L_SINE, lfo1Rate=0.4, lfo1Cutoff=0.9), room)
P(C, "High Pass Blip", dict(EF, filtMode=HP, cutoff=1200, reso=0.6, envCut=-0.6), slap)
P(C, "Fuzz Wah", dict(EF, gtrClean=0.2, gtrFuzz=0.7, filtMode=BP, cutoff=350, reso=0.6, envCut=0.7), room)
P(C, "Octave Filter Bass", dict(EF, gtrClean=0.3, gtrOct=0.8, filtMode=LP24, cutoff=200, reso=0.55, envCut=0.8))
P(C, "Talk Box", dict(EF, gtrClean=0.4, gtrFuzz=0.5, filtMode=VOWEL, cutoff=250, reso=0.6, envCut=0.85), room)
P(C, "Slow Bloom Filter", dict(EF, filtMode=LP24, cutoff=150, reso=0.45, envCut=0.6, aA=0.3), hall)
P(C, "Resonant Ping", dict(EF, filtMode=BP, cutoff=1000, reso=0.9, envCut=0.4), ping)
P(C, "Dub Filter", dict(EF, filtMode=LP24, cutoff=350, reso=0.5, envCut=0.5), tapeEcho, room)
P(C, "Phased Wah", dict(EF, filtMode=BP, cutoff=500, reso=0.5, envCut=0.6), phaseSlow)
P(C, "Synth Filter Clean", dict(EF, filtMode=LP12, cutoff=600, reso=0.4, envCut=0.5, filtEnv=0.4, fD=0.2, fS=0.0), chorus)
P(C, "Wah Chop", dict(EF, filtMode=BP, cutoff=450, reso=0.6, envCut=0.7), chop)
P(C, "Drive Envelope", dict(EF, filtMode=LP24, cutoff=300, reso=0.5, envCut=0.75, filtDrive=0.7), crunch)
P(C, "Bit Wah", dict(EF, filtMode=BP, cutoff=400, reso=0.55, envCut=0.75), crush)
P(C, "Flange Envelope", dict(EF, filtMode=LP12, cutoff=500, reso=0.35, envCut=0.6), flanger, room)

# ============================================================== FUZZ & OCTAVE (guitar direct sources)
C = "Fuzz & Octave"
FO = dict(oscALevel=0.0, ampFollow=0.9, aA=0.002, aS=1.0, aR=0.1, ampVel=0.0)
P(C, "Square Fuzz Synth", dict(FO, gtrFuzz=0.9, cutoff=3000, reso=0.2))
P(C, "Octave Down Fuzz", dict(FO, gtrOct=0.8, gtrFuzz=0.4, cutoff=2000), room)
P(C, "Analog Octaver", dict(FO, gtrOct=0.7, gtrClean=0.6, cutoff=1500))
P(C, "Gated Square", dict(FO, gtrFuzz=0.8, cutoff=1800, reso=0.4, aR=0.03), slap)
P(C, "Square + Sub Synth", dict(FO, gtrFuzz=0.6, oscAWave=SINE, oscALevel=0.6, trkOct=-1, cutoff=2200))
P(C, "Fuzz + Saw Octave", dict(FO, gtrFuzz=0.6, oscAWave=SAW, oscALevel=0.5, trkOct=-1, cutoff=2500, reso=0.25), warm)
P(C, "Ring Fuzz", dict(FO, gtrFuzz=0.5, oscAWave=SINE, oscALevel=0.2, oscBWave=SINE, oscBSemi=7, ringLevel=0.8, cutoff=4000), dict(drvOn=1, drvType=FUZZ, drvAmt=0.8))
P(C, "Velcro Fuzz", dict(FO, gtrFuzz=0.9, filtMode=HP, cutoff=400, reso=0.3), dict(drvOn=1, drvType=CRUSH, drvAmt=0.65, drvMix=0.7))
P(C, "Octave Up Fuzz", dict(FO, gtrFuzz=0.6, oscAWave=PULSE, oscALevel=0.4, trkOct=1, cutoff=4000), fuzz)
P(C, "Synth Fuzz Wall", dict(FO, gtrFuzz=0.7, oscAWave=SAW, oscALevel=0.5, oscAUnison=3, oscADetune=0.3, cutoff=2500), fuzz, room)
P(C, "Muffled Fuzz", dict(FO, gtrFuzz=0.9, filtMode=LP24, cutoff=700, reso=0.3))
P(C, "Fuzz Lead Filter", dict(FO, gtrFuzz=0.9, cutoff=1200, filtEnv=0.5, fD=0.4, fS=0.3, reso=0.45), echo8d)
P(C, "Double Octave Down", dict(FO, gtrOct=0.8, oscAWave=PULSE, oscALevel=0.5, trkOct=-2, cutoff=900))
P(C, "Octave Organ Pedal", dict(FO, gtrOct=0.4, gtrClean=0.5, oscAWave=WTO, oscAShape=2/7, oscALevel=0.5, trkQuant=1.0, cutoff=4000), dict(trmOn=1, trmRate=6.0, trmDepth=0.2, trmPan=1))
P(C, "Fuzz Wah Octave", dict(FO, gtrFuzz=0.6, gtrOct=0.5, filtMode=BP, cutoff=500, reso=0.5, envCut=0.6))
P(C, "Starved Fuzz", dict(FO, gtrFuzz=0.7, cutoff=2500), dict(drvOn=1, drvType=CRUSH, drvAmt=0.75, drvMix=1.0))
P(C, "Square Phaser", dict(FO, gtrFuzz=0.85, cutoff=3500), phaseSlow)
P(C, "Fuzz Into Tape", dict(FO, gtrFuzz=0.8, cutoff=2800), tapeEcho, room)
P(C, "Octave + Fifth Fuzz", dict(FO, gtrFuzz=0.5, oscAWave=SAW, oscALevel=0.4, trkChord=C_POWER, cutoff=2000), fuzz)
P(C, "Sputter Gate", dict(FO, gtrFuzz=0.7, gtrOct=0.5, filtMode=BP, cutoff=900, reso=0.4, lfo2Wave=L_SQ, lfo2Rate=12.0, lfo2Amp=0.9))
P(C, "Clean Octave Down", dict(FO, gtrOct=0.6, gtrClean=0.9, cutoff=3000, dryLvl=0.0), chorus)
P(C, "Square Flange", dict(FO, gtrFuzz=0.8, cutoff=3000), flanger)

# ============================================================== HARMONY & CHORDS (tracker harmony)
C = "Harmony & Chords"
HC = dict(ampFollow=0.6, trkQuant=1.0)
P(C, "Power Chord Synth", dict(HC, trkChord=C_POWER, oscAWave=SAW, cutoff=2200, reso=0.2), crunch, room)
P(C, "Major Stab", dict(HC, trkChord=C_MAJ, oscAWave=SAW, oscAUnison=2, cutoff=1500, filtEnv=0.5, fD=0.2, fS=0.2, aD=0.4, aS=0.3), plate)
P(C, "Minor Stab", dict(HC, trkChord=C_MIN, oscAWave=PULSE, oscAShape=0.3, cutoff=1300, filtEnv=0.5, fD=0.2, fS=0.2, aD=0.4, aS=0.3), room)
P(C, "Maj7 Dream", dict(HC, trkChord=C_MAJ7, oscAWave=TRI, oscBWave=SINE, oscBOct=1, oscBLevel=0.3, aA=0.2, aR=1.5, ampFollow=0.3), chorus, hall)
P(C, "Min7 Rhodes Chord", dict(HC, trkChord=C_MIN7, oscAWave=FM11, oscAShape=0.4, shpEnv=0.4, fD=0.5, fS=0.0, aD=2.0, aS=0.3), trem, room)
P(C, "Quartal Brass", dict(HC, trkChord=C_FOURTHS, oscAWave=SAW, cutoff=800, filtEnv=0.5, fA=0.04, fD=0.3), room)
P(C, "Octaves Lead", dict(HC, trkChord=C_OCTAVES, oscAWave=SAW, cutoff=3500, trkQuant=0.5), echo8d)
P(C, "Fifths Organ", dict(HC, trkChord=C_FIFTH, oscAWave=WTO, oscAShape=2/7, aS=1.0, aR=0.06, ampVel=0.0), room)
P(C, "Sus4 Pluck", dict(HC, trkChord=C_SUS4, oscAWave=PLUCK, oscAShape=0.6, ampFollow=0.0, aS=1.0, aR=0.8), ping)
P(C, "Major Pad Chord", dict(HC, trkChord=C_MAJ, oscAWave=SAW, oscAUnison=3, oscADetune=0.25, cutoff=1800, aA=0.4, aR=1.5, trkMode=1, ampFollow=0.2), ensemble, hall)
P(C, "Minor Choir Chord", dict(HC, trkChord=C_MIN, oscAWave=WTV, oscAShape=0.5, aA=0.4, aR=1.5, ampFollow=0.2), cathedral)
P(C, "Octave Down Harmony", dict(HC, trkChord=C_OCTDN, oscAWave=SAW, oscBWave=PULSE, oscBLevel=0.4, cutoff=1400), room)
P(C, "Octave Up Bells", dict(HC, trkChord=C_OCTUP, oscAWave=BELL, oscAShape=0.3, ampFollow=0.0, aD=1.5, aS=0.0, aR=1.0), plate)
P(C, "Maj7 Keys", dict(HC, trkChord=C_MAJ7, oscAWave=FM21, oscAShape=0.25, shpEnv=0.3, fD=0.6, fS=0.0, aD=1.8, aS=0.2), chorus, room)
P(C, "Min7 Pad", dict(HC, trkChord=C_MIN7, oscAWave=SAW, oscAUnison=2, cutoff=1200, aA=0.6, aR=2.0, ampFollow=0.1), phaseSlow, hall)
P(C, "Power Fuzz Chord", dict(HC, trkChord=C_POWER, oscAWave=PULSE, oscAShape=0.2, cutoff=3000), fuzz)
P(C, "Fourths Pad", dict(HC, trkChord=C_FOURTHS, oscAWave=WTA, oscAShape=0.3, aA=0.6, aR=2.0, ampFollow=0.1), longEcho, hall)
P(C, "Chord Memory Stab", dict(HC, trkChord=C_MIN7, oscAWave=SAW, oscBWave=SAW, oscBFine=10, oscBLevel=0.7, cutoff=900, reso=0.3, filtEnv=0.6, fD=0.15, fS=0.0, aD=0.3, aS=0.0, aR=0.3), echo8d)
P(C, "Major Organ", dict(HC, trkChord=C_MAJ, oscAWave=WTO, oscAShape=4/7, aS=1.0, aR=0.05, ampVel=0.0), dict(trmOn=1, trmRate=6.5, trmDepth=0.25, trmPan=1))
P(C, "Sus Strings", dict(HC, trkChord=C_SUS4, oscAWave=SAW, oscAUnison=4, oscADetune=0.25, cutoff=2500, aA=0.3, aR=1.2, ampFollow=0.2), ensemble)
P(C, "Fifth Pluck", dict(HC, trkChord=C_FIFTH, oscAWave=SAW, cutoff=700, filtEnv=0.6, fD=0.18, fS=0.0, aD=0.4, aS=0.0), ping, room)
P(C, "Octaves Sub Pad", dict(HC, trkChord=C_OCTAVES, oscAWave=SINE, oscBWave=TRI, oscBLevel=0.4, aA=0.5, aR=1.5, ampFollow=0.2), hall)

# ============================================================== VOCAL & FORMANT
C = "Vocal & Formant"
P(C, "Aah Choir", dict(PADB, oscAWave=WTV, oscAShape=1.0, oscAUnison=4, oscADetune=0.2, aA=0.3, aR=1.2), ensemble, hall)
P(C, "Ooh Choir", dict(PADB, oscAWave=WTV, oscAShape=0.6, oscAUnison=3, oscADetune=0.15, aA=0.4, aR=1.2, cutoff=3000), hall)
P(C, "Solo Voice", dict(LEADB, oscAWave=WTV, oscAShape=0.0, aA=0.06), vib, plate)
P(C, "Talk Lead", dict(LEADB, oscAWave=SAW, filtMode=VOWEL, cutoff=400, reso=0.5, envCut=0.6), warm, room)
P(C, "Robot Voice", dict(LEADB, oscAWave=PULSE, oscAShape=0.4, filtMode=VOWEL, cutoff=1000, reso=0.6, lfo1Wave=L_SH, lfo1Rate=6.0, lfo1Cutoff=0.7, trkQuant=1.0), crush)
P(C, "Vowel Sweep", dict(LEADB, oscAWave=SAW, oscBWave=SAW, oscBFine=7, oscBLevel=0.6, filtMode=VOWEL, cutoff=100, reso=0.45, lfo1Wave=L_TRI, lfo1Rate=0.5, lfo1Cutoff=1.0), room)
P(C, "Wow Bass Voice", dict(BASSB, oscAWave=SAW, filtMode=VOWEL, cutoff=150, reso=0.5, filtEnv=0.6, fA=0.08, fD=0.3, fS=0.0), room)
P(C, "Whisper Pad", dict(PADB, oscAWave=NOISE, oscALevel=0.6, oscBWave=WTV, oscBShape=0.3, oscBLevel=0.4, filtMode=VOWEL, cutoff=800, reso=0.6, aA=0.6, aR=1.5), cathedral)
P(C, "Monk Drone", dict(PADB, oscAWave=WTV, oscAShape=0.55, trkOct=-1, trkChord=C_FIFTH, aA=0.8, aR=2.0, cutoff=2000), cathedral)
P(C, "Formant Pluck", dict(PLUCKB, oscAWave=SAW, filtMode=VOWEL, cutoff=200, reso=0.55, filtEnv=0.5, fD=0.2, fS=0.0, aD=0.5, aS=0.0), ping)
P(C, "Vocal Morph Lead", dict(LEADB, oscAWave=WTV, oscAShape=0.3, shpEnv=0.6, fA=0.3, fD=1.0, fS=0.2), echo8d, room)
P(C, "Yeah Stab", dict(KEYB, oscAWave=WTV, oscAShape=0.95, filtMode=VOWEL, cutoff=600, reso=0.4, aD=0.3, aS=0.0, aR=0.2), slap)
P(C, "Choir Octaves", dict(PADB, oscAWave=WTV, oscAShape=0.8, oscAUnison=3, trkChord=C_OCTAVES, aA=0.5, aR=1.8), cathedral)
P(C, "Alien Chatter", dict(LEADB, oscAWave=WTV, oscAShape=0.5, lfo2Wave=L_SH, lfo2Rate=7.0, lfo2Shape=1.0, cutoff=5000), ping)
P(C, "Female Ah", dict(PADB, oscAWave=WTV, oscAShape=0.95, trkOct=1, aA=0.3, aR=1.2), vib, hall)
P(C, "Throat Bass", dict(BASSB, oscAWave=WTV, oscAShape=0.6, subLevel=0.4, cutoff=1500))
P(C, "Vowel Organ", dict(KEYB, oscAWave=WTO, oscAShape=6/7, filtMode=VOWEL, cutoff=700, reso=0.35, aS=1.0, aR=0.08, ampVel=0.0), room)
P(C, "Speaking Strings", dict(PADB, oscAWave=SAW, oscAUnison=4, oscADetune=0.3, filtMode=VOWEL, cutoff=400, reso=0.4, lfo1Rate=0.2, lfo1Cutoff=0.6, aA=0.4, aR=1.5), ensemble)

# ============================================================== TEXTURES & FX
C = "Textures & FX"
TX = dict(ampFollow=0.3, trkQuant=0.5)
P(C, "Frozen Cloud", dict(TX, oscAWave=SAW, oscAUnison=5, oscADetune=0.6, ampFollow=0.0, aA=2.0, aS=1.0, aR=6.0, cutoff=1500, spread=1.0), longEcho, cathedral)
P(C, "Ring Mod Chaos", dict(TX, oscAWave=SAW, oscALevel=0.3, oscBWave=SINE, oscBSemi=11, oscBOct=1, ringLevel=1.0, lfo1Wave=L_SH, lfo1Rate=5.0, lfo1Pitch=0.0, lfo1Cutoff=0.5, cutoff=3000), ping)
P(C, "Sample & Hold Burble", dict(TX, oscAWave=SAW, cutoff=800, reso=0.75, lfo1Wave=L_SH, lfo1Rate=9.0, lfo1Cutoff=1.0), echo8d, room)
P(C, "Wind Tunnel", dict(TX, oscAWave=NOISE, filtMode=BP, cutoff=800, reso=0.7, lfo1Wave=L_DRIFT, lfo1Rate=0.3, lfo1Cutoff=0.9, aA=0.8, aR=2.0), hall)
P(C, "Sci-Fi Siren", dict(TX, oscAWave=TRI, lfo1Wave=L_SAW, lfo1Rate=0.8, lfo1Pitch=0.9, cutoff=6000), longEcho)
P(C, "Broken Radio", dict(TX, oscAWave=PULSE, filtMode=BP, cutoff=1800, reso=0.5, lfo2Wave=L_SH, lfo2Rate=14.0, lfo2Amp=0.6), crush, tapeEcho)
P(C, "Glitch Steps", dict(TX, oscAWave=WTD, oscAShape=0.5, lfo2Wave=L_SH, lfo2Rate=10.0, lfo2Shape=1.0, lfo2Amp=0.5, trkQuant=1.0), ping)
P(C, "Underwater Bells", dict(TX, oscAWave=BELL, oscAShape=0.3, ampFollow=0.0, aD=2.0, aS=0.0, aR=2.0), phaseSlow, flanger, cathedral)
P(C, "Drone Swarm", dict(TX, oscAWave=SAW, oscAUnison=5, oscADetune=1.0, oscBWave=SAW, oscBOct=-1, oscBUnison=5, oscBDetune=0.8, oscBLevel=0.5, cutoff=900, ampFollow=0.0, aA=1.5, aS=1.0, aR=4.0, drift=1.0), hall)
P(C, "Laser Harp", dict(TX, oscAWave=SAW, cutoff=200, reso=0.8, filtEnv=1.0, fD=0.5, fS=0.0, aD=1.0, aS=0.0, aR=0.8, ampFollow=0.0), ping, hall)
P(C, "Tape Warble", dict(TX, oscAWave=TRI, oscBWave=SINE, oscBOct=1, oscBLevel=0.4, drift=1.0, lfo1Wave=L_DRIFT, lfo1Rate=1.5, lfo1Pitch=0.3), tapeEcho, room)
P(C, "Shimmer Verb Wash", dict(TX, oscAWave=SINE, oscBWave=SINE, oscBOct=1, oscBLevel=0.5, trkOct=1, aA=0.6, aR=3.0, ampFollow=0.1), longEcho, dict(revOn=1, revSize=1.0, revDamp=0.1, revMix=0.6))
P(C, "Static Pulse", dict(TX, oscAWave=NOISE, oscALevel=0.4, oscBWave=PULSE, oscBLevel=0.5, cutoff=2500, lfo2Wave=L_SQ, lfo2Rate=6.0, lfo2Amp=1.0), room)
P(C, "Dive Bomb", dict(TX, oscAWave=SAW, glide=1.2, trkQuant=0.0, cutoff=2500, filtDrive=0.6, voiceMode=LEGATO, lfo1Wave=L_SAW, lfo1Rate=0.5, lfo1Pitch=0.7), fuzz, room)
P(C, "Metallic Scrape", dict(TX, oscAWave=FM21, oscAShape=1.0, oscBWave=NOISE, oscBLevel=0.2, filtMode=HP, cutoff=1500, reso=0.4), flanger)
P(C, "Ghost Choir", dict(TX, oscAWave=WTV, oscAShape=0.4, oscAUnison=5, oscADetune=0.7, ampFollow=0.0, aA=1.5, aS=1.0, aR=4.0), phaseSlow, cathedral)
P(C, "Insect Buzz", dict(TX, oscAWave=PULSE, oscAShape=0.9, filtMode=BP, cutoff=2500, reso=0.6, lfo1Wave=L_SQ, lfo1Rate=18.0, lfo1Pitch=0.25), room)
P(C, "Space Echoes", dict(TX, oscAWave=SINE, oscAShape=0.4, ampFollow=0.0, aD=0.4, aS=0.0, aR=0.3), dict(dlyOn=1, dlyDiv=9, dlyFb=0.8, dlyPing=1, dlyTone=0.4, dlyMix=0.45), hall)
P(C, "Bitcrushed Pad", dict(TX, oscAWave=SAW, oscAUnison=3, aA=0.6, aR=2.0, cutoff=2500), dict(drvOn=1, drvType=CRUSH, drvAmt=0.85, drvMix=0.7), hall)
P(C, "Wah Drone", dict(TX, oscAWave=SAW, oscBWave=SAW, oscBSemi=-12, oscBLevel=0.5, filtMode=BP, cutoff=500, reso=0.7, lfo1Wave=L_SINE, lfo1Rate=0.25, lfo1Cutoff=1.0, ampFollow=0.0, aS=1.0, aR=3.0), room)
P(C, "Cosmic Sweep", dict(TX, oscAWave=WTD, oscAShape=0.0, lfo2Wave=L_TRI, lfo2Rate=0.05, lfo2Shape=1.0, aA=1.0, aR=3.0, ampFollow=0.1), longEcho, cathedral)
P(C, "Fizzing Arc", dict(TX, oscAWave=NOISE, oscALevel=0.5, oscBWave=SAW, oscBLevel=0.4, filtMode=HP, cutoff=3000, reso=0.5, lfo2Wave=L_SH, lfo2Rate=20.0, lfo2Amp=0.7), crush)
P(C, "Hollow Wind Flute", dict(TX, oscAWave=NOISE, oscALevel=0.4, oscBWave=SINE, oscBLevel=0.4, filtMode=BP, cutoff=1200, reso=0.6, keyTrack=1.0, aA=0.15), vib, hall)
P(C, "Granular Haze", dict(TX, oscAWave=WTV, oscAShape=0.2, oscAUnison=5, oscADetune=0.4, lfo2Wave=L_SH, lfo2Rate=12.0, lfo2Shape=0.5, lfo2Pan=1.0, aA=0.5, aR=2.0), cathedral)

# ============================================================== PHYSICAL STRINGS (pluck model: guitar-like sounds)
C = "Physical Strings"
PS = dict(PLUCKB, oscAWave=PLUCK, aS=1.0, aR=0.6, cutoff=8000)
P(C, "Steel String", dict(PS, oscAShape=0.75), room)
P(C, "Nylon String", dict(PS, oscAShape=0.4, cutoff=4500), room)
P(C, "Electric Clean Model", dict(PS, oscAShape=0.6, cutoff=5000), chorus, room)
P(C, "Twelve String Model", dict(PS, oscAShape=0.8, oscBWave=PLUCK, oscBShape=0.85, oscBOct=1, oscBFine=4, oscBLevel=0.6), room)
P(C, "Mandolin Model", dict(PS, oscAShape=0.9, oscAUnison=2, oscADetune=0.08, trkOct=1), room)
P(C, "Muted Palm String", dict(PS, oscAShape=0.3, cutoff=1500, aD=0.25, aS=0.0, aR=0.1), slap)
P(C, "Overdriven Model", dict(PS, oscAShape=0.7, cutoff=4000), dict(drvOn=1, drvType=CRUNCH, drvAmt=0.7, drvTone=0.45), room)
P(C, "Bass String Model", dict(PS, oscAShape=0.35, trkOct=-1, cutoff=2500, subLevel=0.2))
P(C, "Harp Model", dict(PS, oscAShape=0.55, oscBWave=SINE, oscBLevel=0.2, aR=1.5), hall)
P(C, "Fuzz String Model", dict(PS, oscAShape=0.8, cutoff=3000), fuzz, room)
P(C, "Plucked Cello", dict(PS, oscAShape=0.45, trkOct=-1, filtMode=LP12, cutoff=2000), room)
P(C, "Dobro Model", dict(PS, oscAShape=0.85, filtMode=BP, cutoff=1800, reso=0.35, trkQuant=0.2), room)
P(C, "Koto Model", dict(PS, oscAShape=0.95, oscAUnison=2, oscADetune=0.15, aR=1.0), plate)
P(C, "Chorused Model", dict(PS, oscAShape=0.65, oscAUnison=3, oscADetune=0.2), chorus, echo8d)
P(C, "Clean Jazz Model", dict(PS, oscAShape=0.25, cutoff=2000, filtMode=LP12), room)
P(C, "Ambient String Model", dict(PS, oscAShape=0.6, aR=2.0), longEcho, cathedral)


# ============================================================== validation + output
def load_param_specs():
    """Parse Params.h for ids and ranges (floats/ints/choices/bools)."""
    text = open(os.path.join(SRC, "Params.h")).read()
    specs = {}
    lists = {m.group(1): len(re.findall(r'"[^"]*"', m.group(2)))
             for m in re.finditer(r'inline const juce::StringArray (\w+)\s*\{([^}]*)\}', text)}
    for m in re.finditer(r'addF \("(\w+)",\s*"[^"]*",\s*(?:NR \(([-\d.f]+),\s*([-\d.f]+)\)|skewed \(([-\d.f]+),\s*([-\d.f]+),\s*[-\d.f]+\)|timeRange)', text):
        lo = m.group(2) or m.group(4); hi = m.group(3) or m.group(5)
        if lo is None: lo, hi = "0.001", "10"
        specs[m.group(1)] = ("f", float(lo.rstrip("f")), float(hi.rstrip("f")))
    for m in re.finditer(r'addI \("(\w+)",\s*"[^"]*",\s*(-?\d+),\s*(-?\d+)', text):
        specs[m.group(1)] = ("i", int(m.group(2)), int(m.group(3)))
    for m in re.finditer(r'addC \("(\w+)",\s*"[^"]*",\s*(\w+)', text):
        specs[m.group(1)] = ("c", 0, lists[m.group(2)] - 1)
    for m in re.finditer(r'addB \("(\w+)"', text):
        specs[m.group(1)] = ("b", 0, 1)
    # oscillator params are generated in a loop
    for o in "AB":
        specs[f"osc{o}Wave"] = ("c", 0, lists["oscWaves"] - 1)
        specs[f"osc{o}Oct"] = ("i", -3, 3); specs[f"osc{o}Semi"] = ("i", -12, 12)
        specs[f"osc{o}Fine"] = ("f", -100, 100)
        for k in ("Shape", "Level", "Detune"): specs[f"osc{o}{k}"] = ("f", 0, 1)
        specs[f"osc{o}Unison"] = ("i", 1, 5)
    return specs

def flit(v):
    s = f"{float(v):.5g}"
    if "." not in s and "e" not in s: s += ".0"
    return s + "f"

def main():
    specs = load_param_specs()
    errors = []
    names = set()
    for name, cat, d in BANK:
        if name in names: errors.append(f"duplicate name {name}")
        names.add(name)
        for k, v in d.items():
            if k not in specs: errors.append(f"{name}: unknown param {k}"); continue
            t, lo, hi = specs[k]
            if not (lo - 1e-6 <= v <= hi + 1e-6): errors.append(f"{name}: {k}={v} outside [{lo},{hi}]")
            if k in ("trkSens",): errors.append(f"{name}: {k} is a rig parameter, presets must not set it")
    # distinctness in a normalised parameter space (log scale for frequencies/times)
    keys = sorted(specs)
    defaults = {}
    text = open(os.path.join(SRC, "Params.h")).read()
    def norm(k, v):
        t, lo, hi = specs[k]
        if k in ("cutoff",) or k[:1] in "fa" and k[1:] in ("A", "D", "R"):
            return math.log(max(v, 1e-3)) / math.log(max(hi, 2))
        if t == "c": return v * 0.5   # any choice change counts as a big difference
        return (v - lo) / (hi - lo) if hi > lo else 0
    vecs = [{k: norm(k, v) for k, v in d.items()} for _, _, d in BANK]
    close = []
    for (i, a), (j, b) in itertools.combinations(enumerate(vecs), 2):
        ks = set(a) | set(b)
        dist = math.sqrt(sum((a.get(k, -9) - b.get(k, -9)) ** 2 if (k in a) != (k in b) else (a[k] - b[k]) ** 2 for k in ks))
        if dist < 0.15: close.append((dist, BANK[i][0], BANK[j][0]))
    for c in close: errors.append(f"too similar: {c[1]} ~ {c[2]} ({c[0]:.3f})")
    if errors:
        print("\n".join(errors)); sys.exit(1)

    cats = []
    for _, c, _ in BANK:
        if c not in cats: cats.append(c)
    out = ['// GENERATED by tools/gen_presets.py - edit the script, not this file.',
           '#include "FactoryPresets.h"',
           '#if __has_include("FactoryPresetLevels.h")',
           ' #include "FactoryPresetLevels.h"',
           '#else',
           ' #include <map>',
           ' #include <string>',
           ' static const std::map<std::string, float> factoryPresetLevels;',
           '#endif', '',
           'const std::vector<FactoryPreset>& getFactoryPresets()', '{',
           '    static const std::vector<FactoryPreset> bank = []',
           '    {',
           '        std::vector<FactoryPreset> b {']
    for cat in cats:
        out.append(f'            // ---- {cat}')
        for name, c, d in BANK:
            if c != cat: continue
            vals = ", ".join(f'{{ "{k}", {flit(v)} }}' for k, v in d.items())
            out.append(f'            {{ "{name}", "{cat}", {{ {vals} }} }},')
    out += ['        };',
            '        for (auto& p : b)',
            '        {',
            '            auto it = factoryPresetLevels.find (p.name);',
            '            p.values.push_back ({ "level", it != factoryPresetLevels.end() ? it->second : 0.5f });',
            '        }',
            '        return b;',
            '    }();',
            '    return bank;',
            '}', '']
    open(os.path.join(SRC, "FactoryPresets.cpp"), "w").write("\n".join(out))
    print(f"{len(BANK)} presets in {len(cats)} categories:")
    for cat in cats: print(f"  {cat:20s} {sum(1 for _, c, _ in BANK if c == cat)}")
    if close: print(close)

if __name__ == "__main__":
    main()
