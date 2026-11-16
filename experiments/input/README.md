# Keyboard and Mouse Experiment

Updated status: **November 5, 2026**. There is Windows, X11 and Wayland implementation. The original
order is preserved below; the status of delivery and its evidence are detailed at the end and in
[RESULTS.md](RESULTS.md).

## Ordering for the agent

Implement this experiment by following [common instructions](../AGENTS.md) and [delivery
model](../README.md). Your exclusive directory is `D:/DEVEL/OneInTwo/experiments/input`. Actual
capture and injection are reserved for manual execution by the user.

## Question to Solve

Can Windows and Ubuntu act as the source and destination of input, change screen when crossing a
border and recover control reliably?

The first test will use one monitor per team, a selected physical origin and horizontal layout.
Simultaneous keyboards, remote video, multiple monitors and high applications are left out of the
experiment.

## Interface design

An executable with diagnostic modes, origin and destination, and small components:

1. Windows and Ubuntu/Wayland adapters for capture and injection.
2. Conversion to common events: movement, buttons, wheel and keys.
3. Control status: local, transition, remote and recovery.
4. Direct connection between instances and monitoring their availability.

The source records the configured edge and the destination displays the required geometry. The
prototype should allow you to return to the initial screen, not just send events in one direction.
Return detection and coordinates are part of what is being investigated.

Availability and permissions will be explicitly displayed. An environment that does not allow
capture will not be presented as compatible because it supports injection.

## Compulsory prototype recovery

- Real start by explicit command and a visible preparation period.
- Maximum duration of the trial session, configured and documented.
- Local emergency exit that works while the remote receives input.
- Capture release by failing the connection.
- Independent timer on the receiver to release keys and buttons if the emitter disappears.
- Rejection of backward events after reconnecting or changing control generation.
- Cleaning in cancellation, normal closure and withdrawal of permits.

Ctrl+C in one terminal is not enough as the only way to recover if the keys are directed to the
other computer. If a reliable local output cannot be implemented on a backend, do not enable your
actual capture mode and document the lock.

## What the agent will be able to check automatically

- State machine with clock and fake adapters.
- Conversion of events and coordinates of synthetic screens.
- Order presses and releases and discard obsolete events.
- Loss of connection and timer activation.
- Prevention of synthesized event loops.

Do not initialize actual hooks, portals or devices during those tests.

## Screenplay to be prepared for manual execution

1. Diagnose capabilities and review the emergency exit.
2. Test generation of synthetic events in a test window chosen by the user.
3. Start destination and origin explicitly.
4. Cross the border, move, press, move and return.
5. Try Ctrl, Alt, Shift, AltGr, accents and Spanish keyboard.
6. Disconnect when holding a key or button; check recovery at both ends.
7. Test for cancellation and revocation of permissions.
8. Repeat investing the roles of Windows and Ubuntu.

## Deliverable and evaluation criteria

Adapters and minimum flow code, compilation, self-tests, MANUAL.md and RESULTS.Md. Register by
system capture, injection, crossover, return, keyboard mapping and recovery.

The test will not be considered complete functionally by transmitting motion without return or state
release. The agent can deliver the code ready for evaluation, but the actual capabilities and
recovery will remain marked as pending until they are executed manually.

The expected architectural decision is how to implement the entry contract and what restrictions to
require each platform.

## Current implementation and evidence

- `src/core/`: protocol, HID conversions, geometry, states and controllers.
- `src/platform/wayland_flow.*`: portable logic of authorization, activation and tested closure with
  doubles of `PortalOps`.
- `src/platform/wayland_backend.cpp`: GDBus/libei native adapter for InputCapture source and
  RemoteDesktop/GlobalShortcuts destination.
- `src/platform/windows_backend.cpp`: hooks and SendInput.
- `src/platform/linux_backend.cpp`: X11 alternative; `linux_factory.cpp` selects the backend of the
  session without silent fallback.
- `src/net/tls_link.cpp`: direct communication with certificates and fingerprints at both ends.
- `tests/self_test.cpp`: isolated Wayland kernel and streams, without desktop or network.

The execution of the independent Windows binary on November 5 gave **538 checks, 0 failures**. The
historical figure of 403 belongs to the first installment prior to Wayland enlargement. Native
Wayland and TLS are compiled in Ubuntu; the updated kernel and stream self-test gives **579/0**,
plus native memory regressions. X11 has no new evidence. None of these isolated tests validates
desktop recovery.

Wayland preparation requires InputCapture v2 at source, RemoteDesktop v2 and GlobalShortcuts v1 at destination, resumed devices and a usable EIS region. The destination reserves Ctrl+Shift+F12 with new identity and requires you to press and drop it before opening RemoteDesktop/EIS. `--check-target` test only GlobalShortcuts; It doesn't open control. Emulation begins with the first real event and normal closure releases one's state. These corrections were compiled and tested in isolation. On November 5, the user first verified Ctrl+Shift+F12 separately and then demo RemoteDesktop/EIS with 30 s independent guide and timer: short movement, `oneintwo`, output 0 and desktop responding. Then they passed R2 (emergency with EIS active) and R9 (withdrawal of permission in GNOME), both output 0 and operating desktop confirmed. The planned local phase is completed in the configuration of a monitor. No further freeze was observed; a blocked composer is not guaranteed to be repaired. States withheld, cut-off and entry between teams remain outstanding (EXP-INPUT-02); Ubuntu source blocked by InputCapture v1.

The [manual](MANUAL.md) contains explicit compilation by backend and permission steps, crossing,
return, cancellation and recovery. The [report](RESULTS.md) distinguishes historical evidence,
current verification and manual validation. No isolated result states that the desktop or the
connection between computers is already working.

