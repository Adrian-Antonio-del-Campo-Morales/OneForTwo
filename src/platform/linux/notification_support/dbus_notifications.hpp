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
    std::int32_t timeout_ms = -1; // El servidor decide el tiempo de vida.
};

struct PresentationResult {
    bool accepted = false;
    bool unavailable = false;
    std::uint32_t notification_id = 0;
    std::string detail;
};

// Puerto de presentación y retirada. La especificación de notificaciones de
// escritorio no ofrece una operación general para leer avisos ajenos, por eso
// este adaptador solo publica y retira avisos propios.
class Service {
public:
    virtual ~Service() = default;
    virtual bool available() const = 0;
    virtual PresentationResult present(const Presentation& presentation) = 0;
    virtual void withdraw(std::uint32_t notification_id) = 0;
};

// Todas las operaciones se invocan desde un único hilo de trabajo del adaptador.
std::unique_ptr<Service> makeSessionService();

// Comprobación sin bloqueo de que la sesión gráfica anuncia un bus.
bool session_bus_configured();

} // namespace oneintwo::linux_notifications
