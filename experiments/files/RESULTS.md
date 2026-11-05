# Resultados — Archivos

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
