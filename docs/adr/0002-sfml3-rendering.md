# 0002 — SFML 3 for rendering, audio and input

**Status:** accepted

## Context
The client needs a 2D window, sprites, text, audio and keyboard input. Full game engines are forbidden.

## Options
| Library | Pros | Cons |
|---|---|---|
| **SFML 3** | C++ API (RAII, value types), graphics + audio + input in one library, known by the team, easy to fetch with CPM | Less low-level control than SDL |
| Raylib | Very simple, little code to get started | C-style API with global state, needs more wrapping to fit a decoupled engine |
| SDL3 | Very portable, industry standard | Verbose, needs extra libraries for images, fonts, audio mixing |

## Decision
SFML 3. In our ECS architecture the graphics library is only used by a few client systems (render, input,
audio), so the main criteria were C++ integration, completeness for 2D, and team familiarity.

## Consequences
- SFML must only be included in `client/`, never in `engine/`, `common/` or `server/`.
- On Linux, SFML needs a few system development packages (X11, OpenGL, udev): see the README.
