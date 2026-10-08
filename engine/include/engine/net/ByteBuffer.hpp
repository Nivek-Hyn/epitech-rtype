#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace engine::net {

// Writes values in network byte order (big-endian), field by field.
// We never memcpy whole structs: padding and endianness differ between compilers/OSes.
class ByteWriter {
public:
    void u8(std::uint8_t value) { _buffer.push_back(value); }

    void u16(std::uint16_t value)
    {
        u8(static_cast<std::uint8_t>(value >> 8));
        u8(static_cast<std::uint8_t>(value));
    }

    void u32(std::uint32_t value)
    {
        u16(static_cast<std::uint16_t>(value >> 16));
        u16(static_cast<std::uint16_t>(value));
    }

    void f32(float value) { u32(std::bit_cast<std::uint32_t>(value)); }

    [[nodiscard]] std::size_t size() const { return _buffer.size(); }
    [[nodiscard]] std::vector<std::uint8_t> &data() { return _buffer; }

private:
    std::vector<std::uint8_t> _buffer;
};

// Reads values written by ByteWriter. Reading past the end never overflows:
// it returns 0 and marks the reader as failed, so check ok() once at the end.
class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> data) : _data(data) {}

    std::uint8_t u8()
    {
        if (!canRead(1))
            return 0;
        return _data[_pos++];
    }

    std::uint16_t u16()
    {
        const auto high = u8();
        const auto low = u8();
        return static_cast<std::uint16_t>((high << 8) | low);
    }

    std::uint32_t u32()
    {
        const std::uint32_t high = u16();
        const std::uint32_t low = u16();
        return (high << 16) | low;
    }

    float f32() { return std::bit_cast<float>(u32()); }

    [[nodiscard]] bool ok() const { return !_failed; }
    [[nodiscard]] std::size_t remaining() const { return _data.size() - _pos; }

private:
    bool canRead(std::size_t count)
    {
        if (_failed || remaining() < count) {
            _failed = true;
            return false;
        }
        return true;
    }

    std::span<const std::uint8_t> _data;
    std::size_t _pos = 0;
    bool _failed = false;
};

} // namespace engine::net
