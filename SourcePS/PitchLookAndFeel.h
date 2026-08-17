#pragma once

#include <JuceHeader.h>

namespace hhps
{

// Dark studio-hardware palette, close to the plates Logic's own effects use.
namespace colours
{
inline const juce::Colour panel      { 0xff1b1e22 };
inline const juce::Colour panelLight { 0xff262a30 };
inline const juce::Colour outline    { 0xff3a4048 };
inline const juce::Colour text       { 0xffe8ecf1 };
inline const juce::Colour dim        { 0xff8b949e };
inline const juce::Colour accent     { 0xff4bc3ff };
inline const juce::Colour accentWarm { 0xffffb454 };
} // namespace colours

class PitchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PitchLookAndFeel()
    {
        setColour(juce::Label::textColourId, colours::text);
        setColour(juce::Slider::textBoxTextColourId, colours::text);
        setColour(juce::Slider::textBoxOutlineColourId, colours::outline);
        setColour(juce::Slider::textBoxBackgroundColourId, colours::panel);
        setColour(juce::ComboBox::backgroundColourId, colours::panelLight);
        setColour(juce::ComboBox::outlineColourId, colours::outline);
        setColour(juce::ComboBox::textColourId, colours::text);
        setColour(juce::ComboBox::arrowColourId, colours::accent);
        setColour(juce::PopupMenu::backgroundColourId, colours::panelLight);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha(0.35f));
        setColour(juce::PopupMenu::textColourId, colours::text);
        setColour(juce::ToggleButton::textColourId, colours::text);
        setColour(juce::ToggleButton::tickColourId, colours::accent);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float pos, float startAngle, float endAngle,
                          juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle = startAngle + pos * (endAngle - startAngle);
        const auto thickness = juce::jmax(3.0f, radius * 0.16f);

        g.setColour(colours::panelLight);
        g.fillEllipse(juce::Rectangle<float>(radius * 1.55f, radius * 1.55f).withCentre(centre));

        juce::Path track;
        track.addCentredArc(centre.x, centre.y, radius - thickness * 0.5f, radius - thickness * 0.5f,
                            0.0f, startAngle, endAngle, true);
        g.setColour(colours::outline);
        g.strokePath(track, juce::PathStrokeType(thickness, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

        // Bipolar controls fill outwards from 12 o'clock so "no shift" reads at
        // a glance; unipolar ones fill from the left end of the arc.
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float from = bipolar ? (startAngle + 0.5f * (endAngle - startAngle)) : startAngle;

        juce::Path value;
        value.addCentredArc(centre.x, centre.y, radius - thickness * 0.5f, radius - thickness * 0.5f,
                            0.0f, juce::jmin(from, angle), juce::jmax(from, angle), true);
        g.setColour(slider.isEnabled() ? colours::accent : colours::dim.withAlpha(0.4f));
        g.strokePath(value, juce::PathStrokeType(thickness, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        juce::Path pointer;
        const float pointerLength = radius * 0.62f;
        pointer.addRoundedRectangle(-1.4f, -pointerLength, 2.8f, pointerLength, 1.4f);
        pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centre));
        g.setColour(colours::text);
        g.fillPath(pointer);
    }

    juce::Font getLabelFont(juce::Label&) override
    {
        return juce::Font(juce::FontOptions(13.0f));
    }
};

} // namespace hhps
