#pragma once

#include "../../../integration/feature_contracts.hpp"
#include "../../../platform/desktop.hpp"
#include "pipewire_source.hpp"
#include "portal_screencast.hpp"

#include <functional>
#include <memory>

/*
 * Monitor and window capture adapter for Wayland.
 *
 * selectSource opens a ScreenCast session of its own and retains it until the operation ends. The
 * source's effective type is validated with the portal's response; without that attestation the
 * operation is not available. startCapture consumes only the authorized node with PipeWire.
 *
 * Injecting factories allow you to test the package with a simulated portal and a fake PipeWire
 * consumer without editing components from other branches.
 */

namespace oneintwo::wayland {

struct WaylandCaptureDependencies {
    std::function<std::unique_ptr<PortalScreencastClient>()> portal;
    std::function<std::unique_ptr<PipeWireSource>()> pipewire;
};

std::unique_ptr<DesktopVisualCapture> makeWaylandVisualCaptureWithDependencies(
    PlatformContext context, WaylandCaptureDependencies dependencies);

} // namespace oneintwo::wayland
