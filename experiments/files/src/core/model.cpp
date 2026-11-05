// Responsabilidad: validación de rutas relativas, generación de identificadores
// impredecibles y utilidades comunes del modelo.
#include "core/model.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#else
#include <sys/random.h>
#include <unistd.h>
#endif

namespace o2f {
namespace {

bool isWindowsReservedName(const std::string& component) {
    std::string base = component;
    const std::size_t dot = base.find('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    std::transform(base.begin(), base.end(), base.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    while (!base.empty() && base.back() == ' ') base.pop_back();
    static const char* kReserved[] = {"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4",
                                      "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
                                      "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
    for (const char* r : kReserved) {
        if (base == r) return true;
    }
    return false;
}

std::vector<std::string> splitComponents(const std::string& relPath) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : relPath) {
        if (c == '/') {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    parts.push_back(current);
    return parts;
}

}  // namespace

std::string validateRelativePath(const std::string& relPath, const PathRules& rules) {
    if (relPath.empty()) throw Error("ruta relativa vacía");
    if (relPath.size() > rules.maxRelPathBytes) throw Error("ruta relativa demasiado larga");
    if (relPath.front() == '/') throw Error("ruta absoluta rechazada");
    if (relPath.size() >= 2 && std::isalpha(static_cast<unsigned char>(relPath[0])) &&
        relPath[1] == ':') {
        throw Error("ruta con unidad absoluta rechazada");
    }

    const std::vector<std::string> parts = splitComponents(relPath);
    if (parts.size() > static_cast<std::size_t>(rules.maxDepth) + 1) {
        throw Error("profundidad de ruta fuera de límite");
    }
    for (const std::string& part : parts) {
        if (part.empty()) throw Error("componente de ruta vacío");
        if (part == "." || part == "..") throw Error("componente de ruta no permitido");
        if (part.size() > rules.maxNameBytes) throw Error("nombre de componente demasiado largo");
        for (char c : part) {
            const auto uc = static_cast<unsigned char>(c);
            if (uc < 0x20 || uc == 0x7f) throw Error("carácter de control en el nombre");
        }
        if (rules.windows) {
            for (char c : part) {
                if (c == '\\' || c == '<' || c == '>' || c == ':' || c == '"' || c == '|' || c == '?' ||
                    c == '*') {
                    throw Error("carácter no admitido por el sistema de archivos de destino");
                }
            }
            if (part.back() == '.' || part.back() == ' ') {
                throw Error("nombre con punto o espacio final no admitido en Windows");
            }
            if (isWindowsReservedName(part)) throw Error("nombre reservado del sistema");
        }
    }
    return relPath;
}

PathRules nativePathRules(const Limits& limits) {
    PathRules rules;
#if defined(_WIN32)
    rules.windows = true;
    rules.maxRelPathBytes = 240;  // margen bajo MAX_PATH para el descriptor de Windows
#else
    rules.windows = false;
    rules.maxRelPathBytes = std::min<std::size_t>(limits.maxRelPathBytes, 4000);
#endif
    rules.maxDepth = limits.maxDepth;
    rules.maxNameBytes = limits.maxNameBytes;
    return rules;
}

bool isValidOpaqueId(const std::string& id, std::size_t maxLen) {
    if (id.empty() || id.size() > maxLen) return false;
    for (char c : id) {
        const auto uc = static_cast<unsigned char>(c);
        if (!(std::isalnum(uc) != 0 || c == '-' || c == '_')) return false;
    }
    return true;
}

std::string toHex(const std::uint8_t* data, std::size_t len) {
    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (std::size_t i = 0; i < len; ++i) {
        out.push_back(kHex[data[i] >> 4]);
        out.push_back(kHex[data[i] & 0x0f]);
    }
    return out;
}

bool fixedTimeHexEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff = static_cast<unsigned char>(diff | (static_cast<unsigned char>(std::tolower(
                                                       static_cast<unsigned char>(a[i]))) ^
                                                   static_cast<unsigned char>(std::tolower(
                                                       static_cast<unsigned char>(b[i])))));
    }
    return diff == 0;
}

std::string randomId(std::size_t bytes) {
    std::vector<std::uint8_t> buf(bytes);
    bool ok = false;
#if defined(_WIN32)
    ok = BCryptGenRandom(nullptr, buf.data(), static_cast<ULONG>(buf.size()),
                         BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#else
    std::size_t got = 0;
    while (got < buf.size()) {
        const ssize_t n = ::getrandom(buf.data() + got, buf.size() - got, 0);
        if (n <= 0) break;
        got += static_cast<std::size_t>(n);
    }
    ok = got == buf.size();
#endif
    if (!ok) throw Error("no se pudo obtener aleatoriedad del sistema");
    return toHex(buf.data(), buf.size());
}

std::uint64_t unixNow() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

}  // namespace o2f
