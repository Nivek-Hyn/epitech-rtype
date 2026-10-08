# Contributing

## Workflow

1. Every piece of work starts from a GitHub issue.
2. Create a branch from `main`: `<type>/<issue>-<short-name>`, e.g. `feat/28-udp-socket`, `fix/41-starfield-speed`.
   Types: `feat`, `fix`, `refactor`, `test`, `docs`, `ci`, `chore`.
3. Open a pull request into `main`, with `Closes #<issue>` in the description.
4. At least **one review from someone working on another domain** before merging.
   This keeps everyone aware of the whole architecture.
5. The CI must be green. Merge with "Squash and merge".

Never push directly to `main`.

## Commits

[Conventional Commits](https://www.conventionalcommits.org/): `<type>(<scope>): <summary>`

```
feat(network): add UDP socket wrapper
fix(client): starfield scrolling depends on frame rate
docs(protocol): describe Snapshot splitting
```

Scopes: `engine`, `common`, `server`, `client`, `network`, `gameplay`, `ci`, `docs`.

## Architecture rules

- `engine/` never includes anything from `common/`, `server/` or `client/`.
- SFML is only used in `client/`. Asio is only used in network implementation files.
- Never send an ECS `Entity` over the network: use the `NetworkId` component.
- Any protocol change updates `common/include/common/Protocol.hpp` **and** `docs/protocol.md` in the same PR.
- An important technical decision gets an ADR in `docs/adr/`.

## Code style

- Formatting: `clang-format` with the repository `.clang-format` (run it before committing).
- Classes and structs `PascalCase`, functions and variables `camelCase`, private members `_camelCase`,
  constants `PascalCase`, files named after their main class.
- One namespace per module: `engine::...`, `rtype::protocol`, `rtype::server`, `rtype::client`.
- No warnings: the build uses `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`.
- New logic comes with unit tests in `tests/` when it can be tested without a window or a socket.

## Documentation

All documentation, code comments, commit messages and pull requests are written in **English**.
