// Responsabilidad: modelo común de ofertas de archivos, límites, validación de
// rutas relativas e identificadores opacos compartidos por el resto del prototipo.
#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace o2f {

// Error controlado del prototipo: se muestra al usuario sin volcar datos privados.
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
    std::string id;       // identificador opaco; el receptor nunca envía rutas absolutas
    std::string relPath;  // componentes separados por '/', validados
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

// Lanza Error si la ruta relativa no es segura o excede los límites indicados.
// Devuelve la misma ruta cuando es aceptable.
std::string validateRelativePath(const std::string& relPath, const PathRules& rules);

// Reglas del sistema donde corre el receptor: en Windows se rechazan nombres que
// el sistema no puede representar (reservados, caracteres prohibidos, punto final).
PathRules nativePathRules(const Limits& limits);

// Identificador opaco seguro para nombrar carpetas de caché: solo alfanuméricos,
// guion y guion bajo. Evita que un par remoto introduzca separadores de ruta.
bool isValidOpaqueId(const std::string& id, std::size_t maxLen);

std::string randomId(std::size_t bytes = 16);
std::string toHex(const std::uint8_t* data, std::size_t len);
bool fixedTimeHexEquals(const std::string& a, const std::string& b);
std::uint64_t unixNow();

}  // namespace o2f
