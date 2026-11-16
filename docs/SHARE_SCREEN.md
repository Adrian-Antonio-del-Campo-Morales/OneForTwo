# Share a screen

The [parallel implementation base](PARALLEL_IMPLEMENTATION.md) sets the public signatures, owners, codec, limits and protocol of this delivery. Its decisions specify the proposals of this guide.

## Flow of use

1. On the source computer, open OneInTwo, press **Share screen** and choose a local monitor. If
   there are several, the selector allows you to identify which one will be shared.
2. The linked team shows an invitation that identifies the sender and says **Full screen**. The
   recipient can accept or reject it. OneInTwo starts reading and sending frames after accepting and
   verifying that the permission and the source selection remain valid.
3. The live image appears in a separate OneInTwo window on the receiving computer. The window can be
   resized and retains the proportion of the image. The initial range is live display, read-only,
   without audio or recording.
4. The source sees an indicator and can click **Stop**. Close the viewer also ends the transmission.
   Close the main configuration window leaves the viewer and its indicator available.

The user shares a single physical monitor in each operation. Changing the monitor requires
completion of the operation and selecting another one. Resolution changes of the same monitor can
continue within the transmission. If the monitor disappears or the selection is no longer reliable,
the viewer deletes the image and the operation ends.

## Permissions and lifecycle

**Share screen** requires a sense-by-side permission in the linked pair. It starts off. The session
checks that the source can capture monitors and the receiver can open the viewer. The **Personal**
profile allows to use that permission and **Only submissions** and **Pause** suspend it. The
selection is made on the home desktop, with system authorization when applicable.

The operation goes through **local selection → pending invitation → accepted → live → finished**. An
invitation lasts up to 30 seconds since it is presented, measured with the receiver's monotonic
clock. The emitter applies its own deadline from dispatch. A rejection, a selector closure,
expiration, blocking of any of the equipment, revocation or disconnection terminates the operation.
Each end acts as soon as it detects the event. Reconnection requires new selection and acceptance.

## Basic architecture

| Capa | Responsabilidad |
| --- | --- |
| [Session](../src/core/session.hpp) and [protocol](../src/net/protocol.hpp) | Authorize the mode by meaning. Invitation, decision and closure use the interactive channel. Video data is only supported for the accepted and current operation. |
| [Common visual module](../src/features/visual_share.hpp) | Maintain a single visual operation per session, pending or active, counting both modes and both senses. A simultaneous invitation is rejected by occupation. Coordinate capture, coding, decoding and closing by identity of operation. |
| [Windows](../src/platform/windows/visual_capture.cpp) | Locally select a monitor and capture it with Windows Graphics Capture as a candidate. Keep the native identifier only at source. |
| [Ubuntu/Wayland](../src/platform/linux/visual_capture.cpp) | Consult `AvailableSourceTypes`, request only `MONITOR` and a single source with ScreenCast, and consume a single stream authorized with PipeWire. The portal retains your local selection and the session is requested without persistence. |
| [Visor](../src/ui/visual_viewer.cpp) | Show the image, source equipment and status. Clear the surface when losing the source or authorization, and request the coordinator to close. |

The operation identifier combines session generation, an extreme that initiated the invitation and growing identifier of that end. The receiver rejects repeated or old identifiers, including those already rejected by occupation. The invitation contains the modality, without the name of the monitor or native identifiers. The intended data channel for files also transports video drives with source revision, sequence, dimensions and key image mark. The initial target is up to 30 fotogramas/s, a maximum of 1920 pixels per side and 2.073.600 pixels per image. The receiver checks these limits both in the metadata and in the decoder output. The encoded unit is limited to 1 MiB before booking the reassembly buffer. The first installment uses JPEG independent images according to the parallel implementation base. Its latency and flow require validation.

The transport intersects blocks of files and video fragments with taped queues and maintains control
in its independent channel. If the video queue is filled, captures are discarded before encrypting
them. If a pending encoded drive is lost, its dependents are discarded and restarted from a key
image. The source revision increases when the capture state is initiated or changed. The viewer
emptys the image in each transition and only accepts video from an announced review as available,
starting with a key image; as channels advance independently, the video that arrives before your ad
is discarded. The sender produces periodic key images so that the receiver can

Documentation of adapters. [Windows Graphics Capture](https://learn.microsoft.com/en-us/windows/apps/develop/media-authoring-processing/screen-capture) · [Controller capture on Windows](https://learn.microsoft.com/en-us/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createformonitor) · [Portal ScreenCast](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.ScreenCast.html)

## Acceptance criteria

- On Windows → Ubuntu and Ubuntu → Windows, choose a monitor, accept and view your image in a
  separate window. With two monitors, display different bookmarks in each and check that only the
  one chosen is transmitted.
- Refuse, let expire and cancel an invitation. Verify that OneInTwo does not read or send frames
  before accepting and that the pending status is withdrawn.
- Change the resolution of the same monitor and check that the viewer adapts the image. Disconnect
  that monitor and check to see that the viewer is empty and that another monitor never takes its
  place.
- Close the viewer, block a computer, withdraw permission and cut the network during the broadcast.
  Verify resource release, late data discard, and absence of automatic restart.
- Make a simultaneous file transfer. Check narrow queues, recoverable viewer after congestion and
  interactive channel response when stopping.

## Implementation guide

The product base still requires an executable, object composition, transport and UI. The root CMake
is a reserve. Starting with a C++17 kernel lens and a local test with captured and viewer connected
allows you to verify the image before the exchange between computers.

| Paso | Entry and expected result |
| --- | --- |
| Composition | The coordinator creates session, capture adapter, visual module and viewer. Injects monotonic clock, message output and execution in the UI thread. |
| Selection | `offerSource` reserves a local reference and calls `selectSource`. Callback checks that the reference remains valid. Cancel releases the reservation. |
| Invitation | After selecting, issue `VisualShareOffer`. `receiveOffer` validates permission, generation, modality and occupation before requesting confirmation from the IU. |
| Acceptance | `decideOffer` sends the decision and prepares the viewer. `receiveDecision` starts the capture of the origin only for a valid acceptance. |
| Imagen | The native callback delivers BGRA to the module. A scaled down work and encodes. The session sends fragments. The receiver reensambla, validates, decodes and presents in the graphic thread. |
| Termination | First invalidate the reference, cancel jobs, close capture, vaciar/cerrar viewer and issue the closure if the channel is still available. Repeat the closure must be harmless. |

The minimum status of the module includes reference, mode, phase, source revision, last sequence,
deadlines and references to its dependencies. Add constructors and explicit callbacks to the current
statements when implemented. The module has the timers and the revision. The adapter has the native
resources and communicates changes in availability. Callbacks are serialized in the execution of the
module before modifying its status.

The output of the module requires specific operations to send invitation, decision, status,
activity, image and closure, in addition to presenting a local invitation. These outputs are
connected from the coordinator to session and UI. Current signatures describe entries and must be
completed with those dependencies before linking the product.

The protocol must define version, order of bytes, types, lengths and representation of the
originator relative to the receiver. Each fragment requires reference, revision, sequence, total
length and position. Limit the number of simultaneous reassembly and discard inconsistent
duplicates. Image lengths must be checked with secure arithmetic before multiplying width, height
and four channels.

Apply codec, fragmentation, queue budgets and timers set at the common base. Measure cost, quality
and flow with these decisions and deliver any reproducible limitations to the integrator.

The core tests can use controlled clock, synthetic capture, memory transport, and call-recording
viewer. Cover late acceptance, simultaneous invitations, generation change, old revision,
pre-advertising video and cancellation during encoding. The subsequent native test should display a
real source and verify closure and release of resources.

## Shared rules of status and ownership

| Evento | Condition | Efecto obligatorio |
| --- | --- | --- |
| Selection completed | Local reference still pending and current permit | Keep the source and send a single invitation. |
| Acceptance received | Matches the current local invitation and within the deadline | Start catching only once. |
| Imagen nativa | Available source and accepted operation | Associate review before gluing encoding. |
| Resize | Same authorized source | Increase revision, announce availability and restart decoding from key image. |
| Suspension | Operation in force | Increase review, empty queues and announce suspension. |
| Result of work | Current reference and revision coincide | Turn it over to the next component. Discard it if anyone changed. |
| Local or remote closure | Any active phase | Invalidate the operation and release resources. A remote closure ends without forwarding another closure. |
| Retired and reinstated permit | Even within the same generation | Finish the previous operation. A new operation receives another identifier. |

Each image belongs to the component that receives it by value. An asynchronous work retains its own
buffer and a reference and revision copy. The viewer copies or retains an image with explicit
ownership before returning control of `present`, whose current argument is a borrowed reference.
Native graphic buffers are safely converted or retained before exiting the system callback.

The adapter's callback order must preserve the status changes with respect to the frames. A frame
purchased before suspending is never labeled with the revision of a later resumption. The
implementation can attach a local capture counter if the native API delivers out of order results.
That counter belongs to the adapter and is validated before delivering to the module.

The session negotiation must establish a generation identity recognized by both extremes or an
explicit translation between their local identifiers. The response messages retain the identity of
the originator of the operation. By decoding, `local` and `peer` are interpreted with respect to the
receiver. Add a round-trip test that includes a remote invitation and its response.

The minimum integrated route is real selector → remote acceptance → capture → codec → authenticated
channel → decoder → real viewer → closing from any of the ends. Double tests unlock the parallel
work. Functional delivery requires then to travel that chain with the available adapters and declare
the pending platforms.
