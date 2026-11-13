/*
 * OneInTwo — Transporte autenticado y cifrado.
 * Estado: estructura documental; implementación pendiente.
 *
 * Responsabilidades previstas:
 * - Gestionar conexiones, lecturas parciales, escrituras, cierre y errores usando una biblioteca de TLS mantenida.
 * - Separar canal interactivo y canal de datos voluminosos con colas acotadas y control de flujo.
 * - Intercalar bloques de archivos y fragmentos de vídeo dentro del canal de datos.
 * - Exponer identidad verificada y eventos de conexión a la sesión, sin aceptar automáticamente pares desconocidos.
 * - Aplicar límites de recepción antes de reservar memoria y evitar bloquear los callbacks del escritorio.
 * - Rechazar datos de canales secundarios aún no asociados a una sesión autorizada.
 * - No implementar criptografía propia ni decidir permisos funcionales; bibliotecas y mecanismo de vinculación pendientes.
 */

