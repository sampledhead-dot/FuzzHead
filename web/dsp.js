/* FuzzHead — motor DSP (JavaScript)
 * Mismo diseño que el VST (vst/Source/dsp). Procesa un archivo completo (offline).
 * Funciona como Web Worker o como módulo de Node (para pruebas).
 */
'use strict';
var FuzzDSP = (function () {

  const ENGINES = ['deep', 'vintage', 'solo', 'strings', 'harmony'];
  const DIST_MODES = ['soft', 'hard', 'rich', 'wavefold'];

  const DEFAULTS = {
    instrument: 'guitar',      // 'guitar' | 'bass'
    inputDb: 0, mix: 1, outputDb: 0,
    synthOn: true, synthMix: 1, engine: 'vintage',
    int1: 7, int2: 12, int2On: true,
    distOn: false, distGain: 0.35, distMode: 'soft', distPre: true, distPost: false,
    modOn: false, modDepth: 0.4, modMix: 0.5,
    crushOn: false, crushRate: 44100, crushBits: 24,
    ringOn: false, ringFreq: 440, ringMix: 0.5,
    revOn: false, revTime: 300, revMix: 0.35,
    order: ['crush', 'ring', 'rev']
  };

  // Rango de seguimiento por instrumento
  const RANGES = {
    guitar: { minF: 70, maxF: 1400 },  // ~C#2 .. F6 (fiable E2–C6)
    bass:   { minF: 28, maxF: 700 }    // ~A0 .. F5 (fiable B0–G4)
  };

  // Ataque / release (s) de la envolvente del sintetizador por motor
  const ENGINE_ENV = {
    deep:    [0.005, 0.12],
    vintage: [0.006, 0.15],
    solo:    [0.003, 0.10],
    strings: [0.12, 0.45],
    harmony: [0.006, 0.18]
  };
  // Ganancia por motor para igualar el volumen RMS de la guitarra seca
  const ENGINE_GAIN = { deep: 0.74, vintage: 2.5, solo: 1.23, strings: 1.4, harmony: 2.5 };

  const db2lin = d => Math.pow(10, d / 20);
  const clamp = (x, a, b) => x < a ? a : (x > b ? b : x);
  const TWO_PI = Math.PI * 2;

  // ---------- FFT radix-2 ----------
  function makeFFT(n) {
    const levels = Math.log2(n) | 0;
    const rev = new Uint32Array(n);
    for (let i = 0; i < n; i++) {
      let r = 0, x = i;
      for (let j = 0; j < levels; j++) { r = (r << 1) | (x & 1); x >>= 1; }
      rev[i] = r;
    }
    const cos = new Float64Array(n / 2), sin = new Float64Array(n / 2);
    for (let i = 0; i < n / 2; i++) { cos[i] = Math.cos(TWO_PI * i / n); sin[i] = Math.sin(TWO_PI * i / n); }
    function transform(re, im, inverse) {
      for (let i = 0; i < n; i++) {
        const j = rev[i];
        if (j > i) { let t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
      }
      for (let size = 2; size <= n; size <<= 1) {
        const half = size >> 1, step = n / size;
        for (let i = 0; i < n; i += size) {
          for (let j = i, k = 0; j < i + half; j++, k += step) {
            const c = cos[k], s = inverse ? sin[k] : -sin[k];
            const l = j + half;
            const tr = re[l] * c - im[l] * s;
            const ti = re[l] * s + im[l] * c;
            re[l] = re[j] - tr; im[l] = im[j] - ti;
            re[j] += tr; im[j] += ti;
          }
        }
      }
      if (inverse) for (let i = 0; i < n; i++) { re[i] /= n; im[i] /= n; }
    }
    return { n, transform };
  }

  // ---------- Detector de tono (YIN con correlación por FFT) ----------
  function trackPitch(x, sr, instrument, progress) {
    const rng = RANGES[instrument] || RANGES.guitar;
    const N = x.length;
    const hop = Math.max(64, Math.pow(2, Math.round(Math.log2(sr / 350))));
    let W = 1; while (W < sr / rng.minF * 1.15) W <<= 1;
    const tauMin = Math.max(2, Math.floor(sr / rng.maxF));
    const tauMax = Math.min(W - 1, Math.ceil(sr / rng.minF));
    const fft = makeFFT(4 * W);
    const aRe = new Float64Array(4 * W), aIm = new Float64Array(4 * W);
    const bRe = new Float64Array(4 * W), bIm = new Float64Array(4 * W);
    const P = new Float64Array(2 * W + 1);
    const d = new Float64Array(W), cm = new Float64Array(W);
    const frames = Math.ceil(N / hop);
    const pitch = new Float32Array(frames);   // Hz o 0
    const conf = new Float32Array(frames);
    const gateLin = db2lin(-52);

    for (let f = 0; f < frames; f++) {
      const end = f * hop + hop;            // ventana causal que termina en end
      const start = end - 2 * W;
      // energía de la mitad más reciente para la puerta
      let e = 0;
      for (let i = end - W; i < end; i++) { const v = (i >= 0 && i < N) ? x[i] : 0; e += v * v; }
      const rms = Math.sqrt(e / W);
      if (rms < gateLin) { pitch[f] = 0; conf[f] = 0; continue; }
      aRe.fill(0); aIm.fill(0); bRe.fill(0); bIm.fill(0);
      P[0] = 0;
      for (let i = 0; i < 2 * W; i++) {
        const idx = start + i;
        const v = (idx >= 0 && idx < N) ? x[idx] : 0;
        bRe[i] = v; if (i < W) aRe[i] = v;
        P[i + 1] = P[i] + v * v;
      }
      fft.transform(aRe, aIm, false);
      fft.transform(bRe, bIm, false);
      for (let i = 0; i < 4 * W; i++) {   // conj(A) * B
        const r = aRe[i] * bRe[i] + aIm[i] * bIm[i];
        const im = aRe[i] * bIm[i] - aIm[i] * bRe[i];
        bRe[i] = r; bIm[i] = im;
      }
      fft.transform(bRe, bIm, true);
      const e0 = P[W];
      d[0] = 0; cm[0] = 1;
      let run = 0;
      for (let t = 1; t <= tauMax; t++) {
        const et = P[t + W] - P[t];
        d[t] = Math.max(0, e0 + et - 2 * bRe[t]);
        run += d[t];
        cm[t] = run > 0 ? d[t] * t / run : 1;
      }
      let tau = -1;
      for (let t = tauMin; t <= tauMax; t++) {
        if (cm[t] < 0.15) {
          while (t + 1 <= tauMax && cm[t + 1] < cm[t]) t++;
          tau = t; break;
        }
      }
      if (tau < 0) {
        let best = tauMin;
        for (let t = tauMin; t <= tauMax; t++) if (cm[t] < cm[best]) best = t;
        tau = best;
      }
      const dm = cm[tau];
      if (dm > 0.35) { pitch[f] = 0; conf[f] = 0; continue; }
      let tf = tau;
      if (tau > 1 && tau < tauMax) {
        const s0 = cm[tau - 1], s1 = cm[tau], s2 = cm[tau + 1];
        const den = s0 - 2 * s1 + s2;
        if (Math.abs(den) > 1e-12) tf = tau + 0.5 * (s0 - s2) / den;
      }
      pitch[f] = sr / tf;
      conf[f] = 1 - dm;
      if (progress && (f & 255) === 0) progress(f / frames);
    }
    return { pitch, conf, hop };
  }

  // ---------- Osciladores ----------
  function blep(t, dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1; }
    if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
    return 0;
  }
  function saw(p, dt) { return 2 * p - 1 - blep(p, dt); }
  function pulse(p, dt, pw) {
    let v = p < pw ? 1 : -1;
    v += blep(p, dt);
    let q = p - pw; if (q < 0) q += 1;
    v -= blep(q, dt);
    return v;
  }

  // Filtro de estado variable (TPT / Simper), salida paso bajo
  function SVF() { this.ic1 = 0; this.ic2 = 0; this.lastFc = -1; this.g = 0; }
  SVF.prototype.lp = function (v0, fc, q, sr) {
    // recalcular tan() solo si el corte cambia más de un 0,2 %
    if (Math.abs(fc - this.lastFc) > fc * 0.002) { this.lastFc = fc; this.g = Math.tan(Math.PI * clamp(fc, 20, sr * 0.45) / sr); }
    const g = this.g;
    const k = 1 / q;
    const a1 = 1 / (1 + g * (g + k)), a2 = g * a1, a3 = g * a2;
    const v3 = v0 - this.ic2;
    const v1 = a1 * this.ic1 + a2 * v3;
    const v2 = this.ic2 + a2 * this.ic1 + a3 * v3;
    this.ic1 = 2 * v1 - this.ic1; this.ic2 = 2 * v2 - this.ic2;
    return v2;
  };

  // Una voz de sintetizador para un motor. ratio = transposición (intervalo/detune)
  function Voice(engine, sr, ratio) {
    this.engine = engine; this.sr = sr; this.ratio = ratio;
    this.p1 = Math.random(); this.p2 = Math.random(); this.p3 = Math.random();
    this.p4 = Math.random(); this.p5 = Math.random();
    this.f1 = new SVF(); this.f2 = new SVF();
  }
  Voice.prototype.tick = function (freq, ampN) {
    const sr = this.sr, f = freq * this.ratio;
    const dt = f / sr;
    let s;
    switch (this.engine) {
      case 'deep': {
        this.p1 += dt; if (this.p1 >= 1) this.p1 -= 1;
        this.p2 += dt * 0.5; if (this.p2 >= 1) this.p2 -= 1;
        let v = 0.65 * Math.sin(TWO_PI * this.p1) + 0.55 * Math.sin(TWO_PI * this.p2) + 0.2 * saw(this.p1, dt);
        v = this.f1.lp(v, Math.min(f * 3 + 150, 1500), 0.7, sr);
        s = Math.tanh(1.6 * v) / Math.tanh(1.6);
        break;
      }
      case 'solo': {
        this.p1 += dt; if (this.p1 >= 1) this.p1 -= 1;
        const dt3 = dt * 1.003;
        this.p3 += dt3; if (this.p3 >= 1) this.p3 -= 1;
        let v = 0.6 * saw(this.p1, dt) + 0.5 * pulse(this.p3, dt3, 0.3);
        const fc = clamp(f * 8 * (0.6 + 0.8 * ampN), 500, 10000);
        v = this.f1.lp(v, fc, 2.2, sr);
        s = Math.tanh(2 * v) * 0.6;
        break;
      }
      case 'strings': {
        const d1 = dt * 0.9954, d3 = dt * 1.0046, d4 = dt * 2;
        this.p1 += d1; if (this.p1 >= 1) this.p1 -= 1;
        this.p2 += dt; if (this.p2 >= 1) this.p2 -= 1;
        this.p3 += d3; if (this.p3 >= 1) this.p3 -= 1;
        this.p4 += d4; if (this.p4 >= 1) this.p4 -= 1;
        let v = saw(this.p1, d1) + saw(this.p2, dt) + saw(this.p3, d3) + 0.25 * saw(this.p4, d4);
        v = this.f1.lp(v, clamp(f * 5, 400, 6000), 0.6, sr);
        s = v * 0.4;
        break;
      }
      case 'harmony':
      case 'vintage':
      default: {
        this.p1 += dt; if (this.p1 >= 1) this.p1 -= 1;
        let v = 0.55 * saw(this.p1, dt) + 0.45 * pulse(this.p1, dt, 0.5);
        const mult = this.engine === 'harmony' ? 6 : 4.5;
        v = this.f1.lp(v, clamp(f * mult, 250, 5000), 0.8, sr);
        s = v * 0.9;
      }
    }
    return s;
  };

  // ---------- Sintetizador completo (seguimiento -> voces) ----------
  function renderSynth(x, sr, p, track) {
    const N = x.length;
    const outL = new Float32Array(N), outR = new Float32Array(N);
    const { pitch, hop } = track;
    const engine = p.engine;
    const [att, rel] = ENGINE_ENV[engine] || ENGINE_ENV.vintage;
    const envA = 1 - Math.exp(-1 / (0.002 * sr)), envR = 1 - Math.exp(-1 / (0.08 * sr));
    const smA = 1 - Math.exp(-1 / (att * sr)), smR = 1 - Math.exp(-1 / (rel * sr));
    const glide = 1 - Math.exp(-1 / (0.004 * sr));
    const holdSamples = Math.round(0.08 * sr);

    // Mediana de 3 frames con voz (anti saltos de octava puntuales)
    const smoothP = new Float32Array(pitch.length);
    const hist = [];
    for (let i = 0; i < pitch.length; i++) {
      if (pitch[i] > 0) {
        hist.push(Math.log2(pitch[i])); if (hist.length > 3) hist.shift();
        const s = hist.slice().sort((a, b) => a - b);
        smoothP[i] = Math.pow(2, s[(s.length - 1) >> 1]);
      } else { smoothP[i] = 0; if (i > 0 && pitch[i - 1] === 0) hist.length = 0; }
    }

    // Voces: principal (+ intervalos en Harmony), y detune si Modulación
    const ratios = [1];
    if (engine === 'harmony') {
      ratios.push(Math.pow(2, p.int1 / 12));
      if (p.int2On) ratios.push(Math.pow(2, p.int2 / 12));
    }
    const voiceNorm = (ENGINE_GAIN[engine] || 1) / Math.sqrt(ratios.length);
    const main = ratios.map(r => new Voice(engine, sr, r));
    const modActive = p.modOn && p.modDepth > 0 && p.modMix > 0;
    const cents = p.modDepth * 30;
    const flat = modActive ? ratios.map(r => new Voice(engine, sr, r * Math.pow(2, -cents / 1200))) : null;
    const sharp = modActive ? ratios.map(r => new Voice(engine, sr, r * Math.pow(2, cents / 1200))) : null;
    const modNorm = modActive ? 1 / (1 + 0.6 * p.modMix) : 1;

    let env = 0, amp = 0, logF = 0, haveF = false, lastVoiced = -1e9;
    for (let n = 0; n < N; n++) {
      const fr = Math.min(pitch.length - 1, (n / hop) | 0);
      const tp = smoothP[fr];
      if (tp > 0) {
        const lt = Math.log2(tp);
        if (!haveF || n - lastVoiced > holdSamples) logF = lt;  // nota nueva: salto directo
        haveF = true; lastVoiced = n;
        logF += (lt - logF) * glide;
      }
      const a = Math.abs(x[n]);
      env += (a - env) * (a > env ? envA : envR);
      const open = haveF && (n - lastVoiced) <= holdSamples;
      const target = open ? env * 1.5 : 0;
      amp += (target - amp) * (target > amp ? smA : smR);
      if (amp < 1e-6 && !open) { continue; }
      const freq = Math.pow(2, logF);
      const ampN = Math.min(1, amp * 3);
      let m = 0;
      for (let v = 0; v < main.length; v++) m += main[v].tick(freq, ampN);
      m *= voiceNorm * amp;
      if (modActive) {
        let fl = 0, sh = 0;
        for (let v = 0; v < flat.length; v++) { fl += flat[v].tick(freq, ampN); sh += sharp[v].tick(freq, ampN); }
        fl *= voiceNorm * amp * p.modMix; sh *= voiceNorm * amp * p.modMix;
        outL[n] = (m + 0.9 * fl + 0.1 * sh) * modNorm;
        outR[n] = (m + 0.1 * fl + 0.9 * sh) * modNorm;
      } else { outL[n] = m; outR[n] = m; }
    }
    return [outL, outR];
  }

  // ---------- Efectos ----------
  function distShape(mode, u) {
    switch (mode) {
      case 'hard': { const v = 1.2 * u; return v > 1 ? 1 : (v < -1 ? -1 : v); }
      case 'rich': return Math.tanh(u + 0.35 * u * u / (1 + u * u));
      case 'wavefold': return Math.sin(Math.PI / 2 * u);
      case 'soft': default: return Math.tanh(u);
    }
  }
  function distortion(ch, sr, p) {
    const g = p.distGain, mode = p.distMode;
    const drive = mode === 'wavefold' ? 1 + g * 12 : db2lin(g * 40);
    let comp;
    if (mode === 'wavefold') comp = 0.7;
    else comp = 0.3 / Math.max(1e-3, Math.abs(distShape(mode, 0.3 * drive)));
    const lpHz = mode === 'soft' ? 7000 : (mode === 'rich' ? 5000 : 0);
    const lpC = lpHz ? 1 - Math.exp(-TWO_PI * lpHz / sr) : 1;
    const dcR = 1 - TWO_PI * 20 / sr;
    for (const b of ch) {
      let lp = 0, x1 = 0, y1 = 0;
      for (let i = 0; i < b.length; i++) {
        let y = distShape(mode, b[i] * drive) * comp;
        if (lpHz) { lp += (y - lp) * lpC; y = lp; }
        const o = y - x1 + dcR * y1; x1 = y; y1 = o;  // DC blocker
        b[i] = o;
      }
    }
  }
  function bitcrush(ch, sr, p) {
    const rate = Math.min(p.crushRate, 44100);
    const step = rate / sr;               // < 1 => sample & hold
    const bits = Math.round(p.crushBits);
    const q = Math.pow(2, bits - 1);
    for (const b of ch) {
      let ph = 1, held = 0;
      for (let i = 0; i < b.length; i++) {
        if (step >= 1) held = b[i];
        else { ph += step; if (ph >= 1) { ph -= 1; held = b[i]; } }
        b[i] = bits >= 24 ? held : Math.round(held * q) / q;
      }
    }
  }
  function ringmod(ch, sr, p) {
    const w = TWO_PI * p.ringFreq / sr, m = p.ringMix;
    for (const b of ch) {
      for (let i = 0; i < b.length; i++) {
        const c = Math.sin(w * i);
        b[i] = b[i] * (1 - m) + b[i] * c * m;
      }
    }
  }
  // Respuesta al impulso de la reverb con puerta (determinista, distinta L/R)
  function makeGatedIR(sr, timeMs, seed) {
    const L = Math.max(16, Math.round((0.025 + timeMs / 1000) * sr));
    const ir = new Float32Array(L);
    let s = seed >>> 0;
    const rnd = () => { s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0; return s / 4294967296 * 2 - 1; };
    const rise = Math.max(1, Math.round(Math.min(0.004 * sr, L * 0.1)));
    const fall = Math.max(1, Math.round(Math.min(0.006 * sr, L * 0.15)));
    const ph = (seed % 7) * 0.9;
    let hpX = 0, hpY = 0, lp = 0;
    const hpR = 1 - TWO_PI * 200 / sr, lpC = 1 - Math.exp(-TWO_PI * 6500 / sr);
    for (let i = 0; i < L; i++) {
      let e = 1 - 0.4 * (i / L);                       // casi plano
      if (i < rise) e *= i / rise;
      if (i > L - fall) e *= (L - i) / fall;           // corte abrupto
      e *= 1 + 0.15 * Math.sin(TWO_PI * 7 * i / sr + ph); // modulación interna suave
      let v = rnd() * e;
      lp += (v - lp) * lpC; v = lp;
      const o = v - hpX + hpR * hpY; hpX = v; hpY = o;
      ir[i] = o;
    }
    let E = 0; for (let i = 0; i < L; i++) E += ir[i] * ir[i];
    const k = 0.6 / Math.sqrt(E || 1);
    for (let i = 0; i < L; i++) ir[i] *= k;
    return ir;
  }
  function fftConvolve(x, ir) {
    const N = x.length, L = ir.length;
    let B = 1; while (B < L) B <<= 1;
    const n = 2 * B, fft = makeFFT(n);
    const hRe = new Float64Array(n), hIm = new Float64Array(n);
    hRe.set(ir); fft.transform(hRe, hIm, false);
    const out = new Float32Array(N);
    const re = new Float64Array(n), im = new Float64Array(n);
    for (let pos = 0; pos < N; pos += B) {
      re.fill(0); im.fill(0);
      const len = Math.min(B, N - pos);
      for (let i = 0; i < len; i++) re[i] = x[pos + i];
      fft.transform(re, im, false);
      for (let i = 0; i < n; i++) {
        const r = re[i] * hRe[i] - im[i] * hIm[i];
        const q = re[i] * hIm[i] + im[i] * hRe[i];
        re[i] = r; im[i] = q;
      }
      fft.transform(re, im, true);
      const lim = Math.min(n, N - pos);
      for (let i = 0; i < lim; i++) out[pos + i] += re[i];
    }
    return out;
  }
  function gatedReverb(ch, sr, p) {
    const N = ch[0].length;
    const mono = new Float32Array(N);
    for (let i = 0; i < N; i++) mono[i] = 0.5 * (ch[0][i] + ch[1][i]);
    const wl = fftConvolve(mono, makeGatedIR(sr, p.revTime, 0x9E3779B1));
    const wr = fftConvolve(mono, makeGatedIR(sr, p.revTime, 0x85EBCA6B));
    const m = p.revMix;
    for (let i = 0; i < N; i++) {
      ch[0][i] = ch[0][i] * (1 - m) + wl[i] * m;
      ch[1][i] = ch[1][i] * (1 - m) + wr[i] * m;
    }
  }

  // ---------- Cadena completa ----------
  function process(inputChannels, sr, params, progress) {
    const p = Object.assign({}, DEFAULTS, params || {});
    const prog = progress || function () {};
    const t0 = Date.now();
    // cola extra para que reverb/release no se corten
    const tail = Math.round(sr * (0.35 + (p.revOn ? p.revTime / 1000 : 0)));
    const inN = inputChannels[0].length, N = inN + tail;
    const gIn = db2lin(p.inputDb);
    const dryL = new Float32Array(N), dryR = new Float32Array(N);
    const src1 = inputChannels[0], src2 = inputChannels[1] || inputChannels[0];
    for (let i = 0; i < inN; i++) { dryL[i] = src1[i] * gIn; dryR[i] = src2[i] * gIn; }
    const mono = new Float32Array(N);
    for (let i = 0; i < N; i++) mono[i] = 0.5 * (dryL[i] + dryR[i]);

    prog('Detectando notas', 0.05);
    let track = null;
    let L, R;
    if (p.synthOn) {
      track = trackPitch(mono, sr, p.instrument, f => prog('Detectando notas', 0.05 + 0.45 * f));
      prog('Sintetizando', 0.5);
      const [sL, sR] = renderSynth(mono, sr, p, track);
      L = new Float32Array(N); R = new Float32Array(N);
      const m = p.synthMix;
      for (let i = 0; i < N; i++) { L[i] = dryL[i] * (1 - m) + sL[i] * m; R[i] = dryR[i] * (1 - m) + sR[i] * m; }
    } else {
      L = Float32Array.from(dryL); R = Float32Array.from(dryR);
    }
    const ch = [L, R];
    prog('Efectos', 0.7);
    if (p.distOn && (p.distPre || !p.distPost)) distortion(ch, sr, p);
    for (const fx of p.order) {
      if (fx === 'crush' && p.crushOn) bitcrush(ch, sr, p);
      if (fx === 'ring' && p.ringOn) ringmod(ch, sr, p);
      if (fx === 'rev' && p.revOn) { prog('Reverb', 0.8); gatedReverb(ch, sr, p); }
    }
    if (p.distOn && p.distPost) distortion(ch, sr, p);

    prog('Mezcla', 0.95);
    const gOut = db2lin(p.outputDb), gm = p.mix;
    let peak = 0;
    for (let i = 0; i < N; i++) {
      L[i] = (dryL[i] * (1 - gm) + L[i] * gm) * gOut;
      R[i] = (dryR[i] * (1 - gm) + R[i] * gm) * gOut;
      const a = Math.max(Math.abs(L[i]), Math.abs(R[i])); if (a > peak) peak = a;
    }
    let voiced = 0;
    if (track) for (let i = 0; i < track.pitch.length; i++) if (track.pitch[i] > 0) voiced++;
    return {
      left: L, right: R, sampleRate: sr, peak,
      pitch: track ? track.pitch : null, hop: track ? track.hop : 0,
      voicedRatio: track ? voiced / track.pitch.length : 0,
      ms: Date.now() - t0
    };
  }

  return { process, trackPitch, makeGatedIR, DEFAULTS, ENGINES, DIST_MODES, RANGES };
})();

// ---- Entorno: Worker o Node ----
if (typeof module !== 'undefined' && module.exports) {
  module.exports = FuzzDSP;
} else if (typeof self !== 'undefined' && typeof window === 'undefined') {
  let currentStage = 'inicio';
  self.onmessage = function (e) {
    const msg = e.data;
    if (msg.type !== 'process') return;
    try {
      currentStage = 'inicio';
      const res = FuzzDSP.process(msg.channels, msg.sampleRate, msg.params, (stage, frac) => {
        currentStage = stage;
        self.postMessage({ type: 'progress', id: msg.id, stage, frac });
      });
      let pitchDec = null;
      if (res.pitch) pitchDec = res.pitch;
      self.postMessage({ type: 'done', id: msg.id, left: res.left, right: res.right, sampleRate: res.sampleRate,
        peak: res.peak, pitch: pitchDec, hop: res.hop, voicedRatio: res.voicedRatio, ms: res.ms },
        [res.left.buffer, res.right.buffer]);
    } catch (err) {
      self.postMessage({ type: 'error', id: msg.id, stage: currentStage, message: String(err && err.message || err), stack: String(err && err.stack || '') });
    }
  };
  self.addEventListener('error', function (ev) {
    self.postMessage({ type: 'error', id: -1, stage: currentStage, message: String(ev.message), stack: (ev.filename || '') + ':' + (ev.lineno || '') });
  });
}
