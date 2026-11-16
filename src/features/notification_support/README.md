# Notificaciones entre equipos

Módulo común de avisos manuales y reenvío de notificaciones. Implementa la
lógica independiente de plataforma descrita en
[`docs/NOTIFICACIONES.md`](../../../docs/NOTIFICACIONES.md) y
[`docs/IMPLEMENTACION_PARALELA.md`](../../../docs/IMPLEMENTACION_PARALELA.md),
y se apoya en el contrato [`DesktopNotifications`](../../platform/desktop.hpp)
para presentar, observar y retirar avisos.

Este directorio contiene la política y los ayudantes de la feature. La fachada
pública está en [`../notifications.hpp`](../notifications.hpp) y la
implementación en [`../notifications.cpp`](../notifications.cpp).

## Capacidades

| Capacidad | Entrada principal | Estado |
| --- | --- | --- |
| Enviar un aviso manual | `SharedNotifications::sendManual` | Windows y Linux |
| Recibir avisos manuales o reenviados | `SharedNotifications::receiveNotice` | Windows y Linux |
| Reenviar avisos nuevos de otras aplicaciones | `forwardObserved` y observación nativa | Solo Windows con identidad MSIX y consentimiento |
| Descubrir aplicaciones con avisos actuales | `discoverApplications` | Windows; Linux devuelve `unavailable` |
| Elegir aplicaciones autorizadas | `setAllowedApplications` | Windows; en Linux no hay observación |
| Presentar y retirar avisos propios | `DesktopNotifications::presentNotice` / `withdrawNotices` | Windows y Linux |
| Aplicar permisos por modalidad y sentido | `setAuthorization` | Común |

Los avisos son texto plano. No se reenvían botones, enlaces, adjuntos,
respuestas ni acciones de la aplicación original. La recepción tampoco confirma
que una persona haya visto el aviso: `DesktopResult::ready` solo indica que el
escritorio aceptó la solicitud de presentación.

El perfil y la autorización efectiva los calcula el coordinador. El módulo
recibe un `FeatureSet` ya resuelto para envío y otro para recepción; no lee
preferencias ni conoce los nombres de los perfiles.

## Límites y política

La única fuente de la frecuencia es
[`notification_rate_per_second`](../../integration/feature_contracts.hpp).
No introducir otra cifra en la UI, la sesión o los tests.

| Límite | Valor |
| --- | --- |
| Frecuencia | 3 avisos por ventana de 1 segundo monotónico, por sentido |
| Cola local | 16 avisos pendientes por sentido |
| Caducidad en cola | 10 segundos desde la entrada |
| Deduplicación de observados | 64 huellas, 10 segundos |
| Título | 256 bytes UTF-8; obligatorio |
| Cuerpo | 2048 bytes UTF-8; opcional |
| Nombre visible de aplicación | 128 bytes UTF-8 |
| Tick del coordinador | 100 ms |

La frecuencia de envío es compartida por avisos manuales y reenviados. Un aviso
que supera el límite se descarta y se informa localmente; no se acumula para
intentarlo después. Las colas son acotadas y pierden el aviso nuevo cuando
están llenas. La caducidad se comprueba al reintentar el envío y justo antes de
presentar.

El receptor consume un identificador estructuralmente válido aunque después lo
descarte por frecuencia. Esto evita que una repetición del mismo
`notice_id` provoque otra presentación. No cambiar ese orden sin revisar las
pruebas de deduplicación.

## Implementación

### Fachada y estado

`SharedNotifications` es una fachada con un `Impl` privado. El estado relevante
vive por generación de sesión:

- contador de emisión y último identificador recibido;
- colas de salida y entrada con su instante de entrada;
- marcas monotónicas de frecuencia por sentido;
- huellas de avisos observados para deduplicar;
- lista de aplicaciones autorizadas;
- suscripción de observación y su identificador local.

Al cambiar `SessionGeneration` se retiran los avisos activos, se vacían colas,
se reinician contadores y deduplicación, y se detiene la observación anterior.
La generación se comprueba en todas las entradas y callbacks: un evento de una
generación o suscripción antigua se descarta.

### Recorridos

**Envío manual**

1. `sendManual` comprueba generación, permiso `manual_notifications` y contenido.
2. Se aplica la frecuencia, se asigna un `notice_id` y se construye un
   `RemoteNotification`.
3. `SendFeatureMessage` decide: `queued` transfiere la propiedad, `busy` deja el
   aviso en la cola local acotada, y `denied`/`disconnected` vacían el trabajo
   pendiente.

**Reenvío observado**

1. El adaptador entrega `ObservedNotification` con identidad local, nombre
   visible y contenido.
2. El módulo exige `forwarded_notifications`, una identidad no vacía, una
   aplicación de la lista, ausencia de eco y contenido válido.
3. Deduplica por `app_id`, título y cuerpo durante la ventana configurada.
4. Emite el aviso con el nombre visible; el `app_id` no sale del equipo.

**Recepción**

1. `receiveNotice` valida modalidad, generación, permiso, identificador y
   contenido.
2. Rechaza identificadores ya consumidos o no crecientes.
3. Avanza el último identificador, aplica frecuencia y encola.
4. `tick` comprueba caducidad y permisos antes de llamar a `presentNotice` con
   el nombre verificado del par.

### Observación

La observación solo se inicia si hay permiso de reenvío, una lista de
aplicaciones no vacía y `notificationCapabilities().observe == true`. La lista
puede cambiar durante la observación: se reevalúa al consumir cada evento y al
cambiar la autorización.

El adaptador Windows toma una línea base de los avisos existentes al empezar,
de modo que no se reenvía el historial. Una reconexión o una nueva generación
renueva esa línea base. El módulo no detecta por sí mismo una vuelta de
suspensión: el coordinador debe reiniciar la observación para no reenviar el
historial acumulado. El consentimiento se consulta periódicamente; si el usuario
lo retira, el módulo detiene la observación y reporta el cambio.

`discoverApplications` es una consulta independiente:

- no inicia ni modifica la observación;
- puede solicitar consentimiento y puede completar de forma síncrona o
  asíncrona;
- entrega el callback una sola vez;
- filtra identidades vacías, el propio producto, duplicados y nombres no
  utilizables, y ordena por nombre y luego por identidad.

Una lista vacía solo significa que no se encontraron avisos actuales; no
demuestra que no haya aplicaciones instaladas.

## Adaptadores de plataforma

### Windows

- Presenta con `ToastNotificationManager`.
- Con identidad MSIX usa la identidad del paquete; sin empaquetar usa el
  AppUserModelID del producto y requiere el acceso directo del menú Inicio.
- La observación usa `UserNotificationListener` y exige identidad de paquete y
  la capacidad `userNotificationListener` en el manifiesto.
- `RequestAccessAsync` se invoca desde el hilo de interfaz; la escucha y la
  presentación usan un hilo propio con apartamento MTA.
- Si no hay identidad de paquete, `observe` es falso y el descubrimiento
  devuelve `unavailable` antes de pedir consentimiento.
- El XML del toast se construye escapando el texto; no se interpreta marcado
  del usuario.

El empaquetado, el certificado y la lista de comprobación nativa están en
[`src/platform/windows/notification_support/README.md`](../../platform/windows/notification_support/README.md).

### Linux

- Presenta y retira mediante `org.freedesktop.Notifications`.
- El envío manual y la recepción autorizada funcionan con el servicio de
  notificaciones disponible.
- No existe un proveedor general autorizado para leer avisos de otras
  aplicaciones: `observe` es falso, `discoverApplications` devuelve
  `unavailable` y el reenvío automático no está disponible.
- La presentación usa texto plano y el nombre del par como atribución.

## Integración

Construir primero el runtime, el adaptador de escritorio y la sesión; después el
módulo. Las referencias de `NotificationDependencies` deben seguir vivas durante
toda la vida de `SharedNotifications`.

```cpp
NotificationDependencies dependencies{
    runtime,                        // now + post al hilo principal
    desktop,                        // makeWindowsNotifications / makeLinuxNotifications
    session.sendFeatureMessage,     // solo mensajes RemoteNotification
    [this] { return session.peerDisplayName(); },
    [this](std::string message) { ui.reportNotificationIssue(message); }};
auto notifications = std::make_unique<SharedNotifications>(dependencies);
```

Puntos que el coordinador debe respetar:

- Llamar a `tick()` cada 100 ms; sin tick no se reintentan colas ni se comprueba
  su caducidad.
- Llamar a `setAuthorization` antes de despachar trabajo y al cambiar de
  generación. Una reconexión debe usar una generación nueva.
- Al bloquear, desconectar o pausar, retirar la autorización efectiva; el módulo
  vacía los pendientes y detiene la observación si deja de estar permitida.
- Poblar la selección con `discoverApplications` y llamar después a
  `setAllowedApplications` con los `app_id` elegidos. No usar `app_name` como
  identidad ni `app_id` como texto para la persona usuaria.
- Tratar los callbacks como de un solo uso. `presentNotice` puede completar de
  forma asíncrona; un callback de una generación anterior se ignora.
- No llamar a `presentNotice` directamente para saltarse permisos, filtros,
  límites o deduplicación.
- No incluir título, cuerpo ni identidad nativa en `report_error`; el módulo ya
  evita hacerlo y la UI debe mantener esa garantía al registrar el mensaje.
- Mantener las cabeceras públicas bajo coordinación del integrador. Este
  paquete solo añade las ampliaciones documentadas de descubrimiento y usa la
  constante compartida de frecuencia.

## Problemas y factores importantes

- **Windows necesita empaquetado para observar.** Un ejecutable sin MSIX puede
  presentar avisos si el acceso directo del menú Inicio está registrado, pero no
  puede usar `UserNotificationListener`. El manifiesto debe declarar
  `userNotificationListener` y `runFullTrust`.
- **El consentimiento no es lo mismo que la capacidad.** `observe=true` indica
  soporte técnico; el permiso del sistema y el del par son estados distintos.
  La UI debe refrescar capacidades y autorización tras una revocación.
- **La observación no reproduce historial.** La línea base se toma al iniciar y
  se renueva al reiniciar la observación (reconexión o generación nueva). El
  módulo no recibe eventos de suspensión: tras reanudar, el coordinador debe
  reiniciar la observación para no reenviar el historial acumulado.
- **La lista de descubrimiento es una instantánea.** Solo incluye aplicaciones
  con avisos presentes en ese momento. Refrescar al abrir la selección; no
  interpretar una lista vacía como ausencia de aplicaciones.
- **La identidad nativa puede faltar.** Los avisos sin `app_id` fiable, sin
  nombre visible o del propio producto se descartan. El filtro de eco reconoce
  identificadores que empiezan por `oneintwo` seguido de `.`, `_` o `-`; si
  cambia la identidad del producto, actualizar también el manifiesto MSIX.
- **Las colas son deliberadamente con pérdida.** `queued` significa que la sesión
  aceptó el mensaje, no que el otro equipo lo haya presentado. No hay
  confirmación de entrega ni de lectura humana. Al desconectar, bloquear o
  cambiar de generación se descarta lo pendiente; no se persiste en disco.
- **La frecuencia es defensa en profundidad.** La sesión también aplica sus
  límites. Mantener ambas capas alineadas con
  `notification_rate_per_second`; no añadir literales independientes.
- **El orden de deduplicación importa.** El receptor avanza el identificador
  antes de aplicar frecuencia y conserva esa decisión. Cambiar el orden puede
  provocar repeticiones o descartes distintos a los probados.
- **El texto se valida antes de usarse.** `sanitize_plain_text` sustituye
  controles C0 y DEL, pero presupone UTF-8 válido; no usarlo como sustituto de
  `is_valid_utf8`. Un título compuesto solo por espacios ASCII, tabuladores o
  saltos de línea se rechaza.
- **La UI no debe mostrar errores crudos como contenido del aviso.** Los
  mensajes de `report_error` son estados locales recuperables y no incluyen
  título ni cuerpo; si se registran, conservar esa garantía.
- **El adaptador Linux no observa.** No prometer reenvío automático en Linux sin
  un proveedor autorizado. La prueba nativa puede omitirse con código 77 si no
  hay bus de sesión o servicio de notificaciones.
- **La verificación visual sigue siendo manual.** La CI comprueba compilación,
  pruebas y, en Windows, instalación/retirada del MSIX. No certifica que el
  toast se vea, que el usuario conceda consentimiento ni que el listener
  entregue avisos reales.

## Compilación y pruebas

El paquete y sus dependencias se describen en
[`cmake/notifications/README.md`](../../../cmake/notifications/README.md).

```bash
cmake -S cmake/notifications -B build/notifications \
      -DONEINTWO_BUILD_TESTS=ON -DONEINTWO_BUILD_NATIVE_NOTIFICATION_TESTS=ON
cmake --build build/notifications
ctest --test-dir build/notifications --output-on-failure
```

La suite cubre validación UTF-8 y límites, frecuencia, colas y caducidad,
deduplicación, filtros de identidad y eco, permisos por modalidad, cambios de
perfil, bloqueo, revocación antes de presentar, reconexión, descubrimiento
filtrado y asíncrono, y los ayudantes de presentación de ambos backends. Las
pruebas nativas que requieren escritorio, consentimiento o identidad de paquete
se ejecutan o se omiten según la plataforma; no sustituyen la comprobación
manual descrita en la documentación de Windows.
