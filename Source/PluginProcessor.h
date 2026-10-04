#pragma once
#include <JuceHeader.h>
#include <array>

class StemCleanerProAudioProcessor : public juce::AudioProcessor
{
public:
    StemCleanerProAudioProcessor();
    ~StemCleanerProAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    static juce::StringArray getStemNames()
    { return { "Kick","Bass","Guitar","Synth","Hats","Drums","Piano","Voice" }; }

    void applyStemPreset(int index);
    void startNoiseLearn();
    void stopNoiseLearn();
    bool isLearningNoise() const { return learning.load(); }

    float getInputLevel() const { return inLevel.load(); }
    float getOutputLevel() const { return outLevel.load(); }

    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    void getSpectrumData(float* dest, int numBins);
    juce::String getProblemsText();

private:
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // DSP
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coefs = juce::dsp::IIR::Coefficients<float>;
    juce::dsp::ProcessorDuplicator<Filter, Coefs> hp, lowShelfF, peakLowMid, peakMid, peakHighMid, highShelfF, airF, hissLP;

    double currentSampleRate = 44100.0;
    juce::AudioBuffer<float> dryBuffer;

    float gateEnv = 0.0f, gateGain = 1.0f;
    float fastEnv[2] = {0,0}, slowEnv[2] = {0,0};

    // Learn
    std::atomic<bool> learning { false };
    int learnSamples = 0;
    float learnSum = 0.0f;
    int learnCount = 0;
    std::atomic<float> learnedNoiseDb { -60.0f };

    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f };

    // Spectrum
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize> fifo {};
    std::array<float, fftSize*2> fftData {};
    std::array<float, fftSize/2> spectrumMags {};
    int fifoIndex = 0;
    juce::CriticalSection specLock;

    void pushSpectrum(const float* s, int n);
    void doSpectrumFFT();
    void updateFilterCoefs(float lowG, float lowMidG, float midG, float highMidG, float highG, float airG, float clarity, float warmth, float cleanAmt);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemCleanerProAudioProcessor)
};
