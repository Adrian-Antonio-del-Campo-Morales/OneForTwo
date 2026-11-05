# OneInTwo

Aplicación de escritorio de implementación propia para compartir portapapeles, archivos y teclado/ratón entre dos ordenadores de una red local, con Windows y Ubuntu como objetivos iniciales.

## Estado del proyecto

Actualizado: **5 de noviembre de 2026**. El repositorio contiene la arquitectura del producto y tres prototipos independientes implementados en `experiments/`: portapapeles, archivos y teclado/ratón.

La aplicación integrada todavía no está implementada: `src/` conserva descripciones de responsabilidades, el CMake raíz es una reserva comentada y no hay instaladores. Cada experimento tiene su propio código, CMake, dependencias, pruebas aisladas, manual e informe de resultados.

Los prototipos usan C++17 y bibliotecas generales documentadas en sus informes. La biblioteca gráfica de la aplicación, las versiones mínimas verificadas y la licencia siguen pendientes. El código experimental no se ha integrado automáticamente en el producto.

| Experimento | Estado resumido | Documentación |
| --- | --- | --- |
| Portapapeles | Windows y Wayland implementados; compilación Linux con portal verificada, TLS Linux pendiente | [Manual](experiments/clipboard/MANUAL.md) · [Resultados](experiments/clipboard/RESULTS.md) |
| Archivos | Windows compilado; adaptador Linux escrito, compilación Ubuntu pendiente | [Manual](experiments/files/MANUAL.md) · [Resultados](experiments/files/RESULTS.md) |
| Teclado y ratón | Windows, X11 y Wayland implementados; self-test Windows 538/0; sin evidencia de compilación nativa Linux | [Manual](experiments/input/MANUAL.md) · [Resultados](experiments/input/RESULTS.md) |

Las pruebas reales de portapapeles, exploradores, captura/inyección y comunicación entre equipos siguen pendientes. El detalle de qué se ha compilado o probado está en cada `RESULTS.md`; no se deduce soporte de una plataforma de la mera existencia de código.

## Documentación

- [Estudio preliminar](ESTUDIO_PRELIMINAR.md): requisitos, viabilidad y restricciones.
- [Arquitectura propuesta](ARQUITECTURA.md): componentes, relaciones, flujos y estructura.
- [Pruebas previstas](tests/README.md): comprobaciones de comportamiento.
- [Experimentos de viabilidad](experiments/README.md): estado de los prototipos y orden de validación manual.
- [Pendientes](TODO.md): problemas y verificaciones todavía abiertos.
- [Validación en Ubuntu](VALIDACION_UBUNTU.md): encargo para continuar en la segunda máquina y coordinar las pruebas entre equipos.
- [Distribución Windows](packaging/windows/README.md) y [distribución Ubuntu](packaging/ubuntu/README.md).

## Responsabilidad de este archivo

Servir como entrada al repositorio y describir fielmente su estado. Cuando exista una implementación, recoger los pasos comprobados de compilación y ejecución, las plataformas verificadas y la licencia elegida.

