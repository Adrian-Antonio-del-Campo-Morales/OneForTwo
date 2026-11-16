#include "profiles.hpp"

namespace oneintwo {

FeatureSet requestedFeatures(ProfileId profile) {
    switch (profile) {
    case ProfileId::usual:
        // Usually it does not grant for itself: let pass the authorized by the pair
        // and the explicit habilitation of each capacity.
        return FeatureSet{true, true, true, true, true, true, true, true, true};
    case ProfileId::sends_only:
        // Only shipments retain direct files, links and manual notices;
        // remove clipboard, input, video and automatic forwarding.
        return FeatureSet{false, false, false, true, true, false, false, true, false};
    case ProfileId::paused:
        break;
    }
    // An invalid value is interpreted as Pause.
    return FeatureSet{};
}

} // namespace oneintwo
