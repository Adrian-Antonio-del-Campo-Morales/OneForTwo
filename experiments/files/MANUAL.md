> Updated status: T1 exceeded; T2 received CLI, TLS negative, corte/reintento and automated Windows
> cases exceeded. Pasted Explorer and on demand pending. See RESULTS.md; expected from this script
> are not results.

# Execution manual — file experiment

Status: prototype implemented and compiled on Windows; executed self-tests. **This manual retains
the execution script; the current status is in RESULTS.md**: this document is the script to do so
and the matrix to record the results. Nothing that appears as "expected" is yet validated.

Responsibility for this file: allow the user to play each test without confusing it with automatic
checks, and collect what to observe, how to stop the prototype and how to leave the system as it
was.

## 1. Alcance

Copy and paste, in both directions:

- a small file, an empty file and a folder with subfolders;
- with already downloaded content (alternative **A, cache**);
- with content delivery when pasting (alternative **B, on demand**), only where the prototype offers
  it (Windows).

They are not part of the test: cortar/mover, delete originals, sync folders, mount a virtual file
system or share the entire disk.

## 2. Inventory of environment (P0)

Fill in before starting; without this data the result is not interpretable.

| Dato | Windows | Ubuntu |
| --- | --- | --- |
| System and compilation |  |  |
| Desktop and session (X11/Wayland) |  |  |
| Explorer and version |  |  |
| Compiler and CMake |  |  |
| OpenSSL |  |  |
| GTK3 | no aplica |  |
| Permissions and network used |  |  |

Useful commands:

```powershell
# Windows (PowerShell)
Get-ComputerInfo | Select-Object WindowsProductName, WindowsVersion, OsBuildNumber
(Get-Item C:\Windows\explorer.exe).VersionInfo.ProductVersion
cmake --version
```

```bash
# Ubuntu
lsb_release -a; echo "session: $XDG_SESSION_TYPE"
gnome-shell --version; nautilus --version
cmake --version; openssl version; pkg-config --modversion gtk+-3.0
```

## 3. Units and compilation

### Windows

Requisitos:

- Windows 10/11 x64.
- Visual Studio 2022/2026 **Builded Tools** with "C++ Desktop Development" (proportion `cl.exe` and
  `nmake.exe`) and Windows SDK.
- CMake 3.16 o posterior.
- OpenSSL 3 for Win64 with headers and `libssl.lib`/`libcrypto.lib` (tested with installation in
  `C:\Program Files\OpenSSL-Win64`).

Compile in **‘x64 Native Tools Command Prompt for VS’** (not in Normal PowerShell: `nmake` is not in
the PATH):

```bat
cd /d D:\DEVEL\OneInTwo\experiments\files
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ^
      -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64"
cmake --build build
```

Resulting binary: `build\oneintwo-files.exe`. To run it out of the compilation directory, copy with it `libssl-3-x64.dll` and `libcrypto-3-x64.dll`, or add `C:\Program Files\OpenSSL-Win64\bin` to PATH.

If OpenSSL is not found, CMake warns and compiles without TLS: `--share` and `--receive-offer` are
blocked with an explicit error; plain text is not used as a substitute.

### Ubuntu

```bash
sudo apt install build-essential cmake pkg-config libgtk-3-dev libssl-dev
cd ~/OneInTwo/experiments/files        # use the actual repository path
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Binario: `./build/oneintwo-files`. GTK3 is necessary to publish in the Nautilus clipboard; without
it, CMake warns and that mode returns error. OpenSSL is necessary for inter-computer mode.

### Common check

```bash
# Windows: build\oneintwo-files.exe --self-test
# Ubuntu:  ./build/oneintwo-files --self-test
```

Expected: count of checks with **0 bugs** and the correct self-test text (no clipboard or network)
This mode does not open the clipboard or the network and can always be executed.

## 4. Preparation of disposable data

Use new directories and synthetic files only.

```bat
:: Windows
mkdir D:\o2t-manual
build\oneintwo-files.exe --make-fixtures D:\o2t-manual\data
```

```bash
# Ubuntu
mkdir -p ~/o2t-manual
./build/oneintwo-files --make-fixtures ~/o2t-manual/data
```

Generated contents (deterministic synthetic data):

| Elemento | Size | What for? |
| --- | --- | --- |
| `file-small.txt` | 1024 B | normal file |
| `empty.dat` | 0 B | empty file |
| `resume-日本語-🚀.txt` | 46 B | Unicode names |
| `folder/nested/deep.bin` | 300 KiB | nesting and blocks |
| `folder/empty-subdir/` | empty folder | estructura |
| `large-5mb.bin` | 5 MiB | progress, cancellation and timing |

`--inspect PATH...` displays the manifest without publishing anything (relative paths, short sizes
and identifiers). It does not run the desktop.

Content comparison:

```powershell
Get-FileHash -Algorithm SHA256 D:\o2t-manual\paste\data\file-small.txt
```

```bash
sha256sum ~/o2t-manual/paste/data/file-small.txt
```

## 5. Executable Modes

| Modo | Comando | Efecto |
| --- | --- | --- |
| Ayuda | `--help` (or no arguments) | text only; does not touch the system |
| Self-test | `--self-test [--verbose]` | isolated checks |
| Datos | `--make-fixtures DIR` | creates the synthetic set |
| Manifest | `--inspect PATH...` | summarizes the offer without publishing it |
| Fingerprint | `--fingerprint CERT.pem` | prints the SHA-256 fingerprint without using the network |
| Local cache | `--publish-cache PATH...` | publishes existing local paths |
| Local on-demand | `--publish-ondemand --root PATH` | Windows; read disk content when pasting |
| Remote on-demand | `--publish-ondemand --from HOST:PORT --pin FINGERPRINT --cert CERT --key KEY` | Windows; content ordered from the other computer |
| Serve | `--share --root PATH --cert CERT --key KEY --client-pin CLIENT_FINGERPRINT` | mutual TLS and the file protocol. Press Ctrl+C to stop |
| Receive | `--receive-offer --from HOST:PORT --pin FINGERPRINT --cache DIR --cert CERT --key KEY` | downloads, verifies, and caches the files |
| Receive and paste | add `--publish` to `--receive-offer` | publish on clipboard when verifying |
| Copy to destination | add `--to DESTINO` | verified copy with collisions resolved |
| Cache | `--cache-list --cache DIR` | list offers; `--verbose` adds route |
| Limpieza | `--cache-gc --cache DIR [--max-age-hours N]` | elimina ofertas antiguas |

Common options: `--port N` (default 47654), `--timeout MS` (by default 15000 ms) and `--verbose`.

Prototyping limits: 4096 elements, depth 32, 8 GiB per offer and 256 KiB blocks. In Windows, a
relative path of the descriptor cannot exceed 259 characters. Symbolic links and special types are
rejected.

## 6. Local testing (one equipment)

### L1. Self-test

Run the command in section 3. **Expected:** 0 bugs, no windows or permissions. **Fault:** output
code other than zero; keep the `.o2f-selftest-*` folder you print.

### L2. Local cache on Windows (Explorer)

1. Run `build\oneintwo-files.exe --publish-cache D:\o2t-manual\data`. You must print "posting N
   local elements" and stay waiting.
2. Open Explorer, create `D:\o2t-manual\paste` and enter it.
3. Pegar con `Ctrl+V`.
4. Check that `data` appears with the 6 elements and that the sizes match.
5. Compare `sha256sum`/`Get-FileHash` from `file-small.txt` and `large-5mb.bin`.
6. Return to console and press `Ctrl+C`.

**Expected:** Full copy, originals intact, empty folder preserved. **Fault:** The clipboard appears
empty, only text is pasted, files are missing or the process is not responding. Write down if
Explorer requested additional permissions.

> On Windows, the offer in this mode is materialized by pressing `Ctrl+C`: the
> clipboard can continue pasting paths to the cache after closing the
> The files are local and were already verified.

### L3. Local on-demand on Windows (B)

1. `build\oneintwo-files.exe --publish-ondemand --root D:\o2t-manual\data`
2. Paste into a new folder.
3. See if Explorer creates the files and if the window stays locked while reading the 5 MiBs.
4. Measure time to first file and until finished (chronometer).
5. Press `Ctrl+C`; try pasting back into another folder.

**Expected:** Correct copy, no intermediate cache, with live process during pasting. **Fault:**
Explorer does not offer pasting, pasting error, blocking without message or incomplete files. **
Blocking condition:** If Explorer does not accept `FileGroupDescriptorW`/`FileContents` in this
environment, B is not feasible here and is registered as such.

> When closing the process the supply on demand disappears: no copy that
> materialize. It is an expected difference against L2.

### L4. Local cache in Ubuntu, Nautilus

1. `./build/oneintwo-files --publish-cache ~/o2t-manual/data`
2. A test window appears in Wayland: press **Publish files** and leave it open. It is a local action
   necessary for GTK to deliver a valid selection to the composer; assisted mode does not validate
   automatic publication in the background. In X11 that window is not required.
3. In Nautilus, open a new folder and paste `Ctrl+V`. Prepare the folder before publishing if you
   have to copy its path: copy text then replace the file offer.
4. Check structure, size and `sha256sum`.
5. Close the test window or use `Ctrl+C` in the terminal.

**Waited:** pasted complete with Unicode names. **Fault:** Nautilus does not show the option to
paste files, paste text only or missing items. Register session (`XDG_SESSION_TYPE`), version of
Nautilus and GTK.

> The selection of GTK belongs to the process: when pressing `Ctrl+C` the offer
> It doesn't materialize like Windows.

### L5. On-demand on Ubuntu (B)

```bash
./build/oneintwo-files --publish-ondemand --root ~/o2t-manual/data
```

**Expected:** Explicit error indicating that this implementation only publishes local `file://`
routes and that B would require a GIO/GVfs or FUSE backend, not implemented. **It is not a test of
technical non-viability**: it is a limitation of this delivery and must be registered as such.

### L6. Collisions and destination without writing

With a server on your own computer (two terminals, TLS):

1. Create the two pairs of the 7.1 section and boot: `--share --root D:\o2t-manual\data --cert
   server-cert.pem --key server-key.pem --client-pin CLIENT_FINGERPRINT --bind 127.0.0.1`
2. En otra terminal: `--receive-offer --from 127.0.0.1 --pin FINGERPRINT_SERVIDOR --cache
   D:\o2t-manual\cache --to D:\o2t-manual\copia --cert client-cert.pem --key client-key.pem`
3. Repeat the same command with the same destination.
4. Check that the second copy creates `data (1)` and does not overwrite anything.
5. Create a folder without writing permission and use `--to` pointing to it.

**Expected:** `(1)` suffixes, `(2)`; original content intact; in the destination without writing,
error message, output code 1 and no `.o2f-part` file published. **Fault:** silent overwriting,
success with incomplete files or remains in the destination folder.

### L7. Cancellation

1. Start the L6 step server again.
2. Launch `--receive-offer ... --cache DIR` with the 5 MiBs and press `Ctrl+C` when some file has
   already been downloaded.
3. Revisar `DIR\incoming` y `DIR\offers`.

**Expected:** cancellation message, no published offer, empty `incoming` and no `.o2f-part`.
**Fault:** the offer appears as complete or remains temporary without owner.

### L8. Life of the cache

1. Tras L6/L7: `--cache-list --cache D:\o2t-manual\cache`.
2. `--cache-gc --cache D:\o2t-manual\cache --max-age-hours 0`.
3. Repetir `--cache-list`.

**Expected:** the first list shows verified offers; the gc with 0 hours removes them (except for a
valid concession, which no longer exists here); the second list is empty. **Fault:** an offer in use
is deleted or orphan folders are left.

## 7. Testing between two teams

Mandatory order: first local; then two teams. Only one experiment active in the same graphic
session.

### 7.1 Test certificates (outside the repository)

The remote mode uses **TLS mutual**: each computer presents its own certificate and fixes the
footprint of the other. There is no CA or implicit confidence. Generate a pair in **each** computer,
in a folder outside the repository.

On the **server** team:

```bat
:: Windows
mkdir "%USERPROFILE%\o2t-certs"
cd /d "%USERPROFILE%\o2t-certs"
"C:\Program Files\OpenSSL-Win64\bin\openssl.exe" req -x509 -newkey rsa:3072 ^
  -sha256 -days 2 -nodes -keyout server-key.pem -out server-cert.pem ^
  -subj "/CN=OneInTwo files experiment"
```

```bash
# Ubuntu
mkdir -p ~/o2t-certs && cd ~/o2t-certs
openssl req -x509 -newkey rsa:3072 -sha256 -days 2 -nodes \
  -keyout server-key.pem -out server-cert.pem \
  -subj "/CN=OneInTwo files experiment"
chmod 600 server-key.pem
```

On the computer **client**, repeat the same command by changing `server-` to `client-`. Each team
retains its private key; exchange only public certificates for a channel out of band. Calculate the
footprints with the prototype itself:

```bash
# On the server: fingerprint for the client
./build/oneintwo-files --fingerprint server-cert.pem

# On the client: fingerprint for the server
./build/oneintwo-files --fingerprint client-cert.pem
# Windows: build\oneintwo-files.exe --fingerprint client-cert.pem
```

- The server starts with `--client-pin FINGERPRINT_DEL_CERTIFICADO_DE_CLIENTE`.
- The customer starts with `--pin FINGERPRINT_DEL_CERTIFICADO_DEL_SERVIDOR` and presents its pair with
  `--cert` and `--key`.

If either certificate is regenerated, its footprint changes and the other end will reject the
handshake until the corresponding pin is updated. It is the expected behavior.

#### 7.1.1 Rejecting unpinned identities (manual)

1. Start the server with a `--client-pin` other than the certificate the client will use.
2. Connect the client to its correct pair: the server must reject the handshake ("unauthorised
   customer certificate") and the client must not receive the manifest.
3. Start the server with the correct pin and connect the client with an incorrect `--pin`: the
   client must reject the server certificate.
4. Repeat with both correct pins and check that the transfer works.

**Explicit error in both rejections, without download or publication. **Fault:** Connection accepted
with a different footprint or generic message without diagnosis. This test opens real network:
manual only.

### 7.2 Network and permissions

- Enter server IP: `ipconfig` (Windows) or `ip -4 addr show` (Ubuntu).
- Windows can display a Firewall warning when listening; allow only on the private network.
  Alternative with administrator permissions (revert upon completion): `netsh advfirewall firewall
  add rule name="OneInTwo files experiment" dir=in action=allow protocol=TCP localport=47654`
- Ubuntu with ufw: `sudo ufw allow 47654/tcp`, and remove the rule at the end.
- No need to run as an administrator or install services.

### T1. Windows → Ubuntu (cache)

1. **Windows (origen):**
   ```bat
   build\oneintwo-files.exe --share --root D:\o2t-manual\data ^
     --cert "%USERPROFILE%\o2t-certs\server-cert.pem" ^
     --key  "%USERPROFILE%\o2t-certs\server-key.pem" ^
     --client-pin CLIENT_FINGERPRINT --bind 0.0.0.0 --port 47654
   ```
   Write down the print print and message "listening".
2. **Ubuntu (target):**
   ```bash
   ./build/oneintwo-files --receive-offer --from IP_DEL_WINDOWS \
     --pin FINGERPRINT_SERVIDOR --cache ~/o2t-manual/cache --publish \
     --cert ~/o2t-certs/client-cert.pem --key ~/o2t-certs/client-key.pem
   ```
   You must check the 5 files and post them to the clipboard.
3. In Nautilus, create `~/o2t-manual/paste` and paste.
4. Verify: `data/file-small.txt`, `data/empty.dat` (0 B), `data/resume-日本語-🚀.txt`,
   `data/folder/nested/deep.bin`, `data/folder/empty-subdir/`, `data/large-5mb.bin`; comparar
   hashes.
5. `Ctrl+C` in Ubuntu and then on Windows.

**Expected:** Complete pasted, equal hashes, preserved structure. **Fault:** TLS error (print or
certificate), rejected connection (firewall/IP), erroneous manifest or incomplete files. Register
the exact text.

### T2. Ubuntu → Windows (cache)

1. **Ubuntu (origen):**
   ```bash
   ./build/oneintwo-files --share --root ~/o2t-manual/data \
     --cert ~/o2t-certs/server-cert.pem --key ~/o2t-certs/server-key.pem \
     --client-pin CLIENT_FINGERPRINT --bind 0.0.0.0 --port 47654
   ```
2. **Windows (target):**
   ```bat
   build\oneintwo-files.exe --receive-offer --from IP_DE_UBUNTU ^
     --pin FINGERPRINT_SERVIDOR --cache D:\o2t-manual\cache --publish ^
     --cert "%USERPROFILE%\o2t-certs\client-cert.pem" ^
     --key  "%USERPROFILE%\o2t-certs\client-key.pem"
   ```
3. Paste into Explorer in `D:\o2t-manual\paste` and compare.
4. `Ctrl+C` on Windows; the offer is materialized; repeat pasting in another folder to check that
   the paths searched are still available.
5. `Ctrl+C` en Ubuntu.

**Expected:** identical to T1, including Unicode name. **Fault:** altered names, missing folders or
pasting error.

### T3. Windows ← Ubuntu on demand (B)

1. Ubuntu origin: the same `--share` of T2.
2. Windows target:
   ```bat
   build\oneintwo-files.exe --publish-ondemand --from IP_DE_UBUNTU ^
     --pin FINGERPRINT_SERVIDOR ^
     --cert "%USERPROFILE%\o2t-certs\client-cert.pem" ^
     --key  "%USERPROFILE%\o2t-certs\client-key.pem" --timeout 15000
   ```
3. Paste into a new folder. Time: time to first file and until finished; note if Explorer window
   crashes.
4. With an ongoing transfer, stop the Ubuntu server (`Ctrl+C`) or disconnect the network. Observe
   the browser error.
5. Repeat without interruption and compare hashes.

**Expected:** pasted correctly with the live server; before cut, visible error and no declared
complete file. **Fault:** Undefined blocking, false success or incomplete file presented as valid.
** Blocking condition:** if Explorer does not request `FileContents` or does not support the
descriptor, register B as not viable on Windows for this environment.

> `--timeout` limits each reading TLS. A server that stops responding must
> report an error within the time limit rather than waiting indefinitely.

### T4. Ubuntu on demand

Not executed: the prototype returns "unsupported" because this implementation only publishes local
`file://` routes. A lazy delivery would require a backend GIO/GVfs or FUSE, outside the order; it is
not claimed to be technically impossible.

## 8. Comparison A versus B

Register with the same data of 5 MiB and the same network:

| Metric | A: cache | B: on demand |
| --- | --- | --- |
| Time until you can paste |  |  |
| Total paste time |  |  |
| Prototyping disk space |  |  |
| Perceived Explorer Block |  |  |
| Behavior when the process exits |  |  |
| Error if the network fails while pasting | no aplica |  |
| Integration complexity | baja | alta |

Qualitative observations to be noted:

- Does Explorer/Nautilus show progress? Can it be cancelled from the browser?
- Can you hit it twice in a row?
- What happens if it sticks before the discharge (A) ends?
- Does the browser window respond while reading a large file (B)?

## 9. Stop and restore status

1. `Ctrl+C` in each prototype process. You must print the end of the publication or "stop server";
   the output code must be 0.
2. Delete the data and the test cache:
   ```bat
   rmdir /s /q D:\o2t-manual
   rmdir /s /q "%USERPROFILE%\o2t-certs"
   ```
   ```bash
   rm -rf ~/o2t-manual ~/o2t-certs
   ```
3. If the firewall rule was added, remove it:
   ```bat
   netsh advfirewall firewall delete rule name="OneInTwo files experiment"
   ```
   ```bash
   sudo ufw delete allow 47654/tcp
   ```
4. The prototype does not install services, does not type in the record, or automatically start. The
   `build/` folder can be deleted at any time.

## 10. Manual results matrix

One row per run. Copy this table to `RESULTS.md` or complete it here; do not mark as exceeded
anything that has not been observed.

| Caso | Equipment and meeting | Esperado | Observado | Result |
| --- | --- | --- | --- | --- |
| L1 self-test |  | 0 fallos |  | Pending |
| L2 cache Explorer |  | complete paste with matching hashes |  | Pending |
| L3 on demand local Windows |  | complete paste or a documented blocker |  | Pending |
| L4 Nautilus cache |  | complete paste with matching hashes |  | Pending |
| L5 on demand Linux |  | explicit error: implementation limitation, non-viability |  | Pending |
| L6 collisions and no writing |  | suffixes, no overwrite, and a clear error |  | Pending |
| L7 cancellation |  | no published offer or temporary files |  | Pending |
| L8 cache and gc |  | offers eliminated by age |  | Pending |
| T1 Windows→Ubuntu |  | complete paste with matching hashes |  | Pending |
| T2 Ubuntu→Windows |  | complete paste with matching hashes |  | Pending |
| T3 Windows←Ubuntu on demand |  | complete paste and error on interruption |  | Pending |
| T0 mutual TLS rejection |  | Client or server footprint rejection |  | Pending |

Failure conditions that invalidate a row: different hash, incomplete file submitted as finished,
overwrite without notice, lock without error, or "correct" publication without the browser
requesting the content.
