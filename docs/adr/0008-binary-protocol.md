# 0008 — Binary protocol design

**Status:** accepted

## Context
The protocol must be binary, use UDP, and never crash or overflow on malformed packets.

## Decision
- Fixed 10-byte header (magic, version, type, sequence, payload size), then a payload whose layout depends on the type.
- All integers are big-endian (network byte order); floats are IEEE-754 32-bit sent as their bit pattern.
- Fields are written one by one (`engine::net::ByteWriter`), never by `memcpy` of structs,
  because struct padding and endianness differ between compilers and platforms.
- A datagram is rejected unless: size between 10 and 1200 bytes, magic and version match,
  `payload size` equals the real remaining size, type is known, and the payload is consumed exactly.
- 1200 bytes max keeps datagrams under the usual Internet MTU (no IP fragmentation).

Full specification: [protocol.md](../protocol.md).

## Consequences
- Decoding returns `std::optional` and never throws: invalid packets are silently dropped.
- Any breaking change bumps `Version`, and must update `docs/protocol.md` in the same pull request.
