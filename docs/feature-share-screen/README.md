# Feature `share-screen`: session, pairing and shared screen

This document describes the delivery of branch `feature/share-screen`, which in the [parallel
implementation](../PARALLEL_IMPLEMENTATION.md) corresponds to the package **P**. It is not the
general README of the product nor a declaration that the complete application is finished.

Reference status: branch `feature/share-screen`, commit `3efa340`, local suite **10/10** and
workflow Windows/MSVC in green. Delivery contains common session base, identity, matching, protocol,
transport TLS, composition and visual module. Native desktop capture and notifications belong to
branches V and N, respectively.

## Capacity included

| Capacidad | State in this branch |
| --- | --- |
| Persistent TLS identity per team | Implemented and tested |
| Pairing with code SAS of six figures | Implemented and tested |
| Certificate re-matching and rotation | Implemented and tested |
| Pair store with main pin and backrest | Implemented and tested |
| TLS transport with a pinned SHA-256 fingerprint | Implemented and tested |
| Authenticated session with generations and permissions | Implemented and tested |
| Protocol of functions version 1 | Implemented and tested |
| Visual module: invitation, decision, status, images | Implemented and tested with synthetic capture |
| Qt Screen Viewer and Composition | Implemented and tested |
| Pairing dialog and main window | Linked in the application of tests |
| Native catch of monitor/ventana | Pending integration from V |
| Native notifications | Earrings to integrate from N |
| Complete Product Executable | Skipped in this branch |
| Automatic equipment discovery | Not implemented |
| Audio, recording, remote input control | Out of the reach of this delivery |
| Installer, signature and license | Pending product decision |

### Emparejamiento

- Each team generates a 3072-bit RSA identity and a ten-year self-signed certificate by libcrypto.
  The identity is identified by a `device_id` 32-character hexadecimal and by the SHA-256 print of
  its certificate.
- The dialog allows you to listen or connect manually to a host and port. There is no network
  discovery.
- During the ceremony, TLS with unverified certificate is used and a SAS of six figures is calculated from the transcript. Both people must compare the same SAS and confirm. Until then, the certificate is not trusted.
- The track observed is saved when the ceremony is completed. Later connections use TLS with the pin
  fixed and reject any other certificate.
- If an existing computer is rematched, name, host and port are updated and the new pin remains the
  main one. The previous pin is preserved as a temporary backup.
- The explicit rotation of the certificate of this equipment generates a new identity and forces to
  rematching. The previous key is not reused.

### Session and protocol

- The session is set at a `QtTlsTransport` already authenticated. The greeting exchanges identity
  and generation; the reception rejects repeated or old generations and each connection receives a
  new local generation.
- The session applies permissions by sense and modality. Profiles `usual`, `sends_only` and `paused`
  limit effective capabilities; an invalid saved profile is interpreted as pause.
- The interactive channel carries control and small messages. The data channel transports fragmented
  visual units. The plots have channel, length and payload; the receiver validates version, type,
  length and channel before dispatch.
- The function protocol is version 1. Visual messages include operating reference, source review,
  sequence and image metadata. There is no negotiation of future versions or backward compatibility.
- Operating identifiers start at 1 per generation and sense. An overflow ends the session before
  reusing an identifier.

### Share Screen

- The visual module maintains a single visual operation per session, counting pending invitations
  and active operations in both directions.
- An invitation expires at 30 seconds. The font can be monitor or window; the native adapter decides
  which fonts it actually supports.
- The receiver must accept. Capture does not start before acceptance and the operation is
  invalidated if the generation changes, permission is withdrawn or the source is lost.
- The images are coded as JPEG independent, with initial quality 75 and reduction of
  quality and resolution if necessary. The first installment does not use interframe video.
- Implemented limits: 30 fps, 1920 pixels per side, 2.073.600 pixels per image and 1 MiB per encoded
  unit. The receiver validates the dimensions before reserving the decoded image.
- The viewer emptys the presentation by suspending or closing the source. The operation ends if the
  activity signals are lost; the sender sends activity every second and the receiver tolerates three
  seconds without signals.
- Local or remote closure invalidates operation, cancels jobs and discards late callbacks. A remote
  closure does not generate another response closure.

## Implementation Details

### Identity and storage

`DeviceIdentityStore` saves identity in the configured directory:

```text
identity/
├── identity.id    # device_id hexadecimal
├── identity.crt   # certificado PEM
└── identity.key   # clave privada PEM
```

In POSIX the directory is created with owner permissions and the key is written with
lectura/escritura only for the owner. Identity is generated with libcrypto, not invoking the
`openssl` binary.

`PairingStore` saves the pairs in a TSV with `oneintwo-pairing-v1` header:

```text
device_id <TAB> name <TAB> host <TAB> port <TAB> primary_pin [<TAB> backup_pin]
```

- A pair can have one or two prints; the main one always goes first.
- Writings use `QSaveFile` (single time and atomic replacement) and, in POSIX, `fsync` of the
  archive and directory. `save`, `remove`, `rotatePin` and `retireFallback` publish the status in
  memory only after persisting.
- `reload` retains the last valid state if the file is corrupt. If corruption is detected when you
  build the application, it shows the error and disables the peer storage; there is no automatic
  recovery.
- The backrest is removed after the main print is authenticated by TLS. It is not used as an
  automatic connection fallback: the application always tries the main pin, and the backrest exists
  for the re-matching transition.
- `pinTrusted()` accepts principal and support for compatibility. For authorization decisions
  requiring active identity, `primaryPinTrusted()` or `pinRole()` should be used.

`LocalStore` is different: it saves allowed profiles and applications and still uses a fixed time
frame with `std::filesystem::rename`. It does not have the same guarantees of transaction and
durability as `PairingStore`; it is not appropriate to assume that both persistences behave the
same.

### Transporte TLS

- Qt `QSslServer`/`QSslSocket` with TLS 1.2 or later. Trust is based solely on the SHA-256 of the pair certificate; there is no CA, host name or string validation.
- In normal mode the footprint is mandatory. The server also fixes the client's footprint.
  `allow_unverified` mode is reserved for the SAS ceremony.
- A transport supports a single active connection. The server closes the listener after accepting a
  connection and the handshake has 10 seconds.
- Buffers and plots are narrowed down. TLS and identity errors do not expose the footprint observed
  as if it were an authenticated identity.

### Composition of the application

- `Application` loads identity and pairs, builds the window, coordinator and session, and reuses the
  last saved pair when booting.
- `Coordinator` connects the UI to the session, the visual module and the notices. Sharing actions
  are only authorized with established session and effective permission.
- `SessionLink` is responsible for the greeting of identity and generation and the authorization of
  messages. The profile is applied in the session, not in the IU.
- `ModuleRuntime::post` glues work on the main thread of Qt. UI callbacks, session and modules
  should be executed in that context; they should not be blocked with network or disk operations.

### Construction and testing

Requirements: CMake 3.24 or later, compiler C++17, Qt 6.4 or later with Core/Gui/Widgets/Network and
OpenSSL Crypto. In Windows the workflow uses Qt 6.8.3 (`qtbase`) and Visual Studio 2022 x64.

```bash
cmake -S . -B build/root \
  -DONEINTWO_BUILD_TESTS=ON \
  -DCMAKE_PREFIX_PATH=/ruta/a/Qt/6.x/gcc_64
cmake --build build/root -j 4
QT_QPA_PLATFORM=offscreen \
  ctest --test-dir build/root --output-on-failure
```

The root suite contains ten CTest tests:

```text
oneintwo_protocol
oneintwo_session_link
oneintwo_tls_transport
oneintwo_pairing
oneintwo_visual_share
oneintwo_visual_codec
oneintwo_visual_viewer
oneintwo_composition_harness
oneintwo_pairing_ui
oneintwo_application
```

The workflow [windows-msvc.yml](../../.github/workflows/windows-msvc.yml) installs Qt, configures
the project and compiles `oneintwo_application_tests`. Then it requires CTest to register exactly
`oneintwo_application` and the test passes. It is a validation of the Application link and boot on
Windows, not native capture.

## File Map

| File | Responsabilidad |
| --- | --- |
| `src/net/device_identity.*` | Generation, load and rotation of identity TLS |
| `src/net/pairing_session.*` | SAS ceremony and transition to persistent confidence |
| `src/net/tls_transport.*` | Transportation TLS with pin fixed and channel framing |
| `src/net/session_link.*` | Greetings, generations, permits and authorized dispatch |
| `src/net/protocol.*` | Encoding and decoding feature messages |
| `src/storage/pairing_store.*` | Pairs, pins principal/respaldo and atomic persistence |
| `src/storage/local_store.*` | Permitted Profile & Applications Preferences |
| `src/features/visual_share.*` | Status and rules of visual operation |
| `src/features/visual/visual_codec.*` | Scale, JPEG and image validation |
| `src/ui/visual_viewer.*` | Presentation of the shared image |
| `src/ui/pairing_dialog.*` | Pairing and rotation dialogue |
| `src/app/application.*` | Actual IU composition, session and storage |
| `src/app/coordinator.*` | Connection between IU, session and modules |
| `src/platform/desktop.hpp` | Native catch contract and notifications |
| `tests/*` | Protocol tests, TLS, pairs, IU, composition and application |

## Known problems and important factors

### The branch is not the complete product

The `oneintwo` executable is only created if there are both `oneintwo_notifications` and a native
capture target. In this branch those targets are not part of the composition; the application is
tested with capture doubles and warnings. Therefore:

- The suite verifies contracts, protocol, TLS, composition and boot, not a real desktop capture.
- It should not be said that sharing screen works from end to end until you integrate V and N and
  test both operating systems.
- `tests/application/application_test.cpp` stubs are not a product implementation and should not be
  linked to the executable.

### Capture and notifications belong to other branches

The ownership of the native adapters is defined in [the parallel
base](../PARALLEL_IMPLEMENTATION.md). When integrating:

- V provides the catch Windows/Wayland and its native tests.
- N provides notifications and their adapters.
- P retains the composition, visual contract, protocol and session.
- Transports and protocols should not be duplicated to “run” an isolated branch; integration should
  use common headers.

### No discovery or automatic connection

Pairing requires host and port. The application does not search for peers on the LAN, does not
resolve NAT or maintain automatic reconnection. If the peer changes direction, the pair needs to be
updated.

### The generation of the pair does not persist between processes

`SessionLink` accepts callbacks to remember the last generation of the peer, but `Application` does
not connect them to a warehouse. The protection against repeat generations is effective within the
process; after restarting the application, the local counter restarts and the peer does not have a
persistent brand. It is an outstanding improvement if replay resistance is required between
restarts.

### Backup is not an automatic failover

- The app always uses the main pin to connect.
- The backrest is retained only for the repair window and is removed when a TLS connection
  authenticates the main one.
- If the deed from the vault fails to remove the backup, the session continues and the backup can
  stay; the error is displayed, but the connection is not blocked.
- `pinTrusted()` still accepts both pins for compatibility. Do not use it to decide the active
  identity; use `primaryPinTrusted()`.

### Persistence and attendance

- `PairingStore` is intended for a single writer and does not have mutex. You should not write the
  same file from several processes or threads at once.
- `QSaveFile` avoids timeshares and partial replacements, but does not implement a lock between
  processes or resolve lost scripts if two instances load and save at the same time.
- `LocalStore` still uses a fixed term and does not have the guarantees of `PairingStore`.
- If the torque storage is corrupt when booting, `Application` disables it and shows the error.
  `reload()` on an already loaded instance retains the last valid state, but recovery after corrupt
  booting is manual.

### Seguridad

- Pin is the root of trust. There is no CA or host name; adding host validation without redesigning
  the rotation can break the model.
- `allow_unverified` is only valid within the SAS ceremony. It must never be enabled for a normal
  session or to avoid a pin error.
- Changing the certificate requires resetting; an old private key should not be reused.
- `tests/tls/test_certificates.hpp` certificates and keys are test material. They must not be used
  in a real installation.
- The private key must be left in the user application directory, with restricted permissions. Do
  not register keys, screen content or sensitive data.

### Protocol limits

- Version 1 is the only supported version. Changing types, fields or limits without uploading the
  version breaks the compatibility.
- The interactive and data channel have strict limits. Validate lengths before booking memory and do
  not expand buffers without checking the receiver.
- A visual operation uses JPEG independent images; there is no audio or recording.
- Invitations expire and the session does not automatically resume an operation after a
  disconnection or change of generation.

### Several saved pairs

`PairingStore` supports multiple pairs, but `Application` reuses `devices().back()` when booting. If
multiple computers are saved, booting does not offer an explicit selection; this behavior must be
reviewed before supporting multiple pairs in the product interface.

### Platform differences and coverage

- Linux was locally validated with the full suite; Windows/MSVC validates the Application link and
  boot in IC. The native capture of each platform requires evidence of V.
- No validated support for macOS.
- The UI test uses `QT_QPA_PLATFORM=offscreen`; it does not replace a real desktop test.
- The license, installer and signature remain product decisions.

## Before modifying this feature

1. Read [parallel implementation base](../PARALLEL_IMPLEMENTATION.md) and [screen sharing
   design](../SHARE_SCREEN.md).
2. Respect file property: P should not absorb native V adapters or N notifications.
3. Maintain public signatures and protocol limits; any contract change should be coordinated with
   the integrator.
4. Add contract or session tests to the corresponding suite. For UI changes, run with
   `QT_QPA_PLATFORM=offscreen` and do not confuse that execution with a visual validation.
5. Do not add stubs to the product executable. Doubles belong to `tests/`.
6. Verify the Windows workflow when the change affects the Application link.

## Limitations of this delivery

This branch leaves the P-base ready to integrate, but does not close the product functionality:
missing native capture, notifications, complete executable, discovery, multiple pair selection and
desktop cross-team testing. Current evidence is the local suite and the Application linking and startup
on Windows.
