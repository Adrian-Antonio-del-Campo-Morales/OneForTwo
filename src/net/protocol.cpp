#include "protocol.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>

namespace oneintwo {
namespace {

constexpr std::size_t max_notice_title = max_notification_title_bytes;
constexpr std::size_t max_notice_body = max_notification_body_bytes;
constexpr std::size_t max_notice_app = max_notification_app_name_bytes;

bool validUtf8(std::string_view text) {
    std::size_t index = 0;
    while (index < text.size()) {
        const unsigned char first = static_cast<unsigned char>(text[index]);
        if (first < 0x80) {
            ++index;
            continue;
        }
        int continuation = 0;
        std::uint32_t code_point = 0;
        if ((first & 0xE0) == 0xC0) {
            continuation = 1;
            code_point = first & 0x1Fu;
        } else if ((first & 0xF0) == 0xE0) {
            continuation = 2;
            code_point = first & 0x0Fu;
        } else if ((first & 0xF8) == 0xF0) {
            continuation = 3;
            code_point = first & 0x07u;
        } else {
            return false;
        }
        if (index + static_cast<std::size_t>(continuation) >= text.size()) {
            return false;
        }
        for (int offset = 1; offset <= continuation; ++offset) {
            const unsigned char next =
                static_cast<unsigned char>(text[index + static_cast<std::size_t>(offset)]);
            if ((next & 0xC0) != 0x80) {
                return false;
            }
            code_point = (code_point << 6) | (next & 0x3Fu);
        }
        if ((continuation == 1 && code_point < 0x80) ||
            (continuation == 2 && code_point < 0x800) ||
            (continuation == 3 && code_point < 0x10000) ||
            code_point > 0x10FFFF || (code_point >= 0xD800 && code_point <= 0xDFFF)) {
            return false;
        }
        index += static_cast<std::size_t>(continuation) + 1;
    }
    return true;
}

void putU8(std::vector<std::uint8_t>& out, std::uint8_t value) {
    out.push_back(value);
}

void putU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
}

void putU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
}

void putU64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
    }
}

void putString(std::vector<std::uint8_t>& out, const std::string& value) {
    putU32(out, static_cast<std::uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

std::uint32_t readU32(const std::uint8_t* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) |
           static_cast<std::uint32_t>(data[3]);
}

std::uint16_t readU16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8) |
                                      static_cast<std::uint16_t>(data[1]));
}

class Reader {
public:
    Reader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

    bool u8(std::uint8_t& out) {
        if (remaining() < 1) {
            return false;
        }
        out = data_[position_++];
        return true;
    }

    bool u16(std::uint16_t& out) {
        if (remaining() < 2) {
            return false;
        }
        out = readU16(data_ + position_);
        position_ += 2;
        return true;
    }

    bool u32(std::uint32_t& out) {
        if (remaining() < 4) {
            return false;
        }
        out = readU32(data_ + position_);
        position_ += 4;
        return true;
    }

    bool u64(std::uint64_t& out) {
        if (remaining() < 8) {
            return false;
        }
        out = 0;
        for (int index = 0; index < 8; ++index) {
            out = (out << 8) | data_[position_ + static_cast<std::size_t>(index)];
        }
        position_ += 8;
        return true;
    }

    bool text(std::string& out, std::size_t maximum) {
        std::uint32_t length = 0;
        if (!u32(length) || length > maximum || remaining() < length) {
            return false;
        }
        out.assign(reinterpret_cast<const char*>(data_ + position_), length);
        position_ += length;
        return validUtf8(out);
    }

    bool done() const { return position_ == size_; }

private:
    std::size_t remaining() const { return size_ - position_; }

    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t position_ = 0;
};

std::uint8_t wireInitiator(VisualInitiator initiator) {
    return initiator == VisualInitiator::local ? 0u : 1u;
}

bool decodeInitiator(std::uint8_t wire, VisualInitiator& out) {
    if (wire == 0) {
        out = VisualInitiator::peer;
        return true;
    }
    if (wire == 1) {
        out = VisualInitiator::local;
        return true;
    }
    return false;
}

bool encodeRef(std::vector<std::uint8_t>& out, const VisualShareRef& ref,
               GenerationPair generations) {
    if (ref.generation != generations.local || ref.offer_id == 0) {
        return false;
    }
    putU64(out, generations.local);
    putU8(out, wireInitiator(ref.initiator));
    putU64(out, ref.offer_id);
    return true;
}

ProtocolStatus decodeRef(Reader& reader, GenerationPair generations, VisualShareRef& out) {
    std::uint64_t generation = 0;
    std::uint8_t initiator = 0;
    std::uint64_t offer_id = 0;
    if (!reader.u64(generation) || !reader.u8(initiator) || !reader.u64(offer_id)) {
        return ProtocolStatus::malformed;
    }
    if (generation != generations.peer) {
        return ProtocolStatus::generation_mismatch;
    }
    if (offer_id == 0 || !decodeInitiator(initiator, out.initiator)) {
        return ProtocolStatus::malformed;
    }
    out.generation = generations.local;
    out.offer_id = offer_id;
    return ProtocolStatus::ok;
}

bool sourceFromWire(std::uint8_t value, VisualSourceKind& out) {
    if (value > 1) {
        return false;
    }
    out = value == 0 ? VisualSourceKind::monitor : VisualSourceKind::window;
    return true;
}

bool decisionFromWire(std::uint8_t value, VisualDecision& out) {
    if (value > 2) {
        return false;
    }
    out = value == 0 ? VisualDecision::accepted
                     : (value == 1 ? VisualDecision::rejected : VisualDecision::busy);
    return true;
}

bool stateFromWire(std::uint8_t value, VisualSourceState& out) {
    if (value > 2) {
        return false;
    }
    out = value == 0 ? VisualSourceState::available
                     : (value == 1 ? VisualSourceState::suspended : VisualSourceState::closed);
    return true;
}

bool modeFromWire(std::uint8_t value, NotificationMode& out) {
    if (value > 1) {
        return false;
    }
    out = value == 0 ? NotificationMode::manual : NotificationMode::forwarded;
    return true;
}

bool sizeWithinLimits(VisualSize size) {
    if (size.width == 0 || size.height == 0) {
        return false;
    }
    if (size.width > max_visual_dimension || size.height > max_visual_dimension) {
        return false;
    }
    const std::uint64_t pixels =
        static_cast<std::uint64_t>(size.width) * static_cast<std::uint64_t>(size.height);
    return pixels <= max_visual_pixels;
}

bool validateRef(const VisualShareRef& ref, GenerationPair generations) {
    return ref.generation == generations.local && ref.offer_id != 0;
}

std::vector<std::uint8_t> frameBytes(FeatureWireType type,
                                     const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> frame;
    frame.reserve(feature_frame_header_bytes + payload.size());
    putU16(frame, feature_protocol_version);
    putU16(frame, static_cast<std::uint16_t>(type));
    putU32(frame, static_cast<std::uint32_t>(payload.size()));
    frame.insert(frame.end(), payload.begin(), payload.end());
    return frame;
}

ProtocolStatus commit(ProtocolChannel channel, FeatureWireType type,
                      std::vector<std::uint8_t> payload, std::vector<WireFrame>& out) {
    WireFrame frame;
    frame.channel = channel;
    frame.bytes = frameBytes(type, payload);
    out.push_back(std::move(frame));
    return ProtocolStatus::ok;
}

} // namespace

struct FeatureFrameDecoder::Assembly {
    VisualShareRef ref;
    std::uint64_t revision = 0;
    std::uint64_t sequence = 0;
    VisualSize size;
    bool key_frame = true;
    std::uint32_t total = 0;
    std::vector<std::uint8_t> data;
};

std::vector<WireFrame> encodeFeatureMessage(const FeatureMessage& message,
                                            GenerationPair generations,
                                            ProtocolStatus* error) {
    std::vector<WireFrame> frames;
    const ProtocolStatus status = encodeFeatureMessage(message, generations, frames);
    if (error != nullptr) {
        *error = status;
    }
    return frames;
}

ProtocolStatus encodeFeatureMessage(const FeatureMessage& message,
                                    GenerationPair generations,
                                    std::vector<WireFrame>& out) {
    out.clear();
    if (generations.local == 0 || generations.peer == 0) {
        return ProtocolStatus::generation_mismatch;
    }

    if (const auto* offer = std::get_if<VisualShareOffer>(&message)) {
        if (!validateRef(offer->ref, generations) || offer->ref.initiator != VisualInitiator::local) {
            return ProtocolStatus::out_of_range;
        }
        if (static_cast<unsigned>(offer->source) > 1) {
            return ProtocolStatus::out_of_range;
        }
        std::vector<std::uint8_t> payload;
        if (!encodeRef(payload, offer->ref, generations)) {
            return ProtocolStatus::out_of_range;
        }
        putU8(payload, offer->source == VisualSourceKind::monitor ? 0u : 1u);
        if (payload.size() > max_interactive_payload_bytes) {
            return ProtocolStatus::payload_too_large;
        }
        return commit(ProtocolChannel::interactive, FeatureWireType::offer, std::move(payload), out);
    }

    if (const auto* decision = std::get_if<VisualDecisionMessage>(&message)) {
        if (!validateRef(decision->ref, generations) ||
            static_cast<unsigned>(decision->decision) > 2) {
            return ProtocolStatus::out_of_range;
        }
        std::vector<std::uint8_t> payload;
        if (!encodeRef(payload, decision->ref, generations)) {
            return ProtocolStatus::out_of_range;
        }
        putU8(payload, static_cast<std::uint8_t>(decision->decision));
        if (payload.size() > max_interactive_payload_bytes) {
            return ProtocolStatus::payload_too_large;
        }
        return commit(ProtocolChannel::interactive, FeatureWireType::decision,
                      std::move(payload), out);
    }

    if (const auto* state = std::get_if<VisualStateMessage>(&message)) {
        if (!validateRef(state->ref, generations) || state->revision == 0 ||
            static_cast<unsigned>(state->state) > 2) {
            return ProtocolStatus::out_of_range;
        }
        std::vector<std::uint8_t> payload;
        if (!encodeRef(payload, state->ref, generations)) {
            return ProtocolStatus::out_of_range;
        }
        putU64(payload, state->revision);
        putU8(payload, static_cast<std::uint8_t>(state->state));
        if (payload.size() > max_interactive_payload_bytes) {
            return ProtocolStatus::payload_too_large;
        }
        return commit(ProtocolChannel::interactive, FeatureWireType::state, std::move(payload), out);
    }

    if (const auto* activity = std::get_if<VisualActivityMessage>(&message)) {
        if (!validateRef(activity->ref, generations) || activity->revision == 0) {
            return ProtocolStatus::out_of_range;
        }
        std::vector<std::uint8_t> payload;
        if (!encodeRef(payload, activity->ref, generations)) {
            return ProtocolStatus::out_of_range;
        }
        putU64(payload, activity->revision);
        if (payload.size() > max_interactive_payload_bytes) {
            return ProtocolStatus::payload_too_large;
        }
        return commit(ProtocolChannel::interactive, FeatureWireType::activity,
                      std::move(payload), out);
    }

    if (const auto* stop = std::get_if<VisualStopMessage>(&message)) {
        if (!validateRef(stop->ref, generations)) {
            return ProtocolStatus::out_of_range;
        }
        std::vector<std::uint8_t> payload;
        if (!encodeRef(payload, stop->ref, generations)) {
            return ProtocolStatus::out_of_range;
        }
        if (payload.size() > max_interactive_payload_bytes) {
            return ProtocolStatus::payload_too_large;
        }
        return commit(ProtocolChannel::interactive, FeatureWireType::stop, std::move(payload), out);
    }

    if (const auto* frame = std::get_if<VisualEncodedFrame>(&message)) {
        if (!validateRef(frame->ref, generations)) {
            return ProtocolStatus::out_of_range;
        }
        if (!sizeWithinLimits(frame->size) || frame->bytes.empty() ||
            frame->bytes.size() > max_visual_encoded_bytes || !frame->key_frame ||
            frame->revision == 0 || frame->sequence == 0) {
            return ProtocolStatus::out_of_range;
        }
        if (frame->bytes.size() > std::numeric_limits<std::uint32_t>::max()) {
            return ProtocolStatus::payload_too_large;
        }
        const std::uint32_t total = static_cast<std::uint32_t>(frame->bytes.size());
        std::size_t offset = 0;
        while (offset < frame->bytes.size()) {
            const std::size_t chunk =
                std::min(max_fragment_data_bytes, frame->bytes.size() - offset);
            std::vector<std::uint8_t> payload;
            payload.reserve(visual_fragment_metadata_bytes + chunk);
            if (!encodeRef(payload, frame->ref, generations)) {
                return ProtocolStatus::out_of_range;
            }
            putU64(payload, frame->revision);
            putU64(payload, frame->sequence);
            putU32(payload, frame->size.width);
            putU32(payload, frame->size.height);
            putU8(payload, frame->key_frame ? 1u : 0u);
            putU32(payload, total);
            putU32(payload, static_cast<std::uint32_t>(offset));
            payload.insert(payload.end(), frame->bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                           frame->bytes.begin() + static_cast<std::ptrdiff_t>(offset + chunk));
            if (payload.size() != visual_fragment_metadata_bytes + chunk) {
                return ProtocolStatus::malformed;
            }
            WireFrame wire;
            wire.channel = ProtocolChannel::data;
            wire.bytes = frameBytes(FeatureWireType::frame, payload);
            out.push_back(std::move(wire));
            offset += chunk;
        }
        return ProtocolStatus::ok;
    }

    if (const auto* notice = std::get_if<RemoteNotification>(&message)) {
        if (notice->ref.generation != generations.local || notice->ref.notice_id == 0) {
            return ProtocolStatus::out_of_range;
        }
        if (notice->content.title.size() > max_notice_title ||
            notice->content.body.size() > max_notice_body ||
            notice->app_name.size() > max_notice_app ||
            !validUtf8(notice->content.title) || !validUtf8(notice->content.body) ||
            !validUtf8(notice->app_name)) {
            return ProtocolStatus::out_of_range;
        }
        if (notice->mode == NotificationMode::manual && !notice->app_name.empty()) {
            return ProtocolStatus::out_of_range;
        }
        if (static_cast<unsigned>(notice->mode) > 1) {
            return ProtocolStatus::out_of_range;
        }
        std::vector<std::uint8_t> payload;
        putU64(payload, generations.local);
        putU64(payload, notice->ref.notice_id);
        putU8(payload, static_cast<std::uint8_t>(notice->mode));
        putString(payload, notice->content.title);
        putString(payload, notice->content.body);
        putString(payload, notice->app_name);
        if (payload.size() > max_interactive_payload_bytes) {
            return ProtocolStatus::payload_too_large;
        }
        return commit(ProtocolChannel::interactive, FeatureWireType::notice, std::move(payload), out);
    }

    return ProtocolStatus::unknown_type;
}

FeatureFrameDecoder::FeatureFrameDecoder(GenerationPair generations)
    : generations_(generations) {}

FeatureFrameDecoder::~FeatureFrameDecoder() = default;

void FeatureFrameDecoder::setGenerations(GenerationPair generations) {
    generations_ = generations;
    reset();
}

void FeatureFrameDecoder::reset() {
    buffer_.clear();
    assembly_.reset();
}

std::size_t FeatureFrameDecoder::bufferedBytes() const {
    return buffer_.size();
}

bool FeatureFrameDecoder::hasAssembly() const {
    return assembly_ != nullptr;
}

ProtocolStatus FeatureFrameDecoder::appendFragment(const std::uint8_t* payload,
                                                  std::size_t size,
                                                  DecodeResult& out) {
    if (size < visual_fragment_metadata_bytes) {
        return ProtocolStatus::malformed;
    }
    Reader reader(payload, size);
    VisualShareRef ref;
    std::uint64_t revision = 0;
    std::uint64_t sequence = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint8_t key_frame = 0;
    std::uint32_t total = 0;
    std::uint32_t offset = 0;

    const ProtocolStatus ref_status = decodeRef(reader, generations_, ref);
    if (ref_status != ProtocolStatus::ok) {
        return ref_status;
    }
    if (!reader.u64(revision) || !reader.u64(sequence) || !reader.u32(width) ||
        !reader.u32(height) || !reader.u8(key_frame) || !reader.u32(total) ||
        !reader.u32(offset)) {
        return ProtocolStatus::malformed;
    }
    if (revision == 0 || sequence == 0 || key_frame > 1) {
        return ProtocolStatus::out_of_range;
    }
    const VisualSize dimensions{width, height};
    if (!sizeWithinLimits(dimensions)) {
        return ProtocolStatus::out_of_range;
    }
    if (total == 0 || total > max_visual_encoded_bytes) {
        return ProtocolStatus::out_of_range;
    }
    const std::size_t data_size =
        size - visual_fragment_metadata_bytes;
    if (data_size == 0 || offset > total || data_size > total - offset) {
        return ProtocolStatus::out_of_range;
    }

    if (assembly_ == nullptr) {
        if (offset != 0) {
            return ProtocolStatus::out_of_range;
        }
        assembly_ = std::make_unique<Assembly>();
        assembly_->ref = ref;
        assembly_->revision = revision;
        assembly_->sequence = sequence;
        assembly_->size = dimensions;
        assembly_->key_frame = key_frame == 1;
        assembly_->total = total;
        assembly_->data.reserve(total);
    } else {
        if (assembly_->ref.generation != ref.generation ||
            assembly_->ref.initiator != ref.initiator ||
            assembly_->ref.offer_id != ref.offer_id ||
            assembly_->revision != revision || assembly_->sequence != sequence ||
            assembly_->size.width != width || assembly_->size.height != height ||
            assembly_->total != total || assembly_->key_frame != (key_frame == 1)) {
            return ProtocolStatus::assembly_mismatch;
        }
        if (static_cast<std::size_t>(offset) != assembly_->data.size()) {
            return ProtocolStatus::out_of_range;
        }
    }

    assembly_->data.insert(assembly_->data.end(), payload + visual_fragment_metadata_bytes,
                           payload + size);
    if (assembly_->data.size() > assembly_->total) {
        return ProtocolStatus::out_of_range;
    }
    if (assembly_->data.size() == assembly_->total) {
        VisualEncodedFrame frame;
        frame.ref = assembly_->ref;
        frame.revision = assembly_->revision;
        frame.sequence = assembly_->sequence;
        frame.size = assembly_->size;
        frame.key_frame = assembly_->key_frame;
        frame.bytes = std::move(assembly_->data);
        assembly_.reset();
        out.message = std::move(frame);
    }
    return ProtocolStatus::ok;
}

ProtocolStatus FeatureFrameDecoder::decodePayload(FeatureWireType type,
                                                  const std::uint8_t* payload,
                                                  std::size_t size,
                                                  DecodeResult& out) {
    Reader reader(payload, size);

    switch (type) {
    case FeatureWireType::offer: {
        VisualShareOffer offer;
        std::uint8_t source = 0;
        const ProtocolStatus ref_status = decodeRef(reader, generations_, offer.ref);
        if (ref_status != ProtocolStatus::ok) {
            return ref_status;
        }
        if (!reader.u8(source) || !sourceFromWire(source, offer.source) || !reader.done()) {
            return ProtocolStatus::malformed;
        }
        out.message = std::move(offer);
        return ProtocolStatus::ok;
    }
    case FeatureWireType::decision: {
        VisualDecisionMessage decision;
        std::uint8_t value = 0;
        const ProtocolStatus ref_status = decodeRef(reader, generations_, decision.ref);
        if (ref_status != ProtocolStatus::ok) {
            return ref_status;
        }
        if (!reader.u8(value) || !decisionFromWire(value, decision.decision) ||
            !reader.done()) {
            return ProtocolStatus::malformed;
        }
        out.message = std::move(decision);
        return ProtocolStatus::ok;
    }
    case FeatureWireType::state: {
        VisualStateMessage state;
        std::uint8_t value = 0;
        const ProtocolStatus ref_status = decodeRef(reader, generations_, state.ref);
        if (ref_status != ProtocolStatus::ok) {
            return ref_status;
        }
        if (!reader.u64(state.revision) || !reader.u8(value) ||
            !stateFromWire(value, state.state) || !reader.done()) {
            return ProtocolStatus::malformed;
        }
        if (state.revision == 0) {
            return ProtocolStatus::out_of_range;
        }
        out.message = std::move(state);
        return ProtocolStatus::ok;
    }
    case FeatureWireType::activity: {
        VisualActivityMessage activity;
        const ProtocolStatus ref_status = decodeRef(reader, generations_, activity.ref);
        if (ref_status != ProtocolStatus::ok) {
            return ref_status;
        }
        if (!reader.u64(activity.revision) || !reader.done()) {
            return ProtocolStatus::malformed;
        }
        if (activity.revision == 0) {
            return ProtocolStatus::out_of_range;
        }
        out.message = std::move(activity);
        return ProtocolStatus::ok;
    }
    case FeatureWireType::stop: {
        VisualStopMessage stop;
        const ProtocolStatus ref_status = decodeRef(reader, generations_, stop.ref);
        if (ref_status != ProtocolStatus::ok) {
            return ref_status;
        }
        if (!reader.done()) {
            return ProtocolStatus::malformed;
        }
        out.message = std::move(stop);
        return ProtocolStatus::ok;
    }
    case FeatureWireType::notice: {
        RemoteNotification notice;
        std::uint8_t mode = 0;
        if (!reader.u64(notice.ref.generation) || !reader.u64(notice.ref.notice_id) ||
            !reader.u8(mode) || !modeFromWire(mode, notice.mode) ||
            !reader.text(notice.content.title, max_notice_title) ||
            !reader.text(notice.content.body, max_notice_body) ||
            !reader.text(notice.app_name, max_notice_app) || !reader.done()) {
            return ProtocolStatus::malformed;
        }
        if (notice.ref.generation != generations_.peer || notice.ref.notice_id == 0) {
            return ProtocolStatus::generation_mismatch;
        }
        if (notice.mode == NotificationMode::manual && !notice.app_name.empty()) {
            return ProtocolStatus::malformed;
        }
        notice.ref.generation = generations_.local;
        out.message = std::move(notice);
        return ProtocolStatus::ok;
    }
    case FeatureWireType::frame:
        break;
    }
    return ProtocolStatus::unknown_type;
}

DecodeResult FeatureFrameDecoder::feed(const std::uint8_t* data, std::size_t size) {
    DecodeResult out;
    if (data != nullptr && size > 0) {
        if (buffer_.size() + size > feature_frame_header_bytes + visual_fragment_payload_bytes) {
            out.status = ProtocolStatus::payload_too_large;
            reset();
            return out;
        }
        buffer_.insert(buffer_.end(), data, data + size);
    }

    if (buffer_.size() < feature_frame_header_bytes) {
        out.status = ProtocolStatus::needs_more;
        return out;
    }

    const std::uint16_t version = readU16(buffer_.data());
    if (version != feature_protocol_version) {
        out.status = ProtocolStatus::version_unsupported;
        reset();
        return out;
    }
    const std::uint16_t raw_type = readU16(buffer_.data() + 2);
    if (raw_type < 1 || raw_type > 7) {
        out.status = ProtocolStatus::unknown_type;
        reset();
        return out;
    }
    const auto type = static_cast<FeatureWireType>(raw_type);
    out.channel = type == FeatureWireType::frame ? ProtocolChannel::data
                                                 : ProtocolChannel::interactive;
    const std::uint32_t length = readU32(buffer_.data() + 4);
    const std::size_t limit =
        type == FeatureWireType::frame ? visual_fragment_payload_bytes
                                       : max_interactive_payload_bytes;
    if (length > limit) {
        out.status = ProtocolStatus::payload_too_large;
        reset();
        return out;
    }
    if (buffer_.size() < feature_frame_header_bytes + length) {
        out.status = ProtocolStatus::needs_more;
        return out;
    }

    const std::uint8_t* payload = buffer_.data() + feature_frame_header_bytes;
    ProtocolStatus status = ProtocolStatus::ok;
    if (type == FeatureWireType::frame) {
        status = appendFragment(payload, length, out);
    } else {
        status = decodePayload(type, payload, length, out);
    }
    buffer_.erase(buffer_.begin(),
                  buffer_.begin() + static_cast<std::ptrdiff_t>(feature_frame_header_bytes + length));
    out.status = status;
    if (status != ProtocolStatus::ok && status != ProtocolStatus::needs_more) {
        reset();
    }
    return out;
}

} // namespace oneintwo
