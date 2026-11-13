# Primera prueba LAN — Portapapeles Windows ↔ Ubuntu

Preparado: 5 de noviembre de 2026. Responsabilidad: coordinar una prueba acotada
con los binarios y adaptadores ya desarrollados, sin integrar la aplicación.
La conexión es bidireccional aunque Windows escuche y Ubuntu conecte.

## 1. Qué está preparado

- Asistente Python 3.8+ con biblioteca estándar para generar identidades y
  mostrar los comandos del prototipo real. No ejecuta el prototipo.
- Certificado público Windows y su SHA-256 en `public/`. Su clave privada existe
  solo en la copia Windows original, bajo `.local-state/lan-clipboard/windows/`.
- Muestras de texto distintas para cada sentido, sin datos personales.
- Puerto acordado: **47831/TCP**, distinto al de los otros experimentos.
- Windows escucha únicamente en la dirección LAN elegida. Ubuntu conecta.
- El código del prototipo verificado es la entrega `2ae444d`; las revisiones
  posteriores de preparación/documentación no sustituyen las pruebas reales.

Los certificados duran 14 días desde la generación. Este paquete no inicia una
conexión, modifica el cortafuegos, lee el portapapeles ni concede permisos.
La primera prueba real sigue pendiente. No arrancar al mismo tiempo otro
experimento de portapapeles, archivos o entrada.

## 2. Obtener la preparación en Ubuntu

Conservar cualquier cambio local. Desde una copia limpia, descargar y avanzar
solo si es posible hacerlo sin mezclar historias:

```bash
git fetch origin
git status --short --branch
git merge --ff-only origin/validation/windows
```

Si el avance se rechaza o hay cambios locales, conservarlos y resolver la
sincronización antes de probar. No usar reset destructivo ni forzar el push.
Anotar `git rev-parse HEAD` en ambos extremos; usar la misma revisión al empezar.

El ejecutable Ubuntu esperado es
`experiments/clipboard/build/ubuntu/oneintwo-clipboard`. Si la revisión trae
cambios de C++, reconstruir y verificar TLS y el portal con el entorno ya
validado en Ubuntu. Un cambio solo en este asistente no exige recompilar C++.
Consultar [MANUAL.md](../MANUAL.md) y [RESULTS.md](../RESULTS.md).

## 3. Completar la identidad Ubuntu

Desde la raíz del repositorio:

```bash
python3 -B experiments/clipboard/lan/prepare.py prepare --name ubuntu
```

Puede indicarse `--openssl /ruta/al/openssl` si no está en PATH. Si el agente usa
dependencias locales, mantener su entorno de bibliotecas restringido a las
herramientas y al binario de prueba, sin contaminar aplicaciones del escritorio.

El asistente conserva la clave en `.local-state/lan-clipboard/ubuntu/private.key`
y crea `public/ubuntu.pem` y `public/ubuntu.sha256`. No regenera una identidad
existente: comprueba que su certificado está vigente y corresponde a su clave.
Si la identidad está incompleta o caducada, detenerse y acordar su sustitución;
no sobrescribir una clave que todavía esté usando el otro extremo.

Publicar **solo** esos dos archivos públicos en la rama de trabajo Ubuntu:

```bash
git add -- experiments/clipboard/lan/public/ubuntu.pem experiments/clipboard/lan/public/ubuntu.sha256
git diff --cached --name-only
git commit -m "test: share Ubuntu public identity for clipboard LAN"
git push
```

El agente Ubuntu debe comunicar rama, commit, IP LAN y huella. Windows obtendrá
esa revisión y comprobará la huella antes de arrancar. Nunca intercambiar ni
subir `private.key`, directorios `.local-state`, paquetes o binarios.

## 4. Identidad Windows y preparación local

La máquina Windows original ya tiene preparada su identidad. Solo si se repite
la preparación **en esa misma máquina**:

```powershell
python -B experiments/clipboard/lan/prepare.py prepare --name windows
```

Una copia clonada en otra máquina no tiene esa clave privada: no debe regenerar
la identidad Windows sin acordar un certificado nuevo con el otro extremo.

El certificado público se comprueba independientemente mediante:

```bash
openssl x509 -in experiments/clipboard/lan/public/windows.pem -noout -fingerprint -sha256
```

La huella esperada también figura en `public/windows.sha256`. Comparar con la
huella comunicada desde Windows por la conversación de coordinación.
Verificar de igual modo el certificado público de Ubuntu antes de confiar en él.

En Windows, poner las DLL usadas en la compilación en el PATH de esa consola:

```powershell
$env:Path = "C:\Program Files\OpenSSL-Win64\bin;" + $env:Path
```

## 5. Acordar dirección y cortafuegos

Windows comunica su IP LAN actual al agente Ubuntu. No usar las direcciones de
WSL o de interfaces virtuales. La IP puede cambiar entre sesiones.

En la preparación del 5 de noviembre, la interfaz LAN Windows tiene perfil
**Public** y no hay escuchas en 47831. No se ha cambiado su perfil ni creado
una regla. Si el cortafuegos impide el acceso, preparar una excepción temporal
limitada al ejecutable, la IP Ubuntu, la IP Windows y el puerto. En PowerShell de
administración, sustituyendo ambas direcciones:

```powershell
$probe = (Resolve-Path 'experiments/clipboard/build/windows-ready/Release/oneintwo-clipboard.exe').Path
New-NetFirewallRule -DisplayName 'OneInTwo clipboard LAN test' -Direction Inbound -Action Allow -Profile Public -Protocol TCP -LocalPort 47831 -Program $probe -LocalAddress WINDOWS_IP -RemoteAddress UBUNTU_IP
```

Este documento no ejecuta esa regla. Confirmar el perfil actual y ajustar el
parámetro si ha cambiado. No cambiar el perfil ni ampliar la regla a todos los
programas, direcciones o redes para ocultar un fallo.
Al acabar, retirar únicamente la regla de este ensayo:

```powershell
Remove-NetFirewallRule -DisplayName 'OneInTwo clipboard LAN test'
```

## 6. Mostrar y ejecutar los comandos acordados

Una vez presentes ambos certificados públicos y cada clave privada en su
máquina, el asistente genera el comando exacto. Sustituir `WINDOWS_IP` por la
dirección comunicada. No añadir `--show-content`: la salida mostrará metadatos.

Windows:

```powershell
python -B experiments/clipboard/lan/prepare.py command --name windows --role serve --host WINDOWS_IP
```

Ubuntu:

```bash
python3 -B experiments/clipboard/lan/prepare.py command --name ubuntu --role connect --host WINDOWS_IP
```

`--binary RUTA` permite señalar otro binario equivalente. El asistente comprueba
la existencia del binario y de las identidades, muestra el comando y termina.
**No se ha iniciado una sesión por ejecutar `command`.** Copiar el comando
mostrado a la consola cuando ambos extremos estén preparados.

1. Preparar editores desechables con las dos muestras.
2. Arrancar el comando Windows y esperar el mensaje `listening=`.
3. Arrancar Ubuntu, conceder solo portapapeles y comprobar `devices=0`.
4. Confirmar las huellas y que ambos extremos completan TLS y saludo.
5. Ejecutar la matriz siguiente. Para empezar, dedicar un máximo de tres minutos
   a la sesión; finalizar manualmente con Ctrl+C en ambas consolas.

El prototipo no tiene un temporizador LAN automático: el límite de tres minutos
es de la sesión acordada con el usuario, no una garantía del programa. No usar
`eval` ni `Invoke-Expression` para ejecutar automáticamente el texto mostrado.

## 7. Primera matriz entre equipos

| Caso | Acción | Qué observar |
| --- | --- | --- |
| T0 | En un intento separado, Ubuntu fija su propio certificado en lugar del de Windows con `--peer-cert` | Rechazo TLS, sin saludo de aplicación ni datos remotos; restaurar luego el certificado correcto |
| T1 | Copiar `samples/windows.txt` en un editor Windows y pegar en el editor Ubuntu | Texto exacto, incluidos acentos y saltos de línea |
| T2 | Copiar `samples/ubuntu.txt` en Ubuntu y pegar en Windows | Texto exacto; no vuelve la muestra anterior |
| T3 | Dejar diez segundos sin nuevas copias | Ausencia de reenvíos continuos o cambios repetidos |
| T4 | Detener cliente Ubuntu, volver a conectarlo y hacer una copia nueva | Error/desconexión claros y nueva sincronización, sin restaurar automáticamente contenido antiguo |
| T5 | Ctrl+C en Ubuntu y Windows | Cierre de ambos procesos y sesiones, sin escuchas de este ensayo restantes |

La prueba negativa T0 debe estar coordinada: Windows puede seguir escuchando
tras rechazar al cliente. No confundir un error TLS con puerto inaccesible.
Ninguna prueba de este documento se da por superada por haber generado comandos.

## 8. Evidencia y final de sesión

Registrar resultados observados en [RESULTS.md](../RESULTS.md): revisión de ambos
equipos, roles, puerto, versiones, huellas públicas, resultado de cada caso y
errores. No registrar texto privado. Reutilizar las verificaciones Ubuntu y
Windows ya válidas; no repetir la investigación o las baterías generales.

Al terminar, cerrar los dos prototipos y retirar la regla temporal si se creó.
Las claves locales se conservan para la siguiente sesión hasta caducar; no
borrarlas ni regenerarlas silenciosamente. No se promete restauración del
contenido previo del portapapeles: volver a copiar manualmente lo que se necesite.

Después de completar portapapeles se coordinarán archivos y entrada con sus
manuales. Este paquete no inicia todavía esos experimentos; Ubuntu como origen
de entrada continúa limitado por InputCapture v1 frente al requisito v2.

## 9. Comprobación del asistente

```bash
python3 -B experiments/clipboard/lan/test_prepare.py
```

Tres pruebas aisladas cubren roles/argumentos reales, falta del certificado del
par, rechazo de extremos inválidos y quoting. La generación y comprobación de
la identidad Windows se han realizado sin sockets ni escritorio; no prueban
un handshake TLS. El certificado público no es un resultado funcional.
