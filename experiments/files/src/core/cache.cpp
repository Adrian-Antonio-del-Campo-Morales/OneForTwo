// Responsabilidad: diseño de la caché en disco, concesiones de uso y limpieza.
// Estructura: offers/<id>/ (verificado), incoming/<id>/ (sin verificar),
// leases/<id>.lease (consumidor activo).
#include "core/cache.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <system_error>

#include "core/manifest.hpp"

namespace fs = std::filesystem;

namespace o2f {
namespace {

constexpr std::uint64_t kStaleLeaseSeconds = 24 * 60 * 60;

std::string utf8(const fs::path& p) { return p.u8string(); }

Offer readOfferFromDir(const fs::path& dir) {
    std::ifstream in(dir / "manifest.tsv", std::ios::binary);
    if (!in) throw Error("la oferta de la caché no tiene manifiesto");
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return parseManifestText(text, Limits{});
}

}  // namespace

Lease::Lease(fs::path file) : file_(std::move(file)) {}
Lease::~Lease() {
    std::error_code ec;
    fs::remove(file_, ec);
}

CacheStore::CacheStore(const std::string& rootUtf8) : root_(fs::u8path(rootUtf8)) {
    std::error_code ec;
    fs::create_directories(root_ / "offers", ec);
    fs::create_directories(root_ / "incoming", ec);
    fs::create_directories(root_ / "leases", ec);
    if (!fs::is_directory(root_ / "offers") || !fs::is_directory(root_ / "incoming")) {
        throw Error("no se pudo preparar la carpeta de caché");
    }
}

fs::path CacheStore::newIncomingDir(const std::string& offerId) {
    const std::string label = offerId.substr(0, 12);
    const fs::path dir = root_ / "incoming" / (label + "-" + randomId(4));
    std::error_code ec;
    if (!fs::create_directories(dir, ec) || ec) {
        throw Error("no se pudo crear el directorio de recepción");
    }
    return dir;
}

void CacheStore::abandonIncoming(const fs::path& dir) {
    std::error_code ec;
    fs::remove_all(dir, ec);
}

fs::path CacheStore::commitIncoming(const fs::path& incomingDir, const Offer& offer) {
    if (!isValidOpaqueId(offer.id, 64)) {
        throw Error("la oferta recibida tiene un identificador no utilizable");
    }
    std::ofstream manifest(incomingDir / "manifest.tsv", std::ios::binary | std::ios::trunc);
    if (!manifest) throw Error("no se pudo escribir el manifiesto de la oferta");
    const std::string text = serializeManifestText(offer);
    manifest.write(text.data(), static_cast<std::streamsize>(text.size()));
    manifest.close();
    if (!manifest) throw Error("fallo al guardar el manifiesto de la oferta");

    std::error_code ec;
    fs::path target = root_ / "offers" / offer.id.substr(0, 24);
    int n = 1;
    while (fs::exists(target, ec)) {
        target = root_ / "offers" / (offer.id.substr(0, 24) + "-" + std::to_string(++n));
        if (n > 1000) throw Error("no se pudo asignar carpeta de oferta");
    }
    fs::rename(incomingDir, target, ec);
    if (ec) throw Error("no se pudo publicar la oferta verificada en la caché");
    return target;
}

fs::path CacheStore::findOfferDir(const std::string& offerId) const {
    std::error_code ec;
    for (fs::directory_iterator it(root_ / "offers", ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_directory(ec)) continue;
        try {
            const Offer offer = readOfferFromDir(it->path());
            if (offer.id == offerId) return it->path();
        } catch (const Error&) {
            continue;
        }
    }
    return {};
}

std::unique_ptr<Lease> CacheStore::acquire(const std::string& offerId) {
    const fs::path dir = findOfferDir(offerId);
    if (dir.empty()) throw Error("la oferta solicitada no está en la caché");
    const fs::path leaseFile = root_ / "leases" / (offerId + ".lease");
    std::ofstream out(leaseFile, std::ios::binary | std::ios::trunc);
    if (!out) throw Error("no se pudo registrar la concesión de uso");
    out << unixNow() << '\n';
    out.close();
    if (!out) throw Error("no se pudo registrar la concesión de uso");
    return std::unique_ptr<Lease>(new Lease(leaseFile));
}

std::vector<CachedOffer> CacheStore::list() const {
    std::vector<CachedOffer> offers;
    std::error_code ec;
    for (fs::directory_iterator it(root_ / "offers", ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_directory(ec)) continue;
        try {
            CachedOffer cached;
            cached.offer = readOfferFromDir(it->path());
            cached.dir = it->path();
            offers.push_back(std::move(cached));
        } catch (const Error&) {
            continue;
        }
    }
    std::sort(offers.begin(), offers.end(), [](const CachedOffer& a, const CachedOffer& b) {
        return a.offer.createdAt < b.offer.createdAt;
    });
    return offers;
}

std::size_t CacheStore::sweep(std::uint64_t maxAgeSeconds, std::uint64_t now) {
    std::size_t removed = 0;
    std::error_code ec;
    std::vector<fs::path> offerDirs;
    for (fs::directory_iterator it(root_ / "offers", ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_directory(ec)) offerDirs.push_back(it->path());
    }
    std::vector<fs::path> incomingDirs;
    for (fs::directory_iterator it(root_ / "incoming", ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_directory(ec)) incomingDirs.push_back(it->path());
    }

    const auto fileNow = fs::file_time_type::clock::now();
    auto ageSeconds = [&fileNow](fs::file_time_type time) {
        return std::chrono::duration_cast<std::chrono::seconds>(fileNow - time).count();
    };

    for (const fs::path& dir : offerDirs) {
        std::uint64_t createdAt = 0;
        std::string offerId;
        try {
            const Offer parsed = readOfferFromDir(dir);
            createdAt = parsed.createdAt;
            offerId = parsed.id;
        } catch (const Error&) {
            createdAt = 0;
        }
        if (createdAt == 0) {
            std::error_code mtimeEc;
            const auto mtime = fs::last_write_time(dir, mtimeEc);
            if (mtimeEc) continue;
            createdAt = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::seconds>(mtime.time_since_epoch()).count());
        }
        if (now >= createdAt && now - createdAt < maxAgeSeconds) continue;

        const fs::path leaseFile =
            offerId.empty() ? fs::path() : root_ / "leases" / (offerId + ".lease");
        std::error_code leaseEc;
        if (!leaseFile.empty() && fs::exists(leaseFile, leaseEc)) {
            const auto mtime = fs::last_write_time(leaseFile, leaseEc);
            const auto leaseAge = leaseEc ? -1 : ageSeconds(mtime);
            if (leaseAge >= 0 &&
                static_cast<std::uint64_t>(leaseAge) < kStaleLeaseSeconds) {
                continue;  // un consumidor mantiene la oferta
            }
            std::error_code removeLeaseEc;
            fs::remove(leaseFile, removeLeaseEc);
        }
        std::error_code rmEc;
        fs::remove_all(dir, rmEc);
        if (!rmEc) ++removed;
    }

    for (const fs::path& dir : incomingDirs) {
        std::error_code mtimeEc;
        const auto mtime = fs::last_write_time(dir, mtimeEc);
        if (mtimeEc) continue;
        const auto age = ageSeconds(mtime);
        if (age >= 0 && static_cast<std::uint64_t>(age) < maxAgeSeconds) continue;
        std::error_code rmEc;
        fs::remove_all(dir, rmEc);
        if (!rmEc) ++removed;
    }
    return removed;
}

}  // namespace o2f
