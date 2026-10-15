<!-- Provenance: user decision (the project's own log format of section 1, modelled on the Serilog file sink: output template, levels, source contexts, message templates, session start events, rotation; a project design, not taken from any mod); project decisions of this revision (every message template of sections 1 and 2 written fresh by the spec side from its "must convey" list; the outcome table of the event tap; the bounds of section 3), each with its reason inline; user requirements (the log is what players send with bug reports; no per-frame render-thread logging; the DebugOptions diagnostics; the mod never needs elevation); observation in game of test builds (which facts testers needed from a log across runs with OpenVR and OpenXR on SteamVR with Index; measured cost of opening the log per line on the render thread; a frame loop that rendered with views one display period old); AI.exe 2015 analysis (the script event tap: bytes and call-site count checked against the Steam 2015 binary, PE timestamp 0x54E3199D; the game's own event strings); public Win32, DXGI, OpenVR, OpenXR and Serilog documentation; AI.exe 2015 analysis by the spec side for r3 (ScriptEventTap's address also in five pointer-table slots of .rdata); the OpenXR registry (symbolic XrResult names); a tester's report on SteamVR with a Steam Frame headset (an OpenXR failure with an empty result text, the game not shown in the headset, focus taken by other applications, a view sliding after snap-turn steps, a crash in the pause menu with nothing in the log); project decisions of r3, each with its reason inline (the crash record; the image-range guard of the event tap; the stereo-mode, frame-kind and snap-turn diagnostics; the new required events and their templates, written fresh by the spec side); AI.exe 2015 analysis by the spec side for r4 (the C runtime's own unhandled-exception filter installed by its start-up after DLL attach and its two reporting paths; the padding after ScriptEventTap; the first-person camera's character position and Head1 bone position fields); project decisions of r4, each with its reason inline (the crash record's content as an unordered list, its filter installed again after the game's C runtime starts, a bounded stack use for a stack overflow; the snap trace written fresh from its must-convey list and ended when the view settles, at most 1 s; templates rechecked against their must-convey lists; the OpenXR loss and desktop mirror failure events; acceptance checks for required events); project decisions of r5 (spec r7 cross-check), each with its reason inline (the event for a process that is not the game; the event for a message that could not be shown; the OpenVR interface versions and the OpenVR start-up failures that are not runtime errors; the stereo device creation called once by the game; the settings write leaving the previous file complete; the head-in-wall contact diagnostic; section 5 named the logging of game script events; corrected cross-references to 07 and 13). Revision: r5 -->

# 11. Diagnostics

The mod's log file and its format, what must be logged and when, the rate
rules for the render thread, the extra diagnostics behind the `DebugOptions`
setting, the logging of game script events, the crash record, and what a player
or tester needs to report a crash.

The log is the only channel through which a tester or the implementation
team learns what happened in a player's game. Every decision the mod takes
on its own (build, runtime, patch set, sizes, failures) must be visible
there, once, so one log from a player is enough to tell which path the mod
took.

This document owns all log wording and format. The other documents say
which events they need logged and what each must convey; the templates for
those events are here (§2). A document that defers an event to this one
and is not listed in §2 is a gap in this document, not permission to
invent a template elsewhere.

Addresses are RVAs (01 §3.3). A 2020 value marked "?" is unknown.

## 1. The log file

### 1.1 File

| Item | Requirement |
|---|---|
| Name | `mothervr.log`. The name is kept because players are told to send that file (13) |
| Directory | the directory of the proxy DLL (the game directory, next to `AI.exe`), found from the DLL's own module handle, not from the current directory (Steam may start the game with another one) |
| Path API | wide-character (UTF-16) file APIs, so an install path with characters outside the ANSI code page works |
| Encoding | UTF-8, no byte-order mark. Every path written into the log is converted from UTF-16 to UTF-8; a failed conversion writes an empty value and never throws |
| Line end | CR LF, so the file opens cleanly in Notepad |
| Event size | bounded, so an event can be rendered without allocating (§1.2). Project decision: 4096 bytes, line ends included, which holds the longest event of §2 with two long paths. A longer event is cut at a UTF-8 character boundary and marked as cut (`...` before its line end) |

If the directory cannot be determined or the file cannot be opened (for
example a game folder the player's account cannot write, 12 §7, 13 §3.6),
each event is sent to the debugger output (`OutputDebugStringW`) instead,
rendered the same way, so a developer with a debugger or a system-wide
debug-output viewer still sees it. Logging must never be a reason for the mod or the
game to fail, and the mod never asks for elevation to write the log.
Nothing that can fail runs at attach before the log is opened (or found
unavailable).

### 1.2 Shared by two processes

Starting the game can run `AI.exe` twice: a first process that has Steam
relaunch the game, and the game process. Both load the proxy, so both
write to the same file at the same time [sure, observed: two process ids in
one log within the same second].

- The file is opened once per process at `DLL_PROCESS_ATTACH`, before
  anything else the mod does, and kept open until the process ends (the OS
  releases the handle at exit).
- Open mode: append-only data access (`FILE_APPEND_DATA`), share read,
  write and delete, open-or-create (`OPEN_ALWAYS`). With append-only
  access each `WriteFile` is an atomic append (a Windows file-system
  property), and the player can open or copy the file while the game runs.
- Each event, all its lines and line ends included, reaches the file in
  **one** `WriteFile` call, so events of the two processes interleave whole
  and are never torn.
- Opening the file per event is not acceptable: it measured about 0.1 ms
  per line, and one line took 250 ms during a level load, on the render
  thread (12 §6).
- Writing an event allocates nothing (§1.1 event size).

### 1.3 Rotation

At session start, before its first event, a process that finds the file
larger than **8 MiB** (project decision: several hundred sessions of normal
logging, still small enough to attach to a bug report and open in Notepad)
moves it to `mothervr.log.1`, replacing an older one, and starts a new
`mothervr.log`. It does so only when no other process has the file open;
otherwise, or if the move fails, it appends to the existing file. The log
is otherwise never truncated: a truncation at attach would erase the other
process's events. Exclusive open followed by a rename through that handle
is one way to tell that no other process uses the file.

### 1.4 Event format

Each event is rendered as the Serilog output template

```
{Timestamp:yyyy-MM-dd HH:mm:ss.fff zzz} [{Level:u3}] [{ProcessId}:{ThreadName}] {SourceContext}: {Message:lj}{NewLine}{Exception}
```

with these fields:

| Field | Format |
|---|---|
| `Timestamp` | local time with its UTC offset, for example `2026-10-14 21:46:24.293 +02:00` (offset as `+hh:mm` or `-hh:mm`) |
| `Level` | three uppercase letters, §1.5 |
| `ProcessId` | decimal, no padding |
| `ThreadName` | `main` (the game thread, which is also the thread that runs attach), `render` (the game's render thread), `job` (a game job thread), `mod` (a thread the mod itself created, if the implementation creates any), or `other`. The game thread's id is known at attach; the render thread's at the first stereo BeginFrame or the first `Present` of the game's swapchain (12 §2) |
| `SourceContext` | `OpenMotherVR.<Area>`, Area from §1.6 |
| `Message` | the event's message template (§2) rendered with `:lj` semantics: string values literal, without quotes; numbers in the invariant culture (dot as decimal separator, no group separators), with the precision the template gives; booleans `True` / `False` as Serilog renders them; other structured values (objects, arrays) as JSON |
| `Exception` | empty, or, for an event caused by a Windows or runtime error, one further line, unindented, ending in a line end, that gives the kind of error (`HRESULT`, Win32 error, `EVRInitError`, `XrResult`), its value, and the system's or runtime's own description of it (`FormatMessageW`, `VR_GetVRInitErrorAsEnglishDescription`, `xrResultToString`), for example `HRESULT 0x887A0004: <system description>`. The description is never empty: for an `XrResult` with no instance to ask (a failure of `xrCreateInstance` or of a call before it, where `xrResultToString` cannot be used), it is the result's symbolic name from the OpenXR registry, for example `XR_ERROR_RUNTIME_UNAVAILABLE`, and a value the registry does not define is described by its number and a text saying that it is not a defined result (09 §7) |

Hex values are written as `0x` with uppercase digits; sizes as `WxH`;
paths as plain text (literal strings); timestamps of `AI.exe` as `0x`
and 8 hex digits.

Example (written for this spec, using the window-fit event of §2.2):

```
2026-10-14 21:46:24.293 +02:00 [INF] [50300:render] OpenMotherVR.Frame: Mirror window made borderless over its monitor: 2560x1440, was 2576x1497
```

### 1.5 Levels

| Level | Meaning |
|---|---|
| `VRB` | the periodic diagnostics (§4); only while `DebugOptions` is on |
| `DBG` | other debug detail; only while `DebugOptions` is on |
| `INF` | a state change |
| `WRN` | degraded behaviour, or a fallback was taken |
| `ERR` | a feature is off or broken |
| `FTL` | the mod disabled itself (the game runs unmodified) |

### 1.6 Source contexts

| Area | Covers |
|---|---|
| `Load` | the proxy, build detection, start-up, runtime choice, messages to the player, the crash record (§6) |
| `Patch` | the preflight and every game patch |
| `Stereo` | the stereo device and the game's stereo path (02, 03, 05, 07, 08) |
| `Frame` | frame submission, the swapchain, the desktop mirror, the shader patch |
| `OpenVR` | the OpenVR runtime |
| `OpenXR` | the OpenXR runtime |
| `Input` | controllers, SteamVR Input, OpenXR actions, the gamepad (04) |
| `Config` | the config file and setting changes (06) |
| `UI` | the options menu and the options UI archive (06, 14) |
| `Audio` | audio to the headset (10) |
| `Diagnostics` | the `DebugOptions` diagnostics (§4) |

### 1.7 Session start

The first two events of each process, after any rotation (§1.3). In a
process that is not the game (01 §3.1) neither is written: the host event
of §2.1 is that process's only event apart from the detach event.

**Attach event** (`INF Load`).

Must convey: that a process attached and which kind it is (§1.8); the mod's
release version; the `AI.exe` build (2015, 2020 or unknown) and its PE
timestamp, always (it is what identifies an unknown build); the Windows
build number, read with `RtlGetVersion` (not subject to manifest
compatibility shims); the runtime in effect after command-line overrides
and whether the config file or the command line decided it; the install
directory.

Template:

```
{ProcessKind} process attached; mod {Version}; game {AiBuild} (timestamp {AiTimestamp}); Windows {WindowsBuild}; runtime {Runtime}, from {RuntimeSource}; folder {InstallDir}
```

`ProcessKind` is `Game` or `Launcher` (§1.8); `AiBuild` is `2015`, `2020` or
`unknown`; `Runtime` is `None`, `OpenVR` or `OpenXR`; `RuntimeSource` is
`config` or `command line`.

**Settings event** (`INF Config`).

Must convey: where the settings came from (the file, defaults on a first
launch, or defaults because the file could not be used); the value in
effect of every setting of 06 §1 after sanitising, under its
`mothervr.json` key path (the key names are the compatibility names, so a
tester can compare the line with the player's file); the world scale from
`-vrscale` (1 when absent).

Template:

```
Settings in effect ({ConfigSource}): {Settings}
```

`ConfigSource` is `file`, `defaults, first launch` or `defaults, file
unusable`; `Settings` is a JSON object keyed by the key paths of 06 §1
(`VRRuntime`, `Rendering.PixelScale`, `Input.SnapRotation`, ...), plus
`-vrscale`.

### 1.8 Telling the two processes apart

`ProcessKind` is `Game` when Steam started the process (the environment
variable `SteamGameId` or `SteamAppId` is set), `Launcher` otherwise
[likely: a process started outside Steam is the one that asks Steam to
relaunch the game; see §8].

## 2. Required events

Each event is logged once when it happens, or once per change of the value
it reports (§3). The templates are the project's own, written for this
spec from the "Must convey" column; they stay the same across versions so a
log can be searched for them. A template conveys everything in its "Must
convey" cell; where the two seem to differ, the "Must convey" cell is the
requirement and the template is fixed to match it.

Template conventions: properties are in braces; `{X:0.00}` gives the
number of decimals; a template starts with the thing the event is about,
then its outcome, then details separated by semicolons. Text values whose
wording is not given here (a reason, a step name, a consequence) are the
implementation's own short phrases. "Error" in the "Must convey" column
means the event carries the `Exception` part of §1.4.

### 2.1 Start-up and patching (attach, `main`)

| Level | Area | When | Must convey | Template |
|---|---|---|---|---|
| INF | Load, Config | first two events of each process | §1.7 | §1.7 |
| INF | Load | at attach, in a process that is not the game (01 §3.1), instead of the events of §1.7 | the executable's file name of the host; that the proxy only forwards the Direct3D 11 exports there (no settings, messages, filter or patches), so a log from a test harness or another program is not mistaken for a failed game start | `Not the game ({HostExecutable}): forwarding Direct3D 11 only` |
| WRN | Config | `mothervr.json` exists but cannot be read or parsed (06 §3.1, §3.3) | its path; why (the Windows error, or where parsing stopped); that defaults are used | `Config {Path} ignored, defaults apply: {Reason}` |
| INF | Config | `mothervr.json` does not exist (first launch, 06 §3.1) | its path; that this is a first launch; whether the welcome message is queued and the file written once it is closed, or neither because the mod is not active on this run (06 §3.1) | `Config {Path} absent: first launch; {WelcomeOutcome}` |
| FTL | Load | `AI.exe` is not a recognised build (01 §2.4) | the PE timestamp; that nothing is patched and the game runs as shipped | `Game executable not recognised (timestamp {AiTimestamp}): no patches, game runs as shipped` |
| INF | Patch | a build was recognised | the build; the image base | `Game build {AiBuild} recognised; image base {ImageBase}` |
| INF | Patch | the patch set was decided (01 §2.3) | which set (full, or the UI set for runtime None); the runtime that decided it | `Patch set {PatchSet} for runtime {Runtime}` |
| ERR | Patch | one per core site whose check fails (12 §4) | the site's spec label (01) and RVA; which kind of check failed (12 §4.1); what was found there (the bytes, the callee, the string or the slot value) | `Check failed at {Site} ({Rva}): {Check}; found {Found}` |
| FTL | Patch | after the failed checks above | how many core checks failed; that nothing was written and the game runs as shipped | `{Failed} core check(s) failed: no patches, game runs as shipped` |
| INF / WRN | Patch | the outcome of each feature area at attach (below) | the area; whether it is in place; when in place, on how many sites; when off, why: the failing site's spec label, RVA and kind of check, or that this build has no address for it, or that the patch set does not include it | `{Area}: {Outcome}` (INF when in place, WRN when off) |
| ERR | Patch | a write fails after its checks passed (12 §4.2) | the area and site (label, RVA); the Windows error; that the game is now partly modified: earlier patches stay, the remaining core patches are not attempted | `Writing {Area} at {Rva} refused by Windows; earlier patches stay, later core patches skipped` (error) |
| INF / WRN | Patch | `SkipStartupIntro` is on (06 §6) | whether the intro will be skipped this launch; if not, why (the options archive is not in use, or the site check failed) | `Startup intro: {Outcome}` |
| INF | UI | `MOTHER.PAK` was current and is reused (14) | its path; that no rebuild was needed | `Options archive {Path}: up to date, reused` |
| INF | UI | `MOTHER.PAK` was built or rebuilt (14) | its path; the `UI.PAK` it was built from; how many entries it has; how long it took | `Options archive {Path}: rebuilt from {Source}; {Entries} entries; {Elapsed} ms` |
| ERR / WRN | UI | the options archive cannot be produced (14 §5.9) | the cause category 14 defines (missing, not a valid archive, unknown version with the entry, link or layout check with its detail, write failure with the Windows error; WRN for the busy case); that the options page is off for this session; that VR is unaffected; whether a message is shown | `Options page off this session: {Reason}; VR not affected` (error for a write failure) |
| INF | Input | the gamepad's action maps were taken over (04) | the digital and analog action counts of each map, in map order | `Game action maps ready for VR controls: {Maps}` (`Maps` as `41/6, 19/6, 32/8`, digital/analog per map) |
| INF | Load | every message shown to the player (13 §5) | which message (§2.6); the full text shown | `Shown to the player ({Kind}): {Text}` |
| WRN | Load | a message to the player could not be shown at all (12 §2.4, 13 §5.2) | which message (§2.6); the full text it would have shown, so the log still tells the player's situation; error; for the welcome, that `mothervr.json` is not written and the welcome is tried again at the next launch | `Could not show the player ({Kind}): {Text}; {Consequence}` (error) |
| INF | Load | `DLL_PROCESS_DETACH` (12 §7) | whether the process is ending or the DLL is being unloaded | `Detaching ({DetachKind})` (`process exit` or `unload`) |

**Feature areas.** One outcome event per area, so a log shows at a glance
which parts of the mod are in place. The areas (spec labels; the
implementation may word them differently but keeps one stable name per
area):

| Area | Covers | Patch set |
|---|---|---|
| options UI | UI archive redirect, the menu's call slot, the settings push in the menu Open wrappers (01 §2.3, 06 §4) | both |
| stereo path | the stereo hand-off and device (02) | full |
| game behaviour | the patches of 05 | full |
| controls | the gamepad replacements (04); they are in place only as a whole, so when one of them cannot be written the outcome says the area is off and which slot failed (04 §3.3, 12 §4.2) | full |
| per-eye reflections | 07; when off, the reason names which of its sites failed (for example the clear gate or the reflection render call) | full |
| stereo anti-aliasing | 08 §3 | full |
| culling | 08 §2 | full |
| head-in-wall delay | 08 §4 | full |
| event tap | §5 | full |

Areas the patch set does not include (runtime None) are reported once each
as off for that reason, so a log of an unmodded run is not mistaken for a
failed one.

### 2.2 Direct3D 11 proxy

| Level | Area | When | Must convey | Template |
|---|---|---|---|---|
| INF | Load | the system `d3d11.dll` was loaded (first forwarded call) | its full path | `{Path} loaded as the system Direct3D 11` |
| FTL | Load | the system `d3d11.dll` cannot be used | why: no system directory, the load failed, or the loader returned the proxy itself; error | `Direct3D 11 forwarding impossible: {Reason}` (error) |
| INF | Load | after loading | how many exports were found and how many are absent | `Forwarded exports: {Resolved} found, {Missing} absent` |
| WRN | Load | each export of 01 §1 the system DLL lacks | its name | `Forwarded export {Name} absent from the system library` |
| INF | Frame | the game's swapchain was created | windowed or not; buffer count; size; format; usage; flags | `Game swapchain ready; windowed {Windowed}; {Buffers} buffers of {Size} in {Format}; usage {Usage}; flags {Flags}` |
| INF | Frame | the swapchain request was changed for a headset (03 §9.2) | what the game asked (size, fullscreen) and what was used instead | `Game swapchain request adjusted for the headset: asked {Requested}, fullscreen {AskedFullscreen}; given {Size}, windowed` |
| ERR | Frame | a vtable slot could not be replaced | which method (`Present`, `ResizeBuffers`, `SetFullscreenState`, `CreatePixelShader`); error | `Interception of {Method} failed` (error) |
| INF | Frame | the first `Present` of the game's swapchain; no periodic event | that the first frame reached the display path | `Present reached for the first time` |
| INF | Frame | each `ResizeBuffers` of the game's swapchain (03 §9.2) | the size asked and the size used; buffer count; format; flags | `ResizeBuffers: asked {Requested}, using {Size}; {Buffers} buffers; {Format}; flags {Flags}` |
| INF | Frame | the game window did not fit its monitor and was made borderless over it (03 §9.3) | the new size (the monitor's) and the size it had before, so a tester can see why the window was changed | `Mirror window made borderless over its monitor: {To}, was {From}` |
| INF | Frame | once, when the game window's focus loss starts being kept from the game (03 §9.6); logged even if no focus loss follows, so a log shows the protection is in place when a tester later sees a pause | that focus loss is now kept from the game | `Focus-loss shield on for the game window` |
| INF | Frame | each focus loss kept from the game (03 §9.6) | the application that took the focus (its executable's file name, or that it is unknown), because that names what to close or configure; that the game kept running | `Focus taken by {Executable}; game kept running` |
| WRN | Frame | the game asked for fullscreen with a headset attached; once per process (03 §9.4) | that the request was refused and why | `Request for fullscreen denied, headset attached` |
| INF | Frame | the dither fix of 08 §1 changed a shader: at the first one, then occasionally, at a bounded rate (once per level load, or every few hundred shaders) so a level load adds only a few lines | how many pixel shaders the game created so far, how many were changed, how many changed ones the device refused | `Dither fix: {Patched} of {Created} pixel shaders changed so far; {Rejected} refused by the device` |
| WRN | Frame | the device refused a changed shader; a few at most per process (the count is in the event above) | that the game's original bytecode was used instead; error | `Device refused a dither-fixed shader; the original was created instead` (error) |

### 2.3 Runtime choice (`main`, when the game creates its stereo device)

| Level | Area | When | Must convey | Template |
|---|---|---|---|---|
| INF / WRN | Load | OpenVR availability decided (06 §1.1) | available or not; when available, the runtime folder; when not, why (the paths file missing or not parseable, no runtime entry, no 32-bit loader in the runtime folder) | `OpenVR availability {Available}: {Detail}` (WRN when not available) |
| INF | Load | the OpenXR registry views were read (09 §1) | the 64-bit view's `ActiveRuntime` and the 32-bit view's, each the path or that the view has none | `OpenXR ActiveRuntime: 32-bit view {View32}; 64-bit view {View64}` |
| INF | Load | each OpenXR runtime manifest examined (09 §1) | the manifest; the library it resolves to; that library's PE machine; whether it is a 32-bit sibling of a 64-bit registration | `OpenXR manifest {Manifest} leads to {Library} (machine {Machine}, sibling {Sibling})` |
| INF / WRN | Load | OpenXR availability decided | available or not; the manifest chosen and the rule that chose it (the player's override variable, the 64-bit view's manifest, its 32-bit sibling, or the 32-bit view's entry), or why none (no runtime registered, no x86 build of any registered runtime) | `OpenXR availability {Available}: {Detail}` (WRN when not available) |
| INF / WRN | OpenXR | the chosen manifest was handed to the loader's initialisation (09 §1) | accepted; or why not (the loader has no initialisation entry, or refused the property; error) and that only the environment variable points the loader at the manifest | `Loader directed to {Manifest}: {Outcome}` (WRN when not accepted) |
| WRN | Load | once, when OpenXR will start and the game process is elevated (09 §1); a tester needs it because it is the one case where the environment variable alone cannot reach the loader | that the game runs with administrator rights; that only the loader's initialisation property can point the loader at the runtime | `Administrator rights detected; only the init property directs the OpenXR loader` |
| INF / WRN | Load | the runtime was chosen (06 §1.1) | the preferred runtime and whether it was forced; the runtime that will start, or that none will; why (as configured, fallback because the preferred one is not installed, forced one not installed, none installed, runtime None) | `VR runtime decision: preferred {Preferred}, forced {Forced}; starting {Chosen}; {Reason}` (WRN for a fallback or no VR) |

### 2.4 Runtime, session and stereo state

| Level | Area | When | Must convey | Template |
|---|---|---|---|---|
| INF | Stereo | each call of the game's stereo device creation (03 §6.5) | which call this is (first, or a repeat, which is not expected: the game calls it once, 02 §6.1); whether a device is attached afterwards | `Game asked for its stereo device (call {Call}); device attached {Attached}` |
| INF / ERR | OpenVR | the bundled `openvr_api.dll` (13 §3.2) | its path; written, or already current; on failure the Windows error and which loader will be used instead | `Bundled loader {Path}: {Outcome}` (error on failure) |
| INF | OpenVR | the OpenVR loader was loaded | which file was loaded (the bundled copy or the runtime's own) | `OpenVR loader in use: {Path}` |
| ERR | OpenVR | the runtime's client library, or a library it depends on, cannot be loaded (03 §6.4) | the library's path; the Windows error, so a missing dependency names a file instead of leaving only the runtime's own error number | `Runtime client library {Path} cannot load` (error) |
| INF / ERR | OpenVR | `VR_InitInternal2` returned | success, or failure with the `EVRInitError` (error) | `OpenVR session opened` / `OpenVR session could not be opened` (error) |
| ERR | OpenVR | an interface version the mod needs is unavailable (13 §3.2, a start-up failure) | the interface version string | `Runtime lacks interface {Interface}` |
| INF | OpenVR | once, after the interfaces were obtained (03 §6.4) | each interface version string obtained, so a log shows which SDK level the runtime served | `OpenVR interfaces: {Interfaces}` |
| ERR | OpenVR | the OpenVR start-up failed without an error from the runtime (03 §6.4): the loader could not be loaded, a loader export is missing, or no compositor appeared within the bounded wait | what was missing (the loader file, the export's name, or the compositor); the code the failing call returned, or the Windows error when the loader itself could not be loaded, never an empty text or a bare 0 (error) | `OpenVR start-up failed: {Missing} unavailable` (error) |
| WRN | OpenVR | `IVRApplications` is unavailable, so the scene check of 03 §6.7 is skipped | the interface; that VR is otherwise unaffected | `No {Interface}: cannot check which application the headset shows` |
| INF / WRN | OpenVR | the scene check of 03 §6.7, once per process; the INF result is kept so a log tells a check that passed from one that never ran | INF that the game's frames are the ones shown; WRN otherwise, naming what is shown instead (the executable's file name of that application, that no application holds the scene, or that it is unknown) and that a warning follows | `Headset shows the game's frames` / `Headset shows {SceneOwner} instead of the game; warning the player` (WRN) |
| INF | OpenVR | after a successful start | the headset's manufacturer and model; the recommended per-eye size; the IPD in metres to 4 decimals (0.1 mm) | `Headset {Model} by {Manufacturer}; recommended eye size {EyeSize}; eye separation {Ipd:0.0000} m` |
| ERR | OpenVR | the overlay of the virtual screen could not be created | error | `Virtual-screen overlay unavailable` (error) |
| INF | OpenVR / OpenXR | the symmetric FOV used for rendering was set or changed (03 §3.2, 09 §4) | the vertical and horizontal tangents to 3 decimals; where they came from; whether they are provisional | `Rendering FOV from {Source}: tan vertical {Up:0.000}, tan horizontal {Side:0.000}; provisional {Provisional}` |
| INF | OpenXR | the instance was created | the runtime's name and version | `OpenXR instance on {Name} {Version}` |
| INF | OpenXR | the system was found | the system name; the recommended per-eye size; where the FOV came from | `OpenXR system {Name}: recommended eye size {EyeSize}; FOV from {FovSource}` |
| ERR | OpenXR | an OpenXR call whose failure ends or degrades VR (09) | the call; error, whose description is never empty (§1.4); what it means for the session | `{Call} returned an error; {Consequence}` (error) |
| INF / ERR | OpenXR | session creation on the game's device | success, or failure (error) | `OpenXR session bound to the game's device` / `OpenXR session not created` (error) |
| WRN | OpenXR | the game's adapter differs from the runtime's (09 §2.2) | both adapters (description or LUID); that the mod continues | `Adapter mismatch: game on {GameAdapter}, runtime wants {RuntimeAdapter}; continuing` |
| INF | OpenXR | each eye swapchain created | size; format; image count | `Eye swapchain {Size}, {Images} images, {Format}` |
| WRN | OpenXR | no swapchain format matches the game's target; once (09) | the game's format; that no layer is submitted | `No eye swapchain format matches {Format}; no layer will be submitted` |
| INF | OpenXR | every session state change, whatever the state (09 §3) | the new state by its OpenXR name; for `READY`, whether `xrBeginSession` succeeded | `OpenXR session now {State}` |
| WRN | OpenXR | once, a running session the runtime has not shown within the bound of 09 §3.1 | its current state; how long it has been running; that a warning follows | `Headset not showing the session (state {State}) after {Seconds} s; warning the player` |
| WRN | OpenXR | the session reached `EXITING` (09 §3; a loss does not quit, 12 §7) | that the game was asked to quit and VR is off until restart | `Runtime requested exit; game asked to quit, VR off until restart` |
| ERR | OpenXR | the session or the instance was lost (09 §3) | that VR is off for this run and the game keeps running on the mirror | `OpenXR session lost; VR off for this run, game continues` |
| ERR | Frame | the desktop mirror's resources cannot be created (03 §8.3) | the resource that failed; error; that VR is unaffected; what the window shows instead (03 §8.3) | `Desktop mirror unavailable: {Resource}; VR not affected; {Fallback}` (error) |
| INF | OpenXR | the first valid views | the IPD in metres to 4 decimals | `Eye separation {Ipd:0.0000} m (first valid views)` |
| INF | Stereo | the game started the sensor | that stereo rendering began | `Game started stereo rendering` |
| INF | Stereo | each new side-by-side size answer (03 §4.1) | the effective pixel scale to 2 decimals; the side-by-side size; its megapixels to 1 decimal; the texture size in use; the device state | `Pixel scale {PixelScale:0.00} gives a {Size} stereo target ({Megapixels:0.0} MP); texture {TextureSize}; device state {State}` |
| INF | Stereo | the game configured rendering | the texture size it configured | `Game configured rendering; texture {Size}` |
| INF | Stereo | first stereo BeginFrame | each eye's offset from the head, in metres (JSON arrays) | `First stereo frame; eye offsets {Left} and {Right} m` |
| INF | Stereo | each recenter (03 §2.1, 05, 09) | what triggered it (chord, menu, headset put on, loading finished, the runtime's own recenter); the yaw in degrees to 1 decimal; the head's position and axes; the headset activity level | `{Trigger} recenter: yaw {Yaw:0.0} deg; position {Position}; axes {Axes}; headset activity {Activity}` |
| ERR | Stereo | the runtime failed to start, or the OpenXR session could not be created (12 §7) | which runtime; at which step; error; that VR stays off for this run and a message follows | `VR stays off for this run: {Runtime} failed at {Step}` (error) |
| INF | Stereo | the first stereo frame with each of the game's AA modes (08 §3) | the game's AA mode; what the mod does with it in stereo (runs it per eye, runs SMAA T2x as T1x, skips it); the `Rendering.StereoAntiAliasing` value behind that | `Stereo anti-aliasing for game mode {Mode}: {Action} (setting {Setting})` |
| INF | Stereo | each new reflection view and each change of its flag byte `+0x36B` (07 §6); bounded per process (enough for the reflection views of one level and their flag changes during a load) so that a flag that toggles every frame cannot flood the log | which view; the new and the old flags; the view's target size; the device state | `Planar reflection {View}: flags now {To}, were {From}; target {Size}; device state {State}` |
| INF | Stereo | the first per-eye reflection render (07) | that reflections now render per eye | `First per-eye planar reflection rendered` |
| INF | Stereo | the mod raised the game's display-reset flag (12 §6.3) | which settings changed (key paths of 06 §1) | `Display reset raised; changed: {Changed}` |

### 2.5 Input, config, UI, audio

| Level | Area | When | Must convey | Template |
|---|---|---|---|---|
| INF / ERR | Input | the SteamVR Input files were written (13 §3.3) | the folder; on failure the Windows error and that the legacy path applies | `Input binding files in {Folder}: {Outcome}` (error on failure) |
| INF | Input | SteamVR Input is in use (04 §5.8) | the manifest's path; the number of actions | `Action manifest {Manifest} registered; {Actions} actions` |
| WRN | Input | the legacy controller path is used (04 §5.8.3) | why (`IVRInput` unavailable, the manifest refused) | `Falling back to legacy controller reads: {Reason}` |
| WRN | Input | once, when a controller is active but the hand pose actions have no binding | that the hand poses are unbound; that the player should restore the default SteamVR binding for this game | `No binding for the hand pose actions; restore the default SteamVR binding for this game` |
| INF | Input | OpenVR: each change of the device in a hand role | the hand; the controller type; the device index | `Controller for {Hand} hand: {Type}, device index {Device}` |
| INF | Input | OpenXR: the action set was attached | that actions are live | `Action set attached to the session` |
| ERR | Input | an action or action space could not be created | which one; error | `Creating {Action} failed` (error) |
| WRN | Input | a suggested binding set was refused | the interaction profile; error | `Suggested bindings refused for {Profile}` (error) |
| INF | Input | OpenXR: each change of the `xrSyncActions` result | the result | `Action sync now returns {Result}` |
| INF / WRN | Input | OpenXR: each change of a hand's interaction profile | the hand; the profile, or that none is bound (WRN) | `{Hand} hand profile: {Profile}` |
| WRN | Input | OpenXR, per hand, each change of the outcome when the grip cannot be located | the hand; the locate result; the location flags; whether the pose and trigger actions are active | `{Hand} grip unavailable; locate {Result}; flags {Flags}; actions active: pose {PoseActive}, trigger {TriggerActive}` |
| INF | UI | the first settings push of the process (06 §4.4); later pushes are not logged | that the menu received the settings | `Menu received the settings (first push this process)` |
| WRN | UI | the settings cannot be pushed; once (06 §4.4) | why (the menu's call addresses are not available) | `Menu settings push unavailable: {Reason}` |
| INF | Config | each setting changed from the menu or a console command (06) | the key path; the new value; where the change came from (menu or console) | `{Key} changed to {Value} by {Source}` |
| INF / ERR | Config | `mothervr.json` written (06, 13 §3.4) | its path; on failure the Windows error, that the change holds for this session only, and that the file on disk is still the previous complete one (06 §3.1) | `Settings saved to {Path}` / `Saving settings to {Path} failed; previous file kept; the change holds for this session` (error) |
| INF | Audio | the redirect of 10 §3.2 was installed | the headset's audio endpoint | `Headset audio endpoint {Endpoint} now receives the game's output` |
| ERR | Audio | the redirect failed (10) | that the game's output stays on the default device; the `HRESULT` (error); that a message follows | `Game output stays on the default device: redirect failed` (error) |
| WRN | Audio | the reopen function's address is unknown on this build or its bytes differ (10 §2.2) | why; that the switch happens at the game's next reopen of its output | `Output reopen not available ({Reason}); switch happens at the game's next reopen` |

### 2.6 Messages to the player

Every message the mod shows (13 §5) is also logged (`INF Load`, §2.1) with
its full text and a `Kind` naming which message it is (and one that could
not be shown, `WRN Load`, §2.1, with the same `Kind`): unsupported build,
welcome, `UI.PAK` missing, UI archive unusable, runtime failed, audio
failed, headset not showing the game. What each message conveys and when
it appears are in 13 §5; the rules for showing the messages decided at
attach are in 12 §2.4, and the timing of the others in 13 §5.2.

## 3. Rate rules

The render thread (12 §2) never logs per frame:

- an event is logged once when it happens, or once when the reported value
  changes (a size, a mode, a controller, a result code);
- a periodic diagnostic is one `VRB` event every **900 frames** (10 s at
  90 Hz: often enough to watch a change while testing, rare enough to cost
  nothing measurable), written only while `DebugOptions` is on;
- the counter behind a periodic diagnostic runs whether or not the option
  is on, so turning it on gives a full window at the next report;
- an event that can repeat without a state change (the reflection view
  events, the shader counts, the shader rejections) has a per-process bound
  (§2), so no game behaviour can flood the log;
- a log write is one `WriteFile` of an event rendered without allocation
  (§1.2), never a file open.

Job threads (the cull replacements) never log.

## 4. `DebugOptions` diagnostics

`DebugOptions` (06 §1.3) gates the diagnostics below and every `VRB` and
`DBG` event. It is read each time a diagnostic is due, so turning it on or
off in the menu takes effect at once.

### 4.1 Pose age (OpenXR)

Purpose: prove that the game renders with views predicted for the display
time of the frame they are submitted in (09). A test build whose frame loop
sat at the wrong point was observed in game to render every frame with
views one display period old (measured mean 1.01 periods); the probe stays
to catch a regression.

What is reported: for each window of 900 frames submitted with a
projection layer, on the render thread,

- how many were rendered with views predicted for that frame's own display
  time ("on time");
- how many with views predicted for one display period earlier ("one
  period late");
- the rest, including frames whose age cannot be computed;
- the mean age, in display periods to 2 decimals, over the frames whose
  age is known.

A frame's age is the difference between the display time of the frame
being ended and the display time the rendered views were predicted for,
divided by the predicted display period, both times being the runtime's
frame-state values, and rounded to the nearest whole period for the counts.

`VRB Diagnostics`:

```
Pose age, mean {Mean:0.00} periods over {Frames} frames; on time {OnTime}; one period late {Late}; other {Other}
```

Expected with a correct loop: 900 on time, mean 0.00.

OpenVR needs no probe of the prediction: its poses are read after
`WaitGetPoses` for the frame being rendered (03). That the pose declared
with each submitted frame is the one the game rendered with (the handover
of 03 §11) is checked by 03's acceptance, not by this probe.

### 4.2 Hand poses relative to the head

Purpose: check controller tracking and the coordinate conventions from the
log while moving the controllers, without asking the player what they saw.

What is reported every 900 submitted frames: for each hand, its grip
position relative to the head, expressed in the head's own frame (x right,
y up, z backwards, so forward is negative z, as in both runtimes'
conventions), in metres with 2 decimals, as a JSON array of three numbers.
The head pose is the one of the same prediction as the hands (OpenVR: the
compositor's latest poses; OpenXR: the head located with the frame's
views). For a hand without a valid pose, a short reason instead: that the
hand is not tracked; with OpenVR, that the action system is unavailable;
with OpenXR, whether the grip pose action is active and whether the grip
was located.

`VRB Diagnostics`:

```
Hand offsets from the head (m, head frame); left {Left}; right {Right}
```

A right controller held at the chest in front of the head reads about
`[0.20,-0.30,-0.30]`.

### 4.3 Game events

Purpose: find the names of events not yet in §5.3 by playing.

What is reported: each distinct string the game's scripts pass to the event
tap (§5), the first time it is seen, and every string that changes the
tracked state, each time it does. Must convey: the string; whether it is
seen for the first time; the tracked state after it (in interaction, the
kind, hiding).

`DBG Diagnostics`:

```
Script event "{Text}"; first seen {First}; interacting {Interaction} ({Kind}); hidden {Hiding}
```

Remembering which strings were seen is the only diagnostic that may
allocate. That is acceptable because it runs only with the option on, only
for a string not seen before or one that changes the state, and never on
the render thread's per-frame path; it must be safe when the tap is called
from more than one thread (§8).

### 4.4 Reflection target capture

Only while `DebugOptions` is on and planar reflections render per eye
(07), a developer key captures, once per press, the render target bound
after each of that frame's two reflection passes to a file in the game
directory (13 §3.5), in a format that keeps the texture's own format (a
DDS file with a DX10 header is one). It serves the check of 07 §7 step 5,
gated as 07 §6 says. The key and the file
names are the implementation's choice; each file's name must show which
pass it holds.

| Level | Must convey | Template |
|---|---|---|
| DBG | which pass the file holds; the file written; size; format | `Reflection capture, pass {Pass}: {Path}, {Size} {Format}` |
| WRN | which pass; that no render target was bound | `Reflection capture, pass {Pass}: nothing bound` |

It is gated by the option because it is the one place where the render
thread reads back from the GPU and writes a file; with the option off, the
capture key does nothing.

### 4.5 Stereo mode, frame kind and snap turns

Purpose: follow which menus, videos and loading screens are shown on the
virtual screen, and find out why the view keeps moving toward the turn
direction for a moment after a snap-turn step while walking (reported by a
tester on SteamVR with a Steam Frame headset). The snap trace records the
camera's yaw and look state and the character's head position relative to
the character's own position, which together show whether the camera or
the character is still turning.

Game facts used by the snap trace [sure unless marked; read by the spec
side in the 2015 binary; 2020: ?]. Fields of the first-person camera
(FollowBehavior, 04 §2.7 lists its other fields), both written by its
vtable slot 6 (0x2C9F0, which also clears much of the camera's other state
[likely: its set-up or reset]) and by its slot 8 (0x2E970, thiscall,
`ret 8`, first argument a float time step [likely: the per-frame
update]):

| Offset | Type | What the game writes there |
|---|---|---|
| +0x1C0 | float x 3 | the followed character's position, copied from that character object (`+0x70` to `+0x78` of the object the camera reaches through its `+0x34` and then `+0x148`). Slot 8 stores this position, raised by a constant on its second (Y) component, into the object at `+0x34`, so Y is up and the other two components are horizontal [sure]; it is a world position [likely] |
| +0x1CC | float x 3 | the translation part of the transform of the character's bone named `Head1` (the game's own string at 0xDC6640), taken from the character's skeleton (`+0x3F0` of the character object) with the game's transform mode 7; the bone index is looked up by that name in slot 6 and kept at `+0x248`. In the same space as `+0x1C0` [likely: the game subtracts other positions from it to measure distances; mode 7 is not decoded] |

| Level | Area | Thread | When | Must convey | Template |
|---|---|---|---|---|---|
| DBG | Stereo | `main` | each change of the current stereo mode (stereo object `+0x60`, 05 §4.4: 0 the world, 1 the virtual screen), whatever changed it | the mode before and after | `Stereo mode now {To} (was {From})` |
| DBG | Frame | `render` | each change between head-tracked frames and screen frames (03 §6.2, 09 §5.2), once per change, on either runtime | the new kind (`head-tracked` or `screen`) | `Frame kind changed to {FrameKind}` |
| DBG | Stereo | `main` | at every stereo update of the game thread (05 §3.2) from a snap-turn step (04 §6) until the view has settled (below) | unordered: the time since the step; the camera's yaw (`+0x204`, 04 §2.7) and how far its mirror field `+0x1FC` differs from it (the step writes both, 04 §6, so any difference comes from the game); the head's horizontal offset from the character's position (`+0x1CC` minus `+0x1C0`, X and Z components, above): its size and its direction; whether game look is enabled (`+0x264`); whether keyboard and mouse are in use (04 §7) | `Snap +{Elapsed} ms: yaw {Yaw:0.0000} half-turns, mirror off by {YawGap:0.0000}; head {HeadOffset:0} mm from the character toward {HeadBearing:0} deg; look {Look}; keyboard and mouse {KeyboardMouse}` |

`HeadOffset` is the length of the horizontal offset in millimetres, which
resolves a head still moving around the character's position by a few
millimetres per update; `HeadBearing` is its direction, `atan2(x, z)` of its two
horizontal components in degrees (a spec-side convention for this event,
not a game convention). The yaw has 4 decimals (0.0001 half turn is about
0.02 degrees), finer than one frame of settling.

The snap trace is the one diagnostic logged every game update. That is
acceptable because it runs only with the option on, only on the game
thread, and only for a short time after a step: it ends when the view has
settled (the two yaw fields are equal and the head offset changed by less
than 1 mm since the previous update), and at the latest 1 s after the step
(project decision: a bound so a view that never settles cannot log every
update for long, and longer than the slide the tester described as lasting
a moment, so the whole movement is in the log). A further step restarts
it. On a build where the two position fields are unknown (2020) the event
leaves out the head offset.

### 4.6 Head-in-wall contacts

Purpose: give the measurement 08 §4.2 asks for (the longest incidental
contact of the head with geometry), from the log of a test session,
instead of asking the tester how long the view darkened.

A contact is the time the game's head-in-wall fade (`HeadClipFade`, 08 §4)
stays below exactly 1.0, from its first value below 1.0 to the next value
of exactly 1.0, measured with the same monotonic clock as the delay of
08 §4.2.

| Level | Area | Thread | When | Must convey | Template |
|---|---|---|---|---|---|
| DBG | Stereo | `main` | each contact that ends, while stereo is running and the head-in-wall delay is in place | its length in milliseconds; whether it lasted longer than the delay D of 08 §4.2, so the view darkened | `Head-in-wall contact {Duration} ms; darkened {Darkened}` |

One event per contact, on the game thread only and only with the option
on, so walking along a wall adds a few lines per second at most. A
contact still running when stereo stops is not reported.

## 5. Logging of game script events

### 5.1 Game fact

| Name | 2015 | 2020 | Kind | What is there |
|---|---|---|---|---|
| `ScriptEventTap` | 0xA1BCC0 | ? | function | `8b c1 c2 04 00` (`mov eax, ecx; ret 4`): a no-op in the retail build, thiscall with one stack argument `const char*`, returns `this`, callee pops 4 bytes. Target of 683 `call rel32` sites in `.text` [sure, counted in the 2015 binary] |

The game's scripts and behaviour code pass debug strings to it [likely,
from the strings seen]: per-frame engine chatter (strings starting with
`C`, for example `CATHODE:...`, and strings starting with `on_`, for
example `on_update`), and the start and end of interactions and hiding.

The address of `ScriptEventTap` also appears in five slots of pointer
tables in `.rdata` (at 0x1076A18, 0x1076EA0, 0x1076F80, 0x1077060 and
0x1077140) [sure, counted in the 2015 binary], so code other than the
scripts' debug calls reaches the same five bytes. Calls through those
slots need not pass a string [likely; the spec side has not traced their
callers]. The event names of §5.3 are string literals in `AI.exe`'s own
sections [sure].

The five bytes are followed by eleven `cc` padding bytes; the next
function starts at 0xA1BCD0 [sure, read in the 2015 binary].

### 5.2 Hook

The logging of game script events (the feature area "event tap" of §2.1)
hooks `ScriptEventTap`. It is an optional feature area (§2.1, 12 §4.2): it is installed with
the other optional fixes once the core set has passed its checks, so a
failure of the core set leaves it off too. It is installed only if the five
bytes at the site are exactly `8b c1 c2 04 00`. The replacement keeps the
game's contract: it returns `this` in EAX and pops the 4-byte argument
(12 §5). On a mismatch, or an unknown 2020 address, nothing is hooked, the
state below stays at its initial values (not in interaction, kind none, not
hiding) and the area's outcome event says so.

The hook changes at most the five bytes of the body (a 5-byte jump fits
exactly); the padding after it is not written, so the change to the game
stays inside the function it replaces. Because the body only returns
`this`, the replacement may keep the contract itself instead of calling
the original.

**Only arguments inside the game's image are read.** Because the same
code is reached with arguments that need not be strings (§5.1), the
replacement reads its argument as text only when it is an address within
the loaded `AI.exe` image; any other argument, null included, is passed
through unread with the game's contract kept and the tracked state
unchanged. Reading such an argument as a string could fault or read
unrelated memory, in code the game runs during normal play.

The replacement must be cheap: it runs from up to 683 sites, some every
frame. It must discard the frequent per-frame strings (§5.1) cheaply,
before any matching against §5.3; none of the tracked strings starts with
`C` or `on_`, and an empty string is never tracked.

### 5.3 Tracked state

Three values, each readable from any thread without tearing (each a single
atomic value):

- **in interaction** (bool),
- **interaction kind**: none, generic, suit change, keypad, hacking,
  cutting, terminal, ladder, rewire,
- **hiding** (bool).

The state after a string is received, by exact, case-sensitive match of the
whole string. Where a string fits more than one description, its own row
decides; a string not in the table leaves the state unchanged.

| String received | In interaction | Kind | Hiding |
|---|---|---|---|
| `start_interaction` | yes | generic if it was none, else unchanged | unchanged |
| `<prefix>_start`, prefix from the table below | yes | that prefix's kind | unchanged |
| `hiding_visibility_start` | unchanged | unchanged | yes |
| `hiding_visibility_finish` | no | none | no |
| `stop_interaction` | no | none | unchanged |
| `ladder_interaction_finish` | no | none | unchanged |
| `suit_change_interaction_finish` | no | none | unchanged |
| `<prefix>_finish` for any other prefix below | unchanged | generic if it was that prefix's kind, else unchanged | unchanged |

| Prefix | Kind |
|---|---|
| `suit_change_interaction` | suit change |
| `keypad_interaction` | keypad |
| `hacking_interaction` | hacking |
| `cutting_panel` | cutting |
| `terminal_interaction` | terminal |
| `ladder_interaction` | ladder |
| `rewire_interaction` | rewire |

v1 uses this state only for §4.3; v2 features (d-pad and lever emulation,
lean while hiding, no turning during interactions) will read it. Installing
it in v1 keeps the hook checked on each build before those features depend
on it.

## 6. Crash reporting

### 6.1 The crash record

A player's log must show where a crash happened, without anything else
from the player (reported by a tester whose game crashed in the pause
menu with nothing in the log to locate it).

Game fact [sure, read by the spec side in the 2015 binary; the sites are
in 01 §4.6; 2020: ?]: `AI.exe` imports `d3d11.dll`, so the proxy attaches
before the game's C runtime starts (01 §2.1). One of that runtime's
initialisers (`CrtFilterInstall`) then makes the runtime's own filter
(`CrtUnhandledFilter`) the process's top-level unhandled-exception filter,
discarding the filter it replaced. That filter hands an MSVC C++ exception
to the C runtime's terminate and lets the search continue for any other
exception; it never calls the filter it replaced. So a filter installed
only at attach is gone before the game's own code runs. The game installs
no other filter. Two C runtime failure reports (`CrtFailFast`) remove the
filter and report straight to Windows; a termination through them is seen
only in Windows' own record (§6.2).

Required:

- **The mod's filter is in effect while the game runs.** It is installed
  at attach (01 §2.2), so a crash before the game's C runtime starts is
  recorded, and installed again once that runtime has started, at the
  first `D3D11CreateDeviceAndSwapChain` call through the proxy (01 §1.3,
  §2.2), which runs on the game thread after the C runtime's start-up; a
  crash between the two reaches only Windows' record. Each time, the
  filter it replaced is kept, except that the mod's filter never keeps
  itself as the replaced filter (installing it a second time would
  otherwise make it call itself).
- **Passed on.** After writing the record, the filter passes the
  exception to the filter it replaced and returns that filter's result, or
  lets the search continue when there was none. So Windows still records
  the crash (Event Viewer, Windows Error Reporting) and the C runtime still
  handles a C++ exception as it would without the mod.
- **Once.** The record is written at most once per process; a second
  exception, including one raised while the record is written, adds
  nothing and is passed on at once. A failure while writing the record
  never keeps the exception from the filter that runs next.
- **No allocation.** The record is written without allocating memory,
  because the process may be in any state (a corrupt heap among them). Log
  writes are already allocation-free (§3); module names are found from
  addresses without allocating, through wide-character APIs, and logged
  as UTF-8 (§1.1).
- **No second fault.** Stack words are read only within the faulting
  thread's own stack (between the stack pointer and that thread's stack
  base), so recording a crash cannot fault itself.
- **Stack overflow.** A stack overflow is recorded too. Windows runs the
  filter on the overflowing thread with little stack left: on x86 about the
  one 4 KiB guard page, less what exception dispatch has used [likely,
  from Windows' documented stack guard behaviour]. Writing the record uses
  at most 2 KiB of the faulting thread's stack (project decision: half that
  page, so dispatch and the record both fit), or is done by another thread
  while the faulting thread waits.

Must convey, unordered, in one or more `ERR Load` events (how the record is
split into events and in which order is the implementation's choice; each
event fits the bound of §1.1): enough to locate the fault from the log
alone, namely

- the exception code;
- the faulting instruction as module file name plus offset (or the
  absolute address when no module contains it), and the thread;
- for an access violation, the kind of access (read, write or execute)
  and the address accessed;
- the CPU's integer registers and its instruction pointer, the latter even
  when it equals the faulting address, so the register set is complete
  for comparison with a debugger's;
- the probable callers: each of the stack words nearest the stack pointer
  whose value lies in a loaded module, as module file name plus offset,
  with its position on the stack. A bounded number of words is examined
  (project decision: enough to name at least the faulting function's
  caller and that caller's caller in the game's usual frame sizes, few
  enough that the record stays a handful of lines).

Templates, for the split into four events (another split carries the same
properties):

| Content | Template |
|---|---|
| code, place, thread | `Crash: exception {Code} at {Module}+{Offset}, thread {ThreadId}` |
| access violation | `Crash: {AccessKind} access to {Address} refused` |
| registers and instruction pointer | `Crash registers: {Registers}` (`Registers` a JSON object keyed by register name) |
| each probable caller | `Crash caller candidate at stack +{StackOffset}: {Module}+{Offset}` |

### 6.2 What a player sends and what a tester does with it

What a player sends (player guide, 13):

- `mothervr.log` (and `mothervr.log.1` if present) from the game directory,
  which holds the crash record of §6.1;
- for a crash, optionally the Windows Event Viewer entry under Windows
  Logs > Application for `AI.exe` (event 1000 "Application Error", or
  1001 or 1026): faulting module name, exception code and fault offset,
  the same facts as the record gives, for a termination the record cannot
  see (the C runtime's failure reports of §6.1).

What a tester does with it:

- When the faulting module is `AI.exe`, the fault offset (in the crash
  record or the Event Viewer entry) is an RVA of the build named in the
  attach event (§1.7), and so are the stack entries in `AI.exe`; disassemble the game around it
  and compare with the address tables of 01: a fault in or near a patched
  site or a replaced function, or at the first return after a call into
  the mod, points at a patch or a calling-convention error (12 §5).
- When the faulting module is the proxy, the offset is an RVA of the proxy
  build; each release keeps its symbol file (13 §7, Symbols row, and
  §8.3) so the offset can be mapped.
- The last events of the faulting process (by process id) show how far it
  got: no `OpenMotherVR.Patch` events after the attach event points at
  attach; nothing after the first-`Present` event points at the first
  frame's submission.

## 7. Acceptance

1. Start the game twice from Steam without deleting the log: both sessions
   are present, each process's first event is the attach event, and every
   event parses with the template of §1.4 (a script that matches the
   timestamp with offset, the bracketed level, the bracketed process id and
   thread name and the `OpenMotherVR.` source context finds no bad event;
   continuation lines appear only after an event with an error).
2. When Steam relaunches the game, two process ids appear in one run, one
   whose attach event names a `Launcher` process and one a `Game` process.
3. Grow the log past 8 MiB (append padding with the game closed) and start
   the game: `mothervr.log.1` holds the old content and `mothervr.log`
   starts with the new session.
4. Install the game under a path with non-ASCII characters (for example a
   folder named `Spiele Übung`): the game starts, the log is written, and
   the install directory in the attach event is that path in correct
   UTF-8.
5. A recognised 2015 build: the attach event names build 2015 and timestamp
   0x54E3199D, no failed-check `ERR`, and one outcome event per feature
   area of §2.1, each in place.
6. Run a copy of `AI.exe` with an edited timestamp: the `FTL` unrecognised
   build event, no feature-area event, and the message shown and logged.
7. With OpenVR: both availability events, the runtime decision with its
   reason, the session-opened event, the headset event with per-eye size
   and IPD, the stereo target event with megapixels, one controller event
   per hand, the stereo-rendering-started event. With OpenXR: the instance,
   system and eye swapchain events, the session reaching running, each
   hand's interaction profile.
8. Play 5 minutes with `DebugOptions` off: after start-up no event is
   written during steady gameplay except state changes (menus, resets,
   controller changes); no `VRB` or `DBG` event appears.
9. Turn `DebugOptions` on in the menu: within about 10 s (900 frames at 90
   Hz) a hand offsets event appears and repeats about every 10 s; with
   OpenXR a pose age event shows 900 on time. Moving the right controller
   from the chest to arm's length forward changes its z from about -0.3 to
   about -0.6.
10. With `DebugOptions` on, use a keypad and a terminal and hide in a
    locker: script event entries show the start and end strings with the
    state changing as §5.3 says; no string starting with `CATHODE` or
    `on_` appears.
11. Start with OpenVR selected and the headset unplugged: the OpenVR
    session `ERR` with its `EVRInitError` line, the stereo "VR stays off"
    `ERR`, the message logged with its text; the game continues flat.
12. Make the game folder read-only for the player's account: the game
    starts, and with a debug-output viewer running the events appear there.
13. Crash test (a developer build that faults on purpose in a replaced game
    method, during play, so after the game's C runtime has installed its
    own filter): the log's last events for that process are the crash
    record: the exception code and module plus offset, the access kind and
    address for an access violation, the registers with the instruction
    pointer, and at least two probable callers naming modules; Event
    Viewer still shows the `AI.exe` entry with the same exception code and
    offset. Repeated with the game installed under a path with non-ASCII
    characters, the module paths are correct UTF-8. Repeated with a
    deliberate unbounded recursion (a stack overflow), the record is
    written as well.
14. Focus and window: with a headset attached and the game set to
    fullscreen, the log shows the window made borderless with its old size
    and the monitor's, and the focus-loss event once; clicking another
    window logs a focus loss naming that application's executable.
15. OpenXR start-up: both registry views' values, the manifest chosen with
    its rule, the loader direction outcome, every session state change in
    order (for example `IDLE`, `READY`, `SYNCHRONIZED`, `VISIBLE`,
    `FOCUSED`). An OpenXR failure at instance creation carries a non-empty
    symbolic result name in its error line.
16. With `DebugOptions` on: opening and closing the pause menu logs the
    stereo mode change both ways and the frame kind change both ways, each
    once; a snap-turn step during play logs snap trace events until the
    view settles, for at most about 1 s, then none until the next step;
    the trace shows the yaw and the head offset. No string the event tap
    would read lies outside the game's image (no fault, no garbage in the
    script event entries) while navigating every menu page.
17. Required events (each run in the setting it needs): the OpenVR
    interface versions obtained; `IVRApplications` unavailable logged when
    the runtime lacks it; the loader direction event of §2.3 both when
    accepted and when refused; the rule that chose the OpenXR manifest for
    every rule, and the player's `XR_RUNTIME_JSON` with its value; both
    adapters in the adapter-mismatch event; a binding file that cannot be
    written logged with the reason before the legacy fallback; OpenVR
    unavailability telling a missing paths file, an unparseable one and
    one with no runtime entry apart; the pose-age mean over the frames
    whose age is known only.
18. Patch events: edit a copy of `AI.exe` so one core site differs: one
    `ERR` naming the site's label, RVA, the kind of check and the bytes
    found, then the `FTL` with the count; with a recognised build, the
    patch-set event and one outcome event per feature area, each with its
    number of sites.
19. Config and messages: change a setting from the options page and
    another with an `mvr_*` console command: one change event each, naming
    the key, the value and the source, and one save event each; make
    `mothervr.json` read-only and change a setting: the save failure event
    with its Windows error. Every message shown appears once in the log
    with its kind; the menu-push-unavailable event, where it applies,
    appears once per process, not at every push.
20. Session start: the attach event carries the mod's version, the Windows
    build, the process kind and the runtime's source; the settings event
    lists every key of 06 §1. An event longer than the bound of §1.1 (for
    example a long install path written several times in a test event)
    ends with the cut marker and is valid UTF-8.
21. Reflection capture off: with `DebugOptions` off, pressing the capture
    key while a per-eye reflection renders writes no file and logs
    nothing.
22. Not the game: loading the proxy into another 32-bit program writes
    one host event naming that program's executable and no attach,
    settings, patch or message event (01 §6 item 14).
23. OpenVR start-up without a runtime error: with a developer build that
    hides one loader export, or with SteamVR unable to serve its
    compositor within the bound, the start-up failure event names what
    was missing and a non-zero code; a normal start logs the interface
    versions obtained once.
24. Head-in-wall contacts: with `DebugOptions` on, brush a wall with the
    head and then hold the head inside it: one contact event each, the
    first not darkened, the second darkened, with lengths that match.

## 8. Open questions

- Whether `+0x1CC` (the `Head1` bone's translation, §4.5) is in world
  space like `+0x1C0`, and what transform mode 7 selects. To resolve:
  follow the mode through the transform lookup at 0x2A2FB0, or compare the
  two fields in game with the character standing still (a world-space
  head lies within a few tenths of a metre of the character's position,
  horizontally). If it is not, the trace's head offset is meaningless and
  must be computed in one space once mode 7 is known.
- The 2020 offsets of the two fields of §4.5. To resolve: find the
  FollowBehavior methods in the 2020 `AI.exe` that look up `Head1`.
- The 2020 RVA of `ScriptEventTap`. To resolve: in the 2020 `AI.exe`, find
  the function whose body is exactly `8b c1 c2 04 00` and that is the
  target of several hundred `call rel32` sites (about 683), with callers
  pushing pointers to strings such as `start_interaction`.
- Which thread calls `ScriptEventTap` (the game thread, job threads, or
  both). To resolve: record the thread of the first call of each distinct
  string in a debug build. The state is atomic either way.
- The complete list of interaction strings. Only those in §5.3 are known
  from play; levers, valves and doors may announce themselves under other
  names. To resolve: play with `DebugOptions` on and collect the script
  event entries seen for the first time.
- Whether `SteamGameId` / `SteamAppId` reliably tell the Steam-started game
  process from the relaunching one (§1.8), on Steam and on the 2020 build's
  other stores. To resolve: log the environment of both processes in a
  test build started from Steam, from the desktop shortcut and by running
  `AI.exe` directly.
