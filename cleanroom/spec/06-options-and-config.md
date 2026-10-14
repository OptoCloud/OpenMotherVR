<!-- Provenance: game analysis of AI.exe 2015 (UI callback handler, script setter methods, menu open methods, bootup movie call, UI archive path, quit flag); format analysis of the game's DATA/UI.PAK and its Scaleform movies; the stock game UI scripts; MotherVR 0.8.1's settings, config file format and options menu, observed and restated as requirements; user requirements (no shipped game archive, v1 fixes, runtime values None / OpenVR / OpenXR, -vrscale is the world scale); the stock game's ActionScript menu classes (from the player's UI.PAK); checksums computed from the Steam 2015 DATA/UI.PAK; answers to Q1, Q2, Q6 (ANSWERS.md). Revision: r2 -->

# 06 Options and config

The mod's settings, the config file, the protocol between the game's UI and
the mod, the options page the player sees, skip-intro, and how the page is
added to the game without shipping a modified game archive.

Addresses are RVAs in the 2015 Steam build (image base 0x400000) unless
marked. 2020 values come from the per-build address table and are not
re-verified.

## 1. Settings

| Setting | Type | Default | Range / values | Takes effect |
|---|---|---|---|---|
| VR runtime | enum | None | 0 None, 1 OpenXR, 2 OpenVR (SteamVR) | next launch; changing it from the menu exits the game (4.5) |
| Mirror view type | enum | Right eye | 0 Both eyes, 1 Right eye, 2 Left eye | immediately (doc 03) |
| Symmetric mirror view | bool | on | | immediately (doc 03) |
| Pixel scale | float | 1.0 | 0.5 to 4.0, step 0.1; out of range resets to 1.0 | when render targets are next sized (doc 03) |
| Clipping blinders | bool | on | | immediately (doc 05: head-in-wall threshold) |
| Snap rotation | bool | on | | immediately (doc 04 §6, §7) |
| Snap rotation segments | int | 12 | 24, 12, 6, 4, 3, 2 | immediately |
| Relative walking | bool | on | | stored only; no effect in v1, hidden in the menu |
| Strafing | bool | on | | stored only; no effect in v1, hidden in the menu |
| Toggle grips | bool | off | | immediately (doc 04 §5.6) |
| Toggle crouch | bool | off | | stored only; no effect in v1, hidden in the menu |
| Dev. debug options | bool | off | | immediately, UI only (5.4) |
| Skip startup intro | bool | off | | next launch (section 6) |

The defaults are the values the original seeded before reading its config
file. With the runtime defaulting to None, VR is off until the player
enables it in the menu; a first-launch message says so (3.1). With runtime
None the game is the unmodded game except for the VR options page (01
§2.3).

Snap segment normalisation: any stored value maps to the largest supported
value not above it (≥ 24 → 24, ≥ 12 → 12, ≥ 6 → 6, ≥ 4 → 4, ≥ 3 → 3,
≥ 2 → 2), and values below 2 map to 12. The normalised value is the one used
for turning and shown in the menu (the original snapped with the raw value
but showed the normalised one).

**Runtime values.** v1 has two backends, OpenVR (03) and OpenXR (09). The
original's value 1 selected its Oculus (LibOVR) backend, which v1 does not
have; Meta's PC runtime is itself an OpenXR runtime, so value 1 now means
**OpenXR**. The menu slot the original labelled "Oculus" selects OpenXR and
is relabelled (5.1), and a config file or command line that says "Oculus"
means OpenXR. There is no value 3.

The world scale is not a setting: it exists only as the `-vrscale`
command-line override (section 2, 03).

## 2. Command line

These override the stored settings for one run and are not saved:

| Argument | Effect |
|---|---|
| `-vrforcenone` | runtime None for this run (the unmodded game, 01 §2.3) |
| `-vrforceopenxr` | OpenXR |
| `-vrforceoculus` | OpenXR (the original's switch; kept so existing shortcuts work) |
| `-vrforceopenvr` | OpenVR |
| `-vrscale <f>` | **world scale** `s` for this run (metres scale of head and eye positions, 03), accepted in 0.495 to 1.505, otherwise 1.0. Not the pixel scale: the pixel scale comes only from the config file and the menu |

If several force switches are given, the last one wins. The number after
`-vrscale` is a decimal with a dot, separated from the switch by a space.

## 3. Config file

### 3.1 Location and life cycle

- File `mothervr.json` in the game's install folder (the folder holding
  `AI.exe`). The same name as MotherVR so existing settings carry over.
- Read once at startup, before the VR runtime is chosen.
- Written after every settings command from the menu (4.2), whole file.
- If the file is missing at startup: show a one-time message box that
  welcomes the player and says VR is enabled in Options > MotherVR after the
  game starts, then write the file with defaults.
- A file that fails to parse must not crash the game: use defaults for
  everything, and do not overwrite the broken file until the player changes
  a setting.

### 3.2 Shape (as MotherVR 0.8.1 writes it)

```json
{
    "Settings": {
        "VRRuntime": "OpenVR",
        "Rendering": {
            "MirrorViewType": "BothEyes",
            "SymmetricMirrorView": true,
            "PixelScale": 1.0,
            "ClippingBlinders": true
        },
        "Input": {
            "SnapRotation": true,
            "SnapRotationSegments": 12,
            "VRInputRelativeWalking": true,
            "VRInputStrafing": true,
            "ToggleGrips": false,
            "ToggleCrouch": false
        },
        "DebugOptions": false,
        "SkipStartupIntro": false
    }
}
```

String values: `VRRuntime` is written as `"None"`, `"OpenXR"` or
`"OpenVR"` (MotherVR 0.8.1 does not know `"OpenXR"`; it is written anyway,
because writing `"Oculus"` would make the original start its LibOVR
backend); read as in 3.3;
`MirrorViewType` is `"BothEyes"`, `"RightEye"` or `"LeftEye"` (in that
numeric order, 0..2). `DebugOptions` is the dev. debug options flag. Key
order and indentation do not matter.

### 3.3 Compatibility

The mod must read:

- files written by MotherVR 0.8.1 (3.2);
- files written by the earlier reconstruction in this project, which differ
  as follows: a top-level `"DebugOptions"` **object** holding
  `"SkipStartupIntro"` and `"DevDebugOptions"` booleans (instead of the two
  booleans inside `"Settings"`), and `VRRuntime` values `"Disabled"` (=
  None) or `"OpenXR"`;
- `VRRuntime` `"Oculus"` (MotherVR's value 1): read as OpenXR, and left as
  it is in the file until the player changes a setting (the rewrite then
  stores `"OpenXR"`);
- integer values where an enum string is expected (0..2 as in section 1;
  `VRRuntime` 1 = OpenXR);
- string values compared case-insensitively.

Each key is read independently: a missing key or a value of the wrong type
falls back to that setting's default without affecting other keys. Unknown
keys must be preserved when the file is rewritten (later versions add
settings). The mod writes the 3.2 shape, so MotherVR 0.8.1 can still read
the file.

All settings in section 1 are persisted, including dev. debug options and
skip intro. Command-line overrides are not.

## 4. UI protocol (game facts and required use)

### 4.1 UI to mod: the UI callback handler

The game's Scaleform movies call native code with
`ExternalInterface.call(name, args...)`. The game dispatches these through
one handler object whose vtable is at RVA **0xF00074**; slot 1 (address of
the slot: RVA **0xF00078**, 2020: 0xF177D8) holds the handler method, stock
implementation RVA **0x43A0A0** (2020: 0x44B250):

```
void __thiscall Callback(void* movie, const char* name, const void* args, unsigned argCount)
```

`ret 0x10`. (The signature matches Scaleform's external-interface callback;
`args` points to `argCount` Scaleform values.) The stock method hashes
`name`, looks it up in a table of registered names (the table hangs off
`this+0x0C`), and calls the registered function; unknown names are
ignored.

The settings commands are sent as a single string with no arguments:
`name` is `"<command> <value>"`, for example `"mvr_pixelscale 1.5"`.

Game fact: the stock menu scripts do not call `ExternalInterface.call`
directly; the shared menu class prefixes every name with a per-movie prefix
(`"fe_"` in the front end, `"go_"` in the game-over menu, none in the pause
menu; 7.6). A page that sends its commands through that helper produces
`"fe_mvr_pixelscale 1.5"`. The handler must accept a command with or without
one of these two prefixes; a page may also call `ExternalInterface.call`
directly with the bare command.

Required: replace the slot (it is in read-only data; change the page
protection) with a method of the same ABI. If `name` starts with `mvr_`,
handle it and do not forward it; otherwise forward the call unchanged to
the stock method. Parse a copy of the string: the caller's buffer must not
be modified (forwarding a string that was tokenised in place corrupts it).

### 4.2 Commands

| Command | Value | Effect |
|---|---|---|
| `mvr_vrruntime` | 0 None, 1 OpenXR, 2 OpenVR | set VR runtime, save, then request the game to quit (4.5) |
| `mvr_mirrorview` | 0, 1, 2 | mirror view type |
| `mvr_mirrorviewsym` | 0 / 1 | symmetric mirror view |
| `mvr_pixelscale` | decimal, e.g. `1.5` | pixel scale; outside 0.5..4.0 becomes 1.0 |
| `mvr_clipblinding` | 0 / 1 | clipping blinders, applied at once |
| `mvr_snaprotation` | 0 / 1 | snap rotation |
| `mvr_snaprotationsegments` | 24, 12, 6, 4, 3, 2 | snap segments (normalise anything else) |
| `mvr_inputrelativewalking` | 0 / 1 | relative walking (stored) |
| `mvr_inputstrafing` | 0 / 1 | strafing (stored) |
| `mvr_togglegrips` | 0 / 1 | toggle grips |
| `mvr_togglecrouch` | 0 / 1 | toggle crouch (stored) |
| `mvr_debug` | 0 / 1 | dev. debug options |
| `mvr_skipintro` | 0 / 1 | skip startup intro |

Booleans are any integer, non-zero meaning on. A missing or unparsable
value leaves the setting unchanged (the original did not check for a
missing value). Every
handled command saves the config file. Settings that affect rendering must
be picked up by the stereo code on its next frame (doc 03).

### 4.3 Mod to UI: script setter methods

The game has thiscall methods that invoke a public ActionScript function
by name on the movie owned by `this` (a menu object). They take a critical
section internally and must be called on the game's main thread.

| Purpose | RVA 2015 | RVA 2020 | Arguments after `this` | `ret` |
|---|---|---|---|---|
| call with two strings | **0x42B450** | 0x43C840 | `const char* name, const char* a, const char* b` | `ret 0xC` |
| call with a bool | **0x42A7D0** | 0x43B920 | `const char* name, bool v` (32-bit slot) | `ret 8` |
| call with an int | **0x1341A0** | 0x144470 | `const char* name, int v` | `ret 8` |
| call with a float | **0x134240** | 0x144510 | `const char* name, float v` | `ret 8` |

The patched menu movies (section 7.7) define these functions, which the mod
calls:

| Function | Argument | Meaning |
|---|---|---|
| `setMotherVROptionsEnabled` | bool | VR is running (a headset device exists and started); call first |
| `setMotherSetVRRuntime` | int 0..2 | configured runtime: 0 None, 1 OpenXR, 2 OpenVR |
| `setMotherDebug` | int 0 / 1 | dev. debug options |
| `setMotherSkipIntro` | bool | skip startup intro |
| `setMotherMirrorView` | int 0..2 | mirror view type |
| `setMotherMirrorViewSym` | bool | symmetric mirror |
| `setMotherPixelScale` | float | pixel scale |
| `setMotherClipBlinding` | bool | clipping blinders |
| `setMotherSnapRotation` | bool | snap rotation |
| `setMotherSnapRotationSegments` | int | normalised segments |
| `setMotherInputRelativeWalking` | bool | relative walking |
| `setMotherInputStrafing` | bool | strafing |
| `setMotherToggleGrips` | bool | toggle grips |
| `setMotherToggleCrouch` | bool | toggle crouch |
| `addMotherViewControl` | string, string | add one (action, control) row to View Controls (doc 04 §5.7) |

These names are the interface between the mod and its own movie patches
(7.7). The implementation writes both sides, so it may rename them as long
as both match; keeping them costs nothing. Unlike the menu setters, they
are not in the stock movies: a movie without the patch ignores the call.

Also call the push when the runtime is None (the page must show the runtime
choice so the player can turn VR on); `setMotherVROptionsEnabled` is then
false.

### 4.4 When to push settings

The menu objects' **open** methods are the moment the menu movie exists and
can receive calls. The mod wraps vtable slot 0 (Open; thiscall, no args,
plain `ret`) of:

| Menu | Vtable RVA 2015 | Stock Open RVA | Vtable RVA 2020 | Stock Open RVA 2020 |
|---|---|---|---|---|
| Front-end menu | **0xF000E0** | 0x441950 | 0xF17840 | 0x452B00 |
| Pause menu | **0xF00180** | 0x441D90 | 0xF178E0 | 0x452F40 |

After calling the stock Open, push every setting to that menu object with
the setters of 4.3 (same `this`), `setMotherVROptionsEnabled` first and
`setMotherSetVRRuntime` second, then the View Controls rows. Add the rows
only once per menu movie instance (the original re-added them on every
open, so they could appear twice). The same Open slots are also wrapped for
stereo-mode handling (doc 05); both behaviours go in one wrapper.

The game-over menu movie carries the same options page; its menu object and
open method are not yet identified (section 8).

### 4.5 Changing the runtime quits the game

The byte at RVA **0x17DA920** (2020: 0x17EAAD0) is read through a one-line
getter (RVA 0x614760) from many places; setting it to 1 makes the game shut
down (inferred: it is the flag the original set both for the runtime menu
items, labelled "selecting this will exit the game", and for the Oculus
runtime's quit request). After `mvr_vrruntime` the mod saves the config and
sets this byte.

## 5. The options page

### 5.1 Placement and items

A "MotherVR" entry is added to the game's Options page (after Game, Video,
Audio) in the front-end, pause and game-over menus, with the description
"Adjust MotherVR settings" or similar. Its page:

```
MotherVR
├── VR Runtime                      (submenu)
│   ├── VR Runtime Set              read-only stepper: None / OpenXR / SteamVR
│   ├── Use OpenXR                  sends mvr_vrruntime 1
│   ├── Use SteamVR                 sends mvr_vrruntime 2
│   └── Disable VR                  sends mvr_vrruntime 0
├── Rendering                       (submenu)
│   ├── Mirror View                 stepper: Both Eyes (0) / Right Eye (1) / Left Eye (2)
│   ├── Mirror View Symmetry        checkbox
│   ├── Pixel Scale                 slider 0.5 to 4.0, snapped to 0.1
│   └── Clip Blinding               checkbox
├── Input                           (submenu)
│   ├── View Controls               read-only rows from addMotherViewControl
│   ├── Snap Rotation               checkbox
│   ├── Snap Rotation Segments      stepper: 24 (15°), 12 (30°), 6 (60°), 4 (90°), 3 (120°), 2 (180°)
│   ├── Input Relative Walking      checkbox (hidden in v1)
│   ├── Input Strafing              checkbox (hidden in v1)
│   ├── Toggle Aim/Motion Sensor    checkbox (toggle grips)
│   └── Toggle Crouch               checkbox (hidden in v1)
├── Dev. Debug Options              checkbox
└── Skip Startup Intro              checkbox
```

Labels match the original so existing guides apply, except that the
original's "Oculus" (the stepper value and "Use Oculus") reads "OpenXR".
Each control sends its command (4.2) when changed; the pixel scale is
rounded to one decimal before sending.

### 5.2 State rules

- The MotherVR entry is visible once `setMotherVROptionsEnabled` has been
  called (so it never shows without the mod).
- Rendering and Input are enabled only when VR is running.
- The "VR Runtime Set" stepper is display only.
- The "Use ..." item for the current runtime is hidden. The three "Use /
  Disable" items carry the description "Requires restart! Selecting this
  will exit the game." or equivalent.
- In the pause menu the VR Runtime submenu is disabled (the runtime can
  only be changed from the front end).
- When VR is running, the stock Video options that VR forces or that make
  no sense in the headset are disabled: field of view, deep colour, motion
  blur, depth of field, chromatic aberration, screen-space ambient
  occlusion, film grain. This applies in all three menus (the original
  missed the game-over menu).

### 5.3 Restart requirements

Runtime changes exit the game and apply on the next launch. Skip intro
applies on the next launch. Pixel scale applies when render targets are
next sized. Everything else applies at once.

### 5.4 Dev. debug options

The game's UI already contains two debug entries that the stock scripts
hide:

- "Select Level" on the front-end Campaign page (menu item id
  `DEBUG_LOAD_LEVEL`);
- "Trigger Debug Checkpoint" under Load Game (front-end and pause menu).

Both must be visible exactly when dev. debug options is on: hidden by
default, shown or hidden as the checkbox changes, and set accordingly when
the setting is pushed on menu open. (The original left the pause menu's
debug-checkpoint entry visible regardless and did not apply the pushed
value to visibility.) The flag has no other effect.

## 6. Skip intro

At startup the front end asks the loading-screen movie to play the bootup
sequence. The call is made from the front-end menu's Open method: at RVA
**0x441AED** (2020: 0x452C9D) the instruction `68 <imm32>` pushes the
address of the string `"ShowBootup"` (at RVA 0xEFE278), followed by the
arguments `1` (PC) and the language id, and a call to the game's
movie-invoke method at RVA 0x42AAD0 (thiscall, `ret 0xC`). The movie's
`ShowBootup(isPC, language)` plays the publisher, studio and hardware
ident videos (`Movies/FOX_IDENT.usm`, `Movies/CA_IDENT.usm`,
`Movies/AMD_IDENT.usm` on PC) and the legal screen, then reports
`FinishedBootup` to the game.

Required: when skip intro is on at startup, point the push operand (RVA
0x441AEE, 4 bytes) at a NUL-terminated `"SkipBootup"` string owned by the
mod. The patched loading-screen movie implements `SkipBootup(isPC,
language)`: the same set-up as `ShowBootup`, but with a playlist that only
waits about half a second and plays nothing, so the game still receives
`FinishedBootup` and continues. The setting is read once at startup.

## 7. Delivering the menu: patching the player's UI.PAK

### 7.1 Requirement

The options page, the VR glyphs and the other UI changes of 7.7 are
produced **at install time (or at the mod's first start) by patching the
player's own `DATA\UI.PAK`**. The project ships only its own material: the
code that patches, the patch description (movie changes it authored, as
bytecode or as a description it applies), and its own glyph images. It
never ships a copy or a modified copy of a game archive or of a game movie,
and never ships another mod's archive, movies or patch data.

The implementation must **author its own movie changes**. It cannot reuse
another mod's: those are that mod's expression. What it may use is this
section: the archive format (7.2), the checks (7.4), the game's UI scripting
interface (7.6) and the required menu behaviour (sections 4, 5 and 7.7),
all of which are facts about the game or requirements.

### 7.2 Archive format (PAK2)

Little-endian:

| Offset | Content |
|---|---|
| 0 | magic `PAK2` |
| 4 | u32 `namesSize`: bytes of the name block |
| 8 | u32 `count`: number of entries |
| 12 | u32 `alignment` |
| 16 | name block: `count` NUL-terminated names, in entry order, packed (`namesSize` is exactly the sum of the name lengths plus one NUL each) |
| 16 + namesSize | u32 `end[count]`: absolute file offset where entry *i* ends |
| then | data: entry 0 starts right after the offset table, entry *i* > 0 starts at `end[i-1]`; each start is rounded up to `alignment` with zero padding that is not part of the entry. The last entry ends at the end of the file |

The Steam 2015 `DATA\UI.PAK`: 17,521,556 bytes, 505 entries, a 16,480-byte
name block, alignment 4, first entry at offset 18,516. Names are upper-case
paths with forward slashes, for example `DATA/UI/PAUSEMENU.GFX` and
`DATA/UI/GLYPH_360_A.DDS`, and are **sorted in ascending byte order**.

Writing an archive:

- keep the format, the alignment (4) and the padding rule;
- keep the entries sorted by name in ascending byte order: new entries go
  at their sorted position, not at the end. [guess] Whether the game's
  lookup needs the order is not known (it may search by bisection); an
  archive that keeps it is known to load;
- replaced entries keep their names; unchanged entries are copied byte for
  byte;
- recompute every `end` offset.

### 7.3 Which archive the game loads

A pointer at RVA **0x124F748** (2020: 0x125F978) points to the string
`data\ui.pak` (at RVA 0xFE3C34) that the game opens for its UI. The mod
redirects this pointer, at attach (01 §2.3), to its own NUL-terminated path
string in the same form (`data\` followed by a file name of the mod's
choosing). Required: write the patched archive next to the original under
that new name in `DATA\`; never overwrite `UI.PAK` (Steam file verification
restores it, and uninstalling must leave the game stock: removing the mod's
DLL makes the game open `UI.PAK` again).

The patched archive is rebuilt when it is missing, when the source
`UI.PAK` changed (record the source's size and a hash, or its size and
last-write time, with the patched file), or when the mod's patch changed
(record a version). Building takes well under a second; doing it at every
start is also acceptable.

### 7.4 Verifying the input archive

The patch is made against known movies. Before patching, the mod must check
that each entry it changes is exactly the one it was made for: the entry
exists, its size matches and its CRC-32 (the zlib / IEEE 802.3 polynomial,
over the entry's bytes without padding) matches. For the Steam 2015
`DATA\UI.PAK`:

| Entry | Size | CRC-32 |
|---|---|---|
| `DATA/UI/CHALLENGELEADERBOARDCOMPONENT.GFX` | 49770 | B1149B90 |
| `DATA/UI/CHALLENGEMAPOVERLAY.GFX` | 100580 | DF061961 |
| `DATA/UI/CRAFTINGSCREEN.GFX` | 76681 | 560CE12C |
| `DATA/UI/GAMEOVERMENU.GFX` | 136327 | 2A6BDA7C |
| `DATA/UI/GAMMASCREEN.GFX` | 66630 | 0BDF6322 |
| `DATA/UI/HUB.GFX` | 47269 | 95B354E0 |
| `DATA/UI/HUB_INVENTORY.GFX` | 71608 | 9EE59720 |
| `DATA/UI/HUB_JOURNAL.GFX` | 59383 | 35A99F8F |
| `DATA/UI/HUB_LOGS.GFX` | 54924 | 322E896B |
| `DATA/UI/HUB_MAP.GFX` | 107652 | 8AA8936B |
| `DATA/UI/HUB_MISSING.GFX` | 78654 | 1CCF1FC7 |
| `DATA/UI/LOADINGSCREEN.GFX` | 19585 | CDF572A3 |
| `DATA/UI/NEWFRONTENDMENU.GFX` | 220324 | 3BCF6656 |
| `DATA/UI/PAUSEMENU.GFX` | 134679 | 36912009 |
| `DATA/UI/PICKUPOVERLAY.GFX` | 65768 | EBCA7423 |
| `DATA/UI/POPUPOVERLAY.GFX` | 46794 | 7C3694A6 |
| `DATA/UI/RADIALMENU.GFX` | 163252 | 399AA2BC |
| `DATA/UI/REACTION_GAME.GFX` | 48255 | 62101EDD |
| `DATA/UI/TERMINALINTERACTION.GFX` | 60163 | CB7C7FE5 |
| `DATA/UI/TRCSYSTEMOVERLAY.GFX` | 30737 | AEFCF9C5 |
| `DATA/UI/WEAPONSTUFF.GFX` | 71688 | 09984F4E |

For reference, the whole file's SHA-256 is
`a8a0a68ac597b853133fd206f70b0acc7d5718949cf21a4a26aca1bd07524b43`; the check
is per entry so that a change elsewhere in the archive does not stop the
patch. The table lists every movie 7.7 may change; an implementation that
changes fewer checks only those it changes. The 2020 build's `UI.PAK` is
not yet characterised; its entries get their own rows once known.

The new glyph entries (7.8) must not already exist in the input.

### 7.5 When the player's UI.PAK differs

If `DATA\UI.PAK` is missing, unreadable, not PAK2, or any checked entry
differs (another game version, another mod's archive installed over it):

- VR still works: the stereo device, input, game patches and config file
  are unaffected;
- the mod builds no archive and does **not** redirect the UI archive
  pointer, so the game uses `UI.PAK` as it is;
- there is no options page and there are no VR glyphs: prompts show the
  game's own pad glyphs (04 §4.2 stays in stock-glyph mode), settings are
  changed in the config file (section 3), and skip intro is not applied
  (the stock loading-screen movie has no `SkipBootup`, so the push operand
  of section 6 is left alone);
- the mod logs which entry failed and tells the player once, after the game
  window exists (01 §2.2): VR runs, the VR options page is not available,
  and settings are in `mothervr.json`;
- a patched archive left from an earlier run is not used.

A patch step that fails on a movie that passed the checks (unexpected
structure inside it) is handled the same way: no archive, no redirect.
Never write a partly patched archive.

### 7.6 The game's UI scripting interface (game facts)

UI movies (`.GFX`) are uncompressed Scaleform files: magic `GFX`, version
byte 0x0B, u32 total length, then a SWF-equivalent tag stream. Their
ActionScript 3 code lives in DoABC tags. A patch edits the ABC bytecode
(adding classes, methods, fields and constants, or replacing method bodies)
and fixes the file's length field; the archive entry's size changes with
it.

The three game menus share one class, `frontend.GameMenu` (a
`MovieClipEx`), in `NEWFRONTENDMENU.GFX` (subclass `NewFrontendMenu`),
`PAUSEMENU.GFX` (subclass `PauseMenu`) and `GAMEOVERMENU.GFX` (subclass
`GameOverMenu`). Each movie has its own copy of the class; a change to the
shared behaviour must be made in all three.

**Building pages.** In its constructor the menu class creates its items as
fields and attaches them with `page.addItem(item)`. The item classes, in
package `frontend.menu`:

| Class | Constructor (stock) | Behaviour |
|---|---|---|
| `MenuItem` | `(label:String, onPress:Function = null)` | a plain entry; `onPress` runs when it is chosen |
| `MenuItemExpandable` | `(label:String, onPress:Function = null, items:Array = null)` | an entry that opens a sub-page; its `page` (`MenuPage`) takes `addItem`, `addItemAtIndex`, `removeItem`, `removeAllItems` |
| `MenuItemOptionCheckBox` | `(label:String, startTrue:Boolean = true, descriptions:Array = null, onIndexChange:Function = null)` | on/off; `selected` |
| `MenuItemOptionOptionStepper` | `(label:String, items:Array = null, labels:Array = null, onIndexChange:Function = null)` | steps through `items` showing `labels`; `selectedIndex`, `selectedItem`, `setItems(items, labels, descriptions)` |
| `MenuItemOptionSlider` | `(label:String, items:Array = null, onChange:Function = null, isSequence:Boolean = true, minimum:Number = 0, maximum:Number = 10, showValue:Boolean = false)` | with `isSequence` false, a continuous slider from `minimum` to `maximum` (`value`; `snapping`, `snapInterval`, default interval maximum / 10) whose handler is a value-change listener |
| `MenuItemOptionButton` | `(label:String, eventType:String = null, onEvent:Function = null)` | a button-style option |

Common item properties: `id` (`MenuItemId`), `name`, `description`,
`visible`, `enabled`, `data`. Labels are localisation keys (`AI_UI_MENU_...`)
or literal text: the stock "Trigger Debug Checkpoint" and "Select Level"
items use literal English, so a page can use literal labels without new
localisation entries. The Options page holds the `Game`, `Video` and
`Audio` expandable items; the VR entry is added after `Audio`.

**Item ids.** `frontend.enum.MenuItemId` holds fixed ids (for example
`DEBUG_CHECKPOINTS` = 12, `DEBUG_LOAD_LEVEL` = 15) and hands out automatic
ids from 300 up. The native side addresses items by id through the menu's
public functions `setItemVisible(id:int, visible:Boolean)`,
`setItemEnabled(id:int, enabled:Boolean)` and
`setItemDescription(id:int, text:String)`.

**Native to script.** The game calls public functions of the menu class by
name through the invoke methods of 4.3 (for example `setInvertXAxis(bool)`,
`setFieldOfView(int)`, `setStereoMode(int)`). A patch adds its own public
functions (4.3), which the same invoke methods reach by name.

**Script to native.** The class's helper `call(name, ...args)` calls
`ExternalInterface.call` with the movie's prefix in front of `name`
(`"fe_"` for the front end, `"go_"` for the game-over menu, `""` for the
pause menu). Stock names include `set_mouse_speed`, `set_field_of_view`,
`set_stereo_mode`, `save_engine_settings`, `on_page_changed`,
`on_item_selected`, `update_debug_checkpoints` and
`jump_to_debug_checkpoint`. All reach the handler of 4.1; the stock handler
ignores names it has not registered.

**Video options.** The Video page's items are fields of the menu class
(field of view, film grain, deep colour, motion blur, depth of field,
chromatic aberration, SSAO, stereo mode, ...); greying one out (5.2) is
setting its `enabled` to false.

**Debug items.** `GameMenu` creates "Trigger Debug Checkpoint"
(`MenuItemId.DEBUG_CHECKPOINTS`) under Load Game and hides it in its set-up;
`NewFrontendMenu` creates "Select Level" (`MenuItemId.DEBUG_LOAD_LEVEL`) on
the Campaign page.

**Prompts.** Each movie that shows button prompts has its own copy of the
class `ImageSubstitution`, which maps input ids (the names of 04 §2.3,
lower-cased) to glyph images loaded from the archive and replaces `$a$`,
`$b$`, `$x$`, `$y$` tokens in text. Its PC/Xbox path is where VR glyph ids
are registered (7.7).

### 7.7 Required changes per movie

| Movie(s) | Required change |
|---|---|
| `NEWFRONTENDMENU.GFX`, `PAUSEMENU.GFX`, `GAMEOVERMENU.GFX` (`GameMenu`) | add the VR options page (5.1, 5.2) after Audio on the Options page, sending the commands of 4.2, with the setter functions of 4.3 and `addMotherViewControl`; the debug-item visibility (5.4); the Video-options greying (5.2); the runtime entries labelled None / OpenXR / SteamVR |
| `NEWFRONTENDMENU.GFX` (`NewFrontendMenu`) | on entering the Campaign page, show Select Level only with dev. debug options on, and keep Trigger Debug Checkpoint hidden there |
| `LOADINGSCREEN.GFX` | add `SkipBootup` (section 6) |
| every movie with `ImageSubstitution` (CHALLENGELEADERBOARDCOMPONENT, CHALLENGEMAPOVERLAY, CRAFTINGSCREEN, GAMEOVERMENU, GAMMASCREEN, HUB, HUB_INVENTORY, HUB_JOURNAL, HUB_LOGS, HUB_MAP, HUB_MISSING, NEWFRONTENDMENU, PAUSEMENU, PICKUPOVERLAY, RADIALMENU, REACTION_GAME, TERMINALINTERACTION, TRCSYSTEMOVERLAY, WEAPONSTUFF) | on the PC/Xbox glyph path, register the VR glyph ids of 04 §4.1 (lower-case `vr_a` ... `vr_tp_lr`) with their images (7.8); and make the inline prompts that the stock class hard-wires to A / B / X / Y (the confirm and cancel prompts, and the `$a$`, `$b$`, `$x$`, `$y$` tokens) resolve to the control bound to the matching action (`use`, `back_close`, `reload_weapon`, `alt_2`) so that they show VR glyphs too |
| `CRAFTINGSCREEN.GFX` | always use the gamepad form of the craft button prompt (VR controllers arrive as the pad even when the keyboard flag is set) |
| HUD overlays (PICKUPOVERLAY, WEAPONSTUFF, POPUPOVERLAY, TRCSYSTEMOVERLAY) | layout changes so HUD elements stay readable and inside the view on the head-locked HUD quad; see 05 for the required placement |

`CREDITSSCREEN.GFX`, `MOVIEPLAYER.GFX` and `SUBTITLES.GFX` need no change.

### 7.8 Glyph images

Add one image per VR glyph id: `DATA/UI/GLYPH_VR_A.DDS`, `_B`, `_X`, `_Y`,
`_LSC`, `_RSC`, `_LG`, `_RG`, `_MENU`, `_TP`, `_TP_C`, `_TP_U`, `_TP_D`,
`_TP_L`, `_TP_R`, `_TP_LR` (and optionally `_TP_MENU`). Match the stock
glyphs' format: DDS, 32 × 32, DXT5, no mipmaps (1152 bytes each, like
`GLYPH_360_A.DDS`). The images are drawn by this project.

## 8. Open questions

- The game-over menu's object, vtable and Open method, needed to push
  settings into its copy of the page.
- Whether pixel-scale changes can be applied without a restart (depends on
  03's render-target handling).
- The full meaning of the quit flag at RVA 0x17DA920 (confirmed only as
  "the game exits when it is set").
- Whether the game's archive lookup needs sorted names (7.2).
