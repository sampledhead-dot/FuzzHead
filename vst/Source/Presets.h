#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <utility>

// Presets de fábrica: mismos valores que la app web (FACTORY en index.html).
// engine: 0 Deep, 1 Vintage, 2 Solo, 3 Strings, 4 Harmony · distMode: 0 Soft, 1 Hard, 2 Rich, 3 Wavefold
// order: índice en kOrders (0 = Bitcrush, Ring, Reverb · 3 = Ring, Reverb, Bitcrush)
inline juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }

struct FactoryPreset { const char* name; std::vector<std::pair<const char*, float>> values; };

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> list {
        { "Default", {} },
        { "Bajo de sub", { {"engine",0}, {"synthMix",0.85f}, {"distOn",1}, {"distMode",2}, {"distGain",0.25f} } },
        { "Lead de los 80", { {"engine",2}, {"modOn",1}, {"modDepth",0.35f}, {"modMix",0.45f}, {"revOn",1}, {"revTime",420}, {"revMix",0.3f} } },
        { "Vintage cálido", { {"engine",1}, {"synthMix",0.8f}, {"modOn",1}, {"modDepth",0.2f}, {"modMix",0.35f} } },
        { "Cuerdas nocturnas", { {"engine",3}, {"modOn",1}, {"modDepth",0.5f}, {"modMix",0.6f}, {"revOn",1}, {"revTime",800}, {"revMix",0.4f} } },
        { "Quintas y octava", { {"engine",4}, {"int1",7}, {"int2",12}, {"int2On",1}, {"revOn",1}, {"revTime",250}, {"revMix",0.25f} } },
        { "Terceras menores", { {"engine",4}, {"int1",3}, {"int2",-12}, {"int2On",1}, {"modOn",1}, {"modDepth",0.25f}, {"modMix",0.4f} } },
        { "Fuzz plegado", { {"engine",2}, {"distOn",1}, {"distMode",3}, {"distGain",0.55f}, {"distPre",0}, {"distPost",1} } },
        { "Lo-fi cassette", { {"engine",1}, {"crushOn",1}, {"crushRate",9000}, {"crushBits",9}, {"distOn",1}, {"distMode",0}, {"distGain",0.3f} } },
        { "Campanas metálicas", { {"engine",1}, {"ringOn",1}, {"ringFreq",830}, {"ringMix",0.6f}, {"revOn",1}, {"revTime",350}, {"revMix",0.3f}, {"order",3} } },
        { "Explosión gated", { {"engine",0}, {"distOn",1}, {"distMode",1}, {"distGain",0.4f}, {"revOn",1}, {"revTime",550}, {"revMix",0.55f} } },
        { "Guitarra sola + FX", { {"synthOn",0}, {"distOn",1}, {"distMode",2}, {"distGain",0.45f}, {"revOn",1}, {"revTime",300}, {"revMix",0.3f} } },
    };
    return list;
}
