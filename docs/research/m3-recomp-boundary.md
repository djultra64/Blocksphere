# M3 — límite de función en la geometría del tablero

Dos cierres nativos Linux (volcados locales de los procesos 2597117 y
2609593, 2026-09-29) terminaron en `func_800924E0`, instrucción invitada
`0x80092518`, al leer `a0 = 1` como puntero. La llamada procedía de
`func_8009423C` en `0x8009470C`. Un volcado M1 anterior mostró la misma ruta,
por lo que el fallo precede los cambios recientes de audio y gráficos.

El origen está en `config/recomp/symbols.toml`: se declaró una función nueva
en `0x8009401C` dentro del epílogo de la función que empieza en
`0x80093E44`. La salida N64Recomp anterior terminaba esta última en
`0x80094018`, dejando sin ejecutar las cargas de `s6`, `s7`, `fp` y el
`sp += 0x50` de `0x8009401C..2C`. El llamador leía entonces sus variables
locales con un `sp` desplazado, incluida la dirección de la geometría, que
resultaba ser `1`.

Se retiró el límite falso y se regeneró el C privado con la ROM autorizada y
el N64Recomp fijado por el proyecto. La nueva `static_0_80093E44` incluye el
epílogo completo y no genera `func_8009401C` independiente. No se modificó
el C generado manualmente. La build nativa Linux compiló y las 21 pruebas de
`test_m1_recomp.py` pasaron fuera del sandbox. La prueba automatizada posterior
se mantuvo estable durante tres minutos, pero no entró en Rescue; falta una
sesión de juego que recorra esta función para acreditar la regresión dinámica.

Los volcados, capturas y código generado permanecen en `.local/m3/`, fuera de
Git y de los paquetes.
