<!-- Provenance: game analysis of AI.exe 2015 (UI external-call handler and its return size, script invoke methods and their return sizes, menu Open methods, bootup movie call, quit flag getter, display-reset flag, settings handlers named in 02); the stock game's ActionScript menu classes from the player's own UI.PAK (item constructors, option setters, item data replacement, automatic ids, page maps, input-mapper selection, slider snapping); the earlier mod's settings file keys and values, command-line switches and `mvr_*` console commands, kept exactly as compatibility names; compatibility facts about tools that read the same settings file; user requirements (no shipped game archive or mod-derived UI data, v1 fixes, runtime values None / OpenVR / OpenXR, -vrscale is the world scale); user decisions for r3 and r4 (default runtime OpenVR in a fresh file; Oculus means OpenXR; runtime None is the unmodded game with the patch set decided at attach; skip intro only with the archive redirect; settings push also on loading-screen Open; no other mod named; own labels and texts); spec-side decisions for r5 (VR page hidden in the game-over menu; the game's Stereo Mode item greyed in VR; Camera Speed stays active; one rule for command values; -vrscale as a whole argument; no rewrite of the settings file at start-up; reads of values only earlier builds of this project wrote are dropped); answers to Q1, Q2, Q6 (ANSWERS.md). 2020 values were taken from the earlier mod's per-build address table (and, for inline sites, its installer) and have not been verified against a 2020 `AI.exe`; a tester's report on SteamVR with a Steam Frame headset (after an OpenXR failure the page offered no visible way back to SteamVR); project decision of r5, with its reason inline (the page shows the runtime this run drives; the stored preference is kept). Revision: r5 -->

# 06 Options and config

The mod's settings, the config file `mothervr.json`, the command line, the
protocol between the game's UI and the mod (commands from the menu, values
pushed to the menu), the options page the player sees and how it must
behave, and skip intro.

How the options page gets into the game's UI archive (building
`DATA\MOTHER.PAK` from the player's own `DATA\UI.PAK`, staleness,
validation, the archive format) is in `14-ui-resources.md`. This document
defines what the page must do; 14 relies on it.

Addresses are RVAs (image base 0x400000). 2015 values are verified against
the Steam 2015 `AI.exe` (PE timestamp 0x54E3199D). 2020 values were taken
from the earlier mod's per-build address table (and, for inline sites, its
installer) and have not been verified against a 2020 `AI.exe`.

Settings are named by their `mothervr.json` key throughout. Menu labels are
the implementation's own wording.

## 1. Settings

Keys are inside the root object `Settings`; a dotted key is inside a nested
object (`Rendering.PixelScale` is `Settings.Rendering.PixelScale`). The keys
and the string values are kept exactly: players' existing settings files
use them.

| Key | JSON type | Default | Values / range | Takes effect |
|---|---|---|---|---|
| `VRRuntime` | string | `"OpenVR"` | `"None"` 0, `"OpenXR"` 1 (also read as `"Oculus"`), `"OpenVR"` 2 (1.1) | next launch; changing it from the menu quits the game (4.5) |
| `Rendering.MirrorViewType` | string | `"RightEye"` | `"BothEyes"` 0, `"RightEye"` 1, `"LeftEye"` 2 | at once, with a render-target rebuild (4.6, 05 §10) |
| `Rendering.SymmetricMirrorView` | bool | true | | at once, with a render-target rebuild (4.6, 05 §10) |
| `Rendering.PixelScale` | number | 1.0 | 0.5 to 4.0 inclusive (1.2) | at once, with a render-target rebuild (4.6, 05 §10) |
| `Rendering.ClippingBlinders` | bool | true | | at once (05 §7.2) |
| `Rendering.StereoAntiAliasing` | string | `"Auto"` | `"Auto"`, `"On"`, `"Off"` | at once (08 §3: Auto runs AA only at pixel scale 1.3 or below); no menu control |
| `Input.SnapRotation` | bool | true | | at once (04 §6, §7) |
| `Input.SnapRotationSegments` | integer | 12 | 24, 12, 6, 4, 3, 2 (1.2) | at once |
| `Input.VRInputRelativeWalking` | bool | true | | stored only; no effect in v1, no menu control |
| `Input.VRInputStrafing` | bool | true | | stored only; no effect in v1, no menu control |
| `Input.ToggleGrips` | bool | false | | at once: the grips toggle aiming and the motion tracker instead of being held (04 §5.6) |
| `Input.ToggleCrouch` | bool | false | | stored only; no effect in v1, no menu control |
| `DebugOptions` | bool | false | | at once (1.3) |
| `SkipStartupIntro` | bool | false | | next launch (section 6) |

The numbers after the runtime and mirror values are the integers the
`mvr_*` commands and the pushes use (4.2, 4.3); the file stores the
strings.

`VRRuntime` defaults to OpenVR (user decision r4: installing the DLL and
starting SteamVR should give VR with no setup). Making OpenXR the default
is a v2 item (00 §1.1).

The world scale is not a setting: it exists only as the `-vrscale`
command-line switch (section 2, 03).

The pixel scale multiplies the per-eye size the runtime already recommends,
and that size already includes SteamVR's own resolution setting (03 §10).
Wherever the page or the player guide shows the pixel scale, it must say
so and recommend 1.0 with the runtime's own resolution setting as the way
to change resolution, because 1.5 renders 2.25 times the pixels.

### 1.1 Runtime values and the runtime that is started

The runtime has three values: **None** (0), **OpenXR** (1) and **OpenVR**
(2). v1 has two backends, OpenVR (03) and OpenXR (09), and no LibOVR
backend. Players' existing files and shortcuts may name value 1 `Oculus`;
because Meta's PC runtime is itself an OpenXR runtime, everything that says
Oculus means **OpenXR**: the stored string `"Oculus"` and the switch
`-vrforceoculus` (user decision, ANSWERS Q2). The mod never writes
`"Oculus"`.

- **None**: the unmodded game. No stereo device is created and only the
  small UI patch set is applied (01 §2.3: UI archive redirect, the
  external-call slot of 4.1, the settings push in the menu Open wrappers of
  4.4, skip intro when on). The patch set is decided **at DLL attach** from
  the config file and the command line (section 2), which are both known
  then (user decision, ANSWERS Q6).
- **OpenVR**, **OpenXR**: the full patch set is applied at attach and this
  is the preferred backend.

The backend is chosen once, when the game creates its stereo device (02
§6.1), not at DLL attach. Availability (no runtime DLL is loaded to decide
it):

- OpenVR is available when `%LOCALAPPDATA%\openvr\openvrpaths.vrpath`
  parses as JSON, its `runtime` array's first element is a string, and
  `<that path>\bin\win32\openvr_api.dll` exists.
- OpenXR is available when a 32-bit runtime manifest is found (09).

The runtime started:

| Preferred (forced switch if any, else `VRRuntime`) | Preferred available | Forced | Result |
|---|---|---|---|
| None | | | no VR |
| OpenVR or OpenXR | yes | | the preferred runtime |
| OpenVR or OpenXR | no | yes | no VR (a forced runtime never falls back) |
| OpenVR or OpenXR | no | no | OpenVR if available, else OpenXR if available, else no VR |

The availability of each runtime, the preferred one (and whether it was
forced) and the one chosen are logged (11 §2.3). If the chosen runtime then
fails to start, the player is told once and the game runs without VR (00
§2.7, 03, 09). Neither a fallback nor a failure changes the stored
`VRRuntime`; the options page shows the runtime this run drives (5.2).

### 1.2 Value ranges

The values in use, pushed to the menu and written to the file always lie
in these ranges:

- `Rendering.PixelScale`: [0.5, 4.0]. A value read from the file outside
  that range (or NaN) is replaced by 1.0. The range keeps the render-target
  size between a quarter and sixteen times the runtime's recommendation
  (03 §4.1 caps it further).
- `Input.SnapRotationSegments`: one of 24, 12, 6, 4, 3, 2, so that the step
  (360/n degrees) is a whole number of degrees that divides a full turn.
  Any other integer of at least 2 maps to the largest of these not above it;
  a value read from the file below 2 is replaced by 12.

The other settings cannot hold an invalid value: reading (3.3) and the
commands (4.2) yield only listed values.

### 1.3 `DebugOptions`

`DebugOptions` (a control on the VR page) does two things:

- UI: shows the game's hidden "Select Level" and "Trigger Debug Checkpoint"
  entries (5.5).
- Diagnostics: turns on the extra log output that 11 lists as gated by it
  and enables the reflection capture key (07 §6, 11 §4). It is read where
  it is used, so a change from the menu applies at once.

It has no other effect on gameplay.

## 2. Command line

Read from `GetCommandLineW`, split into arguments as `CommandLineToArgvW`
does. Every argument after the program name is compared **as a whole** and
**case-insensitively** with the switches below. Command-line values apply
to this run only and are never saved. The switch names are kept exactly so
that players' existing shortcuts keep working.

| Switch | Effect |
|---|---|
| `-vrforcenone` | runtime None for this run |
| `-vrforceopenvr` | OpenVR, forced |
| `-vrforceopenxr` | OpenXR, forced |
| `-vrforceoculus` | OpenXR, forced (kept so existing shortcuts work) |
| `-vrscale <value>` | **world scale** `s` for this run (metres scale of head and eye positions, 03). Not the pixel scale: the pixel scale comes only from the config file and the menu |

Force switches: if several are present, the **last one wins**, so a switch
added at the end of a shortcut overrides one already in it. "Forced" means
no fallback (1.1): a forced runtime that is not available gives no VR for
this run, with the usual log lines.

`-vrscale`: the value is the **next** argument, read as a decimal number
(`.` as the decimal point, whatever the system locale, because shortcuts
are shared between players). If `-vrscale` appears more than once, the
last one wins, as with the force switches. The value is accepted when
0.495 <= s <= 1.505; anything else, a value that is not a number, or a
missing value gives 1.0. The bounds are kept for compatibility: existing
shortcuts that were accepted (0.5 to 1.5 as typed) keep their effect. It
is read once, when the stereo device is first created.

No other argument is recognised: unknown arguments are ignored, and nothing
on the command line pauses or stops the mod while the DLL attaches.

## 3. Config file

### 3.1 Location and life cycle

- File `mothervr.json` in the folder that holds the mod's DLL (the game's
  install folder, next to `AI.exe`). Not the current directory: Steam may
  start the game with another one. The name is kept for compatibility, so
  that players' existing settings carry over.
- Read **once**, at DLL attach (01 §2.2), before anything else that depends
  on settings. The settings read are logged once (11 §2.1).
- **Not rewritten at start-up.** Loading the file never writes it, so a
  file the player edited, or one that does not parse, is not replaced
  unless the player changes a setting; a missing key stays missing until
  then.
- **First launch** (the file does not exist; an error while checking for it
  does not count as "does not exist"): the defaults are used and the
  welcome message is queued. Its content (13 §5.3): the mod is active,
  the default runtime is SteamVR, and how to change the runtime. The
  message is never shown under the loader lock, appears in front of the
  game window, and the game's device creation does not complete until it
  is closed (01 §2.2, 13 §5.2). **The file is written with the defaults
  only after the player closes the message**, because a process that
  exits first (Steam relaunching the game) must leave the welcome for the
  next launch. Once the file exists the message is not shown again, so a
  player normally sees it once. In a game folder that cannot be written
  the file never appears, so the message is shown at every launch; this is
  accepted for v1.
- **After the player changes a setting** (4.2), the file holds the new
  settings. Nothing else writes it. Command-line values are never written.
- A file that exists but cannot be read or does not parse as JSON must not
  crash the game: the defaults are used, the welcome message is not shown,
  and the file is left alone until the player changes a setting. That
  write replaces it with the 3.2 content holding the current settings (its
  old content cannot be preserved).
- A failed write (file locked, read-only folder) does not affect the game:
  the settings in memory stay as set for this run. Logging it is allowed
  (11).

### 3.2 Content

The file is UTF-8 JSON holding the keys of section 1 with these value
types. Formatting, white space and key order are the implementation's
choice. With the defaults it parses to:

```json
{
    "Settings": {
        "DebugOptions": false,
        "Input": {
            "SnapRotation": true,
            "SnapRotationSegments": 12,
            "ToggleCrouch": false,
            "ToggleGrips": false,
            "VRInputRelativeWalking": true,
            "VRInputStrafing": true
        },
        "Rendering": {
            "ClippingBlinders": true,
            "MirrorViewType": "RightEye",
            "PixelScale": 1.0,
            "StereoAntiAliasing": "Auto",
            "SymmetricMirrorView": true
        },
        "SkipStartupIntro": false,
        "VRRuntime": "OpenVR"
    }
}
```

What is written:

- Enum settings are written only as strings from their documented sets:
  `VRRuntime` as `"None"`, `"OpenXR"` or `"OpenVR"`; `MirrorViewType` as
  `"BothEyes"`, `"RightEye"` or `"LeftEye"`; `StereoAntiAliasing` as
  `"Auto"`, `"On"` or `"Off"`. Compatibility fact: older tools that read
  this file may read an enum value they do not know, or an integer, as the
  enum's first value (`VRRuntime` None, `MirrorViewType` BothEyes). Writing
  strings from the documented sets keeps the shared settings readable by
  them. `"OpenXR"` is one such unknown value (an older tool then starts
  without VR, which is accepted); it is written anyway, because `"Oculus"`
  would make such a tool start a LibOVR backend.
- A file that said `"Oculus"` keeps it until the next write, which stores
  `"OpenXR"`.
- `PixelScale` is a JSON number that reads back as the setting's value.
- **Unknown keys are preserved.** The write keeps every key of the document
  as last read (an empty object if there was none or it did not parse) and
  sets only the keys of section 1: other keys in the root, in `Settings`,
  in `Rendering` and in `Input` keep their values. If `Settings`,
  `Rendering` or `Input` exists but is not an object, it is replaced by an
  object.

### 3.3 Reading

Each key is read on its own; a missing key or a value of the wrong type
leaves that setting at its default without affecting other keys. A root
that is not an object gives all defaults. A `Settings`, `Rendering` or
`Input` that is missing or not an object gives defaults for the keys inside
it.

| Kind | Accepted |
|---|---|
| bool | JSON `true` / `false` only (a number is the wrong type) |
| number (`PixelScale`) | any JSON number; then 1.2 |
| integer (`SnapRotationSegments`) | a JSON integer; a number with a fraction may be truncated or treated as the wrong type (implementation's choice); then 1.2 |
| enum | a string equal (**case-sensitive**) to one of the names below; anything else, including an integer, is the default |

| Enum | Strings |
|---|---|
| `VRRuntime` | `"None"`, `"OpenXR"`, `"Oculus"` (= OpenXR), `"OpenVR"` |
| `MirrorViewType` | `"BothEyes"`, `"RightEye"`, `"LeftEye"` |
| `StereoAntiAliasing` | `"Auto"`, `"On"`, `"Off"` |

All settings of section 1 are persisted, including `DebugOptions`,
`SkipStartupIntro` and `StereoAntiAliasing`.

### 3.4 Threads

The file is read on the thread that attaches the DLL, and written on the
thread that handles a command (the game's main thread, 4.1) or, on first
launch, after the welcome message is closed. Settings are read by the game
thread, the render thread and job threads (mirror, pixel scale, snap turn,
AA). Requirement: a change made by a command is seen by every reader from
its next frame, and no reader ever sees a torn value (each setting is a
single word; publish them atomically or under a lock that the render thread
never waits on for long).

## 4. UI protocol (game facts and required use)

### 4.1 UI to mod: the external-call handler

The game's Scaleform movies call native code with
`ExternalInterface.call(name, args...)`. The game dispatches these through
one handler object whose vtable is at RVA **0xF00074** (2020: 0xF177D4,
derived as the slot address minus 4). Slot 1, at RVA **0xF00078** (2020:
0xF177D8), holds the handler method, stock implementation RVA
**0x43A0A0** (2020: 0x44B250):

```
void __thiscall Callback(void* context, const char* name, const void* args, unsigned argCount)
```

`ret 0x10` [sure, 2015: verified in `AI.exe`]. (`args` points to
`argCount` Scaleform values.) The stock method hashes `name`, looks it up in
a table of registered names (the table hangs off `this+0x0C`), and calls the
registered function; unknown names are ignored.

The slot is in read-only data. It is replaced by a method of the same ABI
only if it holds the stock method's address (image base + 0x43A0A0);
otherwise nothing is patched (01 §2.4).

A `name` that, after leading space characters, begins with `mvr_` is a mod
command and is **not** forwarded. Every other call reaches the stock method
**unchanged** (same `this`, same four arguments, same `name` pointer and
buffer contents).

A mod command is one string with no Scaleform arguments: the command name,
a space, and the value, for example `"mvr_pixelscale 1.5"`. Further text
after the value is ignored. The `mvr_` command names and value encodings of
4.2 are kept exactly: players type them in guides and existing menus send
them.

Game fact: the stock menu class's helper `call(name, ...)` prepends a
per-movie prefix (`"fe_"` front end, `"go_"` game-over menu, none in the
pause menu; 7.4); a command sent through it (`"fe_mvr_..."`) is not
recognised as a mod command and reaches the stock handler, which ignores
it. The page therefore calls `ExternalInterface.call` directly with the
bare command string.

### 4.2 Commands

Values are decimal: integers with an optional sign; the pixel scale a
decimal number with `.` as the decimal point, whatever the system locale
(the menu sends forms such as `1.5` and `2`).

**One rule for bad values:** a command whose value is missing, is not a
number of its kind, or is outside the accepted values below changes
nothing (no setting, no quit, no rebuild). An unknown `mvr_` name also
changes nothing.

| Command | Accepted values | Effect |
|---|---|---|
| `mvr_vrruntime` | 0, 1, 2 | 0 stores None, 1 stores **OpenXR**, 2 stores OpenVR; then the game quits (4.5) |
| `mvr_mirrorview` | 0, 1, 2 | `MirrorViewType` 0 BothEyes, 1 RightEye, 2 LeftEye |
| `mvr_mirrorviewsym` | 0, 1 | `SymmetricMirrorView` off / on |
| `mvr_pixelscale` | 0.5 to 4.0 | `PixelScale` |
| `mvr_clipblinding` | 0, 1 | `ClippingBlinders` off / on; applied to the head-in-wall threshold immediately (05 §7.2) |
| `mvr_snaprotation` | 0, 1 | `SnapRotation` off / on |
| `mvr_snaprotationsegments` | integers of at least 2 | `SnapRotationSegments`, mapped by 1.2 |
| `mvr_inputrelativewalking` | 0, 1 | `VRInputRelativeWalking` (stored) |
| `mvr_inputstrafing` | 0, 1 | `VRInputStrafing` (stored) |
| `mvr_togglegrips` | 0, 1 | `ToggleGrips` |
| `mvr_togglecrouch` | 0, 1 | `ToggleCrouch` (stored) |
| `mvr_debug` | 0, 1 | `DebugOptions` |
| `mvr_skipintro` | 0, 1 | `SkipStartupIntro` (next launch) |

After a command changes a setting:

- the values used, pushed and saved satisfy 1.2;
- if `PixelScale`, `MirrorViewType` or `SymmetricMirrorView` now differs
  from the values the render targets were built with, a rebuild has been
  requested (4.6);
- the file holds the new settings (3.1); for `mvr_vrruntime` this holds
  before the game acts on its quit request (4.5).

Whether a command that changes nothing (a bad value, or the value already
set) rewrites the file is the implementation's choice. Settings that
affect rendering are picked up by the render thread on its next frame
(3.4).

### 4.3 Mod to UI: script invoke methods

The game has thiscall methods that call a public ActionScript function by
name on the movie owned by `this` (a menu object). They take a critical
section internally and must be called on the game's main thread (in
practice: from inside the menu Open wrappers of 4.4).

| Purpose | RVA 2015 | RVA 2020 | Arguments after `this` | `ret` |
|---|---|---|---|---|
| call with two strings | **0x42B450** | 0x43C840 | `const char* name, const char* a, const char* b` | `ret 0xC` |
| call with a bool | **0x42A7D0** | 0x43B920 | `const char* name, int v` (0 or 1 in a 32-bit slot) | `ret 8` |
| call with an int | **0x1341A0** | 0x144470 | `const char* name, int v` | `ret 8` |
| call with a float | **0x134240** | 0x144510 | `const char* name, float v` (32-bit float in a stack slot) | `ret 8` |

[sure, 2015: return sizes verified; each starts with the SEH prologue
`6A FF 68 <imm32> 64 A1 00 00 00 00`.] A function name the movie does not
define is ignored, so pushing to a stock (unpatched) movie is harmless.
Game fact: the float method passes a 32-bit float, which reaches the script
as a Number with that float's exact value (1.1 arrives as
1.100000023841858).

**Messages pushed to the page.** Each value below is pushed as one call of
a public ActionScript function that the implementation's own page defines.
The function names and the invoke method used for each value are the
implementation's choice (any method that carries the value's type: a bool
may go through the bool or the int method); they exist only between its DLL
and its own page (14).

| Message | Value pushed |
|---|---|
| VR running | true when the stereo device was created and its runtime started (02 §6.1); false before that, with runtime None, and after a runtime failure |
| `VRRuntime` | the runtime this run drives (5.2): None 0, OpenXR 1, OpenVR 2. None after a failure, the started runtime after a fallback; never read back into the stored setting |
| `DebugOptions` | on / off |
| `SkipStartupIntro` | on / off |
| `Rendering.MirrorViewType` | 0 both eyes, 1 right, 2 left |
| `Rendering.SymmetricMirrorView` | on / off |
| `Rendering.PixelScale` | the value |
| `Rendering.ClippingBlinders` | on / off |
| `Input.SnapRotation` | on / off |
| `Input.SnapRotationSegments` | the value (1.2) |
| `Input.VRInputRelativeWalking` | on / off |
| `Input.VRInputStrafing` | on / off |
| `Input.ToggleGrips` | on / off |
| `Input.ToggleCrouch` | on / off |
| One controls-help row | action and control, as two strings: one call per row of 04 §5.7 |

All messages are pushed at every push point (4.4). The page's state rules
(5.3) must hold after a push whatever order the values arrive in. The push
is made even with runtime None (VR running false), so the player can turn
VR on. If the invoke methods' addresses are not available, no push is made
and this is logged once. `StereoAntiAliasing` is not pushed (no menu
control). A pushed value never makes the page send a command back (5.2).

### 4.4 When to push settings

The menu objects' **Open** methods (vtable slot 0; thiscall, no stack
arguments, plain `ret`) are the moment their movie exists and can receive
calls. The same Open slots are wrapped for stereo-mode handling (05 §5);
both behaviours go in one wrapper, which calls the stock Open first with the
same `this`, because the movie must exist before it can receive calls.

| Menu | Vtable 2015 / 2020 | Stock Open 2015 / 2020 | Push after the stock Open |
|---|---|---|---|
| Front-end menu | **0xF000E0** / 0xF17840 | 0x441950 / 0x452B00 | yes |
| Pause menu | **0xF00180** / 0xF178E0 | 0x441D90 / 0x452F40 | yes, after entering mode 1 (05 §5) |
| Loading screen | **0xF00218** / 0xF17978 | 0x441EA0 / 0x453050 | yes, after entering mode 1 (05 §5); its movie has no options page, so the calls are ignored |

The loading-screen push is a user decision (r3): the Open wrapper exists
there anyway (05 §5), it costs a few calls per load that the stock movie
ignores, and it seeds the render-target record of 4.6 at the first loading
screen when no menu has opened before it. An implementation that also
defines the receiving functions in its loading-screen movie may use them;
none is required.

The front-end vtable's slot 0 is replaced only if it holds the stock Open
(image base + 0x441950); otherwise nothing is patched (01 §2.4). The first
push is logged once, not every push (11).

Controls-help rows are sent at every push. The page shows each row once.

The game-over menu's object and Open method are not identified (section 9),
so its copy of the page receives no push; 5.1 says what it shows.

### 4.5 Changing the runtime quits the game

The byte at RVA **0x17DA920** (2020: 0x17EAAD0) is the game's exit request
flag: getter RVA 0x614760 (`mov al, [flag]; ret`, verified), setter RVA
0x614770 (2015; 2020 setter not known, and not needed: writing the byte has
the same effect). When `mvr_vrruntime` accepts a value, the byte is set to
1 and the game shuts down normally. The new runtime is in the file before
the game's main loop next reads the flag (4.2), and applies at the next
launch.

### 4.6 Rebuilding render targets after a settings change

The byte at RVA **0x1358470** (2020: 0x1368620) is the game's display-reset
flag: when it is set, the game's frame loop rebuilds its back buffer and
render targets as after a window resize, asking the stereo device for the
render-target size again (03 §4). The mod keeps a record of the
`PixelScale`, `MirrorViewType` and `SymmetricMirrorView` the targets were
built with (taken from the settings at the first push or command), and sets
the flag to 1 only when a command changes one of them. Unchanged values
never set it: a rebuild freezes the picture for about 0.2 s. With runtime
None it is never set (there are no stereo render targets to resize).
Details of the rebuild are in 05 §10.

So pixel scale and mirror changes made in the menu apply without a
restart. 03 §4.2's rule still holds after the rebuild: the size the device
answers is the configured size.

## 5. The options page

What the page must offer and how it must behave. Labels, descriptions,
help texts, grouping and visual layout are the implementation's own; only
strings the game itself contains are named. The game's menu classes it is
built with are in section 7.

### 5.1 Where it appears

One new expandable entry (the VR page), titled `MotherVR` (the title
players know, a user decision), is added to the game's **Options** page,
after the stock Game, Video and Audio entries. Why there: the Options page
is the one page all three menus share through `GameMenu` (7.1).

- **Front end and pause menu:** the VR page is visible once a push has
  reached the movie (4.4), so a movie whose DLL is not the mod never shows
  it.
- **Game-over menu:** the VR page is hidden (spec-side decision r5: that
  menu receives no push, 4.4, so it could only show values that may be
  wrong; the same settings are reachable from the front end and the pause
  menu).

Stock ids (game facts in 7.3):

- Every stock page and item keeps its stock id. Automatic ids are handed
  out in creation order, so no new page or item is created before a stock
  page or item the game addresses by id, and no stock item's construction
  is moved. Pages and rows the game never addresses by automatic id (a
  root page whose id is replaced at once, rows created at run time) are
  exempt; 14 §2.7.2 gives the exact facts.
- Every VR page is attached before the movie assigns `menu.rootPage`,
  because the game finds pages and items (to restore a page with
  `setCurrentPage`, or to show, hide or enable an item by id) only through
  the maps built at that assignment.
- In the front end the Options page also holds the storage-device item,
  which `NewFrontendMenu` appends after the `GameMenu` items; whether the VR
  entry comes before or after it is the implementation's choice, because
  the game addresses that item by its fixed id (`SELECT_STORAGE_DEVICE`),
  not by position.

### 5.2 Controls

The page offers one control per setting below.

| Setting | What the control must do | Command sent on change (4.2) |
|---|---|---|
| `VRRuntime` | show the runtime this run drives (None, OpenXR, OpenVR, which players know as SteamVR; see below) and let the player choose another; choosing one quits the game (4.5), so the page makes that clear before the choice takes effect | `mvr_vrruntime 0` / `1` / `2` |
| `Rendering.MirrorViewType` | choose both eyes, right eye or left eye | `mvr_mirrorview <0..2>` |
| `Rendering.SymmetricMirrorView` | on/off | `mvr_mirrorviewsym <0/1>` |
| `Rendering.PixelScale` | choose a value from 0.5 to 4.0 in multiples of 0.1 and show the current value with one decimal | `mvr_pixelscale <value>` |
| `Rendering.ClippingBlinders` | on/off | `mvr_clipblinding <0/1>` |
| `Input.SnapRotation` | on/off | `mvr_snaprotation <0/1>` |
| `Input.SnapRotationSegments` | choose 24, 12, 6, 4, 3 or 2 (the step angle is 360/n degrees; showing it is the implementation's choice) | `mvr_snaprotationsegments <n>` |
| `Input.ToggleGrips` | on/off | `mvr_togglegrips <0/1>` |
| `DebugOptions` | on/off | `mvr_debug <0/1>` |
| `SkipStartupIntro` | on/off | `mvr_skipintro <0/1>` |
| Controls help | a read-only list of the pushed (action, control) rows (4.3) | none |

`Input.VRInputRelativeWalking`, `Input.VRInputStrafing` and
`Input.ToggleCrouch` have no effect in v1 and get no visible control (their
commands and pushes stay valid).

**The runtime shown is the one this run drives**, not the stored
preference (project decision of spec r6):

| This run | Runtime shown |
|---|---|
| the stored or forced runtime started | that runtime |
| the preferred runtime was not available and another one started (1.1) | the one that started |
| the chosen runtime failed to start, or failed later and detached (01 §5.3, 09 §7) | None |
| no runtime chosen (None stored or forced, or nothing available) | None |

The stored `VRRuntime` is not rewritten by any of these: a restart tries
the stored preference again. Why: a page that showed the stored runtime
after a failure would claim that runtime is running and offer only the
other choices; showing None tells the player VR is off and offers both
the stored runtime (a retry, through a restart) and the other one (the
way back, for example to SteamVR after OpenXR failed). Choosing any value
other than the one shown stores it and quits (4.2, 4.5), including the
value already stored.

Values and sending:

- On/off sends 1 or 0; a choice sends the integer value, not a label.
- Pixel scale: the 0.1 step applies from the control's first use, so the
  player never reaches a value between steps. A pushed value (a 32-bit
  float, 4.3) is shown and sent rounded to one decimal, so 1.1 shows as
  `1.1`. It is sent as a decimal string that 4.2 reads as the shown value
  (for example `1.5`).
- A control sends only when the player changes it, never when a pushed
  value sets it (game fact on the stock setters: 7.2). Each handler is
  added to its control once.
- Controls-help rows cannot be selected: pressing one changes nothing and
  does not enter the stock input-mapping selection, and the rows do not
  register with the game's input-mapping table (7.2).

### 5.3 State rules

- With VR running false, the rendering and input controls are disabled;
  the runtime control stays usable.
- In the **pause menu** the runtime cannot be changed: changing it quits
  the game, which must not happen in the middle of a level.
- While VR is running, the stock Video items below are disabled; with VR
  not running they are enabled. The rule holds whenever the Video page is
  shown, including after the game has replaced an item's data (7.5:
  replacing it resets `enabled`) and after the advanced Video options are
  shown or hidden. It applies in the menus that receive a push (front end
  and pause menu). The game-over menu has no VR-running state, so its Video
  items stay as in the stock game (section 9).

| Item (7.5) | Why it is disabled in VR |
|---|---|
| `_fieldOfViewItem` | the headset's projection sets each eye's field of view (03), so the setting has no effect while VR runs |
| `_deepColourItem` | the stereo path uses the standard back-buffer format whatever this setting says (01 `RiftTest5`), so it has no effect |
| `_motionBlurItem` | set off when stereo starts (02 §6.5): blur driven by head motion is unpleasant in a headset |
| `_depthOfFieldItem` | set off when stereo starts (02 §6.5): the game blurs by where it assumes the player looks, not where the eyes look |
| `_chromaticAberrationItem` | a flat-screen lens imitation; in a headset it adds colour fringes on top of the real lenses' (spec-side decision) |
| `_screenSpaceAmbientOcclusionItem` | never runs while stereo runs (05 §7.1): screen-space AO is wrong across the side-by-side target, so the setting has no effect |
| `_filmGrainIntensityItem` | the grain is fixed to the screen, so in a headset it stays glued to the view while the world moves (spec-side decision) |
| `_stereoModeItem` | the game's own Stereo Mode setting (sends `set_stereo_mode`); changing it runs the setting 0x18 handler (02 §6.5), which would break the stereo path (spec-side decision r5) |

- The stock Game page's Camera Speed (`_cameraSensitivityItem`, sends
  `set_gamepad_speed`) stays enabled in VR: it scales smooth turning on the
  stick, which goes through the game's own look (04 §5); snap turn
  ignores it (04 §6).

### 5.4 Initial values

The page is shown only after a push (5.1), so the player sees only pushed
values. Fields the page keeps for the pushed values start at the defaults
of section 1 (runtime OpenVR, VR running false).

### 5.5 `DebugOptions` in the menus

The game's UI already contains two debug entries that the stock scripts
hide (game strings):

- "Select Level" on the front-end Campaign page (menu item id
  `DEBUG_LOAD_LEVEL`, 15);
- "Trigger Debug Checkpoint" under Load Game (`DEBUG_CHECKPOINTS`, 12), in
  the front end, pause and game-over menus.

Required:

- Front end: each time the Campaign page is entered, "Select Level" is
  visible exactly when `DebugOptions` is on, and "Trigger Debug
  Checkpoint" is hidden.
- Pause menu: "Trigger Debug Checkpoint" is visible exactly when
  `DebugOptions` is on, from the push onwards and whenever the control
  changes.
- Game-over menu: "Trigger Debug Checkpoint" stays hidden as in the stock
  game (no push, 4.4).

### 5.6 What applies when

Runtime changes quit the game and apply on the next launch.
`SkipStartupIntro` applies on the next launch. `PixelScale` and the mirror
settings apply at once through a render-target rebuild (4.6). Everything
else applies at once without a rebuild.

## 6. Skip intro

At startup the front end asks the loading-screen movie to play the bootup
sequence. The call is made from the front-end menu's Open method: at RVA
**0x441AED** (2020: 0x452C9D) the instruction `68 <imm32>` pushes the
address of the string `"ShowBootup"` (2015: bytes `68 78 E2 2F 01`, string at
RVA 0xEFE278; 2020: the string's address is whatever this operand holds),
followed by the arguments `1` (PC) and the language id, and a call to the
game's movie-invoke method at RVA 0x42AAD0 (2015; thiscall, `ret 0xC`). The
movie's `ShowBootup(isPC, language)` plays the publisher, studio and
hardware ident videos (`Movies/FOX_IDENT.usm`, `Movies/CA_IDENT.usm`,
`Movies/AMD_IDENT.usm` on PC) and the legal screen, then reports
`FinishedBootup` to the game.

The site is patched only if its byte is 0x68 and the operand points to the
NUL-terminated `ShowBootup`; otherwise nothing is patched (01 §2.4).

Required: when `SkipStartupIntro` is on at startup **and** the options
archive is in use (14: the UI archive pointer was redirected), the operand
(site + 1, 4 bytes) points at a NUL-terminated string, owned by the mod,
naming the skip-intro function that the project's loading-screen movie
defines (14; the name is the implementation's choice). That function
takes the same `(isPC, language)` arguments, plays none of the ident videos
or the legal screen, and still reports `FinishedBootup`, so the game
continues to the title screen. The event is logged (11). Without the
options archive the stock movie has no such function, so the operand is
left alone. The setting is read once at startup.

## 7. The game's UI scripting interface (game facts)

What the page is built with. The archive the movies live in and how the
modified movies are produced and validated is in 14.

### 7.1 Movies

UI movies (`.GFX`) are uncompressed Scaleform files: magic `GFX`, version
byte 0x0B, u32 total length, then a SWF-equivalent tag stream. Their
ActionScript 3 code lives in DoABC tags.

The three game menus share one class, `frontend.GameMenu` (a
`MovieClipEx`), in `NEWFRONTENDMENU.GFX` (subclass `NewFrontendMenu`),
`PAUSEMENU.GFX` (subclass `PauseMenu`) and `GAMEOVERMENU.GFX` (subclass
`GameOverMenu`). Each movie has its own copy of the class; a change to the
shared behaviour must be made in all three.

### 7.2 Building pages

In its constructor `GameMenu` creates its items as fields and attaches them
with `page.addItem(item)`; the Options page (`_optionsItem.page`) gets the
Game, Video and Audio expandable items in that order. After attaching, the
constructor calls its set-up methods (ids, descriptions, transitions,
visibility); the stock visibility set-up hides the debug checkpoint item.
The item classes, in package `frontend.menu`:

| Class | Constructor (stock) | Behaviour |
|---|---|---|
| `MenuItem` | `(label:String, onPress:Function = null)` | a plain entry; `onPress` runs when it is chosen |
| `MenuItemExpandable` | `(label:String, onPress:Function = null, items:Array = null)` | an entry that opens a sub-page; `onPress` runs when the page opens; its `page` (`MenuPage`) takes `addItem`, `addItemAtIndex`, `removeItem`, `removeAllItems` |
| `MenuItemOptionCheckBox` | `(label:String, startTrue:Boolean = true, descriptions:Array = null, onIndexChange:Function = null)` | on/off; `selected`, `selectedItem` (Boolean) |
| `MenuItemOptionOptionStepper` | `(label:String, items:Array = null, labels:Array = null, onIndexChange:Function = null)` | steps through `items` showing `labels`; `selectedIndex`, `selectedItem`, `setItems(items, labels, descriptions)` |
| `MenuItemOptionSlider` | `(label:String, items:Array = null, onChange:Function = null, isSequence:Boolean = true, minimum:Number = 0, maximum:Number = 10, showValue:Boolean = false)` | with `isSequence` false, a continuous slider from `minimum` to `maximum` (`value`) whose handler is a value-change listener |
| `MenuItemOptionButton` | `(name:String, label:String = null, onPress:Function = null)` | `name` is the left column; `label` is the right-column text (stored in `MenuItemDataButton.label`, shown as `--` when empty); a press always dispatches `ITEM_PRESS` |
| `MenuItemOptionInputMapper` | `(name:String, label:String = null, onPress:Function = null)` (as `MenuItemOptionButton`) | a two-column row as on the stock input-mapping page; see below |

Slider facts: the shown text is `String(value)`, with no formatting.
`snapping` defaults to false, so dragging gives arbitrary values.
`snapInterval` defaults to `maximum / 10`, and reaches the live slider only
when the item's component is created (when its page is shown) or when
`minimum` or `maximum` is set; setting it later has no effect on the
`onNext` / `onPrev` steps.

Option setters: `selected`, `selectedItem`, `selectedIndex` (check box,
stepper) and `value` (slider) detach the item's listeners while they update
the component and reattach them on the next frame. Without a component
(page not shown) they change only the data. So a value set through these
setters never fires the item's change handler; handlers fire on player
interaction. `MenuItemOption.addEventListener` adds every listener with a
weak reference and throws if the same function is added twice.

Input-mapper rows: pressing one toggles its `selected`. The selection is one
static slot, `selectedInputMapper`, shared with the stock Input Mapping
page; the game renames the selected row with
`setSelectedInputMappingItemLabel` and clears it with
`setSelectedInputMappingItemUnselect`. Setting a row's `data` registers it
in a static table used by `setInputMappingItemLabel(id, ...)`. A
controls-help row built from this class must therefore not be selectable
(for example with `enabled` false) and must not set `data` (5.2).

Common item properties: `id` (`MenuItemId`), `name`, `description`,
`visible`, `enabled`, `data`. `enabled`, `visible` and `description` are
stored in the item's `MenuItemData`. Labels are localisation keys
(`AI_UI_MENU_...`) or literal text: the stock "Trigger Debug Checkpoint" and
"Select Level" items use literal English, so a page can use literal labels
without new localisation entries. A page's `BEFORE_BACK` event
(`MenuPageEvent`) and an expandable's `onPress` are where per-page refresh
logic goes (the stock Video page uses them).

### 7.3 Ids and page maps

- `frontend.enum.MenuItemId` holds fixed ids (for example
  `DEBUG_CHECKPOINTS` = 12, `DEBUG_LOAD_LEVEL` = 15) and hands out
  automatic ids from 300 up. Every `MenuPage` (`MenuPageId`) and every
  `MenuItemData` takes the next automatic id when it is constructed, in
  creation order, from a counter in each movie.
- Field initializers run before the constructor body; subclass
  (`NewFrontendMenu`) items built before `super()` come before all
  `GameMenu` items.
- The subclass's `menu.rootPage = ...` (after `super()`) builds the page
  and item maps once (`rebuildPageMap`, `rebuildItemMap`). The game's
  `setCurrentPage(pageId, index)`, `setItemVisible(id:int,
  visible:Boolean)`, `setItemEnabled(id:int, enabled:Boolean)` and
  `setItemDescription(id:int, text:String)` find pages and items only
  through those maps.
- `on_page_changed` sends the page id to the game; `on_item_selected` sends
  the selected item's index on the current page.
- The front-end Options page also holds `_selectStorageDeviceItem`, which
  `NewFrontendMenu` appends after `super()`; it is addressed by id
  `SELECT_STORAGE_DEVICE`.

### 7.4 Calls between native code and script

- **Native to script.** The game calls public functions of the menu class
  by name through the invoke methods of 4.3 (for example
  `setInvertXAxis(bool)`, `setFieldOfView(int)`, `setStereoMode(int)`). A
  page adds its own public functions (4.3), which the same methods reach by
  name.
- **Script to native.** The class's helper `call(name, ...args)` calls
  `ExternalInterface.call` with the movie's prefix in front of `name`
  (`"fe_"` front end, `"go_"` game-over menu, `""` pause menu). Stock names
  include `set_mouse_speed`, `set_gamepad_speed`, `set_field_of_view`,
  `set_stereo_mode`, `save_engine_settings`, `on_page_changed`,
  `on_item_selected`, `update_debug_checkpoints` and
  `jump_to_debug_checkpoint`. All reach the handler of 4.1. Mod commands
  bypass the helper (4.1).
- **Return values.** A script-to-native call can return a value: the stock
  `ImageSubstitution.get_button_from_action_mapping` returns
  `String(ExternalInterface.call(...))`. How `AI.exe` sets that value has
  not been traced (section 9).

### 7.5 Video options and debug items

- The Video page's items are fields of the menu class
  (`_fieldOfViewItem`, `_deepColourItem`, `_motionBlurItem`,
  `_depthOfFieldItem`, `_chromaticAberrationItem`,
  `_screenSpaceAmbientOcclusionItem`, `_filmGrainIntensityItem`,
  `_stereoModeItem`, ...); greying one out (the Video items listed in 5.3)
  is setting its `enabled` to false.
- The game's `setFieldOfViewData`, `setDeepColourData`,
  `setMotionBlurData`, ... and `setStereoModeData` replace an item's
  `MenuItemData` (only the id is kept), so `enabled` set earlier is lost.
  `setAdvancedVideoOptionsVisible(bool)` (public) toggles `visible` on the
  advanced Video items, `_stereoModeItem` included.
- `_stereoModeItem` sends `set_stereo_mode`, the game's own Stereo Mode
  setting (02 §6.5).
- `GameMenu` creates "Trigger Debug Checkpoint"
  (`MenuItemId.DEBUG_CHECKPOINTS`) under Load Game and hides it in its
  set-up. `NewFrontendMenu` creates "Select Level"
  (`MenuItemId.DEBUG_LOAD_LEVEL`) on the Campaign page; the Campaign
  expandable's open handler is where 5.5's front-end rule goes.

### 7.6 Movie changes that belong to the options page

| Movie(s) | Required change |
|---|---|
| `NEWFRONTENDMENU.GFX`, `PAUSEMENU.GFX`, `GAMEOVERMENU.GFX` (`GameMenu`) | the VR entry and its controls (5.1 to 5.4), sending the commands of 4.2 directly through `ExternalInterface.call`; the public functions that receive the pushes of 4.3; the debug-item visibility (5.5); the Video-item greying (5.3) |
| `NEWFRONTENDMENU.GFX` (`NewFrontendMenu`) | 5.5's Campaign-page rule |
| `LOADINGSCREEN.GFX` | the skip-intro function (section 6) |

Other movie changes (VR glyphs, prompt substitution, HUD layout) are listed
in 14 with their reasons in 04 and 05.

## 8. Acceptance

Checks that show an implementation is correct. "Log" means `mothervr.log`
(11).

1. Delete `mothervr.json` and start the game: the welcome message appears
   before the game renders; after it is closed, `mothervr.json` exists and
   parses to the 3.2 content (`VRRuntime` `"OpenVR"`). Kill the game
   before closing the message on a fresh install: no file is written, and
   the next start shows the message again.
2. Put `"Oculus"` in `VRRuntime` with a 32-bit OpenXR runtime installed:
   the log shows the preference OpenXR and the OpenXR backend starts; the
   page shows the runtime as OpenXR; the file still says `"Oculus"` until a
   menu change, then says `"OpenXR"`. `-vrforceoculus` behaves as
   `-vrforceopenxr`.
2a. Runtime None (in the file or by `-vrforcenone`): the game runs exactly
   as unmodded except for the VR page in Options (and skip intro when on);
   the log shows the UI-only patch set.
3. Start with `-vrforceopenxr` on a machine with no 32-bit OpenXR runtime:
   the log shows OpenXR preferred, forced, and chosen None; the game runs
   flat; the file is unchanged. Without the switch, the same machine
   chooses OpenVR.
4. `-vrforcenone -vrforceopenvr` starts OpenVR (last wins);
   `-VRFORCEOPENVR` works (case-insensitive); `-vrforceopenvrx` does
   nothing.
5. `-vrscale 1.6` gives world scale 1.0; `-vrscale 0.8` and
   `-VRSCALE 0.8` give 0.8 (head motion scaled, 03);
   `-vrscale 0.8 -vrscale 1.2` gives 1.2; `-vrscale` as the last argument
   gives 1.0; an unknown argument such as `-foo` changes nothing.
6. Hand-edit `PixelScale` to 9, `SnapRotationSegments` to 5 and add
   `"Foo": 1` at the root and `"Bar": 2` inside `Input`: the log's settings
   entry shows pixel scale 1; the menu shows 1.0 and 4 segments; after
   changing any option the file has `PixelScale` 1.0,
   `SnapRotationSegments` 4, and still has `Foo` and `Input.Bar`.
7. Start the game with a hand-edited file (any valid content, including a
   missing key): after start-up and play without a menu change, the file is
   byte-for-byte unchanged.
8. A file with invalid JSON: the game starts with defaults, no welcome
   message, the file is unchanged until a menu change, which replaces it.
9. Change the pixel scale in the menu during gameplay: one short freeze, the
   headset image sharpness changes at once, the log shows a new
   render-target size (03). Re-select the same value: no freeze. Open and
   close the VR page several times without changing anything: no freeze.
10. Choose OpenXR on the front end's VR page: the game exits at once and
    the file says `"OpenXR"`; in the pause menu the runtime cannot be
    changed.
11. Turn clipping blinders off in the pause menu and push the head into a
    wall: no blackout, without leaving the menu or restarting.
12. With runtime None, or a runtime that failed to start, Options shows
    the VR page with the runtime None, its rendering and input controls are
    greyed, and the runtime can still be changed in the front end. After an
    OpenXR failure, `mothervr.json` still says `"OpenXR"`; choosing OpenXR
    on the page quits and the next launch tries OpenXR again, choosing
    SteamVR quits and the next launch runs OpenVR. With `"OpenXR"` stored
    and no usable OpenXR runtime (fallback to OpenVR), the page shows
    OpenVR.
13. `DebugOptions` on: the front-end Campaign page shows Select Level;
    Load Game shows Trigger Debug Checkpoint in the pause menu; off hides
    both; the log gains the gated diagnostics (1.3).
14. `SkipStartupIntro` on, restart: the title screen appears without the
    ident videos or the legal screen; the log records that the intro was
    skipped.
15. Open the options page several times: each controls-help row appears
    once; pressing a row changes nothing, and the stock Input Mapping page
    still selects and renames its own rows normally afterwards.
16. A stock (non-mod) `ExternalInterface` call such as
    `fe_set_field_of_view` still works on the Video page (forwarded
    untouched); `mvr_mirrorview 7`, `mvr_pixelscale 9` and `mvr_vrruntime`
    without a value change no setting, and the last does not quit.
17. In VR, open the Video page in the front end and the pause menu: the
    eight items of 5.3 are greyed, also after showing and hiding the
    advanced options; Camera Speed on the Game page is not greyed. With
    runtime None they are all enabled.
18. Die in game: the game-over menu's Options page has no VR page.
19. With the script event tap of 11 on, visit every stock page of all three
    menus with the stock archive and then with `MOTHER.PAK`: the
    `on_page_changed` ids are identical.

## 9. Open questions

- The game-over menu's object, vtable and Open method, needed to push
  settings into its copy of the page, which would let it show the VR page
  and grey its Video items like the other menus. Look for the vtable whose
  Open calls the same movie-load routine with `GAMEOVERMENU`, near the
  front-end and pause vtables at 0xF000E0 / 0xF00180. An alternative is a
  pull through a script-to-native call's return value (7.4), once it is
  traced how `AI.exe` sets that value.
- The full meaning of the quit flag at RVA 0x17DA920 beyond "the game exits
  cleanly when it is set" [likely]; the requirement relies only on that.
  Follow the getter's callers in the main loop.
- Whether the game's video menu reacts well to the display-reset flag being
  set while the Options page is open in all three menus (seen in the front
  end and pause menu only).
- 2020 build: the `ShowBootup` string RVA, the movie-invoke method and the
  quit-flag setter are not listed; read the first from the push operand
  (section 6); the other two are not needed.
