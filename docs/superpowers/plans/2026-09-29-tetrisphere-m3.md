# Tetrisphere M3 — paridad técnica de NTPE rev0

Fecha: 2026-09-29. Estado: plan de ejecución; ninguna prueba M3 se presume aprobada.
El usuario autorizó continuar hasta M3 después de cerrar M2. HITL-1 y HITL-2
siguen pendientes por separado. No avanzar a M4.

## Condiciones de entrada

Cerrar M2 con una build exacta de Linux y Windows, sesiones nativas de al menos
30 minutos, persistencia, controles dentro de las indicaciones originales,
paquetes sin ROM y revisión independiente. Conservar el commit de M2 como base
de comparación. Si un defecto de M2 se descubre durante M3, corregirlo y
revalidar las pruebas afectadas antes de atribuir cobertura a M3.

## Tareas ejecutables

1. **Dos puertos independientes.** Sustituir el `ControllerSlot` único por dos
   slots con identidad SDL estable. Exponer entrada, información de dispositivo,
   vibración y familia por jugador; no asignar el mismo mando a ambos. Pruebas
   RED→GREEN con dos mandos virtuales, alta/baja en distinto orden y
   reconexión sin intercambio de P1/P2. Confirmar con dos mandos físicos.
   Gestionar ambos puertos bajo un solo lock y publicar snapshots coherentes;
   `SDL_CONTROLLERDEVICEADDED.which` es índice de enumeración y los eventos
   de baja/botón usan instance ID. Al quitar P1, P2 conserva puerto. Un tercer
   mando no altera los ocupados. El teclado pertenece solo a P1 y
   `device_info(0)` debe ser coherente con su entrada. Registrar actividad
   útil por jugador, sin cambiar familia por mera conexión o ruido de eje.
2. **Observabilidad de la lógica.** Añadir hooks de N64Recomp en TOML y código
   host separado; regenerar C reproduciblemente. Grabar por tick la entrada
   consumida en `func_8006E418` y cursores de anillo `0x800E1710/14`, junto
   con límites de dispatch y delta. Confirmar con traza la semántica de los dos
   valores por puerto; una lectura SDL previa no equivale a entrada consumida.
3. **Semilla y estado canónico.** Investigar y controlar, en un fixture de QA,
   RNG global `0x800E2AA0` (`func_80081E90`) y semillas de jugador
   `contexto+0x239C`. Confirmar el campo de score `+0x2AF8`, profundidad y
   valores del tablero P1/P2 y el reloj de Time Trial antes de etiquetarlos.
   Serializar estado versionado por campo lógico, con hashes por subsistema y
   primer valor divergente. Excluir punteros, colas host y datos volátiles.
4. **Replay y referencia.** Ejecutar dos runs idénticos en Linux desde ROM,
   EEPROM inicial y configuración documentadas; demostrar igualdad y una prueba
   negativa que provoque divergencia. Repetir en Windows real desde la misma
   fuente. Comparar contra ares 148 con breakpoints equivalentes. Registrar
   diferencias de batching de actualización sin esconderlas alterando tiempo.
5. **Matriz completa de contenido.** Usar `docs/research/modes-matrix.md` como
   inventario. Registrar por Linux y Windows `pending/pass/fail`, build y
   evidencia para intro/demo, las tres rutas Training, Rescue, Hide & Seek,
   Puzzle, Time Trial completo, Vs CPU, Vs Player, Practice, Lines, opciones,
   nombres/slots, guardado/carga, robots, desbloqueos y finales aplicables.
   Cubrir progresión larga normal y estados tardíos mediante fixtures privados;
   accesos especiales no sustituyen progresión normal.
6. **Entrada, imagen y sonido.** Probar pausa, fin, reconexión y acciones
   simultáneas de dos jugadores. Extender indicaciones originales a los modos
   recorridos y verificar glifo más acción con Xbox, PlayStation y Switch
   físicos disponibles; registrar familias no disponibles. Comparar imagen,
   música, efectos y cadencia con la referencia M0 con tolerancias justificadas.
   Medir tres ventanas de 60 s tras calentamiento por escena y una ronda Time
   Trial completa sin pausa; separar VI, lógica, tareas gráficas y presentación.
7. **Cierre.** Corregir todo P0/P1 de lógica, progreso, audio o imagen base y
   fallos silenciosos. Ejecutar suites, recorridos y revisión independiente
   sobre los commits finales. Crear `docs/milestones/M3.md` con comandos,
   hashes, builds, matriz por plataforma, resultados y pendientes humanos;
   actualizar backlog y AGENTS.md. Preparar build y guion HITL-2 sin atribuir
   aprobación al usuario. Integrar localmente solo en rama inequívoca y probar
   el resultado integrado.

## Puntos de investigación antes de afirmar paridad

- `func_8006E98C` drena un anillo de 180 posiciones; las dos palabras de
  entrada por puerto están en `0x800E1720` y `0x800E1CC0`, con ocho bytes por
  tick. Hay que observar el segundo valor antes de llamarlo analógico.
- El getter de tablero `func_8008A7A0` lee `int16` desde una base activa; las
  bases candidatas P1/P2 son `0x80116BF0` y `0x8014D268`. Las dimensiones
  lógicas válidas todavía no están demostradas.
- `0x800E4478` cuenta pasos de contexto P1 y `0x800E4480` dispatches; ninguno
  debe etiquetarse como reloj universal. El temporizador visible de Time Trial
  no está identificado.
- La referencia M0 inventarió modos y menús, pero no aporta partidas tardías
  exhaustivas. Construir fixtures privados de cada ruta y contrastar contenido
  tardío sin convertir inferencias del manual en resultados ejecutados.

## Regla de evidencia

Cada resultado enlaza fuente/commit, build ID, ROM por hash, fixture, plataforma,
pasos y artefactos privados. Un test virtual no acredita mando físico; Wine no
acredita Windows real; una captura aislada no demuestra una acción ni una ruta
completa. HITL-2 es la revisión humana extensa de fidelidad, música, sensación
de control y dos jugadores sobre la build exacta conservada.
