# Notificaciones nativas en Windows

Material privado del paquete de notificaciones. El integrador incorpora estos
requisitos al empaquetado del producto; el código de la aplicación no necesita
conocerlos salvo por el AppUserModelID compartido en
[`identity.hpp`](../../../features/notification_support/identity.hpp).

## Identidad

| Dato | Valor |
| --- | --- |
| AppUserModelID del producto | `OneInTwo.Desktop` |
| Nombre visible | `OneInTwo` |
| Capacidad de observación | `userNotificationListener` |
| Versión mínima práctica | Windows 10 1809 (17763) o posterior |

El AppUserModelID se declara en `src/features/notification_support/identity.hpp`
y debe coincidir exactamente con el del acceso directo o del paquete MSIX. Una
aplicación empaquetada expone su identidad como
`<PackageFamilyName>!OneInTwo.Desktop`; el filtro de eco reconoce la familia.

## Presentación

- **Aplicación empaquetada (camino de producto):** `ToastNotificationManager`
  usa la identidad del paquete. No hace falta acceso directo para mostrar el
  aviso, pero el manifiesto debe declarar la aplicación y su capacidad de
  notificaciones. La activación al pulsar no está implementada: el aviso carece
  de `launch`, botones y acciones, tal como exige el contrato.
- **Compilación sin empaquetar (desarrollo):** Windows exige un acceso directo
  del menú Inicio con `System.AppUserModel.ID = OneInTwo.Desktop`. Ejecutar, sin
  elevación:

  ```powershell
  powershell -ExecutionPolicy Bypass -File register-unpackaged-toast.ps1 `
      -TargetPath "C:\ruta\OneInTwo.exe"
  ```

  Sin ese acceso, `CreateToastNotifier` puede no lanzar una excepción y el aviso
  no mostrarse. El recorrido nativo debe confirmarse visualmente.

## Observación de avisos de otras aplicaciones

1. Empaquetar con MSIX e incorporar al `AppxManifest.xml` la capacidad de
   [`appxmanifest-capability.xml`](appxmanifest-capability.xml):

   ```xml
   <uap3:Capability Name="userNotificationListener" />
   ```

2. El adaptador anuncia `observe=true` únicamente si `GetCurrentPackageFullName`
   confirma identidad de paquete. Sin ella, `requestObservationPermission` y
   `startObserving` completan con `unavailable` y la interfaz debe mostrar la
   función como no disponible.
3. En el primer arranque con reenvío autorizado y una lista no vacía, el módulo
   llama a `startObserving`. El adaptador solicita consentimiento con
   `RequestAccessAsync` desde el hilo de interfaz del coordinador.
4. Si el usuario retira el consentimiento en **Configuración → Privacidad →
   Notificaciones**, el adaptador lo detecta en su sondeo de dos segundos,
   abandona la suscripción y entrega `state_changed(denied)`.
5. Al iniciar cada suscripción se lee la lista existente con
   `GetNotificationsAsync` y se usa como línea base: no se reenvía el historial
   del centro de notificaciones. Una reconexión o un regreso de suspensión
   reinicia la observación y toma una línea base nueva.

## Requisitos de integración

- Llamar a `winrt::init_apartment()` durante el arranque del hilo de interfaz o
  garantizar que Qt ya inicializó COM en ese hilo (Qt Widgets lo hace con
  `OleInitialize`). `RequestAccessAsync` debe invocarse desde ese hilo; el módulo
  ya lo hace desde el ejecutor principal.
- Conectar `NotificationDependencies::report_error` para reflejar la retirada de
  consentimiento en la interfaz. Las cabeceras compartidas no incluyen un evento
  de observación hacia el coordinador; el adaptador informa mediante
  `state_changed` al módulo, que emite el error local
  `observación de notificaciones denegada por el escritorio`. El coordinador
  debe volver a consultar `notificationCapabilities()` y actualizar el estado del
  control de reenvío.
- El módulo llama a `setAllowedApplications` con AppUserModelID locales. La
  interfaz debe poblar esa lista con los identificadores que entrega
  `ObservedNotification::app_id`; el nombre visible solo se usa para atribución.
- No registrar activadores de toast ni acciones: el receptor presenta texto
  plano y no permite respuestas remotas.

## Comprobación nativa pendiente

En un equipo Windows con MSVC y Windows SDK 10.0.17763 o posterior:

```powershell
cmake -S cmake/notifications -B build/notifications `
      -DONEINTWO_BUILD_TESTS=ON -DONEINTWO_BUILD_NATIVE_NOTIFICATION_TESTS=ON
cmake --build build/notifications --config Release
ctest --test-dir build/notifications -C Release --output-on-failure
build/notifications/tests/notifications/Release/oneintwo_notifications_native_windows.exe
```

El recorrido imprime las capacidades, presenta un aviso manual atribuido a
`EQUIPO-DE-PRUEBA`, inicia la observación si está disponible y espera un aviso
nuevo. Debe verificarse visualmente:

- [ ] El aviso muestra el título y el cuerpo Unicode y la línea `De EQUIPO-DE-PRUEBA`.
- [ ] No aparecen botones, enlaces ni acciones.
- [ ] Sin paquete ni acceso directo registrado, `presentNotice` completa con
      `unavailable` (código de salida 77) y no se muestra nada.
- [ ] Con paquete y consentimiento, `observe=true` y `startObserving` completa con
      `ready`.
- [ ] Un aviso nuevo de una aplicación autorizada llega una sola vez con su
      nombre visible; los avisos previos al inicio no se reenvían.
- [ ] Los avisos de OneInTwo no se reenvían (sin eco).
- [ ] Al retirar el consentimiento en Configuración, la observación se detiene en
      menos de cinco segundos y se informa al coordinador.
- [ ] `withdrawNotices` retira los avisos propios activos.
- [ ] Bloquear la sesión retira la autorización efectiva y los avisos pendientes
      no reaparecen al reanudar.

Registrar sistema, versión, método de empaquetado y resultado. Las pruebas
anteriores a esta lista no certifican Windows.
