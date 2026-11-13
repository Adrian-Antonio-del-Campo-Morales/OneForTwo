#include "../desktop.hpp"
#include "../../integration/feature_contracts.hpp"
#include "visual/pipewire_source.hpp"
#include "visual/portal_screencast.hpp"
#include "visual/wayland_visual_capture.hpp"

/*
 * Selección y captura local de monitores y ventanas en Wayland mediante
 * ScreenCast y PipeWire. El tipo solicitado se comprueba contra el anuncio
 * del portal y contra la fuente devuelta; sin atestación se informa de
 * indisponibilidad en lugar de recortar una región del escritorio.
 */

namespace oneintwo {

std::unique_ptr<DesktopVisualCapture> makeWaylandVisualCapture(PlatformContext context)
{
    wayland::WaylandCaptureDependencies dependencies;
    dependencies.portal = [] { return wayland::makePortalScreencastClient(); };
    dependencies.pipewire = [] { return wayland::makePipeWireSource(); };
    return wayland::makeWaylandVisualCaptureWithDependencies(std::move(context),
                                                             std::move(dependencies));
}

} // namespace oneintwo
