# Enviar notificaciones entre equipos

La [base de implementación paralela](IMPLEMENTACION_PARALELA.md) fija las firmas públicas, los propietarios, el codec, los límites y el protocolo de esta entrega. Sus decisiones concretan las propuestas de esta guía.

## Flujo de uso

| Modalidad | Origen | Receptor |
| --- | --- | --- |
| Manual | En OneInTwo, pulsar **Enviar notificación**, escribir un título y, opcionalmente, un mensaje. | Recibe una notificación local de OneInTwo atribuida al equipo vinculado. |
| Automática | Activar **Reenviar notificaciones** y elegir las aplicaciones autorizadas en un escritorio que permita observarlas. A partir de ese momento se reenvían los avisos nuevos de esas aplicaciones. | Recibe una notificación local de OneInTwo atribuida al equipo vinculado y con el nombre de la aplicación como texto. |

El contenido es texto plano. La notificación recibida carece de enlaces activos, botones de la aplicación original, adjuntos y respuestas remotas. El receptor aplica sus ajustes de notificaciones, incluido **No molestar**. El emisor puede saber si el envío entró en el canal local. El protocolo no confirma que una persona haya visto el aviso.

La primera entrega de producto contempla envío manual y presentación nativa en Windows y Ubuntu. El reenvío automático es una capacidad adicional del origen. Un origen Ubuntu con el adaptador base puede enviar manualmente y recibir tanto avisos manuales como avisos reenviados por Windows. El control de reenvío muestra **No disponible** cuando falta un proveedor de observación compatible.

## Permisos, contenido y vigencia

Los avisos manuales y los automáticos tienen permisos separados por sentido en cada par y empiezan desactivados. El perfil **Habitual** permite ambos y **Solo envíos** permite avisos manuales. **Pausa** suspende ambos. El reenvío requiere además el permiso de lectura del escritorio de origen y una lista local de aplicaciones vacía por defecto. Los avisos de OneInTwo y los que carezcan de una identidad de aplicación fiable se excluyen en origen. Al activar la observación se registra el estado previo para reenviar solo incorporaciones nuevas, sin reproducir avisos del centro de notificaciones.

Cada aviso lleva la generación de sesión, un identificador creciente por sentido, su modalidad, título y cuerpo. Los automáticos añaden un nombre visible de la aplicación de origen. El nombre verificado del equipo se obtiene localmente de la sesión. Los límites UTF-8 son 256 bytes para el título, 2048 para el cuerpo y 128 para el nombre visible. El título es obligatorio. Se rechazan datos mal formados o fuera de límite y se escapa el marcado al presentar. El identificador nativo de la aplicación se queda en origen.

El módulo descarta repeticiones, avisos antiguos y avisos recibidos durante bloqueo. Los pendientes caducan a los diez segundos de espera en cada cola local y se aplica un límite de frecuencia al enviar y recibir. El exceso se descarta para que el canal interactivo siga disponible. Una desconexión, un bloqueo o una retirada de permiso descartan avisos pendientes y detienen la observación si queda desautorizada. La interfaz retira sus avisos activos cuando el escritorio ofrece esa operación. El sistema puede gestionar su propio historial de notificaciones. OneInTwo solo persiste preferencias, permisos y aplicaciones autorizadas, y registra estados y errores sin contenido.

## Arquitectura básica

| Capa | Responsabilidad |
| --- | --- |
| [Sesión](../src/core/session.hpp) y [protocolo](../src/net/protocol.hpp) | Negociar modalidades por sentido, validar estructura y tamaño, y despachar avisos autorizados por el canal interactivo. La autorización se vuelve a comprobar antes de presentar. |
| [Módulo de notificaciones](../src/features/notifications.hpp) | Normalizar el envío manual y el evento nativo. Filtrar aplicaciones, evitar ecos y duplicados, aplicar límites y coordinar la presentación. |
| [Windows](../src/platform/windows/notifications.cpp) | Probar presentación nativa de OneInTwo. Evaluar `UserNotificationListener` para avisos de otras aplicaciones, con la capacidad de manifiesto, consentimiento local y compatibilidad con el empaquetado del producto. |
| [Ubuntu/Wayland](../src/platform/linux/notifications.cpp) | Presentar avisos propios mediante `org.freedesktop.Notifications`. La interfaz estándar descrita permite publicar y gestionar avisos, y carece de una operación general para leer los de otras aplicaciones. La observación automática queda pendiente de un proveedor autorizado. |
| [Coordinador](../src/app/application.cpp) y [configuración](../src/ui/main_window.cpp) | Conectar el formulario, los permisos, el estado de disponibilidad y la lista de aplicaciones con sesión y escritorio. |

Documentación de los adaptadores. [Windows Notification Listener](https://learn.microsoft.com/en-us/windows/apps/develop/notifications/app-notifications/notification-listener) · [Especificación de notificaciones de escritorio](https://specifications.freedesktop.org/notification/latest/protocol.html)

## Criterios de aceptación

- Enviar manualmente texto Unicode en ambos sentidos. Comprobar que se muestra una sola vez, con el equipo de origen, y que se respeta **No molestar**.
- Habilitar el reenvío desde Windows si el listener y su permiso están disponibles. Verificar que se recibe un aviso nuevo de una aplicación autorizada y que quedan fuera los avisos previos, los de aplicaciones excluidas y los de OneInTwo.
- Duplicar avisos, exceder los límites, retirar permisos, cambiar a **Solo envíos** y bloquear o desconectar un equipo. Verificar el descarte correspondiente, la ausencia de eco y que los avisos pendientes no reaparecen al reconectar.
- En Ubuntu sin proveedor de observación, comprobar que el reenvío automático se presenta como no disponible, mientras funcionan los avisos manuales y la recepción autorizada.

## Guía de implementación

El coordinador inyecta en `SharedNotifications` el adaptador nativo, la salida de mensajes de sesión, un reloj monotónico y las preferencias de aplicaciones permitidas. Completar el constructor y las operaciones de configuración al implementar. El adaptador publica cambios de disponibilidad y revocación para que la sesión recalcule permisos efectivos.

| Entrada | Recorrido |
| --- | --- |
| Formulario | `sendManual` valida texto y permiso, asigna identificador y solicita envío a la sesión. |
| Evento del escritorio | `forwardObserved` comprueba identidad de aplicación, lista permitida y exclusión de OneInTwo antes de construir el aviso remoto. |
| Mensaje remoto | La sesión valida estructura y permiso. `receiveNotice` comprueba generación, orden, límites y vigencia, obtiene la identidad local del par y solicita presentación. |
| Retirada de permiso | `setAuthorization` actualiza el estado, descarta pendientes afectados y detiene observación cuando deja de estar permitida. |
| Bloqueo o desconexión | El coordinador retira autorización efectiva y cancela pendientes. Al reconectar se inicia otra generación y otra observación de novedades. |

Mantener el contador de emisión, el último identificador recibido, las colas con su instante de entrada y el estado de observación por generación. El envío manual y el automático comparten el contador por sentido. Definir una política concreta de frecuencia y capacidad de cola antes de implementar el transporte, documentarla y probar el descarte de exceso.

La observación necesita distinguir la carga inicial de los avisos nuevos y reconocer sus identificadores nativos mientras esté activa. Mantener esa deduplicación en el adaptador. Tras volver de una suspensión, tomar una nueva línea base para evitar reenviar el historial acumulado.

En Windows, verificar primero presentación y empaquetado con un aviso propio. Después conectar lectura y permiso del listener. En Ubuntu, implementar presentación y retirada mediante el servicio disponible. Cada backend entrega una fábrica local del contrato `DesktopNotifications` para la composición de producto.

Las pruebas aisladas deben controlar tiempo y autorización y registrar llamadas al adaptador. Cubrir texto UTF-8 en el límite, aviso repetido, generación antigua, cola caducada, app excluida y retirada justo antes de presentar. La validación nativa comprueba atribución, configuración del escritorio, novedades y ausencia de eco. Registrar el soporte automático realmente disponible por plataforma.

## Revocación, resultados y observación

La consulta de capacidades describe soporte técnico. El consentimiento del sistema y el permiso del par forman estados distintos que la sesión combina antes de autorizar. La retirada del consentimiento debe generar un evento hacia el coordinador aunque no llegue un aviso nuevo.

Al implementar `DesktopNotifications`, completar el contrato con un callback de estado de observación y resultado de inicio. Asociar cada suscripción a un identificador local creciente. `stopObserving` invalida ese identificador antes de desconectar el backend. Cualquier evento ya encolado de una suscripción anterior se descarta, incluso si después se vuelve a habilitar el permiso.

La lista de aplicaciones permitidas puede cambiar durante la observación. Volver a comprobarla al consumir cada evento pendiente. El filtro usa la identidad local fiable de aplicación. El nombre visible se emplea únicamente para atribución en la presentación.

`presentNotice` debe expresar si el escritorio aceptó la solicitud. La firma común ya utiliza un callback asíncrono de resultado. El llamador conserva la referencia del aviso en ese callback. El estado de presentación del sistema y la lectura humana son resultados independientes. Un fallo de presentación se comunica localmente con su motivo y descarta el aviso consumido.

La deduplicación avanza al consumir un identificador estructuralmente válido y autorizado, incluido un aviso descartado por límite de frecuencia. Así, repetirlo posteriormente no provoca otra presentación. Una nueva generación reinicia el contador y elimina los pendientes anteriores. Las comprobaciones de expiración se ejecutan justo antes del envío y de la presentación.

Como prueba de integración mínima, conectar el formulario real con sesión, codificación de mensaje y presentación nativa en el otro extremo. Después activar observación de una aplicación autorizada y comprobar que un único evento atraviesa el mismo recorrido.
