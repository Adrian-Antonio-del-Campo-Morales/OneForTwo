# Experimento de copiar y pegar archivos

La siguiente prueba acordada es [Windows → Ubuntu, caché y pegado en Nautilus](lan/README.md). Incluye el manifiesto de datos sintéticos y el comando receptor; el resultado entre equipos sigue pendiente hasta observarlo.

## Encargo para el agente

Implementa este experimento siguiendo [las instrucciones comunes](../AGENTS.md) y [el modelo de entrega](../README.md). Tu directorio exclusivo es `D:/DEVEL/OneInTwo/experiments/files`. Las operaciones sobre portapapeles y exploradores reales se ejecutarán manualmente.

## Pregunta que debe resolver

¿Cómo representar una oferta de archivos remotos para que Explorer y Nautilus puedan pegarlos? ¿Es suficiente una caché local o resulta viable entregar contenido bajo demanda?

El alcance es copiar un archivo y una carpeta de prueba en ambos sentidos. No incluye cortar, borrar originales, sincronizar carpetas, montar un sistema de archivos virtual ni compartir todo el disco.

## Diseño superficial

Un ejecutable con componentes pequeños:

1. Adaptador de oferta de archivos en el portapapeles de cada sistema.
2. Manifiesto de archivos/carpetas y mapa de identificadores a archivos autorizados.
3. Transferencia por bloques y validación de integridad.
4. Entrega local al explorador y control de vida de temporales.

Este experimento implementará su acceso nativo al portapapeles de archivos dentro de su propia carpeta. No dependerá del agente de texto ni de sus resultados.

La transferencia podrá probarse localmente con fixtures sintéticos y manualmente entre equipos mediante dirección explícita. La representación del explorador debe contener archivos que pueda consumir, no solo una ruta remota pegada como texto.

## Dos alternativas delimitadas

**A. Caché local, prototipo funcional de referencia.** Recibir contenido en una carpeta privada de prueba, verificarlo y anunciar archivos locales listos para pegar. Mostrar que el contenido está preparado antes del pegado. Evitar eliminarlo mientras el explorador lo necesite.

**B. Entrega bajo demanda, prueba acotada de viabilidad.** Investigar la API nativa y probar con un archivo si permite entregar contenido cuando el explorador lo solicite. La prueba podrá usar contenido sintético local antes de conectarlo a la red. Documentar tiempos de espera, cancelación y diferencias entre sistemas.

No construir un proveedor de sistema de archivos completo para forzar B. Si requiere esa ampliación o no hay una vía documentada viable, entregar A y una conclusión fundamentada sobre B. El objetivo es comparar alternativas, no prometer que ambas serán posibles.

## Garantías del prototipo

Usar únicamente datos desechables dentro de la carpeta de prueba elegida. Conservar originales, no sobrescribir silenciosamente, rechazar rutas fuera del destino y tipos especiales no soportados. Los identificadores remotos solo pueden acceder a elementos ofrecidos.

Guardar transferencias incompletas separadas, validar contenido y mostrar errores sin publicar un archivo incompleto como terminado. Los logs no incluirán contenidos ni rutas personales por defecto.

## Qué podrá comprobar el agente automáticamente

- Manifiestos, Unicode en nombres y límites.
- Archivos vacíos y carpetas con datos sintéticos.
- Bloques, integridad, truncado y cancelación.
- Colisiones y rutas que intenten escapar del destino.
- Vida de ofertas y temporales con un consumidor falso.

Las pruebas de archivos usarán un temporal dentro del directorio del experimento. No abrirán exploradores, publicarán ofertas nativas ni inicializarán el portapapeles.

## Guion que debe preparar para ejecución manual

1. Crear el conjunto de datos de prueba.
2. Publicar una oferta local y pegarla en Explorer o Nautilus.
3. Probar archivo, carpeta y archivo vacío, comprobando tamaño y hash.
4. Conectar ambos equipos y repetir Windows→Ubuntu y Ubuntu→Windows.
5. Comparar caché y prueba bajo demanda donde esta exista.
6. Cancelar o interrumpir una transferencia y verificar que no aparece como completada.
7. Probar colisiones y un destino sin escritura.
8. Cerrar el prototipo y revisar cuándo se eliminan sus temporales.

## Entregable y criterio de evaluación

Código, compilación independiente, self-tests, MANUAL.md y RESULTS.md. La tabla de resultados distinguirá transferencia de contenido, publicación nativa y pegado real en cada explorador, además del coste de caché y límites de B.

Enviar correctamente bytes no prueba que se pueda pegar en el explorador. El pegado nativo y la experiencia entre dos equipos permanecerán pendientes hasta su ejecución manual.

La decisión arquitectónica esperada es qué estrategia de entrega usar y qué responsabilidad tendrá cada adaptador de archivos.

## Implementación entregada (4 de noviembre de 2026)

El prototipo está implementado y compilado en Windows; las pruebas de escritorio
siguen pendientes y se describen en [MANUAL.md](MANUAL.md). El estado y la
evidencias están en [RESULTS.md](RESULTS.md).

```text
files/
├── CMakeLists.txt          compilación independiente (C++17)
├── MANUAL.md               ejecución manual, permisos y matriz de resultados
├── RESULTS.md              estado, evidencia y límites
├── src/core/               manifiesto, transferencia, caché, protocolo, TLS
├── src/platform/win/       CF_HDROP (A) y FILEDESCRIPTOR/FILECONTENTS (B)
├── src/platform/linux/     x-special/gnome-copied-files y text/uri-list (A)
└── tests/                  176 comprobaciones aisladas
```

Decisiones principales:

1. **A (caché) es la referencia**: se descarga, se verifica por archivo y solo
   entonces se publica contenido local.
2. **B (bajo demanda) es una prueba acotada de Windows**: descriptor OLE y
   streams que piden rangos al origen. En Linux no está implementada: el adaptador
   publica solo rutas `file://` locales. Nautilus acepta URIs válidas (no solo
   `file://`) y GVfs
   permite esquemas por D-Bus, así que B exigiría un backend GIO/GVfs o FUSE; es
   una limitación de esta entrega, no una imposibilidad demostrada (RESULTS.md §5.1).
3. El par remoto nunca recibe rutas absolutas: solo rutas relativas validadas e
   identificadores de la oferta vigente.
4. Los archivos incompletos viven en `incoming/` y nunca se publican; `offers/`
   contiene solo contenido verificado con SHA-256.
5. El modo remoto exige TLS mutuo con ambas huellas fijadas: el cliente verifica
   el servidor y el servidor exige el certificado del cliente (`--client-pin`).
   Sin OpenSSL se bloquea; no se degrada a texto plano ni se omite la identidad
   de ninguna de las dos partes.

Uso rápido: `--self-test`, `--make-fixtures DIR`, `--inspect RUTA...` y
`--fingerprint CERT.pem`; después, según el manual, `--publish-cache`,
`--publish-ondemand`, `--share` y `--receive-offer`.

