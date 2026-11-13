# Recorrido nativo de notificaciones

## Ubuntu / Linux

`linux_presentation` usa el servicio real de `org.freedesktop.Notifications`. Sin
una sesión gráfica devuelve 77 y CTest lo registra como omitido; no se cuenta como
aprobado.

Para comprobar la codificación real del bus y la retirada sin un escritorio, se
puede levantar un bus de sesión privado y el servicio mínimo:

```bash
cmake -S cmake/notifications -B build/notifications \
      -DONEINTWO_BUILD_TESTS=ON -DONEINTWO_BUILD_NATIVE_NOTIFICATION_TESTS=ON
cmake --build build/notifications

dbus-run-session -- bash -c '
  ./build/notifications/tests/notifications/oneintwo_notifications_dbus_stub &
  stub=$!
  sleep 0.5
  ./build/notifications/tests/notifications/oneintwo_notifications_native_linux
  status=$?
  sleep 4
  kill $stub
  wait $stub 2>/dev/null
  exit $status
'
```

Resultado esperado con el servicio mínimo: `present=1 observe=0`, presentación
`aceptada por el servicio`, `CloseNotification id=41` en el servicio y código 0.
Esto valida el recorrido de presentación y retirada del adaptador; no demuestra
la presentación visual de un escritorio real ni el reenvío automático, que Linux
no ofrece con el adaptador base.

En una sesión de escritorio real (GNOME/KDE con un demonio de notificaciones) el
mismo ejecutable debe mostrar el aviso con la atribución `De EQUIPO-DE-PRUEBA` y
confirmar visualmente que no hay botones ni enlaces.

## Windows

El ejecutable `oneintwo_notifications_native_windows` y su lista de comprobación
están descritos en
[`src/platform/windows/notification_support/README.md`](../../../src/platform/windows/notification_support/README.md).
