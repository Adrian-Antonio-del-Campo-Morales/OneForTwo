#include "visual_share.hpp"

/*
 * Módulo visual común para monitor y ventana. Implementación pendiente.
 *
 * - Mantener una operación pendiente o activa y su modalidad autorizada.
 * - Solicitar selección local y abrir la adquisición tras aceptar y comprobar permisos.
 * - Validar identidad, revisión de fuente, caducidad, secuencia y límites de imagen.
 * - Coordinar la codificación, las colas acotadas y la recuperación por imagen clave.
 * - Vaciar el visor al suspender la fuente y cerrar la operación al perder la autorización.
 * - Descartar resultados nativos y datos tardíos tras cerrar o cambiar de generación.
 * Ver docs/COMPARTIR_PANTALLA.md y docs/COMPARTIR_VENTANA.md.
 */

/*
 * Implementar primero selección e invitación con reloj y salidas inyectadas. Serializar callbacks antes de tocar estado. Mantener referencia, fase, revisión, secuencia y plazos. Completar las salidas hacia sesión y UI desde el coordinador. La guía de docs/COMPARTIR_PANTALLA.md describe el recorrido y las decisiones de codec y protocolo pendientes.
 */
