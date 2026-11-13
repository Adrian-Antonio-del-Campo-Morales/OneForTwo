"""Regresión aislada de argumentos/quoting; sin certificados privados, red o escritorio."""

from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import prepare


class CommandTests(unittest.TestCase):
    def test_roles_use_actual_cli_and_require_peer(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            binary = base / "path with spaces" / "probe.exe"
            binary.parent.mkdir()
            binary.touch()
            for name in ("windows", "ubuntu"):
                (base / name).mkdir()
                (base / name / "cert.pem").touch()
                (base / name / "private.key").touch()
            (base / "ubuntu.pem").touch()
            with patch.object(prepare, "STATE", base), patch.object(prepare, "PUBLIC", base), \
                    patch.object(prepare, "fingerprint", return_value="digest"):
                args = prepare.command("windows", "serve", "192.0.2.1", 47831, str(binary))
                self.assertEqual(args[1:5], ["--serve", "--bind", "192.0.2.1", "--port"])
                self.assertIn("--peer-cert", args)
                args = prepare.command("windows", "connect", "192.0.2.2", 47831, str(binary))
                self.assertEqual(args[1:4], ["--connect", "--host", "192.0.2.2"])
                (base / "ubuntu.pem").unlink()
                with self.assertRaisesRegex(ValueError, "Falta el archivo"):
                    prepare.command("windows", "serve", "192.0.2.1", 47831, str(binary))

    def test_rejects_invalid_endpoint(self):
        for host, port in [("", 1), ("--flag", 1), ("a b", 1), ("localhost", 0), ("localhost", 65536)]:
            with self.subTest(host=host, port=port), self.assertRaises(ValueError):
                prepare.command("windows", "serve", host, port)

    def test_quotes_literal_paths(self):
        self.assertEqual(prepare.display(["a b", "x'y", "$value"], windows=True),
                         "& 'a b' 'x''y' '$value'")
        self.assertEqual(prepare.display(["a b", "safe"], windows=False), "'a b' safe")


if __name__ == "__main__":
    unittest.main()
