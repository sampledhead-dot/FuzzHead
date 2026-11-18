# FuzzHead — plugin VST3 / AU / Standalone (JUCE 9)

## Compilar sin instalar nada (recomendado)
1. Crea un repositorio en GitHub y sube **todo** el contenido de la carpeta `fuzzhead-v1.0.0` (incluida `.github`).
2. Ve a **Actions → build-vst**. Cada push compila Windows y macOS (≈10–15 min).
3. Descarga el artefacto `fuzzhead-windows` o `fuzzhead-macos`.

## Compilar en tu ordenador
Requisitos: CMake ≥ 3.22, Git y un compilador (Visual Studio 2022 en Windows · Xcode en macOS).

```bash
cmake -S vst -B build -DCMAKE_BUILD_TYPE=Release      # descarga JUCE 9.0.5 automáticamente
cmake --build build --config Release
```
Con una copia local de JUCE: `-DJUCE_DIR=/ruta/a/JUCE`.

Resultados en `build/FuzzHead_artefacts/Release/`:
- **Windows**: copia `VST3/FuzzHead.vst3` a `C:\Program Files\Common Files\VST3\`
- **macOS**: `VST3/FuzzHead.vst3` → `/Library/Audio/Plug-Ins/VST3/` · `AU/FuzzHead.component` → `/Library/Audio/Plug-Ins/Components/`
- **Standalone**: app independiente para tocar en directo con tu interfaz de audio.

En macOS, un plugin sin firmar puede quedar bloqueado: `xattr -cr "/Library/Audio/Plug-Ins/VST3/FuzzHead.vst3"`.

## Pruebas
```bash
cmake -S vst -B build -DFUZZHEAD_BUILD_TESTS=ON && cmake --build build --target FuzzHeadTests
./build/FuzzHeadTests_artefacts/Release/FuzzHeadTests
```
Comprueba precisión del detector (guitarra y bajo), niveles de cada motor, los 6 órdenes de efectos,
guardado/restauración de estado y genera una captura del editor en `/tmp/fuzzhead_editor.png`.

## Presets de usuario
Archivos `.preset` (XML) en `Documentos/FuzzHead/User Presets/`. Se pueden copiar entre ordenadores.
