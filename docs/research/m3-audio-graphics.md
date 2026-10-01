# M3 — audio y medallas del menú

Fecha: 2026-09-29. Código nativo probado en Linux y Windows: `52de8a1`.
El commit `6305e7a` sólo corrigió las
firmas `main` de tres pruebas SDL de Windows; no cambia el ejecutable probado.

## Audio

El VI y las tareas RSP de audio avanzan a 60 Hz. El dispositivo SDL obtuvo
44 100 Hz, estéreo S16 y bloques de 1024 fotogramas. Con la longitud AI cruda
y dos espacios de DMA, `osAiSetNextBuffer` rechazó 47 de 599 intentos durante
los primeros 10 segundos, entregó 397 552 fotogramas y observó 189 envíos
después de vaciarse la cola de software (`.local/m3/audio-metrics-04.out`).
Esto explica el riesgo de chasquidos y una tasa efectiva inferior a 44,1 kHz.

El código ahora consulta la longitud AI ajustada por N64ModernRuntime y
conserva hasta cuatro bloques DMA host antes de reportar cola llena. El
experimento de 110 segundos con esos ajustes (`audio-metrics-09.out`) no
observó colas vacías y entregó 4 850 512 fotogramas a los 110 segundos. La
build con esos valores predeterminados (`audio-metrics-10.out`) observó cuatro
colas vacías sólo durante el arranque, ninguna más entre 10 y 90 segundos,
y entregó 3 968 976 fotogramas a los 90 segundos. Las cifras son acumuladas:
incluyen el inicio del proceso, durante el cual SDL y el juego arrancan en
distintos momentos. La métrica de cola de software detecta riesgo de falta de
datos; no mide el búfer físico ni la calidad audible. En la escucha nativa
Linux, el usuario informó que el audio mejoró mucho y lo considera aceptable
para 1.0. Oyó pequeños errores al abrir la ventana y ocasionalmente después;
quedan registrados para revisión posterior. Esta valoración no acredita aún
la escucha de la build Windows.

`TETRISPHERE_AUDIO_METRICS=1` emite contadores cada 10 segundos. Las variables
`TETRISPHERE_AUDIO_FIFO_CAPACITY=2` y
`TETRISPHERE_AUDIO_LENGTH_MODE=raw` reproducen los valores anteriores para
comparación privada.

La prueba nativa Windows en sesión interactiva 1 (`windows-smoke-52de8a1`)
registró 3 085 568 fotogramas enviados a los 70 segundos y cinco colas
vacías durante el arranque, sin vacíos adicionales en la ventana registrada.
El recibo identifica el binario SHA-256
`00da55e734fde2c2829f8404c5f90a5f181070782126cc2da1f789b27dd24cb4`
y el build ID
`82665a2da179491ab257d8f3bcf71a064096e65c42d6bca4daf885991e9775d0`.

## Medallas

La captura RDRAM nativa privada `menu-visual-snapshot-02.rdram` contiene 18
triángulos `G_TRI1` cuyo selector de color plano es 1; en el menú, cada
medalla usa ese selector en uno de sus dos triángulos. La referencia ares
conserva los cuatro vértices originales. RT64 decodifica los tres índices de
`G_TRI1` pero omite el selector, dejando que el primer vértice determine el
color plano. El primer triángulo elige así un vértice de color cero y se
vuelve invisible. El segundo elige el vértice coloreado y sólo esa mitad se
ve. La [documentación original de `gSP1Triangle`](https://ultra64.ca/files/documentation/online-manuals/functions_reference_manual_2.0i/gsp/gSP1Triangle.html)
define los valores 0, 1 y 2 como selección del vértice para color plano.

`F3DFlatShadePatch` rota cíclicamente los tres índices antes de que RT64 lea
la lista y restaura la memoria invitada después. Conserva orientación,
interpolación y orden de los vértices, a la vez que coloca el vértice indicado
en la primera posición. La ejecución nativa registró 18 correcciones en el
menú y la captura privada `menu-visual-snapshot-02.png` muestra las medallas
completas. La captura nativa Windows privada
`windows-smoke-52de8a1/native-m3-menu.png` registró también 18 correcciones.
El usuario confirmó que las medallas se ven enteras en Linux. Sigue pendiente
la comparación visual humana en Windows sobre la build exacta.

## Pruebas afectadas

La build Linux del commit de código pasó 37/37 CTests y 154/154 pruebas Python.
Cuatro pruebas enfocadas de audio, controles y color plano pasaron en Windows
con el checkout exacto `6305e7a`; `audio_output_test`,
`controller_slot_test` y `controller_ports_test` requirieron `main(int,
char**)` para enlazar con SDL2main en MSVC. La DLL SDL2 de la build nativa se
copió junto a los ejecutables de pruebas antes de correr CTest.
