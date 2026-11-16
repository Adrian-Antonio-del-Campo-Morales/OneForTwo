# Native Visual Capture on Windows

This README describes the Windows backend of the feature `feature/share-window`, which lives in this
directory. The full feature shares the contract [`DesktopVisualCapture`](../../desktop.hpp) with the
Wayland backend ([`../linux/visual`](../../linux/visual/)); here is documented the Windows
implementation, its limits and what the other components must assume.

Status of this branch:

- local selection of monitors and windows;
- capture with Windows Graphics Capture (WGC) and D3D11;
- delivery of images BGRA to the common contract;
- states of availability, suspension and closure;
- portable tests with false ports, core tests WGC and a real smoke test Win32/WGC;
- IC `Windows MSVC` with SDK 19041 and with the current SDK.

The product integration (module `VisualShare`, codec, transport, permissions and viewer) belongs to
the coordinator and is not implemented in this package. This backend does not encode, does not
encrypt, does not send images and does not open any viewer windows by itself.

## Capacidades

| Capacidad | Estado | Observaciones |
| --- | --- | --- |
| Capture a Monitor | Yes | Selection by `HMONITOR`; the identity of the monitor is preserved. |
| Capture a window | Yes | Selection by `HWND`; no further window search by title. |
| Selector local | Yes | Win32 modal dialog with font list and Compartir/Cancelar. buttons |
| Exclude the application itself | Yes | `EnumWindows` discards windows from the current process. |
| 8-bit picture BGRA | Yes | Contiguous rows, top to bottom, opaque alpha. |
| Adapt to size changes | Yes | The WGC frame pool is recreated and the transition frame is discarded. |
| Minimization suspension | Yes | A minimized window passes to `suspended`; when restoring it returns to `available`. |
| Closure of the source | Yes | Closed window or lost monitor produce `closed`. |
| Hide cursor | Better effort | Only if the runtime exposes `IGraphicsCaptureSession2`; a property failure does not cancel the capture. |
| Capture audio, region or virtual screen | No | There is no region cut or automatic replacement by monitor. |
| Encode, transport or present | No | The backend delivers `VisualFrame` in memory; the rest is done by the coordinator. |
| Guarantee protected content or HDR | No | The system can return black or a non-convertible frame; a result is not forced. |

### Limits of the image contract

`VisualFrame` contains `size` and a `std::vector<std::uint8_t>` with exactly `width * height * 4`
bytes. Backend:

- requires dimensions greater than zero;
- limits each side to 16384 pixels and the total to 64 My pixels in internal validation;
- does not deliver formats other than `DXGI_FORMAT_B8G8R8A8_UNORM`;
- compact the `RowPitch` of the tagging texture in adjacent rows;
- force the alpha channel to `0xFF` before publishing the image.

The product output limits (1920 pixels per side, 2.073.600 pixels and 30 fps) are the responsibility
of the common module. This backend does not scale or limit the frequency itself.

## Flow of use

```text
selectSource(offer, kind)
        |
        v
enumerate(kind) -> selector Win32 -> adopt(NativeSource)
        |                                   |
        |                              CreateForWindow/Monitor
        |                                   |
        +---- terminal result -----------+
        |
startCapture(offer, callbacks)
        |
        v
WGC frame pool -> capture thread -> D3D11 copy to BGRA
        |
        v
ModuleRuntime.post -> CaptureModel -> calls.frame / callbacks.state_changed
```

1. The coordinator calls `selectSource` with an operating reference and source type (`monitor` or
   `window`).
2. The adapter checks capabilities, lists fonts and opens the selector.
3. The user chooses a source or cancel. The selection produces a single terminal response: `ready`,
   `cancelled`, `unavailable` or `error`.
4. Upon acceptance of the operation, the coordinator calls `startCapture`. WGC creates the session
   and begins delivering frames.
5. `stopCapture` invalidates the operation, stops the capture and discards late callbacks. It is
   idempotent.

### Selection results

| Status | Result |
| --- | --- |
| Selected Source and Capture Object Created | `ready` |
| User cancels or closes selector | `cancelled` |
| There is already an active selection or capture for another reference | `error` |
| Unsupported Type, Empty List or Unable to Create Capture Object | `unavailable` |
| Cannot create selector window | `cancelled` |

Windows does not display a system consent dialogue for WGC: functional authorization and local
selection are the barrier. The adapter does not return `denied` in the current flow; that result is
left for platforms or permissions that do have an explicit system decision.

## Public contract

Implementation satisfied [`DesktopVisualCapture`](../../desktop.hpp):

| Operation | Responsabilidad |
| --- | --- |
| `visualCapabilities()` | It indicates whether WGC and interoperability are available. It is a capacity check, not a guarantee that a particular source can be captured. |
| `selectSource()` | List, ask, and adopt a font. It retains a single selection or capture per adapter. |
| `startCapture()` | Starts the acquisition for the already selected reference. If the reference does not match, publish `closed`. |
| `stopCapture()` | Cancel a pending selection or release the active capture. Do not re-deliver frames from that reference. |

Callbacks are also part of the contract:

- `frame` passes `VisualFrame` by value;
- `state_changed` entrega `available`, `suspended` o `closed`;
- callbacks are published by `ModuleRuntime.post`, never executed within the native thread of the
  frame pool;
- the consumer must treat them as serialized in the module thread and must not block or release
  exceptions from them.

### Estados

| Estado | Significado |
| --- | --- |
| `available` | The source is still selected and can deliver content. It is not guaranteed that there is a frame in every instant. |
| `suspended` | The window is minimized; frames are not published until restored. |
| `closed` | The window disappeared, changed process, the monitor no longer matches or WGC failed. Operation must end. |

There is no initial `available` state guaranteed before the first frame. The first image can be the first sign of content; the coordinator should not stay waiting for an initial `available`. There is also no state of error in detail: an acquisition failure is represented as `closed` and the coordinator decides how to inform the user.

## Implementation

### Files

| File | Papel |
| --- | --- |
| [`windows_capture_factory.cpp`](windows_capture_factory.cpp) | Choose the real port WGC or the port of unavailability according to the headers. |
| [`capture_model.cpp`](capture_model.cpp) | Portable model: selection, active operation, BGRA validation, order of states and discard of late callbacks. |
| [`win32_capture_port.hpp`](win32_capture_port.hpp) | Port between model and platform: list, ask, adopt and arrancar/parar. |
| [`win32_selector.cpp`](win32_selector.cpp) | Win32 listing, local dialogue and creation of the capture object. |
| [`wgc_capture_core.cpp`](wgc_capture_core.cpp) | WinRT independent capture loop: pool, frames, states and stop. |
| [`wgc_capture.cpp`](wgc_capture.cpp) | Actual adapters of WinRT/D3D11: `GraphicsCaptureItem`, frame pool, tagging texture and copy BGRA. |
| [`unavailable_port.cpp`](unavailable_port.cpp) | Port that declares zero capabilities when WGC is not compiled. It does not simulate capture. |
| [`win32_parent_qt.cpp`](win32_parent_qt.cpp) | He gets the `HWND` father from `PlatformContext.parent` if compiled with Qt. |
| [`win32_parent_default.cpp`](win32_parent_default.cpp) | Null parent when there is no Qt. |

### Selection and Selector

The list of windows uses `EnumWindows` and discards:

- the selector window;
- windows not visible;
- Untitled windows;
- windows with `WS_EX_TOOLWINDOW`;
- windows of the current process;
- "cloaked" windows of DWM.

For each window it retains `HWND`, title and size calculated with `DWMWA_EXTENDED_FRAME_BOUNDS`
(with `GetWindowRect` as backup).The title and size are **only for the local selection**: they are
not sent to the pair and are not used to relocate the window.

Monitors are listed with `EnumDisplayMonitors`. The label includes the device name, size and if it
is the main one. The retained identity is `HMONITOR`; in addition the device name is saved to detect
that the monitor is no longer the same.

The selector is a Win32 window of its own with a list and buttons. While it is open it disables the
`HWND` parent, if it exists.

- hand over the chosen source or `std::nullopt`;
- reactivates the father;
- guarantees a single delivery through `PromptState::delivered`.

`cancelPrompt()` publishes `WM_CLOSE`; it does not destroy the window synchronously. The selector
callback runs on the UI thread and the model forwards it to `ModuleRuntime.post`.

### WGC y D3D11

`createWin32Capture`:

1. checks `GraphicsCaptureSession::IsSupported()` and the interoperability plant;
2. creates `GraphicsCaptureItem` with `CreateForWindow` or `CreateForMonitor`;
3. crea un `ID3D11Device` **hardware** con `D3D11_CREATE_DEVICE_BGRA_SUPPORT`;
4. converts the DXGI device to `IDirect3DDevice` for WGC;
5. creates a `CreateFreeThreaded` wire-free frame pool with `B8G8R8A8UIntNormalized` format and two
   buffers;
6. delivers the `WgcCaptureCore` with a `WgcSource` that checks the identity.

The `WgcCaptureCore` loop:

- Check availability and minimization every 200 ms;
- does not acquire frames while suspended;
- use `TryGetNextFrame` and wait 10 ms if there is no frame;
- if the content size changes, closes the transition frame, recreates the pool and continues with
  the next frame;
- copies the texture to a tagging texture with CPU read access, crosses the rows respecting
  `RowPitch`, compact to BGRA and opaque alpha strength;
- publishes `closed` if the frame pool, source or copy fail irrecoverably.

The copy is a full reading at CPU. There is no zero-copy path or backend conversion.

### Threads and property

- Inputs of `CaptureModel` must be executed in the module thread (`ModuleRuntime`), usually the main
  thread of Qt.
- `ModuleRuntime.post` must glue, not run inline. The model uses that executor to serialize frames,
  states and selection results.
- `WgcCaptureCore` creates its own `std::thread`. Core callbacks come from that thread and the model
  publishes them.
- `stop()` is synchronous: marks the stop, makes `join` thread and releases session, pool and
  factory. It should not be called from the catch thread callback itself.
- Capture is not restarted after `stop()`; to reshare requires a new selection and invitation.
- The model uses a `epoch` and the operating reference to rule out frames and states that arrive
  after a stop.

## Construction

Requisitos:

- Windows 10/11 with an interactive graphical session for real capture;
- Visual Studio 2022 with C++ and Windows SDK **10.0.19041** or later tools;
- CMake 3.24 o posterior;
- Qt 6 Widgets only if you want to use the selector parent;
- C++17.

```powershell
cmake -S cmake/capture_windows -B build/capture_windows `
  -G "Visual Studio 17 2022" -A x64 `
  -DONEINTWO_BUILD_TESTS=ON `
  -DONEINTWO_CAPTURE_QT=OFF `
  -DONEINTWO_REQUIRE_WGC=ON

cmake --build build/capture_windows --config Release
ctest --test-dir build/capture_windows -C Release --output-on-failure
```

`ONEINTWO_REQUIRE_WGC=ON` causes the configuration to fail if the headers of WGC are missing,
instead of quietly constructing the port of indisponibility. It is the option used by IC.

On Linux you can build the portable model and its tests, but the native Windows target is not
compiled. The model uses a fake port and does not need WGC, D3D11 or a desktop:

```bash
cmake -S cmake/capture_windows -B build/capture_windows \
  -DONEINTWO_BUILD_TESTS=ON -DONEINTWO_CAPTURE_QT=OFF
cmake --build build/capture_windows
ctest --test-dir build/capture_windows --output-on-failure
```

### SDK y CMake

- The minimum SDK is **10.0.19041** because the code uses
  `IGraphicsCaptureSession2`/`IsCursorCaptureEnabled` in the headers.
- In runtime the cursor property is obtained by QI and is optional. A Windows that supports WGC but
  not that interface can capture, although the cursor can appear in the image.
- The SDK 19041 lane uses `/permissive` for `oneintwo_capture_windows` because the cppwinrt included in that SDK does not compile with current `/permissive-` and MSVC v143. `/permissive` should not be applied globally or changed to `/permissive-` without retesting SDK.
- The target of the model retains `/permissive-`; the exception is only for the old native target.
- `ONEINTWO_CAPTURE_QT=ON` searches for Qt6 Widgets. If not available, the null parent is used; IC
  disables it to not depend on Qt.
- Import libs include `runtimeobject` and `windowsapp`. `runtimeobject.lib` contributes imports of
  Windows Runtime, but there is no `runtimeobject.dll` to be loaded into the test:
  `RoGetActivationFactory` is checked at `combase.dll`.
- `windowsapp.lib` is a library umbrella and `windowsapp.dll` may not exist as independent DLL; the
  test treats it as optional. `GraphicsCapture.dll` is also checked optionally and the final verdict
  gives `IsSupported` plus interoperability.

### Evidence

| Test | What it covers |
| --- | --- |
| `capture_windows` | Portable model with false port: capabilities, selection, cancellation, frames BGRA, alpha, sizes, order of states and late callbacks. |
| `capture_windows_wgc` | WGC core with doubles: optional cursor, configuration order, stop without subsequent callbacks and pool recreation. |
| `capture_windows_wgc_probe` | Test with false runtime: DLL/entry point absent and WGC unsupported. |
| `capture_windows_wgc_probe_process` | Fake test execution as process and check output codes. |
| `capture_windows_wgc_smoke` | Win32 + Royal WGC: creates a window, receives at least one valid BGRA frame, closes it and checks that there are no callbacks after `stop`. |
| `test_cmake_guards.ps1` | CMake negative guards: SDK prior to 19041 and API missing cursor. |

The smoke test returns **77** and CTest marks it as skip when there is no visible window station or
WGC is not supported. It is the correct behavior in a runner without interactive desktop, in
RDP/headless or in a locked session; it should not be interpreted as a verified capture. On a real
Windows computer you have to run it in a unlocked session to validate the native path.

The workflow `Windows MSVC` compiles and executes the tests in two yarns: SDK `10.0.19041.0` and SDK
by default of the runner. It also builds the link check, the real test and the smoke. The branch
should not declare catch compatibility just because it has green CI: the smike may have been
omitted.

## Major challenges and factors

### 1. WGC is not a API of universal capture

`IsSupported()` may be true and the creation of the capture object fails equally by policy, by the
graphic adapter, by a non-interactive session or by the state of the desktop. `visualCapabilities()`
is a thick check; the actual result is known when adopting and booting. `unavailable` and `closed`
must be treated as normal results, not as impossible errors.

The capture of protected content (DRM, some high applications or system surfaces) can produce black
or fail without a different state. There is no detection of protected content. UAC, the secure
desktop, the lock screen and some surfaces of the system are not captureable; the coordinator must
stop the operation when the session is blocked.

Windows can draw an edge or capture indicator over the source. The backend does not control that
system decoration and should not attempt to hide it.

### 2. GPU requirements and format

The backend creates a D3D11 **hardware** device, without fallback WARP. In a VM without GPU, in RDP,
on a headless server or with an adapter that does not allow BGRA, `D3D11CreateDevice` format or pool
creation may fail and the source is unavailable.

The copy only accepts `DXGI_FORMAT_B8G8R8A8_UNORM`. If the texture arrives in another format,
`WinRtFrameCopier::copy` returns `false` and the frame is discarded. The loop does not change to
`closed` due to a repeated copy failure: the consumer can see a frozen image without receiving an
error state. A format change, HDR/10 bits or a mapping failure must be investigated with local
instrumentation; it is not resolved by retrieving the capture from the coordinator.

### 3. Source identity and reuse of handles

The selection retains the `HWND` or `HMONITOR` and the catch object. You don't search by title again. For a window, `available()` checks `IsWindow` and PID remains the same; for a monitor, `GetMonitorInfo` and device name.

These checks reduce the risk, but **do not eliminate** the reuse of a `HWND`/`HMONITOR` for another
object if the system recycles the handle between two checks. The main barrier is the retained
capture object and the closing of the operation; a new window with the same title should not be
adopted or the reference reused. If the window closes, the operation ends and a new invitation is
needed.

Monitors can change topology, resolve again, or reconnect. The backend does not fail another
monitor: if the identity does not match, it publishes `closed`.

### 4. Selector life cycle

There are two points that the integrator must respect:

- The selector's callback captures the `CaptureModel`. `cancelPrompt()` only publishes `WM_CLOSE`,
  so the answer can arrive asynchronously. The adapter must not be destroyed with a pending
  selector: the terminal response must be delivered and `ModuleRuntime.post` drained before
  releasing the model and its runtime.
- `prompt_window_` is not cleaned when the user closes the selector normally. A call after
  `cancelPrompt()` could send `WM_CLOSE` to a recycled `HWND`. `IsWindow` reduces the risk window,
  but does not prove the handle identity. It is a point to harden if the selector life cycle is
  changed.

Nor should the `QWidget` parent be destroyed while the selector is open: `WM_DESTROY` tries to
reactivate it with `EnableWindow`, and the handle may have ceased to be valid.

### 5. Wires, apartment COM and stop

The model does not impose the thread, but the product contract requires that its entries be executed
in the module thread and that `post` do not execute inline. If you call `selectSource` or
`startCapture` from another thread, serialization and `epoch` cease to protect the state of the
intended form.

WinRT objects are created in the thread that calls `createWin32Capture` and the loop uses them from
its own thread. `initialiseApartment` tries MTA and tolerates that the thread is already
initialized, but no agile references or explicit marshalling are used. Do not change the thread of
creation or consumption without checking the agility of the objects and apartment COM.

`WgcCaptureCore::stop()` does `join`. Calling it from the frame pool callback, from the catch thread
or from a path that can wait for that thread produces a lock. The model calls it from the module
thread after invalidating the operation; maintaining that direction.

### 6. Without frame does not mean closed source

The backend does not have a frame inactivity timer. A minimized window is detected by `IsIconic` and
produces `suspended`; a window that still exists but does not deliver frames (e.g. a blocked surface
or a copy failure) can leave the consumer without image and without `closed`. The common module must
use its activity timer and its closing policy; it is not enough to wait for a frame to know that the
source is alive.

On a monitor, the session lock or screen shutdown is not automatically converted to `suspended` or
`closed`. The coordinator must attend `DesktopEvents::locked` and the authorization changes, and
stop the operation.

### 7. Privacy and security

Images are unencrypted screen content in memory. The backend does not delete buffers when releasing
them, does not apply wording and does not distinguish sensitive content. The coordinator should:

- not record titles, source labels or frames;
- do not send `NativeSource.label` per protocol;
- treat `VisualFrame` as sensitive data and release the operation as soon as possible;
- close capture by blocking the session, withdrawing permissions, or losing the connection.

Titles are used only in the local selector. A window tag is not a stable identity: it should not be
used to find it again.

### 8. Rendimiento

Each frame is copied from GPU to a tagging texture and from there to a `std::vector`. It is an operation of width × high × 4 bytes per frame, plus the cost of `Map`/`Unmap`. In 4K or large windows the cost of CPU and memory can be high; backend does not scale before copying or limits to 30 fps.

The loop does not introduce a wait between frames when the pool delivers frames continuously.
Frequency control, scale and discard by queue belong to the common module. Do not add a queue of
frames here: the property and limits are already defined in the contract.

The internal maximum sizes (16384/64 My pixels) are a defense against absurd reserves, not a
performance target. A frame within those limits can still be too big for the output budget.

### 9. DPI, windows and contents

The window size tag uses extended bounds of DWM; the actual size of the WGC content may differ by
edges, shadows or DPI. Do not use that label to crop or calculate the image. The image size is
always the one that delivers the frame.

The application must be aware of DPI per monitor so that the selector position and labels are
consistent. An application DPI-oneware can see virtualized coordinates; the capture remains system,
but the selection IU may be misplaced.

WGC delivers the surface that the system considers the selected window. Menus, dialogs and popups
that the system treats as independent windows are left out; a desktop region is not cut off. A
window with an empty title or a tool window does not appear in the list.

### 10. Evidence coverage

The portable tests do not exercise WGC, D3D11, the Win32 selector or the behavior of a real session. The actual smoke test only checks that a valid frame with size and buffer arrives, that the closure produces `closed` and that there are no callbacks after `stop`; does not verify content, occlusion, change of monitor, DPI, HDR, DRM or interaction with the selector.

In IC the smoke can be omitted due to lack of a visible desktop. Manual validation in Windows should
include at least:

1. two windows with different patterns, overlapped and on different monitors;
2. move and resize the original window;
3. minimize and restore;
4. close the window and open another with the same title;
5. cancel the selector and close it with the X;
6. test a protected window or a high application;
7. check the cursor and the border or indicator system capture;
8. repeat with the minimum SDK and with the current SDK.

## Relationship with Wayland

The Wayland backend is at [`../../linux/visual`](../../linux/visual/) and implements the same
facade. It does not share native code with Windows:

| Aspecto | Windows | Wayland |
| --- | --- | --- |
| Selection | Win32 window and monitor list. | ScreenCast portal dialogue; the application does not list windows. |
| Authorisation | Local selection; no WGC dialogue. | Portal and composer; may return cancelled or denied. |
| Source type | `HWND`/`HMONITOR` retenido. | The portal returns `source_type`; without attestation it is considered unavailable. |
| Datos | D3D11/WGC, copia BGRA. | PipeWire, conversion from BGRA/BGRx/RGBA/RGBx. |
| Cursor | Optional QI of `IGraphicsCaptureSession2`. | `cursor_mode` from the portal. |
| Shutdown | `IsWindow`, PID, monitor identity, and WGC. | Closure of the PipeWire portal and closure. |

Differences in availability, format and selection should not be hidden with a Windows failback on
monitor or a region capture. The common contract requires each backend to declare what it can
actually prove.

## Checklist for the integrator

- [ ] Call operations from the module thread and use a real `ModuleRuntime.post` that I actually
      plow.
- [ ] Wait for a terminal response of `selectSource`; do not create a second selection while there
      is a slope.
- [ ] After `ready`, call `startCapture` only for the accepted reference.
- [ ] Do not expect an initial `available` state; accept the first frame as content.
- [ ] Empty the viewfinder upon receipt of `suspended` and close the operation upon receipt of
      `closed`.
- [ ] Stop the capture by blocking the session, withdrawing permissions, closing the viewer or
      losing the connection.
- [ ] Scale and limit frequency in the common module; do not assume that the backend delivers
      1920×1080 or 30 fps.
- [ ] Do not register titles, tags or frames; do not send native identifiers by protocol.
- [ ] Test the real path in a unlocked Windows session, not just CTest.
- [ ] If the selector or life cycle is changed, try again cancellation, parent closure and
      destruction during an outstanding selection.

## Related documentation

- [Share a window](../../../../docs/SHARE_WINDOW.md)
- [Share a screen](../../../../docs/SHARE_SCREEN.md)
- [Parallel implementation and common contract](../../../../docs/PARALLEL_IMPLEMENTATION.md)
- [Common visual contract](../../../features/visual_share.hpp)
- [ Desktop contract](../../desktop.hpp)
- [package CMake](../../../../cmake/capture_windows/CMakeLists.txt)
- [Smoke test WGC](../../../../tests/capture_windows/wgc_smoke.cpp)
- [Backend Wayland](../../linux/visual/wayland_visual_capture.hpp)
