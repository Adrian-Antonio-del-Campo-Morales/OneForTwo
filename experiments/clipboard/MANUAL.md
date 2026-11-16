# Implementation manual — clipboard experiment

Date: November 4, 2026.

Status: Prototyping with Windows adapter and Linux/Wayland adapter based on `xdg-desktop-portal`
RemoteDesktop + Clipboard. Wayland adapter is implemented and compiled, but **has not been executed
in a real graphic session**: this environment does not have a desktop. Reading, writing,
observation, permissions and connection between computers are still manual.

This document belongs to `experiments/clipboard/`. It does not depend on the root CMake, product
code or other experiments. It should not run more than one experiment at a time in the same
graphical session during manual testing.

## 1. What is verified and what is not

Checked by the agent, without touching the desktop or network:

- Windows compilation with MSVC/VS 2026, CMake 4.3.1 and OpenSSL 3.5.
- Linux kernel and Linux adapter compilation in Ubuntu 20.04 (WSL).
- Linux compilation of the Wayland adapter with `libdbus-1` (heads removed locally from a `.deb`;
  nothing installed globally).
- `--self-test`: **151 checks, 0 bugs** in the Linux compilation with portal; **149, 0 bugs ** in
  Windows and in the Linux build without portal.
- `--inventory`, `--help`,`--version` and `--emit-samples` (files only).

Unverified: lectura/escritura actual clipboard, pasted in an editor, background observation,
portal dialog and permissions, real Wayland session, TLS Linux running, connection between two computers
and desktop permissions. That could not be tested here **does not show Wayland does not allow
access**; implementation exists and pending validation is described below.

Differences not to be confused:

- **Implemented and compiled:** Wayland adapter and D-Bus flow.
- **Developed Desktop Restriction:** in GNOME 46 the Share button in the RemoteDesktop dialog is
  only enabled with “Allow Remote Interaction” or a screen source; with `SelectDevices(types=0)`
  that switch does not grant input devices. It is documented with the source in section 10.
- **Not implemented:** the X11 backend. Not used as a silent substitute.
- **Not verified:** everything that requires a graphic session, a real portal or two machines.

## 2. Requisitos

### Windows

- Windows 10 u 11 x64.
- Visual Studio 2026 Build Tools with the C++ and Windows SDK workload, or an equivalent
  installation including MSVC and the Visual Studio generator.
- CMake. CMake included in Visual Studio (`cmake version 4.3.1-msvc1`) was used on this machine.
  System CMake (3.24) does not generate projects for VS 18.
- OpenSSL 3.x Win64 with development libraries and DLL. On this machine: `C:\Program
  Files\OpenSSL-Win64`.
- DLL `libssl-3-x64.dll` and `libcrypto-3-x64.dll` must be in `PATH` when running the binary. If
  OpenSSL is not found when configuring, the prototype is compiled without inter-team mode and
  indicates it.

Routes used on this machine:

```text
CMake:   C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
OpenSSL: C:\Program Files\OpenSSL-Win64
```

### Ubuntu

To compile the native Wayland kernel and adapter:

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libdbus-1-dev
```

To also enable the mode between computers with TLS:

```bash
sudo apt install libssl-dev
```

`libdbus-1-dev` provides `dbus/dbus.h` and, in Linux multiarch, `dbus/dbus-arch-deps.h`; CMake
locates them with `pkg-config`. If missing, CMake issues a warning and compiles the Linux adapter as
an explicit limitation (`portal_client=unavailable`). Nothing is installed automatically nor is
`sudo` used from the binary.

The agent did not install packages globally. For verification of this experiment `libdbus-1-dev` was
downloaded locally and extracted in `/tmp`; that path is disposable. In a normal installation the
previous command is sufficient.

WSL is used to compile and run auto-tests, not to test the clipboard: there is no graphical session.
Desktop tests must be done in a real Ubuntu session.

## 3. Separate compilation

There are three different checks. Do not mix “compile” with “works on the desktop”.

### 3.1 Core, No Native Adapter

It serves to verify the model, engine, protocol and executable without relying on libdbus or
OpenSSL:

```bash
cd ~/path/to/OneInTwo/experiments/clipboard   # or /mnt/d/... when building in WSL
cmake -S . -B build/linux-core -DOIT_ENABLE_PORTAL=OFF -DOIT_ENABLE_PEER=OFF
cmake --build build/linux-core -j
./build/linux-core/oneintwo-clipboard --self-test
```

Expected result: `checks=149 failures=0`, `SELF-TEST RESULT: PASS`, output code 0. The Linux adapter
compiled in this mode only reports its limitation; it does not fake reading or writing.

### 3.2 Native Wayland adapter

With `libdbus-1-dev` installed:

```bash
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux -j
./build/linux/oneintwo-clipboard --self-test
./build/linux/oneintwo-clipboard --inventory
```

The settings must show:

```text
-- xdg-desktop-portal backend enabled (libdbus: ...)
```

Expected result: `checks=151 failures=0`, `SELF-TEST RESULT: PASS`, and inventory `portal_client=compiled`. If a notice of `libdbus-1` and `portal_client=unavailable` appears, the Wayland adapter was not compiled; install `libdbus-1-dev` and re-configurate.

If the package cannot be installed globally, a local alternative (used here) is to download and
extract the `.deb`:

```bash
mkdir -p /tmp/oit-deps && cd /tmp/oit-deps
apt-get download libdbus-1-dev
dpkg-deb -x libdbus-1-dev_*.deb dbus
cmake -S /ruta/a/experiments/clipboard -B build/linux-local \
  -DCMAKE_BUILD_TYPE=Release \
  -DDBUS_INCLUDE_DIR=/tmp/oit-deps/dbus/usr/include/dbus-1.0 \
  -DDBUS_ARCH_INCLUDE_DIR=/tmp/oit-deps/dbus/usr/lib/x86_64-linux-gnu/dbus-1.0/include \
  -DDBUS_LIBRARY=/lib/x86_64-linux-gnu/libdbus-1.so.3
```

The multiarch path and the version of the `.deb` can be changed. No headers were copied to the
repository or changed the system.

### 3.3 Windows

```powershell
cd D:\DEVEL\OneInTwo\experiments\clipboard
$cmake = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake -S . -B build/win -G "Visual Studio 18 2026" -A x64 `
    -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64"
& $cmake --build build/win --config Release
```

Expected result: `build\win\Release\oneintwo-clipboard.exe`, output code
0. It is normal to see C4996 notices about `fopen` in `tls_channel.cpp`; they do not prevent
   compilation.

Before running on that console:

```powershell
$env:PATH = "C:\Program Files\OpenSSL-Win64\bin;$env:PATH"
```

If the Visual Studio path changes, it can be located with:

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
```

## 4. Reproducible isolated checks

These commands **do not**read or write the actual clipboard, do not open sockets, do not start the
session bus and do not capture input. They are the ones already executed:

```powershell
.\build\win\Release\oneintwo-clipboard.exe --self-test
.\build\win\Release\oneintwo-clipboard.exe --inventory
.\build\win\Release\oneintwo-clipboard.exe --version
```

```bash
./build/linux/oneintwo-clipboard --self-test
./build/linux/oneintwo-clipboard --inventory
```

Key expected output on Linux with portal:

```text
PASS text_validation
...
PASS portal_message_helpers
...
checks=151 failures=0
SELF-TEST RESULT: PASS
```

`--self-test` uses synthetic chains and a fake adapter. Check UTF-8, boundaries, line breaks, logical order, echo, duplicates, generations, continuance and malformed plots. In the compilation with portal adds `portal_message_helpers`, which builds and parses D-Bus messages in memory: dictionary `a{sv}`, `mime_types`, `Response`, `SelectionOwnerChanged` and `SelectionTransfer`. It does not open a connection or request permissions.

Without arguments only the help is shown; no service is initialized. `--inventory` on Windows displays `backend=win32-clipboard`, `read=yes write=yes watch=yes independent_of_input=yes` and `tls=openssl` capabilities. On Linux Wayland with compiled portal shows `backend=xdg-desktop-portal-clipboard`, `portal_client=compiled` and declared capabilities. Those capabilities describe the adapter, **no** a concession already negotiated: the dialogue and the result of `Start` are only evaluated when creating the port (`--read-text`, `--publish-*`, `--watch` or a session between computers). On Linux X11 shows `backend=linux-x11-unimplemented`; in a session without graphics, `linux-no-graphical-session`.

## 5. Manual validation in Ubuntu Wayland

Requirements: real Wayland graphic session, `xdg-desktop-portal` and desktop backend (e.g.
`xdg-desktop-portal-gnome`), and binary compiled with `libdbus-1-dev`. You should not test the
experiment in a X11 session or force `WAYLAND_DISPLAY` over X11.

First register the environment, without opening any session:

```bash
echo "session=$XDG_SESSION_TYPE desktop=$XDG_CURRENT_DESKTOP wayland=$WAYLAND_DISPLAY"
gnome-shell --version 2>/dev/null || true
dpkg -l | grep -E 'xdg-desktop-portal|gnome-shell' | head -20
./build/linux/oneintwo-clipboard --inventory
```

Write down the GNOME version, the `xdg-desktop-portal-gnome` version and the backend. The mere
presence of the interface in D-Bus does not grant access.

1. **Reading.** Copy a known synthetic text into a Wayland editor and run:

   ```bash
   ./build/linux/oneintwo-clipboard --read-text --show-content
   ```

   The RemoteDesktop dialog must appear. In GNOME 46:
   - “Allow Remote Clipboard” must be activated (the adapter calls `RequestClipboard` before
     `Start`);
   - “Allow Remote Interaction” must be enabled to enable Share; the adapter has ordered
     `SelectDevices(types=0)`, so that switch **does not grant keyboard, mouse or touch**. If the
     portal returns a mask other than zero, the adapter rejects the session and ends with error;
   - do not select a screen source to “unlock” Share: this experiment only shares text and does not
     need screen capture.

   After approval, compare the content. If cancelled, `Denied`/`clipboard unavailable` should be
   reported; a X11 failback should not be opened.

2. **Write and paste.**Run:

   ```bash
   ./build/linux/oneintwo-clipboard --publish-text "Hello 🌍 😀" --show-content
   ```

   Approve the dialog and paste into a Wayland editor with Ctrl+V. The text must appear. In Wayland the process retains the offer and handles pasting requests for a maximum of **60 seconds after authorizing**; paste within that interval. Ctrl+C ends before. When the offer is removed. `readback=match` checks the internal copy of the adapter and **does not show a real pasting**: the editor must be observed. The portal protocol does not transport the private Windows marker; the output indicates it and the engine uses the content footprint as an echo backup. Verify `readback=match`.

3. **Remark in the background.** Execute:

   ```bash
   ./build/linux/oneintwo-clipboard --watch --show-content
   ```

   With the experiment window inactive, copy text into another Wayland application. A `change #1`
   must appear; check that there is no echo of the text published by the experiment itself. Stop
   with Ctrl+C. No pulsation must be captured: the process only receives signals from the portal.

4. **Permission denied.**Repeat reading and press Cancel in dialog. Register status and output code.
   Do not read clipboard or start another backend.

5. **Portal missing or not supported.**In a session without `org.freedesktop.portal.Desktop` or
   without the Clipboard interface, the port must fail with `unavailable`/`not supported`, not
   simulated successfully. It must not be fixed by installing a different portal during this test.

6. **X11.** In a real X11 session, `--inventory` should report `linux-x11-unimplemented`, and clipboard modes should exit with `clipboard unavailable`. The prototype lacks an X11 implementation. This result says nothing about X11's clipboard capabilities. The prototype never falls back to Wayland under X11.

7. **Security and data.** Use only synthetic text. Do not paste secrets. The portal dialog and
   clipboard content belong to the desktop; they are not recorded in logs except `--show-content`,
   which is explicit.

Success conditions: the text is read and pasted; the observation detects a copy; the adapter does
not request input or screenshot; permission denied is reported; X11 and missing portal are not
presented as success. Any bug is recorded with the exact version of the desktop and message, does
not become a feasibility conclusion.

## 6. Local tests on Windows

Run on a console with OpenSSL in `PATH`. Write down each observation in the `RESULTS.md` matrix. Do
not paste real secrets.

1. **Inventory.**Run `--inventory` and save the `platform`, `windows_version`,`session_id`,`backend`
   and `limitation` lines.
2. **Publish a file.** Create a disposable folder and run:

   ```powershell
   .\build\win\Release\oneintwo-clipboard.exe --emit-samples "$env:TEMP\oneintwo-clipboard-test"
   .\build\win\Release\oneintwo-clipboard.exe --publish-file "$env:TEMP\oneintwo-clipboard-test\accents.txt" --show-content
   ```

   Expected: `written bytes=47 ...`, `readback=match`, and contents between bookmarks. Open Notepad
   and paste with Ctrl+V.
3. **Synthetic text by argument.** Execute `--publish-text "Hello 🌍 😀" --show-content` and paste into
   Notepad. In `cmd.exe` emoji can depend on the console; if there are problems, use
   `--publish-file`.
4. **Read.** Copy a known text from Notepad, run `--read-text` (without `--show-content`) and check
   that it only displays metadata. Repeat with `--read-text --show-content` and compare the content.
5. **Watch in the background.** Run `--watch --show-content`, switch to Notepad, copy a line and
   return to the console. Expected: a single `change #1` with `marker=absent`; should not be
   repeated indefinitely. Stop with Ctrl+C.
6. **Another focused application.**Repeat step 5 by copying from another program.The experiment
   window does not need to be active.
7. **No input capture.**This prototype does not include capture or injection of keyboard or mouse input; there
   is no mode to activate it.
8. **Expected failure case.** With another program keeping the clipboard open, `--read-text` can
   return `busy` after retry. Register the exact message; do not present it as a success.
9. **Non-textual format.** Copy an image and run `--watch`: no text change or clipboard should
   appear.

## 7. Connection between two teams

Actual scope of this delivery:

- **Windows ↔ Windows:** the Windows adapter is implemented; the TLS channel is compiled on Windows,
  but manual execution is still pending.
- **Windows ↔ Ubuntu:** Wayland adapter can leer/escribir if the portal grants permission. In Linux
  it is missing to compile TLS in this environment (there is no `libssl-dev`), so the full
  connection **is not verified**. Install `libssl-dev`, recompile and check `tls=openssl` before
  testing.
- **Ubuntu → Windows:**The Wayland adapter allows reading and publishing, but has not been executed;
  it is not declared working until manual testing.
- **Ubuntu → Ubuntu:** equally pending manual testing.

### 7.1 Certificates and fingerprints

Creating a folder outside the repository (or using `certificates/`, already ignored). Each computer
generates its own pair; `alice`/`bob` are examples.

```bash
mkdir -p certificates
cd certificates
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout alice.key -out alice.crt -days 7 \
  -subj "/CN=oneintwo-clipboard-alice"
openssl x509 -in alice.crt -noout -fingerprint -sha256
chmod 600 alice.key
```

On Windows, use the `openssl.exe` of `C:\Program Files\OpenSSL-Win64\bin` and protect the key with
user permissions. **Do not share `.key`.** Exchange only the public certificate `.crt` of the other
computer by a means that allows verification of its provenance (e.g., compare the fingerprint by
voice).The program prints `local_certificate_sha256` and `peer_certificate_sha256`; they must match
OpenSSL footprints.

### 7.2 Start server and client

In team A (server), with explicit port and firewall limited to the private network:

```powershell
.\build\win\Release\oneintwo-clipboard.exe --serve --bind 0.0.0.0 --port 47831 `
  --cert certificates\alice.crt --key certificates\alice.key `
  --peer-cert certificates\bob.crt --show-content
```

On team B (client Ubuntu):

```bash
./build/linux/oneintwo-clipboard --connect --host 192.168.1.50 --port 47831 \
  --cert certificates/bob.crt --key certificates/bob.key \
  --peer-cert certificates/alice.crt --show-content
```

Replace IP with server. There is no discovery or default port. If Ubuntu acts as a server, reverse
roles and check that `--inventory` displays `tls=openssl`; in this environment that compilation was
not verified.

When booting in Ubuntu, the portal adapter can open the dialog before the TLS channel is ready.
Approve the RemoteDesktop session with `devices=0`; do not select screen or enable input. If the
portal refuses access, the channel can continue to be set, but the Ubuntu end will reject text with
`unavailable`.

### 7.3 Transfer in both directions

1. With the session active, copy text to A and paste it into a B editor. In A `local copy captured
   ... sent` must appear; in B `received update` and `remote update applied`.
2. Repeat in sense B → A. In B the portal must have granted reading; in A the Windows clipboard is
   written natively.
3. To inject samples without relying on the editor, run on a second terminal of the sending
   computer:

   ```powershell
   .\build\win\Release\oneintwo-clipboard.exe --publish-file "$env:TEMP\oneintwo-clipboard-test\emoji.txt"
   ```

   The publication process does not write the internal marker, so the session observer treats it as
   a copy of the user and forwards it. Paste on the other team and check accents, emoji and line
   breaks.
4. Quickly copy three different texts. Expected: wins the last operation according to the logical
   counter; no loops; echoes appear as `echo suppressed`.
5. Stop one end with Ctrl+C. The other must end the session and report the shutdown.
6. Reconnect without copying anything. Expected: **no** old text is forwarded.
7. Test an incorrect certificate in `--peer-cert`: the handshake must fail by fixing the footprint.

The inter-team transfer does not alter P2's verdict on its own: it must be recorded if Ubuntu read,
published and observed the actual clipboard, and whether the portal's permission was granted or
denied.

## 8. Stop and restore

- `--watch`, `--serve` y `--connect`: Ctrl+C.
- `--read-text`, `--inventory`, `--self-test` y `--emit-samples`: terminan solos.
- `--publish-text` and `--publish-file`: in Windows they end after publishing; in Wayland they serve
  the offer for up to 60 seconds and then they finish. Ctrl+C allows you to finish earlier.
- Upon completion of Wayland mode, the portal session is closed. The prototype does not create
  services, registration keys, firewall rules or startup tasks.
- If a firewall rule was created for the test, remove it manually.
- Delete `certificates/` and sample folder. `build/` directories are local artifacts ignored.
- On Windows, the published text remains on the clipboard after exiting; replace it or delete it
  with normal desktop usage.
- In Wayland, the permission of the portal can be re-requested in each process; there is no
  persistent confidence to revoke in this prototype.

## 9. User matrix

Fill with real observations only. Do not convert a pending row to success by having compiled it. `No
ejecutado` is the correct initial value.

### 9.1 Local system testing

| System and Session | Read | Write | Watch in the background | No input capture | Observed permissions | Result |
| --- | --- | --- | --- | --- | --- | --- |
| Windows 10/11 (indicate version) | Pending | Pending | Pending | Pending | None of the portal; possible `busy` | Not run |
| Ubuntu + GNOME Wayland | Implemented; manual check pending | Implemented; manual check pending | Implemented; manual check pending | Yes per design; verify `devices=0` | RemoteDesktop + Clipboard; Share and “Allow Remote Interaction” | Not run |
| Ubuntu + X11 | Not implemented | Not implemented | Not implemented | Not applicable | None requested | Not run |

### 9.2 Inter-equipment testing

| Source → destination | Text | Expected result | Observed result |
| --- | --- | --- | --- |
| Windows → Windows | Bidireccional | Expected to work when both TLS and pasting succeed | Pending |
| Windows → Ubuntu | Bidireccional | Portal + TLS Linux; permiso manual | Pending |
| Ubuntu → Windows | Bidireccional | Portal + TLS Linux; permiso manual | Pending |
| Ubuntu → Ubuntu | Bidireccional | Portal at both ends; manual permission | Pending |

## 10. Constraints and primary sources

### Portal oficial

The Clipboard interface does not create a session of its own: it expands a session of RemoteDesktop
or InputCapture. The order implemented is:

```text
RemoteDesktop.CreateSession
Clipboard.RequestClipboard
RemoteDesktop.SelectDevices(types=0)
RemoteDesktop.Start
Clipboard.SetSelection / SelectionRead / SelectionWrite / SelectionWriteDone
```

Fuentes:

- Clipboard (v1):
  <https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Clipboard.html>
- RemoteDesktop (v2):
  <https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.RemoteDesktop.html>
- XML de Clipboard:
  <https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/main/data/org.freedesktop.portal.Clipboard.xml>
- XML de RemoteDesktop:
  <https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/main/data/org.freedesktop.portal.RemoteDesktop.xml>

`SelectDevices` documents `types` as a bitmask and its default value is “all”; This adapter explicitly sends `0`. `Start` returns `devices` and `clipboard_enabled`; the adapter requires `clipboard_enabled=true` and rejects any `devices` other than zero.

### GNOME 46: the switch that enables Share

En `xdg-desktop-portal-gnome` 46.0, `remotedesktopdialog.c`:

- `update_button_sensitivity()` only enables Share if a screen source is selected or if “Allow
  Remote Interaction” is active;
- `get_selected_device_types()` returns `dialog->device_types` when that switch is active, and `dialog->device_types` is the mask received by `SelectDevices`. With `types=0`, the result remains 0;
- `button_clicked()` reads “Allow Remote Clipboard” and sends it as `clipboard_enabled`;
  `clipboard_requested` makes that switch visible and activates.

Source:
<https://gitlab.gnome.org/GNOME/xdg-desktop-portal-gnome/-/raw/46.0/src/remotedesktopdialog.c> and
<https://gitlab.gnome.org/GNOME/xdg-desktop-portal-gnome/-/raw/46.0/src/clipboard.c>.

Therefore, in GNOME 46 the case “only clipboard” is a restriction of the dialog interface: you have
to activate “Allow Remote Interaction” in order for Share to be enabled, but **no entry is granted**
if the mask is 0. The adapter does not enable input or select screen; if a portal/escritorio returns
a mask other than zero, it refuses to continue. Other desktops and versions can present different
dialogues; they have not been tested here.

### X11

The X11 **backend is not implemented** in this prototype. The detection of a X11 session produces `linux-x11-unimplemented` and the Wayland portal is not used as a substitute. Implementing it would require `libX11`, selection property and `SelectionRequest`/`SelectionNotify`, in addition to a product decision on permits. It is not claimed that X11 makes it impossible.

## 11. Known limitations

- **Wayland not validated on desktop.** The code compiles and D-Bus messages are tested in memory,
  but no real portal has been opened.
- **TLS Linux not compiled here.** WSL does not have `libssl-dev`; it is not stated that the Linux
  compilation with OpenSSL works until repeated with the package.
- **Portal and permissions.** Dialogue, application identification, and desktop policy may deny or
  limit the session. There is no persistent confidence or reuse of a session between processes.
- **No input capture.**Do not call `Notify*` or `ConnectToEIS`; do not capture screen.The target is
  text only.
- ** Text only.** Images, enriched format, files and history are left out. 256 KiB limit; empty text
  and non-textual formats are ignored; NUL is rejected.
- **End of line.** The model uses LF; Windows writes CRLF and normalizes when reading. A loose CR is
  preserved.
- **Marker and echo.** The portal protocol does not have the private format of Windows. The engine
  suppresses the echo by content footprint during 1,5 s; an identical human copy in that window can
  be deleted. It is a heuristic.
- **Windows and privileges.** elevadas/UIPI applications are not covered. Another process can keep
  the clipboard occupied; it is retryed and then `busy` reported.
- **No waiting times TLS.** A pair that connects and does not complete the handshake can block
  acceptance up to Ctrl+C.

## 12. How to register a bug

Write down: date, system and session, version of Ubuntu/GNOME, version of
`xdg-desktop-portal`/backend, exact command, expected result, observed result, output code and
relevant log lines. Do not include private clipboard content or keys. Add the corresponding row in
`RESULTS.md`; a documented limitation is not an observed fault, and a correct compilation does not
show access to the desktop.
