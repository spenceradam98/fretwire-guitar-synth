#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Themes.h"

// One labelled control bound to a parameter. The widget type follows the
// parameter type: choice -> combo box, bool -> switch, otherwise a knob.
class ParamControl : public juce::Component
{
public:
    ParamControl (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& label);
    void resized() override;
private:
    juce::Label label;
    std::unique_ptr<juce::Slider> slider;
    std::unique_ptr<juce::ComboBox> combo;
    std::unique_ptr<juce::ToggleButton> toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bAtt;
};

// A titled panel with a row of controls and an optional power switch.
class Section : public juce::Component, private juce::AudioProcessorValueTreeState::Listener
{
public:
    Section (juce::AudioProcessorValueTreeState&, const juce::String& title,
             std::vector<std::pair<const char*, const char*>> controls, const char* powerId = nullptr);
    ~Section() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    int numControls() const { return (int) controls.size(); }
private:
    void parameterChanged (const juce::String&, float) override;
    juce::AudioProcessorValueTreeState& apvts;
    juce::String title, powerParam;
    std::vector<std::unique_ptr<ParamControl>> controls;
    std::unique_ptr<juce::ToggleButton> power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
};

// Shows what the tracker hears: note name + cents, input level, string mode.
class TunerDisplay : public juce::Component, private juce::Timer
{
public:
    explicit TunerDisplay (FretwireProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    FretwireProcessor& proc;
    float note = -1, level = 0;
    int voices = 0;
};

class FretwireEditor : public juce::AudioProcessorEditor
{
public:
    explicit FretwireEditor (FretwireProcessor&);
    ~FretwireEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designW = 1440, designH = 900;

private:
    class Content : public juce::Component
    {
    public:
        explicit Content (FretwireEditor& e) : owner (e) {}
        void paint (juce::Graphics&) override;
        void resized() override;
        FretwireEditor& owner;
    };

    void applyTheme (int index);
    void refreshCategoryBox();
    void refreshPresetBox();
    void showSaveDialog();

    FretwireProcessor& proc;
    FretwireLookAndFeel laf;
    Content content { *this };
    juce::TooltipWindow tooltips { this, 600 };

    juce::ComboBox categoryBox, presetBox, themeBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "Save" }, initButton { "Init" };
    juce::MidiKeyboardComponent keyboard;
    std::vector<std::unique_ptr<Section>> rows[6];
    TunerDisplay tuner;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    juce::String shownCategory;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FretwireEditor)
};
