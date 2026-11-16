#pragma once

#include "../features/profiles.hpp"

#include <set>
#include <string>

namespace oneintwo {

// Non-secret local preferences: peer profile and authorized applications.
// It doesn't save content, images, credentials, or history.
class LocalStore {
public:
    explicit LocalStore(std::string path);

    // A pair without preference starts in Custom; an invalid value is
    // interprets as Pause.
    ProfileId loadProfile(const std::string& peer) const;
    void saveProfile(const std::string& peer, ProfileId profile);

    std::set<std::string> loadAllowedApplications() const;
    void saveAllowedApplications(const std::set<std::string>& app_ids);

private:
    void load();
    bool save() const;

    std::string path_;
    std::string peer_device_;
    ProfileId profile_ = ProfileId::usual;
    bool has_profile_ = false;
    std::set<std::string> allowed_applications_;
};

} // namespace oneintwo
