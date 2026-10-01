# Plan de desarrollo: Tetrisphere PC 1.0

> **Para ejecución por agentes:** aplicar `superpowers:executing-plans` por tareas. Este documento es una propuesta de desarrollo por hitos; el análisis de la ROM determinará las direcciones, símbolos y firmas internas antes de implementar cada subsistema.

**Objetivo:** entregar Tetrisphere nativo en Linux y Windows con fidelidad al original, presentación visual de hasta 60 FPS si es posible, renderizado 4K obligatorio y viewport seleccionable 4:3/16:9 ampliado. 120 FPS es opcional para 1.0.

**Arquitectura:** N64Recomp traduce el juego; N64ModernRuntime implementa los servicios de consola; RT64 presenta gráficos. Un frontend de PC administra ROM, entrada, ajustes y archivos, con parches específicos de Tetrisphere separados del código generado.

**Stack propuesto:** C/C++20, N64Recomp/RSPRecomp, N64ModernRuntime, RT64, RecompFrontend/SDL2/RmlUi, CMake/Ninja, Python, splat/spimdisasm y Ghidra cuando resulte útil.

**Especificación:** [Alcance y diseño propuestos](../specs/2026-09-27-tetrisphere-design.md).

**Estado (2026-09-28):** M0 y M1 técnico completados: prototipo nativo ejecutado en Linux y Windows real con entrada, RT64, música y efectos; paquetes ROM-free auditados. [Evidencia M1, reproducción y límites](../../milestones/M1.md). HITL-1 sigue pendiente y no se atribuye aprobación humana.

**Datos confirmados por el usuario:** ROM US en inglés, ahora identificada como NTPE rev0 por hash; acceso a Windows real, segundo mando y pantalla 4K en Bazzite; no dispone de N64. El usuario confirmó que el equipo objetivo es ROG Ally RC71L con Bazzite. La ficha Windows se obtuvo por SSH autorizado; la ejecución del port en ambos sistemas corresponde a M1 y siguientes.

## Restricciones globales

- Linux x86-64 y Windows x86-64 se compilan desde M1; ambas versiones son necesarias para 1.0.
- ROM US en inglés identificada por hash como referencia inicial. Una segunda revisión no se considera compatible por similitud del nombre.
- La meta de 60 FPS visuales no puede acelerar simulación, audio ni temporizadores. 120 FPS y cadencia sin límite son opcionales para 1.0.
- Resolución original, 720p, 1080p, 1440p y 2160p; 4:3 original o 16:9 con ampliación horizontal.
- Se preservan todos los modos originales, contenido, progresión y multijugador local.
- Las ROM, recursos extraídos, trazas con contenido original y datos privados de prueba permanecen fuera de los artefactos públicos.
- Dependencias fijadas, builds identificables y registro de procedencia/licencias antes de redistribuir componentes.
- HITL aprueba hitos grandes con evidencias. Las correcciones y decisiones rutinarias dentro del alcance aprobado las realiza el agente.

## Riesgos prioritarios de revisión

1. ROM incorrecta, truncada o con distinto orden de bytes: normalizar formatos reconocidos y rechazar revisiones no soportadas antes de ejecutar. Pruebas en M0/M2.
2. Geometría que aparece, desaparece o reutiliza memoria: evitar interpolar entre objetos distintos. Pruebas en M4.
3. Dos jugadores, cursores y HUD en 16:9: conservar independencia de mandos, correspondencia visual y área de juego. Pruebas en M3/M5.
4. Pérdida de foco, minimización, cambios de pantalla o carga GPU elevada: conservar tiempos, audio y guardado. Pruebas en M2/M4/M6.
5. Interrupción de escritura y actualización de versión: no destruir el último guardado válido. Pruebas en M2/M6.

## Organización prevista

Rutas propuestas, que se crearán cuando las necesite su hito:

| Ruta | Responsabilidad |
|---|---|
| `config/roms/`, `config/recomp/` | Manifiestos de revisiones, secciones, símbolos y configuración de recompilación. |
| `tools/rom/`, `tools/analysis/` | Validación, normalización y generación reproducible de metadatos. |
| `src/game/` | Registro del juego y enlace con el runtime. |
| `src/platform/` | Audio, entrada, archivos y temporización del host donde no los cubra el frontend. |
| `src/graphics/` | Integración RT64, presentación, métricas y resolución. |
| `src/ui/` | Importador de ROM y menús de PC. |
| `patches/` | Parches MIPS/C para funciones concretas del juego. |
| `generated/` | Salida regenerable; no editar manualmente. |
| `tests/`, `tools/qa/` | Regresiones, capturas, replays y recogida de diagnósticos. |
| `docs/research/`, `docs/qa/`, `docs/milestones/` | Hallazgos, protocolos y evidencias de aceptación. |
| `packaging/`, `.github/workflows/` | Paquetes y CI cuando exista remoto disponible. |

## M0 — Referencia y alcance cerrado

**Entrega:** ficha reproducible de la ROM, inventario de comportamiento y entorno de pruebas. No requiere todavía un port funcional.

- [x] Identificar la ROM aportada: región, revisión, tamaño, orden de bytes y SHA-256 del contenido normalizado. No descargar una ROM como dependencia.
- [x] Crear un validador con resultado estructurado: revisión reconocida o motivo de rechazo. Cubrir cabeceras inválidas, archivo truncado, formatos reconocidos y revisión distinta.
- [x] Registrar CPU, GPU, RAM, controlador gráfico, distribución Linux, versión Windows, mandos y refresco del monitor de los equipos de prueba.
- [x] Caracterizar la ROG Ally RC71L con Bazzite: controles integrados, modo escritorio/juego cuando esté disponible, perfil energético y alimentación. Resolver acceso a Windows real y pantalla 4K para las pruebas que lo requieran; dejar explícito cualquier recurso aún no disponible.
- [x] Inventariar todos los modos, opciones, contenidos, desbloqueos y rutas de guardado originales usando la ROM y su documentación disponible.
- [x] Registrar referencia de arranque, partida, giro de esfera, cadenas, efectos, música, pausa y dos jugadores. Usar consola si está disponible y una referencia emulada de precisión documentada; registrar versión/configuración y diferencias conocidas.
- [x] Medir cadencia VI, actualización lógica y dibujo en escenas representativas. Documentar ralentizaciones y velocidad real.
- [x] Auditar dependencias y recursos reutilizables. Fijar versiones iniciales de herramientas, sin afirmar compatibilidad todavía.

**Salida comprobable:** `docs/research/baseline.md`, manifiesto de ROM sin contenido original, matriz de modos y protocolo reproducible. La ausencia de ROM permite preparar herramientas, pero impide dar M0 por completado.

**HITL-0:** RC71L, recursos y referencia emulada confirmados por el usuario; ejecución/cierre de M0 autorizados explícitamente. Alcance y referencias entregados en M0. Esta autorización no se convierte en evaluación perceptual de fidelidad; esa revisión requiere la build M3/HITL-2.

## M1 — Viabilidad de recompilación, gráficos y audio

**Entrega:** prototipo nativo con diagnóstico y evidencia de la ruta técnica; primer punto de decisión sobre esfuerzo.

- [x] Mapear arranque, segmentos, compresión, funciones, tablas de salto, llamadas indirectas y overlays si existen. Validar símbolos con análisis estático y trazas de ejecución; no confiar ciegamente en detección automática.
- [x] Identificar funciones de sistema/libultra y conectar sus equivalentes del runtime. Fallar con diagnóstico ante rutas aún no implementadas, sin esconder fallos con stubs permanentes.
- [x] Generar C con N64Recomp y compilar un ejecutable mínimo en ambos sistemas. Confirmar que la regeneración con los mismos insumos es consistente.
- [x] Identificar los microcódigos gráficos y de audio. Verificar comandos reales frente al soporte RT64 y ejecutar una tarea representativa de audio; evaluar RSPRecomp cuando corresponda.
- [x] Llegar a una pantalla real y a una escena jugable con RT64. Ejecutar música y efectos por la ruta candidata de audio.
- [x] Investigar incompatibilidades con pruebas acotadas. Si hace falta ampliar RT64 o manejar una variante RSP, documentar comandos afectados y demostrar un caso mínimo antes de extrapolar.
- [x] Separar resultados demostrados de hipótesis: boot, escena, entrada, sonido y compilación por sistema. Preparar backlog de compatibilidad y revisión de esfuerzo.

**Salida comprobable:** builds de diagnóstico para Linux/Windows, mapa inicial de símbolos, trazas de fallos y `docs/research/feasibility.md`. Compilar sin ejecutar no basta para cerrar el hito.

**HITL-1:** prueba breve de escena y sonido, revisión de riesgos y decisión de continuar con la ruta demostrada. Si un requisito esencial sigue sin ruta verificable, M1 permanece abierto.

## M2 — Port base jugable, versión 0.1

**Entrega:** una sesión completa y persistente a resolución/aspecto/cadencia originales en ambas plataformas.

- [ ] Resolver carga de datos, overlays, colas, eventos, DMA y temporización necesarios para una partida completa.
- [ ] Integrar importación local de ROM y validación; avisar de formato/revisión inválidos con un mensaje accionable.
- [ ] Conectar gamepad/teclado, audio y guardado al runtime. Modelar acciones separadas de botones físicos; detectar el último dispositivo y sustituir los botones y textos de control dentro de las indicaciones originales del juego, conservando su posición, estilo y animación. Cubrir Xbox, PlayStation, Nintendo Switch y teclado, con override manual y remapeo opcional. El panel superpuesto de M2 es provisional y no satisface este criterio. Identificar primero cómo se dibujan «Select», «Back», pausa y las instrucciones necesarias de Training; usar una intervención verificable en la rutina original o, si procede, una textura sustituida sin afectar otros usos. Detectar el mecanismo real de guardado del juego antes de elegir la implementación. Véanse `docs/research/m2-native-prompts.md` y la tarea 4 del plan detallado de M2.
- [ ] Añadir configuración persistente, directorios de datos por usuario, escritura segura con copia recuperable y versión de formato de configuración.
- [ ] Probar inicio → partida → fin → menú, guardar → salir → reiniciar y cargar, reconexión de mando, pérdida de foco y dispositivo de audio.
- [ ] Añadir un diagnóstico local con identificador de build, ROM por hash, GPU, backend, ajustes y errores, sin empaquetar la ROM.

**Salida comprobable:** partida completa en cada sistema, progreso persistente y sonido sin interrupciones reproducibles. Pruebas automáticas de configuración, validación e interrupción de guardado; sesión real de al menos 30 minutos por sistema.

**Revisión humana de avance:** guion corto de controles, sonido y guardado. En el checkpoint de controles, comprobar en el menú que el botón aparece en la indicación original «Select»/«Back», sin panel adicional, y que al pulsarlo ocurre la acción indicada; repetir tras cambiar al último dispositivo activo y con un remapeo. Revisar también una indicación de Training. Registrar por separado las familias de mando físico disponibles y las pendientes. Los defectos se incorporan al hito; la puerta formal de fidelidad llega en M3.

## M3 — Paridad con el original, versión 0.3

**Entrega:** experiencia completa de la revisión soportada antes de introducir mejoras visuales.

- [ ] Recorrer el inventario de M0: modos, tutoriales, menús, progresión, desbloqueos, transiciones y finales aplicables.
- [ ] Completar una ruta larga de progresión y cubrir estados tardíos mediante partidas y fixtures locales reproducibles. Los accesos de prueba no sustituyen validar la progresión normal.
- [ ] Verificar multijugador local con dos dispositivos independientes, pausa, fin de partida y reconexión. Ampliar la verificación de indicaciones originales de M2 a todos los modos: probar al menos un mando de cada familia disponible y confirmar que cada prompt visible activa la acción indicada, incluyendo las posiciones A/B y X/Y distintas entre Xbox y Nintendo.
- [ ] Comparar entradas grabadas por tick y semilla controlada, cuando sea posible: tablero, puntuación, temporizadores, RNG y estado de progresión. Excluir direcciones y datos volátiles de los hashes de estado.
- [ ] Comparar imagen y sonido frente a M0; documentar tolerancias justificadas para renderizadores distintos y errores perceptuales, sin exigir igualdad de píxeles indiscriminada.
- [ ] Corregir fallos de lógica, audio, guardado y gráficos base antes de usarlos como referencia para mejoras.

**Salida comprobable:** matriz completa con resultado por modo en ambas plataformas; cero fallos conocidos de progresión o pérdida de datos y sin sustituciones silenciosas de funciones originales.

**HITL-2:** revisión extensa de fidelidad, música, sensación de control y dos jugadores. Esta build se conserva como referencia de regresión.

## M4 — Presentación fluida hasta 60 FPS, versión 0.5

**Entrega:** fluidez visual ampliada con ritmo original preservado.

- [ ] Separar reloj de simulación, programación de audio y presentación según las medidas de M0/M3. El FPS de renderizado nunca será el reloj del juego.
- [ ] Implementar 60 FPS visuales si la plataforma lo permite, con métricas separadas de simulación, frames generados y presentados. 120 FPS, cadencia sin límite y VSync independiente son mejoras opcionales para 1.0.
- [ ] Añadir los parches y comandos extendidos RT64 necesarios para identificar cámara, esfera, piezas, partículas y otros elementos dinámicos.
- [ ] Mantener identidades estables entre frames; reiniciar interpolación en cambios de escena, respawn, pausa, teletransporte y reutilización de objetos. Tratar geometría deformable y cambios de topología explícitamente.
- [ ] Verificar que los frames adicionales contienen movimiento intermedio real donde corresponde, no únicamente duplicados ni un contador mayor.
- [ ] Reproducir las mismas entradas por tick a los cuatro ajustes y comparar estado lógico con M3. Probar saturación GPU y 30 FPS aunque la lógica requiera más actualizaciones por segundo.
- [ ] Probar ritmo musical, sincronía, latencia, foco, minimización y monitores de distinto refresco; sin acumulación ilimitada de trabajo ni aceleración al recuperar la ventana.

**Salida comprobable:** pruebas de invariancia lógica, capturas de movimiento intermedio y trazas de tiempos de frame. En hardware de referencia capaz y sin límite externo, el promedio se mantiene dentro de ±2 % del límite elegido tras calentamiento, sin deriva de tiempo. Evaluar p95/p99 y tirones por separado del promedio.

**Revisión humana de avance:** comparación A/B de giro de esfera, cursor, colocación y destrucción de piezas, efectos y música. Se consolidará con M5 en HITL-3.

## M5 — Resolución y viewport, versión 0.7

**Entrega:** resolución original hasta 2160p y 4:3/16:9 seleccionables, compatibles con todos los modos de FPS.

- [ ] Distinguir resolución interna, tamaño de ventana, escala DPI y área activa de presentación. Implementar los presets definidos en la especificación.
- [ ] Mantener campo vertical y ampliar proyección horizontal para 16:9. Ajustar viewport, scissor y culling para que el contenido adicional se dibuje correctamente.
- [ ] Adaptar HUD, textos, cursores, menús y composición de dos jugadores; verificar que la selección de piezas siga coincidiendo con su posición dibujada.
- [ ] Para fondos/imágenes 2D de tamaño fijo, conservar proporciones y composición mediante bandas o extensión de fondo compatible. La excepción afecta al recurso 2D, no permite encerrar toda la partida 16:9 en un viewport 4:3.
- [ ] Probar render targets, efectos de framebuffer y lectura de imagen que puedan depender de dimensiones originales.
- [ ] Permitir cambios de ajustes con recuperación segura ante una resolución no utilizable. Verificar persistencia, ventana redimensionada y pantalla completa.
- [ ] Ejecutar matriz mínima: 2 sistemas × 2 aspectos × 2 cadencias (original y 60 solicitado) × 5 resoluciones = 40 combinaciones en la ruta gráfica común. Extender a 30/120/sin límite si esas opciones se entregan; backends adicionales repiten su matriz pertinente.

**Salida comprobable:** capturas verificadas de viewport real, esfera sin deformación, HUD sin recortes y sesiones a 3840×2160 en ambos sistemas. La matriz de configuraciones usa escenas representativas; no implica completar todo el juego 40 veces.

**HITL-3:** revisar juntas fluidez y presentación, en especial 4K, 4:3 frente a 16:9 y multijugador. 120 FPS es opcional para 1.0; ajustar los criterios de rendimiento al equipo medido sin retirar 4K ni 16:9.

## M6 — Beta multiplataforma, versión 0.9

**Entrega:** paquetes instalables o extraíbles y pruebas de uso fuera del entorno de desarrollo.

- [ ] Producir ZIP portable para Windows y paquete Linux portable reproducible; evaluar AppImage y conservar un archivo tar como alternativa si resulta más compatible.
- [ ] Fijar mínimos de OS, bibliotecas y GPU a partir de resultados. Probar Linux en Wayland/X11 cuando ambos estén disponibles y Windows real; Wine no sustituye la validación Windows.
- [ ] Ejecutar builds limpias de Release y Debug/diagnóstico, análisis estático y sanitizers donde sean aplicables. Comprobar rutas con espacios, caracteres no ASCII y directorios sin permiso de escritura.
- [ ] Probar importación inicial, configuración dañada, actualización sobre datos existentes, guardado recuperable, desconexión de dispositivos, suspensión/reanudación y cierre normal/anómalo.
- [ ] Validar en la ROG Ally RC71L/Bazzite controles integrados, navegación sin teclado externo, legibilidad de menús, perfiles de energía y suspensión/reanudación. Registrar consumo y rendimiento sin prometer autonomía antes de medir.
- [ ] Perfil CPU/GPU y memoria: al menos dos horas continuas por sistema con cambios de modos y ajustes, sin fallos ni crecimiento sostenido inexplicado tras calentamiento.
- [ ] Automatizar builds y pruebas sin ROM con CI público si hay remoto; las pruebas con ROM/GPU requieren un entorno privado autorizado. Un runner sin GPU solo acredita compilación y pruebas compatibles.
- [ ] Preparar README, controles, troubleshooting, notas de versión, checksums y avisos de dependencias. Auditar el contenido de los paquetes.

**Salida comprobable:** instalación limpia y uso en ambos sistemas, diagnósticos reproducibles, matriz de hardware y listado de problemas conocidos con severidad.

**HITL-4:** beta en equipos reales. Yo entrego binarios y guion; tú reportas experiencia, incompatibilidades y errores. Yo reproduzco, añado regresiones relevantes y entrego correcciones.

## M7 — Release candidate y 1.0

**Entrega:** dos paquetes finales derivados de la misma revisión de código y configuración de dependencias.

- [ ] Congelar funcionalidades; corregir únicamente defectos y repetir las pruebas afectadas más el smoke test general.
- [ ] Repetir aceptación completa de modos originales, guardado, audio, entrada, resolución, aspecto y FPS en Linux y Windows.
- [ ] Cerrar todos los defectos P0/P1. Documentar cualquier P2 aceptado que no incumpla un requisito; los errores que alteren jugabilidad, audio esencial o funciones prometidas no se rebajan para publicar.
- [ ] Verificar regeneración/build desde un checkout limpio con los insumos locales documentados. Emitir manifiesto de versiones y hashes de paquetes.
- [ ] Preparar etiqueta 1.0, notas, código fuente correspondiente según componentes usados y paquetes sin ROM ni recursos originales extraídos.
- [ ] Realizar HITL final sobre la candidata exacta. Cualquier corrección posterior invalida su aprobación para las áreas afectadas y requiere nueva verificación.

**HITL-5:** aceptación explícita de la candidata para Linux y Windows. Publicar la 1.0 tras esa aceptación; el silencio no cuenta como aprobación.

## Trabajo autónomo y protocolo HITL

Dentro de un hito, el agente implementa, prueba, diagnostica y corrige sin pedir permiso por cada archivo o commit. Mantiene un registro de estado: listo para probar, en corrección, aceptado o bloqueado con evidencia. Se avanza por dependencias; mientras una revisión está pendiente se puede realizar trabajo independiente que no presuponga su aceptación.

Cada entrega humana contendrá:

1. Identificador de build y commit, paquetes por sistema y hashes.
2. Resumen de cambios, alcance probado automáticamente y limitaciones verificadas.
3. Guion numerado con estado inicial, acciones, resultado esperado y duración orientativa.
4. Comparativas de imagen/audio o métricas cuando ayuden a evaluar el cambio.
5. Lista de errores abiertos y decisión concreta solicitada: aceptar, corregir o revisar alcance.

Formato de error: build; SO; CPU/GPU/driver; backend; hash de ROM; resolución/aspecto/FPS/VSync; modo/nivel; pasos; esperado/observado; frecuencia; log y captura o vídeo opcional. El agente facilita la exportación del diagnóstico; no exige reconstruir el entorno manualmente.

Severidad: P0 = pérdida de datos o fallo general; P1 = bloqueo, regresión de reglas o incumplimiento importante del objetivo; P2 = defecto acotado sin bloqueo; P3 = mejora menor. Un hallazgo que cambia la viabilidad, rompe guardados o compromete la fidelidad activa revisión extraordinaria con una propuesta concreta.

## Dependencias, calendario y control del esfuerzo

Ruta principal: M0 → M1 → M2 → M3 → M4 → M5 → M6 → M7. Los trabajos de resolución pueden prepararse tras M3, pero su integración se valida junto con interpolación. La compilación multiplataforma acompaña toda la ruta.

No hay base para prometer una fecha de 1.0 antes de analizar la ROM. M1 concentra la incertidumbre de símbolos, microcódigo y runtime; M4 concentra la de interpolación. La agenda se reestimará al cerrar M1 y M4, separando implementación, validación y espera de HITL. No se convierte esa espera en tiempo de desarrollo estimado ni se presume ejecución continua entre sesiones.

Al comenzar cada hito, se detallarán únicamente sus tareas ejecutables: archivos concretos, interfaces confirmadas por la versión fijada de las dependencias, pruebas de fallo/corrección y comandos de validación. Esto evita inventar direcciones o APIs antes de conocer el juego. Cada hallazgo se registra para que el siguiente tramo pueda continuar sin repetir la investigación.

## Definición de terminado

1.0 requiere todos los requisitos de la especificación verificados en ambas plataformas, todos los modos originales cubiertos, persistencia fiable, opciones gráficas interoperables y aceptación HITL de la candidata. Una demo, una build que compila o un contador que indica 120 FPS no constituye por sí mismo el resultado solicitado.

Las fuentes técnicas y los supuestos están enlazados en la especificación. Este plan no afirma que Tetrisphere ya sea compatible con RT64 ni que exista una recompilación funcional en este repositorio.
