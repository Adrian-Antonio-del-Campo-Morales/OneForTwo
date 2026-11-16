#include "../desktop.hpp"
#include "../../integration/feature_contracts.hpp"
#include "visual/pipewire_source.hpp"
#include "visual/portal_screencast.hpp"
#include "visual/wayland_visual_capture.hpp"

/*
 * Local selection and capture of monitors and windows in Wayland via ScreenCast and PipeWire. The
 * type requested is checked against the portal ad and against the returned source; without
 * attestation unavailability is reported instead of trimming a desktop region.
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
