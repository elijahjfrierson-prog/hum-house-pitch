#include "PitchEditor.h"
#include "PitchProcessor.h"

namespace hhps
{

namespace
{
constexpr int kWidth = 760;
constexpr int kHeight = 430;
} // namespace

PitchEditor::Knob::Knob(juce::AudioProcessorValueTreeState& state, const char* paramId,
                        const juce::String& caption, juce::Component& parent)
    : attachment(state, paramId, slider)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 74, 18);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                               juce::MathConstants<float>::pi * 2.8f, true);
    parent.addAndMakeVisible(slider);

    label.setText(caption, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, colours::dim);
    parent.addAndMakeVisible(label);
}

void PitchEditor::Knob::setBounds(juce::Rectangle<int> area)
{
    label.setBounds(area.removeFromTop(16));
    slider.setBounds(area);
}

void PitchEditor::Knob::setEnabled(bool shouldBeEnabled)
{
    slider.setEnabled(shouldBeEnabled);
    label.setColour(juce::Label::textColourId,
                    shouldBeEnabled ? colours::dim : colours::dim.withAlpha(0.4f));
    label.repaint();
}

PitchEditor::PitchEditor(PitchProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      semitones(p.state(), pid::semitones, "SEMI TONES", *this),
      cents(p.state(), pid::cents, "CENTS", *this),
      mix(p.state(), pid::mix, "MIX", *this),
      grainMs(p.state(), pid::grainMs, "GRAIN SIZE", *this),
      crossfade(p.state(), pid::crossfade, "CROSSFADE", *this),
      dblDetune(p.state(), pid::dblDetune, "DETUNE", *this),
      dblDelay(p.state(), pid::dblDelay, "DELAY", *this),
      dblWidth(p.state(), pid::dblWidth, "WIDTH", *this),
      dblLevel(p.state(), pid::dblLevel, "LEVEL", *this),
      lowCut(p.state(), pid::lowCut, "LOW CUT", *this),
      output(p.state(), pid::output, "OUTPUT", *this),
      grainAutoAttachment(p.state(), pid::grainAuto, grainAutoButton),
      dblOnAttachment(p.state(), pid::dblOn, dblOnButton),
      monoInputAttachment(p.state(), pid::monoInput, monoInputButton)
{
    setLookAndFeel(&lnf);

    titleLabel.setText("HumHouse Pitch Shifter", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(21.0f).withStyle("Bold")));
    titleLabel.setColour(juce::Label::textColourId, colours::text);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("granular pitch shift  \u00b7  guitar dual tracking",
                          juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    subtitleLabel.setColour(juce::Label::textColourId, colours::dim);
    addAndMakeVisible(subtitleLabel);

    int id = 1;
    for (const auto& preset : factoryPresets())
        presetBox.addItem(preset.name, id++);
    presetBox.setSelectedId(processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;
        if (index >= 0 && index != processor.getCurrentProgram())
            processor.setCurrentProgram(index);
    };
    addAndMakeVisible(presetBox);

    addAndMakeVisible(grainAutoButton);
    addAndMakeVisible(dblOnButton);
    addAndMakeVisible(monoInputButton);
    grainAutoButton.onStateChange = [this] { refreshEnablement(); };
    dblOnButton.onStateChange = [this] { refreshEnablement(); };

    refreshEnablement();
    setSize(kWidth, kHeight);
    startTimerHz(8);
}

PitchEditor::~PitchEditor()
{
    setLookAndFeel(nullptr);
}

void PitchEditor::refreshEnablement()
{
    grainMs.setEnabled(! grainAutoButton.getToggleState());
    const bool doubling = dblOnButton.getToggleState();
    for (auto* k : { &dblDetune, &dblDelay, &dblWidth, &dblLevel })
        k->setEnabled(doubling);
}

void PitchEditor::timerCallback()
{
    // Presets can also be recalled from the host, so keep the box in step.
    const int wanted = processor.getCurrentProgram() + 1;
    if (presetBox.getSelectedId() != wanted)
        presetBox.setSelectedId(wanted, juce::dontSendNotification);
}

void PitchEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::panel);

    juce::ColourGradient header(colours::panelLight, 0.0f, 0.0f,
                               colours::panel, 0.0f, 72.0f, false);
    g.setGradientFill(header);
    g.fillRect(0, 0, getWidth(), 72);

    g.setColour(colours::accent.withAlpha(0.7f));
    g.fillRect(0, 71, getWidth(), 1);

    for (const auto& [title, area] : sectionTitles)
    {
        g.setColour(colours::panelLight.withAlpha(0.55f));
        g.fillRoundedRectangle(area.toFloat(), 8.0f);
        g.setColour(colours::outline);
        g.drawRoundedRectangle(area.toFloat().reduced(0.5f), 8.0f, 1.0f);

        g.setColour(colours::accentWarm);
        g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
        g.drawText(title, area.reduced(10, 6).removeFromTop(14),
                   juce::Justification::centredLeft);
    }
}

void PitchEditor::layoutSection(juce::Rectangle<int> area, const std::vector<Knob*>& knobs)
{
    if (knobs.empty())
        return;
    const int each = area.getWidth() / static_cast<int>(knobs.size());
    for (auto* knob : knobs)
        knob->setBounds(area.removeFromLeft(each).reduced(4, 0));
}

void PitchEditor::resized()
{
    auto bounds = getLocalBounds();

    auto header = bounds.removeFromTop(72).reduced(16, 10);
    auto presetArea = header.removeFromRight(220);
    presetBox.setBounds(presetArea.withSizeKeepingCentre(220, 26));
    titleLabel.setBounds(header.removeFromTop(28));
    subtitleLabel.setBounds(header);

    bounds.reduce(12, 10);
    sectionTitles.clear();

    auto topRow = bounds.removeFromTop(bounds.getHeight() / 2 - 4);
    auto pitchArea = topRow.removeFromLeft(static_cast<int>(topRow.getWidth() * 0.56f));
    topRow.removeFromLeft(8);
    auto timingArea = topRow;

    sectionTitles.emplace_back("PITCH", pitchArea);
    sectionTitles.emplace_back("TIMING", timingArea);

    auto pitchInner = pitchArea.reduced(10, 0).withTrimmedTop(22).withTrimmedBottom(8);
    layoutSection(pitchInner, { &semitones, &cents, &mix });

    auto timingInner = timingArea.reduced(10, 0).withTrimmedTop(22).withTrimmedBottom(8);
    auto autoRow = timingInner.removeFromBottom(22);
    grainAutoButton.setBounds(autoRow.removeFromLeft(90));
    monoInputButton.setBounds(autoRow.removeFromRight(100));
    layoutSection(timingInner, { &grainMs, &crossfade });

    bounds.removeFromTop(8);
    auto bottomRow = bounds;
    auto dblArea = bottomRow.removeFromLeft(static_cast<int>(bottomRow.getWidth() * 0.7f));
    bottomRow.removeFromLeft(8);
    auto outArea = bottomRow;

    sectionTitles.emplace_back("DUAL TRACKING", dblArea);
    sectionTitles.emplace_back("OUTPUT", outArea);

    auto dblInner = dblArea.reduced(10, 0).withTrimmedTop(22).withTrimmedBottom(8);
    dblOnButton.setBounds(dblInner.removeFromTop(20).removeFromLeft(120));
    layoutSection(dblInner, { &dblDetune, &dblDelay, &dblWidth, &dblLevel });

    auto outInner = outArea.reduced(10, 0).withTrimmedTop(30).withTrimmedBottom(8);
    layoutSection(outInner, { &lowCut, &output });
}

} // namespace hhps
