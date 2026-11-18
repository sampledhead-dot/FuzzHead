#pragma once
#include "PluginProcessor.h"
#include "UpdateBanner.h"

namespace col
{
    // Paleta inspirada en el logo de Sampled Head: naranja, crema (placa del sampler) y negro grafiti
    const juce::Colour bg (0xff17130f), panel (0xfff3efe9), panel2 (0xffe8e2d9), line (0xffd6cec2),
                       text (0xff111111), muted (0xff6f675e), accent (0xffd9600b),
                       cream (0xfff3efe9), orangeD (0xffa94a07), orangeL (0xffef7a22),
                       synth = accent, dist = accent, mod = accent, crush = accent, ring = accent, rev = accent;
}

class FuzzLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FuzzLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
};

// Knob con etiqueta y valor
struct Knob : public juce::Component
{
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label, juce::Colour c, bool onDark = false);
    void resized() override;
    juce::Slider slider;
    juce::Label name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Panel de módulo: fondo, franja de color, título e interruptor On
struct ModulePanel : public juce::Component
{
    ModulePanel (const juce::String& t, juce::Colour c) : title (t), colour (c) {}
    void paint (juce::Graphics&) override;
    juce::String title; juce::Colour colour; bool dim = false; int titleX = 14;
};

class FuzzHeadEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FuzzHeadEditor (FuzzHeadProcessor&);
    ~FuzzHeadEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setParam (const juce::String& id, float v);
    float getParam (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void refreshPresets();
    void selectPreset (int itemId);
    void moveFx (int fx, int dir);
    std::unique_ptr<juce::ToggleButton> makeSwitch (const juce::String& id, juce::Colour c);

    FuzzHeadProcessor& proc;
    FuzzLookAndFeel lnf;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    juce::TextButton instGuitar { "Guitarra" }, instBass { "Bajo" };
    juce::ComboBox presetBox;
    juce::TextButton prevB { "<" }, nextB { ">" }, saveB { "Guardar" }, delB { "Borrar" };
    std::unique_ptr<Knob> kIn, kMix, kOut;

    ModulePanel pSynth { juce::CharPointer_UTF8 ("MOTOR DE SÍNTESIS"), col::synth }, pDist { juce::CharPointer_UTF8 ("DISTORSIÓN"), col::dist }, pMod { juce::CharPointer_UTF8 ("MODULACIÓN"), col::mod };
    ModulePanel pCrush { "BITCRUSH", col::crush }, pRing { "RING MOD", col::ring }, pRev { "GATED REVERB", col::rev };
    std::unique_ptr<juce::ToggleButton> swSynth, swDist, swMod, swCrush, swRing, swRev, swInt2;
    std::vector<std::unique_ptr<BA>> buttonAtts;

    juce::OwnedArray<juce::TextButton> engineButtons, modeButtons;
    juce::TextButton preB { "PRE" }, postB { "POST" };
    juce::Label engineDesc, int2Label { {}, "Intervalo 2" };
    std::unique_ptr<Knob> kSynthMix, kInt1, kInt2, kGain, kDepth, kModMix, kCrush, kBits, kRingF, kRingMix, kTime, kRevMix;
    juce::TextButton fxLeft[3], fxRight[3];
    juce::Label versionLabel;
public:
    shupdate::UpdateBanner update { "FuzzHead", "FuzzHead", FuzzHeadProcessor::kVersion, col::text, col::cream, col::accent };
private:
    int lastOrder = -1, lastEngine = -1, presetCount = 0;
    juce::String lastPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FuzzHeadEditor)
};
