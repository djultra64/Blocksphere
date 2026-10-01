# M0 — Protocolo de referencia y HITL-0

Fecha: 2026-09-27. Referencia candidata: ares 148 + ROM `tetrisphere-us-rev0`. Guion de referencia M0 y regresión completa M3/M5. Resultados ejecutados y límites: [registro ampliado](../research/m0-reference-session.md) y [muestras temporales](m0-timing-2026-09-27.json). Las filas sin evidencia permanecen en regresión; no son resultados ejecutados. M0 usa inventario documental/ROM y escenas representativas según su plan; no exige completar las campañas, todas las pistas o todas las combinaciones de hardware.

## Preparación en Bazzite

Tiempo orientativo: 10 minutos para configuración; 25–40 minutos para la sesión inicial. Los recorridos completos y contenido tardío se registran aparte.

1. Validar el ZIP con `python3 tools/rom/validate.py /path/to/Tetrisphere.zip`; comprobar `recognized: true` y el hash del manifiesto. No descargar una ROM.
2. Desde la raíz del proyecto, preparar directorios privados:

   ```bash
   mkdir -p .local/m0/saves .local/m0/captures
   cp config/ares148-reference.bml .local/m0/ares-session.bml
   ```

   La copia local del dump `.local/m0/tetrisphere-us.z64` ya fue preparada durante M0. En otro checkout puede obtenerse del ZIP validado:

   ```bash
   unzip -p /path/to/Tetrisphere.zip 'Tetrisphere (USA).z64' > .local/m0/tetrisphere-us.z64
   python3 tools/rom/validate.py .local/m0/tetrisphere-us.z64
   ```

3. Abrir la referencia con sus rutas privadas. Usar la misma copia de configuración en reinicios para conservar asignaciones; no repetir `cp` sobre una sesión ya configurada:

   ```bash
   flatpak run --filesystem="$PWD/.local/m0" dev.ares.ares \
     --settings-file "$PWD/.local/m0/ares-session.bml" \
     --setting "Paths/Saves=$PWD/.local/m0/saves/" \
     --setting "Paths/Screenshots=$PWD/.local/m0/captures/" \
     --no-file-prompt --system 'Nintendo 64' \
     "$PWD/.local/m0/tetrisphere-us.z64"
   ```

4. En Settings, confirmar calidad SD, aspecto original, VI activado, PixelAccuracy, sin shader/supersampling/mezcla/runahead/rewind, velocidad normal. Registrar cualquier ajuste distinto. La configuración de la prueba de introducción es otra copia, `ares-reference.bml`; no reemplazarla.
5. Asignar controles en Input/Nintendo 64: P1 al integrado/HHD y P2 al segundo mando. Asignar todos los controles N64 y guardar una foto o tabla de equivalencias. Confirmar que cada mando actúa solo sobre su jugador. La enumeración del sistema por sí sola no cuenta.
6. Elegir salida y refresco. Para referencia original usar un modo estable y registrar sincronización; no forzar el port a ese refresco. Conectar después la pantalla 4K y volver a recoger el inventario. La captura original de M0 usa Xvfb y no acredita presentación en esa pantalla.
7. Registrar perfil energético, TDP si está disponible, corriente/batería, sesión escritorio/juego, versiones del paquete/runtime y frecuencia de audio. Calentar dos minutos antes de una medida sostenida; registrar stutters de shaders por separado.

Inventario Linux actualizado:

```bash
python3 tools/qa/collect_linux.py > .local/m0/linux-session.json
flatpak info dev.ares.ares
```

El colector debe ejecutarse desde una terminal de la sesión real; un sandbox sin acceso al display devuelve campos no disponibles. No modifica perfiles ni pantallas.

## Sesión de referencia y regresión

Grabar vídeo y audio de la ventana con OBS ya disponible. Guardar archivos en `.local/m0/captures/`; registrar resolución/FPS de captura y origen de audio. No publicar contenido original. Una grabación a 60 FPS no es una medida del tick de lógica.

| ID | Estado inicial y acción | Evidencia esperada |
|---|---|---|
| BOOT | Guardado nuevo, encender; dejar intro/demo y pasar al menú | Ruta de inicio, imagen y música; anotar diferencias respecto al perfil previo |
| TRAIN | Abrir cada ruta de entrenamiento | Lista real de lecciones y controles; completar una básica |
| RESCUE | Crear un nombre de prueba y comenzar | Esfera/cursor/HUD, progreso de al menos una etapa |
| ROTATE | Mover cursor y girar en varias direcciones durante 30 s | Movimiento, límites, sensación y alineación |
| CHAIN | Preparar una cadena con piezas especiales y usar magia | Score, contador, efectos y audio; guardar entrada/escena repetible |
| PAUSE | Pausar 10 s reales y reanudar | Estado y reloj; registrar tratamiento de la música |
| TIMER | Time Trial, 5 min de reloj del juego | Tiempo real desde inicio a fin; anotar pausas y preparación |
| PUZZLE | Resolver un puzzle sencillo, reiniciar otro y usar cámara | Drags/drops, reset, perspectivas |
| OTHER | Abrir Hide & Seek, Practice y Vs CPU | Menús, parámetros y una escena de cada uno |
| AUDIO | Revisar música/efectos/cambios de escena y opciones | Grabación audible, loops y volumen; anotar clics/cortes |
| TWO | Partida VS con dos mandos | Independencia P1/P2, ataques, pausa, victoria/derrota |
| SAVE | Progresar, salir por menú y cerrar ares normalmente | Archivo nuevo, tamaño/hash y ubicación exacta |
| LOAD | Reiniciar la misma configuración y nombre | Progreso persistente; repetir con segundo nombre |
| CONTENT | Recorrer opciones, robots, unlocks y créditos | Actualizar matriz con disponible/bloqueado y ruta de acceso |

Usar datos de prueba propios. Antes de pruebas de borrado o cierre anómalo, copiar los guardados privados; estas pruebas posteriores no se ejecutan sobre progreso personal.

## Medidas temporales y ampliación M3

Cobertura representativa M0: introducción/menú, Rescue/pausa, Training/combos guionizados, Puzzle, TimeTrial y VS; medidas instrumentadas en menú, Training, Puzzle, TimeTrial, VS y Hide&Seek. Series repetidas en menú, TimeTrial y Hide&Seek. Para ampliar M3: tres ventanas de60s por escena después de calentamiento y una ronda TimeTrial completa sin pausa. Las muestras mixtas existentes llevan notas; no representan una ronda completa ni una escena estable.

Registrar por separado:

- **VI:** contador de retraces/interrupciones de la consola emulada identificado en el emulador, dividido por duración de reloj real. Documentar punto de muestreo, versión y configuración.
- **Lógica:** llamadas al tick de actualización identificado por traza/análisis del juego, sin confundirlas con VI. Registrar dirección/símbolo y contexto cuando se confirme.
- **Dibujo:** tareas originales de gráficos/submissions del juego y frames realmente diferentes. No contar presents del host como tareas N64.
- **Velocidad:** tiempo del juego frente a reloj monótono real, con pausas identificadas. Registrar compilación de shaders, audio underruns y saturación CPU/GPU.

El VPS mostrado por ares y un vídeo aislado son referencias auxiliares. El mapa verificado de M0 distingue pasos de jugador, actualizaciones comunes, dispatcher y graphics completions; véase el registro ampliado para resets y límites. Para cualquier subsistema nuevo sin contador verificable, dejar su medida como `pending` y preparar instrumentación. No completar celdas con 30/60 Hz supuestos. La instrumentación y su coste quedan bajo responsabilidad del agente.

Formato de registro privado, una fila por ventana:

```text
scene,repeat,rom_sha256,emulator_version,config_sha256,host_snapshot,
wall_seconds,vi_count,logic_count,draw_task_count,unique_frames,
game_seconds,paused_seconds,shader_stalls,audio_underruns,notes
```

Campos sin instrumento fiable permanecen vacíos y se explican. El agente consolidará promedios, variación y ralentizaciones cuando haya datos suficientes.

## Equipo Windows y pantalla 4K

En PowerShell del equipo Windows, desde una copia del proyecto:

```powershell
New-Item -ItemType Directory -Force .local/m0 | Out-Null
powershell -NoProfile -File tools/qa/collect_windows.ps1 > .local/m0/windows-hardware.json
```

El script ya se ejecutó por SSH autorizado en Windows; el snapshot público está en `windows-hardware-2026-09-27.json`. Si la política de scripts impide abrirlo, puede pegarse su contenido en la consola; no hace falta cambiar una política global. Confirmar versión/build de Windows, CPU, GPU/driver, RAM física, resolución/refresco activos y nombres de mandos. Verificar valores de pantalla en Configuración porque WMI puede devolver datos incompletos. La ejecución del port en Windows corresponde a hitos posteriores, pero el acceso y la ficha deben quedar resueltos ahora.

En Bazzite con la pantalla 4K conectada, repetir `collect_linux.py` y anotar modo actual y refresco. El ancho de un escritorio con dos monitores 1080p no acredita una salida individual 3840×2160.

## Devolución HITL-0

Equipo RC71L y ficha Windows ya confirmados. Registrar pantalla/refresco 4K al conectarla y resultado por ID. Para un fallo: escena, pasos, esperado/observado, frecuencia, configuración, hash de ROM y log/captura privada opcional. No se necesita volver a aportar la ROM.

Confirmar alcance: USA rev0, Linux y Windows x86-64, contenido original y dos jugadores, presentación 30/60/120/sin límite sin acelerar lógica/audio, resolución hasta 4K y 4:3/16:9 ampliado. El usuario confirmó RC71L, referencia emulada por ausencia de N64, disponibilidad de equipos y autorizó explícitamente terminar M0. Se entrega este alcance para revisión; no se atribuye una evaluación perceptual de fidelidad por esa autorización. La evaluación del port ocurre en HITL-2/M3, con una build concreta.
