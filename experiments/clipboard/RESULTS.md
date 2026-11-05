# Resultados — Portapapeles

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
