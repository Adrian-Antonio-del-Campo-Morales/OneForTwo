# Clipboard experiment

For the first connection between Windows and Ubuntu, follow the [LAN](lan/README.md) preparation.
Includes identity and command wizard, Windows public certificate and synthetic samples; does not
start connections automatically.

## Ordering for the agent

Implement this experiment by following [common instructions](../AGENTS.md) and [delivery
model](../README.md). Your unique directory is `D:/DEVEL/OneInTwo/experiments/clipboard`. Tests with
the actual clipboard will be executed manually by the user.

## Question to Solve

Can we share text between Windows and Ubuntu, with the app in the background and without activating
the keyboard and mouse control? What permissions and capabilities does each environment require?

The goal is to test Unicode text. Images, history, enriched formatting and files are left out of
this experiment.

## Interface design

A console executable with four small pieces:

1. Windows Adapter: observe, read and publish text.
2. Adapter Ubuntu/Wayland: request the available authorised route and communicate its limitations.
3. Common logic: identify changes, delete echo and resolve order of operations.
4. Minimum direct communication to connect two instances during manual testing.

Propose explicit modes for diagnosis, local observation, publication of synthetic text and
connection between two teams. The observation mode should show sufficient status and metadata for
diagnosis, without dumping private content by default.

The test reader and writer should not capture keyboard or mouse to appear independent. If access to
the clipboard requires a session associated with another capability, document exactly the requested
authorization and limitation. Do not grant functional remote control just by sharing text.

## Flow between equipment

A local copy generates an identified transaction. The other instance applies it and recognizes the
resulting notification as its own script. Prevent that notification from returning indefinitely to
the source. Define deterministic order to concurrent copies and do not automatically forward old
text when reconnecting.

The connection uses explicit addresses, without discovery. Channel authorization follows common
rules; do not build a product linking screen.

## What the agent will be able to check automatically

- Unicode and validation of sizes with synthetic chains.
- Duplicate operations and loop prevention using fake adapters.
- Ordering of concurrent copies.
- Rejection of operations from a previous session.
- Access States granted, denied and not available simulated.

Doubles do not replace native delivery adapters.

## Screenplay to be prepared for manual execution

1. Get versions and session capabilities.
2. Copy a test text into an editor and observe the change.
3. Publish synthetic text and paste it into the editor.
4. Repeat with another focused program, no active experiment window.
5. Repeat without active input capture; record permissions and restrictions.
6. Connect Windows and Ubuntu and try both senses.
7. Try accents, emoji, line breaks and quick copies.
8. Disconnect, reconnect and remove permissions; check errors and absence of unexpected old text.

## Deliverable and evaluation criteria

Prototyping code, independent compilation, self-tests, MANUAL.md and RESULTS.Md. The report will
include a table by system and session: reading, writing, background observation, uncaptured operation
and required permissions.

The feasibility conclusion will be pending until manual testing. If a desktop track does not provide
independent access, report it as a limitation; do not quietly replace it with a manual mode and
declare the objective to be met.

The expected architectural decision is what contract and permissions the clipboard adapter needs.

## Implementation delivered (4 November 2026)

The prototype is implemented in `src/`, with auto-tests in `tests/selftest.cpp` and independent
compilation in `CMakeLists.txt`. Reproducible instructions are in [MANUAL.md](MANUAL.md) and the
evidence-separated state in [RESULTS.Md](RESULTS.md).

Piezas:

- `src/model.*`: UTF-8 validation, boundaries, end of line and logical order Lamport with tiebreaker
  by origin.
- `src/engine.*`: sending local copies, remote application, copying by marker and fingerprint,
  duplicates, delays and session generations.
- `src/protocol.*`: wefts with length and limits, messages Hello/Clipboard/ Applied/Error/Bye and
  structural validation.
- `src/platform/windows_clipboard.cpp`: `CF_UNICODETEXT`, notifications with
  `AddClipboardFormatListener` in a message only window and marker in a private format. Do not
  capture or inject input.
- `src/platform/linux_clipboard.cpp`: Choose Wayland → portal and X11 → explicit limitation; do not
  substitute Wayland for X11 nor pretend to read or write.
- `src/platform/portal_clipboard.cpp`: Wayland real adapter over `xdg-desktop-portal` RemoteDesktop + Clipboard, with libdbus. Requests `SelectDevices(types=0)`, requires `clipboard_enabled=true`, rejects any non-zero input mask and does not call `Notify*` or `ConnectToEIS`. Only text UTF-8; does not capture screen.
- `src/net/*`: direct connection with TLS 1.2+, certificate of torque set by SHA-256 and protocol messages. No discovery or persistent confidence.
- `src/cli.cpp`: explicit modes; no arguments only shows help.

Verified status: Windows (MSVC/OpenSSL 3.5) compilation and Linux compilation on Ubuntu 20.04 WSL, including Wayland adapter compilation with libdbus (151 isolated checks) and portal-free compilation (149). The actual clipboard tests, portal dialogue, desktop and two computers are pending. Wayland has built-in adapter; X11 is not implemented and is not used as a silent substitute. The Linux compilation with TLS was not verified for lack of `libssl-dev`. No desktop validation is declared or P2 closed without manual testing of [MANUAL.md](MANUAL.md).

Native Linux Dependency: `libdbus-1-dev` (and `libssl-dev` for inter-team mode). The compilation commands separated by kernel, Wayland adapter and manual validation are at [MANUAL.md](MANUAL.md); evidence and primary sources are at [RESULTS.md](RESULTS.md).

