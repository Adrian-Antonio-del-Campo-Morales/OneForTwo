# Results — Clipboard

## 1st session Windows ↔ Ubuntu closed — 5 November 2026

Windows server with `780f8c5` correction, PID 159244, listening to 192.168.1.147:47831 and fixing
the reported Ubuntu certificate. Ubuntu agent reports `devices=0` and user confirmation of correct
pasting in both directions. Effective review and customer log must accompany the comparison: they
are not deduced from the protocol.

Complete Windows record, metadata only and without clipboard text:
[windows-session-01.log](lan/evidence/windows-session-01.log).

| Relative time | Windows evidence |
| --- | --- |
| Inicio | Certificate Ubuntu SHA-256 correct, TLS accepted and `session ready`, generation `808fb17411cd9893` |
| +129/+130 ms | Ubuntu update of 48 bytes received and applied; Windows deletes its own notification by bookmark |
| +145201 ms | Local copy of 147 bytes sent with logic 2 |
| +145302 ms | 147 byte Ubuntu update, logical 2, rejected as prior to current status |
| +145304 ms | Ubuntu confirms applied the Windows source operation with logic 2 |
| +145478/+145583 ms | Second local capture of 147 bytes with logic 3; Ubuntu confirms application |
| +184345/+184346 ms | Windows sends 145 bytes with logic 4 and receives 145 bytes of Ubuntu origin with logic 4; rejects the latter as previous |
| +184460 ms | Ubuntu confirms applied the Windows source operation with logic 4 |
| +203392/+203393 ms | Reading TLS ends with `unexpected eof`; session ends and server waits again |

The EOF is consistent with the customer's closure communicated by the user; it does not demonstrate
an orderly TLS closure or itself constitutes an authentication failure. The log displays a single
session and does not prove reconnection.

**Result:** TLS/saludo and two-way glue confirmed for this session. **The echo policy is not considered correct.**There are finite re-emissions and two local catches of 147 bytes separated by about 277 ms. Without correlating user events and actions at both ends, its cause cannot be attributed. The initial update of 48 bytes coincides with the previous selection indicated by agent Ubuntu; the start-up policy is not considered validated. The capabilities field received by Windows says `independent_of_input=no`; This is retained in the log and must be interpreted with the Ubuntu report, which confirms `devices=0`, without concluding that entry has been granted.

Upon receipt of customer closure confirmation, only the identified Windows process was stopped. Checks: `StillRunning=false`, zero listeners in 192.168.1.147:47831. Temporary rule `OneInTwo-Clipboard-LAN-47831` was withdrawn; the high assistant confirmed `RulePresent=false`. Logs and closure were filed in `.local-state/lan-clipboard/windows/session-01/`. There is no active server and no other attempt has been initiated.

Earrings: comparison of both logs, actual rejection of incorrect certificate and reconnection, the
latter two in another coordinated session. They are not repeated or automatically overtaken from
this result.

## Windows Server Boot and PEM Correction — November 5, 2026

After incorporating `e2af22b` and verifying the public footprint Ubuntu communicated by the user, an attempt was made to boot the agreed server. The first process ended with output 1, before listening, by `OPENSSL_Uplink: no OPENSSL_Applink`. The two PEM readings of the transport passed `FILE*` of the application to OpenSSL. They were replaced by a C++ readout and `PEM_read_bio_X509` on a BIO of memory, without changing authentication, protocol or certificates.

Only clipboard was reconstructed and it was verified:

- `clipboard_tls_certificate_test` with the Windows public certificate and its SHA-256: correct
  reading and fingerprinting; absent and non-PEM file rejection.
- The same regression with the public certificate Ubuntu: exceeded.
- `oneintwo-clipboard.exe --self-test`: **149 checks, 0 failures**.
- Second real start: active process, `listening=192.168.1.147:47831` and `netstat` showing
  `LISTENING` for the PID 159244.

The server uses the local Windows private key and fixes the public certificate Ubuntu; identities have not been regenerated. Logs ignored at `.local-state/lan-clipboard/windows/`: `server.log`, `server-error.log` and `server-error-before-pem-fix.log`. Compilation at `build/windows-ready/build-pem-fix.log`.

The new regression executable is built when there is OpenSSL. From the experiment folder can be run
without network or desktop:

```powershell
.\build\windows-ready\Release\clipboard_tls_certificate_test.exe lan/public/windows.pem lan/public/windows.sha256 lan/samples/windows.txt
```

The boot confirms listening and loading of certificates, no handshake or synchronization. The server
is waiting for the Ubuntu client; the connection results and copies will be recorded after observing
them. The common PEM reading correction has not yet been compiled in Ubuntu.

## Preparation LAN Ubuntu — 5 November 2026

`origin/validation/windows` was downloaded and progress was made without conflict from `2ae444d` to
`6c2ec07600454faaf68bc2752ff492ec6c01b970`. The two commissions add evidence from Windows and LAN
preparation; they do not change the validated C++. Self-tests were not recompiled or repeated.

Command: `env -u LD_LIBRARY_PATH /usr/bin/python3 -B experiments/clipboard/lan/prepare.py prepare --name ubuntu`, output 0. 14-day identity generated, key retained only in `.local-state/lan-clipboard/ubuntu/private.key`, mode 0600 and excluded from Git. Only `lan/public/ubuntu.pem` and `ubuntu.sha256` are exported. Footprint DER SHA-256: `1eaf59dcb2489296c37ac5aa755b03a70637f47371d4bdfa0aabd69e36685c1e`. The fingerprint of the Windows certificate also matches its file SHA-256; this checks coherence of the package, does not replace the confirmation between operators. IP LAN itself consulted with `ip -brief -4 address show scope global` and route by default; communicated to the user without publishing it in Git.

The prototype was not executed, the clipboard was not accessed, no test network was opened and no
firewall was changed. Earrings: shared revision with Windows, its IP LAN, fingerprint confirmation
and coordinated booting in 47831/TCP.

## Windows Reconstruction of Ubuntu Delivery — November 5, 2026

Verified sources: `2ae444df56689dbaebe9b796daca26a55bd02917`, received from
`origin/validation/ubuntu`. Check performed on the local branch `validation/windows`, without
modifying sources or repeating Ubuntu tests.

CMake from Visual Studio 18 2026, `Visual Studio 18 2026` generator, x64, Release and OpenSSL
configuration enabled. Reconstruction completed, output 0. Binary:
`build/windows-ready/Release/oneintwo-clipboard.exe`.

Self-test Windows: **149 checks, 0 bugs**, output 0. The previous binary was executed with
`--self-test`, without desktop or network. The main executable also finished correctly with
`--help`. The clipboard was not accessed, portals were opened, capturaron/inyectaron devices were
opened and no test network was initiated.

Unknown local evidence: `build/windows-ready/configure-ubuntu-sync.log`,
`build-ubuntu-sync.log`,`self-test-ubuntu-sync.log` and `help-ubuntu-sync.log`. This evidence closes
the Windows reconstruction of this revision; it does not demonstrate the actual functioning of the
desktop or between computers. The local evidence Ubuntu and its limits are preserved below.

## Closure of the local isolated copy case — 5 November 2026

With the same binary and Git `450cbb2e340a381bda01274eb86df0e05934e822` base plus the previous local
corrections, `--watch` was executed on the real portal. The user authorized and confirmed a single
copy. An auxiliary window with disposable sample recorded **a single action `copy-clipboard`** at
**19:20:24.324 CET**; the observer did not change or filter its results. The known sample has 62
bytes, `af28ca2273e2d495` footprint.

`watch.log` contains an earlier notification of other content (183 bytes) and ** a single
notification of the sample** (62 bytes), with no new entries during the following **12 s**.
`devices=0`, `clipboard_enabled=true`. The user confirmed "a single copy and finished observation".
Local case exceeded; it does not demonstrate absence of echoes between teams.

Evidence ignored in `manual-output/local-final-20261105/`: `watch_once.py`, `command.json`, `watch.log`, `copy-actions.json`, `process.json`. The aid executed `timeout --signal=TERM --kill-after=1s 90s experiments/clipboard/build/ubuntu/oneintwo-clipboard --watch`, with the local libraries only on the son. The group stopped its own after observation at **19:20:36.329 CET**; the supervisor returned 124 upon receiving the external signal, not by an error of the portal. No tests were added nor read, pasted, denied or self-tests already valid.

## Local Desktop Validation — November 5, 2026

The user resumed testing and confirmed his presence. He worked only with synthetic texts in GNOME
Text Editor, without initiating capture or injection input or test network. The agent executed the
commands and read the logs; the actions and pasting of the editor were **user observations**,
because this session does not offer visual control of the native windows.

Base revision: `450cbb2e340a381bda01274eb86df0e05934e822`, `validation/ubuntu` branch, plus the
local changes described here and the TLS header already documented. No commit or post. Environment
and dependencies: those of the initial compilation below, now in `.local-state/ubuntu-validation/`.

### Observed results

| Caso | Evidencia | Result |
| --- | --- | --- |
| Clipboard dialogue only | User: Share initially disabled; enabled by enabling Allow Remote Interaction and Allow Remote Clipboard, without selecting screen | Observed on GNOME 50 |
| First read request | The dialogue exhausted the external limit of 90 s; exit 137 after the intended forced termination | He did not evaluate reading; it was repeated |
| Read before the fix | After authorisation, `cannot read portal selection pipe: Resource temporarily unavailable`, exit 2 | Actual, corrected and repeated failure |
| Read after the fix | `read-03.log`: 61 bytes, 3 lines, accents and emoji; comparison with the correct editor sample, output 0 | Superado |
| First publication | `written` and `readback=match`, output 0; user reports that it does not paste the expected text | Actual failure; internal readback does not test pasted |
| Corrected publication | `publish-02.log`: 57 bytes, 3 lines; user confirms that pastes the three correct lines into new document; closes to 60 s, output 0 | Overtaken with user observation |
| Permissions granted | Publication and observation: `portal_start clipboard_enabled=true devices=0` | Verified by actual portal response |
| Background observation | User copy with focused editor; `watch-03.log` displays changes of 191 bytes/8 lines and 207 bytes/9 lines, without displaying content | Verified detection; user clarifies that he pressed Ctrl+C several times |
| A notification by copy/no echoes | There are repeated fingerprint notifications and several deliberate copies; no single isolated pulse or synchronized session was measured | Exact correspondence and absence of remote echoes remain unverified |
| Permission denied | User press Cancel; `clipboard unavailable: the portal start request was cancelled or failed (response 1)`, output 2 | Passed. No clipboard read or fallback backend |

The saved reading sample has an additional final LF to the text selected in the editor. The
comparison accepted exactly the three lines expected without that LF (61 bytes), without altering
accents, emoji or inner jumps.

### Required corrections and verification

1. `SelectionRead` returned a non-blocking descriptor. The pipe reading was extracted to a function that uses `poll`, tolerates EINTR/EAGAIN, retains the limit of 256 KiB and waits for datos/EOF with a total term of 30 s. The new isolated regression uses synthetic pipers for deferred Unicode data, EOF, empty pipe and timeout, without bus or clipboard. Battery changes from **151/0 to 153/0**; `self-test-final.log` records the result of the current state.
2. The publishing mode ended after checking your internal copy and closed the session before the
   editor could request the data. The CLI now handles portal events for up to 60 s after
   authorizing, or until another selection replaces it. Windows publishing behavior is preserved.
   The manual and help explains the pasting interval.
3. The `--watch` metadata without `--show-content` remained in the buffer when redirecting the output. They were emptied immediately after printing each change. Diagnostics of permissions and notifications were added, without content. `watch-02.log` confirms receipt of notifications; `watch-03.log` already allows observing changes in real time. Duplicate filtering was not changed to convert the Ctrl+C repeat into an artificial success.

### Commands, logs, and shutdown

From the root, load `source .local-state/ubuntu-validation/env.sh`; then, from
`experiments/clipboard`, the real modes were executed:

```bash
./build/ubuntu/oneintwo-clipboard --read-text --show-content
./build/ubuntu/oneintwo-clipboard --publish-file RUTA_MUESTRA/publicacion.txt --show-content
./build/ubuntu/oneintwo-clipboard --watch
./build/ubuntu/oneintwo-clipboard --read-text   # Cancel the dialog
./build/ubuntu/oneintwo-clipboard --self-test
```

The unknown local samples and logs are `manual-output/ubuntu-local-20261105-de47_iib/`: `lectura.txt`, `publicacion.txt`, `read.log`, `read-02.log`, `read-03.log`, `publish-01.log`, `publish-02.log`, `watch-01.log`, `watch-02.log`, `watch-03.log`, `denied-01.log`, reconstruction logs and `self-test-final.log`. The reconstruction was done with `cmake --build experiments/clipboard/build/ubuntu -j4`, exit 0. The lecturas/publicaciones were wrapped in `timeout --signal=TERM --kill-after=3s` with external limits of 180/240 s; the first attempt used INT with 90 s and KILL after 5 s. The observers were monitored for 180 s and stopped at the end of the case (supervisory exits 130/124; do not indicate rejection of the portal). He was not an active observer as he went on to the next experiment. The editor and samples are kept for the user.

Windows and two-way synchronization are still pending coordination. The following historical
evidence is not modified and the product is not integrated.

## Initial compilation of native Ubuntu — November 5, 2026

**Current result: compilation with portal and TLS completed; 151 checks, 0 bugs, output 0.** The
user limited this session to compilation and isolated tests. No permissions, clipboards or test
connection was opened. The evidence of Windows and WSL from the later sections is preserved as
historical; their Linux build blocks are updated here.

### Environment and review

- Directorio: `/home/pholtergate/DEV/OneForTwo`.
- Git Base: `450cbb2e340a381bda01274eb86df0e05934e822`. Local branch `validation/ubuntu`; no commit
  or release of these changes.
- Verified code: that base **plus the local correction** from `<signal.h>` to `<csignal>` in
  `src/net/tls_channel.cpp`. SHA-256 of the corrected file:
  `3be6de1a8ec6f3fff237ad3392820066754360f150f05bc6185e631a3effd842`.
- Local inventory date: 2026-11-05, `Europe/Madrid` area.
- Ubuntu 26.04.1 LTS, Linux 7.0.0-42-generic, x86_64; inherited environment `XDG_SESSION_TYPE=wayland`,
  `XDG_CURRENT_DESKTOP=ubuntu:GNOME`, `WAYLAND_DISPLAY=wayland-0`, `DISPLAY=:0`.
- Installed packages: GNOME Shell `50.1-0ubuntu1.5`, Mutter `50.1-0ubuntu2.4`, Nautilus `1:50.2.2-0ubuntu0.2`, xdg-desktop-portal `1.21.1+ds-1ubuntu3.1`, GNOME backend `50.0-0ubuntu1`, and GTK backend `1.15.3-2ubuntu1`. These are package versions. The bus was not queried, and compositor interfaces and permissions remain unverified.
- GCC/G++ `15.2.0-16ubuntu1`, GNU Make 4.4.1, pkg-config 2.5.1; CMake local 4.2.3. libdbus 1.16.2
  (`1.16.2-2ubuntu4`) y OpenSSL 3.5.5 (`3.5.5-1ubuntu3.7`).

Inventory obtained with `git status --short --branch`, `git rev-parse HEAD`, `cat /etc/os-release`, `uname -srmo`, `printenv`, `c++ --version`, `pkg-config --version`, `openssl version`, `dpkg-query -W` and `pkg-config --modversion dbus-1 openssl`. Portal support is not inferred from the Ubuntu version.

### Local dependencies for this validation

CMake and the dbus development packages, OpenSSL, GTK30, GLib/GIO and libei were missing. Packages and their dependencies were downloaded, **without installation**, in `/tmp/onefortwo-ubuntu-validation-20261105/debs` and extracted by `dpkg-deb -x` in the subdirectory `root`. The process maintained UID 1000, without `sudo`, global changes or firewall changes. Executing libraries already installed through links within that local root are reused; absent auxiliary libraries are also extracted there.

Then, at the user's request, **all this preparation was moved to `/home/pholtergate/DEV/OneForTwo/.local-state/ubuntu-validation/`**, including the 101 `.deb` packages, libraries, tools, configuration and download logs (approximately 301 MiB). The folder is excluded by rule `/.local-state/` existing in `.gitignore`. Links to system libraries remain valid; none points to `/tmp`.

Completed initial download command (historic, output 0, 101 packages, 56,1 MB):

```bash
mkdir -p /tmp/onefortwo-ubuntu-validation-20261105/{debs,root}
apt-get --download-only --yes --no-install-recommends \
  -o Debug::NoLocking=1 -o Acquire::Retries=0 -o Acquire::http::Timeout=10 \
  -o Dir::Cache::archives=/tmp/onefortwo-ubuntu-validation-20261105/debs \
  install cmake libdbus-1-dev libssl-dev libgtk-3-dev libglib2.0-dev libei-dev
for deb in /tmp/onefortwo-ubuntu-validation-20261105/debs/*.deb; do
  dpkg-deb -x "$deb" /tmp/onefortwo-ubuntu-validation-20261105/root
done
```

The first attempt in the sandbox failed with `Temporary failure resolving` (exit 100); the repeat
with enabled network access worked. Creating the branch also required write access to `.git` outside
the sandbox; user privileges were not elevated.

The current local `.local-state/ubuntu-validation/env.sh` file configures the following; it must be
loaded with `source` from Bash before compiling or repeating the tests. Calculates the root from its
own location:

```bash
export OIT_VALIDATION_DEPS="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/root" && pwd)"
export PATH="$OIT_VALIDATION_DEPS/usr/bin:$PATH"
export LD_LIBRARY_PATH="$OIT_VALIDATION_DEPS/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LIBRARY_PATH="$OIT_VALIDATION_DEPS/usr/lib/x86_64-linux-gnu${LIBRARY_PATH:+:$LIBRARY_PATH}"
export PKG_CONFIG_SYSROOT_DIR="$OIT_VALIDATION_DEPS"
export PKG_CONFIG_LIBDIR="$OIT_VALIDATION_DEPS/usr/lib/x86_64-linux-gnu/pkgconfig:$OIT_VALIDATION_DEPS/usr/share/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig"
export CMAKE_PREFIX_PATH="$OIT_VALIDATION_DEPS/usr"
export CPATH="$OIT_VALIDATION_DEPS/usr/include/x86_64-linux-gnu${CPATH:+:$CPATH}"
```

The preparation no longer depends on `/tmp` being preserved. It remains unique to this Ubuntu
installation: it uses run libraries installed in the system. Dependencies, binaries or environment
scripts have not been added to version control. `.deb` are conserved to be able to repeat the
extraction. The other experiments use this same preparation, without linking to each other.

### Compilation and self-test observed

From the root of the repository:

```bash
source .local-state/ubuntu-validation/env.sh
cmake --fresh -S experiments/clipboard -B experiments/clipboard/build/ubuntu \
  -DCMAKE_BUILD_TYPE=Release -DOIT_ENABLE_PORTAL=ON -DOIT_ENABLE_PEER=ON \
  -DOPENSSL_ROOT_DIR="$OIT_VALIDATION_DEPS/usr"
cmake --build experiments/clipboard/build/ubuntu -j4
cd experiments/clipboard
./build/ubuntu/oneintwo-clipboard --self-test
./build/ubuntu/oneintwo-clipboard --inventory
```

Final configuration, build, self-test, and inventory exited with code 0. After relocating the dependencies, the cache was regenerated with `--fresh`, the executable was rebuilt with the persistent paths, and the self-test passed again at **151/0**. The build included `portal_clipboard.cpp` and transport sources `tls_channel.cpp` and `peer.cpp`. Final definitions include `OIT_HAVE_PORTAL=1` and `OIT_HAVE_OPENSSL=1`. The ELF binary links `libdbus-1.so.3`, `libssl.so.3`, and `libcrypto.so.3`, and `ldd` reports no missing libraries. The read-only inventory reports `portal_client=compiled`, `tls=openssl`, and `backend=xdg-desktop-portal-clipboard`. It reports capabilities without requesting permissions.

```text
checks=151 failures=0
SELF-TEST RESULT: PASS
```

Incidences resolved before the end result:

1. `openssl/opensslconf.h: No such file or directory`: the extracted multiarch headers had to be
   displayed; `CPATH` was corrected, without editing the code.
2. `error: 'signal' is not a member of 'std'`: the Linux transport included `<signal.h>` and called `std::signal`. Only that header was changed to `<csignal>`; the reconstruction and the 151 checks passed.

Ignored local logs in `build/ubuntu/` include `configure.log`, `build.log`, `build-initial.log`, `build-tls-failure.log`, `self-test.log`, `inventory.log`, `libraries.log`, `dependencies-download.log`, `dependencies-dns-failure.log`, `dependencies-packages.tsv`, and a copy of `dependencies-env.sh`. That copy preserves the original temporary configuration for evidence. For development, load the current file from `.local-state`. The same build directory records the post-move run in `configure-persistent.log`, `build-persistent.log`, `self-test-persistent.log`, `inventory-persistent.log`, and `libraries-persistent.log`.

### Desktop, two computers and next step

Reading, publishing and pasting, background observation, denied permission and actual concession of
`devices=0`: ** pending by user decision**. The experience of GNOME 50 has not been measured or a
handshake validated.

Before resuming: present user, editor with synthetic text, prepared output and a single active
experiment, following MANUAL §5. Before connecting Windows, agree on a revision that includes local
correction, reconstruction of both ends, addresses and port 47831/TCP, roles, public certificates and fingerprints
certificates, duration and observation. No contact or connection with Windows; no key has been
generated. The publication by Git remains to be agreed.

## Historical evidence preserved — previous delivery and review

Date: November 4, 2026.

Documentary review: November 5, 2026. Evidence of the agent's delivery is preserved; this review
does not repeat compilations or evidence of clipboard or network. The consolidated status is shown
in [experiments/README.md](../README.md).

Status: Windows adapter and Linux/Wayland adapter implemented and compiled. Wayland adapter uses
`xdg-desktop-portal` RemoteDesktop + Clipboard, asks for zero input devices and does not capture
keyboard, mouse or screen. Insulated auto-tests pass with **151 checks, 0 bugs** in the Linux
compilation with portal and **149, 0 bugs ** in Windows and in the compilation without portal. **No
tests were run with the actual clipboard, the portal dialog or two computers.** P2 is still pending
manual validation, but no longer due to lack of Wayland adapter.

## 1. Implementation and compilation

| Elemento | Estado | Evidence or blocker |
| --- | --- | --- |
| Text model, logical order and protocol | Implementado | `src/model.*`, `src/engine.*`, `src/protocol.*` |
| Native Windows adapter | Implemented and built | `src/platform/windows_clipboard.cpp`; MSVC/VS 2026; real clipboard check pending |
| Adaptador Linux/Wayland | Implemented and compiled with libdbus | `src/platform/portal_clipboard.cpp`; RemoteDesktop + Clipboard; `devices=0`; not validated in graphical session |
| Linux Backend Selection | Implementado | `src/platform/linux_clipboard.cpp`; Wayland → portal; X11 → explicit limitation, no failback |
| Transport TLS between equipment | Implemented and compiled in Windows | OpenSSL 3.5; `src/net/tls_channel.cpp`, `src/net/peer.cpp`; without proof of execution |
| TLS transport on Linux | **Not built** | WSL does not have `libssl-dev`; the process was not upgraded and packages were not installed globally |
| CLI and explicit modes | Implementado | `--help`, `--inventory`, `--self-test`, `--read-text`, `--publish-*`, `--watch`, `--emit-samples`, `--serve`, `--connect` |
| Isolated self-tests | Run and passed | 151 checks Linux with portal; 149 checks Windows and Linux without portal |
| `MANUAL.md` | Entregado | Separate compilation, permissions, commands and manual matrix |
| Windows Compilation | Verificada | CMake 4.3.1-msvc1, VS generator 18 2026 x64, MSVC 19.51.36257; output 0 |
| Linux compilation with portal | Verificada | Ubuntu 20.04.6 WSL, g++ 9.4.0, CMake 3.25.2, libdbus 1.12.16; output 0 |
| Linux compilation without portal | Verificada | `-DOIT_ENABLE_PORTAL=OFF`; 149 checks, output 0 |
| Manual desktop testing/two computers | Earrings | This agent did not have to leer/escribir the actual clipboard or open network |

## 2. Environment and evidence of compilation

### Windows

- OS observed by `--inventory`: Windows 10.0.19045, session 1.
- Compiler: MSVC 19.51.36257.0 (Visual Studio 2026 Build Tools).
- CMake: 4.3.1-msvc1 included with Visual Studio.
- OpenSSL: 3.5 (Win64) en `C:\Program Files\OpenSSL-Win64`.
- Comando:

```powershell
$cmake = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake -S . -B build/win -G "Visual Studio 18 2026" -A x64 -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64"
& $cmake --build build/win --config Release
```

Result: compilation completed, output code 0; binary `build\win\Release\oneintwo-clipboard.exe`. Notices C4996 of `fopen` in `tls_channel.cpp`; It's not mistakes. The execution required `C:\Program Files\OpenSSL-Win64\bin` at `PATH` to load DLL.

### Linux with portal support

- Ubuntu 20.04.6 LTS in WSL2, g++ 9.4.0, CMake 3.25.2, 16 processors.
- `libdbus-1-dev` 1.12.16-2ubuntu2.3 downloaded as `.deb` and extracted in `/tmp/oit-deps/dbus`; it
  was not installed globally. The running library was `/lib/x86_64-linux-gnu/libdbus-1.so.3`.
- `pkg-config` did not have the `.pc` of local extraction; the two header directories (including the multiarch `dbus-arch-deps.h`) and the library were explicitly passed. In a normal Ubuntu, `libdbus-1-dev` and `pkg-config` allow to configure without manual routes.
- Real command of the verified compilation:

```bash
cmake -S . -B build/linux-portal2 -DCMAKE_BUILD_TYPE=Release \
  -DDBUS_INCLUDE_DIR=/tmp/oit-deps/dbus/usr/include/dbus-1.0 \
  -DDBUS_ARCH_INCLUDE_DIR=/tmp/oit-deps/dbus/usr/lib/x86_64-linux-gnu/dbus-1.0/include \
  -DDBUS_LIBRARY=/lib/x86_64-linux-gnu/libdbus-1.so.3
cmake --build build/linux-portal2 -j8
```

The configuration showed:

```text
-- xdg-desktop-portal backend enabled (libdbus: /lib/x86_64-linux-gnu/libdbus-1.so.3)
```

and the compilation ended with code 0.`--inventory` in WSL shows `portal_client=compiled`, but there is no graphical session and the backend remains `linux-no-graphical-session`; this only verifies the compilation.

### Linux without portal support

```bash
cmake -S . -B build/linux-noportal -DCMAKE_BUILD_TYPE=Release -DOIT_ENABLE_PORTAL=OFF
cmake --build build/linux-noportal -j8
./build/linux-noportal/oneintwo-clipboard --self-test
```

Result: code 0 and `checks=149 failures=0`. Confirms that the kernel, Windows does not apply, and
the Linux limitation adapter continues to compile when libdbus is not available. The backend is
reported as not compiled, without simulating reading or writing.

### Linux TLS limitation

WSL does not have `libssl-dev`; `apt-get download libssl-dev` failed with 404 for the candidate version. Nothing was installed. Consequently, the verified Linux compilation uses `peer_unavailable.cpp`, `tls=unavailable`, and it cannot be claimed that the handshake TLS Linux works. To close this part requires `sudo apt install libssl-dev` in Ubuntu and repeat the compilation; it is a limitation of the verification environment, not a design decision.

## 3. Isolated self-tests

Command: `oneintwo-clipboard --self-test` (Windows and Linux). Linux output with portal:

```text
PASS text_validation
PASS line_endings
PASS ordering_and_ids
PASS fingerprint
PASS protocol_roundtrip_and_limits
PASS portal_message_helpers
PASS engine_local_capture
PASS engine_remote_and_echo
PASS engine_rejections
PASS engine_concurrency
PASS engine_session_isolation
checks=151 failures=0
SELF-TEST RESULT: PASS
```

Windows:

```text
checks=149 failures=0
SELF-TEST RESULT: PASS
```

Actual coverage of these tests:

- UTF-8 strict: accents, emoji, truncations, overlong sequences, substitutes, NUL and empty text.
- 256 KiB limit, CRLF→LF standardization and conversion back to Windows.
- Logical order Lamport with stable tiebreaker by source identifier; convergence with concurrent
  copies.
- Own echo per marker and per content footprint; a later human copy outside the window is sent.
- Duplicate, backward operations and updates from another generation.
- Invalid text rejection or too large without writing the clipboard.
- Step: cero/excesivas lengths, truncation, leftover bytes, unknown type and incorrect protocol
  version.
- Absence of automatic forwarding by creating a new session and rejecting an operation from the
  previous generation.
- Engine without adapter: does not declare success; reports `unavailable`.
- Portal, only in memory: building and parsing of `a{sv}`, priority MIME, `Response`, `SelectionOwnerChanged` and `SelectionTransfer`; does not open the bus.

The clipboard is not initialized or any socket is opened during `--self-test`. The adapter used for
the engine is `FakePort`, defined in `tests/selftest.cpp`.

Additional checks performed (no desktop):

- `--inventory` en Windows: `backend=win32-clipboard`, `read=yes write=yes watch=yes
  independent_of_input=yes`, `tls=openssl`.
- `--inventory` on Linux with portal, WSL without graphic session: `portal_client=compiled`,
  `backend=linux-no-graphical-session`,`read=no write=no watch=no`, `tls=unavailable`.
- `--inventory` on Linux without portal: `portal_client=unavailable`, limitation backend.
- No arguments: it only helps; the clipboard is not initialized.
- `--emit-samples` wrote four valid UTF-8 files of 47, 55, 49 and 64893 bytes; they were validated
  with Python and deleted.
- `--version` and arguments errors return what is expected.

## 4. Verified and pending checks

| Affirmation | Estado |
| --- | --- |
| Windows prototype compiles with MSVC and OpenSSL | Verificado |
| The kernel and Linux adapter compile in Ubuntu 20.04 WSL | Verificado |
| Wayland adapter compiles with libdbus and D-Bus flow is implemented | Verified by code compilation and revision |
| 151 isolated checks pass on Linux with portal | Verificado |
| 149 isolated checks pass on Windows and Linux without portal | Verificado |
| The policy of echo, duplicate, order and generations works with doubles | Verificado |
| Protocol rejects malformed plots within proven limits | Verificado |
| Windows reads, writes, and watches the real clipboard | **Manual check pending** |
| Wayland reads, writes, and watches the real clipboard | **Manual check pending**; no graphic session here |
| The portal dialog grants `clipboard_enabled=true` and `devices=0` | **Manual check pending**; adapter rejects any mask other than zero |
| The observation continues with the inactive experiment window | **Manual check pending** |
| The TLS handshake and certificate fixing work in progress | **Manual check pending** |
| Windows ↔ Windows transfers text in both directions | **Manual check pending** |
| Windows ↔ Ubuntu transfers text | **Manual check pending**; in addition to compiling TLS Linux in this environment |
| Ubuntu lee/escribe clipboard via portal | ** Implemented; manual pending** |
| X11 reads and writes the clipboard | **Not implemented** in this prototype |
| Wayland allows a clipboard-only session without input | **Restriction of known IU**: GNOME 46 requires enabling “Allow Remote Interaction” or a screen; with `types=0` does not grant entry. Confirm missing on real desktop |
| The Linux compilation with OpenSSL works | **Unverified** (missing `libssl-dev` in the environment) |

## 5. Permissions, capabilities, and constraints

### Implemented flow

`org.freedesktop.portal.RemoteDesktop` creates the session and presents the dialog;
`org.freedesktop.portal.Clipboard` expands that session. The order is:

```text
CreateSession
RequestClipboard
SelectDevices(types=0)
Start
SetSelection / SelectionRead / SelectionWrite / SelectionWriteDone
```

`SelectDevices` documents `types` as bitmask with default value “all”; `0` is sent to avoid ordering keyboard, mouse or touch. `Start` returns `clipboard_enabled` and `devices`. The adapter:

- exige `clipboard_enabled=true`;
- rejects any `devices` other than zero and closes the session;
- never calls `Notify*` or `ConnectToEIS`;
- does not select screen fonts;
- does not fall back to X11.

### GNOME 46

In `xdg-desktop-portal-gnome` 46.0, the Share dialog only enables if “Allow Remote Interaction” is active or there is a selected screen source. `get_selected_device_types()` returns the `SelectDevices` mask when the switch is active; with `types=0` it continues to return zero. Therefore:

- is a **interface restriction**: the switch must be activated to enable Share;
- is not an entry permit in this case, because the mask is 0;
- If a portal version ignored the mask and granted devices, the adapter would refuse to continue.

The actual dialog has not been tested. Other versions and desktops may vary; it does not generalize
to “Wayland blocks the clipboard”.

### X11

There is no X11 backend implemented. In a X11 session the adapter returns `linux-x11-unimplemented`
and does not attempt the Wayland portal. It is an absence of implementation, not a conclusion about
the capabilities of X11.

### Environmental constraints

There is no graphical session in WSL; opening the portal or reading the actual clipboard requires a
real Ubuntu session. The lack of that environment does not prove unfeasibility; it prevents running
manual validation.

## 6. Manual test matrix

All rows are not running. Complete from `MANUAL.md`; do not convert a build into a desktop test.

| Caso | Exact environment | Esperado | Observado | Result |
| --- | --- | --- | --- | --- |
| Read copied text | Windows 10/11 | Correct metadata; content only with `--show-content` | Not run | Pending |
| Post file and paste | Windows 10/11 + Notepad | `readback=match`; correct text pasted | Not run | Pending |
| Watch in the background | Windows 10/11, inactive window | One change for copy, no echo. | Not run | Pending |
| Copy an image or other non-text format | Windows 10/11 | No text change; no clipboard deleted | Not run | Pending |
| Read via portal | Ubuntu + GNOME Wayland | Dialogue; `clipboard_enabled=true`, `devices=0`; correct text | Not run | Pending |
| Post and paste via portal | Ubuntu + GNOME Wayland | Text pasted; no input capture | Not run | Pending |
| Watch in the background | Ubuntu + GNOME Wayland | Change when copying to another app; no echo | Not run | Pending |
| Cancel permission request | Ubuntu + GNOME Wayland | `Denied`/`unavailable`, without fallback | Not run | Pending |
| X11 | Ubuntu + X11 | `linux-x11-unimplemented`, without fallback | Not run | Not implemented |
| TLS with the correct certificate | Windows ↔ Windows | Handshake succeeds and fingerprints match | Not run | Pending |
| TLS with the wrong certificate | Windows ↔ Windows | Handshake rejected because the pin does not match | Not run | Pending |
| Bidirectional Unicode transfer | Windows ↔ Windows | Accents, emoji and correct lines | Not run | Pending |
| Quick copies | Windows ↔ Windows | Wins the last one; no loops | Not run | Pending |
| Disconnection and reconnection | Windows ↔ Windows | No old text is forwarded; new copy yes | Not run | Pending |
| Windows ↔ Ubuntu | Windows ↔ Ubuntu | Portal + TLS; manual permission | Not run | Pending |
| Ubuntu → Windows | Ubuntu → Windows | Portal + TLS; manual permission | Not run | Pending |

## 7. Decisions and dependencies

- C++17, CMake independent within the experiment; not dependent on root or product CMake.
- Win32 (`CF_UNICODETEXT`, `AddClipboardFormatListener`, message-only hidden window, private
  bookmark format) is not captured or injected.
- Wayland: `xdg-desktop-portal` RemoteDesktop + Clipboard via libdbus. Libdbus was chosen not to add
  GLib/GIO to the prototype. X11 was not used as fallback.
- `SelectDevices(types=0)` and `devices == 0` check: The clipboard does not depend on input control.
- OpenSSL 3.5 (Apchap license 2.0) for TLS 1.2 or higher; the pair certificate is set to SHA-256. CA, host name, account or discovery is not used. No cryptography of its own has been implemented.
- Text limit: 256 KiB; UTF-8 strict; NUL and empty text rejected; metadata with bytes, code points,
  lines and 64-bit FNV-1a.
- Concurrence: Lamport logic counter; tie by major source identifier; new generation per connection;
  no forwarding of the previous clipboard.
- Eco: `oit1:<endpoint>:<generation>:<origin>:<logical>` marker in a private Windows format. Wayland
  portal does not transport that format; `session_is_owner` is used and 1,5 content footprint window
  is used as backup.
- Adapter contract: explicit capabilities and limitation, lectura/escritura, asynchronous
  observation with waiting and stopping, and a write marker of its own. In Wayland the marker is
  empty and the limitation is reported on the output.

## 8. Blockages and pending work

1. **Wayland Desktop Validation**: not executed. Requires actual graphical session and approve
   dialog. It is not an implementation lock; it is a pending test. You have to register versions,
   permission, `clipboard_enabled` and `devices`.
2. **TLS on Linux**: not compiled due to lack of `libssl-dev`; nothing installed globally. Without
   this build you cannot try Windows ↔ Ubuntu.
3. **X11**: not implemented. It would be another backend, with libX11 and selection property; it was
   not included to not quietly replace Wayland.
4. **Two-team tests**: not executed by the rules of this phase.
5. **Permissions and persistence**: the portal may ask again; there is no persistent confidence to
   revoke. The exact experience depends on the desktop and version.

## 9. Primary sources

- Clipboard (v1):
  <https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Clipboard.html>
- RemoteDesktop (v2):
  <https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.RemoteDesktop.html>
- Official Clipboard XML:
  <https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/main/data/org.freedesktop.portal.Clipboard.xml>
- Official RemoteDesktop XML:
  <https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/main/data/org.freedesktop.portal.RemoteDesktop.xml>
- GNOME 46 dialogue:
  <https://gitlab.gnome.org/GNOME/xdg-desktop-portal-gnome/-/raw/46.0/src/remotedesktopdialog.c>
- Implementation of the Clipboard portal GNOME:
  <https://gitlab.gnome.org/GNOME/xdg-desktop-portal-gnome/-/raw/46.0/src/clipboard.c>
- Libdbus documentation on descriptors:
  <https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html>

## 10. Conclusion

What can be said: the Windows prototype is implemented and compiled; the Wayland kernel and adapter
are deployed and compiled; all 151 isolated checks pass on Linux with portal and all 149 pass on
Windows and Linux without portal. The Wayland adapter requests a RemoteDesktop session with
`types=0`, requires `clipboard_enabled=true`, rejects input devices and does not use X11 as a
substitute.

What cannot be said yet: that the actual clipboard works on Windows or on a Wayland desktop, that
the portal dialog grants the session in a specific version, that Windows and Ubuntu exchange text,
or that the Linux compilation with TLS works. P2 is still pending manual validation; the Wayland
implementation already exists and the GNOME 46 restriction is documented with its source.

Next recommended step: install `libdbus-1-dev` and `libssl-dev` on a real Ubuntu, compile, run `--inventory` and then test `MANUAL.md` in order: read, escritura/pegado, observation, permission denied and finally Windows ↔ Ubuntu.
