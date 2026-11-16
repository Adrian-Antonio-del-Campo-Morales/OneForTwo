# Instructions for experiment agents

## Scope and ownership

These instructions apply to all three experiments. Your assignment must identify exactly one of `clipboard`, `input`, or `files`. Read that directory's README and the shared README before starting.

Modify only the subdirectory assigned to you, including code, tests, documentation, permitted local downloads, and build outputs. Leave this file, the shared README, the preliminary study, the architecture, the root CMake project, product code, and the other experiments untouched. Describe any required shared change in your RESULTS.md.

Keep the existing branch and repository state. The assignment does not include commits, branch changes, repository-wide cleanup, or operations on another owner's files. Preserve existing work.

## Phase objective

Implement the prototype and prepare it for the user's manual run. The assignment authorizes compilation and isolated tests of your code. Leave the real desktop and test network communication untouched.

In particular, leave the clipboard and input devices alone. Do not open portal dialogs, publish offers in the file manager, or start demonstration processes. Check a suite's scope before running it. Self-tests must use synthetic data without initializing those services.

Consult documentation and declare dependencies. Keep installations local, preserve permissions and firewall settings, and work without elevating the process. If a dependency is missing, document the blocker and continue independent work.

## Design and delivery

- Implement your own prototype using C++ and a standalone build within your directory. Existing application engines are outside scope.
- Keep product infrastructure outside scope, including accounts, discovery, updaters, installers, and a complete UI.
- Starting with no arguments must be inert. Real modes require explicit selection.
- A stub must report its actual status so it cannot be mistaken for successful native integration.
- Record design decisions in README, reproducible commands in MANUAL, and evidence in RESULTS.
- Document each source file's responsibility.
- Limit test data and logs. Keep private content out of logs.
- Protect or ignore private material and generated artifacts.
- Distinguish implementation, successful compilation, verified self-tests, and pending manual execution.
- Claim validation only for operating systems you actually accessed.

Work directly on this assignment. The three experiment directories support independent work without coordination of edits or desktop sessions.

## Completion condition

Deliver the prototype within the README scope, the isolated checks you can run, and the manual. List concrete limitations and incomplete work. Optional improvements and manual tests are outside the assignment.
