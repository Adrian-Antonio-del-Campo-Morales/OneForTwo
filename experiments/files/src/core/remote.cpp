// Responsibility: client and file protocol server. Each request
// is validated against the current offer; errors are communicated without local routes.
#include "core/remote.hpp"

#include <algorithm>
#include <vector>

#include "core/net.hpp"
#include "core/wire.hpp"

namespace o2f {
namespace {

std::string decodeErrorPayload(const std::vector<std::uint8_t>& payload) {
    try {
        ByteReader r(payload.data(), payload.size());
        return r.str(512);
    } catch (const Error&) {
        return "error remoto sin detalle";
    }
}

void sendError(Stream& stream, const std::string& message) {
    try {
        writeFrame(stream, MsgType::Error, encodeError(message));
    } catch (const Error&) {
        // The pair no longer listens; the connection will close.
    }
}

struct Hello {
    std::uint32_t version = 0;
    std::string mode;
};

Hello parseHello(const std::vector<std::uint8_t>& payload) {
    ByteReader r(payload.data(), payload.size());
    Hello hello;
    hello.version = r.u32();
    hello.mode = r.str(32);
    if (r.remaining() != 0) throw Error("greeting with excess data");
    return hello;
}

void sendChunk(Stream& stream, const std::string& entryId, std::uint64_t offset,
               const std::uint8_t* bytes, std::size_t len) {
    ByteWriter w;
    w.str(entryId);
    w.u64(offset);
    w.bytes(bytes, len);
    writeFrame(stream, MsgType::Data, w.take());
}

void streamFile(Stream& stream, const OfferStore& store, const Entry& entry,
                const Limits& limits) {
    Sha256 hash;
    std::uint64_t offset = 0;
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(
        std::min<std::uint64_t>(limits.chunkBytes, 256u * 1024u)));
    while (offset < entry.size) {
        const std::size_t want = static_cast<std::size_t>(
            std::min<std::uint64_t>(buffer.size(), entry.size - offset));
        const std::size_t got = store.read(entry.id, offset, buffer.data(), want);
        if (got == 0) throw Error("the source file ended before the declared");
        sendChunk(stream, entry.id, offset, buffer.data(), got);
        hash.update(buffer.data(), got);
        offset += got;
    }
    ByteWriter w;
    w.str(entry.id);
    w.u64(entry.size);
    w.str(hash.hexDigest());
    writeFrame(stream, MsgType::FileEnd, w.take());
}

}  // namespace

RemoteLink::RemoteLink(std::unique_ptr<Stream> stream, const Limits& limits)
    : stream_(std::move(stream)), limits_(limits) {}

RemoteLink::~RemoteLink() {
    try {
        sayGoodbye();
    } catch (const Error&) {
    }
    if (stream_) stream_->close();
}

std::unique_ptr<RemoteLink> RemoteLink::attach(std::unique_ptr<Stream> stream,
                                               const std::string& mode, const Limits& limits) {
    if (!stream) throw Error("non-initialized remote flow");
    std::unique_ptr<RemoteLink> link(new RemoteLink(std::move(stream), limits));
    link->handshake(mode);
    return link;
}

std::unique_ptr<RemoteLink> RemoteLink::connect(const std::string& host, std::uint16_t port,
                                                const std::string& pin,
                                                const std::string& clientCertFile,
                                                const std::string& clientKeyFile,
                                                const std::string& mode, int timeoutMs,
                                                const Limits& limits) {
    if (!tlsAvailable()) throw Error("TLS not available: compile with OpenSSL to use the network");
    std::unique_ptr<TcpStream> raw = tcpConnect(host, port, timeoutMs);
    raw->setReadTimeoutMs(timeoutMs);
    std::unique_ptr<Stream> secure =
        wrapTlsClient(std::move(raw), pin, clientCertFile, clientKeyFile);
    return attach(std::move(secure), mode, limits);
}

void RemoteLink::handshake(const std::string& mode) {
    ByteWriter hello;
    hello.u32(kProtocolVersion);
    hello.str(mode);
    writeFrame(*stream_, MsgType::Hello, hello.take());

    std::vector<std::uint8_t> payload;
    const MsgType reply = readFrame(*stream_, payload);
    if (reply == MsgType::Error) throw Error("the server rejected the connection: " + decodeErrorPayload(payload));
    if (reply != MsgType::HelloOk) throw Error("unexpected greeting response");
    ByteReader r(payload.data(), payload.size());
    const std::string offerId = r.str(128);
    const std::string label = r.str(256);
    if (offerId.empty()) throw Error("the server did not announce an offer");
    if (r.remaining() != 0) throw Error("response greeting with excess data");
    if (mode == "fetch" && label.size() > 256) throw Error("out-of-limit offer label");
}

void RemoteLink::requestManifest() {
    std::lock_guard<std::mutex> lock(mutex_);
    writeFrame(*stream_, MsgType::List);
    std::vector<std::uint8_t> payload;
    const MsgType reply = readFrame(*stream_, payload);
    if (reply == MsgType::Error) throw Error("the server rejected the offer: " + decodeErrorPayload(payload));
    if (reply != MsgType::Manifest) throw Error("unexpected manifest response");
    offer_ = decodeOffer(payload.data(), payload.size(), limits_);
    for (const Entry& e : offer_.entries) {
        if (!e.isDir && e.size > limits_.maxTotalBytes) throw Error("file off-limit");
    }
}

void RemoteLink::fetchFile(const Entry& entry, StagedFile& stage, CancelToken& cancel,
                           const ProgressFn& progress) {
    if (offer_.find(entry.id) == nullptr) throw Error("the item does not belong to the offer received");
    if (entry.isDir) throw Error("no folder content requested");

    std::lock_guard<std::mutex> lock(mutex_);
    writeFrame(*stream_, MsgType::Get, encodeEntryRef(entry.id));
    std::uint64_t expectedOffset = 0;
    for (;;) {
        cancel.throwIfCancelled();
        std::vector<std::uint8_t> payload;
        const MsgType reply = readFrame(*stream_, payload);
        if (reply == MsgType::Error) throw Error("remote failure: " + decodeErrorPayload(payload));
        if (reply == MsgType::FileEnd) {
            ByteReader r(payload.data(), payload.size());
            const std::string id = r.str(128);
            const std::uint64_t size = r.u64();
            const std::string digest = r.str(128);
            if (id != entry.id) throw Error("file end for another item");
            if (size != entry.size || expectedOffset != entry.size) {
                throw Error("size received other than declared");
            }
            stage.finish(digest);
            return;
        }
        if (reply != MsgType::Data) throw Error("unexpected response during transfer");
        ByteReader r(payload.data(), payload.size());
        const std::string id = r.str(128);
        const std::uint64_t offset = r.u64();
        std::vector<std::uint8_t> chunk(r.remaining());
        if (!chunk.empty()) r.bytes(chunk.data(), chunk.size());
        if (id != entry.id) throw Error("block of another element");
        if (offset != expectedOffset) throw Error("Out of order block");
        if (chunk.empty()) throw Error("empty block in transfer");
        stage.writeAt(offset, chunk.data(), chunk.size());
        expectedOffset += chunk.size();
        if (expectedOffset > entry.size) throw Error("more bytes were received than the declared size");
        if (progress) {
            Progress p;
            p.bytesDone = expectedOffset;
            p.bytesTotal = entry.size;
            progress(p);
        }
    }
}

std::size_t RemoteLink::readAt(const Entry& entry, std::uint64_t offset, void* buffer,
                               std::size_t len) {
    if (offer_.find(entry.id) == nullptr) throw Error("the item does not belong to the offer received");
    if (entry.isDir) throw Error("no folder content requested");
    if (offset > entry.size) throw Error("scrolling out of the file");
    if (len > limits_.chunkBytes) throw Error("reading greater than the allowed block");

    std::lock_guard<std::mutex> lock(mutex_);
    ByteWriter w;
    w.str(entry.id);
    w.u64(offset);
    w.u32(static_cast<std::uint32_t>(len));
    writeFrame(*stream_, MsgType::Read, w.take());

    std::vector<std::uint8_t> payload;
    const MsgType reply = readFrame(*stream_, payload);
    if (reply == MsgType::Error) throw Error("remote failure: " + decodeErrorPayload(payload));
    if (reply != MsgType::Data) throw Error("unexpected response to reading");
    ByteReader r(payload.data(), payload.size());
    const std::string id = r.str(128);
    const std::uint64_t dataOffset = r.u64();
    if (id != entry.id || dataOffset != offset) throw Error("Incoherent reading response");
    const std::size_t available = r.remaining();
    const std::size_t take = std::min(len, available);
    if (take > 0) r.bytes(buffer, take);
    if (available > len) throw Error("server returned more bytes than requested");
    return take;
}

void RemoteLink::sayGoodbye() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!stream_) return;
    writeFrame(*stream_, MsgType::Bye);
}

void serveSession(const OfferStore& store, Stream& stream,
                  const std::function<void(const std::string&)>& note) {
    std::vector<std::uint8_t> payload;
    const MsgType first = readFrame(stream, payload);
    if (first != MsgType::Hello) throw Error("the customer's greeting was expected");
    const Hello hello = parseHello(payload);
    if (hello.version != kProtocolVersion) {
        sendError(stream, "non-compatible protocol version");
        throw Error("client with different protocol version");
    }
    if (hello.mode != "fetch" && hello.mode != "ondemand") {
        sendError(stream, "Unknown session mode");
        throw Error("Unknown session mode");
    }
    {
        ByteWriter w;
        w.str(store.offer().id);
        w.str(store.offer().label);
        writeFrame(stream, MsgType::HelloOk, w.take());
    }
    if (note) note("session initiated in mode " + hello.mode);

    for (;;) {
        const MsgType type = readFrame(stream, payload);
        if (type == MsgType::Bye) return;
        if (type == MsgType::List) {
            const std::vector<std::uint8_t> encoded = encodeOffer(store.offer());
            if (encoded.size() > kMaxFrameBytes) {
                sendError(stream, "manifest too big for a plot");
                throw Error("out-of-limit manifest");
            }
            writeFrame(stream, MsgType::Manifest, encoded);
            continue;
        }
        if (type == MsgType::Get) {
            ByteReader r(payload.data(), payload.size());
            const std::string id = r.str(128);
            const SourceItem* item = store.find(id);
            if (item == nullptr) {
                sendError(stream, "unauthorised item");
                throw Error("request for an item not offered");
            }
            if (item->meta.isDir) {
                sendError(stream, "the element is a folder");
                continue;
            }
            try {
                streamFile(stream, store, item->meta, Limits{});
            } catch (const std::exception& e) {
                // The origin could change during shipment: the client must see a
                // explicit error and not an incomplete file as finished.
                sendError(stream, e.what());
                throw;
            }
            continue;
        }
        if (type == MsgType::Read) {
            ByteReader r(payload.data(), payload.size());
            const std::string id = r.str(128);
            const std::uint64_t offset = r.u64();
            const std::uint32_t len = r.u32();
            const SourceItem* item = store.find(id);
            if (item == nullptr) {
                sendError(stream, "unauthorised item");
                throw Error("request for an item not offered");
            }
            if (item->meta.isDir) {
                sendError(stream, "the element is a folder");
                continue;
            }
            if (len > 256u * 1024u) {
                sendError(stream, "too large reading block");
                continue;
            }
            std::vector<std::uint8_t> buf(len);
            const std::size_t got = store.read(id, offset, buf.data(), buf.size());
            sendChunk(stream, id, offset, buf.data(), got);
            continue;
        }
        sendError(stream, "unexpected message");
        throw Error("unexpected protocol message");
    }
}

void serveOffer(const OfferStore& store, const ServerOptions& options, CancelToken& cancel,
                const std::function<void(const std::string&)>& note) {
    if (!tlsAvailable()) throw Error("TLS not available: Compile with OpenSSL to serve the offer");
    if (options.certFile.empty() || options.keyFile.empty()) {
        throw Error("certificate and key required to serve an offer");
    }
    if (options.clientPin.size() != 64) {
        throw Error("the customer certificate footprint is required (--client-pin)");
    }
    std::string bindError;
    std::unique_ptr<TcpListener> listener =
        TcpListener::bind(options.bindAddress, options.port, bindError);
    if (!listener) throw Error(bindError);
    if (note) note("escuchando en " + options.bindAddress + ":" + std::to_string(listener->port()));

    while (!cancel.cancelled()) {
        std::unique_ptr<TcpStream> raw = listener->accept(500);
        if (!raw) continue;
        raw->setReadTimeoutMs(options.readTimeoutMs);
        try {
            std::unique_ptr<Stream> secure =
                wrapTlsServer(std::move(raw), options.certFile, options.keyFile, options.clientPin);
            if (note) note("mutual accepted customer TLS");
            serveSession(store, *secure, note);
            if (note) note("session completed");
        } catch (const std::exception& e) {
            if (note) note(std::string("meeting rejected: ") + e.what());
        }
    }
}

}  // namespace o2f
