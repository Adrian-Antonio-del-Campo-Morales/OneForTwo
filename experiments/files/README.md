# Experiment to copy and paste files

The next agreed test is [Windows → Ubuntu, cache and glued to Nautilus](lan/README.md). Includes the
synthetic data manifest and the receiver command; the result between teams remains pending until it
is observed.

## Ordering for the agent

Implement this experiment by following [common instructions](../AGENTS.md) and [delivery
model](../README.md). Your exclusive directory is `D:/DEVEL/OneInTwo/experiments/files`. Operations
on actual clipboards and browsers will be executed manually.

## Question to Solve

How to represent an offer of remote files so that Explorer and Nautilus can paste them? Is a local
cache enough or is it feasible to deliver content on demand?

The scope is to copy a file and a test folder in both directions. It does not include cutting,
deleting originals, synchronizing folders, mounting a virtual file system or sharing the entire
disk.

## Interface design

An executable with small components:

1. File offer adapter on the clipboard of each system.
2. file and folder manifest and ID map to authorized files.
3. Block transfer and integrity validation.
4. Local delivery to the explorer and temporary life control.

This experiment will implement your native access to the file clipboard within your own folder. It
will not depend on the text agent or its results.

The transfer can be tested locally with synthetic fixtures and manually between computers via
explicit address. The browser's representation must contain files that you can consume, not just a
remote path pasted as text.

## Two alternatives and their scope

**A. Local cache, functional prototype reference.** Receive content in a private test folder, verify
it and announce local files ready to paste. Show that the content is ready before pasting. Avoid
deleting it while the browser needs it.

**B. Delivery on demand, limited feasibility test.** Investigate native API and test with a file if
it allows delivering content when requested by the browser. The test may use local synthetic content
before connecting it to the network. Document waiting times, cancellations and differences between
systems.

Do not build a full file system provider to force B. If you require such expansion or there is no
viable documented path, deliver A and a well-founded conclusion about B. The goal is to compare
alternatives, not promise that both will be possible.

## Prototyping guarantees

Use only disposable data within the chosen test folder. Keep originals, do not overwrite silently,
refuse off-target routes and special unsupported types. Remote identifiers can only access offered
items.

Save separate incomplete transfers, validate content, and display errors without publishing an
incomplete file as finished. Logs will not include content or personal paths by default.

## What the agent will be able to check automatically

- Manifestos, Unicode in names and limits.
- Empty files and folders with synthetic data.
- Blocks, integrity, truncation and cancellation.
- Collisions and routes that try to escape fate.
- Life of offers and temporary with a fake consumer.

File tests will use a temporary within the experiment directory. They will not open browsers,
publish native offers, or initialize the clipboard.

## Screenplay to be prepared for manual execution

1. Create the test data set.
2. Publish a local offer and paste it into Explorer or Nautilus.
3. Test file, folder and empty file, checking size and hash.
4. Connect both computers and repeat Windows→Ubuntu and Ubuntu→Windows.
5. Compare cache and test on demand where it exists.
6. Cancel or interrupt a transfer and verify that it does not appear as completed.
7. Try collisions and a fate without writing.
8. Close the prototype and check when its time frames are removed.

## Deliverable and evaluation criteria

Code, independent compilation, self-tests, MANUAL.md and RESULTS.Md. The results table will
distinguish transfer of content, native publication and actual pasted in each browser, in addition
to the cost of cache and B limits.

Sending correctly bytes does not prove that it can be pasted into the browser. Native pasting and
experience between two computers will remain pending until manual execution.

The expected architectural decision is what delivery strategy to use and what responsibility each
file adapter will have.

## Implementation delivered (4 November 2026)

The prototype is implemented and compiled on Windows; the desktop tests are still pending and
described in [MANUAL.md](MANUAL.md). The status and evidence are in [RESULTS.Md](RESULTS.md).

```text
files/
├── CMakeLists.txt          standalone build (C++17)
├── MANUAL.md               manual operation, permissions, and results matrix
├── RESULTS.md              status, evidence, and limits
├── src/core/               manifest, transfer, cache, protocol, and TLS
├── src/platform/win/       CF_HDROP (A) y FILEDESCRIPTOR/FILECONTENTS (B)
├── src/platform/linux/     x-special/gnome-copied-files y text/uri-list (A)
└── tests/                  176 isolated checks
```

Design decisions:

1. **A (cache) is the reference**: it is downloaded, checked by file and only then local content is
   published.
2. **B (on demand) is a Windows limited test**: descriptor OLE and streams that request ranges at source. In Linux it is not implemented: the adapter publishes only local `file://` routes. Nautilus accepts valid URIs (not only `file://`) and GVfs allows schemas by D-Bus, so B would require a GIO/GVfs or FUSE backend; it is a limitation of this delivery, not a proven impossibility (RESULTS.md §5.1).
3. The remote pair never receives absolute routes: only validated relative routes and current offer
   identifiers.
4. Incomplete files live in `incoming/` and are never published; `offers/` contains only content
   verified with SHA-256.
5. Remote mode requires mutual TLS with both fingerprints fixed: the client verifies the server and
   the server requires the client certificate (`--client-pin`). Without OpenSSL it is blocked; it
   does not degrade to plain text or omit the identity of either party.

Quick use: `--self-test`, `--make-fixtures DIR`,`--inspect RUTA...` and `--fingerprint CERT.pem`;
then, according to the manual, `--publish-cache`, `--publish-ondemand`,`--share` and
`--receive-offer`.

