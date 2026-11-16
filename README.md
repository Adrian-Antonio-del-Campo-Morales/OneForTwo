# OneInTwo

Own implementation desktop application for sharing clipboards, files and keyboard and mouse input between two
computers on a local network, with Windows and Ubuntu as initial targets.

## Project status

Updated: **November 5, 2026**. The repository contains the product architecture and three
independent prototypes implemented in `experiments/`: clipboard, files, and keyboard and mouse input.

The integrated application is not yet implemented: `src/` retains descriptions of responsibilities
and design contracts in C++, the CMake root is a commented reservation and there are no installers.
Each experiment has its own code, CMake, dependencies, isolated testing, manual and results report.

Prototypes use C++17 and general libraries documented in their reports. The application's graphical
library, minimum verified versions and license are still pending. The experimental code has not been
automatically integrated into the product.

| Experiment | Summary statement | Documentation |
| --- | --- | --- |
| Clipboard | Windows and Wayland/TLS compiled; verified local Ubuntu reading, pasting and observation; synchronization between pending computers | [Manual](experiments/clipboard/MANUAL.md) · [Results](experiments/clipboard/RESULTS.md) |
| Files | Windows and Ubuntu/GTK/TLS compiled; pasted assisted and cases TLS local Ubuntu verified; transfers between pending equipment | [Manual](experiments/files/MANUAL.md) · [Results](experiments/files/RESULTS.md) |
| Keyboard and Mouse | Windows and Wayland/TLS compiled; self-tests 579/0; local Ubuntu injection and outputs verified with a monitor; Ubuntu source blocked by InputCapture v1 | [Manual](experiments/input/MANUAL.md) · [Results](experiments/input/RESULTS.md) |

The local phase available in Ubuntu has observed results and recorded limitations. Ubuntu delivery
has been rebuilt on Windows without repeating its desktop. Windows desktop and cross-team testing
are still pending. The detail of each case and the historical entry incident is in the `RESULTS.md`
and `TODO.md`; no support is deducted from a platform from the mere existence of code.

## Documentation

- [Preliminary study](PRELIMINARY_STUDY.md): requirements, feasibility and restrictions.
- [Proposed architecture](ARCHITECTURE.md): components, relationships, flows and structure.
- [Proposed functions](FEATURES.md) brings together submissions, profiles and screen
  designs, window and notifications.
- [Provisioned tests](tests/README.md): behavioural checks.
- [Experiences of feasibility](experiments/README.md): status of prototypes and manual validation order.
- [Open issues](TODO.md): Problems and verifications still open.
- [Validation in Ubuntu](UBUNTU_VALIDATION.md): Order to continue on the second machine and coordinate tests between teams.
- [First test LAN clipboard](experiments/clipboard/lan/README.md): public identities, preparation of
  both ends and execution matrix.
- [Windows distribution](packaging/windows/README.md) and [Ubuntu distribution](packaging/ubuntu/README.md).

## Responsibility for this file

Serve as an input to the repository and accurately describe its status. When an implementation
exists, collect the verified build and run steps, verified platforms, and the license chosen.

