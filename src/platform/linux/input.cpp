/*
 * OneInTwo — Entrada nativa Linux.
 * Estado: estructura documental; implementación pendiente.
 *
 * Responsabilidades previstas:
 * - Integrar captura e inyección del backend Wayland validado a través de sesiones autorizadas y EIS.
 * - Traducir eventos y geometría, gestionar barreras, activación, desactivación y devolución del control.
 * - Respetar decisiones del compositor y comunicar pérdida de permisos/capacidades.
 * - Liberar recursos al cerrar y ejecutar recuperación sin esperar al equipo remoto.
 * - Usar portal_session.cpp para permisos; no aplicar la lógica del protocolo de red ni asumir un atajo de activación universal.
 */

