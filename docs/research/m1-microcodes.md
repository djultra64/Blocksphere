# Microcódigos y rutas audiovisual de M1

## Alcance y privacidad

Esta caracterización usa tareas `OSTask` reales de Tetrisphere USA rev0. Los
descriptores, RDRAM, listas de comandos, microcódigos y PCM permanecen bajo
`.local/m1/microcode/`, ignorado por Git. El repositorio conserva únicamente
metadatos acotados, hashes, inventarios y decisiones de compatibilidad en
`config/recomp/microcodes.json`.

Las tareas se capturaron en la entrada real de `osSpTaskLoad` (`0x800d1abc`),
donde `a0` aún es el `OSTask *` original. Se contrastó con el constructor
gráfico `0x800cce7c..0x800cd2bc`, el constructor de audio en `0x8007ed88` y el
scheduler que llama `osSpTaskLoad` en `0x80028f98`. Esto evita etiquetar los
registros SP/AI MMIO como descriptores de tarea.

## Gráficos

La tarea viva es tipo 1 y contiene `rspboot` en `0x800dd300`, texto en
`0x800dd3d0`, datos en `0x800f1570`, una display list F3D y buffers de
salida/yield reales. La cadena de datos dice `RSP SW Version: 2.0H,
02-12-97`.

La identificación no depende sólo de esa cadena. Con el layout word-swapped
que RT64 espera, la captura produce:

- texto: 0x13f8 bytes, XXH3-64 `0x7d8eb8bdcae7df81`;
- datos: 0x800 bytes, XXH3-64 `0x880fce853ce3e422`.

El primer hash tiene tres candidatos en la base fijada de RT64; el segundo
reduce la intersección exactamente a `F3D_SM64_FINAL`. El diagnóstico final
usa el `RT64::Interpreter` fijado en
`43373749dac9bbc1b653e6a02aed40a9e1783bed`, carga ese GBI y ejecuta la lista
real desde `OSTask.data_ptr`, incluidas sus llamadas y retornos anidados. La
ruta CPU de RT64 produjo 7 476 índices de cara, 416 llamadas de juego y dos
envíos de framebuffer. Cada entrada nula de la tabla se reemplaza sólo por un
handler de rechazo que registra opcode y dirección y hace fallar el test.

La orden `FULLSYNC` del diagnóstico conserva `State::flush` y el envío
preliminar de framebuffer mediante `State::submitFramebufferPair`. El límite
demostrado es el intérprete real, las listas anidadas, la generación de draw
data y esos envíos CPU preliminares. El adaptador no ejecuta el resto del
`State::fullSync`: finalización CPU de operaciones de framebuffer, validación
de tiles/raw-TMEM, construcción de descriptores de shader/render, warnings,
uploads ni presentación GPU. Task 7 debe ejecutar ese `fullSync` y
postprocesamiento sin modificar con un dispositivo real. Por tanto esta
evidencia no acredita una imagen presentada. La lista es GBI/F3D, no un stream
RDP crudo; `tools/analysis/microcode.py --kind gbi` conserva el opcode
completo y `--kind rdp` sólo corresponde a streams RDP LLE.

## Audio y RSPRecomp

La tarea viva es tipo 2: `rspboot=0x800dd300/0xd0`,
`ucode=0x800de7d0`, `ucode_data=0x800f1d70/0x800` y una lista real de 895
comandos. No coincide con una ABI de audio implementada por el runtime fijado,
por lo que M1 usa RSPRecomp.

El primer intento se colgó porque se recompiló el texto como si comenzara en
IMEM `0x1000`. Desensamblar el `rspboot` capturado demostró que carga 0xf80
bytes a IMEM `0x1080` y salta a `0x1080`. Con esa dirección y los 16 destinos
de la tabla indirecta registrados en `config/recomp/audio-rsp.toml`, la misma
tarea sale por `RspExitReason::Broke`.

Los comandos `SETBUFF` (`0x08`) dan el tamaño y los `SAVEBUFF` (`0x06`) la
dirección física. Los cinco guardados PCM terminales son cuatro bloques de
0x280 bytes y uno de 0x80: 2 688 bytes, 1 344 muestras o 672 frames estéreo.
Frente al snapshot detenido antes de ejecutar la tarea, cambiaron 2 649 bytes
de esos rangos y quedaron 1 343 muestras no nulas. El diagnóstico entrega
cada rango a `AudioBridge`: registra cinco callbacks, 1 344 muestras y 672
frames. No se usa audio del emulador ni un stub que finja éxito.

## Reproducción privada

Partiendo de los artefactos privados capturados y de las dependencias fijadas:

```sh
.local/m1/toolchain/n64recomp-build/RSPRecomp config/recomp/audio-rsp.toml

python3 tools/analysis/microcode.py \
  --descriptor .local/m1/microcode/final-private/gfx-descriptor.bin \
  --commands .local/m1/microcode/final-private/gfx-command-list.bin \
  --kind gbi \
  --supported 0x01,0x03,0x04,0x06,0xb3,0xb4,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbf,0xe4,0xe6,0xe7,0xe8,0xe9,0xed,0xf0,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf9,0xfa,0xfc,0xfd,0xfe,0xff \
  --output .local/m1/microcode/gfx-inventory.json

cmake -S . -B .local/m1/microcode/integration-build -G Ninja \
  -DTETRISPHERE_GENERATED_DIR=.local/m1/runtime/review3-final-a/functions \
  -DTETRISPHERE_GENERATION_MANIFEST=.local/m1/runtime/review3-final-manifest-a.json \
  -DTETRISPHERE_RSP_RECOMP_SOURCE=.local/m1/microcode/audio-rsp-recompiled.c \
  -DTETRISPHERE_GRAPHICS_SNAPSHOT=.local/m1/microcode/live-rdram.bin \
  -DTETRISPHERE_GRAPHICS_COMMANDS=.local/m1/microcode/final-private/gfx-command-list.bin \
  -DTETRISPHERE_AUDIO_SNAPSHOT=.local/m1/microcode/audio-live-rdram.bin
cmake --build .local/m1/microcode/integration-build \
  --target microcode_contract_test microcode_live_test
ctest --test-dir .local/m1/microcode/integration-build \
  --output-on-failure -R '^microcode_'
```

Antes de configurar, `tools/build/verify_microcode_inputs.py` vincula por hash
los snapshots, descriptores, comandos y ambos microcódigos al manifiesto
sanitizado. CMake genera un receipt y el target `ALL`
`verify_microcode_inputs` rechaza cualquier cambio posterior. RT64 tiene el
mismo receipt/guard de revisión, árbol limpio e identidad de fuentes que
N64ModernRuntime; esa identidad participa en el build ID.

En el host actual esos comandos se ejecutan dentro del SDK Flatpak 25.08
porque el host no expone CMake. El test `microcode_live` tiene timeout de 15 s,
rechaza tamaños/capturas incoherentes, exige la revisión exacta y limpia de
RT64, selecciona el RSP sólo para el hash/dirección de microcódigo observada y
comprueba invariantes concretos de salida PCM.

## Backlog acotado

- Capturar otras escenas para ampliar cobertura; la tarea registrada sí recorre
  su grafo anidado completo, pero un opcode nuevo falla cerrado hasta añadir y
  probar compatibilidad.
- Una variante de microcódigo de audio no selecciona automáticamente esta
  función recompilada: debe capturarse, identificarse y probarse por separado.
- La fidelidad perceptual, música y efecto dentro de una escena nativa completa
  pertenecen a la integración de Task 7/HITL; esta prueba acredita la tarea RSP
  y los callbacks PCM, no una escucha humana.
