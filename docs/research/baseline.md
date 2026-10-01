# M0 — Referencia de Tetrisphere

Fecha: 2026-09-27. **Estado: referencia técnica de M0 consolidada; ver entrega y límites.**

Actualización de hardware, instrumentación y sesión: [registro ampliado](m0-reference-session.md).

La ROM y los equipos Linux/Windows están identificados. Se registraron modos, giro, cadenas guionizadas, audio, pausa, VS y persistencia de Puzzle2 tras reinicio. Hay contadores separados de VI, pasos lógicos del jugador, despacho de estado y tareas gráficas; las observaciones de imágenes se registran aparte. La cobertura exhaustiva de fidelidad corresponde a M3. No se ha construido un port nativo.

## ROM objetivo

| Campo | Resultado medido |
|---|---|
| Entrada | ZIP local aportado por el usuario; una ROM de 8 MiB |
| Nombre interno | TETRISPHERE |
| Código | NTPE; país `0x45` |
| Revisión de cabecera | 0 |
| Región objetivo | USA, NTSC-U, inglés |
| Tamaño | 8 388 608 bytes |
| Formato recibido | z64, big endian (`80371240`) |
| SHA-256 normalizado | `f7cbc93ac273488bfb89955ab6ae9b60c730907872107a67a56b945e8579ef87` |
| SHA-1 normalizado | `32adaa274ceed246aeee9f53592d642a121f3bdb` |
| MD5 normalizado | `3f88078e2d9dbf6c9372f6373cf9ae09` |
| CRC1/CRC2 almacenados en cabecera | `3c1fdabe / 02a4e0ba` |
| Entrada declarada | `0x80025c50` |

MD5 y pareja CRC coinciden con `Tetrisphere (U) [!]` en el [catálogo de Mupen64Plus fijado por commit](https://github.com/mupen64plus/mupen64plus-core/blob/b20b27ebf9e5b099a978e86dba609111dc98c837/data/mupen64plus.ini). La revisión 0 se obtiene del byte `0x3f`; no se deduce del nombre del ZIP. Los CRC de cabecera no se han recalculado como checksum CIC. El SHA-256 de todo el contenido determina la revisión reconocida.

El [manifiesto](../../config/roms/tetrisphere-us-rev0.json) contiene únicamente metadatos. Reconocer este hash no acredita compatibilidad con N64Recomp o RT64. Una ROM PAL, modificada o de revisión distinta se rechaza aunque conserve título y CRC de cabecera.

## Validador reproducible

Desde la raíz del proyecto:

```bash
python3 tools/rom/validate.py /path/to/Tetrisphere.zip
python3 -m unittest discover -s tests -v
```

El primer comando devuelve JSON y código 0 para la revisión reconocida; código 1 y `reason` para un rechazo. `--help` describe la interfaz. Admite dumps z64/v64/n64 identificados por magia; normaliza en memoria. También lee un ZIP que contenga exactamente una entrada `.z64`, `.v64` o `.n64`; no extrae archivos. Los nombres dentro del ZIP no se usan como rutas de escritura.

Rechazos: cabecera incompleta/incorrecta, tamaño no alineado, truncamiento, tamaño excedente, revisión desconocida o contenido modificado, archivo inaccesible, ZIP ambiguo/sin ROM/corrupto/cifrado. El límite de lectura de contenido depende del tamaño soportado, 8 MiB. No intenta reparar dumps ni quitar prefijos arbitrarios.

Se ejecutaron 19 pruebas de ROM con datos sintéticos propios y una validación adicional sobre la ROM local. Las pruebas cubren normalización completa, corrupción del contenido conservando cabecera, otra revisión/país, límites de tamaño, ZIP y salida CLI desde otro directorio. Las pruebas públicas no requieren ROM.

## Equipo Linux observado

El [snapshot reproducible](../qa/linux-hardware-2026-09-27.json) se obtuvo con `tools/qa/collect_linux.py`. No incluye nombres de usuario/host, números de serie ni claves.

| Campo | Observación |
|---|---|
| Modelo DMI | **ROG Ally RC71L_RC71L**; confirmado por el usuario |
| CPU | AMD Ryzen Z1 Extreme, x86-64 |
| RAM visible para Linux | 11 922 120 KiB; no equivale a RAM física instalada por la reserva UMA |
| GPU | AMD Phoenix, PCI `1002:15bf` |
| Controlador host | amdgpu; RADV/Mesa 26.0.4; Vulkan GPU 1.4.335 |
| Sistema | Bazzite 43, imagen `43.20260420.0`, Kinoite/deck |
| Kernel | `6.17.7-ba29.fc43.x86_64` |
| Sesión medida | KDE/Wayland, escritorio |
| Perfil ACPI | performance; potencia TDP exacta pendiente |
| Alimentación durante inventario | corriente conectada; batería 79 %, sin cargar |
| Pantallas activas | DP-12: 1920×1080 a 74.973 Hz; DP-11: 1920×1080 a 60 Hz |
| Pantalla integrada | eDP-1: 1920×1080/120 Hz disponible, desactivada |
| Mandos detectados | Handheld Daemon Controller; Microsoft X-Box 360 pad; HHD 4.1.5 |

No se atribuye el mando virtual a una persona ni se acredita independencia P1/P2 por enumerarlo. La pantalla 4K está disponible según el usuario; no aparece conectada en este snapshot. El modo juego, uso con batería, TDP efectivo y reconexión siguen pendientes.

El usuario confirmó acceso a Windows real, segundo mando y pantalla 4K en Bazzite. Confirmó que no dispone de Nintendo 64. La ficha Windows se obtuvo por SSH autorizado usando el colector PowerShell. Véase [snapshot Windows](../qa/windows-hardware-2026-09-27.json): Windows 11 Pro build 26200, Ryzen 5 5600X, RTX 2060, 32 GiB instalados, monitor Samsung LS32B30 a 1080p/60 Hz. No se ejecutó todavía un port nativo.

## Referencia emulada observada

ares v148 instalado mediante Flatpak: `dev.ares.ares`, commit de paquete `5b6ccf0aef391c237a34e41cf14da438446aa454371408e3c0d418f818132a77`, runtime `org.freedesktop.Platform/x86_64/25.08`. ares describe su objetivo de precisión y preservación en su [repositorio oficial](https://github.com/ares-emulator/ares). Eso no sustituye pruebas específicas del juego ni una comparación con consola.

Se arrancó la ROM validada en una pantalla Xvfb aislada de 1024×768, con OpenGL 3.2 para presentar, calidad SD, procesamiento VI activado, sin shader, supersampling, mezcla entre cuadros, runahead ni rewind. Audio SDL, 48 kHz, bloqueo de audio activado. El registro del paquete informa OpenGL Mesa **26.2.2**, distinto del Mesa host 26.0.4. Se observaron avisos de compilación de shaders, incluidos aproximadamente 256 ms; no se confunden con ralentización original del juego.

La captura a los 18 segundos muestra la animación inicial y el rótulo NTSC. La barra de estado indica **59 VPS** en ese instante. Es una lectura puntual del emulador: no prueba 59 FPS de dibujo, frecuencia lógica ni cadencia VI sostenida.

Evidencia privada, excluida de Git:

- `.local/m0/captures/boot-18s.png`: captura de introducción, no partida.
- `.local/m0/ares-boot.log`: registro de esta ejecución.
- `.local/m0/ares-reference.bml`: configuración efectiva de la prueba de arranque.
- `.local/m0/tetrisphere-us.z64`: copia local del dump, sin redistribuir.

Esta fue la prueba inicial de introducción. Las sesiones posteriores adoptaron el [perfil nuevo](../../config/ares148-reference.bml): PixelAccuracy, latencia20ms y ExpansionPak desactivado. Se verificaron cierre normal, guardado/carga, señal audible y dos puertos de entrada por teclado. El [registro ampliado](m0-reference-session.md) conserva sus diferencias y límites; no se confunden sus ajustes con los de la introducción inicial.

## Inventario y tiempos

La [matriz de contenido](modes-matrix.md) diferencia etiquetas observadas en la ROM, documentación y comprobación jugable pendiente. Localizar una etiqueta no demuestra que el modo sea accesible ni completo.

| Magnitud | Estado |
|---|---|
| Arranque y dibujo de introducción | Observado |
| Cadencia VI por escena | Contador identificado y muestreado; ver registro ampliado |
| Actualización lógica por escena | Pasos de actualización P1 identificados y medidos; separación activo/pausa comprobada |
| Tareas de dibujo por escena | Completions y despachos medidos; cambios de imagen del host registrados aparte |
| Relación tiempo del juego / reloj real | COP0 Count frente a reloj real medido; ronda completa TimeTrial queda en regresión |
| Escenas de juego/cadenas/dos jugadores | Cadencia variable de gráficos registrada; emulación cercana a tiempo real en las ventanas medidas |

El [protocolo](../qa/m0-reference-protocol.md) distingue la referencia representativa de M0 del recorrido exhaustivo de M3/M5. Un vídeo o VPS aislado no fija el reloj del port. El mapa de contadores y sus fingerprints respaldan las [medidas](../qa/m0-timing-2026-09-27.json); no se presupone una frecuencia de dibujo única para todos los modos.

## Herramientas y procedencia

Python 3.14.3, Git 2.53.0 y GCC 15.2.1 están disponibles. CMake, Ninja y Clang no se localizaron en PATH; no se ha instalado una toolchain nueva. Ghidra 12.1.3 está instalado como Flatpak; splat/spimdisasm no se han ejecutado.

Los [commits candidatos](../../config/dependencies-m0.json) y la [auditoría inicial](dependencies.md) fijan procedencia para la siguiente investigación. No constituyen un conjunto integrado que compile. N64Recomp/RSPRecomp, runtime, RT64 y frontend necesitan validación de compatibilidad en M1.

## Entrega M0

Véase [entrega y decisiones de alcance](../milestones/M0.md). No hace falta aportar otra ROM ni ejecutar el colector Windows. Se conserva un inventario de regresión: niveles tardíos, efectos completos, perfiles de energía, mandos físicos y presentación 4K deberán comprobarse en sus hitos. La viabilidad N64Recomp/RT64 y los relojes de todos los subsistemas siguen siendo investigación de M1/M3/M4, sin promesa de compatibilidad.
