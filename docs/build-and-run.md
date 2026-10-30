# Build, run and distribute

Run these commands from the repository root unless stated otherwise. Use separate build directories for
different generators, compilers and configurations. The [validation record](validation.md) distinguishes
tested environments from outstanding checks.

## Dependencies and first build

You need a C++20 compiler, a build tool, Git and CMake. SFML 3.1 requires CMake 3.28+ when the client is
enabled; the server-only build and standalone engine require 3.24+. Asio, SFML and GoogleTest are fetched
by `cmake/Dependencies.cmake`. The first configure/build needs Internet access and takes longer.

Linux client builds also need platform libraries:

```sh
# Fedora
sudo dnf install libX11-devel libXrandr-devel libXcursor-devel libXi-devel mesa-libGL-devel systemd-devel
# Ubuntu/Debian
sudo apt install libx11-dev libxrandr-dev libxcursor-dev libxi-dev libgl1-mesa-dev libudev-dev
```

Use the command for your distribution. These are explicit system setup steps, not commands run by CMake.
No system-wide installation of Asio or SFML is needed. Compilers, SDKs, graphics drivers and low-level
platform libraries still belong to the build/runtime environment.

To share CPM downloads between Linux builds, optionally set a cache path before configuring:

```sh
export CPM_SOURCE_CACHE="$HOME/.cache/CPM"
```

In PowerShell the equivalent is `$env:CPM_SOURCE_CACHE="$HOME/.cache/CPM"`. This cache is optional.
Do not copy CMake build directories, virtual environments or compiler caches between Linux and Windows.

## Linux

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure --no-tests=error
./r-type_server 4242
# Another terminal, same working directory:
./r-type_client 127.0.0.1 4242
```

The executables are placed at the repository root. Single-configuration Debug and Release builds share
that output path, so rebuild the intended configuration immediately before running or packaging it.

## Native Windows

Use a Developer PowerShell with Visual Studio Build Tools 2022, the C++ tools and a Windows SDK available:

```powershell
cmake -S . -B build/windows -G "Visual Studio 17 2022" -A x64
cmake --build build/windows --config Release --parallel 4
ctest --test-dir build/windows -C Release --output-on-failure --no-tests=error
.\Release\r-type_server.exe 4242
# Another terminal, same repository working directory:
.\Release\r-type_client.exe 127.0.0.1 4242
```

Visual Studio places these executables in the configuration directory under the repository root:
`Release/` or `Debug/`. A Ninja build uses the root output path instead. Keep the repository root as the
client's working directory so `assets/` can be found.

PowerShell does not automatically stop on every failed native command: inspect its output and
`$LASTEXITCODE`. For the compilation database and clang-tidy, use the separate Ninja workflow in the
[quality guide](static-analysis.md).

## Server without graphics

```sh
cmake -S . -B build/server -DRTYPE_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build/server --config Debug --parallel 4
ctest --test-dir build/server -C Debug --output-on-failure --no-tests=error
```

SFML and graphical assets are not required for that build. Add `-DRTYPE_BUILD_TESTS=OFF` only when tests
are intentionally excluded; do not present a build without tests as a successful test run.

## Install runtime files

After building the desired configuration:

```sh
cmake --install build --config Debug --component GameRuntime --prefix "$PWD/build/installed-game"
```

For the Windows build above, use `build/windows` and `--config Release` instead.
The prefix must be an absolute destination; `$PWD/build/installed-game` also works in PowerShell.
Use a fresh destination when switching selected programs: installation does not remove old files.

The directory contains executable files, resources, launch instructions and dependency notices. With the
client disabled it excludes the client, graphical assets and SFML notices. `GameRuntime` excludes engine
development headers/libraries and dependency development files from this installation.

Open a terminal in the installed directory and run `./r-type_server` and `./r-type_client` on Linux, or
`.\r-type_server.exe` and `.\r-type_client.exe` in Windows PowerShell. No source or build directory is
needed at runtime, but the current resource lookup requires this working directory.

MSVC Release installations include compiler runtime files detected by CMake. Operating-system libraries
and graphics drivers are not bundled. Linux executables need compatible system C/C++ runtimes and
platform libraries. Debug Windows builds are intended for development rather than clean-machine release checks.

## Create an archive

```sh
cmake --build build --config Debug --parallel 4
cpack --config build/CPackConfig.cmake -C Debug -B build/packages
```

For Visual Studio Release, use `build/windows`, `--config Release`, its `CPackConfig.cmake`, and `-C Release`.
For a single-configuration Release build, configure with `-DCMAKE_BUILD_TYPE=Release` first.
CPack packages files already built; it does not select and compile a different configuration for you.

The default archive is TAR.GZ on Linux and ZIP on Windows; `-G ZIP` or `-G TGZ` overrides the format.
Names identify the version, platform, processor, client/server selection and configuration. Each archive
has a SHA-256 file. Extract it, enter its single top-level directory and follow `RUNNING.txt`.
Only game runtime files are included, not sources, tests, SDK headers or static development libraries.
Dependency notices and their stated limits are in `THIRD_PARTY_NOTICES.txt` inside the archive.

These files have been exercised on Linux and on the Windows development PC. Testing an archive on
another Windows machine without development tools remains outstanding; see [validation.md](validation.md).

## Troubleshooting

| Symptom | Check |
|---|---|
| CMake or the compiler is not found on Windows | Use the Developer PowerShell for the installed C++ toolchain and inspect tool versions. |
| Missing X11/OpenGL/udev development headers | Install the documented platform packages for the Linux distribution. |
| Git reports dubious ownership while downloading dependencies | Check ownership and use an appropriate filesystem/cache. If an exception is necessary, restrict it to the verified dependency path; do not trust every directory globally. |
| Different generator or compiler in an existing build | Select a fresh build directory; do not reuse another machine's CMake cache. |
| Black window or texture load error | Check the client log, asset paths and application connection separately. A previous black screen had no confirmed cause; do not assume every black screen is a resource error. |
| Client does not connect | Confirm the server command, host address and UDP port. `127.0.0.1` means the same computer. Check private-network firewall access without disabling it globally. |
| clang-tidy fails on Windows Asio | Preserve the traces and exit status; this is a recorded open check, not a reason to remove analysis checks. |
| CTest reports no tests | Ensure tests were enabled and that `-C` matches a multi-configuration build. Keep `--no-tests=error`. |
