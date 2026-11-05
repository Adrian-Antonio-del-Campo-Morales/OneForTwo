#!/usr/bin/python3
"""Guía experimental de entrada con recuperación independiente por logind.

Por defecto solo GlobalShortcuts; --inject-demo permite la demostración local
RemoteDesktop/EIS. --recover pertenece al temporizador del mismo usuario.
La GUI no supervisa ni garantiza el compositor.
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
        raise RuntimeError("No se identifica la sesión gráfica propia.")
    result = run(["loginctl", "show-session", session, "-p", "User", "-p", "Type", "-p", "VTNr"])
    fields = dict(line.split("=", 1) for line in result.stdout.splitlines() if "=" in line)
    if result.returncode or fields.get("User") != str(os.getuid()) or fields.get("Type") != "wayland":
        raise RuntimeError("La sesión debe ser Wayland y pertenecer al usuario actual.")
    if not fields.get("VTNr", "").isdigit() or int(fields["VTNr"]) < 1:
        raise RuntimeError("No se conoce la consola gráfica de recuperación.")
    return session, fields["VTNr"]


def recover(case, run=command, reason="timer"):
    data = json.loads((case / "case.json").read_text())
    scope = data["scope"]
    session = data["session"]
    if not re.fullmatch(r"app-io\.github\.OneForTwo\.InputExperiment-guided[0-9a-f]{12}\.scope", scope):
        raise ValueError("Ámbito de recuperación ajeno a esta prueba.")
    if not re.fullmatch(r"[A-Za-z0-9_-]+", session):
        raise ValueError("Sesión de recuperación inválida.")
    evidence = {"started": datetime.datetime.now().astimezone().isoformat(), "reason": reason}
    # Cerrar solo el ámbito aleatorio de este ensayo, nunca la guía ni la sesión.
    try:
        stopped = run(["systemctl", "--user", "kill", "--signal=KILL", scope], timeout=3)
        evidence["kill_exit"] = stopped.returncode
        evidence["kill_message"] = stopped.stderr.strip()
    except subprocess.SubprocessError as exc:
        evidence["kill_error"] = str(exc)
    try:
        check = run(["loginctl", "show-session", session, "-p", "User", "--value"], timeout=3)
    except subprocess.SubprocessError as exc:
        evidence["error"] = "No se pudo comprobar la sesión: " + str(exc)
        save(case / "recovery.json", evidence)
        return 1
    if check.returncode or check.stdout.strip() != str(os.getuid()):
        evidence["error"] = "No se reactiva una sesión sin comprobar su propietario."
        save(case / "recovery.json", evidence)
        return 1
    try:
        activated = run(["timeout", "--kill-after=1s", "5s", "loginctl", "--no-ask-password", "activate", session])
        evidence["activate_exit"] = activated.returncode
        evidence["activate_message"] = activated.stderr.strip()
        state = run(["loginctl", "show-session", session, "-p", "Active", "-p", "VTNr"], timeout=3)
        evidence["session_state"] = state.stdout.strip()
    except subprocess.SubprocessError as exc:
        evidence["error"] = "No se pudo completar la vuelta: " + str(exc)
    evidence["finished"] = datetime.datetime.now().astimezone().isoformat()
    save(case / ("recovery-" + uuid.uuid4().hex[:12] + ".json"), evidence)
    save(case / "recovery.json", evidence)
    return 0 if evidence.get("activate_exit") == 0 and "Active=yes" in evidence.get("session_state", "").splitlines() else 1


def prepare_case(run=command, mode="shortcut"):
    if mode not in ("shortcut", "inject-demo", "emergency-check", "revoke-check"):
        raise ValueError("Modo de prueba inválido.")
    session, vt = own_session(run)
    token = uuid.uuid4().hex[:12]
    case = INPUT / "manual-output" / ("guided-" + datetime.datetime.now().strftime("%Y%m%d-%H%M%S-") + token)
    case.mkdir(parents=True)
    scope = "app-" + APP_ID + "-guided" + token + ".scope"
    recovery_unit = "onefortwo-return-" + token
    save(case / "case.json", {"session": session, "vt": vt, "scope": scope,
                             "recovery_unit": recovery_unit, "seconds": DURATION, "mode": mode})
    # Preflight desde un servicio, en el mismo contexto que la recuperación.
    preflight = run(["systemd-run", "--user", "--wait", "--pipe", "--collect",
                     "--unit=onefortwo-preflight-" + token, "--property=RuntimeMaxSec=7s",
                     "/usr/bin/timeout", "--kill-after=1s", "5s", "/usr/bin/loginctl",
                     "--no-ask-password", "activate", session], timeout=10)
    (case / "preflight.log").write_text(preflight.stdout + preflight.stderr)
    if preflight.returncode:
        raise RuntimeError("No funciona la reactivación independiente; no se inicia la prueba.")
    # Armar primero el temporizador, fuera del ámbito que podrá ser terminado.
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
        raise RuntimeError("No se pudo armar la recuperación; no se inicia la prueba.")
    active = run(["systemctl", "--user", "is-active", recovery_unit + ".timer"])
    if active.returncode or active.stdout.strip() != "active":
        raise RuntimeError("El temporizador no está activo; no se inicia la prueba.")
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
        raise RuntimeError("Modo de prueba inválido; no se abre el portal.")
    remaining = data["deadline_monotonic"] - time.monotonic()
    if remaining < 1:
        raise RuntimeError("La recuperación ya ha vencido; no se abre el portal.")
    recovery_unit = data.get("recovery_unit", "")
    if not re.fullmatch(r"onefortwo-return-[0-9a-f]{12}", recovery_unit):
        raise RuntimeError("Falta el temporizador de esta prueba.")
    active = run(["systemctl", "--user", "is-active", recovery_unit + ".timer"])
    remaining = data["deadline_monotonic"] - time.monotonic()
    if active.returncode or active.stdout.strip() != "active" or remaining < 1:
        raise RuntimeError("La recuperación no está armada; no se abre el portal.")
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
            super().__init__(title="OneInTwo — " + ("salida de emergencia" if check == "emergency-check" else
                                                   "retirar permiso" if check else
                                                   "entrada local" if inject else "guía del atajo") + " (30 segundos)")
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
                             ("R2 — cerrar el control con el atajo" if check == "emergency-check" else
                              "R9 — retirar el permiso desde GNOME" if check == "revoke-check" else
                              "Prueba local: ratón y texto" if inject else "Prueba del atajo de salida") + '</span>')
            box.pack_start(title, False, False, 0)
            self.status = Gtk.Label(xalign=0)
            self.status.set_line_wrap(True)
            self.status.set_markup('<b>Preparado. La prueba aún no ha empezado.</b>')
            box.pack_start(self.status, False, False, 0)
            self.phase = None
            self.countdown = Gtk.Label(xalign=0)
            self.countdown.set_markup('<b>Temporizador detenido: pulsa Iniciar cuando estés preparado.</b>')
            box.pack_start(self.countdown, False, False, 0)
            steps = Gtk.Label(xalign=0, yalign=0)
            steps.set_line_wrap(True)
            steps.set_max_width_chars(78)
            steps.set_text((
                "1. Pulsa «Iniciar prueba de 30 segundos». El límite incluye los diálogos.\n\n"
                "2. Si GNOME pide añadir el atajo Ctrl+Shift+F12, pulsa «Añadir» / «Add». "
                "Cuando esta guía diga AHORA, pulsa Y SUELTA Ctrl+Shift+F12 UNA VEZ.\n\n"
                "3. Después, en el diálogo de interacción remota, permite teclado y ratón "
                "y pulsa «Compartir» / «Share», SIN seleccionar pantalla. "
                "Si solo queda un monitor activo, continúa; si hay varios, cancela.\n\n"
                "4. Cuando la guía indique «ZONA DE PRUEBA», haz clic en el recuadro vacío "
                "de abajo. No escribas ni cambies de ventana. Tras la cuenta atrás el prototipo "
                "moverá el puntero un poco y escribirá oneintwo, sin hacer clic.\n\n"
                + ("5. Espera el mensaje AHORA SALIDA. Entonces pulsa Y SUELTA Ctrl+Shift+F12 otra vez. "
                   "La prueba debe cerrar el control. Espera al final de los 30 segundos."
                   if check == "emergency-check" else
                   "5. Espera AHORA RETIRA EL PERMISO. En la barra superior de GNOME, pulsa el indicador "
                   "«Detener compartición de pantalla» / «Stop Screen Sharing» (icono de detener). No uses Cancelar en esta guía: "
                   "se debe retirar desde GNOME. No pulses otra vez el atajo en este paso. Espera al final de los 30 segundos."
                   if check == "revoke-check" else
                   "5. Espera el final a los 30 segundos. Para parar antes: botón Cancelar "
                   "o Ctrl+Shift+F12. Nunca uses Ctrl+Alt+F12.")
            ) if inject else (
                "1. Pulsa «Iniciar prueba de 30 segundos» en esta ventana.\n\n"
                "2. Si GNOME abre «Añadir atajos de teclado» / «Add Keyboard Shortcuts», "
                "comprueba que «Devolver el control local (OneInTwo)» indica Ctrl+Shift+F12. "
                "Pulsa «Añadir» / «Add», ARRIBA A LA DERECHA.\n\n"
                "   Si aparece Ctrl+Alt+F12 o pide compartir pantalla, teclado o ratón: "
                "pulsa «Cancelar» / «Cancel». No autorices eso.\n\n"
                "3. Vuelve a esta guía. ESPERA al mensaje «AHORA: pulsa y suelta Ctrl+Shift+F12». "
                "Entonces pulsa y suelta esa combinación UNA SOLA VEZ.\n\n"
                "4. No pulses más teclas. A los 30 segundos se cerrará la prueba y se intentará "
                "volver automáticamente a tu sesión gráfica principal. La guía seguirá abierta."))
            scroll = Gtk.ScrolledWindow()
            scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
            scroll.set_min_content_height(160)
            scroll.add(steps)
            box.pack_start(scroll, True, True, 0)
            self.target = None
            if inject:
                target_label = Gtk.Label(label="ZONA DE PRUEBA — aquí debe aparecer únicamente: oneintwo", xalign=0)
                box.pack_start(target_label, False, False, 0)
                self.target = Gtk.TextView()
                self.target.set_size_request(-1, 90)
                self.target.set_left_margin(12)
                box.pack_start(self.target, False, False, 0)
            buttons = Gtk.Box(spacing=12)
            self.start = Gtk.Button(label="Iniciar prueba de 30 segundos")
            self.start.get_style_context().add_class("suggested-action")
            self.start.connect("clicked", self.begin)
            buttons.pack_start(self.start, True, True, 0)
            self.cancel = Gtk.Button(label="Cancelar y volver al escritorio")
            self.cancel.set_sensitive(False)
            self.cancel.connect("clicked", self.stop)
            buttons.pack_start(self.cancel, True, True, 0)
            box.pack_start(buttons, False, False, 0)
            note = Gtk.Label(xalign=0)
            note.set_line_wrap(True)
            note.set_text("La vuelta automática cambia de sesión y cierra el prototipo. "
                          "No puede garantizar reparar un compositor o una imagen bloqueados.")
            box.pack_start(note, False, False, 0)
            self.connect("delete-event", self.closing)
            GLib.timeout_add(150, self.tick)

        def begin(self, _button):
            if self.busy or self.case is not None:
                return
            self.busy = True
            self.start.set_sensitive(False)
            self.status.set_text("Preparando la vuelta automática. Todavía no pulses el atajo.")
            def worker():
                try:
                    case = prepare_case(mode=check or ("inject-demo" if inject else "shortcut"))
                    GLib.idle_add(self.launch, case)
                except Exception as exc:
                    GLib.idle_add(self.failed, str(exc))
            threading.Thread(target=worker, daemon=False).start()

        def failed(self, message):
            self.busy = False
            self.status.set_text("No se inicia la prueba: " + message)
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
                self.failed("El plazo de recuperación ya ha vencido; vuelve a preparar el ensayo.")
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
            self.status.set_text("Paso 2: en el diálogo del atajo pulsa «Añadir» / «Add» (arriba derecha).")
            return False

        def stop(self, _button=None):
            self.cancelled = True
            self.cancel.set_sensitive(False)
            self.status.set_text("Cerrando la prueba e intentando volver al escritorio…")
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
            self.countdown.set_markup(f'<span size="large" weight="bold">Vuelta automática en {remaining} segundos</span>')
            log = self.case / "prototype.log"
            text = log.read_text(errors="replace") if log.exists() else ""
            recovery = self.case / "recovery.json"
            if self.process and self.process.poll() is not None and not (self.case / "process.json").exists():
                save(self.case / "process.json", {"exit": self.process.returncode})
                self.log.close()
            if recovery.exists():
                result = json.loads(recovery.read_text())
                ok = result.get("activate_exit") == 0 and "Active=yes" in result.get("session_state", "").splitlines()
                self.show_phase("Prueba cerrada. Sesión gráfica reactivada según el sistema. "
                                     "Confirma en el chat si la imagen y el escritorio responden."
                                     if ok else "Prueba cerrada. No se pudo confirmar la vuelta gráfica; informa en el chat.")
                self.cancel.set_sensitive(False)
                self.countdown.set_text("Prueba terminada. No pulses más el atajo.")
            elif check and self.process and self.process.poll() is not None:
                self.show_phase("Salida recibida. Control cerrado; espera el final de los 30 segundos."
                                if ("Control detenido por emergencia local." in text if check == "emergency-check" else
                                    "Control detenido por cierre del escritorio o dispositivo." in text) else
                                "No se confirmó la salida esperada. Espera el cierre automático.")
            elif check and "CONTROL ACTIVO:" in text:
                self.show_phase("PASO 5 — AHORA SALIDA\nSEGUNDA pulsación: Ctrl+Shift+F12. Pulsa y suelta UNA VEZ."
                                if check == "emergency-check" else
                                "AHORA RETIRA EL PERMISO: pulsa el indicador superior de GNOME «Stop Screen Sharing».", ready=True)
            elif inject and self.process and self.process.poll() is not None:
                self.show_phase("Demostración terminada. Comprueba el recuadro y espera el final de los 30 segundos."
                                if self.process.returncode == 0 else
                                "No se completó la demostración. No pulses más teclas; espera el cierre automático.")
            elif inject and "Demostracion de inyeccion:" in text:
                message = "ZONA DE PRUEBA: haz clic en el recuadro vacío de abajo. No escribas ni cambies de ventana."
                if self.phase != message:
                    self.show_phase(message, ready=True)
                    self.target.grab_focus()
            elif "Atajo local verificado" in text:
                self.show_phase("Atajo recibido. No pulses más teclas; espera la vuelta automática.")
            elif self.process and self.process.poll() is not None:
                self.show_phase("El prototipo ha terminado. No pulses el atajo; espera la vuelta automática.")
            elif ("Comprobacion de salida." in text or (inject and "Antes de abrir RemoteDesktop/EIS" in text)) and not self.cancelled:
                self.show_phase(("PASO 2 — PRIMERA pulsación\n" if check else "") + "AHORA: pulsa y suelta\nCtrl + Shift + F12\nUNA SOLA VEZ" +
                                ("\nDespués autoriza la interacción remota (paso 3)." if inject else ""), ready=True)
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
    parser.add_argument("--inject-demo", action="store_true", help="Guía de inyección local con zona de texto desechable")
    parser.add_argument("--emergency-check", action="store_true", help="Comprobar emergencia con EIS activo")
    parser.add_argument("--revoke-check", action="store_true", help="Comprobar retirada manual de permiso con EIS activo")
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
