# 0003 — Standalone Asio for networking

**Status:** accepted

## Context
Server and client communicate over UDP. The subject allows Asio or OS sockets wrapped in our own abstractions.

## Options
| Option | Pros | Cons |
|---|---|---|
| **Asio (standalone)** | Cross-platform out of the box, asynchronous I/O, header-only, no Boost | Asynchronous model takes time to learn |
| BSD sockets / Winsock | Full understanding of the system API | Two implementations to write and maintain (Linux / Windows) |

## Decision
Standalone Asio, hidden behind our own interfaces (`INetworkServer`, `INetworkClient`).
Game code never includes Asio directly.

## Consequences
- Only the network implementation files include `<asio.hpp>`.
- Replacing Asio later would only affect those files.
