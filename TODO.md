# Problemas pendientes

## EXP-CLIP-TLS-01 — Fallo de lectura PEM al arrancar Windows

Estado: **corregido y verificado en Windows**, 5 de noviembre de 2026.

El servidor terminó por `no OPENSSL_Applink` antes de abrir el puerto. Se
cambiaron ambas lecturas de certificados del transporte a BIO de memoria.
Regresión aislada superada con los dos certificados públicos, rechazos
correctos y self-test 149/0. Servidor real activo en 192.168.1.147:47831,
escucha confirmada. Handshake y copia entre equipos todavía pendientes;
recompilación de esta corrección en Ubuntu pendiente. Evidencia en
`experiments/clipboard/RESULTS.md`.

## EXP-LAN-01 — Completar la preparación de la primera conexión

Estado: identidades públicas Windows/Ubuntu preparadas; pendiente de IP Windows y coordinación del arranque.

El [paquete LAN de portapapeles](experiments/clipboard/lan/README.md) incluye
asistente probado sin red/escritorio, muestras sintéticas y certificado público
Windows. La clave privada queda solo en `.local-state/lan-clipboard/windows/`.
El puerto 47831 está libre; el perfil Windows observado es Public. No se ha
creado una regla de cortafuegos ni abierto una escucha.

Ubuntu avanzó sin conflictos a `6c2ec07`, generó su identidad y exportó solo
`public/ubuntu.pem` y `public/ubuntu.sha256`. Clave privada local ignorada;
huellas de ambos certificados coherentes con sus archivos SHA-256. IP LAN
Ubuntu consultada y comunicada al usuario fuera de Git. Sin recompilar C++
ni repetir pruebas ya válidas: los commits Windows no cambiaron esas fuentes.
Falta que Windows obtenga esta entrega y comunique su IP LAN actual.
Después, sincronizar ambos extremos, comprobar identidades y aplicar solo la
regla temporal necesaria antes de ejecutar la matriz acordada. Ninguna prueba
entre equipos está superada por esta preparación.

## EXP-BUILD-01 — Completar la evidencia de compilación nativa

Estado: **completado en Ubuntu/Wayland y Windows para la revisión de entrega; X11 sin evidencia nueva**. Actualizado: 5 de noviembre de 2026.

Reconstrucción Windows realizada sobre `2ae444df56689dbaebe9b796daca26a55bd02917`
en `validation/windows`: los tres prototipos compilan en Release x64 con OpenSSL.
Self-tests: portapapeles 149/0, archivos 176/0 y entrada 579/0; ayuda de los
tres ejecutables correcta. Sin pruebas Windows de escritorio ni red.

Base verificada: `450cbb2e340a381bda01274eb86df0e05934e822`, rama local
`validation/ubuntu`, más la corrección mínima `<signal.h>` → `<csignal>` en
el transporte TLS de portapapeles y las correcciones locales de lectura de
pipes, vida de publicación y vaciado del log; acción GTK y formato de Nautilus
en archivos; preparación y salida local de la demostración de entrada.
Rama de entrega de estas correcciones: `validation/ubuntu`.

- Portapapeles: portal libdbus y TLS OpenSSL compilados en Ubuntu 26.04.1; **153/0**, salida 0, tras añadir la regresión de lectura diferida de pipes (151/0 iniciales). Corregidos `std::signal`, EAGAIN, vida de la oferta y vaciado de metadatos.
- Archivos: adaptador GTK3 y TLS compilados, sin sustituir el adaptador por `clipboard_unavailable`; **176/0** previos. Reconstrucción posterior correcta tras finalizar los nombres recibidos; L6/L7/L8 reales superados sin repetir la batería.
- Entrada: aplicación Wayland GDBus/libei y TLS compilada con X11 desactivado; **579/0** tras las regresiones de preparación segura (538/0 anteriores), más Response y señales del atajo en memoria. Corregidos callback/resultados, tipo de sesión, recepción del atajo y separación de la salida respecto al control EIS. R2/R9 locales superados; recuperación ante cortes entre equipos pendiente; X11 sin evidencia nueva.
- Windows: reconstrucción de los tres experimentos y self-tests completados sobre el commit de entrega. Evidencia nueva al inicio de sus `RESULTS.md`; se conserva la evidencia anterior como histórica.

Las dependencias se conservan en `.local-state/ubuntu-validation/`, dentro del
proyecto y excluidas de Git, sin instalación global, `sudo` ni cambios de
cortafuegos. Para usarlas desde la raíz del proyecto:
`source .local-state/ubuntu-validation/env.sh`. Se trasladaron desde `/tmp` y
se reconfiguraron las tres compilaciones para usar la ubicación persistente.
Comandos, versiones, incidencias y logs se registran en los tres `RESULTS.md`.

Usar las opciones explícitas de cada manual para evitar una compilación satisfactoria que omita el backend o TLS. No confundir los flujos con dobles con los adaptadores nativos. Registrar comandos, dependencias, versiones y errores.

## EXP-MANUAL-01 — Validar capacidades en los equipos reales

Estado: **fase local prevista completada en la configuración soportada; limitaciones registradas y casos entre equipos pendientes**.

Portapapeles y pegado asistido de archivos ya tienen evidencia local. Tras
la pausa solicitada se reanudó entrada: cancelación real de RemoteDesktop
verificada después de corregir Response y el tipo de `session_handle`.
La preparación requiere identidad de aplicación y un solo monitor activo.
El usuario confirmó pulsar Ctrl+Alt+F12, observar todo el PC congelado y
**necesitar reiniciar para recuperarlo**.
El proceso no recibió la emergencia y terminó por tiempo; no queda activo.
El nuevo atajo, sin EIS, pasó después con la guía y el temporizador de 30 s:
salida 0 y escritorio respondiendo, confirmado por el usuario.
Después pasó la demostración local EIS: movimiento breve y texto `oneintwo`,
salida 0 y escritorio respondiendo, confirmados por el usuario. Después pasaron
R2 (emergencia con EIS activo) y R9 (retirada desde GNOME), ambas salida 0 y
escritorio operativo. Cortes y estados retenidos entre equipos siguen pendientes.

El 5 de noviembre, el usuario reanudó la validación con presencia confirmada
después de la etapa aislada. Se probaron lectura, publicación/pegado,
observación y cancelación del portal de portapapeles en GNOME Wayland.
Los fallos encontrados y las repeticiones se conservan en los informes.
También se probó el pegado asistido de archivos en Nautilus, con integridad
correcta, el diagnóstico y la inyección local de entrada. No se ha iniciado
captura ni conexión con Windows. La única red de prueba ha sido TLS de
archivos en loopback para L6/L7/L8.

- Portapapeles: lectura, pegado, observación y cancelación verificados, `devices=0`. Copia aislada posterior: una acción humana, una notificación de la muestra y 12 s sin repeticiones. Pendientes entre equipos: ecos, reconexión y sincronización real.
- Archivos: pegado asistido en Nautilus correcto, 5 archivos/5.551.150 bytes íntegros. L6/L7/L8 con TLS mutuo local superados tras corregir el renombrado de `.o2f-part` verificados en la CLI: colisiones sin sobrescritura, error de permisos, cancelación sin restos y caché limpia. Publicación automática y bajo demanda Linux son limitaciones registradas; pendientes Explorer y transferencias Windows/Ubuntu.
- Entrada: diagnóstico real con InputCapture v1, RemoteDesktop v2 y GlobalShortcuts v1. Origen Ubuntu bloqueado por requisito v2; no se habilitó captura ni X11. Las primeras preparaciones fallaron antes del diálogo; se corrigieron Response y el tipo de `session_handle`. Cancelación real verificada. Se corrigió también la ruta de recepción de GlobalShortcuts; se alcanzó la espera con identidad de aplicación y un monitor. El usuario comunica congelación al pulsar el atajo; recuperación fallida. Se preparó el nuevo `--check-target` solo de GlobalShortcuts y Ctrl+Shift+F12, con 579/0 aisladas. El atajo aislado y la demostración local EIS pasaron con guía de 30 s, texto, movimiento y escritorio respondiendo; R2/R9 locales superados con salida 0 y escritorio operativo. Cruce/retorno y cortes entre equipos pendientes.
- Red: comprobar aceptación y rechazo de identidades en los prototipos y transferencias reales.

Seguir los tres `MANUAL.md`, un experimento cada vez, y registrar resultados observados en sus `RESULTS.md`. Los self-tests y la presencia de código no cierran estos puntos.

Para reanudar escritorio: usuario presente, datos/ventana desechables y salida
preparada; comprobar el atajo local antes de habilitar captura. Antes de cada
caso Windows/Ubuntu, acordar una revisión común que incluya la corrección,
binarios, roles, IP/puerto, huellas públicas, duración, observación y
recuperación. El usuario ha autorizado publicar la rama de validación; todavía no
se ha conectado con Windows. La integración de la aplicación sigue siendo
trabajo posterior.

## EXP-INPUT-01 — Conciliar la entrega Wayland y su documentación

Estado: **resuelto en documentación**, 5 de noviembre de 2026.

`experiments/input/README.md`, `MANUAL.md` y `RESULTS.md` ya describen la implementación Wayland, sus requisitos, las pruebas manuales y los límites de la evidencia. La comprobación aislada del binario Windows dio 538/0; los 403/0 anteriores quedan identificados como históricos. Las verificaciones nativas y manuales permanecen abiertas en los puntos anteriores.

## EXP-INPUT-02 — Congelación observada al probar la salida local

Estado: **correcciones comprobadas localmente con R2/R9; recuperación ante cortes entre equipos pendiente**, 5 de noviembre de 2026.

En GNOME Wayland, con un monitor activo y RemoteDesktop/EIS autorizados,
el usuario pulsó Ctrl+Alt+F12 durante `--check-target --max-seconds 60` y
observó que todo el PC quedó congelado. El proceso no recibió la señal de
emergencia y terminó por tiempo, salida 1. No se enviaron eventos de
movimiento, botones ni teclas, pero sí se preparó una sesión EIS real.
El usuario confirmó que **tuvo que reiniciar para recuperarse**. Nuevo
arranque a las 13:10:57 CET; no queda el prototipo activo. Se conservaron
los registros del arranque afectado: hubo actividad posterior de servicios,
una suspensión y un error DRM a las 13:08:10, sin causa concluyente del fallo.
La consulta inicial sin mensajes correspondía ya al arranque nuevo.

Por petición posterior del usuario se preparó una corrección: destino Wayland
con Ctrl+Shift+F12 e identificador nuevo, prueba `--check-target` solo de
GlobalShortcuts, pulsación y liberación obligatorias antes de RemoteDesktop/EIS,
emulación diferida hasta el primer evento y cierre del estado propio.
El atajo anterior coincide con VT 12 de Mutter; esto confirma un conflicto,
pero no determina toda la causa de la congelación. Compilación nativa,
**579/0** y regresiones de señales en memoria superadas sin bus ni escritorio.
Después se intentó solo GlobalShortcuts por petición del usuario, con
`RuntimeMaxSec=60s`, `TimeoutStopSec=3s` y `timeout 60s` más 3 s. El gestor
cerró la prueba a los 63 s, salida 137 y ningún prototipo restante. No llegó
a registrarse la espera del atajo; falta confirmar qué vio el usuario.
El límite del proceso está comprobado, la recuperación gráfica no. El caso
antiguo sigue fallido.

El nuevo atajo aislado y la demostración RemoteDesktop/EIS ya pasaron con
recuperación independiente preparada y usuario presente. R2/R9 también
superados: emergencia durante control y retirada desde GNOME. Pendientes:
liberación de estados retenidos y recuperación ante cortes en la fase entre
equipos. La nueva gestión EIS tiene evidencia local real de movimiento,
letras, cierre normal y ambas salidas.
No se considera resuelto el incidente por cambiar la combinación. Detalles,
comandos y límites en `experiments/input/RESULTS.md`.

Preparación adicional de EXP-INPUT-02: guía con pasos y botones en pantalla,
Inicio manual, temporizador independiente que cierra solo el ensayo y reactiva
la sesión gráfica a los 30 s. Supervisor probado con `sleep` (cierre y
`activate_exit=0`); no es prueba de recuperación de imagen congelada. Tres
regresiones imprescindibles del supervisor superadas. No ampliar ni repetir
la batería general para esta preparación. Tras regresar el usuario se abrió
la guía. Primer intento sin «AHORA» visible, salida 124; se llevó estado y
cuenta atrás arriba. Repetición `guided-20261105-140927-fa41253e7d58`:
atajo recibido, salida 0, temporizador a los 30 s y reactivación salida 0.
El usuario vio los mensajes y confirmó el escritorio respondiendo durante
toda la prueba. Solo GlobalShortcuts, sin EIS. No demuestra retorno desde
otra VT ni reparación de una imagen congelada; el incidente sigue abierto.

Demostración posterior solicitada por el usuario:
`guided-20261105-141528-30d8de65e4b9`, `--inject-demo --arm-seconds 5`,
temporizador independiente de 30 s. Atajo comprobado antes de RemoteDesktop,
EIS preparado, movimiento breve y `oneintwo`, salida 0. El usuario confirmó
texto correcto, movimiento y escritorio respondiendo. Temporizador ejecutado
a las 14:15:58 CET, reactivación salida 0; el prototipo ya había terminado.
No se observó congelación en esta ejecución. No prueba recuperar una imagen
congelada ni estados retenidos; no se cierra el incidente por este único caso.

Intento previo: `guided-20261105-192232-74937ef202aa`, modo R2 preparado
con `--inject-demo --wait-for-stop`. Preparación rechazada por varias regiones
EIS/monitores, salida 1 antes de inyectar; supervisor de 30 s y activación de
la sesión correctos. El usuario decidió dejar R2 y R9 pendientes en lugar de
desactivar otro monitor. No repetir ni iniciar R9 hasta que lo solicite.
El usuario reanudó después con un monitor activo. R2:
`guided-20261105-192937-e3dd65eea268`, emergencia recibida durante EIS activo,
salida 0. R9: `guided-20261105-193129-20e181ab3953`, permiso retirado desde
el indicador de GNOME, desconexión EIS y salida 0. En ambos confirmó salida
recibida y escritorio respondiendo; en R9 también indicador retirado. Cierres
antes del temporizador, que actuó después con activación salida 0.
No quedan R2/R9 locales pendientes. No se afirma reparación de un compositor
congelado ni causa completa del incidente histórico. No añadir esa prueba
como condición nueva de cierre local. Siguiente fase: coordinación Windows.
