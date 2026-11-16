# Results — Keyboard and Mouse

## Windows Reconstruction of Ubuntu Delivery — November 5, 2026

Verified sources: `2ae444df56689dbaebe9b796daca26a55bd02917`, received from
`origin/validation/ubuntu`. Check performed on the local branch `validation/windows`, without
modifying sources or repeating Ubuntu tests.

CMake from Visual Studio 18 2026, `Visual Studio 18 2026` generator, x64, Release and OpenSSL
configuration enabled. Reconstruction completed, output 0. Binary:
`build/windows-ready/Release/input_experiment.exe`.

Self-test Windows: **579 checks, 0 bugs**, output 0. `input_self_test.exe` was executed, which does
not initialize native adapters. The main executable also finished correctly with `--help`. The
clipboard was not accessed, portals were opened, capturaron/inyectaron devices were opened and no
test network was initiated.

Unknown local evidence: `build/windows-ready/configure-ubuntu-sync.log`,
`build-ubuntu-sync.log`,`self-test-ubuntu-sync.log` and `help-ubuntu-sync.log`. This evidence closes
the Windows reconstruction of this revision; it does not demonstrate the actual functioning of the
desktop or between computers. The local evidence Ubuntu and its limits are preserved below.

## R2 and R9 exceeded in Ubuntu — 5 November 2026

The user explicitly resumed both checks. Before opening them, `org.gnome.Mutter.DisplayConfig.GetCurrentState` confirmed one active logical display, HDMI-1, with the built-in connected display disabled. The checks used the same Git baseline `450cbb2e340a381bda01274eb86df0e05934e822` plus local changes and the same binary `47c3afafab7cd8d860565b51f930107cb01d914209f9dad8dbe855ebaecb8d97`. The C++ sources and binary stayed as built, and no tests were rerun. The guide clarified its R2/R9 titles, first and second presses, and that the shortcut does not revoke permissions. Python syntax passed. The previous manifest matched the C++ sources and binary. The guide text changes superseded its helper hash. The updated snapshot is `resume-build.sha256`.

From the root, the guide was first executed with `--emergency-check` and, after confirming the
operating desktop user, another run with `--revoke-check`:

```bash
env -u LD_LIBRARY_PATH GDK_BACKEND=wayland /usr/bin/python3 experiments/input/tools/shortcut_check.py --emergency-check
env -u LD_LIBRARY_PATH GDK_BACKEND=wayland /usr/bin/python3 experiments/input/tools/shortcut_check.py --revoke-check
```

Both verified Ctrl+Shift+F12 before RemoteDesktop/EIS, authorized keyboard and pointer without screen and
sent the short demo before waiting for output. No buttons or keys deliberately retained and no
network.

| Caso | Native Evidence and Guide | User Observation |
| --- | --- | --- |
| R2 — `guided-20261105-192937-e3dd65eea268` | 19:29:37.705 CET armed timer; EIS active, second click requested 19:30:00.071; ‘Local emergency stop control’, exit 0; guide confirmed closure to 19:30:04.875 | «Yes, output received and desktop responding» |
| R9 — `guided-20261105-193129-20e181ab3953` | Armed timer 19:31:29.269 CET; EIS active, required withdrawal 19:31:39.636; ‘Disconnected by EIS’ and ‘Control stopped by desktop or device closure’ output 0; guide confirmed closure 19:31:43.690 | ‘Yes: output received, indicator removed and desktop responding’ after pressing the GNOME indicator, without using the shortcut or Cancel |

The independent supervisors ran at 19:30:07.756–19:30:07.771 and 19:31:59.384–19:31:59.400 CET, respectively. Both recorded `activate_exit=0`, session 1, VT 2, and `Active=yes`. Both returned `kill_exit=1` because the prototype had already exited normally. The orderly shutdown occurred **before** the timer. Each case preserves `case.json`, `prototype.log`, `guide-events.log`, `process.json`, and the recovery JSON files. Launch logs are in `manual-output/local-final-20261105/{emergency-resume-ui,revoke-ui}.log`.

Result: **R2 and R9 locals exceeded** with EIS really active and human confirmation. With cases of
clipboards and files already closed, the local phase foreseen in this supported configuration is
completed. The previous incident is preserved and it is not claimed that the timer repairs a frozen
composer or that the whole cause of the blocking is proven. Cuts with retained states,
cruce/retorno, complete keyboard and both senses belong to the phase between pending equipment.
Origin Ubuntu remains blocked by InputCapture v1; Windows was not initiated nor product integration.

## R2/R9 prepared; pending by user decision — 5 November 2026

Only the necessary connection was prepared to check output with EIS active: `--inject-demo --wait-for-stop` retains the session after the demonstration and waits for an emergency `TargetSignal` or closure; records which received and closed the backend. Exhausted time or external interruption do not report success. Without that argument the previous demonstration retains its behavior. The guide reuses independent recovery of 30 s, adds `--emergency-check` and `--revoke-check` modes and displays the action in your window. There are no deliberately retained keys or buttons or connection to Windows.

Git base `450cbb2e340a381bda01274eb86df0e05934e822` plus local changes. `source .local-state/ubuntu-validation/env.sh` and `cmake --build experiments/input/build/ubuntu-wayland -j4`: output 0. New binary SHA-256 `47c3afafab7cd8d860565b51f930107cb01d914209f9dad8dbe855ebaecb8d97`. The same **3 essential regressions** of the supervisor continue to pass; no tests were added or the general battery repeated. The actual execution affected was to provide evidence of the new mode, which remains pending.

Comando R2:

```bash
env -u LD_LIBRARY_PATH GDK_BACKEND=wayland /usr/bin/python3 experiments/input/tools/shortcut_check.py --emergency-check
```

Case `manual-output/guided-20261105-192232-74937ef202aa/`: previous shortcut granted and tested, but preparation EIS rejected with “EIS exposes several regions; the experiment only admits one monitor”. Exit **1**, before demonstration and output waiting. It was not injected. Armed timer 19:22:32.526 CET; recovery 19:23:02.580–19:23:02.597, `activate_exit=0`, session 1/VT 2 active. The prototype was already finished; there was no living scope to kill.

It was asked to leave a single monitor and repeat only R2/R9. The user decided **"I prefer to leave
the two entrance tests pending"**. R2 was not repeated nor R9 was started. The own aids that
remained open were closed. The previous local demonstration remains successful; this rejection is
not presented as injection regression or as a recovery test.

To resume, present user and an active monitor. R2: follow NOW to check the shortcut and, after the text, EXIT NOW to press it again. R9: same script with `--revoke-check`; after the text press the top indicator of GNOME Stop Screen Sharing. That action was contrasted with the `remoteAccess.js` installed (`gresource extract /usr/lib/gnome-shell/libshell-18.so /org/gnome/shell/ui/status/remoteAccess.js`): the click runs `handle.stop()`. It was not replaced by killing the prototype or closing its own session by API. R9 has no result of execution.

Commands, guide log and manifest SHA-256 in `manual-output/local-final-20261105/`; the case retains
`case.json`, `prototype.log`, `guide-events.log`,`process.json` and recoveries.

## Local Wayland injection verified — 5 November 2026

The user requested and performed the supervised local demonstration. He confirmed **exact text
`oneintwo`, pointer and desktop movement responding**. The disposable guide box was used, without
synthesized clicks, capture, network, or Windows connection. This execution opened RemoteDesktop/EIS
after physically checking Ctrl+Shift+F12; the above freeze was not observed.

Same Git base `450cbb2e340a381bda01274eb86df0e05934e822` plus local changes; same binary SHA-256 `b0d8e9ebedde3d9cd98cfd11e3843ac009beab9c30bb3525d8eae06168b398d8`. Before booting the six fonts and executable were tested with `sha256sum -c experiments/input/manual-output/ubuntu-local-20261105/recovery-hardening.sha256`: all correct. C++ was not modified or reconstructed for this demonstration.

The 30 s independent monitor and guide were reused with an explicit `--inject-demo` mode,
authorization instructions, disposable box and warning to focus it during the 5 s countdown. The
default mode continues to test only the shortcut. The save requires active timer and current timer
also for injection. This check was extended within the **same 3 essential regressions** of the
helper; command `/usr/bin/python3 experiments/input/tests/test_shortcut_check.py`: **3 tests, OK**,
without GTK or real commands. `git diff --check` correct; the general battery was not repeated.

Command from the root:

```bash
env -u LD_LIBRARY_PATH GDK_BACKEND=wayland /usr/bin/python3 experiments/input/tools/shortcut_check.py --inject-demo
```

The guide launched within its own scope the binary with `--inject-demo --arm-seconds 5 --max-seconds
30`, also limited by `timeout` to the remaining timer. Evidence in
`manual-output/guided-20261105-141528-30d8de65e4b9/`:

- Own session 1, UID 1000, VT 2; independent preflight output 0.
- The timer was armed at **14:15:28.737 CET**. The guide displayed NOW at **14:15:28.958** and TEST AREA at **14:15:44.432**.
- `prototype.log`: shortcut Ctrl+Shift+F12 granted, precheck, countdown 5...1 and "Develop
  completed. Check text and cursor". `process.json`: **exit 0**; guide recorded final at
  **14:15:50.888**.
- Independent recovery executed **14:15:58.796–14:15:58.812**: `activate_exit=0`, `VTNr=2`,
  `Active=yes`. `kill_exit=1` because the scope had already finished normally. The guide confirmed
  closure to the 14:15:58.849.
- Human observation: **"Yes: correct text, movement and desk responding."** The agent did not
  visually inspect the desktop.

Result: preparation RemoteDesktop/EIS, short absolute movement, press and release of the letters of `oneintwo` and normal closure validated locally in this execution. The timer acted when the prototype had already finished and the session was still active; it does not show recovery of a frozen image, of another VT, nor with teclas/botones retained. No clicks, wheel, modifiers, withdrawal of permissions, cruce/retorno nor entry between equipment. EXP-INPUT-02 retains these limits; origin Ubuntu remains blocked by InputCapture v1 compared to the

## Insulated cut verified with 30 s guide and timer

November 5, 2026, Wayland Session 1, VT 2. Git Base `450cbb2e340a381bda01274eb86df0e05934e822` plus
local changes, `b0d8e9ebedde3d9cd98cfd11e3843ac009beab9c30bb3525d8eae06168b398d8` binary.
`--check-target` was repeated only: GlobalShortcuts, no RemoteDesktop, EIS, capture, injection or
Windows connection.

Command from the root, Python system without local libraries:

```bash
env -u LD_LIBRARY_PATH GDK_BACKEND=wayland /usr/bin/python3 experiments/input/tools/shortcut_check.py
```

- First guided essay: `manual-output/guided-20261105-140452-8d87e9ff735e/`. The portal granted Ctrl+Shift+F12 and the prototype announced the wait, but the user did not see "NOW" nor remember the countdown. Ended by timeout, exit 124, no shortcut check. The timer executed the reactivation at 14:05:22.254–14:05:22.270 CET, output 0, active session. It is preserved in `recovery-51e42fa99d55.json`; the subsequent closure of the guide generated another recovery with `reason=cancel`. Visual success is not attributed to the first trial.
- Correction limited to the guide: status and countdown, large "NOW", notice to bring your window
  in front and phase change log. Checking correct syntax and `git diff --check`. No tests were added
  or the general battery was repeated for this presentation setting.
- Requested repeat: `manual-output/guided-20261105-140927-fa41253e7d58/`. The timer was armed at **14:09:27.707 CET**. `guide-events.log` records “NOW” at **14:09:28.005**, “Shortcut received” at **14:09:32.662**, and shutdown at **14:09:57.904**. `prototype.log` confirms “Local shortcut verified; closing sessions”, and `process.json` shows exit **0**. Independent recovery ran at 14:09:57.777–14:09:57.792 with `activate_exit=0`, `VTNr=2`, and `Active=yes`. `kill_exit=1` indicates that the prototype scope had already ended normally. The user confirmed **they saw the messages and countdown, and the display, mouse, and keyboard responded throughout the test**.

Result: local shortcut Ctrl+Shift+F12 isolated and timer execution at the 30 s verified. The session
was already active: no return has been tested from another VT or repair of a frozen image. Recovery
with RemoteDesktop/EIS and injection are still pending; EXP-INPUT-02 remains open. No other test is
started for this result.

## 30 s recovery preparation and guidance

November 5, 2026. The user nuanced the previous test: he believes the image was frozen, although the
system continued to operate below. This observation does not confirm a kernel freeze nor does it
allow to determine whether the shortcut was activated during the configuration or during the waiting
of the prototype. The 60 s test continues without checking the shortcut or graphical recovery.

He asked for another test with automatic return to the main session at 30 s and clear steps inside a window. **Only one pilot guide** was added in `tools/shortcut_check.py`, without integrating the main application. Do not open portals when booting: the user must press "Start 30 seconds test". Indicates the order: Start → check the Ctrl+Shift+F12 → "Add" / "Add" top right at GNOME → wait for the message "NOW" in the guide → press and drop the shortcut once → wait the return. "Cancel and return to the desktop" closes the essay and tries to reactivate the session. If GNOME requests pantalla/control remote or shows the old shortcut, the instruction is to cancel. The name of the button was contrasted with the [official definition of the GNOME 50.0](https://github.com/GNOME/gnome-control-center/blob/50.0/global-shortcuts-provider/cc-global-shortcut-dialog.blp) dialog.

The guide first prepares a verification service and a temporary user timer **outside the scope of the prototype**. At 30 s the service closes only the random range of this trial with SIGKILL and calls `loginctl --no-ask-password activate SESION`. Check owner, VT and status; does not end or unlock sessions, does not restart GNOME and does not use sudo. The child refuses to open the portal without active timer or if the deadline has already expired. `RuntimeMaxSec=30s` and `timeout` add limits to the prototype. The guide shows countdown and the current step reading only its synthetic log. System Python is used with PyGObject/GTK3 already available; No installation.

### Evidence of preparation

- Self session identified: ID 1, UID 1000, Wayland, VT 2, active. `loginctl --no-ask-password
  activate 1` returned 0. The same call from an independent transit service also returned 0, without
  authentication. Local login policy supports VT change for activa/inactiva. Session It was not
  changed to another console to check it.
- Supervisor check **without the prototype or portal**: the timer was armed and `sleep 90` was started inside the trial scope. Recovery ran at **13:52:42.998 CET** and finished at **13:52:43.012 CET**. The recorded values were `kill_exit=0`, the harmless process terminated by SIGKILL (`-9`), `activate_exit=0`, `VTNr=2`, and `Active=yes`. Logs in `manual-output/guided-20261105-135212-67659af36348/` include `case.json`, `preflight.log`, `timer.log`, `sleep.log`, `recovery.json`, and `preflight-result.json`. `kind=supervisor-preflight-sleep` identifies this as a preflight. It verifies independent timer execution, termination, and reactivation of the already-active session. Return from another VT and compositor or display freeze recovery remain unverified.
- Upon request to limit maintenance, only **3 new essential tests** of the supervisor are retained:
  shutdown and activation even if the sending of signal fails, rejection of start without active timer or
  with expired deadline, and abortion if the timer cannot be set up. Command: `/usr/bin/python3
  experiments/input/tests/test_shortcut_check.py`; **3 tests, OK, output 0**, without real commands,
  GTK or bus. The general battery 579/0 was not extended or repeated, whose previous evidence
  remains valid for C++ without any change in this preparation.

During the AFK pause, the guide was not opened or the interactive essay was initiated; the timer and
process query did not find any pending tests. After the user's explicit return, the two cases
described above were performed. Visual observation is for the user, not for the agent's inspection.

## Corrected test with an external limit of 60 s — 5 November 2026

**Fixed process limit; shortcut not yet verified.** The user asked to repeat with 60 s timeout. It
started only `--check-target` of the corrected version, without RemoteDesktop, EIS, capture or
network. The test reached the limit and required forced closure; there is no record of the
description granted, the announcement of wait or the activation of the shortcut. There is no user
observation about the dialog and the desktop response; it is not classified as success or another
freeze without that confirmation.

Same Git base and `b0d8e9ebedde3d9cd98cfd11e3843ac009beab9c30bb3525d8eae06168b398d8` binary; SHA-256
font and executable were tested before booting. Application identity still ready and without another
active prototype. `loginctl` confirmed `VTNr=2`, `Type=wayland`,`Active=yes` before trial.

Command, from the root:

```bash
env -u LD_LIBRARY_PATH systemd-run --user --scope --collect \
  --unit=app-io.github.OneForTwo.InputExperiment-shortcut60a \
  --property=RuntimeMaxSec=60s --property=TimeoutStopSec=3s \
  /bin/bash -c 'source .local-state/ubuntu-validation/env.sh; exec timeout --signal=TERM --kill-after=3s 60s ./experiments/input/build/ubuntu-wayland/input_experiment --check-target --max-seconds 60'
```

Two saves: the user manager limits the life of the scope to 60 s and waits 3 s before forcing its
closure; `timeout` also applies 60 s and 3 s to the process. All 60 s include start and permission,
they do not start after the dialogue. During execution, `systemctl --user show` confirmed
`RuntimeMaxUSec=1min`, `TimeoutStopUSec=3s`,`ActiveState=active`.

Hora local CET (`Europe/Madrid`):

- Inicio **13:35:38**.
- Journal of the field: **13:36:38**, `Scope reached runtime time limit. Stopping.`
- **13:36:41**, `Failed with result 'timeout'`; command exits with **137**.
- Later scan of `/proc/*/exe`: `input_experiment_running_pids=[]`.

137 corresponds to this observed forced termination; it is not recorded as a spontaneous crash of
the prototype. The user was advised not to press the shortcut once the execution was completed. A
second test was not released without collecting his observation. It cannot be concluded that the
timer recovers a blocked composer: it is only proven that the manager closed the process and did not
leave that prototype active.

Logs ignored in `manual-output/ubuntu-local-20261105/`: `shortcut-only-60s-01.log`, `shortcut-only-60s-01-guard.log`, `shortcut-only-60s-01-supervisor.log`, `shortcut-only-60s-01-cleanup.log`. The journal was consulted only for the scope of this test in the range 13:35:30–13:36:50. Updated manual with both 60 s guards; no additional changes of code, dependencies or permanent configuration.

## Preparation corrected after the incident — 5 November 2026

**Prepared code, native compilation and out-of-date tests; no new graphical test.** The user asked
to prepare the experiment to avoid another block. The shortcut conflict and preparation order were
corrected; **It is not claimed that the complete cause of freezing is proven or that the graphical
recovery is already working**. EXP-INPUT-02 is still open for such validation. GlobalShortcuts,
RemoteDesktop, EIS, capture or network were not opened when preparing these corrections.

Base review: `450cbb2e340a381bda01274eb86df0e05934e822`, `validation/ubuntu` branch, plus previous
local changes and those described here. No commit or publication, no new dependencies or global
installation.

### Changes and evidence that justifies them

1. **Target Shortcut Wayland: Ctrl+Shift+F12**, identifier `oneintwo-emergency-v2`. It does not reuse Ctrl+Alt+F12's persistent concession identifier. The [Mutter values 50.1](https://raw.githubusercontent.com/GNOME/mutter/50.1/data/org.gnome.mutter.wayland.gschema.xml.in) assign Ctrl+Alt+F12 to change to VT 12 and its [implementation](https://raw.githubusercontent.com/GNOME/mutter/50.1/src/core/keybindings.c) records it as a non-mascable combination. Check the default value **installed**, without touching configuration: `GSETTINGS_BACKEND=memory gsettings get org.gnome.mutter.wayland.keybindings switch-to-session-12` → `['<Primary><Alt>F12']`, log `default-vt-shortcut.log`. The previous normal query returned the same with dconf notices by the sandbox; the backend in memory was used to check the value of the schema without writing cache. This confirms the conflict of the previous design; does not prove that the whole incident was just a change of VT.
2. **`--check-target` in Wayland only prepares GlobalShortcuts.** Do not call RemoteDesktop or
   connect EIS, do not start emulation and do not need geometry. The previous failed test remains
   registered as failure; changing the shortcut does not turn that trial into success. Windows/X11
   maintain the previous procedure and Ctrl+Alt+F12; they have not been run there again.
3. **Physical verification before control.**`--target` and `--inject-demo` reserve the new shortcut
   and expect activation **and release** from their own session, with a limit of 30 s. Only after
   requesting RemoteDesktop and EIS. Rejection, timing or closure of the shortcut abort without
   opening control. The click used to verify is consumed and is not delivered as an emergency of the
   control that has not yet begun. A mark between executions is not saved.
4. **Delayed emulation and closure.** Before `EI_EVENT_DEVICE_ADDED` began emulation immediately with sequence 0, also in the check described as "no injection". Restarted devices are now expected and `start_emulating` is only called when sending the first real event, with increasing sequence. Button and wheel capabilities are requested and tested; specific references to the devices are maintained. Pause, withdrawal or disconnection make you reject new events and stop the flow, without automatically resuming it. When closing, releases of the still usable state are sent, your transactions are completed and references and descriptors are released. A second `close(fd)` is deleted in the issuer's setup failure because [libei takes possession of the descriptor](https://libinput.pages.freedesktop.org/libei/api/group__libei-sender.html).
5. **The demonstration for the first error** of movement, pulsation or release, clears the
   destination and returns 1; a local output is reported as interruption and not as demonstration
   completed correctly.

### Checks carried out without desktop

From the root, using the persistent dependencies of the project:

```bash
source .local-state/ubuntu-validation/env.sh
cmake --build experiments/input/build/ubuntu-wayland -j4
env -u DISPLAY -u WAYLAND_DISPLAY -u XDG_RUNTIME_DIR -u DBUS_SESSION_BUS_ADDRESS \
  ./experiments/input/build/ubuntu-wayland/input_experiment --self-test
env -u DISPLAY -u WAYLAND_DISPLAY -u XDG_RUNTIME_DIR -u DBUS_SESSION_BUS_ADDRESS \
  ./experiments/input/build/ubuntu-wayland/input_self_test
env -u DISPLAY -u WAYLAND_DISPLAY -u XDG_RUNTIME_DIR -u DBUS_SESSION_BUS_ADDRESS \
  ./experiments/input/build/ubuntu-wayland/input_experiment --help
```

All commands exited with **0**. Rebuilt native Wayland + TLS executable:

```text
PASS native_portal_response_and_shortcut (isolated, no bus)
Checks: 579, failures: 0
```

The 579 core checks and flows include 41 additional ones: shortcut-only preparation, prohibition of
opening control without verification, shortcut rejection, pre-closure and failures in each
authorization phase. The native binary also checks Response and callbacks Activated/Deactivated in
memory: ignore someone else's session and old identifier, require activation prior to release,
consume the check without issuing an emergency and detect shortcut closure. These native groups are
additional to the 579 counter. The help shows the shortcut of each backend and the scope of
`--check-target`.

Logs ignored in `manual-output/ubuntu-local-20261105/`: `build-recovery-hardening.log`, `self-test-recovery-hardening.log`, `portable-self-test-recovery-hardening.log`, `help-recovery-hardening.log`, `default-vt-shortcut.log`, `recovery-hardening.sha256`. The last contains SHA-256 of the six affected source files and binary; the executable is `b0d8e9ebedde3d9cd98cfd11e3843ac009beab9c30bb3525d8eae06168b398d8`. `git diff --check` does not report errors.

**Limit of this delivery:** there is no simulated EIS transmitter or real server in these tests.
They compile their new device management, but they do not prove that the composer accepts their
events or retrieves the desktop. The new shortcut reservation has not been graphically tested
either. The next possible trial is only GlobalShortcuts, supervised and with independent recovery;
do not open control or injection or connect Windows yet. The Ubuntu source remains blocked by
InputCapture v1 in front of the v2 requirement; no capture or modification of Mutter, console,
controller

## Local validation initiated — 5 November 2026

**Entry blocked by recovery failure reported by the user.** When pressing Ctrl+Alt+F12 during the
last wait, the user observed that the entire PC was frozen and confirmed that it had to restart to
recover it. The process did not receive the emergency and ended for time. Do not repeat the shortcut
or start injection until it clarified what happened and prepare a proven independent recovery.
Verified permissions cancellation; isolated tests do not certify the recovery of the desktop. No
capture or network was executed and no movements or test keys were sent.

Git Base: `450cbb2e340a381bda01274eb86df0e05934e822`, `validation/ubuntu` branch, plus the local
corrections described below and in the other experiments. No commit or publication. Environment and
libraries: those of the initial compilation that follows, with persistent dependencies of the
project.

### Real diagnosis and catch blocking

Command from the root, after loading `.local-state/ubuntu-validation/env.sh`:

```bash
./experiments/input/build/ubuntu-wayland/input_experiment --diagnose
```

Output 0; `manual-output/ubuntu-local-20261105/diagnose-01.log`:

```text
Plataforma: Linux Wayland (InputCapture + RemoteDesktop + libei)
Capture reported: no
Injection reported: yes
InputCapture v1 with keyboard and pointer; RemoteDesktop v2; GlobalShortcuts v1
```

The backend requires **InputCapture v2**, while this desktop displays v1. Ubuntu as its source is
blocked with this backend; the requirement was not reduced, capture was not executed or X11 was used
as a substitute. It is not an assertion of InputCapture's impossibility v1: to support it would
require another flow and validation. The declared injection still depends on permissions, shortcut
and EIS devices. The historical diagnostic label "Destiny Local Activity: If" refers to
`targetLocalOverride` by GlobalShortcuts

Recovery prepared: `loginctl show-user UID -p Display --value` and `loginctl show-session SESION -p VTNr -p Type -p Active` returned `VTNr=2`, `Type=wayland`, `Active=yes`. On this Ctrl+Alt+F2 machine is the expected return to the desktop if Ctrl+AlT+F12 changes console. The shortcut was not tested or changed during this session.

### Actual preparation failed and correction

`--check-target --max-seconds 30` was added to prepare permissions and check Ctrl+Alt+F12 **without
calling any injection function or opening network**. It is necessary to check the output before a
real demonstration. `--inject-demo` was also corrected to start the target guard and attend stop
signals during the countdown and demonstration; before only query the console interruption. These
desktop paths have not yet been successfully verified.

The first execution of `--check-target` was prepared for the user to cancel the permission, but
ended earlier with:

```text
Could not prepare the target: CreateSession: the portal did not respond in time
process_exit=1
```

The log also retains invalid connection GLib notices during cleaning. The user was temporarily
absent and could not confirm any dialogue: **is not registered as denied permission or proven
shortcut**.

Concrete cause found in `PortalClient::request`: `onResponse` expected a `RequestWait*`, but the generic subscription gave him `PortalClient*`. The response did not update the wait and could corrupt the connecting object. In addition, consumers expect a tuple `(a{sv})`, while the function directly returned `a{sv}`. Correction:

- Subscribe `Response` with the correct wait state, also if you change the path returned by the
  portal.
- Maintain the tuple contract by delivering results and correctly release the dictionary reference.
- Add a synthetic response regression in memory to the application `--self-test` compiled with
  Wayland, without creating backend, bus or permissions.

Final commands, output 0:

```bash
source .local-state/ubuntu-validation/env.sh
cmake --build experiments/input/build/ubuntu-wayland -j4
./experiments/input/build/ubuntu-wayland/input_experiment --self-test
```

```text
PASS native_portal_response (isolated, no bus)
Checks: 538, failures: 0
```

The native response check is additional to the 538 core and streams; `input_self_test` retains its
portable reach. It does not yet show that the actual portal authorizes or cancels correctly. SHA-256
of the corrected adapter: `891ddccf87c83ea30c66bdbce23058d5b0145bdba237808f1412b84dfe068b93`.

Logs ignored in `manual-output/ubuntu-local-20261105/`: `diagnose-01.log`, `target-denied-01.log` (name identifies the expected case, not its result), `build-local-exit.log`, `self-test-local-exit.log`, `build-response-fix.log` and `self-test-response-fix.log`. The diagnosis had an external limit of 30 s and preparation of 240 s, with `timeout --signal=TERM --kill-after=3s`.

### Resumption: actual cancellation and type of session

`--check-target --max-seconds 30` was repeated with the previous correction. The
`target-denied-02.log` execution ended before the dialog, exit 1: `RemoteDesktop.CreateSession no
returned a session_handle`. It is not a denial.

[Official RemoteDesktop specification](https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/main/data/org.freedesktop.portal.RemoteDesktop.xml) retains `session_handle` as `s` for compatibility, although it contains an object path. The adapter only accepted `o`. The RemoteDesktop reading was corrected exclusively to `s`, validating the route with `g_variant_is_object_path`; InputCapture v2 retains its own contract. Synthetic native regression now includes that field and uses the same reader as actual preparation. Compilation and `input_experiment --self-test`: output 0, `PASS native_portal_response`, **538/0**; logs `build-session-handle.log` and `self-test-session-handle.log`. SHA-256 of `wayland_backend.cpp` after this correction: `454d7f94c804c327d75f15a5c3fb52d68a3717389086c7fa130b7f505a8cb52c`.

New execution, same variables and external save of 240 s:

```bash
./experiments/input/build/ubuntu-wayland/input_experiment --check-target --max-seconds 30
```

The user confirmed to press Cancel. `target-denied-03.log` retains `No se pudo preparar el destino:
the user cancelled authorization`, `process_exit=1`, without invalid connection GLib warnings from
the first attempt. ** Verified cancellation**, without going into injection or network.

### Identity for GlobalShortcuts

`--check-target --max-seconds 60` with an external limit of 240 s ended with `CreateSession:
GDBus.Error:org.freedesktop.portal.Error.NotAllowed: An app id is required`
(`target-shortcut-01.log`, output 1). The
[1.21.1](https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/1.21.1/src/global-shortcuts.c)
portal rejects unidentified shortcuts; the [host application
recognition](https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/1.21.1/src/xdp-app-info-host.c)
requires application scope and input `.desktop` available.

`io.github.OneForTwo.InputExperiment.desktop` was prepared within the local directory ignored of
logs and a link of the same name in `/home/pholtergate/.local/share/applications/`, only for the
user, without sweat, autostart or global installation. Identity is provided through a transient
`systemd-run --user --scope --collect`, without changing services. The field test with `cat
/proc/self/cgroup` returned 0.

The first release with the loaded build environment produced an error of its own `systemd-run` (`calendar_spec_weekday_conflicts`, output 127, `target-shortcut-02.log`): `LD_LIBRARY_PATH` local included a libsystemd incompatible with the installed tool. The command was corrected: start `systemd-run` with `env -u LD_LIBRARY_PATH` and load `env.sh` only within your child process. No system libraries were modified.

```bash
env -u LD_LIBRARY_PATH systemd-run --user --scope --collect \
  --unit=app-io.github.OneForTwo.InputExperiment-shortcut03 \
  /bin/bash -c 'source .local-state/ubuntu-validation/env.sh; exec timeout --signal=TERM --kill-after=3s 240s ./experiments/input/build/ubuntu-wayland/input_experiment --check-target --max-seconds 60'
```

Recovery was again consulted: VT 2, Wayland, active session (`recovery-02.log`). This check is not
yet equivalent to testing the shortcut.

### Preparation with permissions and limit of a monitor

The user confirmed to authorize the `target-shortcut-03.log` dialogues. The application identity allowed progress to EIS, but the preparation was rejected: `el EIS expone varias regiones; el experimento solo admite un monitor`, exit 1. The shortcut was not pressed or input injected. Read-only consultation at `org.gnome.Mutter.DisplayConfig.GetCurrentState`: **2 logic monitors and 2 physicists**, 1,5 and 1,0 scales; log `monitor-layout.log`, no hardware identifiers. The user temporarily disabled one; new query: **1 logical monitor**, `monitor-single.log`. The experiment was not extended to several monitors or altered the configuration by code.

When reviewing the emergency reception, an incorrect subscription was detected: `Activated` GlobalShortcuts is issued from the main object of the portal, not from the Session route. Reference: [official implementation v1](https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/1.20.3/src/global-shortcuts.c). The subscription path was corrected to `/org/freedesktop/portal/desktop` and the first argument is filtered by the session itself before accepting the shortcut identifier. Compilation and isolated test: output 0, `PASS native_portal_response` plus **538/0**; logs `build-shortcut-signal.log` and `self-test-shortcut-signal.log`. This synthetic regression of response does not check subscription; Your evidence must be the next real pulsation. SHA-256 adapter: `ca17d1358e3639bdc04aaf9663c5a0475e0810b41a0f32e9ff84c5416852560c`.

### Emergency wait: not yet verified

`target-shortcut-04.log`, a single monitor, reached `Check without injection or networking. Press
Ctrl+Alt+F12 ahora.`, but EIS was disconnected immediately afterwards. The program detected the
interruption and went out 1, without injecting. The user indicated that a second dialogue did not
appear; this does not show a lack of shortcut authorization, as BindShortcuts returned its
identifier and the preparation had advanced to EIS. The cause of this interruption was not
determined.

A diagnosis of activated libei was added only with `ONEINTWO_EI_DEBUG=1` and `--check-target
--max-seconds 60` was repeated in the user area with 240 s external guard. `target-eis-debug-05.log`
displays keyboard, pointer and absolute region, complete preparation and connection maintained until
60 s exhaust. No emergency signal was received; output 1. Closing at the end of that log is
initiated by the client during cleaning, does not confirm the previous disconnection. No movements,
buttons or keys were sent. No race cause is presumed without further evidence.

To distinguish a returned identifier from a usable combination, the `trigger_description` of the
output granted by the portal was added to the log. Compilation in `build-shortcut-description.log`.
No demonstration with injection is enabled until the physical emergency test is passed.

### Communicated incident: freezing when pressing the shortcut

After the last wait, the user confirmed that ** did press Ctrl+Alt+F12** and that "the entire PC was
frozen when I did." It is recorded as user observation and **recovery failure**, not as a simple
pending test or as a shortcut success. Then he confirmed that ** had to restart to recover the
computer**. There is not enough evidence to attribute it to the composer, EIS, the shortcut reserve
or a virtual console change; the code is not modified for an assumed cause nor is the operation
repeated to reproduce the lock.

The associated execution is `target-eis-debug-05.log`: it reached the wait, recorded changes of
modifiers, did not receive `Emergency` and ended after 60 s with output 1. The closing EIS of the
end was initiated by the client. No movements, buttons or keys were sent; yes RemoteDesktop was
opened, EIS was connected and its emulation transaction was initiated during preparation. Therefore
"no event injection" does not mean that there was no actual authorized control session.

Subsequent checks, without opening new dialogues:

- `/proc/*/exe` tour, comparing with the exact binary path: `input_experiment_running_pids=[]`; that
  prototype was not left active.
- `journalctl -b --since '2026-11-05 13:03:30' --until '2026-11-05 13:05:00' --no-pager -o short-iso
  _UID=1000
  --grep='[Ee][Ii][Ss]|libei|emulat|[Mm]utter|gnome-shell|[Ss]egfault|[Cc]rash|[Ff]ailed|[Ff]reeze|[Tt]imeout'`:
  `-- No entries --`, preserved in `freeze-session-journal.log`. The absence of messages on that
  filter does not rule out a freeze or prove its cause; it is not an inspection of the core or all
  boots.

The version executed was `build-eis-diagnostic.log`; the description of the combination was added
later and **has not been executed on the desktop**. No demo, capture or input tests are enabled with
Windows.

### Recovery by restarting and recording of affected boot

The user confirmed that it was necessary to restart. Read-only queries: `date --iso-8601=seconds`, `uptime -s`, `journalctl --list-boots --no-pager`. Restart: **2026-11-05 13:10:57 CET (Europe/Madrid)**, `6e1a6c6cf11a4266ad8256229db2dbe5`; affected start: `d67316910f8348f19064305dcc789bb3`, whose journal ends at 13:10:48. The exact comparison of `/proc/*/exe` returned `input_experiment_running_pids=[]`. The prototype was not executed again.

The initial query with `journalctl -b` was already done on the new boot; its absence of messages
does not inform of the previous incident. The selection was corrected using the affected boot ID.
13:03:30–13:11:00 interval, ISO output with time zone, without new dialogues or input operations:

```bash
journalctl -b d67316910f8348f19064305dcc789bb3 \
  --since '2026-11-05 13:03:30' --until '2026-11-05 13:11:00' \
  --no-pager -o short-iso _COMM=gnome-shell
journalctl -b d67316910f8348f19064305dcc789bb3 \
  --since '2026-11-05 13:03:30' --until '2026-11-05 13:11:00' \
  --no-pager -o short-iso -u systemd-logind.service
journalctl -b d67316910f8348f19064305dcc789bb3 \
  --since '2026-11-05 13:03:30' --until '2026-11-05 13:11:00' \
  -k -p warning --no-pager -o short-iso
```

Local logs ignored: `freeze-previous-session.log`,
`freeze-previous-logind.log`,`freeze-previous-kernel.log`. The kernel retains the relevant warning
and a count of the 85 repeated lines of `kauditd_printk_skb: callbacks suppressed`, omitted for not
providing details of the bug. Messages from other applications were discarded.

Observations without a causal finding:

- The kernel continued to record messages after pulsation. A 13:08:04 warning appears at
  `i2c_hid_get_input: IRQ triggered but there's no data`. There is no panic, oops or GPU lock
  explicit in the consulted warnings.
- Logind records the lid closing and suspension starting at 13:08:10, the lid opening at 13:08:18, and suspension ending at 13:08:19. At 13:08:10, GNOME Shell logged `drmModeAtomicCommit: Permission denied` while disabling `/dev/dri/card1`. These entries follow the prototype waiting period and do not establish the cause of the reported freeze.
- At 13:10:18 the power button is recorded; at 13:10:41 GNOME Shell and session is closed. They are
  tests of service activity during the incident and subsequent shutdown, not that the interface was
  usable.

**Result: Recovery only by user reboot; cause not determined.** EXP-INPUT-02 is kept open. No
speculative patch has been applied, the shortcut changed, graphic services restarted or new desktop
tests performed. Windows historical evidence is preserved.

### Resumption point

The cancellation is already verified and the user recovered the computer by rebooting. The relevant records of the affected boot were kept; they do not allow to determine the cause. Continue only with evidence of read alone and isolated tests until justifying a specific correction. Do not repeat Ctrl+Alt+F120, `--check-target` or injection demonstrations until solving EXP-INPUT-02 and have independent recovery checked. If the test session is terminated, the user can restore the layout of two monitors; the agent does not modify it. Do not connect Windows until agreed revision, roles, ports, fingerprints and case.

## Initial compilation of native Ubuntu — November 5, 2026

**Current result: native Wayland + TLS compiled; independent self-test with 538 checks, 0 failures,
output 0.** The following Windows evidence is preserved. This execution updates the historical
absence of native Wayland compilation; it does not certify capture, injection or recovery.

### Environment and review

- Git base: `450cbb2e340a381bda01274eb86df0e05934e822`, `validation/ubuntu` branch,
  `/home/pholtergate/DEV/OneForTwo` directory. Unmodified input sources. The only local code patch
  in the set affects the TLS header of the clipboard experiment.
- Fecha: 2026-11-05 (`Europe/Madrid`). Ubuntu 26.04.1 LTS, Linux 7.0.0-42-generic x86_64; entorno
  heredado Wayland, `ubuntu:GNOME`, GNOME Shell `50.1-0ubuntu1.5`, Mutter `50.1-0ubuntu2.4`.
- GCC/G++ `15.2.0-16ubuntu1`, CMake local 4.2.3, Make 4.4.1, pkg-config 2.5.1; GIO/GLib 2.88.0
  (`2.88.0-1ubuntu0.1`), libei 1.5.0 (`1.5.0-3`), OpenSSL 3.5.5 (`3.5.5-1ubuntu3.7`).
- Portal Packages: xdg-desktop-portal `1.21.1+ds-1ubuntu3.1`, GNOME `50.0-0ubuntu1`,GTK
  `1.15.3-2ubuntu1`. No bus was consulted: InputCapture v2, RemoteDesktop v2, GlobalShortcuts v1 and
  EIS devices are still not checked running.

Inventory: `git status --short --branch`, `git rev-parse HEAD`, `cat /etc/os-release`, `uname -srmo`, session variables, `c++ --version`, `dpkg-query -W`, and `pkg-config --modversion gio-2.0 libei-1.0 openssl`. Dependencies moved from `/tmp` to the Git-ignored `.local-state/ubuntu-validation/` inside the project, without a global installation or `sudo`. See [setup and variables](../clipboard/RESULTS.md#local-dependencies-for-this-validation).

### Compilation and isolated testing

From the root of the repository:

```bash
source .local-state/ubuntu-validation/env.sh
cmake --fresh -S experiments/input -B experiments/input/build/ubuntu-wayland \
  -DCMAKE_BUILD_TYPE=Release -DONEINTWO_WAYLAND=ON \
  -DONEINTWO_X11=OFF -DONEINTWO_WITH_TLS=ON \
  -DOPENSSL_ROOT_DIR="$OIT_VALIDATION_DEPS/usr"
cmake --build experiments/input/build/ubuntu-wayland -j4
cd experiments/input
./build/ubuntu-wayland/input_self_test
```

The final three commands returned 0. `input_experiment` and `input_self_test` were built, including the compilation of `wayland_backend.cpp`, `linux_factory.cpp` and `tls_link.cpp`; definitions `OIT_WITH_WAYLAND=1` and `ONEINTWO_WITH_TLS=1`, without `OIT_WITH_X11`. The native executable links GIO/GObject/GLib, `libei.so.1`, `libssl.so.3` and `libcrypto.so.3`; `ldd` does not report missing libraries. No errors or observed warnings. No `ONEINTWO_BACKEND=none` was used. After the transfer the cache was re-registered with `--fresh`, both executables were reconstructed with persistent routes and the self-test was repeated: **538/0**.

```text
OneInTwo input experiment: self-tests aislados
Checks: 538, failures: 0
```

The independent test exercises core and Wayland flows with doubles; not instance the backend GDBus/libei. Compile that backend and execute the doubles are different evidences. Local logs ignored in `build/ubuntu-wayland/`: `configure.log`, `build.log`, `self-test.log`, `libraries.log`. The execution after the transfer is `configure-persistent.log`, `build-persistent.log`, `self-test-persistent.log` and `libraries-persistent.log`.

### Desktop, coordination and remaining locks

The user requested only the build and isolated tests at this stage. `--diagnose`, `--inject-demo`, `--source`, and `--target` were not run. Permissions, the local shortcut, EIS geometry and devices, the editor demonstration, crossing and returning, the Spanish keyboard layout, and recovery remain pending. No test listener or connection was started.

Next local step, with user present: diagnosis D2; disposable editor, local output Ctrl+Alt+F12 and
independent recovery medium prepared; authorize and check the actual shortcut before prolonging any
control. Do not enable reliable local capture cul-de-sac. The demonstration must be limited and
observed; the output of the program does not certify text or movement.

Before connecting Windows, agree to review and rebuild both ends, roles, IP and 47830/TCP port,
public certificates and fingerprints, editor, duration, boot order and recovery. Start with identities and
short sessions according to the manual. No keys have been generated or contacted with the other
computer. Current Windows reconstruction and optional X11 continue with no new evidence; X11 does
not replace Wayland validation. There is no Wayland/TLS build block left on this machine. Product
integration remains delayed.

## Historical evidence preserved — previous delivery and review

Documentary review: **November 5, 2026**. Agent's initial evidence: November 4, 2026. This report
distinguishes presence of implementation, recorded compilation, isolated tests and manual
validation.

## 1. Current status

The prototype contains Windows, Linux/X11 and Linux/Wayland. Wayland is no longer an absent backend:
there are its native adapter, portable authorization flows and the selection of backends. Previous
versions of this report described only the first X11 delivery and did not reflect those changes.

| Componente | Implementation | Available evidence |
| --- | --- | --- |
| Core, protocol, geometry and controllers | Implemented | Existing Windows Self-test: **538 checks, 0 bugs**, executed November 5 |
| Wayland flows without desktop | Implementeds en `wayland_flow.*` | Included in the self-test with PortalOps doubles |
| Windows adapter | Implemented | Initial agent compilation with MSVC/OpenSSL; later binary in `build/cmake/` |
| Adaptador X11 | Implemented | No native Linux compilation recorded in this report |
| Wayland GDBus/libei adapter | Implemented en `wayland_backend.cpp` | No recorded evidence of compilation of native Linux adapter |
| Linux selection and CMake options | Implementadas | Code revision: Wayland and X11 are optional and selected according to session |
| Mutual TLS and fingerprints | Implemented | Windows compilation documented by agent; handshake pending |
| Real desktop and two computers | Evidence prepared | ** Manual pendiente**, both in Windows and Ubuntu |

The November 5 review did not recompile the application or run capture, injection, portals,
graphical diagnosis or network. It ran only the existing self-test independent binary. The presence
of a binary or native code does not certify that the Linux backend compiles.

## 2. Current implementation

- `src/core/`: events, HID uses, conversions, protocol, geometry, teclas/botones status and
  controllers.
- `src/platform/windows_backend.cpp`: hooks, SendInput and emergency exit.
- `src/platform/linux_backend.cpp`: X11 implementation, not used as a Wayland session failback.
- `src/platform/linux_factory.cpp`: selection per session and backend included in the compilation.
- `src/platform/wayland_flow.*`: authorization sequences, barriers, activation, release, closure and
  recovery using the PortalOps contract.
- `src/platform/wayland_backend.cpp`: GDBus, InputCapture, RemoteDesktop, GlobalShortcuts and EIS
  communication with libei.
- `src/net/tls_link.cpp`: TLS transport and fingerprint identity check at both ends.
- `src/main.cpp`: explicit modes; preparation of permissions before starting communication.
- `tests/self_test.cpp`: core and Wayland flow tests with doubles, no real connections.

The code retains the range of a monitor per equipment, horizontal layout and a selected physical
origin. It does not synchronize modifiers or buttons that were already pressed when crossing; they
must be released before crossing.

## 3. Evidence and compilation limits

### Initial evidence of the staff member — 4 November

| Entorno | Evidence declared in the first delivery |
| --- | --- |
| Windows 10.0.19045 x64 | MSVC 19.51.36257.0, tools 14.51.36231, Windows SDK 10.0.26100.0 |
| CMake Windows | 3.24.3, NMake, OpenSSL 3.0.5 |
| WSL Ubuntu 20.04 | GCC 9.4.0, CMake 3.25.2; only kernel without backend or TLS |
| First battery isolated | 403 checks, 0 bugs in MSVC and GCC 9, prior to Wayland enlargement |

These initial results are historical; they do not verify the Wayland adapter added later.

### Review of November 5

`build/cmake/input_experiment.exe`, `build/cmake/input_self_test.exe` and `build/help.txt` were
located, with a date of November 4 after Wayland fonts. Saved help already describes Wayland. A
native build of Linux is not inferred from this.

The `build/cmake-configure.log` file retains a failed attempt due to the absence of NMake in PATH;
it is not evidence of a correct final compilation. Commands to rebuild are maintained in
[MANUAL.md](MANUAL.md).

There is no recorded result for the WSL portable binary after Wayland enlargement. There is no
evidence of compilation of the GDBus/libei backend, X11 or native Linux TLS. These checks are still
pending and are not replaced by a compilation with `ONEINTWO_BACKEND=none`.

## 4. Insulated check performed on 5 November

From the experiment directory:

```powershell
.\build\cmake\input_self_test.exe
```

Observed output, output code 0:

```text
OneInTwo input experiment: self-tests aislados
Checks: 538, failures: 0
```

The test executable calls only `runSelfTests()`. It does not create native backends, does not open
the session bus and does not establish connections.

Coverage of the current battery:

- Protocol and rejection of invalid messages.
- HID/Windows/evdev/X11 key conversion and geometry.
- origen/destino states, sequences and generations.
- Release of teclas/botones, emergency and timers.
- Geometry and Wayland barriers with synthetic data.
- Sequences InputCapture and RemoteDesktop with PortalOps doubles.
- Denial of permits, closure, loss of capture and portal edge events.

The battery tests the logic of the flows; it does not test the GDBus/libei code, the composer, the
actual shortcut reserve or the cursor response. Tests of the other experiments have not been
repeated during this update.

## 5. Requirements for the current Wayland backend

These are requirements of this implementation, not a general guarantee of compatibility of a version
of Ubuntu.

| Papel | Verification or remedy required by the code |
| --- | --- |
| Compilation | `gio-2.0`, `libei-1.0`, pkg-config; OpenSSL for communication between teams |
| Source | InputCapture version 2 or higher and keyboard and pointer capabilities |
| Authorized source | A display area, EIS connection, border barrier and activation granted by the composer |
| Destino | RemoteDesktop version 2 or higher and GlobalShortcuts version 1 or higher |
| Authorized target | Awarded keyboard and pointer, accepted emergency shortcut and usable EIS devices |
| Geometry of destination | Absolute pointer, pointer device for botones/rueda and a valid desktop region |

The source uses CreateSession2/Start/ConnectToEIS/GetZones/SetPointerBarriers/Enable. The
destination prepares RemoteDesktop and reserves the shortcut using GlobalShortcuts before listening
to the network. If local output is not granted, the flow is rejected.

The geometry can only be known after the sessions are authorized. The previous diagnosis does not
show that permissions will be granted nor that EIS will offer the necessary devices. No minimum
version of verified Ubuntu has been set.

## 6. Capacities and constraints to be validated

- The capture is activated by the composer upon reaching a barrier. Control uses that notification,
  not a simulated global observation of the cursor.
- The return in origin requests Release with a suggested position. It is not guaranteed that the
  composer places the pointer exactly in that position.
- Wayland destination cannot restore a global position that it does not know; this backend does not
  offer that restoration.
- The destination requires a local shortcut granted. The combination that actually reserves the
  desktop must be checked and its activation reached the process.
- The Linux destination does not detect local physical activity such as the Windows backend; the
  emergency output must be used.
- A change of zones, closure of the portal or loss of EIS should cause the expected transition; it
  needs to be validated on the desktop.
- The target watchdog covers loss of origin, not the death of the recipient process itself.
- There is no automatic reconnection or guaranteed support for multiple monitors, IME or keys
  outside the table.
- The high windows and secure Windows desktop are not part of the warranty of this experiment.

## 7. Pending matrix and next step

| Area | Estado | Test required |
| --- | --- | --- |
| Windows compilation of the current state | No new compilation in this review | CMake reconstruction |
| Native Linux/Wayland Compilation | No recorded evidence | Set `ONEINTWO_WAYLAND=ON`, enable TLS, and disable X11 |
| Compilation Linux/X11 | No recorded evidence | Force X11 to evaluate that alternative |
| Wayland permissions | Manual pending | Grant, deny, cancel, and revoke sessions |
| Local input | Manual pending | Demonstration tapered in a disposable editor |
| Crossing and returning | Manual pending | Both directions between Windows and Ubuntu |
| Recovery | Manual pending | Shortage at both ends, network cutting, withdrawal of permission and loss of origin |
| TLS | Manual pending | Correct and incorrect identity at both ends |
| Keyboard portability | Manual pending | Spanish distribution, modifiers and buttons |

Follow [MANUAL.md](MANUAL.md), registering system version, session, portals, libraries and observed
result. The documentary discrepancy is resolved; desktop validation and native Linux compilation
remain open in [TODO.Md](../../TODO.md).

## 8. Conclusion

There is implementation of Windows, X11 and Wayland, and up-to-date isolated evidence for the kernel
and dual flows. There is still not enough evidence to claim that the Wayland adapter compiles or
that the two teams cross, inject and regain control on a real desktop. This delivery prepares manual
evaluation; it does not certify platform support.

