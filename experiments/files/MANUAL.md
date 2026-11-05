# Manual de ejecución — experimento de archivos

Estado: prototipo implementado y compilado en Windows; self-tests aislados
ejecutados. **El pegado real en Explorer/Nautilus, el TLS mutuo y la transferencia
entre dos equipos no se han ejecutado**: este documento es el guion para hacerlo
y la matriz para registrar los resultados. Nada de lo que aparece como «esperado»
está validado todavía.

Responsabilidad de este archivo: permitir que el usuario reproduzca cada prueba
sin confundirla con las comprobaciones automáticas, y recoger qué observar,
cómo detener el prototipo y cómo dejar el sistema como estaba.

## 1. Alcance

Se prueba copiar y pegar, en ambos sentidos:

- un archivo pequeño, un archivo vacío y una carpeta con subcarpetas;
- con el contenido ya descargado (alternativa **A, caché**);
- con entrega del contenido al pegar (alternativa **B, bajo demanda**), solo
  donde el prototipo la ofrece (Windows).

No forman parte de la prueba: cortar/mover, borrar originales, sincronizar
carpetas, montar un sistema de archivos virtual ni compartir todo el disco.

## 2. Inventario del entorno (P0)

Rellenar antes de empezar; sin estos datos el resultado no es interpretable.

| Dato | Windows | Ubuntu |
| --- | --- | --- |
| Sistema y compilación |  |  |
| Escritorio y sesión (X11/Wayland) |  |  |
| Explorador y versión |  |  |
| Compilador y CMake |  |  |
| OpenSSL |  |  |
| GTK3 | no aplica |  |
| Permisos y red usados |  |  |

Comandos útiles:

```powershell
# Windows (PowerShell)
Get-ComputerInfo | Select-Object WindowsProductName, WindowsVersion, OsBuildNumber
(Get-Item C:\Windows\explorer.exe).VersionInfo.ProductVersion
cmake --version
```

```bash
# Ubuntu
lsb_release -a; echo "sesión: $XDG_SESSION_TYPE"
gnome-shell --version; nautilus --version
cmake --version; openssl version; pkg-config --modversion gtk+-3.0
```

## 3. Dependencias y compilación

### Windows

Requisitos:

- Windows 10/11 x64.
- Visual Studio 2022/2026 **Build Tools** con «Desarrollo de escritorio con C++»
  (proporciona `cl.exe` y `nmake.exe`) y Windows SDK.
- CMake 3.16 o posterior.
- OpenSSL 3 para Win64 con cabeceras y `libssl.lib`/`libcrypto.lib`
  (probado con la instalación en `C:\Program Files\OpenSSL-Win64`).

Compilar en **«x64 Native Tools Command Prompt for VS»** (no en PowerShell
normal: `nmake` no está en el PATH):

```bat
cd /d D:\DEVEL\OneInTwo\experiments\files
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ^
      -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64"
cmake --build build
```

Binario resultante: `build\oneintwo-files.exe`. Para ejecutarlo fuera del
directorio de compilación, copiar junto a él `libssl-3-x64.dll` y
`libcrypto-3-x64.dll`, o añadir `C:\Program Files\OpenSSL-Win64\bin` al PATH.

Si OpenSSL no se encuentra, CMake avisa y compila sin TLS: `--share` y
`--receive-offer` se bloquean con un error explícito; no se usa texto plano como
sustituto.

### Ubuntu

```bash
sudo apt install build-essential cmake pkg-config libgtk-3-dev libssl-dev
cd ~/OneInTwo/experiments/files        # ruta real del repositorio
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Binario: `./build/oneintwo-files`. GTK3 es necesario para publicar en el
portapapeles de Nautilus; sin él, CMake avisa y ese modo devuelve error. OpenSSL
es necesario para el modo entre equipos.

### Comprobación común

```bash
# Windows: build\oneintwo-files.exe --self-test
# Ubuntu:  ./build/oneintwo-files --self-test
```

Esperado: recuento de comprobaciones con **0 fallos** y el texto
«self-test correcto (sin portapapeles ni red)». Este modo no abre el
portapapeles ni la red y puede ejecutarse siempre.

## 4. Preparación de datos desechables

Usar carpetas nuevas, nunca documentos personales.

```bat
:: Windows
mkdir D:\o2t-manual
build\oneintwo-files.exe --make-fixtures D:\o2t-manual\datos
```

```bash
# Ubuntu
mkdir -p ~/o2t-manual
./build/oneintwo-files --make-fixtures ~/o2t-manual/datos
```

Contenido creado (determinista, sin datos reales):

| Elemento | Tamaño | Para qué |
| --- | --- | --- |
| `file-small.txt` | 1024 B | archivo normal |
| `empty.dat` | 0 B | archivo vacío |
| `ñandú-日本語.txt` | 46 B | nombres Unicode |
| `folder/nested/deep.bin` | 300 KiB | anidamiento y bloques |
| `folder/empty-subdir/` | carpeta vacía | estructura |
| `large-5mb.bin` | 5 MiB | progreso, cancelación y tiempos |

`--inspect RUTA...` muestra el manifiesto sin publicar nada (rutas relativas,
tamaños e identificadores acortados). No ejecuta el escritorio.

Comparación de contenido:

```powershell
Get-FileHash -Algorithm SHA256 D:\o2t-manual\pegado\datos\file-small.txt
```

```bash
sha256sum ~/o2t-manual/pegado/datos/file-small.txt
```

## 5. Modos del ejecutable

| Modo | Comando | Efecto |
| --- | --- | --- |
| Ayuda | `--help` (o sin argumentos) | solo texto; no toca el sistema |
| Self-test | `--self-test [--verbose]` | comprobaciones aisladas |
| Datos | `--make-fixtures DIR` | crea el conjunto sintético |
| Manifiesto | `--inspect RUTA...` | resume la oferta; no publica |
| Huella | `--fingerprint CERT.pem` | imprime el SHA-256; no abre red |
| Caché local | `--publish-cache RUTA...` | publica rutas locales existentes |
| Bajo demanda local | `--publish-ondemand --root RUTA` | Windows; contenido leído del disco al pegar |
| Bajo demanda remoto | `--publish-ondemand --from HOST:PUERTO --pin HUELLA --cert CERT --key CLAVE` | Windows; contenido pedido al otro equipo |
| Servir | `--share --root RUTA --cert CERT --key CLAVE --client-pin HUELLA_CLIENTE` | TLS mutuo + protocolo; Ctrl+C detiene |
| Recibir | `--receive-offer --from HOST:PUERTO --pin HUELLA --cache DIR --cert CERT --key CLAVE` | descarga, verifica y cachea |
| Recibir y pegar | añadir `--publish` a `--receive-offer` | publica en el portapapeles al verificar |
| Copiar a destino | añadir `--to DESTINO` | copia verificada con colisiones resueltas |
| Caché | `--cache-list --cache DIR` | lista ofertas; `--verbose` añade ruta |
| Limpieza | `--cache-gc --cache DIR [--max-age-hours N]` | elimina ofertas antiguas |

Opciones comunes: `--port N` (por defecto 47654), `--timeout MS` (por defecto
15000 ms) y `--verbose`.

Límites del prototipo: 4096 elementos, profundidad 32, 8 GiB por oferta y bloques
de 256 KiB. En Windows, una ruta relativa del descriptor no puede superar 259
caracteres. Se rechazan enlaces simbólicos y tipos especiales.

## 6. Pruebas locales (un solo equipo)

### L1. Self-test

Ejecutar el comando de la sección 3. **Esperado:** 0 fallos, sin ventanas ni
permisos. **Fallo:** código de salida distinto de cero; conservar la carpeta
`.o2f-selftest-*` que imprime.

### L2. Caché local en Windows (Explorer)

1. Ejecutar `build\oneintwo-files.exe --publish-cache D:\o2t-manual\datos`.
   Debe imprimir «publicando N elementos locales» y quedarse esperando.
2. Abrir Explorer, crear `D:\o2t-manual\pegado` y entrar en ella.
3. Pegar con `Ctrl+V`.
4. Comprobar que aparece `datos` con los 6 elementos y que los tamaños coinciden.
5. Comparar `sha256sum`/`Get-FileHash` de `file-small.txt` y `large-5mb.bin`.
6. Volver a la consola y pulsar `Ctrl+C`.

**Esperado:** copia completa, originales intactos, carpeta vacía conservada.
**Fallo:** el portapapeles aparece vacío, solo se pega texto, faltan archivos o
el proceso no responde. Anotar si Explorer pidió permisos adicionales.

> En Windows, la oferta de este modo se materializa al pulsar `Ctrl+C`: el
> portapapeles puede seguir pegando rutas a la caché después de cerrar el
> proceso. Los archivos son locales y ya estaban verificados.

### L3. Bajo demanda local en Windows (B)

1. `build\oneintwo-files.exe --publish-ondemand --root D:\o2t-manual\datos`
2. Pegar en una carpeta nueva.
3. Observar si Explorer crea los archivos y si la ventana se queda bloqueada
   mientras se leen los 5 MiB.
4. Medir tiempo hasta el primer archivo y hasta terminar (cronómetro).
5. Pulsar `Ctrl+C`; intentar pegar de nuevo en otra carpeta.

**Esperado:** copia correcta, sin caché intermedia, con el proceso vivo durante
el pegado. **Fallo:** Explorer no ofrece pegar, error al pegar, bloqueo sin
mensaje o archivos incompletos. **Condición de bloqueo:** si Explorer no acepta
`FileGroupDescriptorW`/`FileContents` en este entorno, B no es viable aquí y se
registra como tal.

> Al cerrar el proceso la oferta bajo demanda desaparece: no hay copia que
> materializar. Es una diferencia esperada frente a L2.

### L4. Caché local en Ubuntu (Nautilus)

1. `./build/oneintwo-files --publish-cache ~/o2t-manual/datos`
2. En Nautilus, abrir una carpeta nueva y pegar `Ctrl+V`.
3. Comprobar estructura, tamaño y `sha256sum`.
4. `Ctrl+C` en la terminal.

**Esperado:** pegado completo con nombres Unicode. **Fallo:** Nautilus no
muestra la opción de pegar archivos, pega solo texto o faltan elementos.
Registrar sesión (`XDG_SESSION_TYPE`), versión de Nautilus y de GTK.

> La selección de GTK pertenece al proceso: al pulsar `Ctrl+C` la oferta
> desaparece. No se materializa como en Windows.

### L5. Bajo demanda en Ubuntu (B)

```bash
./build/oneintwo-files --publish-ondemand --root ~/o2t-manual/datos
```

**Esperado:** error explícito indicando que esta implementación solo publica
rutas `file://` locales y que B requeriría un backend GIO/GVfs o FUSE, no
implementado. **No es una prueba de inviabilidad técnica**: es una limitación de
esta entrega y debe registrarse como tal.

### L6. Colisiones y destino sin escritura

Con un servidor en el propio equipo (dos terminales, TLS):

1. Crear los dos pares de la sección 7.1 y arrancar:
   `--share --root D:\o2t-manual\datos --cert servidor-cert.pem --key servidor-key.pem --client-pin HUELLA_CLIENTE --bind 127.0.0.1`
2. En otra terminal:
   `--receive-offer --from 127.0.0.1 --pin HUELLA_SERVIDOR --cache D:\o2t-manual\cache --to D:\o2t-manual\copia --cert cliente-cert.pem --key cliente-key.pem`
3. Repetir el mismo comando con el mismo destino.
4. Comprobar que la segunda copia crea `datos (1)` y no sobrescribe nada.
5. Crear una carpeta sin permiso de escritura y usar `--to` apuntando a ella.

**Esperado:** sufijos `(1)`, `(2)`; contenido original intacto; en el destino sin
escritura, mensaje de error, código de salida 1 y ningún archivo `.o2f-part`
publicado. **Fallo:** sobrescritura silenciosa, éxito con archivos incompletos o
restos en la carpeta de destino.

### L7. Cancelación

1. Arrancar de nuevo el servidor del paso L6.
2. Lanzar `--receive-offer ... --cache DIR` con los 5 MiB y pulsar `Ctrl+C`
   cuando ya se haya descargado algún archivo.
3. Revisar `DIR\incoming` y `DIR\offers`.

**Esperado:** mensaje de cancelación, sin oferta publicada, `incoming` vacío y
ningún `.o2f-part`. **Fallo:** la oferta aparece como completa o quedan
temporales sin dueño.

### L8. Vida de la caché

1. Tras L6/L7: `--cache-list --cache D:\o2t-manual\cache`.
2. `--cache-gc --cache D:\o2t-manual\cache --max-age-hours 0`.
3. Repetir `--cache-list`.

**Esperado:** la primera lista muestra las ofertas verificadas; el gc con 0 horas
las elimina (salvo una concesión vigente, que aquí ya no existe); la segunda
lista queda vacía. **Fallo:** se elimina una oferta en uso o quedan carpetas
huérfanas.

## 7. Pruebas entre dos equipos

Orden obligatorio: primero local; después dos equipos. Solo un experimento
activo en la misma sesión gráfica.

### 7.1 Certificados de prueba (fuera del repositorio)

El modo remoto usa **TLS mutuo**: cada equipo presenta su propio certificado y
fija la huella del otro. No hay CA ni confianza implícita. Generar un par en
**cada** equipo, en una carpeta fuera del repositorio.

En el equipo **servidor**:

```bat
:: Windows
mkdir "%USERPROFILE%\o2t-certs"
cd /d "%USERPROFILE%\o2t-certs"
"C:\Program Files\OpenSSL-Win64\bin\openssl.exe" req -x509 -newkey rsa:3072 ^
  -sha256 -days 2 -nodes -keyout servidor-key.pem -out servidor-cert.pem ^
  -subj "/CN=OneInTwo files experiment"
```

```bash
# Ubuntu
mkdir -p ~/o2t-certs && cd ~/o2t-certs
openssl req -x509 -newkey rsa:3072 -sha256 -days 2 -nodes \
  -keyout servidor-key.pem -out servidor-cert.pem \
  -subj "/CN=OneInTwo files experiment"
chmod 600 servidor-key.pem
```

En el equipo **cliente**, repetir el mismo comando cambiando `servidor-` por
`cliente-`. Cada equipo conserva su clave privada; intercambiar solo los
certificados públicos por un canal fuera de banda. Calcular las huellas con el
propio prototipo:

```bash
# En el servidor: huella que usará el cliente
./build/oneintwo-files --fingerprint servidor-cert.pem

# En el cliente: huella que usará el servidor
./build/oneintwo-files --fingerprint cliente-cert.pem
# Windows: build\oneintwo-files.exe --fingerprint cliente-cert.pem
```

- El servidor arranca con `--client-pin HUELLA_DEL_CERTIFICADO_DE_CLIENTE`.
- El cliente arranca con `--pin HUELLA_DEL_CERTIFICADO_DEL_SERVIDOR` y presenta
  su par con `--cert` y `--key`.

Si se regenera cualquiera de los dos certificados, su huella cambia y el otro
extremo rechazará el handshake hasta actualizar el pin correspondiente. Es el
comportamiento esperado.

#### 7.1.1 Rechazo de identidades no fijadas (manual)

1. Arrancar el servidor con un `--client-pin` distinto del certificado que usará
   el cliente.
2. Conectar el cliente con su par correcto: el servidor debe rechazar el
   handshake («certificado de cliente no autorizado») y el cliente no debe
   recibir el manifiesto.
3. Arrancar el servidor con el pin correcto y conectar el cliente con un `--pin`
   incorrecto: el cliente debe rechazar el certificado del servidor.
4. Repetir con ambos pines correctos y comprobar que la transferencia funciona.

**Esperado:** error explícito en ambos rechazos, sin descarga ni publicación.
**Fallo:** conexión aceptada con una huella distinta o mensaje genérico sin
diagnóstico. Esta prueba abre red real: solo manual.

### 7.2 Red y permisos

- Anotar la IP del servidor: `ipconfig` (Windows) o `ip -4 addr show` (Ubuntu).
- Windows puede mostrar un aviso de Firewall al escuchar; permitir solo en la
  red privada. Alternativa con permisos de administrador (revertir al terminar):
  `netsh advfirewall firewall add rule name="OneInTwo files experiment" dir=in action=allow protocol=TCP localport=47654`
- Ubuntu con ufw: `sudo ufw allow 47654/tcp`, y retirar la regla al final.
- No hace falta ejecutar como administrador ni instalar servicios.

### T1. Windows → Ubuntu (caché)

1. **Windows (origen):**
   ```bat
   build\oneintwo-files.exe --share --root D:\o2t-manual\datos ^
     --cert "%USERPROFILE%\o2t-certs\servidor-cert.pem" ^
     --key  "%USERPROFILE%\o2t-certs\servidor-key.pem" ^
     --client-pin HUELLA_CLIENTE --bind 0.0.0.0 --port 47654
   ```
   Anotar la huella impresa y el mensaje «escuchando».
2. **Ubuntu (destino):**
   ```bash
   ./build/oneintwo-files --receive-offer --from IP_DEL_WINDOWS \
     --pin HUELLA_SERVIDOR --cache ~/o2t-manual/cache --publish \
     --cert ~/o2t-certs/cliente-cert.pem --key ~/o2t-certs/cliente-key.pem
   ```
   Debe verificar los 5 archivos y publicar en el portapapeles.
3. En Nautilus, crear `~/o2t-manual/pegado` y pegar.
4. Verificar: `datos/file-small.txt`, `datos/empty.dat` (0 B),
   `datos/ñandú-日本語.txt`, `datos/folder/nested/deep.bin`,
   `datos/folder/empty-subdir/`, `datos/large-5mb.bin`; comparar hashes.
5. `Ctrl+C` en Ubuntu y después en Windows.

**Esperado:** pegado completo, hashes iguales, estructura conservada.
**Fallo:** error TLS (huella o certificado), conexión rechazada (firewall/IP),
manifiesto erróneo o archivos incompletos. Registrar el texto exacto.

### T2. Ubuntu → Windows (caché)

1. **Ubuntu (origen):**
   ```bash
   ./build/oneintwo-files --share --root ~/o2t-manual/datos \
     --cert ~/o2t-certs/servidor-cert.pem --key ~/o2t-certs/servidor-key.pem \
     --client-pin HUELLA_CLIENTE --bind 0.0.0.0 --port 47654
   ```
2. **Windows (destino):**
   ```bat
   build\oneintwo-files.exe --receive-offer --from IP_DE_UBUNTU ^
     --pin HUELLA_SERVIDOR --cache D:\o2t-manual\cache --publish ^
     --cert "%USERPROFILE%\o2t-certs\cliente-cert.pem" ^
     --key  "%USERPROFILE%\o2t-certs\cliente-key.pem"
   ```
3. Pegar en Explorer en `D:\o2t-manual\pegado` y comparar.
4. `Ctrl+C` en Windows; la oferta queda materializada; repetir el pegado en otra
   carpeta para comprobar que las rutas cacheadas siguen disponibles.
5. `Ctrl+C` en Ubuntu.

**Esperado:** idéntico a T1, incluido el nombre Unicode. **Fallo:** nombres
alterados, carpetas perdidas o error al pegar.

### T3. Windows ← Ubuntu bajo demanda (B)

1. Ubuntu origen: el mismo `--share` de T2.
2. Windows destino:
   ```bat
   build\oneintwo-files.exe --publish-ondemand --from IP_DE_UBUNTU ^
     --pin HUELLA_SERVIDOR ^
     --cert "%USERPROFILE%\o2t-certs\cliente-cert.pem" ^
     --key  "%USERPROFILE%\o2t-certs\cliente-key.pem" --timeout 15000
   ```
3. Pegar en una carpeta nueva. Cronometrar: tiempo hasta el primer archivo y
   hasta terminar; anotar si la ventana de Explorer se bloquea.
4. Con una transferencia en curso, detener el servidor Ubuntu (`Ctrl+C`) o
   desconectar la red. Observar el error del explorador.
5. Repetir sin la interrupción y comparar hashes.

**Esperado:** pegado correcto con el servidor vivo; ante corte, error visible y
ningún archivo declarado completo. **Fallo:** bloqueo indefinido, éxito falso o
archivo incompleto presentado como válido. **Condición de bloqueo:** si Explorer
no solicita `FileContents` o no admite el descriptor, registrar B como no viable
en Windows para este entorno.

> `--timeout` limita cada lectura TLS. Un servidor que deja de responder debe
> producir un error acotado, no un bloqueo infinito.

### T4. Ubuntu bajo demanda

No se ejecuta: el prototipo devuelve «no soportado» porque esta implementación
solo publica rutas `file://` locales. Una entrega perezosa requeriría un backend
GIO/GVfs o FUSE, fuera del encargo; no se afirma que sea técnicamente imposible.

## 8. Comparación A frente a B

Registrar con los mismos datos de 5 MiB y la misma red:

| Métrica | A: caché | B: bajo demanda |
| --- | --- | --- |
| Tiempo hasta poder pegar |  |  |
| Tiempo total de pegado |  |  |
| Espacio en disco del prototipo |  |  |
| Bloqueo percibido del explorador |  |  |
| Comportamiento al cerrar el proceso |  |  |
| Error si falla la red al pegar | no aplica |  |
| Complejidad de integración | baja | alta |

Observaciones cualitativas que hay que anotar:

- ¿Explorer/Nautilus muestra progreso? ¿Se puede cancelar desde el explorador?
- ¿Se puede pegar dos veces seguidas?
- ¿Qué ocurre si se pega antes de terminar la descarga (A)?
- ¿La ventana del explorador responde mientras se lee un archivo grande (B)?

## 9. Detener y restaurar el estado

1. `Ctrl+C` en cada proceso del prototipo. Debe imprimir el final de la
   publicación o «servidor detenido»; el código de salida debe ser 0.
2. Borrar los datos y la caché de prueba:
   ```bat
   rmdir /s /q D:\o2t-manual
   rmdir /s /q "%USERPROFILE%\o2t-certs"
   ```
   ```bash
   rm -rf ~/o2t-manual ~/o2t-certs
   ```
3. Si se añadió la regla de firewall, retirarla:
   ```bat
   netsh advfirewall firewall delete rule name="OneInTwo files experiment"
   ```
   ```bash
   sudo ufw delete allow 47654/tcp
   ```
4. El prototipo no instala servicios, no escribe en el registro ni crea inicio
   automático. La carpeta `build/` puede borrarse en cualquier momento.

## 10. Matriz de resultados manuales

Una fila por ejecución. Copiar esta tabla a `RESULTS.md` o completarla aquí; no
marcar como superado nada que no se haya observado.

| Caso | Equipo y sesión | Esperado | Observado | Resultado |
| --- | --- | --- | --- | --- |
| L1 self-test |  | 0 fallos |  | Pendiente |
| L2 caché Explorer |  | pegado completo y hash igual |  | Pendiente |
| L3 bajo demanda local Windows |  | pegado completo o bloqueo documentado |  | Pendiente |
| L4 caché Nautilus |  | pegado completo y hash igual |  | Pendiente |
| L5 bajo demanda Linux |  | error explícito: limitación de implementación, no inviabilidad |  | Pendiente |
| L6 colisiones y sin escritura |  | sufijos, sin sobrescritura, error claro |  | Pendiente |
| L7 cancelación |  | sin oferta publicada ni temporales |  | Pendiente |
| L8 caché y gc |  | ofertas eliminadas según edad |  | Pendiente |
| T1 Windows→Ubuntu |  | pegado completo, hash igual |  | Pendiente |
| T2 Ubuntu→Windows |  | pegado completo, hash igual |  | Pendiente |
| T3 Windows←Ubuntu bajo demanda |  | pegado completo; corte con error |  | Pendiente |
| T0 TLS mutuo negativo |  | rechazo de huella de cliente o servidor |  | Pendiente |

Condiciones de fallo que invalidan una fila: hash distinto, archivo incompleto
presentado como terminado, sobrescritura sin aviso, bloqueo sin error, o
publicación «correcta» sin que el explorador haya solicitado el contenido.
