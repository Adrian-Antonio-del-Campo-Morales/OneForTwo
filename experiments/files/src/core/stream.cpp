// Responsabilidad: flujo en memoria con contrapresión acotada para los
// self-tests del protocolo. No abre descriptores ni red.
#include "core/stream.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <vector>

#include "core/model.hpp"

namespace o2f {
namespace {

constexpr std::size_t kMaxBuffered = 32u * 1024u * 1024u;

struct PipeState {
    std::mutex mutex;
    std::condition_variable readable;
    std::condition_variable writable;
    std::vector<std::uint8_t> data;
    std::size_t readPos = 0;
    bool closed = false;

    std::size_t available() const { return data.size() - readPos; }
    void compact() {
        if (readPos > 0 && (readPos > 64u * 1024u || readPos == data.size())) {
            data.erase(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(readPos));
            readPos = 0;
        }
    }
};

class PipeEnd : public Stream {
public:
    PipeEnd(std::shared_ptr<PipeState> incoming, std::shared_ptr<PipeState> outgoing)
        : incoming_(std::move(incoming)), outgoing_(std::move(outgoing)) {}

    ~PipeEnd() override { close(); }

    std::size_t readSome(void* buffer, std::size_t len) override {
        std::unique_lock<std::mutex> lock(incoming_->mutex);
        incoming_->readable.wait(lock, [this] {
            return incoming_->available() > 0 || incoming_->closed || outgoing_->closed;
        });
        const std::size_t take = std::min(len, incoming_->available());
        if (take == 0) return 0;
        std::memcpy(buffer, incoming_->data.data() + incoming_->readPos, take);
        incoming_->readPos += take;
        incoming_->compact();
        incoming_->writable.notify_all();
        return take;
    }

    void writeAll(const void* buffer, std::size_t len) override {
        const auto* bytes = static_cast<const std::uint8_t*>(buffer);
        std::unique_lock<std::mutex> lock(outgoing_->mutex);
        while (len > 0) {
            outgoing_->writable.wait(lock, [this] {
                return outgoing_->available() < kMaxBuffered || outgoing_->closed || closed_;
            });
            if (outgoing_->closed || closed_) throw Error("flujo de prueba cerrado");
            const std::size_t space = kMaxBuffered - outgoing_->available();
            const std::size_t take = std::min(len, space);
            outgoing_->data.insert(outgoing_->data.end(), bytes, bytes + take);
            bytes += take;
            len -= take;
            outgoing_->readable.notify_all();
        }
    }

    void close() override {
        bool expected = false;
        if (!closed_.compare_exchange_strong(expected, true)) return;
        {
            std::lock_guard<std::mutex> lock(incoming_->mutex);
            incoming_->closed = true;
        }
        incoming_->readable.notify_all();
        {
            std::lock_guard<std::mutex> lock(outgoing_->mutex);
            outgoing_->closed = true;
        }
        outgoing_->readable.notify_all();
        outgoing_->writable.notify_all();
    }

private:
    std::shared_ptr<PipeState> incoming_;
    std::shared_ptr<PipeState> outgoing_;
    std::atomic<bool> closed_{false};
};

}  // namespace

void readExact(Stream& stream, void* buffer, std::size_t len) {
    auto* bytes = static_cast<std::uint8_t*>(buffer);
    std::size_t done = 0;
    while (done < len) {
        const std::size_t got = stream.readSome(bytes + done, len - done);
        if (got == 0) throw Error("flujo terminado antes de completar la lectura");
        done += got;
    }
}

LoopbackPair makeLoopbackPair() {
    auto ab = std::make_shared<PipeState>();
    auto ba = std::make_shared<PipeState>();
    LoopbackPair pair;
    pair.a.reset(new PipeEnd(ba, ab));
    pair.b.reset(new PipeEnd(ab, ba));
    return pair;
}

}  // namespace o2f
