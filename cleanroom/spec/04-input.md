<!-- Provenance: game analysis of AI.exe 2015 (disassembly of the input manager, XInput pad device, action maps, device selection, keyboard/mouse devices, FollowBehavior camera); behaviour of MotherVR 0.8.1 observed through analysis and restated as requirements; user requirements for v1 fixes (thumbstick controllers, bounds safety) and for the runtime input layer (SteamVR Input with default bindings, OpenXR actions, the original's Touch layout for thumbstick controllers); public OpenVR and OpenXR input APIs; in-game testing of an earlier rebuild on SteamVR with Index controllers, under OpenVR and OpenXR. Revision: r2 -->

# 04 Input

How Alien: Isolation reads its input devices, and how VR controllers must
drive the game through them. Also snap turn, look gating and recentering
inputs.

Addresses are RVAs in the 2015 Steam build (image base 0x400000) unless
marked otherwise. The 2020 column comes from the per-build address table
and has not been re-verified instruction by instruction; confirm before use.

All game methods named "thiscall" take `this` in ECX, their arguments on the
stack, and pop them (`ret N` given). A replacement must match `ret N`, the
return register (AL for bool, EAX for pointers and ints, ST0 for float) and
must preserve EBX, ESI, EDI and EBP.

## 1. Summary

- The game has one input manager owning a keyboard, a mouse and an XInput
  gamepad device, and three action maps that bind game actions to device
  controls.
- VR controllers reach the game **through the gamepad device**. The mod
  rewrites every gamepad binding so that each action is bound to its own
  virtual control code, then answers the pad's query methods per action:
  the real pad's state for the action's original control, merged with the
  VR state for that action.
- Button prompts ask the pad for a control's name. The mod answers with VR
  glyph ids while VR controllers are the active input, and with the stock
  names while a real pad is in use.
- The controllers are read through the runtime's action system (SteamVR
  Input under OpenVR, OpenXR actions under OpenXR) with default bindings per
  controller type, so players can rebind them; a legacy path covers
  runtimes without SteamVR Input (5.8).
- Snap turn writes the first-person camera's yaw directly. Recentering is
  triggered by LB+RB on a real pad, Ctrl+Alt on the keyboard, or a request
  from the VR runtime.

## 2. Game facts

### 2.1 Input manager

A global pointer holds the input manager: RVA **0x1359B44** (2020:
0x1369CF4). Fields used:

| Offset | Type | Meaning |
|---|---|---|
| +0x40 | device*[16] | registered devices |
| +0x44 | int | device count (at most 16) |
| +0x48 | action map | map 0, in-game player actions (section 2.4) |
| +0x2998 | action map | map 1, menu / UI actions |
| +0x3A0C | action map | map 2, a third map (meaning unknown; no VR action is routed to it) |
| +0x4A70 | device* | mouse |
| +0x4A74 | device* | keyboard |
| +0x4A78 | device* | XInput pad |
| +0x4A7C | device* | the currently active pad device |

Each action map holds a three-entry "device by type" array at map+0x103C, so
the manager offsets **+0x1084 / +0x1088 / +0x108C** are map 0's pad /
keyboard / mouse pointers (likewise +0x39D4.. for map 1 and +0x4A48.. for
map 2).

Getters (thiscall, no args, plain `ret`, result in EAX):

| RVA 2015 | RVA 2020 | Returns |
|---|---|---|
| 0x576CD0 | 0x4C8A80 | mouse (+0x4A70) |
| 0x576CE0 | 0x4C8A90 | keyboard (+0x4A74) |
| 0x576CF0 | 0x4C8AA0 | active pad (+0x4A7C) |

### 2.2 Devices

Every device object starts with a vtable and shares these fields:

| Offset | Type | Meaning |
|---|---|---|
| +0x0C | int | device type: 0 = pad, 1 = keyboard, 2 = mouse |
| +0x10 | byte | enabled / present |
| +0x11 | byte | **is the active device** (set by device selection, section 2.5) |

Keyboard device (vtable RVA 0xFE7628, ctor RVA 0x60E9C0, 0x228 bytes):

- `bool KeyDown(int scancode)` at RVA **0x60EB20** (2020: 0x560FE0),
  thiscall, `ret 4`. Tests bit 7 of the key-state byte at
  `this + 0x14 + scancode` (DirectInput-style scan codes: 0x1D = left Ctrl,
  0x38 = left Alt).
- `bool AnyKey()` at RVA **0x60EE00** (2020: 0x5612D0), thiscall, plain
  `ret`. Returns the byte at +0x224 ("a key was pressed this frame").

Mouse device (vtable RVA 0xFE75E0, ctor RVA 0x60E520, 0xB0 bytes):

- `bool Active()` at RVA **0x9BBE70** (2020: 0x4C87E0), thiscall, plain
  `ret`. True when the dword at +0x54 is non-zero (the mouse moved or
  clicked).

### 2.3 The XInput pad device

The pad class has vtable RVA **0xFE7670** (2020: the table records
vtable+4 = 0xF22B34, so the vtable is at 0xF22B30). Constructor RVA
0x60EE10, object size 0x2B8, no RTTI. The vtable lives in read-only data;
patching a slot needs a page-protection change.

Object fields:

| Offset | Type | Meaning |
|---|---|---|
| +0x04 | float | trigger press threshold (0.5) |
| +0x0C | int | device type, 0 |
| +0x10 | byte | enabled |
| +0x11 | byte | is the active device |
| +0x18 | dword | non-zero when any button was pressed this frame |
| +0x1C | 16-bit mask | button state, bit n = control code n (n < 0x10) |
| +0x268, +0x26C | float | left stick X, Y |
| +0x270, +0x274 | float | right stick X, Y |
| +0x278, +0x27C | float | left, right trigger (0..1) |
| +0x280, +0x284 | float | touch zoom in, out |
| +0x289 | byte | controller connected |
| +0x290 | int | XInput user index |

Vtable slots used (offset from the vtable start):

| Slot | RVA 2015 | RVA 2020 | Method | ABI |
|---|---|---|---|---|
| +0x00 | 0x610FE0 | | scalar deleting destructor | thiscall, `ret 4` |
| +0x04 | **0x576730** | 0x4C84D0 | `bool IsDown(int code)`: codes below 0x10 test the button mask; 0x10 / 0x11 compare LT / RT against the threshold; anything else is false | thiscall, `ret 4`, AL |
| +0x08 | **0x576780** | 0x4C8520 | `float GetValue(int code)`: non-negative magnitude. Buttons 1 or 0; 0x10 / 0x11 / 0x16 / 0x17 the raw trigger or zoom value; sticks (0x12..0x15) return 0 | thiscall, `ret 4`, ST0 |
| +0x0C | **0x576820** | 0x4C85C0 | `float GetAxis(int code, int negate)`: signed value, multiplied by -1 when `negate` is non-zero. Buttons ±1 or 0, triggers, sticks, zoom | thiscall, `ret 8`, ST0 |
| +0x10 | 0x576830 | | `int NameToControl(const char*)` | thiscall, `ret 4` |
| +0x14 | **0x576840** | 0x4C85E0 | `const char* ControlToName(code)`: the control's name for prompts and the input-mapping screen. The code is passed in a 32-bit stack slot; only the low 16 bits are meaningful and they compare as **signed 16-bit** | thiscall, `ret 4`, EAX |
| +0x1C | 0x576850 | | `int ControlType(code)`: 0 button, 2 trigger (0x10, 0x11), 1 axis (0x12..0x17) | thiscall, `ret 4` |
| +0x20 | 0x60EE30 | | per-frame read: calls `XInputGetState(+0x290)` and sets +0x289 | |
| +0x28 | 0x60F280 | | scans XInput users 0..3 | |
| +0x2C | **0x576880** | 0x4C8620 | `bool HasActivity()`: true when +0x18 is non-zero or any of \|LX\|, \|LY\|, \|RX\|, \|RY\|, LT, RT exceeds 0.85 | thiscall, plain `ret`, AL |
| +0x40 | | | "active state changed" notification, called by device selection | |

Connected flag: in the method at RVA 0x60EE30 the instruction at RVA
**0x60EE6F** (2020: 0x56133F) is `setne cl` (`0F 95 C1`), following a
compare of the XInputGetState result with 0x48F (ERROR_DEVICE_NOT_CONNECTED);
CL is then stored to +0x289.

Pad control codes (the game's names, from `ControlToName`):

| Code | Name | Code | Name |
|---|---|---|---|
| 0x00 | A | 0x0C | LT_BUTTON |
| 0x01 | B | 0x0D | RT_BUTTON |
| 0x02 | X | 0x0E | LSTICK_BUTTON |
| 0x03 | Y | 0x0F | RSTICK_BUTTON |
| 0x04 | DPAD_UP | 0x10 | LT (analog) |
| 0x05 | DPAD_DOWN | 0x11 | RT (analog) |
| 0x06 | DPAD_LEFT | 0x12 | LSTICK_X |
| 0x07 | DPAD_RIGHT | 0x13 | LSTICK_Y |
| 0x08 | BACK | 0x14 | RSTICK_X |
| 0x09 | START | 0x15 | RSTICK_Y |
| 0x0A | LB | 0x16 | TOUCH_ZOOM_IN |
| 0x0B | RB | 0x17 | TOUCH_ZOOM_OUT |

A code of -1 (0xFFFF as 16 bits) means "no control". The game's own pad
code space ends at 0x17, so codes from 0x100 up are free for the mod.

The prompt pipeline hands these names to the UI movies, which look them up
case-insensitively as input ids ("A" becomes the UI's `a` glyph, "LB"
becomes `lb`). Any name returned here becomes a glyph lookup in the UI.

### 2.4 Action maps

Each action map is an object with this layout (base-class constructor RVA
**0x5974E0**; map 0's constructor RVA 0x597750, map 1's 0x5978B0):

| Offset | Content |
|---|---|
| +0x000 | vtable |
| +0x004 | digital actions: 41 (0x29) entries of 0x40 bytes |
| +0xA44 | analog actions: 16 (0x10) entries of 0x44 bytes |
| +0xE84 | composite list: entries of 0x24 bytes (capacity believed to be 12) |
| +0x1038 | int: composite entry count |
| +0x103C | device*[3]: pad, keyboard, mouse used by this map |
| +0x1054 | int: number of digital actions in use |
| +0x1058 | int: number of analog actions in use |

Action entry (digital and analog share the first 0x38 bytes):

| Offset | Content |
|---|---|
| +0x00 | up to 6 binding records of 8 bytes |
| +0x30 | pointer to the binding records (the constructor points it at +0x00 of the same entry) |
| +0x34 | int: binding count |
| +0x38.. | per-action state (digital: a byte initialised to 0x10 and a dword; analog: two floats and a flags byte at +0x40) |

Binding record (8 bytes):

| Offset | Content |
|---|---|
| +0x00 | 4 bytes, meaning unknown |
| +0x04 | int16: control code (device-specific; for the pad, the codes in 2.3) |
| +0x06 | byte: device type (0 pad, 1 keyboard, 2 mouse) |
| +0x07 | byte: flags (bit 0x40 is cleared by the mod on pad bindings it virtualises; meaning unknown) |

So binding *i* of an entry has its code at entry+0x04+8*i, its device at
entry+0x06+8*i and its flags at entry+0x07+8*i.

Composite entry (0x24 bytes): seven dword control codes at +0x00, a count at
+0x1C, a byte at +0x20, and a "keep" byte at +0x21. Composite entries whose
+0x21 byte is zero combine raw pad controls (believed to be stick vectors or
chords). They are the only path, other than per-action bindings, by which a
raw pad code reaches an action.

When the game evaluates an action it calls the method of
`device_by_type[binding.device]` (IsDown for digital actions, GetValue or
GetAxis for analog ones) with `binding.code`.

Action indices that matter for VR (inferred from which actions the original
mod drove for each in-game function; the AI.exe names are unknown):

Map 0 (player), digital:

| Index | Function |
|---|---|
| 0x08 | Sprint |
| 0x09 | Crouch |
| 0x0A | Aim |
| 0x0B, 0x0E | Fire / throw / melee |
| 0x0C, 0x0F | Interact |
| 0x0D | Radial menu |
| 0x10, 0x11 | Reload / activate |
| 0x1A | Previous weapon |
| 0x1B | Next weapon |
| 0x1C | Motion tracker |
| 0x1D | Flashlight |
| 0x26 | Map |
| 0x27 | (a second left-grip action; meaning unknown) |
| 0x28 | (a second right-grip action; meaning unknown) |

Map 0, analog: 0 = move X, 1 = move Y, 3 = turn (look X).

Map 1 (menus), digital: 0 and 0x10 = confirm, 1 = (driven with the radial /
centre input), 8 = pause, 9 = (left grip), 0x0A and 0x0F = back, 0x0B and
0x0E = (driven with the flashlight gesture). Analog: 0 = left trigger,
1 = right trigger, 2 / 3 = left stick X / Y, 4 / 5 = right stick X / Y.

The stock pad layout, as far as the prompts show it: A interact, B radial
menu, X reload, Y flashlight, BACK map, START pause, LB motion tracker,
RB aim, LT crouch, RT fire, left stick move (click sprint), right stick look.

### 2.5 Device selection

The input manager's device selection, RVA **0x57CBC0** (thiscall on the
manager), runs every frame. For each registered device of type 0 whose +0x11
is clear it calls `HasActivity()` (vtable +0x2C). When that returns true it
clears +0x11 on every device (calling their +0x40 notification), sets the
new device's +0x11, calls its +0x40, and stores it in +0x4A7C and in each
map's `device_by_type[type]`. The game refreshes its button prompts when
the active device changes.

### 2.6 Left-stick prompt check

`bool IsMoveOnLeftStick()` at RVA **0x57DCE0**, thiscall with ECX = map 0
(manager+0x48), no args, plain `ret`, result in AL. It walks the binding
records of analog action 0 (pointer at map+0xA74, count at map+0xA78), takes
the first record whose device byte is 0 (pad) and returns
`code == 0x12` (LSTICK_X); with no pad binding it returns true.

It is called only from the prompt-name builder at RVA 0x42CFF0 (2020:
0x43E140), by two `E8 rel32` calls at RVA **0x42D0A0** and **0x42D18C**
(2020: 0x43E1F0 and +0xEC from it), to choose `"lstick"` or `"rstick"` for
the prompt keys `look_controls`, `hack_left_stick_controls` and
`hack_right_stick_controls`.

### 2.7 First-person camera (for snap turn and look gating)

First-person look is the game's FollowBehavior camera. Its vtable is at RVA
**0xDC6600** (2020: 0xDDD6E8). Slot 0 is `OnActivate(arg)` (RVA 0x02C930)
and slot 1 is `OnDeactivate(arg)` (RVA 0x02BBF0), both thiscall `ret 4`.
Hooking these two slots is how the mod learns which camera instance is the
live first-person camera and whether it is active.

| Offset | Type | Meaning |
|---|---|---|
| +0x1FC | float | yaw, mirror copy |
| +0x204 | float | yaw. ±1.0 = ±180°; the game keeps it wrapped to [-1, 1] |
| +0x208 | float | pitch (clamped by the game) |
| +0x264 | byte | **look input enabled**: the game applies stick and mouse look deltas (slot 11, RVA 0x02D200) only while this is set |

The stock OnActivate sets +0x264 to 1; doc 05 removes that store so the mod
controls the byte.

The game's stereo object (global pointer RVA 0x1366A2C, see doc 02) has the
current stereo mode at +0x60: 1 means menus are on the virtual screen.

## 3. Required approach: virtualise the pad

### 3.1 Principle

Every action that has (or gets) a pad binding is bound to a **virtual pad
code** unique to that action:

| Map | Base | Digital action k | Analog action j |
|---|---|---|---|
| 0 (player) | 0x100 | 0x100 + k | 0x100 + numDigital + j |
| 1 (menus) | 0x200 | 0x200 + k | 0x200 + numDigital + j |
| 2 (other) | 0x400 | 0x400 + k | 0x400 + numDigital + j |

`numDigital` is that map's +0x1054 count at the time of the remap. With 41
digital actions, map 0's turn axis (analog 3) is 0x12C; the mod must
compute it from the count rather than assume 0x12C.

For each action the mod remembers the action's **original pad code** (or
"none"). When the game asks the pad about a virtual code, the mod looks up
the action, asks the real pad about the original code (through the stock
method), and merges in the VR state for that action. VR input is therefore
written **per game action**, not per physical button, and a real gamepad
keeps working alongside VR controllers.

### 3.2 Remap rules

For each of the three maps, for every digital action k < +0x1054 and every
analog action j < +0x1058:

1. Walk the action's bindings. Consider only records with device byte 0.
2. The first pad record: if its code is below 0x100, remember the code as
   the action's original code and replace it with the action's virtual
   code. If it is already virtual, keep it (the remap must be idempotent).
   For digital actions clear flag bit 0x40.
3. Any further pad records of the same action: set their code to -1, so a
   real pad reaches the action only through the virtual code.
4. If the action has no pad record: remember "none" and append a pad record
   with the virtual code (flags 0 for digital, 1 for analog), **only if the
   action has fewer than 6 bindings**. The original appended without a
   capacity check; with 6 bindings that writes over the entry's binding
   pointer.
5. Composite entries whose +0x21 byte is zero: set all their codes to -1.

The game can rebuild its bindings (loading, changing the controller
preset). A pad query that arrives with a raw pad code (0..0xFF, not -1)
means the maps hold un-virtualised bindings again; the mod must then remap
all three maps before answering. Remapping once as soon as the maps exist
is also allowed.

Per-action state the mod keeps must be sized from the game's array sizes
(41 digital, 16 analog per map) and every index must be range-checked
against the counts read at remap time.

### 3.3 Pad method replacements

The mod replaces the pad vtable slots +0x04, +0x08, +0x0C, +0x14 and +0x2C
(same ABI as the stock methods) and keeps the stock entry points to call.
For a virtual code c, let the map be chosen by c (0x100..0x1FF map 0,
0x200..0x3FF map 1, 0x400 and up map 2), i = c - base, and orig = the
action's original code (or none).

| Method | Required result |
|---|---|
| IsDown(c) | c < 0x100: remap if c is not -1, return false. Otherwise: stock IsDown(orig) (false if none) OR the action's VR button state, the latter **only if i is a digital action index** (i < numDigital). An analog index must not read the digital table. |
| GetValue(c) | c < 0x100: 0. Otherwise g = stock GetValue(orig) (0 if none); if i ≥ numDigital and i - numDigital < numAnalog, v = the VR value of that analog action, else v = 0; return max(g, v). |
| GetAxis(c, negate) | c < 0x100: 0. Otherwise g = stock GetAxis(orig, negate); v as for GetValue; return whichever of g, v has the larger magnitude. The VR value is already signed in game convention and is not negated (the original behaved so; keep it unless a binding with `negate` set is found to need otherwise). |
| ControlToName(c) | Compare c as signed 16-bit. c < 0x100: return null. In stock-glyph mode return stock ControlToName(orig). In VR-glyph mode return "VR_MENU" when orig is none, else the VR glyph name for orig (section 4); orig outside 0..0x17 must yield a safe name, never an exception. |
| HasActivity() | stock HasActivity() OR "VR controllers were active since the last call"; consume the VR flag. Also drive the prompt switch (section 4.2). |

The original had out-of-range reads in IsDown (analog index into the
digital table), in GetValue and GetAxis (a digital index wrapped modulo 256
into the analog table) and threw out of ControlToName for codes past the
name table. These must not happen.

### 3.4 The pad must stay connected

With no physical pad, the game would treat the pad as absent and stop
querying it. The connected flag (+0x289) must be forced to 1: at RVA
0x60EE6F replace `setne cl` with `mov cl, 1` + `nop` (`B1 01 90`). The
patch site is listed with the other game patches in doc 05.

### 3.5 When VR is off

When VR is disabled or failed to start, the pad must behave exactly as
stock: do not virtualise bindings and do not change prompts.

## 4. Button prompts

### 4.1 Glyph ids

The patched UI (doc 06) understands these VR glyph ids, each backed by an
image named `GLYPH_VR_<ID>.DDS`:

| Id | Meaning |
|---|---|
| VR_A, VR_B, VR_X, VR_Y | face buttons |
| VR_LSC, VR_RSC | left / right thumbstick click |
| VR_LG, VR_RG | left / right grip |
| VR_MENU | menu button (also used for "unbound") |
| VR_TP | touchpad |
| VR_TP_C, VR_TP_U, VR_TP_D, VR_TP_L, VR_TP_R | touchpad centre / up / down / left / right press |
| VR_TP_LR | touchpad left or right |

Required mapping from the game's original pad code to the name returned in
VR-glyph mode, per controller scheme (section 5). Codes not listed return
the game's own pad name (so LT shows the trigger glyph, and so on):

| Pad code | Touchpad scheme | Thumbstick + face-button scheme | WMR scheme |
|---|---|---|---|
| 0x00 A (interact) | VR_TP_D | VR_A | VR_TP_D |
| 0x01 B (radial) | VR_TP_C | VR_RSC | VR_RSC |
| 0x02 X (reload) | VR_TP_U | VR_B | VR_TP_U |
| 0x03 Y (flashlight) | game name "Y" (no single control; gesture) | VR_Y | VR_TP |
| 0x08 BACK (map) | VR_MENU | VR_X | VR_MENU |
| 0x09 START (pause) | VR_MENU | VR_MENU | VR_MENU |
| 0x0A LB (tracker) | VR_LG | VR_LG | VR_LG |
| 0x0B RB (aim) | VR_RG | VR_RG | VR_RG |

The glyph set follows the scheme of the current controllers (5.5). Until a
controller has been identified, the touchpad set is used under OpenVR (the
original's only set) and the thumbstick set under OpenXR. On Index
controllers the thumbstick set shows X / Y for the left A / B, which take
those roles (5.3). With the default bindings the glyphs name the right
controls; a player who rebinds in the runtime sees the default glyphs.

### 4.2 Switching prompts with the input source

The mod keeps a "VR glyph mode" flag, initially on. Inside HasActivity, when
there was any activity this call (pad or VR) and the source differs from
the current mode (VR activity present ≠ VR glyph mode), set the mode to the
new source and make the game re-run its "active device changed" path so
prompts re-query `ControlToName`. One way that follows from the game's
device selection (2.5): on the flip, clear the pad's +0x11 and set the
keyboard's +0x11 (map 0's `device_by_type`, manager +0x1084 / +0x1088); on
the next call restore pad = 1, keyboard = 0. The pad then becomes active
again on the next selection pass and fires its notification.

Because VR activity is ORed into HasActivity, the pad becomes the active
device whenever VR controllers are used, so the game shows pad-style
prompts (with VR glyphs) rather than keyboard prompts.

When the patched UI archive is not in use (06 §7.5), the movies know no VR
glyph ids: the mod then stays in stock-glyph mode permanently, so prompts
show the game's own pad glyphs.

### 4.3 Left-stick prompt check

Once bindings are virtualised, the stock check (2.6) sees a virtual code
and always answers false. Both call sites must instead get: "map 0 analog
action 0's original pad code is LSTICK_X (0x12)", or true when it has none.
Redirect the two calls (same ABI: thiscall, no args, AL) to a function that
answers that.

## 5. VR controller mapping

### 5.1 Common rules

- Each frame, before the game reads input, the mod reads both hand
  controllers and writes VR state for map 0 and map 1 actions. Map 2 gets
  no VR state.
- A digital action is "active" when its value is true; an analog value
  when its magnitude exceeds 0.85. Any active write sets the "VR controllers
  were active" flag consumed by HasActivity.
- Thresholds: trigger as a button > 0.5; analog grip as a button > 0.5;
  touchpad centre zone \|x\| < 0.33 and \|y\| < 0.33; "near head" = hand
  position within 0.2 m of the head position (both in the same tracking
  space); weapon-switch stick deflection > 0.5.
- Analog outputs must stay within [-1, 1] (the original scaled touchpad
  movement by 1.1 without clamping; clamp).
- When the runtime reports lost input focus, a missing controller, or the
  session is not running, **all VR action state must be cleared** (the
  original's Oculus path left stale state latched on focus loss).
- No haptics in v1.

Definitions used below (R = right hand, L = left hand; pad = touchpad or
thumbstick axes; "click" = pad/stick press):

- horizontal = \|R.x\| > \|R.y\|
- centre = R pad inside the centre zone
- padDown = R click and not horizontal and not centre and R.y < 0
- padUp = R click and not horizontal and not centre and R.y > 0
- padCentre = R click and centre

### 5.2 Touchpad scheme (Vive wand; v1 behaviour of the original)

Map 0:

| Action | Input |
|---|---|
| Move X / Y | L pad x / y while the L pad is touched, else 0 |
| Sprint | L pad click |
| Crouch | L trigger > 0.5 |
| Aim | R grip (toggle option, 5.6) |
| Fire / throw / melee (both indices) | R trigger > 0.5 |
| Interact (both) | padDown |
| Radial menu | padCentre |
| Reload / activate (both) | padUp |
| Motion tracker | L grip and not padCentre (toggle option, 5.6) |
| Flashlight | padDown while the right hand is near the head |
| Map | L application-menu button |
| Grip actions 0x27 / 0x28 | L grip / R grip, raw |
| Turn | see below |

Turn: when horizontal and not centre, R.x × 0.85, else 0. With snap
rotation on, the value is only produced while the R pad is clicked (so a
resting thumb does not turn); with snap off, touch is enough (smooth turn
through the game's own look).

Map 1:

| Action | Input |
|---|---|
| Confirm (both) | padDown or L pad click |
| Index 1 | padCentre |
| Pause | R application-menu button |
| Index 9 | L grip, raw |
| Back (both) | padUp |
| Indices 0x0B / 0x0E | the flashlight gesture |
| Analog 0 / 1 | L / R trigger value |
| Analog 2 / 3 | L pad x / y while touched |
| Analog 4 / 5 | R pad x / y while touched |

### 5.3 Thumbstick and face-button scheme (Oculus Touch; also Index, Quest, Pico)

The original used this only with the Oculus runtime. v1 must use it for
every controller with a thumbstick and A/B/X/Y-style buttons, under any
runtime.

Map 0:

| Action | Input |
|---|---|
| Move X / Y | L stick |
| Sprint | L stick click |
| Crouch | L trigger > 0.5 |
| Aim | R grip (toggle option) |
| Fire (both) | R trigger > 0.5 |
| Interact (both) | A (right lower face button) |
| Radial menu | R stick click |
| Reload / activate (both) | B (right upper face button) |
| Previous weapon | R stick: not horizontal and y < -0.5 |
| Next weapon | R stick: not horizontal and y > 0.5 |
| Motion tracker | L grip and not R stick click (toggle option) |
| Flashlight | Y, or A while the right hand is near the head |
| Map | X (left lower face button) |
| Grip actions 0x27 / 0x28 | L grip / R grip, raw |
| Turn | R stick x when horizontal, else 0 |

Map 1:

| Action | Input |
|---|---|
| Confirm (both) | A |
| Index 1 | R stick click |
| Pause | left menu button (the Touch menu button); on Index, which has no menu button, the left trackpad press |
| Index 9 | L grip, raw |
| Back (both) | B |
| Indices 0x0B / 0x0E | the flashlight gesture |
| Analog 0 / 1 | L / R trigger |
| Analog 2 / 3 | L stick |
| Analog 4 / 5 | R stick |

On Index controllers the left A/B act as X/Y; the trackpads are unused in
this scheme. Grip is the controller's grip button or squeeze > 0.5.

### 5.4 Thumbstick and touchpad scheme (Windows Mixed Reality)

WMR controllers have a thumbstick, a touchpad, trigger, grip and menu, but
no A/B/X/Y. Required mapping: movement, sprint (L stick click), turn and
weapon switching on the sticks as in 5.3; radial menu on the right stick
click as in 5.3; interact and reload on the right touchpad as padDown /
padUp of 5.2 (using the touchpad axes, not the stick); flashlight on the
left touchpad click, and also interact near the head; map on the left menu
button; pause on the right menu button; grips and triggers as in 5.3. Not
yet verified on WMR hardware.

### 5.5 Choosing the scheme

The scheme follows the right-hand controller the runtime reports:

| Runtime | Touchpad scheme (5.2) | WMR scheme (5.4) | Thumbstick scheme (5.3) |
|---|---|---|---|
| OpenVR, SteamVR Input | Turn engage is bound and active (5.8.2); glyphs from `Prop_ControllerType_String` `vive_controller` | `holographic_controller` | anything else, e.g. `knuckles`, `oculus_touch` |
| OpenVR, legacy (5.8.3) | always | - | - |
| OpenXR | `/interaction_profiles/htc/vive_controller` | `/interaction_profiles/microsoft/motion_controller` | any other profile, e.g. `valve/index_controller`, `oculus/touch_controller`, `khr/simple_controller` |

The glyph table (4.1) and the View Controls rows (5.7) follow the scheme.
Re-evaluate when the controllers change (OpenVR: a new device index for a
hand role; OpenXR: the interaction-profile-changed event).

### 5.6 Option effects

- **Toggle grips** ("Toggle Aim/Motion Sensor"): when on, Aim flips on each
  rising edge of the right grip and Motion tracker flips on each rising edge
  of the left grip; when off they follow the grip while held.
- **Toggle crouch**, **relative walking**, **strafing**: stored and shown to
  the UI only; they change no input behaviour in v1 (the original had no
  native effect for them) and the menu hides them (doc 06).
- **Snap rotation** and **segments**: section 6.

### 5.7 View Controls rows

The options menu has a read-only "View Controls" page fed by the mod with
(action, control) rows describing the active scheme. Rows for the touchpad
scheme: Move, Sprint, Crouch, Turn, Aim, Fire / Throw / Melee, Motion
Tracker, Interact, Radial Menu, Reload / Activate, Flashlight (interact
near the head), Map Menu, Pause Menu, each with its control from 5.2. The
thumbstick scheme adds Next / Previous Weapon and names the sticks and
buttons from 5.3. Delivery is described in doc 06.

### 5.8 Runtime input layer

Sections 5.2 to 5.4 say which game action each physical control drives.
This section says how the controls are read from the runtime. The mod reads
**semantic inputs** (5.8.1) from the runtime's action system, with default
bindings that reproduce 5.2 to 5.4, and turns them into the game action
state of 5.8.5. Because the runtime does the binding, players can rebind in
the runtime's own binding UI and controllers the mod does not know still
work.

Status: OpenVR (SteamVR Input) and OpenXR verified in game on SteamVR with
Valve Index controllers. The Touch, WMR and Vive defaults follow the same
rules and are not yet verified on hardware.

#### 5.8.1 Semantic inputs

| Input | Type | Shown to the player as | Drives (5.8.5) |
|---|---|---|---|
| Move | 2D vector | Move | move X / Y |
| Turn | 2D vector | Turn / weapon select | turn (x), previous / next weapon (y, thumbstick schemes) |
| Turn engage | boolean | Turn (touchpad click) | touchpad scheme only: turning needs a click while snap rotation is on |
| Sprint | boolean | Sprint | sprint |
| Crouch | 1D (0..1) | Crouch | crouch (> 0.5) |
| Fire | 1D (0..1) | Fire / throw / melee | fire (> 0.5) |
| Aim | boolean | Aim | aim (toggle option) |
| Tracker | boolean | Motion tracker | motion tracker (toggle option) |
| Interact | boolean | Interact (near head: flashlight) | interact; flashlight when the right hand is near the head |
| Reload | boolean | Reload / activate / back | reload; back in menus |
| Radial | boolean | Radial menu | radial menu |
| Flashlight | boolean | Flashlight | flashlight |
| Map | boolean | Map | map |
| Pause | boolean | Pause | pause |

The identifiers used in the runtime's files are the implementation's
choice; the shown names are what the player sees in the runtime's binding
UI.

#### 5.8.2 OpenVR: SteamVR Input

Required when the runtime is OpenVR and the `IVRInput` interface is
available:

1. After `VR_Init`, write the **action manifest** and one **default binding
   file per controller type** into a folder in the game directory (next to
   `AI.exe`; the folder name is the mod's choice). Write them at every start,
   so an updated mod updates its defaults. SteamVR keeps a player's own
   bindings separately, so rewriting the defaults does not lose them.
2. The manifest declares one action set (usage `single`), the 14 actions of
   5.8.1 with their types (`boolean`, `vector1`, `vector2`), English
   localisation of the set ("Alien: Isolation") and of each action (the
   shown names above), and `default_bindings` naming the binding file for
   each controller type: `knuckles`, `oculus_touch`,
   `holographic_controller`, `vive_controller`.
3. Call `IVRInput::SetActionManifestPath` with the manifest's absolute path,
   then get the set and action handles (`GetActionSetHandle`,
   `GetActionHandle`).
4. Each frame (device slot 5, before writing game state): one
   `UpdateActionState` for the set, then `GetDigitalActionData` /
   `GetAnalogActionData` for each action, from any device. An action whose
   data is not `bActive` counts as false / 0.
5. The scheme is the touchpad scheme exactly when Turn engage is active
   (bound on the current controller); otherwise a thumbstick scheme.

Default bindings (`L` / `R` = left / right hand; SteamVR input source paths
and modes):

| Input | `knuckles` (Index) | `oculus_touch` | `holographic_controller` (WMR) | `vive_controller` (wands) |
|---|---|---|---|---|
| Move | L thumbstick, joystick mode, position | L joystick, joystick, position | L joystick, joystick, position | L trackpad, trackpad mode, position |
| Sprint | L thumbstick click | L joystick click | L joystick click | L trackpad click (same source) |
| Turn | R thumbstick position | R joystick position | R joystick position | R trackpad, trackpad mode, position |
| Turn engage | - | - | - | R trackpad click (same source) |
| Radial | R thumbstick click | R joystick click | R joystick click | R trackpad, dpad mode (click), center |
| Interact | R `a` click | R `a` click | R trackpad, dpad mode (click), south | R trackpad dpad, south |
| Reload | R `b` click | R `b` click | R trackpad dpad, north | R trackpad dpad, north |
| Map | L `a` click | L `x` click | L application_menu click | L application_menu click |
| Flashlight | L `b` click | L `y` click | L trackpad, button mode, click | - (interact near the head) |
| Crouch | L trigger, trigger mode, pull | L trigger pull | L trigger pull | L trigger pull |
| Fire | R trigger pull | R trigger pull | R trigger pull | R trigger pull |
| Tracker | L grip, grab mode, grab | L grip, button mode, click | L grip, button mode, click | L grip, button mode, click |
| Aim | R grip, grab mode, grab | R grip, button, click | R grip, button, click | R grip, button, click |
| Pause | L trackpad, button mode, click (Index has no menu button) | L application_menu click (the Touch menu button) | R application_menu click | R application_menu click |

These reproduce 5.3 (Index, Touch), 5.4 (WMR) and 5.2 (wands). On Index the
left A / B take the roles of X / Y.

#### 5.8.3 OpenVR: legacy fallback

When `IVRInput` is not available (an old runtime) or
`SetActionManifestPath` fails, read the controllers the way the original
did and log that the legacy path is used:

- the hands are `IVRSystem::GetTrackedDeviceIndexForControllerRole` for
  `TrackedControllerRole_LeftHand` / `_RightHand`; their state comes from
  `IVRSystem::GetControllerState`;
- every controller is treated as a Vive wand: the **touchpad scheme** of
  5.2. Axis 0 is the pad, axis 1 the trigger; the touchpad button
  (`k_EButton_SteamVR_Touchpad`) is the pad click and its touched bit the
  pad touch; `k_EButton_Grip` is the grip; `k_EButton_ApplicationMenu` is
  the menu button;
- Move is the left pad while it is touched; Turn the right pad while it is
  touched; Turn engage the right pad click; Sprint the left pad click;
  Interact / Reload / Radial the right pad click regions (padDown, padUp,
  padCentre of 5.1); Map the left menu, Pause the right menu; Crouch / Fire
  the trigger values; Tracker / Aim the grips.

The glyphs are then the touchpad set (4.1).

#### 5.8.4 OpenXR: actions and suggested bindings

Required when the runtime is OpenXR (09), after the session exists:

- one action set; per-hand actions with the subaction paths
  `/user/hand/left` and `/user/hand/right`: grip pose (pose), trigger
  (float), squeeze (float), stick (vector2), stick click (boolean),
  primary (boolean), secondary (boolean), menu (boolean), pad (vector2),
  pad click (boolean), pad touch (boolean). A haptic output may be declared
  for v2; v1 does not use it;
- suggested bindings for each profile below, then
  `xrAttachSessionActionSets`; a grip-pose action space per hand;
- each frame, after `xrWaitFrame` / `xrBeginFrame` (09): `xrSyncActions`
  for the set, then the states per hand. An inactive state counts as
  false / 0; `XR_SESSION_NOT_FOCUSED` is a success code under which every
  state reads inactive, which clears all VR state as 5.1 requires;
- on `XrEventDataInteractionProfileChanged`, read the right hand's profile
  with `xrGetCurrentInteractionProfile`: `/interaction_profiles/htc/vive_controller`
  selects the touchpad scheme, `/interaction_profiles/microsoft/motion_controller`
  the WMR scheme, any other profile the thumbstick scheme; the glyph set
  (4.1) follows. Until a profile is bound there is no VR input.

Suggested bindings (paths under `/user/hand/<hand>/`; `L` left, `R` right,
both hands when not marked):

| Action | `oculus/touch_controller` | `valve/index_controller` | `htc/vive_controller` | `microsoft/motion_controller` | `khr/simple_controller` |
|---|---|---|---|---|---|
| grip pose | `input/grip/pose` | same | same | same | same |
| trigger | `input/trigger/value` | same | same | same | `input/select/click` |
| squeeze | `input/squeeze/value` | `input/squeeze/value` | `input/squeeze/click` | `input/squeeze/click` | - |
| stick | `input/thumbstick` | `input/thumbstick` | - | `input/thumbstick` | - |
| stick click | `input/thumbstick/click` | `input/thumbstick/click` | - | `input/thumbstick/click` | - |
| primary | L `input/x/click`, R `input/a/click` | `input/a/click` | - | - | - |
| secondary | L `input/y/click`, R `input/b/click` | `input/b/click` | - | - | - |
| menu | L `input/menu/click` | L `input/trackpad/force` | `input/menu/click` | `input/menu/click` | `input/menu/click` |
| pad | - | - | `input/trackpad` | `input/trackpad` | - |
| pad click | - | - | `input/trackpad/click` | `input/trackpad/click` | - |
| pad touch | - | - | `input/trackpad/touch` | `input/trackpad/touch` | - |

Per-hand states to semantic inputs (grip = squeeze > 0.5):

| Input | Thumbstick scheme | Touchpad scheme (wands) | WMR scheme |
|---|---|---|---|
| Move | L stick | L pad while L pad touched | L stick |
| Sprint | L stick click | L pad click | L stick click |
| Turn | R stick | R pad while R pad touched | R stick |
| Turn engage | - | R pad click | - |
| Radial | R stick click | R pad click in the centre zone | R stick click |
| Interact | R primary | R pad click, padDown region (5.1) | R pad click, padDown region |
| Reload | R secondary | R pad click, padUp region | R pad click, padUp region |
| Map | L primary | L menu | L menu |
| Flashlight | L secondary | - | L pad click |
| Pause | L menu or R menu | R menu | R menu |
| Crouch / Fire | L / R trigger | L / R trigger | L / R trigger |
| Tracker / Aim | L / R grip | L / R grip | L / R grip |

"Near the head" (5.1) is the right hand's grip pose within 0.2 m of the
head (the VIEW space origin), both located in the same base space at the
frame's predicted display time.

#### 5.8.5 Semantic inputs to game actions

Whatever the runtime, the game action state of 5.2 to 5.4 is written from
the semantic inputs. With `turn` = Turn, `horizontal` = |turn.x| > |turn.y|
and `centre` = |turn.x| < 0.33 and |turn.y| < 0.33:

Map 0:

| Game action | Value |
|---|---|
| Move X / Y | Move; touchpad scheme × 1.1, clamped to [-1, 1] |
| Sprint (0x08) | Sprint |
| Crouch (0x09) | Crouch > 0.5 |
| Aim (0x0A) | Aim, with the toggle option (5.6) |
| Fire (0x0B, 0x0E) | Fire > 0.5 |
| Interact (0x0C, 0x0F) | Interact |
| Radial (0x0D) | Radial |
| Reload (0x10, 0x11) | Reload |
| Previous / next weapon (0x1A / 0x1B) | thumbstick and WMR schemes: not horizontal and turn.y < -0.5 / > 0.5 |
| Motion tracker (0x1C) | Tracker and not Radial, with the toggle option |
| Flashlight (0x1D) | Flashlight, or Interact while the right hand is near the head |
| Map (0x26) | Map |
| 0x27 / 0x28 | Tracker / Aim, raw |
| Turn (analog 3) | horizontal and not centre: turn.x (× 0.85 in the touchpad scheme), else 0; in the touchpad scheme with snap rotation on, 0 unless Turn engage is held |

Map 1:

| Game action | Value |
|---|---|
| Confirm (0, 0x10) | Interact, or (touchpad scheme) Sprint, the left pad click |
| Index 1 | Radial |
| Pause (8) | Pause |
| Index 9 | Tracker, raw |
| Back (0x0A, 0x0F) | Reload |
| 0x0B, 0x0E | the flashlight value of map 0 |
| Analog 0 / 1 | Crouch / Fire values |
| Analog 2 / 3 | Move |
| Analog 4 / 5 | Turn x / y |

The activity flag and the clearing rules of 5.1 apply to these values.

## 6. Snap turn

Inputs: the turn value is the pad's GetAxis answer for map 0's turn
virtual code (0x100 + numDigital + 3), queried with the replaced method so
it is the larger-magnitude of the real right stick and the VR turn.

Each frame, when all of these hold:

- VR is running and the headset is tracked,
- snap rotation is on,
- the first-person camera is active (2.7, tracked through its activate and
  deactivate slots),
- the stereo mode is not 1 (no menu on the virtual screen),
- \|turn\| > 0.5,
- the snap latch is clear,

then add ±step to the camera yaw (+0x204) with the sign of `turn`, wrap the
result into [-1, 1] (add or subtract 2), store it in both +0x204 and
+0x1FC, set the latch and note the time.

Step size: yaw is in half-turn units, so **step = 2 / segments**.
`segments` must be one of 24, 12, 6, 4, 3, 2 (15°, 30°, 60°, 90°, 120°,
180°; doc 06 normalises other values). The original computed
1 / (segments / 2) in integer arithmetic, which gives 180° for 3 segments
and divides by zero below 2; do not reproduce that.

The latch is released when \|turn\| ≤ 0.5, or, while the stick is held and
the camera is active, once more than 300 ms have passed since the last
step (auto-repeat).

## 7. Game look enabling

The mod owns FollowBehavior +0x264 (look input enabled):

- No tracked headset (VR off, failed, or not yet tracking): set it to 1 so
  the game's normal look works.
- Otherwise set it to `(not snap rotation) or keyboardMouseMode`.

`keyboardMouseMode` is set when the keyboard reports AnyKey or the mouse
reports Active (2.2) and cleared when the pad's HasActivity (including VR
activity) returns true. With snap off, the game's own smooth look runs from
the right stick or the VR turn value; with snap on, look only comes from
the head (and the mouse while keyboard/mouse is in use).

## 8. Recentering inputs

Recentering (resetting the seated yaw and position, doc 03) is triggered by
any of:

- LB and RB held together on a real pad, read with the **stock** IsDown and
  raw codes 0x0A / 0x0B (the replaced method ignores raw codes);
- left Ctrl and left Alt held together (KeyDown 0x1D and 0x38);
- a recenter request from the VR runtime (OpenXR: a pending change of the
  LOCAL reference space, 09; OpenVR has no such request in v1).

It fires once per press of the combination: after a recenter, the next one
requires the combination to be released first. The game's own LB+RB
recenter in its stereo update and the pause menu's "reset view" are routed
to the same recenter (doc 05). After a recenter the virtual screen is
re-placed in front of the player (doc 03).

## 9. Original problems and required behaviour

| Original behaviour | Required |
|---|---|
| IsDown read the digital table with an analog index | range-check, use the game value only |
| GetValue / GetAxis wrapped a digital index into the analog table | range-check, VR value 0 |
| ControlToName threw for original codes past the name table | return a safe name |
| Remap appended a 7th binding without a capacity check | append only below 6 |
| Turn axis hard-coded as 0x12C | derive from the map's digital count |
| Snap step used integer division (3 segments = 180°, < 2 divides by zero) | step = 2 / segments over the normalised set |
| Touchpad movement scaled by 1.1 without clamp | clamp to [-1, 1] |
| Oculus input stayed latched on focus loss | clear all VR state |
| Under OpenVR every controller was treated as a Vive wand | per-family schemes (5.3, 5.4) through SteamVR Input default bindings; the wand treatment only as the legacy fallback (5.8.3) |

## 10. Open questions

- AI.exe names of the three maps and of map 0 actions 0x27 / 0x28 and map 1
  actions 1, 9, 0x0B, 0x0E.
- Meaning of the composite list and its +0x21 flag.
- Whether the binding records' first dword or flag bit 0x40 matter to the
  game after virtualisation.
- Whether any binding is evaluated through GetAxis with `negate` set; if so
  the VR value may need the same negation.
