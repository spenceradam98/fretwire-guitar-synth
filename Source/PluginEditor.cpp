#include "PluginEditor.h"

// =========================================================== ParamControl
ParamControl::ParamControl (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text)
{
    auto* param = s.getParameter (id);
    jassert (param != nullptr);
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.5f));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
    {
        combo = std::make_unique<juce::ComboBox>();
        combo->addItemList (choice->choices, 1);
        addAndMakeVisible (*combo);
        cAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, *combo);
    }
    else if (dynamic_cast<juce::AudioParameterBool*> (param) != nullptr)
    {
        toggle = std::make_unique<juce::ToggleButton>();
        addAndMakeVisible (*toggle);
        bAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, id, *toggle);
    }
    else
    {
        slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
        slider->setPopupDisplayEnabled (true, true, nullptr);
        slider->setTooltip (param->getName (64));
        addAndMakeVisible (*slider);
        sAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, *slider);
        slider->setDoubleClickReturnValue (true, (double) dynamic_cast<juce::RangedAudioParameter*> (param)
                                                     ->convertFrom0to1 (param->getDefaultValue()));
    }
}

void ParamControl::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromBottom (16));
    if (slider) slider->setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight())));
    if (combo)  combo->setBounds (r.withSizeKeepingCentre (r.getWidth() - 2, 22));
    if (toggle) toggle->setBounds (r.withSizeKeepingCentre (r.getWidth(), 24));
}

// ================================================================ Section
Section::Section (juce::AudioProcessorValueTreeState& s, const juce::String& t,
                  std::vector<std::pair<const char*, const char*>> list, const char* powerId)
    : apvts (s), title (t)
{
    for (auto& [id, name] : list)
        addAndMakeVisible (*controls.emplace_back (std::make_unique<ParamControl> (s, id, name)));
    if (powerId != nullptr)
    {
        powerParam = powerId;
        power = std::make_unique<juce::ToggleButton>();
        power->setTooltip ("Switch " + title + " on/off");
        addAndMakeVisible (*power);
        powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, powerParam, *power);
        apvts.addParameterListener (powerParam, this);
    }
}

Section::~Section() { if (powerParam.isNotEmpty()) apvts.removeParameterListener (powerParam, this); }

void Section::parameterChanged (const juce::String&, float)
{
    juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<Section> (this)] { if (sp) sp->repaint(); });
}

void Section::paint (juce::Graphics& g)
{
    if (auto* l = dynamic_cast<FretwireLookAndFeel*> (&getLookAndFeel()))
    {
        const bool active = powerParam.isEmpty() || apvts.getRawParameterValue (powerParam)->load() > 0.5f;
        l->drawPanel (g, getLocalBounds().toFloat(), title, active);
    }
}

void Section::resized()
{
    auto r = getLocalBounds().reduced (6, 4);
    auto header = r.removeFromTop (20);
    if (power) power->setBounds (header.removeFromRight (40));
    if (controls.empty()) return;
    const float w = (float) r.getWidth() / (float) controls.size();
    for (size_t i = 0; i < controls.size(); ++i)
        controls[i]->setBounds (juce::Rectangle<float> (r.getX() + w * (float) i, (float) r.getY(), w, (float) r.getHeight()).toNearestInt().reduced (2, 0));
}

// ================================================================= Editor
FretwireEditor::FretwireEditor (FretwireProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard), tuner (p)
{
    setLookAndFeel (&laf);
    auto& s = p.apvts;
    auto add = [&] (int row, std::unique_ptr<Section> sec) { content.addAndMakeVisible (*sec); rows[row].push_back (std::move (sec)); };

    using CL = std::vector<std::pair<const char*, const char*>>;

    // ---- Row 0: guitar input + tracking
    add (0, std::make_unique<Section> (s, "Guitar Input", CL {
        { "inGain", "Input" }, { "trkRange", "Range" }, { "trkSens", "Threshold" }, { "trkDyn", "Pick Dyn" },
        { "dryLvl", "Dry" }, { "synLvl", "Synth" } }));
    add (0, std::make_unique<Section> (s, "Tracking", CL {
        { "trkOct", "Octave" }, { "trkSemi", "Transpose" }, { "trkQuant", "Snap" }, { "trkMode", "Mode" },
        { "trkChord", "Harmony" }, { "ampFollow", "Amp Follow" }, { "envCut", "Env > Filter" } }, "trkOn"));

    // ---- Row 1: oscillators + mixer
    for (int o = 0; o < 2; ++o)
    {
        static std::vector<std::string> ids[2];
        auto pre = std::string (o == 0 ? "oscA" : "oscB");
        auto& v = ids[o];
        v = { pre + "Wave", pre + "Oct", pre + "Semi", pre + "Fine", pre + "Shape", pre + "Level", pre + "Unison", pre + "Detune" };
        add (1, std::make_unique<Section> (s, o == 0 ? "Oscillator A" : "Oscillator B", CL {
            { v[0].c_str(), "Wave" }, { v[1].c_str(), "Octave" }, { v[2].c_str(), "Semi" }, { v[3].c_str(), "Fine" },
            { v[4].c_str(), "Shape" }, { v[5].c_str(), "Level" }, { v[6].c_str(), "Unison" }, { v[7].c_str(), "Detune" } }));
    }
    add (1, std::make_unique<Section> (s, "Mixer", CL {
        { "subLevel", "Sub" }, { "subWave", "Sub Wave" }, { "noiseLevel", "Noise" }, { "ringLevel", "Ring" }, { "xmod", "X-Mod" } }));

    // ---- Row 2: guitar direct + filter + envelopes
    add (2, std::make_unique<Section> (s, "Guitar Direct", CL {
        { "gtrClean", "Clean" }, { "gtrFuzz", "Square" }, { "gtrOct", "Oct Down" } }));
    add (2, std::make_unique<Section> (s, "Filter", CL {
        { "filtMode", "Mode" }, { "cutoff", "Cutoff" }, { "reso", "Reso" }, { "filtDrive", "Drive" },
        { "filtEnv", "Env Amt" }, { "keyTrack", "Key Trk" }, { "filtVel", "Velocity" }, { "shpEnv", "Env>Shape" } }));
    add (2, std::make_unique<Section> (s, "Filter Env", CL {
        { "fA", "A" }, { "fD", "D" }, { "fS", "S" }, { "fR", "R" } }));
    add (2, std::make_unique<Section> (s, "Amp Env", CL {
        { "aA", "A" }, { "aD", "D" }, { "aS", "S" }, { "aR", "R" }, { "ampVel", "Vel" } }));

    // ---- Row 3: modulation + voice
    add (3, std::make_unique<Section> (s, "LFO 1  (vibrato / sweep)", CL {
        { "lfo1Wave", "Wave" }, { "lfo1Rate", "Rate" }, { "lfo1Delay", "Fade In" }, { "lfo1Pitch", "> Pitch" }, { "lfo1Cutoff", "> Cutoff" } }));
    add (3, std::make_unique<Section> (s, "LFO 2  (motion)", CL {
        { "lfo2Wave", "Wave" }, { "lfo2Rate", "Rate" }, { "lfo2Shape", "> Shape" }, { "lfo2Pan", "> Pan" }, { "lfo2Amp", "> Amp" } }));
    add (3, std::make_unique<Section> (s, "Voice", CL {
        { "voiceMode", "Keys Mode" }, { "glide", "Glide" }, { "drift", "Drift" }, { "spread", "Spread" },
        { "bendRange", "Bend" }, { "level", "Level" }, { "master", "Master" } }));

    // ---- Row 4: FX part 1
    add (4, std::make_unique<Section> (s, "Drive", CL {
        { "drvType", "Type" }, { "drvAmt", "Amount" }, { "drvTone", "Tone" }, { "drvMix", "Mix" } }, "drvOn"));
    add (4, std::make_unique<Section> (s, "Phaser", CL {
        { "phsRate", "Rate" }, { "phsDepth", "Depth" }, { "phsFb", "Feedback" }, { "phsMix", "Mix" } }, "phsOn"));
    add (4, std::make_unique<Section> (s, "Chorus / Flanger", CL {
        { "choMode", "Mode" }, { "choRate", "Rate" }, { "choDepth", "Depth" }, { "choFb", "Feedback" }, { "choMix", "Mix" } }, "choOn"));

    // ---- Row 5: FX part 2
    add (5, std::make_unique<Section> (s, "Tremolo", CL {
        { "trmShape", "Shape" }, { "trmRate", "Rate" }, { "trmDepth", "Depth" }, { "trmPan", "Auto-Pan" } }, "trmOn"));
    add (5, std::make_unique<Section> (s, "Delay", CL {
        { "dlySync", "Sync" }, { "dlyDiv", "Division" }, { "dlyMs", "Time" }, { "dlyFb", "Feedback" },
        { "dlyTone", "Tone" }, { "dlyPing", "Ping-Pong" }, { "dlyMix", "Mix" } }, "dlyOn"));
    add (5, std::make_unique<Section> (s, "Reverb", CL {
        { "revSize", "Size" }, { "revDamp", "Damping" }, { "revWidth", "Width" }, { "revMix", "Mix" } }, "revOn"));

    // ---- Header
    for (auto* c : { (juce::Component*) &categoryBox, (juce::Component*) &presetBox, (juce::Component*) &themeBox,
                     (juce::Component*) &prevButton, (juce::Component*) &nextButton, (juce::Component*) &saveButton,
                     (juce::Component*) &initButton, (juce::Component*) &keyboard, (juce::Component*) &tuner })
        content.addAndMakeVisible (c);

    categoryBox.setTooltip ("Preset category");
    presetBox.setTooltip ("Preset");
    themeBox.setTooltip ("UI theme");
    for (int i = 0; i < (int) getThemes().size(); ++i) themeBox.addItem (getThemes()[(size_t) i].name, i + 1);
    themeBox.onChange = [this] { applyTheme (themeBox.getSelectedItemIndex()); };

    categoryBox.onChange = [this] { shownCategory = categoryBox.getText(); refreshPresetBox(); };
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0 && id - 1 != proc.presets.getCurrentIndex()) proc.presets.load (id - 1);
    };
    prevButton.onClick = [this] { proc.presets.step (-1); };
    nextButton.onClick = [this] { proc.presets.step (1); };
    initButton.onClick = [this] { proc.presets.loadInit(); };
    saveButton.onClick = [this] { showSaveDialog(); };
    proc.presets.onPresetChanged = [sp = juce::Component::SafePointer<FretwireEditor> (this)]
    {
        if (sp) { sp->refreshCategoryBox(); }
    };

    keyboard.setKeyWidth (22.0f);
    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (36);

    addAndMakeVisible (content);
    content.setSize (designW, designH);
    refreshCategoryBox();
    applyTheme (proc.getThemeIndex());

    setResizable (true, true);
    setResizeLimits (designW / 2, designH / 2, designW * 3 / 2, designH * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) designW / designH);
    setSize (designW * 4 / 5, designH * 4 / 5);
}

FretwireEditor::~FretwireEditor()
{
    proc.presets.onPresetChanged = nullptr;
    setLookAndFeel (nullptr);
}

void FretwireEditor::applyTheme (int index)
{
    laf.setTheme (index);
    proc.setThemeIndex (laf.getThemeIndex());
    themeBox.setSelectedItemIndex (laf.getThemeIndex(), juce::dontSendNotification);
    sendLookAndFeelChange();
    keyboard.repaint();
    repaint();
}

void FretwireEditor::refreshCategoryBox()
{
    auto& pm = proc.presets;
    const int cur = pm.getCurrentIndex();
    if (cur >= 0) shownCategory = pm.getEntries()[cur].category;
    auto cats = pm.getCategories();
    categoryBox.clear (juce::dontSendNotification);
    categoryBox.addItemList (cats, 1);
    if (shownCategory.isEmpty() && ! cats.isEmpty()) shownCategory = cats[0];
    categoryBox.setSelectedItemIndex (juce::jmax (0, cats.indexOf (shownCategory)), juce::dontSendNotification);
    refreshPresetBox();
}

void FretwireEditor::refreshPresetBox()
{
    auto& pm = proc.presets;
    presetBox.clear (juce::dontSendNotification);
    for (int idx : pm.getIndicesInCategory (shownCategory))
        presetBox.addItem (pm.getEntries()[idx].name, idx + 1);
    if (pm.getCurrentIndex() >= 0 && pm.getEntries()[pm.getCurrentIndex()].category == shownCategory)
        presetBox.setSelectedId (pm.getCurrentIndex() + 1, juce::dontSendNotification);
    else
        presetBox.setText (pm.getCurrentIndex() < 0 ? pm.getCurrentName() : juce::String ("-- choose --"), juce::dontSendNotification);
}

void FretwireEditor::showSaveDialog()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save Preset", "Saved to " + PresetManager::getUserDir().getFullPathName(),
                                                      juce::MessageBoxIconType::NoIcon, this);
    saveDialog->addTextEditor ("name", proc.presets.getCurrentName(), "Name");
    saveDialog->addTextEditor ("cat", "User", "Folder");
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        if (result == 1 && saveDialog != nullptr)
            proc.presets.saveUser (saveDialog->getTextEditorContents ("name"), saveDialog->getTextEditorContents ("cat"));
        saveDialog.reset();
    }), false);
}

void FretwireEditor::paint (juce::Graphics& g) { g.fillAll (laf.theme().bgBottom); }

void FretwireEditor::resized()
{
    const float scale = (float) getWidth() / (float) designW;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, designW, designH);
}

// ---------------------------------------------------------------- Content
void FretwireEditor::Content::paint (juce::Graphics& g)
{
    auto& laf = owner.laf;
    const auto& t = laf.theme();
    laf.drawBackground (g, getLocalBounds().toFloat());

    // Header strip + logo
    auto header = getLocalBounds().removeFromTop (64).toFloat().reduced (10, 8);
    g.setColour (t.panel);
    g.fillRoundedRectangle (header, t.corner);
    g.setColour (t.panelEdge.withAlpha (0.6f));
    g.drawRoundedRectangle (header, t.corner, 1.4f);

    auto logo = header.removeFromLeft (250).reduced (14, 0);
    juce::String word = t.uppercaseTitles ? "FRETWIRE" : "fretwire";
    auto sub = logo.removeFromBottom (14);
    g.setFont (laf.titleFont (32.0f));
    // six "strings" behind the wordmark
    for (int i = 0; i < 6; ++i)
    {
        const float y = logo.getY() + 6.0f + (float) i * (logo.getHeight() - 12.0f) / 5.0f;
        g.setColour ((i < 3 ? t.accent2 : t.accent).withAlpha (0.35f));
        g.drawLine (logo.getX() - 6, y, logo.getRight(), y, 0.6f + 0.25f * (float) i);
    }
    g.setColour (t.title);
    g.drawText (word, logo, juce::Justification::centredLeft);
    g.setColour (t.dimText);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (t.uppercaseTitles ? "GUITAR SYNTHESISER" : "guitar synthesiser", sub, juce::Justification::centredLeft);
}

void FretwireEditor::Content::resized()
{
    auto& e = owner;
    auto r = getLocalBounds().reduced (10, 0);
    auto header = r.removeFromTop (64).reduced (0, 8);
    header.removeFromLeft (260);
    auto hdr = header.reduced (6, 10);
    e.themeBox.setBounds (hdr.removeFromRight (190));
    hdr.removeFromRight (20);
    e.saveButton.setBounds (hdr.removeFromRight (60)); hdr.removeFromRight (6);
    e.initButton.setBounds (hdr.removeFromRight (50)); hdr.removeFromRight (16);
    e.nextButton.setBounds (hdr.removeFromRight (30)); hdr.removeFromRight (4);
    e.prevButton.setBounds (hdr.removeFromRight (30)); hdr.removeFromRight (6);
    e.presetBox.setBounds (hdr.removeFromRight (260)); hdr.removeFromRight (6);
    e.categoryBox.setBounds (hdr.removeFromRight (150));

    e.keyboard.setBounds (r.removeFromBottom (78).reduced (0, 6));
    e.keyboard.setKeyWidth ((float) e.keyboard.getWidth() / 50.0f); // 24..108 = 50 white keys
    e.keyboard.setLowestVisibleKey (24);

    const int rowH = (r.getHeight() - 4) / 6;
    bool first = true;
    for (auto& row : e.rows)
    {
        auto rr = r.removeFromTop (rowH).reduced (0, 4);
        if (first) { e.tuner.setBounds (rr.removeFromLeft (230)); rr.removeFromLeft (10); first = false; }
        int total = 0;
        for (auto& s : row) total += s->numControls();
        const int gap = 10;
        const float unit = (float) (rr.getWidth() - gap * ((int) row.size() - 1)) / (float) total;
        float x = (float) rr.getX();
        for (auto& s : row)
        {
            const float w = unit * (float) s->numControls();
            s->setBounds (juce::Rectangle<float> (x, (float) rr.getY(), w, (float) rr.getHeight()).toNearestInt());
            x += w + (float) gap;
        }
    }
}

// ---------------------------------------------------------------- Tuner
void TunerDisplay::timerCallback()
{
    const float n = proc.engine.trackedNote.load(), l = proc.engine.inputLevel.load();
    const int v = proc.engine.activeVoices.load();
    if (std::abs (n - note) > 0.01f || std::abs (l - level) > 0.002f || v != voices) { note = n; level = l; voices = v; repaint(); }
}

void TunerDisplay::paint (juce::Graphics& g)
{
    auto* laf = dynamic_cast<FretwireLookAndFeel*> (&getLookAndFeel());
    if (laf == nullptr) return;
    const auto& t = laf->theme();
    auto r = getLocalBounds().toFloat();
    laf->drawPanel (g, r, proc.getNumStrings() == 6 ? "Tracker  (hex: 6 strings)" : "Tracker", true);
    r = r.reduced (12, 6); r.removeFromTop (20);

    // note name + cents needle
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    auto noteArea = r.removeFromLeft (r.getWidth() * 0.5f);
    g.setFont (laf->titleFont (30.0f));
    if (note >= 0)
    {
        const int nn = (int) std::lround (note);
        const float cents = (note - (float) nn) * 100.0f;
        g.setColour (std::abs (cents) < 8 ? t.accent : t.text);
        g.drawText (juce::String (names[((nn % 12) + 12) % 12]) + juce::String (nn / 12 - 1), noteArea.removeFromTop (noteArea.getHeight() * 0.62f), juce::Justification::centred);
        auto bar = noteArea.reduced (6, 6);
        g.setColour (t.track); g.fillRoundedRectangle (bar, 3);
        g.setColour (t.accent2);
        const float x = bar.getCentreX() + bar.getWidth() * 0.5f * juce::jlimit (-1.0f, 1.0f, cents / 50.0f);
        g.fillRect (juce::Rectangle<float> (3, bar.getHeight() + 4).withCentre ({ x, bar.getCentreY() }));
    }
    else
    {
        g.setColour (t.dimText);
        g.drawText ("--", noteArea.removeFromTop (noteArea.getHeight() * 0.62f), juce::Justification::centred);
    }

    // input meter
    r.removeFromLeft (8);
    auto meter = r.removeFromLeft (14).reduced (0, 4);
    g.setColour (t.track); g.fillRoundedRectangle (meter, 3);
    const float db = juce::Decibels::gainToDecibels (level, -72.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, (db + 72.0f) / 72.0f);
    g.setColour (db > -3 ? juce::Colours::red : t.accent);
    g.fillRoundedRectangle (meter.withTrimmedTop (meter.getHeight() * (1 - frac)), 3);
    r.removeFromLeft (8);
    g.setColour (t.dimText);
    g.setFont (juce::FontOptions (11.0f));
    g.drawFittedText ("in " + juce::String (db, 0) + " dB\nvoices " + juce::String (voices), r.toNearestInt(), juce::Justification::centredLeft, 3);
}
