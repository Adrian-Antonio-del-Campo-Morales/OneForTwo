"""Regresiones aisladas de recuperación: ningún comando real ni GTK."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("shortcut_check", Path(__file__).parents[1] / "tools/shortcut_check.py")
guide = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guide)


class RecoveryTests(unittest.TestCase):
    def case(self, directory):
        path = Path(directory)
        guide.save(path / "case.json", {"scope": "app-io.github.OneForTwo.InputExperiment-guided0123456789ab.scope",
                                       "session": "1", "recovery_unit": "onefortwo-return-0123456789ab", "deadline_monotonic": time.monotonic() + 30})
        return path

    def test_recovers_after_kill_failure_without_touching_other_units(self):
        calls = []
        def run(args, **kwargs):
            calls.append(args)
            if args[0] == "systemctl":
                raise subprocess.TimeoutExpired(args, 3)
            text = str(os.getuid()) if "User" in args else "Active=yes\nVTNr=2\n"
            return subprocess.CompletedProcess(args, 0, text, "")
        with tempfile.TemporaryDirectory() as directory:
            case = self.case(directory)
            self.assertEqual(guide.recover(case, run), 0)
            self.assertEqual(calls[0], ["systemctl", "--user", "kill", "--signal=KILL",
                                      "app-io.github.OneForTwo.InputExperiment-guided0123456789ab.scope"])
            self.assertEqual(calls[2][-4:], ["loginctl", "--no-ask-password", "activate", "1"])
            self.assertIn("kill_error", json.loads((case / "recovery.json").read_text()))

    def test_no_portal_without_unexpired_active_recovery_timer(self):
        with tempfile.TemporaryDirectory() as directory:
            case = self.case(directory)
            inactive = lambda args: subprocess.CompletedProcess(args, 3, "inactive", "")
            with self.assertRaisesRegex(RuntimeError, "armada"):
                guide.child_command(case, inactive)
            data = json.loads((case / "case.json").read_text())
            data["mode"] = "inject-demo"
            guide.save(case / "case.json", data)
            with self.assertRaisesRegex(RuntimeError, "armada"):
                guide.child_command(case, inactive)
            active = lambda args: subprocess.CompletedProcess(args, 0, "active", "")
            args = guide.child_command(case, active)
            self.assertIn("--inject-demo", args)
            self.assertNotIn("--check-target", args)
            self.assertEqual(args[-4:], ["--arm-seconds", "5", "--max-seconds", "30"])
            data["deadline_monotonic"] = time.monotonic() - 1
            guide.save(case / "case.json", data)
            with self.assertRaisesRegex(RuntimeError, "vencido"):
                guide.child_command(case, lambda *a: self.fail("No debe lanzar comandos"))

    def test_timer_failure_prevents_preparation(self):
        def run(args, **kwargs):
            text = "1" if "show-user" in args else f"User={os.getuid()}\nType=wayland\nVTNr=2\n"
            failed = args[0] == "systemd-run" and "--on-active=30s" in args
            return subprocess.CompletedProcess(args, int(failed), text, "fallo simulado" if failed else "")
        with tempfile.TemporaryDirectory() as directory, patch.object(guide, "INPUT", Path(directory)):
            with self.assertRaisesRegex(RuntimeError, "armar"):
                guide.prepare_case(run)


if __name__ == "__main__":
    unittest.main()
