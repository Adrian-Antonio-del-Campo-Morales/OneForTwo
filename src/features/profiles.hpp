#pragma once

#include "../core/session.hpp"

namespace oneintwo {

enum class ProfileId {
    usual,
    sends_only,
    paused,
};

// Devuelve los límites del perfil. La sesión calcula los permisos efectivos.
FeatureSet requestedFeatures(ProfileId profile);

} // namespace oneintwo
