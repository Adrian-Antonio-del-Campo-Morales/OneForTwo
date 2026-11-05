# OneInTwo — Encargo de validación en Ubuntu

Preparado: 5 de noviembre de 2026.

Responsabilidad: permitir que el agente de la máquina Ubuntu continúe la verificación pendiente con el mismo código que Windows. El repositorio compartido es [OneForTwo](https://github.com/Adrian-Antonio-del-Campo-Morales/OneForTwo); el nombre interno del producto y de los ejecutables sigue siendo OneInTwo.

## 1. Preparar la copia de trabajo

Si aún no hay una copia local:

```bash
git clone https://github.com/Adrian-Antonio-del-Campo-Morales/OneForTwo.git
cd OneForTwo
```

Si ya existe una copia, comprobar primero rama y cambios locales; actualizar sin sobrescribir trabajo. Registrar el identificador completo de la revisión que se verifica:

```bash
git status --short --branch
git rev-parse HEAD
```

Ambas máquinas deben usar la misma revisión durante las pruebas entre equipos. Las rutas `~/OneInTwo` de los manuales son ejemplos; tras clonar en `OneForTwo`, ajustar las rutas al directorio real. No compilar con el CMake raíz: cada experimento tiene su propia compilación.

Para evitar mezclar correcciones con la copia Windows, trabajar preferentemente en una rama local `validation/ubuntu` creada desde la revisión compartida. No subir binarios, cachés, dependencias descargadas, claves ni archivos personales.

## 2. Documentación y alcance del nuevo encargo

Leer [AGENTS.md](AGENTS.md), [TODO.md](TODO.md), [estado de experimentos](experiments/README.md) y los README, MANUAL y RESULTS de cada experimento. Consultar estudio y arquitectura para interpretar requisitos, sin comenzar la integración del producto.

Las instrucciones de [experiments/AGENTS.md](experiments/AGENTS.md) describen la fase anterior de implementación sin pruebas de escritorio. Para este encargo, el usuario inicia deliberadamente una nueva fase de validación en Ubuntu:

- Se trabajan los tres experimentos, uno cada vez, y sus informes. El alcance no se limita a una sola carpeta experimental.
- Inventario, compilación y self-tests son la primera etapa.
- Las pruebas locales del escritorio se realizan en la sesión gráfica de prueba del usuario, con datos desechables, permisos concedidos por él y recuperación preparada.
- La captura de entrada o cualquier otra operación sobre Windows requiere coordinación con ese equipo; no se inicia una prueba de dos máquinas de forma unilateral.
- La instalación global de paquetes, elevación y cambios de cortafuegos no quedan autorizados por este documento. Si hacen falta, indicar exactamente qué debe preparar el usuario; continuar con las partes independientes.
- Se conservan las restantes restricciones: no usar motores de otras aplicaciones, no ampliar el producto, no publicar secretos, no ejecutar campañas amplias ni modificar historial ajeno.

Este documento acompaña el encargo que el usuario dé al agente; no constituye un permiso independiente para controlar sesiones o máquinas ajenas. Si solo se dispone de SSH o de una sesión sin escritorio, completar la primera etapa y registrar las pruebas gráficas como pendientes.

## 3. Etapa inicial: inventario y compilación completa

Registrar en los `RESULTS.md` existentes: revisión Git, fecha, Ubuntu, escritorio, sesión Wayland/Xorg, compilador, CMake, OpenSSL, dependencias y versiones de portales que se puedan comprobar. No interpretar el nombre de la distribución como evidencia de disponibilidad de una API.

Orden de trabajo: portapapeles → archivos → teclado/ratón. Reutilizar la evidencia Windows ya documentada; no repetir pruebas ajenas para completar los informes Linux.

### Portapapeles

Dependencias de desarrollo a comprobar: compilador C++17, CMake, pkg-config, libdbus y OpenSSL.

```bash
cmake -S experiments/clipboard -B experiments/clipboard/build/ubuntu \
  -DCMAKE_BUILD_TYPE=Release -DOIT_ENABLE_PORTAL=ON -DOIT_ENABLE_PEER=ON
cmake --build experiments/clipboard/build/ubuntu -j
```

Comprobar que la configuración incluye el adaptador del portal y el transporte OpenSSL. Esas opciones pueden producir una variante limitada si faltan dependencias: que CMake termine no demuestra que ambos componentes estén incluidos.

Desde `experiments/clipboard`, ejecutar únicamente el self-test inicialmente:

```bash
./build/ubuntu/oneintwo-clipboard --self-test
```

Referencia previa: 151 comprobaciones Linux con portal. La compilación Linux con TLS aún no tiene evidencia. Registrar el resultado real; no reutilizar la cifra de una variante distinta.

### Archivos

Comprobar GTK3 y OpenSSL además de las herramientas comunes:

```bash
cmake -S experiments/files -B experiments/files/build/ubuntu \
  -DCMAKE_BUILD_TYPE=Release -DO2F_ENABLE_GTK=ON -DO2F_ENABLE_TLS=ON
cmake --build experiments/files/build/ubuntu -j
```

Comprobar que se incluyeron GTK3 y TLS: ambas dependencias pueden omitirse con advertencia. Desde `experiments/files`, para que los temporales del self-test queden dentro de su carpeta:

```bash
./build/ubuntu/oneintwo-files --self-test
```

Referencia Windows: 176 comprobaciones. No hay evidencia previa de compilación del adaptador Linux. No cerrar la validación por compilar la alternativa `clipboard_unavailable`.

### Teclado y ratón

En Wayland, comprobar GIO, libei, pkg-config y OpenSSL. Forzar el backend objetivo:

```bash
cmake -S experiments/input -B experiments/input/build/ubuntu-wayland \
  -DCMAKE_BUILD_TYPE=Release -DONEINTWO_WAYLAND=ON \
  -DONEINTWO_X11=OFF -DONEINTWO_WITH_TLS=ON
cmake --build experiments/input/build/ubuntu-wayland -j
```

Desde `experiments/input`:

```bash
./build/ubuntu-wayland/input_self_test
```

Referencia aislada Windows: 538 comprobaciones, incluyendo flujos Wayland con dobles. No demuestra la compilación o ejecución del adaptador nativo Linux. Si la sesión real es Xorg, documentarlo; el manual contiene una alternativa X11, que no sustituye la validación Wayland.

### Tratamiento de fallos

Si aparece un error, conservar el comando y mensaje relevantes, investigar su causa concreta y corregir únicamente los bloqueos de compilación o funcionamiento de estos experimentos dentro del encargo. Repetir solo las comprobaciones afectadas. Si el bloqueo es una dependencia o capacidad ausente, registrarlo y avanzar al siguiente experimento independiente. No modificar las condiciones del test para convertir un fallo real en éxito.

## 4. Etapa local: escritorio Ubuntu

Seguir el [manual de portapapeles](experiments/clipboard/MANUAL.md), el [de archivos](experiments/files/MANUAL.md) y el [de entrada](experiments/input/MANUAL.md), uno cada vez. Antes de una prueba que cambie el escritorio, dejar preparados los datos desechables, la salida y los permisos que debe conceder el usuario.

1. Portapapeles: lectura, publicación, pegado en editor, observación en segundo plano y permiso denegado. Comprobar que no se concedan dispositivos de entrada y que funcione con captura inactiva.
2. Archivos: generar datos sintéticos, publicar caché y pegar realmente en Nautilus. Comparar tamaño/hash, estructura y nombres. Distinguir bytes transferidos de pegado correcto.
3. Entrada: inventario de portales, permisos, atajo local y demostración acotada de inyección en una ventana desechable. No habilitar captura si no hay recuperación local fiable. No confundir un flujo con dobles con la respuesta real del compositor.

Los diálogos del sistema pueden necesitar intervención del usuario. Si el agente no puede observar la operación en el editor o explorador, preparar el paso para el usuario y registrar su observación como tal. No marcar un caso como superado basándose solo en la salida del programa.

## 5. Etapa entre equipos: coordinar antes de conectar

Requiere que el usuario o el agente del otro equipo prepare Windows. Antes de lanzar cada caso acordar:

- Revisión Git y experimento, ejecutables y dependencias de cada extremo.
- Direcciones y puerto concreto; origen/destino y orden de arranque.
- Certificados propios y comprobación de huellas. Intercambiar certificados públicos o huellas, nunca claves privadas.
- Ventana o carpeta de prueba, duración, salida de emergencia y resultado que se va a observar.

Primero comprobar identidad aceptada y rechazada sin control remoto; después sincronización de texto, copia de archivos y por último entrada con sesiones cortas. Registrar ambos sentidos. La recuperación de entrada se comprueba solo cuando ya se ha verificado la salida local.

Hasta que el otro extremo esté preparado, estos casos quedan pendientes. GitHub comparte fuentes y resultados; no es el canal de transporte de los prototipos.

## 6. Entrega de resultados

Actualizar los `RESULTS.md` existentes sin borrar evidencia Windows ni atribuirle resultados Linux. Separar:

1. Entorno y revisión verificada.
2. Implementado y, si hubo correcciones, qué cambió.
3. Compilación nativa con los componentes realmente incluidos.
4. Self-tests aislados.
5. Pruebas de escritorio observadas.
6. Pruebas de dos equipos observadas o pendientes.
7. Bloqueos concretos, decisiones propuestas y siguiente paso.

Actualizar [TODO.md](TODO.md) y el [resumen de experimentos](experiments/README.md) solo cuando exista evidencia nueva. No crear informes paralelos, unificar protocolos ni trasladar código al producto durante este encargo.

Para intercambiar correcciones y resultados por Git, conservar una revisión identificable y acordar su publicación con el usuario. No sobrescribir ni integrar automáticamente el trabajo de la otra máquina. La rama de validación permite revisar esos cambios desde Windows.
