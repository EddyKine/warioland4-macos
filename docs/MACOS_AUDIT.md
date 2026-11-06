# Evaluación macOS arm64 — 27 de octubre de 2026

Estado actualizado: **juego ejecutable en macOS arm64 y aplicación abierta**.

## Implementación y resultado posterior

La auditoría inicial de abajo quedó superada por un backend Darwin experimental.
Se implementaron memoria GBA reubicada, traducción de punteros, handlers arm64 de
accesos a memoria, símbolos/secciones Mach-O y soporte POSIX para archivos/ajustes.
Los errores iniciales del enlazador, reserva de direcciones altas, accesos ordenados,
escrituras SIMD y espejos IWRAM se reprodujeron y corrigieron de forma incremental.

Aplicación: `/Volumes/Extreme SSD/GBA/Wario Land 4.app` (arm64, SDL3 incluido).
Doble clic para jugar. ROM propia fuera del bundle, original intacto.
Guardados: `/Volumes/Extreme SSD/GBA/saves/wl4.sav`.
Ajustes: `/Volumes/Extreme SSD/GBA/settings.cfg`.
Log: `/Volumes/Extreme SSD/GBA/wl4-last-run.log`.

Pasaron corutinas, BIOS/LZ77, I/O/DMA/interrupciones, ajustes/teclado/mando virtual,
guardado y recuperación. La ruta de 8500 cuadros llegó a Hall of Hieroglyphs sin
fallos; un proceso nuevo recargó su partida y mostró ese nivel en el menú.
La aplicación real usa Metal, muestra título y menú, recibe Enter y entra en
pantalla completa con F11. SDL abrió audio sin errores; la calidad audible queda
para comprobación del usuario. Se dejó abierta para uso.

Documentación técnica y límites: `port/macos/README.md`.
El juego completo aún no fue recorrido; no se afirma equivalencia con mGBA.
Sin cambios en fuentes compartidas GBA, sin commits ni publicación.

## Auditoría inicial (histórica)

Los siguientes obstáculos describen el punto de partida, no el estado ejecutable actual.

## Entorno y ubicación

Mac arm64, macOS 27.0.1, Apple Clang 21, Xcode SDK, CMake, Ninja y SDL3 ya instalados.
Fuentes: `/Volumes/Extreme SSD/GBA/warioland4`, rama `port`, commit
`b71d137afbdf0195586cdbf1e01056b44f5bcfee`.
Rama Linux inspeccionada: `8e49d1ff41c6e430f6aea12803e53731096cfdf8`.
Builds y evidencias: `build/macos-audit`. Temporales y cachés propios:
`/Volumes/Extreme SSD/GBA/.work`. No se instalaron dependencias nuevas.
Las herramientas del sistema pueden escribir registros/cachés internos; no se afirma cero escrituras internas.

## Resultados comprobados

- CMake encuentra SDL3. Se corrigió la inclusión accidental de metadatos AppleDouble
  `._*.c` en los globs y se restringió `-mno-ms-bitfields` a x86_64.
- `wl4-crt-test` es Mach-O arm64 y pasó `CRT render checks: PASS` con SDL dummy
  y renderizador software. Sin prueba de ventana real, Metal, pantalla o audio.
- `wl4-rumble-event-test` es Mach-O arm64 y pasó `Native rumble event checks: PASS`.
  Esto verifica eventos internos, sin mando físico.
- El build completo falla por `section(".text")` en `src/color_fading.c:915`:
  Mach-O exige segmento y sección. No se modificó código compartido para ocultarlo.
- El backend seleccionado usa `windows.h`, VirtualAlloc y excepciones Windows;
  settings y pruebas de I/O/guardado también usan Windows. Hace falta backend Darwin.
- El decodificador de accesos y los handlers actuales dependen de registros/instrucciones
  x86. La rama Linux utiliza ucontext x86, memfd_create y MAP_FIXED_NOREPLACE;
  no constituye un backend macOS arm64 utilizable directamente.
- La página del host mide 16384 bytes; el runtime presupone páginas de 4096.
- La prueba no destructiva con mmap y direcciones sugeridas recibió direcciones altas
  en lugar de cada dirección GBA pedida. Los experimentos con PAGEZERO reducido
  terminaron con código 137 antes de main, incluso tras firma ad hoc. No se determinó
  la causa del cierre, ni se demostró que cualquier diseño alternativo sea imposible.
- Todavía deben adaptarse símbolos absolutos y convenciones Mach-O, preservando
  direcciones GBA de 32 bits y semántica de DMA/volatile; no basta cambiar el compilador.

## Segunda ROM propia: compatible

Se verificaron `/Volumes/Extreme SSD/Wario Land 4 (USA, Europe).gba` y el
miembro del mismo nombre en `/Volumes/Extreme SSD/Wario Land 4 (USA, Europe).zip`.
Ambos: 8388608 bytes, código AWAE, MD5 `5fe47355a33e3fabec2a1607af88a404`.
El validador original compilado para arm64 devuelve `Supported ROM: PASS`, exit 0.
Copia de trabajo: `/Volumes/Extreme SSD/GBA/.work/rom/baserom_us.gba`.
Se preservaron originales. Evidencia: `build/macos-audit/second-rom-check.json`.
La ROM compatible resuelve el requisito de datos, pero no el backend macOS.

### Pruebas adicionales de memoria

`mach_memory_probe.c` consulta primero las regiones existentes para no sobrescribirlas.
`mach_vm_allocate` con dirección fija `0x02000000` devuelve `KERN_INVALID_ADDRESS`
(`(os/kern) invalid address`) en este ejecutable arm64 normal. El experimento de
enlazado con `-no_pie` produce advertencias explícitas: `-no_pie ignored for arm64*`
y `Linking with PIE, -image_base will be ignored`. El ejecutable con PAGEZERO reducido
termina antes de main, exit 137. Estos ensayos no aportan un backend funcional; no
se afirma que todos los rediseños de memoria posibles sean inviables.

Un port arm64 exige un diseño de acceso a memoria distinto o demostrar primero una
forma soportada de conservar las direcciones bajas, más handlers arm64 y enlazado Mach-O.
No se ha arrancado el juego. Logs: `mach-memory-probe.log`, `link-nopie.log`.

## Primera ROM propia (resultado anterior)

Se encontró dentro de:
`/Volumes/Extreme SSD/Wario Land 4 /Wario Land 4 - 50961/files/32968/50961.zip`
(el nombre de la carpeta contiene un espacio final), miembro `wario4.gba`.

Tamaño: 8388608 bytes. Código de juego: AWAE.
MD5 observado: `7cac34287d40b046e961687333226c36`.
MD5 que exige el port: `5fe47355a33e3fabec2a1607af88a404`.

Se compiló y ejecutó el validador original del proyecto en arm64: devuelve 1 y
`Unsupported ROM`. Se leyó la ROM directamente del ZIP en memoria, sin modificar
el original ni extraerla al disco. No se puede determinar solo por el hash la causa
de la diferencia. No se descargó ninguna ROM ni se relajó la validación.

## Repetir las pruebas de componentes

Desde la raíz del repositorio:

```sh
export TMPDIR='/Volumes/Extreme SSD/GBA/.work/tmp'
export CLANG_MODULE_CACHE_PATH='/Volumes/Extreme SSD/GBA/.work/cache/clang'
cmake -S . -B build/macos-audit -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=arm64 -DSDL3_DIR=/opt/homebrew/lib/cmake/SDL3
cmake --build build/macos-audit --target wl4-crt-test wl4-rumble-event-test
SDL_VIDEODRIVER=dummy build/macos-audit/wl4-crt-test build/macos-audit/crt-results - software
build/macos-audit/wl4-rumble-event-test
```

Logs: `configure.log`, `build.log` (primer fallo AppleDouble), `configure-adapted.log`,
`build-adapted.log`, `crt-test.log`, `rumble-test.log`, `rom-validation.log`,
`rom-check.json`, `memory-probe*.log`. Fuentes de diagnósticos: `memory_probe.c` y
`rom_check.c`, todo bajo `build/macos-audit/`.

## Alcance de cambios y trabajo pendiente

Cambios reversibles solo en CMake y documentación; no hay commits, pushes ni release.
`git diff --exit-code -- src include port/generated` devuelve 0.
No se tocó tools/agbcc. No se verificó una reconstrucción GBA byte a byte:
faltan el toolchain GBA preparado y los objetos de referencia locales del mantenedor.
La segunda ROM compatible está disponible para futuras pruebas.

La ROM propia compatible ya está disponible. Para ejecutar el juego hace falta una
implementación Darwin/arm64 del motor de memoria y excepciones, ajustes Mach-O,
pruebas de BIOS/I/O/corutinas/guardado y después gameplay/audio/entrada con ROM.
No hay ejecutable wl4 funcional ni prueba de gameplay completada.
