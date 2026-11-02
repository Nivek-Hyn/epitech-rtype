# R-Type

A networked multiplayer remake of the classic shoot'em up R-Type, built on our own C++ game engine.
Up to 4 players connect to an authoritative server and fight waves of Bydos together.

This site is the developer documentation: it explains how the project is organized, why it is built this way,
and how to extend it. Build and run instructions are in the
[README on GitHub](https://github.com/EpitechPGE3-2026/G-ING-500-MAR-5-1-rtype-5#readme).

## Where to start

| You want to... | Read |
|---|---|
| Build, run, install or package the game | [Build and run](build-and-run.md) |
| Understand the big picture | [Architecture overview](architecture.md) |
| Use or extend the engine (ECS, game loop, events) | [Engine](engine.md) |
| Know why we chose each technology | [Technical and comparative study](technical-study.md) and the [decision records](adr/README.md) |
| Write a new client or change the protocol | [Network protocol](protocol.md) |
| Add a component, a system or an enemy | [How-to guides](how-to.md) |
| Understand component fields and network ownership | [Shared data](shared-data.md) |
| Check what has actually been tested on Linux and Windows | [Validation record](validation.md) |
| Prepare delivery checks and the defense | [Delivery and defense guide](delivery.md) |
| Look up a class or a function | [API reference](html/index.html), generated from source comments with Doxygen (see below) |
| Know what we do for players with disabilities | [Accessibility study](accessibility.md) |
| Contribute | [Contributing guide on GitHub](https://github.com/EpitechPGE3-2026/G-ING-500-MAR-5-1-rtype-5/blob/main/CONTRIBUTING.md) and the [documentation guidelines](documentation-guidelines.md) |

## Read this documentation

- **On GitHub**: every page of `docs/` is Markdown, rendered directly by GitHub with its diagrams, links and outline.
- **As a website**, with navigation, search, dark mode and the API reference: generate it locally with the commands
  below. It is not deployed automatically, because publishing it would need GitHub Actions minutes.

Install Doxygen and Graphviz (`doxygen` and `dot` on PATH), and use Python 3.9+ with the pinned packages
in `docs/requirements.txt`. They are documentation tools, not game runtime dependencies.

On Linux, from the repository root:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r docs/requirements.txt
.venv/bin/python scripts/build_docs.py
.venv/bin/python -m http.server 8000 --bind 127.0.0.1 --directory build/documentation-site
```

In Windows PowerShell, use the virtual environment's Python directly; activation is not required:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r docs/requirements.txt
.\.venv\Scripts\python.exe scripts/build_docs.py
.\.venv\Scripts\python.exe -m http.server 8000 --bind 127.0.0.1 --directory build/documentation-site
```

The build script can be called from another working directory using its absolute path. It checks for
tools, builds Doxygen first and then runs MkDocs in strict mode. Missing tools or a failed stage return
a nonzero status. Doxygen warnings are errors; the script does not install tools or publish anything.
Native Windows execution of this documentation script is not part of the existing game validation.

Open `http://127.0.0.1:8000`. Serve the directory rather than opening HTML with `file://`: navigation
uses directory URLs such as `html/`, resolved by the server to `html/index.html`. Stop the local server
with Ctrl+C when finished.

The site includes `docs/html/` with API class pages, diagrams, styles and scripts. Generated HTML is ignored
by Git. After changing public interfaces/comments, rerun the build and check both guides and API pages.
For live Markdown editing, generate the API first, then run the environment's `python -m mkdocs serve`.
