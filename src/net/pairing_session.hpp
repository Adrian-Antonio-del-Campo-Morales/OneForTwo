#pragma once

#include "net/transport.hpp"

#include <functional>
#include <string>
#include <vector>

namespace oneintwo {

struct PairingLocal {
    std::string device_id;
    std::string name;
    std::string fingerprint;
};

struct PairingPeer {
    std::string device_id;
    std::string name;
    std::string fingerprint;
};

enum class PairingState {
    idle,
    waiting_peer,
    waiting_confirmation,
    complete,
    rejected,
    failed,
};

struct PairingCallbacks {
    std::function<void(std::string sas, PairingPeer peer)> sas_ready;
    std::function<void(PairingPeer peer)> completed;
    std::function<void(std::string message)> failed;
};

// Ceremony SAS on a channel TLS temporary unauthenticated.
// two ends trust the certificate until the two people compare
// the same SAS and both confirm.
// this channel, and is only used in later connections with pin fixed.
class PairingSession {
public:
    PairingSession(SessionTransport& transport, PairingLocal local,
                   std::string expected_peer_id, PairingCallbacks callbacks);
    ~PairingSession();

    PairingSession(const PairingSession&) = delete;
    PairingSession& operator=(const PairingSession&) = delete;

    void start();
    void confirm();
    void reject(const std::string& reason = {});

    PairingState state() const { return state_; }
    const PairingPeer& peer() const { return peer_; }
    const std::string& sas() const { return sas_; }
    const std::string& transcriptHash() const { return transcript_hash_; }

private:
    void onFrame(SessionChannelKind channel, std::vector<std::uint8_t> frame);
    void onClosed();
    void receiveCommit(const std::vector<std::string>& fields);
    void receiveReveal(const std::vector<std::string>& fields);
    void receiveConfirm(const std::vector<std::string>& fields);
    void receiveReject(const std::vector<std::string>& fields);

    void computeTranscript();
    void sendCommit();
    void sendReveal();
    bool sendMessage(const std::string& message);
    void finishWithError(const std::string& message);
    void finishComplete();
    void detach();

    SessionTransport& transport_;
    PairingLocal local_;
    std::string expected_peer_id_;
    PairingCallbacks callbacks_;
    PairingPeer peer_;
    std::string peer_nonce_;
    std::string local_nonce_;
    std::string local_commit_;
    std::string peer_commit_;
    std::string transcript_hash_;
    std::string sas_;
    PairingState state_ = PairingState::idle;
    bool local_confirmed_ = false;
    bool peer_confirmed_ = false;
    bool peer_committed_ = false;
    bool reveal_sent_ = false;
    bool reveal_received_ = false;
    bool terminal_callback_sent_ = false;
};

}  // namespace oneintwo
