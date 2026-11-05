// Responsabilidad: comprobaciones aisladas del núcleo. Usan datos sintéticos en
// un temporal bajo el directorio actual y no inicializan escritorio, portapapeles
// ni red.
#include "tests/self_test.hpp"

#include "core/cache.hpp"
#include "core/manifest.hpp"
#include "core/model.hpp"
#include "core/net.hpp"
#include "core/remote.hpp"
#include "core/sha256.hpp"
#include "core/store.hpp"
#include "core/stream.hpp"
#include "core/transfer.hpp"
#include "core/wire.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace o2f {
namespace {

int g_failures = 0;
int g_checks = 0;
bool g_verbose = false;
std::string g_current;
fs::path g_tmp;

void checkImpl(bool ok, const char* expression, int line) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::cerr << "FALLO [" << g_current << "] " << expression << " (línea " << line << ")\n";
    }
}

#define CHECK(expr) checkImpl((expr), #expr, __LINE__)

#define CHECK_THROWS(expr)                                                   \
    do {                                                                     \
        bool threw = false;                                                  \
        try {                                                                \
            (void)(expr);                                                    \
        } catch (const std::exception&) {                                    \
            threw = true;                                                    \
        }                                                                    \
        checkImpl(threw, "lanza error: " #expr, __LINE__);                   \
    } while (0)

void begin(const std::string& name) {
    g_current = name;
    if (g_verbose) std::cout << "  " << name << '\n';
}

void writeFile(const fs::path& file, const std::string& data) {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) throw Error("no se pudo escribir un dato sintético");
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!out) throw Error("fallo al escribir un dato sintético");
}

std::string readAll(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::string pattern(std::size_t size) {
    std::string data(size, '\0');
    for (std::size_t i = 0; i < size; ++i) {
        data[i] = static_cast<char>((i * 31u + (i >> 8)) % 251u);
    }
    return data;
}

fs::path makeTree() {
    const fs::path root = g_tmp / "tree";
    writeFile(root / "a.txt", "hello");
    writeFile(root / "empty.bin", "");
    writeFile(root / fs::u8path("ñandú-日本語.txt"), "internacional");
    writeFile(root / "sub" / "b.bin", pattern(300000));
    fs::create_directories(root / "sub" / "empty");
    return root;
}

void testSha256() {
    begin("sha256");
    CHECK(sha256Hex("", 0) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha256Hex("abc", 3) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const char* longText = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK(sha256Hex(longText, std::strlen(longText)) ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    Sha256 split;
    split.update("abcdbcdecdefdefgefghfghighij", 28);
    split.update("hijkijkljklmklmnlmnomnopnopq", 28);
    CHECK(split.hexDigest() == sha256Hex(longText, std::strlen(longText)));
}

void testPathValidation() {
    begin("validación de rutas");
    PathRules posix;
    CHECK(validateRelativePath("ñandú-日本語.txt", posix) == "ñandú-日本語.txt");
    CHECK(validateRelativePath("sub/file.txt", posix) == "sub/file.txt");
    CHECK_THROWS(validateRelativePath("", posix));
    CHECK_THROWS(validateRelativePath("../escape", posix));
    CHECK_THROWS(validateRelativePath("a/../../b", posix));
    CHECK_THROWS(validateRelativePath("/etc/passwd", posix));
    CHECK_THROWS(validateRelativePath("C:/Windows", posix));
    CHECK_THROWS(validateRelativePath("a//b", posix));
    CHECK_THROWS(validateRelativePath("a/./b", posix));
    CHECK_THROWS(validateRelativePath(std::string("a/\x01" "b"), posix));
    CHECK_THROWS(validateRelativePath(std::string(300, 'x'), posix));

    PathRules windows;
    windows.windows = true;
    CHECK_THROWS(validateRelativePath("CON", windows));
    CHECK_THROWS(validateRelativePath("aux.txt", windows));
    CHECK_THROWS(validateRelativePath("name.", windows));
    CHECK_THROWS(validateRelativePath("name ", windows));
    CHECK_THROWS(validateRelativePath("a:b.txt", windows));
    CHECK_THROWS(validateRelativePath("a\\b.txt", windows));
    CHECK_THROWS(validateRelativePath("a?b", windows));
    CHECK(validateRelativePath("carpeta/archivo.txt", windows) == "carpeta/archivo.txt");

    PathRules shallow;
    shallow.maxDepth = 1;
    CHECK_THROWS(validateRelativePath("a/b/c.txt", shallow));
    CHECK(validateRelativePath("a/b.txt", shallow) == "a/b.txt");
}

void testManifest() {
    begin("manifiesto");
    const fs::path root = makeTree();
    BuiltOffer built = buildOffer({root.u8string()}, Limits{});
    CHECK(built.offer.entries.size() == 7);
    CHECK(built.offer.fileCount() == 4);
    CHECK(built.offer.totalBytes == 300018);
    CHECK(!built.offer.id.empty());
    CHECK(built.offer.label == "7 elementos");

    std::set<std::string> ids;
    std::size_t dirs = 0;
    for (const Entry& entry : built.offer.entries) {
        ids.insert(entry.id);
        CHECK(entry.id.size() == 32);
        if (entry.isDir) ++dirs;
        CHECK(built.absoluteByRel.count(entry.relPath) == 1);
    }
    CHECK(ids.size() == built.offer.entries.size());
    CHECK(dirs == 3);

    Limits small = Limits{};
    small.maxEntries = 3;
    CHECK_THROWS(buildOffer({root.u8string()}, small));

    Limits shallow = Limits{};
    shallow.maxDepth = 1;
    CHECK_THROWS(buildOffer({root.u8string()}, shallow));

    BuiltOffer twice = buildOffer({root.u8string(), root.u8string()}, Limits{});
    CHECK(twice.offer.entries.size() == 14);
    CHECK(twice.offer.entries[0].relPath == "tree");
    CHECK(twice.offer.entries[7].relPath == "tree (2)");

    BuiltOffer single = buildOffer({(root / "a.txt").u8string()}, Limits{});
    CHECK(single.offer.entries.size() == 1);
    CHECK(single.offer.label == "a.txt");
}

void testManifestText() {
    begin("manifiesto serializado");
    const fs::path root = makeTree();
    const Offer offer = buildOffer({root.u8string()}, Limits{}).offer;
    const std::string text = serializeManifestText(offer);
    const Offer parsed = parseManifestText(text, Limits{});
    CHECK(parsed.id == offer.id);
    CHECK(parsed.createdAt == offer.createdAt);
    CHECK(parsed.entries.size() == offer.entries.size());
    CHECK(parsed.totalBytes == offer.totalBytes);
    for (std::size_t i = 0; i < offer.entries.size(); ++i) {
        CHECK(parsed.entries[i].id == offer.entries[i].id);
        CHECK(parsed.entries[i].relPath == offer.entries[i].relPath);
        CHECK(parsed.entries[i].size == offer.entries[i].size);
    }

    std::string escaped = text;
    const std::size_t position = escaped.find("tree/a.txt");
    CHECK(position != std::string::npos);
    escaped.replace(position, 10, "tree/../evil");
    CHECK_THROWS(parseManifestText(escaped, Limits{}));

    ByteWriter w;
    w.str("offer1");
    w.str("etiqueta");
    w.u64(1);
    w.u32(1);
    w.str("entry1");
    w.str("../escape");
    w.u8(0);
    w.u64(4);
    CHECK_THROWS(decodeOffer(w.data().data(), w.data().size(), Limits{}));

    ByteWriter huge;
    huge.str("offer1");
    huge.str("etiqueta");
    huge.u64(1);
    huge.u32(1000000);
    CHECK_THROWS(decodeOffer(huge.data().data(), huge.data().size(), Limits{}));

    ByteWriter badOfferId;
    badOfferId.str("../escape");
    badOfferId.str("etiqueta");
    badOfferId.u64(1);
    badOfferId.u32(0);
    CHECK_THROWS(decodeOffer(badOfferId.data().data(), badOfferId.data().size(), Limits{}));
    CHECK_THROWS(parseManifestText("O2F1\t../../x\t1\tetiqueta\n", Limits{}));
}

void testStore() {
    begin("autorización de la oferta");
    const fs::path root = makeTree();
    const OfferStore store = OfferStore::fromPaths({root.u8string()}, Limits{});
    const Entry* file = nullptr;
    for (const Entry& entry : store.offer().entries) {
        if (!entry.isDir && entry.relPath == "tree/a.txt") file = &entry;
    }
    CHECK(file != nullptr);
    if (file == nullptr) return;
    std::string buffer(5, '\0');
    CHECK(store.read(file->id, 0, buffer.data(), buffer.size()) == 5);
    CHECK(buffer == "hello");
    CHECK_THROWS(store.read("identificador-inexistente", 0, buffer.data(), buffer.size()));
    CHECK_THROWS(store.read(file->id, 100, buffer.data(), buffer.size()));
    CHECK(store.read(file->id, file->size, buffer.data(), 1) == 0);

    std::string dirId;
    for (const Entry& entry : store.offer().entries) {
        if (entry.isDir) dirId = entry.id;
    }
    CHECK(!dirId.empty());
    CHECK_THROWS(store.read(dirId, 0, buffer.data(), 4));

    const OfferStore other = OfferStore::fromPaths({(root / "sub").u8string()}, Limits{});
    CHECK_THROWS(other.read(file->id, 0, buffer.data(), 4));
}

void testStagedFiles() {
    begin("recepción por bloques");
    const std::string content = pattern(300000);
    const fs::path staging = g_tmp / "staging";
    fs::create_directories(staging);

    fs::path partPath;
    {
        StagedFile stage(staging, "sub/file.bin", content.size());
        partPath = stage.partPath();
        for (std::size_t offset = 0; offset < content.size(); offset += 65536) {
            const std::size_t take = std::min<std::size_t>(65536, content.size() - offset);
            stage.writeAt(offset, content.data() + offset, take);
        }
        CHECK(stage.written() == content.size());
        stage.finish(sha256Hex(content.data(), content.size()));
        CHECK(fs::exists(partPath));
    }
    CHECK(readAll(partPath) == content);

    const fs::path dest = g_tmp / "dest";
    fs::create_directories(dest);
    writeFile(dest / "file.bin", "anterior");
    const fs::path placed = placeFileNoOverwrite(partPath, dest, "file.bin");
    CHECK(placed.filename().u8string() == "file (1).bin");
    CHECK(readAll(dest / "file.bin") == "anterior");
    CHECK(readAll(placed) == content);

    begin("truncado y digest");
    {
        StagedFile partial(staging, "partial.bin", 1000);
        partial.writeAt(0, content.data(), 600);
        CHECK_THROWS(partial.finish(sha256Hex(content.data(), 600)));
        partial.discard();
        CHECK(!fs::exists(partial.partPath()));
    }
    {
        StagedFile corrupt(staging, "corrupt.bin", 3);
        corrupt.writeAt(0, "abc", 3);
        CHECK_THROWS(corrupt.finish(sha256Hex("abd", 3)));
    }
    CHECK(!fs::exists(staging / "corrupt.bin.o2f-part"));

    {
        StagedFile ordered(staging, "ordered.bin", 10);
        CHECK_THROWS(ordered.writeAt(5, "abc", 3));
    }
    {
        StagedFile tooLong(staging, "long.bin", 2);
        CHECK_THROWS(tooLong.writeAt(0, "abcd", 4));
    }
    CHECK_THROWS(StagedFile(staging, "../escape", 1));
    CHECK_THROWS(placeFileNoOverwrite(partPath, g_tmp / "missing", "file.bin"));
}

void testCopyOut() {
    begin("copia al destino y colisiones");
    const fs::path source = g_tmp / "offer-source";
    writeFile(source / "a.txt", "nuevo");
    writeFile(source / "sub" / "b.txt", "b");

    Offer offer;
    offer.id = "offer-copy";
    offer.label = "3 elementos";
    offer.totalBytes = 4;
    Entry first;
    first.id = "e1";
    first.relPath = "a.txt";
    first.size = 5;
    Entry folder;
    folder.id = "e2";
    folder.relPath = "sub";
    folder.isDir = true;
    Entry second;
    second.id = "e3";
    second.relPath = "sub/b.txt";
    second.size = 1;
    offer.entries = {first, folder, second};

    const fs::path dest = g_tmp / "copy-dest";
    writeFile(dest / "a.txt", "antiguo");
    writeFile(dest / "sub" / "keep.txt", "conservar");

    CancelToken cancel;
    const CopyOutStats stats = copyOutOffer(offer, source, dest, cancel);
    CHECK(stats.files == 2);
    CHECK(stats.bytes == 6);
    CHECK(readAll(dest / "a.txt") == "antiguo");
    CHECK(readAll(dest / "a (1).txt") == "nuevo");
    CHECK(readAll(dest / "sub" / "keep.txt") == "conservar");
    CHECK(fs::is_directory(dest / "sub (1)"));
    CHECK(readAll(dest / "sub (1)" / "b.txt") == "b");

    cancel.cancel();
    CHECK_THROWS(copyOutOffer(offer, source, g_tmp / "copy-dest-2", cancel));
}

void testCacheLifecycle() {
    begin("caché y concesiones");
    CacheStore cache((g_tmp / "cache").u8string());
    const fs::path incoming = cache.newIncomingDir("0123456789abcdef0123456789abcdef");
    writeFile(incoming / "file.txt", "contenido");

    Offer offer;
    offer.id = "0123456789abcdef0123456789abcdef";
    offer.label = "file.txt";
    offer.createdAt = unixNow();
    Entry entry;
    entry.id = "entry-1";
    entry.relPath = "file.txt";
    entry.size = 9;
    offer.entries.push_back(entry);
    offer.totalBytes = 9;

    const fs::path published = cache.commitIncoming(incoming, offer);
    CHECK(fs::is_directory(published));
    CHECK(fs::exists(published / "manifest.tsv"));
    CHECK(cache.list().size() == 1);
    CHECK(!cache.findOfferDir(offer.id).empty());

    {
        auto lease = cache.acquire(offer.id);
        CHECK(lease != nullptr);
        CHECK(cache.sweep(0, unixNow()) == 0);
        CHECK(fs::is_directory(published));
    }
    CHECK(cache.sweep(0, unixNow() + 100) == 1);
    CHECK(!fs::exists(published));

    const fs::path secondIncoming = cache.newIncomingDir("ffffffffffffffffffffffffffffffff");
    writeFile(secondIncoming / "file.txt", "otro");
    Offer second = offer;
    second.id = "ffffffffffffffffffffffffffffffff";
    const fs::path secondDir = cache.commitIncoming(secondIncoming, second);
    CHECK(cache.sweep(3600, unixNow()) == 0);
    CHECK(fs::is_directory(secondDir));

    const fs::path orphan = cache.newIncomingDir("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
    CHECK(cache.sweep(0, unixNow() + 1000) >= 2);
    CHECK(!fs::exists(orphan));
    CHECK(!fs::exists(secondDir));
}

void testWire() {
    begin("tramas");
    LoopbackPair pair = makeLoopbackPair();
    writeFrame(*pair.a, MsgType::Hello);
    ByteWriter w;
    w.u32(kProtocolVersion);
    w.str("fetch");
    writeFrame(*pair.a, MsgType::Hello, w.take());
    writeFrame(*pair.a, MsgType::List);

    std::vector<std::uint8_t> payload;
    CHECK(readFrame(*pair.b, payload) == MsgType::Hello);
    CHECK(payload.empty());
    CHECK(readFrame(*pair.b, payload) == MsgType::Hello);
    ByteReader r(payload.data(), payload.size());
    CHECK(r.u32() == kProtocolVersion);
    CHECK(r.str(32) == "fetch");
    CHECK(readFrame(*pair.b, payload) == MsgType::List);

    LoopbackPair oversized = makeLoopbackPair();
    const std::uint8_t header[5] = {0xff, 0xff, 0xff, 0x7f, 1};
    oversized.a->writeAll(header, sizeof(header));
    CHECK_THROWS(readFrame(*oversized.b, payload));

    LoopbackPair unknown = makeLoopbackPair();
    const std::uint8_t badType[5] = {1, 0, 0, 0, 99};
    unknown.a->writeAll(badType, sizeof(badType));
    CHECK_THROWS(readFrame(*unknown.b, payload));

    LoopbackPair truncated = makeLoopbackPair();
    const std::uint8_t shortFrame[6] = {6, 0, 0, 0, 3, 'a'};
    truncated.a->writeAll(shortFrame, sizeof(shortFrame));
    truncated.a->close();
    CHECK_THROWS(readFrame(*truncated.b, payload));
}

void testProtocol() {
    begin("protocolo de transferencia");
    const fs::path root = makeTree();
    const OfferStore store = OfferStore::fromPaths({root.u8string()}, Limits{});
    const BuiltOffer built = buildOffer({root.u8string()}, Limits{});

    std::string serverError;
    {
        LoopbackPair pair = makeLoopbackPair();
        std::thread server([&] {
            try {
                serveSession(store, *pair.a, nullptr);
            } catch (const std::exception& e) {
                serverError = e.what();
            }
        });
        const fs::path recv = g_tmp / "recv";
        {
            auto link = RemoteLink::attach(std::move(pair.b), "fetch", Limits{});
            link->requestManifest();
            CHECK(link->offer().entries.size() == store.offer().entries.size());
            CHECK(link->offer().id == store.offer().id);

            std::size_t index = 0;
            for (const Entry& entry : link->offer().entries) {
                CHECK(entry.id == store.offer().entries[index].id);
                ++index;
            }

            for (const Entry& entry : link->offer().entries) {
                if (entry.isDir) continue;
                CHECK(entry.size <= 300000);
                StagedFile stage(recv, entry.relPath, entry.size);
                CancelToken cancel;
                link->fetchFile(entry, stage, cancel, nullptr);
                const fs::path source = fs::u8path(built.absoluteByRel.at(entry.relPath));
                CHECK(readAll(stage.partPath()) == readAll(source));
            }
            {
                StagedFile stage(recv, "unauthorized.txt", 1);
                CancelToken cancel;
                Entry fake;
                fake.id = "no-existe";
                fake.relPath = "unauthorized.txt";
                fake.size = 1;
                CHECK_THROWS(link->fetchFile(fake, stage, cancel, nullptr));
            }
        }
        server.join();
    }
    CHECK(serverError.empty());

    begin("lectura bajo demanda");
    {
        LoopbackPair pair = makeLoopbackPair();
        std::thread server([&] {
            try {
                serveSession(store, *pair.a, nullptr);
            } catch (const std::exception&) {
            }
        });
        {
            auto link = RemoteLink::attach(std::move(pair.b), "ondemand", Limits{});
            link->requestManifest();
            const Entry* big = nullptr;
            for (const Entry& entry : link->offer().entries) {
                if (!entry.isDir && entry.size == 300000) big = &entry;
            }
            CHECK(big != nullptr);
            if (big != nullptr) {
                std::string expected = readAll(root / "sub" / "b.bin");
                std::string buffer(1000, '\0');
                CHECK(link->readAt(*big, 0, buffer.data(), buffer.size()) == 1000);
                CHECK(buffer == expected.substr(0, 1000));
                CHECK(link->readAt(*big, 150000, buffer.data(), buffer.size()) == 1000);
                CHECK(buffer == expected.substr(150000, 1000));
                CHECK(link->readAt(*big, 300000, buffer.data(), buffer.size()) == 0);
                Entry fake{"inventado", "x.bin", false, 10};
                CHECK_THROWS(link->readAt(fake, 0, buffer.data(), 10));
            }
        }
        server.join();
    }

    begin("cancelación en curso");
    {
        LoopbackPair pair = makeLoopbackPair();
        std::thread server([&] {
            try {
                serveSession(store, *pair.a, nullptr);
            } catch (const std::exception&) {
            }
        });
        {
            auto link = RemoteLink::attach(std::move(pair.b), "fetch", Limits{});
            link->requestManifest();
            const Entry* big = nullptr;
            for (const Entry& entry : link->offer().entries) {
                if (!entry.isDir && entry.size == 300000) big = &entry;
            }
            CHECK(big != nullptr);
            if (big != nullptr) {
                const fs::path recv = g_tmp / "cancel-recv";
                {
                    StagedFile stage(recv, big->relPath, big->size);
                    CancelToken cancel;
                    CHECK_THROWS(link->fetchFile(*big, stage, cancel, [&cancel](const Progress&) {
                        cancel.cancel();
                    }));
                }
                CHECK(!fs::exists(recv / "sub" / "b.bin.o2f-part"));
            }
        }
        server.join();
    }

    begin("petición no autorizada");
    {
        LoopbackPair pair = makeLoopbackPair();
        std::thread server([&] {
            try {
                serveSession(store, *pair.a, nullptr);
            } catch (const std::exception&) {
            }
        });
        {
            ByteWriter hello;
            hello.u32(kProtocolVersion);
            hello.str("fetch");
            writeFrame(*pair.b, MsgType::Hello, hello.take());
            std::vector<std::uint8_t> payload;
            CHECK(readFrame(*pair.b, payload) == MsgType::HelloOk);
            writeFrame(*pair.b, MsgType::Get, encodeEntryRef("identificador-ajeno"));
            CHECK(readFrame(*pair.b, payload) == MsgType::Error);
            ByteReader r(payload.data(), payload.size());
            CHECK(!r.str(512).empty());
        }
        server.join();
    }
}

void testOrphanCleanup() {
    begin("limpieza de temporales");
    const fs::path dir = g_tmp / "orphans";
    writeFile(dir / "one.o2f-part", "x");
    writeFile(dir / "keep.txt", "y");
    CHECK(removeOrphanParts(dir) == 1);
    CHECK(!fs::exists(dir / "one.o2f-part"));
    CHECK(fs::exists(dir / "keep.txt"));
}

// Certificado público autofirmado, generado solo para esta prueba. No incluye
// clave privada ni se usa para abrir red: comprueba el cálculo de la huella.
const char* kTestCertificatePem = R"(-----BEGIN CERTIFICATE-----
MIIDMzCCAhugAwIBAgIUWOfckL1uyBR9n1m5jmd8MPnc4AQwDQYJKoZIhvcNAQEL
BQAwKTEnMCUGA1UEAwweT25lSW5Ud28gc2VsZi10ZXN0IGNlcnRpZmljYXRlMB4X
DTI2MTEwNDE4MDEzNloXDTM2MTEwMTE4MDEzNlowKTEnMCUGA1UEAwweT25lSW5U
d28gc2VsZi10ZXN0IGNlcnRpZmljYXRlMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8A
MIIBCgKCAQEAp99rQr/cbo5q698L9bmhlXIkURZooJC3T6xLY0cMUVt513NFQu08
BgCj81FO+ABpT6d+abEGwJNGBlUW2mevI1Ch+884mKpKJVtT7iK960ZDl+rgdi9C
Ij/blfEGPv2X3mEPRdeIu3kqOkSCZJdMwbZcZw0Ljdr0PY3sgfNn/jKdMko18FHe
fOSSXGsVBdkDHDYjpr7rZHqy8E2NIjbdHwUy9WBwIeH0lia//5Xp1yMH3wzmHL1f
gkcaLuxTky7KxU/TnhFOE8BA4tKBnz7b+m5jsFTQhzHE642aBL8cHfHT6BJHf4yW
9JRtCl3vjBaVT6/3yZQvxQKxqepShDE35QIDAQABo1MwUTAdBgNVHQ4EFgQUwQ25
fl9HB1WIojp1S1fobk1a1pYwHwYDVR0jBBgwFoAUwQ25fl9HB1WIojp1S1fobk1a
1pYwDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAguzHAL0j0/wd
KjtM0rVDzj3RVLkeY7tm9rn+whEQXtlhUZfgYkC3Y3csv26047wKg4vzeEMsKhFP
WvRgyo3UWLKxeK1oYxG1hyYjrVQds6AmsBrL51yS/qHRt/7qTW4IHoa+g8AQKrBa
fGM31pRgYtfUkAfmkR5nXeJJPFOldYRJEmDca2QNuX0PK0fVHDCa56WwyZipPl4Z
lG9szAs18XOj/jlmqWspYt8rANE6Re9CGgv0VTJhQ97B4pTM1RxFTmyJwZCPObKK
KBsdDrV8fKXIKW+E4ooQ635iEFWpQ3+mgrYUe9TDtfnGfwOIsFYhgECnOkx/8c4U
Sj+sEpQR8Q==
-----END CERTIFICATE-----
)";

void testCertificateFingerprint() {
    begin("huella de certificado");
    if (!tlsAvailable()) {
        std::cout << "huella de certificado: omitida (compilado sin OpenSSL)\n";
        return;
    }
    const fs::path cert = g_tmp / "selftest-cert.pem";
    writeFile(cert, kTestCertificatePem);
    const std::string pin = certificateFingerprint(cert.u8string());
    CHECK(pin.size() == 64);
    CHECK(pin == "5b9e9ce508e973354a5d7fccb55b8858ebcd2312b66b1dcddb46cb41cc74b9d8");
    const fs::path invalid = g_tmp / "invalid-cert.pem";
    writeFile(invalid, "no es un certificado PEM\n");
    CHECK_THROWS(certificateFingerprint(invalid.u8string()));
}

}  // namespace

int runSelfTests(bool verbose) {
    g_verbose = verbose;
    g_failures = 0;
    g_checks = 0;
#if defined(_WIN32)
    const int pid = _getpid();
#else
    const int pid = static_cast<int>(getpid());
#endif
    g_tmp = fs::current_path() / (".o2f-selftest-" + std::to_string(pid));
    std::error_code ec;
    fs::remove_all(g_tmp, ec);
    fs::create_directories(g_tmp, ec);
    if (!fs::is_directory(g_tmp)) {
        std::cerr << "no se pudo crear el temporal de pruebas\n";
        return 1;
    }
    std::cout << "self-test aislado en " << g_tmp.u8string() << '\n';

    try {
        testSha256();
        testPathValidation();
        testManifest();
        testManifestText();
        testStore();
        testStagedFiles();
        testCopyOut();
        testCacheLifecycle();
        testWire();
        testProtocol();
        testCertificateFingerprint();
        testOrphanCleanup();
    } catch (const std::exception& e) {
        ++g_failures;
        std::cerr << "FALLO no controlado [" << g_current << "]: " << e.what() << '\n';
    }

    std::cout << g_checks << " comprobaciones, " << g_failures << " fallos\n";
    if (g_failures == 0) {
        fs::remove_all(g_tmp, ec);
        std::cout << "self-test correcto (sin portapapeles ni red)\n";
        return 0;
    }
    std::cerr << "los datos del fallo se conservan en " << g_tmp.u8string() << '\n';
    return 1;
}

}  // namespace o2f
