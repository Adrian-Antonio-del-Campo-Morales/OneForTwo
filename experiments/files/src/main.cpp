// Responsabilidad: interfaz de línea de comandos del experimento. Separa los
// modos inocuos (ayuda, self-test, datos sintéticos) de los modos manuales que
// tocan el portapapeles o abren red.
#include "core/cache.hpp"
#include "core/manifest.hpp"
#include "core/net.hpp"
#include "core/remote.hpp"
#include "core/store.hpp"
#include "core/transfer.hpp"
#include "platform/clipboard.hpp"
#include "tests/self_test.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace o2f {
namespace {

const char* kUsage = R"(OneInTwo — experimento de archivos (prototipo)

  oneintwo-files --help
  oneintwo-files --self-test [--verbose]

Preparación sin escritorio:
  --make-fixtures DIR            crea un conjunto sintético desechable
  --inspect RUTA...              muestra la oferta que se construiría
  --fingerprint CERT.pem         muestra la huella SHA-256 de un certificado

Publicación local (portapapeles real, solo ejecución manual):
  --publish-cache RUTA...
  --publish-ondemand --root RUTA
  --publish-ondemand --from HOST[:PUERTO] --pin HUELLA --cert CERT.pem --key CLAVE.pem

Entre dos equipos (TLS mutuo obligatorio):
  --share --root RUTA --cert CERT.pem --key CLAVE.pem --client-pin HUELLA
          [--bind 0.0.0.0] [--port 47654]
  --receive-offer --from HOST[:PUERTO] --pin HUELLA --cache DIR
          --cert CERT.pem --key CLAVE.pem [--to DESTINO] [--publish]

Caché:
  --cache-list --cache DIR
  --cache-gc --cache DIR [--max-age-hours N]

Opciones: --port N, --timeout MS, --verbose. Cada equipo usa su propio par
certificado/clave. --pin fija la huella SHA-256 del servidor y --client-pin la
del cliente; ambas son 64 caracteres hexadecimales.)";

struct Args {
    std::string mode;
    std::vector<std::string> paths;
    std::string root;
    std::string cache;
    std::string from;
    std::string pin;
    std::string clientPin;
    std::string cert;
    std::string key;
    std::string bindAddress = "0.0.0.0";
    std::string to;
    std::uint16_t port = kDefaultPort;
    int timeoutMs = 15000;
    bool publish = false;
    bool verbose = false;
    bool help = false;
    std::uint64_t maxAgeHours = 24;
};

CancelToken* g_cancel = nullptr;

#if defined(_WIN32)
BOOL WINAPI onConsoleCtrl(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) {
        if (g_cancel != nullptr) g_cancel->cancel();
        return TRUE;
    }
    return FALSE;
}
void installCancel(CancelToken& token) {
    g_cancel = &token;
    SetConsoleCtrlHandler(onConsoleCtrl, TRUE);
}
#else
void onSignal(int) {
    if (g_cancel != nullptr) g_cancel->cancel();
}
void installCancel(CancelToken& token) {
    g_cancel = &token;
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
}
#endif

bool allHex(const std::string& text) {
    return std::all_of(text.begin(), text.end(), [](unsigned char c) {
        return std::isxdigit(c) != 0;
    });
}

void parseEndpoint(const std::string& text, std::string& host, std::uint16_t& port) {
    if (text.empty()) throw Error("dirección remota vacía");
    const std::size_t colon = text.rfind(':');
    if (colon == std::string::npos) {
        host = text;
        return;
    }
    host = text.substr(0, colon);
    if (host.empty()) throw Error("dirección remota sin equipo");
    try {
        const int parsed = std::stoi(text.substr(colon + 1));
        if (parsed <= 0 || parsed > 65535) throw Error("puerto remoto fuera de rango");
        port = static_cast<std::uint16_t>(parsed);
    } catch (const Error&) {
        throw;
    } catch (...) {
        throw Error("puerto remoto inválido");
    }
}

Args parseArgs(const std::vector<std::string>& args) {
    Args out;
    auto setMode = [&out](const std::string& mode) {
        if (!out.mode.empty() && out.mode != mode) {
            throw Error("solo se permite un modo por ejecución");
        }
        out.mode = mode;
    };
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        auto value = [&args, &i, &a]() -> std::string {
            if (i + 1 >= args.size()) throw Error("falta el valor de " + a);
            return args[++i];
        };
        if (a == "--help" || a == "-h") {
            out.help = true;
        } else if (a == "--verbose") {
            out.verbose = true;
        } else if (a == "--publish") {
            out.publish = true;
        } else if (a == "--self-test") {
            setMode("self-test");
        } else if (a == "--make-fixtures") {
            setMode("make-fixtures");
            out.paths.push_back(value());
        } else if (a == "--inspect") {
            setMode("inspect");
        } else if (a == "--fingerprint") {
            setMode("fingerprint");
            out.paths.push_back(value());
        } else if (a == "--share") {
            setMode("share");
        } else if (a == "--receive-offer") {
            setMode("receive");
        } else if (a == "--publish-cache") {
            setMode("publish-cache");
        } else if (a == "--publish-ondemand") {
            setMode("publish-ondemand");
        } else if (a == "--cache-list") {
            setMode("cache-list");
        } else if (a == "--cache-gc") {
            setMode("cache-gc");
        } else if (a == "--root") {
            out.root = value();
        } else if (a == "--cache") {
            out.cache = value();
        } else if (a == "--from") {
            out.from = value();
        } else if (a == "--pin") {
            out.pin = value();
        } else if (a == "--client-pin") {
            out.clientPin = value();
        } else if (a == "--cert") {
            out.cert = value();
        } else if (a == "--key") {
            out.key = value();
        } else if (a == "--bind") {
            out.bindAddress = value();
        } else if (a == "--to") {
            out.to = value();
        } else if (a == "--port") {
            try {
                const int parsed = std::stoi(value());
                if (parsed <= 0 || parsed > 65535) throw Error("puerto fuera de rango");
                out.port = static_cast<std::uint16_t>(parsed);
            } catch (const Error&) {
                throw;
            } catch (...) {
                throw Error("puerto inválido");
            }
        } else if (a == "--timeout") {
            try {
                out.timeoutMs = std::stoi(value());
                if (out.timeoutMs < 1000) throw Error("tiempo de espera demasiado corto");
            } catch (const Error&) {
                throw;
            } catch (...) {
                throw Error("tiempo de espera inválido");
            }
        } else if (a == "--max-age-hours") {
            try {
                out.maxAgeHours = std::stoull(value());
            } catch (...) {
                throw Error("antigüedad máxima inválida");
            }
        } else if (!a.empty() && a[0] == '-') {
            throw Error("opción desconocida: " + a);
        } else {
            out.paths.push_back(a);
        }
    }
    return out;
}

void requireValue(const std::string& value, const char* name) {
    if (value.empty()) throw Error(std::string("falta ") + name);
}

void printEntries(const Offer& offer) {
    std::cout << describeOffer(offer) << '\n';
    for (const Entry& entry : offer.entries) {
        std::cout << (entry.isDir ? "D " : "F ") << entry.size << '\t' << entry.relPath << '\n';
    }
}

void writePattern(const fs::path& file, std::uint64_t bytes) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) throw Error("no se pudo crear un archivo sintético");
    std::vector<char> buffer(64 * 1024);
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    std::uint64_t left = bytes;
    while (left > 0) {
        const std::size_t take = static_cast<std::size_t>(std::min<std::uint64_t>(left, buffer.size()));
        for (std::size_t i = 0; i < take; ++i) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            buffer[i] = static_cast<char>(state & 0xff);
        }
        out.write(buffer.data(), static_cast<std::streamsize>(take));
        left -= take;
    }
    if (!out) throw Error("fallo al escribir un archivo sintético");
}

void writeText(const fs::path& file, const std::string& text) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) throw Error("no se pudo crear un archivo sintético");
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out) throw Error("fallo al escribir un archivo sintético");
}

int cmdMakeFixtures(const Args& args) {
    if (args.paths.empty()) throw Error("indique la carpeta de datos: --make-fixtures DIR");
    const fs::path root = fs::u8path(args.paths.front());
    std::error_code ec;
    fs::create_directories(root / "folder" / "nested", ec);
    fs::create_directories(root / "folder" / "empty-subdir", ec);
    if (!fs::is_directory(root)) throw Error("no se pudo crear la carpeta de datos");

    writeText(root / "file-small.txt", std::string(1023, 'A') + "\n");
    writeText(root / "empty.dat", "");
    writeText(root / fs::u8path("ñandú-日本語.txt"),
              "OneInTwo: ñ á é í ó ú ü 日本語 🐧\n");
    writePattern(root / "folder" / "nested" / "deep.bin", 300u * 1024u);
    writePattern(root / "large-5mb.bin", 5u * 1024u * 1024u);

    std::cout << "datos sintéticos creados; incluye archivo vacío, carpeta anidada, "
                 "carpeta vacía, nombre Unicode y un archivo de 5 MiB\n";
    return 0;
}

int cmdInspect(const Args& args) {
    if (args.paths.empty()) throw Error("indique al menos una ruta: --inspect RUTA...");
    const BuiltOffer built = buildOffer(args.paths, Limits{});
    printEntries(built.offer);
    return 0;
}

int cmdFingerprint(const Args& args) {
    if (args.paths.empty()) throw Error("indique el certificado: --fingerprint CERT.pem");
    if (!tlsAvailable()) throw Error("TLS no disponible: compile con OpenSSL");
    const std::string pin = certificateFingerprint(args.paths.front());
    std::cout << "huella SHA-256: " << pin << '\n';
    return 0;
}

class LocalSource : public ContentSource {
public:
    explicit LocalSource(std::shared_ptr<OfferStore> store) : store_(std::move(store)) {}
    bool read(const Entry& entry, std::uint64_t offset, void* buffer, std::size_t len,
              std::size_t& got, std::string& err) override {
        try {
            got = store_->read(entry.id, offset, buffer, len);
            return true;
        } catch (const std::exception& e) {
            err = e.what();
            return false;
        }
    }

private:
    std::shared_ptr<OfferStore> store_;
};

class RemoteSource : public ContentSource {
public:
    explicit RemoteSource(RemoteLink* link) : link_(link) {}
    bool read(const Entry& entry, std::uint64_t offset, void* buffer, std::size_t len,
              std::size_t& got, std::string& err) override {
        try {
            // El explorador puede pedir bloques mayores que la trama del protocolo:
            // se subdividen aquí, manteniendo acotada cada petición remota.
            std::size_t total = 0;
            auto* out = static_cast<std::uint8_t*>(buffer);
            while (total < len) {
                const std::size_t want = std::min<std::size_t>(len - total, 256u * 1024u);
                const std::size_t part =
                    link_->readAt(entry, offset + total, out + total, want);
                if (part == 0) break;
                total += part;
            }
            got = total;
            return true;
        } catch (const std::exception& e) {
            err = e.what();
            return false;
        }
    }

private:
    RemoteLink* link_;
};

int cmdShare(const Args& args) {
    requireValue(args.root, "--root");
    requireValue(args.cert, "--cert");
    requireValue(args.key, "--key");
    requireValue(args.clientPin, "--client-pin");
    if (args.clientPin.size() != 64 || !allHex(args.clientPin)) {
        throw Error("la huella del cliente debe ser SHA-256 en hexadecimal (64 caracteres)");
    }
    if (!tlsAvailable()) throw Error("TLS no disponible: compile con OpenSSL");
    std::error_code ec;
    if (!fs::is_regular_file(fs::u8path(args.cert), ec)) throw Error("el certificado indicado no existe");
    if (!fs::is_regular_file(fs::u8path(args.key), ec)) throw Error("la clave indicada no existe");

    OfferStore store = OfferStore::fromPaths({args.root}, Limits{});
    std::cout << describeOffer(store.offer()) << '\n';
    std::cout << "huella del certificado (SHA-256): "
              << certificateFingerprint(args.cert) << '\n';

    ServerOptions options;
    options.bindAddress = args.bindAddress;
    options.port = args.port;
    options.certFile = args.cert;
    options.keyFile = args.key;
    options.clientPin = args.clientPin;
    options.readTimeoutMs = args.timeoutMs;

    CancelToken cancel;
    installCancel(cancel);
    std::cout << "sirviendo la oferta; Ctrl+C para detener\n";
    serveOffer(store, options, cancel, [](const std::string& note) {
        std::cout << "servidor: " << note << '\n';
    });
    std::cout << "servidor detenido\n";
    return 0;
}

int cmdReceive(const Args& args) {
    requireValue(args.from, "--from");
    requireValue(args.pin, "--pin");
    requireValue(args.cache, "--cache");
    requireValue(args.cert, "--cert");
    requireValue(args.key, "--key");
    if (args.pin.size() != 64 || !allHex(args.pin)) {
        throw Error("la huella debe ser SHA-256 en hexadecimal (64 caracteres)");
    }
    std::string host;
    std::uint16_t port = args.port;
    parseEndpoint(args.from, host, port);

    const Limits limits;
    CancelToken cancel;
    installCancel(cancel);
    auto link = RemoteLink::connect(host, port, args.pin, args.cert, args.key, "fetch",
                                    args.timeoutMs, limits);
    link->requestManifest();
    std::cout << describeOffer(link->offer()) << '\n';

    CacheStore cache(args.cache);
    const fs::path incoming = cache.newIncomingDir(link->offer().id);
    try {
        std::uint64_t fileIndex = 0;
        const std::uint64_t fileTotal = link->offer().fileCount();
        for (const Entry& entry : link->offer().entries) {
            cancel.throwIfCancelled();
            if (entry.isDir) {
                std::error_code ec;
                fs::create_directories(incoming / fs::u8path(entry.relPath), ec);
                if (ec) throw Error("no se pudo crear una carpeta en la recepción");
                continue;
            }
            StagedFile stage(incoming, entry.relPath, entry.size);
            link->fetchFile(entry, stage, cancel, nullptr);
            ++fileIndex;
            std::cout << "archivo " << fileIndex << '/' << fileTotal
                      << " verificado (" << entry.size << " bytes)\n";
            if (args.verbose) std::cout << "  " << entry.relPath << '\n';
        }
        const fs::path published = cache.commitIncoming(incoming, link->offer());
        std::cout << "oferta verificada y publicada en la caché\n";

        if (!args.to.empty()) {
            CopyOutStats stats = copyOutOffer(link->offer(), published, fs::u8path(args.to), cancel);
            std::cout << "copiados " << stats.files << " archivos (" << stats.bytes
                      << " bytes) al destino\n";
        }
        if (args.publish) {
            ClipboardRequest request;
            // Solo los elementos de primer nivel: publicar cada archivo suelto
            // perdería la estructura de carpetas al pegar.
            for (const Entry& entry : link->offer().entries) {
                if (entry.relPath.find('/') != std::string::npos) continue;
                request.localPaths.push_back((published / fs::u8path(entry.relPath)).u8string());
            }
            std::string err;
            std::cout << "publicando en el portapapeles; Ctrl+C para retirar la oferta\n";
            const int rc = runClipboardPublisher(request, true, err);
            if (rc != 0) throw Error(err);
            std::cout << "publicación finalizada (la caché conserva el contenido verificado)\n";
        }
    } catch (...) {
        cache.abandonIncoming(incoming);
        throw;
    }
    return 0;
}

int cmdPublishCache(const Args& args) {
    if (args.paths.empty()) throw Error("indique rutas: --publish-cache RUTA...");
    ClipboardRequest request;
    for (const std::string& path : args.paths) {
        std::error_code ec;
        fs::path absolute = fs::absolute(fs::u8path(path), ec).lexically_normal();
        if (ec || !fs::exists(absolute)) throw Error("una ruta publicada no existe");
        request.localPaths.push_back(absolute.u8string());
    }
    std::string why;
    if (!clipboardAvailable(why)) throw Error(why);
    std::string err;
    std::cout << "publicando " << request.localPaths.size()
              << " elementos locales; Ctrl+C para terminar\n";
    const int rc = runClipboardPublisher(request, true, err);
    if (rc != 0) throw Error(err);
    std::cout << "publicación finalizada (los archivos locales no se modifican)\n";
    return 0;
}

int cmdPublishOnDemand(const Args& args) {
    std::string why;
    if (!onDemandClipboardAvailable(why)) throw Error(why);
    ClipboardRequest request;
    std::unique_ptr<RemoteLink> link;
    std::shared_ptr<ContentSource> source;
    if (!args.from.empty()) {
        requireValue(args.pin, "--pin");
        requireValue(args.cert, "--cert");
        requireValue(args.key, "--key");
        if (args.pin.size() != 64 || !allHex(args.pin)) {
            throw Error("la huella debe ser SHA-256 en hexadecimal (64 caracteres)");
        }
        std::string host;
        std::uint16_t port = args.port;
        parseEndpoint(args.from, host, port);
        link = RemoteLink::connect(host, port, args.pin, args.cert, args.key, "ondemand",
                                   args.timeoutMs, Limits{});
        link->requestManifest();
        request.offer = link->offer();
        source = std::make_shared<RemoteSource>(link.get());
    } else if (!args.root.empty()) {
        auto store = std::make_shared<OfferStore>(OfferStore::fromPaths({args.root}, Limits{}));
        request.offer = store->offer();
        source = std::make_shared<LocalSource>(store);
    } else {
        throw Error("indique --root RUTA o --from HOST[:PUERTO]");
    }
    request.source = source;
    std::cout << describeOffer(request.offer) << '\n';
    std::cout << "publicando entrega bajo demanda; pegue en el explorador y use Ctrl+C para retirar\n";
    std::string err;
    const int rc = runClipboardPublisher(request, false, err);
    if (rc != 0) throw Error(err);
    return 0;
}

int cmdCacheList(const Args& args) {
    requireValue(args.cache, "--cache");
    CacheStore cache(args.cache);
    const std::vector<CachedOffer> offers = cache.list();
    if (offers.empty()) {
        std::cout << "la caché no contiene ofertas\n";
        return 0;
    }
    for (const CachedOffer& cached : offers) {
        std::cout << cached.offer.id.substr(0, 12) << "  " << cached.offer.entries.size()
                  << " elementos  " << cached.offer.totalBytes << " bytes  creada "
                  << cached.offer.createdAt;
        if (args.verbose) std::cout << "  " << cached.dir.u8string();
        std::cout << '\n';
    }
    return 0;
}

int cmdCacheGc(const Args& args) {
    requireValue(args.cache, "--cache");
    CacheStore cache(args.cache);
    const std::uint64_t removed = cache.sweep(args.maxAgeHours * 3600, unixNow());
    std::cout << "eliminadas " << removed << " ofertas o recepciones abandonadas\n";
    return 0;
}

int runMain(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cout << kUsage << '\n';
        return 0;
    }
    Args parsed;
    try {
        parsed = parseArgs(args);
    } catch (const Error& e) {
        std::cerr << "error: " << e.what() << "\n\n" << kUsage << '\n';
        return 2;
    }
    if (parsed.help || parsed.mode.empty()) {
        std::cout << kUsage << '\n';
        return 0;
    }
    try {
        if (parsed.mode == "self-test") return runSelfTests(parsed.verbose);
        if (parsed.mode == "make-fixtures") return cmdMakeFixtures(parsed);
        if (parsed.mode == "inspect") return cmdInspect(parsed);
        if (parsed.mode == "fingerprint") return cmdFingerprint(parsed);
        if (parsed.mode == "share") return cmdShare(parsed);
        if (parsed.mode == "receive") return cmdReceive(parsed);
        if (parsed.mode == "publish-cache") return cmdPublishCache(parsed);
        if (parsed.mode == "publish-ondemand") return cmdPublishOnDemand(parsed);
        if (parsed.mode == "cache-list") return cmdCacheList(parsed);
        if (parsed.mode == "cache-gc") return cmdCacheGc(parsed);
        throw Error("modo no reconocido: " + parsed.mode);
    } catch (const Error& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "error inesperado: " << e.what() << '\n';
        return 1;
    }
}

}  // namespace
}  // namespace o2f

#if defined(_WIN32)
namespace {
std::string wideToUtf8(const wchar_t* text) {
    if (text == nullptr) return std::string();
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) return std::string();
    std::string out(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), needed, nullptr, nullptr);
    out.pop_back();
    return out;
}
}  // namespace

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.push_back(wideToUtf8(argv[i]));
    return o2f::runMain(args);
}
#else
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return o2f::runMain(args);
}
#endif
