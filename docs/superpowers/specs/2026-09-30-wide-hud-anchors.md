# Anclaje estable de la UI en 16:9

Fecha: 2026-09-30. Estado: diseño en implementación y verificación.

## Problema observado

El HUD de Rescue vuelve brevemente a su posición 4:3 al sumar puntos y
permanece allí durante la salida del nivel. El efecto de calavera puede acabar
en la posición antigua. Los videos de la sesión del 2026-09-30 muestran que
una secuencia de proyecciones cambia durante estas acciones. El selector
actual identifica la UI por índices, orden y contenido de esas proyecciones;
por ello pierde la coincidencia cuando el juego compone otra animación. El
mismo defecto puede afectar otros modos aunque la sesión solo haya mostrado
Rescue.

## Comportamiento buscado

Cada elemento de juego con posición fija conserva el mismo anclaje relativo
en todos los cuadros de 16:9, incluidos pausa, pérdida de corazones,
puntuación, fin de nivel y retorno. El modo 4:3 conserva las coordenadas
originales. Los menús, resultados y efectos que deben quedar centrados no
heredan un desplazamiento lateral. La trayectoria de un efecto móvil permanece
continua y termina junto al elemento al que pertenece.

La clasificación se hace una vez por productor de UI, no por nivel ni por
estado transitorio. Su cobertura se evalúa sobre las familias de UI usadas en
todos los modos, incluido Vs con dos jugadores. Un productor sin clasificación
explícita conserva su presentación original y se registra como cobertura
pendiente; nunca recibe un anclaje especulativo por textura o coordenada.

## Diseño

1. Los hooks de N64Recomp identifican los sitios donde la lógica original
   emite un elemento o grupo semántico. Guardan el intervalo de comandos del
   display list producido con una etiqueta `left`, `right`, `center` o
   `animation`, más identidad de buffer/tarea. Un rango interior más específico
   tiene prioridad sobre el rango de su productor exterior. El puntero de
   retorno `r31` no es un identificador válido en llamadas directas del código
   recompilado; se usan sitios de llamada verificados y su continuación.
2. El intérprete RT64 asocia la etiqueta vigente al comando ejecutado, incluso
   si entra en una lista hija. Cierra un lote de triángulos cuando cambia la
   etiqueta y la conserva en cada draw call. El host entrega los rangos a la
   tarea gráfica correspondiente; los rangos de un buffer reutilizado no
   contaminan la siguiente tarea.
3. En la composición ancha se aplica el desplazamiento a cada draw call según
   su etiqueta y el área de jugador. La composición nativa que lee la memoria
   del juego sigue en 320×240. La captura de pausa usa la imagen ancha ya
   compuesta para evitar reintroducir el HUD 4:3 en la transición.
4. El inventario de productores distingue HUD fijo, menús/resultados
   centrados y efectos móviles. El anclaje de una animación requiere estudiar
   sus coordenadas de origen y destino; no se desplaza todo el vuelo como si
   fuera un icono fijo.

## Invariantes y pruebas

- Dos draw calls de distinto grupo no comparten lote ni etiqueta.
- Una lista hija usa la clasificación de su llamada actual, no la de un uso
  anterior de esa dirección.
- La reutilización de los dos buffers DL no conserva rangos de una tarea
  anterior. Los resultados centrados que reutilizan el atlas de Rescue siguen
  centrados.
- El cambio de orden, número o contenido opcional de proyecciones no cambia
  el anclaje de un productor conocido.
- En 4:3, la geometría visible es idéntica a la original.
- Una captura de video automatizada mide las posiciones del HUD al puntuar y
  acabar Rescue. La revisión manual se concentra en una muestra por familia
  de UI y en el efecto de calavera; no exige recorrer cada nivel o acción.

## Límites

El transporte de etiquetas es común a todos los modos, pero su cobertura
depende de completar y verificar el inventario de productores. La colocación
de UI de dos jugadores necesita anclas relativas a cada mitad. No se declara
resuelto todo el juego a partir de una única partida de Rescue. La salida
física 4K y la revisión en Windows siguen siendo criterios de M5 pendientes.
