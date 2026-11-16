# Inter-equipment notifications

Common module of manual notices and forwarding of notifications. Implements the independent platform logic described in [`docs/NOTIFICATIONS.md`](../../../docs/NOTIFICATIONS.md) and [`docs/PARALLEL_IMPLEMENTATION.md`](../../../docs/PARALLEL_IMPLEMENTATION.md), and relies on the contract [`DesktopNotifications`](../../platform/desktop.hpp) to present, observe and withdraw notices.

This directory contains the policy and the helpers of the feature. The public façade is in [`../notifications.hpp`](../notifications.hpp) and implementation in [`../notifications.cpp`](../notifications.cpp).

## Capacidades

| Capacidad | Main entrance | Estado |
| --- | --- | --- |
| Send a manual warning | `SharedNotifications::sendManual` | Windows and Linux |
| Receive manual or forward notices | `SharedNotifications::receiveNotice` | Windows and Linux |
| Forward new notices from other applications | `forwardObserved` and native observation | Windows only with MSIX identity and consent |
| Discover apps with current ads | `discoverApplications` | Windows; Linux devuelve `unavailable` |
| Choosing Authorized Applications | `setAllowedApplications` | Windows; in Linux there is no observation |
| Submit and withdraw your own notices | `DesktopNotifications::presentNotice` / `withdrawNotices` | Windows and Linux |
| Apply permissions by modality and meaning | `setAuthorization` | Common |

Notices are plain text. No buttons, links, attachments, responses or actions from the original
application are forwarded. Reception also does not confirm that a person has seen the notice:
`DesktopResult::ready` only indicates that the desktop accepted the submission request.

The profile and the effective authorization are calculated by the coordinator. The module receives a
`FeatureSet` already resolved for shipping and another for receiving; it does not read preferences
or know the names of the profiles.

## Limits and policy

The only source of the frequency is
[9`notification_rate_per_second`](../../integration/feature_contracts.hpp). Do not enter another
number in the IU, session or tests.

| Limit | Valor |
| --- | --- |
| Frecuencia | 3 window warnings of 1 second monotonic, by meaning |
| Cola local | 16 outstanding sense notices |
| Expiration in queue | 10 seconds from the entrance |
| Deduplication of observed | 64 prints, 10 seconds. |
| Title | 256 bytes UTF-8; obligatorio |
| Cuerpo | 2048 bytes UTF-8; optional |
| Visible application name | 128 bytes UTF-8 |
| Coordinator's tick | 100 ms |

The shipping frequency is shared by manual and forwarding notices. A notice that exceeds the limit
is discarded and reported locally; it does not accumulate to try later. Queues are narrowed and lose
the new notice when they are full. Expiry is checked when retrying the shipment and just before
submitting.

The receiver consumes a structurally valid identifier even if it is later discarded by frequency.
This prevents a repetition of the same `notice_id` from causing another presentation. Do not change
that order without reviewing the deduplication tests.

## Implementation

### Facade and status

`SharedNotifications` is a facade with a `Impl` The relevant state lives by session generation:

- emission counter and last identifier received;
- output and input queues with their input moment;
- Monotonic frequency markings per sense;
- warning tracks observed for duplicating;
- list of authorised applications;
- Observation subscription and local identifier.

When changing `SessionGeneration` active notices are removed, queues are emptied, counters and
deduplication are restarted, and the previous observation is stopped. Generation is checked in all
entries and callbacks: an event of one generation or old subscription is discarded.

### Recorridos

**Delivery manual**

1. `sendManual` checks generation, permission `manual_notifications` and contents.
2. The frequency is applied, a `notice_id` is assigned and a `RemoteNotification` is built.
3. `SendFeatureMessage` decides: `queued` transfers the property, `busy` leaves the notice in the
   local tail bounded, and `denied`/`disconnected` empty the pending work.

**Return observed**

1. The adapter delivers `ObservedNotification` with local identity, visible name and content.
2. The module requires `forwarded_notifications`, a non-empty identity, an application of the list,
   absence of echo and valid content.
3. It doubles by `app_id`, title and body during the configured window.
4. Issue the notice with the visible name; the `app_id` does not leave the team.

**Reception**

1. `receiveNotice` validates modality, generation, permission, identifier and content.
2. Rejects identifiers already consumed or not growing.
3. Advances the latest identifier, applies frequency and glues.
4. `tick` checks expiration and permissions before calling `presentNotice` with the verified name of
   the pair.

### Observation

Observation is only initiated if there is forwarding permission, a list of non-empty applications,
and `notificationCapabilities().observe == true`. The list may change during observation: it is
re-evaluated by consuming each event and changing the authorization.

The Windows adapter takes a baseline of the existing warnings at the start, so that the history is
not forwarded. A reconnection or a new generation renews that baseline. The module does not detect a
suspension spin by itself: the coordinator must restart the observation in order not to forward the
accumulated history. Consent is consulted periodically; if the user withdraws it, the module stops
the observation and reports the change.

`discoverApplications` is an independent consultation:

- does not initiate or modify the observation;
- may seek consent and may complete in a synchronous or asynchronous manner;
- delivers the callback once;
- filters empty identities, the product itself, duplicates and unusable names, and orders by name
  and then by identity.

An empty list only means that no current notices were found; it does not show that there are no
installed applications.

## Platform adapters

### Windows

- Presenta con `ToastNotificationManager`.
- Identity MSIX uses the package identity; unpacked uses the Product AppUserModelID and requires
  direct access from the Start menu.
- Observation uses `UserNotificationListener` and requires package identity and
  `userNotificationListener` capacity in the manifest.
- `RequestAccessAsync` is invoked from the interface thread; listening and presentation use a thread
  of their own with apartment MTA.
- If there is no package identity, `observe` is false and the discovery returns `unavailable` before
  requesting consent.
- The XML toast is built by escaping the text; it is not interpreted by the user.

Packaging, certificate and native checklist are in [`src/platform/windows/notification_support/README.md`](../../platform/windows/notification_support/README.md).

### Linux

- Present and withdraw by `org.freedesktop.Notifications`.
- Manual shipping and authorized reception work with the notification service available.
- There is no general authorized provider for reading notices from other applications: `observe` is
  fake, `discoverApplications` returns `unavailable` and automatic forwarding is not available.
- The presentation uses plain text and the name of the pair as attribution.

## Integration

First build the runtime, desktop adapter and session; then the module. `NotificationDependencies`
references must remain alive for the entire life of `SharedNotifications`.

```cpp
NotificationDependencies dependencies{
    runtime,                        // now + post to the main thread
    desktop,                        // makeWindowsNotifications / makeLinuxNotifications
    session.sendFeatureMessage,     // solo mensajes RemoteNotification
    [this] { return session.peerDisplayName(); },
    [this](std::string message) { ui.reportNotificationIssue(message); }};
auto notifications = std::make_unique<SharedNotifications>(dependencies);
```

Points to be respected by the coordinator:

- Call `tick()` every 100 ms; without tick, no queues are reintended and no expiration is checked.
- Call `setAuthorization` before dispatching work and changing generation. A reconnection must use a
  new generation.
- By blocking, disconnecting or pausing, withdraw the effective authorization; the module empties
  the earrings and stops the observation if it is no longer allowed.
- Populate the selection with `discoverApplications` and then call `setAllowedApplications` with the
  `app_id` chosen. Do not use `app_name` as identity or `app_id` as text for the user.
- Treat callbacks as single-use. `presentNotice` can complete asynchronously; a callback of an
  earlier generation is ignored.
- Do not call `presentNotice` directly to skip permissions, filters, limits or deduplication.
- Do not include title, body or native identity in `report_error`; the module already avoids doing
  so and the UI must maintain that guarantee when registering the message.
- Keeping public headers under integrator coordination. This package only adds documented discovery
  extensions and uses the frequency-sharing constant.

## Major challenges and factors

- **Windows needs packaging to observe.** An executable without MSIX can submit notices if the Home
  menu shortcut is registered, but cannot use `UserNotificationListener`. The manifest must declare
  `userNotificationListener` and `runFullTrust`.
- **The consent is not the same as the capacity.** `observe=true` indicates technical support; the
  permission of the system and that of the pair are different states. The UI must refresh
  capabilities and authorization after a revocation.
- **The observation does not reproduce history.**The base line is taken at the start and is renewed
  at the restart of the observation (reconnection or new generation).The module does not receive
  suspension events: after resumption, the coordinator must restart the observation so as not to
  forward the accumulated history.
- **The discovery list is a snapshot.** Only includes applications with notices present at that
  time. Refresh when opening the selection; do not interpret an empty list as an absence of
  applications.
- **The native identity may be missing.** The notices without `app_id` reliable, without visible
  name or the product itself are discarded. The eco filter recognizes identifiers that start with
  `oneintwo` followed by `.`, `_` or `-`; if the product identity changes, also update the manifest
  MSIX.
- **The queues are deliberately lost.** `queued` means that the session accepted the message, not
  that the other team presented it. There is no confirmation of delivery or human reading. When
  disconnecting, blocking or changing generation, the pending is discarded; it does not persist on
  disk.
- **Frequency is defense in depth.** The session also applies its limits. Keep both layers aligned
  with `notification_rate_per_second`; do not add independent literals.
- **The order of deduplication matters.** The receiver advances the identifier before applying
  frequency and retains that decision. Changing the order may cause repetitions or discards other
  than those tested.
- **The text is validated before use.** `sanitize_plain_text` replaces controls C0 and DEL, but
  presupposes UTF-8 valid; do not use it as a substitute for `is_valid_utf8`. A title composed only
  of ASCII spaces, tabs or line breaks is rejected.
- **IU should not display raw errors as notice content.** `report_error` messages are recoverable
  local states and do not include title or body; if registered, keep that warranty.
- **The Linux adapter does not observe.** Do not promise automatic forwarding on Linux without an
  authorized provider. The native test can be omitted with code 77 if there is no session bus or
  notification service.
- **Visual verification remains manual.** The IC checks compilation, testing and, in Windows,
  installation and removal of MSIX. It does not certify that the toast is seen, that the user gives
  consent or that the listener delivers real notices.

## Compilation and testing

The package and its dependencies are described in
[9`cmake/notifications/README.md`](../../../cmake/notifications/README.md).

```bash
cmake -S cmake/notifications -B build/notifications \
      -DONEINTWO_BUILD_TESTS=ON -DONEINTWO_BUILD_NATIVE_NOTIFICATION_TESTS=ON
cmake --build build/notifications
ctest --test-dir build/notifications --output-on-failure
```

The suite covers UTF-8 validation and limits, frequency, queues and expiration, deduplication,
identity and echo filters, permissions by modality, profile changes, blocking, revocation before
presenting, reconnection, filtered and asynchronous discovery, and presentation assistants for both
backends. Native tests requiring desktop, consent or package identity are executed or omitted
according to the platform; they do not replace the manual check described in the Windows
documentation.
