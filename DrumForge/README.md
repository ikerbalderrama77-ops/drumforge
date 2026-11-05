# DRUMFORGE (VST3)

## Compilar e instalar en tu PC (Windows / macOS)
Requisitos: CMake >= 3.22, Git, y Visual Studio 2022 (Windows) o Xcode (macOS). Necesita internet la primera vez (descarga JUCE 8.0.4).

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --parallel
    ctest --test-dir build -C Release --output-on-failure

Con `DRUMFORGE_COPY_AFTER_BUILD=ON` (por defecto) JUCE copia el VST3 a:
- Windows: `C:\Program Files\Common Files\VST3`  (ejecuta la terminal como administrador)
- macOS: `~/Library/Audio/Plug-Ins/VST3`

El .vst3 queda tambien en `build/DRUMFORGE_artefacts/Release/VST3/`.

## Sin instalar nada: GitHub Actions
Sube esta carpeta a un repositorio de GitHub; `.github/workflows/build.yml` compila en Windows y macOS, corre las pruebas y publica el .vst3 como artefacto descargable.
