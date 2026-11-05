# OneInTwo

Aplicación de escritorio de implementación propia para compartir portapapeles, archivos y teclado/ratón entre dos ordenadores de una red local, con Windows y Ubuntu como objetivos iniciales.

## Estado del proyecto

Actualizado: **5 de noviembre de 2026**. El repositorio contiene la arquitectura del producto y tres prototipos independientes implementados en `experiments/`: portapapeles, archivos y teclado/ratón.

La aplicación integrada todavía no está implementada: `src/` conserva descripciones de responsabilidades, el CMake raíz es una reserva comentada y no hay instaladores. Cada experimento tiene su propio código, CMake, dependencias, pruebas aisladas, manual e informe de resultados.

Los prototipos usan C++17 y bibliotecas generales documentadas en sus informes. La biblioteca gráfica de la aplicación, las versiones mínimas verificadas y la licencia siguen pendientes. El código experimental no se ha integrado automáticamente en el producto.

| Experimento | Estado resumido | Documentación |
| --- | --- | --- |
| Portapapeles | Windows y Wayland/TLS compilados; lectura, pegado y observación locales Ubuntu verificados; sincronización entre equipos pendiente | [Manual](experiments/clipboard/MANUAL.md) · [Resultados](experiments/clipboard/RESULTS.md) |
| Archivos | Windows y Ubuntu/GTK/TLS compilados; pegado asistido y casos TLS locales Ubuntu verificados; transferencias entre equipos pendientes | [Manual](experiments/files/MANUAL.md) · [Resultados](experiments/files/RESULTS.md) |
| Teclado y ratón | Windows y Wayland/TLS compilados; self-tests 579/0; inyección y salidas locales Ubuntu verificadas con un monitor; origen Ubuntu bloqueado por InputCapture v1 | [Manual](experiments/input/MANUAL.md) · [Resultados](experiments/input/RESULTS.md) |

La fase local disponible en Ubuntu tiene resultados observados y limitaciones registradas. La entrega Ubuntu se ha reconstruido en Windows sin repetir su escritorio. Siguen pendientes el escritorio Windows y las pruebas entre equipos. El detalle de cada caso y del incidente histórico de entrada está en los `RESULTS.md` y `TODO.md`; no se deduce soporte de una plataforma de la mera existencia de código.

## Documentación

- [Estudio preliminar](ESTUDIO_PRELIMINAR.md): requisitos, viabilidad y restricciones.
- [Arquitectura propuesta](ARQUITECTURA.md): componentes, relaciones, flujos y estructura.
- [Pruebas previstas](tests/README.md): comprobaciones de comportamiento.
- [Experimentos de viabilidad](experiments/README.md): estado de los prototipos y orden de validación manual.
- [Pendientes](TODO.md): problemas y verificaciones todavía abiertos.
- [Validación en Ubuntu](VALIDACION_UBUNTU.md): encargo para continuar en la segunda máquina y coordinar las pruebas entre equipos.
- [Primera prueba LAN de portapapeles](experiments/clipboard/lan/README.md): identidades públicas, preparación de ambos extremos y matriz de ejecución.
- [Distribución Windows](packaging/windows/README.md) y [distribución Ubuntu](packaging/ubuntu/README.md).

## Responsabilidad de este archivo

Servir como entrada al repositorio y describir fielmente su estado. Cuando exista una implementación, recoger los pasos comprobados de compilación y ejecución, las plataformas verificadas y la licencia elegida.

