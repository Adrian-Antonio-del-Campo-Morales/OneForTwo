#pragma once

#include "../features/profiles.hpp"

#include <set>
#include <string>

namespace oneintwo {

// Preferencias locales no secretas: perfil por par y aplicaciones autorizadas.
// No guarda contenidos, imágenes, credenciales ni historial.
class LocalStore {
public:
    explicit LocalStore(std::string path);

    // Un par sin preferencia empieza en Habitual; un valor inválido se
    // interpreta como Pausa.
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
