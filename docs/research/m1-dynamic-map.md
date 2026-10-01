# M1: contraste dinámico del mapa

Fecha: 2026-09-28. Alcance: NTPE rev0 normalizada, ares 148 y GDB RSP en
solo lectura. Este documento resume direcciones, conteos y procedencia; las
capturas JSONL, imágenes, estados y ROM permanecen bajo `.local/m1/` o
`.local/m0/` y no se distribuyen.

## Entorno y reproducción

Se usó el Flatpak `dev.ares.ares` versión 148, commit de paquete
`5b6ccf0aef391c237a34e41cf14da438446aa454371408e3c0d418f818132a77`,
runtime `org.freedesktop.Platform/x86_64/25.08`. El perfil público
`config/ares148-reference.bml` tuvo SHA-256
`cdc0128e1691c45e2c339d51718d3a6b53c11417f4f099ce610bfd2e4dc6cfb1`.
Se reutilizó la asignación privada de teclado y guardados de M0 sin modificar
su evidencia. La configuración visible fue PixelAccuracy, aspecto estándar,
VI activo, 48 kHz/20 ms, sin Expansion Pak, runahead ni rewind.

Comandos públicos, con ares ya iniciado por el protocolo M0, DebugServer en
`[::1]:19123` y la ROM validada fuera de Git:

```sh
python3 tools/analysis/trace_runtime.py collect \
  --rom .local/m0/tetrisphere-us.z64 \
  --tracepoints config/recomp/tracepoints.json \
  --output .local/m1/traces/final-reset-a.jsonl \
  --seconds 45 --max-events 100

python3 tools/analysis/trace_runtime.py collect \
  --rom .local/m0/tetrisphere-us.z64 \
  --tracepoints config/recomp/tracepoints.json \
  --output .local/m1/traces/final-playable-a.jsonl \
  --seconds 100 --max-events 100

python3 tools/analysis/trace_runtime.py merge \
  --static-map .local/m1/analysis/static-map.json \
  --capture .local/m1/traces/final-reset-a.jsonl \
  --capture .local/m1/traces/final-playable-a.jsonl \
  --output .local/m1/traces/final-merged.json
```

Durante la primera ventana se abrió **Nintendo 64 → Reset** después de armar
los puntos. Durante la segunda se pasó del título al menú, se eligió
Single/Rescue 1:1, se llegó al tablero y se mantuvo Right durante dos segundos.
La imagen privada final muestra esfera, piezas y HUD de Rescue; los hits de
entrada/actualización abajo proceden de la misma ventana. Una ejecución anterior
de 120 s repitió los mismos hits de escena y callback. Dos resets acotados
repitieron entrada, zero-fill, handoff e inicio posterior.

Los SHA-256 de la evidencia final privada fueron:

| Archivo privado | SHA-256 |
|---|---|
| `final-reset-a.jsonl` | `4b7b23238ab159dfb90e86c61073f8ef656652898db97f6d157bf70c05b78e15` |
| `final-playable-a.jsonl` | `ce2b0b45434c03744f088c3de5616b6a731dcf4f44f3cd02109cfed7f2185df4` |
| `final-merged.json` | `b7bed60898312be398ef4b984be765dcb8373d328c0959e7eb9ed513b943556c` |

## Observaciones

La fusión final contiene 106 eventos saneados. No contiene bytes de memoria ni
ROM, paquetes RSP, credenciales, guardados, audio o imagen.

| Categoría | Evidencia dinámica | Dictamen |
|---|---|---|
| Entrada | `0x80025c50`, 1 hit tras Reset | **Confirmada en ejecución.** |
| Limpieza inicial | `0x80025c60`, 1 hit | **Confirmada la ejecución** del bucle estático; el rango sigue sustentado por el análisis de instrucciones. |
| Handoff | `0x80025c80 → 0x80029510`, 1 hit; además `0x80029510`, 1 hit | **Confirmado** como salto indirecto constante, no llamada. |
| Callback temprano | `0x80026498 → 0x8007ef9c`, 8 hits en escena | **Confirmados sitio y destino observado.** No demuestra que sea el único callback posible. |
| Entrada de juego | `0x8006e418`, 2 hits | **Confirmada en Rescue jugable.** La etiqueta semántica sigue procediendo de M0. |
| Inicialización de modo | `0x8006ee6c`, 2 hits | **Confirmada en la transición a Rescue.** |
| Actualización de juego | `0x800b7e3c`, 2 hits | **Confirmada en Rescue jugable.** |
| Escrituras AI | 24 observaciones, sitio RSP aproximado `0x8007edd0` | **Confirmada ruta de envío AI.** Los valores leídos de `AI_DRAM_ADDR` son estado de registro y no se etiquetan como punteros `OSTask` ni como PCM demostrado. |
| DMA RSP | 32 observaciones en `0x800d1b9c` y 32 en `0x800d1c08` | **Confirmada actividad RSP.** Fuentes observadas: `0x80162820/60` y `0x800dd300/d3d0`; gráficos frente a audio queda **abierto** para Task 6. |
| PI/cargas posteriores | 0 hits en `PI_RD_LEN` | **Abierto.** El Reset de ares alcanzó la entrada, pero no expuso por GDB la transferencia IPL3 anterior; tampoco se observó DMA PI posterior en las ventanas. |
| Overlays | 0 rangos observados y 0 candidatos estáticos confirmados | **Abierto**, no “ausente”. |
| Compresión | ningún evento ni stream validado | **Abierto**; una traza sin hit no excluye codec propio o carga por otra ruta. |

ares informa watchpoints de MMIO al cliente y el PC leído puede representar el
punto observable del bloque/caller, no necesariamente la instrucción `sw`
exacta. Por eso los tres sitios AI/RSP anteriores son ubicaciones de envío
observadas y no límites de función ni nombres libultra. `SP_DRAM_ADDR` y
`AI_DRAM_ADDR` se guardan como `source_address`; no se falsean como descriptores
`OSTask`. Task 6 debe identificar las estructuras y microcódigos leyendo rangos
acotados en privado y contrastándolos con RT64/RSPRecomp.

## Contraste con el mapa estático

- La entrada, la limpieza y el handoff de Task 2 concuerdan con ejecución. No se
  modifican `sections.json` ni `symbols.toml`.
- La llamada indirecta estática `0x80026498` deja de tener destino totalmente
  desconocido: `0x8007ef9c` fue el único visto en estas dos escenas, pero otros
  destinos permanecen posibles.
- El handoff `0x80025c80` apareció en el conjunto estático como salto indirecto,
  no en `indirect_calls`; esa diferencia de categoría es esperada y queda
  **confirmada**, no se promociona como llamada.
- Los otros 137 candidatos de llamada indirecta y 198 saltos indirectos no
  observados permanecen **abiertos**. Pedir un breakpoint o no obtener un hit
  no los rechaza. La fusión clasifica por separado llamadas y saltos: el
  handoff es `jump` y el callback es `call`.
- La ausencia de PI/overlay observados no contradice el DMA IPL3 confirmado por
  estática: el servidor se arma después de que ares ha realizado esa fase. No se
  amplía ni reduce el único segmento publicado.
- El seed `0x80070300` rechazado en Task 2 no obtuvo nueva evidencia que revierta
  el dictamen; sigue rechazado como inicio independiente.

## Límites

Esta instrumentación no escribe memoria invitada. Los breakpoints ralentizan el
host y son evidencia de alcance, no de temporización. Los conteos son de ventanas
acotadas, no cobertura total del juego. La señal AI demuestra el camino de DMA,
no música audible ni fidelidad; la clasificación de tareas y comandos reales es
trabajo de Task 6. La escena ejecutada aquí pertenece a la referencia ares y no
sustituye la demostración nativa Linux/Windows de Tasks 7–8.
