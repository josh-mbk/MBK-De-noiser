#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

namespace ids
{
    inline constexpr const char* reduction = "reduction";
    inline constexpr const char* threshold = "threshold";
    inline constexpr const char* smoothing = "smoothing";
    inline constexpr const char* mix       = "mix";
    inline constexpr const char* output    = "output";
    inline constexpr const char* mode      = "mode";
}

/**
 *  MBK De-noiser - réducteur de bruit spectral (STFT, 2048 points, recouvrement 75 %)
 *  - Mode Adaptive : le bruit de fond est estimé en continu (minimum statistics simplifié)
 *  - Mode Learned  : le bruit est appris sur 2 s à la demande (bouton LEARN)
 */
class MBKDenoiserAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int fftOrder    = 11;
    static constexpr int fftSize     = 1 << fftOrder;   // 2048
    static constexpr int numBins     = fftSize / 2 + 1;
    static constexpr int overlap     = 4;
    static constexpr int hopSize     = fftSize / overlap;
    static constexpr int maxChannels = 2;

    MBKDenoiserAudioProcessor();
    ~MBKDenoiserAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MBK De-noiser"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return (double) fftSize / juce::jmax (1.0, currentSampleRate); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- API pour l'interface ---
    void startLearning() { learnGeneration.fetch_add (1); learning.store (true); }
    bool isLearning() const noexcept { return learning.load(); }
    float getLearnProgress() const noexcept { return learnProgress.load(); }

    std::atomic<float> inputLevel { 0.0f }, outputLevel { 0.0f }, reductionDb { 0.0f };

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    struct Channel
    {
        Channel() : fft (fftOrder) {}
        void reset (int gen);

        juce::dsp::FFT fft;
        std::array<float, fftSize>     inBuf {}, outBuf {};
        std::array<float, fftSize * 2> work {};
        std::array<float, numBins>     noiseAdaptive {}, noiseLearned {}, noiseSum {}, target {}, gain {};
        int  pos = 0, hopCount = 0;
        int  learnGen = 0, learnFramesLeft = 0;
        bool firstFrame = true, hasLearned = false;
    };

    void processFrame (Channel& c, bool isFirstChannel);

    std::array<Channel, maxChannels> channels;
    std::array<float, fftSize> window {};

    std::atomic<float> *pReduction, *pThreshold, *pSmoothing, *pMix, *pOutput, *pMode;
    juce::SmoothedValue<float> smMix, smOut;

    // paramètres du bloc courant (thread audio uniquement)
    float floorGain = 0.1f, threshFactor = 2.0f, attackCoef = 0.2f, releaseCoef = 0.6f;
    bool  learnedMode = false;

    std::atomic<int>   learnGeneration { 0 };
    std::atomic<bool>  learning { false };
    std::atomic<float> learnProgress { 0.0f };
    int learnFramesTotal = 180;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MBKDenoiserAudioProcessor)
};
