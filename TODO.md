# Problemas pendientes

## EXP-BUILD-01 — Completar la evidencia de compilación nativa

Estado: pendiente. Actualizado: 5 de noviembre de 2026.

- Portapapeles: falta compilar TLS en Linux; el adaptador de portal sí tiene evidencia de compilación.
- Archivos: falta compilar el adaptador GTK y el prototipo completo en Ubuntu.
- Entrada: no hay evidencia registrada de compilación nativa Linux de Wayland, X11 o TLS. Confirmar además la reconstrucción de la aplicación Windows después de la ampliación Wayland; la revisión documental solo ejecutó el self-test existente.

Usar las opciones explícitas de cada manual para evitar una compilación satisfactoria que omita el backend o TLS. No confundir los flujos con dobles con los adaptadores nativos. Registrar comandos, dependencias, versiones y errores.

## EXP-MANUAL-01 — Validar capacidades en los equipos reales

Estado: pendiente. No se han ejecutado pruebas de escritorio o comunicación entre equipos.

- Portapapeles: permisos con cero dispositivos, lectura/escritura/observación y funcionamiento con captura inactiva.
- Archivos: pegado en Explorer/Nautilus, caché, integridad, cancelación y comparación bajo demanda Windows.
- Entrada: permisos, atajo local, cruce y retorno en ambos sentidos, liberación de teclas y recuperación ante cortes.
- Red: comprobar aceptación y rechazo de identidades en los prototipos y transferencias reales.

Seguir los tres `MANUAL.md`, un experimento cada vez, y registrar resultados observados en sus `RESULTS.md`. Los self-tests y la presencia de código no cierran estos puntos.

## EXP-INPUT-01 — Conciliar la entrega Wayland y su documentación

Estado: **resuelto en documentación**, 5 de noviembre de 2026.

`experiments/input/README.md`, `MANUAL.md` y `RESULTS.md` ya describen la implementación Wayland, sus requisitos, las pruebas manuales y los límites de la evidencia. La comprobación aislada del binario Windows dio 538/0; los 403/0 anteriores quedan identificados como históricos. Las verificaciones nativas y manuales permanecen abiertas en los puntos anteriores.
