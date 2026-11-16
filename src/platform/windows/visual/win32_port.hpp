#pragma once

#include "../../../integration/feature_contracts.hpp"
#include "win32_capture_port.hpp"

#include <cstdint>

namespace oneintwo::windows {

std::unique_ptr<WindowsCapturePort> makeWin32CapturePort(const PlatformContext& context);

bool windowsGraphicsCaptureSupported();

// It retains a catch object for the HWND or HMONITOR indicated.
std::unique_ptr<NativeCapture> createWin32Capture(std::uint64_t handle, VisualSourceKind kind);

} // namespace oneintwo::windows
