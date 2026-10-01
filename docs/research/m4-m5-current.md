# M4/M5 — estado de la presentación nativa

Los ajustes persistentes v4 permiten solicitar resolución interna Auto, 240p,
720p, 1080p, 1440p o 2160p, aspecto 4:3/16:9 y cadencia original/30/60/120.
La CLI `tetrisphere-config graphics` los guarda. El comando
`tetrisphere-config graphics window-mode windowed|fullscreen` selecciona el
modo persistente. En ventana, el tamaño se limita al área utilizable. En
pantalla completa con resolución 2160p se solicita
un modo físico exacto 3840×2160 si SDL lo anuncia en alguna pantalla conectada;
se prefiere la pantalla actual. Se comprueban pantalla, modo aplicado y drawable
antes de declararlo UHD. Si no está disponible, se
usa pantalla completa a la resolución actual del escritorio. El evento
`graphics_surface_dimensions` registra el tamaño drawable, el modo aplicado y
las dimensiones del display para verificar el resultado. El multiplicador de
render interno de RT64 se fija por separado (1, 3, 4.5, 6 o 9 respecto a 240
líneas). Por tanto, 2160p
interno puede renderizarse en una ventana menor, pero esto no acredita una
sesión a 3840×2160 en la pantalla física.
En la prueba Linux `menu-4k-internal-aspect-fix`, el menú permaneció estable
durante 45 s sin costuras visibles con escala solicitada 9× y altura interna
2160. El drawable midió 1831×1002: esa captura no valida salida física 4K,
ya que los monitores conectados no ofrecen ese modo.
Otra prueba Linux solicitó fullscreen y 2160p interno: SDL aplicó fullscreen
desktop 1920×1080 al no encontrar modo 4K y el menú se presentó estable con
bandas negras (`.local/m5/fullscreen-4k-internal-fallback.png`).
Windows repitió el fallback en el binario `3c2e59a`: modo de escritorio y
drawable 1920×1080, altura interna solicitada 2160, menú 4:3 completo y
estable durante 30 s. Evidencia privada:
`.local/m5/windows-fullscreen-3c2e59a/`.
En Windows, el binario de `86c946c` se ejecutó con escala solicitada 9×,
altura interna 2160, drawable 1835×1032 y aspecto 16:9 durante una ruta
automatizada de menú y Training. El menú mantuvo bandas limpias y la ruta
Training cambió la configuración efectiva a 16:9 sin cerrarse. Evidencia
privada: `.local/m5/windows-menu-aspect-86c946c/`.
Otra ruta Windows con ese binario alcanzó VS → Battle → 2 Players → editor de
nombres. La build permaneció viva 573 s, pero el teclado remoto sólo controla
P1; el campo P2 no recibió entrada y no se llegó al tablero. Capturas y log
privados: `.local/m5/windows-vs-two-ipac-adaptive-86c946c/`. Una prueba
posterior con dos puertos SDL virtuales independientes sí llegó al tablero,
según se detalla abajo.
Para ensayos aislados, `TETRISPHERE_DATA_DIR` dirige guardado, ajustes y
diagnósticos a una carpeta privada; la CLI admite `--data-dir` para esa misma
carpeta. Un arranque normal sigue usando el directorio por usuario de SDL.

El puente RT64 aplica resolución, aspecto y solicitud de refresco al crear el
renderer y en cambios de `GraphicsConfig`. El diagnóstico etiqueta los valores
como `requested_unverified`; `graphics_surface_dimensions` distingue ventana
lógica, drawable en píxeles y altura interna solicitada. No hay aún selector
de VSync ni cadencia sin límite. RT64 limita `RefreshRate::Manual` al refresco
de la swapchain (`rt64_workload_queue.cpp:216–232`). El evento actual
`rt64_present` señala una petición de actualización de pantalla del runtime;
no mide necesariamente presentaciones reales de la GPU.
Los ajustes `30` y `120` siguen disponibles como solicitudes experimentales;
la matriz de aceptación actual cubre original y 60. Una opción visible no
equivale a una cadencia visual validada.

En el commit `0f90561`, se ejecutó la matriz de 40 combinaciones de menú:
Linux 20/20 capturas tras 62 s por caso, cierre con código 0, dispositivo RT64
listo y presentación registrada; Windows 20/20 capturas tras aproximadamente
un minuto por caso, proceso vivo y ajustes solicitados comprobados. Cada caso
usó un directorio de datos privado. En 2160p solicitado, las ventanas quedaron
limitadas por los monitores de 1080p: Linux 1831×1002 para 16:9 y Windows
1835×1032; son tamaños de salida, no la resolución interna real. El menú
seguía en 4:3 con bandas laterales en ambos sistemas. Las capturas de Windows
en 240p y 2160p permiten revisar la posición de las medallas translúcidas sin
costura vertical; la comparación humana de su recorte con el original sigue
pendiente. Evidencia privada: `.local/m5/matrix-linux-20260930T052453Z/` y
`.local/m5/windows-graphics-matrix-0f90561-full/`. La matriz acredita arranque
y menú; no acredita una partida jugable ni 4K físico en cada combinación.
En una ruta Windows separada con la misma build, teclado P1 avanzó de menú a
Single/Rescue: el estado `guest_scene_hint` pasó de `0` a `-1`, el aspecto
efectivo de 4:3 a 16:9 y una captura un minuto después seguía mostrando la
esfera y el HUD dentro del ancho disponible con el proceso vivo. Evidencia
privada: `.local/m5/windows-single-rescue-adaptive-0f90561/`. No se accionó P2
ni se probó VS en este recorrido.

Un cambio de tamaño de ventana Linux durante el menú provocó dos SIGSEGV
repetibles en el cuerpo invitado `func_800D8330`, que escribe en el registro
N64 SP_STATUS. La ruta se activa al solicitar `osSpTaskYield` en `0x800D1D40`
cuando una tarea gráfica se demora. El runtime ya completa SP antes de procesar
la lista gráfica y devuelve `osSpTaskYielded=0`; faltaba enlazar la operación
Yield correspondiente. Un build de trabajo con ese enlace pasó el mismo resize
4:3 a 1831×1002 tras 12 s, registró tres yields y una consulta Yielded falsa,
alcanzó Rescue, capturó el tablero después de 101 s y terminó con código 0.
Evidencia privada: `.local/m5/rescue-aspect-20260930T061001Z/rescue-43/`;
comparación previa al arreglo:
`.local/m5/rescue-aspect-20260930T055132Z/rescue-43/` y
`.local/m5/rescue-aspect-20260930T055935Z/rescue-43/`. Se requiere
recompilación Windows del enlace antes de atribuirle allí la corrección.

Una prueba separada usó Xvfb 3840×2160 y Vulkan lavapipe con ese binario de
trabajo. SDL registró `fullscreen_kind=exclusive-uhd`, modo aplicado,
drawable y display de 3840×2160; RT64 creó el dispositivo, presentó frames y
se capturó una imagen a los 30 s con el proceso vivo. Evidencia privada:
`.local/m5/virtual-uhd-20260930T0614Z/`. Esta pantalla es virtual y no mide
el rendimiento de GPU ni la salida física de un monitor. SDL advirtió después
que revirtió el cambio de modo al no detectar la ventana fullscreen, así que
el evento inicial no acredita fullscreen exclusivo sostenido. Se observó un
SIGSEGV en un hilo de rasterizado lavapipe durante esa sesión; el proceso
devolvió -11 al detener la prueba. El volcado no identifica SIGTERM como causa.
La estabilidad de esta ruta queda sin acreditar. Esa sesión no midió el tamaño
interno de RT64.

Con un probe RT64 opt-in compilado desde copias de las dos fuentes RT64 fijadas,
dos rutas Linux Single/Rescue solicitaron 16:9 y 2160p internos en una ventana
1831×1002. El presentador recibió un target y textura de 2880×2160 en menú
4:3 y de **3840×2160 en Rescue 16:9**. Cada corrida terminó con código 0,
6120 presentaciones y 6120 workloads en aproximadamente 103 s; ambas
registraron `vi_original_rate=60` y cero frames interpolados. El ajuste
`original` produjo `target_rate=0`; el ajuste `60` produjo `target_rate=60`,
igual a la cadencia VI original. No hubo frames adicionales por el ajuste 60
en estas escenas. Evidencia privada:
`.local/m5/rt64-probe-20260930T063348Z/probe-original/`,
`.local/m5/rt64-probe-20260930T063610Z/probe-60/` y
`.local/m5/rt64-probe-comparison.json`. El probe mide render y envío a
swapchain; no demuestra scanout físico 4K ni ausencia de tirones a 4K de
salida.

En una captura privada Linux de menú a 1080p interno y 16:9, RT64 mezcló
capas 4:3 y 16:9 con costuras verticales visibles
(`.local/m5/menu-16-9-1080.png`). El estado del juego en `0x800E121C`
selecciona la ruta de menú con valores no negativos; una captura de tablero VS
observó el cambio 0 → -1. El render conserva la elección 16:9 y usa aspecto
efectivo 4:3 para menús, con bandas laterales. La captura
`.local/m5/menu-16-9-aspect-fix.png` muestra bandas limpias sin costuras.
El selector 16:9 usa el objetivo manual exacto de RT64 en el juego. El
presentador agrega bandas si la ventana difiere de esa proporción, mientras
los menús conservan su composición 4:3. En una ventana Linux 1831×1002, la
región activa medida en una fila central fue 1336×1002 en menú (4:3) y
1781×1002 en Rescue (aproximadamente 16:9 por redondeo de píxeles). Capturas
privadas: `.local/m5/sdl-event-exact-16-9-20260930T051429Z/`.
El cambio de aspecto al entrar en Training se observó en Windows. En Linux,
una secuencia de entrada inyectada en `SDL_PollEvent` llegó a un tablero
Single/Rescue en la build con el arreglo: `guest_scene_hint` cambió de `0` a
`-1`, RT64 aplicó 16:9 y la imagen ocupó el ancho con esfera circular y HUD
visible. La sesión duró 99 s sin cierre inesperado; evidencia privada
`.local/m5/sdl-event-injected-single-20260930T045827Z/board.png`. Esta
inyección verifica la transición y la imagen, pero no sustituye mandos reales.
Faltan el retorno al menú con este arreglo y el recorrido equivalente de VS
en Linux.

Una ruta Linux nativa de Practice con la build diagnóstica de SHA
`091b9d083e35f3f5729505a1304fb745d57d678ea8bb61d0f7e1203ec7375bad`
solicitó 2160p internos y 16:9. Se observó el tablero, rotación y pausa; RT64
presentó target y textura **3840×2160** en una ventana de 1831×1002, y el
proceso terminó con código 0. La esfera se ve circular y el HUD completo en
`.local/m5/modes-uhd-pilot-20260930T091451Z/`. Esto acredita la ruta visual
de Practice en Linux, sin afirmar salida física 4K ni completar la progresión.
En otra ruta nativa, Time Trial abrió con reloj 04:49, produjo una eliminación
válida y llegó a 03:38; no se completó la ronda de cinco minutos.
Evidencia privada: `.local/m5/time-trial-pilot-20260930T090648Z/`.
Al intentar Hide & Seek, la misma build abortó durante la introducción con
`fatal_training_prompt font_identity_mismatch` en `0x8016BF30`. No existe
todavía un tablero estable verificable de ese modo en este ensayo.
Evidencia privada: `.local/m5/time-trial-pilot-20260930T091151Z/`.
El commit `5302639` reconoció una segunda fuente IA8 exacta que el juego copia
en esa misma dirección. Una build Linux posterior mostró la Z de teclado
limpia dentro de la instrucción original; al pulsar Z avanzó a un tablero
Hide & Seek con HUD y corazones. Otra ruta Training Basics mostró Z y la
misma pulsación avanzó al texto siguiente. Ambas terminaron con código 0.
El ensayo con selección manual PlayStation terminó usando la familia teclado
por la entrada activa; no acredita todavía el glifo de un mando PlayStation.
Evidencia privada: `.local/m5/hide_font_probe/evidence-1790761999/` y
`.local/m5/hide_font_probe/training-1790762171/`.
Una ruta VS CPU Linux con la build diagnóstica de SHA
`091b9d083e35f3f5729505a1304fb745d57d678ea8bb61d0f7e1203ec7375bad`
llegó al tablero real tras la introducción,
mostró movimiento del rival, rotación de P1 y pausa, y cerró con código 0.
RT64 presentó target/textura 3840×2160, pero el contenido visible siguió
limitado a aproximadamente 4:3, con bandas negras laterales. Esta observación
extiende el defecto de widescreen a VS CPU, además de VS Player.
Evidencia privada: `.local/m5/modes-uhd-pilot-20260930T091930Z/`.

En Windows, la build instrumentada del commit `8413620` llegó a VS → Battle →
2 Players con mandos SDL virtuales independientes (P1 navegó antes con teclado).
La pulsación de P2 confirmó la entrada a un tablero real con dos esferas, HUD,
corazones y cursores separados, visible en
`.local/m5/windows-vs-virtual-keyboard-p2-8413620/vs-virtual-019-1-south.png`.
La misma captura, de 1851×1071 con bordes de ventana, conserva negro en
x≈8–235 y x≈1614–1842 a una fila central; el contenido activo mide
aproximadamente 1378×1008, cercano a 4:3. El log pasó a aspecto efectivo
16:9, así que VS aún no acredita widescreen real; se investiga si la
proyección, el viewport o la composición VI retienen 4:3. Una captura inicial
no acredita que ambos jugadores respondan durante el combate ni que la
partida llegue a su fin. Tras otros 60 s, la captura
`vs-virtual-020-wait60.png` seguía mostrando los dos tableros y efectos
activos, sin cierre del proceso; persistían las bandas laterales. La misma
sesión Windows permaneció viva 1392 s y se cerró normalmente tras mostrar
un resultado WIN. Registró 83 415 VI y 83 296 polls de entrada; hubo acciones
en ambos puertos, aunque no se demostró su efecto independiente durante el
combate. No equivale todavía a la sesión de 30 minutos requerida por M2.
Una repetición Windows con el probe inicial de aspecto (`b1792a4`), ajustes
2160p internos/16:9/60 solicitados y dos puertos SDL llegó al comienzo de
VS, pero terminó antes del tablero con `fatal_unsupported osAiSetNextBuffer`:
tamaño `0xFFFFFE40` (−448 bytes) en el callsite invitado `0x8007EDC8`.
Hasta ese punto se observó target 2880×2160 de menú y el cambio de aspecto
efectivo a 16:9, sin ningún present de tablero a 3840×2160. La investigación
apunta a una transición de frecuencia 44,1→30 kHz con DMA previo pendiente;
la coincidencia aritmética aún requiere confirmación de la traza instrumentada.
Evidencia privada: `.local/m5/windows-vs-virtual-b1792a4-2160p-probe/`.
Una repetición con instrumentación opt-in de audio y la misma configuración sí
llegó al tablero y permaneció más de 60 s en combate. RT64 registró
presentaciones válidas de target y textura **3840×2160**, con drawable
1835×1032; las bandas laterales 4:3 siguieron visibles. En muestras del
tablero, 90 proyecciones con scissor 160×240 de un framebuffer 320×240 tuvieron
`adjust_aspect_ratio=false` y `covers_whole_width=false`; otras 22 con
scissor 320×240 sí ajustaron la proyección. El viewport amplio fue falso en
89 de 135 decisiones muestreadas, verdadero en 46. El probe de esa build
tomaba muestras por llamada, por lo que estos conteos no aseguran cobertura
simétrica de P1 y P2; sí muestran que RT64 rechaza la ampliación de
proyecciones parciales. La traza de audio en el intento estable midió el cambio
44 100→30 000 Hz con 432 frames pendientes antes de reiniciar la cola, luego
longitud reportada 0 y primer DMA nuevo de 720 frames. El fallo anterior sigue
siendo intermitente y no está resuelto. Evidencia privada:
`.local/m5/windows-vs-virtual-b1792a4-audio-transition-probe/`.

Una nueva build Windows del commit `22f710f` con probe ampliado quedó detenida
en la selección `Core Area` antes de llegar al tablero. Los dos puertos
virtuales recibieron acciones adicionales, pero la imagen permaneció idéntica;
las interrupciones VI y el sondeo de entrada continuaron, mientras
`audio_rsp_tasks` quedó fijo en 17 963 desde aproximadamente el segundo 300
hasta el cierre a los 888 s, y la cola SDL llegó a cero. No se obtuvo una
traza de viewport de VS de este intento, ni se ha identificado todavía la
causa del estancamiento. Evidencia privada:
`.local/m5/windows-vs-virtual-22f710f-2160p-probe-retry/`.

Una captura de tablero de juego Linux en 16:9 (`.local/m5/rescue-native-x11-20260930T043107Z/07-rescue-candidate.png`)
muestra imagen hasta los bordes, esfera centrada y HUD dentro del cuadro. La
ruta se alcanzó con teclado en el binario anterior al arreglo de menú; no
acredita aún dos mandos físicos ni una sesión prolongada. Para VS, RT64 decide
la corrección horizontal de la matriz y la expansión de
viewport/scissor por heurísticas independientes. Ambas exigen, en modo Auto,
que el viewport cubra el ancho de origen. Si VS usa vistas parciales, forzar
solamente la matriz puede dejar las vistas estrechas. Es preciso identificar
los draws reales y coordinar matriz, viewport, scissor, HUD y cursor por jugador
antes de declarar 16:9 correcto para VS. Quedan pendientes la validación
widescreen del tablero VS en Windows, validación 4K física, métricas de frames realmente presentados
en las escenas jugables y revisión HITL-3. La matriz obligatoria de 40
configuraciones ya se ejecutó en menú, pero aún faltan las comprobaciones de
viewport y HUD en cada modo representativo. 120 FPS es opcional;
la opción 60 aún requiere medir frames visuales realmente producidos. En una
prueba Rescue Linux de 20 s con MangoHud, ambos ajustes `original` y `60`
registraron aproximadamente 60 presentaciones Vulkan/s y cambios de píxel en
las 600 muestras de vídeo de la escena jugable. No se observó mejora medible
de la opción 60 frente a original; falta atribuir los cambios de geometría a
interpolación y repetir a 4K/Windows. Evidencia privada:
`.local/m5/refresh-original-20260930T050742Z/` y
`.local/m5/refresh-60-20260930T051013Z/`.
En esos CSV de MangoHud, 1200 muestras `original` promediaron 60,006 FPS
con p95/p99 de frametime 16,899/17,071 ms; 1199 muestras del ajuste `60`
promediaron 60,009 FPS con p95/p99 16,878/17,223 ms. Son ventanas cortas
de Rescue en el monitor de 1080p y una build anterior a los probes; no
representan rendimiento sostenido con salida 4K.

Una captura posterior sin pérdida FFV1 de Rescue a 60 Hz durante 24 s registró
1440 frames con PTS espaciados 16–17 ms, sin huecos superiores a 20 ms. En dos
regiones interiores de la esfera, una medición subpíxel con correlación normalizada
encontró desplazamiento superior a 0,2 píxeles en 297/300 pares al girar a la
derecha y 296/299 al girar a la izquierda; durante la pausa, ambas regiones
permanecieron quietas en 114/120 pares. La mediana fue aproximadamente 8
píxeles por frame en sentidos opuestos, sin patrón alterno de duplicados a 30
Hz en esas ventanas. Evidencia privada:
`.local/m5/motion60-20260930T064829Z/motion-original/`. La captura X11 no está
sincronizada directamente con `vkQueuePresent` y estos dos segmentos de 5 s no
acreditan todavía el ritmo sostenido ni los percentiles de frame time a 4K.

Una pareja adicional de Rescue 4:3/16:9 con la misma build diagnóstica,
ventana 1831×1002 y resolución interna 1080p midió áreas activas de
1336×1002 y 1781×1002 respectivamente: el ancho creció 445 píxeles
(33,3 %) y mantuvo la altura. Evidencia privada:
`.local/m5/rescue-aspect-20260930T070031Z/geometry-analysis.json`.
Las capturas a los 101,6 s no coincidieron en estado: una tenía puntuación
0 y dos corazones, la otra 10 300 y tres corazones. Por ello el distinto
diámetro visible de la esfera no sirve para concluir si la proyección conserva
la escala vertical. Falta una comparación visual del mismo estado del tablero.

La sonda de aspecto del commit `e242de5` selecciona los mismos workload IDs
no nulos en proyección y viewport. Una sesión diagnóstica Linux de menú a
Single/Rescue registró 516 pares de eventos con clave
`(workload_id, fb_pair_index, projection_index, transforms_index)`, sin filas
sin pareja ni truncamiento. Las 305 muestras previas del menú tenían escala de
aspecto 1,0 y target 2880×2160. En las 211 muestras de Rescue con 16:9,
`adjust_aspect_ratio=true`, `projection_ratio_scale=0,75`,
`use_wide_viewport=true` y target 3840×2160. La captura muestra esfera
circular y HUD visible. El proceso cerró con código 0; el drawable físico era
1831×1002, por lo que esta prueba sigue siendo de resolución **interna**.
El binario diagnóstico SHA256
`091b9d083e35f3f5729505a1304fb745d57d678ea8bb61d0f7e1203ec7375bad`
incluía además una instrumentación de audio sin commit, opt-in y desactivada.
Evidencia privada: `.local/m5/aspect-probe-smoke-20260930T0232-local/`.
La sonda excluye renderizado síncrono RT64 con workload ID0, que ocurre antes
de asignar un ID al frame presentado. Estos datos son consistentes con campo
vertical conservado en Rescue; aún falta la comparación del mismo estado A/B
y la comprobación equivalente en VS.

Una sesión nativa Linux con el binario Release de `8413620` (SHA256
`a94dae92e1ae745ea45265bd20b6fb00d64cd0a6358251cfaadf6b51be942299`)
permaneció 1904,75 s, entró en Single/Rescue tras aproximadamente 103 s y
recibió 117 pulsaciones direccionales automatizadas más confirmaciones
periódicas para reiniciar tras las pantallas de resultado. Las capturas a
10, 20 y 25 minutos mostraron tablero; las de 5 y 15 minutos mostraron un
resultado `Try Again`. Esto acredita un proceso y presentación prolongados con
intentos repetidos, no una partida ininterrumpida ni una ruta hasta el menú.
La VI registró 114 015 ticks en 1900 s (≈60,0/s). La cola de audio a 40 kHz
terminó con 39 vaciados detectados; el contador subió de 4 a 39 entre los
minutos 24 y 26, y luego permaneció estable. Puede relacionarse con los
chasquidos ocasionales reportados, sin captura de audio que demuestre su
coincidencia perceptual. El arnés terminó con `xdotool windowclose`, que
destruye directamente la ventana X11: el proceso recibió `BadWindow` para ese
mismo XID y abortó con `-6`. La prueba no acredita un defecto de cierre normal;
hay que repetirlo con `xdotool windowquit`, que solicita `SDL_QUIT`. Evidencia
privada: `.local/m5/m2-long-rescue-20260930T071558Z/summary.json` y sus
logs/capturas.

En una ruta Linux posterior con el binario SHA256
`4a29879a32e13cd707da45461427e6090ac7b8e5d336b5b76079f21852eaa8c7`,
el nombre especial `LINES` añadió la fila original al selector Single. Se
abrieron las reglas, se confirmó su última página y apareció el tablero Lines
1:1 con esfera circular, HUD y tres corazones. Una entrada direccional y una
acción desplazaron cursor/tablero, la pausa abrió su menú y el proceso cerró
con código 0. RT64 registró target y textura de **3840×2160** durante el
tablero, con drawable físico de 1831×1002. El ensayo utilizó un guardado
aislado y una copia RT64 de trabajo anterior a la última guardia de scissor
del parche VS; acredita esta ruta Linux, no una build final ni salida física
4K. Evidencia privada: `.local/m5/lines-linux-20260930T0425/`, especialmente
`01-lines-unlocked.png`, `01-lines-board-live.png`, `01-lines-action.png` y
`01-lines-pause.png`.

El commit `4e48682` amplió el límite de espera de `osAiGetLength` conforme al
tamaño y frecuencia reales del buffer SDL: dos periodos de callback más 5 ms,
con máximo de 100 ms. Para 1024 frames a 30 kHz resulta 74 ms; los 16 ms
anteriores podían vencer antes del primer drenaje normal. La prueba de unidad
simula un drenaje a 34 ms y el CTest Linux pasó 40/40. Una repetición VS CPU
4:3 alcanzó tablero a los 79 s, seguía activa a los 89 s y cerró con código
0 sin fatal; no cruzó el umbral que fuerza la espera, por lo que el caso
temporal real sigue pendiente de repetición. Evidencia privada:
`.local/m5/audio-vs-74ms-43/`.

### Revalidación de la build panorámica y de audio

El commit `c81cc54` coordina proyección, viewport y scissor de las dos
mitades de VS, y mantiene el borrado de color dentro de cada mitad. En Linux,
VS CPU a 2160p internos/16:9 llegó al tablero, recibió entrada de P1 y cerró
con código 0. Su área activa midió 1781×1002 píxeles frente a 1336×1002 en
4:3 con la misma altura; RT64 registró target y textura de 3840×2160, y dos
mitades de color de 1920×2160. El monitor seguía siendo de 1080p. Evidencia
privada: `.local/m5/vs-final-169-20260930T0438/`; binario Release SHA256
`b661b5d0621738de29f78df361c498ecbfadc6da1e91f96ece29fa45b0d3d3b0`.

En Windows, la build instrumentada del mismo código, con dos puertos SDL
virtuales, pasó de `Core Area` al tablero VS de dos jugadores. La captura
`vs-virtual-022-1-south.png` muestra las dos esferas, HUD y cursores en
16:9, sin las bandas 4:3 del intento anterior. RT64 registró target y textura
3840×2160, swapchain 1835×1032 y presentaciones correctas. La sesión siguió
viva hasta el cierre solicitado a los 1162 s; no se registraron fatales ni
timeouts de audio. La ronda llegó pronto a `WIN`, de modo que faltan acciones
visuales independientes de P1 y P2 dentro de una partida sostenida. El proceso
no fue instrumentado para registrar su código de salida al cierre. Evidencia
privada: `.local/m5/windows-vs-virtual-7c87593-audio74-wide/`. El monitor
Windows LS32B30 anunció como máximo 1920×1080; tampoco acredita salida física
4K. Esta ejecución supera el bloqueo anterior de `Core Area`, pero no
identifica retrospectivamente su causa ni prueba que sea imposible repetirlo.

La sesión final de Rescue Linux con esa build duró 1904,4 s, incluidos 1800 s
tras entrar en la escena, y terminó por `SDL_QUIT` con código 0. Recibió 117
pulsaciones direccionales y 29 confirmaciones. Hubo nuevas rondas tras
`Try Again`, por lo que es estabilidad de sesión activa, no una partida
individual de 30 minutos. El último registro de audio midió 114 014 VI,
114 010 tareas RSP de audio y 6 vaciados de la cola de software; no registró
un fatal. Evidencia privada:
`.local/m5/m2-long-rescue-final-20260930T104842Z/summary.json`.

Con la misma build, Time Trial Linux llegó al tablero con contador 4:54;
al dejarlo avanzar perdió sus tres vidas y mostró `TOTALS`. La confirmación
volvió al menú y el proceso cerró con código 0. Cubre entrada, juego y ruta
de derrota/resultado, pero no completar los cinco minutos de supervivencia.
Evidencia privada: `.local/m5/time-trial-full-20260930T0440/`.

La suite Python actual pasó 161/161 tests. La compilación completa de un
directorio CMake auxiliar de M5 se detuvo en `microcode_live_test` porque ese
entorno no tiene `X11/X.h`; su ejecución CTest parcial tampoco constituye una
suite verde. El CTest 40/40 anterior corresponde a la build anterior al último
parche VS. Las pruebas Python del parche RT64 y la ejecución real Linux/Windows
son la evidencia actual de esa corrección; resta repetir la suite CMake completa
en un entorno con las cabeceras de desarrollo correspondientes.

Una nueva matriz Linux sobre esa build completó 9 de 20 casos de menú
(`original` y `60`, cinco resoluciones, dos aspectos) sin fallos; abarcó los
cinco presets en 4:3/original y cuatro en 4:3/60. Se interrumpió al pedir el
usuario encargarse de las pruebas visuales personalmente, antes de 16:9 y de
terminar la combinación 2160p/60. Las capturas de 1080p muestran las medallas
translúcidas completas. Las primeras 500 tramas de la traza canónica fueron
idénticas entre 240p y 2160p en 4:3/original. Evidencia privada:
`.local/m5/matrix-linux-final-20260930T112046Z/summary.json`.

El primer caso del runner persistente Windows de la build `7c87593` llegó
al tablero Hide tras menú → Single → Hide. La transición pasó de `gate=0`
a `-1`, registró 16:9 efectivo, font de Training y ningún fatal; el proceso
estuvo vivo hasta pedir cierre a los 559,93 s. El runner registró
`forced_close=false` pero `exit_code=null`; por ello se deja el código de
salida como pendiente. Puzzle, Lines y Time Trial no se iniciaron en Windows
tras el cambio a revisión manual. Evidencia privada:
`.local/m5/windows-modes-7c87593-hide-puzzle-01/`.

## Medallas del menú tras volver de Rescue

Una captura FFV1 de la región derecha del menú en Xvfb/Lavapipe confirmó
parpadeo tras Rescue → Pause → Exit: 300 cuadros nominales a 60 fps en cinco
segundos, 108 cuadros visuales únicos y 59 cambios entre medallas magenta
fuertes y tenues. El menú inicial con el mismo binario no mostró alternancia.
La traza del invitado encontró que los 14 quads de medallas escriben el alfa
dinámico sólo en el vértice 1; los otros tres conservaban valores de la arena
de vértices reutilizada después del juego. RT64 interpola alfa incluso con
sombreado plano. Un primer parche que consultaba el puntero de arena actual
no resolvió el problema, porque el display list enviado podía referirse al
buffer anterior. El parche final toma las 14 direcciones `G_VTX` del display
list y, bajo la firma exacta de material y triángulos de medallas, pone a cero
los tres alfa no inicializados durante el procesamiento y restaura la memoria
al terminar. Mantiene el alfa dinámico del vértice 1 y el aspecto inicial.

Con build Linux normal y sonda RT64 desactivada, SHA-256
`86cd6a0ae27a1da649e72ee984c27ae26ac4cb534899fa2a1292a1b0f84acbfd`,
el mismo recorrido terminó con código 0: 300 capturas, 90 cuadros únicos,
magenta entre 1175 y 2353 píxeles y **cero** alternancias fuertes/tenues.
Antes eran 59; la prueba sintética de firma, direcciones y restauración pasó.
El capturador repite cuadros porque Lavapipe presenta menos de 60 cuadros
únicos por segundo, así que esta evidencia acredita los cuadros observados en
Linux virtual, no una garantía sobre todas las GPU o sobre Windows. Evidencia
privada anterior: `.local/m5/rescue-hud-probe-20260930T132708Z/`;
posterior: `.local/m5/rescue-hud-probe-20260930T141233Z/`.

## Requisito para el release: importación única de ROM

La primera ejecución debe aceptar una ROM aportada por el usuario, reconocer
la revisión soportada, normalizarla y guardar de forma atómica los datos que
necesita el runtime en una ubicación local privada. Los arranques posteriores
deben encontrar esos datos sin volver a abrir el selector, incluso si se movió
el archivo original. El runtime actual consume bytes de la ROM completa
(incluidas lecturas PIO y DMA), por lo que el formato inicial del caché debe
preservar la imagen normalizada íntegra; extraer solo gráficos o audio no basta.

Antes de publicar: validar tamaño e identidad del caché al leerlo; recuperar
una importación interrumpida o corrupta pidiendo una ROM válida; admitir una
ubicación de datos junto al ejecutable cuando sea escribible y usar la carpeta
de datos del usuario en instalaciones de solo lectura; conservar guardados y
ajustes existentes; probar el segundo arranque sin la ROM original en Linux y
Windows. El empaquetado debe excluir ROM, caché, activos extraídos y datos de
prueba del repositorio y de los artefactos públicos.

Implementación candidata Linux (2026-09-30): el arranque acepta z64, v64,
n64 o ZIP, valida SHA-256 de NTPE rev0 y escribe la imagen z64 normalizada
en `data/tetrisphere-us-rev0/rom.z64` junto al ejecutable. Si esa ubicación
no se puede escribir, prueba `SDL_GetPrefPath("Tetrisphere", "Tetrisphere")`.
`TETRISPHERE_DATA_DIR` aísla el caché en pruebas. El arranque sin argumento
comprueba tamaño e identidad antes de usarlo y solo abre el selector cuando
falta o es inválido. El fichero y su directorio quedan privados en POSIX.
Una prueba Linux bajo Xvfb importó un ZIP, retiró la copia de prueba y arrancó
sin argumento con el mismo hash validado; otra comprobó la ubicación junto
al ejecutable sin inicializar SDL antes de resolverla. Falta una prueba del
flujo real en Windows y auditar el paquete del futuro release. Los archivos
`RUN.txt` de los paquetes M1 históricos describen esas builds, que todavía
exigían pasar la ROM al arrancar.

## Tablero: recorte de piezas al girar (pospuesto)

El video facilitado por el usuario muestra segmentos que desaparecen en la
silueta de la esfera al girar, lejos del borde del viewport. El juego genera
una ventana móvil de 15×15 celdas con una malla de 16×16 vértices; los
recorridos, normales y buffers están dimensionados de acuerdo con ella. La
observación es compatible con ese límite, pero aún falta una traza que
identifique el descarte exacto de cada pieza. Ampliar los bucles sin rediseñar
esas estructuras puede leer fuera de tablas o alterar el orden de dibujo y la
lógica visual. Por decisión del usuario, se deja para un fix posterior salvo
que aparezca una solución pequeña y verificablemente segura. No se cambia el
render del tablero en M5 por esta observación.

## Prueba manual de Rescue: transiciones del HUD y audio

Los videos del usuario `2026-09-30 14-33-14.mp4` y
`2026-09-30 14-33-34.mp4` muestran que el HUD vuelve momentáneamente a su
posición 4:3 al pausar y al perder corazones. Son rutas distintas.

En la pérdida de corazones, el efecto añade proyecciones y desplaza el
indicador derecho de índice 7 a 8 o 9; además, un rectángulo del HUD puede
desaparecer durante unos cuadros. Una candidata identificó la proyección
ortográfica por rectángulo y viewport, manteniendo las firmas de Rescue,
esfera y scissor. Una captura privada Linux de cinco segundos,
`.local/m5/rescue-hud-probe-20260930T210723Z/04-heart-loss-top.mkv`, registra
150 cuadros a 30 fps durante el descenso de corazones: Rescue permaneció en
x=180 y el indicador azul derecho en x=1494 en los 150 cuadros. Esta prueba
no cubre todas las animaciones posibles ni Windows.

La pausa toma otra ruta: `func_8006F894` copia 320×240×2 bytes del framebuffer
a una textura en RAM (observada en 0x80166F20). RT64 lee esos 40 mosaicos sin
vínculo con el framebuffer ancho. Como la lectura nativa a RAM se compone en
4:3 cada cuadro, la captura contiene el HUD centrado aunque la presentación
normal ya lo haya anclado en 16:9. El activo que empieza en 0x8018C720 es un
recurso del menú, no una segunda captura. La prueba roja privada
`.local/m5/pause_snapshot_provenance_check.py` confirma 40/40 mosaicos con
`tile_copy_used=false`. La corrección fiel requiere congelar la composición
GPU ancha de ese cuadro y asociarla sólo a la textura de transición; no se
considera verificada hasta probar pausa, retorno y repetición en ambos
aspectos.

Para el crackling, las métricas históricas registran aproximadamente 19
rechazos de DMA cada diez segundos en Linux a 44,1 kHz y 14 en Windows a
40 kHz. El juego no reintenta esos bloques rechazados. El feedback anterior
informaba sólo el primer bloque pendiente; una simulación con la fórmula de
DMA del invitado reprodujo pérdidas. El ajuste en revisión informa la cola
SDL completa menos un callback, acota AI_LEN al presupuesto seguro del
invitado, usa una cola host de ocho bloques y espera de forma acotada sólo si
se llena excepcionalmente. Las pruebas de modelo de 100 segundos a 30, 40 y
44,1 kHz, con variación de callbacks, dieron cero pérdidas y esperas, con
latencia mediana estimada de 74, 57 y 55 ms respectivamente. En la build
privada `.local/m5/audio-feedback-build/` pasaron 167 pruebas Python y 42
CTests. Xvfb/Lavapipe sólo avanzó tres presentaciones tanto con el binario
nuevo como con el anterior; no proporcionó métricas de juego ni una prueba
audible. En la revisión manual Linux del 2026-09-30, el usuario indicó que el
audio ya suena prácticamente perfecto. Los registros de esas sesiones muestran
cero rechazos de la cola AI; la percepción en Windows sigue sin comprobarse
con la build nueva.

Esa misma revisión descubrió una regresión visual grave en la primera
implementación de la captura ancha de pausa. Los videos privados
`2026-09-30 16-27-23.mp4`, `16-27-29.mp4`, `16-27-48.mp4`, `16-27-59.mp4` y
`16-28-12.mp4` muestran mosaicos opacos grandes durante transiciones, además
de un recentrado del HUD durante puntuación. La captura se aplicaba después
de que RT64 calculaba la geometría de los mosaicos y también se activaba en
transiciones ajenas a Rescue. Las capturas automáticas anteriores mostraron
la pausa estable, pero omitieron los cuadros de entrada y salida; no se deben
usar para declarar esta ruta correcta. La corrección y la nueva validación
visual están en curso.

La traza de puntuación posterior descartó la hipótesis de una segunda
captura: no hubo llamadas al hook, capturas ni aplicaciones de mosaicos
durante esa animación. La proyección que contiene las anclas de Rescue pasó
del índice 4 al 5 por una proyección adicional antes del mundo; el
desplazamiento omitido fue de 192 píxeles. Una candidata buscaba la tríada
mundo, piezas siguientes y HUD por su contenido y usaba sus índices reales,
con rechazo de coincidencias ambiguas. La prueba sintética incluía el
indicador derecho en índice 10; la revisión manual posterior mostró que esa
regla seguía sin cubrir todas las transiciones.

En la build normal Linux SHA-256
`131271e7b87e5eb8bad8f6caab63c6f440a7ed4eed9249e5cac335d1d3bedf79`,
la captura de pausa se sustituye antes de calcular la geometría. Una prueba
A/B de 60 fps mantuvo el marcador derecho alrededor de x=1558 en entrada,
Continue y segunda pausa; sin la sustitución saltó alrededor de x=1369.
Exit regresó al menú visible. Pasaron 172 pruebas Python y la compilación
normal. La revisión manual posterior en GPU real confirmó que el HUD principal
mejoró, pero encontró un salto breve al puntuar y que la calavera de pieza
incorrecta aparece en la posición anterior de la UI. La puntuación y el
efecto de error siguen abiertos; no atribuirles cierre con las pruebas
sintéticas ni con la pausa automatizada.

Los videos posteriores `2026-09-30 17-17-49.mp4`, `17-18-10.mp4` y
`17-18-26.mp4` añaden un salto breve de Rescue al sumar puntos, una calavera
desalineada al perder corazón y un retorno persistente del HUD a 4:3 al
terminar el nivel. El problema no está limitado a una transición: el selector
por secuencia de proyecciones queda invalidado cuando el juego recompone sus
capas. El usuario pidió una solución determinista compartida por los modos.

Se diseñó transporte de procedencia desde los productores originales de UI
hasta cada draw call RT64, con anclaje por función semántica y región de
jugador, independiente del orden de proyecciones. El inventario privado
`.local/m5/ui_producer_inventory.json` registra las 87 llamadas directas del
productor principal y 117 con helpers. El análisis posterior del flujo de
control permite clasificar con confianza 24 de las principales como izquierda,
20 derecha y 25 centro; 18 conservan su posición nativa, incluidas llamadas
compartidas con Vs. Ocho llamadas derechas del
modo 4 sólo se etiquetan cuando el contador de jugadores es uno, porque el
bloque original no tiene esa condición. La misma textura se utiliza en
el HUD lateral y en resultados
centrados, así que una regla global basada solo en textura o posición no es
segura. El diseño y el plan están en `docs/superpowers/specs/2026-09-30-wide-hud-anchors.md`
y `docs/superpowers/plans/2026-09-30-wide-hud-anchors.md`. No se acredita
cobertura de todos los modos hasta cerrar el inventario y probar el transporte.

La traza posterior aisló el salto intermitente de Rescue: los productores
preparan dos buffers de listas gráficas antes de que RT64 consuma el primero.
En la sesión privada `combo-hud-probe-20261001T001148Z`, la OSTask 3180
recibió 11 rangos etiquetados y el título tuvo UID 1; la OSTask 3181, cuyo
buffer ya tenía otros 11 rangos pendientes, recibió cero y el título tuvo
UID 0. La cola borraba todos los rangos después de cada OSTask. La corrección
debe conservar los rangos del segundo buffer y rechazar etiquetas obsoletas
cuando se reutilice una dirección. Esa sesión confirma la causa, no todavía
el arreglo visual.

El análisis de código identificó los sprites de corazón y calavera en
`func_80078BA8` (JAL 0x80078C70, 0x80078CE8 y 0x80078D48) y el modelo de
transición en `func_800C3CEC` (JAL 0x800C3DC0). El inicializador coloca el
grupo 1P en x=35/50/65; esos productores también se usan en Vs. Sus hooks
requieren el gate de un jugador y modo distinto de 3. El manifiesto revisado
contiene 69 productores principales y estos cuatro productores adicionales;
la comprobación contra la ROM NTPE rev0 validó esos 146 hooks iniciales.
La comparación A/B privada posterior mostró el modelo de pérdida junto a
los corazones tras aplicar su anclaje; resta la revisión manual final.

Las ocho llamadas del bloque del mando de modo 3, antes indeterminadas,
también se clasificaron como `center`: sus posiciones permanecen fijas en
el centro y `func_80074CD0` cambia únicamente el canal alpha al pulsar o
liberar botones. Esto evita trasladar las luces del mando como si fueran
indicadores laterales.

La cola revisada conserva hasta dos OSTask intermedias y compara una huella
de los bytes de cada rango antes de aplicar la etiqueta. La toma ocurre antes
de los parches temporales de display list. En la build de diagnóstico posterior
el modelo de pérdida recibió UID 1 en una proyección Perspective; la política
se amplió a ese tipo por draw call. La build normal Linux en
`.local/m5/ui-tag-73-normal` compiló y pasó 183/183 pruebas Python y 42/42
CTest. Una captura privada de pausa y Continue mostró el título anclado en
los cuadros visibles y las dos capturas de GPU correctas; aún falta la
revisión manual del usuario de puntos, calavera y fin de nivel en pantalla
real. Las pruebas automáticas no acreditan por sí solas Windows ni 4K físico.

La revisión manual posterior confirmó que el salto del HUD desapareció,
pero reveló grupos separados: las piezas siguientes quedaron en x de 4:3,
al igual que el contador, ondas y gema azules de la derecha. La captura
4:3 privada `rescue-aspect-20260930T061001Z/rescue-43/rescue-2.png` permite
comparar estos grupos con Rescue/corazones y con el marco dorado derecho.
La causa es que esos elementos se dibujan mediante helpers fuera de las 87
llamadas directas clasificadas. El manifiesto ahora incluye cinco scopes
auxiliares validados contra la ROM: el preview izquierdo completo en
`0x800CF8A8 → 0x800A5BB4` y cuatro derechos en `0x80076ED8 → 0x80078DA0`,
`0x80076F64 → 0x80078514`, `0x80077470/0x800774E0 → 0x8007570C`.
El helper `0x80076ED8` dibuja el relleno de la gema azul: una traza de
workload 3540 le asignó UID 0 mientras su marco contiguo tenía UID 2,
confirmando la separación exacta de la captura. Los cinco scopes se
restringen a un jugador; el de previews excluye además el modo 3. La nueva
configuración suma 156 hooks verificados. La build y revisión visual están
en curso; la candidata anterior no cierra
el problema de composición.

La auditoría estática posterior separó los HUD por valor de modo y número de
jugadores. En la ruta 1P de modos 2 y 5, los previews, corazones y calaveras
comparten anclaje izquierdo; gema, cuenco, ondas y reloj comparten el derecho.
El modo 4 omite previews/corazones/gema/ondas y usa su propio panel derecho
de ocho productores más el reloj. El modo 3 usa panel, mando y luces centrales;
los helpers de Vs se llaman desde otros PCs por jugador y permanecen nativos.
El modelo de resultado de modos 2/5 permanece centrado. Las ramas que
sustentan esta separación están en 0x8007647C, 0x80076E08–0x80076E30 y
0x800A5C0C. La matriz de capturas 4:3/16:9 sigue en preparación; esta
clasificación no sustituye su revisión visual.

La build normal Linux de 78 scopes tiene SHA-256
`42f9f194a3acb56c7da167709bd1967ca538d2b66326d5a89d95c110c5a28a02`.
Pasó 185/185 pruebas Python y 42/42 CTests. Una captura privada de Rescue
comparada con 4:3 y con la candidata de 73 scopes muestra los previews
aproximadamente 190 píxeles más a la izquierda y el contador, la gema y
las ondas aproximadamente 190 píxeles más a la derecha, junto a sus marcos.
El usuario realizará la comparación visual de los demás modos; esas
pruebas todavía no se acreditan aquí. La candidata anterior de 73 scopes
queda supersedida para revisión manual.

## Revisión visual posterior: Puzzle, pausa y Multi

La revisión del usuario de la build de 78 scopes señaló una franja congelada
al pie de Puzzle, deformación de la imagen bajo pausa en Time Trial y Vs CPU,
medallas que parpadean al elegir Audio desde pausa y un posible botón erróneo
en las instrucciones de Multi. El zoom de la esfera en 16:9 se difiere:
requiere separar proyecciones de esfera y HUD y comprobar que el cursor siga
señalando la pieza correcta; no es un ajuste simple y seguro de esta fase.

En Puzzle el fallo también existía en 4:3. Una traza real privada con la
build `ui-tag-78-probe` registró viewport y scissor de 320×240, pero
`target_height=219` durante el tablero. Dos capturas de 320×240 en
`/tmp/tetrisphere-puzzle-interactive/` mostraron cambios en las filas 0–218
y ningún cambio en las 219–239. La política acotada
`full_frame_target_height` conserva las 240 filas VI si el buffer de color
coincide con VI 320×240, una proyección perspectiva usa scissor completo y
el alto detectado está entre 216 y 239. La build privada `full-frame-probe`
dio `target_height=240`; en `/tmp/tetrisphere-puzzle-after-43/` y
`/tmp/tetrisphere-puzzle-after-169/` las filas 219–239 cambiaron entre
cuadros y el fondo se ve continuo en 4:3 y 16:9. La segunda captura mide
427×240. Esta evidencia acredita presentación, no lecturas de framebuffer o
copias posteriores de esas filas; `drawColorRect` conserva su extensión
original. La política puede actuar también en otra escena 3D de pantalla
completa con la misma condición, por diseño.

La captura ancha de pausa tenía un filtro exclusivo de Rescue. En una traza
de Training, otro modo de juego, el mismo grid de cuarenta mosaicos llegó
con `source_rescue=false`, `writer_match=true` y antes fue rechazado. La
corrección acepta cualquier fuente de juego con escritor verificado y aspecto
16:9; el hook invitado se limita a `scene_gate=-1`. Una prueba privada de
Training registró captura y aplicación de mosaicos. La composición original
del marco y mascota de pausa es igual en una captura 4:3 y otra 16:9; no se
desplazó. En Time Trial se comparó la misma pausa con el mismo binario:
`.local/m5/pause-compare-e30fafd/time-43-pause.png` conserva el recorte
original del marco y `.local/m5/pause-compare-e30fafd/time-169-pause.png`
muestra esa composición trasladada al centro ancho, con captura 16:9
registrada. En Vs CPU, `.local/m5/pause-compare-e30fafd/vs-cpu-169-pause.png`
contiene la pausa y la traza registra captura ancha; el siguiente tap reanudó la
partida. También allí el marco visible coincide con la composición 4:3.
Queda pendiente la revisión en GPU real.

La ruta Rescue → pausa → Audio se ejecutó en la build Linux de hash
`7b50c720a9fc9ccdd4b4b2ac8b04bb2ee68be6105993700a220ba0924d6015fb`.
Una captura privada de cinco segundos a 60 fps, en
`.local/m5/rescue-hud-probe-20261001T031856Z/`, conservó 2160 píxeles
dorados de la medalla central en cada uno de sus 300 cuadros. No se reprodujo
su desaparición en Xvfb/Lavapipe; aún debe comprobarse en GPU real.

La revisión `e30fafd` compiló como Windows Release nativo en el equipo remoto:
el recibo privado `.local/m5/windows-build-e30fafd-receipt.json` y el hash
independiente del ejecutable coinciden en
`4b87a99504d1d8a861f924027769c1fe7ac10804f536a9d2cd4da99f2a957db7`.
La fuente remota estaba limpia, el manifiesto de generación coincidió 7/7
entradas y 24/24 salidas y las DLL requeridas estaban presentes. No se
ejecutó la build Windows: la prueba visual y funcional nativa queda pendiente.

La instrucción «MULTI / DIG THROUGH THE PIECES…» es del subtipo Multi de
Hide & Seek en Single, no del menú VS de dos jugadores. Su cadena original
pide A de N64 y el hook específico `0x800EFB9C` la clasifica como confirmar.
Con teclado Z produjo la entrada A de N64 (`0x8000`) y el renderer registró
una variante de glifo Z en Training; la ruta automática hacia Hide & Seek →
Multi acabó en Training antes de mostrar la instrucción. Las capturas de
configuración VS con dos mandos virtuales muestran A select / B back,
coherente con las acciones Xbox. El prompt y la acción de la instrucción
Multi concreta siguen pendientes de cotejar en pantalla; no se cambió sin
una discrepancia reproducible.

El usuario mostró después la instrucción real de Hide & Seek → Multi con
Xbox y teclado: la A parecía una P/F y la Z quedaba incompleta. Los logs de
esa sesión registraron variantes para Xbox A y teclado Z del font Hide exacto
(`0x8ED3320BEE952129`), así que el reemplazo sí se ejecutó. El invitado
dibuja A/B en sólo 11 columnas y C en 10, aunque la textura reserva 20 por
carácter. La versión anterior centraba el glifo de 10 píxeles en la celda de
20 y el renderer recortaba su lado derecho. `821cf67` conserva la limpieza
completa de la celda y centra el glifo dentro del ancho que se dibuja. La
regresión de raster falló antes del cambio y luego pasó para Xbox A, cruz de
PlayStation, B de Nintendo y Z de teclado; la build Linux pasó 42/42 CTests.
El usuario confirmó después de probar esta nueva build en Linux que el aviso
«se ve bien». Esa aceptación visual corresponde a Linux; Windows conserva
pendiente la prueba de ejecución del binario actualizado.
La misma fuente `821cf67` compiló nativamente como Windows Release en el PC:
el recibo privado `.local/m5/windows-build-821cf67-receipt.json` y el hash
independiente del ejecutable coinciden en
`fbc9619211bfdb8a45cd97cf6d9ca180cf4928ee5ebf9331dfe723d5e8f3c489`.
La fuente remota estaba limpia y se verificaron las tres DLL requeridas. No
se ejecutó el juego en Windows ni se acreditó allí el aspecto del glifo.

## Revisión manual Windows de la build actual

El 2026-10-01 se inició el ejecutable Windows de `821cf67` en la sesión
interactiva, con ajuste 16:9/1080p y datos de prueba aislados. El hash del
ejecutable coincidió con el recibo anterior. El usuario reportó que pasaron
todos los casos solicitados: HUD de Rescue al puntuar, perder un corazón y
pausar; regreso por Audio y Exit sin parpadeo de medallas; franja inferior de
Puzzle animada; pausa de Time Trial y Vs CPU sin deformación; aviso de
Hide & Seek → Multi legible con mando Xbox; y partida Vs Player con dos I-PAC
sin bloqueo. Es una aceptación visual y funcional humana de esos recorridos,
no una medición automatizada de cada cuadro. La salida física 3840×2160 en
ambos sistemas y la beta empaquetada siguen pendientes para 1.0.
