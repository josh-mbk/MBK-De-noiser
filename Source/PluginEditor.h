#pragma once

#include "PluginProcessor.h"

namespace mbk
{
    inline const juce::Colour bgTop    { 0xFF0B1220 };
    inline const juce::Colour bgBottom { 0xFF152238 };
    inline const juce::Colour panel    { 0xFF1B2A44 };
    inline const juce::Colour accent   { 0xFF21D4C0 };
    inline const juce::Colour accent2  { 0xFF3B82F6 };
    inline const juce::Colour text     { 0xFFE6EDF7 };
    inline const juce::Colour muted    { 0xFF7B8AA3 };
    inline const juce::Colour warn     { 0xFFF59E0B };
}

//==============================================================================
class MBKLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MBKLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId,       mbk::text);
        setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId,               mbk::muted);
        setColour (juce::TextButton::buttonColourId,        mbk::panel);
        setColour (juce::TextButton::textColourOffId,       mbk::text);
        setColour (juce::ComboBox::backgroundColourId,      mbk::panel);
        setColour (juce::ComboBox::textColourId,            mbk::text);
        setColour (juce::ComboBox::outlineColourId,         mbk::accent.withAlpha (0.5f));
        setColour (juce::ComboBox::arrowColourId,           mbk::accent);
        setColour (juce::PopupMenu::backgroundColourId,     mbk::panel);
        setColour (juce::PopupMenu::textColourId,           mbk::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, mbk::accent2);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float arcR = radius - 3.0f;
        const float angle = startAngle + pos * (endAngle - startAngle);

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (mbk::panel.brighter (0.15f));
        g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setGradientFill (juce::ColourGradient (mbk::accent2, centre.x - arcR, centre.y,
                                                 mbk::accent,  centre.x + arcR, centre.y, false));
        g.strokePath (value, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float knobR = arcR - 10.0f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xFF2B3D5E), centre.x, centre.y - knobR,
                                                 juce::Colour (0xFF121C30), centre.x, centre.y + knobR, false));
        g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

        juce::Path pointer;
        pointer.addRoundedRectangle (-1.75f, -knobR + 5.0f, 3.5f, knobR * 0.42f, 1.75f);
        g.setColour (mbk::accent);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg,
                               bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto c = bg;
        if (down) c = c.darker (0.2f); else if (over) c = c.brighter (0.12f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (mbk::accent.withAlpha (0.7f));
        g.drawRoundedRectangle (r, 8.0f, 1.3f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return juce::Font (juce::FontOptions (13.0f, juce::Font::bold));
    }
};

//==============================================================================
class LevelBar : public juce::Component
{
public:
    LevelBar (juce::String n, juce::Colour c) : name (std::move (n)), colour (c) {}

    void setLevel (float target)
    {
        level = target > level ? target : level * 0.85f + target * 0.15f;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        auto labelArea = b.removeFromLeft (34.0f);
        g.setColour (mbk::muted);
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText (name, labelArea, juce::Justification::centredLeft);

        auto bar = b.withSizeKeepingCentre (b.getWidth(), 8.0f);
        g.setColour (mbk::panel.darker (0.3f));
        g.fillRoundedRectangle (bar, 4.0f);
        auto fill = bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, level));
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.6f), bar.getX(), 0.0f,
                                                 colour, bar.getRight(), 0.0f, false));
        g.fillRoundedRectangle (fill, 4.0f);
    }

private:
    juce::String name;
    juce::Colour colour;
    float level = 0.0f;
};

//==============================================================================
class MBKDenoiserAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit MBKDenoiserAudioProcessorEditor (MBKDenoiserAudioProcessor&);
    ~MBKDenoiserAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    void initKnob (Knob&, const juce::String& title, const char* paramId, const juce::String& suffix);

    MBKDenoiserAudioProcessor& processor;
    MBKLookAndFeel laf;

    std::array<Knob, 5> knobs;
    juce::ComboBox modeBox;
    juce::Label    modeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    juce::TextButton learnButton { "LEARN NOISE" };

    LevelBar inBar  { "IN",  mbk::accent2 };
    LevelBar outBar { "OUT", mbk::accent };
    LevelBar grBar  { "GR",  mbk::warn };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MBKDenoiserAudioProcessorEditor)
};
