# Manual de ejecución — experimento de portapapeles

Fecha: 4 de noviembre de 2026.

Estado: prototipo con adaptador Windows y adaptador Linux/Wayland basado en
`xdg-desktop-portal` RemoteDesktop + Clipboard. El adaptador Wayland está
implementado y compilado, pero **no se ha ejecutado en una sesión gráfica real**:
este entorno no tiene escritorio. Las pruebas de lectura, escritura,
observación, permisos y conexión entre equipos siguen siendo manuales.

Este documento pertenece a `experiments/clipboard/`. No depende del CMake raíz,
del código de producto ni de otros experimentos. No se debe ejecutar más de un
experimento a la vez en la misma sesión gráfica durante las pruebas manuales.

## 1. Qué está verificado y qué no

Verificado por el agente, sin tocar el escritorio ni la red:

- Compilación Windows con MSVC/VS 2026, CMake 4.3.1 y OpenSSL 3.5.
- Compilación Linux del núcleo y del adaptador Linux en Ubuntu 20.04 (WSL).
- Compilación Linux del adaptador Wayland con `libdbus-1` (cabeceras extraídas
  localmente de un `.deb`; no se instaló nada globalmente).
- `--self-test`: **151 comprobaciones, 0 fallos** en la compilación Linux con
  portal; **149, 0 fallos** en Windows y en la compilación Linux sin portal.
- `--inventory`, `--help`, `--version` y `--emit-samples` (sólo archivos).

No verificado: lectura/escritura real del portapapeles, pegado en un editor,
observación en segundo plano, diálogo/permisos del portal, sesión Wayland real,
TLS Linux en ejecución, conexión entre dos equipos y permisos del escritorio.
Que no se haya podido probar aquí **no demuestra que Wayland no permita el
acceso**; la implementación existe y la validación pendiente está descrita abajo.

Diferencias que no deben confundirse:

- **Implementado y compilado:** el adaptador Wayland y su flujo D-Bus.
- **Restricción demostrada del escritorio:** en GNOME 46 el botón Share del
  diálogo RemoteDesktop sólo se habilita con “Allow Remote Interaction” o una
  fuente de pantalla; con `SelectDevices(types=0)` ese interruptor no concede
  dispositivos de entrada. Está documentado con la fuente en el apartado 10.
- **No implementado:** el backend X11. No se usa como sustituto silencioso.
- **No verificado:** todo lo que requiere una sesión gráfica, un portal real o
  dos máquinas.

## 2. Requisitos

### Windows

- Windows 10 u 11 x64.
- Visual Studio 2026 Build Tools con la carga de trabajo C++ y Windows SDK, o
  una instalación equivalente que incluya MSVC y el generador de Visual Studio.
- CMake. En esta máquina se usó el CMake incluido en Visual Studio
  (`cmake version 4.3.1-msvc1`). El CMake del sistema (3.24) no genera
  proyectos para VS 18.
- OpenSSL 3.x Win64 con bibliotecas de desarrollo y DLL. En esta máquina:
  `C:\Program Files\OpenSSL-Win64`.
- Las DLL `libssl-3-x64.dll` y `libcrypto-3-x64.dll` deben estar en `PATH` al
  ejecutar el binario. Si OpenSSL no se encuentra al configurar, el prototipo
  se compila sin modo entre equipos y lo indica.

Rutas usadas en esta máquina:

```text
CMake:   C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
OpenSSL: C:\Program Files\OpenSSL-Win64
```

### Ubuntu

Para compilar el núcleo y el adaptador Wayland nativo:

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libdbus-1-dev
```

Para habilitar además el modo entre equipos con TLS:

```bash
sudo apt install libssl-dev
```

`libdbus-1-dev` aporta `dbus/dbus.h` y, en Linux multiarch,
`dbus/dbus-arch-deps.h`; CMake los localiza con `pkg-config`. Si falta, CMake
emite un aviso y compila el adaptador Linux como limitación explícita
(`portal_client=unavailable`). No se instala nada automáticamente ni se usa
`sudo` desde el binario.

El agente no instaló paquetes globalmente. Para la verificación de este
experimento se descargó localmente `libdbus-1-dev` y se extrajo en `/tmp`; ese
camino es desechable. En una instalación normal basta el comando anterior.

WSL sirve para compilar y ejecutar las auto-pruebas, no para probar el
portapapeles: no hay sesión gráfica. Las pruebas de escritorio deben hacerse en
una sesión Ubuntu real.

## 3. Compilación separada

Hay tres comprobaciones distintas. No mezclar “compila” con “funciona en el
escritorio”.

### 3.1 Núcleo, sin adaptador nativo

Sirve para verificar el modelo, el motor, el protocolo y el ejecutable sin
depender de libdbus ni de OpenSSL:

```bash
cd ~/ruta/a/OneInTwo/experiments/clipboard   # o /mnt/d/... si se compila en WSL
cmake -S . -B build/linux-core -DOIT_ENABLE_PORTAL=OFF -DOIT_ENABLE_PEER=OFF
cmake --build build/linux-core -j
./build/linux-core/oneintwo-clipboard --self-test
```

Resultado esperado: `checks=149 failures=0`, `SELF-TEST RESULT: PASS`, código
de salida 0. El adaptador Linux compilado en este modo sólo informa su
limitación; no finge lectura ni escritura.

### 3.2 Adaptador nativo Wayland

Con `libdbus-1-dev` instalado:

```bash
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux -j
./build/linux/oneintwo-clipboard --self-test
./build/linux/oneintwo-clipboard --inventory
```

La configuración debe mostrar:

```text
-- xdg-desktop-portal backend enabled (libdbus: ...)
```

Resultado esperado: `checks=151 failures=0`, `SELF-TEST RESULT: PASS`, y en el
inventario `portal_client=compiled`. Si aparece un aviso de `libdbus-1` y
`portal_client=unavailable`, el adaptador Wayland no se compiló; instalar
`libdbus-1-dev` y volver a configurar.

Si no se puede instalar el paquete globalmente, una alternativa local (la usada
aquí) es descargar y extraer el `.deb`:

```bash
mkdir -p /tmp/oit-deps && cd /tmp/oit-deps
apt-get download libdbus-1-dev
dpkg-deb -x libdbus-1-dev_*.deb dbus
cmake -S /ruta/a/experiments/clipboard -B build/linux-local \
  -DCMAKE_BUILD_TYPE=Release \
  -DDBUS_INCLUDE_DIR=/tmp/oit-deps/dbus/usr/include/dbus-1.0 \
  -DDBUS_ARCH_INCLUDE_DIR=/tmp/oit-deps/dbus/usr/lib/x86_64-linux-gnu/dbus-1.0/include \
  -DDBUS_LIBRARY=/lib/x86_64-linux-gnu/libdbus-1.so.3
```

La ruta multiarch y la versión del `.deb` pueden cambiar. No se copió ninguna
cabecera al repositorio ni se modificó el sistema.

### 3.3 Windows

```powershell
cd D:\DEVEL\OneInTwo\experiments\clipboard
$cmake = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake -S . -B build/win -G "Visual Studio 18 2026" -A x64 `
    -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64"
& $cmake --build build/win --config Release
```

Resultado esperado: `build\win\Release\oneintwo-clipboard.exe`, código de salida
0. Es normal ver avisos C4996 sobre `fopen` en `tls_channel.cpp`; no impiden la
compilación.

Antes de ejecutar en esa consola:

```powershell
$env:PATH = "C:\Program Files\OpenSSL-Win64\bin;$env:PATH"
```

Si la ruta de Visual Studio cambia, se puede localizar con:

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
```

## 4. Comprobaciones aisladas (reproducibles)

Estas órdenes **no** leen ni escriben el portapapeles real, no abren sockets,
no abren el bus de sesión y no capturan entrada. Son las que ya se ejecutaron:

```powershell
.\build\win\Release\oneintwo-clipboard.exe --self-test
.\build\win\Release\oneintwo-clipboard.exe --inventory
.\build\win\Release\oneintwo-clipboard.exe --version
```

```bash
./build/linux/oneintwo-clipboard --self-test
./build/linux/oneintwo-clipboard --inventory
```

Salida esperada clave en Linux con portal:

```text
PASS text_validation
...
PASS portal_message_helpers
...
checks=151 failures=0
SELF-TEST RESULT: PASS
```

`--self-test` usa cadenas sintéticas y un adaptador falso. Comprueba UTF-8,
límites, saltos de línea, orden lógico, eco, duplicados, generaciones,
concurrencia y tramas malformadas. En la compilación con portal añade
`portal_message_helpers`, que construye y parsea mensajes D-Bus en memoria:
diccionario `a{sv}`, `mime_types`, `Response`, `SelectionOwnerChanged` y
`SelectionTransfer`. No abre una conexión ni solicita permisos.

Sin argumentos sólo se muestra la ayuda; no se inicializa ningún servicio.
`--inventory` en Windows muestra `backend=win32-clipboard`, capacidades
`read=yes write=yes watch=yes independent_of_input=yes` y `tls=openssl`.
En Linux Wayland con portal compilado muestra
`backend=xdg-desktop-portal-clipboard`, `portal_client=compiled` y capacidades
declaradas. Esas capacidades describen el adaptador, **no** una concesión ya
negociada: el diálogo y el resultado de `Start` sólo se evalúan al crear el
puerto (`--read-text`, `--publish-*`, `--watch` o una sesión entre equipos).
En Linux X11 muestra `backend=linux-x11-unimplemented`; en una sesión sin
gráficos, `linux-no-graphical-session`.

## 5. Validación manual en Ubuntu Wayland

Requisitos: sesión gráfica Wayland real, `xdg-desktop-portal` y el backend del
escritorio (por ejemplo `xdg-desktop-portal-gnome`), y el binario compilado con
`libdbus-1-dev`. No se debe probar con el experimento en una sesión X11 ni
forzar `WAYLAND_DISPLAY` sobre X11.

Primero registrar el entorno, sin abrir ninguna sesión:

```bash
echo "session=$XDG_SESSION_TYPE desktop=$XDG_CURRENT_DESKTOP wayland=$WAYLAND_DISPLAY"
gnome-shell --version 2>/dev/null || true
dpkg -l | grep -E 'xdg-desktop-portal|gnome-shell' | head -20
./build/linux/oneintwo-clipboard --inventory
```

Anotar la versión de GNOME, la versión de `xdg-desktop-portal-gnome` y el
backend. La mera presencia de la interfaz en D-Bus no concede acceso.

1. **Lectura.** Copiar un texto sintético conocido en un editor Wayland y
   ejecutar:

   ```bash
   ./build/linux/oneintwo-clipboard --read-text --show-content
   ```

   Debe aparecer el diálogo de RemoteDesktop. En GNOME 46:
   - “Allow Remote Clipboard” debe quedar activado (el adaptador llama a
     `RequestClipboard` antes de `Start`);
   - “Allow Remote Interaction” debe activarse para habilitar Share; el
     adaptador ha pedido `SelectDevices(types=0)`, así que ese interruptor **no
     concede teclado, ratón ni táctil**. Si el portal devolviera un mask distinto
     de cero, el adaptador rechaza la sesión y termina con error;
   - no seleccionar una fuente de pantalla para “desbloquear” Share: este
     experimento sólo comparte texto y no necesita captura de pantalla.

   Tras aprobar, comparar el contenido. Si se cancela, debe informarse
   `Denied`/`clipboard unavailable`; no debe abrirse un fallback X11.

2. **Escritura y pegado.** Ejecutar:

   ```bash
   ./build/linux/oneintwo-clipboard --publish-text "Hola ñ 😀" --show-content
   ```

   Aprobar el diálogo y pegar en un editor Wayland con Ctrl+V. Debe aparecer el
   texto. El protocolo del portal no transporta el marcador privado de Windows;
   la salida lo indica y el motor usa la huella de contenido como respaldo de
   eco. Verificar `readback=match`.

3. **Observación en segundo plano.** Ejecutar:

   ```bash
   ./build/linux/oneintwo-clipboard --watch --show-content
   ```

   Con la ventana del experimento inactiva, copiar texto en otra aplicación
   Wayland. Debe aparecer un `change #1`; comprobar que no hay eco del texto
   publicado por el propio experimento. Detener con Ctrl+C. No debe capturarse
   ninguna pulsación: el proceso sólo recibe señales del portal.

4. **Permiso denegado.** Repetir la lectura y pulsar Cancelar en el diálogo.
   Registrar el estado y el código de salida. No debe leerse el portapapeles ni
   iniciarse otro backend.

5. **Portal ausente o no compatible.** En una sesión sin
   `org.freedesktop.portal.Desktop` o sin la interfaz Clipboard, el puerto debe
   fallar con `unavailable`/`not supported`, no con éxito simulado. No se debe
   arreglar instalando un portal distinto durante esta prueba.

6. **X11.** En una sesión X11 real, `--inventory` debe mostrar
   `linux-x11-unimplemented` y los modos de portapapeles deben terminar con
   `clipboard unavailable`. Esto es una ausencia de implementación del
   prototipo, no una prueba de que X11 no permita acceder al portapapeles. No se
   usa Wayland como sustituto silencioso en X11.

7. **Seguridad y datos.** Usar sólo texto sintético. No pegar secretos. El
   diálogo del portal y el contenido del portapapeles pertenecen al escritorio;
   no se registran en los logs salvo `--show-content`, que es explícito.

Condiciones de éxito: el texto se lee y se pega; la observación detecta una
copia; el adaptador no solicita entrada ni captura pantalla; el permiso denegado
se informa; X11 y portal ausente no se presentan como éxito. Cualquier fallo se
registra con la versión exacta del escritorio y el mensaje, no se convierte en
una conclusión de viabilidad.

## 6. Pruebas locales en Windows

Ejecutar en una consola con OpenSSL en `PATH`. Anotar cada observación en la
matriz de `RESULTS.md`. No pegar secretos reales.

1. **Inventario.** Ejecutar `--inventory` y guardar las líneas `platform`,
   `windows_version`, `session_id`, `backend` y `limitation`.
2. **Publicar un archivo.** Crear una carpeta desechable y ejecutar:

   ```powershell
   .\build\win\Release\oneintwo-clipboard.exe --emit-samples "$env:TEMP\oneintwo-clipboard-test"
   .\build\win\Release\oneintwo-clipboard.exe --publish-file "$env:TEMP\oneintwo-clipboard-test\accents.txt" --show-content
   ```

   Esperado: `written bytes=47 ...`, `readback=match`, y el contenido entre
   marcadores. Abrir Notepad y pegar con Ctrl+V.
3. **Texto sintético por argumento.** Ejecutar
   `--publish-text "Hola ñ 😀" --show-content` y pegar en Notepad. En `cmd.exe`
   los emoji pueden depender de la consola; si hay problemas, usar
   `--publish-file`.
4. **Leer.** Copiar un texto conocido desde Notepad, ejecutar `--read-text`
   (sin `--show-content`) y comprobar que sólo muestra metadatos. Repetir con
   `--read-text --show-content` y comparar el contenido.
5. **Observar en segundo plano.** Ejecutar `--watch --show-content`, cambiar a
   Notepad, copiar una línea y volver a la consola. Esperado: un solo
   `change #1` con `marker=absent`; no debe repetirse indefinidamente. Detener
   con Ctrl+C.
6. **Otra aplicación enfocada.** Repetir el paso 5 copiando desde otro programa.
   La ventana del experimento no necesita estar activa.
7. **Sin captura de entrada.** Este prototipo no incluye captura ni inyección de
   teclado/ratón; no hay un modo que la active.
8. **Caso de fallo esperado.** Con otro programa manteniendo el portapapeles
   abierto, `--read-text` puede devolver `busy` tras los reintentos. Registrar
   el mensaje exacto; no presentarlo como éxito.
9. **Formato no textual.** Copiar una imagen y ejecutar `--watch`: no debe
   aparecer un cambio de texto ni borrarse el portapapeles.

## 7. Conexión entre dos equipos

Alcance real de esta entrega:

- **Windows ↔ Windows:** el adaptador Windows está implementado; el canal TLS
  está compilado en Windows, pero la ejecución manual sigue pendiente.
- **Windows ↔ Ubuntu:** el adaptador Wayland puede leer/escribir si el portal
  concede permiso. En Linux falta compilar TLS en este entorno (no hay
  `libssl-dev`), así que la conexión completa **no está verificada**. Instalar
  `libssl-dev`, recompilar y comprobar `tls=openssl` antes de probar.
- **Ubuntu → Windows:** el adaptador Wayland permite leer y publicar, pero no
  se ha ejecutado; no se declara funcionando hasta la prueba manual.
- **Ubuntu → Ubuntu:** igualmente pendiente de prueba manual.

### 7.1 Certificados y huellas

Crear una carpeta fuera del repositorio (o usar `certificates/`, ya ignorada).
Cada equipo genera su propio par; `alice`/`bob` son ejemplos.

```bash
mkdir -p certificates
cd certificates
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout alice.key -out alice.crt -days 7 \
  -subj "/CN=oneintwo-clipboard-alice"
openssl x509 -in alice.crt -noout -fingerprint -sha256
chmod 600 alice.key
```

En Windows, usar el `openssl.exe` de `C:\Program Files\OpenSSL-Win64\bin` y
proteger la clave con los permisos del usuario. **No compartir `.key`.**
Intercambiar únicamente el certificado público `.crt` del otro equipo por un
medio que permita verificar su procedencia (por ejemplo, comparar la huella por
voz). El programa imprime `local_certificate_sha256` y
`peer_certificate_sha256`; deben coincidir con las huellas de OpenSSL.

### 7.2 Iniciar el servidor y el cliente

En el equipo A (servidor), con puerto explícito y firewall limitado a la red
privada:

```powershell
.\build\win\Release\oneintwo-clipboard.exe --serve --bind 0.0.0.0 --port 47831 `
  --cert certificates\alice.crt --key certificates\alice.key `
  --peer-cert certificates\bob.crt --show-content
```

En el equipo B (cliente Ubuntu):

```bash
./build/linux/oneintwo-clipboard --connect --host 192.168.1.50 --port 47831 \
  --cert certificates/bob.crt --key certificates/bob.key \
  --peer-cert certificates/alice.crt --show-content
```

Sustituir la IP por la del servidor. No hay descubrimiento ni puerto por
defecto. Si Ubuntu actúa como servidor, invertir los roles y comprobar que
`--inventory` muestra `tls=openssl`; en este entorno esa compilación no se
verificó.

Al arrancar en Ubuntu, el adaptador del portal puede abrir el diálogo antes de
que el canal TLS quede listo. Aprobar la sesión de RemoteDesktop con
`devices=0`; no seleccionar pantalla ni habilitar entrada. Si el portal deniega
el acceso, el canal puede seguir estableciéndose, pero el extremo Ubuntu
rechazará texto con `unavailable`.

### 7.3 Transferencia en ambos sentidos

1. Con la sesión activa, copiar texto en A y pegarlo en un editor de B. En A
   debe aparecer `local copy captured ... sent`; en B `received update` y
   `remote update applied`.
2. Repetir en sentido B → A. En B el portal debe haber concedido lectura; en A
   el portapapeles Windows se escribe de forma nativa.
3. Para inyectar muestras sin depender del editor, ejecutar en una segunda
   terminal del equipo que envía:

   ```powershell
   .\build\win\Release\oneintwo-clipboard.exe --publish-file "$env:TEMP\oneintwo-clipboard-test\emoji.txt"
   ```

   El proceso de publicación no escribe el marcador interno, por lo que el
   observador de la sesión lo trata como copia del usuario y lo reenvía. Pegar en
   el otro equipo y comprobar acentos, emoji y saltos de línea.
4. Copiar rápidamente tres textos distintos. Esperado: gana la última operación
   según el contador lógico; no hay bucles; los ecos aparecen como `echo
   suppressed`.
5. Detener un extremo con Ctrl+C. El otro debe terminar la sesión e informar
   del cierre.
6. Reconectar sin copiar nada. Esperado: **no** se reenvía el texto antiguo.
7. Probar un certificado incorrecto en `--peer-cert`: el handshake debe fallar
   por la fijación de huella.

La transferencia entre equipos no modifica el veredicto de P2 por sí sola: hay
que registrar si Ubuntu leyó, publicó y observó el portapapeles real, y si el
permiso del portal fue concedido o denegado.

## 8. Detener y restaurar

- `--watch`, `--serve` y `--connect`: Ctrl+C.
- `--read-text`, `--publish-text`, `--publish-file`, `--inventory`,
  `--self-test` y `--emit-samples`: terminan solos.
- Al terminar un modo Wayland se cierra la sesión del portal. El prototipo no
  crea servicios, claves de registro, reglas de firewall ni tareas de inicio.
- Si se creó una regla de firewall para la prueba, retirarla manualmente.
- Borrar `certificates/` y la carpeta de muestras. Los directorios `build/` son
  artefactos locales ignorados.
- En Windows, el texto publicado permanece en el portapapeles tras salir;
  sustituirlo o borrarlo con el uso normal del escritorio.
- En Wayland, el permiso del portal puede volver a solicitarse en cada proceso;
  no hay confianza persistente que revocar en este prototipo.

## 9. Matriz para el usuario

Rellenar sólo con observaciones reales. No convertir una fila pendiente en
éxito por haber compilado. `No ejecutado` es el valor inicial correcto.

### 9.1 Pruebas locales por sistema

| Sistema y sesión | Leer | Escribir | Observar en segundo plano | Sin captura de entrada | Permisos observados | Resultado |
| --- | --- | --- | --- | --- | --- | --- |
| Windows 10/11 (indicar versión) | Pendiente | Pendiente | Pendiente | Pendiente | Ninguno del portal; posible `busy` | No ejecutado |
| Ubuntu + GNOME Wayland | Implementado; pendiente manual | Implementado; pendiente manual | Implementado; pendiente manual | Sí por diseño; verificar `devices=0` | RemoteDesktop + Clipboard; Share y “Allow Remote Interaction” | No ejecutado |
| Ubuntu + X11 | No implementado | No implementado | No implementado | No aplica | No solicitados | No ejecutado |

### 9.2 Pruebas entre equipos

| Origen → destino | Texto | Estado esperado | Observado |
| --- | --- | --- | --- |
| Windows → Windows | Bidireccional | Debe funcionar si TLS y pegado son correctos | Pendiente |
| Windows → Ubuntu | Bidireccional | Portal + TLS Linux; permiso manual | Pendiente |
| Ubuntu → Windows | Bidireccional | Portal + TLS Linux; permiso manual | Pendiente |
| Ubuntu → Ubuntu | Bidireccional | Portal en ambos extremos; permiso manual | Pendiente |

## 10. Restricciones y fuentes primarias

### Portal oficial

La interfaz Clipboard no crea una sesión propia: amplía una sesión de
RemoteDesktop o InputCapture. El orden implementado es:

```text
RemoteDesktop.CreateSession
Clipboard.RequestClipboard
RemoteDesktop.SelectDevices(types=0)
RemoteDesktop.Start
Clipboard.SetSelection / SelectionRead / SelectionWrite / SelectionWriteDone
```

Fuentes:

- Clipboard (v1): <https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Clipboard.html>
- RemoteDesktop (v2): <https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.RemoteDesktop.html>
- XML de Clipboard: <https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/main/data/org.freedesktop.portal.Clipboard.xml>
- XML de RemoteDesktop: <https://raw.githubusercontent.com/flatpak/xdg-desktop-portal/main/data/org.freedesktop.portal.RemoteDesktop.xml>

`SelectDevices` documenta `types` como un bitmask y su valor por defecto es
“todos”; este adaptador envía explícitamente `0`. `Start` devuelve `devices` y
`clipboard_enabled`; el adaptador exige `clipboard_enabled=true` y rechaza
cualquier `devices` distinto de cero.

### GNOME 46: el interruptor que habilita Share

En `xdg-desktop-portal-gnome` 46.0, `remotedesktopdialog.c`:

- `update_button_sensitivity()` sólo habilita Share si hay una fuente de
  pantalla seleccionada o si está activo “Allow Remote Interaction”;
- `get_selected_device_types()` devuelve `dialog->device_types` cuando ese
  interruptor está activo, y `dialog->device_types` es el mask recibido por
  `SelectDevices`. Con `types=0`, el resultado sigue siendo 0;
- `button_clicked()` lee “Allow Remote Clipboard” y lo envía como
  `clipboard_enabled`; `clipboard_requested` hace visible y activa ese
  interruptor.

Fuente:
<https://gitlab.gnome.org/GNOME/xdg-desktop-portal-gnome/-/raw/46.0/src/remotedesktopdialog.c>
y <https://gitlab.gnome.org/GNOME/xdg-desktop-portal-gnome/-/raw/46.0/src/clipboard.c>.

Por tanto, en GNOME 46 el caso “sólo portapapeles” es una restricción de la
interfaz del diálogo: hay que activar “Allow Remote Interaction” para que Share
se habilite, pero **no se concede entrada** si el mask es 0. El adaptador no
activa entrada ni selecciona pantalla; si un portal/escritorio devuelve un mask
distinto de cero, se niega a continuar. Otros escritorios y versiones pueden
presentar diálogos distintos; no se han probado aquí.

### X11

El backend X11 **no está implementado** en este prototipo. La detección de una
sesión X11 produce `linux-x11-unimplemented` y no se usa el portal Wayland como
sustituto. Implementarlo requeriría `libX11`, propiedad de selección y
`SelectionRequest`/`SelectionNotify`, además de una decisión de producto sobre
permisos. No se afirma que X11 lo haga imposible.

## 11. Limitaciones conocidas

- **Wayland no validado en escritorio.** El código compila y los mensajes
  D-Bus se prueban en memoria, pero no se ha abierto un portal real.
- **TLS Linux no compilado aquí.** WSL no tiene `libssl-dev`; no se afirma que
  la compilación Linux con OpenSSL funcione hasta repetirla con el paquete.
- **Portal y permisos.** El diálogo, la identificación de la aplicación y la
  política de cada escritorio pueden denegar o limitar la sesión. No hay
  confianza persistente ni se reutiliza una sesión entre procesos.
- **Sin captura de entrada.** No se llama a `Notify*` ni a `ConnectToEIS`; no
  se captura pantalla. El objetivo es sólo texto.
- **Sólo texto.** Imágenes, formato enriquecido, archivos e historial quedan
  fuera. Límite de 256 KiB; el texto vacío y los formatos no textuales se
  ignoran; NUL se rechaza.
- **Finales de línea.** El modelo usa LF; Windows escribe CRLF y normaliza al
  leer. Un CR suelto se conserva.
- **Marcador y eco.** El protocolo del portal no tiene el formato privado de
  Windows. El motor suprime el eco por huella de contenido durante 1,5 s; una
  copia humana idéntica en esa ventana puede suprimirse. Es una heurística.
- **Windows y privilegios.** Aplicaciones elevadas/UIPI no están cubiertas. Otro
  proceso puede mantener el portapapeles ocupado; se reintenta y luego se
  informa `busy`.
- **Sin tiempos de espera TLS.** Un par que conecte y no complete el handshake
  puede bloquear la aceptación hasta Ctrl+C.

## 12. Cómo registrar un fallo

Anotar: fecha, sistema y sesión, versión de Ubuntu/GNOME, versión de
`xdg-desktop-portal`/backend, comando exacto, resultado esperado, resultado
observado, código de salida y las líneas de log relevantes. No incluir contenido
privado del portapapeles ni claves. Añadir la fila correspondiente en
`RESULTS.md`; una limitación documentada no es un fallo observado, y una
compilación correcta no demuestra acceso al escritorio.
