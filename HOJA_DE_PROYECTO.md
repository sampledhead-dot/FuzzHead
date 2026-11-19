# FuzzHead — Hoja de proyecto

Familia de apps de **Sampled Head**.

**Versión actual:** 1.0.1 (18-11-2026)

**Repositorio:** https://github.com/sampledhead-dot/FuzzHead · **Web:** https://sampledhead-dot.github.io/FuzzHead/

## Descripción
Sintetizador para guitarra y bajo, inspirado en la funcionalidad de un plugin comercial de tipo "guitar synth"
(manual de referencia del usuario), con nombre, diseño visual y código propios.
Antes se llamaba "Trazo GS-1" (versiones de prueba v1 y v2).
Convierte una línea monofónica de guitarra o bajo en sintetizador siguiendo el tono (bends, slides, vibrato),
y la pasa por una cadena de efectos. Dos productos que comparten el mismo diseño DSP:

1. **App web** (https://sampledhead-dot.github.io/FuzzHead/ · `index.html`): subir un archivo o grabar con el micrófono/interfaz, aplicar el
   procesado, escuchar A/B (procesado/original) y exportar WAV de 24 bits estéreo.
2. **Plugin** (`vst/`): VST3 (Win/Mac), AU (Mac) y app Standalone, en C++ con JUCE 9.0.5. Tiempo real.

## Estética
Inspirada en el logo de Sampled Head: gran círculo **naranja** (#d9600b) detrás del logotipo, módulos como la
**placa crema** de un sampler (#f3efe9) con tornillos, títulos en **pegatina negra** ligeramente girada,
botones de selección como **pads** naranjas, knobs como los botones redondos del logo (disco naranja, arco negro,
puntero crema) y la palabra **FUZZHEAD** en negro con contorno crema, sombra y goteos (estilo grafiti).
Icono propio: placa de sampler sobre círculo naranja. Misma paleta en la web y en el plugin.

## Funciones
- Selector **Guitarra / Bajo** (rango del detector): guitarra 70–1400 Hz (fiable E2–C6), bajo 28–700 Hz (fiable B0–G4).
- **5 motores**: Deep (seno + sub-octava), Vintage (sierra+cuadrada filtrada), Solo (lead con filtro dinámico),
  Strings (3 sierras desafinadas, ataque lento), Harmony (voz original + 2 intervalos de −24 a +24 semitonos; el 2.º desactivable).
- **Synth Mix** (guitarra seca ↔ sinte) y **On/Off** del motor (con el motor apagado los efectos actúan sobre la señal seca).
- **Distorsión** Soft / Hard / Rich / Wavefold, Gain, etapas **PRE** y **POST** (siempre queda una activa).
- **Modulación**: dos voces desafinadas (±30 cents a 100 %) abiertas en estéreo; solo con el motor encendido.
- **Bitcrush** (44,1 kHz → 441 Hz, 24 → 1 bits), **Ring Mod** (20 Hz–5 kHz + Mix), **Gated Reverb** (0–1000 ms + Mix).
- Bitcrush, Ring Mod y Gated Reverb **reordenables** (6 órdenes).
- Globales **Input** (±24 dB), **Mix** (seco/procesado), **Output** (±24 dB).
- **Presets**: 12 de fábrica (iguales en web y plugin) + de usuario (web: navegador + exportar/importar JSON;
  plugin: archivos `.preset` en Documentos/FuzzHead/User Presets). "Default" reservado.

## Arquitectura
```
Input gain ─┬───────────────────────────── seco ───────────────────────────┐
            └─ mono → Detector YIN → Motor (+Modulación) → Synth Mix          │
                 → Dist PRE → [Bitcrush · Ring Mod · Gated Reverb] → Dist POST ┴→ Mix global → Output
```
- **Detector de tono**: YIN causal, salto de ~2,7 ms (128 muestras a 48 kHz), ventana 1024 (guitarra) / 2048 (bajo);
  la función de diferencia se calcula con correlación por FFT. Umbral 0,15, interpolación parabólica,
  puerta a −52 dBFS, mediana de 3 frames contra saltos de octava, portamento de 4 ms y "hold" de 80 ms.
- **Osciladores** con polyBLEP (sin aliasing grueso); filtro SVF TPT.
- **Reverb con puerta**: convolución con una respuesta al impulso generada (ruido filtrado, envolvente casi plana
  y corte abrupto, distinta en L/R, modulación interna suave). Web: FFT overlap-add propia. Plugin: `juce::dsp::Convolution`.
- **Web**: todo el DSP corre en un Web Worker (no bloquea la interfaz). Procesado offline del archivo completo;
  se recalcula 0,3 s después de tocar un control. Archivo único autocontenido (funciona desde `file://`).
- **Plugin**: sin reservas de memoria en el hilo de audio (los dos detectores, guitarra y bajo, se preparan a la vez;
  la IR de la reverb se regenera en el hilo de mensajes). Parámetros automatizables y estado guardado en el proyecto del DAW.

### Archivos
| Ruta | Contenido |
|---|---|
| `index.html` | App web publicada por GitHub Pages (generada por `build_web.js`) |
| `dist/fuzzhead-vX.Y.Z.html` | Copia standalone para probar en local/móvil |
| `manifest.webmanifest`, `icon-*.png`, `apple-touch-icon.png` | Manifest e iconos (los lee el portal SH-apps) |
| `web/dsp.js` | Motor DSP en JavaScript (fuente) |
| `web/index.template.html` | Interfaz web (fuente) |
| `build_web.js` | Une plantilla + DSP → `index.html` y `dist/` (`node build_web.js`) |
| `tests/test_dsp.js` | Pruebas del DSP en Node (precisión y rendimiento) |
| `tests/test_web.py` | Prueba de extremo a extremo en Chrome headless (Playwright) |
| `vst/Source/dsp/FuzzDSP.h` | DSP en C++ (mismo diseño que `dsp.js`) |
| `vst/Source/PluginProcessor.*` | Parámetros, cadena de señal, presets, estado |
| `vst/Source/PluginEditor.*` | Interfaz del plugin |
| `vst/Source/Presets.h` | Presets de fábrica |
| `vst/tests/TestMain.cpp` | Pruebas offline del plugin |
| `.github/workflows/build-vst.yml` | Compilación automática Windows/macOS en GitHub Actions |

## Reglas permanentes
- **Versionado semántico** `MAYOR.MENOR.PARCHE` (empezando en 1.0.0): PARCHE = correcciones, MENOR = funciones nuevas
  compatibles, MAYOR = cambios que rompen presets o compatibilidad. La misma versión en web y plugin
  (`APP_VERSION` en `index.template.html`, `project(... VERSION)` en `vst/CMakeLists.txt` y la etiqueta del editor).
- Entrega numerada `fuzzhead-vX.Y.Z.zip` con carpeta `fuzzhead-vX.Y.Z`; la versión aparece en el pie (web) y en el editor (plugin).
- Siempre incluir la versión standalone de un solo archivo de la app web.
- Informe de error automático (.txt) en la web: se descarga solo ante cualquier error y con el botón
  "Descargar informe (.txt)". Nunca incluye el contenido del audio, solo nombre/tamaño/tipo.
- **Paridad**: cualquier cambio de sonido se hace en `web/dsp.js` **y** en `vst/Source/dsp/FuzzDSP.h`, con los mismos
  valores. Los presets de fábrica deben coincidir en `index.template.html` y `Presets.h`.
- Nombre **FuzzHead** y estética Sampled Head; no usar marcas, nombres ni diseño del producto de referencia (Audio Hertz / Soklo).
- Probar antes de entregar, incluido pulsar **cada** interruptor y botón en varios presets (tests/test_web.py).
- Probar antes de entregar (Node + Chrome headless para la web; `FuzzHeadTests` para el plugin).

## Historial de versiones
- **VST v1.1.0** (18-11-2026): aviso de versión nueva en la ventana del plugin. La versión del VST va aparte de la web (la web sigue en v1.0.1).
- **1.0.1** (18-11-2026): publicación en GitHub. `index.html` en la raíz (lo sirve GitHub Pages), `manifest.webmanifest`
  e iconos 192/512/maskable/apple-touch (placa de sampler sobre círculo naranja con pegatina FUZZHEAD), para que el
  portal SH-apps muestre su logo y se pueda añadir a la pantalla de inicio. Añadida al portal SH-apps (v8).
- **1.0.0** (18-11-2026): renombrado a **FuzzHead**; nuevo versionado semántico; nueva estética inspirada en el logo de
  Sampled Head en la web y en el plugin. Los presets de usuario guardados en el navegador con el nombre anterior se
  migran automáticamente. Plugin: fabricante "Sampled Head", códigos Smhd/Fzhd, presets en Documentos/FuzzHead/User Presets.
- *Versiones de prueba con el nombre anterior "Trazo GS-1":*
  - **v2** (18-11-2026): web — corregido que los interruptores On/Off de Bitcrush, Ring Mod y Gated Reverb no respondían
  (se conectaban antes de añadir esos módulos a la página). Ahora se usa delegación de eventos. Plugin sin cambios.
  - **v1** (18-11-2026): primera versión. App web completa (subir/grabar, 5 motores, 5 efectos, orden, presets,
  A/B, WAV 24 bits, informe de error) y plugin JUCE (VST3/AU/Standalone) con la misma cadena y presets.

## Límites conocidos
- **Monofónico**: los acordes y notas que se solapan confunden al detector (igual que el original).
- **Bajo**: las notas más graves (B0–E1) tardan ~60–90 ms en engancharse (hacen falta dos periodos de la onda);
  la guitarra ~12–20 ms. En el plugin esto es latencia de detección, no se compensa.
- **Web**: procesa el archivo entero, no en vivo; no hay monitorización en tiempo real mientras grabas
  (para tocar en directo usa la app Standalone o el plugin). Archivos de más de 10 minutos se rechazan.
- **Web**: la grabación usa el códec del navegador (Opus/AAC); para máxima calidad, sube un WAV.
- **Distorsión sin sobremuestreo**: a ganancia alta puede haber algo de aliasing (pendiente: 2× oversampling).
- **Plugin**: compilado y probado solo en Linux en esta entrega (sin audio real ni DAW). Windows/macOS se compilan
  con el workflow de GitHub Actions o localmente; no se ha podido ejecutar dentro de un DAW. Sin firma de código en macOS.
- Los sonidos de los motores son diseños propios aproximados a las descripciones del manual, no una réplica.

## Aviso de versión nueva (v1.1.0)
`Source/UpdateBanner.h` (común a los plugins de Sampled Head):
- Solo con la ventana abierta (nunca al escanear ni en el hilo de audio) y como mucho una vez al día por ordenador. Lo guarda en `<datos de usuario>/SampledHead/<Plugin>.settings`: `%APPDATA%` en Windows, `~/Library/Application Support` en Mac.
- Pide a la API pública de GitHub la lista de releases del repo y elige la más alta con etiqueta `vst-vX.Y.Z`, sin contar borradores ni pre-releases.
- Si es más nueva que la instalada, aparece una franja abajo a la derecha con «Descargar» (abre la release en el navegador) y «✕» (no volver a avisar de esa versión).
- El plugin no se reemplaza solo: el DAW lo tiene cargado y está en una carpeta del sistema.
- Usa la red del sistema: WinINet en Windows y NSURLSession en Mac. En Linux (solo pruebas) está sin curl y no consulta nada.
- Pruebas: `SH_UPDATE_FAKE=<json>` y `SH_UPDATE_SETTINGS=<carpeta>`. Las pruebas cubren comparar versiones, elegir la release, que el aviso aparezca, que «✕» lo oculte y que con la misma versión no avise.
- **Importante al publicar:** la versión del plugin está en `CMakeLists.txt` (`project(... VERSION x.y.z)`, que también da nombre a la release) y en `kVersion` del procesador. Hay que cambiar las dos.

## Mac (19-11-2026)
- Los plugins para Mac se compilan en las máquinas **Intel** de GitHub (`macos-15-intel`). El binario es **universal** (Intel + Apple Silicon), con VST3, AU y Standalone y firma ad-hoc.
- Con las máquinas Apple Silicon (`macos-15`) las tareas nunca empezaban. GitHub lo explicaba en las anotaciones de cada tarea: «The job was not acquired by Runner of type hosted even after multiple attempts» y «Due to capacity constraints, jobs targeting macOS arm64 runners may experience longer queue times». Era falta de capacidad de GitHub, no un problema de la cuenta.
- Ahora se compila para Mac siempre. Al lanzarlo a mano se puede desmarcar «mac».
- Sin certificado de Apple (Developer ID, 99 €/año), la primera vez macOS pide permiso: Ajustes del Sistema → Privacidad y seguridad → «Abrir igualmente», o `xattr -cr` sobre el plugin.
- No probado en un Mac real ni en Logic, Ableton u otro DAW de Mac.
