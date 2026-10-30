# Delivery and defense guide

Use this page to coordinate the remaining work and prepare a repeatable demonstration. It records
acceptance criteria, not new features or deadlines. Follow the owner shown in each GitHub issue;
this page does not reassign another member's work.

## Local work ready for review

| Area | Evidence available | What prevents calling the whole delivery complete |
|---|---|---|
| Standalone engine, #102 | Independent build, module selection, installation and separate consumer tested on Linux/Windows | Review and integrate the selected branches; the playable second game is a separate task. |
| Quality checks, #8 and #12 | Pinned format/tidy configuration, local runner, successful Linux checks | Windows Asio diagnostics remain unresolved; do not suppress them to report success. |
| Installation/archives, #15 | Runtime installation, notices, Linux archives and Windows Release ZIP inspected and exercised | Windows server Ctrl+C and a second Windows runtime environment remain unverified. Main notices are not a complete license audit. |
| README and documentation, #58 and #64 | Setup instructions, module/API guides, generated site and validation record | Each domain must update its page when its branch changes the implementation. |
| Extension guide, #63 | Compilable custom component/system example | Gameplay owner must document the actual JSON/wave schema after integration; another member must follow the guides. |
| Technical study, #115 | Engine alternatives, costs, storage/security limits and accessibility status | Include actual network measurements and final configurable gameplay results when those tasks land. |

All new work must be reviewed at a named revision. A successful test of a private branch is not evidence
that its changes are present in `main`. Do not close an issue merely because its documentation exists.

## Group decisions still needed

- Confirm who delivers and explains the second playable game (#103), including the discussion with Katia.
  A console consumer proves package use but is not that playable demonstration.
- Agree the data needed for health, HUD, game states and levels across gameplay, networking and client (#101, #23).
  Start with the [existing shared data](shared-data.md), not assumed future fields.
- Finish and demonstrate snapshot completion/reassembly, sequencing and measured network improvements with their owners.
- Integrate the gameplay content and client/accessibility work before claiming campaign, boss, menus, remapping or sound.
- Keep optional ideas separate from the tracked delivery scope. A new idea is not automatically a new requirement.

## Before the demonstration

1. Select the actual delivery branch/revision and record `git rev-parse HEAD`.
2. Build it with the documented prerequisites, then run [quality checks](static-analysis.md) and tests.
   Keep tool versions, command output and exit statuses; use CTest's `--no-tests=error`.
3. Run `python scripts/build_docs.py` with the documented tools. Check the API and guides through the local site.
4. Build/package the intended configuration, extract outside the source tree and use that directory for the game.
5. Keep the server's host/UDP port and each terminal's role written down. Start one server for each scenario.
   Do not switch host roles while another member is still checking the previous scenario.

The [build guide](build-and-run.md) has exact Linux/Windows commands. The [validation record](validation.md)
contains the existing results and limits, including the earlier qualitative stuttering report.

## Acceptance checklist

These are remaining acceptance actions; unchecked items are not implied failures or successful tests.

- [ ] From the selected revision, run the installed client/server on Linux and native Windows.
- [ ] Exercise a mixed-OS four-player game; test the fifth-player response and disconnect/reconnect behavior.
- [ ] Hold fire and create the planned maximum entity load; verify packet limits and snapshot completeness.
- [ ] Play the delivered campaign through victory and defeat, including the boss and player damage/death rules.
- [ ] Exercise packet loss, latency, duplication and reordering with the network owner's procedure; record measurements.
- [ ] Verify each delivered accessibility setting and saved preference using its issue's acceptance test.
- [ ] Close clients normally and use Ctrl+C in each server console; record exit status rather than closing the terminal.
- [ ] Check the second playable game's startup, controls, restart and actual engine reuse.
- [ ] Have another domain member follow the engine and content guides from the documented prerequisites.
- [ ] Make sure every member can explain the common protocol and the main architectural decisions.

An additional Windows machine without development tools is a useful distribution check when available.
It has not been tested and is not replaced by hiding tools from PATH on the development PC.
Windows-specific follow-up was deferred; resume it deliberately instead of repeating already successful tests.

## Questions to prepare together

| Question | Evidence or explanation to show |
|---|---|
| What makes the engine reusable? | Build engine/ alone, install it and run the separate consumer; explain which data belongs to the game. |
| Why modules rather than one library or dynamic plugins? | Show the selected targets, their dependencies and current limits in the technical study. |
| What happens when an entity dies and its id is reused? | Show component cleanup and id-reuse tests; explain the lack of a generation counter. |
| Who owns a component returned by get? | Registry/storage retains ownership; the pointer is borrowed and can be invalidated by storage changes. |
| What happens when the server is too slow? | Explain the fixed step, accumulated time, catch-up cap and deliberately dropped simulation time. |
| Why does the client send actions? | Server authority and input flags; explain remapping without confusing local keys and wire values. |
| Why not send a C++ struct directly? | Padding/representation and byte order; show encode/decode and a round-trip test. |
| What happens if a packet is lost or late? | Distinguish replaceable state, discrete events, sequence rules and snapshot completion; acknowledge unfinished work. |
| What is actually accessible today? | Demonstrate delivered settings and the status table, not only the design plan. |
| What did the Windows tests prove? | Native build, tests, archives and two-client cross-play; distinguish unmeasured performance and open shutdown/analysis checks. |

Answer using actual code and observed behavior. If a claim requires a measurement or a teammate's change,
get that evidence before presenting it as a result.

## Evidence to retain

For each scenario record the revision, OS/compiler versions, build configuration, archive checksum,
host/client roles, commands, expected behavior, actual behavior and exit codes. Include entity/player
counts and network conditions for measurements. A screenshot can show rendering; it cannot by itself
prove smooth motion, correct timing or graceful shutdown.

Store raw local logs, transfers and generated files outside versioned source (for example under ignored
build/). Update the public validation page with concise results, retaining open checks and failures.
Never commit credentials, local absolute user paths or a report that labels an unexecuted check successful.
