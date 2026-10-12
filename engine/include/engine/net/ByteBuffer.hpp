#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace engine::net {

// Writes values in network byte order (big-endian), field by field.
// Never memcpy whole structs: padding and endianness differ between compilers/OSes (see docs/adr/0008).
class ByteWriter {
public:
    // TODO(network): append value as 1 / 2 / 4 bytes, most significant byte first.
    void u8(std::uint8_t /* value */) { throw std::logic_error("ByteWriter::u8 not implemented"); }
    void u16(std::uint16_t /* value */) { throw std::logic_error("ByteWriter::u16 not implemented"); }
    void u32(std::uint32_t /* value */) { throw std::logic_error("ByteWriter::u32 not implemented"); }
    // TODO(network): write the 32 bits of the float as a u32 (hint: std::bit_cast).
    void f32(float /* value */) { throw std::logic_error("ByteWriter::f32 not implemented"); }

    // TODO(network): number of bytes written / the bytes themselves.
    std::size_t size() const { throw std::logic_error("ByteWriter::size not implemented"); }
    std::vector<std::uint8_t> &data() { throw std::logic_error("ByteWriter::data not implemented"); }
};

// Reads values written by ByteWriter.
// Reading past the end must never overflow: return 0 and mark the reader as failed, so ok() can be checked once at the
// end.
class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> /* bytes */)
    {
        throw std::logic_error("ByteReader not implemented");
    }

    // TODO(network): read the next 1 / 2 / 4 bytes (big-endian), or return 0 and fail if there aren't enough.
    std::uint8_t u8() { throw std::logic_error("ByteReader::u8 not implemented"); }
    std::uint16_t u16() { throw std::logic_error("ByteReader::u16 not implemented"); }
    std::uint32_t u32() { throw std::logic_error("ByteReader::u32 not implemented"); }
    float f32() { throw std::logic_error("ByteReader::f32 not implemented"); }

    // TODO(network): false once a read went past the end.
    bool ok() const { throw std::logic_error("ByteReader::ok not implemented"); }
    // TODO(network): number of bytes not read yet.
    std::size_t remaining() const { throw std::logic_error("ByteReader::remaining not implemented"); }
};

} // namespace engine::net
