/*
 * OneInTwo — Sincronización de portapapeles de texto.
 * Estado: estructura documental; implementación pendiente.
 *
 * Responsabilidades previstas:
 * - Gestionar cambios locales y remotos, formatos admitidos, límites y prevención de eco.
 * - Asignar identificadores de operación y un orden lógico determinista para resolver copias concurrentes.
 * - Distinguir notificaciones de escrituras propias de copias nuevas del usuario.
 * - Aplicar texto mediante el contrato de escritorio y publicar cambios mediante la sesión autorizada.
 * - Invalidar operaciones pendientes al cambiar de sesión y no reenviar contenido antiguo al reconectar.
 * - Las ofertas y el contenido de archivos pertenecen a files.cpp; no escribir el portapapeles por una segunda vía nativa.
 */

