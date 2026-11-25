#include "PluginEditor.h"

static float dbToMeter (float gain)
{
    return juce::jmap (juce::Decibels::gainToDecibels (gain, -60.0f), -60.0f, 0.0f, 0.0f, 1.0f);
}

MBKDenoiserAudioProcessorEditor::MBKDenoiserAudioProcessorEditor (MBKDenoiserAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&laf);

    initKnob (knobs[0], "REDUCTION", ids::reduction, " dB");
    initKnob (knobs[1], "THRESHOLD", ids::threshold, " dB");
    initKnob (knobs[2], "SMOOTHING", ids::smoothing, " %");
    initKnob (knobs[3], "MIX",       ids::mix,       " %");
    initKnob (knobs[4], "OUTPUT",    ids::output,    " dB");

    modeLabel.setText ("MODE", juce::dontSendNotification);
    modeLabel.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    addAndMakeVisible (modeLabel);

    modeBox.addItemList ({ "Adaptive", "Learned" }, 1);
    addAndMakeVisible (modeBox);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ids::mode, modeBox);

    learnButton.setTooltip ("Jouez uniquement le bruit de fond (silence) pendant 2 secondes");
    learnButton.onClick = [this] { processor.startLearning(); };
    addAndMakeVisible (learnButton);

    addAndMakeVisible (inBar);
    addAndMakeVisible (outBar);
    addAndMakeVisible (grBar);

    setSize (620, 400);
    startTimerHz (30);
}

MBKDenoiserAudioProcessorEditor::~MBKDenoiserAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MBKDenoiserAudioProcessorEditor::initKnob (Knob& k, const juce::String& title,
                                               const char* paramId, const juce::String& suffix)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                  juce::MathConstants<float>::pi * 2.75f, true);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    k.slider.setTextValueSuffix (suffix);
    k.slider.setNumDecimalPlacesToDisplay (1);
    k.slider.setDoubleClickReturnValue (true, processor.apvts.getParameter (paramId)->convertFrom0to1 (
                                                  processor.apvts.getParameter (paramId)->getDefaultValue()));
    addAndMakeVisible (k.slider);

    k.label.setText (title, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, paramId, k.slider);
}

void MBKDenoiserAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (mbk::bgTop, 0.0f, 0.0f, mbk::bgBottom, 0.0f, b.getBottom(), false));
    g.fillAll();

    // en-tête
    auto header = b.removeFromTop (74.0f).reduced (24.0f, 12.0f);
    g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)));
    g.setColour (mbk::accent);
    g.drawText ("MBK", header.removeFromLeft (70.0f), juce::Justification::centredLeft);
    g.setColour (mbk::text);
    g.drawText ("DE-NOISER", header.removeFromLeft (220.0f), juce::Justification::centredLeft);

    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.setColour (mbk::muted);
    g.drawText (juce::String ("Spectral Noise Reducer  -  v") + JucePlugin_VersionString,
                header, juce::Justification::centredRight);

    g.setColour (mbk::accent.withAlpha (0.25f));
    g.fillRect (24.0f, 74.0f, (float) getWidth() - 48.0f, 1.0f);

    // panneau du bas
    auto bottom = juce::Rectangle<float> (16.0f, (float) getHeight() - 112.0f, (float) getWidth() - 32.0f, 96.0f);
    g.setColour (mbk::panel.withAlpha (0.55f));
    g.fillRoundedRectangle (bottom, 12.0f);
}

void MBKDenoiserAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (84);

    auto bottom = area.removeFromBottom (112).reduced (32, 20);
    auto knobArea = area.reduced (16, 0);

    const int colW = knobArea.getWidth() / 5;
    for (auto& k : knobs)
    {
        auto col = knobArea.removeFromLeft (colW);
        k.label.setBounds (col.removeFromTop (20));
        k.slider.setBounds (col.reduced (4, 0));
    }

    auto left = bottom.removeFromLeft (230);
    auto row1 = left.removeFromTop (30);
    modeLabel.setBounds (row1.removeFromLeft (48));
    modeBox.setBounds (row1.reduced (0, 1));
    left.removeFromTop (8);
    learnButton.setBounds (left.removeFromTop (34));

    bottom.removeFromLeft (28);
    inBar.setBounds  (bottom.removeFromTop (22));
    outBar.setBounds (bottom.removeFromTop (22));
    grBar.setBounds  (bottom.removeFromTop (22));
}

void MBKDenoiserAudioProcessorEditor::timerCallback()
{
    inBar.setLevel  (dbToMeter (processor.inputLevel.exchange  (0.0f)));
    outBar.setLevel (dbToMeter (processor.outputLevel.exchange (0.0f)));
    grBar.setLevel  (juce::jlimit (0.0f, 1.0f, processor.reductionDb.load() / 40.0f));

    if (processor.isLearning())
    {
        learnButton.setButtonText ("LEARNING...  " + juce::String (juce::roundToInt (processor.getLearnProgress() * 100.0f)) + " %");
        learnButton.setColour (juce::TextButton::buttonColourId, mbk::accent2.withAlpha (0.6f));
    }
    else
    {
        learnButton.setButtonText ("LEARN NOISE");
        learnButton.setColour (juce::TextButton::buttonColourId, mbk::panel);
    }
}
