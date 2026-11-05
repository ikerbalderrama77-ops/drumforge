#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>

// Biquad RBJ sin asignaciones de memoria (seguro para tiempo real)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() noexcept { z1 = z2 = 0.0f; }
    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void setPeaking   (double sr, double freq, double q, double gainDb) noexcept;
    void setHighShelf (double sr, double freq, double q, double gainDb) noexcept;
    void setHighpass  (double sr, double freq, double q) noexcept;

private:
    void set (double nb0, double nb1, double nb2, double a0, double na1, double na2) noexcept;
};

class DrumForgeAudioProcessor : public juce::AudioProcessor
{
public:
    DrumForgeAudioProcessor();
    ~DrumForgeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DRUMFORGE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    float getPeakIn() const { return peakInMeter.load(); }
    float getPeakOut() const { return peakOutMeter.load(); }
    float getClipReduction() const { return clipReductionMeter.load(); }

    static constexpr int scopeSize = 128;
    float getScopeIn (int i) const  { return scopeIn[(size_t) i].load (std::memory_order_relaxed); }
    float getScopeOut (int i) const { return scopeOut[(size_t) i].load (std::memory_order_relaxed); }
    int getScopeWritePos() const    { return scopeWrite.load (std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    std::atomic<float>* raw (const char* id) const { return apvts.getRawParameterValue (id); }
    float whiteNoise() noexcept
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (int32_t) rng * (1.0f / 2147483648.0f);
    }

    // Parámetros (punteros atómicos cacheados)
    std::atomic<float> *pMode, *pDelta, *pSolo, *pLayerType, *pBlend, *pTone, *pPreDelay, *pAttack,
        *pSustain, *pSubBody, *pMud, *pSat, *pAir, *pClipMode, *pOutput, *pMix, *pBypass;

    double sr = 44100.0;
    std::vector<float> delayL, delayR;
    int delayMask = 0, delayWritePos = 0;

    float envFast = 0, envSlow = 0;
    int snapAge = 0, snapMax = 2200, transientCooldown = 0;
    bool snapActive = false;
    float snapPhase = 0, snapEnergy = 0;

    float tsFastL = 0, tsSlowL = 0, tsFastR = 0, tsSlowR = 0;
    float subPhase = 0, subEnv = 0;
    uint32_t rng = 22222u;

    Biquad mudL, mudR, airL, airR, modeL, modeR;

    juce::SmoothedValue<float> sBlend, sTone, sAttack, sSustain, sSub, sSat, sMix, sGain;

    int scopeDecim = 24, scopeCount = 0;
    std::array<std::atomic<float>, scopeSize> scopeIn {}, scopeOut {};
    std::atomic<int> scopeWrite { 0 };

    std::atomic<float> peakInMeter { 0.0f }, peakOutMeter { 0.0f }, clipReductionMeter { 0.0f };
    float peakInHold = 0, peakOutHold = 0, clipHold = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumForgeAudioProcessor)
};
