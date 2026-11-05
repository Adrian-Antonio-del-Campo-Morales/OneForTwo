// Responsabilidad: recorrer las rutas ofrecidas, validar cada nombre relativo,
// rechazar tipos especiales y enlaces simbólicos, y serializar el manifiesto.
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
    throw Error("no se pudo asignar un nombre único en la oferta");
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
        if (ec) throw Error("no se pudo consultar el elemento ofrecido");
        if (fs::is_symlink(st)) throw Error("enlaces simbólicos no soportados");
        if (fs::is_regular_file(st)) {
            addFile(absolute, topName, 0);
        } else if (fs::is_directory(st)) {
            addDirectory(absolute, topName, 0);
        } else {
            throw Error("tipo de elemento no soportado (solo archivos y carpetas ordinarios)");
        }
    }

    BuiltOffer take() { return std::move(result_); }

private:
    void push(Entry entry, const fs::path& absolute) {
        if (result_.offer.entries.size() >= limits_.maxEntries) {
            throw Error("la oferta supera el número máximo de elementos");
        }
        validateRelativePath(entry.relPath, rules_);
        if (!result_.absoluteByRel.emplace(entry.relPath, absolute.u8string()).second) {
            throw Error("ruta duplicada en la oferta");
        }
        if (!entry.isDir) {
            result_.offer.totalBytes += entry.size;
            if (result_.offer.totalBytes > limits_.maxTotalBytes) {
                throw Error("la oferta supera el tamaño total permitido");
            }
        }
        result_.offer.entries.push_back(std::move(entry));
    }

    void addFile(const fs::path& absolute, const std::string& rel, std::uint32_t depth) {
        if (depth > limits_.maxDepth) throw Error("profundidad de carpeta fuera de límite");
        std::error_code ec;
        const std::uintmax_t size = fs::file_size(absolute, ec);
        if (ec) throw Error("no se pudo leer el tamaño de un archivo ofrecido");
        Entry e;
        e.id = randomId();
        e.relPath = rel;
        e.isDir = false;
        e.size = static_cast<std::uint64_t>(size);
        push(std::move(e), absolute);
    }

    void addDirectory(const fs::path& absolute, const std::string& rel, std::uint32_t depth) {
        if (depth > limits_.maxDepth) throw Error("profundidad de carpeta fuera de límite");
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
        if (ec) throw Error("no se pudo enumerar una carpeta ofrecida");
        std::sort(children.begin(), children.end(),
                  [](const fs::directory_entry& a, const fs::directory_entry& b) {
                      return a.path().filename().u8string() < b.path().filename().u8string();
                  });

        for (const auto& child : children) {
            const std::string name = child.path().filename().u8string();
            validateRelativePath(name, rules_);
            const std::string childRel = rel + "/" + name;
            const fs::file_status st = child.symlink_status(ec);
            if (ec) throw Error("no se pudo consultar un elemento de la carpeta");
            if (fs::is_symlink(st)) throw Error("enlaces simbólicos no soportados");
            if (fs::is_directory(st)) {
                addDirectory(child.path(), childRel, depth + 1);
            } else if (fs::is_regular_file(st)) {
                addFile(child.path(), childRel, depth + 1);
            } else {
                throw Error("tipo de elemento no soportado dentro de la carpeta");
            }
        }
    }

    Limits limits_;
    PathRules rules_;
    BuiltOffer result_;
};

}  // namespace

BuiltOffer buildOffer(const std::vector<std::string>& rootsUtf8, const Limits& limits) {
    if (rootsUtf8.empty()) throw Error("no se indicó ninguna ruta para la oferta");
    if (rootsUtf8.size() > limits.maxLocalPaths) throw Error("demasiadas rutas de origen");

    const PathRules probe = nativePathRules(limits);
    Builder builder(limits, probe.windows);
    std::set<std::string> usedNames;

    for (const std::string& root : rootsUtf8) {
        const fs::path absolute = fs::u8path(root);
        std::error_code ec;
        if (!fs::exists(absolute, ec) || ec) throw Error("una ruta ofrecida no existe");
        std::string name = absolute.filename().u8string();
        if (name.empty()) throw Error("rutaofrecida sin nombre utilizable");
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
    if (!std::getline(in, line)) throw Error("manifiesto vacío");
    std::istringstream header(line);
    std::string magic, id, created, label;
    if (!std::getline(header, magic, '\t') || magic != "O2F1") throw Error("manifiesto no reconocido");
    if (!std::getline(header, id, '\t') || !std::getline(header, created, '\t') ||
        !std::getline(header, label)) {
        throw Error("cabecera de manifiesto incompleta");
    }
    Offer result;
    result.id = id;
    result.label = sanitizeLabel(label);
    if (!isValidOpaqueId(result.id, 64)) throw Error("manifiesto con identificador inválido");
    try {
        result.createdAt = std::stoull(created);
    } catch (...) {
        throw Error("fecha de manifiesto inválida");
    }

    const PathRules rules = nativePathRules(limits);
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream row(line);
        std::string id2, type, sizeText, rel;
        if (!std::getline(row, id2, '\t') || !std::getline(row, type, '\t') ||
            !std::getline(row, sizeText, '\t') || !std::getline(row, rel)) {
            throw Error("fila de manifiesto incompleta");
        }
        if (type != "D" && type != "F") throw Error("tipo de elemento inválido en manifiesto");
        Entry e;
        e.id = id2;
        e.isDir = type == "D";
        try {
            e.size = std::stoull(sizeText);
        } catch (...) {
            throw Error("tamaño inválido en manifiesto");
        }
        if (!isValidOpaqueId(e.id, 64)) throw Error("identificador inválido en manifiesto");
        e.relPath = validateRelativePath(rel, rules);
        if (e.isDir && e.size != 0) throw Error("carpeta con tamaño distinto de cero");
        if (!e.isDir) result.totalBytes += e.size;
        if (result.find(e.id) != nullptr) throw Error("identificador duplicado en manifiesto");
        if (result.entries.size() >= limits.maxEntries) throw Error("manifiesto demasiado grande");
        for (const Entry& prev : result.entries) {
            if (prev.relPath == e.relPath) throw Error("ruta duplicada en manifiesto");
        }
        result.entries.push_back(std::move(e));
    }
    if (result.totalBytes > limits.maxTotalBytes) throw Error("manifiesto supera el tamaño permitido");
    return result;
}

std::string describeOffer(const Offer& offer) {
    std::ostringstream out;
    out << "oferta " << offer.id.substr(0, 8) << "…: " << offer.entries.size() << " elementos ("
        << offer.fileCount() << " archivos), " << offer.totalBytes << " bytes";
    return out.str();
}

}  // namespace o2f
