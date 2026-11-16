#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace oneintwo::linux_notifications {

struct Presentation {
    std::string application_name;
    std::string title;
    std::string body;
    std::uint32_t replaces_id = 0;
    std::int32_t timeout_ms = -1; // The server decides the time of life.
};

struct PresentationResult {
    bool accepted = false;
    bool unavailable = false;
    std::uint32_t notification_id = 0;
    std::string detail;
};

// Port of presentation and withdrawal. The specification of notifications of
// desktop does not offer a general operation to read other people's notices, so
// This adapter only publishes and withdraws its own notices.
class Service {
public:
    virtual ~Service() = default;
    virtual bool available() const = 0;
    virtual PresentationResult present(const Presentation& presentation) = 0;
    virtual void withdraw(std::uint32_t notification_id) = 0;
};

// All operations are invoked from a single working thread of the adapter.
std::unique_ptr<Service> makeSessionService();

// Check without blocking that the graphic session announces a bus.
bool session_bus_configured();

} // namespace oneintwo::linux_notifications
