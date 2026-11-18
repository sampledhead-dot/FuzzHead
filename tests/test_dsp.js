// Pruebas del motor DSP en Node: precisión del detector y rendimiento
const DSP = require(process.argv[2]);
const sr = 48000;

// Karplus-Strong: nota pulsada realista
function pluck(freq, dur, amp) {
  const N = Math.round(dur * sr), out = new Float32Array(N);
  const P = Math.round(sr / freq); const buf = new Float32Array(P);
  let s = 12345; for (let i = 0; i < P; i++) { s = (s * 1103515245 + 12345) >>> 0; buf[i] = (s / 4294967296 * 2 - 1) * amp; }
  let idx = 0;
  for (let i = 0; i < N; i++) { const a = buf[idx], b = buf[(idx + 1) % P]; buf[idx] = 0.996 * 0.5 * (a + b); out[i] = a; idx = (idx + 1) % P; }
  return out;
}
function seq(notes) {
  const parts = notes.map(([f, d]) => pluck(f, d, 0.5));
  const N = parts.reduce((a, b) => a + b.length, 0); const x = new Float32Array(N);
  let o = 0; for (const p of parts) { x.set(p, o); o += p.length; } return x;
}
const midi = m => 440 * Math.pow(2, (m - 69) / 12);

function checkTracking(name, instrument, notes) {
  const x = seq(notes.map(m => [midi(m), 0.5]));
  const t = DSP.trackPitch(x, sr, instrument);
  let off = 0, ok = 0, total = 0, maxErr = 0;
  notes.forEach((m, k) => {
    // evaluar entre 60 ms y 350 ms dentro de cada nota
    const a = Math.round((k * 0.5 + 0.06) * sr / t.hop), b = Math.round((k * 0.5 + 0.35) * sr / t.hop);
    for (let f = a; f < b; f++) {
      total++;
      if (t.pitch[f] <= 0) { off++; continue; }
      const cents = 1200 * Math.log2(t.pitch[f] / midi(m));
      if (Math.abs(cents) < 30) ok++; maxErr = Math.max(maxErr, Math.abs(cents));
    }
  });
  console.log(`${name}: ${(100 * ok / total).toFixed(1)}% dentro de ±30 cents, sin voz ${(100 * off / total).toFixed(1)}%, error máx ${maxErr.toFixed(0)} cents`);
  return ok / total;
}

const g = checkTracking('Guitarra E2..C6', 'guitar', [40, 45, 52, 57, 64, 69, 76, 84]);
const bs = checkTracking('Bajo B0..G3', 'bass', [23, 28, 33, 38, 43, 48, 55]);

// rendimiento y sanidad de toda la cadena con todo activado
const x = seq([[midi(52), 1], [midi(55), 1], [midi(57), 1], [midi(59), 1], [midi(64), 1], [midi(62), 1], [midi(60), 1], [midi(57), 3]]);
for (const engine of DSP.ENGINES) {
  const params = { engine, distOn: true, distMode: 'rich', distPre: true, distPost: true, modOn: true,
    crushOn: true, crushRate: 8000, crushBits: 8, ringOn: true, revOn: true, revTime: 600, order: ['rev', 'ring', 'crush'] };
  const r = DSP.process([x], sr, params);
  let nan = 0; for (let i = 0; i < r.left.length; i++) if (!isFinite(r.left[i]) || !isFinite(r.right[i])) nan++;
  console.log(`${engine.padEnd(8)} 10 s de audio en ${r.ms} ms · pico ${r.peak.toFixed(2)} · NaN ${nan}`);
  if (nan) process.exit(1);
}
for (const m of DSP.DIST_MODES) {
  const r = DSP.process([x], sr, { synthOn: false, distOn: true, distMode: m, distGain: 0.8 });
  console.log(`dist ${m.padEnd(8)} pico ${r.peak.toFixed(2)}`);
}
if (g < 0.9 || bs < 0.85) { console.log('FALLO precisión'); process.exit(1); }
console.log('OK');
