// Responsibility: common model of file offers, limits, validation of
// relative paths and opaque identifiers shared by the rest of the prototype.
#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace o2f {

// Prototyping Controlled Error: The user is shown without overturning private data.
struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

constexpr std::uint32_t kProtocolVersion = 1;

struct Limits {
    std::size_t maxEntries = 4096;
    std::uint32_t maxDepth = 32;
    std::uint64_t maxTotalBytes = 8ull * 1024 * 1024 * 1024;
    std::size_t maxRelPathBytes = 1024;
    std::size_t maxNameBytes = 255;
    std::uint64_t chunkBytes = 256 * 1024;
    std::size_t maxManifestBytes = 8 * 1024 * 1024;
    std::size_t maxLocalPaths = 4096;
};

struct Entry {
    std::string id;       // opaque identifier; receiver never sends absolute paths
    std::string relPath;  // components separated by '/', validated
    bool isDir = false;
    std::uint64_t size = 0;
};

struct Offer {
    std::string id;
    std::string label;
    std::uint64_t createdAt = 0;
    std::vector<Entry> entries;
    std::uint64_t totalBytes = 0;

    const Entry* find(const std::string& entryId) const {
        for (const auto& e : entries) {
            if (e.id == entryId) return &e;
        }
        return nullptr;
    }
    std::size_t fileCount() const {
        std::size_t n = 0;
        for (const auto& e : entries) {
            if (!e.isDir) ++n;
        }
        return n;
    }
};

struct PathRules {
    bool windows = false;
    std::uint32_t maxDepth = 32;
    std::size_t maxRelPathBytes = 1024;
    std::size_t maxNameBytes = 255;
};

// Launch Error if the relative path is not safe or exceeds the limits indicated.
// Returns the same route when it is acceptable.
std::string validateRelativePath(const std::string& relPath, const PathRules& rules);

// System rules where the receiver runs: in Windows names that are rejected
// the system cannot represent (reserved, prohibited characters, end point).
PathRules nativePathRules(const Limits& limits);

// Safe opaque identifier for naming cache folders: alphanumeric only,
// screenplay and bass script. Prevents a remote pair from introducing route separators.
bool isValidOpaqueId(const std::string& id, std::size_t maxLen);

std::string randomId(std::size_t bytes = 16);
std::string toHex(const std::uint8_t* data, std::size_t len);
bool fixedTimeHexEquals(const std::string& a, const std::string& b);
std::uint64_t unixNow();

}  // namespace o2f
