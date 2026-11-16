# Package `oneintwo_notifications`

Implementation of manual shipping, receipt and forwarding of notices according to [`docs/NOTIFICATIONS.md`](../../docs/NOTIFICATIONS.md) and [`docs/PARALLEL_IMPLEMENTATION.md`](../../docs/PARALLEL_IMPLEMENTATION.md). Public headers are limited to the changes documented in this package: the expansions of discovery in `SharedNotifications` and `DesktopNotifications`and the frequency limit of three warnings per second.

## Composition

| Elemento | File |
| --- | --- |
| Common and political module of colas/frecuencia | `src/features/notifications.cpp`, `src/features/notification_support/` |
| Backend Windows (toast + `UserNotificationListener`) | `src/platform/windows/notifications.cpp`, `src/platform/windows/notification_support/` |
| Backend Linux (`org.freedesktop.Notifications`) | `src/platform/linux/notifications.cpp`, `src/platform/linux/notification_support/` |
| Target and testing | `cmake/notifications/`, `tests/notifications/` |

The target exports `src` as public and C++17. Select a single backend: Windows links `windowsapp`
and `shell32`; Linux links `dbus-1`. The other factory declared at `feature_contracts.hpp` remains
undefined, as required by the common base.

```bash
cmake -S cmake/notifications -B build/notifications \
      -DONEINTWO_BUILD_TESTS=ON -DONEINTWO_BUILD_NATIVE_NOTIFICATION_TESTS=ON
cmake --build build/notifications
ctest --test-dir build/notifications --output-on-failure
```

In Ubuntu, `libdbus-1-dev` must be installed or indicated `DBUS_INCLUDE_DIR`,
`DBUS_ARCH_INCLUDE_DIR` and `DBUS_LIBRARY`. Windows requires MSVC 2019+, Windows SDK 10.0.17763+
(C++/WinRT) and packaging described in `src/platform/windows/notification_support/README.md`.

## Connection with session and IU

Build the desktop adapter before the module and keep the `NotificationDependencies` references alive
throughout the module lifetime.

```cpp
NotificationDependencies dependencies{
    runtime,                       // now + post to the main thread
    desktop,                       // makeWindowsNotifications / makeLinuxNotifications
    session.sendFeatureMessage,    // SendFeatureMessage: solo mensajes RemoteNotification
    [this] { return session.peerDisplayName(); }, // verified name of the pair
    [this](std::string message) { ui.reportNotificationIssue(message); }};
auto notifications = std::make_unique<SharedNotifications>(dependencies);
```

| Momento | Llamada |
| --- | --- |
| Every 100 ms from the coordinator | `tick()` |
| New Authorization, Profile or Generation | `setAuthorization(generation, effectiveSend, effectiveReceive)` |
| Locking or disconnecting | `setAuthorization(generation, {}, {})` |
| Reconnection | New generation + `setAuthorization`; restart counters, queues and observation |
| List of applications in the form | `setAllowedApplications(std::set<std::string>)` (AppUserModelID locales) |
| Send from the form | `sendManual(generation, {title, body})` |
| Warning observed by the adapter | Internal: `startObserving` → filter → `forwardObserved` |
| `RemoteNotification` received from the meeting | `receiveNotice(notice)` |

Semantics that the coordinator must respect:

- Any input with a generation other than the current one is discarded without error. When changing
  generation, queues are emptied, counters are restarted and a new baseline of observation is taken.
- The module only emits `RemoteNotification`; the session encodes and applies its limits. `queued` transfers ownership; `busy` leaves the warning on the local tail bounded; `disconnected` and `denied` empty the pending work.
- The profile **Only shipments** is expressed by withdrawing `forwarded_notifications` in both
  directions and `manual_notifications` in reception. **Pause** withdraws everything.
- The observation starts only with forwarding permission, non-empty list and
  `notificationCapabilities().observe`. A change of list with both valid maintains the subscription
  and is reassessed when consuming each event.
- The UI populates the selection with `discoverApplications`: it is an asynchronous query that can
  request consent, does not initiate observation and returns unique, ordered and filtered
  applications. A result other than `ready` does not include a list.
- The UI gets from `notificationCapabilities()` if the desktop can present and observe.
  `requestObservationPermission` allows an explicit concession button; its result is asynchronous.
  The withdrawal of consent reaches the coordinator by `DesktopEvents::capabilities_changed` on
  Windows and by the local error of the module.
- `report_error` never includes title or body; it serves for recoverable states (permission, limits,
  tail, presentation).

## Decisions and limits of contract

- **Frequency:** `docs/PARALLEL_IMPLEMENTATION.md` fixes three window warnings for a second
  monotonic. `feature_contracts.hpp` declares the same figure (`notification_rate_per_second = 3`)
  and the local policy uses it as the only source. Tests check the effective limit on shipping and
  receiving.
- **Apps discovery:** solved with `SharedNotifications::discoverApplications` and
  `DesktopNotifications::discoverApplications`. Returns `NotificationApplication {app_id, app_name}`
  from existing desktop warnings. The observation is not booted with an empty list: the discovery is
  a separate query, can be repeated and does not alter the baseline or authorization.
- **Observation status:** the contract does not expose an observation event to the UI. The Windows
  backend reports `capabilities_changed` and the module issues the local error; the UI must reflect
  the status of the forwarding control based on capabilities, authorization and such errors.
- **Linux:** There is no authorized provider to read other people's notices. It announces
  `observe=false`, `startObserving` complete with `unavailable` and retains manual shipping,
  reception and withdrawal.

## Evidence of this package

- 49 cases in `oneintwo_notifications_tests`: UTF-8 and exact limits, mandatory title, frequency,
  tails and expiration, deduplication, identity and echo filters, permissions by modality, change of
  profile, blocking, revocation before presenting, reconnection, filtered and asynchronous
  discovery, presentation and removal of the Linux adapter with controlled service, and helpers of
  the baseline and text XML of Windows.
- Linux tour with private session bus and minimum service: Royal `Notify` with Unicode and
  attribution `From TEST-DEVICE`, `CloseNotification` and `observe=false`. Without graphic
  desktop the visual presentation is not certified.
- Windows: the backend and its packaging material are implemented, but they were not compiled or
  executed in this Linux environment. The native checklist is at
  `src/platform/windows/notification_support/README.md`.
