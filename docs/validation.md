# Validation record

Status recorded on 30 October 2026, following Linux checks and Windows sessions on 29 October.
This record describes tested feature-branch snapshots. It does not imply that all changes have been
merged into `main`, or that every delivery criterion has passed.

## Tested revisions

| Area | Current revision | Original tested revision | Scope |
|---|---|---|---|
| Standalone engine and extension guide | `b210202ac370b99fc12888dd90ccd6ed76c3cf06` | `f0e254ff370fadb6f1d74d3bd2228ad010b22a54` | Includes the standalone package, module options and custom-system example. |
| Local quality checks | `4fec5e86cd4f5ec063c30f532cdfbc53adbc93e4` | `7e5feda1174d09d5b4067088b0becc5d45d09084` | Includes the Asio include-path fix and exclusive protocol message-type selection. |
| Game installation and archives | `060dec9056408f516bd4c2501528f9b28a9e76c1` | `c3c3f281adf99ce0bf27b42aad86ba70939b0207` | Includes both fixes, resources, main dependency notices and detected MSVC runtime files. |

Commit messages were simplified after the tests. The current revisions have the same file contents as
the original tested revisions; authors, dates and branch structure were preserved. Both identifiers are
listed so the Windows reports can still be matched to the current branches.

Windows environment: Windows 10 Home 22H2 x64, MSVC 19.44.35229.0 from Visual Studio Build Tools 2022,
Windows SDK 10.0.26100.0, CMake/CTest 3.31.6, Ninja 1.12.1, Python 3.14.3 and clang-format/clang-tidy 23.1.3.
Linux game checks used GCC 15.3.1. Results below combine local Linux command output, the Windows
verification reports, screenshots and user observations. They are not all automated tests.

## Results

| Check | Linux | Windows |
|---|---|---|
| Engine built without SFML, Asio or GoogleTest | Passed | Passed in Debug and Release. |
| Installed engine consumed by a separate project | Passed | Passed, including an installation path containing a space. |
| Custom component and system example | Passed | Passed; the six manual guide commands also passed. |
| Physics-only package; Systems without ECS rejected | Passed | Passed, with the expected rejection message. |
| Game unit tests | 58/58 passed | 58/58 passed in Debug and Release builds reported by the Windows session. |
| Static analysis of 17 project source files | Passed | Failed: three Asio `NewDelete` diagnostics remain unresolved. |
| Protocol warning C4702 after the targeted fix | No equivalent warning observed | Protocol.cpp recompiled without C4702 in the recorded Release build. |
| Runtime archives, file inventory and SHA-256 | TAR.GZ and ZIP checked | Release ZIP checked; assets, main notices and eight MSVC runtime DLLs audited. |
| Runtime launched outside the source/build tree | Passed | Passed from the extracted archive in a path containing spaces. |
| Normal client window closure | Exit code 0 observed | Exit code 0 recorded. |
| Server shutdown using Ctrl+C | Not part of this Windows check | Not validated; closing its console did not establish graceful shutdown. |

The Windows warning result concerns the recorded recompilation; it does not claim that every dependency
was rebuilt without warnings. Archive notices are the main upstream notices, not an exhaustive license audit.

## Cross-play observations

The two computers were on the same local network. Separate scenarios exercised:

1. A Windows client connected to a Linux server, alongside a Linux client.
2. A Linux client connected to a Windows server, alongside a Windows client.

Application connection messages were observed. User reports confirmed movement and shooting during
the tests; screenshots showed both players, enemies and projectiles on the two platforms. The first
scenario had reported stuttering. The final coordinated scenario was described as fairly smooth.
These are qualitative observations: no latency, packet-loss, FPS or bandwidth benchmark was performed.
The earlier black screen was not reproduced after restarting the coordinated scenario; its cause was not established.

## Open verification points

- Three clang-tidy `clang-analyzer-cplusplus.NewDelete` diagnostics arise through the Windows Asio thread backend.
  They involve error paths after a thread/event allocation failure. No runtime memory error was reproduced,
  and no definitive false-positive classification was established. Checks were not disabled and Asio was not patched.
  Keep the detailed traces for investigation with the network maintainer or upstream.
- The Windows server's Ctrl+C shutdown still needs a console test with its exit status recorded.
- The archive has not been tested on another Windows machine without development tools. Reduced PATH checks
  on the development PC do not substitute for that environment.
- This cross-play exercise used two clients; it does not replace the final four-player acceptance test.
- Degraded-network measurements and the complete final campaign acceptance remain separate tasks.

The remaining Windows-specific checks were deferred on 30 October so other project issues can proceed.
They remain open and must not be counted as successful checks.

## Reproduce and update

Use the installed-package commands in the [engine guide](engine.md) and the
[component/system walkthrough](how-to.md#run-the-component-and-system-example). For game build,
installation and packaging, use the root README from the tested packaging revision.
Compile with the same configuration that will be installed or packaged; run CTest with `--no-tests=error`.

For each new verification, record the commit, OS/compiler versions, exact command, exit status and whether
the result was automated or observed manually. Retain failures and untested items when updating this page.
Raw logs and transferred bundles contain local paths and are kept outside versioned documentation.
