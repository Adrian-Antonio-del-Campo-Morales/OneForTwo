# Experimento de portapapeles

Para la primera conexión entre Windows y Ubuntu, seguir la [preparación LAN](lan/README.md). Incluye asistente de identidades y comandos, certificado público Windows y muestras sintéticas; no inicia conexiones automáticamente.

## Encargo para el agente

Implementa este experimento siguiendo [las instrucciones comunes](../AGENTS.md) y [el modelo de entrega](../README.md). Tu directorio exclusivo es `D:/DEVEL/OneInTwo/experiments/clipboard`. Las pruebas con el portapapeles real las ejecutará manualmente el usuario.

## Pregunta que debe resolver

¿Podemos compartir texto entre Windows y Ubuntu, con la aplicación en segundo plano y sin activar el control de teclado y ratón? ¿Qué permisos y capacidades exige cada entorno?

El objetivo es probar texto Unicode. Imágenes, historial, formato enriquecido y archivos quedan fuera de este experimento.

## Diseño superficial

Un ejecutable de consola con cuatro piezas pequeñas:

1. Adaptador Windows: observar, leer y publicar texto.
2. Adaptador Ubuntu/Wayland: solicitar la vía autorizada disponible y comunicar sus limitaciones.
3. Lógica común: identificar cambios, suprimir eco y resolver el orden de operaciones.
4. Comunicación directa mínima para conectar dos instancias durante la prueba manual.

Proponer modos explícitos para diagnóstico, observación local, publicación de texto sintético y conexión entre dos equipos. El modo de observación debe mostrar estado y metadatos suficientes para el diagnóstico, sin volcar contenido privado por defecto.

El lector y escritor de prueba no deben capturar teclado ni ratón para aparentar independencia. Si el acceso al portapapeles exige una sesión asociada a otra capacidad, documentar exactamente la autorización solicitada y la limitación. No conceder control remoto funcional solo por compartir texto.

## Flujo entre equipos

Una copia local genera una operación identificada. La otra instancia la aplica y reconoce la notificación resultante como escritura propia. Evitar que esa notificación regrese indefinidamente al origen. Definir orden determinista ante copias concurrentes y no reenviar automáticamente texto antiguo al reconectar.

La conexión usa direcciones explícitas, sin descubrimiento. La autorización del canal sigue las reglas comunes; no construir una pantalla de vinculación de producto.

## Qué podrá comprobar el agente automáticamente

- Unicode y validación de tamaños con cadenas sintéticas.
- Operaciones duplicadas y prevención de bucles mediante adaptadores falsos.
- Orden de copias concurrentes.
- Rechazo de operaciones de una sesión anterior.
- Estados de acceso concedido, denegado y no disponible simulados.

Los dobles no sustituyen a los adaptadores nativos de la entrega.

## Guion que debe preparar para ejecución manual

1. Obtener versiones y capacidades de la sesión.
2. Copiar un texto de prueba en un editor y observar el cambio.
3. Publicar texto sintético y pegarlo en el editor.
4. Repetir con otro programa enfocado, sin ventana del experimento activa.
5. Repetir sin captura de entrada activa; registrar permisos y restricciones.
6. Conectar Windows y Ubuntu y probar ambos sentidos.
7. Probar acentos, emoji, saltos de línea y copias rápidas.
8. Desconectar, reconectar y retirar permisos; comprobar errores y ausencia de texto antiguo inesperado.

## Entregable y criterio de evaluación

Código del prototipo, compilación independiente, self-tests, MANUAL.md y RESULTS.md. El informe incluirá una tabla por sistema/sesión: lectura, escritura, observación en segundo plano, funcionamiento sin captura y permisos exigidos.

La conclusión de viabilidad quedará pendiente hasta las pruebas manuales. Si una vía del escritorio no ofrece acceso independiente, informarlo como limitación; no reemplazarla silenciosamente por un modo manual y declarar cumplido el objetivo.

La decisión arquitectónica esperada es qué contrato y permisos necesita el adaptador de portapapeles.

## Implementación entregada (4 de noviembre de 2026)

El prototipo está implementado en `src/`, con auto-pruebas en `tests/selftest.cpp`
y compilación independiente en `CMakeLists.txt`. Las instrucciones reproducibles
están en [MANUAL.md](MANUAL.md) y el estado separado por evidencia en
[RESULTS.md](RESULTS.md).

Piezas:

- `src/model.*`: validación UTF-8, límites, finales de línea y orden lógico
  Lamport con desempate por origen.
- `src/engine.*`: envío de copias locales, aplicación remota, supresión de eco
  por marcador y por huella, duplicados, atrasos y generaciones de sesión.
- `src/protocol.*`: tramas con longitud y límites, mensajes Hello/Clipboard/
  Applied/Error/Bye y validación estructural.
- `src/platform/windows_clipboard.cpp`: `CF_UNICODETEXT`, notificaciones con
  `AddClipboardFormatListener` en una ventana de sólo mensajes y marcador en un
  formato privado. No captura ni inyecta entrada.
- `src/platform/linux_clipboard.cpp`: elige Wayland → portal y X11 → limitación
  explícita; no sustituye Wayland por X11 ni finge lectura o escritura.
- `src/platform/portal_clipboard.cpp`: adaptador Wayland real sobre
  `xdg-desktop-portal` RemoteDesktop + Clipboard, con libdbus. Solicita
  `SelectDevices(types=0)`, exige `clipboard_enabled=true`, rechaza cualquier
  mask de entrada distinto de cero y no llama a `Notify*` ni `ConnectToEIS`.
  Sólo texto UTF-8; no captura pantalla.
- `src/net/*`: conexión directa con TLS 1.2+, certificado del par fijado por
  SHA-256 y mensajes del protocolo. Sin descubrimiento ni confianza persistente.
- `src/cli.cpp`: modos explícitos; sin argumentos sólo muestra ayuda.

Estado verificado: compilación Windows (MSVC/OpenSSL 3.5) y compilación Linux
en Ubuntu 20.04 WSL, incluida la compilación del adaptador Wayland con libdbus
(151 comprobaciones aisladas) y la compilación sin portal (149). Las pruebas de
portapapeles real, diálogo del portal, escritorio y dos equipos están pendientes.
Wayland tiene adaptador implementado; X11 no está implementado y no se usa como
sustituto silencioso. La compilación Linux con TLS no se verificó por falta de
`libssl-dev`. No se declara validación de ningún escritorio ni se cierra P2 sin
las pruebas manuales de [MANUAL.md](MANUAL.md).

Dependencia nativa Linux: `libdbus-1-dev` (y `libssl-dev` para el modo entre
equipos). Los comandos de compilación separados por núcleo, adaptador Wayland y
validación manual están en [MANUAL.md](MANUAL.md); la evidencia y las fuentes
primarias están en [RESULTS.md](RESULTS.md).

