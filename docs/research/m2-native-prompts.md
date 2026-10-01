# M2 — indicaciones integradas en el juego

Fecha: 2026-09-29. Investigación estática reanudada por autorización del usuario.
Los hallazgos siguientes identifican recursos y rutas de código de NTPE rev0;
no acreditan por sí solos sustitución ejecutada ni el criterio de prompts de M2.

## Resultado solicitado

Modificar las indicaciones originales dentro de la presentación del juego,
conservando posición, escala, tipografía, color, animación y contexto. El
usuario rechazó el panel ImGui y también cubrir con él los rótulos originales.
Separar acción lógica, asignación física y familia del dispositivo activo.

## Recursos confirmados

Se recorrieron los recursos SQSH de la ROM privada, se descomprimieron mediante
el extractor privado `n64tetristools-research` y se inspeccionaron sus imágenes.
Las imágenes y resultados auxiliares permanecen en
`.local/m2/select-research/`; no se incorporan activos originales a Git.

| Recurso | Offset ROM SQSH | Tamaño | Bytes descomprimidos / payload comprimido |
|---|---|---|---|
| Iconos A y B del menú | `0x75516A` | 65 × 12 | 788 / 286 |
| Texto «select» y «back» | `0x76518E` | 93 × 12 | 1124 / 629 |

Los tamaños descomprimidos incluyen una cabecera de ocho bytes: ancho y alto
como enteros big-endian de 16 bits, tipo 2 y campo reservado. Los píxeles son
**IA8**, con intensidad en el nibble alto y alpha en el bajo. El tipo se
confirma en el renderer, no sólo por la apariencia del recurso.

En el atlas de botones, los límites inclusivos de píxeles con alpha no nulo
son A: x=0…9, y=1…10; B: x=53…63, y=1…10. Las celdas de trabajo pueden ocupar
x=0…11 y x=53…64, y=0…11, conservando ancho y separación. El texto empieza
aproximadamente en x=13 y x=66 de su recurso independiente. La coincidencia de
separación permite sustituir iconos sin modificar letras de una fuente común.
La igualdad del origen final de ambos dibujos debe confirmarse con una traza.

Identidad de los datos descomprimidos, incluida la cabecera:

| Recurso | SHA-256 | FNV-1a de 64 bits |
|---|---|---|
| A/B | `410d5856b06f7c7a8638d48bf3806bd8c3e6ce01d70fdce880d77612816c42da` | `0x52057514f3d4d110` |
| select/back | `c071743fff9f80772e0797c25d369d057574f926cb9344ca4ddca2c2df3380de` | `0x30ce038b8e5b2a1c` |

FNV usa offset basis 14695981039346656037 y primo 1099511628211, módulo 2^64.
En el runtime se deben leer los bytes N64 mediante `MEM_BU`: el almacenamiento
host de RDRAM tiene orden intercambiado y no se puede hashear como un array
contiguo equivalente al archivo z64.

## Carga y dibujo: evidencia estática

`func_80081264` implementa el algoritmo SQSH: a0 apunta al payload comprimido,
a1 al destino y a2 contiene el tamaño comprimido. Su llamada directa observada
es `0x80081530`, dentro de `func_80081444`. El punto `0x80081538` de esta última
es común a la descompresión y a la copia de un bloque sin compresión. Antes de
esa instrucción, los campos de stack son:

| Campo | Significado |
|---|---|
| `MEM_W(sp + 0x2C)` | Inicio del destino descomprimido |
| `MEM_W(sp + 0x24)` | Tamaño descomprimido |
| `MEM_W(sp + 0x28)` | Inicio del payload de entrada |
| `MEM_W(sp + 0x20)` | Tamaño del payload de entrada |

La llamada `0x80081D84`, en `func_80081BE8`, alcanza esa rutina al procesar el
tag J del contenedor. Los SQSH identificados tienen un encabezado de contenedor
con dicho tag y longitud antes de su firma. No se encontró una referencia ROM
absoluta directa a estos dos offsets: el contenedor explica por qué buscar el
entero del offset no bastaba para identificar la carga.

Los renderers `func_8003396C`, `func_800353F4` y `func_80036FE4` reciben en a1 una
imagen con cabecera. En el primero:

- `0x80033990`, `0x8003399C` y `0x800339A8` leen ancho, alto y tipo.
- El caso tipo 2 salta de `0x80034960` a `0x80034E08`.
- `0x80034E34` construye SetTextureImage con `FD68`, formato IA de ocho bits;
  `0x80034E44` selecciona los píxeles en a1 + 8.

Esto confirma el formato y una ruta genérica capaz de dibujar los recursos.
Una build diagnóstica aislada ejecutó el menú en Linux/X11 durante 73 segundos:
el evento `prompt_atlas_renderer` registró el hash original exacto en
`func_800353F4`, imagen `0x801ADB40`, a2/a3=0. La captura privada
`.local/m2/prompt-probe-build/runtime-menu.png` muestra el menú real en esa
ejecución. El registro de retorno fue cero y no identifica el callsite alto;
la identidad de datos y el renderer sí están demostrados para el menú. Sigue
pendiente la traza de pausa y sus argumentos de posición, recorte, color y
escala. La build diagnóstica no acredita por sí sola el comportamiento de la
build final ni la sustitución dinámica.

## Training: cadenas y parser originales

`func_80088E50` selecciona cadenas de Training, escribe los punteros
`0x80103958` y `0x8010395C`, y llama a `func_80074AC8`, que alcanza el parser y
layout `func_800743BC`. La primera cadena está en ROM `0xC9B54`, VRAM
`0x800EE7A4`; la referencia directa que la selecciona está en `0x80088F2C`.

Direcciones exactas de los caracteres de botón identificados en las cadenas:

| VRAM del carácter | Referencia original / contexto |
|---|---|
| `0x800EE7CB` | A, avanzar texto |
| `0x800EEA30` | B, arrastrar |
| `0x800EEBCA` | A, colocar |
| `0x800EF294` | C seguido de #: C-down, magia |
| `0x800EF404` | B, piezas de cristal |
| `0x800EF983` | L, opciones de Puzzle |
| `0x800EF98D` | R, opciones de Puzzle |
| `0x800EF9F4` | C seguido de comilla doble: C-up, Puzzle |
| `0x800EFB9C` | A, Multi |

La dirección de estos dos botones C está confirmada estáticamente: la fuente
IA8 de 20 × 1024 en ROM SQSH `0x79A080` contiene una flecha hacia arriba en el
glifo `0x22` (comilla doble), hacia abajo en `0x23` (#) y hacia la derecha en
`0x24` ($). Por tanto, `%C#%` representa **C-down** y `%C"%` representa
**C-up**. El segundo token contiene comilla doble, no apóstrofo.

La cadena de carga respalda la identificación de la fuente usada: la llamada
`0x8003FF80` a `func_8002A8B4` solicita el índice `0xC2` del contenedor ROM
`0x736C50`. La tabla se consulta en base + 2 + índice × 4
(`0x8002A904`/`0x8002A908`); esa entrada apunta al recurso `0x79A07A`, cuya
cabecera de seis bytes precede al SQSH `0x79A080`. El resultado se guarda en
`0x800E1014` y la llamada `0x8003FF98` a `func_80038284` lo registra como
font1. Training alcanza `func_800384E4`, que selecciona font1. El renderer
`func_800382B4` calcula la fila del glifo como (carácter − `0x20`) × altura.
Esta comprobación combina tabla, carga, selector y píxeles originales; no es
una nueva traza de ejecución.

El signo % también representa comillas o apóstrofos en el texto; no es un
token general de mando.

El parser copia caracteres desde s0 a buffers usados para dibujar y medir:
`0x80074680` lee t9 y `0x8007468C` lo almacena; `0x80074698` lee t7 y
`0x800746A0` lo almacena. Hooks antes de `0x80074684` y `0x8007469C` pueden
adaptar respectivamente r25 y r15, discriminando s0 por dirección exacta,
sin alterar todas las letras A/B de la fuente. El dibujo final llama
`func_800384E4` en `0x80074840`, que alcanza `func_800382B4`.

Esta es una propuesta de intervención respaldada por lectura estática; aún no
se ha acreditado en ejecución. Deben actualizarse tanto medición como dibujo,
y respetarse acciones distintas de confirmar/cancelar durante la partida.
PlayStation necesita símbolos propios o etiquetas correctas; sustituir una
letra arbitraria no representa automáticamente cruz o círculo.

## Pausa y límites

La captura histórica privada `.local/m2/linux-repeat-pause2.png` muestra
«A select / B back» en la presentación de pausa, con el mismo aspecto y
separación del menú. Es evidencia visual previa, no una traza que demuestre
identidad del recurso ni ejecución de un parche nuevo.

Las cadenas PAUSE de ROM `0xC82B8` y `0xC82C0` pertenecen a las leyendas de la
pantalla de controles: tienen referencias de medición/dibujo en
`0x800636D4` y `0x80063718`. No deben confundirse con instrucciones para
reanudar una partida.

## Integración y verificación pendientes

El N64Recomp fijado permite `[[patches.hook]]` con `func`, `text` y
`before_vram`; los cambios deben vivir en configuración y fuentes, seguidos
de regeneración. No editar C generado manualmente.

La opción más localizada es intervenir al dibujar el atlas confirmado y
resolver las dos acciones desde la asignación efectiva. Conservar tipografía,
transformación y texto originales; producir únicamente los iconos necesarios
para Xbox, PlayStation, Switch y teclado. Después del primer cambio, comprobar
el hash del último resultado conocido además de la identidad original; una
dirección guardada permanentemente no basta si el juego reutiliza memoria.

Faltan la traza del callsite, comprobación de usos compartidos, invalidación
correcta de caché RT64 al cambiar de dispositivo, ejecución de menú/pausa y
Training, y pruebas conjuntas de prompt y acción. Las pruebas físicas no
realizadas deben seguir explícitamente pendientes. El panel ImGui anterior no
satisface estos criterios.
