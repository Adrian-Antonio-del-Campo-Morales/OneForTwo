# Paquete `oneintwo_notifications`

Implementación del envío manual, la recepción y el reenvío de avisos según
[`docs/NOTIFICACIONES.md`](../../docs/NOTIFICACIONES.md) y
[`docs/IMPLEMENTACION_PARALELA.md`](../../docs/IMPLEMENTACION_PARALELA.md).
Las cuatro cabeceras públicas no se modifican.

## Composición

| Elemento | Archivo |
| --- | --- |
| Módulo común y política de colas/frecuencia | `src/features/notifications.cpp`, `src/features/notification_support/` |
| Backend Windows (toast + `UserNotificationListener`) | `src/platform/windows/notifications.cpp`, `src/platform/windows/notification_support/` |
| Backend Linux (`org.freedesktop.Notifications`) | `src/platform/linux/notifications.cpp`, `src/platform/linux/notification_support/` |
| Target y pruebas | `cmake/notifications/`, `tests/notifications/` |

El target exporta `src` como include público y C++17. Selecciona un solo backend:
Windows enlaza `windowsapp` y `shell32`; Linux enlaza `dbus-1`. La otra fábrica
declarada en `feature_contracts.hpp` queda sin definir, como exige la base común.

```bash
cmake -S cmake/notifications -B build/notifications \
      -DONEINTWO_BUILD_TESTS=ON -DONEINTWO_BUILD_NATIVE_NOTIFICATION_TESTS=ON
cmake --build build/notifications
ctest --test-dir build/notifications --output-on-failure
```

En Ubuntu, `libdbus-1-dev` debe estar instalado o indicarse `DBUS_INCLUDE_DIR`,
`DBUS_ARCH_INCLUDE_DIR` y `DBUS_LIBRARY`. En Windows se requiere MSVC 2019+,
Windows SDK 10.0.17763+ (C++/WinRT) y el empaquetado descrito en
`src/platform/windows/notification_support/README.md`.

## Conexión con sesión y UI

Construir el adaptador de escritorio antes que el módulo y mantener las
referencias de `NotificationDependencies` vivas durante toda la vida del módulo.

```cpp
NotificationDependencies dependencies{
    runtime,                       // now + post al hilo principal
    desktop,                       // makeWindowsNotifications / makeLinuxNotifications
    session.sendFeatureMessage,    // SendFeatureMessage: solo mensajes RemoteNotification
    [this] { return session.peerDisplayName(); }, // nombre verificado del par
    [this](std::string message) { ui.reportNotificationIssue(message); }};
auto notifications = std::make_unique<SharedNotifications>(dependencies);
```

| Momento | Llamada |
| --- | --- |
| Cada 100 ms desde el coordinador | `tick()` |
| Autorización, perfil o generación nuevos | `setAuthorization(generation, effectiveSend, effectiveReceive)` |
| Bloqueo o desconexión | `setAuthorization(generation, {}, {})` |
| Reconexión | Generación nueva + `setAuthorization`; reinicia contadores, colas y observación |
| Lista de aplicaciones del formulario | `setAllowedApplications(std::set<std::string>)` (AppUserModelID locales) |
| Enviar desde el formulario | `sendManual(generation, {título, cuerpo})` |
| Aviso observado por el adaptador | Interno: `startObserving` → filtro → `forwardObserved` |
| `RemoteNotification` recibido de la sesión | `receiveNotice(notice)` |

Semántica que el coordinador debe respetar:

- Toda entrada con una generación distinta de la vigente se descarta sin error.
  Al cambiar de generación se vacían colas, se reinician contadores y se toma
  una línea base nueva de observación.
- El módulo solo emite `RemoteNotification`; la sesión codifica y aplica sus
  límites. `queued` transfiere la propiedad; `busy` deja el aviso en la cola
  local acotada; `disconnected` y `denied` vacían el trabajo pendiente.
- El perfil **Solo envíos** se expresa retirando `forwarded_notifications` en
  ambos sentidos y `manual_notifications` en recepción. **Pausa** retira todo.
- La observación se inicia solo con permiso de reenvío, lista no vacía y
  `notificationCapabilities().observe`. Un cambio de lista con ambos válidos
  mantiene la suscripción y se reevalúa al consumir cada evento.
- La UI puebla la selección con `discoverApplications`: es una consulta
  asíncrona que puede solicitar consentimiento, no inicia la observación y
  devuelve aplicaciones únicas, ordenadas y filtradas. Un resultado distinto de
  `ready` no incluye lista.
- La UI obtiene de `notificationCapabilities()` si el escritorio puede presentar
  y observar. `requestObservationPermission` permite un botón explícito de
  concesión; su resultado es asíncrono. La retirada de consentimiento llega al
  coordinador por `DesktopEvents::capabilities_changed` en Windows y por el error
  local del módulo.
- `report_error` nunca incluye título ni cuerpo; sirve para estados
  recuperables (permiso, límites, cola, presentación).

## Decisiones y límites de contrato

- **Frecuencia:** `docs/IMPLEMENTACION_PARALELA.md` fija tres avisos por ventana
  de un segundo monotónico. `feature_contracts.hpp` declara esa misma cifra
  (`notification_rate_per_second = 3`) y la política local la usa como única
  fuente. Las pruebas comprueban el límite efectivo en envío y recepción.
- **Descubrimiento de aplicaciones:** resuelto con
  `SharedNotifications::discoverApplications` y
  `DesktopNotifications::discoverApplications`. Devuelve
  `NotificationApplication {app_id, app_name}` a partir de los avisos existentes
  en el escritorio. No se arranca la observación con una lista vacía: el
  descubrimiento es una consulta aparte, puede repetirse y no altera la línea
  base ni la autorización.
- **Estado de observación:** el contrato no expone un evento de observación hacia
  la UI. El backend Windows informa `capabilities_changed` y el módulo emite el
  error local; la UI debe reflejar el estado del control de reenvío a partir de
  capacidades, autorización y esos errores.
- **Linux:** no existe un proveedor autorizado para leer avisos ajenos. Se anuncia
  `observe=false`, `startObserving` completa con `unavailable` y se conservan
  envío manual, recepción y retirada.

## Evidencia de este paquete

- 49 casos en `oneintwo_notifications_tests`: UTF-8 y límites exactos, título
  obligatorio, frecuencia, colas y caducidad, deduplicación, filtros de
  identidad y eco, permisos por modalidad, cambio de perfil, bloqueo,
  revocación antes de presentar, reconexión, descubrimiento filtrado y
  asíncrono, presentación y retirada del adaptador Linux con servicio
  controlado, y ayudantes de la línea base y el texto XML de Windows.
- Recorrido Linux con bus de sesión privado y servicio mínimo: `Notify` real con
  Unicode y atribución `De EQUIPO-DE-PRUEBA`, `CloseNotification` y
  `observe=false`. Sin escritorio gráfico no se certifica la presentación visual.
- Windows: el backend y su material de empaquetado están implementados, pero no
  se compilaron ni ejecutaron en este entorno Linux. La lista de comprobación
  nativa está en `src/platform/windows/notification_support/README.md`.
