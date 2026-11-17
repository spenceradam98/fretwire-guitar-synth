#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// ============================================================================
//  Theme system: each theme = palette + procedural background pattern + knob
//  style. FretwireLookAndFeel renders every control from the active theme.
// ============================================================================

struct Theme
{
    enum class Pattern { Tolex, Tweed, Burst, Grille, Neon, Pickguard };
    enum class Knob    { ChickenHead, NeonRing, Bakelite, SoftDot };

    juce::String name;
    juce::Colour bgTop, bgBottom, patternA, patternB;
    juce::Colour panel, panelEdge, title, text, dimText;
    juce::Colour accent, accent2, knobBody, knobPointer, track;
    Pattern pattern;
    Knob knob;
    float corner;
    bool uppercaseTitles;
};

inline const std::vector<Theme>& getThemes()
{
    using C = juce::Colour;
    static const std::vector<Theme> themes =
    {
        { "Stage Black",          // black tolex pedalboard, amber jewel lights
          C (0xff141414), C (0xff0a0a0a), C (0x14ffffff), C (0x10000000),
          C (0xd91c1c1c), C (0xff3a3a3a), C (0xffffb020), C (0xffe8e8e8), C (0xff8c8c8c),
          C (0xffffb020), C (0xffff5a36), C (0xff222222), C (0xfff2f2f2), C (0xff383838),
          Theme::Pattern::Tolex, Theme::Knob::Bakelite, 6.0f, true },

        { "Tweed '59",            // lacquered tweed amp, oxblood grille, chicken-head knobs
          C (0xffc9a86a), C (0xffa98443), C (0x30603a12), C (0x22fff2c8),
          C (0xd94a1f1a), C (0xffe0c48a), C (0xfff6e2b3), C (0xfff3e6c6), C (0xffc7a777),
          C (0xfff0c15a), C (0xffd8643a), C (0xff1b120c), C (0xfff3e6c6), C (0xff6e3b2a),
          Theme::Pattern::Tweed, Theme::Knob::ChickenHead, 4.0f, true },

        { "Cherry Sunburst",      // flame-top burst, cream binding
          C (0xfff0a03a), C (0xff5a0b0b), C (0x22000000), C (0x18ffd27a),
          C (0xcc2a0806), C (0xfff3e2c0), C (0xfff3e2c0), C (0xfff6ead2), C (0xffc69d82),
          C (0xffffc864), C (0xffff5040), C (0xfff1e4c8), C (0xff2a0806), C (0xff5c1c16),
          Theme::Pattern::Burst, Theme::Knob::SoftDot, 12.0f, false },

        { "Blackface",            // 60s blue-grey faceplate, silver grille cloth
          C (0xff2b3540), C (0xff1a2129), C (0x26c7d3dd), C (0x14000000),
          C (0xdd131a21), C (0xffa9b8c4), C (0xffdfe8ef), C (0xffdfe8ef), C (0xff8496a5),
          C (0xff6fc3ff), C (0xffff6f61), C (0xff0d0f12), C (0xfff4f4f4), C (0xff35424f),
          Theme::Pattern::Grille, Theme::Knob::Bakelite, 3.0f, true },

        { "Neon Pedal",           // synth-pedal enclosure, glowing LEDs
          C (0xff12052a), C (0xff050a1f), C (0x3300f0ff), C (0x30ff2bd6),
          C (0xbb0c0620), C (0xff6b4cff), C (0xff00f0ff), C (0xfff1eaff), C (0xff9a8fc0),
          C (0xff00f0ff), C (0xffff2bd6), C (0xff160c2c), C (0xff00f0ff), C (0xff2e2350),
          Theme::Pattern::Neon, Theme::Knob::NeonRing, 14.0f, true },

        { "Surf Green",           // seafoam offset guitar, mint pickguard
          C (0xff8fd3be), C (0xff5aa893), C (0x30ffffff), C (0x14004030),
          C (0xccf4f1e6), C (0xff1e4a40), C (0xff1e4a40), C (0xff1e2a28), C (0xff5d7a72),
          C (0xffd9503f), C (0xff1e7f6b), C (0xfffbf8ee), C (0xffd9503f), C (0xffcbd8d0),
          Theme::Pattern::Pickguard, Theme::Knob::SoftDot, 10.0f, false },
    };
    return themes;
}

// ---------------------------------------------------------------------------
class FretwireLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FretwireLookAndFeel() { setTheme (0); }

    const Theme& theme() const { return getThemes()[(size_t) themeIndex]; }
    int getThemeIndex() const { return themeIndex; }

    void setTheme (int index)
    {
        themeIndex = juce::jlimit (0, (int) getThemes().size() - 1, index);
        const auto& t = theme();
        const auto onBody = t.knobBody.contrasting (1.0f).interpolatedWith (t.knobBody.getPerceivedBrightness() > 0.5f ? t.text : t.knobPointer, 0.25f);
        const auto blackKey = t.knobBody.getPerceivedBrightness() > 0.5f ? t.text : t.knobBody;
        setColour (juce::Label::textColourId, t.text);
        setColour (juce::ComboBox::backgroundColourId, t.knobBody);
        setColour (juce::ComboBox::textColourId, onBody);
        setColour (juce::ComboBox::outlineColourId, t.panelEdge.withAlpha (0.6f));
        setColour (juce::ComboBox::arrowColourId, t.accent);
        setColour (juce::PopupMenu::backgroundColourId, t.knobBody.withAlpha (0.98f));
        setColour (juce::PopupMenu::textColourId, onBody);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, t.accent);
        setColour (juce::PopupMenu::highlightedTextColourId, t.knobBody);
        setColour (juce::PopupMenu::headerTextColourId, t.title);
        setColour (juce::TextButton::buttonColourId, t.knobBody);
        setColour (juce::TextButton::buttonOnColourId, t.accent);
        setColour (juce::TextButton::textColourOffId, onBody);
        setColour (juce::TextButton::textColourOnId, t.knobBody);
        setColour (juce::Slider::textBoxTextColourId, onBody);
        setColour (juce::Label::textWhenEditingColourId, onBody);
        setColour (juce::BubbleComponent::backgroundColourId, t.knobBody);
        setColour (juce::BubbleComponent::outlineColourId, t.accent);
        setColour (juce::TooltipWindow::backgroundColourId, t.knobBody);
        setColour (juce::TooltipWindow::textColourId, onBody);
        setColour (juce::AlertWindow::backgroundColourId, t.knobBody);
        setColour (juce::AlertWindow::textColourId, onBody);
        setColour (juce::TextEditor::backgroundColourId, t.panel.withAlpha (1.0f));
        setColour (juce::TextEditor::textColourId, t.text);
        setColour (juce::TextEditor::outlineColourId, t.panelEdge);
        setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, t.accent.withAlpha (0.7f));
        setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, t.accent2.withAlpha (0.35f));
        setColour (juce::MidiKeyboardComponent::whiteNoteColourId, t.knobPointer.interpolatedWith (juce::Colours::white, 0.85f));
        setColour (juce::MidiKeyboardComponent::blackNoteColourId, blackKey);
        setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, t.knobBody.withAlpha (0.5f));
        setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
        setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, t.knobBody);
        setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, t.accent);
    }

    juce::Font titleFont (float h) const
    {
        return juce::Font (juce::FontOptions (h, juce::Font::bold)).withExtraKerningFactor (theme().uppercaseTitles ? 0.12f : 0.03f);
    }

    // ------------------------------------------------------------ background
    void drawBackground (juce::Graphics& g, juce::Rectangle<float> r) const
    {
        const auto& t = theme();
        g.setGradientFill (juce::ColourGradient (t.bgTop, r.getTopLeft(), t.bgBottom, r.getBottomRight(), false));
        g.fillRect (r);
        juce::Random rnd (1959);
        switch (t.pattern)
        {
            case Theme::Pattern::Tolex:
            {   // pebbled vinyl: thousands of tiny specks
                for (int i = 0; i < 9000; ++i)
                {
                    const float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
                    g.setColour (i % 2 ? t.patternA : t.patternB);
                    g.fillEllipse (x, y, 1.6f + rnd.nextFloat() * 1.8f, 1.6f + rnd.nextFloat() * 1.8f);
                }
                break;
            }
            case Theme::Pattern::Tweed:
            {   // diagonal twill weave
                for (float d = -r.getHeight(); d < r.getWidth(); d += 7.0f)
                {
                    g.setColour (t.patternA);
                    g.drawLine (r.getX() + d, r.getY(), r.getX() + d + r.getHeight(), r.getBottom(), 2.2f);
                    g.setColour (t.patternB);
                    g.drawLine (r.getX() + d + 3.5f, r.getY(), r.getX() + d + 3.5f + r.getHeight(), r.getBottom(), 1.0f);
                }
                for (float d = 0; d < r.getWidth() + r.getHeight(); d += 21.0f)
                {
                    g.setColour (t.patternA.withMultipliedAlpha (0.6f));
                    g.drawLine (r.getX() + d, r.getY(), r.getX() + d - r.getHeight(), r.getBottom(), 1.2f);
                }
                break;
            }
            case Theme::Pattern::Burst:
            {   // radial sunburst + faint flame figuring
                const auto c = r.getCentre();
                juce::ColourGradient burst (t.bgTop, c.x, c.y, t.bgBottom, r.getRight(), r.getBottom(), true);
                burst.addColour (0.55, t.bgTop.interpolatedWith (t.bgBottom, 0.45f));
                g.setGradientFill (burst);
                g.fillRect (r);
                for (int i = 0; i < 70; ++i)
                {
                    juce::Path p;
                    const float x0 = r.getX() + r.getWidth() * (float) i / 70.0f;
                    p.startNewSubPath (x0, r.getY());
                    for (float y = r.getY(); y <= r.getBottom(); y += 20.0f)
                        p.lineTo (x0 + 10.0f * std::sin (y * 0.02f + (float) i), y);
                    g.setColour (i % 2 ? t.patternA : t.patternB);
                    g.strokePath (p, juce::PathStrokeType (5.0f + 6.0f * rnd.nextFloat()));
                }
                break;
            }
            case Theme::Pattern::Grille:
            {   // grille cloth: fine crossing threads
                for (float x = r.getX(); x < r.getRight(); x += 4.0f) { g.setColour (t.patternA); g.drawVerticalLine ((int) x, r.getY(), r.getBottom()); }
                for (float y = r.getY(); y < r.getBottom(); y += 4.0f) { g.setColour (t.patternB); g.drawHorizontalLine ((int) y, r.getX(), r.getRight()); }
                break;
            }
            case Theme::Pattern::Neon:
            {   // glowing circuit traces
                for (int i = 0; i < 46; ++i)
                {
                    juce::Path p;
                    float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
                    p.startNewSubPath (x, y);
                    for (int k = 0; k < 4; ++k)
                    {
                        if (k % 2) x += (rnd.nextFloat() - 0.5f) * 300.0f; else y += (rnd.nextFloat() - 0.5f) * 200.0f;
                        p.lineTo (x, y);
                    }
                    g.setColour (i % 2 ? t.patternA : t.patternB);
                    g.strokePath (p, juce::PathStrokeType (1.5f));
                    g.fillEllipse (x - 3, y - 3, 6, 6);
                }
                break;
            }
            case Theme::Pattern::Pickguard:
            {   // big offset pickguard curves
                for (int i = 0; i < 5; ++i)
                {
                    juce::Path p;
                    const float o = (float) i * 70.0f;
                    p.startNewSubPath (r.getX() - 50, r.getBottom() - o);
                    p.cubicTo (r.getX() + r.getWidth() * 0.3f, r.getY() + r.getHeight() * 0.3f - o,
                               r.getX() + r.getWidth() * 0.7f, r.getBottom() + 100 - o, r.getRight() + 50, r.getY() + 150 - o);
                    g.setColour (i % 2 ? t.patternA : t.patternB);
                    g.strokePath (p, juce::PathStrokeType (34.0f));
                }
                break;
            }
        }
    }


    void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, bool active) const
    {
        const auto& t = theme();
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (r.translated (0, 3), t.corner);
        g.setColour (t.panel);
        g.fillRoundedRectangle (r, t.corner);
        g.setColour (t.panelEdge.withAlpha (active ? 0.9f : 0.35f));
        g.drawRoundedRectangle (r.reduced (0.5f), t.corner, 1.4f);
        g.setColour (active ? t.title : t.dimText);
        g.setFont (titleFont (13.0f));
        g.drawText (t.uppercaseTitles ? title.toUpperCase() : title, r.reduced (12, 4).withHeight (18),
                    juce::Justification::centredLeft);
    }

    // ----------------------------------------------------------------- knobs
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startA, float endA, juce::Slider& s) override
    {
        const auto& t = theme();
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float size = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (size, size);
        const auto c = r.getCentre();
        const float rad = size * 0.5f;
        const float ang = startA + pos * (endA - startA);
        const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
        const float zeroAng = bipolar ? startA + (float) s.valueToProportionOfLength (0.0) * (endA - startA) : startA;

        auto arc = [&] (float radius, float a0, float a1, juce::Colour col, float thick)
        {
            juce::Path p; p.addCentredArc (c.x, c.y, radius, radius, 0, a0, a1, true);
            g.setColour (col); g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        auto pointerAt = [&] (float r0, float r1) { return std::make_pair (c.getPointOnCircumference (r0, ang), c.getPointOnCircumference (r1, ang)); };

        switch (t.knob)
        {
            case Theme::Knob::ChickenHead:
            {
                for (int i = 0; i <= 10; ++i)
                {   // scale ticks
                    const float a = startA + (endA - startA) * i / 10.0f;
                    g.setColour (t.dimText.withAlpha (0.8f));
                    g.drawLine ({ c.getPointOnCircumference (rad * 0.86f, a), c.getPointOnCircumference (rad, a) }, 1.2f);
                }
                arc (rad * 0.78f, juce::jmin (zeroAng, ang), juce::jmax (zeroAng, ang), t.accent.withAlpha (0.8f), 2.0f);
                g.setColour (t.knobBody);
                g.fillEllipse (r.reduced (rad * 0.3f));
                juce::Path head; // pointer-shaped "chicken head" knob
                const float hl = rad * 0.72f, hw = rad * 0.26f;
                head.startNewSubPath (0, -hl);
                head.quadraticTo (hw, -hl * 0.2f, hw * 1.2f, hl * 0.35f);
                head.quadraticTo (0, hl * 0.6f, -hw * 1.2f, hl * 0.35f);
                head.quadraticTo (-hw, -hl * 0.2f, 0, -hl);
                head.closeSubPath();
                const auto tr = juce::AffineTransform::rotation (ang).translated (c);
                g.setColour (juce::Colours::black.withAlpha (0.3f));
                g.fillPath (head, tr.translated (1.5f, 2.0f));
                g.setColour (t.knobBody.brighter (0.15f));
                g.fillPath (head, tr);
                g.setColour (t.knobPointer);
                auto [p0, p1] = pointerAt (rad * 0.05f, rad * 0.66f);
                g.drawLine ({ p0, p1 }, 2.2f);
                break;
            }
            case Theme::Knob::NeonRing:
            {
                arc (rad * 0.86f, startA, endA, t.track, 4.0f);
                for (int glow = 3; glow > 0; --glow)
                    arc (rad * 0.86f, juce::jmin (zeroAng, ang), juce::jmax (zeroAng, ang), t.accent.withAlpha (0.18f), 4.0f + glow * 3.0f);
                arc (rad * 0.86f, juce::jmin (zeroAng, ang), juce::jmax (zeroAng, ang), t.accent, 3.5f);
                g.setColour (t.knobBody);
                g.fillEllipse (r.reduced (rad * 0.3f));
                g.setColour (t.accent2.withAlpha (0.5f));
                g.drawEllipse (r.reduced (rad * 0.3f), 1.0f);
                auto [p0, p1] = pointerAt (rad * 0.25f, rad * 0.62f);
                g.setColour (t.knobPointer);
                g.drawLine ({ p0, p1 }, 2.5f);
                break;
            }
            case Theme::Knob::Bakelite:
            {
                arc (rad * 0.92f, startA, endA, t.track, 1.5f);
                arc (rad * 0.92f, juce::jmin (zeroAng, ang), juce::jmax (zeroAng, ang), t.accent, 3.0f);
                auto body = r.reduced (rad * 0.18f);
                g.setGradientFill (juce::ColourGradient (t.knobBody.brighter (0.35f), body.getTopLeft(), t.knobBody, body.getBottomRight(), false));
                g.fillEllipse (body);
                g.setColour (t.knobBody.brighter (0.6f).withAlpha (0.5f));
                for (int i = 0; i < 24; ++i)
                {   // knurled edge
                    const float a = juce::MathConstants<float>::twoPi * i / 24.0f + ang;
                    g.drawLine ({ c.getPointOnCircumference (rad * 0.66f, a), c.getPointOnCircumference (rad * 0.8f, a) }, 1.0f);
                }
                auto [p0, p1] = pointerAt (rad * 0.4f, rad * 0.62f);
                g.setColour (t.knobPointer);
                g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (p1));
                juce::ignoreUnused (p0);
                break;
            }
            case Theme::Knob::SoftDot:
            {
                arc (rad * 0.88f, startA, endA, t.track, 5.0f);
                arc (rad * 0.88f, juce::jmin (zeroAng, ang), juce::jmax (zeroAng, ang), t.accent, 5.0f);
                auto body = r.reduced (rad * 0.28f);
                g.setColour (juce::Colours::black.withAlpha (0.08f));
                g.fillEllipse (body.translated (0, 2));
                g.setColour (t.knobBody);
                g.fillEllipse (body);
                auto [p0, p1] = pointerAt (0, rad * 0.5f);
                juce::ignoreUnused (p0);
                g.setColour (t.accent2);
                g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (p1));
                break;
            }
        }
    }

    // --------------------------------------------------------------- toggles
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        const auto& t = theme();
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();
        auto pill = r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 4, 34.0f), juce::jmin (r.getHeight() - 4, 16.0f));
        g.setColour (on ? t.accent : t.track);
        g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
        if (over) { g.setColour (t.text.withAlpha (0.15f)); g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f); }
        const float d = pill.getHeight() - 4;
        auto dot = juce::Rectangle<float> (d, d).withCentre ({ on ? pill.getRight() - d * 0.5f - 2 : pill.getX() + d * 0.5f + 2, pill.getCentreY() });
        g.setColour (on ? t.knobBody : t.dimText);
        g.fillEllipse (dot);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const auto& t = theme();
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (t.knobBody);
        g.fillRoundedRectangle (r, juce::jmin (6.0f, t.corner));
        g.setColour (box.hasKeyboardFocus (false) ? t.accent : t.panelEdge.withAlpha (0.5f));
        g.drawRoundedRectangle (r, juce::jmin (6.0f, t.corner), 1.0f);
        juce::Path arrow;
        const float ax = (float) w - 10.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (t.accent);
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (12.0f)); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (2, 1, box.getWidth() - 16, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        const auto& t = theme();
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (down ? t.accent : (over ? t.knobBody.brighter (0.2f) : t.knobBody));
        g.fillRoundedRectangle (r, juce::jmin (6.0f, t.corner));
        g.setColour (t.panelEdge.withAlpha (0.5f));
        g.drawRoundedRectangle (r, juce::jmin (6.0f, t.corner), 1.0f);
    }

private:
    int themeIndex = 0;
};
