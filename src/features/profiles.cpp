#include "profiles.hpp"

namespace oneintwo {

FeatureSet requestedFeatures(ProfileId profile) {
    switch (profile) {
    case ProfileId::usual:
        // Habitual no concede por sí mismo: deja pasar lo autorizado por el par
        // y por la habilitación explícita de cada capacidad.
        return FeatureSet{true, true, true, true, true, true, true, true, true};
    case ProfileId::sends_only:
        // Solo envíos conserva archivos directos, enlaces y avisos manuales;
        // retira portapapeles, entrada, vídeo y reenvío automático.
        return FeatureSet{false, false, false, true, true, false, false, true, false};
    case ProfileId::paused:
        break;
    }
    // Un valor inválido se interpreta como Pausa.
    return FeatureSet{};
}

} // namespace oneintwo
