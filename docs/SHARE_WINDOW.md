# Share a single window

The [parallel implementation base](PARALLEL_IMPLEMENTATION.md) sets the public signatures, owners, codec, limits and protocol of this delivery. Its decisions specify the proposals of this guide.

## Flow of use

1. On the source computer, click **Share window** and choose a local window from an application.
2. The linked team receives an invitation that identifies the sender and says **A window**. Upon
   accepting it, OneInTwo presents its live image in an independent viewer.
3. The viewer allows you to resize the image and view it in read-only mode. The selection retains
   the identity of the window while moving or resizing. The video only includes the content that the
   system delivers for that surface. Menus and dialogues that the system treats as independent
   windows are left out.
4. **Stop** at the source and close the viewer at the receiver finish the transmission. Close the
   original window also finishes it. To share another, a new invitation is created.

The isolation depends on the system capturing the window as a native source. If that capability does
not exist or the window can no longer be safely identified, OneInTwo indicates that the function is
unavailable or stops the broadcast. It is excluded to cut a region from the screen and any automatic
change to the monitor capture. If the source is suspended when minimized, the viewer is empty and
indicates the status. You can re-show the same window when the adapter confirms that it is still the
authorized source. If the system closes the capture, a new invitation is needed.

## Permits and life cycle

**Share window** has its own permission in sense, initially disabled. The receiver can allow windows
and keep the full screen disabled. The **Personal** profile, local selection and authorization, and
acceptance of each invitation apply. The window title serves for local selection and is outside of
protocol and diagnosis. The capture indicator remains accessible at source.

The expiration, video limits and invalidation described in [Share a
screen](SHARE_SCREEN.md#permissions-and-lifecycle) are reused. Moving the window to another
monitor does not change the authorization. If the source loses the ability to identify the surface,
the operation closes even if a new window appears with the same title.

## Basic architecture

| Capa | Responsabilidad |
| --- | --- |
| [Session](../src/core/session.hpp) and [protocol](../src/net/protocol.hpp) | Apply the specific window permission. The invitation modality is associated with the operation and checked before accepting, capturing or delivering video. |
| [Common visual module](../src/features/visual_share.hpp) | Share the invitation cycle, statuses, transport, limits and presentation with the full screen. Ask the desktop for exactly a window font. |
| [Windows](../src/platform/windows/visual_capture.cpp) | Locally select a suitable `HWND` and use native window capture as a candidate. Link the operation to the catch object and attend to its closure without re-searching by title. |
| [Ubuntu/Wayland](../src/platform/linux/visual_capture.cpp) | Enable the option only if the portal announces `WINDOW`. Request a single source of that type, keep the ScreenCast session and consume only the authorized stream. |
| [Visor](../src/ui/visual_viewer.cpp) | Share the same receiving window and type **A window**. Empty during suspension or closure and adjust the presentation to the new size. |

Documentation of adapters. [Windows window
capture](https://learn.microsoft.com/en-us/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createforwindow)
· [ScreenCast font
types](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.ScreenCast.html#org-freedesktop-portal-screencast-availablesourcetypes)

## Acceptance criteria

- In both ways, share a window with an identifiable pattern. Overlay another window with a different
  marker, move the original to another monitor and resize it. Check that the video retains only the
  content of the chosen source.
- Minimize and restore the source. Verify that, if the capture is suspended, the viewer is empty and
  only recovers the same window after confirming its availability. If the capture is closed, verify
  that a new invitation is needed.
- Close the source window and open another window with the same title. The viewer must be emptied
  and the second window must be left out of the previous operation.
- With enabled window permission and disabled screen, check that the first mode works and that the
  second one is rejected. In an environment without native window capture, check the unavailability
  status and the absence of a desktop video.

## Implementation guide

Implement this modality as a variant of `VisualSourceKind` within the same visual module. The
permission is selected from the modality fixed when creating the offer. Keep that association in all
transitions and check it again when starting the capture.

In Windows, the adapter must keep the capture object and its graphic resource, attend to closing and
size changes, and convert the buffers to BGRA with contiguous rows. The local selector must identify
a verifiable window source. The life of a handle requires checking together with that of the capture
object to avoid accidental reuse. Post suspension or closure when the system prevents acquiring
content.

In Wayland, reuse asynchronous request patterns and `portal_session.cpp` cancellations and available
experiments. Create your own ScreenCast session for the operation, request `WINDOW` with a single
source and validate the answer before consuming PipeWire. Keep together pending request, session,
PipeWire connection and operating reference. The cancellation must also close a request that still
awaits response.

The `DesktopVisualCapture` contract is shared by both platforms. Each implementation requires a
visible factory for the coordinator who returns the adapter through that contract. Use the factories
declared in src/integration/feature_contracts.hpp. The owner of the composition selects the factory
when compiling and links only the corresponding backend.

Validate the window first on a local circuit captures → viewer. Then reuse the invitation and screen
transport. Use two windows with different patterns to check occlusion, movement, resizing,
minimizing, closing and re-using the title. Write down what behavior each desktop actually delivers
when suspending the source.

## Selection and isolation contract

The capture factory receives the local context required by the platform, including the parent of the
selector and the executor of the native thread. Returns a uniquely owned object through
`DesktopVisualCapture`. The coordinator retains that object until the cancellation of its callbacks
is completed.

`selectSource` produces a single terminal response per request as long as the consumer remains
alive. The result `ready` means that the adapter retains a valid source of the requested type.
`cancelled`, `denied`, `unavailable` and `error` release their pending resources. If the source
disappears between selection and acceptance, the start reports closure and the operation ends.

The backend must validate the effective type of the source returned by its selector. When the system
does not allow that type to be credited, the operation ends as unavailable. The isolation test must
include different content behind and above the window, in addition to a separate dialog from the
application.

The stop is idempotent and cancels both selection and acquisition. The already glued callbacks
retain the original reference and the module discards them by having it invalidated. Destruction
disconnects native callbacks and waits for or cancels works that can still access the object.
