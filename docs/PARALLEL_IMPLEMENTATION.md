# Parallel display, window and notifications implementation

This base defines the first compatible delivery of the three functions with three independent
branches. It complements the functional designs and replaces their previous proposals to agree
signatures, codec or limits during implementation. The public statements in
[feature_contracts.hpp](../src/integration/feature_contracts.hpp),
[visual_share.hpp][(../src/features/visual_share.hpp), [notifications.h
pp](../src/features/notifications.hpp) and [desktop.hpp_(../src/platform/desktop.hpp) form the
common contract of this delivery.

## Base and branches

All three agents must start from the same commit that contains this document and all four headers.
The prompts are delivered separately. The integrator prepares the label
refs/tags/oneintwo-parallel-base-v1 and communicates its SHA before boot. A remote clone only serves
as a base when it contains that commit. Each agent creates an independent local clone with
--no-hardlinks --no-checkout and a worktree within its own working directory. The original
repository is used as a reading source. Pending local changes from another person require to be kept
outside their commits.

Each branch delivers commits of its package. The integrator will then perform the combination and
joint testing. Agents can develop and test their package with double local interfaces from the first
day. Doubles live in their test folders and are excluded from the product executable.

| Agent and branch | Write property | Result |
| --- | --- | --- |
| P · `feature/share-screen` | CMake root, `src/main.cpp`, `src/app/`, `src/core/session.cpp`, `src/net/`, `src/storage/`, `src/ui/main_window.cpp`, `src/features/profiles.cpp`, `src/features/visual_share.cpp`, `src/features/visual/`, `src/ui/visual_viewer.cpp`, `cmake/session/`, `cmake/visual/`, `tests/session/`, `tests/visual/` | Display, common visual module, viewer, session, network and composition of the three functions. |
| V · `feature/share-window` | `src/platform/windows/visual_capture.cpp`, `src/platform/windows/visual/`, `src/platform/linux/visual_capture.cpp`, `src/platform/linux/visual/`, `cmake/capture_windows/`, `cmake/capture_wayland/`, `tests/capture_windows/`, `tests/capture_wayland/` | Native window and monitor captures on Windows and Wayland. |
| N · `feature/notifications` | `src/features/notifications.cpp`, `src/features/notification_support/`, `src/platform/windows/notifications.cpp`, `src/platform/linux/notifications.cpp`, `src/platform/windows/notification_support/`, `src/platform/linux/notification_support/`, `cmake/notifications/`, `tests/notifications/` | Manual delivery, receipt and forwarding of notices. |

The public headers of this base are under the ownership of the integrator. Each module implements
its `Impl` in its own file. Private session extensions belong to P, including its private
representation in session.hpp. SessionGeneration, FeatureSet and the already declared signatures are
retained as a public contract. An discovered incompatibility is delivered to the integrators with
specific case and minimal proposal while the rest of the package continues. The integrator
incorporates any public correction to a new common base.

The global documents and `TODO.md` are consolidated when integrated. Each agent returns outstanding
defects with description and reproduction to incorporate `TODO.md`. The delivery of each branch
includes SHA base and final, files, executed commands, native results and verification limits in its
final response.

## Joint decisions

- V++17, CMake 3.24 or later and Qt 6 Widgets as a graphical base. Components declare their
  dependencies by target. P sets the dependencies resolution of the executable.
- P implements JPEG independent images using Qt Gui, with initial quality 75. It reduces quality and
  then resolution if necessary to respect 1 MiB per image. If it does not fit, it discards the frame
  and communicates recoverable error. The receiver validates dimensions of JPEG before reserving the
  decoded image. Codec support is checked when booting.
- All JPEG units carry `key_frame=true`. Each complete image allows to recover the presentation. The
  frequency limit is 30 fps and the dimensions and pixels limits of the visual contract are
  retained.
- P implements a single authenticated and encrypted transport of the product. It can reuse the code
  of the experiments after reading it and adapting it to the common protocol. The identity of the
  pair must be verified and the data channels linked to the same session. Test credentials belong to
  the local testing environment.
- The first target to capture Linux is Wayland with ScreenCast and PipeWire. V retains its own
  capture session within its area, reusing repository patterns. V implements both font types.
- The capture keeps BGRA eight-bit, contiguous rows from top to bottom and alpha opaque. The exact
  size is wide × high × 4 with proven multiplication. The backend validates its native dimensions
  before saving memory. P scale to the output limit while retaining proportion. Conversion of native
  formats and stride belongs to the backend.

These choices are design decisions for this delivery. Your performance and availability are verified
when implementing.

## Independent construction

P creates `cmake/visual/CMakeLists.txt`. V creates `cmake/capture_windows/CMakeLists.txt` and `cmake/capture_wayland/CMakeLists.txt`. N creates `cmake/notifications/CMakeLists.txt`. Each file functions as a standalone project and subdirectory of the root. It gets the root of the repository from its own path.

The public targets are `oneintwo_visual`, `oneintwo_capture_windows`, `oneintwo_capture_wayland` and
`oneintwo_notifications`. Targets export the `src` folder as public and V++17. Each package links
its dependencies through targets and allows you to activate your tests with `ONEINTWO_BUILD_TESTS`.
N selects your backend according to the platform and exports only the respective factory.

Each package can be tested with `cmake -S cmake/<paquete> -P build/<paquete>`, compilation and CTest
from that folder. Test targets link doubles for external dependencies. P creates the final
executable by linking visual, notifications and capture of the platform, as well as session and Qt
Widgets. During isolated work P can check composition by means of a test executable with double
factories. The final executory requires actual implementations.

## Threads, property and life of objects

`ModuleRuntime.post` glues work on the main thread Qt, never executes it inline. Public inputs of
modules, dispatch of messages, `tick` and callbacks of UI are executed there. The coordinator calls
`tick` every 100 ms. The injected clock is monotonic and the tests explicitly control it.

Adapters can have their own threads. They deliver callbacks sorted by `post`. Codec and network jobs
keep buffers with explicit property and return results to the same executor. Work queues must be
narrowed.

P creates first runtime, adapters and viewer, then modules, and finally connects the actions. The
references of `VisualDependencies` and `NotificationDependencies` remain valid during the life of
each module. Before destroying, P removes permissions, stops modules and adapters, disconnects
callbacks and destroys consumers before their dependencies. Pending callbacks check a token of life
and operation before accessing the recipient.

`PlatformContext.parent` identifies the local window for selectors and permissions. `DesktopEvents`
callbacks report blocking and changes of soporte/consentimiento. P recheck capabilities and
recalculate authorization. Each backend manages the resources of its function. Repeated blocking
notifications are idepotent.

The capture stop invalidates the pending selection or acquisition. The selection terminal response
is delivered once as long as the consumer remains alive. The frames after a stop retain the original
reference and are discarded. The viewer emits `user_closed` only for a user closing action. `close`
program releases the viewer without causing a cycle of messages.

## Connection of inputs and outputs

| Action or message | Destino |
| --- | --- |
| Share Monitor or Window | P calls `VisualShare::offerSource` with the current generation. |
| Invitation | P dispatches `VisualShareOffer` to `receiveOffer`. P calls `show_offer`. |
| Accept or reject in IU | P calls `decideOffer` and sends `VisualDecisionMessage` through `send`. |
| Remote decision, status, activity and closure | P calls `receiveDecision`, `receiveSourceState`, `receiveActivity`, and `receiveStop`, respectively. |
| Validated full image | P delivers `VisualEncodedFrame` at `receiveFrame`. |
| Close viewer | The factory created by P calls `VisualShare::stop`. |
| Notice Form | P calls `SharedNotifications::sendManual`. |
| Remote warning | P calls `receiveNotice`. |
| Discover apps | P calls `SharedNotifications::discoverApplications`. N consults the adapter and returns a snapshot; does not change permission or observation. |
| Change of permitted apps | P calls `setAllowedApplications`. N restarts or updates its observation according to permissions. |
| Change of authorisation | P calls `setAuthorization` on both modules before dispatching more work. |

P and N exclusively use `SendFeatureMessage` to transmit. `queued` means that the session acquired the message. `busy` allows you to discard video and keep alerts in a local queue. `disconnected` or `denied` cancel the work concerned. If sending a visual control message fails, P ends the operation locally. The other end applies its timer.

N requests native permission when discovering applications or starting observation. The discovery is
an independent query that does not start observation; it continues to require functional forwarding
permission and an enabled list. `startObserving` reports start or error and subsequent withdrawals
by `state_changed`. Callbacks capture the subscription identifier. `presentNotice` complete once
with the result of the request to the desktop. The permission is checked again before submitting.

P implements the existing profile limits. Custom allows the capabilities granted. Only shipments
retain manual notices and remove video and forward. Pause removes all. The interface allows to
enable each mode and sense separately, accept offers and stop operations. The lock removes effective
capabilities and empties the presentation.

## Function protocol, version 1

P is the sole owner of the byte format. P and N produce and consume the `FeatureMessage` types.
Memory compatibility is fixed by the header and P implements this network representation.

All integers use big endian. A plot starts with version u16 equal to 1, type u16 and length of
payload u32. Unknown versions, types and enumerations and leftover bytes are rejected. The strings
have length u32 followed by valid UTF-8. Bools are encoded as u8 with values 0 or 1. Domain
enumeration use the stated order, starting with zero.

| Tipo | Canal | Payload in order |
| --- | --- | --- |
| 1 · oferta | Interactivo | visual reference, u8 mode |
| 2 - Decision | Interactivo | visual reference, decision u8 |
| 3 · estado | Interactivo | visual reference, revision u64, status u8 |
| 4 · actividad | Interactivo | visual reference, revision u64 |
| 5 · cierre | Interactivo | visual reference |
| 6 · fragmento | Datos | visual reference, u64 revision, u64 sequence, u32 width, u32 height, key_frame u8, total u32, offset u32, bytes remaining |
| 7 · warning | Interactivo | generation u64, notice_id u64, mode u8, title, body, app_name |

A visual reference contains generation u64, initiator u8 and offer identifier u64. In the cable,
initirator 0 represents the emitter of that plot and 1 the receiver. When decoding is reversed to
the perspective of the receiver. The sending of a response reexpresses the initiator from the
perspective of who sends it.

P assigns to each authenticated connection a new and growing local generation. During the
establishment it exchanges both generations and knows the remote one. In each outgoing plot it
serializes its local generation. In reception it demands that the value coincide with that announced
by the pair and translates it to the local generation before constructing the domain object. Each
data channel is associated with that authenticated connection. When reconnecting previous queues and
associations are destroyed. Operations identifiers begin in 1 per generation and sense. Overflow
ends the session before reusing values.

The interactive channel supports up to 4096 bytes of payload per plot. The fragment supports 65536
bytes of data plus its 50 bytes of metadata. Each image has a maximum of 1 MiB. The reassembly
maintains a single image per operation, requires contiguous offsets from zero and identical metadata
in all its fragments. A structural error ends the channel and the operation. When the operation is
withdrawn, its fragments are discarded.

P intersperses fragments and other data works by one turn per operation. It retains at most two
queued encoded images and a reassembled receiver. P maintains at most one additional pending capture
to the running codec work. The interactive channel has a budget of 64 KiB and returns `busy` if
exceeded. Limits are checked before copying or reserving memory.

## Time, loss and deduplication

P sends activity every second from the source as long as the selected source remains valid, also
during suspension. The receiver ends at three seconds without status, activity or valid image of the
operation. The deadline starts when accepting. The first revision is 1 and increases when changing
availability or size. The sequence starts at 1 and grows throughout the operation.The receiver
requires current revision and sequence higher than the last one submitted.

An image received before the announcement of your review is discarded. By announcing new revision
the viewer is emptied and the decoder restarted. A later JPEG image can recover the presentation. A
closing, revocation or new generation invalidates the pending jobs before executing cleanup.

N retains a maximum of 16 outstanding notices. By overcoming capacity it discards the new one and
reports the local motive. It applies a maximum of three window warnings of a second monotonic, both
when sending and when receiving. A notice expires at ten seconds in each local queue. The receiver
consumes the authorized identifier even if it discards by frequency. The session avoids accumulating
network alerts beyond the same local deadline. When closing connection any pending notice is
discarded.

## Compatibility check and final integration

Each package compiles against the common headers without modifying them and tests the observable
results of their interfaces. P includes back and forth of all messages, limits, fragmentation and
translation of generation and initiator. P test selection, acceptance, revision and closing with
synthetic clock and capture. V tests both types of source and cancellation. N test form,
observation, filters, revocation and presentation.

The integrator incorporates P, V and N into an integration branch, resolves the compilation
connections and reuses the valid evidence of each delivery. It then compiles the executable on
Windows and Ubuntu and runs manual shipping, screen and window between computers. It checks lock,
permissions, closure, reconnection and saturation with the same binaries. The functions whose native
adaptor or test remains pending are explicitly identified.

The common base commit and its label must be available to all three agents before opening their
branches. The oneintwo-parallel-base-v1 label is an agreed reference for preparation and its
existence must be verified.

Screen, Window and Notification orders are delivered separately from the repository. Each includes
local isolation, ownership, testing and delivery format. Three-pack delivery replaces the previous
five-pack delivery.
