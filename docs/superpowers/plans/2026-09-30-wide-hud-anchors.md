# Plan: anclaje determinista de UI en 16:9

**Objetivo:** quitar la dependencia del HUD respecto al orden de
proyecciones y conservar su posición lateral durante cualquier transición de
juego. Diseño: [wide-hud-anchors](../specs/2026-09-30-wide-hud-anchors.md).

## 1. Transporte de procedencia

**Archivos:** `include/tetrisphere/`, `src/graphics/rt64_context.cpp`,
`tools/build/patch_rt64_probe.py`, `CMakeLists.txt`, tests de contrato.

- [ ] Prueba roja de rangos DL, prioridad de rangos interiores, lista hija,
      cambios de lote y reutilización de buffer.
- [ ] Añadir cola por tarea de rangos etiquetados; transportar las etiquetas
      desde comandos RT64 a draw calls sin modificar la memoria original.
- [ ] Verificar que el compositor ancho recibe la etiqueta y que la ruta
      nativa/4:3 produce la misma geometría.

## 2. Inventario de productores

**Archivos:** manifiesto legible en `config/`, generador/validador en
`tools/analysis/`, tests estáticos.

- [ ] Enumerar llamadas de los productores comunes de gameplay, resultados,
      entrenamiento, pausa, menús y Vs. Dar a cada una `left`, `right`,
      `center`, `animation` o `native`, con región de jugador cuando aplique.
- [ ] El generador verifica ROM, función, JAL, continuación y cobertura de
      todas las llamadas conocidas; rechaza huecos o solapamientos.
- [ ] Generar hooks de inicio y fin de emisión desde el manifiesto. La
      continuación es JAL+8; no usar `r31` ni delay slots.
- [ ] Auditar draw calls sin etiqueta por familia de pantalla en una traza
      compacta para descubrir productores ausentes.

## 3. Política de anclaje

**Archivos:** `tools/build/patch_rt64_probe.py` y helper de geometría.

- [ ] Desplazar elementos fijos de izquierda/derecha por draw call y por
      región de jugador, independiente de proyecciones y elementos vecinos.
- [ ] Mantener resultados, menús, mundo y overlays centrados. Definir la
      transformación de animaciones por extremos; verificar calavera.
- [ ] Retirar el selector Rescue dependiente de proyecciones cuando el
      inventario lo reemplace y no haya doble desplazamiento.
- [ ] Conservar la captura GPU ancha de pausa y verificar entrada, Continue,
      segunda pausa y Exit sin mosaicos ni retorno a 4:3.

## 4. Verificación

- [ ] Pruebas sintéticas de proyecciones reordenadas, HUD parcial, cantidad
      cambiante de dígitos, resultado centrado, P1/P2, 4:3 y buffer DL
      reutilizado.
- [ ] Compilar Linux y Windows desde el mismo código fuente; ejecutar pruebas
      enfocadas y revisar el diff.
- [ ] Medir en video Linux las posiciones de Rescue al puntuar, perder
      corazón y acabar nivel. Hacer revisión manual breve por familia de UI,
      con el usuario manejando la ventana; recoger los resultados.
- [ ] Registrar límites y evidencia en `docs/research/m4-m5-current.md`.
      No cerrar M5 hasta completar la salida física 4K y Windows.
