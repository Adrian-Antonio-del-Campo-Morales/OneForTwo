> Archivos, estado vigente: T1 completo; T2 recepción CLI e integridad, negativos TLS, corte/reintento y casos Windows superados. Pegado Explorer/bajo demanda pendientes por helper de escritorio. Consulte files/RESULTS.md.

# Experimentos de viabilidad

Actualizado: **5 de noviembre de 2026**. Los tres prototipos están compilados en Ubuntu/Wayland con sus dependencias y TLS. Fase local disponible de portapapeles y archivos completada, con limitaciones registradas. En entrada pasaron atajo, demostración EIS, R2 y R9 locales con un monitor, salida 0 y escritorio operativo confirmado. Se conserva el incidente anterior que requirió reinicio (EXP-INPUT-02). Las pruebas entre equipos no se han iniciado.

La entrega Ubuntu `2ae444d` también se ha reconstruido en Windows en la rama
`validation/windows`, sin cambios de fuentes: compilación con OpenSSL y
self-tests superados, portapapeles 149/0, archivos 176/0 y entrada 579/0.
La ayuda de los tres binarios funciona. El escritorio Windows y las pruebas
entre equipos siguen pendientes. `main` aún no contiene estas ramas.

## Estado consolidado

| Experimento | Evidencia aislada registrada | Pendientes principales |
| --- | --- | --- |
| [Portapapeles](clipboard/README.md) | Ubuntu portal + TLS: **153/0**. Lectura, pegado, observación, permiso `devices=0` y cancelación. Copia aislada: una notificación de la muestra y 12 s sin repeticiones. Windows 149/0 y Linux sin portal 149/0 históricos | Ecos, reconexión y sincronización real entre equipos |
| [Archivos](files/README.md) | GTK3 + TLS: **176/0** previos. Pegado asistido en Nautilus: 5 archivos/5.551.150 bytes íntegros. L6/L7/L8 reales con TLS local superados tras corregir los nombres temporales de recepción. Windows 176/0 histórico | Transferencias y comparación Windows. Publicación automática y bajo demanda Linux: limitaciones registradas |
| [Teclado y ratón](input/README.md) | Wayland GDBus/libei + TLS, **579/0** en Ubuntu y reconstrucción Windows. Diagnóstico, cancelación, atajo, demostración y R2/R9 locales Ubuntu superados con un monitor y escritorio operativo | Cruce/retorno, teclado completo y cortes entre equipos. Origen Ubuntu bloqueado por InputCapture v1 frente al requisito v2. X11 sin evidencia nueva |

Entrada: atajo aislado y posterior demostración RemoteDesktop/EIS verificados
con el usuario presente, guía y temporizador de 30 s. Texto `oneintwo`,
movimiento breve y escritorio respondiendo confirmados; salida 0. Supervisor
probado con proceso inocuo y 3 regresiones esenciales; no se repitió la
batería general. R2/R9 también superados. Cortes y estados retenidos entre
equipos siguen pendientes; reparar una imagen congelada no está garantizado
(EXP-INPUT-02).

R2 se intentó después con un modo mínimo de espera tras la demostración:
rechazo de varias regiones EIS antes de inyectar, salida 1; timer de 30 s
correcto. Tras la pausa, el usuario reanudó con un monitor: R2 y R9 pasaron
con EIS activo, salida 0 y escritorio respondiendo. Fase local prevista
completada; siguiente fase, coordinación Windows.

Los recuentos describen comprobaciones de distinta naturaleza y no certifican soporte de los escritorios. La evidencia Windows histórica se conserva, junto con la reconstrucción aislada de la entrega Ubuntu descrita arriba. La evidencia de entrada 403/0 corresponde a la entrega anterior a Wayland y no describe toda la batería actual.

Entorno nuevo: Ubuntu 26.04.1, sesión heredada GNOME Wayland, GCC 15.2,
CMake local 4.2.3 y OpenSSL 3.5.5. Base Git
`450cbb2e340a381bda01274eb86df0e05934e822`, rama `validation/ubuntu`, más
las correcciones locales de portapapeles: `<signal.h>` → `<csignal>`, espera
acotada de pipes no bloqueantes, vida de publicación y vaciado del log;
acción GTK local y formato de archivos de Nautilus; preparación y salida
local de la demostración de entrada, corrección del callback/resultado GDBus,
tipo de sesión RemoteDesktop y ruta de recepción del atajo.
Dependencias locales en `.local-state/ubuntu-validation/`,
dentro del proyecto y excluidas de Git, sin instalación global. Se trasladaron
desde `/tmp` y se repitieron las compilaciones y self-tests con las nuevas
rutas. Para cargar el entorno desde la raíz:
`source .local-state/ubuntu-validation/env.sh`.
Configuración, compilación y self-tests finales de los tres experimentos:
salida 0. Los informes detallan componentes realmente incluidos, comandos,
versiones y los intentos iniciales que no validaban la variante completa.

Se han ejecutado sesiones locales del portal de portapapeles; el usuario
observó el pegado en GNOME Text Editor y Nautilus. El diagnóstico de entrada
ya consultó las interfaces reales. Solo se probó TLS de archivos en loopback;
no hay conexión con Windows.
Para los siguientes casos, preparar datos,
presencia del usuario y recuperación;
antes de conectar Windows, acordar revisión compartida, extremos, puerto,
identidades y caso concreto. Rama de entrega: `validation/ubuntu`. La
integración del producto permanece aplazada.

| Experimento | Instrucciones manuales | Evidencia y limitaciones |
| --- | --- | --- |
| Portapapeles | [MANUAL.md](clipboard/MANUAL.md) | [RESULTS.md](clipboard/RESULTS.md) |
| Archivos | [MANUAL.md](files/MANUAL.md) | [RESULTS.md](files/RESULTS.md) |
| Teclado y ratón | [MANUAL.md](input/MANUAL.md) | [RESULTS.md](input/RESULTS.md) |

Orden recomendado: inventariar las máquinas reales, completar las compilaciones nativas pendientes y probar localmente texto, pegado de archivos y entrada; después repetir entre equipos. Los casos de identidad incorrecta y recuperación forman parte de cada validación. El objetivo de portapapeles sigue siendo funcionar con la captura de entrada inactiva.

Los problemas y verificaciones abiertos se mantienen en [TODO.md](../TODO.md).

## Modelo de trabajo

Los agentes han trabajado en prototipos independientes y directorios exclusivos. La siguiente fase es la ejecución manual por el usuario en Windows y Ubuntu, un experimento cada vez. Las reglas siguientes siguen aplicándose a las correcciones y comprobaciones aisladas que se les encarguen.

**Los agentes no ejecutarán pruebas que lean o escriban el portapapeles real, capturen o inyecten entrada, abran sesiones de permisos del escritorio o publiquen archivos en el explorador.** Tampoco arrancarán escuchas de red o conectarán equipos durante esta fase. No necesitan coordinar turnos de uso del escritorio.

| Encargo | Directorio exclusivo | Diseño |
| --- | --- | --- |
| Portapapeles | `experiments/clipboard/` | [Encargo del agente](clipboard/README.md) |
| Teclado y ratón | `experiments/input/` | [Encargo del agente](input/README.md) |
| Archivos | `experiments/files/` | [Encargo del agente](files/README.md) |

Las [instrucciones comunes para agentes](AGENTS.md) forman parte de los tres encargos. La actualización documental no inicia agentes ni demostraciones.

## Qué significa entregar un experimento completo

El agente entregará código funcional del prototipo, construcción independiente, comprobaciones aisladas, un manual de ejecución y un informe de estado. No basta con dejar pseudocódigo o funciones que devuelvan éxito sin hacer nada.

Completo para entrega no significa validado en ambos sistemas. Cada resultado separará:

1. Implementado.
2. Compilado, indicando plataforma y herramientas.
3. Comprobado de forma aislada, sin escritorio ni red.
4. Pendiente de ejecución manual.
5. Bloqueado o no soportado, con explicación y evidencia.

Si falta un entorno Ubuntu o Windows, el agente continuará con lo que pueda implementar y verificar. No afirmará compilación o funcionamiento en el sistema ausente. Una limitación no se sustituirá por un backend simulado presentado como real.

## Estructura de cada entrega

Los directorios ya contienen diseño, código, configuración de compilación, manual y resultados. Su organización general es:

```text
<experimento>/
├── README.md           Diseño y decisiones propias del experimento
├── RESULTS.md          Estado, evidencia y pruebas manuales pendientes
├── MANUAL.md           Instrucciones exactas para el usuario
├── CMakeLists.txt      Compilación independiente
├── src/               Prototipo y adaptadores nativos necesarios
├── tests/             Comprobaciones aisladas y datos sintéticos
└── build/             Salida local ignorada
```

La estructura interna concreta se explica en cada README. Los prototipos no dependen del CMake raíz, de `src/` del producto ni de otro experimento. Cualquier corrección mantendrá esa independencia y las responsabilidades de cada archivo.

Los prototipos utilizan C++17. Se han utilizado bibliotecas generales documentadas, no motores de otras aplicaciones. La elección final de dependencias del producto y su licencia sigue pendiente. Los agentes no instalarán herramientas globalmente ni modificarán otros proyectos para resolver dependencias; las instalaciones que indiquen los manuales son pasos explícitos del usuario.

## Ejecución y comprobación separadas

El arranque sin argumentos y `--help` solo mostrarán ayuda. Cada prototipo ofrecerá `--self-test` o un ejecutable de pruebas equivalente que use datos sintéticos y dobles locales, sin inicializar el escritorio ni abrir red.

Las funciones reales se activarán mediante comandos explícitos documentados en `MANUAL.md`. El inventario del entorno también será un modo manual cuando requiera consultar una sesión gráfica. Los scripts de compilación no ejecutarán automáticamente demostraciones.

Los prototipos podrán incluir comunicación directa mínima por dirección explícita para pruebas entre equipos. Cada agente será dueño de esa implementación dentro de su carpeta. Usarán transporte estándar autenticado/cifrado y bibliotecas generales, sin construir descubrimiento, vinculación persistente o un protocolo común de producción. No habrá un archivo de red compartido que los tres deban editar.

Los permisos del sistema, certificados o claves de prueba se prepararán explícitamente durante la ejecución manual y quedarán fuera del repositorio. El manual explicará la comprobación de identidad; no desactivará silenciosamente la validación del canal.

## Contenido obligatorio del manual

- Herramientas y dependencias necesarias en cada sistema.
- Comandos exactos de compilación y ejecución; nombres reales de binarios.
- Preparación de una carpeta y datos desechables.
- Pasos numerados para probar localmente y entre los dos equipos.
- Permisos esperados, resultado observable y condiciones de fallo.
- Forma de detener el prototipo y restaurar el estado.
- Matriz donde el usuario pueda registrar resultados sin confundirlos con pruebas automáticas.

El usuario realizará primero pruebas locales y después las de dos equipos. Solo habrá un experimento activo en una misma sesión gráfica durante esas pruebas.

## Relación con el estudio preliminar

- P0: cada manual recogerá el inventario relevante de su plataforma.
- P1: experimento de entrada.
- P2: experimento de portapapeles.
- P3: experimento de archivos.
- P4: casos de recuperación incluidos en los manuales de entrada y archivos.
- P5: concurrencia entre transferencia y control, aplazada a la integración posterior; no requiere acoplar estos prototipos.

El código útil podrá trasladarse al producto después de validar resultados y revisar su diseño. La integración será un trabajo posterior, con un responsable único.

