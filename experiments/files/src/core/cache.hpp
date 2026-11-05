// Responsabilidad: caché privada de recepción. Separa datos incompletos,
// publica ofertas verificadas, conserva concesiones de uso y elimina temporales
// solo cuando la política lo permite.
#pragma once

#include "core/model.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace o2f {

class Lease {
public:
    ~Lease();
    Lease(const Lease&) = delete;
    Lease& operator=(const Lease&) = delete;

private:
    friend class CacheStore;
    explicit Lease(std::filesystem::path file);
    std::filesystem::path file_;
};

struct CachedOffer {
    Offer offer;
    std::filesystem::path dir;
};

class CacheStore {
public:
    explicit CacheStore(const std::string& rootUtf8);

    const std::filesystem::path& root() const { return root_; }
    std::filesystem::path newIncomingDir(const std::string& offerId);
    void abandonIncoming(const std::filesystem::path& dir);

    // Mueve una recepción completa a offers/ y escribe su manifiesto.
    std::filesystem::path commitIncoming(const std::filesystem::path& incomingDir,
                                         const Offer& offer);

    std::filesystem::path findOfferDir(const std::string& offerId) const;
    std::unique_ptr<Lease> acquire(const std::string& offerId);
    std::vector<CachedOffer> list() const;

    // Elimina ofertas antiguas sin concesión vigente y recepciones abandonadas.
    std::size_t sweep(std::uint64_t maxAgeSeconds, std::uint64_t now);

private:
    std::filesystem::path root_;
};

}  // namespace o2f
