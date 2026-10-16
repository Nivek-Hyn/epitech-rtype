# R-Type

A networked multiplayer remake of the classic shoot'em up R-Type, built on our own C++ game engine.
Up to 4 players connect to an authoritative server and fight waves of Bydos together.

This site is the developer documentation: it explains how the project is organized, why it is built this way,
and how to extend it. Build and run instructions are in the
[README on GitHub](https://github.com/EpitechPGE3-2026/G-ING-500-MAR-5-1-rtype-5#readme).

## Where to start

| You want to... | Read |
|---|---|
| Understand the big picture | [Architecture overview](architecture.md) |
| Use or extend the engine (ECS, game loop, events) | [Engine](engine.md) |
| Know why we chose each technology | [Technical and comparative study](technical-study.md) and the [decision records](adr/README.md) |
| Write a new client or change the protocol | [Network protocol](protocol.md) |
| Add a component, a system or an enemy | [How-to guides](how-to.md) |
| Know what we do for players with disabilities | [Accessibility study](accessibility.md) |
| Contribute | [Contributing guide on GitHub](https://github.com/EpitechPGE3-2026/G-ING-500-MAR-5-1-rtype-5/blob/main/CONTRIBUTING.md) and the [documentation guidelines](documentation-guidelines.md) |

## Build this site locally

```sh
python3 -m venv .venv
.venv/bin/pip install -r docs/requirements.txt
.venv/bin/mkdocs serve      # then open http://127.0.0.1:8000
```
