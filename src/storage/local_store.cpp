#include "local_store.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace oneintwo {
namespace {

bool validToken(const std::string& value) {
    return !value.empty() && value.find('\t') == std::string::npos &&
           value.find('\n') == std::string::npos && value.find('\r') == std::string::npos;
}

std::string profileName(ProfileId profile) {
    switch (profile) {
    case ProfileId::usual:
        return "usual";
    case ProfileId::sends_only:
        return "sends_only";
    case ProfileId::paused:
        return "paused";
    }
    return "paused";
}

bool parseProfile(const std::string& value, ProfileId& profile) {
    if (value == "usual") {
        profile = ProfileId::usual;
        return true;
    }
    if (value == "sends_only") {
        profile = ProfileId::sends_only;
        return true;
    }
    if (value == "paused") {
        profile = ProfileId::paused;
        return true;
    }
    return false;
}

} // namespace

LocalStore::LocalStore(std::string path) : path_(std::move(path)) {
    load();
}

void LocalStore::load() {
    std::ifstream input(path_);
    if (!input) {
        return;
    }
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind("profile\t", 0) == 0) {
            const std::string rest = line.substr(8);
            const std::size_t separator = rest.find('\t');
            if (separator == std::string::npos) {
                continue;
            }
            const std::string peer = rest.substr(0, separator);
            const std::string value = rest.substr(separator + 1);
            ProfileId parsed = ProfileId::paused;
            if (!parseProfile(value, parsed)) {
                parsed = ProfileId::paused; // valor inválido: Pausa
            }
            if (peer_device_.empty()) {
                peer_device_ = peer;
                profile_ = parsed;
                has_profile_ = true;
            }
        } else if (line.rfind("app\t", 0) == 0) {
            const std::string app = line.substr(4);
            if (validToken(app)) {
                allowed_applications_.insert(app);
            }
        }
    }
}

bool LocalStore::save() const {
    if (path_.empty()) {
        return false;
    }
    std::error_code error;
    const std::filesystem::path path(path_);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
    }
    const std::string temporary = path_ + ".tmp";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) {
            return false;
        }
        if (has_profile_ && validToken(peer_device_)) {
            output << "profile\t" << peer_device_ << '\t' << profileName(profile_) << '\n';
        }
        for (const std::string& app : allowed_applications_) {
            if (validToken(app)) {
                output << "app\t" << app << '\n';
            }
        }
        if (!output) {
            return false;
        }
    }
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}

ProfileId LocalStore::loadProfile(const std::string& peer) const {
    if (has_profile_ && peer == peer_device_) {
        return profile_;
    }
    return ProfileId::usual;
}

void LocalStore::saveProfile(const std::string& peer, ProfileId profile) {
    if (!validToken(peer)) {
        return;
    }
    peer_device_ = peer;
    profile_ = profile;
    has_profile_ = true;
    save();
}

std::set<std::string> LocalStore::loadAllowedApplications() const {
    return allowed_applications_;
}

void LocalStore::saveAllowedApplications(const std::set<std::string>& app_ids) {
    allowed_applications_.clear();
    for (const std::string& app : app_ids) {
        if (validToken(app)) {
            allowed_applications_.insert(app);
        }
    }
    save();
}

} // namespace oneintwo
