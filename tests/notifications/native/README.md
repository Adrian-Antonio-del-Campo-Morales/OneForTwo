# Native route of notifications

## Ubuntu / Linux

`linux_presentation` uses the actual `org.freedesktop.Notifications` service. Without a graphical
session it returns 77 and CTest records it as omitted; it is not counted as approved.

To check the actual coding of the bus and the withdrawal without a desk, you can lift a private
session bus and minimum service:

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

Expected result with minimum service: `present=1 observe=0`, `accepted by the service`,
`CloseNotification id=41` presentation in service and code 0. This validates the presentation and
removal path of the adapter; it does not demonstrate the visual presentation of a real desktop or
the automatic forwarding, which Linux does not offer with the base adapter.

In a real desktop session (GNOME/KDE with a notification daemon) the same executable must display
the notice with the `From TEST-DEVICE` attribution and visually confirm that there are no buttons
or links.

## Windows

The executable `oneintwo_notifications_native_windows` and its checklist are described in
[`src/platform/windows/notification_support/README.md`](../../../src/platform/windows/notification_support/README.md).
