#pragma once

#include <cstdint>

namespace oneintwo {

// Current generation assigned by the session. The network format will be defined in protocol.
using SessionGeneration = std::uint64_t;

// The session evaluates this set by meaning. The profiles provide a local limit.
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

// Initial contract to apply the profile and consult the resulting authorisation.
class Session {
public:
    // Effective permissions = permissions granted by the profile limits pair.
    // Both sets start empty: no capacity is granted without
    // explicit user and negotiation empowerment.
    void applyProfileLimits(FeatureSet limits);
    FeatureSet effectiveSendFeatures() const;
    FeatureSet effectiveReceiveFeatures() const;

    // Private extensions of this delivery to connect session and coordinator.
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
