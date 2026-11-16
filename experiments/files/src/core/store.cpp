// Responsibility: reading access limited to the elements of an offer.
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
    if (item == nullptr) throw Error("identifier not included in the current tender");
    if (item->meta.isDir) throw Error("the requested item is a folder");

    std::error_code ec;
    const std::uintmax_t currentSize = fs::file_size(item->absolute, ec);
    if (ec) throw Error("source file is no longer available");
    if (currentSize != item->meta.size) throw Error("the source file changed size");
    if (offset > item->meta.size) throw Error("scrolling out of the offered file");
    if (offset == item->meta.size) return 0;

    const std::size_t want = static_cast<std::size_t>(
        std::min<std::uint64_t>(len, item->meta.size - offset));
    std::ifstream in(item->absolute, std::ios::binary);
    if (!in) throw Error("could not open source file");
    in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!in) throw Error("could not position source file");
    in.read(static_cast<char*>(buffer), static_cast<std::streamsize>(want));
    const std::streamsize got = in.gcount();
    if (got < 0) throw Error("failed reading of source file");
    return static_cast<std::size_t>(got);
}

}  // namespace o2f
