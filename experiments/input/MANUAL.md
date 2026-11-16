# Run Manual — Keyboard and Mouse

Updated: **November 5, 2026**. Describes the current code, including Windows, Wayland and the
alternative X11. Shortcut, local demo and verified R2/R9 Wayland; recovery from cuts and entry
between pending computers. This manual is for explicit user execution; compiling or running
self-tests does not allow automatic booting of capture or injection.

## 1. Before you begin

- One monitor per unit and horizontal layout.
- One device provides the keyboard and mouse. Either device can act as the source.
- Use an editor and disposable data. Unleash modifiers and buttons before crossing.
- Without arguments or with `--help`, the program displays help.
- `input_self_test` and `--self-test` do not use desktop or network.
- `--diagnose` consults the desktop and, in Wayland, the session bus; does not request control
  sessions.
- `--check-target` in Wayland opens only GlobalShortcuts and waits for the shortcut; does not open
  RemoteDesktop, EIS nor network, nor requires geometry. It is the first supervised test of the
  corrected version, verified with the user and the 30 s guide.
- `--inject-demo` moves the cursor and writes. `--source` and `--target` use devices and network.
- Target Wayland: **Ctrl+Shift+F12**, using GlobalShortcuts and with new identifier. Windows/X11
  retain Ctrl+Alt+F12. Do not use Ctrl+AlT+F12 for this Wayland test: it matches the mutter console
  change.
- Native Ubuntu/Wayland compilation, demonstration, emergency with active EIS and verified
  withdrawal of permission. Cuts between pending equipment; the previous restart incident is
  preserved (EXP-INPUT-02).

## 2. Requirements and compilation

Run from the actual experiment folder. The main application and its installers are not integrated:
these commands build only the prototype.

### 2.1 Windows

You need to compile C++17, Windows SDK, CMake 3.16 or later and OpenSSL with cabeceras/bibliotecas
for the same architecture. The initial reference environment is shown in [RESULTS.md](RESULTS.md).

In an x64 Native Tools Command Prompt:

```bat
cd /d D:\DEVEL\OneInTwo\experiments\input
cmake -S . -B build\windows -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64"
cmake --build build\windows
build\windows\input_self_test.exe
```

If `rc.exe` or `mt.exe` are missing, load the Windows SDK tools or indicate their routes using `CMAKE_RC_COMPILER` and `CMAKE_MT`. Adjust the routes to the actual installation.

Before running `input_experiment.exe`, provide your OpenSSL DLL. For example, in PowerShell:

```powershell
$env:Path = "C:\Program Files\OpenSSL-Win64\bin;" + $env:Path
```

The independent self-test does not require OpenSSL DLL.

### 2.2 Ubuntu with Wayland — initial target

Identify session and libraries:

```bash
echo "$XDG_SESSION_TYPE"
echo "$WAYLAND_DISPLAY"
pkg-config --modversion gio-2.0 libei-1.0 openssl
```

The code requires InputCapture v2 for origin; RemoteDesktop v2 and GlobalShortcuts v1 for
destination. Actual availability will be checked in diagnosis and preparation. It is not stated that
a specific version of Ubuntu offers them.

In an Ubuntu whose repositories provide dependencies, the user can install:

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libssl-dev libglib2.0-dev libei-dev
```

If a package does not exist in that version, record the lock; do not present the backend as built.
Installations and system changes are manual steps.

Force Wayland compilation avoids confusing a binary without backend with a satisfactory result:

```bash
cd ~/OneInTwo/experiments/input
cmake -S . -B build/wayland -DCMAKE_BUILD_TYPE=Release \
  -DONEINTWO_WAYLAND=ON -DONEINTWO_X11=OFF -DONEINTWO_WITH_TLS=ON
cmake --build build/wayland -j
./build/wayland/input_self_test
```

These commands are pending instructions, not a verified Linux build.

### 2.3 Ubuntu with Xorg — separate alternative

Only if you want to try X11:

```bash
sudo apt install build-essential cmake libssl-dev libx11-dev libxi-dev libxtst-dev
cmake -S . -B build/x11 -DCMAKE_BUILD_TYPE=Release \
  -DONEINTWO_X11=ON -DONEINTWO_WAYLAND=OFF -DONEINTWO_WITH_TLS=ON
cmake --build build/x11 -j
./build/x11/input_self_test
```

Run that binary from an Xorg session. The backend selection does not replace Wayland with X11 within
a Wayland session.

### 2.4 Single core and double flows

For an environment without desktop or native headers:

```bash
cmake -S . -B build/portable -DONEINTWO_BACKEND=none -DONEINTWO_WITH_TLS=OFF
cmake --build build/portable --target input_self_test -j
./build/portable/input_self_test
```

The native application is not built. The reference observed in Windows on November 5th is **538
checks, 0 bugs**, output 0. A difference in number requires checking code version and binary; any
bug prevents continuing with the actual tests. This battery does not validate GDBus/libei or X11.

## 3. Certificates and identity

Each team uses its own certificate and key. To exchange and compare the traces of the public
certificate by a means of trust; to keep the private key on your computer.

PowerShell:

```powershell
New-Item -ItemType Directory -Force certificates | Out-Null
openssl req -x509 -newkey rsa:3072 -sha256 -nodes -days 30 -keyout certificates\win.key -out certificates\win.crt -subj "/CN=OneInTwo-Windows"
openssl x509 -in certificates\win.crt -noout -fingerprint -sha256
```

Ubuntu:

```bash
mkdir -p certificates
chmod 700 certificates
openssl req -x509 -newkey rsa:3072 -sha256 -nodes -days 30 \
  -keyout certificates/ubuntu.key -out certificates/ubuntu.crt -subj "/CN=OneInTwo-Ubuntu"
chmod 600 certificates/ubuntu.key
openssl x509 -in certificates/ubuntu.crt -noout -fingerprint -sha256
```

Save the fingerprints: Windows will receive Ubuntu's and Ubuntu's from Windows.
`--peer-fingerprint` supports 64 hexadecimal digits, with separators. The abbreviated prints of
examples should be replaced by full values.

These certificates serve for experiments; they do not implement the persistent linkage of the
application.

## 4. Diagnosis and permits

Windows:

```powershell
.\build\windows\input_experiment.exe --diagnose
```

Ubuntu/Wayland:

```bash
./build/wayland/input_experiment --diagnose
```

In Xorg, replace the executable with `./build/x11/input_experiment`. If both adapters were compiled,
the program chooses according to the session.

Register name of backend, session, declared capabilities and notes. In Wayland the diagnosis
consults versions of portals; the definitive geometry and EIS devices are checked when preparing
each paper. A declared capacity is not a permission granted.

| Papel Wayland | Manual preparation expected |
| --- | --- |
| Origen | Authorize InputCapture for teclado/puntero; obtain an area and assemble a barrier on the edge |
| Destino | Authorize RemoteDesktop for teclado/puntero and reserve the emergency shortcut using GlobalShortcuts |
| Target with insufficient devices | Rejection if keyboard is missing, absolute pointer, pointer for botones/rueda or a valid region |
| Denial or cancellation | Explicit error and release of prepared sessions, without enabling control |
| Withdrawal of permits during the test | Recovery or closure; recording actual behaviour |

The destination prepares permissions before listening to the network. The source also prepares
InputCapture before connecting. During that preparation do not bring the cursor to the test edge
until the program announces that it is armed.

If the desktop does not reserve the required local output, the destination backend must refuse the
preparation. Do not take the shortcut for accepting a dialogue: check that the combination actually
releases the control.

### Check the output without injecting

#### Guidance supervised with automatic lap at 30 seconds

This isolated trial passed with the user present on November 5th; it does not yet validate EIS
control or recovery of a frozen image. To repeat it, with the present user, from the root of the
repository:

```bash
env -u LD_LIBRARY_PATH GDK_BACKEND=wayland /usr/bin/python3 experiments/input/tools/shortcut_check.py
```

The window explains the steps and **does not start the test when opening**:

1. Click "Start 30-second test" on the guide.
2. In GNOME, check the row "Return Local Control (OneInTwo)" with Ctrl+Shift+F12 and click **"Add" /
   "Add", top right**. If you propose Ctrl+Alt+F12 or share pantalla/control, click Cancel.
3. Return to the guide and wait **"NOW: press and release Ctrl+Shift+F12"**, big up, next to the
   countdown. Press it once. If the dialog is no longer displayed because the permission is saved,
   wait for the guide message as well.
4. Wait without further keys. At 30 s the timer closes the scope of the test and attempts are made
   to reactivate the original graphic session. "Cancel and return to desktop" allows it to do so
   earlier.

The return depends on a transitory service separated from the guide and the prototype. It is first checked that you can reactivate the session without asking for password; If it fails, it does not start the trial. The supervisor's test with a `sleep` process worked. A return from another VT has not been validated and GNOME or the image is promised to be repaired if the composer crashes. Always confirm the visual result with the user. It is still only GlobalShortcuts, without RemoteDesktop, EIS, capture and network. Python, PyGObject and GTK3 are already present; Nothing's installed. Logs are at `manual-output/guided-*`.

#### Previous direct command with 60 seconds save

The previous trial with Ctrl+Alt+F12 opened EIS and ended in a user-reported freeze, with reboot
recovery. It is still recorded as failure in EXP-INPUT-02. The preparation has been fixed by user
request. The first corrected attempt, only from GlobalShortcuts, ended at 63 s by external guard;
the shortcut was not yet verified.

The new version uses Ctrl+Shift+F12 and `oneintwo-emergency-v2`, so as not to reuse a persistent
Ctrl+Alt+F12 concession. `--check-target` opens **GlobalShortcuts only**: no RemoteDesktop
permission, EIS connection, emulation transaction or network. You do not need to disable monitors
for this check.

In a future supervised session, with independent recovery prepared, authorize only the shortcut. If
the dialog proposes the old combination, cancel; do not press it. When the wait appears, press
Ctrl+Shift+F12 once. You must print "Verified local shortcut" and close with code 0; rejected
permission, closed session or time out give code 1. Do not advance to injection if failed. The
waiting limit starts after permission; also use the external save indicated below. Do not interpret
this separation as a guarantee that a composer or graphic controller can never be blocked.

GlobalShortcuts requires application identity. In the Ubuntu validation, a
`io.github.OneForTwo.InputExperiment.desktop` entry was prepared in the project log directory, with
a link of the same name in `~/.local/share/applications/`. You must identify this prototype with
`Type=Application`, `Name=OneInTwo — input test`,`Exec` pointing to the real
binary,`Terminal=true` and `NoDisplay=true`; do not use the identity of another application. The
link is temporary and can be removed when finished. No global installation or autostart is needed.

From the root of the repository, with that record prepared, launch the prototype in a transitional
scope of application:

```bash
env -u LD_LIBRARY_PATH systemd-run --user --scope --collect \
  --unit=app-io.github.OneForTwo.InputExperiment-check \
  --property=RuntimeMaxSec=60s --property=TimeoutStopSec=3s \
  /bin/bash -c 'source .local-state/ubuntu-validation/env.sh; exec timeout --signal=TERM --kill-after=3s 60s ./experiments/input/build/ubuntu-wayland/input_experiment --check-target --max-seconds 60'
```

The 60 external seconds include permission and start, not just waiting for the shortcut. The systemd
manager limits the scope regardless of the prototype loop; `timeout` also limits the process, with
three seconds of margin before forced closure. This does not guarantee to recover a blocked
composer. If time runs out in a dialog, record the essay as incomplete.

Load local libraries into the child: `systemd-run` must use those of the system. The log indicates
the description of the combination returned by the portal; receiving an identifier does not prove
that the physical combination works. The shortcut dialog may not be repeated if the desktop retains
the concession. To investigate an uninjected disconnection, `ONEINTWO_EI_DEBUG=1` enables the libei
log; limit it to preparation with test data. Do not enable it on sessions with private content.

## 5. Optional local injection

This test does move and type. The isolated shortcut, the 30 s supervisor and a local demonstration
passed on November 5: `oneintwo` text, movement and desktop responding confirmed by the user. With
the user present, the local guide includes a disposable text box and steps to authorize the
demonstration:

```bash
env -u LD_LIBRARY_PATH GDK_BACKEND=wayland /usr/bin/python3 experiments/input/tools/shortcut_check.py --inject-demo
```

Select Start and press and release Ctrl+Shift+F12 when the guide says NOW. Authorize remote keyboard and mouse interaction without screen access. When the guide says TEST AREA, focus its text box. Keep the same window open and leave the keyboard alone. The prototype waits 5 s, then attempts to move the pointer by a few pixels and type `oneintwo` without clicking. The independent 30 s limit includes the dialogs and closes only this trial's scope. If it expires first, record the trial as incomplete. Select Cancel to finish early. The guide does not record the text box contents. Ask the user to confirm the text, pointer movement, and desktop response. Logs are in `manual-output/guided-*`.

With an external disposable editor, the example Windows is:

```powershell
.\build\windows\input_experiment.exe --inject-demo --arm-seconds 5
```

In Wayland, both `--inject-demo` and `--target` first reserve the shortcut and require **clicking
and dropping Ctrl+Shift+F12 in 30 seconds**, before requesting RemoteDesktop or connecting EIS. If
activation, release or portal is missing, they end up without opening control. This verification is
repeated at each run; the separate test does not keep a mark of confidence.

Only after the RemoteDesktop permissions appear. Authorize keyboard and mouse without screen, with a
single active monitor and disposable editor ready. Then start the countdown: focus the editor during
that interval. EIS expects resumed devices and does not start emulation when preparing permissions;
it starts when sending the first real event. Pause, withdrawal or closing stop the flow, without
automatically resuming it. The normal lock releases the keys and buttons that this emitter has left
pressed and finishes its emulation.

The test tries a short movement and writes `oneintwo`. Register what you observed: "Finished demo"
does not certify text, movement or recovery. The following previous direct command only limits the
process; to repeat on this machine use the 30 s guide above, which adds independent recovery. From
the root and with the application identity prepared:

```bash
env -u LD_LIBRARY_PATH systemd-run --user --scope --collect \
  --unit=app-io.github.OneForTwo.InputExperiment-demo \
  --property=RuntimeMaxSec=60s --property=TimeoutStopSec=3s \
  /bin/bash -c 'source .local-state/ubuntu-validation/env.sh; exec timeout --signal=TERM --kill-after=3s 60s ./experiments/input/build/ubuntu-wayland/input_experiment --inject-demo --arm-seconds 10'
```

The guard limits the process; it does not guarantee to restore a frozen composer.

Complete before `--check-target`. The demo handles the shortcut during the countdown and between
events; do not start capture if local recovery has not been tested on the actual desktop.

## 6. Red local

### Local checks R2/R9 before moving on to two teams

They require an active monitor and present user. The guides retain the independent 30 s supervisor
and the shortcut check before EIS:

```bash
env -u LD_LIBRARY_PATH GDK_BACKEND=wayland /usr/bin/python3 experiments/input/tools/shortcut_check.py --emergency-check
# In a separate run, after the first has finished:
env -u LD_LIBRARY_PATH GDK_BACKEND=wayland /usr/bin/python3 experiments/input/tools/shortcut_check.py --revoke-check
```

They follow the local injection script, but keep EIS open after writing. R2: when you say EXIT NOW, press and drop Ctrl+Shift+F12 again. R9: when reading REVOKE PERMISSION NOW, press the upper indicator of GNOME ‘Stop Screen Sharing’. Do not use the Cancel button as a substitute for withdrawing permission. The guide should confirm the expected output and the desktop respond. A timeout doesn't validate the output. After initial rejection by multiple monitors and pause, R2 and R9 passed with a single monitor on November 5: output 0, closure before the timer and operating desktop confirmed by the user. Evidence detailed in RESULTS.md.

The CLI uses `--inject-demo --wait-for-stop --arm-seconds 3 --max-seconds 30`; that additional
argument serves only for this experimental test.

Example port: **47830/TCP**. Both computers must be achievable. The target listens to the interfaces
that use their implementation; the user must limit the firewall to the source IP.

Optional example on Windows with an admin console:

```powershell
New-NetFirewallRule -DisplayName "OneInTwo input 47830" -Direction Inbound -Protocol TCP -LocalPort 47830 -RemoteAddress 192.168.1.20 -Action Allow -Profile Private
```

Remove the rule when finished:

```powershell
Remove-NetFirewallRule -DisplayName "OneInTwo input 47830"
```

If Ubuntu uses UFW, a limited rule would be:

```bash
sudo ufw allow from 192.168.1.20 to any port 47830 proto tcp
```

Su retirada:

```bash
sudo ufw delete allow from 192.168.1.20 to any port 47830 proto tcp
```

Replace IP with the real source. The agent does not execute these changes. Do not post keys or open
the port to the Internet.

## 7. Test Windows as source and Ubuntu as destination

First short session: destination 60 seconds and origin 30 seconds, to leave room for preparation.
Control timers start after preparing permissions and the connection, do not limit all the time spent in the
dialogues.

1. Launch destination Ubuntu first and complete permissions.
2. Wait for the TLS listening ad.
3. Start the Windows source.
4. Focus on Ubuntu the disposable editor before crossing.
5. Wait for the armed source announcement and cross with modificadores/botones loose.

Ubuntu:

```bash
./build/wayland/input_experiment --target --listen 47830 \
  --cert certificates/ubuntu.crt --key certificates/ubuntu.key \
  --peer-fingerprint "HUELLA_COMPLETA_DE_WINDOWS" --max-seconds 60
```

Windows, located to the left of Ubuntu:

```powershell
.\build\windows\input_experiment.exe --source --connect 192.168.1.50:47830 --edge right `
  --cert certificates\win.crt --key certificates\win.key `
  --peer-fingerprint "HUELLA_COMPLETA_DE_UBUNTU" --arm-seconds 10 --max-seconds 30
```

Replace direction and fingerprints. The return edge is deduced at destination. Check movement,
click, wheel, text and turn on the opposite edge. Immediately test the local output before extending
the duration.

## 8. Investing roles

Windows as target:

```powershell
.\build\windows\input_experiment.exe --target --listen 47830 `
  --cert certificates\win.crt --key certificates\win.key `
  --peer-fingerprint "HUELLA_COMPLETA_DE_UBUNTU" --max-seconds 60
```

Ubuntu as source, located to the right of Windows:

```bash
./build/wayland/input_experiment --source --connect 192.168.1.20:47830 --edge left \
  --cert certificates/ubuntu.crt --key certificates/ubuntu.key \
  --peer-fingerprint "HUELLA_COMPLETA_DE_WINDOWS" --arm-seconds 10 --max-seconds 30
```

The Wayland origin receives the activation of the composer's barrier. Upon return request Release
with suggested position: verify the actual return without requiring the cursor to appear in an exact
coordinate.

A correct address does not validate the inverse. If Xorg is tested, repeat and register in separate
rows.

## 9. Matriz manual

The Observed column distinguishes local Ubuntu evidence from pending cases. Register system,
session and desktop, portals, libei and layout; the details and previous attempts are preserved in
RESULTS.md.

| Caso | Action | Result to be checked | Observado |
| --- | --- | --- | --- |
| D1 | Diagnosis Windows | Reported backend and capabilities are consistent | Pending |
| D2 | Wayland Diagnosis | Required portals and capabilities; no failback X11 | Ubuntu verified. InputCapture v1 blocks source operation |
| D3 | Optional Xorg diagnosis | X11/XI2/XTest, if that backend was compiled | Pending |
| P1 | Deny or cancel InputCapture | Origin rejects and cleans preparation | Pending |
| P2 | Deny RemoteDesktop or the shortcut | Destination does not enable control session | Cancellation RemoteDesktop verified in Ubuntu |
| P3 | Grant permissions | Geometry and required devices available | Ubuntu target verified with one monitor |
| I1/I2 | Local injection Windows/Wayland | Movement and text in the chosen editor | I2 Ubuntu exceeded; I1 Windows pending |
| C1 | Windows → Ubuntu | Crossing, entry and return | Pending |
| C2 | Ubuntu → Windows | Activation by barrier, entry and return | Pending |
| C3 | Go out and get close to the edge again | No repeated crossings or duplicate sessions | Pending |
| K1 | Spanish keyboard | ñ, accents, AltGr, symbols and modifiers | Pending |
| K2 | Click, drag and wheel after crossing | Correct actions and releases | Pending |
| R1 | Emergency recovery at source | Catch released without waiting for destination | Pending |
| R2 | Emergency recovery at target | Released entry and local operational exit | Local Ubuntu check passed on November 5, 2026 |
| R3 | Physical activity in Windows destination | Session ends and state is released | Pending |
| R4 | Network cut with key or mouse button pressed | Local Recovery and Remote Watchdog | Pending |
| R5 | Close source process from another medium | Destination frees by its timer | Pending |
| R6 | Reaching maximum duration | Closure and cleaning on both ends | Pending |
| R7 | Cancel before crossing | Departure and closure of authorized meetings | Pending |
| R8 | Improper print on each end | Rejection, no control session authorized | Pending |
| R9 | Remove session or permission from desktop | Error, release and recovery | Local Ubuntu check passed on November 5, 2026 |
| R10 | EIS closure or change of zone | Recovery or rejection with explanation | Pending |
| R11 | Lack of portal/devices/fit | Concrete error, without simulating success | Pending |
| R12 | Puerto inaccesible | No active capture or injected input left | Pending |

To test an incorrect footprint, modify a digit and repeat with the error at each end separately. To
prove loss of origin, have another means of closing the process; do not kill the receiver while
keeping inlet injected as first test.

The key identity is HID positional. Use Spanish distribution at both ends and register the physical
key if the character does not match. Example:

```text
♪ I'm going to ♪
Ctrl, Alt, Shift, and AltGr
```

## 10. Recovery and limits

- Source: The expected emergency combination is Ctrl+Alt+F12 during capture. You must return to
  local control; your actual behavior is unverified.
- Destination Windows: local emergency hook and physical activity detection.
- Destination X11: Save the shortcut using XGrabKey.
- Wayland Destination: GlobalShortcuts with Ctrl+Shift+F12; requires activation and release before
  RemoteDesktop/EIS. If failed, no control is opened.
- Loss of communication: target of the 2 s code in origin and 1,5 s of watchdog in destination. They
  are configured timers, not measured latencies.
- Wayland origin requests return position; Wayland destination does not restore a previous global
  cursor.
- The watchdog does not survive the death of the receiver itself. Use the intended output and
  release states before necessarily closing that process.
- A monitor, unique region, without automatic reconnection, video, clipboard or multiple devices.
- Control of Windows elevated windows and secure desktop is not guaranteed.
- Modifier keys and mouse buttons held before crossing are not synchronized.
- The compiled library and API do not demonstrate actual session support.

## 11. Diagnosis of problems

| Symptoms | What to check |
| --- | --- |
| Wayland detected, backend missing from build | Rebuild with `ONEINTWO_WAYLAND=ON` and its dependencies; do not use a portable build |
| Catch not available | InputCapture v2 and teclado/puntero capabilities that detects the prototype |
| Injection not available | RemoteDesktop v2 and GlobalShortcuts v1, then check permissions and EIS devices |
| Zero geometry in diagnosis | Repeat observation after preparing permissions; not necessarily known before |
| Preparation of rejected destination | Unallocated catch, absent absolute device, absent region or several regions |
| Permission error or timeout | denied or cancelled dialogue or no-response portal; record full text |
| No conecta | Destination already authorized and listening, IP, port and firewall |
| Certificate error | Full footprint of the other equipment and correct certificado/clave pair |
| Windows executable fails to start | OpenSSL DLL compatible architecture in PATH |
| Cannot return to the source | Register border, coordinates, backend and status; use local output and mark bug |
| Wrong keys | Distribution, HID conversion and modifiers; not valid proof to appear text |

## 12. Registro

Write down the results in [RESULTS.md](RESULTS.md), keeping separate compilation, self-test and each
real test. The 538/0 reference corresponds to the self-test Windows executed on November 5th; it is
not evidence of a handshake, portal, or actual pasting.

