#pragma once

#include "../../../features/visual_share.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

/*
 * Cliente de org.freedesktop.portal.ScreenCast.
 *
 * Se usa una sesión propia por operación: CreateSession, SelectSources con el
 * tipo exacto solicitado y una única fuente, Start y OpenPipeWireRemote. La
 * solicitud y la sesión se conservan juntas hasta que el consumidor cierra o
 * el portal emite Closed. La cancelación cierra también una solicitud que aún
 * espera respuesta.
 */

namespace oneintwo::wayland {

struct ScreenCastSupport {
    bool monitor = false;
    bool window = false;
};

struct ScreenCastSource {
    std::uint32_t node_id = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool has_source_type = false;
    std::uint32_t source_type = 0; // 1 monitor, 2 ventana, 4 virtual
    std::string stream_id;
    std::string mapping_id;
};

enum class ScreenCastStatus {
    ready,
    cancelled,
    denied,
    unavailable,
    error,
};

class PortalScreencastClient {
public:
    using CapabilitiesResult = std::function<void(ScreenCastSupport, bool available)>;
    using SessionResult = std::function<void(ScreenCastStatus, ScreenCastSource)>;
    using SessionClosed = std::function<void()>;

    virtual ~PortalScreencastClient() = default;

    virtual void probe(CapabilitiesResult complete) = 0;
    // Retiene la sesión portal y su descriptor PipeWire hasta close().
    // parent_window es el identificador local del padre del diálogo ("" si no hay).
    virtual void open(VisualSourceKind kind, std::string parent_window, SessionClosed on_closed,
                      SessionResult complete) = 0;
    // Respuesta terminal una sola vez; cierra una solicitud pendiente.
    virtual void cancel() = 0;
    // Libera sesión y descriptor. No entrega estados al consumidor.
    virtual void close() = 0;
    // Copia del descriptor autorizado, propiedad de quien la recibe.
    virtual int duplicatePipeWireFd() const = 0;
};

std::unique_ptr<PortalScreencastClient> makePortalScreencastClient();

} // namespace oneintwo::wayland
