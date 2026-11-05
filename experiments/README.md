# Experimentos de viabilidad

Actualizado: **5 de noviembre de 2026**. Los tres prototipos tienen código, construcción independiente, pruebas aisladas, manual y resultados. La validación de escritorio y entre dos equipos continúa pendiente; el detalle de compilación difiere entre experimentos.

## Estado consolidado

| Experimento | Evidencia aislada registrada | Pendientes principales |
| --- | --- | --- |
| [Portapapeles](clipboard/README.md) | Windows 149/0; Linux con portal 151/0; Linux sin portal 149/0, según informe del 4 de noviembre | Compilar TLS Linux y validar permisos, lectura/escritura y sincronización real |
| [Archivos](files/README.md) | Windows 176/0, según informe del 4 de noviembre | Compilar adaptador GTK en Ubuntu, probar pegado, TLS mutuo y transferencias |
| [Teclado y ratón](input/README.md) | Binario self-test Windows 538/0, ejecutado el 5 de noviembre; incluye flujos Wayland con dobles | Confirmar compilación nativa Linux y probar permisos, cruce, retorno y recuperación |

Los recuentos describen comprobaciones de distinta naturaleza y no certifican soporte de los escritorios. No se han vuelto a ejecutar portapapeles ni archivos en esta revisión. La evidencia histórica de entrada 403/0 corresponde a la entrega anterior a Wayland y no describe toda la batería actual.

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

