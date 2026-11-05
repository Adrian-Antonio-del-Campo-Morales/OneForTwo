// Responsabilidad: almacén del lado emisor. Solo permite leer elementos incluidos
// en el manifiesto vigente, mediante identificadores opacos, nunca rutas remotas.
#pragma once

#include "core/manifest.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <string>

namespace o2f {

struct SourceItem {
    Entry meta;
    std::filesystem::path absolute;
};

class OfferStore {
public:
    OfferStore() = default;
    static OfferStore fromPaths(const std::vector<std::string>& rootsUtf8, const Limits& limits);

    const Offer& offer() const { return offer_; }
    const SourceItem* find(const std::string& entryId) const;
    bool has(const std::string& entryId) const { return find(entryId) != nullptr; }

    // Copia hasta len bytes desde offset del elemento ofrecido.
    // Devuelve bytes copiados (0 en fin de archivo) o lanza Error.
    std::size_t read(const std::string& entryId, std::uint64_t offset, void* buffer,
                     std::size_t len) const;

private:
    Offer offer_;
    std::map<std::string, SourceItem> items_;
};

}  // namespace o2f
