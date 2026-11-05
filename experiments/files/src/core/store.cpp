// Responsabilidad: acceso de lectura acotado a los elementos de una oferta.
#include "core/store.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace o2f {

OfferStore OfferStore::fromPaths(const std::vector<std::string>& rootsUtf8, const Limits& limits) {
    BuiltOffer built = buildOffer(rootsUtf8, limits);
    OfferStore store;
    store.offer_ = std::move(built.offer);
    for (const Entry& e : store.offer_.entries) {
        const auto it = built.absoluteByRel.find(e.relPath);
        if (it == built.absoluteByRel.end()) throw Error("manifiesto interno incoherente");
        store.items_.emplace(e.id, SourceItem{e, fs::u8path(it->second)});
    }
    return store;
}

const SourceItem* OfferStore::find(const std::string& entryId) const {
    const auto it = items_.find(entryId);
    return it == items_.end() ? nullptr : &it->second;
}

std::size_t OfferStore::read(const std::string& entryId, std::uint64_t offset, void* buffer,
                             std::size_t len) const {
    const SourceItem* item = find(entryId);
    if (item == nullptr) throw Error("identificador no incluido en la oferta vigente");
    if (item->meta.isDir) throw Error("el elemento solicitado es una carpeta");

    std::error_code ec;
    const std::uintmax_t currentSize = fs::file_size(item->absolute, ec);
    if (ec) throw Error("el archivo de origen ya no está disponible");
    if (currentSize != item->meta.size) throw Error("el archivo de origen cambió de tamaño");
    if (offset > item->meta.size) throw Error("desplazamiento fuera del archivo ofrecido");
    if (offset == item->meta.size) return 0;

    const std::size_t want = static_cast<std::size_t>(
        std::min<std::uint64_t>(len, item->meta.size - offset));
    std::ifstream in(item->absolute, std::ios::binary);
    if (!in) throw Error("no se pudo abrir el archivo de origen");
    in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!in) throw Error("no se pudo posicionar el archivo de origen");
    in.read(static_cast<char*>(buffer), static_cast<std::streamsize>(want));
    const std::streamsize got = in.gcount();
    if (got < 0) throw Error("lectura fallida del archivo de origen");
    return static_cast<std::size_t>(got);
}

}  // namespace o2f
