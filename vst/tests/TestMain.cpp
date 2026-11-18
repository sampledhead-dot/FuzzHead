// Pruebas offline del plugin: detector, niveles por motor, cadena completa, estado y captura del editor.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>

static constexpr double SR = 48000.0;
static int failures = 0;
#define CHECK(c, msg) do { if (!(c)) { std::cout << "  FALLO: " << msg << "\n"; ++failures; } } while (0)

static float midi (int m) { return 440.0f * std::pow (2.0f, (float) (m - 69) / 12.0f); }

static std::vector<float> pluck (float f, double dur, float amp)
{
    const int N = (int) (dur * SR), P = (int) std::round (SR / f);
    std::vector<float> out ((size_t) N), buf ((size_t) P);
    uint32_t s = 12345;
    for (auto& v : buf) { s = s * 1103515245u + 12345u; v = ((float) s / 4294967296.0f * 2.0f - 1.0f) * amp; }
    size_t idx = 0;
    for (int i = 0; i < N; ++i)
    {
        const float a = buf[idx], b = buf[(idx + 1) % (size_t) P];
        buf[idx] = 0.996f * 0.5f * (a + b); out[(size_t) i] = a; idx = (idx + 1) % (size_t) P;
    }
    return out;
}

static double trackingScore (bool bass, const std::vector<int>& notes)
{
    fuzz::PitchTracker t; t.prepare (SR, bass);
    int ok = 0, total = 0;
    for (int m : notes)
    {
        auto x = pluck (midi (m), 0.5, 0.5);
        for (int i = 0; i < (int) x.size(); ++i)
        {
            t.push (x[(size_t) i]);
            if (i > (int) (0.06 * SR) && i < (int) (0.35 * SR) && (i % 128) == 0)
            {
                ++total;
                const float p = t.getPitch();
                if (p > 0 && std::abs (1200.0f * std::log2 (p / midi (m))) < 30.0f) ++ok;
            }
        }
    }
    return (double) ok / total;
}

static void setP (FuzzHeadProcessor& p, const juce::String& id, float v)
{
    auto* r = p.apvts.getParameter (id); r->setValueNotifyingHost (r->convertTo0to1 (v));
}

// Procesa x en bloques de 256 y devuelve (pico, rms, nan)
static std::tuple<float, float, int> run (FuzzHeadProcessor& p, const std::vector<float>& x)
{
    p.prepareToPlay (SR, 256);
    juce::AudioBuffer<float> buf (2, 256); juce::MidiBuffer midiBuf;
    float peak = 0; double e = 0; int nan = 0; size_t n = 0;
    for (size_t pos = 0; pos < x.size(); pos += 256)
    {
        const int len = (int) std::min<size_t> (256, x.size() - pos);
        buf.setSize (2, len, false, false, true);
        for (int i = 0; i < len; ++i) { buf.setSample (0, i, x[pos + (size_t) i]); buf.setSample (1, i, x[pos + (size_t) i]); }
        p.processBlock (buf, midiBuf);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < len; ++i)
        {
            const float v = buf.getSample (c, i);
            if (! std::isfinite (v)) ++nan; else { peak = std::max (peak, std::abs (v)); e += v * v; ++n; }
        }
    }
    return { peak, (float) std::sqrt (e / (double) std::max<size_t> (1, n)), nan };
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::cout << "== Detector ==\n";
    const double g = trackingScore (false, { 40, 45, 52, 57, 64, 69, 76, 84 });
    const double b = trackingScore (true, { 23, 28, 33, 38, 43, 48, 55 });
    std::cout << "  Guitarra E2..C6: " << g * 100 << " % dentro de ±30 cents\n";
    std::cout << "  Bajo B0..G3:     " << b * 100 << " % dentro de ±30 cents\n";
    CHECK (g > 0.9, "precisión guitarra"); CHECK (b > 0.85, "precisión bajo");

    // señal: seno de 110 Hz a 0.5 (igual que la prueba de niveles en JS)
    std::vector<float> sine ((size_t) (SR * 2));
    for (size_t i = 0; i < sine.size(); ++i) sine[i] = 0.5f * std::sin (6.2831853f * 110.0f * (float) i / (float) SR);
    std::cout << "== Niveles por motor (seno 110 Hz, RMS seco = 0.354) ==\n";
    const char* names[] = { "Deep", "Vintage", "Solo", "Strings", "Harmony" };
    for (int e = 0; e < 5; ++e)
    {
        FuzzHeadProcessor p; setP (p, ids::engine, (float) e);
        auto [pk, rms, nan] = run (p, sine);
        std::cout << "  " << names[e] << ": pico " << pk << " rms " << rms << " nan " << nan << "\n";
        CHECK (nan == 0, "NaN"); CHECK (rms > 0.12f && rms < 0.8f, "nivel fuera de rango");
    }

    std::cout << "== Cadena completa, todos los órdenes ==\n";
    std::vector<float> riff;
    for (int m : { 52, 55, 57, 59, 64, 62, 60, 57 }) { auto x = pluck (midi (m), 0.6, 0.5); riff.insert (riff.end(), x.begin(), x.end()); }
    for (int order = 0; order < 6; ++order)
    {
        FuzzHeadProcessor p;
        setP (p, ids::engine, 4); setP (p, ids::distOn, 1); setP (p, ids::distMode, (float) (order % 4)); setP (p, ids::distPost, 1);
        setP (p, ids::modOn, 1); setP (p, ids::crushOn, 1); setP (p, ids::crushRate, 8000); setP (p, ids::crushBits, 8);
        setP (p, ids::ringOn, 1); setP (p, ids::revOn, 1); setP (p, ids::revTime, 600); setP (p, ids::order, (float) order);
        auto [pk, rms, nan] = run (p, riff);
        std::cout << "  orden " << order << ": pico " << pk << " rms " << rms << " nan " << nan << "\n";
        CHECK (nan == 0, "NaN cadena"); CHECK (pk < 4.0f && rms > 0.01f, "nivel cadena");
    }
    {   // bajo + sinte apagado
        FuzzHeadProcessor p; setP (p, ids::instrument, 1); setP (p, ids::synthOn, 0); setP (p, ids::distOn, 1);
        auto [pk, rms, nan] = run (p, riff); CHECK (nan == 0, "NaN sin sinte");
        std::cout << "  sinte apagado + dist: rms " << rms << "\n";
    }

    std::cout << "== Estado y presets ==\n";
    {
        FuzzHeadProcessor a; a.loadFactory (9);   // Campanas metálicas
        juce::MemoryBlock mb; a.getStateInformation (mb);
        FuzzHeadProcessor c; c.setStateInformation (mb.getData(), (int) mb.getSize());
        const float ringF = c.apvts.getRawParameterValue (ids::ringFreq)->load();
        const int order = (int) c.apvts.getRawParameterValue (ids::order)->load();
        std::cout << "  preset restaurado: " << c.currentPresetName << " ringFreq " << ringF << " order " << order << "\n";
        CHECK (std::abs (ringF - 830.0f) < 1.0f && order == 3, "restaurar estado");
        CHECK (a.getNumPrograms() == 12, "número de presets");
    }

    std::cout << "== Captura del editor ==\n";
    {
        FuzzHeadProcessor p; p.prepareToPlay (SR, 256); p.loadFactory (5);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
        juce::File out ("/tmp/fuzzhead_editor.png"); out.deleteFile();
        juce::FileOutputStream fos (out); juce::PNGImageFormat png; png.writeImageToStream (img, fos);
        std::cout << "  " << out.getFullPathName() << " " << img.getWidth() << "x" << img.getHeight() << "\n";
    }

    std::cout << "== Aviso de versión nueva ==\n";
    {
        using namespace shupdate;
        CHECK (newer ("1.0.9", "1.0.10") && newer ("1.0.0", "1.1.0") && ! newer ("1.1.0", "1.1.0") && ! newer ("2.0.0", "1.9.9"), "comparar versiones");
        const auto rel = juce::JSON::parse (R"([{"tag_name":"apps-v5.0.0","html_url":"x"},{"tag_name":"vst-v99.0.0","draft":true,"html_url":"borrador"},
            {"tag_name":"vst-v9.9.9","html_url":"https://github.com/r/releases/tag/vst-v9.9.9"},{"tag_name":"vst-v1.0.0","html_url":"vieja"}])");
        const auto l = pickLatest (rel);
        CHECK (l.version == "9.9.9" && l.url.endsWith ("vst-v9.9.9"), "elegir la última release vst-v (sin borradores ni otras)");
        CHECK (pickLatest (juce::JSON::parse ("basura")).version.isEmpty(), "respuesta no válida");
        auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sh-update-test"); tmp.deleteRecursively(); tmp.createDirectory();
        auto fake = tmp.getChildFile ("releases.json"); fake.replaceWithText (juce::JSON::toString (rel));
        setenv ("SH_UPDATE_SETTINGS", tmp.getFullPathName().toRawUTF8(), 1); setenv ("SH_UPDATE_FAKE", fake.getFullPathName().toRawUTF8(), 1);
        auto open = [&] (bool snap) {
            FuzzHeadProcessor p; p.prepareToPlay (SR, 256);
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            auto& up = dynamic_cast<FuzzHeadEditor&> (*ed).update; up.waitForCheck (5000);
            const bool shown = up.hasUpdate();
            if (snap) { auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f); juce::File out ("/tmp/fuzzhead_editor_update.png"); out.deleteFile(); juce::FileOutputStream fos (out); juce::PNGImageFormat png; png.writeImageToStream (img, fos); }
            if (shown && ! snap) up.ignoreThisVersion();
            return std::make_pair (shown, up.latestVersion());
        };
        auto a = open (true);  CHECK (a.first && a.second == "9.9.9", "con versión nueva aparece el aviso");
        std::cout << "  aviso: " << (a.first ? "sí" : "no") << " (v" << a.second << ")\n";
        auto b = open (false); auto c = open (false); CHECK (! c.first, "tras pulsar ✕ no vuelve a avisar de esa versión");
        fake.replaceWithText (R"([{"tag_name":"vst-v)" + juce::String (FuzzHeadProcessor::kVersion) + R"(","html_url":"x"}])"); tmp.getChildFile ("FuzzHead.settings").deleteFile();
        auto d = open (false); CHECK (! d.first, "con la misma versión no hay aviso");
        unsetenv ("SH_UPDATE_FAKE"); unsetenv ("SH_UPDATE_SETTINGS"); tmp.deleteRecursively();
        juce::ignoreUnused (b);
    }
    std::cout << (failures ? "FALLOS: " + std::to_string (failures) : std::string ("TODO OK")) << "\n";
    return failures ? 1 : 0;
}
