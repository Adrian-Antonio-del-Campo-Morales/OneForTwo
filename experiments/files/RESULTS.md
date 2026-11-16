# Results — Files

## State in force — LAN tests of 5 November 2026

Common code review: `698a9a51e0de008d0bcdf9fa3fad9683d296e5de`. No code changes or new batteries.
This state replaces the historical slope statements below.

| Caso | Observed result |
| --- | --- |
| T1 Windows → Ubuntu | Passed: mutual TLS, five files totaling 5,551,150 bytes, manual paste in Nautilus without `--to`, exact paths, sizes, SHA-256 hashes, and directories. The receiver exited with code 0. The Windows server stopped and the temporary firewall rule was removed |
| T2 Ubuntu → Windows | Receive and CLI copy with `--to` passed. Five files totaling 5,551,150 bytes had matching structures and hashes. Paste through Explorer remains pending. No clipboard offer was published |
| T0 incorrect server pin | Output 1, fingerprint mismatch. No download or cache entry |
| T0 incorrect client pin | Output 1 on client (reading failure TLS); server records unauthorised client certificate; no download or cache entry |
| Windows Collision | Second copy creates `data (1)`; both trees are accurate and intact |
| Windows permissions | ACL Write Deny only in disposable destination; output 1, empty destination. Original ACL restored in finally |
| LAN interruption and Windows retry | Local opaque relay TCP interrupts after 400.000 bytes encrypted. Output 1; empty incoming/offers and zero .o2f-part. Direct retry output 0; all five sizes and SHA-256 hashes match |
| Windows cache | Own Cache GC removes two offers; empty back list. Saved destination copies |
| T3 on demand / L2-L3 Explorer | Not executed: helper Computer Use fails before accessing desktop even after reset (setup refresh had errors / trued Node process existed unexpectedly) |

Sources T1: `lan/evidence/ubuntu-t1-receiver.log` and `lan/evidence/ubuntu-t1-integrity.json`. New Windows evidence: `lan/evidence/windows-auto-*`; originals in `.local-state/lan-files/windows/automatic-01/` (conserved synthetic data). Initial Ubuntu server PID 200354 closed with SIGINT; Further supervisors 200657 and 200867 closed with SIGINT to their group after checking cmdline. Final consultation `ss -ltnp sport = :47654`: no listening. No new rule was created or the Ubuntu firewall altered. Windows did not start server in this round. Text suspended. T2 does not credit glued Explorer; T3 still has no evidence.


## Windows Reconstruction of Ubuntu Delivery — November 5, 2026

Verified sources: `2ae444df56689dbaebe9b796daca26a55bd02917`, received from
`origin/validation/ubuntu`. Check performed on the local branch `validation/windows`, without
modifying sources or repeating Ubuntu tests.

CMake from Visual Studio 18 2026, `Visual Studio 18 2026` generator, x64, Release and OpenSSL
configuration enabled. Reconstruction completed, output 0. Binary:
`build/windows-ready/Release/oneintwo-files.exe`.

Self-test Windows: **176 checks, 0 bugs**, output 0. The previous binary was executed with
`--self-test`, without desktop or network. The main executable also finished correctly with
`--help`. The clipboard was not accessed, portals were opened, capturaron/inyectaron devices were
opened and no test network was initiated.

Unknown local evidence: `build/windows-ready/configure-ubuntu-sync.log`,
`build-ubuntu-sync.log`,`self-test-ubuntu-sync.log` and `help-ubuntu-sync.log`. This evidence closes
the Windows reconstruction of this revision; it does not demonstrate the actual functioning of the
desktop or between computers. The local evidence Ubuntu and its limits are preserved below.

## Local cases L6/L7/L8 completed — November 5, 2026

Base Git `450cbb2e340a381bda01274eb86df0e05934e822` plus local changes. The five already verified
archivos/5.551.150 bytes, server and actual client were used with **TLS mutual only in 127.0.0.1**,
fixed prints and pairs of two-day test certificates, 0600 mode keys within the ignored local output.
No clipboard, global permissions or Windows.

L6 discovered a CLI failure: `fetchFile` verified every `.o2f-part`, but `cmdReceive` did not rename it to the final name before publishing the offer. That is why `--to` failed with "lack of a verified file in the cache offer". Corrected only `cmdReceive` in `src/main.cpp`: renamed after verification, still within `incoming`; a failure leaves the reception. The `StagedFile` contract was not changed. Reconstruction `source .local-state/ubuntu-validation/env.sh` and `cmake --build experiments/files/build/ubuntu -j4`, exit 0. Corrected binary SHA-256 `35dd15ad8e2127f74453fa2e2a1e4f780c3fb7971be6d4c174de23914450b6cd`. The following actual cases constitute proof of correction; no self-tests were added or repeated.

| Caso | Result |
| --- | --- |
| L6 first reception and repetition | Outputs 0; `data` and `data (1)` with structure and SHA-256 identical to origin, without overwriting |
| L6 unwritable destination | Disposable folder mode 0500, output 1, "cannot create folder in destination"; empty destination; restored mode to 0700 |
| L7 cancellation | SIGINT after observing 262.144 bytes in partial reception; exit 1, ‘transfer cancelled’; `incoming` and `offers` empty, no `.o2f-part` |
| L8 cache | Initial list with offers; `--cache-gc --max-age-hours 0`, output 0; empty back list, originals and destination copy intact |

To be able to interrupt the 5 MiB on time, L7 used a loopback opaque TCP relay that delayed server cipher blocks→client (16 KiB/10 ms). It does not decipher or alter TLS nor substitute servidor/receptor for doubles. It was only used for L7. Server normally stopped, output 0.

Reproducible command: `/usr/bin/python3
experiments/files/manual-output/local-final-20261105-fixed/run.py`. `commands.json` retains complete
arguments and selected ports; `results.json`: **PASS**. L6/L7/L8 logs and scripts are in the same
folder ignored. The first attempt in `local-final-20261105/` was retained: initial script error by
including the prefix printed on the print (corrected without changing the prototype) and then actual
temporary name failure.

The local phase available is completed with the already documented supported glue limitation.
Automatic publication in the background and on demand Linux continue as implementation limitations;
no product work was added. Windows/Ubuntu transfers pending coordination.

## Local Desktop Validation — November 5, 2026

**Petado asistido en Nautilus verified by the user and by comparison of structure, sizes and
SHA-256: 5 files, 5.551.150 bytes, without differences.**Previous failures and Windows evidence are
preserved. Network has not been tested.

Git Base: `450cbb2e340a381bda01274eb86df0e05934e822`, branch `validation/ubuntu`, plus local adapter corrections GTK described below. No commit or publication. Ubuntu 26.04.1, GNOME Wayland, Nautilus `1:50.2.2-0ubuntu0.2`, GTK3 3.24.52; tools and dependencies of the initial compilation that follows. `GDK_BACKEND=wayland` was forced in all three publications: none substitute Wayland for X11.

### Runs and required fixes

| Caso | Observed result | Estado |
| --- | --- | --- |
| L4, first attempt without window | User cannot paste; empty destination. Trace shows `set_selection(..., 0)` without local event. Stop directed with SIGTERM, output 0 | Actual failure |
| L4, second try with local button | The offer is published with serial 55210 and the composer requests `x-special/gnome-copied-files`, but Nautilus does not paste. Directed stop, output 0 | Actual formatting failure |
| L4, third attempt after correction | User press Post files and confirm `data` folder pasted. Full comparison with correct source. Close window with X; process ends with code 0 | Passed with assisted paste |
| L5, on demand Linux | Explicit error: only local `file://` routes; B requires an unimplemented GIO/GVfs or FUSE backend; output 1 | Expected behavior; limitation of this delivery |

Fixes in `src/platform/linux/clipboard_linux.cpp`:

1. The GTK process without a window sent a selection with serial 0. In Wayland now opens a minimum
   test window and publishes after the local button **Publish files**, preserving the process until
   closing the window or receiving SIGINT/SIGTERM. No hooks are installed or global input is
   captured.
2. `x-special/gnome-copied-files` was sent as `copy\n` followed by URIs with CRLF and final jump. The Parser of Nautilus 50 separates by LF and rejects empty lines: `copy` plus `\nURI` is now issued per element, without final jump. `text/uri-list` retains its CRLF. The [parser published by GNOME for Nautilus 50.2.2](https://raw.githubusercontent.com/GNOME/nautilus/50.2.2/src/nautilus-clipboard.c) was used as a reference.

Rebuilding the adapter with GTK/TLS ended with code 0. The self-test after format change gave **176
checks, 0 failures**, without initializing GTK nor the clipboard. The rear correction of the
`<utility>` header was also recompiled correctly; it does not modify that behavior.

### Commands and evidence

From the root, load `source .local-state/ubuntu-validation/env.sh`:

```bash
./experiments/files/build/ubuntu/oneintwo-files --make-fixtures experiments/files/manual-output/ubuntu-01/data
./experiments/files/build/ubuntu/oneintwo-files --inspect experiments/files/manual-output/ubuntu-01/data
GDK_BACKEND=wayland ./experiments/files/build/ubuntu/oneintwo-files --publish-cache experiments/files/manual-output/ubuntu-01/data
./experiments/files/build/ubuntu/oneintwo-files --publish-ondemand --root experiments/files/manual-output/ubuntu-01/data
cmake --build experiments/files/build/ubuntu -j4
```

The `manual-output/ubuntu-01/paste` destination was created empty and the user opened it in
Nautilus before publishing. The agent did not copy files to the target: after the user pasted, he
compared the trees with Python (`Path.rglob`, size and `hashlib.sha256`), including the name
`resume-日本語-🚀.txt`, `empty.dat` of 0 B, the empty folder and the 5 MiB of `large-5mb.bin`. Preserved
origin and identical destination. The inspected manifest has 9 entries including root and
directories; the historical references to 6 elements are not

Ignored local logs in `manual-output/ubuntu-01/` include `fixtures.log`, `inspect.log`, `publish-cache-01.log`, `wayland-publish-debug.log`, `publish-cache-02.log` (Wayland trace), `publish-cache-03.log`, `integrity.log` (sizes and hashes of all five files), `ondemand-linux.log`, rebuild logs, and `self-test-gnome-format.log`. Publishing was supervised with `timeout --signal=TERM --kill-after=3s` (180/240 s). The diagnostic check ran for 3 s. All three desktop publishers exited with code 0 after the recorded stops. None remained active when the input experiment began.

### Limits and subsequent cases

The success is ** assisted by a local action**: it does not declare automatic delivery in the
background in Wayland. It would be necessary to validate another authorized path for that contract;
it does not integrate the portal of the other experiment nor expand the product. L6/L7/L8 with local
transport TLS and Windows/Ubuntu transfers remain pending. Certificates were not generated nor a
hearing was opened. Cases between teams require common review, identity, roles, addresses, port,
observation and recovery agreed previously.

## Initial compilation of native Ubuntu — November 5, 2026

**Current result: native prototype GTK3 + TLS compiled; 176 checks, 0 failures, output 0.** The
evidence Windows posterior is preserved in its entirety. The historical statements of "Ubuntu
uncompiled" are updated by this execution, without validating the paste in Nautilus.

### Environment, revision and components

- Git base: `450cbb2e340a381bda01274eb86df0e05934e822`, `validation/ubuntu` branch,
  `/home/pholtergate/DEV/OneForTwo` directory. No code changes in this experiment; the only local
  code patch of the set affects the header of the clipboard transport.
- Date: 2026-11-05 (`Europe/Madrid`). Ubuntu 26.04.1 LTS, Linux 7.0.0-42-generic x86_64; inherited session Wayland, `ubuntu:GNOME`.
- GNOME Shell `50.1-0ubuntu1.5`, Mutter `50.1-0ubuntu2.4`, Nautilus `1:50.2.2-0ubuntu0.2`.
- GCC/G++ `15.2.0-16ubuntu1`, CMake 4.2.3, locally extracted, Make 4.4.1, pkg-config 2.5.1; GTK3 3.24.52 (`3.24.52-0ubuntu1`), OpenSSL 3.5.5 (`3.5.5-1ubuntu3.7`), GLib/GIO 2.88.0.
- Portal Packages: xdg-desktop-portal `1.21.1+ds-1ubuntu3.1`, GNOME `50.0-0ubuntu1`,GTK
  `1.15.3-2ubuntu1`. The bus was not consulted or capabilities or permissions were negotiated.

Inventory using `git status --short --branch`, `git rev-parse HEAD`, `cat /etc/os-release`, `uname -srmo`, session variables `c++ --version`, `dpkg-query -W` and `pkg-config --modversion gtk+-3.0 openssl gio-2.0`. Units moved from `/tmp` to `.local-state/ubuntu-validation/`, within the project and excluded from Git, without global installation or `sudo`: [preparation and exact variables](../clipboard/RESULTS.md#local-dependencies-for-this-validation).

### Observed commands and results

From the root of the repository:

```bash
source .local-state/ubuntu-validation/env.sh
cmake --fresh -S experiments/files -B experiments/files/build/ubuntu \
  -DCMAKE_BUILD_TYPE=Release -DO2F_ENABLE_GTK=ON -DO2F_ENABLE_TLS=ON \
  -DOPENSSL_ROOT_DIR="$OIT_VALIDATION_DEPS/usr"
cmake --build experiments/files/build/ubuntu -j4
cd experiments/files
./build/ubuntu/oneintwo-files --self-test
```

Final configuration, compilation and self-test: output 0. CMake reports `TLS: OpenSSL 3.5.5` and `Found gtk+-3.0, version 3.24.52`. `src/platform/linux/clipboard_linux.cpp`, `O2F_WITH_GTK=1` and `O2F_WITH_TLS=1` were compiled; the executable links `libgtk-3.so.0`, `libssl.so.3` and `libcrypto.so.3`. `ldd` does not detect libraries missing. This is not variant `clipboard_unavailable`. After the transfer, the cache was re-registered with `--fresh`, reconstructed with persistent routes and repeated self-test: **176/0**.

```text
176 checks, 0 failures
self-test passed (no clipboard or network)
```

Temporal `.o2f-selftest-*` was created within `experiments/files` and was removed when finished correctly. GTK and clipboard was not initialized.

The first attempt omitted GTK because the local environment of pkg-config excluded the already installed metadata of `zlib`, `expat` and `shared-mime-info`. That binary also gave 176/0, but **was not used to close the native compilation**. The system's metadata paths were added to `PKG_CONFIG_LIBDIR`, reconfigured, compiled the GTK adapter and repeated the self-test. The core issued a notice `-Wunused-function` in `src/core/cache.cpp:21`; does not prevent compiling and no cleaning was performed outside the validation.

Local logs ignored in `build/ubuntu/`: `configure.log`, `build.log`, `self-test.log`, `libraries.log` and previous attempts `configure-without-gtk.log`, `build-without-gtk.log`, `self-test-without-gtk.log`. The post-transfer execution is `configure-persistent.log`, `build-persistent.log`, `self-test-persistent.log` and `libraries-persistent.log`.

### Earrings and next step

By user indication, only the isolated stage was completed: no offers were published and no Nautilus
was opened. L1 Ubuntu is exceeded; L4 glued, L5 limitation on demand, L6 colisiones/permisos with
TLS, L7 cancellation and L8 cache and garbage collection are still pending manual executions. The isolated cases of
self-test are not confused with these observations.

Before L4: present user, synthetic manual data, new destination, Ctrl+C prepared and real comparison of structure, sizes, and SHA-256 hashes after pasting. Before T0/T1/T2/T3: agree with Windows the shared review, binary, roles, IP and port 47654/TCP, public certificates and both fingerprints, folders, duration and stop; first test identity rechazada/aceptada. No test network or key generated. Windows evidence has not been repeated. There is no lock pending compilation GTK/TLS on this machine; desktop and coordination between computers remain open. Integration of deferred application.

## Historical evidence preserved — previous delivery and review

Updated November 4, 2026.

Documentary review: November 5, 2026. The evidence of the corrected delivery of the agent is
preserved; his compilations or checks have not been repeated or carried out desktop or network
tests. The consolidated status is shown in [experiments/README.md](../README.md).

Responsibility for this file: record what is implemented, what has been compiled and checked in
isolation, and what is still pending manual execution. A correct compilation does not show that the
clipboard or browser works.

## 1. Estado

| Elemento | Estado | Evidence or blocker |
| --- | --- | --- |
| Transfer, manifest, cache and protocol | Implementado | `src/core/` (model, manifest, warehouse, blocks, cache, plots, TLS) |
| Cache publication on Windows | Implementado | `src/platform/win/clipboard_win.cpp` (`CF_HDROP` + `OleFlushClipboard`) |
| On-demand delivery on Windows | Implemented, validation pending | `CFSTR_FILEDESCRIPTORW` + `CFSTR_FILECONTENTS` with `IStream` |
| Cache publication in Ubuntu | Implemented, build pending | GTK3 with `x-special/gnome-copied-files` and `text/uri-list` |
| On-demand delivery on Ubuntu | Unimplemented and outside scope | Publishes only `file://` routes; B would require GIO/GVfs or FUSE backend; evidence in §5.1 |
| Compilation Windows x64 | Ejecutada | MSVC 14.51.36231, CMake 3.24.3, OpenSSL 3.0.5, Windows 10.0.19045 |
| Compilation Ubuntu | **No ejecutada** | There is no host Ubuntu in this session; no claim |
| Self-tests aislados | **Run: 176 checks, 0 failures** | `oneintwo-files.exe --self-test` |
| Customer TLS Authentication | Implemented, not executed | Customer certificate and fingerprint fixed with `--client-pin`; unexercised handshake |
| `MANUAL.md` | Delivered | Commands, permissions, shutdown, and test matrix |
| Real paste in Explorer or Nautilus | **Pending manual implementation** | Graphical session required |
| Transfer between two teams | **Pending manual implementation** | Requires certificates and two hosts |
| On-demand paste over the network | **Pending manual implementation** | Requires browser and server |

## 2. Evidence of compilation and self-test

Accurate compilation environment:

```text
Windows 10.0.19045.7807 (x64)
Visual Studio 2026 Build Tools 18, MSVC 14.51.36231
CMake 3.24.3
OpenSSL 3.0.5 (Win64, C:\Program Files\OpenSSL-Win64)
Generador: NMake Makefiles, CMAKE_BUILD_TYPE=Release
```

Commands equivalent to those executed (in ‘x64 Native Tools Command Prompt’):

```bat
cd /d D:\DEVEL\OneInTwo\experiments\files
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ^
      -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64"
cmake --build build
build\oneintwo-files.exe --self-test
```

Relevant output (last compilation):

```text
-- TLS: OpenSSL 3.0.5
[100%] Built target oneintwo-files
isolated self-test in ...\experiments\files\.o2f-selftest-<pid>
176 checks, 0 failures
self-test passed (no clipboard or network)
```

- Binario: `build/oneintwo-files.exe` (Release x64, aproximadamente 368 KB).
- No compiler notices in the last full compilation.
- The self-test timer is created under the current directory and deleted when finished correctly.
- `--fingerprint` on the synthetic public certificate returned `5b9e9ce5…b9d8`, identical to the
  SHA-256 calculated with `openssl x509 -outform DER | openssl dgst -sha256`.

Command line checks executed in addition to self-test:

| Comando | Observed result |
| --- | --- |
| no arguments / `--help` | only help, code 0 |
| `--make-fixtures test-data` | 6 elements, 5 files, 5 551 150 bytes |
| `--inspect test-data` | manifest with empty file, empty folder and correct Unicode name |
| `--fingerprint CERT.pem` | SHA-256 footprint of a public PEM; matches OpenSSL |

`test-data` was disposable material and was then removed.

## 3. What Self-tests Cover

No desktop, no network and no clipboard (176 checks):

- SHA-256: known vectors, empty text and incremental calculation.
- Routes: Unicode accepted; `..`, absolute routes, empty components, control characters, Windows
  reserved names, final punto/espacio, depth and length.
- Manifesto: empty file, empty folder, nesting, Unicode, element limit, depth limit, first level
  names repeated.
- Serialized manifest: back and forth, rejection of manipulated paths and invalid identifiers, limit
  of elements.
- Authorization: read only by offered identifier; unknown identifiers, other offer, folders and
  scrolls out of range.
- Reception: 64 KiB blocks, correct size and digest, truncated, altered digest, off-order writing,
  excess bytes, escape route.
- Destination: file and folder collision, original intact, unfused folders, non-existent
  destination.
- Cache: publication, manifesto, list, search, concession of use, gc by age, cleaning of abandoned
  receptions, invalid offer identifier.
- Frames: round trip, excessive length, unknown type, and truncated body.
- Certificate: SHA-256 imprint of a synthetic PEM embedded, correct length and rejection of an
  invalid PEM.
- Memory protocol: manifest, download by blocks with digest, read by ranges for on demand, end of
  file, cancellation in progress and rejection of a request from a foreign identifier.
- Cleaning of orphans `.o2f-part`.

## 4. What is NOT proven (not to say otherwise)

- **Native paste**: nothing has been published on the actual clipboard or stuck in Explorer or
  Nautilus.
- **Explorer/OLE**: it has not been verified that it accepts `FileGroupDescriptorW` +
  `FileContents`, nor the lock of the STA thread while the content arrives, nor the perceived
  waiting times.
- **Nautilus/GTK/Wayland**: The Linux adapter has not been compiled even in this session; its syntax
  and behavior are unverified.
- **TLS mutual between teams**: the code was compiled with OpenSSL 3.0.5, but no listening was
  opened and no handshake was performed. Customer certificate, fingerprint rejection and timeouts
  are not exercised live.
- **Real network transfer**: the protocol was tested only on a couple of memory streams.
- **Full disk**: there is a way of error, but the condition was not provoked.
- **Destiny without writing permission**: covered by the manual, not executed.
- **Cortar/mover, resumption, ACL/permisos POSIX, symbolic links**: out of reach or explicitly
  rejected.
- **Multiple simultaneous clients**: the server handles one session at a time.

Any row in the manual matrix that has not been observed is ‘Pending’, even if the self-test is
green.

## 5. Prototyping decisions

1. **A (cache) is the functional reference.** It is downloaded, checked by file (size and SHA-256)
   in a private receiving directory and only then the offer is published. Paste delivers local
   files, which is what both explorers consume in a documented way.
2. **B (on demand) is limited to Windows.** The manifest is displayed as `FileGroupDescriptorW` and
   each file as `IStream` under `FileContents`; the stream requests ranges to the source when
   Explorer reads them. A file system provider is not built.
3. **Linux does not implement B by scope, not by proven impossibility.** The adapter publishes only local `file://` routes. Nautilus deserializes `x-special/gnome-copied-files` accepting any valid URI and GVfs extends GIO with D-Bus backends, without requiring FUSE; a lazy delivery would be possible with its own GIO/GVfs backend or with a FUSE mount, both out of order. The mode returns an explanatory error and is not presented as a technical conclusion.
4. **Content never travels on absolute routes.**The recipient receives a manifest with validated
   relative paths and opaque identifiers; he can only request identifiers of the current
   offer.Identifiers used as folder name are restricted to alphanumerics, `-` and `_`.
5. **Separation of responsibilities.**
   - `core/manifest` + `core/store`: what is offered and with what limits.
   - `core/transfer`: Block reception, `.o2f-part`, digest and placement without overwriting.
   - `core/cache`: verified offers, leases, and cleanup.
   - `core/wire` + `core/remote`: minimum protocol and element authorization.
   - `core/net`: TCP and TLS with fingerprint fixation.
   - `platform/*/clipboard_*`: only layer that knows native formats. The transfer engine does not
     know Explorer or GTK; the adapter does not know the network protocol.
6. **TLS mutual mandatory in remote mode**: self-signed certificates and both SHA-256 fingerprints
   compared out of band. The client fixes the server's (`--pin`) and the server requires and fixes
   the client's ( `--client-pin`). Without OpenSSL, the remote mode is blocked; there is no silent
   drop to plain text.
7. **Explicit limits**: 4096 elements, depth 32, 8 GiB per offer, blocks of 256 KiB, 259 characters
   per path in the Windows descriptor.
8. **Life of the offer**: publication blocks up to Ctrl+C. In Windows, cache materializes with
   `OleFlushClipboard` and routes remain available after closing; Linux removes selection upon exit;
   delivery on demand disappears with the process because there is no copy to materialize.

### 5.1 Evidence on Nautilus, GVfs and FUSE

The statement ‘Nautilus requires FUSE’ is not supported by the sources consulted:

- `nautilus-clipboard.c` records `x-special/gnome-copied-files` and `GDK_TYPE_FILE_LIST`; when deserializing, validate each line with `g_uri_is_valid` and build a `NautilusFile` with `nautilus_file_get_by_uri`. Does not filter to `file://`: accepts any valid URI. <https://gitlab.gnome.org/GNOME/nautilus/-/raw/main/src/nautilus-clipboard.c>
- The documentation of GVfs describes GIO as extension point and backends as D-Bus processes; the daemon FUSE is a separate piece, deactivated with `GVFS_DISABLE_FUSE`. An own scheme can be served by GIO/GVfs without FUSE. <https://wiki.gnome.org/Projects/gvfs/doc>
- The `org.freedesktop.portal.FileTransfer` portal exports existing file descriptors to the document
  warehouse; it does not allow delivering lazy content.
  <https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.FileTransfer.html>

Conclusion: B in Linux is not implemented, it is not technically impossible. This delivery publishes
only local `file://` routes. A lazy delivery would require a GIO/GVfs backend for an own scheme
(D-Bus) or a FUSE mount that materializes the files; both are out of the scope of the experiment.

## 6. Dependencies and sources

| Dependency | Used or planned version | Source and license | Use |
| --- | --- | --- | --- |
| OpenSSL | 3.0.5 (Windows) | openssl.org — Apache-2.0 | Remote mode TLS |
| GTK3 | 3.x (Ubuntu, `libgtk-3-dev`) | GNOME — LGPL-2.1-or-later | clipboard on Linux |
| Win32 / OLE / Shell | Windows SDK 10.0.26100 | Sistema Windows | `CF_HDROP`, `IDataObject`, `IStream` |
| Standard library C++17 | MSVC 14.51 / libstdc++ | implementation of the standard | files, threads, containers |
| SHA-256 | self-implementation | FIPS 180-4, original code | transfer integrity |

No code of sharing applications or third-party engines has been incorporated. `std::filesystem` is
used with UTF-8 (`u8path`) routes to store Unicode names.

## 7. Limits and outstanding work

1. **Ubuntu without compiling** due to lack of host in this session. Concrete risk: compilation
   errors or API GTK3 not detected. The manual indicates how to compile and what dependency to
   install.
2. **B in Linux not implemented**: is not a conclusion of non-viability. It would require a GIO/GVfs
   backend by D-Bus or a FUSE mount (evidence in §5.1), outside the scope of this delivery.
3. **B on Windows unvalidated**: `GetData` is synchronous and runs on the STA thread; while content
   arrives, Explorer can be locked. No cancellation from browser, only socket timeout. Measure in
   T3.
4. **One client at a time** and without resumption: one cut forces the transfer to be repeated. The
   recipient does not publish partial offers.
5. **No full disk proof** no cache fee: `--cache-gc` cleans by age, not by space.
6. **Without CA or persistent confidence**: the footprint is passed on each connection. For the
   product it would be necessary to decide identity storage and revocation.
7. **Metadata**: POSIX, ACL, dates and attributes are not preserved; only name, type and content.
8. **Required common changes** (outside my directory, not made): no request. The experiment is
   self-contained.

## 8. Provisional conclusion

- It can be said: the offer engine, block transfer, SHA-256, cache with concessions, collision
  resolution, route validation and protocol authorized by identifiers is implemented and passes 176
  isolated checks on Windows.
- It cannot be said yet: that Explorer or Nautilus paste files, that the OLE descriptor works on
  this version of Windows, or that delivery on demand is usable with real network.
- Mutual authentication is implemented and compiled, but no handshake has been executed: it is not
  claimed to accept or reject live certificates.
- **Recommendation for the product**: adopt A as a base strategy, with visible "prepared" status and
  a private cache folder with quota and cleaning. Treat B as an optional Windows exclusive
  optimization, only if T3 shows correct pasting, locked down and clear errors. Do not enter a
  virtual file system for Linux at this stage.
- Proposed responsibility for adapters: `file_clipboard` has native formats and their life cycle;
  `files` has offers, identifiers, blocks, integrity and cache. The adapter must not decide the
  trust or protocol.

## 9. Historical matrix of initial delivery (replaced by current status)

| Caso | Exact environment | Esperado | Observado | Result |
| --- | --- | --- | --- | --- |
| L1 self-test |  | 0 fallos | 176/0 en Windows 10 19045 | Passed (isolated) |
| L2 cache Explorer |  | complete paste with matching hashes | Not run | Pending |
| L3 on demand Windows |  | complete paste or a documented blocker | Not run | Pending |
| L4 Nautilus cache |  | complete paste with matching hashes | Not run | Pending |
| L5 on demand Linux |  | explicit error: implementation limitation, non-viability | Not run | Pending |
| L6 collisions or unwritable destination |  | suffixes and a clear error | Not run | Pending |
| L7 cancellation |  | no published offer or temporary files | Not run | Pending |
| L8 cache and gc |  | offers removed after expiry | Not run | Pending |
| T1 Windows→Ubuntu |  | complete paste with matching hashes | Not run | Pending |
| T2 Ubuntu→Windows |  | complete paste with matching hashes | Not run | Pending |
| T3 Windows←Ubuntu on demand |  | complete paste and an explicit error on interruption | Not run | Pending |
| T0 mutual TLS rejection |  | client or server fingerprint rejected | Not run | Pending |

The time, space and perceived lock comparison between A and B is defined in `MANUAL.md` §8 and
remains empty until manual execution.
