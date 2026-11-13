# Compartir una sola ventana

La [base de implementación paralela](IMPLEMENTACION_PARALELA.md) fija las firmas públicas, los propietarios, el codec, los límites y el protocolo de esta entrega. Sus decisiones concretan las propuestas de esta guía.

## Flujo de uso

1. En el equipo de origen, pulsar **Compartir ventana** y elegir una ventana local de una aplicación.
2. El equipo vinculado recibe una invitación que identifica al remitente y dice **Una ventana**. Al aceptarla, OneInTwo presenta su imagen en directo en un visor independiente.
3. El visor permite redimensionar la imagen y verla en modo de solo lectura. La selección conserva la identidad de la ventana mientras se mueve o redimensiona. El vídeo incluye únicamente el contenido que el sistema entregue para esa superficie. Los menús y diálogos que el sistema trate como ventanas independientes quedan fuera.
4. **Detener** en el origen y cerrar el visor en el receptor terminan la transmisión. Cerrar la ventana original también la termina. Para compartir otra, se crea una invitación nueva.

El aislamiento depende de que el sistema capture la ventana como fuente nativa. Si esa capacidad no existe o la ventana ya no puede identificarse con seguridad, OneInTwo indica que la función no está disponible o detiene la emisión. Queda excluido recortar una región de la pantalla y cualquier cambio automático a la captura de monitor. Si al minimizar se suspende la fuente, el visor se vacía e indica el estado. Puede volver a mostrar la misma ventana cuando el adaptador confirme que sigue siendo la fuente autorizada. Si el sistema cierra la captura, hace falta una nueva invitación.

## Permisos y ciclo de vida

**Compartir ventana** tiene su propio permiso por sentido, desactivado inicialmente. El receptor puede permitir ventanas y mantener deshabilitada la pantalla completa. Se aplican el perfil **Habitual**, la selección y autorización locales, y la aceptación de cada invitación. El título de la ventana sirve para la selección local y queda fuera del protocolo y del diagnóstico. El indicador de captura permanece accesible en el origen.

Se reutilizan la caducidad, los límites de vídeo y la invalidación descritos en [Compartir una pantalla](COMPARTIR_PANTALLA.md#permisos-y-ciclo-de-vida). Mover la ventana a otro monitor no cambia la autorización. Si el origen pierde la capacidad de identificar la superficie, la operación se cierra aunque aparezca una ventana nueva con el mismo título.

## Arquitectura básica

| Capa | Responsabilidad |
| --- | --- |
| [Sesión](../src/core/session.hpp) y [protocolo](../src/net/protocol.hpp) | Aplicar el permiso específico de ventana. La modalidad de la invitación queda asociada a la operación y se comprueba antes de aceptar, capturar o entregar vídeo. |
| [Módulo visual común](../src/features/visual_share.hpp) | Compartir el ciclo de invitación, los estados, el transporte, los límites y la presentación con la pantalla completa. Solicitar al escritorio exactamente una fuente del tipo ventana. |
| [Windows](../src/platform/windows/visual_capture.cpp) | Seleccionar localmente un `HWND` apto y usar la captura nativa de ventana como vía candidata. Vincular la operación al objeto de captura y atender su cierre sin volver a buscar por título. |
| [Ubuntu/Wayland](../src/platform/linux/visual_capture.cpp) | Habilitar la opción solo si el portal anuncia `WINDOW`. Solicitar una sola fuente de ese tipo, mantener la sesión ScreenCast y consumir únicamente el stream autorizado. |
| [Visor](../src/ui/visual_viewer.cpp) | Compartir la misma ventana receptora y rotular el modo **Una ventana**. Vaciarla durante la suspensión o cierre y ajustar la presentación al nuevo tamaño. |

Documentación de los adaptadores. [Captura de ventana en Windows](https://learn.microsoft.com/en-us/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createforwindow) · [Tipos de fuente de ScreenCast](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.ScreenCast.html#org-freedesktop-portal-screencast-availablesourcetypes)

## Criterios de aceptación

- En ambos sentidos, compartir una ventana con un patrón identificable. Superponer otra ventana con un marcador distinto, mover la original a otro monitor y redimensionarla. Comprobar que el vídeo conserva solo el contenido de la fuente elegida.
- Minimizar y restaurar la fuente. Verificar que, si la captura se suspende, el visor se vacía y solo recupera la misma ventana tras confirmar su disponibilidad. Si la captura se cierra, verificar que una nueva invitación es necesaria.
- Cerrar la ventana de origen y abrir otra con el mismo título. El visor debe vaciarse y la segunda ventana debe quedar fuera de la operación anterior.
- Con permiso de ventana habilitado y pantalla deshabilitada, verificar que la primera modalidad funciona y que la segunda se rechaza. En un entorno sin captura nativa de ventana, comprobar el estado de indisponibilidad y la ausencia de un vídeo de escritorio.

## Guía de implementación

Implementar esta modalidad como una variante de `VisualSourceKind` dentro del mismo módulo visual. El permiso se selecciona a partir de la modalidad fijada al crear la oferta. Conservar esa asociación en todas las transiciones y comprobarla otra vez al arrancar la captura.

En Windows, el adaptador debe mantener el objeto de captura y su recurso gráfico, atender cierre y cambios de tamaño, y convertir los buffers a BGRA con filas contiguas. El selector local debe identificar una fuente de ventana verificable. La vida de un handle requiere comprobarse junto con la del objeto de captura para evitar reutilización accidental. Publicar suspensión o cierre cuando el sistema impida adquirir contenido.

En Wayland, reutilizar los patrones de solicitudes asíncronas y cancelación de `portal_session.cpp` y los experimentos disponibles. Crear una sesión ScreenCast propia para la operación, solicitar `WINDOW` con una sola fuente y validar la respuesta antes de consumir PipeWire. Mantener juntos solicitud pendiente, sesión, conexión PipeWire y referencia de operación. La cancelación debe cerrar también una solicitud que todavía espera respuesta.

El contrato `DesktopVisualCapture` es compartido por ambas plataformas. Cada implementación necesita una fábrica visible para el coordinador que devuelva el adaptador mediante ese contrato. Usar las fábricas declaradas en src/integration/feature_contracts.hpp. El propietario de la composición selecciona la fábrica al compilar y enlaza únicamente el backend correspondiente.

Validar la ventana primero en un circuito local captura → visor. Después reutilizar la invitación y el transporte de pantalla. Usar dos ventanas con patrones distintos para comprobar oclusión, movimiento, cambio de tamaño, minimización, cierre y reutilización del título. Anotar qué comportamiento entrega realmente cada escritorio al suspender la fuente.

## Contrato de selección y aislamiento

La fábrica de captura recibe el contexto local que necesite la plataforma, incluido el padre del selector y el ejecutor del hilo nativo. Devuelve un objeto con propiedad única a través de `DesktopVisualCapture`. El coordinador conserva ese objeto hasta completar la cancelación de sus callbacks.

`selectSource` produce una única respuesta terminal por solicitud mientras el consumidor siga vivo. El resultado `ready` significa que el adaptador retiene una fuente válida del tipo solicitado. `cancelled`, `denied`, `unavailable` y `error` liberan sus recursos pendientes. Si la fuente desaparece entre selección y aceptación, el inicio informa de cierre y la operación termina.

El backend debe validar el tipo efectivo de la fuente devuelta por su selector. Cuando el sistema no permita acreditar ese tipo, la operación termina como no disponible. La prueba de aislamiento debe incluir contenido distinto detrás y encima de la ventana, además de un diálogo separado de la aplicación.

La parada es idempotente y cancela tanto selección como adquisición. Los callbacks ya encolados conservan la referencia original y el módulo los descarta al haberla invalidado. La destrucción desconecta callbacks nativos y espera o cancela los trabajos que todavía puedan acceder al objeto.
