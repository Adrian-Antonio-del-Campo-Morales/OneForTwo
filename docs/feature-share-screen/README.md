# Feature `share-screen`: sesión, emparejamiento y pantalla compartida

Este documento describe la entrega de la rama `feature/share-screen`, que en la
[implementación paralela](../IMPLEMENTACION_PARALELA.md) corresponde al paquete
**P**. No es el README general del producto ni una declaración de que la
aplicación completa esté terminada.

Estado de referencia: rama `feature/share-screen`, commit `3efa340`, suite local
**10/10** y workflow Windows/MSVC en verde. La entrega contiene la base común de
sesión, identidad, emparejamiento, protocolo, transporte TLS, composición y
módulo visual. La captura nativa de escritorio y las notificaciones pertenecen a
las ramas V y N, respectivamente.

## Capacidades incluidas

| Capacidad | Estado en esta rama |
| --- | --- |
| Identidad TLS persistente por equipo | Implementada y probada |
| Emparejamiento con código SAS de seis cifras | Implementado y probado |
| Reemparejamiento y rotación de certificado | Implementados y probados |
| Almacén de pares con pin principal y respaldo | Implementado y probado |
| Transporte TLS con huella SHA-256 fijada | Implementado y probado |
| Sesión autenticada con generaciones y permisos | Implementada y probada |
| Protocolo de funciones versión 1 | Implementado y probado |
| Módulo visual: invitación, decisión, estado, imágenes | Implementado y probado con captura sintética |
| Visor de pantalla y composición Qt | Implementados y probados |
| Diálogo de emparejamiento y ventana principal | Vinculados en la aplicación de pruebas |
| Captura nativa de monitor/ventana | Pendiente de integrar desde V |
| Notificaciones nativas | Pendientes de integrar desde N |
| Ejecutable completo de producto | Omitido en esta rama |
| Descubrimiento automático de equipos | No implementado |
| Audio, grabación, control remoto de entrada | Fuera del alcance de esta entrega |
| Instalador, firma y licencia | Pendientes de decisión de producto |

### Emparejamiento

- Cada equipo genera una identidad RSA de 3072 bits y un certificado
  autofirmado de diez años mediante libcrypto. La identidad se identifica por un
  `device_id` hexadecimal de 32 caracteres y por la huella SHA-256 de su
  certificado.
- El diálogo permite escuchar o conectarse manualmente a un host y puerto. No
  hay descubrimiento de red.
- Durante la ceremonia se usa TLS con certificado no verificado y se calcula una
  SAS de seis cifras a partir del transcript. Ambas personas deben comparar la
  misma SAS y confirmar. Hasta entonces no se confía en el certificado.
- La huella observada se guarda al completar la ceremonia. Las conexiones
  posteriores usan TLS con el pin fijado y rechazan cualquier otro certificado.
- Si se vuelve a emparejar un equipo existente, se actualizan nombre, host y
  puerto y el pin nuevo queda como principal. El pin anterior se conserva como
  respaldo transitorio.
- La rotación explícita del certificado de este equipo genera una identidad
  nueva y obliga a volver a emparejar. No se reutiliza la clave anterior.

### Sesión y protocolo

- La sesión se establece sobre un `QtTlsTransport` ya autenticado. El saludo
  intercambia identidad y generación; la recepción rechaza generaciones
  repetidas o antiguas y cada conexión recibe una generación local nueva.
- La sesión aplica permisos por sentido y por modalidad. Los perfiles `usual`,
  `sends_only` y `paused` limitan las capacidades efectivas; un perfil guardado
  inválido se interpreta como pausa.
- El canal interactivo transporta control y mensajes pequeños. El canal de datos
  transporta unidades visuales fragmentadas. Las tramas tienen canal, longitud y
  payload; el receptor valida versión, tipo, longitud y canal antes de despachar.
- El protocolo de funciones es la versión 1. Los mensajes visuales incluyen
  referencia de operación, revisión de fuente, secuencia y metadatos de imagen.
  No hay negociación de versiones futuras ni compatibilidad hacia atrás.
- Los identificadores de operación empiezan en 1 por generación y sentido. Un
  desbordamiento termina la sesión antes de reutilizar un identificador.

### Compartir pantalla

- El módulo visual mantiene una sola operación visual por sesión, contando
  invitaciones pendientes y operaciones activas en ambos sentidos.
- Una invitación caduca a los 30 segundos. La fuente puede ser monitor o
  ventana; el adaptador nativo decide qué fuentes admite realmente.
- El receptor debe aceptar. La captura no empieza antes de la aceptación y la
  operación se invalida si cambia la generación, se retira el permiso o se
  pierde la fuente.
- Las imágenes se codifican como JPEG independientes, con calidad inicial 75 y
  reducción de calidad/resolución si hace falta. La primera entrega no usa
  vídeo interframe.
- Límites implementados: 30 fps, 1920 píxeles por lado, 2.073.600 píxeles por
  imagen y 1 MiB por unidad codificada. El receptor valida las dimensiones antes
  de reservar la imagen decodificada.
- El visor vacía la presentación al suspender o cerrar la fuente. La operación
  termina si se pierden las señales de actividad; el emisor envía actividad cada
  segundo y el receptor tolera tres segundos sin señales.
- El cierre local o remoto invalida la operación, cancela trabajos y descarta
  callbacks tardíos. Un cierre remoto no genera otro cierre de respuesta.

## Detalles de implementación

### Identidad y almacenamiento

`DeviceIdentityStore` guarda la identidad en el directorio configurado:

```text
identity/
├── identity.id    # device_id hexadecimal
├── identity.crt   # certificado PEM
└── identity.key   # clave privada PEM
```

En POSIX el directorio se crea con permisos de propietario y la clave se escribe
con lectura/escritura solo para el propietario. La identidad se genera con
libcrypto, no invocando el binario `openssl`.

`PairingStore` guarda los pares en un TSV con cabecera
`oneintwo-pairing-v1`:

```text
device_id <TAB> nombre <TAB> host <TAB> puerto <TAB> pin_principal [<TAB> pin_respaldo]
```

- Un par puede tener una o dos huellas; el principal siempre va primero.
- Las escrituras usan `QSaveFile` (temporal único y reemplazo atómico) y, en
  POSIX, `fsync` del archivo y del directorio. `save`, `remove`, `rotatePin` y
  `retireFallback` publican el estado en memoria solo después de persistirlo.
- `reload` conserva el último estado válido si el archivo está corrupto. Si la
  corrupción se detecta al construir la aplicación, esta muestra el error y
  deshabilita el almacén de pares; no hay recuperación automática.
- El respaldo se retira después de autenticar por TLS la huella principal. No se
  usa como fallback automático de conexión: la aplicación siempre intenta el pin
  principal, y el respaldo existe para la transición de reemparejamiento.
- `pinTrusted()` acepta principal y respaldo por compatibilidad. Para decisiones
  de autorización que exijan la identidad activa debe usarse
  `primaryPinTrusted()` o `pinRole()`.

`LocalStore` es distinto: guarda perfiles y aplicaciones permitidas y todavía
usa un temporal fijo con `std::filesystem::rename`. No tiene las mismas garantías
de transacción y durabilidad que `PairingStore`; no conviene asumir que ambas
persistencias se comportan igual.

### Transporte TLS

- Qt `QSslServer`/`QSslSocket` con TLS 1.2 o posterior. La confianza se basa
  exclusivamente en el SHA-256 del certificado del par; no hay CA, nombre de
  host ni validación de cadena.
- En modo normal la huella es obligatoria. El servidor también fija la huella
  del cliente. El modo `allow_unverified` está reservado a la ceremonia SAS.
- Un transporte admite una sola conexión activa. El servidor cierra el listener
  después de aceptar una conexión y el handshake tiene un plazo de 10 segundos.
- Los buffers y tramas están acotados. Los errores de TLS y de identidad no
  exponen la huella observada como si fuera una identidad autenticada.

### Composición de la aplicación

- `Application` carga identidad y pares, construye la ventana, el coordinador y
  la sesión, y reutiliza el último par guardado al arrancar.
- `Coordinator` conecta la UI con la sesión, el módulo visual y los avisos. Las
  acciones de compartición solo se autorizan con sesión establecida y permiso
  efectivo.
- `SessionLink` se encarga del saludo de identidad/generación y de la
  autorización de mensajes. El perfil se aplica en la sesión, no en la UI.
- `ModuleRuntime::post` encola trabajo en el hilo principal de Qt. Los callbacks
  de UI, sesión y módulos deben ejecutarse en ese contexto; no se deben bloquear
  con operaciones de red o disco.

### Construcción y pruebas

Requisitos: CMake 3.24 o posterior, compilador C++17, Qt 6.4 o posterior con
Core/Gui/Widgets/Network y OpenSSL Crypto. En Windows el workflow usa Qt 6.8.3
(`qtbase`) y Visual Studio 2022 x64.

```bash
cmake -S . -B build/root \
  -DONEINTWO_BUILD_TESTS=ON \
  -DCMAKE_PREFIX_PATH=/ruta/a/Qt/6.x/gcc_64
cmake --build build/root -j 4
QT_QPA_PLATFORM=offscreen \
  ctest --test-dir build/root --output-on-failure
```

La suite raíz contiene diez pruebas CTest:

```text
oneintwo_protocol
oneintwo_session_link
oneintwo_tls_transport
oneintwo_pairing
oneintwo_visual_share
oneintwo_visual_codec
oneintwo_visual_viewer
oneintwo_composition_harness
oneintwo_pairing_ui
oneintwo_application
```

El workflow [windows-msvc.yml](../../.github/workflows/windows-msvc.yml) instala
Qt, configura el proyecto y compila `oneintwo_application_tests`. Después exige
que CTest registre exactamente `oneintwo_application` y que la prueba pase. Es
una validación del enlace y arranque de Application en Windows, no de la captura
nativa.

## Mapa de archivos

| Archivo | Responsabilidad |
| --- | --- |
| `src/net/device_identity.*` | Generación, carga y rotación de la identidad TLS |
| `src/net/pairing_session.*` | Ceremonia SAS y transición a confianza persistente |
| `src/net/tls_transport.*` | Transporte TLS con pin fijado y framing de canales |
| `src/net/session_link.*` | Saludo, generaciones, permisos y despacho autorizado |
| `src/net/protocol.*` | Codificación/decodificación de mensajes de funciones |
| `src/storage/pairing_store.*` | Pares, pins principal/respaldo y persistencia atómica |
| `src/storage/local_store.*` | Preferencias de perfil y aplicaciones permitidas |
| `src/features/visual_share.*` | Estado y reglas de la operación visual |
| `src/features/visual/visual_codec.*` | Escalado, JPEG y validación de imágenes |
| `src/ui/visual_viewer.*` | Presentación de la imagen compartida |
| `src/ui/pairing_dialog.*` | Diálogo de emparejamiento y rotación |
| `src/app/application.*` | Composición real de UI, sesión y almacenamiento |
| `src/app/coordinator.*` | Conexión entre UI, sesión y módulos |
| `src/platform/desktop.hpp` | Contrato de captura y notificaciones nativas |
| `tests/*` | Pruebas de protocolo, TLS, pares, UI, composición y Application |

## Problemas conocidos y factores importantes

### La rama no es el producto completo

El ejecutable `oneintwo` solo se crea si existen a la vez
`oneintwo_notifications` y un target de captura nativo. En esta rama esos
targets no forman parte de la composición; la aplicación se prueba con dobles de
captura y avisos. Por tanto:

- La suite verifica contratos, protocolo, TLS, composición y arranque, no una
  captura real de escritorio.
- No se debe afirmar que compartir pantalla funciona de extremo a extremo hasta
  integrar V y N y probar ambos sistemas operativos.
- Los stubs de `tests/application/application_test.cpp` no son una implementación
  de producto y no deben enlazarse en el ejecutable.

### Captura y notificaciones pertenecen a otras ramas

La propiedad de los adaptadores nativos está definida en
[la base paralela](../IMPLEMENTACION_PARALELA.md). Al integrar:

- V aporta la captura Windows/Wayland y sus pruebas nativas.
- N aporta notificaciones y sus adaptadores.
- P conserva la composición, el contrato visual, el protocolo y la sesión.
- No se deben duplicar transportes ni protocolos para “hacer funcionar” una
  rama aislada; la integración debe usar las cabeceras comunes.

### No hay descubrimiento ni conexión automática

El emparejamiento requiere host y puerto. La aplicación no busca peers en la
LAN, no resuelve NAT ni mantiene reconexión automática. Si el peer cambia de
dirección, hay que actualizar el par.

### La generación del par no se persiste entre procesos

`SessionLink` acepta callbacks para recordar la última generación del peer, pero
`Application` no los conecta a un almacén. La protección contra repetición de
generaciones es efectiva dentro del proceso; tras reiniciar la aplicación, el
contador local se reinicia y el peer no dispone de una marca persistida. Es una
mejora pendiente si se requiere resistencia a replay entre reinicios.

### El respaldo no es un failover automático

- La aplicación usa siempre el pin principal para conectarse.
- El respaldo se retiene solo para la ventana de reemparejamiento y se retira
  cuando una conexión TLS autentica el principal.
- Si la escritura del almacén falla al retirar el respaldo, la sesión continúa y
  el respaldo puede permanecer; se muestra el error, pero no se bloquea la
  conexión.
- `pinTrusted()` sigue aceptando ambos pines por compatibilidad. No usarlo para
  decidir la identidad activa; usar `primaryPinTrusted()`.

### Persistencia y concurrencia

- `PairingStore` está pensado para un único escritor y no tiene mutex. No se debe
  escribir el mismo archivo desde varios procesos o hilos a la vez.
- `QSaveFile` evita temporales compartidos y reemplazos parciales, pero no
  implementa un bloqueo entre procesos ni resuelve escrituras perdidas si dos
  instancias cargan y guardan a la vez.
- `LocalStore` todavía usa un temporal fijo y no tiene las garantías de
  `PairingStore`.
- Si el almacén de pares está corrupto al arrancar, `Application` lo deshabilita
  y muestra el error. `reload()` sobre una instancia ya cargada conserva el
  último estado válido, pero la recuperación tras un arranque corrupto es manual.

### Seguridad

- El pin es la raíz de confianza. No hay CA ni nombre de host; agregar validación
  de host sin rediseñar la rotación puede romper el modelo.
- `allow_unverified` solo es válido dentro de la ceremonia SAS. Nunca debe
  habilitarse para una sesión normal ni para evitar un error de pin.
- Cambiar el certificado obliga a reemparejar; no se debe reutilizar una clave
  privada antigua.
- Los certificados y claves de `tests/tls/test_certificates.hpp` son material de
  prueba. No deben usarse en una instalación real.
- La clave privada debe quedar en el directorio de aplicación del usuario, con
  permisos restringidos. No registrar claves, contenido de pantalla ni datos
  sensibles.

### Límites del protocolo

- La versión 1 es la única soportada. Cambiar tipos, campos o límites sin subir
  la versión rompe la compatibilidad.
- El canal interactivo y el de datos tienen límites estrictos. Validar longitudes
  antes de reservar memoria y no ampliar buffers sin revisar el receptor.
- Una operación visual usa JPEG de imágenes independientes; no hay audio ni
  grabación.
- Las invitaciones caducan y la sesión no reanuda automáticamente una operación
  tras una desconexión o un cambio de generación.

### Varios pares guardados

`PairingStore` admite varios pares, pero `Application` reutiliza
`devices().back()` al arrancar. Si se guardan varios equipos, el arranque no
ofrece una selección explícita; hay que revisar este comportamiento antes de
soportar múltiples pares en la interfaz de producto.

### Diferencias de plataforma y cobertura

- Linux se validó localmente con la suite completa; Windows/MSVC valida el
  enlace y el arranque de Application en CI. La captura nativa de cada plataforma
  requiere la evidencia de V.
- No hay soporte validado para macOS.
- La UI de pruebas usa `QT_QPA_PLATFORM=offscreen`; no sustituye una prueba de
  escritorio real.
- La licencia, el instalador y la firma siguen siendo decisiones de producto.

## Antes de modificar esta feature

1. Leer [la base de implementación paralela](../IMPLEMENTACION_PARALELA.md) y el
   [diseño de compartir pantalla](../COMPARTIR_PANTALLA.md).
2. Respetar la propiedad de archivos: P no debe absorber adaptadores nativos de V
   ni notificaciones de N.
3. Mantener las firmas públicas y los límites de protocolo; cualquier cambio de
   contrato debe coordinarse con el integrador.
4. Añadir pruebas de contrato o de sesión en la suite correspondiente. Para
   cambios de UI, ejecutar con `QT_QPA_PLATFORM=offscreen` y no confundir esa
   ejecución con una validación visual.
5. No añadir stubs al ejecutable de producto. Los dobles pertenecen a `tests/`.
6. Verificar el workflow Windows cuando el cambio afecte al enlace de
   Application.

## Limitaciones de esta entrega

Esta rama deja la base P lista para integrarse, pero no cierra la funcionalidad
de producto: faltan captura nativa, notificaciones, ejecutable completo,
descubrimiento, selección de múltiples pares y pruebas de escritorio entre
equipos. La evidencia actual es la suite local y el enlace/arranque de
Application en Windows.
