# Pruebas previstas

Estado: planificación de la aplicación integrada. Esta carpeta todavía no contiene una suite del producto. Los tres experimentos sí tienen pruebas aisladas propias y evidencia de alcance específico en sus respectivos `RESULTS.md`.
Responsabilidad: reunir pruebas del producto y sus criterios de aceptación. Los experimentos que investigan APIs pertenecen a [experiments](../experiments/README.md).

## Núcleo automatizable sin escritorio

- Sesión: vinculación rechazada, identidad cambiada, revocación, conexiones duplicadas y reconexión con nueva generación.
- Protocolo: tramas parciales, tamaños excesivos, mensajes desconocidos, orden inválido y operaciones sin permiso.
- Portapapeles: concurrencia, supresión de eco, formato no soportado y ausencia de reproducción de contenido antiguo.
- Archivos: rutas que escapan del destino, nombres incompatibles, colisiones, archivos vacíos, integridad, disco lleno y cancelación.
- Entrada: transiciones de foco, rechazo de eventos de otra generación, liberación de teclas y temporizador del receptor.

Usar dobles pequeños del contrato de escritorio y del transporte solo donde ayuden a observar estas garantías. No crear ahora un framework de simulación.

## Integración y comprobación manual

Windows y Ubuntu deben probarse en ambos papeles. Verificar el pegado real en Explorer/Nautilus, el retorno del cursor, AltGr y acentos, bloqueo de sesión, permisos revocados y desconexiones con botones pulsados.

Comparar archivos por tamaño y hash. Medir respuesta interactiva durante transferencias grandes. Registrar versiones de sistema, escritorio, portales, teclado y método de medición.

## Condición para afirmar soporte

Una prueba del núcleo no certifica un escritorio. Publicar únicamente las combinaciones comprobadas y sus límites. El primer incremento integrado elegirá la herramienta de pruebas del producto; los casos enumerados aquí no se contabilizan como pruebas superadas. Las pruebas manuales de los prototipos se ejecutarán según sus manuales y no se trasladarán automáticamente como certificación de la aplicación futura.

