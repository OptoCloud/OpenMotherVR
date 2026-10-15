<!-- Provenance: spec side's answers to the implementation side's QUESTIONS.md r1 (questions on spec r2); user decisions for spec r3; AI.exe 2015 analysis (mode-stack container); DXGI public API semantics; user decisions for spec r4 (2026-10-14); project decisions for spec r5 (clean-room review of every document); a tester's report on SteamVR with a Steam Frame headset and project decisions for spec r6 (2026-10-15); AI.exe 2015 analysis by the spec side for spec r6 (the game's window procedure on focus loss and deactivation; the five pointer-table slots holding ScriptEventTap's address); for spec r7: the second clean-room review, the coverage review and the cross-document check of every document, with their project decisions, and AI.exe 2015 analysis by the spec side (the single call site of the device creation function, the C runtime's own exception filter and its fail-fast paths, the game window's procedure and size handler, the event tap's padding and pointer-table slots, the follow camera's position fields and yaw setter, the pad helper's code range, the action map's composite flag, the pass set-up's type argument, the setting-name table), each re-read for this revision. 2020 values were taken from the earlier mod's per-build address table (and, for inline sites, its installer) and have not been verified against a 2020 `AI.exe`; spec r7 adds none. Revision: r5 -->

# Answers and decisions (spec r3 to r7)

This file records the spec side's answers to the implementation side's
questions (spec r3) and the decisions taken for spec r4 to r7. Each entry
names the documents and sections it changed. Where an entry is a decision
rather than a game fact, the reason is given. Section numbers in the
"Changed" lines are those of the revision that made the change; a later
revision may have moved them.

## Q1. `-vrscale`: world scale or pixel scale?

**World scale.** `-vrscale <f>` sets the world (metres) scale `s` for one
run. Its accepted range, [0.495, 1.505] with any other value giving 1.0, is
kept for compatibility with existing shortcuts that pass the switch. The
pixel scale is only a config and options-page setting (its range and reason
are in 06 §1) and has no command-line override. This matches the reading
given in the question.

Changed: `06-options-and-config.md` §1 (runtime values paragraph: world
scale is not a setting) and §2 (command-line table); `03-hmd-and-frames.md`
parameter table (sources of `s` and `p`).

## Q2. Runtime values without a backend ("Oculus", "OpenXR")

**The runtime values are None, OpenVR and OpenXR, and v1 has both
backends.** OpenXR is now specified (`09-openxr.md`). Value 1 (`"Oculus"`
in existing config files) now means OpenXR: the options page offers OpenXR
for it, and a config file saying `"Oculus"`, `VRRuntime` 1 or
`-vrforceoculus` all select OpenXR (Meta's PC runtime is an OpenXR runtime,
and v1 has no LibOVR backend). The mod writes `"OpenXR"`. There is no
value 3.

Changed: `06-options-and-config.md` §1, §2 (`-vrforceopenxr` added,
`-vrforceoculus` = OpenXR), §3.2, §3.3, §4.2 (`mvr_vrruntime 1`), §4.3
(the runtime value pushed to the options page), §5.1 (the runtime choices
offered); `00-overview.md` §2.5, §3; new `09-openxr.md`.

## Q3. Mirror "both eyes" with symmetric off: crops of different height

**Both crops use the union of the two vertical ranges; each keeps its own
horizontal range.** `v0 = min(vMin_0, vMin_1)`, `v1 = max(vMax_0, vMax_1)`;
`a` uses the summed widths over `v1 - v0`; eye 0 left, eye 1 right, each
half as wide as its crop's share. Why: a common vertical range keeps the
same rows of the target at the same height in both halves and stretches
neither crop; a mean height would stretch one of them. On current headsets
(mirror-image eyes) the ranges are equal, so this only matters for unusual
hardware; an implementation that gives each crop its own vertical range
differs only in that case.

Changed: `03-hmd-and-frames.md` §8.2.

## Q4. Fullscreen after swap-chain creation

**Yes, block it while the stereo device exists.** The swapchain must stay
windowed while a headset is attached, and a later switch to fullscreen is
refused: `IDXGISwapChain::SetFullscreenState(TRUE, ...)` returns
`DXGI_ERROR_NOT_CURRENTLY_AVAILABLE` without changing state, `FALSE` passes
through, DXGI's Alt+Enter is disabled with `MakeWindowAssociation(...,
DXGI_MWA_NO_ALT_ENTER)`, and `ResizeTarget` passes through (windowed, it
only resizes the window, which the mirror follows). Without a stereo device
none of this applies. Why: the side-by-side back buffer is larger than any
display mode, so a fullscreen transition cannot succeed and would leave the
swapchain in a mode the mirror cannot use; the refusal code is the one DXGI
documents for an impossible transition. How the game's video menu reacts to
the refusal is still to be checked in game.

Changed: `03-hmd-and-frames.md` §4.5 in r3; the rule is now in §9.4.

## Q5. Raising the mode stack count with no stack storage

**Raise it only through the game's own append routine.** Game fact: the
stack is a growable int array `{+0x68 data, +0x6C count, +0x70 capacity}`
that starts empty; the game grows it only in its append routine at RVA
0x294540 (thiscall, ECX = stereo +0x68, one argument `const int*`, `ret 4`,
returns the new element's address; 8 entries on first use, then doubling),
called from the stereo push-mode function (`StereoPushMode`, 01 §4.5) at
RVA 0x5D9C53. So: lower the count by writing it; raise it by appending the
value 1 through that routine. Where the routine's address is unknown (2020
build), the count is raised only when the array has storage and spare
capacity; otherwise it is left unchanged and the event is logged (11).

Changed: `05-game-patches.md` §3.2f; `01-loading.md` §4.5
(`StereoModeStackAppend`).

## Q6. Patches when the VR runtime is None

**With runtime None the game is the unmodded game.** No stereo device is
created and no game patch is applied, except a small UI set that lets the
player turn VR on: the UI archive redirect, the game's UI external-call
slot, the settings push when the front-end and pause menus open (without
the stereo-mode handling), and skip intro when it is on (the set is listed
in 01 §2.3). No Rift tests, pad, camera, SSAO, mode-stack, UI-quad,
render-path or shader patches; no D3D11 wrapping beyond forwarding the
`d3d11.dll` exports (01 §1). The patch set is decided at attach from the
config file and the command line, which are both known then. When the
runtime is OpenVR or OpenXR but fails to start, all patches are already in
place and the game runs flat on the stereo path's no-device behaviour (02);
the player can choose None for the unmodded game.

Changed: `01-loading.md` §2.2 and new §2.3 (patch sets; the old §2.3 is now
§2.4); `00-overview.md` §2.7; `06-options-and-config.md` §1, §2, §4.3.

## Other changes in spec r3

| Document | Change |
|---|---|
| `04-input.md` r2 | §5.8 new: the runtime input layer (semantic inputs; SteamVR Input action manifest and default bindings for `knuckles`, `oculus_touch`, `holographic_controller`, `vive_controller`; legacy fallback without `IVRInput`; OpenXR actions and suggested bindings per interaction profile; semantic inputs to game actions). §4.1 glyph table gains a WMR column; §5.3 pause on Index; §5.4 WMR mapping settled; §5.5 scheme choice per runtime; §8 runtime recenter request |
| `06-options-and-config.md` r2 | §7 rewritten: patching the player's `UI.PAK` (requirement, PAK2 format including sorted names, which archive loads, per-entry size and CRC-32 checks of the Steam 2015 movies, behaviour when the archive differs, the game's UI scripting interface, required movie changes, glyph images); the implementation authors its own movie changes; §4.1 the per-movie prefixes of the game's own UI calls (game fact) |
| `09-openxr.md` r1 | new: the stereo device on OpenXR, verified on SteamVR with Index |
| `01-loading.md` r2 | §4.7 lists every game address 07 and 08 use, with checked bytes for the sites |
| `00-overview.md` r3 | runtimes, controllers, VR-not-available behaviour |
| `README.md` r3 | index: 09 and this file |

## User decisions for spec r4

These are decisions, not answers to questions; each says why.

| Decision | Reason | Where |
|---|---|---|
| A fresh `mothervr.json` selects OpenVR. None stays the unmodded game (Q6) and `"Oculus"` stays OpenXR (Q2). | Installing the DLL and starting SteamVR should give VR with no setup. | `06-options-and-config.md` §1, §1.1; `00-overview.md` §2; `13-distribution.md` §5.3 |
| The log uses a Serilog-style format defined by this project. | A familiar, parseable format; the project owns its wording. | `11-diagnostics.md` §1, §2 |
| The spec names no other mod or mod author. Names players or their files depend on (`mothervr.json` keys and values, `MOTHER.PAK`, `mothervr.log`, `mothervr_input`, `openvr_api.dll`, `mvr_*`, `-vrforce*`, `-vrscale`, the page title `MotherVR`) stay exact for compatibility and are written in code spans. | Clean-room record; `tools/spec_lint.py` enforces it. | all documents; `../CLEANROOM.md` |
| The options menu is authored by this project, compiled at build time and linked into the player's own `UI.PAK` at install time. No game or other mod UI file is shipped or used as a source. | The menu must not derive from another mod's assets. | `14-ui-resources.md`; `06-options-and-config.md` §5, §7 |
| Messages, labels and log text are specified by what they must convey; the implementation writes its own wording. | Wording is expression, not a requirement. | `../CLEANROOM.md`; `13-distribution.md` §5; `06-options-and-config.md` §5 |
| A mirror window that does not fit its monitor is shrunk and centred (replaced in spec r6: it becomes borderless over its monitor). | With a headset the window keeps the fullscreen client size and runs off screen. | `03-hmd-and-frames.md` §9.3 |

## Documents in spec r4

| Document | Revision | Change |
|---|---|---|
| `00-overview.md` | r5 | rewritten for v1; clean-room review applied |
| `01-loading.md` | r4 | loading rules, preflight as one rule, runtime selection, address table; review applied |
| `02-game-stereo-path.md` | r3 | layouts, device behaviour as required results; review applied |
| `03-hmd-and-frames.md` | r4 | sizing, frame timing, mirror and swapchain as results; invalid poses; review applied |
| `04-input.md` | r4 | controls timing, defaults per controller, code scheme as constraints; review applied |
| `05-game-patches.md` | r4 | every byte patch in one table, per-frame outcome rules; review applied |
| `06-options-and-config.md` | r4 | settings, command line, console commands, options page by meaning; review applied |
| `07-reflections.md` | r3 | required state per render call, verify before write; review applied |
| `08-render-fixes.md` | r3 | lighting fix, pop-in, AA, wall fade delay as results; review applied |
| `09-openxr.md` | r3 | frame loop as constraints, invalid poses, session failure detaches; review applied |
| `10-audio.md` | r2 | game audio on the headset's device, as required results; review applied |
| `11-diagnostics.md` | r2 | log format and required events with fresh templates; review applied |
| `12-threads-and-robustness.md` | r2 | threads, verify before write, failure table as properties; review applied |
| `13-distribution.md` | r2 | files written, messages by content, package; review applied |
| `14-ui-resources.md` | r2 | options menu resources, stock survey, linking; review applied |
| `README.md` | r5 | index |

## Spec r5

Spec r5 applies a clean-room review of every document: each document's
revision goes up by one. The review removed descriptions of the earlier
mod's internals and of the project's own earlier code, rewrote ordered
procedures as required results (an order is kept only where the game,
Windows, D3D11, COM or a VR runtime forces it, with the reason), gave every
numeric parameter a reason or marked it as a project decision, left invented
names (messages between the options page and the DLL, glyph and action
names, the gamepad code scheme) to the implementation, and labels 2020
values as taken from the earlier mod's table and not verified against a
2020 `AI.exe`. 11 alone defines log wording and 13 alone defines messages
to the player.

Project decisions in r5:

| Decision | Reason | Where |
|---|---|---|
| The eye height `H` is the larger of the two eye heights. | Neither eye's crop may be cut off; the previous wording ("the height of the wider eye") gave a different value when the wider eye is not the taller one. | `03-hmd-and-frames.md` |
| The OpenVR loader is an x86 `openvr_api.dll` from a public OpenVR SDK release that serves the interface versions the implementation requests (`IVRSystem_017`, `IVRCompositor_021`, `IVROverlay_016` or later). A missing interface is a start-up failure. The check that the local copy is current must not read the whole file on every launch; how it checks is the implementation's choice. | A public release can be redistributed; naming interface versions rather than one release lets any compatible release be shipped; reading the file each launch would slow every start for no gain. | `13-distribution.md`; `01-loading.md` |
| SteamVR need not be running: initialisation may start it. There is no check for its process. Start-up fails only if the runtime cannot start or no headset appears within a bounded wait of about 10 s. | The runtime's own initialisation starts SteamVR; a process check would refuse a working setup and race with its start-up; the bound keeps a missing headset from holding up the game. | `01-loading.md` §5; `03-hmd-and-frames.md` |
| An invalid head pose yields the last valid pose, or the neutral pose if there was none. A recenter is never taken from an invalid pose; it stays pending until a valid pose arrives. Looking straight up or down (horizontal length of the forward axis below 1e-4) keeps the previous yaw, and no result is ever NaN. | A held pose is less disorienting than a jump to the origin; a reference taken from an invalid pose would offset every later frame; yaw is undefined when the forward axis is vertical, and a NaN would reach the game's camera. | `03-hmd-and-frames.md`; `09-openxr.md` |
| With an unknown or unwritable game folder the game still starts. The mod never needs elevation, a registry change or a folder rename. Each missing file degrades on its own: no options page without `MOTHER.PAK`, the runtime's own loader without a local `openvr_api.dll`, log output to the debugger if the log file cannot be opened. | The proxy must never stop the game from starting; a game installed under a protected folder is common; one missing file must not disable unrelated features. | `12-threads-and-robustness.md`; `13-distribution.md` |

## Spec r6

A tester on SteamVR with a Steam Frame headset reported: OpenXR failing
at instance creation with `XR_ERROR_RUNTIME_UNAVAILABLE` (-51) and a
failure message with an empty result text and no way back to SteamVR; the
game not shown in the headset with SteamVR's Desktop Game Theatre on; the
game pausing, or staying at a loading screen, when SteamVR or a streaming
client took the desktop focus; menus and the cursor off centre in the
shrunk mirror window; the view sliding toward the turn direction after a
snap-turn step while walking; a crash in the pause menu with nothing in the
log. The project took the decisions below; each document changed has its
revision raised by one.

| Decision | Reason | Where |
|---|---|---|
| OpenXR discovery reads both registry views. The manifest chosen is an x86 build of the runtime the 64-bit view names (that manifest when its library is x86, else a usable x86 sibling); the 32-bit view's entry is used only when that yields nothing. A non-empty `XR_RUNTIME_JSON` set by the player still wins. Both views' values are logged. | SteamVR and most runtimes write only the 64-bit view when the player makes them active, so a 32-bit entry that differs is usually left over from a runtime used earlier; taking it loads a runtime the player did not choose, or one that cannot run (the reported -51). | `09-openxr.md` §1; `01-loading.md` §5.2; `11-diagnostics.md` §2.3 |
| The chosen manifest reaches the loader both as `XR_RUNTIME_JSON` in the environment and as a loader initialisation property (`XR_KHR_loader_init` with `XR_EXT_loader_init_properties`, before any other loader call). An elevated process is logged once. | The loader ignores the environment in an elevated process but honours its initialisation properties there; the specification requires the initialisation before other loader calls. | `09-openxr.md` §1, §2.2; `11-diagnostics.md` §2.3 |
| An OpenXR failure's text is never empty: with no instance (a failure of instance creation or before it), the message and the log name the `XrResult` by its symbolic name from the OpenXR registry. | `xrResultToString` needs an instance; an empty text left the player and the bug report with a bare number. | `09-openxr.md` §7; `13-distribution.md` §5.3; `11-diagnostics.md` §1.4, §2.4 |
| The OpenXR failure message also says how to go back to SteamVR (the options page's SteamVR choice, or `-vrforceopenvr`), that a restart tries OpenXR again, and, for -51, that no usable 32-bit OpenXR runtime could be loaded and that the log lists what was tried. | A player who switched to OpenXR and could not start it had no visible way back; -51 is only diagnosable from the log. | `09-openxr.md` §7; `13-distribution.md` §5.3 |
| The options page shows the runtime this run drives: None after the chosen runtime failed, the fallback runtime after a fallback. The stored `VRRuntime` is not rewritten. | Showing the stored runtime after a failure claimed it was running and hid the retry; showing None offers both a retry (a restart) and the other runtime, and keeping the stored preference makes a restart retry it. | `06-options-and-config.md` §1.1, §4.3, §5.2; `01-loading.md` §5.3; `12-threads-and-robustness.md` §7 |
| With OpenVR, about 3 s of frames after the first submission, the compositor's scene application is compared with the game's process; when it is another process, it is logged with its executable and the player is warned once, without blocking, that SteamVR is not showing the game, that Desktop Game Theatre is the usual cause and which Steam game setting to turn off. With OpenXR, a running session that has not reached `VISIBLE` or `FOCUSED` 10 s after it began is logged with its state and gets the equivalent warning once. Every OpenXR session state change is logged. | With the Theatre on, start-up succeeds but the game's frames never reach the headset; nothing fails, so without a check the player sees only a flat window. 3 s and 10 s let the runtime settle first; a non-blocking warning keeps frames flowing so the fix shows at once. | `03-hmd-and-frames.md` §6.4, §6.7; `09-openxr.md` §3, §3.1, §7; `13-distribution.md` §5.2, §5.3, §8; `11-diagnostics.md` §2.4, §2.6 |
| A mirror window that does not fit its monitor's work area becomes borderless (caption, sizing frame and system-menu buttons removed) and covers its whole monitor, asynchronously; a window that fits is left alone. Replaces the r4 shrink-and-centre decision. | In its fullscreen display mode the game ignores size changes, so a shrunk client area no longer matched the size the game kept and the menus and cursor were off centre; a borderless window over the monitor has the client size the game keeps. Re-checking after each back-buffer resize is harmless because a window that fits is left alone. | `03-hmd-and-frames.md` §9.3; `00-overview.md` §2.4; `12-threads-and-robustness.md` §7; `11-diagnostics.md` §1.4, §2.2 |
| Game fact (AI.exe 2015, verified by the spec side): the game's window procedure, on `WM_KILLFOCUS` while its display mode is fullscreen, posts the system event that opens the pause menu and releases its mouse buttons; on `WM_ACTIVATE` inactive it releases its cursor clip and clears its keyboard state. Decision: while a headset is attached, the game window is subclassed (ANSI, chained) and focus loss is not passed to the game; activation still is; each swallowed focus loss is logged with the executable of the foreground process. | Under VR the game keeps its fullscreen mode while the swapchain is forced windowed, so SteamVR or a streaming client taking the desktop focus paused the game or left it at a loading screen until the window was clicked. Keyboard and mouse still stop while another window has the focus (Windows sends them only to the foreground window); the gamepad and VR controllers keep working. | `03-hmd-and-frames.md` §6.6, §9.6; `02-game-stereo-path.md` §9.4; `04-input.md` §5.12; `01-loading.md` §2.7; `12-threads-and-robustness.md` §2.2, §7; `11-diagnostics.md` §2.2 |
| The mod installs an unhandled-exception filter at attach. It logs once, without allocating, the exception code, the faulting address as module plus offset, the access-violation kind and address, the registers, and module plus offset for the code addresses among the top 16 stack words (read only within the thread's stack), then calls the filter it replaced. | A crash must be locatable from the log alone; passing the exception on keeps Windows' own crash record and any earlier filter. 16 words reach the callers of the faulting function in the game's usual frame sizes while keeping the record short. | `11-diagnostics.md` §6; `01-loading.md` §2.2; `12-threads-and-robustness.md` §2.2, §2.3, §7; `00-overview.md` §2.9; `22-controls-and-comfort.md` §14 |
| Game fact (AI.exe 2015, counted by the spec side): `ScriptEventTap`'s address also fills five slots of pointer tables in `.rdata`; the body is shared by folded functions that the game calls through those tables with arguments that are not strings. Decision: the tap reads its argument as a name only when it points into the game's image (every event name is a literal there); otherwise the call passes through untouched. | Reading a non-string argument as a string can fault in code the game runs during normal play. | `11-diagnostics.md` §5.1, §5.2; `01-loading.md` §2.7, §4.6; `12-threads-and-robustness.md` §7 |
| With `DebugOptions` on: each change of the stereo mode (world or virtual screen) with before and after; each change between screen frames and head-tracked frames, once per change; for 600 ms after each snap-turn step, at every game-thread update, the camera's two yaw fields, the horizontal offset of the character's head from its root, whether game look is enabled and whether keyboard and mouse are in use. All on the game thread except the frame-kind change (render thread). | To follow which screens use the virtual screen, and to tell the possible causes of the reported slide after a snap step (the eye following the head as the body turns, or game look turning too). 600 ms bounds the per-frame lines to a short window per step. | `11-diagnostics.md` §4.5, §8; `05-game-patches.md` §3.2c, §4.4; `03-hmd-and-frames.md` §6.2 |
| After the touchpad scheme's scaling, the move vector is limited to length 1 (direction kept). The snap step stays `2 / segments` in floating point, as 04 §6 and 05 §3.2c already require. | Per-component clamping left diagonals up to about 1.1 long; the pad's full deflection must equal a stick's round stop, so the scale only moves where full speed is reached. | `04-input.md` §5.1, §5.2, §5.8.5 |

Open: the camera fields holding the character's root and head positions
for the snap-turn trace are not yet given (11 §8); how SteamVR's Desktop
Game Theatre affects an OpenXR session is not observed (09 §12).

Documents in spec r6:

| Document | Revision |
|---|---|
| `00-overview.md` | r6 |
| `01-loading.md` | r5 |
| `02-game-stereo-path.md` | r4 |
| `03-hmd-and-frames.md` | r5 |
| `04-input.md` | r5 |
| `05-game-patches.md` | r5 |
| `06-options-and-config.md` | r5 |
| `09-openxr.md` | r4 |
| `11-diagnostics.md` | r3 |
| `12-threads-and-robustness.md` | r3 |
| `13-distribution.md` | r3 |
| `22-controls-and-comfort.md` | r2 (crash event built on v1's crash record) |
| `ANSWERS.md` | r4 |

## Spec r7

Spec r7 applies a second clean-room review of every document, a coverage
review (requirements that were missing or in conflict between documents)
and then a cross-document check that aligned every document with the
decisions of the first two. A document changed by both raised its revision
twice. Every game fact below was verified by the spec side in the 2015
`AI.exe`; every new 2020 cell is `?`. A decision taken by more than one
document's owner is recorded once, with all the sections it changed.
Section numbers are those of the pass that made the change; the
cross-document check renumbered only 01 §4.8 (now §4.9, after a new
§4.8).

### Game facts

| Decision | Reason | Where |
|---|---|---|
| Game fact: the stereo manager's device creation (RVA 0x5E0D40) has one call site, `StereoInitCall` (0x613941), and no pointer to it exists in the image; the display initialisation holding that site (0x6138F0) has one caller (0x59C307). The game does not create the device again on a display reset. A repeated call stays handled (re-attach, no second runtime start). | Resolved a contradiction between 01 and 02; 01's acceptance on a display reset could not pass as written. | 01 §2.7, §4.2, §6 item 10, §7 Q2; 02 §6.1; 03 §2.2, §6.4, §6.5, §12; 04 §5.12; 05 §3.2e; 09 §2.1; 11 §2.4; 12 §7 |
| The function at 0x5E0D40 is labelled `CreateDevice` in every document (`thiscall(manager, int type)`); its call site keeps the label `StereoInitCall`. | One label for one function; 02, 03 and 09 already used `CreateDevice`, and 03, 09 and 12 refer to `StereoInitCall`. | 01 §2.1, §2.2, §2.3, §2.7, §4.2, §5.2, §5.3; 02 §6.1; 03 §2.2, §12; 09 provenance, §2.1 |
| Game fact: the game's C runtime, in a C initialiser run after the DLL attach and before the game's static constructors (`CrtFilterInstall` 0x6826A6, the last entry of the initialiser table 0xDBB124 to 0xDBB144), installs its own top-level filter (`CrtUnhandledFilter` 0x682664): it ends the process for an MSVC C++ exception, returns `EXCEPTION_CONTINUE_SEARCH` otherwise and never calls the filter it replaced. Its two fail-fast paths (`CrtFailFast`, 0x6770BD and 0x684C81) clear the filter and call `UnhandledExceptionFilter` directly. | A filter installed only at attach is replaced before `WinMain` and never runs, which explains the tester's crash with nothing in the log (spec r6). | 01 §4.6; 11 §6.1 |
| The crash filter is installed at attach and again at the first `D3D11CreateDeviceAndSwapChain` call through the proxy, chaining to the filter it replaced, and never keeps itself as the replaced filter. A crash between the C runtime's initialisation and device creation, or through a fail-fast path, is left to Windows. Changes the r6 decision "installs an unhandled-exception filter at attach". | The device creation export is the first point after the C runtime starts at which the proxy runs on every run, runtime None included, without changing the game's code; installing twice must not make the filter call itself. | 01 §1.3, §2.1, §2.2, §6 item 17; 11 §6.1, §6.2, §7 item 13; 12 §2.2, §7 |
| Game fact: the game window's procedure is RVA 0x612850, registered with `RegisterClassExA` at 0x612F90 (so ANSI). Its `WM_ACTIVATE` branch is 0x61288F; its `WM_KILLFOCUS` branch 0x612910 posts system event 4 (the pause) through 0x58FB90 only when the display object's (RVA 0x1251288) `+0x04` is 2. `WM_SIZE` goes to the display object's handler 0x611B90, which records the size and resizes only when `+0x04` is 0. That 2 is fullscreen and 0 windowed is [likely]. | The r6 focus-loss fact had no address the implementation side could confirm; the size handler turns "the game ignores size changes in fullscreen" from an inference into a traced fact. | 01 §4.6 (`GameWindowProc`, its branches, `WindowDisplayState`, `WindowSizeHandler`); 02 §9.4; 03 §9.3, §9.6, §12, §14 item 2 |
| Game fact: `ScriptEventTap`'s five-byte body is followed by eleven `cc` padding bytes and the next function at 0xA1BCD0; the five `.rdata` slots holding its address are 0x1076A18, 0x1076EA0, 0x1076F80, 0x1077060 and 0x1077140. Decision: the tap's hook writes at most the five body bytes, never the padding; the replacement may keep the contract itself. | Keeps the change to the game inside the function it replaces. | 01 §4.6; 11 §5.1, §5.2 |
| Game fact: in the follow camera (vtable 0xDC6600), slot 6 (0x2C9F0) and the per-update slot 8 (0x2E970) write the followed character's position to `+0x1C0` to `+0x1C8` and the translation of the `Head1` bone's transform to `+0x1CC` to `+0x1D4`; slot 6 looks the bone up by name and keeps its index at `+0x248`; OnActivate (slot 0, 0x2C930) writes none of them. Both use transform mode 7; whether that is world space is [likely]. The yaw setter 0x2DF30 (thiscall, float yaw and bool, `ret 8`) wraps the yaw, limits it to the range at `+0xB4` and `+0xB8`, writes `+0x204` and, with the bool, `+0x1FC`. Resolves the r6 open item on the snap-turn trace's fields for 2015. | The r6 snap-turn trace needed the fields; 04 had said the positions were written at activation, which the binary contradicts. | 01 §4.5 (`FollowSetYaw`, `FollowUpdate`, `FollowVtable`); 04 §2.7, §6, §11; 05 §1; 11 §4.5, §8 |
| Game fact: the stock `ControlToName` (0x576840) passes the code to a helper (0x5764E0) that reads it as unsigned 16 bit and returns null above 0x17, -1 included. The map's digital evaluation (0x586D80) reads a record with flag bit 0x40 as a composite reference and never calls IsDown for it; otherwise it calls IsDown with the code zero-extended. | Grounds the code decisions under "Input" below in the game. | 01 §4.4; 04 §2.3, §2.4, §3.1, §3.2, §3.3 |
| Game fact: the pass set-up 0x595170 (thiscall) takes the pass type as its first stack argument (type 0xB at 0x5B17D7, 4 at 0x5B3F98, 2 and 3 in 0x5B3A80); the viewport setter 0x621350 and the constant writers 0x621C20 and 0x621C90 are cdecl. 24 states them as its own facts. | 24 cited them to a question of 07 that does not establish them. | 24 §7.1, §9.1, §12 items 1, 2 |
| Game fact: the game's setting-name table at 0x124FC20 is indexed by setting id, ids 0x00 to 0x19 (`Depth of Field` to `Audio`, `Stereo Mode` at 0x18). | 02 needed the ids the Stereo Mode handler changes; 24 lists all of them. | 02 §6.5; 24 §1.2 |
| Game facts for 07 and 08: `ENGINE_SETTINGS.XML` offers Planar Reflections Off = 0 and On = 3; the mip build by quality (0x59C8B0) generates mips through the context at quality 2, blurs (0x59BD80) at 3 and does nothing otherwise, and the full mip chain is created only at 2 or 3 (0x5995A0); HeadClipFade (0x346DE0) returns a product of powers of clamped terms, each exactly 1 when nothing is near. SetEye's side effects (0x5B38D0), the cull tests of 0x61FD20 and 0x619A50 and the dither shader counts per level (353 of 887 Torrens, 443 of 1068 `TECH_HUB`, 434 of 1046 `SCI_HOSPITALUPPER`) are restated from the spec side's own reading in 07 and 08. 07's header records how its facts were located. | Each replaces earlier wording with the spec side's own reading of the game. | 07 header, §1, §2.2, §8 item 2; 08 §1.1, §1.2, §2.1, §2.2, §4.2 |
| Game and data facts for 14: `crosshairs_mc` in `WEAPONSTUFF.GFX` is placed once at root depth 1 at (640, 360) and only moved by its class; `UI.PAK` holds 230 DoABC2 tags in 124 movies, all of which parse; `DATA\INPUT.XML` binds `y` to `select_torch` in the default device and all five presets. | Read by the spec side with its own archive reader; the earlier tag count was wrong. | 14 §2.6, §2.10, §3.6.4 |

### Loading, the stereo path and the overview (00, 01, 02)

| Decision | Reason | Where |
|---|---|---|
| The proxy acts only in a process whose main module's file name is `AI.exe` (case-insensitive). In any other host it only forwards the `d3d11.dll` exports and logs one event naming the host: no settings read or written, no message, no crash filter, no patch, no other file. A renamed game executable is not supported. | Such a host is not the game, and a message box there blocks it for nothing, an automated load test included. | 00 §2.8; 01 §1.4, §3.1, §6 item 14; 06 §3.1; 09 §8; 11 §1.7, §2.1, §7 item 22; 12 §2.4, §7; 13 §5.3, §9 item 21; 14 §5.1, §6 item 19 |
| In the game's process the proxy pins itself in memory from attach; a dynamic unload happens only in a host that is not the game. | From attach on, game code holds pointers into the proxy (filter, patches, slots, detours, the focus handling). | 01 §1.4; 12 §7, §8 item 15 |
| At process exit the only work is appending the detach line to the open log. | Reconciles "no work at exit" with the detach line 01 and 11 require. | 00 §4.2 item 24; 01 §1.4; 22 §14 |
| Every replaced vtable slot is checked against the exact function 01 §4 lists for the build; only where the build's value is unknown is the slot checked to lie inside `.text`. | Every 2015 original is known; an in-image check passes a slot that points at another function of `AI.exe` (another build or another tool), and chaining with the wrong ABI would crash. | 01 §2.5.1, §6 item 18; 04 §2.3, §2.7, §10 item 4; 05 §5, §6.1, §6.2; 12 §4.1 |
| A written value derived from an original operand byte (the mouse displacement low bytes) is checked against that original before 0x10 is added. | The written value is right only when the reference size lies 0x10 bytes past the back-buffer size. | 01 §2.5.1, §6 item 19; 05 §7, §7.7; 12 §4.1 |
| Skip intro writes its operand only when `SkipStartupIntro` is on and the UI archive pointer was redirected to the mod's archive in this run. | The stock movie has no skip-intro function. | 01 §2.3, §2.6, §6 item 15; 05 §11, §15 item 21 |
| The reflection fix is all or nothing: both its checks hold or nothing is written. | A per-eye render without the clear skip erases half of the target (07 §5). | 01 §2.5.2 |
| No welcome message and no first-launch `mothervr.json` on a run where the mod is not active (unknown build, failed core checks of 01 §2.5.1). | The welcome would describe a mod that is not running, and writing the file would hide the welcome from the first run on which the mod works. | 00 §2.7; 01 §2.4, §6 item 16; 06 §3.1; 12 §7; 13 §3.4, §5.3, §9 item 22 |
| OpenXR probing changes nothing in the process but the log. `XR_RUNTIME_JSON` and the loader property are set, and an elevated process logged, only when OpenXR is the runtime that will start. | The variable reaches every child process and any later loader in the process, for a runtime the player did not choose. | 01 §5.2; 09 §1, §11 item 16; 11 §2.3; 13 §2 |
| 2020 structure offsets (input manager, action maps, pad device) are assumed equal to 2015 and unverified; a feature that depends on them is enabled on 2020 only after its document's plausibility check passes. | Only RVAs had a 2020 column; the offsets had no build qualifier. | 01 §3.2; 04 conventions |
| With runtime None no replacement is installed on the game's device or swapchain, the pixel shader replacement included; with a stereo device, device creation also applies the window results of 03 §9.3, §9.4 and §9.6. | Runtime None is the unmodded game (Q6); the window results are required at the same moment. | 01 §1.3, §6 item 6 |
| The Stereo Mode handler changes only Depth of Field, SSAO and Motion Blur; VSync keeps the player's value. If the game's setter saves values, a VR run must not leave the three changes in the player's saved settings. | VSync has no effect while the device presents; the player's own effects must come back when VR is off. | 02 §6.5, §10 item 11, §11 item 5; 24 §8 |
| A late detach can be made on the render thread: the device pointer and state (manager `+0x04`) are written by both threads with atomic access; the runtime is not released while a call into it that began on another thread may still run (a bounded wait, or kept until exit). | Slot 3 runs on the render thread after a failed OpenXR session; a game-thread call could otherwise use a released runtime. | 02 §7.4, §9.1, §10 item 12; 03 §11; 12 §3.2 |
| The no-headset outcome tells the player only when a chosen runtime failed; no message when no runtime was chosen. | Matches 01 §5.3. | 02 §6.1, §10 item 8 |
| 2020 values that were not in the earlier mod's table (the allocation sequence after its first byte, manager slot 2, device slot 11) are `?`, and the offset rule that derived them is removed. | Only values from that table may enter as 2020 facts; derived values were unverified. | 02 §3.1, §3.2, §5 |
| Console commands passing through intact is a robustness requirement (00 item 15 moves from §4.1 to §4.2, its number kept). | It is a requirement on the code, not an observation in game. | 00 §4.1, §4.2 |
| 00 gives the head-in-wall delay as about half a second, pointing at 08 §4.2; 00 §1.1 points v2 to its own documents. | One owner for each value and feature. | 00 §1.1, §2.1, §4.1 |
| The UI archive redirect is stated by form: a relative path to the mod's archive in the same form as the stock `data\ui.pak`. | The file name next to the archive is the implementation's (14 §5.1). | 01 §2.6 |
| A second game process can start while the first is running or exiting; files both write must be safe when they overlap. | A Steam relaunch starts a second process while the first exits. | 01 §2.1 |
| 01 §4.8 points to the v2 address tables (21 §20, 22 §2, 23 §2, 24 §1.1, §2.3); v1 uses none of them, and they are collected into 01 §4 in the revision that applies v2's changes to v1. | Copying them now would duplicate tables their owners still revise. | 01 §4.8; 20 §5, §11 |
| 01 §1.4 and the `GameWindowProc` row state the focus handling by 03 §9.6's required result, not by a mechanism. | A required result leaves the mechanism to the implementation. | 01 §1.4, §4.6 |

### Frames, reflections and render fixes (03, 07, 08)

| Decision | Reason | Where |
|---|---|---|
| The mirror window rule is stated as results: the area to fit, the result for a larger window (no title bar or border, client area equal to the monitor rectangle), the windows left alone and the constraints (never waits for the game thread, no activation or Z-order change, ANSI kept). Win32 calls and style bits are the implementation's. | Clean-room review: the previous text could be implemented only one way. | 03 §9.3 |
| Focus loss is a required result: while a headset is attached, focus loss never reaches the game's handler and everything else reaches it unchanged in ANSI form, at the latest from swapchain creation with a headset. Subclassing is one way. Logged: the fix is active, and each kept focus loss with the executable that took the focus. | Clean-room review: the mechanism, install point and guard are the implementation's. | 03 §9.6; 11 §2.2; 12 §2.2, §7 |
| The OpenVR scene-application check runs about 3 s after the first submission: late enough that a normal start has made the game the scene application, early enough that the player is told while still looking at the headset. | The bound needed its own reason. | 03 §6.7 |
| The descriptor header fields (Handle, Type, ProductName, Manufacturer, the three caps) are free: any values, names non-null strings that live as long as the device. | The previous value table had no requirement behind it. | 02 §4; 03 §3.2 |
| The "headset in use" upright threshold 0.7 has its reason: a headset on a desk or hook is tilted further, while a player looking well up or down still counts. | Each numeric parameter needs a reason. | 03 §2.3 |
| Each of the three swapchain replacements is installed independently, with the consequence of each one missing stated. | One failing interception must not disable the others. | 03 §9.1 |
| The mirror draws to the swapchain's back buffer, binding it itself when nothing is bound at end frame; the view it binds may be kept per back buffer and released before `ResizeBuffers`, never created per frame. If its resources cannot be created it logs once, clears to black every frame and still presents. | No allocation per frame on the render thread; a failed mirror must not stop presentation. | 03 §8.3; 11 §2.4 |
| The mod's own shaders are compiled from HLSL at build time and embedded; there is no run-time compiler; the build fails on a compile error; they must not contain the dither pattern of 08 §1.1, so 08 §1 leaves them unchanged. | No run-time dependency or start-up cost for shaders that never change; 08 §1 recognises the shaders it changes by that pattern. | 03 §8.3; 13 §7 |
| SteamVR install fact (observed by the spec side): the x86 `vrclient.dll` is `<root>\bin\vrclient.dll` and its x86 libraries are in `<root>\bin\win32`. A missing dependency is logged with the file tried and the Windows error. | The runtime's own error code alone does not say which file was missing. | 03 §6.4; 11 §2.4 |
| Required OpenVR interfaces are `IVRSystem`, `IVRCompositor` and `IVROverlay` (missing: start-up failure); `IVRApplications` and `IVRInput` are optional (absence logged). The loader forwards requests to SteamVR, so its release need not match the versions requested. | Neither optional interface is needed to show the game; resolves a conflict between 13 §3.2 and 03, 04. | 03 §6.4; 04 §5.8.2, §5.8.3; 11 §2.4; 13 §3.2, §9 item 23 |
| A SteamVR start-up failure that is not a runtime error (missing loader export, missing required interface, no compositor within the wait) names what was missing with the failing call's code, or the Windows error for the loader; its text is never empty and its code never a bare 0. | Otherwise the player and the log get "unknown error" and code 0. | 03 §6.4; 11 §2.4, §7 item 23; 13 §5.3 |
| The screen-frame mirror's open question is restated from the game: whether a UI render target can be found through the renderer. | The question must come from the game. | 03 §8.1, §14 item 4 |
| Each eye's reflection is rendered from its own eye position (parallax required). | A shared image would show both eyes the same reflection. | 07 §4.6 |
| The second reflection pass leaves the first pass's half intact; skipping every renderer clear in the second pass is one acceptable way. | Result instead of mechanism. | 07 §4.2, §4.3 |
| The remedy for a lost viewport origin in the reflection passes is a result (draw and blur only within half k); where to enforce it is the implementation's. | Result instead of interception points. | 07 §8 item 1 |
| Reflection capture runs only while `DebugOptions` is on and the per-eye render is in effect; otherwise the key does nothing. Acceptance step 5 checks the target by any means, the capture being one. | Matches 11 §4.4 and 13 §3.5. | 07 §6, §7 step 5; 11 §4.4 |
| Head-in-wall delay D = 0.5 s, a project decision (long enough to pass a doorway, short enough that holding the head in a wall still blacks out), to be confirmed by measuring the longest incidental contact in game. "Clear" is the game's value exactly 1.0, no tolerance. | The previous values had no reason of this project's own; HeadClipFade returns exactly 1 when nothing is near, so no tolerance is needed. | 00 §2.1, §4.1; 05 §7.2, §15 item 10; 08 §4.2, §7 |
| The dither fix is stated as properties: its input comes from the position relative to the target and `t`; `\|d\|` is at most 8 for `\|t\|` up to 1000 (project decision: `2^8 · 10000 < 2^24`); it still varies per pixel; its amplitude is unchanged. | The previous text prescribed one formula. | 08 §1.4 |
| The dither failure is restated from the spec side's evaluation: the dither is lost beyond about 3300 pixels from the target's corner. | The spec side's own measurement. | 08 §1.2 |
| The cull function is a table of tests in the spec side's own reading; that `+0x50` is derived from FOV and resolution is [likely], with an open question. | Clean-room review. | 08 §2.1, §2.2, §7 |
| Measurements are labelled by their conditions (all three cull tests bypassed; first 3500 shaders of a session), not by a build. | A measurement holds for its conditions. | 08 §1.4, §2.3 |
| 08 §3.2 names the full key `Rendering.StereoAntiAliasing` and logs the first stereo frame per AA mode as 11 §2.4 does; acceptance checks that need a measurement say "by any means" instead of naming a log line 11 does not define. | One owner for log events (11). | 03 §13 item 18; 08 §3.2, §4.2, §6 |

### Input, game patches, audio and UI resources (04, 05, 10, 14)

| Decision | Reason | Where |
|---|---|---|
| The five pad slots, the connected-flag patch and the left-stick redirect are one patch: if one write fails after its checks passed, the others are restored, the controls area is off and its outcome names the failing site. | They depend on each other: a replaced IsDown virtualises the maps, after which a stock GetValue, GetAxis or ControlToName gets codes it does not know. | 04 §3.3, §9, §10 item 27; 11 §2.1; 12 §4.2, §8 item 13 |
| A virtual code's pad record has flag bit 0x40 clear; an added record copies the flags of a game-built pad record of the same kind in the map (0 if none) with bit 0x40 clear. | Bit 0x40 makes the game read the code as a composite index and never call IsDown. | 04 §2.4, §3.2 |
| Replacements compare only the low 16 bits of a code; in stock-glyph mode an action whose original is "none" gets null. `PadControlToName` takes `int code`. | The game passes record codes zero-extended, and the stock method returns null above 0x17, so the prompt looks as for an unbound action in the stock game. | 01 §4.4; 04 §2.3, §3.1, §3.3 |
| VR activity written for a render frame is seen by every HasActivity call of the next game frame. | A single consume per frame let whichever reader ran first hide activity from the other, so device selection could miss VR use. | 04 §3.3, §10 item 26, §11 |
| Keyboard and mouse are each read whenever their own device exists. | No game or player reason requires both devices. | 04 §7, §8; 05 §3.2b, §3.2e |
| The recenter chord re-arms on release alone; a press during the two virtual-screen re-placements is a new recenter and restarts them. | 04 and 05 disagreed. | 04 §8; 05 §3.2e |
| Prompt glyph mode starts as VR, and VR wins a tie with the pad. | A VR run starts with controllers in hand; a resting pad can report stick noise, while VR activity needs a deliberate press. | 04 §4.2 |
| Thresholds: trigger and analog grip 0.5 (the game's own trigger threshold, pad `+0x04`); centre zone 0.33 and near-head 0.2 m (project decisions with reasons); the 0.5 deflection is owned by 05 §3.2c, the 300 ms repeat by 04 §6. | Each numeric parameter needs a reason and one owner. | 04 §5.1, §6; 05 §3.2c |
| Every analog value written to the game is limited to [-1, 1] (0 to 1 for triggers); the move vector may be limited to length 1 in either scheme and must be in the touchpad scheme. | A runtime can report a stick slightly past 1 or a square-gated diagonal; the game's pad never delivers more. | 04 §5.1, §9 |
| The controls help page is a set (functions with a default control in the current set; touchpad: flashlight only near the head, no weapon selection; thumbstick: both flashlight ways). From stereo device creation a set is always chosen, before a controller is identified too; the set crosses from the render thread to the game thread whole. | The previous table fixed an order and layout; the two sets differ in length, so a mixed read can run past the end. | 04 §3.6, §5.7, §10 item 24; 12 §3.2, §8 item 14 |
| "Options archive in use" for prompts means the VR change set was linked or reused and the pointer redirected in this run. | Otherwise stock movies get VR names they have no image for and show no button. | 04 §4.2, §10 item 25 |
| A stopped, lost or exiting OpenXR session keeps the two toggled grip states. | Released grips are no rising edge. | 04 §5.12 |
| Every stereo-mode change is logged with `DebugOptions`, whatever made it, no later than the next stereo update. | Changes made without a redirected push were missed. | 05 §4.4, §15 item 20 |
| The render-target rebuild flag is a result: raised once per change away from the built values, no later than the next options command or settings push. | Result instead of procedure. | 03 §4.2, §6.5; 05 §10 |
| The event tap is an optional area of the full patch set, installed whatever `DebugOptions` says; `DebugOptions` decides only its logging. | The logging setting can change while the game runs. | 05 §12; 11 §2.1, §5.2 |
| The audio redirect is a required result; where it is placed is the implementation's. It stays after an OpenXR session or instance loss; a vanished headset endpoint falls back as 10 §3.3 says. | The game does not require an interception point; a loss does not end the run (09 §3). | 10 §2.4, §3.2, §3.3, §3.4 |
| The UI archive's key file and temporary files have names of the implementation's choosing; linking runs only in the game's process on a recognised build whose checks passed. | Nothing depends on those names; a run where the mod is not active does no work. | 13 §2, §3.1, §3.6, §6, §8, §9 item 19; 14 §5.1, §5.8, §6 items 1, 12, 19 |
| The crosshair is scaled at the root placement of `crosshairs_mc`. | 14 §3.6.2's rules select the outermost placement, and that placement passes them. | 14 §3.6.4, §6 item 13 |

### Options and distribution (06, 13, README)

| Decision | Reason | Where |
|---|---|---|
| A write of `mothervr.json` leaves on disk either the complete previous file or the complete new one (temporary file in the same folder, replaced in one step). A leftover temporary file is never read as settings, is replaced by the next write and is listed for uninstall. A failed save is logged with the fact that the previous file is kept. | A crash, a Steam relaunch or a power loss during a truncating write left a file that does not parse, so the next launch used the defaults and lost the player's settings. | 06 §3.1, §8 item 23; 11 §2.5; 13 §3.1, §3.4, §6, §8, §9 item 19 |
| Reading `mothervr.json` accepts a leading UTF-8 byte-order mark; the mod never writes one. | Common Windows editors add it, and 13 §5.3 sends players to edit the file by hand. | 06 §3.3, §8 item 22 |
| An external call with a null name pointer passes to the stock handler unchanged. | The mod must not change how the game handles calls that are not its commands. | 06 §4.1 |
| `mvr_vrruntime` with an accepted value always stores it and quits, even when it is already stored. | After a failed start the page offers the stored runtime as a retry. | 06 §4.2 |
| An attach-time message that cannot be shown is logged with its kind, text and reason; device creation does not wait for it; the welcome does not count as seen (`mothervr.json` is not written, so it is retried next launch). | A message the player never saw must neither hold up the game nor count as seen. | 06 §3.1; 11 §2.1, §2.6; 12 §2.4; 13 §5.2 |
| Message box caption is the product name `OpenMotherVR`; the page title stays `MotherVR`. | The player must be able to tell which program speaks; `MotherVR` is allowed only as the page title. | 13 §5.1 |
| The OpenXR failure message for -51 conveys the likely cause given in 09 §7 and that the log shows which runtime manifests were examined. | -51 is diagnosable only from the log. | 13 §5.3, §9 item 11 |
| The build pins the OpenVR SDK release and the SHA-256 of the embedded loader and fails on a missing, non-x86 or different file, never embedding a large-file placeholder. | An embedded placeholder makes every OpenVR start fail on players' machines. | 13 §7, §8, §9 item 17 |
| The statically linked OpenXR loader implements `XR_KHR_loader_init` and `XR_EXT_loader_init_properties`; 09 §1's fallback exists for safety only. | The r6 elevated-process decision depends on them. | 09 §1; 13 §7 |
| Release builds produce the PDB of the shipped DLL; it is kept with the release, never in the zip, and listed in `SHA256SUMS.txt` when published. | A crash record's proxy offsets can be mapped only with the matching symbol file. | 11 §6.2; 13 §7, §8, §8.3, §9 item 18 |
| Version suffix only `-`, and a suffix means prerelease; `+` is not used; a malformed version string fails the build; FILEVERSION carries MAJOR.MINOR.PATCH only. | In Semantic Versioning `+` is build metadata; a wrong or empty version resource must never ship. | 13 §7 |
| Every release build checks the DLL's properties automatically and produces no package when one fails. | Each property, when wrong, breaks every install. | 13 §7, §9 item 14 |
| Release process: tag `v<version>` on a main-branch commit that passed CI; the stamped version equals the tag; a suffix publishes a prerelease; release notes are committed before the tag; republishing a tag replaces the release. | A player must match the file they run to a release and its notes; a re-run must not leave two releases of one version. | 13 §8.1, §9 item 20 |
| Release integrity: hosted CI from the tagged commit, every third-party step and library pinned, read-only build jobs, a signed build provenance statement for the zip and DLL where the host offers one. | The DLL runs inside the game with the player's rights. | 13 §8.2 |
| Each release names where its complete corresponding source is. | GPL-3.0 requires it when object code is conveyed. | 13 §8, §8.3 |
| Package: notices cover transitive libraries and the exact OpenVR release; text files use CR LF; packaging fails on a missing notice; the guide lists every file the mod writes, both runtime options, the `-vrforce*` switches, where to report and what to attach, and where the source is. | Testable rules for what was already required in prose. | 13 §8, §9 items 15, 19 |
| The runtime control shows the runtime running in this session (a rewording of the r6 decision). | Clean-room rewording. | 06 §1.1, §4.3, §5.2 |
| 06 §1 points to the v2 keys (`EnhancedMode`, `Comfort`, `Controls`, `Hands`, `Body`, `Interaction`, `Hud`, `Graphics`), read and written by 06 §3's rules, gated by `EnhancedMode`, kept as unknown keys by v1; the v2 documents own their commands. | One place for a reader of 06 to find them. | 06 §1; README |
| Acceptance checks added where an implementation can lag a correct spec: `-vrscale` parsing, skip intro without the options archive, more bad command values, no display reset with runtime None, settings changes crossing threads, diagnostic dumps in the game folder. | Each requirement was already stated; its check was missing. | 06 §8 items 5, 14, 16, 20, 21; 13 §9 item 24 |

### OpenXR, diagnostics and threads (09, 11, 12)

| Decision | Reason | Where |
|---|---|---|
| Only OpenXR's `EXITING` sets the game's quit flag. A session or instance loss stops VR for the run, the game keeps running on the mirror, and the loss is logged as its own `ERR` event. | A loss is not a request to exit; quitting would cost unsaved progress while the game can keep running. Settles the conflict between 09 §3 and 12 §7 in favour of 09. | 04 §5.9; 09 §3, §5.1, §9, §12 item 2; 11 §2.4; 12 §7 |
| A `REG_EXPAND_SZ` `ActiveRuntime` has its environment references expanded before use. | The loader does the same; a manifest given as `%ProgramFiles%\...` must be found. | 09 §1, §11 item 17 |
| A re-anchor of the base space taken while looking straight up or down keeps the previous anchor's yaw with the new position. | The yaw is undefined there, as for the game recenter. | 09 §6.3, §11 item 18 |
| A per-frame OpenXR call that keeps failing is logged when its result changes; a failure during frames never changes what a start-up failure message reports. | No per-frame logging on the render thread (12 §6.1); the start-up message is about start-up. | 09 §7, §11 item 19 |
| The OpenXR failure message's content is an unordered list of facts. | Order is the implementation's (13 §5). | 09 §7, §11 item 14 |
| The not-shown check's bound is about 10 s, measured so a wall-clock change cannot trigger it; the timer and latch are the implementation's. | Result instead of procedure. | 09 §3.1, §7; 12 §7 |
| The crash record is specified by what it must convey, unordered: the instruction pointer always, the registers as one JSON object, and enough stack words for at least the faulting function's caller and that caller's caller. The split into events and the register order are the implementation's. Replaces the r6 "16 stack words". | The r6 text could be implemented only one way. | 11 §6.1 |
| A stack overflow is recorded: writing the record uses at most 2 KiB of the faulting thread's stack, or another thread writes it while the faulting thread waits. | Windows leaves about one 4 KiB page after an overflow on x86, part of which dispatch uses; half of it leaves room for both. | 11 §6.1, §7 item 13; 12 §7 |
| The snap trace ends when the view has settled (both yaw fields equal and the head offset changed by under 1 mm since the previous update), at the latest 1 s after the step; it reports the yaw, the mirror field's difference from it and the head's horizontal offset as millimetres and a bearing. Replaces the r6 600 ms window and field list. | An end condition is a result; 1 s is longer than the reported slide. | 11 §4.5, §7 item 16 |
| An exception caught at a mod entry point is logged once per entry point, and the call returns the slot's no-device result or passes to the game's original. | 12 §7 required catching but not the result. | 12 §7, §8 item 16 |
| A page protection that cannot be restored after a successful write: the write counts as done and is logged with the address and the Windows error. | The page staying writable does not change the game. | 12 §4.3 |
| New `ERR` events: OpenXR session or instance lost; desktop mirror resources unavailable; the runtime's client library or a dependency cannot load; an OpenVR start-up failure without a runtime error. New `INF` events: the OpenVR interface versions obtained; the host, when it is not the game. New `DBG Stereo` event: each ended head-in-wall contact with its length and whether it darkened (for 08 §4.2's measurement). | Each is a state a tester must see in the log. | 11 §2.1, §2.4, §4.6, §7 items 22 to 24 |
| Templates rechecked against their must-convey lists: window made borderless (new size, then old), focus handling active, focus taken (executable only), registry views, headset scene check (a pass line, so a check that passed differs from one that never ran), session not shown; first launch says whether the welcome is queued; pose-age properties `{OnTime}`, `{Late}`; crash and snap events written fresh. | Clean-room review of 11. | 11 §1.4, §2.1 to §2.4, §4.1, §4.5, §6.1 |
| Messages decided at attach follow 12 §2.4; the timing of the others is 13 §5.2. | 12 §2.4 covers only attach-time messages. | 11 §2.6 |
| 11 §5 is titled "Logging of game script events"; the area label "event tap" and the label `ScriptEventTap` stay. | The heading names what is logged, not the mechanism. | 11 §5; 12 §2.2, §4.2, §7; 21 §2.10; 22 §2.3, §2.5, §19 item 3; 23 §2.1 |
| 12 §2.3 states the UI archive's time limits by reference to 14 §5.10 instead of measured times. | One owner for the limits. | 12 §2.3 |

### v2 documents (20 to 24)

| Decision | Reason | Where |
|---|---|---|
| `EnhancedMode` (22 §3.1) is the single master switch for every v2 key of 21 to 24; while off, every v2 key reads as its v1 value and stored values are kept; a next-launch key follows the value at that launch. `VRRuntime`'s default is not gated. [proposal, pending user] | One switch tells a v2 problem from a v1 one and keeps an upgrade at v1. | 20 §1.3, §8, §9; 21 §1, §15; 22 §1, §3.1; 23 intro; 24 §10 |
| Seated and standing play and the height calibration have one owner, 22 (`Comfort.PlayMode`, `Comfort.StandingEyeHeight`; the mode applies at the next recenter; a 3 s countdown and an automatic first calibration). 21 drops its posture and standing-height keys, reads 22's and keeps only the seated eye raise. | The same settings were defined twice with different rules; one key per setting (20 §5). | 20 §2, §4, §5, §10 item 4; 21 §1, §11.4, §11.5, §15, §18, §21, §22 item 22, §23; 22 §3.1, §6.1 |
| The engine settings override (24 §6) is the one v2 feature that still applies after a runtime failure. [proposal] | It changes the game's option list at start-up, before the runtime's outcome is known, cannot be undone after the parse, and is useful in flat play. | 20 §1.2; 24 §11.1 |
| v2 game addresses are listed in the address table of the v2 document that uses them; 01 §4 collects them in a later revision. | Each v2 document keeps its own table while it is revised. | 01 §4.8; 20 §5, §11 |
| Feature and key names chosen afresh by this project: `Body.EyeForward`, `Body.SeatedEyeRaise`, `Hands.MeleeByMotion`, `Hands.ThrowByMotion`, `Hands.ThrowStrength`; the terms "free head" and "control hand-back"; the labels `CharacterRequestCrouch`, `CharacterRequestStand`, `CharacterRegistry`, `CharacterIkStage`, `FinalPoseThunk`, `CameraPosition`, `CameraForward`. | Names in the spec are this project's own. | 20 §2, §3.1, §7, §8, §10; 21 |
| The v2 crash event is logged at `ERR`, the level of v1's crash record, and is stated as a state (both on disk by the time the exception leaves the mod's handling). | Both describe one crash; the previous text followed a sequence. | 22 §14, §16, §18 item 16, §20 |
| During 23's lever mapping no turning happens whatever `Controls.InteractionAwareTurning` says, and 22's d-pad emulation does not take the left stick. | The mapping owns the grips and the move and look actions (one owner per input, 20 §5); a pull made while the world turns is lost. | 22 §4.1, §7, §17, §20; 23 §3.2, §12 item 11, §13 |
| Terminal text scale steps are 1.0, 1.25, 1.5 and 1.75 (each adding 5 px: 25, 30, 35 px). [proposal] | Quarter steps are visible at once; 1.75 keeps about 30 characters per line in the 620-unit body field. | 23 §6.2, §8, §11 item 6 |
| The reduced camera-takeover set is defined by a criterion (first-person animations played at an interaction marker in front of a device without travel), enumerated from `ANIMATION.PAK`. | A list must follow from this project's own rule. | 23 §4.2, §4.6, §12 item 12 |
| Item highlight off is a result (no highlight, prompts unchanged); the means is the implementation's under 12 §4. | The previous text prescribed one internal value. | 23 §7.3 |
| The eye forward offset is not applied during interactions whose screen the game frames for a fixed eye point; which kinds is to be observed in game. [proposal] | Replaces a fixed pair of interaction kinds with no observation behind it. | 21 §11.2, §19, §22 item 24 |
| Melee by motion: a swing by hand hits what the same swing by Fire would hit; whether the arm must play the game's animation is open. | The previous claim about the game's hit test was not verified. | 21 §8.2, §22 item 13 |
| Wrist health: a continuous green-to-red colour, blinking at or below one fifth, each with this project's reason. [proposal] | Thresholds and steps without reasons removed. | 21 §13.2 |
| The acceptance value for the snap repeat is 600 ms. | Any test value works; 600 ms is half v1's rate. | 22 §18 item 3 |
| 24 no longer calls v1's three forced settings an exception to "saved settings never changed", and no longer asserts that the setter changes saved settings (02 §11 item 5 is open). | Follows the 02 §6.5 decision above. | 24 §1.1, §8 |
| Cross-references corrected: 21 §3.1 (03 §1 and 05 §1 for the view matrix convention), 21 §17 (04 §5.9 and this document's §14.3), 23 §3.1 (controls help, 04 §5.7). | The cited sections did not exist or did not cover the claim. | 21 §3.1, §17; 23 §3.1 |

Open after spec r7: whether `SettingsSetOption` (0x5A8910) saves the
values it applies (02 §11 item 5); the meaning of display modes other
than 0 and 2 (03 §14 item 2); whether follow-camera `+0x1CC` is world
space (04 §11, 11 §8); the measured longest incidental head-in-wall
contact (08 §4.2) and the writer of cull context `+0x50` (08 §7); the
virtual screen on an unrecognised build (09 §12); every new 2020 value.
Two inconsistencies remain for the next revision: 03 §9.6 asks the
focus-loss line to carry the process id while 11 §2.2 conveys the
executable only, and 24 §1.1 labels the global at 0x13583E4 `RendererPtr`
while 01 calls it `RendererEyeOwner` (01 §4.7) and gives `RendererPtr` as
0x17DF5CC (01 §4.3).

Documents in spec r7:

| Document | Revision |
|---|---|
| `00-overview.md` | r8 |
| `01-loading.md` | r7 |
| `02-game-stereo-path.md` | r6 |
| `03-hmd-and-frames.md` | r7 |
| `04-input.md` | r7 |
| `05-game-patches.md` | r7 |
| `06-options-and-config.md` | r7 |
| `07-reflections.md` | r4 |
| `08-render-fixes.md` | r5 |
| `09-openxr.md` | r6 |
| `10-audio.md` | r4 |
| `11-diagnostics.md` | r5 |
| `12-threads-and-robustness.md` | r5 |
| `13-distribution.md` | r5 |
| `14-ui-resources.md` | r4 |
| `20-v2-overview.md` | r2 |
| `21-hands-and-body.md` | r3 |
| `22-controls-and-comfort.md` | r4 |
| `23-interactions-and-hud.md` | r3 |
| `24-rendering-v2.md` | r3 |
| `README.md` | r8 |
| `ANSWERS.md` | r5 |
