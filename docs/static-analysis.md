# Static analysis

Use clang-format **23.1.3** for formatting and clang-tidy **23.1.3** for static analysis.
These are development tools, not dependencies of the game or the engine.
The local analysis runner also requires Python 3.9 or newer.
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

To analyze every project source listed in the database, including tests:

```sh
python scripts/lint.py --build-dir build
```

The runner selects sources under the repository's `engine/`, `common/`, `server/`, `client/` and `tests/`
directories, removes duplicate entries, and runs up to four analyses concurrently. Downloaded dependencies
are excluded from the files passed to the analyzer, separately from the configuration's header filter.
Missing tools, an absent or empty compilation database, invalid configuration and analysis failures all
produce a nonzero exit status. No fixes are applied automatically.

On Linux, run the complete local gate with:

```sh
scripts/check.sh
scripts/check.sh '^(Registry|FixedTimestep)\.'
```

Both commands check formatting, configure and build with tests enabled, analyze every compiled project
source, and run tests. The optional expression filters only CTest, not static analysis. A failed format or
analysis check still allows tests to run when the build succeeded; the final command status remains a failure.
CTest finding no matching tests is also an error. Fix build failures before analysis or tests can run.

`PYTHON` selects the Python interpreter for `check.sh` (default `python3`). `CLANG_TIDY` selects an analyzer
executable for both scripts, or use `--clang-tidy` with `lint.py`. For example:

```sh
CLANG_TIDY=/path/to/clang-tidy scripts/check.sh
```

On Windows, Visual Studio generators do not produce the compilation database used by this command.
From a Developer PowerShell with Ninja installed, configure a separate directory:

```powershell
cmake -S . -B build/lint -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/lint --parallel 4
clang-tidy --verify-config
python scripts/lint.py --build-dir build/lint
ctest --test-dir build/lint -C Debug --output-on-failure --no-tests=error
```

Run the formatting check separately in PowerShell:

```powershell
$projectSources = Get-ChildItem engine,common,server,client,tests -Recurse -File -Include *.cpp,*.hpp
clang-format --dry-run --Werror $projectSources.FullName
```

Inspect each command's exit status; these PowerShell commands are separate manual checks, not a wrapper.
`scripts/check.sh` requires Bash and is not a native PowerShell command.

Use the database belonging to the current branch and compiler. If the analyzer cannot find a header or
parse the compiler flags, fix that setup error before treating the run as valid. Windows execution must
be checked on Windows; Linux analysis alone does not establish Windows compatibility.

## Scope and limits

Static analysis supplements strict compiler warnings, tests and review. A clean result does not prove
the absence of bugs, thread races or malformed-packet vulnerabilities. This configuration does not
change compiler warning flags or start any GitHub Actions workflow.

GitHub Actions remains manual-only; its existing build/test and format jobs do not run this new local
analysis step. LLVM documents the command options and compilation database in the
[clang-tidy manual](https://clang.llvm.org/extra/clang-tidy/).
