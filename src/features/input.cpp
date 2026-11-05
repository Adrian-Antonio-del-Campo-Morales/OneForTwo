/*
 * OneInTwo — Control de teclado y ratón.
 * Estado: estructura documental; implementación pendiente.
 *
 * Responsabilidades previstas:
 * - Mantener estados local, transición, remoto y recuperación, separados del estado general de conexión.
 * - Mapear pantallas y bordes, transmitir eventos normalizados y devolver el control al origen.
 * - Mantener teclas/botones activos, secuencia y generación de control; descartar eventos obsoletos.
 * - Coordinar captura y liberación mediante el contrato de escritorio sin ejecutar red en sus callbacks.
 * - Implementar temporizador de seguridad en el receptor y recuperación local en el origen.
 * - Liberar estados ante pérdida de conexión, bloqueo, revocación o cierre; definir comportamiento de entrada física local.
 * - No convertir todos los eventos en texto ni asumir que todas las plataformas admiten iniciar captura con un atajo.
 */

