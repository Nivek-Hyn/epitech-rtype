# Static analysis

Use clang-format **23.1.3** for formatting and clang-tidy **23.1.3** for static analysis.
These are development tools, not dependencies of the game or the engine.
Install them in your active Python environment (a virtual environment is recommended):

```sh
python -m pip install clang-format==23.1.3 clang-tidy==23.1.3
clang-format --version
clang-tidy --version
```

On Linux, the interpreter may be named `python3`. The environment's executable directory must be on `PATH`.
Installing the tools is an explicit developer step; the game build does not install them automatically.

## Selected checks

The root `.clang-tidy` selects checks for correctness:

| Checks | Purpose |
|---|---|
| `clang-analyzer-core.*` | Detect null dereferences, uninitialized values and other invalid operations. |
| `clang-analyzer-cplusplus.*` | Detect lifetime, allocation and C++ object misuse. |
| `clang-analyzer-deadcode.DeadStores` | Detect values assigned but never used. |
| `bugprone-dangling-handle`, `bugprone-stringview-nullptr` | Detect invalid non-owning views. |
| `bugprone-inaccurate-erase`, `bugprone-sizeof-container` | Detect common container mistakes. |
| `bugprone-use-after-move`, `bugprone-move-forwarding-reference` | Detect incorrect move and forwarding usage. |
| `bugprone-unused-raii` | Detect unnamed temporary guards that are destroyed too early. |

Selected diagnostics fail the analysis command. Formatting stays in clang-format; naming, optional
attributes and broad modernization suggestions are not enabled. Project headers are included in reports;
dependency headers are filtered out. A header is analyzed through the source files that include it, so
template checks also need the test files that instantiate the templates.

## Run locally

First configure and build the project normally. CMake writes `build/compile_commands.json` with the
actual C++20 flags, include paths and definitions for each compiled source file.
Then verify the configuration and analyze the files relevant to a change, for example:

```sh
clang-tidy --verify-config
clang-tidy -p build engine/src/Registry.cpp tests/test_registry.cpp
```

To analyze all project source files on Linux, the pinned package also provides the LLVM runner:

```sh
run-clang-tidy.py -p build -j 4 '[/\\](engine|common|server|client|tests)[/\\]'
```

The source filter excludes downloaded dependencies from the files passed to the analyzer. It is separate
from the configuration's header filter. Do not use `-fix` in routine verification: inspect diagnostics and
make deliberate changes instead.

On Windows, Visual Studio generators do not produce the compilation database used by this command.
From a Developer PowerShell with Ninja installed, configure a separate directory:

```powershell
cmake -S . -B build/lint -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/lint --parallel 4
clang-tidy --verify-config
clang-tidy -p build/lint engine/src/Registry.cpp tests/test_registry.cpp
```

Use the database belonging to the current branch and compiler. If the analyzer cannot find a header or
parse the compiler flags, fix that setup error before treating the run as valid. Windows execution must
be checked on Windows; Linux analysis alone does not establish Windows compatibility.

## Scope and limits

Static analysis supplements strict compiler warnings, tests and review. A clean result does not prove
the absence of bugs, thread races or malformed-packet vulnerabilities. This configuration does not
change compiler warning flags or start any GitHub Actions workflow.

For now, run clang-tidy separately from `scripts/check.sh`; integrating it into that script belongs to
issue #12. LLVM documents the command options and compilation database in the
[clang-tidy manual](https://clang.llvm.org/extra/clang-tidy/).
