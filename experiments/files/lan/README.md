# Primer ensayo de archivos Windows → Ubuntu

Preparación: 5 de noviembre de 2026. El experimento de sincronización de texto
está suspendido. Esta prueba utiliza únicamente el ejecutable de archivos;
la publicación de archivos en el portapapeles es parte de su pegado nativo.

## Estado y objetivo

- Windows: `192.168.1.147`, servidor de archivos en `47654/TCP`.
- Ubuntu: `192.168.1.146`, receptor y destino de pegado en Nautilus.
- Fuente Windows: `.local-state/lan-files/windows/datos`.
- Cinco archivos, 5.551.150 bytes, con archivo vacío, Unicode y carpetas vacías.
- [Manifiesto de comparación](windows-fixtures.json): estructura, tamaños y SHA-256 de los datos sintéticos.
- Binario Windows ya compilado con las correcciones de archivos de `2ae444d`.
- Se reutilizan las identidades TLS locales ya verificadas. No se ejecuta el
  prototipo de texto para usarlas.

La prueba se considera superada cuando el usuario pega realmente `datos` en
Nautilus y la comparación del destino coincide con el manifiesto. Recibir y
verificar la caché, por sí solo, no valida el pegado.

## Sincronización y preparación Ubuntu

Conservar los cambios locales y obtener `origin/validation/windows`. Avanzar
solo si es posible sin sobrescribir informes; no usar reset destructivo ni
forzar la publicación. Las ramas pueden tener documentación diferente, pero
antes de probar debe confirmarse que el código del experimento de archivos
coincide, incluidos sus cambios de finalización de recepción y publicación GTK.

Usar el binario `experiments/files/build/ubuntu/oneintwo-files` y las
dependencias locales Ubuntu ya verificadas, restringidas a ese proceso.
No hace falta repetir la batería ni los ensayos locales que ya tienen evidencia.

Preparar el destino **antes** de publicar, para no reemplazar luego la oferta
de archivos copiando una ruta de texto:

```bash
mkdir -p .local-state/lan-files/ubuntu/pegado
```

Abrir esa carpeta en Nautilus. Debe estar vacía. No copiar archivos hacia ella
mediante comandos: el contenido final debe proceder del pegado del usuario.
Si ya contiene datos de otro intento, usar otro destino vacío, sin borrarlos.

Comprobar los dos certificados públicos y las claves propias que ya existen:

- Windows fija Ubuntu: `1eaf59dcb2489296c37ac5aa755b03a70637f47371d4bdfa0aabd69e36685c1e`.
- Ubuntu fija Windows: `d9c9d49228fc82a3ddc31128dd111039fbed948d028a8011adaae72568744006`.

## Arranque y recepción

Windows inicia `--share` con la fuente sintética, certificado/clave propios,
huella Ubuntu fijada, dirección `192.168.1.147` y puerto 47654. Esperar su
confirmación explícita **«Windows sirviendo archivos»** antes de conectar.

Desde la raíz del repositorio, el comando Ubuntu es:

```bash
./experiments/files/build/ubuntu/oneintwo-files --receive-offer \
  --from 192.168.1.147:47654 \
  --pin d9c9d49228fc82a3ddc31128dd111039fbed948d028a8011adaae72568744006 \
  --cert .local-state/lan-clipboard/ubuntu/cert.pem \
  --key .local-state/lan-clipboard/ubuntu/private.key \
  --cache .local-state/lan-files/ubuntu/cache --publish --timeout 120000
```

Las rutas con `lan-clipboard` se refieren solo a los certificados ya creados;
no activan el experimento suspendido. No copiar ni subir las claves privadas.
Adaptar únicamente el entorno de bibliotecas al que ya se utilizó en Ubuntu.

No añadir `--to`: en este caso queremos comprobar el pegado en el explorador,
no una copia directa efectuada por el programa.

## Comprobación observable

1. El receptor completa TLS mutuo y recibe una oferta de cinco archivos.
2. Los cinco contenidos se verifican antes de publicarse en caché.
3. Aparece la ventana GTK. El usuario pulsa **Publicar archivos** y la deja abierta.
4. En Nautilus, el usuario pega con Ctrl+V en el destino ya preparado.
5. Debe aparecer una carpeta `datos`, incluidos `empty.dat` y las carpetas vacías.
6. El agente Ubuntu compara ese `pegado/datos` con `windows-fixtures.json`:
   mismas rutas de archivos y directorios, mismos tamaños y mismos SHA-256.
7. Registrar confirmación del usuario y resultado de la comparación por separado.
8. Cerrar la ventana GTK, recoger salida/log del receptor y comunicar el cierre
   a Windows para detener su servidor y retirar su regla temporal.

Los logs pueden omitir progreso hasta vaciarse sus buffers; no confundir ausencia
de salida inmediata con fallo de transferencia. El resultado debe apoyarse en
la salida del cliente, la observación y los hashes del destino.

## Límites de este primer caso

Solo transferencia Windows → Ubuntu mediante caché y pegado asistido. No se
certifican todavía la dirección inversa, pegado en Explorer, contenido bajo
demanda, publicación automática en Ubuntu, cancelación entre equipos o
colisiones. Se coordinarán después del primer resultado.

No activar teclado/ratón ni sincronización de texto. No iniciar pruebas extra
de certificados o reconexión en paralelo. Conservar los originales y el destino
como evidencia; no sobrescribir ni limpiar la caché mientras se esté pegando.

Los resultados se incorporarán a [RESULTS.md](../RESULTS.md), junto con la
revisión efectiva de ambos extremos y el resultado por archivo. Hasta observar
el caso, permanece pendiente.
