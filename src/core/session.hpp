#pragma once

#include <cstdint>

namespace oneintwo {

// Generación vigente asignada por la sesión. El formato de red se definirá en protocolo.
using SessionGeneration = std::uint64_t;

// La sesión evalúa este conjunto por sentido. Los perfiles aportan un límite local.
struct FeatureSet {
    bool text_clipboard = false;
    bool file_clipboard = false;
    bool input = false;
    bool direct_files = false;
    bool links = false;
    bool share_monitor = false;
    bool share_window = false;
    bool manual_notifications = false;
    bool forwarded_notifications = false;
};

// Contrato inicial para aplicar el perfil y consultar la autorización resultante.
class Session {
public:
    // Permisos efectivos = permisos concedidos por el par ∩ límites del perfil.
    // Ambos conjuntos empiezan vacíos: ninguna capacidad se concede sin
    // habilitación explícita del usuario y de la negociación.
    void applyProfileLimits(FeatureSet limits);
    FeatureSet effectiveSendFeatures() const;
    FeatureSet effectiveReceiveFeatures() const;

    // Ampliaciones privadas de esta entrega para conectar sesión y coordinador.
    void setGrantedFeatures(FeatureSet send, FeatureSet receive);
    FeatureSet grantedSendFeatures() const;
    FeatureSet grantedReceiveFeatures() const;
    FeatureSet profileLimits() const;

private:
    FeatureSet granted_send_{};
    FeatureSet granted_receive_{};
    FeatureSet limits_{true, true, true, true, true, true, true, true, true};
};

} // namespace oneintwo
