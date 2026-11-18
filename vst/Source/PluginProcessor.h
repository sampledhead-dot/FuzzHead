#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/FuzzDSP.h"

namespace ids
{
    #define FUZZ_ID(x) static const juce::String x { #x };
    FUZZ_ID(instrument) FUZZ_ID(inputDb) FUZZ_ID(mix) FUZZ_ID(outputDb)
    FUZZ_ID(synthOn) FUZZ_ID(synthMix) FUZZ_ID(engine) FUZZ_ID(int1) FUZZ_ID(int2) FUZZ_ID(int2On)
    FUZZ_ID(distOn) FUZZ_ID(distGain) FUZZ_ID(distMode) FUZZ_ID(distPre) FUZZ_ID(distPost)
    FUZZ_ID(modOn) FUZZ_ID(modDepth) FUZZ_ID(modMix)
    FUZZ_ID(crushOn) FUZZ_ID(crushRate) FUZZ_ID(crushBits)
    FUZZ_ID(ringOn) FUZZ_ID(ringFreq) FUZZ_ID(ringMix)
    FUZZ_ID(revOn) FUZZ_ID(revTime) FUZZ_ID(revMix)
    FUZZ_ID(order)
    #undef FUZZ_ID
}

// Las 6 permutaciones de Bitcrush(0) / Ring Mod(1) / Gated Reverb(2)
static constexpr int kOrders[6][3] = { {0,1,2}, {0,2,1}, {1,0,2}, {1,2,0}, {2,0,1}, {2,1,0} };

class FuzzHeadProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    FuzzHeadProcessor();
    ~FuzzHeadProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FuzzHead"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 1.2; }

    // Presets de fábrica como "programas"
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Presets de usuario (.preset = XML) en Documentos/FuzzHead/User Presets
    static juce::File userPresetFolder();
    juce::StringArray getUserPresetNames() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::String& name);
    bool deleteUserPreset (const juce::String& name);
    void loadFactory (int index);
    juce::String currentPresetName { "Default" };

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void timerCallback() override;   // regenera la IR de la reverb fuera del hilo de audio
    float p (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }

    fuzz::GuitarSynth synthGuitar, synthBass;   // dos detectores preparados: cambiar de instrumento no reserva memoria
    fuzz::Distortion distPre, distPost;
    fuzz::Bitcrusher crusher;
    fuzz::RingMod ring;
    juce::dsp::Convolution reverb;
    juce::AudioBuffer<float> dry, proc, revBuf;
    juce::SmoothedValue<float> sIn, sOut, sMix, sSynthMix;
    double sr = 44100.0;
    int maxBlock = 512;
    std::atomic<float> loadedRevTime { -1.0f };
    int currentProgram = 0;
    bool lastPre = true, revWasOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FuzzHeadProcessor)
};
