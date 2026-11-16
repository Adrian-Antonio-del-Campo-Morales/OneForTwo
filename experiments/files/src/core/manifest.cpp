// Responsibility: tour the routes offered, validate each relative name,
// reject special types and symbolic links, and serialize the manifest.
#include "core/manifest.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace o2f {
namespace {

std::string sanitizeLabel(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        const auto uc = static_cast<unsigned char>(c);
        out.push_back((uc < 0x20 || uc == 0x7f) ? '_' : c);
    }
    if (out.size() > 80) out.resize(80);
    return out;
}

std::string uniqueTopLevelName(const std::string& name, std::set<std::string>& used, bool windows) {
    auto key = [windows](const std::string& s) {
        if (!windows) return s;
        std::string lower = s;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lower;
    };
    if (used.insert(key(name)).second) return name;
    const std::size_t dot = name.rfind('.');
    const std::string stem = dot == std::string::npos ? name : name.substr(0, dot);
    const std::string ext = dot == std::string::npos ? std::string() : name.substr(dot);
    for (int n = 2; n < 10000; ++n) {
        const std::string candidate = stem + " (" + std::to_string(n) + ")" + ext;
        if (used.insert(key(candidate)).second) return candidate;
    }
    throw Error("could not assign a unique name in the offer");
}

class Builder {
public:
    Builder(const Limits& limits, bool windows)
        : limits_(limits), rules_(nativePathRules(limits)) {
        rules_.windows = windows;
    }

    void addRoot(const fs::path& absolute, const std::string& topName) {
        std::error_code ec;
        const fs::file_status st = fs::symlink_status(absolute, ec);
        if (ec) throw Error("could not refer to the item offered");
        if (fs::is_symlink(st)) throw Error("unsupported symbolic links");
        if (fs::is_regular_file(st)) {
            addFile(absolute, topName, 0);
        } else if (fs::is_directory(st)) {
            addDirectory(absolute, topName, 0);
        } else {
            throw Error("unsupported element type (only ordinary files and folders)");
        }
    }

    BuiltOffer take() { return std::move(result_); }

private:
    void push(Entry entry, const fs::path& absolute) {
        if (result_.offer.entries.size() >= limits_.maxEntries) {
            throw Error("the offer exceeds the maximum number of items");
        }
        validateRelativePath(entry.relPath, rules_);
        if (!result_.absoluteByRel.emplace(entry.relPath, absolute.u8string()).second) {
            throw Error("route duplicated in the offer");
        }
        if (!entry.isDir) {
            result_.offer.totalBytes += entry.size;
            if (result_.offer.totalBytes > limits_.maxTotalBytes) {
                throw Error("the offer exceeds the total size allowed");
            }
        }
        result_.offer.entries.push_back(std::move(entry));
    }

    void addFile(const fs::path& absolute, const std::string& rel, std::uint32_t depth) {
        if (depth > limits_.maxDepth) throw Error("Out-of-limit folder depth");
        std::error_code ec;
        const std::uintmax_t size = fs::file_size(absolute, ec);
        if (ec) throw Error("could not read the size of a file offered");
        Entry e;
        e.id = randomId();
        e.relPath = rel;
        e.isDir = false;
        e.size = static_cast<std::uint64_t>(size);
        push(std::move(e), absolute);
    }

    void addDirectory(const fs::path& absolute, const std::string& rel, std::uint32_t depth) {
        if (depth > limits_.maxDepth) throw Error("Out-of-limit folder depth");
        Entry e;
        e.id = randomId();
        e.relPath = rel;
        e.isDir = true;
        push(std::move(e), absolute);

        std::error_code ec;
        std::vector<fs::directory_entry> children;
        for (fs::directory_iterator it(absolute, ec), end; !ec && it != end; it.increment(ec)) {
            children.push_back(*it);
        }
        if (ec) throw Error("could not list offered folder");
        std::sort(children.begin(), children.end(),
                  [](const fs::directory_entry& a, const fs::directory_entry& b) {
                      return a.path().filename().u8string() < b.path().filename().u8string();
                  });

        for (const auto& child : children) {
            const std::string name = child.path().filename().u8string();
            validateRelativePath(name, rules_);
            const std::string childRel = rel + "/" + name;
            const fs::file_status st = child.symlink_status(ec);
            if (ec) throw Error("could not view folder element");
            if (fs::is_symlink(st)) throw Error("unsupported symbolic links");
            if (fs::is_directory(st)) {
                addDirectory(child.path(), childRel, depth + 1);
            } else if (fs::is_regular_file(st)) {
                addFile(child.path(), childRel, depth + 1);
            } else {
                throw Error("unsupported element type within folder");
            }
        }
    }

    Limits limits_;
    PathRules rules_;
    BuiltOffer result_;
};

}  // namespace

BuiltOffer buildOffer(const std::vector<std::string>& rootsUtf8, const Limits& limits) {
    if (rootsUtf8.empty()) throw Error("no route was indicated for the offer");
    if (rootsUtf8.size() > limits.maxLocalPaths) throw Error("too many routes of origin");

    const PathRules probe = nativePathRules(limits);
    Builder builder(limits, probe.windows);
    std::set<std::string> usedNames;

    for (const std::string& root : rootsUtf8) {
        const fs::path absolute = fs::u8path(root);
        std::error_code ec;
        if (!fs::exists(absolute, ec) || ec) throw Error("a route offered does not exist");
        std::string name = absolute.filename().u8string();
        if (name.empty()) throw Error("routeoffered without usable name");
        name = uniqueTopLevelName(sanitizeLabel(name), usedNames, probe.windows);
        builder.addRoot(absolute, name);
    }

    BuiltOffer built = builder.take();
    built.offer.id = randomId();
    built.offer.createdAt = unixNow();
    if (built.offer.entries.size() == 1) {
        built.offer.label = built.offer.entries.front().relPath;
    } else {
        built.offer.label = std::to_string(built.offer.entries.size()) + " elementos";
    }
    return built;
}

std::string serializeManifestText(const Offer& offer) {
    std::ostringstream out;
    out << "O2F1\t" << offer.id << '\t' << offer.createdAt << '\t' << sanitizeLabel(offer.label)
        << '\n';
    for (const Entry& e : offer.entries) {
        out << e.id << '\t' << (e.isDir ? 'D' : 'F') << '\t' << e.size << '\t' << e.relPath << '\n';
    }
    return out.str();
}

Offer parseManifestText(const std::string& text, const Limits& limits) {
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line)) throw Error("empty manifest");
    std::istringstream header(line);
    std::string magic, id, created, label;
    if (!std::getline(header, magic, '\t') || magic != "O2F1") throw Error("manifest not recognised");
    if (!std::getline(header, id, '\t') || !std::getline(header, created, '\t') ||
        !std::getline(header, label)) {
        throw Error("incomplete manifest header");
    }
    Offer result;
    result.id = id;
    result.label = sanitizeLabel(label);
    if (!isValidOpaqueId(result.id, 64)) throw Error("manifest with invalid identifier");
    try {
        result.createdAt = std::stoull(created);
    } catch (...) {
        throw Error("invalid manifest date");
    }

    const PathRules rules = nativePathRules(limits);
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream row(line);
        std::string id2, type, sizeText, rel;
        if (!std::getline(row, id2, '\t') || !std::getline(row, type, '\t') ||
            !std::getline(row, sizeText, '\t') || !std::getline(row, rel)) {
            throw Error("incomplete manifest row");
        }
        if (type != "D" && type != "F") throw Error("type of invalid element in manifest");
        Entry e;
        e.id = id2;
        e.isDir = type == "D";
        try {
            e.size = std::stoull(sizeText);
        } catch (...) {
            throw Error("Invalid size in manifest");
        }
        if (!isValidOpaqueId(e.id, 64)) throw Error("Invalid identifier in manifest");
        e.relPath = validateRelativePath(rel, rules);
        if (e.isDir && e.size != 0) throw Error("folder with size other than zero");
        if (!e.isDir) result.totalBytes += e.size;
        if (result.find(e.id) != nullptr) throw Error("duplicate identifier in manifest");
        if (result.entries.size() >= limits.maxEntries) throw Error("manifest too large");
        for (const Entry& prev : result.entries) {
            if (prev.relPath == e.relPath) throw Error("duplicated manifest path");
        }
        result.entries.push_back(std::move(e));
    }
    if (result.totalBytes > limits.maxTotalBytes) throw Error("manifesto exceeds the permitted size");
    return result;
}

std::string describeOffer(const Offer& offer) {
    std::ostringstream out;
    out << "oferta " << offer.id.substr(0, 8) << "…: " << offer.entries.size() << " elementos ("
        << offer.fileCount() << " archivos), " << offer.totalBytes << " bytes";
    return out.str();
}

}  // namespace o2f
