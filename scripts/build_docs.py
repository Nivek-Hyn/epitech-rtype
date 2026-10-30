#!/usr/bin/env python3
"""Build the API reference and the documentation site using installed tools."""

import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    project_root = Path(__file__).resolve().parent.parent
    missing = [name for name in ("doxygen", "dot") if shutil.which(name) is None]
    if importlib.util.find_spec("mkdocs") is None:
        missing.append("mkdocs in this Python environment")
    if missing:
        print("Missing documentation tools: " + ", ".join(missing), file=sys.stderr)
        print("See docs/index.md for setup commands.", file=sys.stderr)
        return 1

    commands = [
        ["doxygen", "docs/Doxyfile"],
        [sys.executable, "-m", "mkdocs", "build", "--strict", "--site-dir", "build/documentation-site"],
    ]
    try:
        for command in commands:
            print("Running: " + " ".join(command), flush=True)
            result = subprocess.run(command, cwd=project_root, check=False)
            if result.returncode != 0:
                return 1
    except OSError as error:
        print(f"Could not run documentation tool: {error}", file=sys.stderr)
        return 1

    site = project_root / "build/documentation-site"
    if not (site / "index.html").is_file() or not (site / "html/index.html").is_file():
        print("Documentation output is missing the site or API home page.", file=sys.stderr)
        return 1
    print(f"Documentation ready in {site}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
