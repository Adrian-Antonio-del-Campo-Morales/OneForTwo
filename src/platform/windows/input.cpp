/*
 * OneInTwo — Entrada nativa Windows.
 * Estado: estructura documental; implementación pendiente.
 *
 * Responsabilidades previstas:
 * - Capturar eventos de teclado/ratón y suprimir su efecto local cuando el control remoto esté activo.
 * - Inyectar eventos autorizados y convertir códigos, botones y coordenadas desde el contrato común.
 * - Distinguir eventos sintetizados para evitar bucles y mantener callbacks de captura cortos.
 * - Liberar captura y estados en el hilo adecuado; informar fallos o restricciones de privilegio.
 * - No ejecutar transferencias ni decidir qué equipo tiene permiso de control.
 */

