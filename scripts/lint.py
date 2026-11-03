#!/usr/bin/env python3
"""Analyze project sources from CMake's compilation database, without editing files."""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build", help="CMake build directory")
    parser.add_argument("--clang-tidy", default=os.environ.get("CLANG_TIDY", "clang-tidy"))
    arguments = parser.parse_args()
    project_root = Path(__file__).resolve().parent.parent
    build_directory = Path(arguments.build_dir).resolve()
    analyzer = shutil.which(arguments.clang_tidy)
    if analyzer is None:
        print("Error: clang-tidy not found. See docs/static-analysis.md.", file=sys.stderr)
        return 1

    try:
        database = json.loads((build_directory / "compile_commands.json").read_text(encoding="utf-8"))
        source_files = set()
        for entry in database:
            source = Path(entry["file"])
            if not source.is_absolute():
                source = Path(entry["directory"]) / source
            source = source.resolve()
            if source.is_relative_to(project_root):
                relative = source.relative_to(project_root)
                if relative.parts[0] in {"engine", "common", "server", "client", "tests"}:
                    source_files.add(source)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"Error reading compilation database: {error}", file=sys.stderr)
        print("Configure CMake with a Makefile or Ninja generator first.", file=sys.stderr)
        return 1
    if not source_files:
        print("Error: no project sources found in the compilation database.", file=sys.stderr)
        return 1

    configuration = f"--config-file={project_root / '.clang-tidy'}"
    result = subprocess.run([analyzer, configuration, "--verify-config"], check=False)
    if result.returncode != 0:
        return 1

    def analyze(source):
        return subprocess.run(
            [analyzer, configuration, "-p", str(build_directory), str(source)],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, encoding="utf-8", errors="replace", check=False,
        )

    failed = False
    with ThreadPoolExecutor(max_workers=4) as workers:
        tasks = {workers.submit(analyze, source): source for source in sorted(source_files)}
        for task in as_completed(tasks):
            source = tasks[task]
            result = task.result()
            print(f"{'FAIL' if result.returncode else 'OK'}: {source.relative_to(project_root)}", flush=True)
            if result.stdout:
                print(result.stdout, end="" if result.stdout.endswith("\n") else "\n", flush=True)
            failed = failed or result.returncode != 0
    print(f"Analyzed {len(source_files)} project source files.")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
