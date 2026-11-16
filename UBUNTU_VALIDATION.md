# OneInTwo — Ubuntu Validation Order

Ready: November 5, 2026.

Responsibility: allow the Ubuntu machine agent to continue pending verification with the same code as Windows. The shared repository is [OneForTwo](https://github.com/Adrian-Antonio-del-Campo-Morales/OneForTwo); the internal name of the product and executables remains OneInTwo.

## 1. Prepare working copy

If there is still no local copy:

```bash
git clone https://github.com/Adrian-Antonio-del-Campo-Morales/OneForTwo.git
cd OneForTwo
```

If a copy already exists, check first branch and local changes; update without overwriting work.
Register the full revision identifier that is checked:

```bash
git status --short --branch
git rev-parse HEAD
```

Both machines must use the same revision during inter-team testing. `~/OneInTwo` routes in the
manuals are examples; after cloning in `OneForTwo`, adjust the routes to the actual directory. Do
not compile with the CMake root: each experiment has its own compilation.

To avoid mixing corrections with Windows copy, work preferably on a local branch `validation/ubuntu`
created from the shared review. Do not upload binaries, caches, downloaded dependencies, keys or
personal files.

## 2. Documentation and scope of the new commission

Read [AGENTS.md](AGENTS.md), [TODO.md](TODO.md), [state of experiments](experiments/README.md) and the README, MANUAL and RESULTS Consult study and architecture to interpret requirements, without beginning product integration.

The instructions of [experiments/AGENTS.md](experiments/AGENTS.md) describe the previous implementation phase without desktop testing. For this assignment, the user deliberately starts a new validation phase in Ubuntu:

- The three experiments are worked out, one at a time, and their reports. The scope is not limited
  to a single experimental folder.
- Inventory, compilation and self-tests are the first stage.
- Local desktop tests are performed in the graphical user test session, with disposable data,
  permissions granted by the user and ready recovery.
- Input capture or any other operation on Windows requires coordination with that computer; a test
  of two machines is not initiated unilaterally.
- The global installation of packages, lifts and firewall changes are not authorized by this
  document. If necessary, indicate exactly what the user should prepare; continue with the
  independent parts.
- The remaining restrictions remain: do not use engines from other applications, do not expand the
  product, do not publish secrets, do not run broad campaigns or modify other people's history.

This document accompanies the user's order to the agent; it does not constitute an independent
permission to control sessions or machines outside the company. If only SSH or a session without a
desk are available, complete the first stage and record the graphical tests as pending.

## 3. Initial stage: inventory and complete compilation

Register in the existing `RESULTS.md`: Git revision, date, Ubuntu, desktop, Wayland/Xorg session,
compiler, CMake, OpenSSL, dependencies and versions of portals that can be checked. Do not interpret
the name of the distribution as evidence of availability of a API.

Work Order: clipboard → files → input. Reuse documented Windows evidence; do not repeat
external tests to complete Linux reports.

### Clipboard

Development units to check: C++17 compiler, CMake, pkg-config, libdbus and OpenSSL.

```bash
cmake -S experiments/clipboard -B experiments/clipboard/build/ubuntu \
  -DCMAKE_BUILD_TYPE=Release -DOIT_ENABLE_PORTAL=ON -DOIT_ENABLE_PEER=ON
cmake --build experiments/clipboard/build/ubuntu -j
```

Check that the configuration includes the portal adapter and OpenSSL transport. These options may
produce a limited variant if dependencies are missing: CMake's completion does not show that both
components are included.

Since `experiments/clipboard`, run only the self-test initially:

```bash
./build/ubuntu/oneintwo-clipboard --self-test
```

Previous reference: 151 Linux checks with portal. The Linux compilation with TLS still has no
evidence. Register the actual result; do not reuse the figure of a different variant.

### Files

Check GTK3 and OpenSSL in addition to common tools:

```bash
cmake -S experiments/files -B experiments/files/build/ubuntu \
  -DCMAKE_BUILD_TYPE=Release -DO2F_ENABLE_GTK=ON -DO2F_ENABLE_TLS=ON
cmake --build experiments/files/build/ubuntu -j
```

Check that GTK3 and TLS were included: both dependencies can be omitted with warning. From
`experiments/files`, so that the self-test storms are left inside your folder:

```bash
./build/ubuntu/oneintwo-files --self-test
```

Windows Reference: 176 checks. No previous evidence of build of the Linux adapter. Do not close
validation by compiling the alternative `clipboard_unavailable`.

### Keyboard and Mouse

In Wayland, check GIO, libei, pkg-config and OpenSSL. Force target backend:

```bash
cmake -S experiments/input -B experiments/input/build/ubuntu-wayland \
  -DCMAKE_BUILD_TYPE=Release -DONEINTWO_WAYLAND=ON \
  -DONEINTWO_X11=OFF -DONEINTWO_WITH_TLS=ON
cmake --build experiments/input/build/ubuntu-wayland -j
```

From `experiments/input`:

```bash
./build/ubuntu-wayland/input_self_test
```

Windows isolated reference: 538 checks, including Wayland streams with doubles. It does not
demonstrate the compilation or execution of the native Linux adapter. If the actual session is Xorg,
document it; the manual contains an alternative X11, which does not replace Wayland validation.

### Treatment of faults

If an error appears, keep the relevant command and message, investigate its specific cause and
correct only the compilation or operation blocks of these experiments within the order. Repeat only
the affected checks. If the lock is a dependency or absent capacity, register it and advance to the
next independent experiment. Do not modify the test conditions to convert a real failure to success.

## 4. Local stage: Ubuntu desktop

Follow the [paper clipboard manual](experiments/clipboard/MANUAL.md), the [file](experiments/files/MANUAL.md) and the [entry](experiments/input/MANUAL.md), one at a time. Before a test that changes the desktop, leave the disposable data, output and permissions to be granted by the user.

1. Clipboards: reading, publishing, pasted in editor, background observation and denied permission.
   Check that input devices are not granted and that it works with inactive capture.
2. Files: generate synthetic data, post cache and actually paste into Nautilus. Compare size and hash,
   structure and names. Distinguish bytes transferred from correct pasting.
3. Entry: inventory of portals, permissions, local shortcut and limited injection demonstration in a
   disposable window. Do not enable capture if there is no reliable local recovery. Do not confuse a
   stream with doubles with the composer's real response.

System dialogs may require user intervention. If the agent cannot observe the operation in the
editor or browser, prepare the step for the user and record his or her observation as such. Do not
mark a case as exceeded based only on the output of the program.

## 5. Inter-team stage: coordinate before connecting

It requires the user or agent of the other computer to prepare Windows. Before launching each case
agree:

- Git and experiment review, executables and dependencies of each end.
- Addresses and specific port; origen/destino and boot order.
- Own certificates and fingerprint check. Exchange public certificates or fingerprints, never
  private keys.
- Test window or folder, duration, emergency exit and result to be observed.

First check accepted and rejected identity without remote control; then text synchronization,
copying of files and finally entry with short sessions. Register both senses. Input recovery is
checked only when the local output has already been verified.

Until the other end is ready, these cases remain pending. GitHub shares sources and results; it is
not the transport channel of prototypes.

## 6. Delivering results

Update existing `RESULTS.md` without erasing Windows evidence or attributing Linux results to it.
Separate:

1. Environment and verified review.
2. Implemented and, if there were corrections, what changed.
3. Native compilation with the components actually included.
4. Self-tests aislados.
5. Desktop tests observed.
6. Tests of two teams observed or pending.
7. Concrete blockades, proposed decisions and next step.

Update [TODO.md](TODO.md) and the [summary of experiments](experiments/README.md) Do not create parallel reports, unify protocols, or transfer code to the product during this order.

To exchange corrections and results for Git, keep an identifiable review and agree to publish it
with the user. Do not overwrite or automatically integrate the work of the other machine. The
validation branch allows you to review those changes from Windows.
