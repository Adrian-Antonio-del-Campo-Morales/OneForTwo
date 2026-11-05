// Responsabilidad: transporte fiable entre las dos instancias. Usa TLS con
// certificado propio y fija la huella SHA-256 del par; no hay confianza
// implícita por nombre de equipo ni por dirección de red.
#include "net/link.hpp"

#include "core/protocol.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifndef ONEINTWO_WITH_TLS
#define ONEINTWO_WITH_TLS 0
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#if ONEINTWO_WITH_TLS
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#endif

namespace oit {
namespace {

#if defined(_WIN32)
using Socket = SOCKET;
using SockLen = int;
constexpr Socket kInvalidSocket = INVALID_SOCKET;
#else
using Socket = int;
using SockLen = socklen_t;
constexpr Socket kInvalidSocket = -1;
#endif

void closeSocket(Socket socket) {
  if (socket == kInvalidSocket) return;
#if defined(_WIN32)
  closesocket(socket);
#else
  ::close(socket);
#endif
}

int lastSocketError() {
#if defined(_WIN32)
  return WSAGetLastError();
#else
  return errno;
#endif
}

bool wouldBlock(int error) {
#if defined(_WIN32)
  return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS;
#else
  return error == EWOULDBLOCK || error == EINPROGRESS;
#endif
}

std::string socketErrorText() {
#if defined(_WIN32)
  return "error de socket " + std::to_string(lastSocketError());
#else
  return std::string(std::strerror(lastSocketError()));
#endif
}

bool setNonBlocking(Socket socket, bool enabled) {
#if defined(_WIN32)
  u_long mode = enabled ? 1 : 0;
  return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
  const int flags = fcntl(socket, F_GETFL, 0);
  if (flags < 0) return false;
  return fcntl(socket, F_SETFL, enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)) == 0;
#endif
}

bool setSocketTimeouts(Socket socket, int timeoutMs) {
#if defined(_WIN32)
  DWORD timeout = static_cast<DWORD>(timeoutMs);
  return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
                    sizeof(timeout)) == 0 &&
         setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout),
                    sizeof(timeout)) == 0;
#else
  timeval timeout{};
  timeout.tv_sec = timeoutMs / 1000;
  timeout.tv_usec = (timeoutMs % 1000) * 1000;
  return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 &&
         setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0;
#endif
}

void setNoDelay(Socket socket) {
  int yes = 1;
  setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&yes), sizeof(yes));
}

Socket connectTo(const std::string& host, uint16_t port, std::string& error) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  const std::string service = std::to_string(port);
  addrinfo* results = nullptr;
  const int lookup = getaddrinfo(host.c_str(), service.c_str(), &hints, &results);
  if (lookup != 0 || results == nullptr) {
    error = "no se pudo resolver la dirección del destino";
    return kInvalidSocket;
  }

  Socket connected = kInvalidSocket;
  for (addrinfo* entry = results; entry != nullptr; entry = entry->ai_next) {
    Socket socket = ::socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
    if (socket == kInvalidSocket) continue;

    setNonBlocking(socket, true);
    const int result = ::connect(socket, entry->ai_addr, static_cast<SockLen>(entry->ai_addrlen));
    bool ok = result == 0;
    if (!ok && wouldBlock(lastSocketError())) {
      fd_set writable;
      FD_ZERO(&writable);
      FD_SET(socket, &writable);
      timeval timeout{5, 0};
      const int ready = select(static_cast<int>(socket) + 1, nullptr, &writable, nullptr, &timeout);
      if (ready > 0) {
        int socketError = 0;
        SockLen length = sizeof(socketError);
        if (getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socketError),
                       &length) == 0) {
          ok = socketError == 0;
        }
      }
    }
    setNonBlocking(socket, false);

    if (ok) {
      setNoDelay(socket);
      setSocketTimeouts(socket, 10000);
      connected = socket;
      break;
    }
    closeSocket(socket);
  }

  freeaddrinfo(results);
  if (connected == kInvalidSocket) {
    error = "no se pudo conectar con el destino (" + socketErrorText() + ")";
  }
  return connected;
}

Socket createListener(uint16_t port, std::string& error) {
  Socket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket == kInvalidSocket) {
    error = "no se pudo crear el socket de escucha";
    return kInvalidSocket;
  }

  int yes = 1;
  setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);
  if (bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    error = "no se pudo abrir el puerto " + std::to_string(port) + " (" + socketErrorText() + ")";
    closeSocket(socket);
    return kInvalidSocket;
  }
  if (listen(socket, 1) != 0) {
    error = "no se pudo escuchar en el puerto indicado";
    closeSocket(socket);
    return kInvalidSocket;
  }
  return socket;
}

#if ONEINTWO_WITH_TLS
struct PinHolder {
  std::string pin;
};

std::string opensslError() {
  const unsigned long code = ERR_get_error();
  if (code == 0) return "error TLS sin detalle";
  char buffer[256];
  ERR_error_string_n(code, buffer, sizeof(buffer));
  return std::string(buffer);
}

std::string digestHex(const unsigned char* data, unsigned int length) {
  static const char* digits = "0123456789abcdef";
  std::string out;
  out.reserve(static_cast<size_t>(length) * 2);
  for (unsigned int i = 0; i < length; ++i) {
    out.push_back(digits[(data[i] >> 4) & 0x0F]);
    out.push_back(digits[data[i] & 0x0F]);
  }
  return out;
}

bool certificateMatchesPin(X509* certificate, const std::string& pin) {
  if (certificate == nullptr) return false;
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int length = 0;
  if (X509_digest(certificate, EVP_sha256(), digest, &length) != 1) return false;
  return digestHex(digest, length) == pin;
}

int pinContextIndex() {
  static const int index = SSL_CTX_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr);
  return index;
}

int verifyPinnedCertificate(int, X509_STORE_CTX* store) {
  SSL* ssl = static_cast<SSL*>(
      X509_STORE_CTX_get_ex_data(store, SSL_get_ex_data_X509_STORE_CTX_idx()));
  if (ssl == nullptr) return 0;
  SSL_CTX* context = SSL_get_SSL_CTX(ssl);
  PinHolder* holder = static_cast<PinHolder*>(SSL_CTX_get_ex_data(context, pinContextIndex()));
  X509* certificate = X509_STORE_CTX_get_current_cert(store);
  if (holder == nullptr || certificate == nullptr) return 0;
  if (X509_STORE_CTX_get_error_depth(store) != 0) return 0;
  return certificateMatchesPin(certificate, holder->pin) ? 1 : 0;
}

bool normalizeFingerprint(const std::string& input, std::string& normalized, std::string& error) {
  normalized.clear();
  for (char character : input) {
    if (character == ':' || character == ' ' || character == '-') continue;
    if (character >= 'A' && character <= 'F') character = static_cast<char>(character - 'A' + 'a');
    const bool hex = (character >= '0' && character <= '9') ||
                     (character >= 'a' && character <= 'f');
    if (!hex) {
      error = "la huella debe ser hexadecimal SHA-256 (64 dígitos)";
      return false;
    }
    normalized.push_back(character);
  }
  if (normalized.size() != 64) {
    error = "la huella debe contener 64 dígitos hexadecimales";
    return false;
  }
  return true;
}

SSL_CTX* createTlsContext(bool server, const std::string& certFile, const std::string& keyFile,
                          const std::string& pin, PinHolder*& holder, std::string& error) {
  holder = nullptr;
  SSL_CTX* context = SSL_CTX_new(server ? TLS_server_method() : TLS_client_method());
  if (context == nullptr) {
    error = "no se pudo crear el contexto TLS: " + opensslError();
    return nullptr;
  }
  SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION);
  SSL_CTX_set_verify_depth(context, 1);
  SSL_CTX_set_verify(context, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                     verifyPinnedCertificate);

  const int index = pinContextIndex();
  if (index < 0) {
    error = "no se pudo registrar la huella en OpenSSL";
    SSL_CTX_free(context);
    return nullptr;
  }
  holder = new PinHolder{pin};
  if (SSL_CTX_set_ex_data(context, index, holder) != 1) {
    error = "no se pudo fijar la huella del par en el contexto TLS";
    delete holder;
    holder = nullptr;
    SSL_CTX_free(context);
    return nullptr;
  }

  if (SSL_CTX_use_certificate_chain_file(context, certFile.c_str()) != 1) {
    error = "no se pudo cargar el certificado '" + certFile + "': " + opensslError();
    SSL_CTX_free(context);
    delete holder;
    holder = nullptr;
    return nullptr;
  }
  if (SSL_CTX_use_PrivateKey_file(context, keyFile.c_str(), SSL_FILETYPE_PEM) != 1) {
    error = "no se pudo cargar la clave privada '" + keyFile + "': " + opensslError();
    SSL_CTX_free(context);
    delete holder;
    holder = nullptr;
    return nullptr;
  }
  if (SSL_CTX_check_private_key(context) != 1) {
    error = "el certificado y la clave privada no coinciden";
    SSL_CTX_free(context);
    delete holder;
    holder = nullptr;
    return nullptr;
  }
  return context;
}

class TlsLink : public Link {
 public:
  TlsLink(Socket socket, SSL_CTX* context, SSL* ssl, PinHolder* holder, std::string pin)
      : socket_(socket), context_(context), ssl_(ssl), holder_(holder), pin_(std::move(pin)) {}

  ~TlsLink() override {
    close();
    if (ssl_ != nullptr) SSL_free(ssl_);
    if (context_ != nullptr) SSL_CTX_free(context_);
    delete holder_;
  }

  bool handshake(bool server, std::string& error) {
    if (SSL_set_fd(ssl_, static_cast<int>(socket_)) != 1) {
      error = "no se pudo asociar el socket a TLS";
      return false;
    }
    const int result = server ? SSL_accept(ssl_) : SSL_connect(ssl_);
    if (result != 1) {
      error = "handshake TLS fallido: " + opensslError();
      return false;
    }
    X509* peer = SSL_get_peer_certificate(ssl_);
    if (peer == nullptr) {
      error = "el par no presentó certificado";
      return false;
    }
    const bool matches = certificateMatchesPin(peer, pin_);
    X509_free(peer);
    if (!matches) {
      error = "la huella SHA-256 del certificado del par no coincide";
      return false;
    }
    handshaked_ = true;
    return true;
  }

  bool send(const Message& message, std::string& error) override {
    if (closed_) {
      error = "canal cerrado";
      return false;
    }
    const Frame frame = encodeMessage(message);
    size_t offset = 0;
    while (offset < frame.size()) {
      const int written = SSL_write(ssl_, frame.data() + offset,
                                    static_cast<int>(frame.size() - offset));
      if (written <= 0) {
        const int code = SSL_get_error(ssl_, written);
        if (code == SSL_ERROR_WANT_READ || code == SSL_ERROR_WANT_WRITE) {
          if (!waitWritable(2000)) {
            error = "tiempo de espera agotado escribiendo TLS";
            return false;
          }
          continue;
        }
        error = "error de escritura TLS: " + opensslError();
        return false;
      }
      offset += static_cast<size_t>(written);
    }
    return true;
  }

  bool receive(std::vector<Message>& out, int timeoutMs, std::string& error) override {
    if (closed_) {
      error = "canal cerrado";
      return false;
    }
    if (!parseBuffered(out, error)) return false;
    if (!out.empty()) return true;

    if (SSL_pending(ssl_) == 0) {
      fd_set readable;
      FD_ZERO(&readable);
      FD_SET(socket_, &readable);
      timeval timeout{};
      timeout.tv_sec = timeoutMs / 1000;
      timeout.tv_usec = (timeoutMs % 1000) * 1000;
      const int ready =
          select(static_cast<int>(socket_) + 1, &readable, nullptr, nullptr, &timeout);
      if (ready == 0) return true;
      if (ready < 0) {
        error = "select falló esperando datos TLS";
        return false;
      }
    }

    for (;;) {
      char buffer[4096];
      const int received = SSL_read(ssl_, buffer, sizeof(buffer));
      if (received > 0) {
        buffer_.insert(buffer_.end(), buffer, buffer + received);
        continue;
      }
      const int code = SSL_get_error(ssl_, received);
      if (code == SSL_ERROR_WANT_READ || code == SSL_ERROR_WANT_WRITE) break;
      if (code == SSL_ERROR_ZERO_RETURN) {
        error = "el par cerró la conexión";
        return false;
      }
      error = "error de lectura TLS: " + opensslError();
      return false;
    }
    return parseBuffered(out, error);
  }

  void close() override {
    if (closed_) return;
    closed_ = true;
    if (ssl_ != nullptr && handshaked_) SSL_shutdown(ssl_);
    if (socket_ != kInvalidSocket) {
      closeSocket(socket_);
      socket_ = kInvalidSocket;
    }
  }

 private:
  bool parseBuffered(std::vector<Message>& out, std::string& error) {
    while (buffer_.size() >= kFrameSize) {
      Message message;
      if (!decodeMessage(buffer_.data(), kFrameSize, message, error)) return false;
      out.push_back(message);
      buffer_.erase(buffer_.begin(), buffer_.begin() + kFrameSize);
    }
    return true;
  }

  bool waitWritable(int timeoutMs) {
    fd_set writable;
    FD_ZERO(&writable);
    FD_SET(socket_, &writable);
    timeval timeout{};
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    return select(static_cast<int>(socket_) + 1, nullptr, &writable, nullptr, &timeout) > 0;
  }

  Socket socket_ = kInvalidSocket;
  SSL_CTX* context_ = nullptr;
  SSL* ssl_ = nullptr;
  PinHolder* holder_ = nullptr;
  std::string pin_;
  std::vector<uint8_t> buffer_;
  bool handshaked_ = false;
  bool closed_ = false;
};

std::unique_ptr<Link> makeTlsLink(Socket socket, bool server, const std::string& certFile,
                                  const std::string& keyFile, const std::string& pin,
                                  std::string& error) {
  PinHolder* holder = nullptr;
  SSL_CTX* context = createTlsContext(server, certFile, keyFile, pin, holder, error);
  if (context == nullptr) {
    closeSocket(socket);
    return nullptr;
  }
  SSL* ssl = SSL_new(context);
  if (ssl == nullptr) {
    error = "no se pudo crear la sesión TLS";
    SSL_CTX_free(context);
    delete holder;
    closeSocket(socket);
    return nullptr;
  }
  std::unique_ptr<TlsLink> link(new TlsLink(socket, context, ssl, holder, pin));
  if (!link->handshake(server, error)) return nullptr;
  return link;
}
#endif  // ONEINTWO_WITH_TLS

}  // namespace

bool netInit(std::string& error) {
#if defined(_WIN32)
  WSADATA data{};
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
    error = "no se pudo inicializar Winsock";
    return false;
  }
#else
  (void)error;
#endif
  return true;
}

void netShutdown() {
#if defined(_WIN32)
  WSACleanup();
#endif
}

std::unique_ptr<Link> openTlsClient(const std::string& host, uint16_t port,
                                    const std::string& certFile, const std::string& keyFile,
                                    const std::string& peerFingerprint, std::string& error) {
#if !ONEINTWO_WITH_TLS
  (void)host;
  (void)port;
  (void)certFile;
  (void)keyFile;
  (void)peerFingerprint;
  error = "este ejecutable se compiló sin TLS; reconstruya con OpenSSL";
  return nullptr;
#else
  std::string pin;
  if (!normalizeFingerprint(peerFingerprint, pin, error)) return nullptr;
  const Socket socket = connectTo(host, port, error);
  if (socket == kInvalidSocket) return nullptr;
  return makeTlsLink(socket, false, certFile, keyFile, pin, error);
#endif
}

std::unique_ptr<Link> openTlsServer(uint16_t port, const std::string& certFile,
                                    const std::string& keyFile, const std::string& peerFingerprint,
                                    bool (*shouldStop)(), std::string& error) {
#if !ONEINTWO_WITH_TLS
  (void)port;
  (void)certFile;
  (void)keyFile;
  (void)peerFingerprint;
  (void)shouldStop;
  error = "este ejecutable se compiló sin TLS; reconstruya con OpenSSL";
  return nullptr;
#else
  std::string pin;
  if (!normalizeFingerprint(peerFingerprint, pin, error)) return nullptr;

  const Socket listener = createListener(port, error);
  if (listener == kInvalidSocket) return nullptr;

  while (shouldStop == nullptr || !shouldStop()) {
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(listener, &readable);
    timeval timeout{0, 200000};
    const int ready = select(static_cast<int>(listener) + 1, &readable, nullptr, nullptr, &timeout);
    if (ready < 0) {
      error = "select falló esperando conexión";
      closeSocket(listener);
      return nullptr;
    }
    if (ready == 0) continue;

    Socket client = accept(listener, nullptr, nullptr);
    if (client == kInvalidSocket) {
      error = "accept falló";
      closeSocket(listener);
      return nullptr;
    }
    closeSocket(listener);
    setNoDelay(client);
    setSocketTimeouts(client, 10000);
    return makeTlsLink(client, true, certFile, keyFile, pin, error);
  }

  closeSocket(listener);
  error = "cancelado antes de recibir conexión";
  return nullptr;
#endif
}

}  // namespace oit
