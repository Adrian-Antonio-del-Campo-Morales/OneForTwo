/*
 * OneInTwo — Sesión entre dos equipos y autorización.
 * Estado: estructura documental; implementación pendiente.
 *
 * Responsabilidades previstas:
 * - Mantener identidad remota, estado de vinculación, permisos efectivos y capacidades negociadas.
 * - Separar equipo descubierto, canal establecido y equipo autenticado autorizado.
 * - Asignar una nueva generación al reconectar y resolver intentos simultáneos de conexión.
 * - Autorizar y despachar mensajes validados hacia el módulo correspondiente; denegar tipos fuera de estado.
 * - Vincular los canales de datos a la identidad y generación del canal interactivo.
 * - Notificar pérdida de conexión y revocación sin reproducir acciones antiguas.
 * - Usar transporte y almacenamiento mediante dependencias explícitas; no capturar entrada ni dibujar ventanas.
 */

