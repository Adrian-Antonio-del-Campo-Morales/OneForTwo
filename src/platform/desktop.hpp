#pragma once

#include "../features/notifications.hpp"
#include "../features/visual_share.hpp"

#include <functional>
#include <string>

/*
 * Contrato entre lógica común y escritorio. Las demás operaciones nativas
 * siguen descritas en sus adaptadores y en ARQUITECTURA.md.
 *
 * El escritorio mantiene la geometría, el estado de la sesión gráfica y los
 * permisos del sistema. Su adaptador de portapapeles coordina texto y archivos.
 * La captura de entrada y la inyección son capacidades distintas y deben
 * normalizar eventos y declarar su afinidad de hilo. La apertura de URL se
 * delega al navegador local tras validar y confirmar la oferta.
 */

namespace oneintwo {

enum class DesktopResult {
    ready,
    cancelled,
    denied,
    unavailable,
    error,
};

struct VisualCaptureCapabilities {
    bool monitor = false;
    bool window = false;
};

struct VisualCaptureCallbacks {
    std::function<void(VisualShareRef, VisualFrame)> frame;
    std::function<void(VisualShareRef, VisualSourceState)> state_changed;
};

class DesktopVisualCapture {
public:
    virtual ~DesktopVisualCapture() = default;
    virtual VisualCaptureCapabilities visualCapabilities() const = 0;

    // El adaptador retiene una sola fuente local por referencia.
    virtual void selectSource(VisualShareRef offer, VisualSourceKind kind,
                              std::function<void(VisualShareRef, DesktopResult)> selected) = 0;
    // Se llama tras la aceptación. Los fallos de adquisición cierran la fuente.
    virtual void startCapture(VisualShareRef offer, VisualCaptureCallbacks callbacks) = 0;
    // Cancela el selector pendiente o libera la fuente activa.
    virtual void stopCapture(VisualShareRef offer) = 0;
};

struct NotificationCapabilities {
    bool present = false;
    bool observe = false;
};

class DesktopNotifications {
public:
    virtual ~DesktopNotifications() = default;
    virtual NotificationCapabilities notificationCapabilities() const = 0;
    virtual void requestObservationPermission(std::function<void(DesktopResult)> complete) = 0;
    virtual void startObserving(std::function<void(ObservedNotification)> observed,
                                std::function<void(DesktopResult)> state_changed) = 0;
    virtual void stopObserving() = 0;

    // peer_name procede de la sesión. El adaptador presenta el contenido como texto.
    virtual void presentNotice(const std::string& peer_name,
                               const RemoteNotification& notice,
                               std::function<void(DesktopResult)> complete) = 0;
    virtual void withdrawNotices() = 0;
};

} // namespace oneintwo
