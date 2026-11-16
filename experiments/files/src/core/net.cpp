// Responsibility: sockets TCP and TLS 1.2/1.3 by OpenSSL. The identity of the
// the mark SHA-256 of your certificate is checked; no
// different footprint nor falls to plain text.
#include "core/net.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "core/model.hpp"
#include "core/sha256.hpp"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#if defined(O2F_WITH_TLS)
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#endif

namespace o2f {
namespace fs = std::filesystem;
namespace {

#if defined(_WIN32)
using Socket = SOCKET;
constexpr Socket kInvalidSocket = INVALID_SOCKET;
void closeSocket(Socket s) { ::closesocket(s); }
void ensureNetInit() {
    static const bool ok = [] {
        WSADATA data;
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    if (!ok) throw Error("could not initialise Winsock");
}
std::string lastNetError() {
    char buffer[64] = {0};
    std::snprintf(buffer, sizeof(buffer), "Network error %d", WSAGetLastError());
    return buffer;
}
bool wouldBlock() {
    const int code = WSAGetLastError();
    return code == WSAEWOULDBLOCK || code == WSAEINPROGRESS;
}
#else
using Socket = int;
constexpr Socket kInvalidSocket = -1;
void closeSocket(Socket s) { ::close(s); }
void ensureNetInit() {}
std::string lastNetError() { return std::string("network error: ") + std::strerror(errno); }
bool wouldBlock() { return errno == EINPROGRESS || errno == EWOULDBLOCK || errno == EAGAIN; }
#endif

void setNonBlocking(Socket s, bool on) {
#if defined(_WIN32)
    u_long mode = on ? 1 : 0;
    ioctlsocket(s, FIONBIO, &mode);
#else
    const int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
#endif
}

class SocketHandle {
public:
    explicit SocketHandle(Socket s) : s_(s) {}
    ~SocketHandle() {
        if (s_ != kInvalidSocket) closeSocket(s_);
    }
    Socket get() const { return s_; }
    Socket release() {
        const Socket s = s_;
        s_ = kInvalidSocket;
        return s;
    }

private:
    Socket s_;
};

bool waitWritable(Socket s, int timeoutMs) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(s, &set);
    timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
#if defined(_WIN32)
    return ::select(0, nullptr, &set, nullptr, &tv) > 0;
#else
    return ::select(s + 1, nullptr, &set, nullptr, &tv) > 0;
#endif
}

bool waitReadable(Socket s, int timeoutMs) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(s, &set);
    timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
#if defined(_WIN32)
    return ::select(0, &set, nullptr, nullptr, &tv) > 0;
#else
    return ::select(s + 1, &set, nullptr, nullptr, &tv) > 0;
#endif
}

}  // namespace

TcpStream::~TcpStream() { close(); }

std::size_t TcpStream::readSome(void* buffer, std::size_t len) {
    if (handle_ == -1) return 0;
    for (;;) {
        const int n = ::recv(static_cast<Socket>(handle_), static_cast<char*>(buffer),
                             static_cast<int>(len), 0);
        if (n > 0) return static_cast<std::size_t>(n);
        if (n == 0) return 0;
        if (wouldBlock()) continue;
        throw Error(lastNetError());
    }
}

void TcpStream::writeAll(const void* buffer, std::size_t len) {
    if (handle_ == -1) throw Error("closed connection");
    const auto* p = static_cast<const char*>(buffer);
    std::size_t done = 0;
    while (done < len) {
        const int n = ::send(static_cast<Socket>(handle_), p + done,
                             static_cast<int>(len - done), 0);
        if (n > 0) {
            done += static_cast<std::size_t>(n);
            continue;
        }
        if (n < 0 && wouldBlock()) continue;
        throw Error(lastNetError());
    }
}

void TcpStream::close() {
    if (handle_ != -1) {
        closeSocket(static_cast<Socket>(handle_));
        handle_ = -1;
    }
}

void TcpStream::setReadTimeoutMs(int ms) {
    if (handle_ == -1) return;
#if defined(_WIN32)
    const DWORD timeout = static_cast<DWORD>(ms);
    ::setsockopt(static_cast<Socket>(handle_), SOL_SOCKET, SO_RCVTIMEO,
                 reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    ::setsockopt(static_cast<Socket>(handle_), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

std::unique_ptr<TcpStream> tcpConnect(const std::string& host, std::uint16_t port, int timeoutMs) {
    ensureNetInit();
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    const std::string service = std::to_string(port);
    if (::getaddrinfo(host.c_str(), service.c_str(), &hints, &result) != 0 || result == nullptr) {
        throw Error("could not resolve remote computer address");
    }
    struct AddrGuard {
        addrinfo* p;
        ~AddrGuard() { ::freeaddrinfo(p); }
    } guard{result};

    for (addrinfo* ai = result; ai != nullptr; ai = ai->ai_next) {
        SocketHandle sock(::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol));
        if (sock.get() == kInvalidSocket) continue;
        setNonBlocking(sock.get(), true);
        const int rc = ::connect(sock.get(), ai->ai_addr, static_cast<int>(ai->ai_addrlen));
        bool connected = rc == 0;
        if (!connected && wouldBlock()) {
            connected = waitWritable(sock.get(), timeoutMs);
            if (connected) {
                int soError = 0;
                socklen_t len = sizeof(soError);
                if (::getsockopt(sock.get(), SOL_SOCKET, SO_ERROR,
                                 reinterpret_cast<char*>(&soError), &len) != 0 ||
                    soError != 0) {
                    connected = false;
                }
            }
        }
        if (connected) {
            setNonBlocking(sock.get(), false);
            return std::unique_ptr<TcpStream>(new TcpStream(static_cast<std::intptr_t>(sock.release())));
        }
    }
    throw Error("could not connect to remote computer");
}

TcpListener::~TcpListener() { close(); }

std::unique_ptr<TcpListener> TcpListener::bind(const std::string& address, std::uint16_t port,
                                               std::string& err) {
    ensureNetInit();
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    addrinfo* result = nullptr;
    const std::string service = std::to_string(port);
    // "0.0.0.0" is explicitly passed to listen in IPv4; with nullptr
    // getaddrinfo can first return a non-dual IPv6 socket.
    const char* nodePtr = address.empty() ? nullptr : address.c_str();
    if (::getaddrinfo(nodePtr, service.c_str(), &hints, &result) != 0 || result == nullptr) {
        err = "could not prepare the listening address";
        return nullptr;
    }
    struct AddrGuard {
        addrinfo* p;
        ~AddrGuard() { ::freeaddrinfo(p); }
    } guard{result};

    for (addrinfo* ai = result; ai != nullptr; ai = ai->ai_next) {
        SocketHandle sock(::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol));
        if (sock.get() == kInvalidSocket) continue;
        int yes = 1;
        ::setsockopt(sock.get(), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes),
                     sizeof(yes));
        if (::bind(sock.get(), ai->ai_addr, static_cast<int>(ai->ai_addrlen)) != 0) continue;
        if (::listen(sock.get(), 4) != 0) continue;

        std::unique_ptr<TcpListener> listener(new TcpListener());
        listener->handle_ = static_cast<std::intptr_t>(sock.release());
        sockaddr_storage bound{};
        socklen_t boundLen = sizeof(bound);
        if (::getsockname(static_cast<Socket>(listener->handle_),
                          reinterpret_cast<sockaddr*>(&bound), &boundLen) == 0) {
            if (bound.ss_family == AF_INET) {
                listener->port_ = ntohs(reinterpret_cast<sockaddr_in*>(&bound)->sin_port);
            } else if (bound.ss_family == AF_INET6) {
                listener->port_ = ntohs(reinterpret_cast<sockaddr_in6*>(&bound)->sin6_port);
            }
        }
        return listener;
    }
    err = "could not open listening port";
    return nullptr;
}

std::unique_ptr<TcpStream> TcpListener::accept(int timeoutMs) {
    if (handle_ == -1) return nullptr;
    const Socket listener = static_cast<Socket>(handle_);
    if (!waitReadable(listener, timeoutMs)) return nullptr;
    const Socket client = ::accept(listener, nullptr, nullptr);
    if (client == kInvalidSocket) return nullptr;
    return std::unique_ptr<TcpStream>(new TcpStream(static_cast<std::intptr_t>(client)));
}

void TcpListener::close() {
    if (handle_ != -1) {
        closeSocket(static_cast<Socket>(handle_));
        handle_ = -1;
    }
}

bool tlsAvailable() {
#if defined(O2F_WITH_TLS)
    return true;
#else
    return false;
#endif
}

std::string tlsBackend() {
#if defined(O2F_WITH_TLS)
    return std::string("OpenSSL ") + OPENSSL_VERSION_TEXT;
#else
    return "without TLS (compiled without OpenSSL)";
#endif
}

#if defined(O2F_WITH_TLS)
namespace {

std::string fingerprintOfDer(const unsigned char* der, int len) {
    return sha256Hex(der, static_cast<std::size_t>(len));
}

class TlsStream : public Stream {
public:
    TlsStream(std::unique_ptr<TcpStream> raw, SSL_CTX* ctx, SSL* ssl)
        : raw_(std::move(raw)), ctx_(ctx), ssl_(ssl) {}
    ~TlsStream() override { close(); }

    std::size_t readSome(void* buffer, std::size_t len) override {
        for (;;) {
            const int n = SSL_read(ssl_, buffer, static_cast<int>(len));
            if (n > 0) return static_cast<std::size_t>(n);
            const int err = SSL_get_error(ssl_, n);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) continue;
            if (err == SSL_ERROR_ZERO_RETURN) return 0;
            throw Error("reading failure TLS");
        }
    }

    void writeAll(const void* buffer, std::size_t len) override {
        const auto* p = static_cast<const char*>(buffer);
        std::size_t done = 0;
        while (done < len) {
            const int n = SSL_write(ssl_, p + done, static_cast<int>(len - done));
            if (n > 0) {
                done += static_cast<std::size_t>(n);
                continue;
            }
            const int err = SSL_get_error(ssl_, n);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) continue;
            throw Error("write failure TLS");
        }
    }

    void close() override {
        if (ssl_ != nullptr) {
            SSL_shutdown(ssl_);
            SSL_free(ssl_);
            ssl_ = nullptr;
        }
        if (ctx_ != nullptr) {
            SSL_CTX_free(ctx_);
            ctx_ = nullptr;
        }
        if (raw_) {
            raw_->close();
            raw_.reset();
        }
    }

private:
    std::unique_ptr<TcpStream> raw_;
    SSL_CTX* ctx_ = nullptr;
    SSL* ssl_ = nullptr;
};

X509* peerCertificate(SSL* ssl) {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    return SSL_get1_peer_certificate(ssl);
#else
    return SSL_get_peer_certificate(ssl);
#endif
}

std::string peerFingerprint(SSL* ssl, const char* role) {
    X509* cert = peerCertificate(ssl);
    if (cert == nullptr) throw Error(std::string("el ") + role + " did not submit a certificate");
    unsigned char* der = nullptr;
    const int derLen = i2d_X509(cert, &der);
    X509_free(cert);
    if (derLen <= 0 || der == nullptr) throw Error("could not read pair certificate");
    const std::string hex = fingerprintOfDer(der, derLen);
    OPENSSL_free(der);
    return hex;
}

// Prototyping certificates are self-signed: callback accepts chain
// and the actual authorization is the exact comparison of the fixed footprint.
int allowPinnedPeerCertificate(int, X509_STORE_CTX*) { return 1; }

// Releases context and session unless the TLS flow adopts them.
class TlsHandles {
public:
    TlsHandles(SSL_CTX* ctx, SSL* ssl) : ctx_(ctx), ssl_(ssl) {}
    ~TlsHandles() {
        if (ssl_ != nullptr) SSL_free(ssl_);
        if (ctx_ != nullptr) SSL_CTX_free(ctx_);
    }
    TlsHandles(const TlsHandles&) = delete;
    TlsHandles& operator=(const TlsHandles&) = delete;
    void release() {
        ssl_ = nullptr;
        ctx_ = nullptr;
    }

private:
    SSL_CTX* ctx_ = nullptr;
    SSL* ssl_ = nullptr;
};

}  // namespace

std::string certificateFingerprint(const std::string& pemPath) {
    // In Windows the DLL OpenSSL cannot consume a FILE* of another CRT
    // (OPENSSL_Applink); PEM is read and delivered to OpenSSL as memory.
    std::ifstream in(fs::u8path(pemPath), std::ios::binary);
    if (!in) throw Error("Could not open the above certificate");
    const std::string pem((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
    if (pem.empty() || pem.size() > 1024u * 1024u) {
        throw Error("certificate file does not have a valid size");
    }
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    if (bio == nullptr) throw Error("Could not read the above certificate");
    X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (cert == nullptr) throw Error("the certificate indicated is not a valid PEM X.509");
    unsigned char* der = nullptr;
    const int len = i2d_X509(cert, &der);
    X509_free(cert);
    if (len <= 0 || der == nullptr) throw Error("could not calculate the print of the certificate");
    const std::string hex = fingerprintOfDer(der, len);
    OPENSSL_free(der);
    return hex;
}

std::unique_ptr<Stream> wrapTlsServer(std::unique_ptr<TcpStream> raw, const std::string& certFile,
                                      const std::string& keyFile,
                                      const std::string& expectedClientPinHex) {
    if (expectedClientPinHex.size() != 64) {
        throw Error("the server requires the fingerprint SHA-256 of the client certificate");
    }
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (ctx == nullptr) throw Error("Could not create server context TLS");
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                       allowPinnedPeerCertificate);
    if (SSL_CTX_use_certificate_chain_file(ctx, certFile.c_str()) != 1) {
        SSL_CTX_free(ctx);
        throw Error("Could not load server certificate");
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, keyFile.c_str(), SSL_FILETYPE_PEM) != 1) {
        SSL_CTX_free(ctx);
        throw Error("could not load private server key");
    }
    if (SSL_CTX_check_private_key(ctx) != 1) {
        SSL_CTX_free(ctx);
        throw Error("the private key does not correspond to the certificate");
    }
    SSL* ssl = SSL_new(ctx);
    if (ssl == nullptr) {
        SSL_CTX_free(ctx);
        throw Error("Could not create server session TLS");
    }
    TlsHandles handles(ctx, ssl);
    if (SSL_set_fd(ssl, static_cast<int>(raw->nativeHandle())) != 1) {
        throw Error("could not bind TLS to the server socket");
    }
    if (SSL_accept(ssl) <= 0) throw Error("failed handshake TLS server");
    const std::string actualClient = peerFingerprint(ssl, "cliente");
    if (!fixedTimeHexEquals(actualClient, expectedClientPinHex)) {
        throw Error("Unauthorized customer certificate");
    }
    std::unique_ptr<Stream> stream(new TlsStream(std::move(raw), ctx, ssl));
    handles.release();
    return stream;
}

std::unique_ptr<Stream> wrapTlsClient(std::unique_ptr<TcpStream> raw,
                                      const std::string& expectedSha256Hex,
                                      const std::string& clientCertFile,
                                      const std::string& clientKeyFile) {
    if (expectedSha256Hex.size() != 64) {
        throw Error("the server footprint must have 64 hexadecimal characters");
    }
    if (clientCertFile.empty() || clientKeyFile.empty()) {
        throw Error("the server requires customer certificate: indicate --cert and --key");
    }
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == nullptr) throw Error("could not create customer context TLS");
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    // The server certificate is self-signed in this prototype: trust
    // is established exclusively by comparing its footprint with the given off-band.
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    if (SSL_CTX_use_certificate_chain_file(ctx, clientCertFile.c_str()) != 1) {
        SSL_CTX_free(ctx);
        throw Error("could not load customer certificate");
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, clientKeyFile.c_str(), SSL_FILETYPE_PEM) != 1) {
        SSL_CTX_free(ctx);
        throw Error("could not load private client key");
    }
    if (SSL_CTX_check_private_key(ctx) != 1) {
        SSL_CTX_free(ctx);
        throw Error("the customer key does not match your certificate");
    }
    SSL* ssl = SSL_new(ctx);
    if (ssl == nullptr) {
        SSL_CTX_free(ctx);
        throw Error("Could not create customer session TLS");
    }
    TlsHandles handles(ctx, ssl);
    if (SSL_set_fd(ssl, static_cast<int>(raw->nativeHandle())) != 1) {
        throw Error("could not bind TLS to the client socket");
    }
    if (SSL_connect(ssl) <= 0) throw Error("failure in the customer handshake TLS");
    const std::string actual = peerFingerprint(ssl, "servidor");
    if (!fixedTimeHexEquals(actual, expectedSha256Hex)) {
        throw Error("the fingerprint of the server certificate does not match the expected");
    }
    std::unique_ptr<Stream> stream(new TlsStream(std::move(raw), ctx, ssl));
    handles.release();
    return stream;
}

#else

std::string certificateFingerprint(const std::string&) {
    throw Error("compiled without OpenSSL: certificate footprint cannot be calculated");
}

std::unique_ptr<Stream> wrapTlsServer(std::unique_ptr<TcpStream>, const std::string&,
                                      const std::string&, const std::string&) {
    throw Error("compiled without OpenSSL: TLS not available");
}

std::unique_ptr<Stream> wrapTlsClient(std::unique_ptr<TcpStream>, const std::string&,
                                      const std::string&, const std::string&) {
    throw Error("compiled without OpenSSL: TLS not available");
}

#endif

}  // namespace o2f
