# M3 — mapa provisional de estado NTPE rev0

Investigación estática del C recompilado de NTPE rev0, 2026-09-29. Las
direcciones y operaciones indicadas se verificaron en el código generado;
su equivalencia con la presentación y con ares requiere trazas runtime.

| Campo observado | Dirección o ruta | Evidencia estática | Pendiente |
|---|---|---|---|
| Contexto P1 / P2 | `0x80110220` / `0x80113488` | `func_800AF09C(1/2)` selecciona contexto | Confirmar en partida de dos jugadores |
| Contexto activo | puntero `0x8013DD00` | cambia con selección de jugador | Correlacionar con entrada consumida |
| RNG global | u32 `0x800E2AA0` | `func_80081E90`, escritura `0x80081FF0` | Controlar semilla en fixture |
| Semilla P1 / P2 | contexto `+0x239C`: `0x801125BC` / `0x80115824` | `func_8008A6E4` intercambia RNG global y semilla de jugador | Comparar antes y después de paso lógico |
| Acumulador de puntos | contexto `+0x2AF8` | incrementos, incluido +100 en `0x800C7C54` | Escala visible y modo |
| Valor animado del HUD | `0x800E241C` | se acerca al acumulador en `0x80077218..64` | Comparar cuando la animación termine |
| Reloj candidato | s32 contexto `+0x2B0C` | modo numérico 2 lo inicia en `0x468B`; resta delta y limita a cero en `0x800CED54..64` | Relación con tiempo real, pausa y display |
| Delta candidato | u32 `0x80160C60` | resta del reloj anterior | Semántica temporal |
| Bases de tablero P1/P2 | `0x80116BF0` / `0x8014D268` | selección por `func_800AF09C` y getter `func_8008A7A0` | Dimensiones y valores válidos |

El getter de celdas lee un entero con signo de 16 bits en
`base + 2*(x&31) + 64*(y&31) + 2048*(int16)z`. Las coordenadas horizontales
envuelven 32×32. Varios escritores limitan `z` a 0..7, pero otro acepta 8;
por ello una captura de ocho capas no demuestra estado exhaustivo.

El reloj candidato se inicializa a 18 059 en modo 2. El HUD lee el mismo
campo, divide por 60 y construye texto con dos puntos. El fin se invoca al
llegar a cero bajo condiciones adicionales. Esto identifica una cuenta atrás,
pero aún no demuestra la duración en segundos ni el comportamiento en pausa.

La puntuación formateada multiplica el acumulador por diez en una ruta, y el
HUD animado puede retrasarse respecto del acumulador. Mantener los dos campos
separados en las trazas; no llamar puntuación visible a `+0x2AF8` hasta medir.

`func_8006E98C` drena el anillo de entrada y llama `func_8006E418` en
`0x8006EA1C` con dos palabras crudas por puerto desde `0x800E1720` y
`0x800E1CC0`. El cursor de lectura avanza en `0x8006EA78` y el drenaje sale
por `0x8006EA9C`. El segundo valor no se etiqueta analógico sin comparación
con ares. Las trazas deben registrar entrada consumida aquí, no sólo el estado
SDL previo.

Una prueba Linux privada con una pulsación dirigida y build M3 registró 4096
drenajes, 994 con eventos consumidos. Al pulsar Confirm en teclado, el puerto 0
tuvo `raw_word0=0x8000` en el drenaje 3151 y `raw_word1=0x8000` en el 3152;
ambos volvieron a cero después. El log de la misma ejecución muestra
`training_font_variant` para el botón Z y conservó intacta la fuente original.
Esto confirma el hook y la lectura del botón en la lógica; los nombres
semánticos de las dos palabras siguen pendientes de contraste con ares y P2.

## Experimentos de confirmación

1. Comparar dos arranques con ROM, EEPROM, configuración y entradas idénticas;
   controlar RNG global y ambas semillas de jugador en el fixture.
2. En Time Trial, correlacionar `+0x2B0C`, delta, texto del reloj, pausa,
   reanudación y fin natural.
3. Tras una eliminación, correlacionar `+0x2AF8`, `0x800E241C` y HUD hasta
   estabilizarse.
4. En VS, mover sólo P1 y luego sólo P2 para confirmar contextos y bases de
   tablero. Registrar valores crudos de las nueve capas antes de atribuirles
   significado lógico.
