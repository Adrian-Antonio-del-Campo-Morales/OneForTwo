#include "session.hpp"

namespace oneintwo {
namespace {

FeatureSet intersect(FeatureSet left, FeatureSet right) {
    return FeatureSet{
        left.text_clipboard && right.text_clipboard,
        left.file_clipboard && right.file_clipboard,
        left.input && right.input,
        left.direct_files && right.direct_files,
        left.links && right.links,
        left.share_monitor && right.share_monitor,
        left.share_window && right.share_window,
        left.manual_notifications && right.manual_notifications,
        left.forwarded_notifications && right.forwarded_notifications,
    };
}

} // namespace

void Session::applyProfileLimits(FeatureSet limits) {
    limits_ = limits;
}

FeatureSet Session::effectiveSendFeatures() const {
    return intersect(granted_send_, limits_);
}

FeatureSet Session::effectiveReceiveFeatures() const {
    return intersect(granted_receive_, limits_);
}

void Session::setGrantedFeatures(FeatureSet send, FeatureSet receive) {
    granted_send_ = send;
    granted_receive_ = receive;
}

FeatureSet Session::grantedSendFeatures() const {
    return granted_send_;
}

FeatureSet Session::grantedReceiveFeatures() const {
    return granted_receive_;
}

FeatureSet Session::profileLimits() const {
    return limits_;
}

} // namespace oneintwo
