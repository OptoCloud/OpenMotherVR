<!-- Provenance: spec side's answers to the implementation side's QUESTIONS.md r1 (questions on spec r2); user decisions for spec r3; AI.exe 2015 analysis (mode-stack container); DXGI public API semantics; user decisions for spec r4 (2026-10-14); project decisions for spec r5 (clean-room review of every document); a tester's report on SteamVR with a Steam Frame headset and project decisions for spec r6 (2026-10-15); AI.exe 2015 analysis by the spec side for spec r6 (the game's window procedure on focus loss and deactivation; the five pointer-table slots holding ScriptEventTap's address). Revision: r4 -->

# Answers and decisions (spec r3 to r6)

This file records the spec side's answers to the implementation side's
questions (spec r3) and the decisions taken for spec r4 and r5. Each entry
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
