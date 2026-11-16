# Send notifications between computers

The [parallel implementation base](PARALLEL_IMPLEMENTATION.md) sets the public signatures, owners, codec, limits and protocol of this delivery. Its decisions specify the proposals of this guide.

## Flow of use

| Modalidad | Origen | Receptor |
| --- | --- | --- |
| Manual | On OneInTwo, click **Send notification**, type a title, and optionally, a message. | It receives a local notification from OneInTwo attributed to the linked team. |
| Automatic | Activate **Resend notifications** and choose the authorized applications on a desktop to observe them. From then on, new alerts for those applications are forwarded. | It receives a local notification from OneInTwo attributed to the linked team and the name of the application as text. |

The content is plain text. The notification received lacks active links, original application
buttons, attachments and remote responses. The recipient applies its notification settings,
including **Do not disturb**. The sender may know if the shipment entered the local channel. The
protocol does not confirm that a person has seen the notice.

The first product delivery includes manual shipping and native presentation on Windows and Ubuntu.
Automatic forwarding is an additional capability of origin. A Ubuntu source with the base adapter
can manually send and receive both manual notices and notices forwarded by Windows. Forwarding
control shows **Not available** when a compatible observation provider is missing.

## Permissions, content and validity

Manual and automatic warnings have separate sense permissions in each pair and start off. The
**Standard** profile allows both and **Only sending** allows manual warnings. **Pause** suspends
both. The forwarding also requires the home desktop read permission and a local list of empty
applications by default. The interface populates that list with
`SharedNotifications::discoverApplications`, an asynchronous query that can request consent, does
not initiate observation and returns applications with current notices. OneInTwo notices and those
without a reliable application identity are excluded at source. Activated observation records the
previous status to forward only new additions, without reproducing notifications from

Each notice carries the session generation, a growing identifier by meaning, its mode, title and
body. The automatics add a visible name of the source application. The verified name of the computer
is obtained locally from the session. The UTF-8 limits are 256 bytes for the title, 2048 for the
body and 128 for the visible name. The title is mandatory. Unformed or out of bound data are
rejected and the mark is escaped when presenting. The native identifier of the application remains
at source.

The module discards repetitions, old warnings, and notices received during lockdown. Pendings expire
at 10 seconds in each local queue and a frequency limit is applied when sending and receiving.
Excess is discarded to keep the interactive channel available. A disconnection, blocking, or
withdrawal of permission discards pending notices and stops observation if it is unauthorized. The
interface removes your active alerts when the desktop provides that operation. The system can manage
its own notification history. OneInTwo only persists preferences, permissions, and authorized
applications, and records states and errors without content.

## Basic architecture

| Capa | Responsabilidad |
| --- | --- |
| [Session](../src/core/session.hpp) and [protocol](../src/net/protocol.hpp) | Negotiate modalities by meaning, validate structure and size, and send authorized notices through the interactive channel. The authorization is re-checked before submitting. |
| [Notice Module](../src/features/notifications.hpp) | Normalize manual shipping and native event. Filter applications, avoid echoes and duplicates, apply limits and coordinate presentation. |
| [Windows](../src/platform/windows/notifications.cpp) | Test native OneInTwo presentation. Evaluate `UserNotificationListener` for warnings from other applications, with manifest capability, local consent and compatibility with product packaging. |
| [Ubuntu/Wayland](../src/platform/linux/notifications.cpp) | Submit your own notices using `org.freedesktop.Notifications`. The standard interface described above allows you to publish and manage notices, and lacks a general operation to read those of other applications. Automatic observation is pending from an authorized provider. |
| [Coordinator](../src/app/application.cpp) and [configuration](../src/ui/main_window.cpp) | Connect the form, permissions, availability status and list of apps with session and desktop. |

Documentation of adapters. [Windows Notification
Lister](https://learn.microsoft.com/en-us/windows/apps/develop/notifications/app-notifications/notification-listener)
· [Departure Notification
Specification](https://specifications.freedesktop.org/notification/latest/protocol.html)

## Acceptance criteria

- Manually send Unicode text in both directions. Check that it is displayed only once, with the
  source computer, and that it is respected **Do not disturb**.
- Enable forwarding from Windows if the listener and his permission are available. Verify that a new
  notice is received from an authorized application and that previous notices, those of excluded
  applications and those of OneInTwo are left out.
- Duplicate alerts, exceed limits, withdraw permissions, switch to **Only submissions** and block or
  disconnect a computer. Verify the corresponding discard, lack of echo and that pending notices do
  not reappear when reconnecting.
- In Ubuntu without an observation provider, check that automatic forwarding is not available, while
  manual warnings and authorized reception are in operation.

## Implementation guide

The coordinator injects the native adapter, session message output, a monotonic clock and permitted
application preferences into `SharedNotifications`. Complete the builder and configuration
operations when implementing. The adapter publishes availability and revocation changes for the
session to recalculate effective permissions.

| Entrada | Recorrido |
| --- | --- |
| Formulario | `sendManual` validates text and permission, assigns identifier and requests submission to the session. |
| Selection of applications | P calls `SharedNotifications::discoverApplications` and shows the `NotificationApplication` received; when confirming, call `setAllowedApplications`. |
| Desktop Event | `forwardObserved` checks application identity, allowed list and exclusion of OneInTwo before constructing remote warning. |
| Remote Message | The session validates structure and permission. `receiveNotice` checks generation, order, limits and validity, obtains the local identity of the pair and requests presentation. |
| Withdrawal of leave | `setAuthorization` updates the state, discards affected pending and stops observation when it is no longer allowed. |
| Locking or disconnecting | The coordinator withdraws effective authorization and cancels pending. When reconnecting, another generation and another observation of novelties begin. |

Maintain the emission counter, the last received identifier, queues with their input moment and
observation status per generation. Manual and automatic shipping share the counter by sense. Define
a specific frequency and queue capacity policy before implementing transport, documenting it, and
proving excess discard.

The observation needs to distinguish the initial load from new warnings and recognize their native
identifiers while it is active. Keep that deduplication in the adapter. After returning from a
suspension, take a new baseline to avoid forwarding the accumulated history.

On Windows, first verify presentation and packaged with a notice of your own. Then connect reading
and permission from the listener. On Ubuntu, implement presentation and withdrawal using the
available service. Each backend delivers a local `DesktopNotifications` contract factory for product
composition.

Isolated tests must control time and authorization and record calls to the adapter. Cover UTF-8 text
at the limit, repeated warning, old generation, expired queue, app excluded and removed just before
submitting. Native validation checks attribution, desktop configuration, news and absence of echo.
Register the automatic support actually available per platform.

## Revocation, results and observation

The capability query describes technical support. System consent and peer permission form distinct
states that the session combines before authorizing. Withdrawal of consent should generate an event
to the coordinator even if no new notice arrives.

When implementing `DesktopNotifications`, complete the contract with an observation status and start
result callback. Associating each subscription with a growing local identifier. `stopObserving`
invalidates that identifier before disconnecting the backend. Any event already glued in from a
previous subscription is discarded, even if the permission is later re-enabled.

The list of permitted applications can be changed during observation. Re-check it when consuming
each pending event. The filter uses the local reliable application identity. The visible name is
used only for attribution in the presentation.

`presentNotice` must state whether the desktop accepted the request. The common firm already uses an
asynchronous result callback. The caller retains the warning reference in that callback. System
presentation status and human reading are independent results. A filing failure communicates locally
with your reason and discards the notice consumed.

The deduplication progresses by consuming a structurally valid and authorized identifier, including
a warning discarded by frequency limit. Thus, repeating it later does not cause another
presentation. A new generation restarts the counter and removes the previous slopes. Expiry checks
are executed just before shipment and presentation.

As a minimum integration test, connect the actual form with session, message encoding and native
presentation at the other end. Then activate observation of an authorized application and check that
a single event passes through the same path.
