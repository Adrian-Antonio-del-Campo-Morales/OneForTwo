#include "net/pairing_session.hpp"

#include <QCryptographicHash>
#include <QByteArray>
#include <QRandomGenerator>
#include <QString>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace oneintwo {
namespace {

constexpr const char* kMagic = "O2PAIR1";

std::string randomHex(std::size_t bytes) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes * 2);
    for (std::size_t index = 0; index < bytes; ++index) {
        const quint32 value = QRandomGenerator::system()->generate() & 0xffu;
        result.push_back(kHex[(value >> 4) & 0x0f]);
        result.push_back(kHex[value & 0x0f]);
    }
    return result;
}

std::string sanitizeName(std::string value) {
    for (char& character : value) {
        if (character == '\t' || character == '\n' || character == '\r' ||
            character == '\x1f') {
            character = ' ';
        }
    }
    while (!value.empty() && value.front() == ' ') {
        value.erase(value.begin());
    }
    while (!value.empty() && value.back() == ' ') {
        value.pop_back();
    }
    return value;
}

bool safeId(const std::string& value) {
    if (value.empty() || value.size() > 128) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '-' || character == '_' ||
               character == '.';
    });
}

std::string normalizeFingerprint(const std::string& value) {
    std::string normalized;
    normalized.reserve(64);
    for (const unsigned char character : value) {
        if (std::isxdigit(character)) {
            normalized.push_back(static_cast<char>(std::tolower(character)));
        } else if (character == ':' || character == '-' || character == ' ') {
            continue;
        } else {
            return {};
        }
    }
    return normalized.size() == 64 ? normalized : std::string();
}

std::vector<std::string> splitFields(const std::string& text) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = text.find('\x1f', start);
        if (end == std::string::npos) {
            fields.push_back(text.substr(start));
            return fields;
        }
        fields.push_back(text.substr(start, end - start));
        start = end + 1;
    }
}

void appendField(QByteArray& buffer, const std::string& value) {
    buffer.append(QByteArray::number(static_cast<qulonglong>(value.size())));
    buffer.append(':');
    buffer.append(value.data(), static_cast<int>(value.size()));
}

std::string commitmentFor(const std::string& device_id,
                          const std::string& name,
                          const std::string& fingerprint,
                          const std::string& nonce) {
    QByteArray canonical("O2PAIR1-commit");
    appendField(canonical, device_id);
    appendField(canonical, name);
    appendField(canonical, fingerprint);
    appendField(canonical, nonce);
    return QCryptographicHash::hash(canonical, QCryptographicHash::Sha256)
        .toHex()
        .toStdString();
}

}  // namespace

PairingSession::PairingSession(SessionTransport& transport, PairingLocal local,
                               std::string expected_peer_id,
                               PairingCallbacks callbacks)
    : transport_(transport),
      local_(std::move(local)),
      expected_peer_id_(std::move(expected_peer_id)),
      callbacks_(std::move(callbacks)) {}

PairingSession::~PairingSession() { detach(); }

void PairingSession::start() {
    if (state_ != PairingState::idle) {
        return;
    }
    local_.name = sanitizeName(local_.name);
    if (local_.name.empty()) {
        local_.name = local_.device_id;
    }
    local_.fingerprint = normalizeFingerprint(local_.fingerprint);
    if (!safeId(local_.device_id) || local_.fingerprint.empty()) {
        finishWithError("la identidad local no es válida para emparejar");
        return;
    }
    if (!expected_peer_id_.empty() && !safeId(expected_peer_id_)) {
        finishWithError("el identificador del equipo esperado no es válido");
        return;
    }

    local_nonce_ = randomHex(16);
    transport_.setReceiver(
        [this](SessionChannelKind channel, std::vector<std::uint8_t> frame) {
            onFrame(channel, std::move(frame));
        });
    transport_.setClosedHandler([this] { onClosed(); });
    local_commit_ = commitmentFor(local_.device_id, local_.name,
                                  local_.fingerprint, local_nonce_);
    state_ = PairingState::waiting_peer;
    sendCommit();
}

void PairingSession::sendCommit() {
    if (!sendMessage(std::string(kMagic) + "\x1f" "commit" "\x1f" +
                     local_commit_)) {
        finishWithError("no se pudo enviar el compromiso de emparejamiento");
    }
}

void PairingSession::sendReveal() {
    if (reveal_sent_) {
        return;
    }
    const std::string message =
        std::string(kMagic) + "\x1f" "reveal" "\x1f" + local_.device_id +
        "\x1f" + local_.name + "\x1f" + local_.fingerprint + "\x1f" +
        local_nonce_;
    if (!sendMessage(message)) {
        finishWithError("no se pudo enviar la identidad de emparejamiento");
        return;
    }
    reveal_sent_ = true;
}

void PairingSession::onFrame(SessionChannelKind channel,
                             std::vector<std::uint8_t> frame) {
    if (channel != SessionChannelKind::interactive ||
        state_ == PairingState::complete || state_ == PairingState::failed ||
        state_ == PairingState::rejected) {
        return;
    }
    const std::string message(frame.begin(), frame.end());
    const auto fields = splitFields(message);
    if (fields.size() < 2 || fields[0] != kMagic) {
        finishWithError("mensaje de emparejamiento no reconocido");
        return;
    }
    if (fields[1] == "commit") {
        receiveCommit(fields);
    } else if (fields[1] == "reveal") {
        receiveReveal(fields);
    } else if (fields[1] == "confirm") {
        receiveConfirm(fields);
    } else if (fields[1] == "reject") {
        receiveReject(fields);
    } else {
        finishWithError("tipo de mensaje de emparejamiento no reconocido");
    }
}

void PairingSession::receiveCommit(const std::vector<std::string>& fields) {
    if (peer_committed_ || state_ != PairingState::waiting_peer) {
        finishWithError("el otro equipo envió un compromiso duplicado");
        return;
    }
    if (fields.size() != 3) {
        finishWithError("el compromiso del otro equipo está incompleto");
        return;
    }
    peer_commit_ = fields[2];
    std::transform(peer_commit_.begin(), peer_commit_.end(),
                   peer_commit_.begin(), [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    if (peer_commit_.size() != 64 ||
        !std::all_of(peer_commit_.begin(), peer_commit_.end(),
                     [](unsigned char character) {
                         return std::isxdigit(character);
                     })) {
        finishWithError("el compromiso del otro equipo no es válido");
        return;
    }
    peer_committed_ = true;
    sendReveal();
}

void PairingSession::receiveReveal(const std::vector<std::string>& fields) {
    if (reveal_received_ || !peer_committed_ ||
        state_ != PairingState::waiting_peer) {
        finishWithError("el otro equipo envió una identidad duplicada");
        return;
    }
    if (fields.size() != 6) {
        finishWithError("la identidad del otro equipo está incompleta");
        return;
    }
    PairingPeer peer;
    peer.device_id = fields[2];
    peer.name = sanitizeName(fields[3]);
    peer.fingerprint = normalizeFingerprint(fields[4]);
    peer_nonce_ = fields[5];
    if (!safeId(peer.device_id) || peer.fingerprint.empty() ||
        peer_nonce_.size() != 32 ||
        !std::all_of(peer_nonce_.begin(), peer_nonce_.end(),
                     [](unsigned char character) {
                         return std::isxdigit(character);
                     })) {
        finishWithError("la identidad del otro equipo no es válida");
        return;
    }
    if (commitmentFor(peer.device_id, peer.name, peer.fingerprint,
                      peer_nonce_) != peer_commit_) {
        finishWithError("la identidad revelada no coincide con el compromiso");
        return;
    }
    if (!expected_peer_id_.empty() &&
        peer.device_id != expected_peer_id_) {
        finishWithError("el equipo conectado no es el esperado");
        return;
    }
    const std::string observed =
        normalizeFingerprint(transport_.authenticated_peer_fingerprint());
    if (!observed.empty() && observed != peer.fingerprint) {
        finishWithError(
            "la huella del certificado no coincide con la identidad anunciada");
        return;
    }
    if (peer.device_id == local_.device_id) {
        finishWithError("no se puede emparejar un equipo consigo mismo");
        return;
    }
    if (peer.name.empty()) {
        peer.name = peer.device_id;
    }
    peer_ = peer;
    reveal_received_ = true;
    computeTranscript();
    state_ = PairingState::waiting_confirmation;
    if (callbacks_.sas_ready) {
        callbacks_.sas_ready(sas_, peer_);
    }
}

void PairingSession::computeTranscript() {
    struct Record {
        std::string id;
        std::string name;
        std::string fingerprint;
        std::string nonce;
    };
    std::vector<Record> records{
        Record{local_.device_id, local_.name, local_.fingerprint,
               local_nonce_},
        Record{peer_.device_id, peer_.name, peer_.fingerprint, peer_nonce_},
    };
    std::sort(records.begin(), records.end(),
              [](const Record& left, const Record& right) {
                  if (left.id != right.id) {
                      return left.id < right.id;
                  }
                  return left.fingerprint < right.fingerprint;
              });

    QByteArray canonical(kMagic);
    canonical.append('\x1f');
    for (const Record& record : records) {
        appendField(canonical, record.id);
        appendField(canonical, record.name);
        appendField(canonical, record.fingerprint);
        appendField(canonical, record.nonce);
    }
    const QByteArray digest =
        QCryptographicHash::hash(canonical, QCryptographicHash::Sha256);
    transcript_hash_ = digest.toHex().toStdString();

    const unsigned int value =
        (static_cast<unsigned int>(
             static_cast<unsigned char>(digest.at(0)))
         << 24) |
        (static_cast<unsigned int>(
             static_cast<unsigned char>(digest.at(1)))
         << 16) |
        (static_cast<unsigned int>(
             static_cast<unsigned char>(digest.at(2)))
         << 8) |
        static_cast<unsigned int>(static_cast<unsigned char>(digest.at(3)));
    sas_ = QStringLiteral("%1")
               .arg(static_cast<qulonglong>(value % 1000000u), 6, 10,
                    QLatin1Char('0'))
               .toStdString();
}

void PairingSession::confirm() {
    if (state_ != PairingState::waiting_confirmation || local_confirmed_) {
        return;
    }
    if (!sendMessage(std::string(kMagic) + "\x1f" "confirm" "\x1f" +
                     transcript_hash_)) {
        finishWithError("no se pudo enviar la confirmación de emparejamiento");
        return;
    }
    local_confirmed_ = true;
    if (peer_confirmed_) {
        finishComplete();
    }
}

void PairingSession::receiveConfirm(const std::vector<std::string>& fields) {
    if (state_ != PairingState::waiting_confirmation ||
        transcript_hash_.empty() || fields.size() != 3) {
        finishWithError("confirmación de emparejamiento no válida");
        return;
    }
    std::string received = fields[2];
    std::transform(received.begin(), received.end(), received.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    if (received != transcript_hash_) {
        finishWithError(
            "la confirmación no corresponde a esta conversación de emparejamiento");
        return;
    }
    peer_confirmed_ = true;
    if (local_confirmed_) {
        finishComplete();
    }
}

void PairingSession::reject(const std::string& reason) {
    if (state_ == PairingState::idle || state_ == PairingState::complete ||
        state_ == PairingState::rejected || state_ == PairingState::failed) {
        return;
    }
    sendMessage(std::string(kMagic) + "\x1freject\x1f" + reason);
    state_ = PairingState::rejected;
    detach();
}

void PairingSession::receiveReject(const std::vector<std::string>& fields) {
    const std::string reason =
        fields.size() >= 3 && !fields[2].empty()
            ? fields[2]
            : "el otro equipo rechazó el emparejamiento";
    state_ = PairingState::rejected;
    detach();
    if (callbacks_.failed) {
        callbacks_.failed(reason);
    }
}

void PairingSession::finishComplete() {
    if (state_ == PairingState::complete) {
        return;
    }
    state_ = PairingState::complete;
    detach();
    if (!terminal_callback_sent_) {
        terminal_callback_sent_ = true;
        if (callbacks_.completed) {
            callbacks_.completed(peer_);
        }
    }
}

void PairingSession::finishWithError(const std::string& message) {
    if (state_ == PairingState::complete || state_ == PairingState::failed ||
        state_ == PairingState::rejected) {
        return;
    }
    state_ = PairingState::failed;
    detach();
    if (!terminal_callback_sent_) {
        terminal_callback_sent_ = true;
        if (callbacks_.failed) {
            callbacks_.failed(message);
        }
    }
}

void PairingSession::onClosed() {
    if (state_ != PairingState::complete && state_ != PairingState::failed &&
        state_ != PairingState::rejected) {
        finishWithError("se cerró el canal de emparejamiento");
    }
}

bool PairingSession::sendMessage(const std::string& message) {
    const std::vector<std::uint8_t> frame(message.begin(), message.end());
    return transport_.send(SessionChannelKind::interactive, frame) ==
           TransportSendStatus::sent;
}

void PairingSession::detach() {
    transport_.setReceiver({});
    transport_.setClosedHandler({});
}

}  // namespace oneintwo
