# Experimento de teclado y ratón

Estado actualizado: **5 de noviembre de 2026**. Hay implementación Windows, X11 y Wayland. El encargo original se conserva a continuación; el estado de la entrega y su evidencia se detallan al final y en [RESULTS.md](RESULTS.md).

## Encargo para el agente

Implementa este experimento siguiendo [las instrucciones comunes](../AGENTS.md) y [el modelo de entrega](../README.md). Tu directorio exclusivo es `D:/DEVEL/OneInTwo/experiments/input`. La captura e inyección reales quedan reservadas a ejecución manual del usuario.

## Pregunta que debe resolver

¿Puede Windows y Ubuntu actuar como origen y destino de entrada, cambiar de pantalla al cruzar un borde y recuperar el control de forma fiable?

La primera prueba utilizará un monitor por equipo, un origen físico seleccionado y disposición horizontal. Teclados simultáneos, vídeo remoto, varios monitores y aplicaciones elevadas quedan fuera del experimento.

## Diseño superficial

Un ejecutable con modos de diagnóstico, origen y destino, y componentes pequeños:

1. Adaptadores Windows y Ubuntu/Wayland para captura e inyección.
2. Conversión a eventos comunes: movimiento, botones, rueda y teclas.
3. Estado de control: local, transición, remoto y recuperación.
4. Conexión directa entre instancias y supervisión de su disponibilidad.

El origen registra el borde configurado y el destino expone la geometría necesaria. El prototipo debe permitir volver a la pantalla inicial, no solo enviar eventos en una dirección. La detección del retorno y las coordenadas son parte de lo que se investiga.

La disponibilidad y los permisos se mostrarán explícitamente. Un entorno que no permita captura no se presentará como compatible porque admita inyección.

## Recuperación obligatoria del prototipo

- Inicio real mediante comando explícito y un periodo de preparación visible.
- Duración máxima de la sesión de prueba, configurada y documentada.
- Salida local de emergencia que funcione mientras el remoto recibe entrada.
- Liberación de captura al fallar la conexión.
- Temporizador independiente en el receptor para liberar teclas y botones si desaparece el emisor.
- Rechazo de eventos atrasados tras reconectar o cambiar la generación de control.
- Limpieza en cancelación, cierre normal y retirada de permisos.

Ctrl+C en una terminal no basta como única vía de recuperación si las teclas están dirigidas al otro equipo. Si no se puede implementar una salida local fiable en un backend, no habilitar su modo de captura real y documentar el bloqueo.

## Qué podrá comprobar el agente automáticamente

- Máquina de estados con reloj y adaptadores falsos.
- Conversión de eventos y coordenadas de pantallas sintéticas.
- Orden de pulsación/liberación y descarte de eventos obsoletos.
- Pérdida de conexión y activación de temporizadores.
- Prevención de bucles de eventos sintetizados.

No inicializar hooks, portales o dispositivos reales durante esas pruebas.

## Guion que debe preparar para ejecución manual

1. Diagnosticar capacidades y revisar la salida de emergencia.
2. Probar generación de eventos sintéticos acotados en una ventana de prueba elegida por el usuario.
3. Iniciar destino y origen de forma explícita.
4. Cruzar el borde, mover, pulsar, desplazar y regresar.
5. Probar Ctrl, Alt, Shift, AltGr, acentos y teclado español.
6. Desconectar al mantener una tecla o botón; comprobar recuperación en ambos extremos.
7. Probar cancelación y revocación de permisos.
8. Repetir invirtiendo los papeles de Windows y Ubuntu.

## Entregable y criterio de evaluación

Código propio de los adaptadores y del flujo mínimo, compilación, self-tests, MANUAL.md y RESULTS.md. Registrar por sistema captura, inyección, cruce, retorno, mapeo de teclado y recuperación.

La prueba no se considerará completa funcionalmente por transmitir movimiento sin retorno o sin liberación de estados. El agente puede entregar el código listo para evaluación, pero las capacidades reales y la recuperación permanecerán marcadas como pendientes hasta ejecutarlas manualmente.

La decisión arquitectónica esperada es cómo implementar el contrato de entrada y qué restricciones exigir a cada plataforma.

## Implementación y evidencia actuales

- `src/core/`: protocolo, conversiones HID, geometría, estados y controladores.
- `src/platform/wayland_flow.*`: lógica portable de autorización, activación y cierre probada con dobles de `PortalOps`.
- `src/platform/wayland_backend.cpp`: adaptador nativo GDBus/libei para InputCapture en origen y RemoteDesktop/GlobalShortcuts en destino.
- `src/platform/windows_backend.cpp`: hooks y SendInput.
- `src/platform/linux_backend.cpp`: alternativa X11; `linux_factory.cpp` selecciona el backend de la sesión sin fallback silencioso.
- `src/net/tls_link.cpp`: comunicación directa con certificados y huellas en ambos extremos.
- `tests/self_test.cpp`: núcleo y flujos Wayland aislados, sin escritorio ni red.

La ejecución del binario independiente Windows el 5 de noviembre dio **538 comprobaciones, 0 fallos**. La cifra histórica de 403 pertenece a la primera entrega anterior a la ampliación Wayland. En Ubuntu están compilados Wayland nativo y TLS; el self-test actualizado de núcleo y flujos da **579/0**, más las regresiones nativas en memoria. X11 no tiene evidencia nueva. Ninguna de estas pruebas aisladas valida la recuperación del escritorio.

La preparación Wayland exige InputCapture v2 en origen, RemoteDesktop v2 y
GlobalShortcuts v1 en destino, dispositivos reanudados y una región EIS
utilizable. El destino reserva Ctrl+Shift+F12 con identidad nueva y exige
pulsarlo y soltarlo antes de abrir RemoteDesktop/EIS. `--check-target` prueba
solo GlobalShortcuts; no abre control. La emulación empieza con el primer
evento real y el cierre normal libera el estado propio. Estas correcciones
se compilaron y probaron de forma aislada. El 5 de noviembre el usuario
verificó primero Ctrl+Shift+F12 por separado y después la demostración
RemoteDesktop/EIS con la guía y el temporizador independiente de 30 s:
movimiento breve, texto `oneintwo`, salida 0 y escritorio respondiendo.
Después pasaron R2 (emergencia con EIS activo) y R9 (retirada del permiso en
GNOME), ambas salida 0 y escritorio operativo confirmado. La fase local
prevista está completada en la configuración de un monitor. No se observó
otra congelación; no se garantiza reparar un compositor bloqueado. Estados
retenidos, cortes y entrada entre equipos siguen pendientes (EXP-INPUT-02);
origen Ubuntu bloqueado por InputCapture v1.

El [manual](MANUAL.md) contiene compilación explícita por backend y pasos de permisos, cruce, retorno, cancelación y recuperación. El [informe](RESULTS.md) distingue evidencia histórica, comprobación actual y validación manual. Ningún resultado aislado afirma que ya funcionen el escritorio o la conexión entre equipos.

