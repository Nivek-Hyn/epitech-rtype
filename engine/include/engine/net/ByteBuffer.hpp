#pragma once

#include <bit>
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
    void u8(std::uint8_t value) { _data.push_back(value); }
    void u16(std::uint16_t value)
    {
        u8(static_cast<std::uint8_t>(value >> 8));
        u8(static_cast<std::uint8_t>(value));
    }
    void u32(std::uint32_t value)
    {
        u8(static_cast<std::uint8_t>(value >> 24));
        u8(static_cast<std::uint8_t>(value >> 16));
        u8(static_cast<std::uint8_t>(value >> 8));
        u8(static_cast<std::uint8_t>(value));
    }
    void f32(float value) { u32(std::bit_cast<std::uint32_t>(value)); }

    std::size_t size() const { return _data.size(); }
    std::vector<std::uint8_t> &data() { return _data; }

private:
    std::vector<std::uint8_t> _data;
};

// Reads values written by ByteWriter.
// Reading past the end must never overflow: return 0 and mark the reader as failed, so ok() can be checked once at the
// end.
class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> bytes) : _bytes(bytes) {}

    std::uint8_t u8()
    {
        if (!canRead(1))
            return 0;
        return _bytes[_offset++];
    }
    std::uint16_t u16()
    {
        if (!canRead(2))
            return 0;
        const std::uint16_t value =
            static_cast<std::uint16_t>(_bytes[_offset]) << 8 | static_cast<std::uint16_t>(_bytes[_offset + 1]);
        _offset += 2;
        return value;
    }
    std::uint32_t u32()
    {
        if (!canRead(4))
            return 0;
        const std::uint32_t value =
            static_cast<std::uint32_t>(_bytes[_offset]) << 24 | static_cast<std::uint32_t>(_bytes[_offset + 1]) << 16 |
            static_cast<std::uint32_t>(_bytes[_offset + 2]) << 8 | static_cast<std::uint32_t>(_bytes[_offset + 3]);
        _offset += 4;
        return value;
    }
    float f32() { return std::bit_cast<float>(u32()); }

    bool ok() const { return !_failed; }
    std::size_t remaining() const { return _bytes.size() - _offset; }

private:
    bool canRead(std::size_t count)
    {
        if (_failed || count > remaining()) {
            _failed = true;
            return false;
        }
        return true;
    }

    std::span<const std::uint8_t> _bytes;
    std::size_t _offset = 0;
    bool _failed = false;
};

} // namespace engine::net
