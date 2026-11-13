#include "../desktop.hpp"

/*
 * Selección y captura local mediante ScreenCast y PipeWire en Wayland.
 * Implementación pendiente de validar soporte de MONITOR y WINDOW por separado.
 * Usar el ciclo de vida común de portal_session.cpp para una sesión de ScreenCast
 * propia y cerrar el stream al perder la fuente, la autorización o la sesión gráfica.
 * Ver docs/COMPARTIR_PANTALLA.md y docs/COMPARTIR_VENTANA.md.
 */
