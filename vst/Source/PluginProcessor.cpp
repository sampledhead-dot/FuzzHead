#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using API = juce::AudioParameterInt;
using NR  = juce::NormalisableRange<float>;

static NR logRange (float lo, float hi)
{
    return NR (lo, hi,
               [] (float a, float b, float t) { return a * std::pow (b / a, t); },
               [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); });
}

juce::AudioProcessorValueTreeState::ParameterLayout FuzzHeadProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;
    auto pct = juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return juce::String (juce::roundToInt (x * 100)) + " %"; });
    auto db  = juce::AudioParameterFloatAttributes().withLabel ("dB");
    auto hz  = juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return x >= 1000 ? juce::String (x / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (x)) + " Hz"; });
    auto P = [] (const juce::String& id) { return juce::ParameterID { id, 1 }; };

    v.push_back (std::make_unique<APC> (P (ids::instrument), "Instrumento", juce::StringArray { "Guitarra", "Bajo" }, 0));
    v.push_back (std::make_unique<APF> (P (ids::inputDb), "Input", NR (-24.0f, 24.0f, 0.1f), 0.0f, db));
    v.push_back (std::make_unique<APF> (P (ids::mix), "Mix", NR (0.0f, 1.0f), 1.0f, pct));
    v.push_back (std::make_unique<APF> (P (ids::outputDb), "Output", NR (-24.0f, 24.0f, 0.1f), 0.0f, db));

    v.push_back (std::make_unique<APB> (P (ids::synthOn), "Synth On", true));
    v.push_back (std::make_unique<APF> (P (ids::synthMix), "Synth Mix", NR (0.0f, 1.0f), 1.0f, pct));
    v.push_back (std::make_unique<APC> (P (ids::engine), "Engine", juce::StringArray { "Deep", "Vintage", "Solo", "Strings", "Harmony" }, 1));
    v.push_back (std::make_unique<API> (P (ids::int1), "Interval 1", -24, 24, 7));
    v.push_back (std::make_unique<API> (P (ids::int2), "Interval 2", -24, 24, 12));
    v.push_back (std::make_unique<APB> (P (ids::int2On), "Interval 2 On", true));

    v.push_back (std::make_unique<APB> (P (ids::distOn), "Distortion On", false));
    v.push_back (std::make_unique<APF> (P (ids::distGain), "Dist Gain", NR (0.0f, 1.0f), 0.35f, pct));
    v.push_back (std::make_unique<APC> (P (ids::distMode), "Dist Mode", juce::StringArray { "Soft", "Hard", "Rich", "Wavefold" }, 0));
    v.push_back (std::make_unique<APB> (P (ids::distPre), "Dist Pre", true));
    v.push_back (std::make_unique<APB> (P (ids::distPost), "Dist Post", false));

    v.push_back (std::make_unique<APB> (P (ids::modOn), "Modulation On", false));
    v.push_back (std::make_unique<APF> (P (ids::modDepth), "Mod Depth", NR (0.0f, 1.0f), 0.4f, pct));
    v.push_back (std::make_unique<APF> (P (ids::modMix), "Mod Mix", NR (0.0f, 1.0f), 0.5f, pct));

    v.push_back (std::make_unique<APB> (P (ids::crushOn), "Bitcrush On", false));
    auto crushRange = logRange (441.0f, 44100.0f);
    v.push_back (std::make_unique<APF> (P (ids::crushRate), "Crush", crushRange, 44100.0f, hz));
    v.push_back (std::make_unique<APF> (P (ids::crushBits), "Resolution", NR (1.0f, 24.0f, 1.0f), 24.0f));

    v.push_back (std::make_unique<APB> (P (ids::ringOn), "Ring Mod On", false));
    v.push_back (std::make_unique<APF> (P (ids::ringFreq), "Ring Freq", logRange (20.0f, 5000.0f), 440.0f, hz));
    v.push_back (std::make_unique<APF> (P (ids::ringMix), "Ring Mix", NR (0.0f, 1.0f), 0.5f, pct));

    v.push_back (std::make_unique<APB> (P (ids::revOn), "Gated Reverb On", false));
    v.push_back (std::make_unique<APF> (P (ids::revTime), "Reverb Time", NR (0.0f, 1000.0f, 1.0f), 300.0f, juce::AudioParameterFloatAttributes().withLabel ("ms")));
    v.push_back (std::make_unique<APF> (P (ids::revMix), "Reverb Mix", NR (0.0f, 1.0f), 0.35f, pct));

    v.push_back (std::make_unique<APC> (P (ids::order), "FX Order",
        juce::StringArray { "Crush>Ring>Rev", "Crush>Rev>Ring", "Ring>Crush>Rev", "Ring>Rev>Crush", "Rev>Crush>Ring", "Rev>Ring>Crush" }, 0));
    return { v.begin(), v.end() };
}

FuzzHeadProcessor::FuzzHeadProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    startTimerHz (10);
}

bool FuzzHeadProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    const bool inOk  = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return inOk && outOk;
}

void FuzzHeadProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate; maxBlock = samplesPerBlock;
    synthGuitar.prepare (sr, false);
    synthBass.prepare (sr, true);
    distPre.prepare (sr); distPost.prepare (sr);
    crusher.prepare (sr); ring.prepare (sr);
    dry.setSize (2, samplesPerBlock); proc.setSize (2, samplesPerBlock); revBuf.setSize (2, samplesPerBlock);
    reverb.prepare ({ sr, (juce::uint32) samplesPerBlock, 2 });
    reverb.reset();
    loadedRevTime = -1.0f;
    timerCallback();   // IR inicial
    for (auto* s : { &sIn, &sOut, &sMix, &sSynthMix }) s->reset (sr, 0.03);
    sIn.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p (ids::inputDb)));
    sOut.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p (ids::outputDb)));
    sMix.setCurrentAndTargetValue (p (ids::mix));
    sSynthMix.setCurrentAndTargetValue (p (ids::synthMix));
}

void FuzzHeadProcessor::timerCallback()
{
    // La IR depende de Time y de la frecuencia de muestreo; se genera aquí (hilo de mensajes)
    const float t = p (ids::revTime);
    if (sr <= 0 || std::abs (t - loadedRevTime.load()) < 0.5f) return;
    loadedRevTime = t;
    const int L = fuzz::gatedIRLength ((float) sr, t);
    juce::AudioBuffer<float> ir (2, L);
    fuzz::makeGatedIR (ir.getWritePointer (0), L, (float) sr, 0x9E3779B1u);
    fuzz::makeGatedIR (ir.getWritePointer (1), L, (float) sr, 0x85EBCA6Bu);
    reverb.loadImpulseResponse (std::move (ir), sr, juce::dsp::Convolution::Stereo::yes,
                                juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
}

void FuzzHeadProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    if (n > dry.getNumSamples()) { dry.setSize (2, n, false, false, true); proc.setSize (2, n, false, false, true); revBuf.setSize (2, n, false, false, true); }

    // --- Parámetros ---
    sIn.setTargetValue (juce::Decibels::decibelsToGain (p (ids::inputDb)));
    sOut.setTargetValue (juce::Decibels::decibelsToGain (p (ids::outputDb)));
    sMix.setTargetValue (p (ids::mix));
    sSynthMix.setTargetValue (p (ids::synthMix));
    const bool bass = p (ids::instrument) > 0.5f;
    auto& synth = bass ? synthBass : synthGuitar;
    const bool synthOn = p (ids::synthOn) > 0.5f;

    fuzz::SynthParams sp;
    sp.engine = (fuzz::Engine) juce::roundToInt (p (ids::engine));
    sp.int1 = juce::roundToInt (p (ids::int1)); sp.int2 = juce::roundToInt (p (ids::int2));
    sp.int2On = p (ids::int2On) > 0.5f;
    sp.modOn = p (ids::modOn) > 0.5f; sp.modDepth = p (ids::modDepth); sp.modMix = p (ids::modMix);
    synth.setParams (sp);

    // --- Entrada -> seco (estéreo) ---
    for (int i = 0; i < n; ++i)
    {
        const float g = sIn.getNextValue();
        const float l = inCh > 0 ? buffer.getSample (0, i) : 0.0f;
        const float r = inCh > 1 ? buffer.getSample (1, i) : l;
        dry.setSample (0, i, l * g); dry.setSample (1, i, r * g);
    }

    // --- Sintetizador + mezcla del motor ---
    auto* pL = proc.getWritePointer (0); auto* pR = proc.getWritePointer (1);
    const auto* dL = dry.getReadPointer (0); const auto* dR = dry.getReadPointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float m = sSynthMix.getNextValue();
        if (synthOn)
        {
            float sl, sr2;
            synth.process (0.5f * (dL[i] + dR[i]), sl, sr2);
            pL[i] = dL[i] * (1.0f - m) + sl * m;
            pR[i] = dR[i] * (1.0f - m) + sr2 * m;
        }
        else { pL[i] = dL[i]; pR[i] = dR[i]; }
    }

    // --- Efectos ---
    float* chans[2] = { pL, pR };
    const bool distOn = p (ids::distOn) > 0.5f;
    bool pre = p (ids::distPre) > 0.5f, post = p (ids::distPost) > 0.5f;
    if (! pre && ! post) { pre = lastPre; post = ! lastPre; }   // siempre queda una etapa activa
    else if (pre != post) lastPre = pre;
    const auto mode = (fuzz::DistMode) juce::roundToInt (p (ids::distMode));
    const float gain = p (ids::distGain);
    if (distOn && pre) { distPre.set (mode, gain); distPre.process (chans, 2, n); }

    const int orderIdx = juce::jlimit (0, 5, juce::roundToInt (p (ids::order)));
    for (int k = 0; k < 3; ++k)
    {
        switch (kOrders[orderIdx][k])
        {
            case 0:
                if (p (ids::crushOn) > 0.5f) crusher.process (chans, 2, n, p (ids::crushRate), p (ids::crushBits));
                break;
            case 1:
                if (p (ids::ringOn) > 0.5f) ring.process (chans, 2, n, p (ids::ringFreq), p (ids::ringMix));
                break;
            case 2:
            {
                const bool on = p (ids::revOn) > 0.5f;
                // la entrada mono va a los dos canales de la convolución (IR distinta L/R)
                for (int i = 0; i < n; ++i)
                {
                    const float mono = on ? 0.5f * (pL[i] + pR[i]) : 0.0f;
                    revBuf.setSample (0, i, mono); revBuf.setSample (1, i, mono);
                }
                if (on)
                {
                    juce::dsp::AudioBlock<float> blk (revBuf.getArrayOfWritePointers(), 2, (size_t) n);
                    reverb.process (juce::dsp::ProcessContextReplacing<float> (blk));
                    const float m = p (ids::revMix);
                    for (int i = 0; i < n; ++i)
                    {
                        pL[i] = pL[i] * (1.0f - m) + revBuf.getSample (0, i) * m;
                        pR[i] = pR[i] * (1.0f - m) + revBuf.getSample (1, i) * m;
                    }
                }
                else if (revWasOn) reverb.reset();   // al apagar, la cola se corta en seco
                revWasOn = on;
                break;
            }
            default: break;
        }
    }
    if (distOn && post) { distPost.set (mode, gain); distPost.process (chans, 2, n); }

    // --- Mezcla global + salida ---
    for (int i = 0; i < n; ++i)
    {
        const float gm = sMix.getNextValue(), go = sOut.getNextValue();
        const float l = (dL[i] * (1.0f - gm) + pL[i] * gm) * go;
        const float r = (dR[i] * (1.0f - gm) + pR[i] * gm) * go;
        if (outCh == 1) buffer.setSample (0, i, 0.5f * (l + r));
        else { buffer.setSample (0, i, l); buffer.setSample (1, i, r); }
    }
    for (int c = 2; c < outCh; ++c) buffer.clear (c, 0, n);
}

//==============================================================================
int FuzzHeadProcessor::getNumPrograms() { return (int) factoryPresets().size(); }
const juce::String FuzzHeadProcessor::getProgramName (int i)
{
    const auto& f = factoryPresets();
    return juce::isPositiveAndBelow (i, (int) f.size()) ? u8 (f[(size_t) i].name) : juce::String();
}
void FuzzHeadProcessor::setCurrentProgram (int i) { loadFactory (i); }

void FuzzHeadProcessor::loadFactory (int index)
{
    const auto& f = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) f.size())) return;
    const float instrument = p (ids::instrument);   // los presets de fábrica no cambian el instrumento
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            r->setValueNotifyingHost (r->getDefaultValue());
    auto set = [this] (const juce::String& id, float v)
    {
        if (auto* r = apvts.getParameter (id)) r->setValueNotifyingHost (r->convertTo0to1 (v));
    };
    set (ids::instrument, instrument);
    for (auto& kv : f[(size_t) index].values) set (kv.first, kv.second);
    currentProgram = index;
    currentPresetName = u8 (f[(size_t) index].name);
}

juce::File FuzzHeadProcessor::userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("FuzzHead").getChildFile ("User Presets");
}
juce::StringArray FuzzHeadProcessor::getUserPresetNames() const
{
    juce::StringArray names;
    for (auto& f : userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.preset"))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}
bool FuzzHeadProcessor::saveUserPreset (const juce::String& name)
{
    if (name.trim().isEmpty() || name == "Default") return false;
    auto dir = userPresetFolder(); dir.createDirectory();
    auto xml = apvts.copyState().createXml();
    if (xml == nullptr || ! xml->writeTo (dir.getChildFile (juce::File::createLegalFileName (name) + ".preset"))) return false;
    currentPresetName = name;
    return true;
}
bool FuzzHeadProcessor::loadUserPreset (const juce::String& name)
{
    auto file = userPresetFolder().getChildFile (name + ".preset");
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    currentPresetName = name;
    return true;
}
bool FuzzHeadProcessor::deleteUserPreset (const juce::String& name)
{
    return userPresetFolder().getChildFile (name + ".preset").deleteFile();
}

void FuzzHeadProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", currentPresetName, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}
void FuzzHeadProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            currentPresetName = vt.getProperty ("presetName", "Default").toString();
            apvts.replaceState (vt);
        }
}

juce::AudioProcessorEditor* FuzzHeadProcessor::createEditor() { return new FuzzHeadEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FuzzHeadProcessor(); }
