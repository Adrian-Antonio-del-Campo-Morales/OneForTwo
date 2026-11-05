// Responsabilidad: cliente y servidor del protocolo de archivos. Cada petición
// se valida contra la oferta vigente; los errores se comunican sin rutas locales.
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
        // El par ya no escucha; la conexión se cerrará.
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
    if (r.remaining() != 0) throw Error("saludo con datos sobrantes");
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
        if (got == 0) throw Error("el archivo de origen terminó antes de lo declarado");
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
    if (!stream) throw Error("flujo remoto no inicializado");
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
    if (!tlsAvailable()) throw Error("TLS no disponible: compile con OpenSSL para usar la red");
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
    if (reply == MsgType::Error) throw Error("el servidor rechazó la conexión: " + decodeErrorPayload(payload));
    if (reply != MsgType::HelloOk) throw Error("respuesta de saludo inesperada");
    ByteReader r(payload.data(), payload.size());
    const std::string offerId = r.str(128);
    const std::string label = r.str(256);
    if (offerId.empty()) throw Error("el servidor no anunció una oferta");
    if (r.remaining() != 0) throw Error("saludo de respuesta con datos sobrantes");
    if (mode == "fetch" && label.size() > 256) throw Error("etiqueta de oferta fuera de límite");
}

void RemoteLink::requestManifest() {
    std::lock_guard<std::mutex> lock(mutex_);
    writeFrame(*stream_, MsgType::List);
    std::vector<std::uint8_t> payload;
    const MsgType reply = readFrame(*stream_, payload);
    if (reply == MsgType::Error) throw Error("el servidor rechazó la oferta: " + decodeErrorPayload(payload));
    if (reply != MsgType::Manifest) throw Error("respuesta de manifiesto inesperada");
    offer_ = decodeOffer(payload.data(), payload.size(), limits_);
    for (const Entry& e : offer_.entries) {
        if (!e.isDir && e.size > limits_.maxTotalBytes) throw Error("archivo fuera de límite");
    }
}

void RemoteLink::fetchFile(const Entry& entry, StagedFile& stage, CancelToken& cancel,
                           const ProgressFn& progress) {
    if (offer_.find(entry.id) == nullptr) throw Error("el elemento no pertenece a la oferta recibida");
    if (entry.isDir) throw Error("no se solicita contenido de una carpeta");

    std::lock_guard<std::mutex> lock(mutex_);
    writeFrame(*stream_, MsgType::Get, encodeEntryRef(entry.id));
    std::uint64_t expectedOffset = 0;
    for (;;) {
        cancel.throwIfCancelled();
        std::vector<std::uint8_t> payload;
        const MsgType reply = readFrame(*stream_, payload);
        if (reply == MsgType::Error) throw Error("fallo remoto: " + decodeErrorPayload(payload));
        if (reply == MsgType::FileEnd) {
            ByteReader r(payload.data(), payload.size());
            const std::string id = r.str(128);
            const std::uint64_t size = r.u64();
            const std::string digest = r.str(128);
            if (id != entry.id) throw Error("fin de archivo para otro elemento");
            if (size != entry.size || expectedOffset != entry.size) {
                throw Error("tamaño recibido distinto del declarado");
            }
            stage.finish(digest);
            return;
        }
        if (reply != MsgType::Data) throw Error("respuesta inesperada durante la transferencia");
        ByteReader r(payload.data(), payload.size());
        const std::string id = r.str(128);
        const std::uint64_t offset = r.u64();
        std::vector<std::uint8_t> chunk(r.remaining());
        if (!chunk.empty()) r.bytes(chunk.data(), chunk.size());
        if (id != entry.id) throw Error("bloque de otro elemento");
        if (offset != expectedOffset) throw Error("bloque fuera de orden");
        if (chunk.empty()) throw Error("bloque vacío en la transferencia");
        stage.writeAt(offset, chunk.data(), chunk.size());
        expectedOffset += chunk.size();
        if (expectedOffset > entry.size) throw Error("se recibieron más bytes que el tamaño declarado");
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
    if (offer_.find(entry.id) == nullptr) throw Error("el elemento no pertenece a la oferta recibida");
    if (entry.isDir) throw Error("no se solicita contenido de una carpeta");
    if (offset > entry.size) throw Error("desplazamiento fuera del archivo");
    if (len > limits_.chunkBytes) throw Error("lectura mayor que el bloque permitido");

    std::lock_guard<std::mutex> lock(mutex_);
    ByteWriter w;
    w.str(entry.id);
    w.u64(offset);
    w.u32(static_cast<std::uint32_t>(len));
    writeFrame(*stream_, MsgType::Read, w.take());

    std::vector<std::uint8_t> payload;
    const MsgType reply = readFrame(*stream_, payload);
    if (reply == MsgType::Error) throw Error("fallo remoto: " + decodeErrorPayload(payload));
    if (reply != MsgType::Data) throw Error("respuesta inesperada a la lectura");
    ByteReader r(payload.data(), payload.size());
    const std::string id = r.str(128);
    const std::uint64_t dataOffset = r.u64();
    if (id != entry.id || dataOffset != offset) throw Error("respuesta de lectura incoherente");
    const std::size_t available = r.remaining();
    const std::size_t take = std::min(len, available);
    if (take > 0) r.bytes(buffer, take);
    if (available > len) throw Error("el servidor devolvió más bytes de los solicitados");
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
    if (first != MsgType::Hello) throw Error("se esperaba el saludo del cliente");
    const Hello hello = parseHello(payload);
    if (hello.version != kProtocolVersion) {
        sendError(stream, "versión de protocolo no compatible");
        throw Error("cliente con versión de protocolo distinta");
    }
    if (hello.mode != "fetch" && hello.mode != "ondemand") {
        sendError(stream, "modo de sesión desconocido");
        throw Error("modo de sesión desconocido");
    }
    {
        ByteWriter w;
        w.str(store.offer().id);
        w.str(store.offer().label);
        writeFrame(stream, MsgType::HelloOk, w.take());
    }
    if (note) note("sesión iniciada en modo " + hello.mode);

    for (;;) {
        const MsgType type = readFrame(stream, payload);
        if (type == MsgType::Bye) return;
        if (type == MsgType::List) {
            const std::vector<std::uint8_t> encoded = encodeOffer(store.offer());
            if (encoded.size() > kMaxFrameBytes) {
                sendError(stream, "manifiesto demasiado grande para una trama");
                throw Error("manifiesto fuera de límite");
            }
            writeFrame(stream, MsgType::Manifest, encoded);
            continue;
        }
        if (type == MsgType::Get) {
            ByteReader r(payload.data(), payload.size());
            const std::string id = r.str(128);
            const SourceItem* item = store.find(id);
            if (item == nullptr) {
                sendError(stream, "elemento no autorizado");
                throw Error("petición de un elemento no ofrecido");
            }
            if (item->meta.isDir) {
                sendError(stream, "el elemento es una carpeta");
                continue;
            }
            try {
                streamFile(stream, store, item->meta, Limits{});
            } catch (const std::exception& e) {
                // El origen pudo cambiar durante el envío: el cliente debe ver un
                // error explícito y no un archivo incompleto como terminado.
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
                sendError(stream, "elemento no autorizado");
                throw Error("petición de un elemento no ofrecido");
            }
            if (item->meta.isDir) {
                sendError(stream, "el elemento es una carpeta");
                continue;
            }
            if (len > 256u * 1024u) {
                sendError(stream, "bloque de lectura demasiado grande");
                continue;
            }
            std::vector<std::uint8_t> buf(len);
            const std::size_t got = store.read(id, offset, buf.data(), buf.size());
            sendChunk(stream, id, offset, buf.data(), got);
            continue;
        }
        sendError(stream, "mensaje inesperado");
        throw Error("mensaje de protocolo inesperado");
    }
}

void serveOffer(const OfferStore& store, const ServerOptions& options, CancelToken& cancel,
                const std::function<void(const std::string&)>& note) {
    if (!tlsAvailable()) throw Error("TLS no disponible: compile con OpenSSL para servir la oferta");
    if (options.certFile.empty() || options.keyFile.empty()) {
        throw Error("se requieren certificado y clave para servir una oferta");
    }
    if (options.clientPin.size() != 64) {
        throw Error("se requiere la huella del certificado de cliente (--client-pin)");
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
            if (note) note("cliente TLS mutuo aceptado");
            serveSession(store, *secure, note);
            if (note) note("sesión finalizada");
        } catch (const std::exception& e) {
            if (note) note(std::string("sesión rechazada: ") + e.what());
        }
    }
}

}  // namespace o2f
