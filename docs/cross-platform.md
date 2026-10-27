# Cross-Platform Support

## Target

Linux is required by the project brief. Windows is the cross-platform target so a Windows client can play with
a Linux server, and vice versa. macOS and WSL do not satisfy the brief's Windows requirement.

The codebase is designed around portable C++20, standalone Asio for networking, and SFML for the client. The
network protocol serializes fields explicitly in big-endian order rather than sending C++ structs directly.
The source under `client/`, `server/`, `common/`, and `engine/` currently contains no POSIX-only socket or
process APIs. CMake links Winsock for Asio on Windows.

## Current Validation Status

| Platform | Status |
|---|---|
| Linux | Build and tests have been run locally. |
| Windows | A CI build target and build instructions exist, but this branch has not yet been verified on Windows. |
| Linux/Windows cross-play | Not yet verified. |

Do not describe Windows or cross-play as tested until the Windows CI job passes and a client/server session has
been checked across the two operating systems.

The GitHub Actions workflow contains an Ubuntu/Windows build-and-test matrix. It is intentionally triggered
manually to control CI usage. Start it from the repository's **Actions** tab using the **CI** workflow. Confirm
that configure, build, and CTest pass on both runners.

## Windows Build

Prerequisites:

- Windows 10 or later.
- Visual Studio 2022 with the **Desktop development with C++** workload and a Windows SDK.
- CMake 3.24 or later and Git in `PATH`.

From a Visual Studio Developer PowerShell in the repository root:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

With the Visual Studio multi-configuration generator, executables are placed in the configuration-specific
output directory. From the repository root, run them with:

```powershell
.\Release\r-type_server.exe 4242
.\Release\r-type_client.exe 127.0.0.1 4242
```

The exact output directory can differ with the selected CMake generator. Check the build output if it is not
`Release`.

## Cross-Play Check

To verify cross-play after both platform builds pass:

1. Connect both machines to a network that permits UDP traffic between them.
2. Start the server on one machine using port `4242`.
3. If the server is on Windows, allow inbound UDP traffic on port `4242` in Windows Defender Firewall.
4. Start the client on the other machine and pass the server machine's LAN address as the host argument.
5. Confirm that both clients can join, see the same players, and exchange movement input.
6. Repeat with the operating systems reversed to test the other direction.

The protocol's explicit byte order and fixed-width integer fields are intended to keep messages independent of
host architecture and compiler layout.

## Portability Rules

- Use standard C++, Asio, and SFML APIs; do not add POSIX-only socket or process calls.
- Keep platform-specific build settings in CMake and guard them with platform checks.
- Serialize network fields explicitly. Never send raw structs, pointers, or platform-native representations.
- Run the Linux and Windows CI matrix after changes to networking, CMake, or shared protocol code.
- Update this page when platform validation changes; distinguish a configured target from a verified platform.