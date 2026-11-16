// Responsibility: command line interface of the experiment.
// innocuous modes (aid, self-test, synthetic data) of manual modes which
// They touch the clipboard or open the net.
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

const char* kUsage = R"(OneInTwo — file experiment (prototype)

  oneintwo-files --help
  oneintwo-files --self-test [--verbose]

Setup without desktop access:
  --make-fixtures DIR            creates disposable synthetic fixtures
  --inspect PATH...              shows the offer that would be built
  --fingerprint CERT.pem         shows a certificate's SHA-256 fingerprint

Local publishing (uses the real clipboard, run manually only):
  --publish-cache PATH...
  --publish-ondemand --root PATH
  --publish-ondemand --from HOST[:PORT] --pin FINGERPRINT --cert CERT.pem --key KEY.pem

Between two devices (mutual TLS required):
  --share --root PATH --cert CERT.pem --key KEY.pem --client-pin FINGERPRINT
          [--bind 0.0.0.0] [--port 47654]
  --receive-offer --from HOST[:PORT] --pin FINGERPRINT --cache DIR
          --cert CERT.pem --key KEY.pem [--to DESTINATION] [--publish]

Cache:
  --cache-list --cache DIR
  --cache-gc --cache DIR [--max-age-hours N]

Options: --port N, --timeout MS, --verbose. Each device uses its own
certificate and key pair. --pin pins the server's SHA-256 fingerprint, and
--client-pin pins the client's. Both are 64 hexadecimal characters.)";

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
    if (text.empty()) throw Error("empty remote address");
    const std::size_t colon = text.rfind(':');
    if (colon == std::string::npos) {
        host = text;
        return;
    }
    host = text.substr(0, colon);
    if (host.empty()) throw Error("remote address without equipment");
    try {
        const int parsed = std::stoi(text.substr(colon + 1));
        if (parsed <= 0 || parsed > 65535) throw Error("remote port out of range");
        port = static_cast<std::uint16_t>(parsed);
    } catch (const Error&) {
        throw;
    } catch (...) {
        throw Error("invalid remote port");
    }
}

Args parseArgs(const std::vector<std::string>& args) {
    Args out;
    auto setMode = [&out](const std::string& mode) {
        if (!out.mode.empty() && out.mode != mode) {
            throw Error("only one mode per execution is allowed");
        }
        out.mode = mode;
    };
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        auto value = [&args, &i, &a]() -> std::string {
            if (i + 1 >= args.size()) throw Error("the value of " + a);
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
                if (parsed <= 0 || parsed > 65535) throw Error("port out of range");
                out.port = static_cast<std::uint16_t>(parsed);
            } catch (const Error&) {
                throw;
            } catch (...) {
                throw Error("Invalid port");
            }
        } else if (a == "--timeout") {
            try {
                out.timeoutMs = std::stoi(value());
                if (out.timeoutMs < 1000) throw Error("waiting time too short");
            } catch (const Error&) {
                throw;
            } catch (...) {
                throw Error("Invalid waiting time");
            }
        } else if (a == "--max-age-hours") {
            try {
                out.maxAgeHours = std::stoull(value());
            } catch (...) {
                throw Error("Invalid maximum seniority");
            }
        } else if (!a.empty() && a[0] == '-') {
            throw Error("unknown option: " + a);
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
    if (!out) throw Error("could not create synthetic file");
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
    if (!out) throw Error("failure to write a synthetic file");
}

void writeText(const fs::path& file, const std::string& text) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) throw Error("could not create synthetic file");
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out) throw Error("failure to write a synthetic file");
}

int cmdMakeFixtures(const Args& args) {
    if (args.paths.empty()) throw Error("indicate the data folder: --make-fixtures DIR");
    const fs::path root = fs::u8path(args.paths.front());
    std::error_code ec;
    fs::create_directories(root / "folder" / "nested", ec);
    fs::create_directories(root / "folder" / "empty-subdir", ec);
    if (!fs::is_directory(root)) throw Error("could not create data folder");

    writeText(root / "file-small.txt", std::string(1023, 'A') + "\n");
    writeText(root / "empty.dat", "");
    writeText(root / fs::u8path("resume-日本語-🚀.txt"),
              "OneInTwo: café 日本語 😀\n");
    writePattern(root / "folder" / "nested" / "deep.bin", 300u * 1024u);
    writePattern(root / "large-5mb.bin", 5u * 1024u * 1024u);

    std::cout << "synthetic data created; includes empty file, nested folder, "
                 "empty folder, Unicode name and 5 MiB\n file";
    return 0;
}

int cmdInspect(const Args& args) {
    if (args.paths.empty()) throw Error("indicate at least one route: --inspect RUTA...");
    const BuiltOffer built = buildOffer(args.paths, Limits{});
    printEntries(built.offer);
    return 0;
}

int cmdFingerprint(const Args& args) {
    if (args.paths.empty()) throw Error("indique el certificado: --fingerprint CERT.pem");
    if (!tlsAvailable()) throw Error("TLS not available: compile with OpenSSL");
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
            // The browser can order larger blocks than the layout of the protocol:
            // They subdivide here, keeping every remote request limited.
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
        throw Error("the customer's footprint should be SHA-256 in hexadecimal (64 characters)");
    }
    if (!tlsAvailable()) throw Error("TLS not available: compile with OpenSSL");
    std::error_code ec;
    if (!fs::is_regular_file(fs::u8path(args.cert), ec)) throw Error("the certificate indicated does not exist");
    if (!fs::is_regular_file(fs::u8path(args.key), ec)) throw Error("the indicated key does not exist");

    OfferStore store = OfferStore::fromPaths({args.root}, Limits{});
    std::cout << describeOffer(store.offer()) << '\n';
    std::cout << "certificate footprint (SHA-256): "
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
    std::cout << "serving the offer; Ctrl+C to stop\n";
    serveOffer(store, options, cancel, [](const std::string& note) {
        std::cout << "servidor: " << note << '\n';
    });
    std::cout << "server stopped\n";
    return 0;
}

int cmdReceive(const Args& args) {
    requireValue(args.from, "--from");
    requireValue(args.pin, "--pin");
    requireValue(args.cache, "--cache");
    requireValue(args.cert, "--cert");
    requireValue(args.key, "--key");
    if (args.pin.size() != 64 || !allHex(args.pin)) {
        throw Error("the print must be SHA-256 in hexadecimal (64 characters)");
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
                if (ec) throw Error("could not create folder at reception");
                continue;
            }
            StagedFile stage(incoming, entry.relPath, entry.size);
            link->fetchFile(entry, stage, cancel, nullptr);
            // The content is already verified; keep your final name
            // inside incoming before publishing the entire offer.
            std::error_code renameError;
            fs::rename(stage.partPath(), incoming / fs::u8path(entry.relPath), renameError);
            if (renameError) throw Error("could not finish the verified file at the reception");
            ++fileIndex;
            std::cout << "file " << fileIndex << '/' << fileTotal
                      << " verificado (" << entry.size << " bytes)\n";
            if (args.verbose) std::cout << "  " << entry.relPath << '\n';
        }
        const fs::path published = cache.commitIncoming(incoming, link->offer());
        std::cout << "verified offer and published in cache\n";

        if (!args.to.empty()) {
            CopyOutStats stats = copyOutOffer(link->offer(), published, fs::u8path(args.to), cancel);
            std::cout << "copiados " << stats.files << " archivos (" << stats.bytes
                      << " bytes) to destination\n";
        }
        if (args.publish) {
            ClipboardRequest request;
            // Only the first-level items: publish each single file
            // I'd lose the folder structure when I paste.
            for (const Entry& entry : link->offer().entries) {
                if (entry.relPath.find('/') != std::string::npos) continue;
                request.localPaths.push_back((published / fs::u8path(entry.relPath)).u8string());
            }
            std::string err;
            std::cout << "posting on clipboard; Ctrl+C to withdraw the offer\n";
            const int rc = runClipboardPublisher(request, true, err);
            if (rc != 0) throw Error(err);
            std::cout << "publication completed (the cache retains verified content)\n";
        }
    } catch (...) {
        cache.abandonIncoming(incoming);
        throw;
    }
    return 0;
}

int cmdPublishCache(const Args& args) {
    if (args.paths.empty()) throw Error("specify paths: --publish-cache PATH...");
    ClipboardRequest request;
    for (const std::string& path : args.paths) {
        std::error_code ec;
        fs::path absolute = fs::absolute(fs::u8path(path), ec).lexically_normal();
        if (ec || !fs::exists(absolute)) throw Error("a published path does not exist");
        request.localPaths.push_back(absolute.u8string());
    }
    std::string why;
    if (!clipboardAvailable(why)) throw Error(why);
    std::string err;
    std::cout << "publicando " << request.localPaths.size()
              << " local elements; Ctrl+C to finish\n";
    const int rc = runClipboardPublisher(request, true, err);
    if (rc != 0) throw Error(err);
    std::cout << "publication completed (local files unchanged)\n";
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
            throw Error("the print must be SHA-256 in hexadecimal (64 characters)");
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
    std::cout << "posting delivery on demand; paste into the browser and use Ctrl+C to remove\n";
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
        std::cout << "cache does not contain offers\n";
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
    std::cout << "eliminadas " << removed << " abandoned offers or receptions\n";
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
        throw Error("unrecognized mode: " + parsed.mode);
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
