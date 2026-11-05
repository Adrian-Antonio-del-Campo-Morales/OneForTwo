# Instrucciones para agentes de experimentos

## Alcance y propiedad

Estas instrucciones se aplican a los tres experimentos. El encargo recibido debe identificar exactamente uno: `clipboard`, `input` o `files`. Lee el README de ese directorio y el README común antes de trabajar.

Modifica exclusivamente tu subdirectorio asignado, incluidos código, pruebas, documentación, descargas locales permitidas y salidas de compilación. No edites este archivo, el README común, el estudio, la arquitectura, el CMake raíz, el código de producto ni los otros experimentos. Si necesitas un cambio común, descríbelo en tu RESULTS.md.

No hagas commits, cambios de rama, limpieza general del repositorio ni operaciones sobre archivos ajenos como parte de este encargo. Trabaja con los archivos existentes sin sobrescribir modificaciones ajenas.

## Objetivo de la fase

Implementar el prototipo y dejarlo listo para que el usuario lo ejecute manualmente. Está autorizado compilar y ejecutar pruebas aisladas de tu código. No ejecutar operaciones sobre el escritorio real ni iniciar comunicación de red de prueba.

En particular, no leer/escribir el portapapeles, capturar/inyectar dispositivos, abrir diálogos de portales, crear ofertas en el explorador ni arrancar procesos de demostración. No ejecutar ciegamente una suite que contenga esos casos. Los self-tests solo deben usar datos sintéticos y no inicializar esos servicios.

Consulta documentación y declara dependencias. No instales herramientas globalmente, no cambies permisos/cortafuegos y no eleves el proceso. Si falta una dependencia, documenta el bloqueo y continúa con el trabajo independiente.

## Diseño y entrega

- Implementación propia; no reutilizar motores de aplicaciones existentes.
- C++ provisional y compilación independiente dentro de tu carpeta.
- Evitar infraestructura de producto: cuentas, descubrimiento, actualizadores, instaladores y UI completa.
- Arranque sin argumentos inocuo; modos reales siempre explícitos.
- Ningún stub puede informar éxito como si fuera una integración nativa.
- README con decisiones, MANUAL con comandos reproducibles, RESULTS con evidencia.
- Cada archivo de código debe describir su responsabilidad.
- Limitar datos de prueba y logs; no registrar contenido privado.
- Proteger o ignorar material privado y artefactos generados.
- Separar implementado, compilado, self-test verificado y ejecución manual pendiente.
- Si no hay acceso a un sistema operativo, no afirmar que se ha validado.

No iniciar nuevos agentes para este encargo. Las tres carpetas están diseñadas para poder trabajar sin coordinación de edición ni de sesiones gráficas.

## Condición de finalización

Entregar el prototipo dentro del alcance del README, las comprobaciones aisladas posibles y el manual. Enumerar limitaciones concretas y trabajo que no pudo completarse. No ampliar el proyecto para resolver mejoras opcionales ni realizar pruebas manuales por iniciativa propia.

