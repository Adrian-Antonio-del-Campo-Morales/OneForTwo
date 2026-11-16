#!/usr/bin/python3
"""Input experimental guide with standalone recovery by login. By default only GlobalShortcuts; --inject-demo0 allows local demonstration RemoteDesktop/EIS. --recover belongs to the same user's timer. GUI does not monitor or guarantee the composer.
"""
import argparse
import datetime
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
import uuid

INPUT = Path(__file__).resolve().parents[1]
REPO = INPUT.parents[1]
APP_ID = "io.github.OneForTwo.InputExperiment"
DURATION = 30


def system_env():
    env = os.environ.copy()
    env.pop("LD_LIBRARY_PATH", None)
    return env


def command(args, **kwargs):
    return subprocess.run(args, env=system_env(), text=True, capture_output=True,
                          timeout=kwargs.pop("timeout", 8), **kwargs)


def save(path, value):
    temporary = path.with_name(path.name + "." + uuid.uuid4().hex + ".tmp")
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n")
    temporary.replace(path)


def own_session(run=command):
    result = run(["loginctl", "show-user", str(os.getuid()), "-p", "Display", "--value"])
    session = result.stdout.strip()
    if result.returncode or not re.fullmatch(r"[A-Za-z0-9_-]+", session):
        raise RuntimeError("You do not identify your own graphic session.")
    result = run(["loginctl", "show-session", session, "-p", "User", "-p", "Type", "-p", "VTNr"])
    fields = dict(line.split("=", 1) for line in result.stdout.splitlines() if "=" in line)
    if result.returncode or fields.get("User") != str(os.getuid()) or fields.get("Type") != "wayland":
        raise RuntimeError("The session must be Wayland and belong to the current user.")
    if not fields.get("VTNr", "").isdigit() or int(fields["VTNr"]) < 1:
        raise RuntimeError("The graphical recovery console is not known.")
    return session, fields["VTNr"]


def recover(case, run=command, reason="timer"):
    data = json.loads((case / "case.json").read_text())
    scope = data["scope"]
    session = data["session"]
    if not re.fullmatch(r"app-io\.github\.OneForTwo\.InputExperiment-guided[0-9a-f]{12}\.scope", scope):
        raise ValueError("Scope of recovery outside this test.")
    if not re.fullmatch(r"[A-Za-z0-9_-]+", session):
        raise ValueError("Invalid recovery session.")
    evidence = {"started": datetime.datetime.now().astimezone().isoformat(), "reason": reason}
    # Close only the random scope of this essay, never the guide nor the session.
    try:
        stopped = run(["systemctl", "--user", "kill", "--signal=KILL", scope], timeout=3)
        evidence["kill_exit"] = stopped.returncode
        evidence["kill_message"] = stopped.stderr.strip()
    except subprocess.SubprocessError as exc:
        evidence["kill_error"] = str(exc)
    try:
        check = run(["loginctl", "show-session", session, "-p", "User", "--value"], timeout=3)
    except subprocess.SubprocessError as exc:
        evidence["error"] = "Could not close temporary folder: %s " + str(exc)
        save(case / "recovery.json", evidence)
        return 1
    if check.returncode or check.stdout.strip() != str(os.getuid()):
        evidence["error"] = "A session is not reactivated without checking its owner."
        save(case / "recovery.json", evidence)
        return 1
    try:
        activated = run(["timeout", "--kill-after=1s", "5s", "loginctl", "--no-ask-password", "activate", session])
        evidence["activate_exit"] = activated.returncode
        evidence["activate_message"] = activated.stderr.strip()
        state = run(["loginctl", "show-session", session, "-p", "Active", "-p", "VTNr"], timeout=3)
        evidence["session_state"] = state.stdout.strip()
    except subprocess.SubprocessError as exc:
        evidence["error"] = "Could not close temporary folder: %s " + str(exc)
    evidence["finished"] = datetime.datetime.now().astimezone().isoformat()
    save(case / ("recovery-" + uuid.uuid4().hex[:12] + ".json"), evidence)
    save(case / "recovery.json", evidence)
    return 0 if evidence.get("activate_exit") == 0 and "Active=yes" in evidence.get("session_state", "").splitlines() else 1


def prepare_case(run=command, mode="shortcut"):
    if mode not in ("shortcut", "inject-demo", "emergency-check", "revoke-check"):
        raise ValueError("Invalid test mode.")
    session, vt = own_session(run)
    token = uuid.uuid4().hex[:12]
    case = INPUT / "manual-output" / ("guided-" + datetime.datetime.now().strftime("%Y%m%d-%H%M%S-") + token)
    case.mkdir(parents=True)
    scope = "app-" + APP_ID + "-guided" + token + ".scope"
    recovery_unit = "onefortwo-return-" + token
    save(case / "case.json", {"session": session, "vt": vt, "scope": scope,
                             "recovery_unit": recovery_unit, "seconds": DURATION, "mode": mode})
    # Preflight from a service, in the same context as recovery.
    preflight = run(["systemd-run", "--user", "--wait", "--pipe", "--collect",
                     "--unit=onefortwo-preflight-" + token, "--property=RuntimeMaxSec=7s",
                     "/usr/bin/timeout", "--kill-after=1s", "5s", "/usr/bin/loginctl",
                     "--no-ask-password", "activate", session], timeout=10)
    (case / "preflight.log").write_text(preflight.stdout + preflight.stderr)
    if preflight.returncode:
        raise RuntimeError("Independent reactivation does not work; the test is not initiated.")
    # Set up the timer first, outside the scope that can be finished.
    armed_at = time.monotonic()
    data = json.loads((case / "case.json").read_text())
    data["deadline_monotonic"] = armed_at + DURATION
    data["armed_at"] = datetime.datetime.now().astimezone().isoformat()
    save(case / "case.json", data)
    armed = run(["systemd-run", "--user", "--collect", "--unit=" + recovery_unit,
                 "--on-active=30s", "--timer-property=AccuracySec=1us",
                 "--property=RuntimeMaxSec=20s", "/usr/bin/env", "-u", "LD_LIBRARY_PATH",
                 "/usr/bin/python3", str(Path(__file__).resolve()), "--recover", str(case)])
    (case / "timer.log").write_text(armed.stdout + armed.stderr)
    if armed.returncode:
        raise RuntimeError("Recovery could not be arranged; the test was not initiated.")
    active = run(["systemctl", "--user", "is-active", recovery_unit + ".timer"])
    if active.returncode or active.stdout.strip() != "active":
        raise RuntimeError("The timer is not active; the test is not initiated.")
    return case


def prototype_command(case):
    data = json.loads((case / "case.json").read_text())
    return ["systemd-run", "--user", "--scope", "--collect", "--unit=" + data["scope"],
            "--property=RuntimeMaxSec=30s", "--property=TimeoutStopSec=2s",
            "/bin/bash", "-c", 'source "$1"; exec /usr/bin/python3 "$2" --child "$3"',
            "onefortwo-check", str(REPO / ".local-state/ubuntu-validation/env.sh"),
            str(Path(__file__).resolve()), str(case)]


def child_command(case, run=command):
    data = json.loads((case / "case.json").read_text())
    mode = data.get("mode", "shortcut")
    if mode not in ("shortcut", "inject-demo", "emergency-check", "revoke-check"):
        raise RuntimeError("Invalid test mode; portal does not open.")
    remaining = data["deadline_monotonic"] - time.monotonic()
    if remaining < 1:
        raise RuntimeError("Recovery has already expired; the portal is not open.")
    recovery_unit = data.get("recovery_unit", "")
    if not re.fullmatch(r"onefortwo-return-[0-9a-f]{12}", recovery_unit):
        raise RuntimeError("The timer for this test is missing.")
    active = run(["systemctl", "--user", "is-active", recovery_unit + ".timer"])
    remaining = data["deadline_monotonic"] - time.monotonic()
    if active.returncode or active.stdout.strip() != "active" or remaining < 1:
        raise RuntimeError("Recovery is not armed; the portal is not opened.")
    arguments = ["--check-target"] if mode == "shortcut" else ["--inject-demo", "--arm-seconds", "3" if mode.endswith("-check") else "5"]
    if mode in ("emergency-check", "revoke-check"):
        arguments += ["--wait-for-stop"]
    return ["/usr/bin/timeout", "--signal=TERM", "--kill-after=1s", f"{remaining:.3f}s",
            str(INPUT / "build/ubuntu-wayland/input_experiment"), *arguments, "--max-seconds", "30"]


def gui(inject=False, check=None):
    import threading
    import gi
    gi.require_version("Gtk", "3.0")
    from gi.repository import Gtk, GLib

    class Guide(Gtk.Window):
        def __init__(self):
            super().__init__(title="OneInTwo — " + ("emergency exit" if check == "emergency-check" else
                                                   "withdraw permission" if check else
                                                   "entrada local" if inject else "shortcut guide") + " (30 segundos)")
            self.set_default_size(720, 660)
            self.set_border_width(22)
            self.case = None
            self.process = None
            self.started = None
            self.busy = False
            self.cancelled = False
            box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=16)
            self.add(box)
            title = Gtk.Label(xalign=0)
            title.set_markup('<span size="x-large" weight="bold">' +
                             ("R2 — Close Control with Shortcut" if check == "emergency-check" else
                              "R9 — withdrawal of permit from GNOME" if check == "revoke-check" else
                              "Local test: mouse and text" if inject else "Test of the output shortcut") + '</span>')
            box.pack_start(title, False, False, 0)
            self.status = Gtk.Label(xalign=0)
            self.status.set_line_wrap(True)
            self.status.set_markup('<b> Ready. The test has not yet begun.</b>')
            box.pack_start(self.status, False, False, 0)
            self.phase = None
            self.countdown = Gtk.Label(xalign=0)
            self.countdown.set_markup('<b>Stopped timer: Press Start when you are ready.</b>')
            box.pack_start(self.countdown, False, False, 0)
            steps = Gtk.Label(xalign=0, yalign=0)
            steps.set_line_wrap(True)
            steps.set_max_width_chars(78)
            steps.set_text((
                "1. Click “Start 30-second test”. The limit includes any dialogs.\n\n"
                "2. If GNOME requests to add the shortcut Ctrl+Shift+F12, press “Add”. "
                "When this guide reads NOW, press AND RELEASE Ctrl+Shift+F12 ONCE.\n\n"
                "3. Then, in the remote interaction dialog, allows keyboard and mouse "
                "and click ”Share” / ”Share”, without selecting a screen. "
                "If only one monitor remains active, continue; if there are several, cancel.\n\n"
                "4. When the guide indicates ‘TEST AREA’, click the empty box "
                "from below. Do not type or change windows. After the countdown the prototype "
                "you will move the pointer a bit and write oneintwo, without clicking.\n\n"
                + ("5. Wait for message EXIT NOW. Then press AND RELEASE Ctrl+Shift+F12 again. "
                   "The test must close the control, wait at the end of 30 seconds."
                   if check == "emergency-check" else
                   "5. Wait REVOKE PERMISSION NOW. At the top bar of GNOME, press the indicator "
                   "“Stop Screen Sharing” / “Stop Screen sharing”. Do not use Cancel in this guide: "
                   "must be removed from GNOME. Do not press the shortcut again in this step. Wait at the end of the 30 seconds."
                   if check == "revoke-check" else
                   "5. Wait for the end at 30 seconds. To stop before: Cancel button "
                   "or Ctrl+Shift+F12. Do not use Ctrl+Alt+F12.")
            ) if inject else (
                "1. Click ”Start 30-second test” in this window.\n\n"
                "2. If GNOME opens ”Add keyboard shortcuts” / ”Add Keyboard Shortcuts”, "
                "checks that “Return local control (OneInTwo)” indicates Ctrl+Shift+F12. "
                "Press ”Add” / ”Add”, in the upper-right corner.\n\n"
                "   If Ctrl+Alt+F12 appears or requests to share screen, keyboard or mouse: "
                "click “Cancel” and decline the request.\n\n"
                "3. Return to this guide. wait for the message ”NOW: press and drop Ctrl+Shift+F12”. "
                "Then tap and drop that combination ONE TIME.\n\n"
                "4. Do not press any more keys. At 30 seconds the test will be closed and it will be attempted "
                "automatically return to your main graphic session. The guide will remain open."))
            scroll = Gtk.ScrolledWindow()
            scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
            scroll.set_min_content_height(160)
            scroll.add(steps)
            box.pack_start(scroll, True, True, 0)
            self.target = None
            if inject:
                target_label = Gtk.Label(label="TEST AREA — only oneintwo must appear here", xalign=0)
                box.pack_start(target_label, False, False, 0)
                self.target = Gtk.TextView()
                self.target.set_size_request(-1, 90)
                self.target.set_left_margin(12)
                box.pack_start(self.target, False, False, 0)
            buttons = Gtk.Box(spacing=12)
            self.start = Gtk.Button(label="Start 30-second test")
            self.start.get_style_context().add_class("suggested-action")
            self.start.connect("clicked", self.begin)
            buttons.pack_start(self.start, True, True, 0)
            self.cancel = Gtk.Button(label="Cancel and return to desktop")
            self.cancel.set_sensitive(False)
            self.cancel.connect("clicked", self.stop)
            buttons.pack_start(self.cancel, True, True, 0)
            box.pack_start(buttons, False, False, 0)
            note = Gtk.Label(xalign=0)
            note.set_line_wrap(True)
            note.set_text("The automatic return changes session and closes the prototype. "
                          "You cannot guarantee to repair a blocked composer or image.")
            box.pack_start(note, False, False, 0)
            self.connect("delete-event", self.closing)
            GLib.timeout_add(150, self.tick)

        def begin(self, _button):
            if self.busy or self.case is not None:
                return
            self.busy = True
            self.start.set_sensitive(False)
            self.status.set_text("Preparing the automatic turn. Don't press the shortcut yet.")
            def worker():
                try:
                    case = prepare_case(mode=check or ("inject-demo" if inject else "shortcut"))
                    GLib.idle_add(self.launch, case)
                except Exception as exc:
                    GLib.idle_add(self.failed, str(exc))
            threading.Thread(target=worker, daemon=False).start()

        def failed(self, message):
            self.busy = False
            self.status.set_text("The test is not started: " + message)
            return False

        def launch(self, case):
            self.case = case
            print("validation_case=" + str(case), flush=True)
            if self.cancelled:
                threading.Thread(target=recover, args=(case,), daemon=False).start()
                return False
            data = json.loads((case / "case.json").read_text())
            self.started = data["deadline_monotonic"] - DURATION
            if time.monotonic() >= data["deadline_monotonic"] - 1:
                self.failed("The recovery period has already expired; re-prepared the test.")
                return False
            self.log = (case / "prototype.log").open("w")
            try:
                self.process = subprocess.Popen(prototype_command(case), cwd=REPO, env=system_env(),
                                                stdout=self.log, stderr=subprocess.STDOUT)
            except Exception as exc:
                self.failed(str(exc))
                threading.Thread(target=recover, args=(case,), daemon=False).start()
                return False
            self.cancel.set_sensitive(True)
            self.status.set_text("Step 2: In the shortcut dialog, press ”Add” / ”Add” (above right).")
            return False

        def stop(self, _button=None):
            self.cancelled = True
            self.cancel.set_sensitive(False)
            self.status.set_text("Closing the test and trying to get back to the desk...")
            if self.case:
                threading.Thread(target=recover, args=(self.case,), kwargs={"reason": "cancel"}, daemon=False).start()

        def show_phase(self, message, ready=False):
            if self.phase == message:
                return
            self.phase = message
            if self.case:
                with (self.case / "guide-events.log").open("a") as log:
                    log.write(datetime.datetime.now().astimezone().isoformat() + " " + message.replace("\n", " / ") + "\n")
            if ready:
                self.status.set_markup('<span size="x-large" weight="bold" foreground="#096535">' +
                                       GLib.markup_escape_text(message) + '</span>')
                self.set_urgency_hint(True)
                self.present()
            else:
                self.status.set_text(message)

        def tick(self):
            if not self.case:
                return True
            remaining = max(0, DURATION - int(time.monotonic() - self.started)) if self.started else 0
            self.countdown.set_markup(f'<span size="large" weight="bold">Automatic return in {remaining} seconds</span>')
            log = self.case / "prototype.log"
            text = log.read_text(errors="replace") if log.exists() else ""
            recovery = self.case / "recovery.json"
            if self.process and self.process.poll() is not None and not (self.case / "process.json").exists():
                save(self.case / "process.json", {"exit": self.process.returncode})
                self.log.close()
            if recovery.exists():
                result = json.loads(recovery.read_text())
                ok = result.get("activate_exit") == 0 and "Active=yes" in result.get("session_state", "").splitlines()
                self.show_phase("Closed test. Graphical session reactivated according to system. "
                                     "Confirm in the chat if the image and desktop respond."
                                     if ok else "Closed test. Unable to confirm graphical return; report in chat.")
                self.cancel.set_sensitive(False)
                self.countdown.set_text("Test done. Don't press the shortcut anymore.")
            elif check and self.process and self.process.poll() is not None:
                self.show_phase("Output received. Control closed; waits for the end of 30 seconds."
                                if ("Control stopped by local emergency." in text if check == "emergency-check" else
                                    "Control stopped by closing the desk or device." in text) else
                                "The expected departure was not confirmed.")
            elif check and "CONTROL ACTIVO:" in text:
                self.show_phase("STEP 5 — EXIT NOW\nSEFUND pulsation: Ctrl+Shift+F12. Press and release ONE TIME."
                                if check == "emergency-check" else
                                "REVOKE PERMISSION NOW: press the upper indicator of GNOME ‘Stop Screen Sharing’.", ready=True)
            elif inject and self.process and self.process.poll() is not None:
                self.show_phase("Demonstration finished. Check the box and wait for the end of the 30 seconds."
                                if self.process.returncode == 0 else
                                "The demonstration was not completed. Don't press any more keys; wait for automatic closure.")
            elif inject and "Injection demonstration:" in text:
                message = "TEST AREA: Click the empty box below. Do not type or change windows."
                if self.phase != message:
                    self.show_phase(message, ready=True)
                    self.target.grab_focus()
            elif "Checked local catch" in text:
                self.show_phase("Shortcut received. Don't press any more keys; wait for automatic spin.")
            elif self.process and self.process.poll() is not None:
                self.show_phase("The prototype is finished. Do not press the shortcut; wait for the automatic return.")
            elif ("Check out." in text or (inject and "Antes de abrir RemoteDesktop/EIS" in text)) and not self.cancelled:
                self.show_phase(("STEP 2 — FIRST push \n" if check else "") + "NOW: press and release\nCtrl + Shift + F12\nONE ONLY TIME" +
                                ("\nThen authorizes remote interaction (step 3)." if inject else ""), ready=True)
            return True

        def closing(self, *_args):
            self.stop()
            Gtk.main_quit()
            return False

    window = Guide()
    window.show_all()
    Gtk.main()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recover", type=Path)
    parser.add_argument("--child", type=Path)
    parser.add_argument("--inject-demo", action="store_true", help="Local injection guide with disposable text area")
    parser.add_argument("--emergency-check", action="store_true", help="Check emergency with active EIS")
    parser.add_argument("--revoke-check", action="store_true", help="Check manual withdrawal of permission with active EIS")
    options = parser.parse_args()
    if options.recover:
        return recover(options.recover)
    if options.child:
        try:
            args = child_command(options.child)
        except RuntimeError as exc:
            print(str(exc), flush=True)
            return 124
        os.execv(args[0], args)
    check = "emergency-check" if options.emergency_check else "revoke-check" if options.revoke_check else None
    gui(options.inject_demo or bool(check), check)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
