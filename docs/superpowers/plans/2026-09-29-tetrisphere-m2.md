# Tetrisphere M2 — ejecución técnica

**Objetivo:** entregar 0.1 jugable, persistente y diagnosticable en Linux y Windows reales, conservando la cadencia y el encuadre originales. HITL-1 permanece pendiente; esta petición autoriza el trabajo técnico de M2, no una aprobación humana.

**Base:** `519d966` en `.worktrees/m1`; ROM NTPE rev0 validada; N64Recomp, N64ModernRuntime, RT64 y RSPRecomp fijados por `config/dependencies-m1.json`. La especificación vinculante es `docs/superpowers/specs/2026-09-27-tetrisphere-design.md` y el criterio M2 está en `docs/superpowers/plans/2026-09-27-tetrisphere-pc-port.md`.

**Decisión de presentación del usuario:** los botones y textos de control deben cambiar dentro de las indicaciones originales del juego. El panel ImGui actual, aunque muestre el botón correcto, no cumple el criterio. La investigación y los límites técnicos están en `docs/research/m2-native-prompts.md`. La petición del 2026-09-29 autorizó implementar esta corrección y completar M2; la autorización posterior amplió el trabajo técnico hasta M3. HITL-1 y HITL-2 siguen pendientes y separados.

## Tareas

- [ ] 1. Confirmar con trazas y mapa el mecanismo EEPROM de 4 Kbit y su uso durante partida; escribir primero pruebas de lectura, escritura y recuperación. Implementar almacenamiento versionado en directorio por usuario, commit atómico y copia recuperable. Probar fallos de escritura y reinicio de proceso.
- [ ] 2. Integrar importación local de ROM para z64/v64/n64 y ZIP local con validación de tamaño, revisión y hash; errores accionables, ninguna copia de ROM en paquetes. Añadir pruebas de formatos y rechazo.
- [ ] 3. Incorporar configuración versionada y persistente de mandos, familia manual y audio/vídeo. Separar la asignación de acciones de la identidad física y conectar hotplug, último dispositivo activo, foco y reconexión. Probar acción y prompt juntos.
- [ ] 4. Sustituir las indicaciones de control dentro del dibujo original del juego, conservando posición, escala, estilo y animación; retirar el panel ImGui superpuesto. El caso inicial es el botón junto a «Select»; extenderlo a «Back», pausa y las instrucciones de Training necesarias para la sesión M2. El texto y el glifo deben corresponder a la acción real, al último dispositivo activo y al remapeo, para teclado, Xbox, PlayStation y Nintendo Switch.
  - [ ] Identificar en una captura de comandos del menú el origen del rótulo y botón «Select»: recurso o atlas, coordenadas, carga y función de dibujo. Registrar las direcciones confirmadas para NTPE rev0. La búsqueda de cadenas simple no ha identificado ese rótulo.
  - [ ] Elegir un hook reproducible de N64Recomp en la rutina confirmada. Si el rótulo es un sprite completo, evaluar la sustitución de textura de RT64 solo tras comprobar que no afecta a otros usos y que actualiza correctamente su caché. No editar C generado ni reemplazar globalmente caracteres de la fuente.
  - [ ] Unificar la resolución de acción lógica → asignación efectiva → botón y glifo físico para entrada y dibujo. Cubrir cambio automático de dispositivo, selección manual y remapeo, incluidas las posiciones A/B y X/Y de Switch. Distinguir la acción de menú del uso del mismo botón en partida.
  - [ ] Inventariar y adaptar «Back», pausa, texto de Training y dibujos del mando que se muestren en la ruta M2. Rechazar con diagnóstico cualquier revisión o estructura de datos no compatible.
  - [ ] Comprobar visualmente en Linux y Windows el dibujo original y, con cada dispositivo real disponible, que el botón mostrado ejecuta la acción indicada. Cubrir teclado, familias Xbox/PlayStation/Switch, remapeo y cambio de dispositivo. Registrar explícitamente las comprobaciones físicas pendientes.
- [ ] 5. Corregir el cierre ordenado, recuperar/cambiar dispositivo de audio, y registrar diagnóstico local redactado con build, hash ROM, GPU, backend, ajustes y fallos. Pruebas de cierre, pérdida de foco y audio.
- [ ] 6. Recorrer inicio → partida → fin → menú; resolver solo incompatibilidades observadas de datos, overlays, indirectos, colas, eventos, DMA y timing con regresión previa y fallo explícito en rutas restantes.
- [ ] 7. Compilar y ejecutar builds Linux y Windows desde la misma fuente. Validar una sesión real mínima de 30 minutos en cada sistema, guardado → salida → reinicio → carga, imagen, sonido, entrada y cierre. Wine no cuenta como Windows.
- [ ] 8. Auditar paquetes ROM-free, reproducibilidad, pruebas finales y revisión independiente. Corregir defectos bloqueantes, actualizar backlog, plan y AGENTS.md; crear `docs/milestones/M2.md` con evidencia exacta y guion humano breve. Guardar commits locales. HITL-1 continúa pendiente.

## Reglas de evidencia

Cada prueba tendrá build ID, commit y ruta privada de evidencia. Una build anterior no acredita código posterior. No marcar una tarea completa por compilación sola ni dar por satisfecha la tarea 4 mediante una etiqueta en el log, un panel encima del juego o una captura sin probar la acción correspondiente. Fallos o criterios pendientes se registran sin rebajar M2.

## Punto de control técnico — 2026-09-30

La build Linux Release del commit `8413620` (SHA256
`a94dae92e1ae745ea45265bd20b6fb00d64cd0a6358251cfaadf6b51be942299`)
pasó una ruta de guardado basada en el juego: se creó el perfil F, se resolvió
Puzzle 1 con deslizamiento y eliminación, y se observó Puzzle 2 con tablero y
contadores nuevos. Tras salir al menú y cerrar normalmente (código 0), un
segundo proceso con el mismo EEPROM de 524 bytes cargó F. El juego ofreció
`CONTINUING F'S PREVIOUS GAME / PUZZLE LEVEL 2` y abrió el tablero de Puzzle
2; el segundo cierre también devolvió 0. Evidencia privada con capturas,
hashes y secuencia de entrada:
`.local/m5/puzzle-persistence-20260930T0749Z/`. Se observó una navegación
errónea a VS CPU en el segundo proceso y se corrigió antes de seleccionar
Puzzle; esa ruta no se cuenta como progreso.

Otra sesión Linux de `8413620` mantuvo el proceso vivo 1904,75 s con entrada
repetida y varios intentos Single/Rescue. La VI registró 114 015 ticks en
1900 s y la cola de audio sumó 39 vaciados detectados, agrupados sobre todo
entre los minutos 24 y 26. El arnés destruyó su ventana mediante
`xdotool windowclose` y produjo `BadWindow`; ese cierre no es una prueba de
salida normal. El piloto Puzzle anterior sí usó `windowquit` y salió limpio.
Evidencia privada: `.local/m5/m2-long-rescue-20260930T071558Z/`.

En Windows, la build `8413620` con dos puertos SDL virtuales llegó al tablero
VS con ambos jugadores y permaneció viva al menos 60 s en combate; la sesión
completa duró 1392 s, terminó en resultado WIN y se cerró normalmente.
Evidencia privada: `.local/m5/windows-vs-virtual-keyboard-p2-8413620/`.
El usuario verificó dos I-PAC físicos en Windows, pero la prueba física previa
se trabó al iniciar VS; falta repetir el tablero con ambos controles reales.
Además, una prueba posterior VS a 2160p interno terminó por un tamaño de DMA
de audio negativo (`0xFFFFFE40`) justo antes del tablero. Se investiga la
transición de frecuencia. Hasta corregirla, no se marcan completos los
criterios de sesiones largas Windows, paridad VS ni 4K.
