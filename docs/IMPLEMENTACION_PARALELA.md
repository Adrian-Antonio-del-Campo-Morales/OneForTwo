# Implementación paralela de pantalla, ventana y notificaciones

Esta base define la primera entrega compatible de las tres funciones con tres ramas independientes. Complementa los diseños funcionales y sustituye sus propuestas anteriores de acordar firmas, codec o límites durante la implementación. Las declaraciones públicas en [feature_contracts.hpp](../src/integration/feature_contracts.hpp), [visual_share.hpp](../src/features/visual_share.hpp), [notifications.hpp](../src/features/notifications.hpp) y [desktop.hpp](../src/platform/desktop.hpp) forman el contrato común de esta entrega.

## Base y ramas

Los tres agentes deben partir del mismo commit que contenga este documento y las cuatro cabeceras. Los prompts se entregan por separado. El integrador prepara la etiqueta refs/tags/oneintwo-parallel-base-v1 y comunica su SHA antes del arranque. Un clon remoto solo sirve como base cuando contiene ese commit. Cada agente crea un clon local independiente con --no-hardlinks --no-checkout y un worktree dentro de su propio directorio de trabajo. El repositorio original se utiliza como origen de lectura. Los cambios locales pendientes de otra persona requieren conservarse fuera de sus commits.

Cada rama entrega commits de su paquete. El integrador realizará después la combinación y las pruebas conjuntas. Los agentes pueden desarrollar y probar su paquete con dobles locales de las interfaces ajenas desde el primer día. Los dobles viven en sus carpetas de pruebas y se excluyen del ejecutable de producto.

| Agente y rama | Propiedad de escritura | Resultado |
| --- | --- | --- |
| P · `feature/share-screen` | CMake raíz, `src/main.cpp`, `src/app/`, `src/core/session.cpp`, `src/net/`, `src/storage/`, `src/ui/main_window.cpp`, `src/features/profiles.cpp`, `src/features/visual_share.cpp`, `src/features/visual/`, `src/ui/visual_viewer.cpp`, `cmake/session/`, `cmake/visual/`, `tests/session/`, `tests/visual/` | Pantalla, módulo visual común, visor, sesión, red y composición de las tres funciones. |
| V · `feature/share-window` | `src/platform/windows/visual_capture.cpp`, `src/platform/windows/visual/`, `src/platform/linux/visual_capture.cpp`, `src/platform/linux/visual/`, `cmake/capture_windows/`, `cmake/capture_wayland/`, `tests/capture_windows/`, `tests/capture_wayland/` | Capturas nativas de ventana y monitor en Windows y Wayland. |
| N · `feature/notifications` | `src/features/notifications.cpp`, `src/features/notification_support/`, `src/platform/windows/notifications.cpp`, `src/platform/linux/notifications.cpp`, `src/platform/windows/notification_support/`, `src/platform/linux/notification_support/`, `cmake/notifications/`, `tests/notifications/` | Envío manual, recepción y reenvío de avisos. |

Las cabeceras públicas de esta base quedan bajo propiedad del integrador. Cada módulo implementa su `Impl` en su propio archivo. Las ampliaciones privadas de sesión pertenecen a P, incluida su representación privada en session.hpp. SessionGeneration, FeatureSet y las firmas ya declaradas se conservan como contrato público. Una incompatibilidad descubierta se entrega al integrador con caso concreto y propuesta mínima mientras continúa el resto del paquete. El integrador incorpora cualquier corrección pública a una nueva base común.

Los documentos globales y `TODO.md` se consolidan al integrar. Cada agente devuelve los defectos pendientes con descripción y reproducción para incorporarlos a `TODO.md`. La entrega de cada rama incluye SHA base y final, archivos, comandos ejecutados, resultados nativos y límites de verificación en su respuesta final.

## Decisiones comunes

- V++17, CMake 3.24 o posterior y Qt 6 Widgets como base gráfica. Los componentes declaran sus dependencias por target. P fija la resolución de dependencias del ejecutable.
- P implementa JPEG de imágenes independientes mediante Qt Gui, con calidad inicial 75. Reduce calidad y después resolución si hace falta para respetar 1 MiB por imagen. Si no cabe, descarta el fotograma y comunica error recuperable. El receptor valida dimensiones del JPEG antes de reservar la imagen decodificada. El soporte del codec se comprueba al arrancar.
- Todas las unidades JPEG llevan `key_frame=true`. Cada imagen completa permite recuperar la presentación. El límite de frecuencia es 30 fps y se conservan los límites de dimensiones y píxeles del contrato visual.
- P implementa un único transporte autenticado y cifrado del producto. Puede reutilizar código de los experimentos tras leerlo y adaptarlo al protocolo común. La identidad del par debe verificarse y los canales de datos quedar ligados a la misma sesión. Las credenciales de prueba pertenecen al entorno local de pruebas.
- El primer objetivo de captura Linux es Wayland con ScreenCast y PipeWire. V conserva su propia sesión de captura dentro de su área, reutilizando patrones del repositorio. V implementa ambos tipos de fuente.
- La captura mantiene BGRA de ocho bits, filas contiguas de arriba hacia abajo y alpha opaco. El tamaño exacto es ancho × alto × 4 con multiplicación comprobada. El backend valida sus dimensiones nativas antes de reservar memoria. P escala al límite de salida conservando proporción. La conversión de formatos y stride nativos pertenece al backend.

Estas elecciones son decisiones de diseño para esta entrega. Su rendimiento y disponibilidad se verifican al implementar.

## Construcción independiente

P crea `cmake/visual/CMakeLists.txt`. V crea `cmake/capture_windows/CMakeLists.txt` y `cmake/capture_wayland/CMakeLists.txt`. N crea `cmake/notifications/CMakeLists.txt`. Cada archivo funciona como proyecto independiente y como subdirectorio del raíz. Obtiene la raíz del repositorio desde su propia ruta.

Los targets públicos son `oneintwo_visual`, `oneintwo_capture_windows`, `oneintwo_capture_wayland` y `oneintwo_notifications`. Los targets exportan la carpeta `src` como include público y V++17. Cada paquete enlaza sus dependencias mediante targets y permite activar sus pruebas con `ONEINTWO_BUILD_TESTS`. N selecciona su backend según la plataforma y exporta solo la fábrica correspondiente.

Cada paquete puede comprobarse con `cmake -S cmake/<paquete> -P build/<paquete>`, compilación y CTest de esa carpeta. Los targets de pruebas enlazan dobles para dependencias ajenas. P crea el ejecutable final enlazando visual, notificaciones y la captura de la plataforma, además de sesión y Qt Widgets. Durante el trabajo aislado P puede comprobar composición mediante un ejecutable de prueba con fábricas dobles. El ejecutable final exige las implementaciones reales.

## Hilos, propiedad y vida de objetos

`ModuleRuntime.post` encola trabajo en el hilo principal Qt, nunca lo ejecuta inline. Las entradas públicas de módulos, el despacho de mensajes, `tick` y los callbacks de UI se ejecutan allí. El coordinador llama a `tick` cada 100 ms. El reloj inyectado es monotónico y las pruebas lo controlan explícitamente.

Los adaptadores pueden tener hilos propios. Entregan callbacks ordenados mediante `post`. Los trabajos de codec y red mantienen buffers con propiedad explícita y devuelven resultados al mismo ejecutor. Las colas de trabajo deben estar acotadas.

P crea primero runtime, adaptadores y visor, después módulos, y por último conecta las acciones. Las referencias de `VisualDependencies` y `NotificationDependencies` permanecen válidas durante la vida de cada módulo. Antes de destruir, P retira permisos, detiene módulos y adaptadores, desconecta callbacks y destruye los consumidores antes que sus dependencias. Los callbacks pendientes comprueban un token de vida y operación antes de acceder al destinatario.

`PlatformContext.parent` identifica la ventana local para selectores y permisos. Los callbacks de `DesktopEvents` notifican bloqueo y cambios de soporte/consentimiento. P vuelve a consultar capacidades y recalcula la autorización. Cada backend gestiona los recursos de su función. Las notificaciones de bloqueo repetidas son idempotentes.

La parada de captura invalida la selección o adquisición pendiente. La respuesta terminal de selección se entrega una vez mientras el consumidor siga vivo. Los frames posteriores a una parada conservan la referencia original y se descartan. El visor emite `user_closed` únicamente por una acción de cierre del usuario. `close` programático libera el visor sin provocar un ciclo de mensajes.

## Conexión de entradas y salidas

| Acción o mensaje | Destino |
| --- | --- |
| Compartir monitor o ventana | P llama a `VisualShare::offerSource` con la generación vigente. |
| Invitación | P despacha `VisualShareOffer` a `receiveOffer`. P llama a `show_offer`. |
| Aceptar o rechazar en UI | P llama a `decideOffer`. P emite `VisualDecisionMessage` por `send`. |
| Decisión, estado, actividad y cierre remotos | P llama respectivamente a `receiveDecision`, `receiveSourceState`, `receiveActivity` y `receiveStop`. |
| Imagen completa validada | P entrega `VisualEncodedFrame` por valor a `receiveFrame`. |
| Cerrar visor | La fábrica creada por P llama a `VisualShare::stop`. |
| Formulario de aviso | P llama a `SharedNotifications::sendManual`. |
| Aviso remoto | P llama a `receiveNotice`. |
| Cambio de apps permitidas | P llama a `setAllowedApplications`. N reinicia o actualiza su observación según permisos. |
| Cambio de autorización | P llama a `setAuthorization` en ambos módulos antes de despachar más trabajo. |

P y N usan exclusivamente `SendFeatureMessage` para transmitir. `queued` significa que la sesión adquirió el mensaje. `busy` permite descartar vídeo y mantener avisos en una cola local acotada. `disconnected` o `denied` cancelan el trabajo afectado. Si falla el envío de un mensaje de control visual, P termina localmente la operación. El otro extremo aplica su temporizador.

N solicita el permiso nativo cuando existe permiso funcional de reenvío y una lista de apps habilitada. `startObserving` informa inicio o error y posteriores retiradas mediante `state_changed`. Los callbacks capturan el identificador de suscripción. `presentNotice` completa una vez con el resultado de la solicitud al escritorio. El permiso se comprueba otra vez antes de presentar.

P implementa los límites de perfiles existentes. Habitual permite las capacidades concedidas. Solo envíos conserva avisos manuales y retira vídeo y reenvío. Pausa retira todas. La interfaz permite habilitar por separado cada modalidad y sentido, aceptar ofertas y detener operaciones. El bloqueo retira capacidades efectivas y vacía la presentación.

## Protocolo de funciones, versión 1

P es el único propietario del formato de bytes. P y N producen y consumen los tipos de `FeatureMessage`. La compatibilidad en memoria queda fijada por la cabecera y P implementa esta representación de red.

Todos los enteros usan big endian. Una trama empieza con versión u16 igual a 1, tipo u16 y longitud de payload u32. Se rechazan versiones, tipos y enumeraciones desconocidos y bytes sobrantes. Las cadenas tienen longitud u32 seguida de UTF-8 válido. Los bool se codifican como u8 con valores 0 o 1. Las enumeraciones de dominio usan el orden declarado, empezando por cero.

| Tipo | Canal | Payload en orden |
| --- | --- | --- |
| 1 · oferta | Interactivo | referencia visual, modalidad u8 |
| 2 · decisión | Interactivo | referencia visual, decisión u8 |
| 3 · estado | Interactivo | referencia visual, revisión u64, estado u8 |
| 4 · actividad | Interactivo | referencia visual, revisión u64 |
| 5 · cierre | Interactivo | referencia visual |
| 6 · fragmento | Datos | referencia visual, revisión u64, secuencia u64, ancho u32, alto u32, key_frame u8, total u32, offset u32, bytes restantes |
| 7 · aviso | Interactivo | generación u64, notice_id u64, modalidad u8, título, cuerpo, app_name |

Una referencia visual contiene generación u64, iniciador u8 e identificador de oferta u64. En el cable, iniciador 0 representa al emisor de esa trama y 1 al receptor. Al decodificar se invierte a la perspectiva del receptor. El envío de una respuesta vuelve a expresar el iniciador desde la perspectiva de quien la envía.

P asigna a cada conexión autenticada una generación local nueva y creciente. Durante el establecimiento intercambia ambas generaciones y conoce la remota. En cada trama saliente serializa su generación local. En recepción exige que el valor coincida con el anunciado por el par y lo traduce a la generación local antes de construir el objeto de dominio. Cada canal de datos se asocia a esa conexión autenticada. Al reconectar se destruyen colas y asociaciones anteriores. Los identificadores de operaciones empiezan en 1 por generación y sentido. El desbordamiento termina la sesión antes de reutilizar valores.

El canal interactivo admite como máximo 4096 bytes de payload por trama. El fragmento admite 65536 bytes de datos más sus 50 bytes de metadatos. Cada imagen tiene como máximo 1 MiB. El reensamblado mantiene una sola imagen por operación, exige offsets contiguos desde cero y metadatos idénticos en todos sus fragmentos. Un error estructural termina el canal y la operación. Al retirarse la operación, sus fragmentos se descartan.

P intercala los fragmentos y otros trabajos de datos mediante un turno por operación. Conserva como máximo dos imágenes codificadas en cola y un reensamblado receptor. P mantiene como máximo una captura pendiente adicional al trabajo de codec en ejecución. El canal interactivo tiene un presupuesto de 64 KiB y devuelve `busy` si se supera. Los límites se verifican antes de copiar o reservar memoria.

## Tiempo, pérdida y deduplicación

P envía actividad cada segundo desde el origen mientras la fuente seleccionada siga válida, también durante suspensión. El receptor termina a los tres segundos sin estado, actividad o imagen válida de la operación. El plazo arranca al aceptar. La primera revisión es 1 y aumenta al cambiar disponibilidad o tamaño. La secuencia empieza en 1 y crece durante toda la operación. El receptor exige revisión vigente y secuencia superior a la última presentada.

Una imagen recibida antes del anuncio de su revisión se descarta. Al anunciar revisión nueva se vacía el visor y se reinicia el decodificador. Una imagen JPEG posterior puede recuperar la presentación. Un cierre, revocación o nueva generación invalida los trabajos pendientes antes de ejecutar limpieza.

N conserva como máximo 16 avisos pendientes. Al superar capacidad descarta el nuevo y reporta el motivo local. Aplica un máximo de tres avisos por ventana de un segundo monotónico, tanto al enviar como al recibir. Un aviso caduca a los diez segundos en cada cola local. El receptor consume el identificador autorizado incluso si descarta por frecuencia. La sesión evita acumular avisos en red más allá del mismo plazo local. Al cerrar conexión se descarta todo aviso pendiente.

## Comprobación de compatibilidad e integración final

Cada paquete compila contra las cabeceras comunes sin modificarlas y prueba los resultados observables de sus interfaces. P incluye ida y vuelta de todos los mensajes, límites, fragmentación y traducción de generación/iniciador. P prueba selección, aceptación, revisión y cierre con reloj y captura sintéticos. V prueba ambos tipos de fuente y cancelación. N prueba formulario, observación, filtros, revocación y presentación.

El integrador incorpora P, V y N en una rama de integración, resuelve las conexiones de compilación y reutiliza la evidencia válida de cada entrega. Después compila el ejecutable en Windows y Ubuntu y recorre envío manual, pantalla y ventana entre equipos. Comprueba bloqueo, permisos, cierre, reconexión y saturación con los mismos binarios. Las funciones cuyo adaptador o prueba nativa siga pendiente se identifican expresamente.

El commit de base común y su etiqueta deben estar disponibles para los tres agentes antes de abrir sus ramas. La etiqueta oneintwo-parallel-base-v1 es una referencia acordada para la preparación y su existencia debe comprobarse.

Los encargos de Pantalla, Ventana y Notificaciones se entregan por separado del repositorio. Cada uno incluye aislamiento local, propiedad, pruebas y formato de entrega. El reparto en tres paquetes sustituye el anterior reparto en cinco.
