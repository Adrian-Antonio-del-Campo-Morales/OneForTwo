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
   confirma identidad de paquete. Sin ella, `requestObservationPermission`,
   `discoverApplications` y `startObserving` completan con `unavailable` y la
   interfaz debe mostrar la función como no disponible.
3. Para poblar la lista de aplicaciones, la interfaz llama a
   `SharedNotifications::discoverApplications`. Es una consulta independiente:
   puede solicitar consentimiento, enumera las aplicaciones con avisos actuales
   y no inicia la observación ni altera su línea base. La lista puede refrescarse
   cuando el usuario vuelva a abrir la selección.
4. En el primer arranque con reenvío autorizado y una lista no vacía, el módulo
   llama a `startObserving`. El adaptador solicita consentimiento con
   `RequestAccessAsync` desde el hilo de interfaz del coordinador.
5. Si el usuario retira el consentimiento en **Configuración → Privacidad →
   Notificaciones**, el adaptador lo detecta en su sondeo de dos segundos,
   abandona la suscripción y entrega `state_changed(denied)`.
6. Al iniciar cada suscripción se lee la lista existente con
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
- La interfaz obtiene la selección con `discoverApplications` y llama a
  `setAllowedApplications` con los `NotificationApplication::app_id` elegidos.
  El nombre visible solo se usa para atribución.
- No registrar activadores de toast ni acciones: el receptor presenta texto
  plano y no permite respuestas remotas.

## Paquete MSIX de comprobación (CI)

El job `msix` del workflow de validación compila el recorrido nativo con CRT
estático, lo empaqueta con [`msix/AppxManifest.xml`](msix/AppxManifest.xml) y
[`msix/New-NativeCheckPackage.ps1`](msix/New-NativeCheckPackage.ps1), y publica
el artefacto `oneintwo-notifications-msix` con:

- `OneInTwo-NativeCheck.msix` firmado con un certificado efímero de prueba.
- `OneInTwo-NativeCheck.cer` para confiar en él solo durante la comprobación.
- `install.txt` con los pasos de instalación y arranque.
- `validation.txt` y los logs de `MakeAppx`/`signtool`.

El script comprueba antes de firmar que el manifiesto declara
`userNotificationListener` en `uap3`, `runFullTrust` en `rescap`, la aplicación
full-trust, el ejecutable y los recursos; después empaqueta con `MakeAppx`,
firma con `signtool` y comprueba el contenedor MSIX: manifiesto empaquetado,
ejecutable, recursos y `AppxSignature.p7x`. La verificación criptográfica y la
instalación se completan en el equipo Windows de comprobación. El `.pfx` privado
no se publica.

El paquete usa la identidad `OneInTwo.Notifications.NativeCheck` y el
`Application Id` `NativeCheck`. No es el paquete de producto ni sustituye al
manifiesto del integrador.

## Descubrimiento de aplicaciones

### Sin identidad de paquete (automática)

`notifications-windows-discovery-no-package` es un ejecutable nativo que crea el
adaptador sin MSIX, comprueba que `notificationCapabilities().observe` es falso,
llama a `discoverApplications` y exige `DesktopResult::unavailable` con una
lista vacía. No presenta avisos ni abre ningún diálogo. Se ejecuta en el job
`msvc` de CI y puede repetirse con:

```powershell
ctest --test-dir build/notifications -C Release `
      -R notifications-windows-discovery-no-package --output-on-failure
```

Si el proceso tuviera identidad de paquete, el ejecutable devuelve 77 y CTest lo
marca como omitido: esa variante corresponde a la comprobación manual siguiente.

### Con consentimiento real (manual, MSIX)

Requiere instalar el paquete firmado del artefacto
`oneintwo-notifications-msix`; no se ejecuta en CI porque el runner no instala
MSIX ni puede interactuar con el diálogo de consentimiento. Pasos:

1. Instalar el certificado y el MSIX siguiendo `install.txt`, y abrir la
   aplicación NativeCheck.
2. En la primera llamada a `discoverApplications`, Windows muestra el
   consentimiento de **Configuración → Privacidad → Notificaciones**. Aceptarlo.
3. Confirmar que la llamada completa una sola vez con `ready` y una lista de
   aplicaciones con avisos actuales. La lista puede estar vacía si no hay avisos;
   no debe incluir OneInTwo ni duplicados, y el descubrimiento no debe iniciar la
   observación ni alterar su línea base.
4. Retirar el consentimiento en Configuración, repetir el descubrimiento y
   confirmar que completa con `denied` o `unavailable`, sin lista, sin bloquear
   el proceso y sin iniciar la observación.
5. Registrar sistema, versión de Windows, resultado y si el diálogo apareció una
   sola vez.

## Comprobación nativa pendiente

En un equipo Windows con MSVC y Windows SDK 10.0.17763 o posterior:

```powershell
cmake -S cmake/notifications -B build/notifications `
      -DONEINTWO_BUILD_TESTS=ON -DONEINTWO_BUILD_NATIVE_NOTIFICATION_TESTS=ON
cmake --build build/notifications --config Release
ctest --test-dir build/notifications -C Release --output-on-failure
build/notifications/tests/notifications/Release/oneintwo_notifications_native_windows.exe
```

El recorrido imprime las capacidades, consulta `discoverApplications` (sin
paquete debe devolver `unavailable`), presenta un aviso manual atribuido a
`EQUIPO-DE-PRUEBA`, inicia la observación si está disponible y espera un aviso
nuevo. Para la variante empaquetada, descargar el artefacto
`oneintwo-notifications-msix` y seguir `install.txt`; así el ejecutable tiene
identidad de paquete y puede solicitar la observación. Debe verificarse
visualmente:

- [ ] El aviso muestra el título y el cuerpo Unicode y la línea `De EQUIPO-DE-PRUEBA`.
- [ ] No aparecen botones, enlaces ni acciones.
- [ ] Sin paquete ni acceso directo registrado, `presentNotice` completa con
      `unavailable` (código de salida 77) y no se muestra nada.
- [ ] Con paquete y consentimiento, `observe=true` y `startObserving` completa con
      `ready`.
- [ ] `discoverApplications` sin paquete completa con `unavailable` (prueba
      automática `notifications-windows-discovery-no-package`).
- [ ] Con paquete y consentimiento, `discoverApplications` completa con `ready`,
      enumera las aplicaciones con avisos actuales, excluye OneInTwo y no inicia
      la observación.
- [ ] Tras retirar el consentimiento, `discoverApplications` completa con
      `denied` o `unavailable` y no entrega aplicaciones.
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
