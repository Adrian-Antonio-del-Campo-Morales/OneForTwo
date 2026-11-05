# Resultados — Teclado y ratón

## R2 y R9 superados en Ubuntu — 5 de noviembre de 2026

El usuario reanudó expresamente ambas comprobaciones. Antes de abrirlas,
`org.gnome.Mutter.DisplayConfig.GetCurrentState` confirmó una sola pantalla
lógica activa, HDMI-1 (la integrada conectada estaba desactivada).
Se usó la misma base Git `450cbb2e340a381bda01274eb86df0e05934e822` más
cambios locales, y el binario
`47c3afafab7cd8d860565b51f930107cb01d914209f9dad8dbe855ebaecb8d97`.
No se modificó ni reconstruyó C++ ni se repitieron tests. Solo se aclaró la
presentación de la guía: títulos R2/R9, primera/segunda pulsación y no usar
el atajo como sustituto de retirar permisos. Sintaxis Python correcta.
El manifiesto anterior confirmó fuente C++ y binario; su hash del helper ya
no coincide por esos textos. Snapshot actualizado: `resume-build.sha256`.

Desde la raíz se ejecutó primero la guía con `--emergency-check` y, tras
confirmar el usuario escritorio operativo, otra ejecución con `--revoke-check`:

```bash
env -u LD_LIBRARY_PATH GDK_BACKEND=wayland /usr/bin/python3 experiments/input/tools/shortcut_check.py --emergency-check
env -u LD_LIBRARY_PATH GDK_BACKEND=wayland /usr/bin/python3 experiments/input/tools/shortcut_check.py --revoke-check
```

Ambas verificaron Ctrl+Shift+F12 antes de RemoteDesktop/EIS, autorizaron
teclado/puntero sin pantalla y enviaron la demostración breve antes de esperar
la salida. Sin botones ni teclas deliberadamente retenidos y sin red.

| Caso | Evidencia nativa y de la guía | Observación del usuario |
| --- | --- | --- |
| R2 — `guided-20261105-192937-e3dd65eea268` | Temporizador armado 19:29:37.705 CET; EIS activo, segunda pulsación solicitada 19:30:00.071; «Control detenido por emergencia local», salida 0; guía confirmó cierre a las 19:30:04.875 | «Sí, salida recibida y escritorio respondiendo» |
| R9 — `guided-20261105-193129-20e181ab3953` | Temporizador armado 19:31:29.269 CET; EIS activo, retirada solicitada 19:31:39.636; «Disconnected by EIS» y «Control detenido por cierre del escritorio o dispositivo», salida 0; guía confirmó cierre 19:31:43.690 | «Sí: salida recibida, indicador retirado y escritorio respondiendo» tras pulsar el indicador de GNOME, sin usar el atajo ni Cancelar |

Los supervisores independientes actuaron a las 19:30:07.756–19:30:07.771 y
19:31:59.384–19:31:59.400 CET, respectivamente: `activate_exit=0`, sesión 1,
VT 2, `Active=yes`. En ambos `kill_exit=1` porque el prototipo ya había
cerrado normalmente. El cierre correcto ocurrió **antes** del temporizador.
Los casos guardan `case.json`, `prototype.log`, `guide-events.log`,
`process.json` y los JSON de recuperación; logs de lanzamiento en
`manual-output/local-final-20261105/{emergency-resume-ui,revoke-ui}.log`.

Resultado: **R2 y R9 locales superados** con EIS realmente activo y
confirmación humana. Con los casos de portapapeles y archivos ya cerrados,
la fase local prevista en esta configuración soportada queda completada.
Se conserva el incidente anterior y no se afirma que el temporizador repare
un compositor congelado ni que esté demostrada toda la causa del bloqueo.
Cortes con estados retenidos, cruce/retorno, teclado completo y ambos sentidos
pertenecen a la fase entre equipos pendiente. Origen Ubuntu sigue bloqueado
por InputCapture v1; no se inició Windows ni integración del producto.

## R2/R9 preparados; pendientes por decisión del usuario — 5 de noviembre de 2026

Se preparó solo la conexión necesaria para comprobar salida con EIS activo:
`--inject-demo --wait-for-stop` conserva la sesión después de la demostración
y espera un `TargetSignal` de emergencia o cierre; registra cuál recibió y
cierra el backend. Tiempo agotado o interrupción externa no informan éxito.
Sin ese argumento la demostración anterior conserva su comportamiento.
La guía reutiliza la recuperación independiente de 30 s, añade los modos
`--emergency-check` y `--revoke-check` y muestra la acción en su ventana.
No hay teclas o botones deliberadamente retenidos ni conexión con Windows.

Base Git `450cbb2e340a381bda01274eb86df0e05934e822` más cambios locales.
`source .local-state/ubuntu-validation/env.sh` y
`cmake --build experiments/input/build/ubuntu-wayland -j4`: salida 0.
Nuevo binario SHA-256
`47c3afafab7cd8d860565b51f930107cb01d914209f9dad8dbe855ebaecb8d97`.
Las mismas **3 regresiones esenciales** del supervisor siguen pasando; no se
añadieron tests ni se repitió la batería general. La ejecución real afectada
iba a proporcionar la evidencia del nuevo modo, que continúa pendiente.

Comando R2:

```bash
env -u LD_LIBRARY_PATH GDK_BACKEND=wayland /usr/bin/python3 experiments/input/tools/shortcut_check.py --emergency-check
```

Caso `manual-output/guided-20261105-192232-74937ef202aa/`:
atajo previo concedido y comprobado, pero la preparación EIS rechazó con
«el EIS expone varias regiones; el experimento solo admite un monitor».
Salida **1**, antes de la demostración y de la espera de salida. No se
inyectó. Temporizador armado 19:22:32.526 CET; recuperación
19:23:02.580–19:23:02.597, `activate_exit=0`, sesión 1/VT 2 activa.
El prototipo ya había terminado; no había ámbito vivo que matar.

Se pidió dejar un único monitor y repetir solo R2/R9. El usuario decidió
**«Prefiero dejar las dos pruebas de entrada pendientes»**. No se repitió R2
ni se inició R9. Se cerraron las ayudas propias que siguieran abiertas.
La demostración local anterior conserva su éxito; no se presenta este rechazo
como regresión de inyección ni como prueba de recuperación.

Para reanudar, usuario presente y un monitor activo. R2: seguir AHORA para
comprobar el atajo y, tras el texto, AHORA SALIDA para pulsarlo otra vez.
R9: mismo guion con `--revoke-check`; tras el texto, pulsar el indicador
superior de GNOME «Stop Screen Sharing». Se contrastó esa acción con el
`remoteAccess.js` instalado (`gresource extract
/usr/lib/gnome-shell/libshell-18.so /org/gnome/shell/ui/status/remoteAccess.js`):
el clic ejecuta `handle.stop()`. No se sustituyó por matar el prototipo o
cerrar su propia sesión mediante API. R9 no tiene resultado de ejecución.

Comandos, log de la guía y manifiesto SHA-256 en
`manual-output/local-final-20261105/`; el caso conserva `case.json`,
`prototype.log`, `guide-events.log`, `process.json` y las recuperaciones.

## Inyección local Wayland verificada — 5 de noviembre de 2026

El usuario solicitó y realizó la demostración local supervisada. Confirmó
**texto exacto `oneintwo`, movimiento del puntero y escritorio respondiendo**.
Se usó el recuadro desechable de la guía, sin clics sintetizados, captura,
red ni conexión con Windows. Esta ejecución abrió RemoteDesktop/EIS después
de comprobar físicamente Ctrl+Shift+F12; no se observó la congelación anterior.

Misma base Git `450cbb2e340a381bda01274eb86df0e05934e822` más cambios
locales; mismo binario SHA-256
`b0d8e9ebedde3d9cd98cfd11e3843ac009beab9c30bb3525d8eae06168b398d8`.
Antes del arranque se comprobaron las seis fuentes y el ejecutable con
`sha256sum -c experiments/input/manual-output/ubuntu-local-20261105/recovery-hardening.sha256`:
todo correcto. No se modificó ni reconstruyó C++ para esta demostración.

Se reutilizó la guía y el supervisor independiente de 30 s con un modo
explícito `--inject-demo`, instrucciones de autorización, recuadro desechable
y aviso para enfocarlo durante la cuenta atrás de 5 s. El modo por defecto
sigue probando solo el atajo. La guarda exige temporizador activo y plazo
vigente también para inyección. Se amplió esa comprobación dentro de las
**mismas 3 regresiones esenciales** del helper; comando
`/usr/bin/python3 experiments/input/tests/test_shortcut_check.py`: **3 tests,
OK**, sin GTK ni comandos reales. `git diff --check` correcto; no se repitió
la batería general.

Comando desde la raíz:

```bash
env -u LD_LIBRARY_PATH GDK_BACKEND=wayland /usr/bin/python3 experiments/input/tools/shortcut_check.py --inject-demo
```

La guía lanzó dentro de su ámbito propio el binario con `--inject-demo
--arm-seconds 5 --max-seconds 30`, limitado además por `timeout` al plazo
restante del temporizador. Evidencia en
`manual-output/guided-20261105-141528-30d8de65e4b9/`:

- Sesión propia 1, UID 1000, VT 2; preflight independiente salida 0.
- Temporizador armado **14:15:28.737 CET**. La guía anunció AHORA a las
  **14:15:28.958** y ZONA DE PRUEBA a las **14:15:44.432**.
- `prototype.log`: atajo Ctrl+Shift+F12 concedido, verificación previa,
  cuenta atrás 5…1 y «Demostracion finalizada. Revise el texto y el cursor».
  `process.json`: **salida 0**; guía registró final a las **14:15:50.888**.
- Recuperación independiente ejecutada **14:15:58.796–14:15:58.812**:
  `activate_exit=0`, `VTNr=2`, `Active=yes`. `kill_exit=1` porque el ámbito
  ya había terminado normalmente. La guía confirmó cierre a las 14:15:58.849.
- Observación humana: **«Sí: texto correcto, movimiento y escritorio
  respondiendo.»** El agente no inspeccionó visualmente el escritorio.

Resultado: preparación RemoteDesktop/EIS, movimiento absoluto breve,
pulsación/liberación de las letras de `oneintwo` y cierre normal validados
localmente en esta ejecución. El temporizador actuó cuando el prototipo ya
había acabado y la sesión seguía activa; no demuestra recuperación de una
imagen congelada, de otra VT, ni con teclas/botones retenidos. No se han
probado clics, rueda, modificadores, retirada de permisos, cruce/retorno ni
entrada entre equipos. EXP-INPUT-02 conserva esos límites; origen Ubuntu
sigue bloqueado por InputCapture v1 frente al requisito v2.

## Atajo aislado verificado con guía y temporizador de 30 s

5 de noviembre de 2026, sesión Wayland 1, VT 2. Base Git
`450cbb2e340a381bda01274eb86df0e05934e822` más cambios locales, binario
`b0d8e9ebedde3d9cd98cfd11e3843ac009beab9c30bb3525d8eae06168b398d8`.
Se repitió únicamente `--check-target`: GlobalShortcuts, sin RemoteDesktop,
EIS, captura, inyección ni conexión con Windows.

Comando desde la raíz, Python del sistema sin las bibliotecas locales:

```bash
env -u LD_LIBRARY_PATH GDK_BACKEND=wayland /usr/bin/python3 experiments/input/tools/shortcut_check.py
```

- Primer ensayo guiado: `manual-output/guided-20261105-140452-8d87e9ff735e/`.
  El portal concedió Ctrl+Shift+F12 y el prototipo anunció la espera, pero el
  usuario no vio «AHORA» ni recuerda la cuenta atrás. Terminó por timeout,
  salida 124, sin verificación del atajo. El temporizador ejecutó la
  reactivación a las 14:05:22.254–14:05:22.270 CET, salida 0, sesión activa.
  Se conserva en `recovery-51e42fa99d55.json`; el cierre posterior de la guía
  generó otra recuperación con `reason=cancel`. No se atribuye éxito visual
  al primer ensayo.
- Corrección limitada a la guía: estado y cuenta atrás arriba, «AHORA» grande,
  aviso para traer su ventana delante y registro de cambios de fase.
  Comprobación de sintaxis y `git diff --check` correctas. No se añadieron
  pruebas ni se repitió la batería general por este ajuste de presentación.
- Repetición solicitada: `manual-output/guided-20261105-140927-fa41253e7d58/`.
  Temporizador armado a las **14:09:27.707 CET**; `guide-events.log` registra
  «AHORA» a las **14:09:28.005**, «Atajo recibido» a las **14:09:32.662** y
  cierre a las **14:09:57.904**. `prototype.log` confirma «Atajo local
  verificado; cerrando sesiones» y `process.json` salida **0**.
  Recuperación independiente 14:09:57.777–14:09:57.792, `activate_exit=0`,
  `VTNr=2`, `Active=yes`. `kill_exit=1` indica que el ámbito del prototipo
  ya había terminado normalmente, no un fallo de cierre de un proceso vivo.
  El usuario confirmó: **vio los mensajes y la cuenta atrás, y la imagen,
  ratón y teclado respondieron durante toda la prueba**.

Resultado: atajo local Ctrl+Shift+F12 aislado y ejecución del temporizador a
los 30 s verificados. La sesión ya estaba activa: no se ha probado retorno
desde otra VT ni reparación de una imagen congelada. La recuperación con
RemoteDesktop/EIS y la inyección siguen pendientes; EXP-INPUT-02 permanece
abierto. No se inicia otra prueba por este resultado.

## Preparación de recuperación de 30 s y guía

5 de noviembre de 2026. El usuario matizó el ensayo anterior: cree que se
congeló la imagen, aunque el sistema siguió funcionando por debajo. Esta
observación no confirma una congelación del núcleo ni permite determinar si
el atajo se activó durante la configuración o durante la espera del prototipo.
El ensayo de 60 s continúa sin verificar el atajo ni la recuperación gráfica.

Pidió otra prueba con vuelta automática a la sesión principal a los 30 s y
pasos claros dentro de una ventana. Se añadió **solo una guía experimental**
en `tools/shortcut_check.py`, sin integrar la aplicación principal. No abre
portales al arrancar: el usuario debe pulsar «Iniciar prueba de 30 segundos».
Indica el orden: Iniciar → comprobar la fila Ctrl+Shift+F12 → «Añadir» / «Add»
arriba a la derecha en GNOME → esperar el mensaje «AHORA» en la guía → pulsar
y soltar el atajo una vez → esperar el retorno. «Cancelar y volver al
escritorio» cierra el ensayo e intenta reactivar la sesión. Si GNOME pide
pantalla/control remoto o muestra el atajo antiguo, la instrucción es cancelar.
El nombre del botón se contrastó con la [definición oficial del diálogo de GNOME 50.0](https://github.com/GNOME/gnome-control-center/blob/50.0/global-shortcuts-provider/cc-global-shortcut-dialog.blp).

La guía prepara primero un servicio de comprobación y un temporizador
transitorio de usuario **fuera del ámbito del prototipo**. A los 30 s el
servicio cierra únicamente el ámbito aleatorio de este ensayo con SIGKILL y
llama a `loginctl --no-ask-password activate SESION`. Comprueba propietario,
VT y estado; no termina ni desbloquea sesiones, no reinicia GNOME y no usa
sudo. El hijo se niega a abrir el portal sin temporizador activo o si ya ha
vencido el plazo. `RuntimeMaxSec=30s` y `timeout` añaden límites al prototipo.
La guía muestra cuenta atrás y el paso actual leyendo solo su log sintético.
Se usa Python del sistema con PyGObject/GTK3 ya disponibles; ninguna instalación.

### Evidencia de preparación

- Sesión propia identificada: ID 1, UID 1000, Wayland, VT 2, activa.
  `loginctl --no-ask-password activate 1` devolvió 0. La misma llamada desde
  un servicio transitorio independiente también devolvió 0, sin autenticación.
  La política local de logind admite cambio de VT para sesión activa/inactiva.
  No se cambió a otra consola para comprobarlo.
- Comprobación real del supervisor **sin prototipo ni portal**: se armó el
  temporizador y se puso `sleep 90` dentro del ámbito de ensayo. A las
  **13:52:42.998 CET** se ejecutó la recuperación; terminó a las
  **13:52:43.012 CET**, `kill_exit=0`, proceso inocuo terminado por SIGKILL
  (`-9`), `activate_exit=0`, `VTNr=2`, `Active=yes`. Logs en
  `manual-output/guided-20261105-135212-67659af36348/`: `case.json`,
  `preflight.log`, `timer.log`, `sleep.log`, `recovery.json` y
  `preflight-result.json`. `kind=supervisor-preflight-sleep` distingue este
  caso de una prueba real. Demuestra ejecución independiente del temporizador,
  cierre y reactivación de la sesión ya activa; **no demuestra retorno desde
  otra VT ni recuperación de un compositor o imagen congelados**.
- Por petición de limitar el mantenimiento, se conservan solo **3 pruebas
  nuevas imprescindibles** del supervisor: cierre/activación aunque falle el
  envío de señal, rechazo de inicio sin temporizador activo o con plazo vencido,
  y aborto si no se puede armar el temporizador. Comando:
  `/usr/bin/python3 experiments/input/tests/test_shortcut_check.py`;
  **3 tests, OK, salida 0**, sin comandos reales, GTK ni bus.
  No se amplió ni repitió la batería general 579/0, cuya evidencia anterior
  permanece válida para el C++ sin cambios de esta preparación.

Durante la pausa AFK no se abrió la guía ni se inició el ensayo interactivo;
la consulta de timers y procesos no encontró ensayos pendientes. Después del
regreso explícito del usuario se realizaron los dos casos descritos arriba.
La observación visual corresponde al usuario, no a inspección del agente.

## Ensayo corregido con límite externo de 60 s — 5 de noviembre de 2026

**Límite del proceso comprobado; atajo todavía no verificado.** El usuario
pidió repetir con timeout de 60 s. Se inició solo `--check-target` de la versión
corregida, sin RemoteDesktop, EIS, captura ni red. La prueba alcanzó el límite
y requirió cierre forzado; no hay registro de la descripción concedida, del
anuncio de espera ni de la activación del atajo. Falta la observación del
usuario sobre el diálogo y la respuesta del escritorio; no se clasifica como
éxito ni como otra congelación sin esa confirmación.

Misma base Git y binario `b0d8e9ebedde3d9cd98cfd11e3843ac009beab9c30bb3525d8eae06168b398d8`;
se comprobaron los SHA-256 de fuentes y ejecutable antes de arrancar.
Identidad de aplicación aún preparada y sin otro prototipo activo.
`loginctl` confirmó `VTNr=2`, `Type=wayland`, `Active=yes` antes del ensayo.

Comando, desde la raíz:

```bash
env -u LD_LIBRARY_PATH systemd-run --user --scope --collect \
  --unit=app-io.github.OneForTwo.InputExperiment-shortcut60a \
  --property=RuntimeMaxSec=60s --property=TimeoutStopSec=3s \
  /bin/bash -c 'source .local-state/ubuntu-validation/env.sh; exec timeout --signal=TERM --kill-after=3s 60s ./experiments/input/build/ubuntu-wayland/input_experiment --check-target --max-seconds 60'
```

Dos guardas: el gestor de usuario limita la vida del ámbito a 60 s y espera
3 s antes de forzar su cierre; `timeout` aplica también 60 s y 3 s al proceso.
Los 60 s incluyen inicio y permiso, no empiezan después del diálogo.
Durante la ejecución, `systemctl --user show` confirmó
`RuntimeMaxUSec=1min`, `TimeoutStopUSec=3s`, `ActiveState=active`.

Hora local CET (`Europe/Madrid`):

- Inicio **13:35:38**.
- Journal del ámbito: **13:36:38**, `Scope reached runtime time limit. Stopping.`
- **13:36:41**, `Failed with result 'timeout'`; comando termina con **137**.
- Recorrido posterior de `/proc/*/exe`: `input_experiment_running_pids=[]`.

El 137 corresponde a esta terminación forzada observada; no se registra como
un crash espontáneo del prototipo. Se avisó al usuario de no pulsar el atajo
una vez terminada la ejecución. No se lanzó una segunda prueba sin recoger
su observación. No se puede concluir que el temporizador recupere un compositor
bloqueado: solo queda probado que el gestor cerró el proceso y no dejó ese
prototipo activo.

Logs ignorados en `manual-output/ubuntu-local-20261105/`:
`shortcut-only-60s-01.log`, `shortcut-only-60s-01-guard.log`,
`shortcut-only-60s-01-supervisor.log`, `shortcut-only-60s-01-cleanup.log`.
El journal se consultó únicamente para el ámbito de esta prueba en el
intervalo 13:35:30–13:36:50. Manual actualizado con ambas guardas de 60 s;
sin cambios adicionales de código, dependencias o configuración permanente.

## Preparación corregida tras el incidente — 5 de noviembre de 2026

**Código preparado, compilación nativa y pruebas aisladas superadas; sin nueva
prueba gráfica.** El usuario pidió preparar el experimento para evitar otro
bloqueo. Se corrigieron el conflicto del atajo y el orden de preparación;
**no se afirma que la causa completa de la congelación esté demostrada ni que
la recuperación gráfica ya funcione**. EXP-INPUT-02 sigue abierto para esa
validación. No se abrió GlobalShortcuts, RemoteDesktop, EIS, captura ni red
al preparar estas correcciones.

Revisión base: `450cbb2e340a381bda01274eb86df0e05934e822`, rama
`validation/ubuntu`, más los cambios locales anteriores y los descritos aquí.
Sin commit ni publicación, sin dependencias nuevas ni instalación global.

### Cambios y evidencia que los justifica

1. **Atajo del destino Wayland: Ctrl+Shift+F12**, identificador
   `oneintwo-emergency-v2`. No reutiliza el identificador de la concesión
   persistida de Ctrl+Alt+F12. Los [valores de Mutter 50.1](https://raw.githubusercontent.com/GNOME/mutter/50.1/data/org.gnome.mutter.wayland.gschema.xml.in)
   asignan Ctrl+Alt+F12 al cambio a VT 12 y su [implementación](https://raw.githubusercontent.com/GNOME/mutter/50.1/src/core/keybindings.c)
   lo registra como combinación no enmascarable. Consulta del valor por defecto
   **instalado**, sin tocar configuración:
   `GSETTINGS_BACKEND=memory gsettings get org.gnome.mutter.wayland.keybindings switch-to-session-12`
   → `['<Primary><Alt>F12']`, log `default-vt-shortcut.log`.
   La consulta normal previa devolvió lo mismo con avisos dconf por el sandbox;
   se usó el backend en memoria para comprobar el valor de esquema sin
   escribir caché. Esto confirma el conflicto del diseño previo; no prueba
   que todo el incidente fuese únicamente un cambio de VT.
2. **`--check-target` en Wayland solo prepara GlobalShortcuts.** No llama a
   RemoteDesktop ni conecta EIS, no inicia emulación y no necesita geometría.
   La prueba fallida anterior permanece registrada como fallo; cambiar el
   atajo no convierte aquel ensayo en éxito. Windows/X11 mantienen el
   procedimiento anterior y Ctrl+Alt+F12; no se han vuelto a ejecutar allí.
3. **Verificación física antes del control.** `--target` y `--inject-demo`
   reservan el nuevo atajo y esperan activación **y liberación** de su propia
   sesión, con límite de 30 s. Solo después solicitan RemoteDesktop y EIS.
   Rechazo, timeout o cierre del atajo abortan sin abrir control. La pulsación
   usada para verificar se consume y no se entrega como emergencia del control
   que aún no ha comenzado. No se guarda una marca entre ejecuciones.
4. **Emulación diferida y cierre.** Antes `EI_EVENT_DEVICE_ADDED` iniciaba
   emulación inmediatamente con secuencia 0, también en la comprobación que
   se describía como «sin inyección». Ahora se esperan dispositivos reanudados
   y `start_emulating` solo se llama al enviar el primer evento real, con
   secuencia creciente. Se solicitan y comprueban capacidades de botones y
   rueda; se mantienen referencias propias a los dispositivos. Pausa, retirada
   o desconexión hacen rechazar nuevos eventos y detener el flujo, sin
   reanudarlo automáticamente. Al cerrar se envían las liberaciones del estado
   propio aún utilizable, se finalizan sus transacciones y se liberan referencias
   y descriptor. Se elimina un segundo `close(fd)` en el fallo de setup del
   emisor porque [libei toma posesión del descriptor](https://libinput.pages.freedesktop.org/libei/api/group__libei-sender.html).
5. **La demostración para al primer error** de movimiento, pulsación o
   liberación, limpia el destino y devuelve 1; una salida local se informa
   como interrupción y no como demostración finalizada correctamente.

### Comprobaciones realizadas sin escritorio

Desde la raíz, usando las dependencias persistentes del proyecto:

```bash
source .local-state/ubuntu-validation/env.sh
cmake --build experiments/input/build/ubuntu-wayland -j4
env -u DISPLAY -u WAYLAND_DISPLAY -u XDG_RUNTIME_DIR -u DBUS_SESSION_BUS_ADDRESS \
  ./experiments/input/build/ubuntu-wayland/input_experiment --self-test
env -u DISPLAY -u WAYLAND_DISPLAY -u XDG_RUNTIME_DIR -u DBUS_SESSION_BUS_ADDRESS \
  ./experiments/input/build/ubuntu-wayland/input_self_test
env -u DISPLAY -u WAYLAND_DISPLAY -u XDG_RUNTIME_DIR -u DBUS_SESSION_BUS_ADDRESS \
  ./experiments/input/build/ubuntu-wayland/input_experiment --help
```

Todos devolvieron **0**. Binario nativo Wayland + TLS reconstruido:

```text
PASS native_portal_response_and_shortcut (aislado, sin bus)
Comprobaciones: 579, fallos: 0
```

Las 579 comprobaciones del núcleo y flujos incluyen 41 adicionales: preparación
solo del atajo, prohibición de abrir control sin verificación, rechazo del
atajo, cierre previo y fallos en cada fase de autorización. El binario nativo
comprueba además Response y callbacks Activated/Deactivated en memoria:
ignorar sesión ajena e identificador antiguo, exigir activación previa a la
liberación, consumir la comprobación sin emitir una emergencia y detectar
cierre del atajo. Estos grupos nativos son adicionales al contador 579.
La ayuda muestra el atajo de cada backend y el alcance de `--check-target`.

Logs ignorados en `manual-output/ubuntu-local-20261105/`:
`build-recovery-hardening.log`, `self-test-recovery-hardening.log`,
`portable-self-test-recovery-hardening.log`, `help-recovery-hardening.log`,
`default-vt-shortcut.log`, `recovery-hardening.sha256`.
El último contiene SHA-256 de los seis archivos fuente afectados y el binario;
el ejecutable es `b0d8e9ebedde3d9cd98cfd11e3843ac009beab9c30bb3525d8eae06168b398d8`.
`git diff --check` no informa errores. La evidencia histórica Windows se conserva.

**Límite de esta entrega:** no hay emisor EIS simulado ni servidor real en
estos tests. Compilan su nueva gestión de dispositivos, pero no prueban que
el compositor acepte sus eventos o recupere el escritorio. Tampoco se ha
probado gráficamente la nueva reserva del atajo. El siguiente ensayo posible
es solo GlobalShortcuts, supervisado y con recuperación independiente; no
abrir control/inyección ni conectar Windows todavía. El origen Ubuntu sigue
bloqueado por InputCapture v1 frente al requisito v2; no se amplió captura ni
se modificaron ajustes de Mutter, consola, controladores o energía.

## Validación local iniciada — 5 de noviembre de 2026

**Entrada bloqueada por fallo de recuperación comunicado por el usuario.**
Al pulsar Ctrl+Alt+F12 durante la última espera, el usuario observó que todo
el PC quedó congelado y confirmó que tuvo que reiniciar para recuperarlo.
El proceso no recibió la emergencia y terminó por tiempo. No repetir el atajo ni iniciar inyección hasta aclarar lo ocurrido y
preparar una recuperación independiente comprobada. Cancelación de permisos
verificada; las pruebas aisladas no certifican la recuperación del escritorio.
No se ejecutó captura ni red ni se enviaron movimientos o teclas de prueba.

Base Git: `450cbb2e340a381bda01274eb86df0e05934e822`, rama
`validation/ubuntu`, más las correcciones locales descritas abajo y en los
otros experimentos. Sin commit ni publicación. Entorno y bibliotecas: los
de la compilación inicial que sigue, con dependencias persistentes del proyecto.

### Diagnóstico real y bloqueo de captura

Comando desde la raíz, tras cargar `.local-state/ubuntu-validation/env.sh`:

```bash
./experiments/input/build/ubuntu-wayland/input_experiment --diagnose
```

Salida 0; `manual-output/ubuntu-local-20261105/diagnose-01.log`:

```text
Plataforma: Linux Wayland (InputCapture + RemoteDesktop + libei)
Captura declarada: no
Inyeccion declarada: si
InputCapture v1 con teclado y puntero; RemoteDesktop v2; GlobalShortcuts v1
```

El backend exige **InputCapture v2**, mientras que este escritorio expone v1.
Ubuntu como origen queda bloqueado con este backend; no se redujo el requisito,
no se ejecutó captura ni se usó X11 como sustituto. No es una afirmación de
imposibilidad de InputCapture v1: admitirlo requeriría otro flujo y validación.
La inyección declarada depende todavía de permisos, atajo y dispositivos EIS.
La etiqueta histórica de diagnóstico «Actividad local del destino: si» se
refiere a `targetLocalOverride` por GlobalShortcuts, no a detección de toda la
actividad física como en Windows.

Recuperación preparada: `loginctl show-user UID -p Display --value` y
`loginctl show-session SESION -p VTNr -p Type -p Active` devolvieron
`VTNr=2`, `Type=wayland`, `Active=yes`. En esta máquina Ctrl+Alt+F2 es la
vuelta prevista al escritorio si Ctrl+Alt+F12 cambia de consola. No se llegó
a probar el atajo ni se cambió de consola durante esta sesión.

### Preparación real fallida y corrección

Se añadió `--check-target --max-seconds 30` para preparar los permisos y
comprobar Ctrl+Alt+F12 **sin llamar a ninguna función de inyección ni abrir
red**. Es necesario comprobar la salida antes de una demostración real.
También se corrigió `--inject-demo` para iniciar la guarda del destino y
atender señales de parada durante la cuenta atrás y la demostración; antes
solo consultaba la interrupción de consola. Estos caminos de escritorio aún
no se han verificado con éxito.

La primera ejecución de `--check-target` se preparó para que el usuario
cancelara el permiso, pero terminó antes con:

```text
No se pudo preparar el destino: CreateSession: el portal no respondió a tiempo
process_exit=1
```

El log también conserva avisos GLib de conexión inválida durante la limpieza.
El usuario se ausentó momentáneamente y no pudo confirmar ningún diálogo:
**no se registra como permiso denegado ni atajo probado**.

Causa concreta encontrada en `PortalClient::request`: `onResponse` esperaba
un `RequestWait*`, pero la suscripción genérica le entregaba `PortalClient*`.
La respuesta no actualizaba la espera y podía corromper el objeto de conexión.
Además, los consumidores esperan una tupla `(a{sv})`, mientras que la función
devolvía directamente `a{sv}`. Corrección:

- Suscribir `Response` con el estado de espera correcto, también si cambia
  la ruta devuelta por el portal.
- Mantener el contrato de tupla al entregar resultados y liberar correctamente
  la referencia del diccionario.
- Añadir una regresión de respuesta sintética en memoria al `--self-test` de
  la aplicación compilada con Wayland, sin crear backend, bus ni permisos.

Comandos finales, salida 0:

```bash
source .local-state/ubuntu-validation/env.sh
cmake --build experiments/input/build/ubuntu-wayland -j4
./experiments/input/build/ubuntu-wayland/input_experiment --self-test
```

```text
PASS native_portal_response (aislado, sin bus)
Comprobaciones: 538, fallos: 0
```

La comprobación nativa de respuesta es adicional a las 538 del núcleo y flujos;
`input_self_test` conserva su alcance portable. No demuestra aún que el portal
real autorice o cancele correctamente. SHA-256 del adaptador corregido:
`891ddccf87c83ea30c66bdbce23058d5b0145bdba237808f1412b84dfe068b93`.

Logs ignorados en `manual-output/ubuntu-local-20261105/`: `diagnose-01.log`,
`target-denied-01.log` (el nombre identifica el caso previsto, no su resultado),
`build-local-exit.log`, `self-test-local-exit.log`, `build-response-fix.log` y
`self-test-response-fix.log`. El diagnóstico tuvo límite externo de 30 s y la
preparación de 240 s, con `timeout --signal=TERM --kill-after=3s`.

### Reanudación: cancelación real y tipo de sesión

Se repitió `--check-target --max-seconds 30` con la corrección anterior.
La ejecución `target-denied-02.log` terminó antes del diálogo, salida 1:
`RemoteDesktop.CreateSession no devolvió session_handle`. No es una denegación.

La [especificación oficial de RemoteDesktop](https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/main/data/org.freedesktop.portal.RemoteDesktop.xml)
conserva `session_handle` como `s` por compatibilidad, aunque contiene una ruta
de objeto. El adaptador solo aceptaba `o`. Se corrigió exclusivamente la
lectura de RemoteDesktop a `s`, validando la ruta con
`g_variant_is_object_path`; InputCapture v2 conserva su contrato propio.
La regresión nativa sintética ahora incluye ese campo y utiliza el mismo
lector que la preparación real. Compilación y `input_experiment --self-test`:
salida 0, `PASS native_portal_response`, **538/0**; logs
`build-session-handle.log` y `self-test-session-handle.log`.
SHA-256 de `wayland_backend.cpp` tras esta corrección:
`454d7f94c804c327d75f15a5c3fb52d68a3717389086c7fa130b7f505a8cb52c`.

Nueva ejecución, mismas variables y guarda externa de 240 s:

```bash
./experiments/input/build/ubuntu-wayland/input_experiment --check-target --max-seconds 30
```

El usuario confirmó pulsar Cancelar. `target-denied-03.log` conserva
`No se pudo preparar el destino: el usuario canceló la autorización`,
`process_exit=1`, sin los avisos GLib de conexión inválida del primer intento.
**Cancelación verificada**, sin pasar a inyección ni red.

### Identidad para GlobalShortcuts

`--check-target --max-seconds 60` con límite externo de 240 s terminó con
`CreateSession: GDBus.Error:org.freedesktop.portal.Error.NotAllowed: An app id is required`
(`target-shortcut-01.log`, salida 1). El [portal 1.21.1](https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/1.21.1/src/global-shortcuts.c)
rechaza atajos sin identidad; el [reconocimiento de aplicaciones host](https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/1.21.1/src/xdp-app-info-host.c)
requiere ámbito de aplicación y entrada `.desktop` disponible.

Se preparó `io.github.OneForTwo.InputExperiment.desktop` dentro del directorio
local ignorado de logs y un enlace del mismo nombre en
`/home/pholtergate/.local/share/applications/`, solo para el usuario, sin sudo,
autoinicio ni instalación global. La identidad se proporciona mediante un
ámbito transitorio `systemd-run --user --scope --collect`, sin modificar servicios.
La prueba del ámbito con `cat /proc/self/cgroup` devolvió 0.

El primer lanzamiento con el entorno de compilación cargado produjo un error
del propio `systemd-run` (`calendar_spec_weekday_conflicts`, salida 127,
`target-shortcut-02.log`): `LD_LIBRARY_PATH` local incluía una libsystemd
incompatible con la herramienta instalada. Se corrigió el comando: iniciar
`systemd-run` con `env -u LD_LIBRARY_PATH` y cargar `env.sh` solo dentro de su
proceso hijo. No se modificaron bibliotecas del sistema.

```bash
env -u LD_LIBRARY_PATH systemd-run --user --scope --collect \
  --unit=app-io.github.OneForTwo.InputExperiment-shortcut03 \
  /bin/bash -c 'source .local-state/ubuntu-validation/env.sh; exec timeout --signal=TERM --kill-after=3s 240s ./experiments/input/build/ubuntu-wayland/input_experiment --check-target --max-seconds 60'
```

La recuperación se volvió a consultar: VT 2, Wayland, sesión activa
(`recovery-02.log`). Esta comprobación aún no equivale a probar el atajo.

### Preparación con permisos y límite de un monitor

El usuario confirmó autorizar los diálogos de `target-shortcut-03.log`.
La identidad de aplicación permitió avanzar a EIS, pero se rechazó la
preparación: `el EIS expone varias regiones; el experimento solo admite un monitor`,
salida 1. No se pulsó el atajo ni se inyectó entrada.
Consulta de solo lectura a `org.gnome.Mutter.DisplayConfig.GetCurrentState`:
**2 monitores lógicos y 2 físicos**, escalas 1,5 y 1,0; log
`monitor-layout.log`, sin identificadores de hardware. El usuario desactivó
uno temporalmente; nueva consulta: **1 monitor lógico**, `monitor-single.log`.
No se amplió el experimento a varios monitores ni se alteró la configuración
por código.

Al revisar la recepción de emergencia se detectó una suscripción incorrecta:
`Activated` de GlobalShortcuts se emite desde el objeto principal del portal,
no desde la ruta de Session. Referencia: [implementación oficial v1](https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/1.20.3/src/global-shortcuts.c).
Se corrigió la ruta de suscripción a `/org/freedesktop/portal/desktop` y se
filtra el primer argumento por la sesión propia antes de aceptar el
identificador del atajo. Compilación y prueba aislada: salida 0,
`PASS native_portal_response` más **538/0**; logs
`build-shortcut-signal.log` y `self-test-shortcut-signal.log`. Esta regresión
sintética de respuesta no comprueba la suscripción; su evidencia debe ser
la pulsación real siguiente. SHA-256 del adaptador:
`ca17d1358e3639bdc04aaf9663c5a0475e0810b41a0f32e9ff84c5416852560c`.

### Espera de emergencia: todavía no verificada

`target-shortcut-04.log`, un solo monitor, alcanzó
`Comprobacion sin inyeccion ni red. Pulse Ctrl+Alt+F12 ahora.`, pero EIS se
desconectó inmediatamente después. El programa detectó la interrupción y
salió 1, sin inyectar. El usuario indicó que no apareció un segundo diálogo;
esto no demuestra falta de autorización del atajo, pues BindShortcuts devolvió
su identificador y la preparación había avanzado a EIS. La causa de esta
interrupción no quedó determinada.

Se añadió diagnóstico de libei activable solo con `ONEINTWO_EI_DEBUG=1` y se
repitió `--check-target --max-seconds 60` en el ámbito de usuario con guarda
externa de 240 s. `target-eis-debug-05.log` muestra teclado, puntero y región
absoluta, preparación completa y conexión mantenida hasta agotar los 60 s.
No se recibió señal de emergencia; salida 1. El cierre al final de ese log
lo inicia el propio cliente durante la limpieza, no confirma la desconexión
anterior. No se enviaron movimientos, botones ni teclas. No se presume
una causa de carrera sin más evidencia.

Para distinguir un identificador devuelto de una combinación utilizable se
añadió al log la `trigger_description` de la salida concedida por el portal.
Compilación en `build-shortcut-description.log`. No se habilita demostración
con inyección hasta superar la prueba física de emergencia.

### Incidente comunicado: congelación al pulsar el atajo

Después de la última espera, el usuario confirmó que **sí pulsó Ctrl+Alt+F12**
y que «todo el PC quedó congelado cuando lo hice». Se registra como observación
del usuario y **fallo de recuperación**, no como simple prueba pendiente ni
como éxito del atajo. Después confirmó que **tuvo que reiniciar para
recuperar el equipo**. No hay evidencia suficiente para atribuirlo al compositor, EIS, la
reserva del atajo o un cambio de consola virtual; no se modifica el código
por una causa supuesta ni se repite la operación para reproducir el bloqueo.

La ejecución asociada es `target-eis-debug-05.log`: alcanzó la espera,
registró cambios de modificadores, no recibió `Emergency` y terminó tras
60 s con salida 1. El cierre EIS del final fue iniciado por el cliente.
No se enviaron movimientos, botones ni teclas; sí se abrió RemoteDesktop,
se conectó EIS y se inició su transacción de emulación durante la preparación.
Por tanto «sin inyección de eventos» no significa que no hubiera una sesión
real de control autorizada.

Comprobaciones posteriores, sin abrir nuevos diálogos:

- Recorrido de `/proc/*/exe`, comparando con la ruta exacta del binario:
  `input_experiment_running_pids=[]`; no quedó ese prototipo activo.
- `journalctl -b --since '2026-11-05 13:03:30' --until '2026-11-05 13:05:00' --no-pager -o short-iso _UID=1000 --grep='[Ee][Ii][Ss]|libei|emulat|[Mm]utter|gnome-shell|[Ss]egfault|[Cc]rash|[Ff]ailed|[Ff]reeze|[Tt]imeout'`:
  `-- No entries --`, conservado en `freeze-session-journal.log`.
  La ausencia de mensajes en ese filtro no descarta una congelación ni prueba
  su causa; no es una inspección del núcleo ni de todos los arranques.

La versión ejecutada fue la de `build-eis-diagnostic.log`; la descripción de
la combinación se añadió después y **no se ha ejecutado en el escritorio**.
No se habilitan demostración, captura ni pruebas de entrada con Windows.

### Recuperación mediante reinicio y registros del arranque afectado

El usuario confirmó que fue necesario reiniciar. Consultas de solo lectura:
`date --iso-8601=seconds`, `uptime -s`, `journalctl --list-boots --no-pager`.
Nuevo arranque: **2026-11-05 13:10:57 CET (Europe/Madrid)**,
`6e1a6c6cf11a4266ad8256229db2dbe5`; arranque afectado:
`d67316910f8348f19064305dcc789bb3`, cuyo journal termina a las 13:10:48.
La comparación exacta de `/proc/*/exe` volvió a devolver
`input_experiment_running_pids=[]`. No se ejecutó de nuevo el prototipo.

La consulta inicial con `journalctl -b` se realizó ya en el arranque nuevo;
su ausencia de mensajes no informa del incidente anterior. Se corrigió la
selección usando el ID del arranque afectado. Intervalo 13:03:30–13:11:00,
salida ISO con zona horaria, sin nuevos diálogos ni operaciones de entrada:

```bash
journalctl -b d67316910f8348f19064305dcc789bb3 \
  --since '2026-11-05 13:03:30' --until '2026-11-05 13:11:00' \
  --no-pager -o short-iso _COMM=gnome-shell
journalctl -b d67316910f8348f19064305dcc789bb3 \
  --since '2026-11-05 13:03:30' --until '2026-11-05 13:11:00' \
  --no-pager -o short-iso -u systemd-logind.service
journalctl -b d67316910f8348f19064305dcc789bb3 \
  --since '2026-11-05 13:03:30' --until '2026-11-05 13:11:00' \
  -k -p warning --no-pager -o short-iso
```

Logs locales ignorados: `freeze-previous-session.log`,
`freeze-previous-logind.log`, `freeze-previous-kernel.log`.
El del núcleo conserva la advertencia relevante y un recuento de las 85
líneas repetidas de `kauditd_printk_skb: callbacks suppressed`, omitidas por
no aportar detalle del fallo. Se descartaron mensajes de otras aplicaciones.

Observaciones, sin atribuir causalidad:

- El núcleo siguió registrando mensajes después de la pulsación. A las
  13:08:04 aparece una advertencia `i2c_hid_get_input: IRQ triggered but there's no data`.
  No hay pánico, oops o bloqueo GPU explícito en las advertencias consultadas.
- Logind registra cierre de tapa y suspensión a las 13:08:10, apertura a las
  13:08:18 y fin de suspensión a las 13:08:19. GNOME Shell registra a las
  13:08:10 `drmModeAtomicCommit: Permission denied` al desactivar `/dev/dri/card1`.
  Estos mensajes son posteriores a la espera del prototipo; no demuestran
  la causa de la congelación comunicada.
- A las 13:10:18 se registra el botón de encendido; a las 13:10:41 se cierra
  GNOME Shell y la sesión. Son pruebas de actividad de servicios durante el
  incidente y del apagado posterior, no de que la interfaz fuera utilizable.

**Resultado: recuperación solo mediante reinicio según el usuario; causa no
determinada.** Se mantiene EXP-INPUT-02 abierto. No se ha aplicado un parche
especulativo, cambiado el atajo, reiniciado servicios gráficos ni realizado
nuevas pruebas de escritorio. La evidencia histórica Windows se conserva.

### Punto de reanudación

La cancelación ya está verificada y el usuario recuperó el equipo mediante
reinicio. Se conservaron los registros pertinentes del arranque afectado; no
permiten determinar la causa. Continuar solo con evidencia de solo lectura
y pruebas aisladas hasta justificar una corrección concreta.
No repetir Ctrl+Alt+F12, `--check-target` ni demostraciones de inyección hasta
resolver EXP-INPUT-02 y disponer de recuperación independiente comprobada.
Si se da por terminada la sesión de prueba, el usuario puede restaurar la
disposición de dos monitores; el agente no la modifica.
No conectar Windows hasta acordar revisión, roles, puertos, huellas y caso.
La reconstrucción Windows y la integración del producto siguen pendientes.

## Compilación inicial Ubuntu nativa — 5 de noviembre de 2026

**Resultado actual: aplicación nativa Wayland + TLS compilada; self-test
independiente con 538 comprobaciones, 0 fallos, salida 0.** La evidencia Windows
que sigue se conserva. Esta ejecución actualiza la ausencia histórica de
compilación nativa Wayland; no certifica captura, inyección ni recuperación.

### Entorno y revisión

- Base Git: `450cbb2e340a381bda01274eb86df0e05934e822`, rama
  `validation/ubuntu`, directorio `/home/pholtergate/DEV/OneForTwo`.
  Fuentes de entrada sin modificar. El único parche de código local del
  conjunto afecta a la cabecera TLS del experimento de portapapeles.
- Fecha: 2026-11-05 (`Europe/Madrid`). Ubuntu 26.04.1 LTS,
  Linux 7.0.0-42-generic x86_64; entorno heredado Wayland, `ubuntu:GNOME`,
  GNOME Shell `50.1-0ubuntu1.5`, Mutter `50.1-0ubuntu2.4`.
- GCC/G++ `15.2.0-16ubuntu1`, CMake local 4.2.3, Make 4.4.1,
  pkg-config 2.5.1; GIO/GLib 2.88.0 (`2.88.0-1ubuntu0.1`),
  libei 1.5.0 (`1.5.0-3`), OpenSSL 3.5.5 (`3.5.5-1ubuntu3.7`).
- Paquetes de portales: xdg-desktop-portal `1.21.1+ds-1ubuntu3.1`,
  GNOME `50.0-0ubuntu1`, GTK `1.15.3-2ubuntu1`.
  No se consultó el bus: InputCapture v2, RemoteDesktop v2,
  GlobalShortcuts v1 y dispositivos EIS siguen sin comprobar en ejecución.

Inventario: `git status --short --branch`, `git rev-parse HEAD`,
`cat /etc/os-release`, `uname -srmo`, variables de sesión, `c++ --version`,
`dpkg-query -W`, `pkg-config --modversion gio-2.0 libei-1.0 openssl`.
Las dependencias se trasladaron desde `/tmp` a
`.local-state/ubuntu-validation/`, dentro del proyecto y excluidas de Git,
sin instalación global ni `sudo`;
[preparación y variables](../clipboard/RESULTS.md#dependencias-locales-de-esta-validación).

### Compilación y prueba aislada

Desde la raíz del repositorio:

```bash
source .local-state/ubuntu-validation/env.sh
cmake --fresh -S experiments/input -B experiments/input/build/ubuntu-wayland \
  -DCMAKE_BUILD_TYPE=Release -DONEINTWO_WAYLAND=ON \
  -DONEINTWO_X11=OFF -DONEINTWO_WITH_TLS=ON \
  -DOPENSSL_ROOT_DIR="$OIT_VALIDATION_DEPS/usr"
cmake --build experiments/input/build/ubuntu-wayland -j4
cd experiments/input
./build/ubuntu-wayland/input_self_test
```

Los tres comandos finales devolvieron 0. Se construyeron `input_experiment`
e `input_self_test`, incluida la compilación de `wayland_backend.cpp`,
`linux_factory.cpp` y `tls_link.cpp`; definiciones
`OIT_WITH_WAYLAND=1` y `ONEINTWO_WITH_TLS=1`, sin `OIT_WITH_X11`.
El ejecutable nativo enlaza GIO/GObject/GLib, `libei.so.1`, `libssl.so.3` y
`libcrypto.so.3`; `ldd` no informa bibliotecas ausentes. Sin errores ni avisos
observados. No se usó `ONEINTWO_BACKEND=none`.
Tras el traslado se regeneró la caché con `--fresh`, se reconstruyeron ambos
ejecutables con las rutas persistentes y se repitió el self-test: **538/0**.

```text
OneInTwo input experiment: self-tests aislados
Comprobaciones: 538, fallos: 0
```

La prueba independiente ejercita núcleo y flujos Wayland con dobles; no
instancia el backend GDBus/libei. Compilar ese backend y ejecutar los dobles
son evidencias diferentes. Logs locales ignorados en `build/ubuntu-wayland/`:
`configure.log`, `build.log`, `self-test.log`, `libraries.log`.
La ejecución posterior al traslado queda en `configure-persistent.log`,
`build-persistent.log`, `self-test-persistent.log` y `libraries-persistent.log`.

### Escritorio, coordinación y bloqueos restantes

El usuario pidió completar por ahora solo compilación y pruebas aisladas.
No se ejecutaron `--diagnose`, `--inject-demo`, `--source` ni `--target`.
Permisos, atajo local, geometría/dispositivos EIS, demostración en editor,
cruce/retorno, distribución española y recuperación siguen pendientes.
No se inició ninguna escucha ni conexión de prueba.

Próximo paso local, con usuario presente: diagnóstico D2; editor desechable,
salida local Ctrl+Alt+F12 y medio independiente de recuperación preparados;
autorizar y comprobar el atajo real antes de prolongar cualquier control.
No habilitar captura sin salida local fiable. La demostración debe ser acotada
y observada; la salida del programa no certifica texto o movimiento.

Antes de conectar Windows, acordar revisión y reconstrucción de ambos extremos,
roles, IP y puerto 47830/TCP, certificados públicos/huellas, editor, duración,
orden de arranque y recuperación. Empezar por identidades y sesiones cortas
según el manual. No se han generado claves ni contactado con el otro equipo.
La reconstrucción Windows actual y X11 opcional siguen sin evidencia nueva;
X11 no sustituye la validación Wayland. No queda bloqueo de compilación
Wayland/TLS en esta máquina. La integración del producto permanece aplazada.

## Evidencia histórica conservada — entrega y revisión anteriores

Revisión documental: **5 de noviembre de 2026**. Evidencia inicial del agente: 4 de noviembre de 2026. Este informe distingue presencia de implementación, compilación registrada, pruebas aisladas y validación manual.

## 1. Estado actual

El prototipo contiene Windows, Linux/X11 y Linux/Wayland. Wayland ya no es un backend ausente: existen su adaptador nativo, los flujos portables de autorización y la selección de backend. Las versiones anteriores de este informe describían únicamente la primera entrega X11 y no reflejaban esos cambios.

| Componente | Implementación | Evidencia disponible |
| --- | --- | --- |
| Núcleo, protocolo, geometría y controladores | Implementado | Self-test Windows existente: **538 comprobaciones, 0 fallos**, ejecutado el 5 de noviembre |
| Flujos Wayland sin escritorio | Implementados en `wayland_flow.*` | Incluidos en el self-test con dobles de PortalOps |
| Adaptador Windows | Implementado | Compilación inicial del agente con MSVC/OpenSSL; hay binario posterior en `build/cmake/` |
| Adaptador X11 | Implementado | Sin compilación nativa Linux registrada en este informe |
| Adaptador Wayland GDBus/libei | Implementado en `wayland_backend.cpp` | Sin evidencia registrada de compilación del adaptador nativo Linux |
| Selección Linux y opciones CMake | Implementadas | Revisión del código: Wayland y X11 son opcionales y seleccionados según sesión |
| TLS mutuo y huellas | Implementado | Compilación Windows documentada por el agente; handshake pendiente |
| Escritorio real y dos equipos | Pruebas preparadas | **Pendiente manual**, tanto en Windows como en Ubuntu |

La revisión del 5 de noviembre no recompiló la aplicación ni ejecutó captura, inyección, portales, diagnóstico gráfico o red. Ejecutó únicamente el binario independiente de self-tests existente. La presencia de un binario o de código nativo no certifica que el backend Linux compile.

## 2. Implementación actual

- `src/core/`: eventos, usos HID, conversiones, protocolo, geometría, estado de teclas/botones y controladores.
- `src/platform/windows_backend.cpp`: hooks, SendInput y salida de emergencia.
- `src/platform/linux_backend.cpp`: implementación X11, sin utilizarse como fallback de una sesión Wayland.
- `src/platform/linux_factory.cpp`: selección por sesión y backend incluido en la compilación.
- `src/platform/wayland_flow.*`: secuencias de autorización, barreras, activación, liberación, cierre y recuperación mediante el contrato PortalOps.
- `src/platform/wayland_backend.cpp`: GDBus, InputCapture, RemoteDesktop, GlobalShortcuts y comunicación EIS con libei.
- `src/net/tls_link.cpp`: transporte TLS y comprobación de identidad por huella en ambos extremos.
- `src/main.cpp`: modos explícitos; preparación de permisos antes de iniciar comunicación.
- `tests/self_test.cpp`: pruebas del núcleo y de los flujos Wayland con dobles, sin conexiones reales.

El código conserva el alcance de un monitor por equipo, disposición horizontal y un origen físico seleccionado. No sincroniza modificadores o botones que ya estuvieran pulsados al cruzar; deben soltarse antes del cruce.

## 3. Evidencia y límites de compilación

### Evidencia inicial del agente — 4 de noviembre

| Entorno | Evidencia declarada en la primera entrega |
| --- | --- |
| Windows 10.0.19045 x64 | MSVC 19.51.36257.0, herramientas 14.51.36231, Windows SDK 10.0.26100.0 |
| CMake Windows | 3.24.3, NMake, OpenSSL 3.0.5 |
| WSL Ubuntu 20.04 | GCC 9.4.0, CMake 3.25.2; únicamente núcleo sin backend ni TLS |
| Primera batería aislada | 403 comprobaciones, 0 fallos en MSVC y GCC 9, anteriores a la ampliación Wayland |

Estos resultados iniciales son históricos; no verifican el adaptador Wayland añadido después.

### Revisión del 5 de noviembre

Se localizaron `build/cmake/input_experiment.exe`, `build/cmake/input_self_test.exe` y `build/help.txt`, con fecha del 4 de noviembre posterior a las fuentes Wayland. La ayuda guardada ya describe Wayland. No se infiere de ello una compilación nativa de Linux.

El archivo `build/cmake-configure.log` conserva un intento fallido por ausencia de NMake en PATH; no es evidencia de una compilación final correcta. Los comandos para reconstruir se mantienen en [MANUAL.md](MANUAL.md).

No hay un resultado registrado para el binario portable WSL posterior a la ampliación Wayland. Tampoco hay evidencia de compilación del backend GDBus/libei, X11 o TLS nativo Linux. Esas comprobaciones siguen pendientes y no se sustituyen por una compilación con `ONEINTWO_BACKEND=none`.

## 4. Comprobación aislada ejecutada el 5 de noviembre

Desde el directorio del experimento:

```powershell
.\build\cmake\input_self_test.exe
```

Salida observada, código de salida 0:

```text
OneInTwo input experiment: self-tests aislados
Comprobaciones: 538, fallos: 0
```

El ejecutable de pruebas llama únicamente a `runSelfTests()`. No crea backends nativos, no abre el bus de sesión y no establece conexiones.

Cobertura de la batería actual:

- Protocolo y rechazo de mensajes inválidos.
- Conversión de teclas HID/Windows/evdev/X11 y geometría.
- Estados de origen/destino, secuencias y generaciones.
- Liberación de teclas/botones, emergencia y temporizadores.
- Geometría y barreras Wayland con datos sintéticos.
- Secuencias InputCapture y RemoteDesktop con dobles de PortalOps.
- Denegación de permisos, cierre, pérdida de captura y eventos de borde del portal.

La batería prueba la lógica de los flujos; no prueba el código GDBus/libei, el compositor, la reserva real de atajos ni la respuesta del cursor. No se han repetido las pruebas de los otros experimentos durante esta actualización.

## 5. Requisitos que impone el backend Wayland actual

Estos son requisitos de esta implementación, no una garantía general de compatibilidad de una versión de Ubuntu.

| Papel | Comprobación o recurso exigido por el código |
| --- | --- |
| Compilación | `gio-2.0`, `libei-1.0`, pkg-config; OpenSSL para comunicación entre equipos |
| Origen | InputCapture versión 2 o superior y capacidades de teclado/puntero |
| Origen autorizado | Una zona de pantalla, conexión EIS, barrera en el borde y activación concedida por el compositor |
| Destino | RemoteDesktop versión 2 o superior y GlobalShortcuts versión 1 o superior |
| Destino autorizado | Teclado y puntero concedidos, atajo de emergencia aceptado y dispositivos EIS utilizables |
| Geometría del destino | Puntero absoluto, dispositivo de puntero para botones/rueda y una región válida de escritorio |

El origen usa CreateSession2/Start/ConnectToEIS/GetZones/SetPointerBarriers/Enable. El destino prepara RemoteDesktop y reserva el atajo mediante GlobalShortcuts antes de escuchar en red. Si no se concede la salida local, el flujo se rechaza.

La geometría puede conocerse solo después de autorizar las sesiones. El diagnóstico previo no demuestra que se concederán permisos ni que EIS ofrecerá los dispositivos necesarios. No se ha fijado una versión mínima de Ubuntu verificada.

## 6. Capacidades y restricciones por validar

- La captura se activa por el compositor al alcanzar una barrera. El control usa esa notificación, no una observación global simulada del cursor.
- El retorno en origen solicita Release con una posición sugerida. No se garantiza que el compositor coloque el puntero exactamente en esa posición.
- El destino Wayland no puede restaurar una posición global que no conoce; este backend no ofrece esa restauración.
- El destino requiere un atajo local concedido. Debe comprobarse la combinación que realmente reserve el escritorio y que su activación llegue al proceso.
- El destino Linux no detecta actividad física local como el backend Windows; debe usarse la salida de emergencia.
- Un cambio de zonas, cierre del portal o pérdida de EIS debe causar la transición prevista; falta validarlo en el escritorio.
- El watchdog del destino cubre pérdida del origen, no la muerte del propio proceso receptor.
- No hay reconexión automática ni soporte garantizado para varios monitores, IME o teclas fuera de la tabla.
- Las ventanas elevadas y el escritorio seguro Windows no forman parte de la garantía de este experimento.

## 7. Matriz pendiente y siguiente paso

| Área | Estado | Prueba requerida |
| --- | --- | --- |
| Compilación Windows del estado actual | Sin nueva compilación en esta revisión | Reconstrucción mediante CMake |
| Compilación Linux/Wayland nativa | Sin evidencia registrada | Forzar `ONEINTWO_WAYLAND=ON`, TLS activado y X11 desactivado |
| Compilación Linux/X11 | Sin evidencia registrada | Forzar X11 si se quiere evaluar esa alternativa |
| Permisos Wayland | Pendiente manual | Aceptar, denegar, cancelar y retirar sesiones |
| Entrada local | Pendiente manual | Demostración acotada en un editor desechable |
| Cruce y retorno | Pendiente manual | Ambos sentidos Windows/Ubuntu |
| Recuperación | Pendiente manual | Atajo en ambos extremos, corte de red, retirada de permiso y pérdida del origen |
| TLS | Pendiente manual | Identidad correcta e incorrecta en ambos extremos |
| Portabilidad del teclado | Pendiente manual | Distribución española, modificadores y botones |

Seguir [MANUAL.md](MANUAL.md), registrando versión de sistema, sesión, portales, bibliotecas y resultado observado. La discrepancia documental está resuelta; la validación de escritorio y la compilación nativa Linux continúan abiertas en [TODO.md](../../TODO.md).

## 8. Conclusión

Hay implementación de Windows, X11 y Wayland, y evidencia aislada actualizada para el núcleo y flujos con dobles. Todavía no hay evidencia suficiente para afirmar que el adaptador Wayland compile o que los dos equipos crucen, inyecten y recuperen el control en un escritorio real. Esta entrega prepara la evaluación manual; no certifica soporte de una plataforma.

