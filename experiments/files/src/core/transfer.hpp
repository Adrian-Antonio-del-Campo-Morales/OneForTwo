// Responsabilidad: recepción por bloques en archivos temporales, verificación
// de tamaño y SHA-256, y publicación sin sobrescrituras silenciosas.
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

// Archivo incompleto en un directorio de recepción. Solo se convierte en
// definitivo tras comprobar tamaño y digest; al destruirse sin finish se elimina.
class StagedFile {
public:
    StagedFile(const std::filesystem::path& stagingDir, const std::string& relPath,
               std::uint64_t expectedSize);
    ~StagedFile();
    StagedFile(const StagedFile&) = delete;
    StagedFile& operator=(const StagedFile&) = delete;

    // Escribe un bloque en orden secuencial estricto.
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

// Nombre libre en destDir para el nombre indicado; no crea ni modifica nada.
std::filesystem::path nextFreePath(const std::filesystem::path& destDir,
                                   const std::string& fileName, int& counter);

// Copia source a destDir con un nombre que no exista. copy_file con
// copy_options::none falla si el destino aparece, por lo que nunca lo sobrescribe.
std::filesystem::path placeFileNoOverwrite(const std::filesystem::path& source,
                                           const std::filesystem::path& destDir,
                                           const std::string& fileName);

struct CopyOutStats {
    std::uint64_t files = 0;
    std::uint64_t bytes = 0;
};

// Copia una oferta publicada en la caché a una carpeta de destino, creando
// carpetas nuevas (nunca fusionando) y resolviendo colisiones con sufijos.
CopyOutStats copyOutOffer(const Offer& offer, const std::filesystem::path& offerDir,
                          const std::filesystem::path& destDir, CancelToken& cancel,
                          const ProgressFn& progress = {});

// Elimina archivos .o2f-part huérfanos de un directorio de recepción.
std::size_t removeOrphanParts(const std::filesystem::path& dir);

}  // namespace o2f
