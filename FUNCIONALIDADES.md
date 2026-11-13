# OneInTwo — Funciones propuestas

Diseño del 13 de noviembre de 2026. Las funciones tienen contratos iniciales en el esqueleto de la aplicación. La arquitectura de referencia describe la [sesión y sus permisos](ARQUITECTURA.md#41-sesión-y-mensajes).

| Función | Acción del usuario | Dependencia |
| --- | --- | --- |
| Envío directo de archivos | Elegir archivos o carpetas en OneInTwo y enviarlos al equipo vinculado. El destinatario elige dónde guardarlos. | Sesión de producto y módulo de archivos |
| Envío de enlaces | Escribir o pegar una URL en OneInTwo y enviarla. El destinatario decide si la abre. | Sesión de producto y apertura local de URL |
| Perfiles de uso | Alternar entre uso habitual, solo envíos y pausa desde la ventana principal. | Permisos efectivos de sesión y preferencias locales |
| [Compartir pantalla](docs/COMPARTIR_PANTALLA.md) | Mostrar en directo un monitor local en una ventana del otro equipo. | Captura de monitor, sesión y visor |
| [Compartir ventana](docs/COMPARTIR_VENTANA.md) | Mostrar únicamente la ventana elegida de una aplicación. | Captura nativa de ventana, sesión y visor |
| [Notificaciones](docs/NOTIFICACIONES.md) | Enviar avisos y, cuando el escritorio lo admita, reenviar los de aplicaciones elegidas. | Sesión y adaptador de notificaciones |

## 1. Envío directo de archivos

**Alcance inicial.** Desde OneInTwo, el origen selecciona archivos y carpetas ordinarios y pulsa **Enviar archivos**. El otro equipo muestra el nombre del remitente vinculado, el nombre del primer elemento, el número total de elementos de primer nivel y el tamaño total en bytes. El destinatario puede rechazar la oferta o pulsar **Guardar en…** y seleccionar una carpeta. Cerrar el selector devuelve a la oferta pendiente. Ambos equipos ven el progreso y pueden cancelar.

**Contrato.** La sesión negocia una capacidad y un permiso por sentido específicos para el envío directo. El origen crea una oferta de archivos con modalidad «envío directo», vinculada a la generación de sesión y a un identificador de oferta. La oferta inicial contiene un resumen acotado. El destinatario valida el resumen antes de mostrarlo. Aceptar con un destino válido habilita el manifiesto y los bloques por el flujo común de archivos. El destino pertenece al receptor y permanece local. Las solicitudes de contenido se limitan a los elementos de la oferta aceptada. El módulo de archivos reutiliza sus reglas de rutas, colisiones, temporales, integridad y cancelación.

**Estados.** Oferta pendiente → aceptada → transferencia → verificación → guardada. Desde un estado activo puede pasar a rechazada, cancelada o con error según corresponda. Antes de aceptar, una desconexión, un cambio de generación o la pérdida del permiso retiran la oferta. Tras la aceptación, esos eventos cancelan el trabajo pendiente y limpian sus temporales. Los resultados confirmados antes de la cancelación se conservan. Un nuevo intento tras un corte requiere otra oferta y otra aceptación.

**Criterios de aceptación**

- En ambos sentidos entre Windows y Ubuntu, guardar una carpeta con nombres Unicode, subcarpetas y archivos vacíos. Comparar estructura, tamaños y SHA-256.
- Con el portapapeles de archivos desactivado, completar un envío directo con su propio permiso. Rechazar la misma oferta si el receptor deshabilita el permiso directo.
- Rechazar una oferta, cancelar la selección de destino y cortar una transferencia activa. Verificar respectivamente que no empieza la transferencia, que la oferta vuelve a mostrarse y que se limpian los temporales sin presentar resultados incompletos como guardados.
- Repetir un envío al mismo destino y comprobar que se aplica la política común de colisiones y se conservan los datos existentes.

## 2. Envío de enlaces

**Alcance inicial.** El usuario introduce una URL y pulsa **Enviar enlace**. El destinatario ve el nombre del equipo vinculado, el host y la dirección completa en texto, con las acciones **Abrir** y **Descartar**. Abrir lanza el navegador predeterminado del equipo receptor mediante su adaptador local. La interfaz de OneInTwo muestra la oferta sin cargar vistas previas ni solicitar contenido al sitio.

**Contrato.** La sesión aplica un permiso por sentido específico de enlaces. La oferta lleva una generación, un identificador creciente dentro de esa generación y una URL de hasta 4096 bytes en UTF-8. Ambos extremos validan con el mismo analizador de URL. Se admiten direcciones absolutas `http` y `https` con host válido. Se rechazan credenciales incrustadas, caracteres de control y direcciones mal formadas. La confirmación muestra la URL normalizada y el host en forma ASCII cuando sea internacionalizado. Al abrir se usa exactamente la URL validada que se mostró. El contenido solo vive en memoria mientras la oferta está activa. Los registros guardan el tipo de resultado, sin la URL.

**Estados.** Cada receptor mantiene como máximo una oferta pendiente durante 60 segundos desde que la presenta, medidos con reloj monotónico. Una segunda oferta no sustituye la pendiente. El receptor registra el último identificador válido de la generación, incluidas las ofertas descartadas por estar ocupado. Así, una oferta repetida no aparece después de liberar la anterior. Abrir o descartar consume la oferta. La desconexión, la revocación, el cambio de generación y el bloqueo de pantalla también la retiran. Antes de abrir se comprueban de nuevo el permiso, la generación y la vigencia. Si el sistema rechaza la apertura, la interfaz indica el error local. El estado «Enviado» del emisor se refiere al envío, y el protocolo no comunica si el destinatario abrió el enlace.

**Criterios de aceptación**

- En ambos sentidos, presentar una URL HTTPS y verificar que se abre una sola vez al pulsar **Abrir**, incluso si se repite la acción o se duplica la oferta.
- Rechazar esquemas distintos de HTTP y HTTPS, credenciales en la URL, caracteres de control y direcciones por encima del límite. Comprobar con un adaptador de prueba que el navegador solo recibe la URL aprobada.
- Dejar caducar la oferta, bloquear la pantalla y revocar el permiso entre presentación y confirmación. En cada caso, retirar la acción pendiente y comprobar que el adaptador no recibe una solicitud de apertura.
- Confirmar que una reconexión no vuelve a mostrar la oferta anterior y que el registro no contiene la URL.

## 3. Perfiles de uso

**Alcance inicial.** La ventana principal ofrece tres perfiles predefinidos y muestra qué capacidades están realmente disponibles. Cada equipo elige su propio perfil para el par vinculado.

| Perfil | Portapapeles y entrada | Archivos directos, enlaces y avisos manuales | Pantalla, ventana y reenvío automático |
| --- | --- | --- | --- |
| Habitual | Permitidos por el perfil | Permitidos por el perfil | Permitidos por el perfil |
| Solo envíos | Pausados | Permitidos por el perfil | Pausados |
| Pausa | Pausados | Pausados | Pausados |

**Contrato.** El perfil limita tanto el envío como la recepción. La sesión calcula las capacidades efectivas para cada sentido intersectando el perfil local, los permisos del par, las capacidades admitidas en ambos equipos y las restricciones vigentes del escritorio y del otro extremo. Un cambio de perfil jamás concede permisos. Los permisos de envío directo, enlaces, pantalla, ventana y notificaciones requieren habilitación explícita. El coordinador solicita el cambio y los módulos reciben las retiradas de capacidad. La sesión anuncia el resultado mediante el control de permisos existente; el nombre del perfil solo se guarda en preferencias locales. El perfil seleccionado se recuerda por par. Un par sin preferencia empieza en **Habitual** y un valor almacenado inválido se interpreta como **Pausa**.

**Transición.** La sesión cierra primero el acceso local a las capacidades que salen del perfil. Los módulos liberan teclado y ratón, retiran las ofertas y cancelan los trabajos afectados. Las acciones tardías se descartan aunque pertenezcan a la misma generación de red. Las nuevas capacidades entran en servicio tras confirmar la actualización de permisos en el canal interactivo ordenado. El coordinador descarta trabajo ya encolado de las capacidades retiradas y los módulos invalidan sus identificadores. Los datos que lleguen por el canal secundario requieren una transferencia todavía activa. **Pausa** puede conservar la conexión autenticada. Salir de pausa admite operaciones nuevas y descarta cualquier intento de recuperar el control, ofertas o contenido de la etapa anterior.

**Criterios de aceptación**

- Pasar a **Solo envíos** con un control remoto activo. Verificar la liberación de teclas y botones y comprobar que un envío directo autorizado sigue disponible.
- Pasar a **Pausa** con un envío en curso y un enlace pendiente. Comprobar la cancelación y limpieza, el retiro de la oferta de enlace y el rechazo de eventos tardíos.
- Volver a **Habitual** después de revocar un permiso remoto. Comprobar que la capacidad sigue indisponible, que el resto se negocia y que el perfil seleccionado se recupera al reiniciar.
- Simular un perfil desconocido en preferencias y comprobar que se aplica **Pausa**.

## Puntos de implementación

| Función | Propietario y contrato | Integración |
| --- | --- | --- |
| Envío directo | [Contrato de archivos](src/features/files.hpp) e implementación en [files.cpp](src/features/files.cpp) | La ventana recoge la selección y el destino. El coordinador conecta esas acciones con archivos. La sesión distingue la modalidad al autorizar ofertas, aceptación, bloques y cancelación. |
| Enlaces | [Contrato de enlaces](src/features/links.hpp) e implementación en [links.cpp](src/features/links.cpp) | El protocolo lleva la oferta. La sesión comprueba permisos y generación. La ventana presenta la confirmación y el adaptador de escritorio abre la URL aprobada. |
| Perfiles | [Contrato de perfiles](src/features/profiles.hpp) y reglas en [profiles.cpp](src/features/profiles.cpp) | El coordinador aplica la selección. [Sesión](src/core/session.cpp) calcula los permisos efectivos y [almacenamiento](src/storage/local_store.cpp) conserva la elección local. |

Primero se necesita la sesión integrada con permisos y revocación. A partir de esa base, perfiles y enlaces pueden validarse con dobles de escritorio y transporte. El envío directo requiere además la transferencia común de archivos y puede comprobarse en ambos equipos mediante el selector de destino.
