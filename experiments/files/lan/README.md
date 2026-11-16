# First Windows File Trial → Ubuntu

Preparation: November 5, 2026. The text synchronization experiment is suspended. This test uses only
the file executable; the publication of files on the clipboard is part of its native pasting.

## Status and objective

- Windows: `192.168.1.147`, file server in `47654/TCP`.
- Ubuntu: `192.168.1.146`, receiver and destination glued to Nautilus.
- Windows source: `.local-state/lan-files/windows/datos`.
- Five files, 5.551.150 bytes, with empty file, Unicode and empty folders.
- [Comparation manifest](windows-fixtures.json): structure, sizes and SHA-256 of synthetic data.
- Windows binary already compiled with `2ae444d` file fixes.
- File identities are used, different from those of the text experiment. Windows retains its key in
  `.local-state/lan-files/windows/private.key` and publishes only `public/windows.pem` and
  `public/windows.sha256` in this folder.
- There is no active server or time rule: first the two commands must be remembered and the boot
  with the Ubuntu agent.

The test is considered overtaken when the user actually pastes `datos` into Nautilus and the
destination comparison matches the manifest. Receive and verify the cache, by itself, does not
validate the pasting.

## == sync, corrected by elderman ==

Keep local changes and get `origin/validation/windows`. Advance only if possible without overwriting
reports; do not use destructive reset or force publication. Branches may have different
documentation, but before testing it must be confirmed that the code of the file experiment matches,
including your GTK receiving and publishing completion changes.

Use the already verified `experiments/files/build/ubuntu/oneintwo-files` binary and local Ubuntu
dependencies, restricted to that process. There is no need to repeat the battery or the local tests
that already have evidence.

Prepare the destination **before** to publish, so as not to replace the file offer by copying a text
path:

```bash
mkdir -p .local-state/lan-files/ubuntu/glue
```

Open that folder in Nautilus. It must be empty. Do not copy files to it by command: the final
content must come from the pasting of the user. If it already contains data from another attempt,
use another empty destination, without deleting them.

Check the traces of the dedicated identities and the keys:

- Windows sets Ubuntu, communicated by its agent:
  `fc8fe7d1f4a48636d5fe87792883a1288f76e7db2d24cfaddad7f116d28b2fe5`.
- Ubuntu fija Windows: `6b06e68f0888ba23d88281a8a6fb69f6ad20f7da7b870201cc2278a61f8ccbf0`.

The C++ file code has not changed with respect to `bb7df3b`; subsequent revisions add preparation
and public certificates. Before connecting, confirm the same SHA Git with those documents on both
machines, keeping local reports and without forcing synchronization.

## Start and reception

The agreed Windows command will be as follows, but **is not yet executed**:

```powershell
$env:Path = "C:\Program Files\OpenSSL-Win64\bin;" + $env:Path
& 'D:\DEVEL\OneInTwo\experiments\files\build\windows-ready\Release\oneintwo-files.exe' --share --root 'D:\DEVEL\OneInTwo\.local-state\lan-files\windows\datos' --bind 192.168.1.147 --port 47654 --cert 'D:\DEVEL\OneInTwo\.local-state\lan-files\windows\cert.pem' --key 'D:\DEVEL\OneInTwo\.local-state\lan-files\windows\private.key' --client-pin fc8fe7d1f4a48636d5fe87792883a1288f76e7db2d24cfaddad7f116d28b2fe5 --timeout 120000
```

The firewall exception, when agreed to open it, will be limited to the file executable,
`192.168.1.146` as source, `192.168.1.147` as destination and 47654/TCP. The advance preparation
rule has already been removed.

After confirming commands, revision and preparation, wait for explicit confirmation **"Windows
serving files"** before connecting.

From the root of the repository, the proposed Ubuntu command is as follows. Your agent must indicate
the actual paths of your new identity in `UBUNTU_FILES_CERT` and `UBUNTU_FILES_KEY`, and confirm the
command before executing it:

```bash
./experiments/files/build/ubuntu/oneintwo-files --receive-offer \
  --from 192.168.1.147:47654 \
  --pin 6b06e68f0888ba23d88281a8a6fb69f6ad20f7da7b870201cc2278a61f8ccbf0 \
  --cert "$UBUNTU_FILES_CERT" --key "$UBUNTU_FILES_KEY" \
  --cache .local-state/lan-files/ubuntu/cache --publish --timeout 120000
```

Do not use text identities for this new case or upload private keys. Adapt only the library
environment that was already used in Ubuntu.

Do not add `--to`: in this case we want to check the pasted in the browser, not a direct copy made
by the program.

## Observable check

1. The full mutual TLS receiver receives a five file offer.
2. All five contents are verified before being cached.
3. The GTK window appears. The user presses **Publish files** and leaves it open.
4. In Nautilus, the user pastes Ctrl+V into the destination already prepared.
5. A `datos` folder must appear, including `empty.dat` and empty folders.
6. Agent Ubuntu compares that `pegado/datos` to `windows-fixtures.json`: same paths of files and
   directories, same sizes and same SHA-256.
7. Register user confirmation and comparison result separately.
8. Close the GTK window, collect salida/log from the receiver and communicate the shutdown to
   Windows to stop your server and remove its time rule.

Logs can skip progress until their buffers are emptied; do not confuse absence of immediate output
with transfer failure. The result should be based on customer output, observation and destination
hashes.

## Limits of this first case

Only transfer Windows → Ubuntu by cache and pasted assisted. The reverse address, pasted in
Explorer, content on demand, automatic publication in Ubuntu, cancellation between teams or
collisions are not yet certified. They will be coordinated after the first result.

Do not activate keyboard or mouse input or text synchronization. Do not start extra certificate testing or
parallel reconnection. Keep the originals and destination as evidence; do not overwrite or clean the
cache while it is pasting.

The results will be incorporated into [RESULTS.md](../RESULTS.md), along with the actual revision of
both ends and the result per file. Until the case is observed, it remains pending.
