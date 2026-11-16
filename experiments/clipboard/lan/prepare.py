"""Prepare identities and commands for LAN testing; do not start network or desktop."""

import argparse
import hashlib
import os
from pathlib import Path
import shlex
import shutil
import ssl
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
PUBLIC = Path(__file__).resolve().parent / "public"
STATE = ROOT / ".local-state" / "lan-clipboard"


def fingerprint(path):
    """Calculate SHA-256 of the DER certificate, compatible with the prototype pin."""
    return hashlib.sha256(ssl.PEM_cert_to_DER_cert(path.read_text(encoding="ascii"))).hexdigest()


def openssl_path(explicit=None):
    candidates = [explicit] if explicit else [
        "C:/msys64/usr/bin/openssl.exe",
        shutil.which("openssl"),
        "C:/Program Files/OpenSSL-Win64/bin/openssl.exe",
    ]
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return str(candidate)
    raise ValueError("OpenSSL was not found. Specify --openssl PATH.")


def prepare(name, openssl=None):
    """Generates a local identity without overwriting another; exports only the certificate."""
    directory = STATE / name
    directory.mkdir(parents=True, exist_ok=True)
    if os.name != "nt":
        directory.chmod(0o700)
    cert, key = directory / "cert.pem", directory / "private.key"
    if cert.exists() != key.exists():
        raise ValueError("Incomplete identity: check the local folder; it is not overwritten.")
    tool = openssl_path(openssl)
    if not cert.exists():
        with tempfile.TemporaryDirectory(dir=directory) as temporary:
            temp = Path(temporary)
            subprocess.run([
                tool, "req", "-x509", "-newkey", "rsa:3072", "-sha256", "-nodes",
                "-days", "14", "-keyout", str(temp / "private.key"),
                "-out", str(temp / "cert.pem"), "-subj", "/CN=OneInTwo-LAN-" + name,
            ], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60)
            fingerprint(temp / "cert.pem")
            shutil.copyfile(temp / "private.key", key)
            if os.name != "nt":
                key.chmod(0o600)
            shutil.copyfile(temp / "cert.pem", cert)
    subprocess.run([tool, "x509", "-in", str(cert), "-checkend", "3600", "-noout"],
                   check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10)
    # Check that the existing pair corresponds before re-exporting it.
    cert_public = subprocess.run([tool, "x509", "-in", str(cert), "-pubkey", "-noout"],
                                 check=True, capture_output=True, timeout=10).stdout
    key_public = subprocess.run([tool, "pkey", "-in", str(key), "-pubout"],
                                check=True, capture_output=True, timeout=10).stdout
    if cert_public.replace(b"\r\n", b"\n") != key_public.replace(b"\r\n", b"\n"):
        raise ValueError("The certificate and the local key do not correspond.")
    PUBLIC.mkdir(exist_ok=True)
    exported = PUBLIC / (name + ".pem")
    shutil.copyfile(cert, exported)
    digest = fingerprint(exported)
    (PUBLIC / (name + ".sha256")).write_bytes((digest + "\n").encode("ascii"))
    return exported, digest


def command(name, role, host, port, binary=None):
    """Build arguments from the real binary and check all previous files."""
    if not host or host.startswith("-") or any(c.isspace() for c in host):
        raise ValueError("Indicate an explicit address without spaces or options.")
    if not 1 <= port <= 65535:
        raise ValueError("The port must be between 1 and 65535.")
    if binary:
        executable = Path(binary).expanduser().resolve()
    elif os.name == "nt":
        executable = ROOT / "experiments/clipboard/build/windows-ready/Release/oneintwo-clipboard.exe"
    else:
        executable = ROOT / "experiments/clipboard/build/ubuntu/oneintwo-clipboard"
    own = STATE / name
    peer = PUBLIC / (("ubuntu" if name == "windows" else "windows") + ".pem")
    for path in (executable, own / "cert.pem", own / "private.key", peer):
        if not path.is_file():
            raise ValueError("File missing: " + str(path))
    fingerprint(own / "cert.pem")
    fingerprint(peer)
    mode = ["--serve", "--bind", host] if role == "serve" else ["--connect", "--host", host]
    return [str(executable), *mode, "--port", str(port), "--cert", str(own / "cert.pem"),
            "--key", str(own / "private.key"), "--peer-cert", str(peer)]


def display(argv, windows=None):
    """Show a literal PowerShell or POSIX shell command without executing it."""
    if windows is None:
        windows = os.name == "nt"
    if windows:
        return "& " + " ".join("'" + arg.replace("'", "''") + "'" for arg in argv)
    return shlex.join(argv)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="action", required=True)
    setup = commands.add_parser("prepare", help="It generates identity and exports public certificate.")
    setup.add_argument("--name", required=True, choices=("windows", "ubuntu"))
    setup.add_argument("--openssl")
    plan = commands.add_parser("command", help="Shows the command; does not execute it.")
    plan.add_argument("--name", required=True, choices=("windows", "ubuntu"))
    plan.add_argument("--role", required=True, choices=("serve", "connect"))
    plan.add_argument("--host", required=True)
    plan.add_argument("--port", type=int, default=47831)
    plan.add_argument("--binary")
    args = parser.parse_args()
    try:
        if args.action == "prepare":
            exported, digest = prepare(args.name, args.openssl)
            print("Public certificate:", exported.relative_to(ROOT))
            print("SHA-256:", digest)
            print("Private key preserved only in .local-state/lan-clipboard/" + args.name)
        else:
            print(display(command(args.name, args.role, args.host, args.port, args.binary)))
        return 0
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print("Incomplete preparation:", str(error))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
