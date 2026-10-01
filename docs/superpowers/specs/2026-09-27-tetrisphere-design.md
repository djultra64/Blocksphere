# Tetrisphere PC: alcance propuesto para 1.0

Fecha: 2026-09-27. Estado: baseline técnica M0 completada. ROM NTPE rev0 y referencia emulada instrumentada; port aún no implementado. El usuario confirmó ROG Ally RC71L y acceso a Windows, pantalla 4K y segundo mando. [Evidencia M0](../../milestones/M0.md).

## Objetivo y responsabilidades

Crear un port nativo de Tetrisphere mediante N64Recomp y RT64, con versiones Linux y Windows. Conservar contenido, reglas, velocidad, controles equivalentes, audio, progreso y modos originales, incluido multijugador local. Buscar presentación visual de 60 FPS sin acelerar la simulación; 120 FPS es opcional para 1.0. Resolución 4K y selección entre 4:3 original y viewport 16:9 ampliado son obligatorias.

El agente realizará ingeniería inversa, programación, integración, builds, pruebas automatizadas, diagnóstico y correcciones. El usuario participará en revisiones HITL de hitos grandes, pruebas perceptuales y validación en equipos reales. Cada revisión tendrá un artefacto concreto y un guion reproducible; las tareas rutinarias dentro del hito no necesitarán consultas adicionales.

## Requisitos y supuestos

| Área | Criterio para 1.0 |
|---|---|
| Plataformas | Linux x86-64 y Windows x86-64. Versiones mínimas de sistema y GPU se fijan con evidencia en M0/M1. |
| Fidelidad | Todos los modos y contenido de la revisión soportada, incluidos guardados, música, efectos, menús, desbloqueos y juego local para dos jugadores. Inventario completo en M0. |
| FPS | Meta de 60 FPS visuales si el hardware lo permite; 120 FPS y cadencia sin límite son mejoras opcionales para 1.0. Simulación, temporizadores y audio conservan su ritmo original. |
| Resolución | Resolución original y presets 720p, 1080p, 1440p y 2160p. Renderizado 3D a la resolución elegida, no solo escalado de la ventana. |
| Aspecto | 4:3 conserva encuadre; 16:9 amplía el campo horizontal sin deformar la esfera ni recortar el campo vertical. |
| 4K | En salida 3840×2160: viewport 16:9 de 3840×2160; viewport 4:3 de 2880×2160 con bandas laterales. |
| Uso en PC | Teclado y gamepad configurables, selección de mando por jugador, ventana/pantalla completa, ajustes persistentes, importación de ROM y mensajes de error claros. Los prompts separan acciones de botones físicos, detectan el último dispositivo/familia activa y muestran glifos correspondientes a Xbox, PlayStation o Nintendo Switch; una selección manual resuelve detecciones ambiguas y el remapeo sigue siendo opcional. |
| Distribución | Paquetes para ambos sistemas, documentación y avisos de dependencias; ROM y recursos originales aportados localmente por cada usuario. |

El usuario dispone de una ROM estadounidense en inglés: será la base NTSC-U, identificada por hash como NTPE rev0. Hará las pruebas HITL en una ROG Ally RC71L con Bazzite. Se registrarán versión del sistema, GPU/controlador, modo de energía, refresco y uso conectado/batería para reproducir resultados. No se presupone que el juego simule a 30 o 60 Hz: hay que medirlo. Compatibilidad PAL o con otras revisiones exige sus propios símbolos, tiempos y pruebas.

Los objetivos x86-64, soporte inicial de una sola revisión de ROM y empaquetado portable son propuestas para contener el alcance. El acceso a un equipo de pruebas Windows está confirmado; su ficha está registrada en docs/qa/windows-hardware-2026-09-27.json; falta validar la ejecución del port en ese sistema. La build Windows se preparará desde el inicio; Wine/Proton podrá aportar pruebas auxiliares en Bazzite, pero no sustituirá la aceptación en Windows real. La prueba visual de salida 4K requerirá una pantalla adecuada; las capturas de renderizado permiten verificar resolución interna por separado.

Si se implementa la opción sin límite, eliminará el límite artificial de presentación; el hardware y el modo de presentación del sistema pueden limitar el resultado. VSync independiente y 4K/120 quedan como mejoras posteriores a los requisitos de 4K y 16:9 para 1.0.

La mejora de FPS se define como mayor fluidez visual mediante interpolación integrada en el renderizado, manteniendo la cadencia de simulación original. No implica ejecutar lógica ni muestrear acciones del juego a 120 Hz. Las transiciones discretas y sprites sin fotogramas intermedios conservarán su comportamiento correcto; no se inventará contenido artístico para suavizarlos.

## Arquitectura elegida

ROM validada → mapa de secciones/funciones → N64Recomp → código C generado → compilador nativo → ejecutable C/C++ con N64ModernRuntime, RT64 y frontend.

En ejecución, el programa carga los datos de la ROM local y conecta el juego recompilado con temporización, audio, entrada y guardado del host. Los parches específicos de Tetrisphere se mantendrán separados del código generado. Las dependencias se fijarán por commit y las actualizaciones se validarán con regresiones.

Se propone RecompFrontend para menús y entrada, sujeto a una integración de prueba. SDL2 y RmlUi seguirán la versión utilizada por ese frontend. CMake/Ninja, C++20, Python para herramientas y Clang/MSVC según plataforma completan la base. Vulkan será la primera ruta gráfica común; D3D12 en Windows se habilitará como ruta soportada si supera la misma matriz de pruebas.

La capa de entrada expondrá acciones lógicas del juego y una identidad de dispositivo separada. Los textos y glifos de ayuda se resolverán desde la familia activa — Xbox, PlayStation, Nintendo Switch o teclado — para que la indicación visible coincida con el botón físico aun cuando A/B y X/Y ocupen posiciones distintas. El cambio de familia seguirá el último dispositivo usado, tendrá override manual persistente y nunca será condición para usar remapeo personalizado.

La ingeniería inversa usará splat, spimdisasm y, cuando ayude, Ghidra. RSPRecomp se evaluará para tareas RSP de audio u otras tareas identificadas. Recompilar microcódigo no demuestra por sí mismo que RT64 pueda interpretar los gráficos ni interpolarlos: esa compatibilidad se verificará por separado.

## Alternativas consideradas

1. **N64Recomp + runtime + RT64 con parches específicos — recomendada.** Aprovecha código original y concentra el trabajo manual en identificar funciones, compatibilidad y mejoras visuales.
2. **Decompilación completa y port manual.** Ofrece mayor control, pero añade un trabajo previo considerable y no es necesario como condición de arranque. Se decompilarán solo las funciones que haga falta entender o modificar.
3. **Emulador empaquetado.** Puede servir como referencia de pruebas, pero no cumple el objetivo de port nativo con N64Recomp.

## Límites de alcance

1.0 no incluirá nuevos modos, online, cambios de reglas, texturas HD, modelos nuevos, ray tracing, editor, sistema de mods ni plataformas ARM/macOS. Se conservarán los modos originales. No se ampliará el alcance para resolver un problema de compatibilidad sin documentar su efecto.

La fidelidad será funcional, temporal y visual frente a una referencia registrada. No se promete emulación ciclo a ciclo de todas las características de la consola. Las diferencias de ralentización original que afecten la experiencia se medirán y se someterán a HITL antes de aceptar cambios.

## Incertidumbres que bloquean una promesa de fecha

- No hay código ni ROM en el repositorio al preparar esta propuesta.
- No se ha verificado un mapa completo de funciones de Tetrisphere reutilizable.
- Falta identificar compresión, overlays, saltos indirectos, bibliotecas y microcódigos reales.
- Falta comprobar fidelidad de audio, compatibilidad RT64 e interpolación de geometría dinámica.
- La configuración de ROG Ally RC71L/Bazzite ya se registró en M0; Windows real inventariado; queda ampliar sesiones/perfiles del port en los hitos correspondientes.

La primera decisión de viabilidad se tomará con evidencia en M1. Si una dependencia no soporta una función necesaria, el agente preparará una prueba acotada de adaptación y una revisión del esfuerzo; no sustituirá el requisito silenciosamente.

## Base documental

Fuentes primarias consultadas el 2026-09-27:

- [N64Recomp](https://github.com/N64Recomp/N64Recomp): recompilación desde binario y metadatos, overlays y herramientas RSP.
- [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime): ultramodern/librecomp, temporización, entrada, audio, DMA y guardados.
- [RT64](https://github.com/rt64/rt64): renderizador, APIs gráficas y mejoras de presentación con soporte dependiente del juego.
- [GBI extendida de RT64](https://github.com/rt64/rt64/blob/main/include/rt64_extended_gbi.h): comandos para integración de parches.
- [RecompFrontend](https://github.com/N64Recomp/RecompFrontend): entrada y menús reutilizables.
- [splat](https://github.com/ethteck/splat) y [spimdisasm](https://github.com/Decompollaborate/spimdisasm): herramientas de análisis de binarios.
- [Zelda64Recomp](https://github.com/Zelda64Recomp/Zelda64Recomp): referencia de integración, sin asumir que sus parches son aplicables a Tetrisphere.
- [n64tetristools](https://github.com/chris-gilmore/n64tetristools) y [dcm2xm](https://github.com/chris-gilmore/dcm2xm): referencias específicas para recursos y música de Tetrisphere. El segundo describe reconstrucción parcial; no demuestra fidelidad suficiente para reemplazar el audio original.
