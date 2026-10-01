# Procedencia y dependencias iniciales — M0

Fecha: 2026-09-27. Consultas de solo lectura a los repositorios oficiales. [Manifiesto por commit](../../config/dependencies-m0.json). No se ha compilado ninguna dependencia del port ni se ha demostrado compatibilidad con Tetrisphere.

| Componente | Commit candidato | Hallazgo de raíz | Uso previsto |
|---|---|---|---|
| [N64Recomp](https://github.com/N64Recomp/N64Recomp) | `ffb39cdad1da5de07eaaa48bd1db4a89a7986771` | LICENSE MIT | Traductor y herramienta RSPRecomp del mismo proyecto |
| [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) | `cdf5abbd5026fef5c364c676e4667c45e42b6863` | COPYING GPL versión 3 | Servicios de consola; revisar obligaciones al integrar |
| [RT64](https://github.com/rt64/rt64) | `43373749dac9bbc1b653e6a02aed40a9e1783bed` | LICENSE MIT | Renderizador candidato; microcódigo real pendiente |
| [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) | `b1a1477c6556aeb7ed45defbfb5924f721efebc1` | Sin licencia raíz localizada | Investigación de interfaz; no aprobar reutilización sin aclararla |
| [splat](https://github.com/ethteck/splat) | `1d09139b886f0bece632bd8f367ccc1d3318ca2a` | LICENSE MIT | Análisis, aún no ejecutado |
| [spimdisasm](https://github.com/Decompollaborate/spimdisasm) | `4b5d4c68eeb9f9eae14b3bb713971433ae2c3c43` | LICENSE MIT | Análisis, aún no ejecutado |
| [n64tetristools](https://github.com/chris-gilmore/n64tetristools) | `60dfe8a7e7145047b7043c00b46715a1789756bb` | Sin licencia raíz localizada | Referencia de formatos, sin copiar código/recursos |
| [dcm2xm](https://github.com/chris-gilmore/dcm2xm) | `db035a6f119243877375d0028158adc0cb42d06c` | Sin licencia raíz localizada | Referencia musical; no acredita reconstrucción fiel |
| [ares](https://github.com/ares-emulator/ares) | `4cb8d92b441557cb6bcaf133c4cbc7f6819b1122` | LICENSE compuesta, API NOASSERTION | Investigación; ejecución local usa **v148**, no este HEAD |
| [Mupen64Plus core](https://github.com/mupen64plus/mupen64plus-core) | `b20b27ebf9e5b099a978e86dba609111dc98c837` | Sin archivo de licencia raíz identificado por este sondeo | Consulta del catálogo, sin redistribuir el core |

Las observaciones anteriores proceden de listar la raíz y leer LICENSE/COPYING; no son una auditoría exhaustiva. No encontrar ese archivo no demuestra ausencia de una licencia en otros lugares. El hash del archivo encontrado se conserva en el manifiesto. Se deben revisar cabeceras, subdirectorios, binarios incluidos y dependencias transitivas antes de integrar o redistribuir.

RecompFrontend fija C17/C++20 y CMake mínimo 3.20 en su CMakeLists. Declara submódulos RmlUi, lunasvg y freetype-windows-binaries. Sus gitlinks, licencias, assets y requisitos SDL aún deben fijarse con una configuración que compile; no se elige SDL2/SDL3 por suposición. El plan menciona SDL2 como propuesta y se revisará contra la integración real en M1.

No se han importado recursos reutilizables del juego. La ROM, los guardados, registros con contenido original y capturas se mantienen en `.local/`, ignorada por Git. Los paquetes del futuro port deberán incluir los avisos y el código fuente correspondiente que exija el conjunto finalmente usado.

Toolchain detectada: Python 3.14.3, Git 2.53.0, GCC 15.2.1; Ghidra Flatpak 12.1.3. CMake/Ninja/Clang ausentes de PATH en este entorno. No hay toolchain Windows probada. El mínimo de sistema/GPU sigue sin establecerse.
