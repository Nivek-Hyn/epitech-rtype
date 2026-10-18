# 0004 — Linux and Windows from day one

**Status:** accepted

## Context
Linux is mandatory. Windows support is recommended by the subject, and is much harder to add late.

## Decision
Server and client must build and run on Linux and Windows, with cross-play between them.

## Consequences
- Only portable code: standard library, SFML, Asio. No `<unistd.h>`, `<sys/socket.h>`, etc.
- Network data is serialized field by field in big-endian (see 0008), never as raw structs.
- The CI workflow builds on both Linux and Windows (run on demand, as our Actions quota is used up).
