// Liability: warehouse on the issuing side. Only read elements included
// in the current manifest, using opaque identifiers, never remote routes.
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

    // Copy up to len bytes from offset of the item offered.
    // Returns copied bytes (0 in order of file) or launches Error.
    std::size_t read(const std::string& entryId, std::uint64_t offset, void* buffer,
                     std::size_t len) const;

private:
    Offer offer_;
    std::map<std::string, SourceItem> items_;
};

}  // namespace o2f
