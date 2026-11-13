# Compartir una pantalla

La [base de implementación paralela](IMPLEMENTACION_PARALELA.md) fija las firmas públicas, los propietarios, el codec, los límites y el protocolo de esta entrega. Sus decisiones concretan las propuestas de esta guía.

## Flujo de uso

1. En el equipo de origen, abrir OneInTwo, pulsar **Compartir pantalla** y elegir un monitor local. Si hay varios, el selector permite identificar cuál se va a compartir.
2. El equipo vinculado muestra una invitación que identifica al remitente y dice **Pantalla completa**. El destinatario puede aceptarla o rechazarla. OneInTwo empieza a leer y enviar fotogramas tras aceptar y verificar que el permiso y la selección de origen siguen vigentes.
3. La imagen en directo aparece en una ventana independiente de OneInTwo en el equipo receptor. La ventana se puede redimensionar y conserva la proporción de la imagen. El alcance inicial es visualización en directo, de solo lectura, sin audio ni grabación.
4. El origen ve un indicador y puede pulsar **Detener**. Cerrar el visor también termina la transmisión. Cerrar la ventana principal de configuración deja el visor y su indicador disponibles.

El usuario comparte un solo monitor físico en cada operación. Cambiar de monitor requiere terminar la operación y seleccionar otro. Los cambios de resolución del mismo monitor pueden continuar dentro de la transmisión. Si el monitor desaparece o la selección deja de ser fiable, el visor borra la imagen y la operación termina.

## Permisos y ciclo de vida

**Compartir pantalla** requiere un permiso propio por sentido en el par vinculado. Empieza desactivado. La sesión comprueba que el origen puede capturar monitores y el receptor puede abrir el visor. El perfil **Habitual** permite usar ese permiso y **Solo envíos** y **Pausa** lo suspenden. La selección se hace en el escritorio de origen, con la autorización del sistema cuando corresponda.

La operación pasa por **selección local → invitación pendiente → aceptada → en directo → terminada**. Una invitación dura como máximo 30 segundos desde que se presenta, medidos con el reloj monotónico del receptor. El emisor aplica su propio plazo desde el envío. Un rechazo, un cierre del selector, la caducidad, el bloqueo de cualquiera de los equipos, la revocación o la desconexión terminan la operación. Cada extremo actúa en cuanto detecta el evento. La reconexión requiere una selección y una aceptación nuevas.

## Arquitectura básica

| Capa | Responsabilidad |
| --- | --- |
| [Sesión](../src/core/session.hpp) y [protocolo](../src/net/protocol.hpp) | Autorizar la modalidad por sentido. La invitación, la decisión y el cierre usan el canal interactivo. Los datos de vídeo se admiten solo para la operación aceptada y vigente. |
| [Módulo visual común](../src/features/visual_share.hpp) | Mantener una sola operación visual por sesión, pendiente o activa, contando ambas modalidades y ambos sentidos. Una invitación simultánea se rechaza por ocupación. Coordinar captura, codificación, decodificación y cierre por identidad de operación. |
| [Windows](../src/platform/windows/visual_capture.cpp) | Seleccionar localmente un monitor y capturarlo con Windows Graphics Capture como vía candidata. Conservar el identificador nativo solo en origen. |
| [Ubuntu/Wayland](../src/platform/linux/visual_capture.cpp) | Consultar `AvailableSourceTypes`, solicitar solo `MONITOR` y una sola fuente con ScreenCast, y consumir un único stream autorizado con PipeWire. El portal conserva su selección local y la sesión se solicita sin persistencia. |
| [Visor](../src/ui/visual_viewer.cpp) | Mostrar la imagen, el equipo de origen y el estado. Borrar la superficie al perder la fuente o la autorización, y solicitar el cierre al coordinador. |

El identificador de operación combina generación de sesión, extremo que inició la invitación e identificador creciente de ese extremo. El receptor rechaza identificadores repetidos o antiguos, incluidos los ya rechazados por ocupación. La invitación contiene la modalidad, sin nombre del monitor ni identificadores nativos. El canal de datos previsto para archivos transporta también unidades de vídeo con revisión de fuente, secuencia, dimensiones y marca de imagen clave. El objetivo inicial es hasta 30 fotogramas/s, un máximo de 1920 píxeles por lado y 2.073.600 píxeles por imagen. El receptor comprueba estos límites tanto en los metadatos como en la salida del decodificador. La unidad codificada se limita a 1 MiB antes de reservar el búfer de reensamblado. La primera entrega utiliza JPEG de imágenes independientes según la base de implementación paralela. Su latencia y caudal requieren validación.

El transporte intercala bloques de archivos y fragmentos de vídeo con colas acotadas y mantiene el control en su canal independiente. Si la cola de vídeo se llena, se descartan capturas antes de codificarlas. Si se pierde una unidad codificada pendiente, se descartan sus dependientes y se reinicia desde una imagen clave. La revisión de fuente aumenta al iniciar o cambiar el estado de captura. El visor vacía la imagen en cada transición y solo acepta vídeo de una revisión anunciada como disponible, empezando por una imagen clave. Como los canales avanzan de forma independiente, el vídeo que llegue antes de su anuncio se descarta. El emisor produce imágenes clave periódicas para que el receptor pueda recuperar la imagen. El receptor termina la operación si pierde las señales de actividad. Durante una pausa del contenido, estas señales pueden mantener abierta la operación sin imagen. Para una fuente disponible pero estática, pueden mantener la última imagen solo mientras el adaptador confirme la disponibilidad. Al parar se cierran la fuente y los búferes, se invalida la operación y se descartan callbacks y datos tardíos. Solo se guardan estados y errores de diagnóstico sin imágenes ni nombres de fuentes.

Documentación de los adaptadores. [Windows Graphics Capture](https://learn.microsoft.com/en-us/windows/apps/develop/media-authoring-processing/screen-capture) · [Captura de monitor en Windows](https://learn.microsoft.com/en-us/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createformonitor) · [Portal ScreenCast](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.ScreenCast.html)

## Criterios de aceptación

- En Windows → Ubuntu y Ubuntu → Windows, elegir un monitor, aceptar y ver su imagen en una ventana independiente. Con dos monitores, mostrar marcadores distintos en cada uno y comprobar que solo se transmite el elegido.
- Rechazar, dejar caducar y cancelar una invitación. Verificar que OneInTwo no lee ni envía fotogramas antes de aceptar y que el estado pendiente se retira.
- Cambiar la resolución del mismo monitor y comprobar que el visor adapta la imagen. Desconectar ese monitor y comprobar que el visor se vacía y que otro monitor nunca toma su lugar.
- Cerrar el visor, bloquear un equipo, retirar el permiso y cortar la red durante la emisión. Verificar la liberación de recursos, el descarte de datos tardíos y la ausencia de reanudación automática.
- Hacer una transferencia de archivos simultánea. Verificar colas acotadas, visor recuperable tras la congestión y respuesta del canal interactivo al detener.

## Guía de implementación

La base de producto todavía requiere un ejecutable, composición de objetos, transporte y UI. El CMake raíz es una reserva. Empezar por un objetivo de núcleo C++17 y una prueba local con captura y visor conectados permite verificar la imagen antes del intercambio entre equipos.

| Paso | Entrada y resultado esperado |
| --- | --- |
| Composición | El coordinador crea sesión, adaptador de captura, módulo visual y visor. Inyecta reloj monotónico, salida de mensajes y ejecución en el hilo de UI. |
| Selección | `offerSource` reserva una referencia local y llama a `selectSource`. El callback comprueba que la referencia continúa vigente. Cancelar libera la reserva. |
| Invitación | Tras seleccionar, emitir `VisualShareOffer`. `receiveOffer` valida permiso, generación, modalidad y ocupación antes de solicitar confirmación a la UI. |
| Aceptación | `decideOffer` envía la decisión y prepara el visor. `receiveDecision` inicia la captura del origen únicamente para una aceptación vigente. |
| Imagen | El callback nativo entrega BGRA al módulo. Un trabajo acotado escala y codifica. La sesión envía fragmentos. El receptor reensambla, valida, decodifica y presenta en el hilo gráfico. |
| Terminación | Invalidar primero la referencia, cancelar trabajos, cerrar captura, vaciar/cerrar visor y emitir el cierre si el canal sigue disponible. Repetir el cierre debe ser inocuo. |

El estado mínimo del módulo incluye referencia, modalidad, fase, revisión de fuente, última secuencia, plazos y referencias a sus dependencias. Añadir constructores y callbacks explícitos a las declaraciones actuales cuando se implemente. El módulo posee los temporizadores y la revisión. El adaptador posee los recursos nativos y comunica cambios de disponibilidad. Los callbacks se serializan en el ejecutor del módulo antes de modificar su estado.

La salida del módulo necesita operaciones concretas para enviar invitación, decisión, estado, actividad, imagen y cierre, además de presentar una invitación local. Estas salidas se conectan desde el coordinador a sesión y UI. Las firmas actuales describen entradas y deben completarse con esas dependencias antes de enlazar el producto.

El protocolo debe definir versión, orden de bytes, tipos, longitudes y representación del iniciador relativa al receptor. Cada fragmento requiere referencia, revisión, secuencia, longitud total y posición. Limitar el número de reensamblados simultáneos y descartar duplicados inconsistentes. Las longitudes de imagen deben comprobarse con aritmética segura antes de multiplicar ancho, alto y cuatro canales.

Aplicar el codec, la fragmentación, los presupuestos de cola y los temporizadores fijados en la base común. Medir coste, calidad y caudal con esas decisiones y entregar cualquier limitación reproducible al integrador.

Las pruebas del núcleo pueden usar reloj controlado, captura sintética, transporte en memoria y visor que registra llamadas. Cubrir aceptación tardía, invitaciones simultáneas, cambio de generación, revisión antigua, vídeo adelantado al anuncio y cancelación durante codificación. La prueba nativa posterior debe mostrar una fuente real y verificar cierre y liberación de recursos.

## Reglas compartidas de estado y propiedad

| Evento | Condición | Efecto obligatorio |
| --- | --- | --- |
| Selección completada | Referencia local aún pendiente y permiso vigente | Conservar la fuente y enviar una única invitación. |
| Aceptación recibida | Coincide con la invitación local vigente y dentro del plazo | Iniciar captura una sola vez. |
| Imagen nativa | Fuente disponible y operación aceptada | Asociar revisión antes de encolar codificación. |
| Cambio de tamaño | Misma fuente autorizada | Incrementar revisión, anunciar disponibilidad y reiniciar decodificación desde imagen clave. |
| Suspensión | Operación vigente | Incrementar revisión, vaciar colas y anunciar suspensión. |
| Resultado de trabajo | Coinciden referencia y revisión actuales | Entregarlo al siguiente componente. Descartarlo si cualquiera cambió. |
| Cierre local o remoto | Cualquier fase activa | Invalidar la operación y liberar recursos. Un cierre remoto termina sin reenviar otro cierre. |
| Permiso retirado y restablecido | Incluso dentro de la misma generación | Terminar la operación anterior. Una operación nueva recibe otro identificador. |

Cada imagen pertenece al componente que la recibe por valor. Un trabajo asíncrono conserva su propio búfer y una copia de referencia y revisión. El visor copia o retiene una imagen con propiedad explícita antes de devolver el control de `present`, cuyo argumento actual es una referencia prestada. Los buffers gráficos nativos se convierten o retienen de forma segura antes de salir del callback del sistema.

El orden de callbacks del adaptador debe preservar los cambios de estado respecto de los fotogramas. Un fotograma adquirido antes de suspender nunca se etiqueta con la revisión de una reanudación posterior. La implementación puede adjuntar un contador local de captura si el API nativo entrega resultados fuera de orden. Ese contador pertenece al adaptador y se valida antes de entregar al módulo.

La negociación de sesión deberá establecer una identidad de generación reconocida por ambos extremos o una traducción explícita entre sus identificadores locales. Los mensajes de respuesta conservan la identidad del iniciador de la operación. Al decodificar, `local` y `peer` se interpretan respecto del receptor. Añadir una prueba de ida y vuelta que incluya una invitación remota y su respuesta.

El mínimo recorrido integrado es selector real → aceptación remota → captura → codec → canal autenticado → decodificador → visor real → cierre desde cualquiera de los extremos. Las pruebas con dobles desbloquean el trabajo paralelo. La entrega funcional requiere después recorrer esa cadena con los adaptadores disponibles y declarar las plataformas pendientes.
