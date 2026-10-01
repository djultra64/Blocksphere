# M0 — Registro ampliado de referencia

Fecha: 2026-09-27. Complementa la prueba inicial, no sustituye sus ajustes ni sus resultados. La referencia sigue siendo emulada; no hay consola disponible.

## Equipos confirmados

El usuario confirmó ROG Ally RC71L. El inventario Windows se ejecutó por SSH autorizado, tomando exclusivamente el perfil de conexión existente en Meltdown y sin modificar ese proyecto. El [snapshot](../qa/windows-hardware-2026-09-27.json) y el [colector](../../tools/qa/collect_windows.ps1) no contienen destino SSH, credenciales ni números de serie. Windows 11 Pro build 26200, Ryzen 5 5600X, RTX 2060/controlador 32.0.16.1088, RAM instalada 32 GiB. Pantalla activa Samsung LS32B30 a 1920×1080/60 Hz. Los registros PnP de mandos no equivalen al número de dispositivos físicos.

La pantalla 4K y el segundo mando están disponibles según el usuario; conectar la pantalla y validar el port a 2160p corresponde a las pruebas de presentación posteriores. El snapshot Linux actual conserva dos salidas 1080p. No se inventa modelo/refresco 4K.

## Sesión controlada

Mismo paquete ares 148 y runtime de la prueba inicial. Xvfb privado 1024×768, perfil público con PixelAccuracy, Expansion Pak desactivado, audio SDL 48 kHz/20 ms, sin mejoras visuales. Configuración efectiva privada: `.local/m0/ares-controlled.bml`. Dos puertos N64 tienen conjuntos de teclas distintos. Se llegó al tablero VS de dos esferas, con HUD separados; se observaron caídas, pérdida de vidas y pantalla WIN; se aplicaron movimientos P1 (flecha derecha) y P2 (L) en acciones separadas. Esto acredita los canales lógicos de entrada, no mandos físicos independientes.

Se creó el nombre F, entró en Rescue 1:1, observó esfera/HUD, giró en dos direcciones, eliminó piezas con puntuación 300 y exposición parcial del núcleo, abrió pausa con CONTINUE/AUDIO/EXIT y salió por el menú. Se entró en Training/Basics y se recorrieron páginas, con pausa CONTINUE/SKIP/EXIT. La ruta VS → BATTLE → 2 PLAYERS permite registrar nombres separados, elegir robots y fijar core area (valor inicial 15 para ambos). También se inició Vs CPU con un rival robot; se observó tablero, eliminación de piezas del rival y pausa. Las capturas tomadas durante su introducción se distinguen del tablero. Después se abrió Time Trial, observando cuenta descendente y pérdida de vidas al dejar el tablero inactivo. Las acciones y capturas permanecen en `.local/m0/controlled-actions.jsonl` y `.local/m0/captures/`.

Puzzle1 se resolvió deslizando la pieza azul inferior izquierda una columna a la derecha (B + Right), luego A para eliminar. Se observó transición a Puzzle2 y el hash EEPROM cambió. Se produjo `.local/m0/controlled-saves/Nintendo 64/tetrisphere-us.eeprom`, 512 bytes: EEPROM de 4 Kbit. La prueba inicial sin completar etapa mostró seis slots vacíos tras reinicio. Después de resolver Puzzle1 y salir, LOAD NAME mostró F en el slot1. Un tercer proceso arrancó sin savestate y mostró F en slot1; persistencia del nombre acreditada. Se confirmó LOAD GAME (el primer A sobre el slot abre LOAD GAME/DELETE; el segundo confirma cargar). Luego PUZZLE ofreció «continuing previous game», nivel 2. Persistencia de progreso acreditada tras cierre normal y arranque limpio, sin savestate. SHA-256 EEPROM después de Puzzle1: `faeabea4aea99c178ed69d52966321750b33d1c4e106c8c4647cacc4ac9a24dd`. Fixture privado conservado para regresión. No debe interpretarse el rótulo del catálogo «4KB» como un archivo de 4096 bytes.

El audio aislado se tomó de la entrada PulseAudio del proceso ares, se devolvió a su salida original y se retiró el sink temporal. WAV privado de 59.999979 s, dos canales, PCM de 16 bits/48 kHz; 5 758 905 muestras no nulas, pico absoluto 32768. SHA-256 `d38947f199dd74664f94b9411443c56b08b21c159d67064c682b0837dfd60631`. Otra captura VS con caídas/fin de ronda: 30.001021 s estéreo/48 kHz, 2 879 416 muestras no nulas, SHA-256 `2c8da34d50c6194136cde273b54ba2c5fa8388338bd70ea74a2edaa05897a95a`. Acredita captura de señal; no demuestra fidelidad perceptual ni ausencia de clipping/underruns.

## Instrumentación

[Mapa de contadores](../../config/roms/tetrisphere-us-rev0-counters.json) y [muestreador](../../tools/qa/measure_reference.py). El cliente lee memoria por GDB local de ares; no detiene la CPU ni escribe memoria. Valida el hash de la ROM proporcionada y cinco fingerprints de código vivo. Contadores de 32 bits y reloj de 64 bits contemplan wrap; se comprueba el checksum del transporte.

- VI: retraces del gestor VI, `0x80163b5c`.
- Actualizaciones comunes: `0x800dfee8`, ruta de evento de scheduler `0x29a`. Sigue activa en pausa y menús: no se presenta como frecuencia de toda la simulación del tablero.
- Tareas gráficas completadas: `0x800df714`, evento tipo 2. No equivale a imágenes únicas ni presents del host; el menú mostró variación.
- Tiempo emulado: acumulación COP0 Count `0x80163b50`, 46 875 000 Hz.

Ejemplo desde la raíz con ares ejecutándose y DebugServer habilitado en puerto 19123:

```bash
python3 tools/qa/measure_reference.py --rom .local/m0/tetrisphere-us.z64 \
  --scene rescue-idle --seconds 60 --repeats 3 > .local/m0/timing-rescue.jsonl
```

Las etiquetas de escena son aportadas por el operador; el programa no identifica el modo activo. El coste de muestreo se guarda con cada ventana. Los datos son metadatos numéricos, sin memoria original.

VS en tablero: una ventana de 60.0641 s con movimientos secuenciales, distinta de la introducción: 3590 retraces, 3590 actualizaciones comunes, 1277 tareas gráficas completadas (21.261/s); tiempo emulado/real 0.999052. La menor cadencia de tareas gráficas coexiste con reloj emulado próximo al real. No se atribuye una causa definitiva ni se fija por ello el objetivo de presentación del port. Las [muestras públicas](../qa/m0-timing-2026-09-27.json) incluyen las transiciones y pausas en sus notas. Time Trial inactivo terminó por pérdida de vidas; no se computa como ronda completa. Una primera serie de tres ventanas de 60 s, etiquetada inicialmente single-menu, estuvo realmente en el menú principal tras salir de Rescue. Retraces: 59.765–59.805/s; actualizaciones comunes: 59.371–59.732/s; tareas gráficas: 28.642–33.798/s. Tiempo emulado/real: 0.998976–0.999645. Se corrige la etiqueta al consolidar. En pausa de Rescue, ventana de 10.0452 s: 601 retraces, 601 actualizaciones comunes y 301 tareas gráficas. Son medidas de esta referencia/host, no límites del futuro port.

## Límites vigentes

Los resultados de cierre figuran debajo. La regresión conserva efectos completos de magia, niveles tardíos, independencia física de mandos y todos los accesos especiales; no se declara viable N64Recomp/RT64. M0 cataloga las áreas y registra escenas representativas, sin afirmar ejecución de las campañas completas.

La revisión independiente del muestreador no encontró defectos críticos/importantes. Se corrigió la preservación del error original si también falla detach; se añadieron pruebas de wrap de reloj y rechazo de código vivo incorrecto. La revisión inicial abarcó29 pruebas; la ampliación de cierre se registra debajo.

## Comprobaciones de cierre

El análisis posterior identificó pasos de actualización del jugador en `0x800acb94`: procesa entradas y estado, y el store `0x800acfe4` incrementa `0x800e4478` cuando el campo de contexto `+0x23b2` vale 1. El contexto en `0x80110220` se inicializa con ese valor; el segundo, `0x80113488`, con 2. La ruta `0x8006e98c` drena el anillo de entradas de 180 posiciones y llama a `0x8006e418`, que despacha ambos contextos. Este contador acredita pasos admitidos de actualización de P1, no que todos los subsistemas terminen una simulación completa por incremento.

`0x800e4480` cuenta despachos de estado/actualización/dibujo desde `0x800cce7c` hacia `0x80070118`. La copia de pasos pendientes `0x800e4484` a `0x80160c64` y su puesta a cero preceden a esa llamada: consumen el lote pendiente anterior. El recorrido del tablero utiliza ese delta para temporizadores. Ambos contadores nuevos se reinician al inicializar escenas. El muestreador rechaza sus tasas cuando disminuyen o cambia el identificador de modo; un reset que ocurre y recupera el valor entre muestras requiere detectar la transición por el registro de sesión. No debe tratarse cualquier transición como wrap.

Hide & Seek: tres ventanas de 60 s con inactividad y transición MULTI 1:1 → DRILL 1:2/objetivo. Pasos lógicos: 59.778–59.827/s; tareas gráficas: 32.026–33.523/s. Se observó puntuación 10700 y la instrucción de buscar cinco taladros; no se acredita una victoria manual experta. En pausa real CONTINUE/GOAL/AUDIO/EXIT durante 10.1067 s: 603 VI, 604 actualizaciones comunes, **cero pasos lógicos**, 302 tareas gráficas. Así se distingue la lógica de partida de los temporizadores de UI.

Se observaron demos originales de Training/Advanced de Gravity Combo y Fuse Combo: preparación, caída/deslizamiento, eliminación, cambio de X-value a 2 y contador de power ups. Son referencias guionizadas del juego; las eliminaciones erróneas de Practice no se cuentan como cadenas. Training/Puzzle también se abrió, con límites de drops/slides y pausa. El vídeo privado de Advanced dura 120 s. Su WAV de 120.002354 s contiene silencio: no se utiliza como evidencia audible de esos efectos; las capturas de menú y VS anteriores sí contienen señal.

La [observación de imágenes](../qa/m0-image-observations-2026-09-27.json) captura solo el área de juego a 120 Hz durante 60 s: 7200 muestras, 3588 cambios respecto a la muestra anterior. Esa tasa de 59.8 cambios/s describe la salida de Xvfb, con procesamiento VI; puede incluir cambios de ruido/dithering y no equivale a nueva geometría o tareas gráficas. No se deduce interpolación del port de esos hashes.

La revisión independiente confirmó la separación de pasos del jugador, temporizadores y dibujo, y advirtió los resets y el orden del lote pendiente. Las pruebas nuevas se observaron fallar antes del cambio; suite ampliada: 31 pruebas. El inventario documental y las escenas representativas cierran la referencia de M0; el guion exhaustivo de niveles, magia, música, hardware físico y presentación continúa como regresión de M3/M5.

Lines se desbloqueó escribiendo LINES en NEW NAME y confirmando con Start. Apareció al final de la selección Single; se observaron instrucciones y esfera1:1. Una ventana de60.1038s:3595VI,3594pasos lógicos,1835tareas gráficas; tasas59.813/59.797/30.531 respectivamente, tiempo emulado/real0.999784. No se confundió ese nombre con un minijuego2D.

Opciones originales abiertas: Audio(balance/auto-random-choose/pista), DataEdit(seis slots y LoadGame/Delete, acción de limpiar cartucho), Scores(Rescue/puntuación y Puzzle/tiempo, cinco posiciones), Controls(diagrama). Se cambió pista de Prophetic-Title a AzuleLux. Una segunda captura de audio de20.00225s, con mute retirado solamente del sink temporal propio, produjo1919950muestras no nulas; SHA256232a5561a188de3485e0b1ed3c1b5124fab26758f9575a0010dd18a304735ec3. El mute de la salida física se conservó. El WAV silencioso de Advanced se rechaza como prueba audible; no demuestra que el juego carezca de sonido.
