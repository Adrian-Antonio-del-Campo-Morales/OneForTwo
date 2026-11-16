# First test LAN — Windows clipboards ↔ Ubuntu

Ready: November 5, 2026. Responsibility: coordinate a limited test with the already developed
binaries and adapters, without integrating the application. The connection is bidirectional even if
Windows listens and Ubuntu connects.

## 1. What is prepared

- Python 3.8+ Wizard with standard library to generate identities and display the commands of the
  real prototype. It does not execute the prototype.
- Public certificate Windows and its SHA-256 in `public/`. Its private key exists only in the
  original Windows copy, under `.local-state/lan-clipboard/windows/`.
- Different text samples for each sense, without personal data.
- Agreed port: **47831/TCP**, different from other experiments.
- Windows listens only to the chosen LAN address. Ubuntu connects.
- The verified prototype code is `2ae444d` delivery; subsequent revisions of
  setup and documentation do not replace actual tests.

The certificates last 14 days from generation. This package does not start a connection, modify the
firewall, read the clipboard, or grant permissions. The first real test is still pending. Do not
boot another clipboard, file, or input experiment at the same time.

## 2. Get the preparation in Ubuntu

Keep any local changes. From a clean copy, download and move forward only if it is possible to do so
without mixing stories:

```bash
git fetch origin
git status --short --branch
git merge --ff-only origin/validation/windows
```

If the advance is rejected or there are local changes, keep them and resolve the synchronization
before testing. Do not use destructive reset or force the push. Write down `git rev-parse HEAD` at
both ends; use the same revision when starting.

The expected Ubuntu executable is `experiments/clipboard/build/ubuntu/oneintwo-clipboard`. If the
revision brings C++ changes, rebuild and verify TLS and the portal with the already validated
environment in Ubuntu. A change in this wizard alone does not require C++ recompile. Consult
[MANUAL.md](../MANUAL.md) and [RESULTS.Md](../RESULTS.md).

## 3. Complete the Ubuntu identity setup

From the root of the repository:

```bash
python3 -B experiments/clipboard/lan/prepare.py prepare --name ubuntu
```

`--openssl /path/to/openssl` can be indicated if it is not in PATH. If the agent uses local
dependencies, keep your library environment restricted to tools and test binary, without polluting
desktop applications.

The wizard retains the key in `.local-state/lan-clipboard/ubuntu/private.key` and creates
`public/ubuntu.pem` and `public/ubuntu.sha256`. It does not regenerate an existing identity: it
checks that your certificate is valid and corresponds to your key. If the identity is incomplete or
expired, stop and agree to replace it; do not overwrite a key that is still using the other end.

Publish **only** those two public archives in the Ubuntu branch of work:

```bash
git add -- experiments/clipboard/lan/public/ubuntu.pem experiments/clipboard/lan/public/ubuntu.sha256
git diff --cached --name-only
git commit -m "test: share Ubuntu public identity for clipboard LAN"
git push
```

The Ubuntu agent must communicate branch, commit, IP LAN and fingerprint. Windows will get that
revision and check the print before booting. Never swap or upload `private.key`, `.local-state`
directories, packages or binaries.

## 4. Windows identity and local preparation

The original Windows machine already has its identity ready. Only if the preparation ** is repeated
on that same machine**:

```powershell
python -B experiments/clipboard/lan/prepare.py prepare --name windows
```

A cloned copy on another machine does not have that private key: it must not regenerate the Windows
identity without agreeing a new certificate with the other end.

The public certificate is independently verified by:

```bash
openssl x509 -in experiments/clipboard/lan/public/windows.pem -noout -fingerprint -sha256
```

The expected print also appears in `public/windows.sha256`. Compare to the fingerprint communicated
from Windows by the coordination conversation. Check the public certificate of Ubuntu in the same
way before trusting him.

On Windows, put the DLL used in the compilation on the PATH of that console:

```powershell
$env:Path = "C:\Program Files\OpenSSL-Win64\bin;" + $env:Path
```

## 5. Agree direction and firewall

Windows communicates its current LAN IP to Ubuntu agent. Do not use WSL addresses or virtual
interfaces. IP can switch between sessions.

In the preparation of November 5, the LAN Windows interface has a **Public** profile and there are
no listeners in 47831. No change of profile or rule created. If the firewall prevents access,
prepare a temporary exception limited to the executable, the Ubuntu IP, the Windows IP and the port.
In Admin PowerShell, replacing both addresses:

```powershell
$probe = (Resolve-Path 'experiments/clipboard/build/windows-ready/Release/oneintwo-clipboard.exe').Path
New-NetFirewallRule -DisplayName 'OneInTwo clipboard LAN test' -Direction Inbound -Action Allow -Profile Public -Protocol TCP -LocalPort 47831 -Program $probe -LocalAddress WINDOWS_IP -RemoteAddress UBUNTU_IP
```

This document does not execute that rule. Confirm the current profile and adjust the parameter if
you have changed. Do not change the profile or extend the rule to all programs, addresses or
networks to hide a bug. Upon completion, remove only the rule from this test:

```powershell
Remove-NetFirewallRule -DisplayName 'OneInTwo clipboard LAN test'
```

## 6. Show and execute agreed commands

Once both public certificates and each private key are present on your machine, the wizard generates
the exact command. Replace `WINDOWS_IP` with the communicated address. Do not add `--show-content`:
the output will display metadata.

Windows:

```powershell
python -B experiments/clipboard/lan/prepare.py command --name windows --role serve --host WINDOWS_IP
```

Ubuntu:

```bash
python3 -B experiments/clipboard/lan/prepare.py command --name ubuntu --role connect --host WINDOWS_IP
```

`--binary RUTA` allows you to mark another equivalent binary. The wizard checks the existence of the
binary and identities, displays the command and ends. **No session is logged in to execute
`command`.** Copy the command displayed to the console when both ends are ready.

1. Prepare disposable editors with both samples.
2. Start the Windows command and wait for the message `listening=`.
3. Start Ubuntu, grant only clipboard and check `devices=0`.
4. Confirm the prints and that both ends complete TLS and greeting.
5. Run the following matrix. To start, devote a maximum of three minutes to the session; manually
   finish with Ctrl+C on both consoles.

The prototype does not have an automatic LAN timer: the three-minute limit is for the session agreed
with the user, not a program warranty. Do not use `eval` or `Invoke-Expression` to automatically run
the displayed text.

## 7. First matrix between teams

| Caso | Action | What to Observe |
| --- | --- | --- |
| T0 | In a separate attempt, Ubuntu fixes its own certificate instead of that of Windows with `--peer-cert` | Rejection TLS, no application greeting or remote data; then restore the correct certificate |
| T1 | Copy `samples/windows.txt` to a Windows editor and paste into Ubuntu editor | Accurate text, including accents and line breaks |
| T2 | Copy `samples/ubuntu.txt` to Ubuntu and paste into Windows | Exact text; does not return previous sample |
| T3 | Wait ten seconds without copying again | Absence of continuous forwarding or repeated changes |
| T4 | Stop Ubuntu client, reconnect it and make a new copy | Clear error or disconnect and subsequent resynchronization, without automatically restoring old content |
| T5 | Ctrl+C in Ubuntu and Windows | Closure of both processes and sessions, without listening to this remaining essay |

The negative test T0 must be coordinated: Windows can keep listening after rejecting the client. Do
not confuse a TLS error with inaccessible port. No proof of this document is overtaken by generating
commands.

## 8. Evidence and end of session

Record results observed in [RESULTS.md](../RESULTS.md): review of both computers, roles, port,
versions, public footprints, result of each case and errors. Do not register private text. Reuse
valid Ubuntu and Windows checks; do not repeat research or general batteries.

When finished, close the two prototypes and remove the time rule if created. Local keys are retained
for the next session until they expire; do not delete or regenerate them silently. No restoration of
the previous clipboard content is promised: recopy manually what is needed.

After completing clipboards, files and input will be coordinated with your manuals. This package
does not start those experiments yet; Ubuntu as input source continues to be limited by InputCapture
v1 compared to the v2 requirement.

## 9. Checking the wizard

```bash
python3 -B experiments/clipboard/lan/test_prepare.py
```

Three isolated tests cover actual roles/argumentos, missing pair certificate, invalid end rejection
and quoting. The generation and checking of Windows identity has been done without sockets or
desktop; they do not test a TLS handshake. Public certificate is not a functional result.
