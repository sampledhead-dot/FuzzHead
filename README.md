# FuzzHead · Sampled Head

Sintetizador para guitarra y bajo: sigue la nota que tocas y la convierte en sinte (5 motores) con distorsión,
modulación, bitcrush, ring mod y gated reverb.

- **App web:** https://sampledhead-dot.github.io/FuzzHead/ — sube un audio o graba, escucha A/B y exporta WAV.
- **Plugin VST3 / AU / Standalone:** código en `vst/` (JUCE). Cada push compila Windows y macOS en la pestaña
  **Actions → build-vst** (descarga los binarios en *Artifacts*). Instrucciones en `vst/README.md`.
- Detalles, arquitectura, reglas y límites: `HOJA_DE_PROYECTO.md`.
