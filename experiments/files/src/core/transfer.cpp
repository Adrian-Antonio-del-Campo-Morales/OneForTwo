// Responsabilidad: escritura por bloques con hash incremental, verificación final
// y colocación segura del resultado en el destino.
#include "core/transfer.hpp"

#include <algorithm>
#include <map>
#include <system_error>

namespace fs = std::filesystem;

namespace o2f {
namespace {

std::string stemOf(const std::string& name) {
    const std::size_t dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0) return name;
    return name.substr(0, dot);
}

std::string extOf(const std::string& name) {
    const std::size_t dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0) return std::string();
    return name.substr(dot);
}

std::string suffixed(const std::string& name, int n) {
    if (n <= 0) return name;
    return stemOf(name) + " (" + std::to_string(n) + ")" + extOf(name);
}

}  // namespace

StagedFile::StagedFile(const fs::path& stagingDir, const std::string& relPath,
                       std::uint64_t expectedSize)
    : expectedSize_(expectedSize) {
    validateRelativePath(relPath, nativePathRules(Limits{}));
    const fs::path relative = fs::u8path(relPath);
    const fs::path parent = stagingDir / relative.parent_path();
    std::error_code ec;
    fs::create_directories(parent, ec);
    if (ec && !fs::is_directory(parent)) throw Error("no se pudo preparar la carpeta de recepción");
    partPath_ = parent / (relative.filename().u8string() + ".o2f-part");
    out_.open(partPath_, std::ios::binary | std::ios::trunc);
    if (!out_) throw Error("no se pudo crear el archivo temporal de recepción");
}

StagedFile::~StagedFile() {
    if (!finished_) discard();
}

void StagedFile::closeStream() {
    if (out_.is_open()) {
        out_.flush();
        out_.close();
    }
}

void StagedFile::writeAt(std::uint64_t offset, const void* data, std::size_t len) {
    if (offset != written_) throw Error("bloque fuera de orden en la recepción");
    if (expectedSize_ - written_ < len) throw Error("se recibieron más bytes que el tamaño declarado");
    out_.write(static_cast<const char*>(data), static_cast<std::streamsize>(len));
    if (!out_) throw Error("fallo al escribir el archivo temporal");
    hash_.update(data, len);
    written_ += len;
}

void StagedFile::finish(const std::string& expectedDigestHex) {
    if (written_ != expectedSize_) throw Error("tamaño recibido distinto del declarado");
    const std::string actual = hash_.hexDigest();
    if (!fixedTimeHexEquals(actual, expectedDigestHex)) throw Error("digest SHA-256 no coincide");
    out_.flush();
    if (!out_) throw Error("fallo al vaciar el archivo temporal");
    closeStream();
    finished_ = true;
}

void StagedFile::discard() {
    closeStream();
    std::error_code ec;
    fs::remove(partPath_, ec);
    finished_ = true;
}

fs::path nextFreePath(const fs::path& destDir, const std::string& fileName, int& counter) {
    for (int n = counter; n < counter + 10000; ++n) {
        const fs::path candidate = destDir / fs::u8path(suffixed(fileName, n));
        std::error_code ec;
        if (!fs::exists(candidate, ec)) {
            counter = n + 1;
            return candidate;
        }
    }
    throw Error("demasiadas colisiones de nombre en el destino");
}

fs::path placeFileNoOverwrite(const fs::path& source, const fs::path& destDir,
                              const std::string& fileName) {
    std::error_code ec;
    if (!fs::is_directory(destDir, ec)) throw Error("la carpeta de destino no existe");
    for (int n = 0; n < 1000; ++n) {
        const fs::path candidate = destDir / fs::u8path(suffixed(fileName, n));
        std::error_code existsEc;
        if (fs::exists(candidate, existsEc)) continue;
        std::error_code copyEc;
        fs::copy_file(source, candidate, fs::copy_options::none, copyEc);
        if (!copyEc) return candidate;
        std::error_code afterEc;
        if (!fs::exists(candidate, afterEc)) {
            throw Error("no se pudo escribir el archivo en el destino (permisos o espacio)");
        }
    }
    throw Error("no se pudo resolver la colisión de nombre en el destino");
}

CopyOutStats copyOutOffer(const Offer& offer, const fs::path& offerDir, const fs::path& destDir,
                          CancelToken& cancel, const ProgressFn& progress) {
    std::error_code ec;
    if (!fs::is_directory(offerDir, ec)) throw Error("la oferta de la caché no existe");
    fs::create_directories(destDir, ec);
    if (!fs::is_directory(destDir)) throw Error("no se pudo crear la carpeta de destino");

    CopyOutStats stats;
    std::map<std::string, fs::path> finalDirs;
    std::uint64_t totalBytes = offer.totalBytes;

    for (const Entry& entry : offer.entries) {
        cancel.throwIfCancelled();
        const std::size_t slash = entry.relPath.rfind('/');
        const std::string parentRel = slash == std::string::npos ? std::string() : entry.relPath.substr(0, slash);
        const std::string name = slash == std::string::npos ? entry.relPath : entry.relPath.substr(slash + 1);
        fs::path parent = destDir;
        if (!parentRel.empty()) {
            const auto it = finalDirs.find(parentRel);
            if (it == finalDirs.end()) throw Error("manifiesto con carpeta padre ausente");
            parent = it->second;
        }

        if (entry.isDir) {
            fs::path created;
            for (int n = 0; n < 10000 && created.empty(); ++n) {
                const fs::path candidate = parent / fs::u8path(suffixed(name, n));
                std::error_code dirEc;
                if (fs::create_directory(candidate, dirEc)) {
                    created = candidate;
                } else if (!fs::exists(candidate, dirEc)) {
                    throw Error("no se pudo crear una carpeta en el destino");
                }
            }
            if (created.empty()) throw Error("demasiadas colisiones de carpeta en el destino");
            finalDirs.emplace(entry.relPath, created);
        } else {
            const fs::path source = offerDir / fs::u8path(entry.relPath);
            std::error_code sizeEc;
            if (!fs::is_regular_file(source, sizeEc)) {
                throw Error("falta un archivo verificado en la oferta de la caché");
            }
            placeFileNoOverwrite(source, parent, name);
            stats.files += 1;
            stats.bytes += entry.size;
            if (progress) {
                Progress p;
                p.bytesDone = stats.bytes;
                p.bytesTotal = totalBytes;
                progress(p);
            }
        }
    }
    return stats;
}

std::size_t removeOrphanParts(const fs::path& dir) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return 0;
    std::size_t removed = 0;
    for (fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == ".o2f-part") {
            std::error_code rmEc;
            if (fs::remove(it->path(), rmEc)) ++removed;
        }
    }
    return removed;
}

}  // namespace o2f
