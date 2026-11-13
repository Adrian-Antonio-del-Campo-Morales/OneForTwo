#pragma once

#include "../../../integration/feature_contracts.hpp"
#include "../../../platform/desktop.hpp"
#include "pipewire_source.hpp"
#include "portal_screencast.hpp"

#include <functional>
#include <memory>

/*
 * Adaptador de captura de monitor y ventana para Wayland.
 *
 * selectSource abre una sesión ScreenCast propia y la retiene hasta que la
 * operación termina. El tipo efectivo de la fuente se valida con la respuesta
 * del portal; sin esa atestación la operación no está disponible. startCapture
 * consume únicamente el nodo autorizado con PipeWire.
 *
 * Las fábricas inyectables permiten probar el paquete con un portal simulado y
 * un consumidor PipeWire falso sin editar componentes de otras ramas.
 */

namespace oneintwo::wayland {

struct WaylandCaptureDependencies {
    std::function<std::unique_ptr<PortalScreencastClient>()> portal;
    std::function<std::unique_ptr<PipeWireSource>()> pipewire;
};

std::unique_ptr<DesktopVisualCapture> makeWaylandVisualCaptureWithDependencies(
    PlatformContext context, WaylandCaptureDependencies dependencies);

} // namespace oneintwo::wayland
