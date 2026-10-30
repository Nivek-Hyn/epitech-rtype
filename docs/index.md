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
| Check what has actually been tested on Linux and Windows | [Validation record](validation.md) |
| Look up a class or a function | [API reference](html/index.html), generated from source comments with Doxygen (see below) |
| Know what we do for players with disabilities | [Accessibility study](accessibility.md) |
| Contribute | [Contributing guide on GitHub](https://github.com/EpitechPGE3-2026/G-ING-500-MAR-5-1-rtype-5/blob/main/CONTRIBUTING.md) and the [documentation guidelines](documentation-guidelines.md) |

## Read this documentation

- **On GitHub**: every page of `docs/` is Markdown, rendered directly by GitHub with its diagrams, links and outline.
- **As a website**, with navigation, search, dark mode and the API reference: generate it locally with the commands
  below. It is not deployed automatically, because publishing it would need GitHub Actions minutes.

```sh
python3 -m venv .venv
.venv/bin/pip install -r docs/requirements.txt
doxygen docs/Doxyfile       # API reference in docs/html (needs Doxygen and Graphviz)
.venv/bin/mkdocs serve      # then open http://127.0.0.1:8000
```

Generate the API before starting MkDocs. The site copies the generated `docs/html/` directory, including
class pages, diagrams, styles and search scripts, under `html/`. Its navigation links to the API home page.
Generated HTML stays ignored by Git; contributors regenerate it from the current source comments.

To build the complete site without running a web server:

```sh
doxygen docs/Doxyfile
.venv/bin/mkdocs build --strict --site-dir build/documentation-site
python3 -m http.server 8000 --bind 127.0.0.1 --directory build/documentation-site
```

Open `http://127.0.0.1:8000`. Serve the directory rather than opening HTML with `file://`: the generated
navigation uses directory URLs such as `html/`, which the local web server resolves to `html/index.html`.
Check both the developer guides and the API reference when changing public interfaces.
