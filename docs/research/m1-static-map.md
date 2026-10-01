# M1: mapa estático inicial de NTPE rev0

Fecha: 2026-09-28. Se confirmó estáticamente la carga inicial de 1 MiB y se
exportaron seis funciones con límites revisados. Esto permite consumir símbolos
con N64Recomp; todavía no constituye un inventario completo ni un juego recompilado.

## Reproducción y privacidad

Desde la raíz del repositorio:

```sh
python3 -m unittest tests.test_m1_analysis -v
python3 -m unittest discover -s tests -v
python3 -m tools.analysis.map_rom .local/m0/tetrisphere-us.z64
python3 -m tools.analysis.map_rom .local/m0/tetrisphere-us.z64 --output .local/m1/analysis/static-map-run1.json
python3 -m tools.analysis.map_rom .local/m0/tetrisphere-us.z64 --output .local/m1/analysis/static-map-run2.json
sha256sum .local/m1/analysis/static-map-run1.json .local/m1/analysis/static-map-run2.json
cmp .local/m1/analysis/static-map-run1.json .local/m1/analysis/static-map-run2.json
```

La CLI valida la identidad completa mediante `tools/rom/validate.py` y el
manifiesto `config/roms/tetrisphere-us-rev0.json`, y exige formato z64 normalizado.
Rechaza una salida que sea la propia ROM, incluso mediante enlaces simbólicos
o duros, antes de escribir. La API sintética `analyze_rom(data: bytes) -> AnalysisResult`
comprueba formato, alineación y entrada; no impone la identidad completa para
permitir fixtures de prueba. Su método `to_json()` produce una salida estable:
sin fecha, rutas de entrada, orden aleatorio ni dependencias del entorno.
La CLI impide escribir fuera de `.local` tras resolver enlaces simbólicos.

Las dos corridas privadas dieron el mismo SHA-256 y `cmp` terminó con código 0.
ROM, snapshot, resultados extensos, herramientas auxiliares y desensamblados
permanecen bajo `.local/m1/analysis/` o `.local/m0/`. Este documento y la
configuración contienen metadatos y conclusiones, sin volcados de instrucciones,
datos ni tablas originales.

## Detección automática y dictámenes humanos

`static-map.json` conserva solamente detecciones automáticas: `status=detected`,
confianza y procedencia. `human_rulings` comienza vacío y no se fusiona con los
dictámenes versionados en `config/recomp/sections.json`. Este último distingue
`confirmed`, `rejected` y asuntos abiertos; la confirmación indicada es estática,
no una traza nueva de ejecución. Ningún destino de llamada recibe tamaño de
función ni confirmación automática.

El barrido analiza el primer MiB del cuerpo como traducción provisional
ROM `0x1000` ↔ RAM `0x80025c50`; interpreta instrucciones incluso donde puede
haber datos. Produce 1,138 candidatos de función, 7,264 llamadas directas,
138 llamadas indirectas, 199 saltos indirectos y 33 grupos de punteros.
Las cifras incluyen falsos positivos y no son un censo de código ejecutable.
Detecta además el patrón de limpieza de memoria de la entrada.

El decodificador considera el PC posterior para saltos absolutos, desplazamientos
con signo, delay slots y anulación en ramas likely. Registra llamadas REGIMM y
ramas de coprocesador. El seguimiento local de constantes se invalida tras
transferencias de control; no propaga estados a través de llamadas. Reconoce
secuencias concretas de registros PI, un patrón de copia de palabras y un patrón
de limpieza de dos palabras. Son candidatos de transferencia con ejecución
todavía no probada. Superposiciones de destinos de DMA de fuentes distintas
solo generarían candidatos de overlay, sin afirmar que sean código.

## Carga y arranque confirmados

Rabbitizer, commit `e0d8003047938e2ec3697eaf8d61a84d11d17b43`, procede del
submódulo de N64Recomp fijado en `config/dependencies-m1.json`. Se usó su API C++
y la biblioteca construida en M1. La extensión Python no pudo compilar por falta
de `Python.h`; no se cambió el pin ni se sustituyó por un decodificador incompleto.

La inspección privada de IPL3, en ROM `0x4c0..0x514`, muestra la lectura de la
entrada del encabezado y la programación de PI para esta transferencia:

| Concepto | Intervalo, extremo final excluido | Alcance de la evidencia |
|---|---|---|
| Imagen cargada inicialmente | ROM `0x1000..0x101000` → RAM `0x80025c50..0x80125c50` | 1 MiB programado por IPL3; incluye código y datos |
| Limpieza de la entrada | RAM `0x800f2040..0x80164e60` | Tamaño `0x72e20`, paso de ocho bytes, incremento en delay slot |
| Código de entrada | RAM `0x80025c50..0x80025c88` | Salto final a `0x80029510`, SP resultante `0x800f4848` |

La limpieza sobrescribe parte de la imagen cargada y se extiende más allá de
ella. Por eso no se modela como otra sección ROM solapada. El límite inicial
de limpieza no demuestra por sí solo el límite entre texto y datos del archivo.
La carga PI inicial se confirmó manualmente en IPL3; el analizador automático
del cuerpo no ejecuta ni analiza el cargador completo.

La primera divergencia ROM/snapshot en ROM `0xbaabf` / RAM `0x800df70f`, heredada
de M0 y reproducida aquí, **se rechaza como límite de carga**: corresponde a
estado mutable. La instantánea es de una sesión en marcha, no del arranque puro.

## Símbolos exportados

`config/recomp/symbols.toml` usa la interfaz real `symbols_file_path`:
`[[section]]`, `rom`, `vram`, `size` y `functions` con nombre/dirección/tamaño.
Un arnés privado enlazado al objeto `config.cpp.o` de N64Recomp cargó el archivo
con `Context::from_symbol_file`, obtuvo una sección y seis funciones y comprobó
que sus palabras se cargaban de la ROM. Esto verifica lectura e integración del
formato; no afirma que la recompilación de todas sus dependencias sea posible.

| Nombre exportado | ROM | RAM | Tamaño | Etiqueta M0, solo orientativa |
|---|---|---|---|---|
| `func_80025C50` | `0x1000` | `0x80025c50` | `0x38` | Entrada |
| `func_80028964` | `0x3d14` | `0x80028964` | `0x170` | Scheduler |
| `func_80029510` | `0x48c0` | `0x80029510` | `0xc4` | Inicialización alcanzada desde entrada |
| `func_8006E418` | `0x497c8` | `0x8006e418` | `0x574` | Despacho de entrada |
| `func_8006EE6C` | `0x4a21c` | `0x8006ee6c` | `0x6c0` | Inicialización de modo |
| `func_800B7E3C` | `0x931ec` | `0x800b7e3c` | `0x560` | Actualización de juego |

Se revisaron prólogos, epílogos, delay slots, llamadas y límites de ramas directas.
Los seis intervalos coinciden byte por byte con el snapshot de M0. Las ramas
directas que no son llamadas quedan dentro de sus límites. Se excluyó padding
posterior y se conservaron nombres basados en dirección para no convertir las
etiquetas semánticas de M0 en nombres originales confirmados.

## Correcciones y asuntos abiertos

- **Rechazado:** `0x80070300` como inicio independiente. Es una instrucción interior
  de la rutina candidata que comienza en `0x80070118`; el código anterior continúa
  hacia ella. El seed M0 era una posición de observación, no un límite demostrado.
- **Confirmado:** despacho indirecto en `0x800acce4` mediante tabla en `0x800f1094`
  con siete entradas. Se comprobó el límite del índice, escala, carga y destinos
  dentro de `0x800acb94..0x800adbc0`. El detector agrupa esa tabla con punteros
  vecinos en una corrida de 18 entradas: no debe usarse esa corrida como tamaño
  real de tabla ni exportarse automáticamente.
- **Confirmado parcialmente:** llamada indirecta en `0x80026498`, cuyo destino se
  carga de una estructura. La instrucción y su delay slot están verificados;
  el conjunto de callbacks y su significado siguen abiertos.
- **Candidatos:** `0x80070118`, `0x800acb94` y `0x800cd3b8` tienen evidencia de
  inicio, pero sus flujos completos no se exportan en esta entrega. El resto de
  los candidatos necesita clasificación código/datos y revisión de límites.
- **Compresión abierta:** no aparecen firmas `Yay0`, `MIO0`, `Yaz0`, `RNC1` ni
  `RNC2`. La secuencia literal corta `RNC` advertida en M0 no basta para confirmar
  un stream. El barrido de firmas no descarta formatos propios o sin encabezado;
  no hay descompresor ni stream validado.
- **Overlays abiertos:** no hay overlay ejecutable confirmado. Hace falta seguir
  cargas PI posteriores, destinos en RAM y uso ejecutable; una lista vacía no
  prueba ausencia. ROM posterior a `0x101000` queda sin mapa de carga.

Las pruebas usan únicamente fixtures sintéticas. El RED inicial fue ausencia
del módulo; luego se añadieron regresiones RED/GREEN para la limpieza de memoria
y la invalidación de constantes después de una llamada. Se comprueban también
determinismo, rechazo de solapamientos y coherencia de los símbolos publicados.
