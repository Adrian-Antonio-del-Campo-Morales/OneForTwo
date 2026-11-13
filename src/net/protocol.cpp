/*
 * OneInTwo — Codificación y validación del protocolo.
 * Estado: estructura documental; implementación pendiente.
 *
 * Responsabilidades previstas:
 * - Convertir mensajes del contrato en tramas y reconstruirlos desde bytes recibidos parcialmente.
 * - Validar longitudes, tipos, rangos y estructura antes del despacho hacia la sesión.
 * - Validar modalidad y campos de avisos, y limitar el reensamblado de unidades de vídeo.
 * - Informar errores de protocolo sin ejecutar operaciones del escritorio o del sistema de archivos.
 * - Mantener pruebas con datos malformados y compatibilidad de versión al implementar.
 * - La autorización contextual corresponde a session.cpp y las restricciones propias de operaciones a cada módulo.
 */

