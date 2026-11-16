// Responsibility: block reception in temporary files, verification
// of size and SHA-256, and publication without silent overwriting.
#pragma once

#include "core/model.hpp"
#include "core/sha256.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

namespace o2f {

class CancelToken {
public:
    void cancel() { flag_.store(true); }
    bool cancelled() const { return flag_.load(); }
    void throwIfCancelled() const {
        if (cancelled()) throw Error("transferencia cancelada");
    }

private:
    std::atomic<bool> flag_{false};
};

struct Progress {
    std::uint64_t bytesDone = 0;
    std::uint64_t bytesTotal = 0;
};
using ProgressFn = std::function<void(const Progress&)>;

// Incomplete file in a receiving directory. It only becomes
// After checking size and digest; when destroyed without finish it is removed.
class StagedFile {
public:
    StagedFile(const std::filesystem::path& stagingDir, const std::string& relPath,
               std::uint64_t expectedSize);
    ~StagedFile();
    StagedFile(const StagedFile&) = delete;
    StagedFile& operator=(const StagedFile&) = delete;

    // Write a block in strict sequential order.
    void writeAt(std::uint64_t offset, const void* data, std::size_t len);
    void finish(const std::string& expectedDigestHex);
    void discard();

    const std::filesystem::path& partPath() const { return partPath_; }
    std::uint64_t written() const { return written_; }

private:
    void closeStream();

    std::filesystem::path partPath_;
    std::ofstream out_;
    Sha256 hash_;
    std::uint64_t expectedSize_;
    std::uint64_t written_ = 0;
    bool finished_ = false;
};

// Free name in destDir for the indicated name; does not create or modify anything.
std::filesystem::path nextFreePath(const std::filesystem::path& destDir,
                                   const std::string& fileName, int& counter);

// Copy source to destDir with a name that does not exist. copy_file with
// copy_options::none fails if fate appears, so it never overwrites it.
std::filesystem::path placeFileNoOverwrite(const std::filesystem::path& source,
                                           const std::filesystem::path& destDir,
                                           const std::string& fileName);

struct CopyOutStats {
    std::uint64_t files = 0;
    std::uint64_t bytes = 0;
};

// Copy an offer published in the cache to a destination folder, creating
// new folders (never merging) and resolving collisions with suffixes.
CopyOutStats copyOutOffer(const Offer& offer, const std::filesystem::path& offerDir,
                          const std::filesystem::path& destDir, CancelToken& cancel,
                          const ProgressFn& progress = {});

// Removes orphaned .o2f-part files from a receiving directory.
std::size_t removeOrphanParts(const std::filesystem::path& dir);

}  // namespace o2f
