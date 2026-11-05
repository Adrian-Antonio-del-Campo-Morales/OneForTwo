# Resultados — Teclado y ratón

Revisión documental: **5 de noviembre de 2026**. Evidencia inicial del agente: 4 de noviembre de 2026. Este informe distingue presencia de implementación, compilación registrada, pruebas aisladas y validación manual.

## 1. Estado actual

El prototipo contiene Windows, Linux/X11 y Linux/Wayland. Wayland ya no es un backend ausente: existen su adaptador nativo, los flujos portables de autorización y la selección de backend. Las versiones anteriores de este informe describían únicamente la primera entrega X11 y no reflejaban esos cambios.

| Componente | Implementación | Evidencia disponible |
| --- | --- | --- |
| Núcleo, protocolo, geometría y controladores | Implementado | Self-test Windows existente: **538 comprobaciones, 0 fallos**, ejecutado el 5 de noviembre |
| Flujos Wayland sin escritorio | Implementados en `wayland_flow.*` | Incluidos en el self-test con dobles de PortalOps |
| Adaptador Windows | Implementado | Compilación inicial del agente con MSVC/OpenSSL; hay binario posterior en `build/cmake/` |
| Adaptador X11 | Implementado | Sin compilación nativa Linux registrada en este informe |
| Adaptador Wayland GDBus/libei | Implementado en `wayland_backend.cpp` | Sin evidencia registrada de compilación del adaptador nativo Linux |
| Selección Linux y opciones CMake | Implementadas | Revisión del código: Wayland y X11 son opcionales y seleccionados según sesión |
| TLS mutuo y huellas | Implementado | Compilación Windows documentada por el agente; handshake pendiente |
| Escritorio real y dos equipos | Pruebas preparadas | **Pendiente manual**, tanto en Windows como en Ubuntu |

La revisión del 5 de noviembre no recompiló la aplicación ni ejecutó captura, inyección, portales, diagnóstico gráfico o red. Ejecutó únicamente el binario independiente de self-tests existente. La presencia de un binario o de código nativo no certifica que el backend Linux compile.

## 2. Implementación actual

- `src/core/`: eventos, usos HID, conversiones, protocolo, geometría, estado de teclas/botones y controladores.
- `src/platform/windows_backend.cpp`: hooks, SendInput y salida de emergencia.
- `src/platform/linux_backend.cpp`: implementación X11, sin utilizarse como fallback de una sesión Wayland.
- `src/platform/linux_factory.cpp`: selección por sesión y backend incluido en la compilación.
- `src/platform/wayland_flow.*`: secuencias de autorización, barreras, activación, liberación, cierre y recuperación mediante el contrato PortalOps.
- `src/platform/wayland_backend.cpp`: GDBus, InputCapture, RemoteDesktop, GlobalShortcuts y comunicación EIS con libei.
- `src/net/tls_link.cpp`: transporte TLS y comprobación de identidad por huella en ambos extremos.
- `src/main.cpp`: modos explícitos; preparación de permisos antes de iniciar comunicación.
- `tests/self_test.cpp`: pruebas del núcleo y de los flujos Wayland con dobles, sin conexiones reales.

El código conserva el alcance de un monitor por equipo, disposición horizontal y un origen físico seleccionado. No sincroniza modificadores o botones que ya estuvieran pulsados al cruzar; deben soltarse antes del cruce.

## 3. Evidencia y límites de compilación

### Evidencia inicial del agente — 4 de noviembre

| Entorno | Evidencia declarada en la primera entrega |
| --- | --- |
| Windows 10.0.19045 x64 | MSVC 19.51.36257.0, herramientas 14.51.36231, Windows SDK 10.0.26100.0 |
| CMake Windows | 3.24.3, NMake, OpenSSL 3.0.5 |
| WSL Ubuntu 20.04 | GCC 9.4.0, CMake 3.25.2; únicamente núcleo sin backend ni TLS |
| Primera batería aislada | 403 comprobaciones, 0 fallos en MSVC y GCC 9, anteriores a la ampliación Wayland |

Estos resultados iniciales son históricos; no verifican el adaptador Wayland añadido después.

### Revisión del 5 de noviembre

Se localizaron `build/cmake/input_experiment.exe`, `build/cmake/input_self_test.exe` y `build/help.txt`, con fecha del 4 de noviembre posterior a las fuentes Wayland. La ayuda guardada ya describe Wayland. No se infiere de ello una compilación nativa de Linux.

El archivo `build/cmake-configure.log` conserva un intento fallido por ausencia de NMake en PATH; no es evidencia de una compilación final correcta. Los comandos para reconstruir se mantienen en [MANUAL.md](MANUAL.md).

No hay un resultado registrado para el binario portable WSL posterior a la ampliación Wayland. Tampoco hay evidencia de compilación del backend GDBus/libei, X11 o TLS nativo Linux. Esas comprobaciones siguen pendientes y no se sustituyen por una compilación con `ONEINTWO_BACKEND=none`.

## 4. Comprobación aislada ejecutada el 5 de noviembre

Desde el directorio del experimento:

```powershell
.\build\cmake\input_self_test.exe
```

Salida observada, código de salida 0:

```text
OneInTwo input experiment: self-tests aislados
Comprobaciones: 538, fallos: 0
```

El ejecutable de pruebas llama únicamente a `runSelfTests()`. No crea backends nativos, no abre el bus de sesión y no establece conexiones.

Cobertura de la batería actual:

- Protocolo y rechazo de mensajes inválidos.
- Conversión de teclas HID/Windows/evdev/X11 y geometría.
- Estados de origen/destino, secuencias y generaciones.
- Liberación de teclas/botones, emergencia y temporizadores.
- Geometría y barreras Wayland con datos sintéticos.
- Secuencias InputCapture y RemoteDesktop con dobles de PortalOps.
- Denegación de permisos, cierre, pérdida de captura y eventos de borde del portal.

La batería prueba la lógica de los flujos; no prueba el código GDBus/libei, el compositor, la reserva real de atajos ni la respuesta del cursor. No se han repetido las pruebas de los otros experimentos durante esta actualización.

## 5. Requisitos que impone el backend Wayland actual

Estos son requisitos de esta implementación, no una garantía general de compatibilidad de una versión de Ubuntu.

| Papel | Comprobación o recurso exigido por el código |
| --- | --- |
| Compilación | `gio-2.0`, `libei-1.0`, pkg-config; OpenSSL para comunicación entre equipos |
| Origen | InputCapture versión 2 o superior y capacidades de teclado/puntero |
| Origen autorizado | Una zona de pantalla, conexión EIS, barrera en el borde y activación concedida por el compositor |
| Destino | RemoteDesktop versión 2 o superior y GlobalShortcuts versión 1 o superior |
| Destino autorizado | Teclado y puntero concedidos, atajo de emergencia aceptado y dispositivos EIS utilizables |
| Geometría del destino | Puntero absoluto, dispositivo de puntero para botones/rueda y una región válida de escritorio |

El origen usa CreateSession2/Start/ConnectToEIS/GetZones/SetPointerBarriers/Enable. El destino prepara RemoteDesktop y reserva el atajo mediante GlobalShortcuts antes de escuchar en red. Si no se concede la salida local, el flujo se rechaza.

La geometría puede conocerse solo después de autorizar las sesiones. El diagnóstico previo no demuestra que se concederán permisos ni que EIS ofrecerá los dispositivos necesarios. No se ha fijado una versión mínima de Ubuntu verificada.

## 6. Capacidades y restricciones por validar

- La captura se activa por el compositor al alcanzar una barrera. El control usa esa notificación, no una observación global simulada del cursor.
- El retorno en origen solicita Release con una posición sugerida. No se garantiza que el compositor coloque el puntero exactamente en esa posición.
- El destino Wayland no puede restaurar una posición global que no conoce; este backend no ofrece esa restauración.
- El destino requiere un atajo local concedido. Debe comprobarse la combinación que realmente reserve el escritorio y que su activación llegue al proceso.
- El destino Linux no detecta actividad física local como el backend Windows; debe usarse la salida de emergencia.
- Un cambio de zonas, cierre del portal o pérdida de EIS debe causar la transición prevista; falta validarlo en el escritorio.
- El watchdog del destino cubre pérdida del origen, no la muerte del propio proceso receptor.
- No hay reconexión automática ni soporte garantizado para varios monitores, IME o teclas fuera de la tabla.
- Las ventanas elevadas y el escritorio seguro Windows no forman parte de la garantía de este experimento.

## 7. Matriz pendiente y siguiente paso

| Área | Estado | Prueba requerida |
| --- | --- | --- |
| Compilación Windows del estado actual | Sin nueva compilación en esta revisión | Reconstrucción mediante CMake |
| Compilación Linux/Wayland nativa | Sin evidencia registrada | Forzar `ONEINTWO_WAYLAND=ON`, TLS activado y X11 desactivado |
| Compilación Linux/X11 | Sin evidencia registrada | Forzar X11 si se quiere evaluar esa alternativa |
| Permisos Wayland | Pendiente manual | Aceptar, denegar, cancelar y retirar sesiones |
| Entrada local | Pendiente manual | Demostración acotada en un editor desechable |
| Cruce y retorno | Pendiente manual | Ambos sentidos Windows/Ubuntu |
| Recuperación | Pendiente manual | Atajo en ambos extremos, corte de red, retirada de permiso y pérdida del origen |
| TLS | Pendiente manual | Identidad correcta e incorrecta en ambos extremos |
| Portabilidad del teclado | Pendiente manual | Distribución española, modificadores y botones |

Seguir [MANUAL.md](MANUAL.md), registrando versión de sistema, sesión, portales, bibliotecas y resultado observado. La discrepancia documental está resuelta; la validación de escritorio y la compilación nativa Linux continúan abiertas en [TODO.md](../../TODO.md).

## 8. Conclusión

Hay implementación de Windows, X11 y Wayland, y evidencia aislada actualizada para el núcleo y flujos con dobles. Todavía no hay evidencia suficiente para afirmar que el adaptador Wayland compile o que los dos equipos crucen, inyecten y recuperen el control en un escritorio real. Esta entrega prepara la evaluación manual; no certifica soporte de una plataforma.

