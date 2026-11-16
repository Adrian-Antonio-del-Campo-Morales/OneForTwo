// Responsibility: Serialization with explicit lengths and verification of
// limits. No remote pair field is used to reserve memory without limit.
#include "core/wire.hpp"

#include <algorithm>
#include <cstring>

#include "core/stream.hpp"

namespace o2f {

void ByteWriter::u8(std::uint8_t v) { data_.push_back(v); }
void ByteWriter::u32(std::uint32_t v) {
    for (int i = 0; i < 4; ++i) data_.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xffu));
}
void ByteWriter::u64(std::uint64_t v) {
    for (int i = 0; i < 8; ++i) data_.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xffu));
}
void ByteWriter::str(const std::string& v) {
    if (v.size() > 0xffffffffull) throw Error("too long string for protocol");
    u32(static_cast<std::uint32_t>(v.size()));
    bytes(v.data(), v.size());
}
void ByteWriter::bytes(const void* data, std::size_t len) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    data_.insert(data_.end(), p, p + len);
}

ByteReader::ByteReader(const void* data, std::size_t len)
    : data_(static_cast<const std::uint8_t*>(data)), len_(len), pos_(0) {}

std::uint8_t ByteReader::u8() {
    if (pos_ + 1 > len_) throw Error("truncated message");
    return data_[pos_++];
}
std::uint32_t ByteReader::u32() {
    if (pos_ + 4 > len_) throw Error("truncated message");
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(data_[pos_++]) << (8 * i);
    return v;
}
std::uint64_t ByteReader::u64() {
    if (pos_ + 8 > len_) throw Error("truncated message");
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(data_[pos_++]) << (8 * i);
    return v;
}
std::string ByteReader::str(std::size_t maxLen) {
    const std::uint32_t len = u32();
    if (len > maxLen) throw Error("off-limit protocol string");
    if (pos_ + len > len_) throw Error("string truncated in message");
    std::string out(reinterpret_cast<const char*>(data_ + pos_), len);
    pos_ += len;
    return out;
}
void ByteReader::bytes(void* out, std::size_t len) {
    if (pos_ + len > len_) throw Error("bytes truncated in the message");
    std::memcpy(out, data_ + pos_, len);
    pos_ += len;
}

void writeFrame(Stream& stream, MsgType type, const std::vector<std::uint8_t>& payload) {
    const std::size_t bodyLen = 1 + payload.size();
    if (bodyLen > kMaxFrameBytes) throw Error("too big protocol plot");
    std::uint8_t header[5];
    const auto len = static_cast<std::uint32_t>(bodyLen);
    for (int i = 0; i < 4; ++i) header[i] = static_cast<std::uint8_t>((len >> (8 * i)) & 0xffu);
    header[4] = static_cast<std::uint8_t>(type);
    stream.writeAll(header, sizeof(header));
    if (!payload.empty()) stream.writeAll(payload.data(), payload.size());
}

void writeFrame(Stream& stream, MsgType type) { writeFrame(stream, type, {}); }

MsgType readFrame(Stream& stream, std::vector<std::uint8_t>& payload) {
    std::uint8_t header[4];
    readExact(stream, header, sizeof(header));
    const std::uint32_t bodyLen = static_cast<std::uint32_t>(header[0]) |
                                  (static_cast<std::uint32_t>(header[1]) << 8) |
                                  (static_cast<std::uint32_t>(header[2]) << 16) |
                                  (static_cast<std::uint32_t>(header[3]) << 24);
    if (bodyLen == 0) throw Error("empty protocol plot");
    if (bodyLen > kMaxFrameBytes) throw Error("Out-of-limit protocol plot");
    std::uint8_t typeByte = 0;
    readExact(stream, &typeByte, 1);
    payload.assign(bodyLen - 1, 0);
    if (!payload.empty()) readExact(stream, payload.data(), payload.size());
    const auto type = static_cast<MsgType>(typeByte);
    switch (type) {
        case MsgType::Hello:
        case MsgType::HelloOk:
        case MsgType::List:
        case MsgType::Manifest:
        case MsgType::Get:
        case MsgType::Read:
        case MsgType::Data:
        case MsgType::FileEnd:
        case MsgType::Bye:
        case MsgType::Error:
            return type;
    }
    throw Error("unknown message type in protocol");
}

const char* msgTypeName(MsgType type) {
    switch (type) {
        case MsgType::Hello: return "HELLO";
        case MsgType::HelloOk: return "HELLO_OK";
        case MsgType::List: return "LIST";
        case MsgType::Manifest: return "MANIFEST";
        case MsgType::Get: return "GET";
        case MsgType::Read: return "READ";
        case MsgType::Data: return "DATA";
        case MsgType::FileEnd: return "FILE_END";
        case MsgType::Bye: return "BYE";
        case MsgType::Error: return "ERROR";
    }
    return "UNKNOWN";
}

std::vector<std::uint8_t> encodeOffer(const Offer& offer) {
    ByteWriter w;
    w.str(offer.id);
    w.str(offer.label);
    w.u64(offer.createdAt);
    w.u32(static_cast<std::uint32_t>(offer.entries.size()));
    for (const Entry& e : offer.entries) {
        w.str(e.id);
        w.str(e.relPath);
        w.u8(e.isDir ? 1 : 0);
        w.u64(e.size);
    }
    return w.take();
}

Offer decodeOffer(const void* data, std::size_t len, const Limits& limits) {
    if (len > limits.maxManifestBytes) throw Error("out-of-limit manifest");
    ByteReader r(data, len);
    Offer offer;
    offer.id = r.str(128);
    offer.label = r.str(256);
    offer.createdAt = r.u64();
    if (!isValidOpaqueId(offer.id, 64)) throw Error("Invalid offer identifier");
    const std::uint32_t count = r.u32();
    if (count > limits.maxEntries) throw Error("manifest with too many elements");
    const PathRules rules = nativePathRules(limits);
    offer.entries.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        Entry e;
        e.id = r.str(128);
        e.relPath = r.str(limits.maxRelPathBytes);
        const std::uint8_t dir = r.u8();
        if (dir > 1) throw Error("type of invalid element");
        e.isDir = dir == 1;
        e.size = r.u64();
        if (!isValidOpaqueId(e.id, 64)) throw Error("Invalid item identifier");
        if (e.isDir && e.size != 0) throw Error("folder with size other than zero");
        e.relPath = validateRelativePath(e.relPath, rules);
        if (offer.find(e.id) != nullptr) throw Error("identificador duplicado");
        for (const Entry& prev : offer.entries) {
            if (prev.relPath == e.relPath) throw Error("duplicate path");
        }
        if (!e.isDir) {
            offer.totalBytes += e.size;
            if (offer.totalBytes > limits.maxTotalBytes) throw Error("total size out of limit");
        }
        offer.entries.push_back(std::move(e));
    }
    if (r.remaining() != 0) throw Error("manifest with excess data");
    return offer;
}

std::vector<std::uint8_t> encodeError(const std::string& message) {
    ByteWriter w;
    std::string truncated = message.substr(0, 480);
    w.str(truncated);
    return w.take();
}

std::vector<std::uint8_t> encodeEntryRef(const std::string& entryId) {
    ByteWriter w;
    w.str(entryId);
    return w.take();
}

}  // namespace o2f
