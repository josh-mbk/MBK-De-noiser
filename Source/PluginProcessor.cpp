#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
void MBKDenoiserAudioProcessor::Channel::reset (int gen)
{
    inBuf.fill (0.0f); outBuf.fill (0.0f); work.fill (0.0f);
    noiseAdaptive.fill (0.0f); noiseLearned.fill (0.0f); noiseSum.fill (0.0f);
    target.fill (1.0f); gain.fill (1.0f);
    pos = hopCount = 0;
    learnGen = gen;
    learnFramesLeft = 0;
    firstFrame = true;
    hasLearned = false;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MBKDenoiserAudioProcessor::createLayout()
{
    using juce::AudioParameterFloat;
    using juce::NormalisableRange;
    using juce::ParameterID;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::reduction, 1 }, "Reduction",
                 NormalisableRange<float> (0.0f, 40.0f, 0.1f), 18.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::threshold, 1 }, "Threshold",
                 NormalisableRange<float> (0.0f, 20.0f, 0.1f), 6.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::smoothing, 1 }, "Smoothing",
                 NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::mix, 1 }, "Mix",
                 NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::output, 1 }, "Output",
                 NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (ParameterID { ids::mode, 1 }, "Mode",
                 juce::StringArray { "Adaptive", "Learned" }, 0));

    return { p.begin(), p.end() };
}

//==============================================================================
MBKDenoiserAudioProcessor::MBKDenoiserAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MBK_DENOISER", createLayout())
{
    for (int i = 0; i < fftSize; ++i)   // fenêtre de Hann périodique
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) fftSize);

    pReduction = apvts.getRawParameterValue (ids::reduction);
    pThreshold = apvts.getRawParameterValue (ids::threshold);
    pSmoothing = apvts.getRawParameterValue (ids::smoothing);
    pMix       = apvts.getRawParameterValue (ids::mix);
    pOutput    = apvts.getRawParameterValue (ids::output);
    pMode      = apvts.getRawParameterValue (ids::mode);
}

bool MBKDenoiserAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

void MBKDenoiserAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    learnFramesTotal  = juce::jmax (1, (int) (2.0 * sampleRate / hopSize));   // ~2 secondes

    for (auto& c : channels)
        c.reset (learnGeneration.load());

    smMix.reset (sampleRate, 0.05);  smMix.setCurrentAndSetTargetValue (pMix->load() * 0.01f);
    smOut.reset (sampleRate, 0.05);  smOut.setCurrentAndSetTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

    learning.store (false);
    learnProgress.store (0.0f);
    setLatencySamples (fftSize);
}

//==============================================================================
void MBKDenoiserAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh      = juce::jmin (buffer.getNumChannels(), maxChannels);
    const int numSamples = buffer.getNumSamples();

    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    floorGain    = juce::Decibels::decibelsToGain (-pReduction->load());
    threshFactor = juce::Decibels::decibelsToGain (pThreshold->load());
    const float s = pSmoothing->load() * 0.01f;
    attackCoef   = 0.5f * s;
    releaseCoef  = 0.3f + 0.65f * s;
    learnedMode  = pMode->load() > 0.5f;
    smMix.setTargetValue (pMix->load() * 0.01f);
    smOut.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

    float* data[maxChannels] = {};
    for (int ch = 0; ch < numCh; ++ch)
        data[ch] = buffer.getWritePointer (ch);

    float peakIn = 0.0f, peakOut = 0.0f;
    constexpr int mask = fftSize - 1;

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix = smMix.getNextValue();
        const float og  = smOut.getNextValue();

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto& c = channels[(size_t) ch];
            const float in = data[ch][i];

            const float dry = c.inBuf[(size_t) c.pos];     // échantillon d'il y a fftSize (aligné sur la latence)
            c.inBuf[(size_t) c.pos] = in;
            const float wet = c.outBuf[(size_t) c.pos];
            c.outBuf[(size_t) c.pos] = 0.0f;
            c.pos = (c.pos + 1) & mask;

            if (++c.hopCount >= hopSize)
            {
                c.hopCount = 0;
                processFrame (c, ch == 0);
            }

            const float out = (dry * (1.0f - mix) + wet * mix) * og;
            data[ch][i] = out;

            peakIn  = juce::jmax (peakIn,  std::abs (in));
            peakOut = juce::jmax (peakOut, std::abs (out));
        }
    }

    inputLevel.store  (juce::jmax (inputLevel.load(),  peakIn));
    outputLevel.store (juce::jmax (outputLevel.load(), peakOut));
}

//==============================================================================
void MBKDenoiserAudioProcessor::processFrame (Channel& c, bool isFirstChannel)
{
    constexpr int mask = fftSize - 1;

    // 1) fenêtrage (le plus ancien échantillon est à c.pos)
    for (int i = 0; i < fftSize; ++i)
        c.work[(size_t) i] = c.inBuf[(size_t) ((c.pos + i) & mask)] * window[(size_t) i];
    std::fill (c.work.begin() + fftSize, c.work.end(), 0.0f);

    c.fft.performRealOnlyForwardTransform (c.work.data(), true);

    // 2) demande d'apprentissage ?
    const int gen = learnGeneration.load();
    if (gen != c.learnGen)
    {
        c.learnGen = gen;
        c.learnFramesLeft = learnFramesTotal;
        c.noiseSum.fill (0.0f);
    }
    const bool isLearningNow = c.learnFramesLeft > 0;

    // 3) analyse : estimation du bruit + gain cible par bin
    constexpr float riseCoef = 0.0015f;
    constexpr float adaptiveBias = 1.5f;

    for (int k = 0; k < numBins; ++k)
    {
        const float re = c.work[(size_t) (2 * k)], im = c.work[(size_t) (2 * k + 1)];
        const float mag = std::sqrt (re * re + im * im);

        float& na = c.noiseAdaptive[(size_t) k];
        if (c.firstFrame)           na = mag;
        else if (mag < na)          na = 0.5f * (na + mag);
        else                        na += (mag - na) * riseCoef;

        if (isLearningNow)
            c.noiseSum[(size_t) k] += mag;

        const float noise = (learnedMode && c.hasLearned) ? c.noiseLearned[(size_t) k]
                                                          : na * adaptiveBias;
        const float t = noise * threshFactor;
        const float g = mag > 1.0e-9f ? juce::jmax (floorGain, 1.0f - t / mag) : floorGain;
        c.target[(size_t) k] = juce::jmin (1.0f, g);
    }
    c.firstFrame = false;

    if (isLearningNow && --c.learnFramesLeft == 0)
    {
        const float inv = 1.0f / (float) learnFramesTotal;
        for (int k = 0; k < numBins; ++k)
            c.noiseLearned[(size_t) k] = c.noiseSum[(size_t) k] * inv;
        c.hasLearned = true;
    }

    // 4) lissage fréquentiel (3 points) puis temporel, application du gain
    float gainSum = 0.0f;
    for (int k = 0; k < numBins; ++k)
    {
        const float prev = c.target[(size_t) juce::jmax (0, k - 1)];
        const float next = c.target[(size_t) juce::jmin (numBins - 1, k + 1)];
        const float gt   = 0.25f * prev + 0.5f * c.target[(size_t) k] + 0.25f * next;

        float& gs = c.gain[(size_t) k];
        const float coef = gt > gs ? attackCoef : releaseCoef;
        gs = coef * gs + (1.0f - coef) * gt;

        c.work[(size_t) (2 * k)]     *= gs;
        c.work[(size_t) (2 * k + 1)] *= gs;
        gainSum += gs;
    }

    if (isFirstChannel)
    {
        const float meanGain = juce::jmax (1.0e-5f, gainSum / (float) numBins);
        reductionDb.store (-20.0f * std::log10 (meanGain));
        learning.store (c.learnFramesLeft > 0);
        learnProgress.store (c.learnFramesLeft > 0
                                 ? 1.0f - (float) c.learnFramesLeft / (float) learnFramesTotal : 0.0f);
    }

    // 5) resynthèse + overlap-add (somme des Hann² à 75 % = 1.5)
    c.fft.performRealOnlyInverseTransform (c.work.data());

    constexpr float scale = 1.0f / (0.375f * (float) overlap);
    for (int i = 0; i < fftSize; ++i)
        c.outBuf[(size_t) ((c.pos + i) & mask)] += c.work[(size_t) i] * window[(size_t) i] * scale;
}

//==============================================================================
juce::AudioProcessorEditor* MBKDenoiserAudioProcessor::createEditor()
{
    return new MBKDenoiserAudioProcessorEditor (*this);
}

void MBKDenoiserAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MBKDenoiserAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MBKDenoiserAudioProcessor();
}
