# Captura visual nativa en Windows

Este README describe el backend Windows de la feature `feature/share-window`, que
vive en este directorio. La feature completa comparte el contrato
[`DesktopVisualCapture`](../../desktop.hpp) con el backend Wayland
([`../linux/visual`](../../linux/visual/)); aquí se documenta la implementación
Windows, sus límites y lo que el resto de componentes debe asumir.

Estado de esta rama:

- selección local de monitores y ventanas;
- captura con Windows Graphics Capture (WGC) y D3D11;
- entrega de imágenes BGRA al contrato común;
- estados de disponibilidad, suspensión y cierre;
- pruebas portables con puertos falsos, pruebas del núcleo WGC y un smoke test
  Win32/WGC real;
- CI `Windows MSVC` con SDK 19041 y con el SDK actual.

La integración de producto (módulo `VisualShare`, codec, transporte, permisos y
visor) pertenece al coordinador y no está implementada en este paquete. Este
backend no codifica, no cifra, no envía imágenes y no abre ninguna ventana de
visor por sí solo.

## Capacidades

| Capacidad | Estado | Observaciones |
| --- | --- | --- |
| Capturar un monitor | Sí | Selección por `HMONITOR`; se conserva la identidad del monitor. |
| Capturar una ventana | Sí | Selección por `HWND`; no se vuelve a buscar la ventana por título. |
| Selector local | Sí | Diálogo Win32 modal con lista de fuentes y botones Compartir/Cancelar. |
| Excluir la propia aplicación | Sí | `EnumWindows` descarta las ventanas del proceso actual. |
| Imagen BGRA de 8 bits | Sí | Filas contiguas, de arriba hacia abajo, alpha opaco. |
| Adaptarse a cambios de tamaño | Sí | Se recrea el frame pool de WGC y se descarta el frame de transición. |
| Suspensión por minimización | Sí | Una ventana minimizada pasa a `suspended`; al restaurar vuelve a `available`. |
| Cierre de la fuente | Sí | Ventana cerrada o monitor perdido producen `closed`. |
| Ocultar el cursor | Mejor esfuerzo | Sólo si el runtime expone `IGraphicsCaptureSession2`; un fallo de la propiedad no cancela la captura. |
| Capturar audio, región o pantalla virtual | No | No hay recorte de región ni sustitución automática por monitor. |
| Codificar, transportar o presentar | No | El backend entrega `VisualFrame` en memoria; el resto lo hace el coordinador. |
| Garantizar contenido protegido o HDR | No | El sistema puede devolver negro o un frame no convertible; no se fuerza un resultado. |

### Límites del contrato de imagen

`VisualFrame` contiene `size` y un `std::vector<std::uint8_t>` con exactamente
`width * height * 4` bytes. El backend:

- exige dimensiones mayores que cero;
- limita cada lado a 16384 píxeles y el total a 64 Mi píxeles en la validación
  interna;
- no entrega formatos distintos de `DXGI_FORMAT_B8G8R8A8_UNORM`;
- compacta el `RowPitch` de la textura de staging en filas contiguas;
- fuerza el canal alpha a `0xFF` antes de publicar la imagen.

Los límites de salida del producto (1920 píxeles por lado, 2.073.600 píxeles y
30 fps) son responsabilidad del módulo común. Este backend no escala ni limita
la frecuencia por sí mismo.

## Flujo de uso

```text
selectSource(offer, kind)
        |
        v
enumerate(kind) -> selector Win32 -> adopt(NativeSource)
        |                                   |
        |                              CreateForWindow/Monitor
        |                                   |
        +---- resultado terminal -----------+
        |
startCapture(offer, callbacks)
        |
        v
WGC frame pool -> hilo de captura -> copia D3D11 a BGRA
        |
        v
ModuleRuntime.post -> CaptureModel -> callbacks.frame / callbacks.state_changed
```

1. El coordinador llama a `selectSource` con una referencia de operación y el
   tipo de fuente (`monitor` o `window`).
2. El adaptador comprueba capacidades, enumera fuentes y abre el selector.
3. El usuario elige una fuente o cancela. La selección produce una única
   respuesta terminal: `ready`, `cancelled`, `unavailable` o `error`.
4. Tras la aceptación de la operación, el coordinador llama a `startCapture`.
   WGC crea la sesión y comienza a entregar frames.
5. `stopCapture` invalida la operación, detiene la captura y descarta callbacks
   tardíos. Es idempotente.

### Resultados de selección

| Situación | Resultado |
| --- | --- |
| Fuente elegida y objeto de captura creado | `ready` |
| Usuario cancela o cierra el selector | `cancelled` |
| Ya hay una selección o captura activa para otra referencia | `error` |
| Tipo no soportado, lista vacía o no se puede crear el objeto de captura | `unavailable` |
| No se puede crear la ventana del selector | `cancelled` |

Windows no muestra un diálogo de consentimiento del sistema para WGC: la
autorización funcional y la selección local son la barrera. El adaptador no
devuelve `denied` en el flujo actual; ese resultado queda para plataformas o
permisos que sí tengan una decisión explícita del sistema.

## Contrato público

La implementación satisface [`DesktopVisualCapture`](../../desktop.hpp):

| Operación | Responsabilidad |
| --- | --- |
| `visualCapabilities()` | Indica si WGC y la interoperabilidad están disponibles. Es una comprobación de capacidad, no una garantía de que una fuente concreta se pueda capturar. |
| `selectSource()` | Enumera, pregunta y adopta una fuente. Retiene una sola selección o captura por adaptador. |
| `startCapture()` | Arranca la adquisición para la referencia ya seleccionada. Si la referencia no coincide, publica `closed`. |
| `stopCapture()` | Cancela una selección pendiente o libera la captura activa. No vuelve a entregar frames de esa referencia. |

Los callbacks también forman parte del contrato:

- `frame` entrega `VisualFrame` por valor;
- `state_changed` entrega `available`, `suspended` o `closed`;
- los callbacks se publican mediante `ModuleRuntime.post`, nunca se ejecutan
  dentro del hilo nativo del frame pool;
- el consumidor debe tratarlos como serializados en el hilo del módulo y no
  debe bloquear ni lanzar excepciones desde ellos.

### Estados

| Estado | Significado |
| --- | --- |
| `available` | La fuente sigue siendo la seleccionada y puede entregar contenido. No se garantiza que haya un frame en cada instante. |
| `suspended` | La ventana está minimizada; se dejan de publicar frames hasta que se restaure. |
| `closed` | La ventana desapareció, cambió de proceso, el monitor ya no coincide o WGC falló. La operación debe terminar. |

No hay un estado `available` inicial garantizado antes del primer frame. La
primera imagen puede ser la primera señal de contenido; el coordinador no debe
quedarse esperando un `available` inicial. Tampoco hay un estado de error con
detalle: un fallo de adquisición se representa como `closed` y el coordinador
decide cómo informar al usuario.

## Implementación

### Archivos

| Archivo | Papel |
| --- | --- |
| [`windows_capture_factory.cpp`](windows_capture_factory.cpp) | Elige el puerto WGC real o el puerto de indisponibilidad según las cabeceras. |
| [`capture_model.cpp`](capture_model.cpp) | Modelo portable: selección, una operación activa, validación BGRA, orden de estados y descarte de callbacks tardíos. |
| [`win32_capture_port.hpp`](win32_capture_port.hpp) | Puerto entre el modelo y la plataforma: enumerar, preguntar, adoptar y arrancar/parar. |
| [`win32_selector.cpp`](win32_selector.cpp) | Enumeración Win32, diálogo local y creación del objeto de captura. |
| [`wgc_capture_core.cpp`](wgc_capture_core.cpp) | Bucle de captura independiente de WinRT: pool, frames, estados y parada. |
| [`wgc_capture.cpp`](wgc_capture.cpp) | Adaptadores reales de WinRT/D3D11: `GraphicsCaptureItem`, frame pool, textura de staging y copia BGRA. |
| [`unavailable_port.cpp`](unavailable_port.cpp) | Puerto que declara cero capacidades cuando WGC no está compilado. No simula captura. |
| [`win32_parent_qt.cpp`](win32_parent_qt.cpp) | Obtiene el `HWND` padre desde `PlatformContext.parent` si se compila con Qt. |
| [`win32_parent_default.cpp`](win32_parent_default.cpp) | Padre nulo cuando no hay Qt. |

### Selección y selector

La enumeración de ventanas usa `EnumWindows` y descarta:

- la ventana del selector;
- ventanas no visibles;
- ventanas sin título;
- ventanas con `WS_EX_TOOLWINDOW`;
- ventanas del proceso actual;
- ventanas "cloaked" de DWM.

Para cada ventana conserva `HWND`, título y tamaño calculado con
`DWMWA_EXTENDED_FRAME_BOUNDS` (con `GetWindowRect` como respaldo). El título y
el tamaño son **sólo para la selección local**: no se envían al par ni se usan
para volver a localizar la ventana.

Los monitores se enumeran con `EnumDisplayMonitors`. La etiqueta incluye el
nombre de dispositivo, el tamaño y si es el principal. La identidad retenida es
el `HMONITOR`; además se guarda el nombre de dispositivo para detectar que el
monitor ya no es el mismo.

El selector es una ventana Win32 propia con una lista y botones. Mientras está
abierto desactiva el `HWND` padre, si existe. Al cerrarse:

- entrega la fuente elegida o `std::nullopt`;
- reactiva el padre;
- garantiza una sola entrega por medio de `PromptState::delivered`.

`cancelPrompt()` publica `WM_CLOSE`; no destruye la ventana de forma síncrona.
El callback del selector se ejecuta en el hilo de UI y el modelo lo reenvía a
`ModuleRuntime.post`.

### WGC y D3D11

`createWin32Capture`:

1. comprueba `GraphicsCaptureSession::IsSupported()` y la fábrica de
   interoperabilidad;
2. crea un `GraphicsCaptureItem` con `CreateForWindow` o `CreateForMonitor`;
3. crea un `ID3D11Device` **hardware** con `D3D11_CREATE_DEVICE_BGRA_SUPPORT`;
4. convierte el dispositivo DXGI en `IDirect3DDevice` para WGC;
5. crea un frame pool libre de hilos (`CreateFreeThreaded`) con formato
   `B8G8R8A8UIntNormalized` y dos buffers;
6. entrega el `WgcCaptureCore` con un `WgcSource` que comprueba la identidad.

El bucle de `WgcCaptureCore`:

- comprueba disponibilidad y minimización cada 200 ms;
- no adquiere frames mientras está suspendido;
- usa `TryGetNextFrame` y espera 10 ms si no hay frame;
- si el tamaño de contenido cambia, cierra el frame de transición, recrea el
  pool y continúa con el siguiente frame;
- copia la textura a una textura de staging con CPU read access, recorre las
  filas respetando `RowPitch`, compacta a BGRA y fuerza alpha opaco;
- publica `closed` si el frame pool, la fuente o la copia fallan de forma
  irrecuperable.

La copia es una lectura completa a CPU. No hay camino cero-copia ni conversión
a cargo del backend.

### Hilos y propiedad

- Las entradas de `CaptureModel` deben ejecutarse en el hilo del módulo
  (`ModuleRuntime`), normalmente el hilo principal de Qt.
- `ModuleRuntime.post` debe encolar, no ejecutar inline. El modelo usa ese
  ejecutor para serializar frames, estados y resultados de selección.
- `WgcCaptureCore` crea un `std::thread` propio. Los callbacks del núcleo llegan
  desde ese hilo y el modelo los publica.
- `stop()` es síncrono: marca la parada, hace `join` del hilo y libera sesión,
  pool y fábrica. No debe llamarse desde el propio callback del hilo de captura.
- La captura no se reinicia después de `stop()`; para volver a compartir hace
  falta una selección y una invitación nuevas.
- El modelo usa un `epoch` y la referencia de operación para descartar frames y
  estados que lleguen después de una parada.

## Construcción

Requisitos:

- Windows 10/11 con una sesión gráfica interactiva para la captura real;
- Visual Studio 2022 con herramientas C++ y Windows SDK **10.0.19041** o
  posterior;
- CMake 3.24 o posterior;
- Qt 6 Widgets sólo si se quiere usar el padre del selector;
- C++17.

```powershell
cmake -S cmake/capture_windows -B build/capture_windows `
  -G "Visual Studio 17 2022" -A x64 `
  -DONEINTWO_BUILD_TESTS=ON `
  -DONEINTWO_CAPTURE_QT=OFF `
  -DONEINTWO_REQUIRE_WGC=ON

cmake --build build/capture_windows --config Release
ctest --test-dir build/capture_windows -C Release --output-on-failure
```

`ONEINTWO_REQUIRE_WGC=ON` hace que la configuración falle si faltan las
cabeceras de WGC, en lugar de construir silenciosamente el puerto de
indisponibilidad. Es la opción usada por CI.

En Linux se puede construir el modelo portable y sus pruebas, pero el target
nativo Windows no se compila. El modelo usa un puerto falso y no necesita WGC,
D3D11 ni un escritorio:

```bash
cmake -S cmake/capture_windows -B build/capture_windows \
  -DONEINTWO_BUILD_TESTS=ON -DONEINTWO_CAPTURE_QT=OFF
cmake --build build/capture_windows
ctest --test-dir build/capture_windows --output-on-failure
```

### SDK y CMake

- El SDK mínimo es **10.0.19041** porque el código usa
  `IGraphicsCaptureSession2`/`IsCursorCaptureEnabled` en las cabeceras.
- En runtime la propiedad del cursor se obtiene por QI y es opcional. Un
  Windows que soporte WGC pero no esa interfaz puede capturar, aunque el cursor
  puede aparecer en la imagen.
- La lane de SDK 19041 usa `/permissive` para `oneintwo_capture_windows` porque
  el cppwinrt incluido en ese SDK no compila con `/permissive-` y MSVC v143
  actual. No se debe aplicar `/permissive` globalmente ni cambiar la lane a
  `/permissive-` sin volver a probar ese SDK.
- El target del modelo conserva `/permissive-`; la excepción es sólo para el
  target nativo antiguo.
- `ONEINTWO_CAPTURE_QT=ON` busca Qt6 Widgets. Si no está disponible, se usa el
  padre nulo; CI lo desactiva para no depender de Qt.
- Los import libs incluyen `runtimeobject` y `windowsapp`. `runtimeobject.lib`
  aporta las importaciones de Windows Runtime, pero no existe un
  `runtimeobject.dll` que se deba cargar en el probe: `RoGetActivationFactory`
  se comprueba en `combase.dll`.
- `windowsapp.lib` es una umbrella library y `windowsapp.dll` puede no existir
  como DLL independiente; el probe lo trata como opcional. `GraphicsCapture.dll`
  también se comprueba de forma opcional y el veredicto final lo da
  `IsSupported` más la interoperabilidad.

### Pruebas

| Prueba | Qué cubre |
| --- | --- |
| `capture_windows` | Modelo portable con puerto falso: capacidades, selección, cancelación, frames BGRA, alpha, tamaños, orden de estados y callbacks tardíos. |
| `capture_windows_wgc` | Núcleo WGC con dobles: cursor opcional, orden de configuración, parada sin callbacks posteriores y recreación del pool. |
| `capture_windows_wgc_probe` | Probe con runtime falso: DLL/entry point ausente y WGC no soportado. |
| `capture_windows_wgc_probe_process` | Ejecución del probe falso como proceso y comprobación de códigos de salida. |
| `capture_windows_wgc_smoke` | Win32 + WGC real: crea una ventana, recibe al menos un frame BGRA válido, la cierra y comprueba que no hay callbacks después de `stop`. |
| `test_cmake_guards.ps1` | Guardas negativas de CMake: SDK anterior a 19041 y API de cursor ausente. |

El smoke test devuelve **77** y CTest lo marca como skip cuando no hay una
estación de ventanas visible o WGC no está soportado. Es el comportamiento
correcto en un runner sin escritorio interactivo, en RDP/headless o en una
sesión bloqueada; no debe interpretarse como una captura verificada. En un
equipo Windows real hay que ejecutarlo en una sesión desbloqueada para validar
el camino nativo.

El workflow `Windows MSVC` compila y ejecuta las pruebas en dos lanes: SDK
`10.0.19041.0` y SDK por defecto del runner. También construye el link check,
el probe real y el smoke. La rama no debe declarar compatibilidad de captura
sólo por tener CI verde: el smoke puede haberse omitido.

## Problemas y factores importantes

### 1. WGC no es una API de captura universal

`IsSupported()` puede ser cierto y la creación del objeto de captura fallar
igualmente por política, por el adaptador gráfico, por una sesión no
interactiva o por el estado del escritorio. `visualCapabilities()` es una
comprobación gruesa; el resultado real se conoce al adoptar y arrancar. Hay que
tratar `unavailable` y `closed` como resultados normales, no como errores
imposibles.

La captura de contenido protegido (DRM, algunas aplicaciones elevadas o
superficies del sistema) puede producir negro o fallar sin un estado distinto.
No hay detección de contenido protegido. UAC, el escritorio seguro, la pantalla
de bloqueo y algunas superficies del sistema no son capturables; el coordinador
debe detener la operación al bloquearse la sesión.

Windows puede dibujar un borde o un indicador de captura sobre la fuente. El
backend no controla esa decoración del sistema y no debe intentar ocultarla.

### 2. Requisitos de GPU y formato

El backend crea un dispositivo D3D11 **hardware**, sin fallback WARP. En una VM
sin GPU, en RDP, en un servidor headless o con un adaptador que no permite el
formato BGRA, `D3D11CreateDevice` o la creación del pool pueden fallar y la
fuente queda no disponible.

La copia sólo acepta `DXGI_FORMAT_B8G8R8A8_UNORM`. Si la textura llega en otro
formato, `WinRtFrameCopier::copy` devuelve `false` y el frame se descarta. El
bucle no cambia a `closed` por un fallo de copia repetido: el consumidor puede
ver una imagen congelada sin recibir un estado de error. Un cambio de formato,
HDR/10 bits o un fallo de mapeo debe investigarse con instrumentación local; no
se resuelve reintentando la captura desde el coordinador.

### 3. Identidad de la fuente y reutilización de handles

La selección conserva el `HWND` o `HMONITOR` y el objeto de captura. No se
vuelve a buscar por título. Para una ventana, `available()` comprueba `IsWindow`
y que el PID siga siendo el mismo; para un monitor, `GetMonitorInfo` y el nombre
de dispositivo.

Estas comprobaciones reducen el riesgo, pero **no eliminan** la reutilización de
un `HWND`/`HMONITOR` por otro objeto si el sistema recicla el handle entre dos
comprobaciones. La barrera principal es el objeto de captura retenido y el
cierre de la operación; no se debe adoptar una ventana nueva con el mismo título
ni reutilizar la referencia. Si la ventana se cierra, la operación termina y
hace falta una invitación nueva.

Los monitores pueden cambiar de topología, resolverse de nuevo o reconectarse.
El backend no hace fallback a otro monitor: si la identidad no coincide, publica
`closed`.

### 4. Ciclo de vida del selector

Hay dos puntos que el integrador debe respetar:

- El callback del selector captura el `CaptureModel`. `cancelPrompt()` sólo
  publica `WM_CLOSE`, por lo que la respuesta puede llegar de forma asíncrona.
  No se debe destruir el adaptador con un selector pendiente: hay que entregar
  la respuesta terminal y drenar `ModuleRuntime.post` antes de liberar el
  modelo y su runtime.
- `prompt_window_` no se limpia cuando el usuario cierra el selector de forma
  normal. Una llamada posterior a `cancelPrompt()` podría enviar `WM_CLOSE` a un
  `HWND` reciclado. `IsWindow` reduce la ventana de riesgo, pero no acredita la
  identidad del handle. Es un punto a endurecer si se cambia el ciclo de vida
  del selector.

Tampoco se debe destruir el `QWidget` padre mientras el selector está abierto:
`WM_DESTROY` intenta reactivarlo con `EnableWindow`, y el handle puede haber
dejado de ser válido.

### 5. Hilos, apartamento COM y parada

El modelo no impone el hilo, pero el contrato del producto exige que sus
entradas se ejecuten en el hilo del módulo y que `post` no ejecute inline. Si se
llama a `selectSource` o `startCapture` desde otro hilo, la serialización y el
`epoch` dejan de proteger el estado de la forma prevista.

Los objetos WinRT se crean en el hilo que llama a `createWin32Capture` y el
bucle los usa desde su propio hilo. `initialiseApartment` intenta MTA y tolera
que el hilo ya esté inicializado, pero no se usan referencias ágiles ni
marshalling explícito. No cambiar el hilo de creación o de consumo sin revisar
la agilidad de los objetos y el apartamento COM.

`WgcCaptureCore::stop()` hace `join`. Llamarlo desde el callback del frame
pool, desde el hilo de captura o desde una ruta que pueda esperar a ese hilo
produce un bloqueo. El modelo lo llama desde el hilo del módulo después de
invalidar la operación; mantener esa dirección.

### 6. Sin frame no significa fuente cerrada

El backend no tiene un temporizador de inactividad de frames. Una ventana
minimizada se detecta por `IsIconic` y produce `suspended`; una ventana que
sigue existiendo pero no entrega frames (por ejemplo, una superficie bloqueada
o un fallo de copia) puede dejar al consumidor sin imagen y sin `closed`. El
módulo común debe usar su temporizador de actividad y su política de cierre; no
basta con esperar un frame para saber que la fuente está viva.

En un monitor, el bloqueo de sesión o el apagado de pantalla no se convierten
automáticamente en `suspended` o `closed`. El coordinador debe atender
`DesktopEvents::locked` y los cambios de autorización, y detener la operación.

### 7. Privacidad y seguridad

Las imágenes son contenido de pantalla sin cifrar en memoria. El backend no
borra buffers al liberarlos, no aplica redacción y no distingue contenido
sensible. El coordinador debe:

- no registrar títulos, etiquetas de fuente ni frames;
- no enviar `NativeSource.label` por el protocolo;
- tratar `VisualFrame` como dato sensible y liberar la operación cuanto antes;
- cerrar la captura al bloquear la sesión, retirar permisos o perder la
  conexión.

Los títulos se usan sólo en el selector local. La etiqueta de una ventana no es
una identidad estable: no se debe usar para reencontrarla.

### 8. Rendimiento

Cada frame se copia de GPU a una textura de staging y de ahí a un
`std::vector`. Es una operación de ancho × alto × 4 bytes por frame, además del
coste de `Map`/`Unmap`. En 4K o en ventanas grandes el coste de CPU y memoria
puede ser alto; el backend no escala antes de copiar ni limita a 30 fps.

El bucle no introduce una espera entre frames cuando el pool entrega frames de
forma continua. El control de frecuencia, la escala y el descarte por cola
pertenecen al módulo común. No añadir aquí una cola de frames: la propiedad y
los límites ya están definidos en el contrato.

Los tamaños máximos internos (16384/64 Mi píxeles) son una defensa contra
reservas absurdas, no un objetivo de rendimiento. Un frame dentro de esos
límites todavía puede ser demasiado grande para el presupuesto de salida.

### 9. DPI, ventanas y contenido

La etiqueta de tamaño de una ventana usa los bounds extendidos de DWM; el
tamaño real del contenido de WGC puede diferir por bordes, sombras o DPI. No
uses esa etiqueta para recortar ni para calcular la imagen. El tamaño de la
imagen es siempre el que entrega el frame.

La aplicación debe ser consciente de DPI por monitor para que la posición del
selector y las etiquetas sean coherentes. Una aplicación DPI-unaware puede ver
coordenadas virtualizadas; la captura sigue siendo del sistema, pero la UI de
selección puede quedar mal colocada.

WGC entrega la superficie que el sistema considera la ventana elegida. Los
menús, diálogos y popups que el sistema trate como ventanas independientes
quedan fuera; no se recorta una región del escritorio. Una ventana con título
vacío o una ventana de herramienta no aparece en la lista.

### 10. Cobertura de pruebas

Las pruebas portables no ejercitan WGC, D3D11, el selector Win32 ni el
comportamiento de una sesión real. El smoke test real sólo comprueba que llega
un frame con tamaño y buffer válidos, que el cierre produce `closed` y que no
hay callbacks después de `stop`; no verifica contenido, oclusión, cambio de
monitor, DPI, HDR, DRM ni interacción con el selector.

En CI el smoke puede omitirse por falta de escritorio visible. La validación
manual en Windows debe incluir, como mínimo:

1. dos ventanas con patrones distintos, solapadas y en monitores distintos;
2. mover y redimensionar la ventana original;
3. minimizar y restaurar;
4. cerrar la ventana y abrir otra con el mismo título;
5. cancelar el selector y cerrarlo con la X;
6. probar una ventana protegida o una aplicación elevada;
7. comprobar el cursor y el borde/indicador de captura del sistema;
8. repetir con el SDK mínimo y con el SDK actual.

## Relación con Wayland

El backend Wayland está en [`../../linux/visual`](../../linux/visual/) y
implementa la misma fachada. No comparte código nativo con Windows:

| Aspecto | Windows | Wayland |
| --- | --- | --- |
| Selección | Lista Win32 de ventanas/monitores. | Diálogo del portal ScreenCast; la aplicación no enumera ventanas. |
| Autorización | Selección local; no hay diálogo WGC. | Portal y compositor; puede devolver cancelado o denegado. |
| Tipo de fuente | `HWND`/`HMONITOR` retenido. | El portal devuelve `source_type`; sin atestación se considera no disponible. |
| Datos | D3D11/WGC, copia BGRA. | PipeWire, conversión desde BGRA/BGRx/RGBA/RGBx. |
| Cursor | QI opcional de `IGraphicsCaptureSession2`. | `cursor_mode` del portal. |
| Cierre | `IsWindow`/PID y monitor; WGC. | Cierre de sesión del portal y cierre de PipeWire. |

Las diferencias de disponibilidad, formato y selección no deben ocultarse con
un fallback de Windows a monitor ni con una captura de región. El contrato
común exige que cada backend declare lo que realmente puede acreditar.

## Lista de comprobación para el integrador

- [ ] Llamar a las operaciones desde el hilo del módulo y usar un
      `ModuleRuntime.post` que encole de verdad.
- [ ] Esperar una respuesta terminal de `selectSource`; no crear una segunda
      selección mientras hay una pendiente.
- [ ] Tras `ready`, llamar a `startCapture` sólo para la referencia aceptada.
- [ ] No esperar un estado `available` inicial; aceptar el primer frame como
      contenido.
- [ ] Vaciar el visor al recibir `suspended` y cerrar la operación al recibir
      `closed`.
- [ ] Detener la captura al bloquear la sesión, retirar permisos, cerrar el
      visor o perder la conexión.
- [ ] Escalar y limitar la frecuencia en el módulo común; no asumir que el
      backend entrega 1920×1080 o 30 fps.
- [ ] No registrar títulos, etiquetas ni frames; no enviar identificadores
      nativos por el protocolo.
- [ ] Probar el camino real en una sesión Windows desbloqueada, no sólo CTest.
- [ ] Si se cambia el selector o el ciclo de vida, volver a probar cancelación,
      cierre del padre y destrucción durante una selección pendiente.

## Documentación relacionada

- [Compartir una ventana](../../../../docs/COMPARTIR_VENTANA.md)
- [Compartir una pantalla](../../../../docs/COMPARTIR_PANTALLA.md)
- [Implementación paralela y contrato común](../../../../docs/IMPLEMENTACION_PARALELA.md)
- [Contrato visual común](../../../features/visual_share.hpp)
- [Contrato de escritorio](../../desktop.hpp)
- [CMake del paquete](../../../../cmake/capture_windows/CMakeLists.txt)
- [Smoke test WGC](../../../../tests/capture_windows/wgc_smoke.cpp)
- [Backend Wayland](../../linux/visual/wayland_visual_capture.hpp)
