// Responsabilidad: codificación y validación de tramas y mensajes del protocolo
// mínimo de transferencia. Los tamaños se comprueban antes de reservar memoria.
#pragma once

#include "core/model.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace o2f {

class Stream;

enum class MsgType : std::uint8_t {
    Hello = 1,
    HelloOk = 2,
    List = 3,
    Manifest = 4,
    Get = 5,
    Read = 6,
    Data = 7,
    FileEnd = 8,
    Bye = 9,
    Error = 10,
};

constexpr std::size_t kMaxFrameBytes = 1024u * 1024u;

class ByteWriter {
public:
    void u8(std::uint8_t v);
    void u32(std::uint32_t v);
    void u64(std::uint64_t v);
    void str(const std::string& v);
    void bytes(const void* data, std::size_t len);
    const std::vector<std::uint8_t>& data() const { return data_; }
    std::vector<std::uint8_t> take() { return std::move(data_); }

private:
    std::vector<std::uint8_t> data_;
};

class ByteReader {
public:
    ByteReader(const void* data, std::size_t len);
    std::uint8_t u8();
    std::uint32_t u32();
    std::uint64_t u64();
    std::string str(std::size_t maxLen);
    void bytes(void* out, std::size_t len);
    std::size_t remaining() const { return len_ - pos_; }

private:
    const std::uint8_t* data_;
    std::size_t len_;
    std::size_t pos_;
};

void writeFrame(Stream& stream, MsgType type, const std::vector<std::uint8_t>& payload);
void writeFrame(Stream& stream, MsgType type);
MsgType readFrame(Stream& stream, std::vector<std::uint8_t>& payload);

const char* msgTypeName(MsgType type);

std::vector<std::uint8_t> encodeOffer(const Offer& offer);
Offer decodeOffer(const void* data, std::size_t len, const Limits& limits);
std::vector<std::uint8_t> encodeError(const std::string& message);
std::vector<std::uint8_t> encodeEntryRef(const std::string& entryId);

}  // namespace o2f
