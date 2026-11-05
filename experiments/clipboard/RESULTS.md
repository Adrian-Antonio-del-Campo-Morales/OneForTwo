# Resultados — Portapapeles

## Cierre del caso local de copia aislada — 5 de noviembre de 2026

Con el mismo binario y base Git `450cbb2e340a381bda01274eb86df0e05934e822`
más las correcciones locales previas, se ejecutó `--watch` sobre el portal
real. El usuario autorizó y confirmó una única copia. Una ventana auxiliar
con muestra desechable registró **una sola acción `copy-clipboard`** a las
**19:20:24.324 CET**; no cambió el observador ni filtró sus resultados.
La muestra conocida tiene 62 bytes, huella `af28ca2273e2d495`.

`watch.log` contiene una notificación anterior de otro contenido (183 bytes)
y **una sola notificación de la muestra** (62 bytes), sin nuevas entradas
durante los **12 s siguientes**. `devices=0`, `clipboard_enabled=true`.
El usuario confirmó «una sola copia y observación terminada». Caso local
superado; no demuestra ausencia de ecos entre equipos.

Evidencia ignorada en `manual-output/local-final-20261105/`: `watch_once.py`,
`command.json`, `watch.log`, `copy-actions.json`, `process.json`. La ayuda
ejecutó `timeout --signal=TERM --kill-after=1s 90s
experiments/clipboard/build/ubuntu/oneintwo-clipboard --watch`, con las
bibliotecas locales solo en el hijo. Se detuvo el grupo propio tras la
observación a las **19:20:36.329 CET**; el supervisor devolvió 124 al recibir
la señal externa, no por un error del portal. No se añadieron tests ni se
repitieron lectura, pegado, denegación o self-tests ya válidos.

## Validación local de escritorio — 5 de noviembre de 2026

El usuario reanudó las pruebas y confirmó su presencia. Se trabajó únicamente
con textos sintéticos en GNOME Text Editor, sin iniciar captura/inyección de
entrada ni red de prueba. El agente ejecutó los comandos y leyó los logs;
las acciones y el pegado del editor fueron **observaciones del usuario**,
porque esta sesión no ofrece control visual de las ventanas nativas.

Revisión base: `450cbb2e340a381bda01274eb86df0e05934e822`, rama
`validation/ubuntu`, más los cambios locales descritos aquí y la cabecera TLS
ya documentada. Sin commit ni publicación. Entorno y dependencias: los de la
compilación inicial que sigue, ahora en `.local-state/ubuntu-validation/`.

### Resultados observados

| Caso | Evidencia | Resultado |
| --- | --- | --- |
| Diálogo solo portapapeles | Usuario: Share inicialmente deshabilitado; se habilita activando Allow Remote Interaction y Allow Remote Clipboard, sin seleccionar pantalla | Observado en GNOME 50 |
| Primera solicitud de lectura | El diálogo agotó el límite externo de 90 s; salida 137 tras la terminación forzada prevista | No evaluó la lectura; se repitió |
| Lectura antes de corregir | Tras autorizar, `cannot read portal selection pipe: Resource temporarily unavailable`, salida 2 | Fallo real, corregido y repetido |
| Lectura corregida | `read-03.log`: 61 bytes, 3 líneas, acentos y emoji; comparación con la muestra del editor correcta, salida 0 | Superado |
| Primera publicación | `written` y `readback=match`, salida 0; el usuario informa que no pega el texto esperado | Fallo real; readback interno no prueba pegado |
| Publicación corregida | `publish-02.log`: 57 bytes, 3 líneas; el usuario confirma que pega las tres líneas correctas en documento nuevo; cierre a los 60 s, salida 0 | Superado con observación del usuario |
| Permisos concedidos | Publicación y observación: `portal_start clipboard_enabled=true devices=0` | Verificado por respuesta real del portal |
| Observación en segundo plano | Usuario copia con editor enfocado; `watch-03.log` muestra cambios de 191 bytes/8 líneas y 207 bytes/9 líneas, sin mostrar contenido | Detección verificada; el usuario aclara que pulsó Ctrl+C varias veces |
| Una notificación por copia / ausencia de ecos | Hay notificaciones con huellas repetidas y varias copias deliberadas; no se midió una única pulsación aislada ni una sesión sincronizada | No afirmar correspondencia exacta ni ausencia de eco remoto |
| Permiso denegado | Usuario pulsa Cancelar; `clipboard unavailable: the portal start request was cancelled or failed (response 1)`, salida 2 | Superado; no se lee ni se usa otro backend |

La muestra de lectura guardada tiene un LF final adicional al texto seleccionado
en el editor. La comparación aceptó exactamente las tres líneas esperadas sin
ese LF (61 bytes), sin alterar acentos, emoji o saltos interiores.

### Correcciones necesarias y comprobación

1. `SelectionRead` devolvió un descriptor no bloqueante. Se extrajo la lectura
   del pipe a una función que usa `poll`, tolera EINTR/EAGAIN, conserva el
   límite de 256 KiB y espera datos/EOF con plazo total de 30 s.
   La nueva regresión aislada usa pipes sintéticos para datos Unicode diferidos,
   EOF, pipe vacío y timeout, sin bus ni portapapeles. La batería pasa de
   **151/0 a 153/0**; `self-test-final.log` registra el resultado del estado actual.
2. El modo de publicación terminaba tras verificar su copia interna y cerraba
   la sesión antes de que el editor pudiera solicitar los datos. Ahora la CLI
   atiende eventos del portal durante hasta 60 s tras autorizar, o hasta que
   otra selección la sustituye. El comportamiento de publicación Windows se
   conserva. El manual y la ayuda explican el intervalo de pegado.
3. Los metadatos de `--watch` sin `--show-content` quedaban en el búfer al
   redirigir la salida. Se vacían inmediatamente tras imprimir cada cambio.
   Se añadieron diagnósticos de permisos y notificaciones, sin contenido.
   `watch-02.log` confirma recepción de notificaciones; `watch-03.log` ya
   permite observar los cambios en tiempo real. No se cambió el filtrado de
   duplicados para convertir la repetición de Ctrl+C en un éxito artificial.

### Comandos, logs y parada

Desde la raíz, cargar `source .local-state/ubuntu-validation/env.sh`; después,
desde `experiments/clipboard`, se ejecutaron los modos reales:

```bash
./build/ubuntu/oneintwo-clipboard --read-text --show-content
./build/ubuntu/oneintwo-clipboard --publish-file RUTA_MUESTRA/publicacion.txt --show-content
./build/ubuntu/oneintwo-clipboard --watch
./build/ubuntu/oneintwo-clipboard --read-text   # Cancelar el diálogo
./build/ubuntu/oneintwo-clipboard --self-test
```

Las muestras y logs locales ignorados están en
`manual-output/ubuntu-local-20261105-de47_iib/`: `lectura.txt`,
`publicacion.txt`, `read.log`, `read-02.log`, `read-03.log`,
`publish-01.log`, `publish-02.log`, `watch-01.log`, `watch-02.log`,
`watch-03.log`, `denied-01.log`, logs de reconstrucción y
`self-test-final.log`. La reconstrucción se hizo con
`cmake --build experiments/clipboard/build/ubuntu -j4`, salida 0.
Las lecturas/publicaciones se envolvieron en `timeout --signal=TERM
--kill-after=3s` con límites externos de 180/240 s; el primer intento usó
INT con 90 s y KILL tras 5 s. Los observadores tuvieron supervisión de 180 s
y se detuvieron de forma dirigida al terminar el caso (salidas de supervisión
130/124; no indican rechazo del portal). No quedó observador activo al pasar
al siguiente experimento. El editor y las muestras se conservan para el usuario.

Windows y la sincronización bidireccional siguen pendientes de coordinación.
No se modifica la evidencia histórica siguiente ni se integra el producto.

## Compilación inicial Ubuntu nativa — 5 de noviembre de 2026

**Resultado actual: compilación con portal y TLS completada; 151 comprobaciones,
0 fallos, salida 0.** El usuario limitó esta sesión a compilación y pruebas
aisladas. No se abrió ninguna sesión de permisos, portapapeles ni conexión de
prueba. La evidencia Windows y WSL de las secciones posteriores se conserva
como histórica; sus bloqueos de compilación Linux quedan actualizados aquí.

### Entorno y revisión

- Directorio: `/home/pholtergate/DEV/OneForTwo`.
- Base Git: `450cbb2e340a381bda01274eb86df0e05934e822`.
  Rama local `validation/ubuntu`; sin commit ni publicación de estos cambios.
- Código verificado: esa base **más la corrección local** de `<signal.h>` a
  `<csignal>` en `src/net/tls_channel.cpp`. SHA-256 del archivo corregido:
  `3be6de1a8ec6f3fff237ad3392820066754360f150f05bc6185e631a3effd842`.
- Fecha local del inventario: 2026-11-05, zona `Europe/Madrid`.
- Ubuntu 26.04.1 LTS, Linux 7.0.0-42-generic, x86_64; entorno heredado
  `XDG_SESSION_TYPE=wayland`, `XDG_CURRENT_DESKTOP=ubuntu:GNOME`,
  `WAYLAND_DISPLAY=wayland-0`, `DISPLAY=:0`.
- Paquetes instalados: GNOME Shell `50.1-0ubuntu1.5`, Mutter
  `50.1-0ubuntu2.4`, Nautilus `1:50.2.2-0ubuntu0.2`;
  xdg-desktop-portal `1.21.1+ds-1ubuntu3.1`, backend GNOME `50.0-0ubuntu1`,
  backend GTK `1.15.3-2ubuntu1`. Son versiones de paquetes; no se consultó
  el bus ni se verificaron las interfaces o permisos del compositor.
- GCC/G++ `15.2.0-16ubuntu1`, GNU Make 4.4.1, pkg-config 2.5.1;
  CMake local 4.2.3. libdbus 1.16.2 (`1.16.2-2ubuntu4`) y
  OpenSSL 3.5.5 (`3.5.5-1ubuntu3.7`).

Inventario obtenido con `git status --short --branch`, `git rev-parse HEAD`,
`cat /etc/os-release`, `uname -srmo`, `printenv`, `c++ --version`,
`pkg-config --version`, `openssl version`, `dpkg-query -W` y
`pkg-config --modversion dbus-1 openssl`. No se infiere soporte de portales
a partir de la versión de Ubuntu.

### Dependencias locales de esta validación

Faltaban CMake y los paquetes de desarrollo dbus, OpenSSL, GTK3, GLib/GIO y
libei. Se descargaron los paquetes y sus dependencias, **sin instalar**, en
`/tmp/onefortwo-ubuntu-validation-20261105/debs` y se extrajeron mediante
`dpkg-deb -x` en el subdirectorio `root`. El proceso mantuvo UID 1000, sin
`sudo`, cambios globales ni cambios de cortafuegos. Se reutilizan las
bibliotecas de ejecución ya instaladas mediante enlaces dentro de esa raíz
local; las bibliotecas auxiliares ausentes también están extraídas allí.

Después, a petición del usuario, **se trasladó toda esa preparación a
`/home/pholtergate/DEV/OneForTwo/.local-state/ubuntu-validation/`**, incluidos
los 101 paquetes `.deb`, bibliotecas, herramientas, configuración y logs de
descarga (aproximadamente 301 MiB). La carpeta está excluida por la regla
`/.local-state/` existente en `.gitignore`. Los enlaces a bibliotecas del
sistema siguen siendo válidos; ninguno apunta a `/tmp`.

Comando de descarga inicial completado (histórico, salida 0, 101 paquetes, 56,1 MB):

```bash
mkdir -p /tmp/onefortwo-ubuntu-validation-20261105/{debs,root}
apt-get --download-only --yes --no-install-recommends \
  -o Debug::NoLocking=1 -o Acquire::Retries=0 -o Acquire::http::Timeout=10 \
  -o Dir::Cache::archives=/tmp/onefortwo-ubuntu-validation-20261105/debs \
  install cmake libdbus-1-dev libssl-dev libgtk-3-dev libglib2.0-dev libei-dev
for deb in /tmp/onefortwo-ubuntu-validation-20261105/debs/*.deb; do
  dpkg-deb -x "$deb" /tmp/onefortwo-ubuntu-validation-20261105/root
done
```

El primer intento en el sandbox falló con `Temporary failure resolving`
(salida 100); la repetición con acceso de red permitido funcionó. Crear la
rama también requirió acceso de escritura a `.git` fuera del sandbox; no se
elevaron privilegios del usuario.

El archivo local actual `.local-state/ubuntu-validation/env.sh` configura lo
siguiente; debe cargarse con `source` desde Bash antes de compilar o repetir
las pruebas. Calcula la raíz desde su propia ubicación:

```bash
export OIT_VALIDATION_DEPS="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/root" && pwd)"
export PATH="$OIT_VALIDATION_DEPS/usr/bin:$PATH"
export LD_LIBRARY_PATH="$OIT_VALIDATION_DEPS/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LIBRARY_PATH="$OIT_VALIDATION_DEPS/usr/lib/x86_64-linux-gnu${LIBRARY_PATH:+:$LIBRARY_PATH}"
export PKG_CONFIG_SYSROOT_DIR="$OIT_VALIDATION_DEPS"
export PKG_CONFIG_LIBDIR="$OIT_VALIDATION_DEPS/usr/lib/x86_64-linux-gnu/pkgconfig:$OIT_VALIDATION_DEPS/usr/share/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig"
export CMAKE_PREFIX_PATH="$OIT_VALIDATION_DEPS/usr"
export CPATH="$OIT_VALIDATION_DEPS/usr/include/x86_64-linux-gnu${CPATH:+:$CPATH}"
```

La preparación ya no depende de que se conserve `/tmp`. Sigue siendo propia
de esta instalación Ubuntu: usa bibliotecas de ejecución instaladas en el
sistema. No se han añadido dependencias, binarios o scripts de entorno al
control de versiones. Los `.deb` se conservan para poder repetir la extracción.
Los otros experimentos usan esta misma preparación, sin enlazarse entre sí.

### Compilación y self-test observados

Desde la raíz del repositorio:

```bash
source .local-state/ubuntu-validation/env.sh
cmake --fresh -S experiments/clipboard -B experiments/clipboard/build/ubuntu \
  -DCMAKE_BUILD_TYPE=Release -DOIT_ENABLE_PORTAL=ON -DOIT_ENABLE_PEER=ON \
  -DOPENSSL_ROOT_DIR="$OIT_VALIDATION_DEPS/usr"
cmake --build experiments/clipboard/build/ubuntu -j4
cd experiments/clipboard
./build/ubuntu/oneintwo-clipboard --self-test
./build/ubuntu/oneintwo-clipboard --inventory
```

Configuración, compilación, self-test e inventario finales: salida 0.
Tras el traslado se regeneró la caché con `--fresh`, se reconstruyó el
ejecutable con las rutas persistentes y se repitió el self-test: **151/0**.
Se compiló `portal_clipboard.cpp` y el transporte `tls_channel.cpp`/`peer.cpp`;
las definiciones finales incluyen `OIT_HAVE_PORTAL=1` y `OIT_HAVE_OPENSSL=1`.
El ELF enlaza `libdbus-1.so.3`, `libssl.so.3` y `libcrypto.so.3`;
`ldd` no informa bibliotecas ausentes. Inventario inocuo:
`portal_client=compiled`, `tls=openssl`,
`backend=xdg-desktop-portal-clipboard`. Declara capacidades; no negocia permisos.

```text
checks=151 failures=0
SELF-TEST RESULT: PASS
```

Incidencias resueltas antes del resultado final:

1. `openssl/opensslconf.h: No such file or directory`: faltaba exponer las
   cabeceras multiarch extraídas; se corrigió `CPATH`, sin editar el código.
2. `error: 'signal' is not a member of 'std'`: el transporte Linux incluía
   `<signal.h>` y llamaba a `std::signal`. Se cambió únicamente esa cabecera
   a `<csignal>`; la reconstrucción y las 151 comprobaciones pasaron.

Logs locales ignorados en `build/ubuntu/`: `configure.log`, `build.log`,
`build-initial.log`, `build-tls-failure.log`, `self-test.log`, `inventory.log`,
`libraries.log`, `dependencies-download.log`, `dependencies-dns-failure.log`,
`dependencies-packages.tsv` y copia `dependencies-env.sh`.
La copia `dependencies-env.sh` conserva la configuración temporal inicial
como evidencia; para trabajar se carga el archivo actual de `.local-state`.
La ejecución posterior al traslado queda en `configure-persistent.log`,
`build-persistent.log`, `self-test-persistent.log`, `inventory-persistent.log`
y `libraries-persistent.log`, dentro de la misma carpeta de compilación.

### Escritorio, dos equipos y siguiente paso

Lectura, publicación/pegado, observación en segundo plano, permiso denegado y
concesión real de `devices=0`: **pendientes por decisión del usuario**.
No se ha medido la experiencia de GNOME 50 ni se ha validado un handshake.

Antes de reanudar: usuario presente, editor con texto sintético, salida preparada
y un solo experimento activo, siguiendo MANUAL §5. Antes de conectar Windows,
acordar una revisión que incluya la corrección local, reconstrucción de ambos
extremos, direcciones y puerto 47831/TCP, roles, certificados públicos/huellas,
duración y observación. No se ha contactado ni conectado con Windows; no se han
generado claves. La publicación por Git queda por acordar. Integración del
producto aplazada.

## Evidencia histórica conservada — entrega y revisión anteriores

Fecha: 4 de noviembre de 2026.

Revisión documental: 5 de noviembre de 2026. Se conserva la evidencia de la entrega del agente; esta revisión no repite compilaciones ni pruebas de portapapeles o red. El estado consolidado figura en [experiments/README.md](../README.md).

Estado: adaptador Windows y adaptador Linux/Wayland implementados y compilados.
El adaptador Wayland usa `xdg-desktop-portal` RemoteDesktop + Clipboard, pide
cero dispositivos de entrada y no captura teclado, ratón ni pantalla. Las
auto-pruebas aisladas pasan con **151 comprobaciones, 0 fallos** en la
compilación Linux con portal y **149, 0 fallos** en Windows y en la compilación
sin portal. **No se ejecutaron pruebas con el portapapeles real, el diálogo del
portal ni dos equipos.** P2 sigue pendiente de validación manual, pero ya no por
falta de adaptador Wayland.

## 1. Implementación y compilación

| Elemento | Estado | Evidencia o bloqueo |
| --- | --- | --- |
| Modelo de texto, orden lógico y protocolo | Implementado | `src/model.*`, `src/engine.*`, `src/protocol.*` |
| Adaptador Windows nativo | Implementado y compilado | `src/platform/windows_clipboard.cpp`; MSVC/VS 2026; no ejecutado contra el portapapeles real |
| Adaptador Linux/Wayland | Implementado y compilado con libdbus | `src/platform/portal_clipboard.cpp`; RemoteDesktop + Clipboard; `devices=0`; no validado en sesión gráfica |
| Selección de backend Linux | Implementado | `src/platform/linux_clipboard.cpp`; Wayland → portal; X11 → limitación explícita, sin fallback |
| Transporte TLS entre equipos | Implementado y compilado en Windows | OpenSSL 3.5; `src/net/tls_channel.cpp`, `src/net/peer.cpp`; sin prueba de ejecución |
| Transporte TLS en Linux | **No compilado** | WSL no tiene `libssl-dev`; no se elevó el proceso ni se instalaron paquetes globalmente |
| CLI y modos explícitos | Implementado | `--help`, `--inventory`, `--self-test`, `--read-text`, `--publish-*`, `--watch`, `--emit-samples`, `--serve`, `--connect` |
| Auto-pruebas aisladas | Ejecutadas y superadas | 151 checks Linux con portal; 149 checks Windows y Linux sin portal |
| `MANUAL.md` | Entregado | Compilación separada, permisos, comandos y matriz manual |
| Compilación Windows | Verificada | CMake 4.3.1-msvc1, generador VS 18 2026 x64, MSVC 19.51.36257; salida 0 |
| Compilación Linux con portal | Verificada | Ubuntu 20.04.6 WSL, g++ 9.4.0, CMake 3.25.2, libdbus 1.12.16; salida 0 |
| Compilación Linux sin portal | Verificada | `-DOIT_ENABLE_PORTAL=OFF`; 149 checks, salida 0 |
| Pruebas manuales de escritorio / dos equipos | Pendientes | Este agente no debía leer/escribir el portapapeles real ni abrir red |

## 2. Entorno y evidencia de compilación

### Windows

- SO observado por `--inventory`: Windows 10.0.19045, sesión 1.
- Compilador: MSVC 19.51.36257.0 (Visual Studio 2026 Build Tools).
- CMake: 4.3.1-msvc1 incluido en Visual Studio.
- OpenSSL: 3.5 (Win64) en `C:\Program Files\OpenSSL-Win64`.
- Comando:

```powershell
$cmake = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake -S . -B build/win -G "Visual Studio 18 2026" -A x64 -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64"
& $cmake --build build/win --config Release
```

Resultado: compilación completada, código de salida 0; binario
`build\win\Release\oneintwo-clipboard.exe`. Avisos C4996 de `fopen` en
`tls_channel.cpp`; no son errores. La ejecución necesitó
`C:\Program Files\OpenSSL-Win64\bin` en `PATH` para cargar las DLL.

### Linux con portal

- Ubuntu 20.04.6 LTS en WSL2, g++ 9.4.0, CMake 3.25.2, 16 procesadores.
- `libdbus-1-dev` 1.12.16-2ubuntu2.3 descargado como `.deb` y extraído en
  `/tmp/oit-deps/dbus`; no se instaló globalmente. La librería en ejecución fue
  `/lib/x86_64-linux-gnu/libdbus-1.so.3`.
- `pkg-config` no tenía el `.pc` de la extracción local; se pasaron
  explícitamente los dos directorios de cabeceras (incluido el multiarch
  `dbus-arch-deps.h`) y la librería. En una Ubuntu normal, `libdbus-1-dev` y
  `pkg-config` permiten configurar sin rutas manuales.
- Comando real de la compilación verificada:

```bash
cmake -S . -B build/linux-portal2 -DCMAKE_BUILD_TYPE=Release \
  -DDBUS_INCLUDE_DIR=/tmp/oit-deps/dbus/usr/include/dbus-1.0 \
  -DDBUS_ARCH_INCLUDE_DIR=/tmp/oit-deps/dbus/usr/lib/x86_64-linux-gnu/dbus-1.0/include \
  -DDBUS_LIBRARY=/lib/x86_64-linux-gnu/libdbus-1.so.3
cmake --build build/linux-portal2 -j8
```

La configuración mostró:

```text
-- xdg-desktop-portal backend enabled (libdbus: /lib/x86_64-linux-gnu/libdbus-1.so.3)
```

y la compilación terminó con código 0. `--inventory` en WSL muestra
`portal_client=compiled`, pero no hay sesión gráfica y el backend queda
`linux-no-graphical-session`; esto sólo verifica la compilación.

### Linux sin portal

```bash
cmake -S . -B build/linux-noportal -DCMAKE_BUILD_TYPE=Release -DOIT_ENABLE_PORTAL=OFF
cmake --build build/linux-noportal -j8
./build/linux-noportal/oneintwo-clipboard --self-test
```

Resultado: código 0 y `checks=149 failures=0`. Confirma que el núcleo, Windows
no aplica, y el adaptador Linux de limitación siguen compilando cuando libdbus
no está disponible. El backend se informa como no compilado, sin simular lectura
ni escritura.

### Limitación TLS Linux

WSL no tiene `libssl-dev`; `apt-get download libssl-dev` falló con 404 para la
versión candidata. No se instaló nada. En consecuencia, la compilación Linux
verificada usa `peer_unavailable.cpp`, `tls=unavailable`, y no se puede afirmar
que el handshake TLS Linux funcione. Para cerrar esta parte hace falta
`sudo apt install libssl-dev` en Ubuntu y repetir la compilación; es una
limitación del entorno de verificación, no una decisión de diseño.

## 3. Auto-pruebas aisladas

Comando: `oneintwo-clipboard --self-test` (Windows y Linux). Salida Linux con
portal:

```text
PASS text_validation
PASS line_endings
PASS ordering_and_ids
PASS fingerprint
PASS protocol_roundtrip_and_limits
PASS portal_message_helpers
PASS engine_local_capture
PASS engine_remote_and_echo
PASS engine_rejections
PASS engine_concurrency
PASS engine_session_isolation
checks=151 failures=0
SELF-TEST RESULT: PASS
```

Windows:

```text
checks=149 failures=0
SELF-TEST RESULT: PASS
```

Cobertura real de estas pruebas:

- UTF-8 estricto: acentos, emoji, truncamientos, secuencias overlong,
  sustitutos, NUL y texto vacío.
- Límite de 256 KiB, normalización CRLF→LF y conversión de vuelta a Windows.
- Orden lógico Lamport con desempate estable por identificador de origen;
  convergencia ante copias concurrentes.
- Eco propio por marcador y por huella de contenido; una copia humana posterior
  fuera de la ventana se envía.
- Duplicados, operaciones atrasadas y actualizaciones de otra generación.
- Rechazo de texto inválido o demasiado grande sin escribir el portapapeles.
- Trama: longitudes cero/excesivas, truncamiento, bytes sobrantes, tipo
  desconocido y versión de protocolo incorrecta.
- Ausencia de reenvío automático al crear una sesión nueva y rechazo de una
  operación de la generación anterior.
- Motor sin adaptador: no declara éxito; informa `unavailable`.
- Portal, sólo en memoria: construcción/parseo de `a{sv}`, prioridad de MIME,
  `Response`, `SelectionOwnerChanged` y `SelectionTransfer`; no abre el bus.

No se inicializa el portapapeles ni se abre ningún socket durante `--self-test`.
El adaptador usado para el motor es `FakePort`, definido en
`tests/selftest.cpp`.

Comprobaciones adicionales ejecutadas (sin escritorio):

- `--inventory` en Windows: `backend=win32-clipboard`,
  `read=yes write=yes watch=yes independent_of_input=yes`, `tls=openssl`.
- `--inventory` en Linux con portal, WSL sin sesión gráfica:
  `portal_client=compiled`, `backend=linux-no-graphical-session`,
  `read=no write=no watch=no`, `tls=unavailable`.
- `--inventory` en Linux sin portal: `portal_client=unavailable`, backend de
  limitación.
- Sin argumentos: sólo ayuda; no se inicializa el portapapeles.
- `--emit-samples` escribió cuatro archivos UTF-8 válidos de 47, 55, 49 y
  64893 bytes; se validaron con Python y se borraron.
- `--version` y los errores de argumentos devuelven lo esperado.

## 4. Verificado frente a pendiente

| Afirmación | Estado |
| --- | --- |
| El prototipo Windows compila con MSVC y OpenSSL | Verificado |
| El núcleo y el adaptador Linux compilan en Ubuntu 20.04 WSL | Verificado |
| El adaptador Wayland compila con libdbus y el flujo D-Bus está implementado | Verificado por compilación y revisión de código |
| 151 comprobaciones aisladas pasan en Linux con portal | Verificado |
| 149 comprobaciones aisladas pasan en Windows y Linux sin portal | Verificado |
| La política de eco, duplicados, orden y generaciones funciona con dobles | Verificado |
| El protocolo rechaza tramas malformadas dentro de los límites probados | Verificado |
| Windows lee/escribe/observa el portapapeles real | **Pendiente manual** |
| Wayland lee/escribe/observa el portapapeles real | **Pendiente manual**; no hay sesión gráfica aquí |
| El diálogo del portal concede `clipboard_enabled=true` y `devices=0` | **Pendiente manual**; el adaptador rechaza cualquier mask distinto de cero |
| La observación sigue con la ventana del experimento inactiva | **Pendiente manual** |
| El handshake TLS y la fijación de certificado funcionan en ejecución | **Pendiente manual** |
| Windows ↔ Windows transfiere texto en ambos sentidos | **Pendiente manual** |
| Windows ↔ Ubuntu transfiere texto | **Pendiente manual**; además falta compilar TLS Linux en este entorno |
| Ubuntu lee/escribe el portapapeles vía portal | **Implementado; pendiente manual** |
| X11 lee/escribe el portapapeles | **No implementado** en este prototipo |
| Wayland permite una sesión de sólo portapapeles sin entrada | **Restricción de UI conocida**: GNOME 46 exige habilitar “Allow Remote Interaction” o una pantalla; con `types=0` no concede entrada. Falta confirmar en escritorio real |
| La compilación Linux con OpenSSL funciona | **No verificada** (falta `libssl-dev` en el entorno) |

## 5. Permisos, capacidades y restricciones

### Flujo implementado

`org.freedesktop.portal.RemoteDesktop` crea la sesión y presenta el diálogo;
`org.freedesktop.portal.Clipboard` amplía esa sesión. El orden es:

```text
CreateSession
RequestClipboard
SelectDevices(types=0)
Start
SetSelection / SelectionRead / SelectionWrite / SelectionWriteDone
```

`SelectDevices` documenta `types` como bitmask con valor por defecto “todos”;
se envía `0` para no pedir teclado, ratón ni táctil. `Start` devuelve
`clipboard_enabled` y `devices`. El adaptador:

- exige `clipboard_enabled=true`;
- rechaza cualquier `devices` distinto de cero y cierra la sesión;
- nunca llama a `Notify*` ni `ConnectToEIS`;
- no selecciona fuentes de pantalla;
- no usa X11 como fallback.

### GNOME 46

En `xdg-desktop-portal-gnome` 46.0, el diálogo sólo habilita Share si está
activa “Allow Remote Interaction” o hay una fuente de pantalla seleccionada.
`get_selected_device_types()` devuelve el mask de `SelectDevices` cuando el
interruptor está activo; con `types=0` sigue devolviendo cero. Por tanto:

- es una **restricción de interfaz**: hay que activar el interruptor para que
  Share se habilite;
- no es una autorización de entrada en este caso, porque el mask es 0;
- si un portal/versión ignorara el mask y concediera dispositivos, el adaptador
  se negaría a continuar.

No se ha probado el diálogo real. Otras versiones y escritorios pueden variar;
no se generaliza a “Wayland bloquea el portapapeles”.

### X11

No hay backend X11 implementado. En una sesión X11 el adaptador devuelve
`linux-x11-unimplemented` y no intenta el portal Wayland. Es una ausencia de
implementación, no una conclusión sobre las capacidades de X11.

### Limitaciones del entorno

No hay sesión gráfica en WSL; abrir el portal o leer el portapapeles real
requiere una sesión Ubuntu real. La falta de ese entorno no demuestra
inviabilidad; impide ejecutar la validación manual.

## 6. Matriz manual

Todas las filas están sin ejecutar. Completar desde `MANUAL.md`; no convertir
una compilación en una prueba de escritorio.

| Caso | Entorno exacto | Esperado | Observado | Resultado |
| --- | --- | --- | --- | --- |
| Leer texto copiado | Windows 10/11 | Metadatos correctos; contenido sólo con `--show-content` | No ejecutado | Pendiente |
| Publicar archivo y pegar | Windows 10/11 + Notepad | `readback=match`; texto pegado correcto | No ejecutado | Pendiente |
| Observar en segundo plano | Windows 10/11, ventana inactiva | Un cambio por copia, sin eco | No ejecutado | Pendiente |
| Copiar imagen / formato no textual | Windows 10/11 | Sin cambio de texto; no se borra el portapapeles | No ejecutado | Pendiente |
| Leer vía portal | Ubuntu + GNOME Wayland | Diálogo; `clipboard_enabled=true`, `devices=0`; texto correcto | No ejecutado | Pendiente |
| Publicar y pegar vía portal | Ubuntu + GNOME Wayland | Texto pegado; sin captura de entrada | No ejecutado | Pendiente |
| Observar en segundo plano | Ubuntu + GNOME Wayland | Cambio al copiar en otra app; sin eco | No ejecutado | Pendiente |
| Cancelar permiso | Ubuntu + GNOME Wayland | `Denied`/`unavailable`, sin fallback | No ejecutado | Pendiente |
| X11 | Ubuntu + X11 | `linux-x11-unimplemented`, sin fallback | No ejecutado | No implementado |
| TLS con certificado correcto | Windows ↔ Windows | Handshake y huellas coinciden | No ejecutado | Pendiente |
| TLS con certificado incorrecto | Windows ↔ Windows | Handshake rechazado por pin | No ejecutado | Pendiente |
| Transferencia Unicode bidireccional | Windows ↔ Windows | Acentos, emoji y líneas correctos | No ejecutado | Pendiente |
| Copias rápidas | Windows ↔ Windows | Gana la última; sin bucles | No ejecutado | Pendiente |
| Desconexión y reconexión | Windows ↔ Windows | No se reenvía texto antiguo; nueva copia sí | No ejecutado | Pendiente |
| Windows ↔ Ubuntu | Windows ↔ Ubuntu | Portal + TLS; permiso manual | No ejecutado | Pendiente |
| Ubuntu → Windows | Ubuntu → Windows | Portal + TLS; permiso manual | No ejecutado | Pendiente |

## 7. Decisiones y dependencias

- C++17, CMake independiente dentro del experimento; sin dependencia del CMake
  raíz ni del producto.
- Win32 (`CF_UNICODETEXT`, `AddClipboardFormatListener`, ventana oculta de sólo
  mensajes, formato privado de marcador). No se captura ni se inyecta entrada.
- Wayland: `xdg-desktop-portal` RemoteDesktop + Clipboard vía libdbus. Se
  eligió libdbus para no añadir GLib/GIO al prototipo. No se usó X11 como
  fallback.
- `SelectDevices(types=0)` y comprobación de `devices == 0`: el portapapeles no
  depende de control de entrada.
- OpenSSL 3.5 (licencia Apache 2.0) para TLS 1.2 o superior; el certificado del
  par se fija por SHA-256. No se usa CA, nombre de host, cuenta ni descubrimiento.
  No se ha implementado criptografía propia.
- Límite de texto: 256 KiB; UTF-8 estricto; NUL y texto vacío rechazados;
  metadatos con bytes, puntos de código, líneas y FNV-1a de 64 bits.
- Concurrencia: contador lógico Lamport; empate por identificador de origen
  mayor; generación nueva por conexión; sin reenvío del portapapeles anterior.
- Eco: marcador `oit1:<endpoint>:<generation>:<origin>:<logical>` en un formato
  privado de Windows. El portal Wayland no transporta ese formato; se usa
  `session_is_owner` y la ventana de huella de contenido de 1,5 s como respaldo.
- Contrato del adaptador: capacidades y limitación explícitas, lectura/escritura,
  observación asíncrona con espera y parada, y un marcador de escritura propia.
  En Wayland el marcador queda vacío y la limitación se informa en la salida.

## 8. Bloqueos y trabajo pendiente

1. **Validación en escritorio Wayland**: no ejecutada. Requiere sesión gráfica
   real y aprobar el diálogo. No es un bloqueo de implementación; es una prueba
   pendiente. Hay que registrar versiones, permiso, `clipboard_enabled` y
   `devices`.
2. **TLS en Linux**: no compilado por falta de `libssl-dev`; no se instaló nada
   globalmente. Sin esta compilación no se puede probar Windows ↔ Ubuntu.
3. **X11**: no implementado. Sería otro backend, con libX11 y propiedad de
   selección; no se incluyó para no sustituir silenciosamente Wayland.
4. **Pruebas de dos equipos**: no ejecutadas por las reglas de esta fase.
5. **Permisos y persistencia**: el portal puede volver a preguntar; no hay
   confianza persistente que revocar. La experiencia exacta depende del
   escritorio/versión.

## 9. Fuentes primarias

- Clipboard (v1): <https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Clipboard.html>
- RemoteDesktop (v2): <https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.RemoteDesktop.html>
- XML oficial de Clipboard: <https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/main/data/org.freedesktop.portal.Clipboard.xml>
- XML oficial de RemoteDesktop: <https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/main/data/org.freedesktop.portal.RemoteDesktop.xml>
- Diálogo GNOME 46: <https://gitlab.gnome.org/GNOME/xdg-desktop-portal-gnome/-/raw/46.0/src/remotedesktopdialog.c>
- Implementación GNOME del portal Clipboard: <https://gitlab.gnome.org/GNOME/xdg-desktop-portal-gnome/-/raw/46.0/src/clipboard.c>
- Documentación de libdbus sobre descriptores: <https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html>

## 10. Conclusión

Lo que puede afirmarse: el prototipo Windows está implementado y compila; el
núcleo y el adaptador Wayland están implementados y compilan; las 151
comprobaciones aisladas pasan en Linux con portal y las 149 pasan en Windows y
Linux sin portal. El adaptador Wayland solicita una sesión de RemoteDesktop con
`types=0`, exige `clipboard_enabled=true`, rechaza dispositivos de entrada y no
usa X11 como sustituto.

Lo que no puede afirmarse todavía: que el portapapeles real funcione en Windows
o en un escritorio Wayland, que el diálogo del portal conceda la sesión en una
versión concreta, que Windows y Ubuntu intercambien texto, ni que la compilación
Linux con TLS funcione. P2 sigue pendiente de validación manual; la
implementación Wayland ya existe y la restricción de GNOME 46 está documentada
con su fuente.

Siguiente paso recomendado: instalar `libdbus-1-dev` y `libssl-dev` en una
Ubuntu real, compilar, ejecutar `--inventory` y después las pruebas de
`MANUAL.md` en orden: lectura, escritura/pegado, observación, permiso denegado y
finalmente Windows ↔ Ubuntu.
