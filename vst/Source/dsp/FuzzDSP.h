// FuzzHead — DSP en tiempo real (C++ / JUCE)
// Mismo diseño que web/dsp.js. Todo se reserva en prepare(); nada asigna memoria en el hilo de audio.
#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <algorithm>

namespace fuzz
{
constexpr float kTwoPi = 6.283185307179586f;

enum class Engine { Deep = 0, Vintage, Solo, Strings, Harmony };
enum class DistMode { Soft = 0, Hard, Rich, Wavefold };

struct Range { float minF, maxF; };
inline Range rangeFor (bool bass) { return bass ? Range { 28.0f, 700.0f } : Range { 70.0f, 1400.0f }; }

//==============================================================================
// Detector de tono YIN causal; la correlación se hace por FFT.
class PitchTracker
{
public:
    void prepare (double sampleRate, bool bassMode)
    {
        sr = (float) sampleRate;
        bass = bassMode;
        const auto r = rangeFor (bass);
        hop = std::max (64, (int) std::pow (2.0, std::round (std::log2 (sr / 350.0))));
        W = 1; while ((float) W < sr / r.minF * 1.15f) W <<= 1;
        tauMin = std::max (2, (int) std::floor (sr / r.maxF));
        tauMax = std::min (W - 1, (int) std::ceil (sr / r.minF));
        const int n = 4 * W;
        int order = 0; while ((1 << order) < n) ++order;
        fft = std::make_unique<juce::dsp::FFT> (order);
        fa.assign ((size_t) (2 * n), 0.0f);
        fb.assign ((size_t) (2 * n), 0.0f);
        hist.assign ((size_t) (2 * W), 0.0f);
        P.assign ((size_t) (2 * W + 1), 0.0);
        cm.assign ((size_t) W, 1.0f);
        writePos = 0; counter = 0; pitch = 0.0f;
        gateLin = juce::Decibels::decibelsToGain (-52.0f);
    }
    bool isBass() const { return bass; }

    // Devuelve true si se acaba de calcular un nuevo frame
    bool push (float x)
    {
        hist[(size_t) writePos] = x;
        writePos = (writePos + 1) % (2 * W);
        if (++counter < hop) return false;
        counter = 0;
        analyse();
        return true;
    }
    float getPitch() const { return pitch; }   // Hz, 0 = sin voz

private:
    void analyse()
    {
        const int n = 4 * W;
        std::fill (fa.begin(), fa.end(), 0.0f);
        std::fill (fb.begin(), fb.end(), 0.0f);
        P[0] = 0.0;
        double eRecent = 0.0;
        for (int i = 0; i < 2 * W; ++i)
        {
            const float v = hist[(size_t) ((writePos + i) % (2 * W))]; // más antiguo -> más reciente
            fb[(size_t) i] = v;
            if (i < W) fa[(size_t) i] = v;
            P[(size_t) i + 1] = P[(size_t) i] + (double) v * v;
            if (i >= W) eRecent += (double) v * v;
        }
        if (std::sqrt (eRecent / W) < gateLin) { pitch = 0.0f; return; }

        fft->performRealOnlyForwardTransform (fa.data());
        fft->performRealOnlyForwardTransform (fb.data());
        for (size_t k = 0; k < (size_t) n; ++k)   // conj(A) * B
        {
            const float ar = fa[2 * k], ai = fa[2 * k + 1];
            const float br = fb[2 * k], bi = fb[2 * k + 1];
            fb[2 * k]     = ar * br + ai * bi;
            fb[2 * k + 1] = ar * bi - ai * br;
        }
        fft->performRealOnlyInverseTransform (fb.data());

        const double e0 = P[(size_t) W];
        // r(0) debe valer e0: así obtenemos la escala de la FFT inversa sea cual sea el backend
        const double scale = std::abs (fb[0]) > 1e-20 ? e0 / (double) fb[0] : 1.0;
        double run = 0.0;
        cm[0] = 1.0f;
        for (int t = 1; t <= tauMax; ++t)
        {
            const double et = P[(size_t) (t + W)] - P[(size_t) t];
            const double d = std::max (0.0, e0 + et - 2.0 * fb[(size_t) t] * scale);
            run += d;
            cm[(size_t) t] = run > 0.0 ? (float) (d * t / run) : 1.0f;
        }
        int tau = -1;
        for (int t = tauMin; t <= tauMax; ++t)
        {
            if (cm[(size_t) t] < 0.15f)
            {
                while (t + 1 <= tauMax && cm[(size_t) t + 1] < cm[(size_t) t]) ++t;
                tau = t; break;
            }
        }
        if (tau < 0)
        {
            tau = tauMin;
            for (int t = tauMin; t <= tauMax; ++t) if (cm[(size_t) t] < cm[(size_t) tau]) tau = t;
        }
        if (cm[(size_t) tau] > 0.35f) { pitch = 0.0f; return; }
        float tf = (float) tau;
        if (tau > 1 && tau < tauMax)
        {
            const float s0 = cm[(size_t) tau - 1], s1 = cm[(size_t) tau], s2 = cm[(size_t) tau + 1];
            const float den = s0 - 2.0f * s1 + s2;
            if (std::abs (den) > 1e-12f) tf = (float) tau + 0.5f * (s0 - s2) / den;
        }
        pitch = sr / tf;
    }

    float sr = 44100.0f, gateLin = 0.0f, pitch = 0.0f;
    bool bass = false;
    int hop = 128, W = 1024, tauMin = 2, tauMax = 600, writePos = 0, counter = 0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> fa, fb, hist, cm;
    std::vector<double> P;
};

//==============================================================================
inline float blep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
inline float sawW (float p, float dt) { return 2.0f * p - 1.0f - blep (p, dt); }
inline float pulseW (float p, float dt, float pw)
{
    float v = p < pw ? 1.0f : -1.0f;
    v += blep (p, dt);
    float q = p - pw; if (q < 0.0f) q += 1.0f;
    return v - blep (q, dt);
}
inline void adv (float& p, float dt) { p += dt; if (p >= 1.0f) p -= 1.0f; }

struct SVF
{
    float ic1 = 0, ic2 = 0, lastFc = -1, g = 0;
    float lp (float v0, float fc, float q, float sr)
    {
        if (std::abs (fc - lastFc) > fc * 0.002f)
        {
            lastFc = fc;
            g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.0f, sr * 0.45f, fc) / sr);
        }
        const float k = 1.0f / q;
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        return v2;
    }
    void reset() { ic1 = ic2 = 0; lastFc = -1; }
};

struct Voice
{
    float ratio = 1.0f, p1 = 0, p2 = 0, p3 = 0, p4 = 0;
    SVF f1;
    void randomise (juce::Random& r) { p1 = r.nextFloat(); p2 = r.nextFloat(); p3 = r.nextFloat(); p4 = r.nextFloat(); }

    float tick (Engine e, float freq, float ampN, float sr)
    {
        const float f = freq * ratio, dt = f / sr;
        switch (e)
        {
            case Engine::Deep:
            {
                adv (p1, dt); adv (p2, dt * 0.5f);
                float v = 0.65f * std::sin (kTwoPi * p1) + 0.55f * std::sin (kTwoPi * p2) + 0.2f * sawW (p1, dt);
                v = f1.lp (v, std::min (f * 3.0f + 150.0f, 1500.0f), 0.7f, sr);
                return std::tanh (1.6f * v) / std::tanh (1.6f);
            }
            case Engine::Solo:
            {
                adv (p1, dt); const float dt3 = dt * 1.003f; adv (p3, dt3);
                float v = 0.6f * sawW (p1, dt) + 0.5f * pulseW (p3, dt3, 0.3f);
                v = f1.lp (v, juce::jlimit (500.0f, 10000.0f, f * 8.0f * (0.6f + 0.8f * ampN)), 2.2f, sr);
                return std::tanh (2.0f * v) * 0.6f;
            }
            case Engine::Strings:
            {
                const float d1 = dt * 0.9954f, d3 = dt * 1.0046f, d4 = dt * 2.0f;
                adv (p1, d1); adv (p2, dt); adv (p3, d3); adv (p4, d4);
                float v = sawW (p1, d1) + sawW (p2, dt) + sawW (p3, d3) + 0.25f * sawW (p4, d4);
                v = f1.lp (v, juce::jlimit (400.0f, 6000.0f, f * 5.0f), 0.6f, sr);
                return v * 0.4f;
            }
            case Engine::Vintage:
            case Engine::Harmony:
            default:
            {
                adv (p1, dt);
                float v = 0.55f * sawW (p1, dt) + 0.45f * pulseW (p1, dt, 0.5f);
                const float mult = e == Engine::Harmony ? 6.0f : 4.5f;
                v = f1.lp (v, juce::jlimit (250.0f, 5000.0f, f * mult), 0.8f, sr);
                return v * 0.9f;
            }
        }
    }
};

//==============================================================================
struct SynthParams
{
    Engine engine = Engine::Vintage;
    int int1 = 7, int2 = 12; bool int2On = true;
    bool modOn = false; float modDepth = 0.4f, modMix = 0.5f;
};

class GuitarSynth
{
public:
    void prepare (double sampleRate, bool bassMode)
    {
        sr = (float) sampleRate;
        tracker.prepare (sampleRate, bassMode);
        envA = 1.0f - std::exp (-1.0f / (0.002f * sr));
        envR = 1.0f - std::exp (-1.0f / (0.08f * sr));
        glide = 1.0f - std::exp (-1.0f / (0.004f * sr));
        holdSamples = (int) std::round (0.08f * sr);
        juce::Random r (1234);
        for (auto* bank : { &mainV, &flatV, &sharpV })
            for (auto& v : *bank) { v.randomise (r); v.f1.reset(); }
        env = amp = logF = 0.0f; haveF = false; sinceVoiced = 1 << 30; histN = 0;
    }
    bool isBass() const { return tracker.isBass(); }

    void setParams (const SynthParams& p)
    {
        sp = p;
        static const float att[] = { 0.005f, 0.006f, 0.003f, 0.12f, 0.006f };
        static const float rel[] = { 0.12f, 0.15f, 0.10f, 0.45f, 0.18f };
        static const float gain[] = { 0.74f, 2.5f, 1.23f, 1.4f, 2.5f };
        const int e = (int) p.engine;
        smA = 1.0f - std::exp (-1.0f / (att[e] * sr));
        smR = 1.0f - std::exp (-1.0f / (rel[e] * sr));
        nVoices = 1;
        mainV[0].ratio = 1.0f;
        if (p.engine == Engine::Harmony)
        {
            mainV[1].ratio = std::pow (2.0f, (float) p.int1 / 12.0f); nVoices = 2;
            if (p.int2On) { mainV[2].ratio = std::pow (2.0f, (float) p.int2 / 12.0f); nVoices = 3; }
        }
        voiceNorm = gain[e] / std::sqrt ((float) nVoices);
        const float cents = p.modDepth * 30.0f;
        modActive = p.modOn && p.modDepth > 0.0f && p.modMix > 0.0f;
        for (int i = 0; i < nVoices; ++i)
        {
            flatV[(size_t) i].ratio  = mainV[(size_t) i].ratio * std::pow (2.0f, -cents / 1200.0f);
            sharpV[(size_t) i].ratio = mainV[(size_t) i].ratio * std::pow (2.0f,  cents / 1200.0f);
        }
        modNorm = modActive ? 1.0f / (1.0f + 0.6f * p.modMix) : 1.0f;
    }

    // x: señal mono de entrada; escribe L/R del sinte
    void process (float x, float& outL, float& outR)
    {
        if (tracker.push (x)) onFrame (tracker.getPitch());
        ++sinceVoiced;
        if (targetLog > 0.0f && freshFrame)
        {
            if (! haveF || sinceVoicedAtFrame > holdSamples) logF = targetLog;
            haveF = true;
        }
        freshFrame = false;
        if (sinceVoiced <= holdSamples && haveF) logF += (targetLog - logF) * glide;

        const float a = std::abs (x);
        env += (a - env) * (a > env ? envA : envR);
        const bool open = haveF && sinceVoiced <= holdSamples;
        const float target = open ? env * 1.5f : 0.0f;
        amp += (target - amp) * (target > amp ? smA : smR);
        if (amp < 1e-6f && ! open) { outL = outR = 0.0f; return; }

        const float freq = std::exp2 (logF), ampN = std::min (1.0f, amp * 3.0f);
        float m = 0.0f;
        for (int v = 0; v < nVoices; ++v) m += mainV[(size_t) v].tick (sp.engine, freq, ampN, sr);
        m *= voiceNorm * amp;
        if (modActive)
        {
            float fl = 0.0f, sh = 0.0f;
            for (int v = 0; v < nVoices; ++v)
            {
                fl += flatV[(size_t) v].tick (sp.engine, freq, ampN, sr);
                sh += sharpV[(size_t) v].tick (sp.engine, freq, ampN, sr);
            }
            const float k = voiceNorm * amp * sp.modMix;
            fl *= k; sh *= k;
            outL = (m + 0.9f * fl + 0.1f * sh) * modNorm;
            outR = (m + 0.1f * fl + 0.9f * sh) * modNorm;
        }
        else outL = outR = m;
    }

private:
    void onFrame (float hz)
    {
        if (hz > 0.0f)
        {
            // mediana de los últimos 3 frames con voz
            hist[(size_t) (histN % 3)] = std::log2 (hz); ++histN;
            const int c = std::min (histN, 3);
            std::array<float, 3> s { hist[0], hist[1], hist[2] };
            std::sort (s.begin(), s.begin() + c);
            targetLog = s[(size_t) ((c - 1) >> 1)];
            sinceVoicedAtFrame = sinceVoiced;
            sinceVoiced = 0;
            freshFrame = true;
            lastFrameVoiced = true;
        }
        else
        {
            if (! lastFrameVoiced) histN = 0;
            lastFrameVoiced = false;
        }
    }

    PitchTracker tracker;
    SynthParams sp;
    std::array<Voice, 3> mainV, flatV, sharpV;
    std::array<float, 3> hist {};
    int histN = 0, nVoices = 1, holdSamples = 3528, sinceVoiced = 1 << 30, sinceVoicedAtFrame = 1 << 30;
    bool haveF = false, modActive = false, freshFrame = false, lastFrameVoiced = false;
    float sr = 44100.0f, env = 0, amp = 0, logF = 0, targetLog = 0;
    float envA = 0, envR = 0, smA = 0, smR = 0, glide = 0, voiceNorm = 1, modNorm = 1;
};

//==============================================================================
inline float distShape (DistMode m, float u)
{
    switch (m)
    {
        case DistMode::Hard:     return juce::jlimit (-1.0f, 1.0f, 1.2f * u);
        case DistMode::Rich:     return std::tanh (u + 0.35f * u * u / (1.0f + u * u));
        case DistMode::Wavefold: return std::sin (juce::MathConstants<float>::halfPi * u);
        case DistMode::Soft:
        default:                 return std::tanh (u);
    }
}

class Distortion
{
public:
    void prepare (double sampleRate) { sr = (float) sampleRate; reset(); }
    void reset() { for (auto& s : st) s = {}; }
    void set (DistMode m, float gain)
    {
        mode = m;
        drive = m == DistMode::Wavefold ? 1.0f + gain * 12.0f : juce::Decibels::decibelsToGain (gain * 40.0f);
        comp = m == DistMode::Wavefold ? 0.7f : 0.3f / std::max (1e-3f, std::abs (distShape (m, 0.3f * drive)));
        const float lpHz = m == DistMode::Soft ? 7000.0f : (m == DistMode::Rich ? 5000.0f : 0.0f);
        useLp = lpHz > 0.0f;
        lpC = useLp ? 1.0f - std::exp (-kTwoPi * lpHz / sr) : 1.0f;
        dcR = 1.0f - kTwoPi * 20.0f / sr;
    }
    void process (float* const* ch, int numCh, int n)
    {
        for (int c = 0; c < numCh; ++c)
        {
            auto& s = st[(size_t) c];
            for (int i = 0; i < n; ++i)
            {
                float y = distShape (mode, ch[c][i] * drive) * comp;
                if (useLp) { s.lp += (y - s.lp) * lpC; y = s.lp; }
                const float o = y - s.x1 + dcR * s.y1; s.x1 = y; s.y1 = o;
                ch[c][i] = o;
            }
        }
    }
private:
    struct State { float lp = 0, x1 = 0, y1 = 0; };
    std::array<State, 2> st;
    DistMode mode = DistMode::Soft;
    float sr = 44100, drive = 1, comp = 1, lpC = 1, dcR = 0.999f;
    bool useLp = false;
};

class Bitcrusher
{
public:
    void prepare (double sampleRate) { sr = (float) sampleRate; ph = 1.0f; held = { 0, 0 }; }
    void process (float* const* ch, int numCh, int n, float rateHz, float bitsF)
    {
        const float step = std::min (rateHz, 44100.0f) / sr;
        const int bits = (int) std::round (bitsF);
        const float q = std::pow (2.0f, (float) (bits - 1));
        for (int i = 0; i < n; ++i)
        {
            bool take = step >= 1.0f;
            if (! take) { ph += step; if (ph >= 1.0f) { ph -= 1.0f; take = true; } }
            for (int c = 0; c < numCh; ++c)
            {
                if (take) held[(size_t) c] = ch[c][i];
                ch[c][i] = bits >= 24 ? held[(size_t) c] : std::round (held[(size_t) c] * q) / q;
            }
        }
    }
private:
    float sr = 44100, ph = 1;
    std::array<float, 2> held {};
};

class RingMod
{
public:
    void prepare (double sampleRate) { sr = (float) sampleRate; ph = 0; }
    void process (float* const* ch, int numCh, int n, float freq, float mix)
    {
        const float inc = freq / sr;
        for (int i = 0; i < n; ++i)
        {
            const float c = std::sin (kTwoPi * ph);
            ph += inc; if (ph >= 1.0f) ph -= 1.0f;
            for (int k = 0; k < numCh; ++k) ch[k][i] = ch[k][i] * (1.0f - mix) + ch[k][i] * c * mix;
        }
    }
private:
    float sr = 44100, ph = 0;
};

// IR determinista de la reverb con puerta (igual que makeGatedIR en JS)
inline void makeGatedIR (float* ir, int L, float sr, uint32_t seed)
{
    uint32_t s = seed;
    auto rnd = [&s]() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) s / 4294967296.0f * 2.0f - 1.0f; };
    const int rise = std::max (1, (int) std::round (std::min (0.004f * sr, L * 0.1f)));
    const int fall = std::max (1, (int) std::round (std::min (0.006f * sr, L * 0.15f)));
    const float ph = (float) (seed % 7) * 0.9f;
    float hpX = 0, hpY = 0, lp = 0;
    const float hpR = 1.0f - kTwoPi * 200.0f / sr, lpC = 1.0f - std::exp (-kTwoPi * 6500.0f / sr);
    double E = 0;
    for (int i = 0; i < L; ++i)
    {
        float e = 1.0f - 0.4f * ((float) i / L);
        if (i < rise) e *= (float) i / rise;
        if (i > L - fall) e *= (float) (L - i) / fall;
        e *= 1.0f + 0.15f * std::sin (kTwoPi * 7.0f * i / sr + ph);
        float v = rnd() * e;
        lp += (v - lp) * lpC; v = lp;
        const float o = v - hpX + hpR * hpY; hpX = v; hpY = o;
        ir[i] = o; E += (double) o * o;
    }
    const float k = 0.6f / (float) std::sqrt (E > 0 ? E : 1.0);
    for (int i = 0; i < L; ++i) ir[i] *= k;
}
inline int gatedIRLength (float sr, float timeMs) { return std::max (16, (int) std::round ((0.025f + timeMs / 1000.0f) * sr)); }

} // namespace fuzz
