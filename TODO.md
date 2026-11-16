# Challenges ahead

## VISUAL-WIN-01 — Close selector without callbacks after destroying CaptureModel

State: pending, detected when documenting `feature/share-window` on November 15, 2026.

[`CaptureModel::selectSource`](src/platform/windows/visual/capture_model.cpp) delivers to the port a callback that captures `this`. When the model is destroyed, the selection is reset to `cancelPrompt()`, but the real selector only publishes `WM_CLOSE`; the answer may arrive after the model has disappeared. That callback would execute `post()` on an already liberated object. Current tests cover cancellation while the model is still alive, not destruction with a pending prompt.

Reproduction:

1. Put [`FakeCapturePort`](tests/capture_windows/fake_port.hpp) in ferred mode and call
   `selectSource`.
2. Drain `ModuleRuntime.post`.
3. Destroy `CaptureModel` without completing the prompt.
4. Complete the pending prompt.
5. Repeat with ASan or equivalent instrumentation of life of objects.

**Acceptance criterion:** a regression test shows that the late response to a already destroyed
selector does not execute callbacks, does not access a liberated state and does not cause failure;
while the model is still alive, `stopCapture` delivers exactly a terminal result `cancelled` and no
subsequent callback.

## VISUAL-WIN-02 — Invalid selector HWND and owner when closed

Status: pending, detected in the branch review on November 15, 2026.

In [`win32_selector.cpp`](src/platform/windows/visual/win32_selector.cpp), `prompt_window_` is only placed at `nullptr` when called `cancelPrompt()`. If the user closes the selector with the X or Cancel, `WM_DESTROY` delivers the result but leaves the `HWND` stored. A call after `cancelPrompt()` can send `WM_CLOSE` to a reused handle; In addition, `WM_DESTROY` reactivates the owner with `EnableWindow` without checking that it remains a valid window. Destroying the `QWidget` parent with the open selector exposes the same problem.

Reproduction:

1. Open a selector, close it normally and keep the port instance.
2. Reuse or simulate the reuse of the enclosed `HWND`.
3. Call `cancelPrompt()` and check if a message arrives at the wrong window.
4. Destroy the parent widget with the selector still open and observe the reactivation of the owner.

**Criteria of acceptance:** when closing the selector by any means, the port ceases to retain its
`HWND`, `cancelPrompt()` is a non-op after closing and never sends messages to a reused handle; the
owner is only reactivated if `IsWindow` confirms that it is still valid. A regression test covers
the normal closure followed by `cancelPrompt()` and the destruction of the father with the selector
open.

## VISUAL-WIN-03 — Publish closure for repeated frame copy failures

Status: pending, detected in the branch review on November 15, 2026.

[`WinRtFrameCopier::copy`](src/platform/windows/visual/wgc_capture.cpp) returns `false` if the texture is not `DXGI_FORMAT_B8G8R8A8_UNORM` or if it fails `Map`. [`WgcCaptureCore::poll`](src/platform/windows/visual/wgc_capture_core.cpp) He simply discards that frame and keeps probing. `closed` or any other state, so the consumer can see a frozen image without knowing that the capture was no longer useful. `copy` failure after `Map`the path of exception does not guarantee `Unmap`.

Reproduction:

1. Run `oneintwo_wgc_capture_tests` with a fake `WgcFrame` whose `copyTo` returns `false`
   consecutively.
2. Check that no frames are delivered and that no `closed` are published.
3. Enter a bug after `Map` and check the status of the tagging texture.

**Criteria of acceptance:** after a documented and proven threshold (proposed: 10 consecutive copy
failures or 2 seconds without valid copy), the kernel publishes `closed`, stops the thread and does
not deliver more callbacks. Any successful call `Map` is compensated with `Unmap` in success and in
exception. The false frame test covers the threshold, stop and absence of late callbacks

## VISUAL-WIN-04 — Test identity before reused window handles

State: pending hardening and testing; documented risk on November 15, 2026.

[`WinRtWgcSource::available`](src/platform/windows/visual/wgc_capture.cpp) checks `IsWindow` and that PID of the window remains the same, and for a monitor compares `GetMonitorInfo` with the device name. This reduces the risk, but there is no test that reuses a `HWND` or a `HMONITOR` with the same numerical value for another source. Current tests cover closing and reopening with the same title, not the effective reuse of the handle.

Reproduction:

1. Select a window and start your capture.
2. Simulate with the false port that the source disappears and that the same numerical handle goes
   on to identify another window or the same window with another PID.
3. Deliver frames from the new source to the old catch.
4. Repeat for a monitor that disappears and returns with the same name.

**Criteria of acceptance:** a deterministic test shows that the old operation does not deliver any
new source frames and ends at `closed` before accepting the following; a new invitation can select
the new source. The adapter does not re-find the source by title. The test and manual validation
record the case of proven handle reuse.

## VISUAL-WIN-05 — Validate real catch WGC on Windows

Status: manual execution pending; IC smoke can return skip 77.

The portable tests and the smoke cover parts of the way, but the product matrix has not been
executed on a real Windows desktop with the binary of this branch. The smoke only checks a valid
frame, closure and absence of callbacks after `stop`; it does not check content, DPI, cursor,
capture edge or protected content.

**Criteria of acceptance:** in a Windows session 10/11 interactive and unlocked, `oneintwo_wgc_smoke` ends with code 0 (no 77) and registers a matrix with, at least: two overlapping windows with different patterns, monitor change, resized, minimizing and restoring, closing and reopening with the same title, cancellation of the selector, DPI 100% and 150 % and window protected or elevated with its expected result. Windows build is registered, SDK, GPU, result of cursor/borde and any skip; no support is declared if the smoke was only omitted.

## VISUAL-WAYLAND-01 — Validate Wayland backend with real PipeWire portal

Status: native validation pending; current tests use simulated PipeWire and portal.

The backend [`../../linux/visual`](src/platform/linux/visual) requires that the ScreenCast portal
announce `WINDOW`, that the response attests the type of source and that the authorized PipeWire
node deliver convertible frames to BGRA. Double tests cover the flow and cancellation, but not the
actual variation of PipeWire portal, composer and format versions.

**Acceptance criterion:** in Ubuntu/Wayland with a portal announcing `WINDOW`, select a single
window, receive BGRA frames, change its size, minimize it and restore it by registering if the
portal suspends or maintains the stream, close it and cancel a selection. It is verified that it is
not replaced by a monitor or captured a region, and composer, version of the portal, PipeWire format
and result are recorded. If the portal does not credit the type, the operation must remain
`unavailable`, not “function” by fallback.

## VISUAL-INTEGRATION-01 — Connect Capture, VisualShare, Codec, Transport and Viewer

Status: pending; [9`visual_share.cpp`](src/features/visual_share.cpp) remains the coordinator's stub
and the backend cannot be used as a standalone product.

The common visual contract must maintain an operation, validate permissions, revisions, sequence and
limits, encode JPEG, transport fragments and present in the viewer. It must also cover the absence
of frames, session blocking and closing of the source with the already documented activity policy.

**Acceptance criterion:** two instances with real backends complete the route select → invite →
accept → view image → stop, without sending video before acceptance. Minimize and close the source
empty the viewer and finish the operation; a session lock or a withdrawal of permission terminates
it without automatic resumption. A source that stops delivering frames activates the activity timer
and closes or empties the viewer according to the agreed status. No titles, source tags or frames
are recorded. The integrated test and manual record platforms, versions and limits of the checked.
## FEATURE-SHARE-SCREEN — Delivery integration earrings P

They come from [the documentation of the feature](docs/feature-share-screen/README.md) and remain
separate from the experimental slopes.The criteria seek observable results; they do not replace the
native validation of branches V and N.

## PSS-01 — Persisting peer generation between restarts

Status: **pending**.

`SessionLink` accepts `last_peer_generation` and `remember_peer_generation`, but `Application` does
not connect them to any warehouse. Protection against repeat generations only lives for the duration
of the process.

**Acceptance criterion:**The last remote generation accepted is saved by `device_id` and loaded
when constructing `SessionLinkOptions`. After a reboot of Application with the same warehouse, a
generation equal to or less than the saved one must be rejected with `SessionLinkError::generation`;
a superior one must be accepted. A rejected attempt cannot advance the persistent brand. There must
be an integration test with two sequential instances of Application, not only a unitary test of
`SessionLink`.

## PSS-02 — Make transactional and atomic `LocalStore`

Status: **pending**.

`LocalStore` still writes in a fixed time (`path + ".tmp"`), does not do `fsync` and does not return
the save result. A bug may leave diverging memory preferences of the file or lose the previous
content.

**Acceptance criterion:** `saveProfile` and `saveAllowedApplications` write with a single
temporary and atomic replacement, do `fsync` in POSIX and update the memory status only after
persisting. A forced write failure must keep the previous file and memory status, without residual
temporary. After a correct save and a new instance of `LocalStore`, the profile and permitted
applications must be recovered. There must be a test with the target path locked and another one of
recharge.

## PSS-03 — Select explicitly between several saved pairs

Status: **pending**.

`PairingStore` supports multiple pairs, but `Application` reuses `devices().back()` when booting.
With more than one saved computer there is no selection and the order of the file implicitly
decides.

**Criteria of acceptance:** when booting with two or more pairs, Application must not connect
silently with `devices().back()` or let the order of the TSV decide. There must be an explicit
selection visible to the user and the choice must persist. A test with two different peer and
endpoints must demonstrate that only the selected one is contacted and that the choice survives the
restart. If the selected one no longer exists, the application must display it and not connect to
any other pair.

## PSS-04 — Integrate native capture of V and produce executable

Status: **to be integrated**.

The native catch of monitor/ventana belongs to `feature/share-window` (V) and is not part of the
composition of this branch. Target `oneintwo` is omitted if there are no `oneintwo_capture_windows`
or `oneintwo_capture_wayland`, so today it is only tested with `UnavailableCapture`/dobles..

**Criteria of acceptance:** on Windows and Linux, CMake incorporates the native target of V and
generates `oneintwo` without test factories. A real execution must open the native selector, share a
monitor or window and deliver images to the viewer; stop, viewfinder closure and source loss must
release the capture. The test cannot be declared exceeded using a simulated adapter.

## PSS-05 — Integrate N notifications and remove doubles from product

Status: **to be integrated**.

The native notifications belong to `feature/notifications` (N). The Application kernel is tested
with `SharedNotifications` replaced in `tests/application/application_test.cpp`; the product
executable does not yet link `oneintwo_notifications`.

**Criteria of acceptance:** the product executable links the actual N target and does not contain
the notifications test implementations. At least a manual shipment must be submitted on Windows and
Linux and an adapter failure must reach the UI without blocking the session. The final binary must
compile and boot without simulating `oneintwo_notifications`.

## PSS-06 — Implement Peers discovery and reconnection

Status: **pending**.

Pairing requires manual host and port. There is no discovery in the LAN, resolution of direction
changes or automatic reconnection.

**Criteria of acceptance:** two instances in the same LAN must be able to locate and select the peer
without manually entering its address. A change of IP or peer port must be reflected without
creating a new confidence: identity is still checked with the pin and, if it is in transition, with
SAS. There must be an endpoint discovery and update test, in addition to a proof that an
unauthenticated peer does not gain access to functions.

## PSS-07 — Define and verify the unique writer of `PairingStore`

Status: **pending**.

`PairingStore` does not have mutex or interprocess block. `QSaveFile` avoids timeshares and partial
replacements, but two instances that load and save at the same time may lose an update.

**Criteria of acceptance:** the application must impose a single instance of writing or implementing
a block that serializes mutations. In any case, two independent writers cannot corrupt the file or
lose a confirmed pair; a write failure must keep the previous file. The test must cover two
`PairingStore` on the same path, including a failed script, and demonstrate documented behavior.

## PSS-08 — Recovering a corrupt pair store when booting

Status: **pending**.

If `paired-devices.tsv` is corrupt when building Application, the warehouse is disabled and the
selection of pairs in memory is lost. `reload()` retains the last valid state only if the instance
was already loaded.

**Acceptance criterion:** a file with invalid header, truncated line, invalid port or duplicate
entry cannot quietly delete all pairs or overwrite with an empty state. The application must keep
the invalid file in quarantine or as a copy, display an actionable error and not continue with an
empty warehouse without consent. If a valid copy exists, it is restored; if not, it is offered to
repair or re-matched. The proof must show that valid pairs are not lost without consent and that the
previous state is recoverable.

## PSS-09 — Validate end-to-end screen sharing between computers

Status: **pending**.

The current suite validates protocol, TLS, composition and boot, but not a real capture nor the
entire chain between two teams.

**Acceptance criterion:** with the P, V and N integrated binaries, perform Windows → Linux and
Linux → Windows on a LAN: select monitor or window, accept the invitation, view images in the viewer
and stop from both ends. You must check accept, reject, expiration, viewfinder closure, network loss
and recovery without automatic restart. The evidence should indicate review, platforms and any
non-executed case; `QT_QPA_PLATFORM=offscreen` or doubles do not count as desktop validation.
## NOTIF-WIN-01 — Validate manual presentation and removal on Windows

Status: pending. Requires a Windows computer with MSIX installable, package identity and user
present. The IC only compiles, installs and removes the package; it cannot confirm the visual
presentation or attribution of the computer.

Acceptance criterion:

- [ ] Install the MSIX signed and start the packaged application.
- [ ] A manual notice sent from the other computer appears only once, with title, Unicode body and
      `From <device>`; it does not show buttons, links or actions.
- [ ] `withdrawNotices` withdraws the active own warning.
- [ ] A pending notice does not reappear after blocking, disconnecting or switching generation.
- [ ] Register Windows version, package identity and result.

## NOTIF-WIN-02 — Validate consent, discovery and forwarding

Status: pending. The native test without package already covers closed rejection; real observation,
consent and list of applications require a Windows computer with MSIX and user present.

Acceptance criterion:

- [ ] No package identity, `notificationCapabilities().observe` is fake and `discoverApplications`
      complete with `unavailable` without asking for consent.
- [ ] With package, `observe` reflects technical support; consent is checked upon discovery or
      initiation and a rejection produces `denied`, no `unavailable`.
- [ ] `discoverApplications` may request consent, does not initiate observation or alter its
      baseline, returns unique and orderly applications, excludes OneInTwo and discards empty
      identities.
- [ ] A new notice of an authorized application comes only once with its visible name; previous
      notices, unauthorized applications and OneInTwo notices are not forwarded.
- [ ] Withdraw consent in Settings stops observation in less than five seconds and produces a
      recoverable local error.
- [ ] After re-granting permission and restarting the observation, the baseline is new: the
      accumulated history is not forwarded.
- [ ] Register Windows version, package identity and result.

## NOTIF-LINUX-01 — Validate presentation and withdrawal in Ubuntu real

Status: visual check pending. The adapter is tested with a private session bus, but the presentation
has not been certified on a real graphic desktop.

Acceptance criterion:

- [ ] With `org.freedesktop.Notifications` available, a manual notice and an authorized forwarding
      appear only once, with the source equipment; the forwarding includes the name of the source
      application.
- [ ] The Unicode text is preserved and no buttons, links or actions appear.
- [ ] `withdrawNotices` withdraws own asset notices.
- [ ] `notificationCapabilities().observe` is fake, `discoverApplications` returns `unavailable` and
      manual shipping continues to work.
- [ ] Register version of Ubuntu, notification service and result.

## NOTIF-RESUME-01 — Renew the base line when Windows resumes

Status: pending integration. The module does not itself receive a restart event; the observation
must be restarted in order not to forward notices accumulated during suspension.

Acceptance criterion:

- [ ] The coordinator restarts the observation with a new subscription upon return from suspension,
      or the adapter displays an equivalent signal and the test covers it.
- [ ] Warnings generated during suspension are not forwarded upon resumption.
- [ ] A new warning generated after resuming arrives exactly once.
- [ ] The automated test uses simulated watch and subscription; the manual check in Windows records
      the case and the result.

## NOTIF-MSIX-01 — Complete integrity check APPX

Status: pending. The current check validates PKCS#7 of `AppxSignature.p7x` and that the signatory is
the ephemeral certificate, but does not link the digests `AXPC`, `AXCD`, `AXCT`, `AXBM` and `AXCI`
(if any) to the actual package.

Acceptance criterion:

- [ ] Extract and parse the `SpcIndirectDataContent` signed, including the digest header APPX.
- [ ] Recalculate the applicable digests on the package without its signature entry and compare them
      with the signed values.
- [ ] A valid package passes; altering the payload, central directory, block map, content types or
      signature causes an explicit bug.
- [ ] The job `msix` record `signature_integrity=verified` unused `signtool verify` and the API WinTrust/Appx.
- [ ] The confidence of the chain and installation are outside this criterion and are validated
      separately on the Windows checking machine.

## EXP-FILES-LAN-01 — Complete glued Explorer and on demand

Status: T1 complete; T2 CLI receive and copy and complete integrity. TLS negative both authentication
senses, corte/reintento, collisions, permissions and GC Windows exceeded. Closed servers; no new
rules. Evidence and limits in experiments/files/RESULTS.md. Earrings pasted real Explorer (T2/L2),
on demand Windows (T3/L3) and behavior of Explorer in court. Desktop control blocked by helper boot
failure; no human interaction requested nor replaced pasted by copy CLI. Text suspended.

## EXP-CLIP-LAN-02 — Contrast echoes and initial team selection

Status: Pending conclusion; text experiment suspended by the user. Do not start new tests or text
connections while advancing in files.

TLS and greeting worked and the user confirmed the pasting in both directions. The Windows registry
includes an initial update Ubuntu of 48 bytes, finite re-emissions, rejections by order and two
local 147 bytes catches separated by about 277 ms. The agent Ubuntu reports prior selection and
re-emitting. Compare relative times, origin, generation, logical counter and user actions before
concluding if there are incorrect echoes or duplicate notifications. Do not confuse absence of
infinite loop with correct policy.

Shared log: `experiments/clipboard/lan/evidence/windows-session-01.log`. Do not apply speculative
fixes or repeat desktop tests without an agreed case. Incorrect certificate and reconnection are
still untested.

## EXP-CLIP-TLS-01 — Reading failure PEM on Windows startup

Status: **corrected and verified on Windows**, November 5, 2026.

The server ended by `no OPENSSL_Applink` before opening the port. Both transport certificate
readings were changed to BIO of memory. Insulated regression exceeded with the two public
certificates, correct rejections and self-test 149/0. The server came to listen and completed the
first TLS session with pasted in both directions, confirmed by the user. Then its temporary rule was
closed and withdrawn. Recompiling this correction in Ubuntu pending confirmation with its agent.
Evidence in `experiments/clipboard/RESULTS.md`.

## EXP-LAN-01 — Complete the preparation of the first connection

Status: preparation completed and first session completed. Windows server closed, zero listens and
time rule removed. TLS/saludo and two-way pasted confirmed; echoes, initial selection, incorrect
certificate and reconnect pending validation.

The [package LAN clipboard](experiments/clipboard/lan/README.md) includes tested assistant without red/escritorio, synthetic samples and public certificate Windows. The private key remains only in `.local-state/lan-clipboard/windows/`. The observed Windows profile is Public. A limited rule was opened to the trial and a listener in 47831; both have been closed at the end of the first session.

Ubuntu advanced without conflict to `6c2ec07`, generated his identity and exported only `public/ubuntu.pem` and `public/ubuntu.sha256`. Local private key ignored; prints of both certificates consistent with their archives SHA-256. IP LAN Ubuntu consulted and communicated to the user outside Git. Without recompiling C++ or repeating valid tests: Windows commits did not change those sources. Windows incorporated that delivery, verified the identity and communicated its IP. For the next attempt re-agreed review, identities, boot and time rule. The first session does not automatically validate all matrix cases; see EXP-CLIP-LAN-02 and the clipboard report.

## EXP-BUILD-01 — Complete evidence of native compilation

Status: **completed in Ubuntu/Wayland and Windows for delivery review; X11 without new evidence**.
Updated: November 5, 2026.

Windows reconstruction performed over `2ae444df56689dbaebe9b796daca26a55bd02917` in
`validation/windows`: the three prototypes compile in Release x64 with OpenSSL. Self-tests: 149/0
clipboard, 176/0 files and 579/0 input; help of the three correct executables. Without Windows
desktop or network testing.

Verified base: `450cbb2e340a381bda01274eb86df0e05934e822`, local branch `validation/ubuntu`, plus
the minimum correction `<signal.h>` → `<csignal>` in the transport TLS of clipboards and local
corrections of pipe reading, publication life and emptying of the log; action GTK and format of
Nautilus in files; preparation and local output of the input demonstration. Branch of delivery of
these corrections: `validation/ubuntu`.

- Clipboards: libdbus portal and TLS OpenSSL compiled in Ubuntu 26.04.1; **153/0**, output 0, after
  adding the regression of deferred reading of pipes (151/0 initials).Corrected `std::signal`,
  EAGAIN, life of the supply and emptying of metadata.
- Files: GTK3 and TLS adapter compiled, without replacing the adapter with `clipboard_unavailable`;
  **176/0** previous. Back reconstruction correct after finishing the received names; R$L6/L7/L8
  exceeded without repeating the battery.
- Input: Wayland GDBus/libei and TLS compiled with X11 disabled; **579/0** after safe preparation
  regressions (538/0 above), plus Response and memory shortcut signals. Corrected
  callback/resultados, session type, reception of shortcut and separation of output from control
  EIS, R2/R9 premises exceeded; recovery from pending equipment cuts; X11 without new evidence.
- Windows: reconstruction of the three experiments and self-tests completed on the commission of
  delivery. New evidence at the beginning of its `RESULTS.md`; the previous evidence is preserved as
  historical.

The dependencies are conserved in `.local-state/ubuntu-validation/`, within the project and excluded
from Git, without global installation, `sudo` nor firewall changes. To use them from the root of the
project: `source .local-state/ubuntu-validation/env.sh`. They moved from `/tmp` and reconfigured the
three compilations to use the persistent location. Commands, versions, incidents and logs are
recorded in the three `RESULTS.md`.

Use the explicit options of each manual to avoid a satisfactory compilation that omits the backend
or TLS. Do not confuse streams with doubles with native adapters. Register commands, dependencies,
versions and errors.

## EXP-MANUAL-01 — Validate capabilities on actual equipment

Status: **Intended local phase completed in supported configuration; recorded limitations and cases
between outstanding equipment**.

Clipboards and file assist pasted already have local evidence. After the requested pause entry resumed: actual cancellation of RemoteDesktop verified after correcting Response and the type of `session_handle`. Preparation requires an application identity and a single active monitor. The user confirmed to press Ctrl+Alt+F12, observe the entire frozen PC and **need to restart to recover it**. The process did not receive the emergency and ended for time; It's not active. The new shortcut, without EIS, passed later with the guide and 30 s timer: output 0 and desktop responding, confirmed by the user. Then passed the local demonstration EIS: short movement and text `oneintwo`, output 0 and desktop responding, confirmed by the user. Then they passed R2 (emergency with EIS active) and R9 (withdrawn from GNOME), both output 0 and operating desktop. Courts and states held between teams are still pending.

On November 5, the user resumed the validation with confirmed presence after the isolated stage.
Reading, publishing and pasting, observation and cancellation of the clipboard portal in GNOME Wayland
were tested. The faults found and repetitions are preserved in the reports. Also the supported
pasting of files in Nautilus, with correct integrity, diagnosis and local injection input. No
capture or connection with Windows has been initiated. The only test network has been TLS files in
loopback for L6/L7/L8.

- Clipboards: reading, pasting, observation and cancellation verified, `devices=0`. Back isolated
  copy: a human action, a sample notification and 12 s without repetitions. Earrings between teams:
  echoes, reconnection and real synchronization.
- Archives: pasted assisted in correct Nautilus, 5 archivos/5.551.150 bytes. L6/L7/L8 with TLS local
  mutual overcome after correcting the renamed `.o2f-part` verified in the CLI: collisions without
  overwriting, permission error, cancellation without remains and clean cache. Automatic publication
  and on demand Linux are recorded limitations; pending Explorer and transfers Windows/Ubuntu.
- Entry: real diagnosis with InputCapture v1, RemoteDesktop v2 and GlobalShortcuts v1. Ubuntu origin blocked by requirement v2; no capture or capture enabled X11. The first preparations failed before the dialogue; response and type of `session_handle`. Verified actual cancellation. The GlobalShortcuts reception route was also corrected; the wait was reached with application identity and a monitor. The user communicates freeze when pressing the shortcut; recovery failed. The new one was prepared `--check-target` GlobalShortcuts and Ctrl+Shift+ onlyF12with 579/0 isolated. The isolated shortcut and local demonstration EIS passed with 30 s guide, text, movement and desktop responding; R2/R9 exceeded premises with output 0 and operating desktop. Cruce/retorno and cuts between pending teams.
- Network: verify acceptance and rejection of identities in prototypes and real transfers.

Follow the three `MANUAL.md`, one experiment at a time, and record results observed in its
`RESULTS.md`. Self-tests and the presence of code do not close these points.

To resume desktop: present user, datos/ventana disposables and ready output; check the local
shortcut before enabling capture. Before each case Windows/Ubuntu, agree to a common review
including correction, binary, roles, IP/puerto, public footprints, duration, observation and
recovery. The user has authorized publishing the validation branch; it has not yet been connected to
Windows. The application integration remains work later.

## EXP-INPUT-01 — Conciliate Wayland delivery and documentation

Status: **resolved in documentation**, November 5, 2026.

`experiments/input/README.md`, `MANUAL.md` and `RESULTS.md` already describe Wayland implementation,
its requirements, manual testing and the limits of evidence. The isolated check of the Windows
binary gave 538/0; the above 403/0 are identified as historical. Native and manual verifications
remain open at the above points.

## EXP-INPUT-02 — Freezing observed when testing local output

Status: **Locally proven corrections with R2/R9; recovery from court between teams pending**,
November 5, 2026.

In GNOME Wayland, with an active monitor and RemoteDesktop/EIS authorized, the user pressed Ctrl+Alt+F12 during `--check-target --max-seconds 60` and observed that the entire PC was frozen. The process did not receive the emergency signal and ended in time, exit 1. No movement events, buttons or keys were sent, but a real EIS session was prepared. The user confirmed that ** had to restart to recover**. Restart at 13:10:57 CET; There is no active prototype left. The records of the affected start were kept: there was subsequent service activity, a suspension and a DRM error at 13:08:10, without conclusive cause of the failure. The initial query without messages already corresponded to the new boot.

Upon subsequent request of the user a correction was prepared: Wayland destination with Ctrl+Shift+F12 and new identifier, test `--check-target` GlobalShortcuts only, mandatory pulse and release before RemoteDesktop/EIS, delayed emulation to the first event and closure of the state itself. The above shortcut matches Mutter's VT 12; this confirms a conflict, but does not determine the whole cause of freezing. Native compilation, **579/0** and memory signal regressions exceeded without bus or desktop. Then only GlobalShortcuts was attempted by user request, with `RuntimeMaxSec=60s`, `TimeoutStopSec=3s` and `timeout 60s` plus 3 s. The manager closed the test at 63 s, output 137 and no remaining prototype. The wait for the shortcut was not recorded; it is necessary to confirm what the user saw. The process limit is checked, the graphical recovery is not. The old case is still failed.

The new isolated shortcut and the RemoteDesktop/EIS demonstration have already passed with
independent recovery prepared and user present. R2/R9 also overcome: emergency during control and
withdrawal since GNOME. Pending: release of retained states and recovery before cuts in the
inter-team phase. The new EIS management has real local evidence of movement, letters, normal
closure and both outputs. The incident by changing the combination is not considered solved.
Details, commands and limits in `experiments/input/RESULTS.md`.

Additional preparation of EXP-INPUT-02: guide with on-screen steps and buttons, manual start, independent timer that closes only the test and reactivates the graphic session at 30 s. Supervisor tested with `sleep` (close and `activate_exit=0`); is not evidence of frozen image recovery. Three essential regressions of the supervisor overcome. Do not expand or repeat the general battery for this preparation. Upon return the user opened the guide. First attempt without ‘AHORA’ visible, exit 124; He took state and countdown up. Repeat `guided-20261105-140927-fa41253e7d58`: shortcut received, output 0, timer at 30 s and reactivation output 0. The user saw the messages and confirmed the desktop responding throughout the test. Only GlobalShortcuts, no EIS. does not demonstrate return from another VT or repair of a frozen image; The incident is still open.

Later demo requested by the user: `guided-20261105-141528-30d8de65e4b9`, `--inject-demo --arm-seconds 5`, independent 30 s timer. Checked shortcut before RemoteDesktop, EIS ready, short movement and `oneintwo`, output 0. The user confirmed correct text, movement and desktop responding. Timer executed at 14:15:58 CET, reactivation output 0; The prototype was already finished. No freeze was observed in this execution. It does not prove to recover a frozen image or states held; the incident is not closed because of this only case.

Previous attempt: `guided-20261105-192232-74937ef202aa`, R2 mode prepared with `--inject-demo --wait-for-stop`. Preparation rejected by several regions EIS/monitores, exit 1 before injecting; 30 s supervisor and session activation correct. The user decided to leave R2 and R9 pending instead of deactivating another monitor. Do not repeat or start R9 until requested. The user then resumed with an active monitor. R2: `guided-20261105-192937-e3dd65eea268`, emergency received during EIS active, output 0. R9: `guided-20261105-193129-20e181ab3953`, permission withdrawn from indicator GNOME, disconnect EIS and output 0. In both confirmed output received and desk responding; in R9 also indicator removed. Closings before the timer, which then acted with output 0 activation. There are no outstanding R2/R9 premises. No repair of a frozen composer or full cause of the historical incident is alleged. Do not add that test as a new condition of local closure. Next phase: Windows coordination.
