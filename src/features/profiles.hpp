#pragma once

#include "../core/session.hpp"

namespace oneintwo {

enum class ProfileId {
    usual,
    sends_only,
    paused,
};

// Returns the profile limits. The session calculates the actual permissions.
FeatureSet requestedFeatures(ProfileId profile);

} // namespace oneintwo
