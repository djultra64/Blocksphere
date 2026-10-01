# Matriz de contenido — M0

Fecha: 2026-09-27. Inventario de áreas de contenido para la referencia M0. Incluye rutas documentadas y escenas ejecutadas; la última columna conserva la cobertura exhaustiva de regresión M3. `ROM` significa etiqueta localizada en el dump validado, no función ejecutada. No se afirma haber completado todas las etapas o escuchado todas las pistas.

## Modos y rutas

| Elemento | Evidencia | Cobertura pendiente |
|---|---|---|
| Introducción / título / demo | Introducción y menú observados en ares | Ciclo completo, salto, música |
| Training | ROM `0xc7d44`; BASICS, ADVANCED, PUZZLE, BATTLE, RULES, LINES cerca de `0xc7f40` | Menú real: BASICS, ADVANCED, PUZZLE; las tres rutas abiertas. Advanced: Crystal Pieces, X-value, Gravity/Fuse Combo con demo y contador observados. BATTLE/RULES pertenecen al submenú VS; no son seis rutas iniciales de Training |
| Rescue | ROM `0xc7d50`; manual | Inicio/giro/pausa observados; progresión/final pendientes; manual describe 100 etapas |
| Hide & Seek | ROM `0xc7d58`; manual; sesión M0 | MULTI1:1 y DRILL1:2, instrucciones de descubrir imagen/buscar cinco taladros, tablero y pausa GOAL observados. Otras variantes y final: regresión M3 |
| Puzzle | ROM `0xc7d64`; manual | Puzzle1 resuelto: deslizar pieza inferior izquierda a la derecha y eliminar; transición a Puzzle2 observada. Reinicio/cámara pendientes; manual describe 100 puzzles |
| Time Trial | ROM `0xc7d6c`; manual | Inicio/reloj/pérdida de vidas/totales observados; ronda completa pendiente |
| Vs CPU | ROM `0xc7d78`; tablero VS y rival observados | Esferas/HUD, eliminación del rival y pausa observados; dificultades/finales pendientes |
| Vs Player | Ruta observada VS → BATTLE/RULES → 2 PLAYERS → nombres P1/P2 | Nombres/bots/core area y configuración pieces/layers/rounds accesibles; tablero de dos esferas y entrada por dos puertos observados. Mandos físicos/reconexión pendientes |
| Practice | ROM `0xc7d30`; manual | Menú observado: start/default/pieces (seis tipos), layers (4 inicial), core area (15 inicial); se modificó selección de piezas. Escena jugable, cambios de piezas, bad drops y pausa/salida observados; rangos completos pendientes |
| Lines | ROM `0xc7d80`; NEW NAME LINES ejecutado | Apareció nueva fila LINES en Single; reglas y tablero1:1 abiertos. No es un minijuego 2D inferido del nombre: usa esfera/núcleo. Finales: regresión M3 |

El [manual transcrito](https://www.world-of-nintendo.com/manuals/nintendo_64/tetrisphere.shtml) corresponde a la edición australiana `NUS-NTTP-AUS`. Sirve de pista documental; cada diferencia debe contrastarse con NTPE. Resumen documental: Rescue libera robots; Hide & Seek alterna objetivos; Puzzle limita acciones; Time Trial puntúa durante cinco minutos; VS enfrenta jugadores o CPU. El manual describe Training, Practice, opciones y progreso en cartucho.

## Opciones, contenido y desbloqueos

| Área | Evidencia en ROM | Verificación pendiente |
|---|---|---|
| AUDIO / DATA EDIT / SCORES / CONTROLS | `0xc7d88`–`0xc7da4`; cuatro pantallas abiertas | Inventario funcional debajo; límites, todas las pistas y borrado sobre copia: regresión M3 |
| Nombre, nivel, continuar y cargar | Etiquetas cerca de `0xc7dbc`, `0xc7f0c` | Seis slots; seleccionar slot abre LOAD GAME/DELETE; F persistió tras reinicio y se confirmó LOAD GAME. Límite de nombre/cartucho lleno pendientes |
| Robots | Rocket, Gear, Gyro, Stomp, Wheels, Turbine; Jak en `0xc744c` | Selección, estadísticas, disponibilidad y desbloqueos |
| Contenido especial | VORTEX y G'MEBOY cerca de `0xc7f8c` | Acceso y comportamiento, sin asumir que sean modos normales |
| Dificultad | EASY / NORMAL / HARD cerca de `0xc7b18` | Dónde se aplica y efecto real |
| Música y efectos | Etiquetas de música cerca de `0xc7b30` | Inventario audible completo, loops y transiciones |
| Cadenas, multiplicadores, magia y piezas especiales | Documentación, ROM y demos Advanced | Gravity/Fuse Combo, X2 y power ups observados en demo original; reglas/umbrales de magia y combinaciones exhaustivas: regresión M3 |
| Finales/créditos | CREDITS cerca de `0xc7f84` | Rutas que los activan, contenido tardío |

El inventario cataloga las familias y rutas conocidas; la ejecución exhaustiva de desbloqueos, niveles y contenido tardío requiere regresión M3. No se publican dumps de strings ni recursos extraídos.

### Opciones originales observadas

| Pantalla | Elementos disponibles y comportamiento observado |
|---|---|
| Audio | Balance music/fx, selección AUTO/RANDOM/CHOOSE y selector de pista; entrada inicial PROPHETIC - TITLE, cambio a AZULE LUX observado. Todos los loops y extremos del balance requieren regresión |
| Data Edit | Seis slots; F en slot1. A abre LOAD GAME/DELETE. Acción adicional «clear cartridge data». No se ejecutó el borrado sobre el fixture de progreso |
| Scores | Selector de categoría y cinco entradas de ranking visibles. Rescue muestra puntuación; Puzzle muestra tiempo. Los rankings completos por modo se conservan como regresión |
| Controls | Diagrama de MOVE/DROP/SLIDE/PAUSE/MAGIC; pantalla informativa del juego. Las asignaciones de teclado usadas en M0 pertenecen a ares |

La selección VS permite nombres y robots por jugador, core area, pieces, layers y rounds; el inicio observado usa 15 de core area y 4 layers. Practice expone start/default/pieces/layers/core area. Esas configuraciones forman parte del contenido original y deberán conservarse en el port aunque sus combinaciones completas se prueben en M3.

### Reglas y variantes catalogadas

El manual cataloga piezas normales, power pieces, wild card y piezas oscuras de VS; combos de 3–19, magia desde 20 y X-value hasta 20. Magia: Firecracker, Bundle O' Dynamite, Electro Magnet, Atom, Bomb, Ray Gun. Son umbrales/objetos documentados, no efectos ejecutados completos.

Robots: Rocket, Gear, Gyro, Stomp, Wheels y Turbine observados en selección; Jak también está catalogado en ROM/manual. Su disponibilidad concreta queda por contrastar; no se supone que esté bloqueado. El manual distingue speed y power; no se deduce el acceso a Jak de sus estadísticas.

## Guardado

El [catálogo técnico de Mupen64Plus](https://github.com/mupen64plus/mupen64plus-core/blob/b20b27ebf9e5b099a978e86dba609111dc98c837/data/mupen64plus.ini) identifica `SaveType=Eeprom 4KB`. Se conserva la nomenclatura del catálogo; capacidad efectiva, tamaño del archivo y protocolo se comprobarán en emulación y llamadas del juego. No se confunde una etiqueta de catálogo con una medición.

Rutas de la referencia prevista: `.local/m0/saves/`; capturas en `.local/m0/captures/`. La sesión controlada creó un archivo EEPROM de 512 bytes al registrar el nombre F. Capacidad medida: 4 Kbit. Comprobado: crear F → resolver Puzzle1 → llegar a Puzzle2 → salir normalmente → EEPROM512 bytes/hash → reiniciar proceso → slot1 F → LOAD GAME → continuar Puzzle nivel2. Pendiente para regresiones posteriores: segundo nombre y borrado sobre copia. Se observaron seis slots; falta comprobar seis progresos independientes, Controller Pak y expansiones. Los savestates privados pueden facilitar estados tardíos, pero no sustituyen la validación de progreso normal.

## Accesos especiales documentados para regresión

La [guía contemporánea de M. Stoler (1997)](https://gamefaqs.gamespot.com/n64/198946-tetrisphere/faqs/3332) registra nombres especiales en NEW NAME: CREDITS (créditos), LINES (minijuego), G + icono alienígena + MEBOY (música especial), y una secuencia de cinco iconos (planeta, nave, cohete, corazón, calavera) para selección de niveles. Menciona también VORTEX. Las etiquetas CREDITS/LINES/VORTEX y G'MEBOY están localizadas en la ROM objetivo; esto permite incluirlas en el inventario sin afirmar que se hayan ejecutado. La guía distingue el progreso obtenido con selección de nivel del progreso normal, por lo que esa ruta no servirá como prueba de guardado de campaña.

La misma fuente describe la combinación L + C-right + C-down para revelar iconos en el editor; su descripción inicial usa R, una inconsistencia que debe resolverse con la ROM. No se codifica una combinación supuesta en el port. El inventario conserva el acceso como pendiente de contraste funcional.

El aporte identificado de Baseman en [GameFAQs](https://gamefaqs.gamespot.com/n64/198946-tetrisphere/cheats) describe VORTEX como acceso a una animación en bucle mientras se mantiene Reset. Se cataloga como comportamiento de reset, no como modo normal ni desbloqueo de Jak. Falta contrastarlo en NTPE; no se aplica un reset para sustituir la prueba de guardado normal.
