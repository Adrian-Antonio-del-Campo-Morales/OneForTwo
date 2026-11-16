> Files, current status: T1 complete; T2 CLI reception and integrity, TLS negatives, corte/reintento
> and Windows cases exceeded. Paste Explorer/bajo demands pending by desktop helper. See
> files/RESULTS.md.

# Feasibility experiments

Updated: ** November 5, 2026**. The three prototypes are compiled in Ubuntu/Wayland with their
dependencies and TLS. Local phase available of clipboards and files completed, with recorded
limitations. In input they passed shortcut, demonstration EIS, R2 and R9 local with a monitor,
output 0 and confirmed operating desktop. The previous incident that required restarting
(EXP-INPUT-02) is retained. Inter-team testing has not been initiated.

The Ubuntu `2ae444d` delivery has also been reconstructed on Windows in the `validation/windows`
branch, with no source changes: compilation with OpenSSL and overtaken self-tests, 149/0 clipboards,
176/0 files and 579/0. input The help of the three binaries works. Windows desktop and
inter-computer tests are still pending. `main` still does not contain these branches.

## Consolidated status

| Experiment | Recorded isolated evidence | Main Earrings |
| --- | --- | --- |
| [Clipboard](clipboard/README.md) | Ubuntu portal + TLS: **153/0**. Reading, pasting, observation, `devices=0` permission and cancellation. Insulated copy: a sample notification and 12 s without repetitions. Windows 149/0 and Linux without historical 149/0 portal | Echoes, real reconnection and synchronization between teams |
| [Archives](files/README.md) | GTK3 + TLS: **176/0** previous. Paste assisted in Nautilus: 5 archivos/5.551.150 bytes in full. L6/L7/L8 reais with TLS local exceeded after correcting the temporary names of reception. Windows 176/0 historical | Windows Transfers and Comparisons. Automatic publication and on demand Linux: recorded limitations |
| [Key and mouse](input/README.md) | Wayland GDBus/libei + TLS, **579/0** in Ubuntu and Windows reconstruction. Diagnosis, cancellation, shortcut, demonstration and local R2/R9 Ubuntu exceeded with an operating monitor and desktop | Cruce/retorno, complete keyboard and inter-equipment cuts. Ubuntu source locked by InputCapture v1 versus v2 requirement. X11 without new evidence |

Entry: isolated shortcut and subsequent demonstration RemoteDesktop/EIS verified with the user
present, guide and 30 s timer. Text `oneintwo`, short movement and desktop responding confirmed;
output 0. Supervisor tested with innocuous process and 3 essential regressions; the general battery
was not repeated. R2/R9 also exceeded. Cuts and states held between teams remain pending; repairing
a frozen image is not guaranteed (EXP-INPUT-02).

R2 was then tried with a minimum standby mode after the demonstration: rejection of several EIS
regions before injecting, output 1; timer of 30 s correct. After the pause, the user resumed with a
monitor: R2 and R9 passed with EIS active, output 0 and desktop responding. Planned local phase
completed; next phase, Windows coordination.

The counts describe checks of different nature and do not certify desktop support. The historical
Windows evidence is preserved, along with the isolated reconstruction of the Ubuntu delivery
described above. The 403/0 input evidence corresponds to the previous delivery to Wayland and does
not describe the entire current battery.

New environment: Ubuntu 26.04.1, inherited session GNOME Wayland, GCC 15.2, CMake local 4.2.3 and OpenSSL 3.5.5. Base Git `450cbb2e340a381bda01274eb86df0e05934e822`, branch `validation/ubuntu`, plus local clipboard corrections: `<signal.h>` → `<csignal>`, waiting for non-blocking pipes, publication life and log emptying; action GTK local and file format of Nautilus; preparation and local output of the input demo, callback/resultado GDBus correction, RemoteDesktop session type and shortcut path. Local units in `.local-state/ubuntu-validation/`, within the project and excluded from Git, without global installation. They moved since `/tmp` and repeated the compilations and self-tests with the new routes. To load the environment from root: `source .local-state/ubuntu-validation/env.sh`. Configuration, compilation and final self-tests of the three experiments: output 0. The reports detail components actually included, commands, versions and initial attempts that did not validate the full variant.

Local sessions of the clipboard portal have been executed; the user observed the pasting in GNOME
Text Editor and Nautilus. The input diagnosis already consulted the real interfaces. Only TLS was
tested of files in loopback; there is no connection with Windows. For the following cases, prepare
data, user presence and recovery; before connecting Windows, agree shared review, extremes, port,
identities and specific case. Delivery branch: `validation/ubuntu`. The product integration remains
postponed.

| Experiment | Manual instructions | Evidence and limitations |
| --- | --- | --- |
| Clipboard | [MANUAL.md](clipboard/MANUAL.md) | [RESULTS.md](clipboard/RESULTS.md) |
| Files | [MANUAL.md](files/MANUAL.md) | [RESULTS.md](files/RESULTS.md) |
| Keyboard and Mouse | [MANUAL.md](input/MANUAL.md) | [RESULTS.md](input/RESULTS.md) |

Recommended order: inventory the actual machines, complete the pending native compilations and
locally test text, paste files and input; then repeat between computers. The wrong identity and
recovery cases are part of each validation. The goal of clipboards remains to work with inactive
input capture.

Problems and open checks are kept in [TODO.md](../TODO.md).

## Working model

The agents have worked on independent prototypes and unique directories. The next stage is user
manual execution on Windows and Ubuntu, an experiment every time. The following rules continue to
apply to the individual corrections and checks that will be commissioned.

** Agents will not run tests that read or write the actual clipboard, capture or inject input, open
desktop permission sessions, or publish files in the browser.** They will also not boot network
listeners or connect computers during this phase. They do not need to coordinate desktop usage
shifts.

| Encargo | Dedicated directory | Design |
| --- | --- | --- |
| Clipboard | `experiments/clipboard/` | [Charge of agent](clipboard/README.md) |
| Keyboard and Mouse | `experiments/input/` | [Charge of agent](input/README.md) |
| Files | `experiments/files/` | [Charge of agent](files/README.md) |

[Common instructions for agents](AGENTS.md) are part of the three commissions. The documentary
update does not initiate agents or demonstrations.

## What it means to deliver a complete experiment

The agent will deliver functional code of the prototype, independent construction, isolated checks,
an execution manual and a status report. It is not enough to leave pseudocode or functions that
return success without doing anything.

Complete delivery does not mean validated on both systems. Each result will separate:

1. Implementado.
2. Built, with the platform and tools recorded.
3. Checked in isolation, no desktop, no network.
4. Pending manual execution.
5. Blocked or unsupported, with explanation and evidence.

If an Ubuntu or Windows environment is missing, the agent will continue with what it can implement
and verify. It will not affirm compilation or operation on the missing system. A limitation will not
be replaced by a simulated backend presented as real.

## Structure of each delivery

The directories already contain design, code, compilation configuration, manual and results. Your
general organization is:

```text
<experimento>/
├── README.md           Experiment design and decisions
├── RESULTS.md          Status, evidence, and pending manual checks
├── MANUAL.md           Step-by-step instructions
├── CMakeLists.txt      Standalone build
├── src/               Prototype and required native adapters
├── tests/             Isolated checks and synthetic data
└── build/             Salida local ignorada
```

The specific internal structure is explained in each README. Prototypes do not depend on the root
CMake, `src/` of the product or another experiment. Any correction will maintain that independence
and the responsibilities of each file.

Prototyping uses C++17. Documented general libraries, not engines of other applications, have been
used. The final choice of product dependencies and their license is still pending. Agents will not
install tools globally or modify other projects to resolve dependencies; the facilities indicated in
the manuals are explicit user steps.

## Separate execution and verification

Starting without arguments and `--help` will only show help. Each prototype will offer `--self-test`
or an equivalent test executable that uses synthetic and double local data, without initializing the
desktop or opening network.

The actual functions will be activated by explicit commands documented in `MANUAL.md`. The inventory
of the environment will also be a manual mode when you need to consult a graphical session.
Compilation scripts will not automatically run demonstrations.

Prototypes may include minimal direct communication by explicit direction for testing between teams.
Each agent will own that implementation within their folder. They will use standard
autenticado/cifrado transport and general libraries, without building discovery, persistent linking
or a common production protocol. There will be no shared network file that all three must edit.

System permissions, certificates or test keys will be explicitly prepared during manual execution
and will be left out of the repository. The manual will explain the identity check; it will not
quietly disable channel validation.

## Mandatory content of the manual

- Tools and dependencies needed in each system.
- Accurate compilation and execution commands; real binary names.
- Preparation of a folder and disposable data.
- Numbered steps to test locally and between the two teams.
- Expected permissions, observable result and failure conditions.
- Way to stop the prototype and restore the state.
- Matrix where the user can record results without confusing them with automatic tests.

The user will first perform local tests and then two teams tests. There will be only one active
experiment in the same graphical session during those tests.

## Relationship to the preliminary study

- P0: Each manual will collect the relevant inventory of your platform.
- P1: Input experiment.
- P2: clipboard experiment.
- P3: file experiment.
- P4: Recovery cases included in the input manuals and files.
- P5: congruence between transfer and control, postponed to subsequent integration; does not require
  coupling of these prototypes.

The useful code can be moved to the product after validating results and reviewing its design.
Integration will be a later work, with a single responsible.

