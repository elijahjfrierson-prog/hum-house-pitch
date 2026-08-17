#pragma once

#include <JuceHeader.h>

#include "PitchLookAndFeel.h"

namespace hhps
{

class PitchProcessor;

class PitchEditor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit PitchEditor(PitchProcessor&);
    ~PitchEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // A rotary control plus its caption, wired straight to a parameter.
    struct Knob
    {
        Knob(juce::AudioProcessorValueTreeState& state, const char* paramId,
             const juce::String& caption, juce::Component& parent);

        juce::Slider slider;
        juce::Label label;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;

        void setBounds(juce::Rectangle<int> area);
        void setEnabled(bool shouldBeEnabled);
    };

    void timerCallback() override;
    void refreshEnablement();
    void layoutSection(juce::Rectangle<int> area, const std::vector<Knob*>& knobs);

    PitchProcessor& processor;
    PitchLookAndFeel lnf;

    juce::ComboBox presetBox;
    juce::Label titleLabel, subtitleLabel;

    Knob semitones, cents, mix;
    Knob grainMs, crossfade;
    Knob dblDetune, dblDelay, dblWidth, dblLevel;
    Knob lowCut, output;

    juce::ToggleButton grainAutoButton { "Auto" };
    juce::ToggleButton dblOnButton { "Dual Track" };
    juce::ToggleButton monoInputButton { "Mono In" };
    juce::AudioProcessorValueTreeState::ButtonAttachment grainAutoAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment dblOnAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment monoInputAttachment;

    std::vector<std::pair<juce::String, juce::Rectangle<int>>> sectionTitles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchEditor)
};

} // namespace hhps
