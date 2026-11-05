# Resultados — Archivos

## Casos locales L6/L7/L8 completados — 5 de noviembre de 2026

Base Git `450cbb2e340a381bda01274eb86df0e05934e822` más cambios locales.
Se usaron los cinco archivos/5.551.150 bytes ya verificados, servidor y
cliente reales con **TLS mutuo solo en 127.0.0.1**, huellas fijadas y pares
de certificados de prueba de dos días, claves con modo 0600 dentro de la
salida local ignorada. Sin portapapeles, permisos globales ni Windows.

L6 descubrió un fallo de la CLI: `fetchFile` verificaba cada `.o2f-part`,
pero `cmdReceive` no lo renombraba al nombre definitivo antes de publicar la
oferta. Por eso `--to` fallaba con «falta un archivo verificado en la oferta
de la caché». Se corrigió solo `cmdReceive` en `src/main.cpp`: renombrar tras
la verificación, todavía dentro de `incoming`; un fallo abandona la recepción.
No se cambió el contrato de `StagedFile`. Reconstrucción
`source .local-state/ubuntu-validation/env.sh` y
`cmake --build experiments/files/build/ubuntu -j4`, salida 0.
Binario corregido SHA-256
`35dd15ad8e2127f74453fa2e2a1e4f780c3fb7971be6d4c174de23914450b6cd`.
Los casos reales siguientes constituyen la comprobación de la corrección;
no se añadieron ni repitieron self-tests.

| Caso | Resultado |
| --- | --- |
| L6 primera recepción y repetición | Salidas 0; `datos` y `datos (1)` con estructura y SHA-256 idénticos al origen, sin sobrescribir |
| L6 destino sin escritura | Carpeta desechable modo 0500, salida 1, «no se pudo crear una carpeta en el destino»; destino vacío; modo restaurado a 0700 |
| L7 cancelación | SIGINT después de observar 262.144 bytes en recepción parcial; salida 1, «transferencia cancelada»; `incoming` y `offers` vacíos, ningún `.o2f-part` |
| L8 caché | Lista inicial con ofertas; `--cache-gc --max-age-hours 0`, salida 0; lista posterior vacía, originales y copia de destino intactos |

Para poder interrumpir los 5 MiB a tiempo, L7 utilizó un relay TCP opaco de
loopback que retrasó los bloques cifrados servidor→cliente (16 KiB/10 ms).
No descifra ni altera TLS ni sustituye servidor/receptor por dobles. Solo
se usó para L7. Servidor detenido normalmente, salida 0.

Comando reproducible: `/usr/bin/python3
experiments/files/manual-output/local-final-20261105-fixed/run.py`.
`commands.json` conserva argumentos completos y puertos elegidos;
`results.json`: **PASS**. Los logs L6/L7/L8 y el guion están en esa misma
carpeta ignorada. Se conservó el primer intento en `local-final-20261105/`:
error inicial del guion al incluir el prefijo impreso en la huella (corregido
sin cambiar el prototipo) y, después, fallo real de nombres temporales.

La fase local disponible queda completada con la limitación de pegado
asistido ya documentada. Publicación automática en segundo plano y bajo
demanda Linux siguen como limitaciones de implementación; no se añadió
trabajo de producto. Transferencias Windows/Ubuntu pendientes de coordinación.

## Validación local de escritorio — 5 de noviembre de 2026

**Pegado asistido en Nautilus verificado por el usuario y por comparación de
estructura, tamaños y SHA-256: 5 archivos, 5.551.150 bytes, sin diferencias.**
Se conservan los fallos previos y la evidencia Windows. No se ha probado red.

Base Git: `450cbb2e340a381bda01274eb86df0e05934e822`, rama
`validation/ubuntu`, más las correcciones locales del adaptador GTK descritas
abajo. Sin commit ni publicación. Ubuntu 26.04.1, GNOME Wayland, Nautilus
`1:50.2.2-0ubuntu0.2`, GTK3 3.24.52; herramientas y dependencias de la
compilación inicial que sigue. Se forzó `GDK_BACKEND=wayland` en las tres
publicaciones: ninguna sustituye Wayland por X11.

### Ejecuciones y correcciones necesarias

| Caso | Resultado observado | Estado |
| --- | --- | --- |
| L4, primer intento sin ventana | El usuario no puede pegar; destino vacío. La traza muestra `set_selection(..., 0)` sin evento local. Parada dirigida con SIGTERM, salida 0 | Fallo real |
| L4, segundo intento con botón local | La oferta se publica con serial 55210 y el compositor pide `x-special/gnome-copied-files`, pero Nautilus no pega. Parada dirigida, salida 0 | Fallo real de formato |
| L4, tercer intento corregido | Usuario pulsa Publicar archivos y confirma carpeta `datos` pegada. Comparación completa con origen correcta. Cierra ventana con X; proceso termina con código 0 | Superado como pegado asistido |
| L5, bajo demanda Linux | Error explícito: solo rutas `file://` locales; B requiere un backend GIO/GVfs o FUSE no implementado; salida 1 | Comportamiento esperado; limitación de esta entrega |

Correcciones en `src/platform/linux/clipboard_linux.cpp`:

1. El proceso GTK sin ventana enviaba una selección con serial 0. En Wayland
   ahora abre una ventana mínima de prueba y publica tras el botón local
   **Publicar archivos**, conservando el proceso hasta cerrar la ventana o
   recibir SIGINT/SIGTERM. No se instalan hooks ni se captura entrada global.
2. `x-special/gnome-copied-files` se enviaba como `copy\n` seguido de URIs con
   CRLF y salto final. El parser de Nautilus 50 separa por LF y rechaza líneas
   vacías: ahora se emite `copy` más `\nURI` por elemento, sin salto final.
   `text/uri-list` conserva sus CRLF. Se usó como referencia el
   [parser publicado por GNOME para Nautilus 50.2.2](https://raw.githubusercontent.com/GNOME/nautilus/50.2.2/src/nautilus-clipboard.c).

La reconstrucción del adaptador con GTK/TLS terminó con código 0. El self-test
posterior al cambio de formato dio **176 comprobaciones, 0 fallos**, sin
inicializar GTK ni el portapapeles. La corrección posterior de la cabecera
`<utility>` también se recompiló correctamente; no modifica ese comportamiento.

### Comandos y evidencia

Desde la raíz, cargar `source .local-state/ubuntu-validation/env.sh`:

```bash
./experiments/files/build/ubuntu/oneintwo-files --make-fixtures experiments/files/manual-output/ubuntu-01/datos
./experiments/files/build/ubuntu/oneintwo-files --inspect experiments/files/manual-output/ubuntu-01/datos
GDK_BACKEND=wayland ./experiments/files/build/ubuntu/oneintwo-files --publish-cache experiments/files/manual-output/ubuntu-01/datos
./experiments/files/build/ubuntu/oneintwo-files --publish-ondemand --root experiments/files/manual-output/ubuntu-01/datos
cmake --build experiments/files/build/ubuntu -j4
```

El destino `manual-output/ubuntu-01/pegado` se creó vacío y el usuario lo abrió
en Nautilus antes de publicar. El agente no copió archivos al destino: después
del pegado del usuario, comparó los árboles con Python (`Path.rglob`, tamaño y
`hashlib.sha256`), incluidos el nombre `ñandú-日本語.txt`, `empty.dat` de 0 B,
la carpeta vacía y los 5 MiB de `large-5mb.bin`. Origen conservado y destino
idéntico. El manifiesto inspeccionado tiene 9 entradas incluyendo raíz y
directorios; las referencias históricas a 6 elementos no son ese recuento total.

Logs locales ignorados en `manual-output/ubuntu-01/`: `fixtures.log`,
`inspect.log`, `publish-cache-01.log`, `wayland-publish-debug.log`,
`publish-cache-02.log` (traza Wayland), `publish-cache-03.log`, `integrity.log`
(tamaños y hashes de los cinco archivos), `ondemand-linux.log`, logs de
reconstrucción y `self-test-gnome-format.log`. Las publicaciones se supervisaron
con `timeout --signal=TERM --kill-after=3s` (180/240 s); la prueba de diagnóstico
duró 3 s. Los tres publicadores de escritorio terminaron con código 0 tras las
paradas indicadas. No quedó ninguno activo al pasar al experimento de entrada.

### Límites y siguientes casos

El éxito es **asistido por una acción local**: no se declara entrega automática
en segundo plano en Wayland. Haría falta validar otra vía autorizada para ese
contrato; no se integra el portal del otro experimento ni se amplía el producto.
L6/L7/L8 con transporte TLS local y las transferencias Windows/Ubuntu continúan
pendientes. No se generaron certificados ni se abrió una escucha. Los casos
entre equipos requieren revisión común, identidad, roles, direcciones, puerto,
observación y recuperación acordados previamente.

## Compilación inicial Ubuntu nativa — 5 de noviembre de 2026

**Resultado actual: prototipo nativo GTK3 + TLS compilado; 176 comprobaciones,
0 fallos, salida 0.** La evidencia Windows posterior se conserva íntegra.
Las afirmaciones históricas de «Ubuntu sin compilar» quedan actualizadas por
esta ejecución, sin dar por validado el pegado en Nautilus.

### Entorno, revisión y componentes

- Base Git: `450cbb2e340a381bda01274eb86df0e05934e822`, rama
  `validation/ubuntu`, directorio `/home/pholtergate/DEV/OneForTwo`.
  Sin cambios de código en este experimento; el único parche de código local
  del conjunto afecta a la cabecera del transporte de portapapeles.
- Fecha: 2026-11-05 (`Europe/Madrid`). Ubuntu 26.04.1 LTS,
  Linux 7.0.0-42-generic x86_64; sesión heredada Wayland, `ubuntu:GNOME`.
- GNOME Shell `50.1-0ubuntu1.5`, Mutter `50.1-0ubuntu2.4`,
  Nautilus `1:50.2.2-0ubuntu0.2`.
- GCC/G++ `15.2.0-16ubuntu1`, CMake 4.2.3 extraído localmente,
  Make 4.4.1, pkg-config 2.5.1; GTK3 3.24.52 (`3.24.52-0ubuntu1`),
  OpenSSL 3.5.5 (`3.5.5-1ubuntu3.7`), GLib/GIO 2.88.0.
- Paquetes de portales: xdg-desktop-portal `1.21.1+ds-1ubuntu3.1`,
  GNOME `50.0-0ubuntu1`, GTK `1.15.3-2ubuntu1`. No se consultó el bus
  ni se negociaron capacidades o permisos.

Inventario mediante `git status --short --branch`, `git rev-parse HEAD`,
`cat /etc/os-release`, `uname -srmo`, variables de sesión, `c++ --version`,
`dpkg-query -W` y `pkg-config --modversion gtk+-3.0 openssl gio-2.0`.
Dependencias trasladadas desde `/tmp` a `.local-state/ubuntu-validation/`,
dentro del proyecto y excluidas de Git, sin instalación global ni `sudo`:
[preparación y variables exactas](../clipboard/RESULTS.md#dependencias-locales-de-esta-validación).

### Comandos y resultados observados

Desde la raíz del repositorio:

```bash
source .local-state/ubuntu-validation/env.sh
cmake --fresh -S experiments/files -B experiments/files/build/ubuntu \
  -DCMAKE_BUILD_TYPE=Release -DO2F_ENABLE_GTK=ON -DO2F_ENABLE_TLS=ON \
  -DOPENSSL_ROOT_DIR="$OIT_VALIDATION_DEPS/usr"
cmake --build experiments/files/build/ubuntu -j4
cd experiments/files
./build/ubuntu/oneintwo-files --self-test
```

Configuración, compilación y self-test finales: salida 0. CMake informa
`TLS: OpenSSL 3.5.5` y `Found gtk+-3.0, version 3.24.52`.
Se compiló `src/platform/linux/clipboard_linux.cpp`, con
`O2F_WITH_GTK=1` y `O2F_WITH_TLS=1`; el ejecutable enlaza
`libgtk-3.so.0`, `libssl.so.3` y `libcrypto.so.3`. `ldd` no detecta
bibliotecas ausentes. No es la variante `clipboard_unavailable`.
Tras el traslado se regeneró la caché con `--fresh`, se reconstruyó con las
rutas persistentes y se repitió el self-test: **176/0**.

```text
176 comprobaciones, 0 fallos
self-test correcto (sin portapapeles ni red)
```

El temporal `.o2f-selftest-*` se creó dentro de `experiments/files` y se
eliminó al terminar correctamente. No se inicializó GTK ni el portapapeles.

El primer intento omitió GTK porque el entorno local de pkg-config excluía
los metadatos ya instalados de `zlib`, `expat` y `shared-mime-info`.
Ese binario también dio 176/0, pero **no se usó para cerrar la compilación
nativa**. Se añadieron las rutas de metadatos del sistema a `PKG_CONFIG_LIBDIR`,
se reconfiguró, compiló el adaptador GTK y repitió el self-test.
El núcleo emitió un aviso `-Wunused-function` en `src/core/cache.cpp:21`;
no impide compilar y no se realizó limpieza ajena a la validación.

Logs locales ignorados en `build/ubuntu/`: `configure.log`, `build.log`,
`self-test.log`, `libraries.log` y los intentos previos
`configure-without-gtk.log`, `build-without-gtk.log`, `self-test-without-gtk.log`.
La ejecución posterior al traslado queda en `configure-persistent.log`,
`build-persistent.log`, `self-test-persistent.log` y `libraries-persistent.log`.

### Pendientes y siguiente paso

Por indicación del usuario se completó solo la etapa aislada: no se publicaron
ofertas ni se abrió Nautilus. L1 Ubuntu queda superado; L4 pegado, L5 limitación
bajo demanda, L6 colisiones/permisos con TLS, L7 cancelación y L8 caché/gc
continúan pendientes de sus ejecuciones manuales. No se confunden los casos
aislados del self-test con esas observaciones.

Antes de L4: usuario presente, datos sintéticos del manual, destino nuevo,
Ctrl+C preparado y comparación real de estructura/tamaños/SHA-256 tras pegar.
Antes de T0/T1/T2/T3: acordar con Windows la revisión compartida, binarios,
roles, IP y puerto 47654/TCP, certificados públicos y ambas huellas, carpetas,
duración y parada; probar primero identidad rechazada/aceptada. No se ha
iniciado red de prueba ni generado claves. La evidencia Windows no se ha
repetido. No hay bloqueo pendiente de compilación GTK/TLS en esta máquina;
el escritorio y la coordinación entre equipos siguen abiertos. Integración
de la aplicación aplazada.

## Evidencia histórica conservada — entrega y revisión anteriores

Actualizado: 4 de noviembre de 2026.

Revisión documental: 5 de noviembre de 2026. Se conserva la evidencia de la entrega corregida del agente; no se han repetido sus compilaciones o comprobaciones ni realizado pruebas de escritorio o red. El estado consolidado figura en [experiments/README.md](../README.md).

Responsabilidad de este archivo: registrar qué está implementado, qué se ha
compilado y comprobado de forma aislada, y qué sigue pendiente de ejecución
manual. Una compilación correcta no demuestra que el portapapeles o el
explorador funcionen.

## 1. Estado

| Elemento | Estado | Evidencia o bloqueo |
| --- | --- | --- |
| Transferencia, manifiesto, caché y protocolo | Implementado | `src/core/` (modelo, manifiesto, almacén, bloques, caché, tramas, TLS) |
| Publicación de caché en Windows | Implementado | `src/platform/win/clipboard_win.cpp` (`CF_HDROP` + `OleFlushClipboard`) |
| Entrega bajo demanda en Windows | Implementado, sin validar | `CFSTR_FILEDESCRIPTORW` + `CFSTR_FILECONTENTS` con `IStream` |
| Publicación de caché en Ubuntu | Implementado, sin compilar | GTK3 con `x-special/gnome-copied-files` y `text/uri-list` |
| Entrega bajo demanda en Ubuntu | No implementada (fuera de alcance) | Publica solo rutas `file://`; B requeriría backend GIO/GVfs o FUSE; evidencia en §5.1 |
| Compilación Windows x64 | Ejecutada | MSVC 14.51.36231, CMake 3.24.3, OpenSSL 3.0.5, Windows 10.0.19045 |
| Compilación Ubuntu | **No ejecutada** | No hay host Ubuntu en esta sesión; no se afirma |
| Self-tests aislados | **Ejecutados: 176 comprobaciones, 0 fallos** | `oneintwo-files.exe --self-test` |
| Autenticación TLS del cliente | Implementado, sin ejecutar | Certificado de cliente y huella fijada con `--client-pin`; handshake no ejercitado |
| `MANUAL.md` | Entregado | Comandos, permisos, parada y matriz |
| Pegado real en Explorer/Nautilus | **Pendiente de ejecución manual** | Requiere sesión gráfica |
| Transferencia entre dos equipos | **Pendiente de ejecución manual** | Requiere certificados y dos hosts |
| Pegado bajo demanda con red | **Pendiente de ejecución manual** | Requiere explorador y servidor |

## 2. Evidencia de compilación y self-test

Entorno exacto de la compilación realizada:

```text
Windows 10.0.19045.7807 (x64)
Visual Studio 2026 Build Tools 18, MSVC 14.51.36231
CMake 3.24.3
OpenSSL 3.0.5 (Win64, C:\Program Files\OpenSSL-Win64)
Generador: NMake Makefiles, CMAKE_BUILD_TYPE=Release
```

Comandos equivalentes a los ejecutados (en «x64 Native Tools Command Prompt»):

```bat
cd /d D:\DEVEL\OneInTwo\experiments\files
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ^
      -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64"
cmake --build build
build\oneintwo-files.exe --self-test
```

Salida relevante (última compilación):

```text
-- TLS: OpenSSL 3.0.5
[100%] Built target oneintwo-files
self-test aislado en ...\experiments\files\.o2f-selftest-<pid>
176 comprobaciones, 0 fallos
self-test correcto (sin portapapeles ni red)
```

- Binario: `build/oneintwo-files.exe` (Release x64, aproximadamente 368 KB).
- Sin avisos del compilador en la última compilación completa.
- El temporal del self-test se crea bajo el directorio actual y se elimina al
  terminar correctamente.
- `--fingerprint` sobre el certificado público sintético devolvió
  `5b9e9ce5…b9d8`, idéntico al SHA-256 calculado con
  `openssl x509 -outform DER | openssl dgst -sha256`.

Comprobaciones de línea de comandos ejecutadas además del self-test:

| Comando | Resultado observado |
| --- | --- |
| sin argumentos / `--help` | solo ayuda, código 0 |
| `--make-fixtures test-data` | 6 elementos, 5 archivos, 5 551 150 bytes |
| `--inspect test-data` | manifiesto con archivo vacío, carpeta vacía y nombre Unicode correctos |
| `--fingerprint CERT.pem` | huella SHA-256 de un PEM público; coincide con OpenSSL |

`test-data` era material desechable y se eliminó después.

## 3. Qué cubren los self-tests

Sin escritorio, sin red y sin portapapeles (176 comprobaciones):

- SHA-256: vectores conocidos, texto vacío y cálculo incremental.
- Rutas: Unicode aceptado; `..`, rutas absolutas, componentes vacíos, caracteres
  de control, nombres reservados de Windows, punto/espacio final, profundidad y
  longitud.
- Manifiesto: archivo vacío, carpeta vacía, anidamiento, Unicode, límite de
  elementos, límite de profundidad, nombres de primer nivel repetidos.
- Manifiesto serializado: ida y vuelta, rechazo de rutas manipuladas e
  identificadores inválidos, límite de elementos.
- Autorización: lectura solo por identificador ofrecido; identificadores
  desconocidos, de otra oferta, carpetas y desplazamientos fuera de rango.
- Recepción: bloques de 64 KiB, tamaño y digest correctos, truncado, digest
  alterado, escritura fuera de orden, exceso de bytes, ruta de escape.
- Destino: colisión de archivo y de carpeta, originales intactos, carpetas no
  fusionadas, destino inexistente.
- Caché: publicación, manifiesto, listado, búsqueda, concesión de uso, gc por
  edad, limpieza de recepciones abandonadas, identificador de oferta inválido.
- Tramas: ida y vuelta, longitud excesiva, tipo desconocido, cuerpo truncado.
- Certificado: huella SHA-256 de un PEM sintético embebido, longitud correcta y
  rechazo de un PEM inválido.
- Protocolo en memoria: manifiesto, descarga por bloques con digest, lectura por
  rangos para bajo demanda, fin de archivo, cancelación en curso y rechazo de una
  petición de un identificador ajeno.
- Limpieza de `.o2f-part` huérfanos.

## 4. Qué NO está comprobado (no afirmar lo contrario)

- **Pegado nativo**: no se ha publicado nada en el portapapeles real ni se ha
  pegado en Explorer o Nautilus.
- **Explorer/OLE**: no se ha comprobado que acepte `FileGroupDescriptorW` +
  `FileContents`, ni el bloqueo del hilo STA mientras llega el contenido, ni los
  tiempos de espera percibidos.
- **Nautilus/GTK/Wayland**: el adaptador Linux no se ha compilado siquiera en
  esta sesión; su sintaxis y comportamiento están sin verificar.
- **TLS mutuo entre equipos**: el código se compiló con OpenSSL 3.0.5, pero no se
  abrió ninguna escucha ni se realizó ningún handshake. El certificado de
  cliente, el rechazo de huellas y los timeouts no están ejercitados en vivo.
- **Transferencia real por red**: el protocolo solo se probó sobre un par de
  flujos en memoria.
- **Disco lleno**: hay camino de error, pero no se provocó la condición.
- **Destino sin permiso de escritura**: cubierto por el manual, no ejecutado.
- **Cortar/mover, reanudación, ACL/permisos POSIX, enlaces simbólicos**: fuera de
  alcance o rechazados explícitamente.
- **Múltiples clientes simultáneos**: el servidor atiende una sesión cada vez.

Cualquier fila de la matriz manual que no se haya observado queda «Pendiente»,
aunque el self-test esté en verde.

## 5. Decisiones del prototipo

1. **A (caché) es la referencia funcional.** Se descarga, se verifica por archivo
   (tamaño y SHA-256) en un directorio privado de recepción y solo entonces se
   publica la oferta. El pegado entrega archivos locales, que es lo que ambos
   exploradores consumen de forma documentada.
2. **B (bajo demanda) se limita a Windows.** Se expone el manifiesto como
   `FileGroupDescriptorW` y cada archivo como `IStream` bajo `FileContents`; el
   stream pide rangos al origen cuando Explorer los lee. No se construye un
   proveedor de sistema de archivos.
3. **Linux no implementa B por alcance, no por imposibilidad demostrada.** El
   adaptador publica únicamente rutas `file://` locales. Nautilus deserializa
   `x-special/gnome-copied-files` aceptando cualquier URI válida y GVfs extiende
   GIO con backends D-Bus, sin exigir FUSE; una entrega perezosa sería posible
   con un backend GIO/GVfs propio o con un montaje FUSE, ambos fuera del encargo.
   El modo devuelve un error explicativo y no se presenta como conclusión técnica.
4. **El contenido nunca viaja por rutas absolutas.** El receptor recibe un
   manifiesto con rutas relativas validadas e identificadores opacos; solo puede
   pedir identificadores de la oferta vigente. Los identificadores usados como
   nombre de carpeta se restringen a alfanuméricos, `-` y `_`.
5. **Separación de responsabilidades.**
   - `core/manifest` + `core/store`: qué se ofrece y con qué límites.
   - `core/transfer`: recepción por bloques, `.o2f-part`, digest y colocación sin
     sobrescritura.
   - `core/cache`: ofertas verificadas, concesiones y limpieza.
   - `core/wire` + `core/remote`: protocolo mínimo y autorización por elemento.
   - `core/net`: TCP y TLS con fijado de huella.
   - `platform/*/clipboard_*`: única capa que conoce los formatos nativos.
   El motor de transferencia no conoce Explorer ni GTK; el adaptador no conoce el
   protocolo de red.
6. **TLS mutuo obligatorio en modo remoto**: certificados autofirmados y ambas
   huellas SHA-256 comparadas fuera de banda. El cliente fija la del servidor
   (`--pin`) y el servidor exige y fija la del cliente (`--client-pin`). Sin
   OpenSSL, el modo remoto se bloquea; no hay caída silenciosa a texto plano.
7. **Límites explícitos**: 4096 elementos, profundidad 32, 8 GiB por oferta,
   bloques de 256 KiB, 259 caracteres por ruta en el descriptor de Windows.
8. **Vida de la oferta**: la publicación bloquea hasta Ctrl+C. En Windows, la
   caché se materializa con `OleFlushClipboard` y las rutas siguen disponibles
   tras cerrar; Linux retira la selección al salir; la entrega bajo demanda
   desaparece con el proceso porque no hay copia que materializar.

### 5.1 Evidencia sobre Nautilus, GVfs y FUSE

La afirmación «Nautilus exige FUSE» no se sostiene con las fuentes consultadas:

- `nautilus-clipboard.c` registra `x-special/gnome-copied-files` y
  `GDK_TYPE_FILE_LIST`; al deserializar, valida cada línea con `g_uri_is_valid` y
  construye un `NautilusFile` con `nautilus_file_get_by_uri`. No filtra a
  `file://`: acepta cualquier URI válida.
  <https://gitlab.gnome.org/GNOME/nautilus/-/raw/main/src/nautilus-clipboard.c>
- La documentación de GVfs describe GIO como punto de extensión y los backends
  como procesos D-Bus; el daemon FUSE es una pieza aparte, desactivable con
  `GVFS_DISABLE_FUSE`. Un esquema propio puede servirse por GIO/GVfs sin FUSE.
  <https://wiki.gnome.org/Projects/gvfs/doc>
- El portal `org.freedesktop.portal.FileTransfer` exporta descriptores de archivo
  ya existentes al almacén de documentos; no permite entregar contenido
  perezoso. <https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.FileTransfer.html>

Conclusión: B en Linux no está implementado, no es técnicamente imposible. Esta
entrega publica solo rutas `file://` locales. Una entrega perezosa requeriría un
backend GIO/GVfs para un esquema propio (D-Bus) o un montaje FUSE que materialice
los archivos; ambos quedan fuera del alcance del experimento.

## 6. Dependencias y procedencia

| Dependencia | Versión usada o prevista | Procedencia y licencia | Uso |
| --- | --- | --- | --- |
| OpenSSL | 3.0.5 (Windows) | openssl.org — Apache-2.0 | TLS del modo remoto |
| GTK3 | 3.x (Ubuntu, `libgtk-3-dev`) | GNOME — LGPL-2.1-or-later | portapapeles en Linux |
| Win32 / OLE / Shell | Windows SDK 10.0.26100 | Sistema Windows | `CF_HDROP`, `IDataObject`, `IStream` |
| Biblioteca estándar C++17 | MSVC 14.51 / libstdc++ | implementación del estándar | archivos, hilos, contenedores |
| SHA-256 | implementación propia | FIPS 180-4, código original | integridad de transferencias |

No se ha incorporado código de aplicaciones de compartición ni motores de terceros.
`std::filesystem` se usa con rutas UTF-8 (`u8path`) para conservar nombres Unicode.

## 7. Límites y trabajo pendiente

1. **Ubuntu sin compilar** por falta de host en esta sesión. Riesgo concreto:
   errores de compilación o de API GTK3 no detectados. El manual indica cómo
   compilar y qué dependencia instalar.
2. **B en Linux no implementado**: no es una conclusión de inviabilidad. Requeriría
   un backend GIO/GVfs por D-Bus o un montaje FUSE (evidencia en §5.1), fuera del
   alcance de esta entrega.
3. **B en Windows sin validar**: `GetData` es síncrono y se ejecuta en el hilo
   STA; mientras llega el contenido, Explorer puede quedar bloqueado. No hay
   cancelación desde el explorador, solo el timeout del socket. Medir en T3.
4. **Un cliente a la vez** y sin reanudación: un corte obliga a repetir la
   transferencia. El receptor no publica ofertas parciales.
5. **Sin prueba de disco lleno** ni de cuota de caché: `--cache-gc` limpia por
   edad, no por espacio.
6. **Sin CA ni confianza persistente**: la huella se pasa en cada conexión. Para
   el producto habría que decidir almacenamiento de identidad y revocación.
7. **Metadatos**: no se conservan permisos POSIX, ACL, fechas ni atributos; solo
   nombre, tipo y contenido.
8. **Cambios comunes deseables** (fuera de mi directorio, no realizados): ninguna
   solicitud. El experimento es autocontenido.

## 8. Conclusión provisional

- Se puede afirmar: el motor de oferta, transferencia por bloques, verificación
  SHA-256, caché con concesiones, resolución de colisiones, validación de rutas y
  protocolo autorizado por identificadores está implementado y pasa 176
  comprobaciones aisladas en Windows.
- No se puede afirmar todavía: que Explorer o Nautilus peguen los archivos, que
  el descriptor OLE funcione en esta versión de Windows, ni que la entrega bajo
  demanda sea utilizable con red real.
- La autenticación mutua está implementada y compila, pero no se ha ejecutado
  ningún handshake: no se afirma que acepte o rechace certificados en vivo.
- **Recomendación para el producto**: adoptar A como estrategia base, con estado
  «preparado» visible y una carpeta de caché privada con cuota y limpieza. Tratar
  B como optimización opcional exclusiva de Windows, solo si T3 demuestra pegado
  correcto, bloqueo acotado y errores claros. No introducir un sistema de
  archivos virtual para Linux en esta fase.
- Responsabilidad propuesta de adaptadores: `file_clipboard` posee los formatos
  nativos y su ciclo de vida; `files` posee ofertas, identificadores, bloques,
  integridad y caché. El adaptador no debe decidir la confianza ni el protocolo.

## 9. Matriz manual (pendiente de ejecución del usuario)

| Caso | Entorno exacto | Esperado | Observado | Resultado |
| --- | --- | --- | --- | --- |
| L1 self-test |  | 0 fallos | 176/0 en Windows 10 19045 | Superado (aislado) |
| L2 caché Explorer |  | pegado completo y hash igual | No ejecutado | Pendiente |
| L3 bajo demanda Windows |  | pegado completo o bloqueo documentado | No ejecutado | Pendiente |
| L4 caché Nautilus |  | pegado completo y hash igual | No ejecutado | Pendiente |
| L5 bajo demanda Linux |  | error explícito: limitación de implementación, no inviabilidad | No ejecutado | Pendiente |
| L6 colisiones / sin escritura |  | sufijos y error claro | No ejecutado | Pendiente |
| L7 cancelación |  | sin oferta publicada ni temporales | No ejecutado | Pendiente |
| L8 caché y gc |  | ofertas eliminadas por edad | No ejecutado | Pendiente |
| T1 Windows→Ubuntu |  | pegado completo, hash igual | No ejecutado | Pendiente |
| T2 Ubuntu→Windows |  | pegado completo, hash igual | No ejecutado | Pendiente |
| T3 Windows←Ubuntu bajo demanda |  | pegado completo; corte con error | No ejecutado | Pendiente |
| T0 TLS mutuo negativo |  | rechazo de huella de cliente o servidor | No ejecutado | Pendiente |

La comparación de tiempos, espacio y bloqueo percibido entre A y B está definida
en `MANUAL.md` §8 y queda vacía hasta la ejecución manual.
