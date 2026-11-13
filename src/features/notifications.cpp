#include "notifications.hpp"

/*
 * Avisos manuales y reenvío de notificaciones nuevas. Implementación pendiente.
 *
 * - Aplicar el permiso de cada modalidad y filtrar apps locales antes de enviar.
 * - Excluir los avisos de OneInTwo para evitar el eco en ambos sentidos.
 * - Validar texto, acotar colas y frecuencia, y deduplicar por generación y sentido.
 * - Presentar texto plano con la identidad verificada del par y sin acciones remotas.
 * - Retirar los pendientes de una autorización anterior sin reproducirlos al reconectar.
 * Ver docs/NOTIFICACIONES.md.
 */

/*
 * Inyectar escritorio, envío de sesión, reloj y preferencias de apps. Mantener contador por sentido, deduplicación y plazos de cola. Conectar la revocación del adaptador al cálculo de permisos. La guía de docs/NOTIFICACIONES.md describe entradas y validación.
 */
