#pragma once

namespace oneintwo {

// Common result of an operation that depends on the desktop.
// a header for queries in the notification module
// they can declare it without including the entire desktop contract.
enum class DesktopResult {
    ready,
    cancelled,
    denied,
    unavailable,
    error,
};

} // namespace oneintwo
