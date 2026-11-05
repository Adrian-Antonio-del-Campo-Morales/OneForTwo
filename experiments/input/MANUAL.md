# Manual de ejecución — Teclado y ratón

Actualizado: **5 de noviembre de 2026**. Describe el código actual, incluidos Windows, Wayland y la alternativa X11. Las pruebas reales siguen pendientes. Este manual es para ejecución explícita del usuario; compilar o ejecutar self-tests no autoriza el arranque automático de captura o inyección.

## 1. Antes de empezar

- Un monitor por equipo y disposición horizontal.
- Un equipo aporta el teclado/ratón; cualquiera puede actuar como origen.
- Usar un editor y datos desechables. Soltar modificadores y botones antes de cruzar.
- Sin argumentos o con `--help`, el programa muestra ayuda.
- `input_self_test` y `--self-test` no usan escritorio ni red.
- `--diagnose` consulta el escritorio y, en Wayland, el bus de sesión; no solicita las sesiones de control.
- `--inject-demo` mueve el cursor y escribe. `--source` y `--target` usan dispositivos y red.
- La salida prevista es **Ctrl+Alt+F12**. En Wayland destino debe autorizarse mediante GlobalShortcuts. Verificarla manualmente antes de prolongar el control.
- Falta evidencia de compilación nativa Linux. Si falla, registrar el error; no continuar con una compilación que haya excluido silenciosamente el backend que se quiere evaluar.

## 2. Requisitos y compilación

Ejecutar desde la carpeta real del experimento. La aplicación principal y sus instaladores no están integrados: estos comandos construyen únicamente el prototipo.

### 2.1 Windows

Necesita compilador C++17, Windows SDK, CMake 3.16 o posterior y OpenSSL con cabeceras/bibliotecas para la misma arquitectura. El entorno de referencia inicial figura en [RESULTS.md](RESULTS.md).

En una x64 Native Tools Command Prompt:

```bat
cd /d D:\DEVEL\OneInTwo\experiments\input
cmake -S . -B build\windows -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64"
cmake --build build\windows
build\windows\input_self_test.exe
```

Si faltan `rc.exe` o `mt.exe`, cargar las herramientas del Windows SDK o indicar sus rutas mediante `CMAKE_RC_COMPILER` y `CMAKE_MT`. Ajustar las rutas a la instalación real.

Antes de ejecutar `input_experiment.exe`, proporcionar sus DLL de OpenSSL. Por ejemplo, en PowerShell:

```powershell
$env:Path = "C:\Program Files\OpenSSL-Win64\bin;" + $env:Path
```

El self-test independiente no necesita las DLL de OpenSSL.

### 2.2 Ubuntu con Wayland — objetivo inicial

Identificar sesión y bibliotecas:

```bash
echo "$XDG_SESSION_TYPE"
echo "$WAYLAND_DISPLAY"
pkg-config --modversion gio-2.0 libei-1.0 openssl
```

El código exige InputCapture v2 para origen; RemoteDesktop v2 y GlobalShortcuts v1 para destino. La disponibilidad real se comprobará en diagnóstico y preparación. No se afirma que una versión concreta de Ubuntu los ofrezca.

En una Ubuntu cuyos repositorios proporcionen las dependencias, el usuario puede instalar:

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libssl-dev libglib2.0-dev libei-dev
```

Si un paquete no existe en esa versión, registrar el bloqueo; no presentar el backend como construido. Las instalaciones y cambios de sistema son pasos manuales.

Forzar la compilación Wayland evita confundir un binario sin backend con un resultado satisfactorio:

```bash
cd ~/OneInTwo/experiments/input
cmake -S . -B build/wayland -DCMAKE_BUILD_TYPE=Release \
  -DONEINTWO_WAYLAND=ON -DONEINTWO_X11=OFF -DONEINTWO_WITH_TLS=ON
cmake --build build/wayland -j
./build/wayland/input_self_test
```

Estos comandos son instrucciones pendientes, no una compilación Linux ya verificada.

### 2.3 Ubuntu con Xorg — alternativa separada

Solo si se desea probar X11:

```bash
sudo apt install build-essential cmake libssl-dev libx11-dev libxi-dev libxtst-dev
cmake -S . -B build/x11 -DCMAKE_BUILD_TYPE=Release \
  -DONEINTWO_X11=ON -DONEINTWO_WAYLAND=OFF -DONEINTWO_WITH_TLS=ON
cmake --build build/x11 -j
./build/x11/input_self_test
```

Ejecutar ese binario desde una sesión Xorg. La selección de backend no sustituye Wayland por X11 dentro de una sesión Wayland.

### 2.4 Solo núcleo y flujos con dobles

Para un entorno sin escritorio o cabeceras nativas:

```bash
cmake -S . -B build/portable -DONEINTWO_BACKEND=none -DONEINTWO_WITH_TLS=OFF
cmake --build build/portable --target input_self_test -j
./build/portable/input_self_test
```

No se construye la aplicación nativa. La referencia observada en Windows el 5 de noviembre es **538 comprobaciones, 0 fallos**, salida 0. Una diferencia en el número requiere comprobar versión del código y binario; cualquier fallo impide continuar con las pruebas reales. Esta batería no valida GDBus/libei ni X11.

## 3. Certificados e identidad

Cada equipo usa su certificado y clave propios. Intercambiar y comparar las huellas del certificado público por una vía de confianza; conservar la clave privada en su equipo.

PowerShell:

```powershell
New-Item -ItemType Directory -Force certificates | Out-Null
openssl req -x509 -newkey rsa:3072 -sha256 -nodes -days 30 -keyout certificates\win.key -out certificates\win.crt -subj "/CN=OneInTwo-Windows"
openssl x509 -in certificates\win.crt -noout -fingerprint -sha256
```

Ubuntu:

```bash
mkdir -p certificates
chmod 700 certificates
openssl req -x509 -newkey rsa:3072 -sha256 -nodes -days 30 \
  -keyout certificates/ubuntu.key -out certificates/ubuntu.crt -subj "/CN=OneInTwo-Ubuntu"
chmod 600 certificates/ubuntu.key
openssl x509 -in certificates/ubuntu.crt -noout -fingerprint -sha256
```

Guardar las huellas: Windows recibirá la de Ubuntu y Ubuntu la de Windows. `--peer-fingerprint` admite 64 dígitos hexadecimales, con separadores. Las huellas abreviadas de ejemplos deben sustituirse por valores completos.

Estos certificados sirven para los experimentos; no implementan la vinculación persistente de la aplicación.

## 4. Diagnóstico y permisos

Windows:

```powershell
.\build\windows\input_experiment.exe --diagnose
```

Ubuntu/Wayland:

```bash
./build/wayland/input_experiment --diagnose
```

En Xorg, sustituir el ejecutable por `./build/x11/input_experiment`. Si se compilaron ambos adaptadores, el programa elige según la sesión.

Registrar nombre de backend, sesión, capacidades declaradas y notas. En Wayland el diagnóstico consulta versiones de portales; la geometría definitiva y los dispositivos EIS se comprueban al preparar cada papel. Una capacidad declarada no es un permiso concedido.

| Papel Wayland | Preparación manual esperada |
| --- | --- |
| Origen | Autorizar InputCapture para teclado/puntero; obtener una zona y armar una barrera en el borde |
| Destino | Autorizar RemoteDesktop para teclado/puntero y reservar el atajo de emergencia mediante GlobalShortcuts |
| Destino con dispositivos insuficientes | Rechazo si falta teclado, puntero absoluto, puntero para botones/rueda o una región válida |
| Denegación/cancelación | Error explícito y liberación de las sesiones preparadas, sin habilitar control |
| Retirada de permisos durante la prueba | Recuperación o cierre; registrar comportamiento real |

El destino prepara permisos antes de escuchar en red. El origen también prepara InputCapture antes de conectar. Durante esa preparación no acercar el cursor al borde de prueba hasta que el programa anuncie que está armado.

Si el escritorio no reserva la salida local requerida, el backend destino debe rechazar la preparación. No dar por verificado el atajo por aceptar un diálogo: comprobar que la combinación realmente libera el control.

## 5. Inyección local opcional

Esta prueba sí mueve y escribe. Abrir un editor desechable y lanzar:

```powershell
.\build\windows\input_experiment.exe --inject-demo --arm-seconds 5
```

```bash
./build/wayland/input_experiment --inject-demo --arm-seconds 5
```

En Wayland se solicitan primero los permisos del destino; después comienza la cuenta atrás. Enfocar el editor en ese intervalo. El prototipo intenta un movimiento breve y escribe `oneintwo`. Registrar el resultado real, los errores y la capacidad de salir; la impresión «Demostracion finalizada» no sustituye a observar el editor.

## 6. Red local

Puerto del ejemplo: **47830/TCP**. Ambos equipos deben ser alcanzables. El destino escucha en las interfaces que use su implementación; el usuario debe limitar el cortafuegos a la IP del origen.

Ejemplo opcional en Windows con una consola de administración:

```powershell
New-NetFirewallRule -DisplayName "OneInTwo input 47830" -Direction Inbound -Protocol TCP -LocalPort 47830 -RemoteAddress 192.168.1.20 -Action Allow -Profile Private
```

Retirar la regla al terminar:

```powershell
Remove-NetFirewallRule -DisplayName "OneInTwo input 47830"
```

Si Ubuntu usa UFW, una regla limitada sería:

```bash
sudo ufw allow from 192.168.1.20 to any port 47830 proto tcp
```

Su retirada:

```bash
sudo ufw delete allow from 192.168.1.20 to any port 47830 proto tcp
```

Sustituir la IP por la del origen real. El agente no ejecuta estos cambios. No publicar claves ni abrir el puerto a Internet.

## 7. Prueba Windows como origen y Ubuntu como destino

Primera sesión corta: destino 60 segundos y origen 30 segundos, para dejar margen a la preparación. Los temporizadores de control empiezan después de preparar permisos/conexión, no limitan todo el tiempo empleado en los diálogos.

1. Lanzar primero el destino Ubuntu y completar permisos.
2. Esperar el anuncio de escucha TLS.
3. Lanzar el origen Windows.
4. Enfocar en Ubuntu el editor desechable antes del cruce.
5. Esperar el anuncio de origen armado y cruzar con modificadores/botones sueltos.

Ubuntu:

```bash
./build/wayland/input_experiment --target --listen 47830 \
  --cert certificates/ubuntu.crt --key certificates/ubuntu.key \
  --peer-fingerprint "HUELLA_COMPLETA_DE_WINDOWS" --max-seconds 60
```

Windows, situado a la izquierda de Ubuntu:

```powershell
.\build\windows\input_experiment.exe --source --connect 192.168.1.50:47830 --edge right `
  --cert certificates\win.crt --key certificates\win.key `
  --peer-fingerprint "HUELLA_COMPLETA_DE_UBUNTU" --arm-seconds 10 --max-seconds 30
```

Sustituir dirección y huellas. El borde de retorno se deduce en destino. Verificar movimiento, clic, rueda, texto y vuelta por el borde opuesto. Probar inmediatamente la salida local antes de ampliar la duración.

## 8. Invertir los papeles

Windows como destino:

```powershell
.\build\windows\input_experiment.exe --target --listen 47830 `
  --cert certificates\win.crt --key certificates\win.key `
  --peer-fingerprint "HUELLA_COMPLETA_DE_UBUNTU" --max-seconds 60
```

Ubuntu como origen, situado a la derecha de Windows:

```bash
./build/wayland/input_experiment --source --connect 192.168.1.20:47830 --edge left \
  --cert certificates/ubuntu.crt --key certificates/ubuntu.key \
  --peer-fingerprint "HUELLA_COMPLETA_DE_WINDOWS" --arm-seconds 10 --max-seconds 30
```

El origen Wayland recibe la activación de la barrera del compositor. Al volver solicita Release con posición sugerida: verificar el retorno real sin exigir que el cursor aparezca en una coordenada exacta.

Una dirección correcta no valida la inversa. Si se prueba Xorg, repetir y registrar en filas separadas.

## 9. Matriz manual

Todas estas pruebas están pendientes. Registrar sistema, versión de sesión/escritorio, portales, libei, layout, compilación y resultado observado.

| Caso | Acción | Resultado que se debe comprobar | Observado |
| --- | --- | --- | --- |
| D1 | Diagnóstico Windows | Backend y capacidades declaradas coherentes | Pendiente |
| D2 | Diagnóstico Wayland | Portales y capacidades exigidas; sin fallback X11 | Pendiente |
| D3 | Diagnóstico Xorg opcional | X11/XI2/XTest, si se compiló ese backend | Pendiente |
| P1 | Denegar o cancelar InputCapture | Origen rechaza y limpia la preparación | Pendiente |
| P2 | Denegar RemoteDesktop o el atajo | Destino no habilita la sesión de control | Pendiente |
| P3 | Aceptar permisos | Geometría y dispositivos requeridos disponibles | Pendiente |
| I1/I2 | Inyección local Windows/Wayland | Movimiento y texto en el editor elegido | Pendiente |
| C1 | Windows → Ubuntu | Cruce, entrada y retorno | Pendiente |
| C2 | Ubuntu → Windows | Activación por barrera, entrada y retorno | Pendiente |
| C3 | Salir y acercarse de nuevo al borde | Sin rebotes ni sesiones duplicadas | Pendiente |
| K1 | Teclado español | ñ, acentos, AltGr, símbolos y modificadores | Pendiente |
| K2 | Clic, arrastre y rueda tras cruzar | Acciones y liberaciones correctas | Pendiente |
| R1 | Emergencia en origen | Captura liberada sin esperar al destino | Pendiente |
| R2 | Emergencia en destino | Entrada liberada y salida local operativa | Pendiente |
| R3 | Actividad física en destino Windows | Sesión termina y se libera estado | Pendiente |
| R4 | Corte de red con tecla/botón pulsado | Recuperación local y watchdog remoto | Pendiente |
| R5 | Cerrar proceso origen desde otro medio | Destino libera por su temporizador | Pendiente |
| R6 | Alcanzar duración máxima | Cierre y limpieza en ambos extremos | Pendiente |
| R7 | Cancelar antes de cruzar | Salida y cierre de sesiones autorizadas | Pendiente |
| R8 | Huella incorrecta en cada extremo | Rechazo, sin sesión de control autorizada | Pendiente |
| R9 | Retirar sesión/permiso del escritorio | Error, liberación y recuperación | Pendiente |
| R10 | Cierre de EIS o cambio de zona | Recuperación o rechazo con explicación | Pendiente |
| R11 | Falta de portal/dispositivos/atajo | Error concreto, sin simular éxito | Pendiente |
| R12 | Puerto inaccesible | No queda captura activa ni entrada inyectada | Pendiente |

Para probar una huella incorrecta, modificar un dígito y repetir con el error en cada extremo por separado. Para probar pérdida de origen, disponer de otro medio de cerrar el proceso; no matar el receptor mientras mantenga entrada inyectada como primera prueba.

La identidad de tecla es posicional HID. Usar distribución española en ambos extremos y registrar la tecla física si no coincide el carácter. Ejemplo:

```text
ñ Ñ á é í ó ú ü < > @ € # ~
Ctrl, Alt, Shift y AltGr
```

## 10. Recuperación y límites

- Origen: la combinación de emergencia prevista es Ctrl+Alt+F12 durante captura. Debe devolver a control local; su comportamiento real está sin verificar.
- Destino Windows: hook local de emergencia y detección de actividad física.
- Destino X11: reserva del atajo mediante XGrabKey.
- Destino Wayland: GlobalShortcuts; no se inicia el control si el flujo no concede la reserva.
- Pérdida de comunicación: objetivo del código de 2 s en origen y 1,5 s de watchdog en destino. Son temporizadores configurados, no latencias medidas.
- Wayland origen solicita posición de retorno; Wayland destino no restaura un cursor global previo.
- El watchdog no sobrevive a la muerte del propio receptor. Usar la salida prevista y liberar estados antes de cerrar forzosamente ese proceso.
- Un monitor, región única, sin reconexión automática, vídeo, portapapeles ni dispositivos múltiples.
- No se garantiza control de ventanas elevadas Windows ni del escritorio seguro.
- No se sincronizan modificadores/botones previos al cruce.
- La biblioteca y API compiladas no demuestran soporte de la sesión real.

## 11. Diagnóstico de problemas

| Síntoma | Qué comprobar |
| --- | --- |
| Wayland detectado, backend no incluido | Reconstruir con `ONEINTWO_WAYLAND=ON` y sus dependencias; no usar una compilación portable |
| Captura no disponible | InputCapture v2 y capacidades de teclado/puntero que detecta el prototipo |
| Inyección no disponible | RemoteDesktop v2 y GlobalShortcuts v1; luego permisos y dispositivos EIS |
| Geometría cero en diagnóstico | Repetir la observación después de preparar permisos; no se conoce necesariamente antes |
| Preparación del destino rechazada | Atajo no concedido, dispositivo absoluto ausente, región ausente o varias regiones |
| Error de permisos o tiempo de espera | Diálogo rechazado/cancelado o portal sin respuesta; registrar el texto completo |
| No conecta | Destino ya autorizado y escuchando, IP, puerto y cortafuegos |
| Error de certificado | Huella completa del otro equipo y par certificado/clave correctos |
| Binario Windows no arranca | DLL de OpenSSL de arquitectura compatible en PATH |
| No vuelve al origen | Registrar borde, coordenadas, backend y estado; usar salida local y marcar fallo |
| Teclas incorrectas | Distribución, conversión HID y modificadores; no dar por válida la prueba por aparecer texto |

## 12. Registro

Anotar los resultados en [RESULTS.md](RESULTS.md), manteniendo separadas compilación, self-test y cada prueba real. La referencia de 538/0 corresponde al self-test Windows ejecutado el 5 de noviembre; no es evidencia de un handshake, un portal o un pegado real.

