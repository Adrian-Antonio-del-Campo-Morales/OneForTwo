// Responsabilidad: implementar el canal TLS con OpenSSL. El certificado del
// par se fija por huella SHA-256 (sin CA ni nombres de host), y ambas partes
// presentan certificado. No se implementa criptografía propia.

#include "net/tls_channel.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

namespace oit {
namespace {

#ifdef _WIN32
using socket_t = SOCKET;
constexpr socket_t kInvalidSocket = INVALID_SOCKET;
using socklen_type = int;
#else
using socket_t = int;
constexpr socket_t kInvalidSocket = -1;
using socklen_type = socklen_t;
#endif

struct PinnedPeer {
  unsigned char sha256[32]{};
};

std::once_flag g_init_once;

void platform_init() {
  std::call_once(g_init_once, [] {
#ifdef _WIN32
    WSADATA data{};
    WSAStartup(MAKEWORD(2, 2), &data);
#else
    std::signal(SIGPIPE, SIG_IGN);
#endif
    OPENSSL_init_ssl(0, nullptr);
  });
}

std::string socket_error_text(const char* context) {
#ifdef _WIN32
  return std::string(context) + " (Winsock error " +
         std::to_string(WSAGetLastError()) + ")";
#else
  return std::string(context) + " (" + std::strerror(errno) + ")";
#endif
}

void close_socket(socket_t socket) {
#ifdef _WIN32
  if (socket != kInvalidSocket) {
    closesocket(socket);
  }
#else
  if (socket != kInvalidSocket) {
    ::close(socket);
  }
#endif
}

void shutdown_socket(socket_t socket) {
#ifdef _WIN32
  if (socket != kInvalidSocket) {
    ::shutdown(socket, SD_BOTH);
  }
#else
  if (socket != kInvalidSocket) {
    ::shutdown(socket, SHUT_RDWR);
  }
#endif
}

void set_nodelay(socket_t socket) {
  int enabled = 1;
  setsockopt(socket, IPPROTO_TCP, TCP_NODELAY,
             reinterpret_cast<const char*>(&enabled), sizeof(enabled));
}

std::string openssl_error_text(const char* context) {
  const unsigned long code = ERR_get_error();
  if (code == 0) {
    return context;
  }
  char buffer[256];
  ERR_error_string_n(code, buffer, sizeof(buffer));
  return std::string(context) + ": " + buffer;
}

void clear_openssl_errors() { ERR_clear_error(); }

std::string format_fingerprint(const unsigned char* digest,
                               unsigned int length) {
  std::ostringstream out;
  out << std::hex << std::uppercase;
  for (unsigned int i = 0; i < length; ++i) {
    if (i != 0) {
      out << ':';
    }
    out.width(2);
    out.fill('0');
    out << static_cast<unsigned int>(digest[i]);
  }
  return out.str();
}

bool certificate_digest(X509* certificate, unsigned char* digest,
                        unsigned int& length) {
  return X509_digest(certificate, EVP_sha256(), digest, &length) == 1 &&
         length == 32;
}

int pin_index() {
  static const int index =
      SSL_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr);
  return index;
}

// La verificación no encadena a una CA: exige que el certificado del par sea
// exactamente el fijado por el usuario. preverify_ok se ignora a propósito.
int verify_pinned_peer(int /*preverify_ok*/, X509_STORE_CTX* store) {
  SSL* ssl = static_cast<SSL*>(X509_STORE_CTX_get_ex_data(
      store, SSL_get_ex_data_X509_STORE_CTX_idx()));
  if (ssl == nullptr) {
    return 0;
  }
  auto* pin = static_cast<PinnedPeer*>(SSL_get_ex_data(ssl, pin_index()));
  X509* certificate = X509_STORE_CTX_get_current_cert(store);
  if (pin == nullptr || certificate == nullptr ||
      X509_STORE_CTX_get_error_depth(store) != 0) {
    return 0;
  }
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int length = 0;
  if (!certificate_digest(certificate, digest, length)) {
    return 0;
  }
  return std::memcmp(digest, pin->sha256, 32) == 0 ? 1 : 0;
}

// No pasar FILE* de la CRT de la aplicación a la DLL de OpenSSL en Windows.
X509* read_certificate(const std::string& path, const char* label,
                       std::string& error) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) {
    error = std::string("cannot open ") + label + ": " + path;
    return nullptr;
  }
  const std::streamoff size = file.tellg();
  if (size <= 0 || size > 1024 * 1024) {
    error = std::string("invalid ") + label + " size: " + path;
    return nullptr;
  }
  std::string pem(static_cast<std::size_t>(size), '\0');
  file.seekg(0);
  if (!file.read(&pem[0], static_cast<std::streamsize>(size))) {
    error = std::string("cannot read ") + label + ": " + path;
    return nullptr;
  }
  BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
  if (bio == nullptr) {
    error = openssl_error_text("cannot allocate certificate BIO");
    return nullptr;
  }
  X509* certificate = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  if (certificate == nullptr) {
    error = openssl_error_text((std::string("cannot parse ") + label).c_str()) +
            " (" + path + ")";
  }
  return certificate;
}

bool load_peer_pin(const std::string& path, PinnedPeer& pin,
                   std::string& error) {
  X509* certificate = read_certificate(path, "peer certificate", error);
  if (certificate == nullptr) return false;
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int length = 0;
  const bool ok = certificate_digest(certificate, digest, length);
  if (ok) {
    std::memcpy(pin.sha256, digest, 32);
  }
  X509_free(certificate);
  if (!ok) {
    error = "cannot compute peer certificate fingerprint";
  }
  return ok;
}

bool read_certificate_fingerprint(const std::string& path, std::string& out,
                                  std::string& error) {
  X509* certificate = read_certificate(path, "certificate", error);
  if (certificate == nullptr) return false;
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int length = 0;
  const bool ok = certificate_digest(certificate, digest, length);
  X509_free(certificate);
  if (!ok) {
    error = "cannot compute certificate fingerprint";
    return false;
  }
  out = format_fingerprint(digest, length);
  return true;
}

std::string peer_certificate_fingerprint(SSL* ssl) {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  X509* certificate = SSL_get1_peer_certificate(ssl);
#else
  X509* certificate = SSL_get_peer_certificate(ssl);
#endif
  if (certificate == nullptr) {
    return {};
  }
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int length = 0;
  std::string result;
  if (certificate_digest(certificate, digest, length)) {
    result = format_fingerprint(digest, length);
  }
  X509_free(certificate);
  return result;
}

SSL_CTX* make_context(bool server, const TlsFiles& files, std::string& error) {
  platform_init();
  clear_openssl_errors();
  SSL_CTX* context =
      SSL_CTX_new(server ? TLS_server_method() : TLS_client_method());
  if (context == nullptr) {
    error = openssl_error_text("SSL_CTX_new failed");
    return nullptr;
  }
  if (SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION) != 1) {
    error = openssl_error_text("cannot require TLS 1.2");
    SSL_CTX_free(context);
    return nullptr;
  }
  SSL_CTX_set_options(context, SSL_OP_NO_COMPRESSION);
  if (SSL_CTX_use_certificate_chain_file(context, files.cert_file.c_str()) !=
      1) {
    error = openssl_error_text("cannot load local certificate");
    SSL_CTX_free(context);
    return nullptr;
  }
  if (SSL_CTX_use_PrivateKey_file(context, files.key_file.c_str(),
                                 SSL_FILETYPE_PEM) != 1) {
    error = openssl_error_text("cannot load local private key");
    SSL_CTX_free(context);
    return nullptr;
  }
  if (SSL_CTX_check_private_key(context) != 1) {
    error = openssl_error_text("local certificate and key do not match");
    SSL_CTX_free(context);
    return nullptr;
  }
  int verify_mode = SSL_VERIFY_PEER;
  if (server) {
    verify_mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
  }
  SSL_CTX_set_verify(context, verify_mode, verify_pinned_peer);
  SSL_CTX_set_verify_depth(context, 1);
  return context;
}

socket_t connect_socket(const std::string& host, std::uint16_t port,
                        std::string& error) {
  platform_init();
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* result = nullptr;
  const std::string service = std::to_string(port);
  const int lookup =
      getaddrinfo(host.c_str(), service.c_str(), &hints, &result);
  if (lookup != 0) {
    error = "getaddrinfo failed for " + host;
    return kInvalidSocket;
  }
  socket_t connected = kInvalidSocket;
  for (addrinfo* entry = result; entry != nullptr; entry = entry->ai_next) {
    socket_t socket =
        ::socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
    if (socket == kInvalidSocket) {
      continue;
    }
    if (::connect(socket, entry->ai_addr,
                  static_cast<socklen_type>(entry->ai_addrlen)) == 0) {
      connected = socket;
      break;
    }
    close_socket(socket);
  }
  freeaddrinfo(result);
  if (connected == kInvalidSocket) {
    error = socket_error_text("TCP connect failed");
    return kInvalidSocket;
  }
  set_nodelay(connected);
  return connected;
}

socket_t listen_socket(const std::string& bind_address, std::uint16_t port,
                       std::uint16_t& actual_port, std::string& error) {
  platform_init();
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  const char* node = bind_address.empty() ? nullptr : bind_address.c_str();
  const std::string service = std::to_string(port);
  addrinfo* result = nullptr;
  if (getaddrinfo(node, service.c_str(), &hints, &result) != 0) {
    error = "getaddrinfo failed for bind address";
    return kInvalidSocket;
  }
  socket_t listener = kInvalidSocket;
  for (addrinfo* entry = result; entry != nullptr; entry = entry->ai_next) {
    socket_t socket =
        ::socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
    if (socket == kInvalidSocket) {
      continue;
    }
    int reuse = 1;
    setsockopt(socket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    if (::bind(socket, entry->ai_addr,
               static_cast<socklen_type>(entry->ai_addrlen)) == 0 &&
        ::listen(socket, 1) == 0) {
      listener = socket;
      break;
    }
    close_socket(socket);
  }
  freeaddrinfo(result);
  if (listener == kInvalidSocket) {
    error = socket_error_text("TCP listen failed");
    return kInvalidSocket;
  }

  sockaddr_storage address{};
  socklen_type length = sizeof(address);
  if (getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) ==
      0) {
    if (address.ss_family == AF_INET) {
      actual_port = ntohs(
          reinterpret_cast<sockaddr_in*>(&address)->sin_port);
    } else if (address.ss_family == AF_INET6) {
      actual_port = ntohs(
          reinterpret_cast<sockaddr_in6*>(&address)->sin6_port);
    }
  }
  return listener;
}

}  // namespace

struct TlsChannel::Impl {
  SSL_CTX* context = nullptr;
  SSL* ssl = nullptr;
  socket_t listener = kInvalidSocket;
  socket_t socket = kInvalidSocket;
  PinnedPeer pin{};
  std::uint16_t bound_port = 0;
  std::string peer_fingerprint;
  std::mutex write_mutex;

  ~Impl() { reset(); }

  void reset_peer() {
    if (ssl != nullptr) {
      SSL_free(ssl);
      ssl = nullptr;
    }
    if (socket != kInvalidSocket) {
      close_socket(socket);
      socket = kInvalidSocket;
    }
    peer_fingerprint.clear();
  }

  void reset() {
    reset_peer();
    if (context != nullptr) {
      SSL_CTX_free(context);
      context = nullptr;
    }
    if (listener != kInvalidSocket) {
      close_socket(listener);
      listener = kInvalidSocket;
    }
    bound_port = 0;
  }
};

TlsChannel::TlsChannel() : impl_(std::make_unique<Impl>()) {}
TlsChannel::~TlsChannel() = default;

bool TlsChannel::listen_on(const std::string& bind_address, std::uint16_t port,
                           const TlsFiles& files, std::string& error) {
  impl_->reset();
  if (!load_peer_pin(files.peer_cert_file, impl_->pin, error)) {
    return false;
  }
  impl_->context = make_context(true, files, error);
  if (impl_->context == nullptr) {
    return false;
  }
  std::uint16_t actual_port = 0;
  impl_->listener =
      listen_socket(bind_address, port, actual_port, error);
  if (impl_->listener == kInvalidSocket) {
    impl_->reset();
    return false;
  }
  impl_->bound_port = actual_port;
  return true;
}

bool TlsChannel::accept_next(std::string& error) {
  if (impl_->listener == kInvalidSocket || impl_->context == nullptr) {
    error = "listener is not initialized";
    return false;
  }
  if (impl_->ssl != nullptr) {
    error = "a peer is already connected";
    return false;
  }

  sockaddr_storage address{};
  socklen_type length = sizeof(address);
  socket_t socket = ::accept(impl_->listener,
                             reinterpret_cast<sockaddr*>(&address), &length);
  if (socket == kInvalidSocket) {
    error = socket_error_text("accept failed");
    return false;
  }
  set_nodelay(socket);

  SSL* ssl = SSL_new(impl_->context);
  if (ssl == nullptr) {
    close_socket(socket);
    error = openssl_error_text("SSL_new failed");
    return false;
  }
  SSL_set_ex_data(ssl, pin_index(), &impl_->pin);
  if (SSL_set_fd(ssl, static_cast<int>(socket)) != 1) {
    SSL_free(ssl);
    close_socket(socket);
    error = openssl_error_text("SSL_set_fd failed");
    return false;
  }
  clear_openssl_errors();
  if (SSL_accept(ssl) != 1) {
    error = openssl_error_text("TLS handshake failed");
    SSL_free(ssl);
    close_socket(socket);
    return false;
  }

  impl_->ssl = ssl;
  impl_->socket = socket;
  impl_->peer_fingerprint = peer_certificate_fingerprint(ssl);
  if (impl_->peer_fingerprint.empty()) {
    error = "peer did not present a certificate";
    impl_->reset_peer();
    return false;
  }
  return true;
}

bool TlsChannel::connect_to(const std::string& host, std::uint16_t port,
                            const TlsFiles& files, std::string& error) {
  impl_->reset();
  if (!load_peer_pin(files.peer_cert_file, impl_->pin, error)) {
    return false;
  }
  impl_->context = make_context(false, files, error);
  if (impl_->context == nullptr) {
    return false;
  }
  socket_t socket = connect_socket(host, port, error);
  if (socket == kInvalidSocket) {
    impl_->reset();
    return false;
  }

  SSL* ssl = SSL_new(impl_->context);
  if (ssl == nullptr) {
    close_socket(socket);
    impl_->reset();
    error = openssl_error_text("SSL_new failed");
    return false;
  }
  SSL_set_ex_data(ssl, pin_index(), &impl_->pin);
  if (SSL_set_fd(ssl, static_cast<int>(socket)) != 1) {
    SSL_free(ssl);
    close_socket(socket);
    impl_->reset();
    error = openssl_error_text("SSL_set_fd failed");
    return false;
  }
  clear_openssl_errors();
  if (SSL_connect(ssl) != 1) {
    error = openssl_error_text("TLS handshake failed");
    SSL_free(ssl);
    close_socket(socket);
    impl_->reset();
    return false;
  }

  impl_->ssl = ssl;
  impl_->socket = socket;
  impl_->peer_fingerprint = peer_certificate_fingerprint(ssl);
  if (impl_->peer_fingerprint.empty()) {
    error = "peer did not present a certificate";
    impl_->reset_peer();
    return false;
  }
  return true;
}

bool TlsChannel::write_all(const std::uint8_t* data, std::size_t size,
                           std::string& error) {
  if (impl_->ssl == nullptr) {
    error = "no TLS connection";
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->write_mutex);
  std::size_t offset = 0;
  while (offset < size) {
    clear_openssl_errors();
    const int written = SSL_write(
        impl_->ssl, data + offset, static_cast<int>(size - offset));
    if (written > 0) {
      offset += static_cast<std::size_t>(written);
      continue;
    }
    const int code = SSL_get_error(impl_->ssl, written);
    if (code == SSL_ERROR_WANT_READ || code == SSL_ERROR_WANT_WRITE) {
      continue;
    }
    error = openssl_error_text("TLS write failed");
    return false;
  }
  return true;
}

bool TlsChannel::read_all(std::uint8_t* data, std::size_t size,
                          std::string& error) {
  if (impl_->ssl == nullptr) {
    error = "no TLS connection";
    return false;
  }
  std::size_t offset = 0;
  while (offset < size) {
    clear_openssl_errors();
    const int received = SSL_read(
        impl_->ssl, data + offset, static_cast<int>(size - offset));
    if (received > 0) {
      offset += static_cast<std::size_t>(received);
      continue;
    }
    const int code = SSL_get_error(impl_->ssl, received);
    if (code == SSL_ERROR_WANT_READ || code == SSL_ERROR_WANT_WRITE) {
      continue;
    }
    if (code == SSL_ERROR_ZERO_RETURN) {
      error = "peer closed the TLS connection";
    } else {
      error = openssl_error_text("TLS read failed");
    }
    return false;
  }
  return true;
}

void TlsChannel::interrupt() {
  if (impl_->socket != kInvalidSocket) {
    shutdown_socket(impl_->socket);
  }
}

void TlsChannel::shutdown_peer() {
  if (impl_->ssl != nullptr) {
    // Un único intento: no se espera al cierre del par para no bloquear.
    (void)SSL_shutdown(impl_->ssl);
  }
  impl_->reset_peer();
}

std::uint16_t TlsChannel::local_port() const { return impl_->bound_port; }

std::string TlsChannel::peer_fingerprint() const {
  return impl_->peer_fingerprint;
}

bool describe_local_certificate(const TlsFiles& files, LocalTlsInfo& out,
                                std::string& error) {
  out = LocalTlsInfo{};
  out.backend = std::string("OpenSSL ") + OpenSSL_version(OPENSSL_VERSION);
  return read_certificate_fingerprint(files.cert_file, out.cert_fingerprint,
                                      error);
}

}  // namespace oit
