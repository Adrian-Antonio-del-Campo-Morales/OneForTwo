#include "net/protocol.hpp"

#include "test_support.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace oneintwo;

namespace {

constexpr GenerationPair sender_generations{11, 22};
constexpr GenerationPair receiver_generations{22, 11};

VisualShareRef localRef(std::uint64_t id = 1, std::uint64_t generation = 11) {
    return VisualShareRef{generation, VisualInitiator::local, id};
}

VisualShareRef peerRef(std::uint64_t id = 1, std::uint64_t generation = 22) {
    return VisualShareRef{generation, VisualInitiator::peer, id};
}

std::vector<WireFrame> encodeOk(const FeatureMessage& message,
                                GenerationPair generations = sender_generations) {
    ProtocolStatus error = ProtocolStatus::ok;
    std::vector<WireFrame> frames = encodeFeatureMessage(message, generations, &error);
    CHECK_EQ(error, ProtocolStatus::ok);
    CHECK(!frames.empty());
    return frames;
}

std::vector<FeatureMessage> decodeOk(const std::vector<WireFrame>& frames,
                                     GenerationPair generations = receiver_generations) {
    FeatureFrameDecoder decoder(generations);
    std::vector<FeatureMessage> messages;
    for (const WireFrame& frame : frames) {
        DecodeResult result = decoder.feed(frame.bytes.data(), frame.bytes.size());
        CHECK_EQ(result.status, ProtocolStatus::ok);
        if (result.message.has_value()) {
            messages.push_back(std::move(*result.message));
        }
    }
    return messages;
}

FeatureMessage singleDecode(const FeatureMessage& message) {
    const std::vector<WireFrame> frames = encodeOk(message);
    CHECK_EQ(frames.size(), static_cast<std::size_t>(1));
    const std::vector<FeatureMessage> decoded = decodeOk(frames);
    CHECK_EQ(decoded.size(), static_cast<std::size_t>(1));
    return decoded.front();
}

void put16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
}

void put32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
}

void put64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
    }
}

std::vector<std::uint8_t> rawFrame(std::uint16_t type, const std::vector<std::uint8_t>& payload,
                                   std::uint16_t version = 1) {
    std::vector<std::uint8_t> bytes;
    put16(bytes, version);
    put16(bytes, type);
    put32(bytes, static_cast<std::uint32_t>(payload.size()));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}

std::vector<std::uint8_t> refBytes(std::uint64_t generation, std::uint8_t initiator,
                                   std::uint64_t id) {
    std::vector<std::uint8_t> bytes;
    put64(bytes, generation);
    bytes.push_back(initiator);
    put64(bytes, id);
    return bytes;
}

std::vector<std::uint8_t> fragmentPayload(std::uint64_t generation, std::uint8_t initiator,
                                          std::uint64_t id, std::uint64_t revision,
                                          std::uint64_t sequence, std::uint32_t width,
                                          std::uint32_t height, std::uint8_t key_frame,
                                          std::uint32_t total, std::uint32_t offset,
                                          const std::vector<std::uint8_t>& data) {
    std::vector<std::uint8_t> payload = refBytes(generation, initiator, id);
    put64(payload, revision);
    put64(payload, sequence);
    put32(payload, width);
    put32(payload, height);
    payload.push_back(key_frame);
    put32(payload, total);
    put32(payload, offset);
    payload.insert(payload.end(), data.begin(), data.end());
    return payload;
}

DecodeResult feedRaw(const std::vector<std::uint8_t>& bytes,
                     GenerationPair generations = receiver_generations,
                     FeatureFrameDecoder* existing = nullptr) {
    FeatureFrameDecoder local(generations);
    FeatureFrameDecoder& decoder = existing == nullptr ? local : *existing;
    return decoder.feed(bytes.data(), bytes.size());
}

std::vector<std::uint8_t> jpegLikeBytes(std::size_t size, std::uint8_t seed = 0x5A) {
    std::vector<std::uint8_t> bytes(size);
    for (std::size_t index = 0; index < size; ++index) {
        bytes[index] = static_cast<std::uint8_t>((index * 31 + seed) & 0xFFu);
    }
    return bytes;
}

VisualEncodedFrame makeFrame(std::size_t size = 128, std::uint32_t width = 320,
                             std::uint32_t height = 200) {
    VisualEncodedFrame frame;
    frame.ref = localRef();
    frame.revision = 1;
    frame.sequence = 1;
    frame.size = VisualSize{width, height};
    frame.key_frame = true;
    frame.bytes = jpegLikeBytes(size);
    return frame;
}

} // namespace

TEST_CASE(roundtrip_all_seven_message_types) {
    const auto offer_monitor = singleDecode(VisualShareOffer{localRef(3), VisualSourceKind::monitor});
    const auto* offer = std::get_if<VisualShareOffer>(&offer_monitor);
    CHECK(offer != nullptr);
    CHECK_EQ(offer->ref.generation, static_cast<SessionGeneration>(22));
    CHECK_EQ(offer->ref.initiator, VisualInitiator::peer);
    CHECK_EQ(offer->ref.offer_id, static_cast<std::uint64_t>(3));
    CHECK_EQ(offer->source, VisualSourceKind::monitor);

    const auto offer_window =
        singleDecode(VisualShareOffer{localRef(4), VisualSourceKind::window});
    CHECK_EQ(std::get<VisualShareOffer>(offer_window).source, VisualSourceKind::window);

    const auto decision =
        singleDecode(VisualDecisionMessage{localRef(5), VisualDecision::accepted});
    CHECK_EQ(std::get<VisualDecisionMessage>(decision).decision, VisualDecision::accepted);
    CHECK_EQ(std::get<VisualDecisionMessage>(decision).ref.initiator, VisualInitiator::peer);

    const auto state = singleDecode(VisualStateMessage{localRef(6), 2, VisualSourceState::suspended});
    CHECK_EQ(std::get<VisualStateMessage>(state).revision, static_cast<VisualSourceRevision>(2));
    CHECK_EQ(std::get<VisualStateMessage>(state).state, VisualSourceState::suspended);

    const auto activity = singleDecode(VisualActivityMessage{localRef(7), 9});
    CHECK_EQ(std::get<VisualActivityMessage>(activity).revision,
             static_cast<VisualSourceRevision>(9));

    const auto stop = singleDecode(VisualStopMessage{localRef(8)});
    CHECK_EQ(std::get<VisualStopMessage>(stop).ref.offer_id, static_cast<std::uint64_t>(8));

    const auto frame_message = singleDecode(makeFrame());
    const auto& frame = std::get<VisualEncodedFrame>(frame_message);
    CHECK_EQ(frame.ref.generation, static_cast<SessionGeneration>(22));
    CHECK_EQ(frame.ref.initiator, VisualInitiator::peer);
    CHECK_EQ(frame.revision, static_cast<VisualSourceRevision>(1));
    CHECK_EQ(frame.sequence, static_cast<std::uint64_t>(1));
    CHECK_EQ(frame.size.width, static_cast<std::uint32_t>(320));
    CHECK_EQ(frame.size.height, static_cast<std::uint32_t>(200));
    CHECK(frame.key_frame);
    CHECK_EQ(frame.bytes.size(), static_cast<std::size_t>(128));

    RemoteNotification manual;
    manual.ref = NotificationRef{11, 77};
    manual.mode = NotificationMode::manual;
    manual.content = NotificationContent{"Título", "Cuerpo del aviso"};
    manual.app_name.clear();
    const auto manual_message = singleDecode(manual);
    const auto& received_manual = std::get<RemoteNotification>(manual_message);
    CHECK_EQ(received_manual.ref.generation, static_cast<SessionGeneration>(22));
    CHECK_EQ(received_manual.ref.notice_id, static_cast<std::uint64_t>(77));
    CHECK_EQ(received_manual.mode, NotificationMode::manual);
    CHECK_EQ(received_manual.content.title, std::string("Título"));
    CHECK_EQ(received_manual.content.body, std::string("Cuerpo del aviso"));
    CHECK(received_manual.app_name.empty());

    RemoteNotification forwarded;
    forwarded.ref = NotificationRef{11, 78};
    forwarded.mode = NotificationMode::forwarded;
    forwarded.content = NotificationContent{"App", "Nuevo mensaje"};
    forwarded.app_name = "correo";
    const auto forwarded_message = singleDecode(forwarded);
    const auto& received_forwarded = std::get<RemoteNotification>(forwarded_message);
    CHECK_EQ(received_forwarded.mode, NotificationMode::forwarded);
    CHECK_EQ(received_forwarded.app_name, std::string("correo"));
}

TEST_CASE(initiator_and_generation_translation_is_reversible) {
    // Oferta local de A: el receptor la atribuye al par y a su generación local.
    VisualShareOffer offer{localRef(41, 900), VisualSourceKind::monitor};
    GenerationPair a{900, 700}; // A local 900, conoce B 700
    GenerationPair b{700, 900}; // B local 700, conoce A 900
    const std::vector<WireFrame> offered = encodeOk(offer, a);
    const std::vector<FeatureMessage> received = decodeOk(offered, b);
    CHECK_EQ(received.size(), static_cast<std::size_t>(1));
    const auto& at_b = std::get<VisualShareOffer>(received.front());
    CHECK_EQ(at_b.ref.generation, static_cast<SessionGeneration>(700));
    CHECK_EQ(at_b.ref.initiator, VisualInitiator::peer);

    // Respuesta de B sobre la misma identidad de operación.
    VisualDecisionMessage response{at_b.ref, VisualDecision::accepted};
    const std::vector<WireFrame> answered = encodeOk(response, b);
    const std::vector<FeatureMessage> back = decodeOk(answered, a);
    CHECK_EQ(back.size(), static_cast<std::size_t>(1));
    const auto& at_a = std::get<VisualDecisionMessage>(back.front());
    CHECK_EQ(at_a.ref.generation, static_cast<SessionGeneration>(900));
    CHECK_EQ(at_a.ref.initiator, VisualInitiator::local);
    CHECK_EQ(at_a.ref.offer_id, static_cast<std::uint64_t>(41));
    CHECK_EQ(at_a.decision, VisualDecision::accepted);
}

TEST_CASE(frame_is_split_and_reassembled_byte_by_byte) {
    VisualEncodedFrame frame = makeFrame(max_fragment_data_bytes * 2 + 123, 1280, 720);
    frame.ref = localRef(12);
    frame.sequence = 9;
    const std::vector<WireFrame> frames = encodeOk(frame);
    CHECK_EQ(frames.size(), static_cast<std::size_t>(3));
    for (const WireFrame& wire : frames) {
        CHECK_EQ(wire.channel, ProtocolChannel::data);
        const std::size_t payload_size = wire.bytes.size() - feature_frame_header_bytes;
        CHECK(payload_size <= visual_fragment_payload_bytes);
        CHECK(payload_size > visual_fragment_metadata_bytes);
    }

    FeatureFrameDecoder decoder(receiver_generations);
    DecodeResult result;
    std::vector<std::uint8_t> all;
    for (const WireFrame& wire : frames) {
        all.insert(all.end(), wire.bytes.begin(), wire.bytes.end());
    }
    for (std::size_t index = 0; index < all.size(); ++index) {
        result = decoder.feed(&all[index], 1);
        if (index + 1 < all.size()) {
            CHECK(result.status == ProtocolStatus::ok ||
                  result.status == ProtocolStatus::needs_more);
            if (index + 1 < feature_frame_header_bytes) {
                CHECK_EQ(result.status, ProtocolStatus::needs_more);
            }
        }
    }
    CHECK_EQ(result.status, ProtocolStatus::ok);
    CHECK(result.message.has_value());
    const auto& decoded = std::get<VisualEncodedFrame>(*result.message);
    CHECK(decoded.bytes == frame.bytes);
    CHECK_EQ(decoded.ref.generation, static_cast<SessionGeneration>(22));
    CHECK_EQ(decoded.ref.initiator, VisualInitiator::peer);
    CHECK_EQ(decoded.ref.offer_id, static_cast<std::uint64_t>(12));
    CHECK_EQ(decoded.sequence, static_cast<std::uint64_t>(9));
    CHECK_EQ(decoded.size.width, static_cast<std::uint32_t>(1280));
    CHECK(decoded.key_frame);
}

TEST_CASE(two_messages_in_one_buffer_are_delivered_in_order) {
    const auto stop = encodeOk(VisualStopMessage{localRef(1)}).front();
    const auto activity = encodeOk(VisualActivityMessage{localRef(2), 4}).front();
    std::vector<std::uint8_t> joined = stop.bytes;
    joined.insert(joined.end(), activity.bytes.begin(), activity.bytes.end());

    FeatureFrameDecoder decoder(receiver_generations);
    const DecodeResult first = decoder.feed(joined.data(), joined.size());
    CHECK_EQ(first.status, ProtocolStatus::ok);
    CHECK(first.message.has_value());
    CHECK(std::holds_alternative<VisualStopMessage>(*first.message));

    const DecodeResult second = decoder.feed(nullptr, 0);
    CHECK_EQ(second.status, ProtocolStatus::ok);
    CHECK(second.message.has_value());
    CHECK(std::holds_alternative<VisualActivityMessage>(*second.message));
}

TEST_CASE(protocol_rejects_bad_versions_types_and_lengths) {
    const auto stop = encodeOk(VisualStopMessage{localRef(1)}).front();
    std::vector<std::uint8_t> wrong_version = stop.bytes;
    wrong_version[0] = 0;
    wrong_version[1] = 2;
    CHECK_EQ(feedRaw(wrong_version).status, ProtocolStatus::version_unsupported);

    std::vector<std::uint8_t> unknown_type = stop.bytes;
    unknown_type[2] = 0;
    unknown_type[3] = 9;
    CHECK_EQ(feedRaw(unknown_type).status, ProtocolStatus::unknown_type);

    std::vector<std::uint8_t> oversized_interactive;
    put16(oversized_interactive, 1);
    put16(oversized_interactive, 1);
    put32(oversized_interactive, static_cast<std::uint32_t>(max_interactive_payload_bytes + 1));
    const auto oversized = feedRaw(oversized_interactive);
    CHECK_EQ(oversized.status, ProtocolStatus::payload_too_large);

    std::vector<std::uint8_t> oversized_fragment = refBytes(11, 0, 1);
    put64(oversized_fragment, 1);
    put64(oversized_fragment, 1);
    put32(oversized_fragment, 10);
    put32(oversized_fragment, 10);
    oversized_fragment.push_back(1);
    put32(oversized_fragment, 10);
    put32(oversized_fragment, 0);
    oversized_fragment.resize(visual_fragment_payload_bytes + 1, 0);
    CHECK_EQ(feedRaw(rawFrame(6, oversized_fragment)).status, ProtocolStatus::payload_too_large);

    // Payload declarado mayor que el disponible: espera más bytes, no reserva.
    std::vector<std::uint8_t> truncated = stop.bytes;
    truncated.pop_back();
    CHECK_EQ(feedRaw(truncated).status, ProtocolStatus::needs_more);

    // Bytes sobrantes dentro de una trama completa.
    std::vector<std::uint8_t> offer_payload = refBytes(11, 0, 1);
    offer_payload.push_back(0);
    offer_payload.push_back(0); // sobrante
    CHECK_EQ(feedRaw(rawFrame(1, offer_payload)).status, ProtocolStatus::malformed);

    // Enumeraciones y booleanos fuera de rango.
    std::vector<std::uint8_t> bad_decision_payload = refBytes(11, 0, 2);
    bad_decision_payload.push_back(3);
    CHECK_EQ(feedRaw(rawFrame(2, bad_decision_payload)).status, ProtocolStatus::malformed);

    std::vector<std::uint8_t> bad_frame_payload =
        fragmentPayload(11, 0, 3, 1, 1, 8, 8, 2, 4, 0, {1, 2, 3, 4});
    CHECK_EQ(feedRaw(rawFrame(6, bad_frame_payload)).status, ProtocolStatus::out_of_range);
}

TEST_CASE(protocol_rejects_generation_and_initiator_mismatches) {
    const auto offer = encodeOk(VisualShareOffer{localRef(1), VisualSourceKind::monitor}).front();
    // El receptor espera la generación 99 del par, pero llega 11.
    CHECK_EQ(feedRaw(offer.bytes, GenerationPair{22, 99}).status,
             ProtocolStatus::generation_mismatch);

    std::vector<std::uint8_t> offer_payload = refBytes(11, 0, 1);
    offer_payload.push_back(0);
    std::fill(offer_payload.begin(), offer_payload.begin() + 8, 0); // generación 0
    CHECK_EQ(feedRaw(rawFrame(1, offer_payload)).status, ProtocolStatus::generation_mismatch);

    std::vector<std::uint8_t> bad_initiator = refBytes(11, 2, 1);
    bad_initiator.push_back(0);
    CHECK_EQ(feedRaw(rawFrame(1, bad_initiator)).status, ProtocolStatus::malformed);
}

TEST_CASE(protocol_enforces_visual_limits) {
    VisualEncodedFrame frame = makeFrame();
    frame.size = VisualSize{1921, 10};
    CHECK_EQ(encodeFeatureMessage(frame, sender_generations, nullptr).size(),
             static_cast<std::size_t>(0));
    ProtocolStatus error = ProtocolStatus::ok;
    encodeFeatureMessage(frame, sender_generations, &error);
    CHECK_EQ(error, ProtocolStatus::out_of_range);

    frame.size = VisualSize{1920, 1081}; // 2 075 520 píxeles > 2 073 600
    encodeFeatureMessage(frame, sender_generations, &error);
    CHECK_EQ(error, ProtocolStatus::out_of_range);

    frame.size = VisualSize{0, 10};
    encodeFeatureMessage(frame, sender_generations, &error);
    CHECK_EQ(error, ProtocolStatus::out_of_range);

    frame.size = VisualSize{10, 10};
    frame.key_frame = false;
    encodeFeatureMessage(frame, sender_generations, &error);
    CHECK_EQ(error, ProtocolStatus::out_of_range);

    frame.key_frame = true;
    frame.sequence = 0;
    encodeFeatureMessage(frame, sender_generations, &error);
    CHECK_EQ(error, ProtocolStatus::out_of_range);

    frame.sequence = 1;
    frame.bytes.assign(max_visual_encoded_bytes + 1, 1);
    encodeFeatureMessage(frame, sender_generations, &error);
    CHECK_EQ(error, ProtocolStatus::out_of_range);

    frame.bytes.clear();
    encodeFeatureMessage(frame, sender_generations, &error);
    CHECK_EQ(error, ProtocolStatus::out_of_range);
}

TEST_CASE(protocol_rejects_bad_notices) {
    RemoteNotification notice;
    notice.ref = NotificationRef{11, 1};
    notice.mode = NotificationMode::manual;
    notice.content.title.assign(max_notification_title_bytes + 1, 'x');
    ProtocolStatus error = ProtocolStatus::ok;
    encodeFeatureMessage(notice, sender_generations, &error);
    CHECK_EQ(error, ProtocolStatus::out_of_range);

    notice.content.title.clear();
    notice.content.body.assign(max_notification_body_bytes + 1, 'x');
    encodeFeatureMessage(notice, sender_generations, &error);
    CHECK_EQ(error, ProtocolStatus::out_of_range);

    notice.content.body.clear();
    notice.app_name.assign(max_notification_app_name_bytes + 1, 'x');
    encodeFeatureMessage(notice, sender_generations, &error);
    CHECK_EQ(error, ProtocolStatus::out_of_range);

    notice.app_name.clear();
    notice.content.title = std::string("\xC3\x28", 2); // UTF-8 inválido
    encodeFeatureMessage(notice, sender_generations, &error);
    CHECK_EQ(error, ProtocolStatus::out_of_range);

    notice.content.title = "ok";
    notice.app_name = "app";
    encodeFeatureMessage(notice, sender_generations, &error);
    CHECK_EQ(error, ProtocolStatus::out_of_range); // manual con app_name

    notice.mode = NotificationMode::forwarded;
    const auto frames = encodeFeatureMessage(notice, sender_generations, &error);
    CHECK_EQ(error, ProtocolStatus::ok);
    CHECK_EQ(frames.size(), static_cast<std::size_t>(1));
    const auto decoded = decodeOk(frames);
    CHECK_EQ(std::get<RemoteNotification>(decoded.front()).app_name, std::string("app"));
}

TEST_CASE(fragment_reassembly_rejects_gaps_and_metadata_changes) {
    const std::vector<std::uint8_t> part1(10, 0x11);
    const std::vector<std::uint8_t> part2(10, 0x22);
    FeatureFrameDecoder decoder(receiver_generations);

    const auto first = feedRaw(
        rawFrame(6, fragmentPayload(11, 0, 5, 1, 1, 100, 50, 1, 20, 0, part1)),
        receiver_generations, &decoder);
    CHECK_EQ(first.status, ProtocolStatus::ok);
    CHECK(!first.message.has_value());
    CHECK(decoder.hasAssembly());

    const auto gap = feedRaw(
        rawFrame(6, fragmentPayload(11, 0, 5, 1, 1, 100, 50, 1, 20, 15, part2)),
        receiver_generations, &decoder);
    CHECK_EQ(gap.status, ProtocolStatus::out_of_range);

    FeatureFrameDecoder changed_metadata(receiver_generations);
    const auto begun = feedRaw(
        rawFrame(6, fragmentPayload(11, 0, 6, 1, 1, 100, 50, 1, 20, 0, part1)),
        receiver_generations, &changed_metadata);
    CHECK_EQ(begun.status, ProtocolStatus::ok);
    const auto mismatch = feedRaw(
        rawFrame(6, fragmentPayload(11, 0, 6, 2, 1, 100, 50, 1, 20, 10, part2)),
        receiver_generations, &changed_metadata);
    CHECK_EQ(mismatch.status, ProtocolStatus::assembly_mismatch);

    FeatureFrameDecoder other_operation(receiver_generations);
    feedRaw(rawFrame(6, fragmentPayload(11, 0, 7, 1, 1, 100, 50, 1, 20, 0, part1)),
            receiver_generations, &other_operation);
    const auto interleaved = feedRaw(
        rawFrame(6, fragmentPayload(11, 0, 8, 1, 1, 100, 50, 1, 20, 10, part2)),
        receiver_generations, &other_operation);
    CHECK_EQ(interleaved.status, ProtocolStatus::assembly_mismatch);
}

TEST_CASE(fragment_reassembly_rejects_out_of_range_metadata) {
    const std::vector<std::uint8_t> data(10, 0x33);
    CHECK_EQ(feedRaw(rawFrame(6, fragmentPayload(11, 0, 1, 0, 1, 10, 10, 1, 10, 0, data)))
                 .status,
             ProtocolStatus::out_of_range);
    CHECK_EQ(feedRaw(rawFrame(6, fragmentPayload(11, 0, 1, 1, 0, 10, 10, 1, 10, 0, data)))
                 .status,
             ProtocolStatus::out_of_range);
    CHECK_EQ(feedRaw(rawFrame(6, fragmentPayload(11, 0, 1, 1, 1, 0, 10, 1, 10, 0, data)))
                 .status,
             ProtocolStatus::out_of_range);
    CHECK_EQ(feedRaw(rawFrame(6, fragmentPayload(11, 0, 1, 1, 1, 10, 10, 1, 0, 0, data)))
                 .status,
             ProtocolStatus::out_of_range);
    CHECK_EQ(feedRaw(rawFrame(6, fragmentPayload(11, 0, 1, 1, 1, 10, 10, 1, 10, 5, data)))
                 .status,
             ProtocolStatus::out_of_range);
    CHECK_EQ(feedRaw(rawFrame(6, fragmentPayload(11, 0, 1, 1, 1, 10, 10, 1,
                                                 max_visual_encoded_bytes + 1, 0, data)))
                 .status,
             ProtocolStatus::out_of_range);
    CHECK_EQ(feedRaw(rawFrame(6, fragmentPayload(11, 0, 1, 1, 1, 10, 10, 1, 10, 0, {})))
                 .status,
             ProtocolStatus::out_of_range);
}

TEST_CASE(decoder_reset_discards_partial_assembly) {
    FeatureFrameDecoder decoder(receiver_generations);
    const std::vector<std::uint8_t> data(5, 0x44);
    const auto partial = feedRaw(
        rawFrame(6, fragmentPayload(11, 0, 1, 1, 1, 10, 10, 1, 10, 0, data)),
        receiver_generations, &decoder);
    CHECK_EQ(partial.status, ProtocolStatus::ok);
    CHECK(decoder.hasAssembly());
    decoder.reset();
    CHECK(!decoder.hasAssembly());
}

int main() {
    return oneintwo_test::run();
}
