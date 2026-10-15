<!-- Provenance: game analysis of AI.exe 2015 (disassembly of the input manager, XInput pad device, action maps, device selection, keyboard/mouse devices, FollowBehavior camera, prompt-name builder); 2020 values were taken from the earlier mod's per-build address table (and, for inline sites, its installer) and have not been verified against a 2020 `AI.exe`; the earlier mod's observable controls (which physical control drives which game function, which map actions respond to VR controls) restated as requirements, without its names, wording or code; user requirements for v1 fixes (thumbstick controllers, bounds safety, input cleared without focus, controls handed to the game right after the frame's poses arrive) and for the runtime input layer (SteamVR Input with default bindings, OpenXR actions, the earlier mod's Touch layout for thumbstick controllers, Index pause on the left trackpad); public OpenVR and OpenXR input APIs; in-game observation by the spec side on the 2015 build with SteamVR and Valve Index controllers, under OpenVR (SteamVR Input) and OpenXR (action-map counts, focus loss, profile binding). r4: virtual codes, remap, pad methods, glyphs, prompt switching and the controls help page restated as required results; invented names removed; constants given reasons. r5 (spec r6): a tester's report on SteamVR with a Steam Frame headset (input while another window has the desktop focus); project decision of spec r6, with its reason inline (the touchpad scheme's scaled move limited to length 1 as a vector). r6 (spec r7, second clean-room review): AI.exe 2015 analysis by the spec side of the map's digital evaluation (binding flag bit 0x40 marks a composite reference; codes reach the pad methods zero-extended), of the stock ControlToName for codes above 0x17, and of the FollowBehavior fields (the yaw setter, the character's position and its head bone's position); the five pad slots as one patch, checked against their exact functions; the controls help as content, not a page, and its crossing between threads; prompts while the options archive is not in use; VR activity seen by every reader in a game frame; the remap's flag writes given their game reason; thresholds and the initial glyph mode given reasons; keyboard and mouse each read whenever their own device exists; the recenter chord re-armed by release alone; controller identification and the OpenXR actions stated as results; the structure offsets marked as 2015 facts. r7 (spec r7 cross-check): AI.exe 2015 analysis by the spec side (FollowBehavior +0x1C0, +0x1CC and +0x248 are written by vtable slot 6 and the update, slot 8, not by OnActivate; the `Head1` transform is taken with a mode argument of 7); a repeated CreateDevice is not expected (02 §6.1); a set is chosen at stereo device creation for the controls help; the toggled grip states kept when an OpenXR session stops; only `EXITING` sets the game's exit flag (09 §3). Revision: r7 -->

# 04 Input

How Alien: Isolation reads its input devices, and how VR controllers must
drive the game through them. Also button prompts, snap turn, look gating,
recentering inputs, and the controller poses the mod keeps for later use.

Addresses are RVAs (image base 0x400000), written **2015 / 2020**. The 2015
values were checked in the 2015 `AI.exe`. 2020 values were taken from the
earlier mod's per-build address table (and, for inline sites, its
installer) and have not been verified against a 2020 `AI.exe`; "2020: ?"
means unknown, and a feature that needs an unknown
address stays off on that build (01). Bytes given inline are the 2015
originals; the implementation checks them before writing (01, preflight) and
patches nothing if any check fails.

The structure offsets of section 2 (input manager, devices, pad object,
action maps, binding records, FollowBehavior fields) were read from the
2015 build. On the 2020 build they are assumed equal and are not verified:
before enabling the pad patches there, an implementation confirms the
action map layout (the counts at map +0x1054 and +0x1058 within the
capacities of 2.4, and the counts it logs plausible) and keeps the pad
patches off when that check fails.

In this document a bare "§11" means this document's open questions
(section 11); `11` or "11 §n" with a section number means
`11-diagnostics.md`.

All game methods named "thiscall" take `this` in ECX and their arguments on
the stack, and pop them (`ret N` given). A replacement must match `ret N`,
the return register (AL for bool, EAX for pointers and ints, ST0 for float)
and must preserve EBX, ESI, EDI and EBP.

Confidence: **[sure]** the game code or an in-game observation shows it;
**[likely]** strong inference; **[guess]** plausible, must be confirmed.

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
  controller type, so players can rebind them; a legacy path covers OpenVR
  without SteamVR Input (5.8).
- The VR state for a frame is handed to the game right after the runtime
  delivers the poses for the next frame (5.9).
- Snap turn writes the first-person camera's yaw directly. Recentering is
  triggered by LB+RB on a real pad, Ctrl+Alt on the keyboard, or
  automatically (03 §2.2).

## 2. Game facts

### 2.1 Input manager

A global pointer holds the input manager: **0x1359B44 / 0x1369CF4**. Fields
used [sure]:

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
| +0x4A7C | device* | the currently active pad device (may be null) |

The pointer is null until the game has created the manager; every use must
test it. Each action map holds a three-entry "device by type" array at
map+0x103C, so the manager offsets **+0x1084 / +0x1088 / +0x108C** are map
0's pad / keyboard / mouse pointers (likewise +0x39D4.. for map 1 and
+0x4A48.. for map 2).

Getters (thiscall, no args, plain `ret`, result in EAX). The mod may call
them or read the fields directly (same result):

| RVA 2015 / 2020 | Returns |
|---|---|
| 0x576CD0 / 0x4C8A80 | mouse (+0x4A70) |
| 0x576CE0 / 0x4C8A90 | keyboard (+0x4A74) |
| 0x576CF0 / 0x4C8AA0 | active pad (+0x4A7C) |

### 2.2 Devices

Every device object starts with a vtable and shares these fields:

| Offset | Type | Meaning |
|---|---|---|
| +0x0C | int | device type: 0 = pad, 1 = keyboard, 2 = mouse |
| +0x10 | byte | enabled / present |
| +0x11 | byte | **is the active device** (set by device selection, section 2.5) |

Keyboard device (vtable 0xFE7628 / ?, constructor 0x60E9C0 / ?, 0x228
bytes):

- `bool KeyDown(int scancode)` at **0x60EB20 / 0x560FE0**, thiscall,
  `ret 4`, AL. Tests bit 7 of the key-state byte at `this + 0x14 + scancode`
  (DirectInput-style scan codes: 0x1D = left Ctrl, 0x38 = left Alt).
- `bool AnyKey()` at **0x60EE00 / 0x5612D0**, thiscall, plain `ret`, AL.
  Returns the byte at +0x224 ("a key was pressed this frame").

Mouse device (vtable 0xFE75E0 / ?, constructor 0x60E520 / ?, 0xB0 bytes):

- `bool Active()` at **0x9BBE70 / 0x4C87E0**, thiscall, plain `ret`, AL.
  True when the dword at +0x54 is non-zero (the mouse moved or clicked).

### 2.3 The XInput pad device

The pad class has vtable **0xFE7670 / 0xF22B30**. Constructor 0x60EE10, object size 0x2B8, no RTTI. The vtable lives in
read-only data; writing a slot needs a page-protection change (and the
original protection restored afterwards).

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

| Slot | RVA 2015 / 2020 | Method | ABI |
|---|---|---|---|
| +0x00 | 0x610FE0 / ? | scalar deleting destructor | thiscall, `ret 4` |
| +0x04 | **0x576730 / 0x4C84D0** | `bool IsDown(int code)`: codes below 0x10 test the button mask; 0x10 / 0x11 compare LT / RT against the threshold; anything else is false | thiscall, `ret 4`, AL |
| +0x08 | **0x576780 / 0x4C8520** | `float GetValue(int code)`: non-negative magnitude. Buttons 1 or 0; 0x10 / 0x11 / 0x16 / 0x17 the raw trigger or zoom value; sticks (0x12..0x15) return 0; unknown codes 0 | thiscall, `ret 4`, ST0 |
| +0x0C | **0x576820 / 0x4C85C0** | `float GetAxis(int code, int negate)`: signed value, multiplied by -1 when `negate` is non-zero. Buttons ±1 or 0, triggers, sticks, zoom | thiscall, `ret 8`, ST0 |
| +0x10 | 0x576830 / ? | `int NameToControl(const char*)` | thiscall, `ret 4` |
| +0x14 | **0x576840 / 0x4C85E0** | `const char* ControlToName(code)`: the control's name for prompts and the input-mapping screen. The code is passed in a 32-bit stack slot; only the **low 16 bits** are meaningful: the stock method reads them as an unsigned 16-bit value and returns the name for 0x00..0x17 and **null for anything above**, -1 (0xFFFF) included [sure]. The game keeps the returned pointer, so it must stay valid for the whole process | thiscall, `ret 4`, EAX |
| +0x1C | 0x576850 / ? | `int ControlType(code)`: 0 button, 2 trigger (0x10, 0x11), 1 axis (0x12..0x17) | thiscall, `ret 4` |
| +0x20 | 0x60EE30 / ? | per-frame read: calls `XInputGetState(+0x290)` and sets +0x289 | |
| +0x28 | 0x60F280 / ? | scans XInput users 0..3 | |
| +0x2C | **0x576880 / 0x4C8620** | `bool HasActivity()`: true when +0x18 is non-zero or any of \|LX\|, \|LY\|, \|RX\|, \|RY\|, LT, RT exceeds 0.85 | thiscall, plain `ret`, AL |
| +0x40 | ? | "active state changed" notification, called by device selection | |

The five bold slots are the ones the mod replaces (3.3). Before writing
them, check that each of the five slots holds exactly the function this
table (and 01 §4.4) lists for the running build; if any slot differs,
nothing is patched (01 §2.5). A slot that points into the image but at
another function means the vtable is not the one described here (another
build, or another tool already replaced the slot), and chaining to it
with these ABIs would be wrong.

Connected flag: in the method at 0x60EE30 the instruction at **0x60EE6F /
0x56133F** is `setne cl` (`0F 95 C1`), following a compare of the
XInputGetState result with 0x48F (ERROR_DEVICE_NOT_CONNECTED); CL is then
stored to +0x289 [sure].

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
codes are 0x00..0x17; any other value of the binding record's 16-bit code
field (2.4) that is not -1 is free for the mod. The game passes a
binding's code to the pad methods zero-extended from that field, and never
passes -1 to IsDown [sure for digital actions, 2.4].

The prompt pipeline hands these names to the UI movies, which look them up
case-insensitively as input ids ("A" becomes the UI's `a` glyph, "LB"
becomes `lb`). Any name returned here becomes a glyph lookup in the UI.
Stick and trigger axes follow XInput: +X right, +Y up (forward), triggers
0..1.

### 2.4 Action maps

Each action map is an object with this layout (base-class constructor
0x5974E0; map 0's constructor 0x597750, map 1's 0x5978B0; map 2 uses the
base class, vtable 0xFE3B60; 2020: ? for all four, not needed):

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

Counts in use, observed in game on the 2015 build at the first remap
[sure]: map 0: 41 digital, 6 analog; map 1: 19 digital, 6 analog; map 2:
32 digital, 8 analog. They must still be read from the map, not assumed.

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
| +0x07 | byte: flags. Bit 0x40: the record refers to a composite entry, not a device control (below) [sure]. Bit 0x80: tested after a device reports the control down; its effect is not known (§11). Other bits: meaning unknown |

So binding *i* of an entry has its code at entry+0x04+8*i, its device at
entry+0x06+8*i and its flags at entry+0x07+8*i. A seventh record would
start at +0x30 and overwrite the record pointer and the count.

Composite entry (0x24 bytes): seven dword control codes at +0x00, a count
at +0x1C, a byte at +0x20, and a "keep" byte at +0x21. The map holds a
pointer to its composite list at +0x1034. Composite entries whose +0x21
byte is zero combine raw pad controls (believed to be stick vectors or
chords). They are the only path, other than per-action bindings, by which
a raw pad code reaches an action.

How the game evaluates a digital action [sure, 2015]: the map's
evaluation (0x586D80, thiscall on the map, called once per map at
0x590A08, 0x590A13 and 0x590A1E) walks each action's binding records. A
record with flag bit 0x40 set is a **composite reference**: its code is an
index into the map's composite list, and the record counts as down when
bit 0x02 of that entry's +0x20 byte is set; no device method is called. A
record without bit 0x40 is evaluated by calling IsDown (vtable +0x04) of
`device_by_type[binding.device]` with the record's code zero-extended,
when that device exists, its +0x10 byte is set and the code is not
0xFFFF. Analog actions are evaluated through GetValue or GetAxis of the
same device [likely: from the virtual codes arriving at those methods in
game; the analog evaluation was not traced].

Action indices that matter for VR (inferred from which game function
responds when each action is driven, see the provenance header; the AI.exe
names are unknown). Entries marked *unconfirmed* respond to VR controls in
the observed behaviour but their game function is not known: they are
**kept for parity, function unconfirmed** (open question, §11), so an
implementer must drive them as specified but must not read a design
requirement into them:

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
| 0x27 | *unconfirmed*: driven by the left grip, held |
| 0x28 | *unconfirmed*: driven by the right grip, held |

Map 0, analog: 0 = move X, 1 = move Y, 3 = turn (look X).

Map 1 (menus), digital: 0 and 0x10 = confirm, 8 = pause, 0x0A and 0x0F =
back; *unconfirmed*: 1 (driven with the radial input), 9 (driven by the
left grip, held), 0x0B and 0x0E (driven with the flashlight value).
Analog: 0 = left trigger, 1 = right trigger, 2 / 3 = left stick X / Y,
4 / 5 = right stick X / Y.

The stock pad layout, as the prompts show it: A interact, B radial menu,
X reload, Y flashlight, BACK map, START pause, LB motion tracker, RB aim,
LT crouch, RT fire, left stick move (click sprint), right stick look.

### 2.5 Device selection

The input manager's device selection, **0x57CBC0 / ?** (thiscall on the
manager), runs every frame. For each registered device of type 0 whose
+0x11 is clear it calls `HasActivity()` (vtable +0x2C). When that returns
true it clears +0x11 on every device (calling their +0x40 notification),
sets the new device's +0x11, calls its +0x40, and stores it in +0x4A7C and
in each map's `device_by_type[type]`. The game refreshes its button prompts
when the active device changes [sure for the code; the prompt refresh is
observed].

### 2.6 Left-stick prompt check

`bool IsMoveOnLeftStick()` at **0x57DCE0 / ?** (2020: the target of the call
sites below), thiscall with ECX = map 0 (manager+0x48), no args, plain
`ret`, AL. It walks the binding records of analog action 0 (pointer at
map+0xA74, count at map+0xA78), takes the first record whose device byte is
0 (pad) and returns `code == 0x12` (LSTICK_X); with no pad binding it
returns true.

It is called only from the prompt-name builder at 0x42CFF0 / 0x43E140, by
two `E8 rel32` calls at **0x42D0A0 and 0x42D18C / 0x43E1F0 and 0x43E2DC**
(the second is the first + 0xEC in both builds), to choose `"lstick"` or
`"rstick"` for the prompt keys `look_controls`, `hack_left_stick_controls`
and `hack_right_stick_controls`. Before redirecting, check that both sites
hold `E8` and that both calls reach the same target (2015: 0x57DCE0).

### 2.7 First-person camera (for snap turn and look gating)

First-person look is the game's FollowBehavior camera. Its vtable is at
**0xDC6600 / 0xDDD6E8**. Slot 0 is `OnActivate(arg)` (0x02C930 / 0x03C1A0)
and slot 1 is `OnDeactivate(arg)` (0x02BBF0 / 0x03B340), both thiscall
`ret 4`, return value in EAX. Hooking these two slots (05 §6.1) is how the
mod learns which camera instance is the live first-person camera and
whether it is active. Before writing, check that slot 0 holds the
OnActivate address and slot 1 the OnDeactivate address above, for the
running build.

| Offset | Type | Meaning |
|---|---|---|
| +0x1C0 | float x 3 | the position of the character the camera follows: copied from that character's object (`[[this+0x34]+0x148]`, its +0x70..+0x78) by slot 6 (0x02C9F0 / ?, thiscall, `ret 4`) and in every update (slot 8, 0x02E970 / ?, which takes the frame time); OnActivate (slot 0) does not write it [sure for the copy; likely the character's root position in world space, metres] |
| +0x1CC | float x 3 | the position of the character's head: the translation of the transform of the bone named `Head1`, written by the same two slots, through the bone index at +0x248 [sure]; in the same space as +0x1C0 [likely: the camera code subtracts it from a world-space point] |
| +0x1FC | float | second yaw copy: the game's yaw setter (0x02DF30 / ?, thiscall, a float yaw and a bool, `ret 8`) wraps the yaw, writes it to +0x204 and, when the bool is non-zero, the same value to +0x1FC [sure] |
| +0x204 | float | yaw. ±1.0 = ±180°; the game keeps it wrapped to [-1, 1] |
| +0x208 | float | pitch (clamped by the game) |
| +0x248 | int | index of the `Head1` bone, looked up by name in slot 6 (the name string at RVA 0xDC6640) [sure] |
| +0x264 | byte | **look input enabled**: the game applies stick and mouse look deltas (slot 11, 0x02D200 / ?) only while this is set |

The mod only reads +0x1C0, +0x1CC and +0x248 (for the snap-turn
diagnostic of 11 §4.5), and only while the camera is tracked and active.

The stock OnActivate sets +0x264 to 1; 05 §7.6 removes that store so the
mod controls the byte. After OnDeactivate the object may be freed (level
unload): the mod must stop writing to it once it is deactivated.

The game's stereo object (global pointer 0x1366A2C / 0x1376BDC, 02) has
the current stereo mode at +0x60: 1 means menus are on the virtual screen.

## 3. Required approach: virtualise the pad

### 3.1 Principle

Every action that has (or gets) a pad binding is bound to a **virtual pad
code** of its own. The game constrains these codes, and nothing else does:

- a virtual code is outside the game's pad codes 0x00..0x17 and is not -1
  ("no control"), so the stock pad never mistakes it for a real control;
- it fits the binding record's 16-bit code field (2.4), and the
  replacements recognise it from the low 16 bits of the argument alone,
  because the game passes the field zero-extended (2.4) and only the low
  16 bits are meaningful (2.3);
- it identifies exactly one (map, action) pair, and whether that action is
  digital or analog, so a query can be traced back to its action without
  ambiguity; a code the mod did not assign is recognisable as such.

How the codes are assigned is the implementation's choice.

The number of actions in use is read from each map at the time of
virtualisation (+0x1054 digital, +0x1058 analog), limited to the map's
capacity (41 digital, 16 analog, 2.4). Anything derived from these counts
is derived from the map, never assumed: for example, snap turn (6) needs
the virtual code of map 0's analog action 3 (turn), whatever the digital
count is.

For each action the mod remembers the action's **original pad control**
(a pad code, or "none"); every action reads "none" until the first
virtualisation. When the game asks the pad about a virtual code, the
answer is the real pad's state for the original control (through the
stock method) merged with the VR state for that action (3.3). VR input is
therefore written **per game action**, not per physical button, and a
real gamepad keeps working alongside VR controllers. Whatever code
arrives, a lookup from a code never reads outside the mod's per-action
state.

### 3.2 Virtualisation: what must hold afterwards

A virtualisation pass covers all three maps, every digital action below
that map's digital count and every analog action below its analog count.
Only binding records with device byte 0 (pad) among the first min(count, 6)
records of an action are considered: a seventh record would overwrite the
entry's record pointer and count (2.4). After a pass:

1. **One pad record per action.** Every action that had at least one pad
   record has exactly one that carries a control: the first, now holding
   the action's virtual code. Any further pad records hold -1, so a real
   pad reaches the action only through its virtual code.
2. **The original control is remembered**: the code the first pad record
   held before the pass (which may be -1, "no control"). A first pad
   record with flag bit 0x40 set is a composite reference (2.4): its code
   is an index, not a pad control, so the remembered original is "none"
   (whether the game builds such pad records is open, §11).
3. **Actions without a pad record** remember "none" whatever their record
   count. They also gain a pad record carrying their virtual code when they
   have fewer than 6 records; the count grows by one. The new record has
   device 0, its first 4 bytes are left as they are, and its flags are
   those of a pad record of the same kind (digital or analog) that the
   game built in the same map, or 0 when there is none, with bit 0x40
   clear (item 5); what the other flag bits mean is open (§11).
4. **Composite entries** (count limited to [0, 12]) whose +0x21 byte is
   zero no longer deliver raw pad codes: each of their first min(count, 7)
   code dwords holds -1. Writing -1 into the whole dword satisfies the
   game whether it reads the code as 32 or 16 bits.
5. Every pad record that carries a virtual code has flag bit 0x40 clear.
   Game fact (2.4): the map evaluates a record with bit 0x40 as a
   composite reference and never passes its code to the pad, so a virtual
   code in such a record would not reach the replaced IsDown.
6. **Re-running is harmless**: a pass over maps that are already
   virtualised changes nothing; in particular a record that already holds
   its action's virtual code keeps the remembered original control.
7. Actions beyond the counts in use remember "none".

The game can rebuild its bindings (loading, changing the controller
preset). An IsDown query that arrives with a code that is neither a
virtual code nor -1 means the maps hold un-virtualised bindings again;
the mod must then virtualise all three maps before answering. Triggering
the pass this way is sufficient: in game the first such query comes within
the first seconds of start-up, after the maps are built. A pass as soon as
the maps exist is also allowed, but the trigger on a raw query must stay
for rebuilds. Log the counts of each map once, at the first pass (in game:
41/6, 19/6, 32/8).

### 3.3 Pad method replacements

The mod replaces the pad vtable slots +0x04, +0x08, +0x0C, +0x14 and +0x2C.
The five slots, the connected-flag patch (3.4) and the left-stick redirect
(4.3) form **one patch**: they depend on each other (a replaced IsDown
virtualises the maps, after which a stock GetValue or GetAxis receives
codes it does not know and returns 0, and a stock ControlToName shows
nothing for them). If any of them cannot be written after its checks
passed, the ones already written are restored to the values they held
before, none of the group is left applied, and the log says that the
controls area is off and why (11 §2.1); the game then runs with its stock
pad and VR controllers have no effect.

Each replacement has exactly the stock method's ABI (2.3): thiscall with
`this` in ECX, the same stack arguments popped by the same `ret N`, the
result in the same register (AL, ST0 or EAX), and EBX, ESI, EDI and EBP
preserved. The stock entry points are kept so the replacements can ask the
real pad. "VR digital" and "VR analog" are the per-action VR state of
section 5 (false / 0 until written). Required results:

| Method (slot) | Code is a virtual code | Any other code |
|---|---|---|
| `IsDown(code)` (+0x04, `ret 4`, AL) | digital action: true when the real pad reports the original control down (stock IsDown; false when the original is "none") or the action's VR digital state is set. Analog action: the real pad's answer for the original control only (an analog action has no VR digital state) | false; a code that is not -1 first triggers a virtualisation pass (3.2) |
| `GetValue(code)` (+0x08, `ret 4`, ST0) | the larger of the real pad's stock GetValue for the original control (0 when "none") and the action's VR analog value (0 for a digital action) | 0 |
| `GetAxis(code, negate)` (+0x0C, `ret 8`, ST0) | of the real pad's stock GetAxis for the original control with the same `negate` (0 when "none") and the action's VR analog value (0 for a digital action), the one with the larger magnitude. The VR value is already in the game's sign convention and is not negated (§11) | 0 |
| `ControlToName(code)` (+0x14, `ret 4`, EAX), low 16 bits of the code | stock-glyph mode: the stock name of the original control; null when the original is "none", which is what the stock method returns for -1 (2.3), so the prompt looks as it does for an unbound action in the unmodded game. VR-glyph mode: the glyph for the original control (4.1), or the unbound glyph when the original is "none" or outside 0x00..0x17 | null |

`HasActivity()` (+0x2C, plain `ret`, AL): true when the stock HasActivity
is true or VR controllers were active (5.1). VR activity written for a
render frame is seen as activity by every HasActivity call the game
thread makes for the next game frame: the game's device selection (2.5)
and the mod's own checks alike, so one reader taking it never hides it
from another in the same game frame. It never carries over into a game
frame for which no VR activity was written. The flag the render thread
sets is read and cleared as one atomic operation, so activity written in
between is never lost; how the result is held for the rest of the game
frame is the implementation's choice. The prompt switch of 4.2 is driven
from the same activity.

No method fails or reads outside the mod's state for any code, and every
string `ControlToName` returns lives for the whole process.

Besides the game's action evaluation and device selection (2.5), the mod's
own features need these results:

- snap turn (6) uses the merged turn value (real right stick and VR turn)
  of map 0's turn action;
- game look enabling (7) uses the merged activity (real pad or VR);
- the recenter chord (8) uses the real pad only, through the stock IsDown
  with raw codes, because the replaced IsDown answers false for raw codes;
- the prompt switch (4.2) must also happen while the pad is already the
  active device: the game's device selection only queries devices that are
  not active (2.5), so the mod checks activity itself every frame.

### 3.4 The pad must stay connected

With no physical pad, the game would treat the pad as absent and stop
querying it. The connected flag (+0x289) must be forced to 1: at 0x60EE6F /
0x56133F replace `setne cl` (`0F 95 C1`) with `mov cl, 1` + `nop`
(`B1 01 90`). The patch site is listed with the other game patches in 05
§7.10.

### 3.5 When VR is off or failed

- Runtime None: the pad is not touched at all (no slot writes, no
  connected-flag patch, no remap; 01 §2.3).
- Runtime OpenVR or OpenXR that failed to start: the pad patches are in
  place (they are decided at attach, 01 §2.3), the bindings are
  virtualised, the VR state stays all zero and no glyph set is chosen, so
  every glyph name is the game's own name: the real pad and the prompts
  behave exactly as stock.

### 3.6 Threads

Pad queries (the replaced methods) run on the game thread, as do the
mod's per-frame update (05 §3.2) and the FollowBehavior hooks. The VR state
is written on the render thread (5.9); the general thread rules are in
`12-threads-and-robustness.md`. Requirements for the data that crosses:

- each per-action value (a digital byte or an analog float) is written and
  read as a whole aligned value, so a read never sees a torn value;
- the "VR controllers were active" flag is set and consumed with atomic
  operations;
- a read may see some actions from the newer write and some from the older
  (no frame-consistent snapshot is required; publishing one atomically is
  allowed);
- everything else of section 5 (toggle states, edge detectors, the chosen
  scheme for game actions) is touched only on the render thread; the glyph
  set (4.1) is chosen on the render thread and read on the game thread,
  and a read on the game thread always yields a complete name (the old or
  the new one, never a mix) that lives for the whole process;
- the controls help entries (5.7) are chosen on the render thread and read
  on the game thread when a menu opens: a reader gets one whole set, the
  old one or the new one, never entries of one set with the count of
  another, and never reads outside the set it got.

No allocation, file I/O or logging per frame on the render thread; log on
a change, or every 900 frames behind `DebugOptions` (06).

## 4. Button prompts

### 4.1 VR glyphs and glyph sets

The patched UI (14) provides a glyph image for every VR control
that a default binding (5.2, 5.3) gives to a prompted game function, and
one **unbound** glyph for an action with no pad control; it must not look
like any bound control's glyph, so a player never presses a control the
prompt did not mean:

- the right lower and upper face buttons, the left lower and upper face
  buttons;
- the right stick click;
- the left and right grips;
- the menu button;
- the right touchpad's centre, upper-half and lower-half click regions.

More glyphs may be provided. Glyph ids and image names are the
implementation's choice, under the game's constraints: the movies look up
a name returned by `ControlToName` case-insensitively as an input id
(2.3), so each glyph name the DLL returns must be an id the patched movies
register, must differ from every stock input id (`a`, `lb`, ...), and the
DLL and the UI resources (14) must use the same ids.

The game asks for the name of an action's original pad control (3.3), so
the stock pad layout (2.4) ties each pad code to a game function. Required
result: **a prompt for a game function shows the glyph of the VR control
that drives that function under the default binding of the current set.**
Until a set is chosen (5.5, 3.5), every pad code shows the game's own name.

| Stock pad control (function) | Touchpad set (5.2) | Thumbstick set (5.3) |
|---|---|---|
| A (interact) | right touchpad, lower-half click | right lower face button |
| B (radial menu) | right touchpad, centre click | right stick click |
| X (reload / activate) | right touchpad, upper-half click | right upper face button |
| Y (flashlight) | the game's own name (see below) | left upper face button |
| BACK (map) | menu button | left lower face button |
| START (pause) | menu button | menu button |
| LB (motion tracker) | left grip | left grip |
| RB (aim) | right grip | right grip |
| every other pad code (triggers, sticks, stick clicks, d-pad, zoom) | the game's own name | the game's own name |

Why the other codes keep the game's names: the stock trigger and stick
glyphs already show the control that drives those functions on VR
controllers (left trigger crouches, right trigger fires, the left stick or
pad moves). The flashlight in the touchpad set has no control of its own
(only interact near the head, 5.2), and a glyph cannot show that gesture;
keeping the stock name is a project decision for parity with the observed
controls.

Which set is in use, and when it is chosen, is in 5.5. The glyphs name the
default bindings; a player who rebinds in the runtime still sees the
default glyphs. On Index the thumbstick set shows the left lower and upper
face-button glyphs for the left A / B, which take those roles (5.3).

### 4.2 Switching prompts with the input source

The prompts are in one of two glyph modes, VR or stock, initially VR
(project decision: a VR run starts with the controllers in hand, and a real
pad switches to stock at its first use). Required results:

- When VR controllers show activity, the mode is VR; when only the real pad
  shows activity (the stock HasActivity), the mode is stock. Activity from
  both in the same check selects VR (project decision: a pad lying on a
  desk can report stick noise, while VR activity needs a deliberate press
  or a deflection past 0.85). No activity leaves the mode as it is.
- When the mode changes, the prompts on screen redraw with the new glyphs
  without the player having to touch another device, and this also works
  while the pad is already the active device (3.3).
- Afterwards the pad is the active device again, so the prompts stay
  pad-style. Because VR activity is part of HasActivity (3.3), the game's
  device selection makes the pad the active device whenever VR controllers
  are used, so the game shows pad-style prompts (with VR glyphs) rather
  than keyboard prompts.

Game facts for causing the redraw (2.5): the game re-queries
`ControlToName` for its prompts when device selection activates a device;
device selection examines only type-0 devices whose +0x11 is clear and
calls the +0x40 notification of the devices it changes; map 0's pad and
keyboard are at manager +0x1084 / +0x1088 and the active pad at +0x4A7C.
The redraw is therefore caused through the game's own device selection;
how is the implementation's choice. Game fields are touched only while the
input manager exists, and after the redraw exactly one device is active,
the pad. Every device pointer the redraw uses (map 0's pad and keyboard
entries, the active pad) is tested before use: a map entry can be null
before a device of that type was registered or selected. If a device the
redraw needs is absent, the glyph mode still changes and the prompts
redraw at the game's next device change; no game field is written through
a null pointer.

When the patched UI archive is not in use, the movies know no VR glyph
ids: the mod then stays in stock-glyph mode for the whole run, so prompts
show the game's own pad glyphs. "In use" means that in this run the VR
change set of 14 §1.3 was linked or reused and the UI archive pointer was
redirected to it (14 §5); with a missing or rejected archive (14 §5.9), or
with the menu set only, it is not in use.

### 4.3 Left-stick prompt check

Once bindings are virtualised, the stock check (2.6) sees a virtual code
and always answers false. Both call sites must instead get: "map 0 analog
action 0's original pad code is LSTICK_X (0x12)", or true when it has none
(before the first remap every action is "none", so true). Redirect the two
calls (same ABI: thiscall, no args, AL, plain `ret`; ECX is not needed) to
a function that answers that. Listed with the game patches in 05 §7.11.

## 5. VR controller mapping

### 5.1 Common rules

- Once per frame (5.9) the mod reads both hand controllers and writes the
  VR state of every map 0 and map 1 action listed in 5.8.5; every other
  entry stays 0. Map 2 gets no VR state.
- The activity flag consumed by HasActivity is set when any written
  digital value is true or any written analog value has \|v\| > 0.85
  (map 1 values included). 0.85 is the game's own HasActivity threshold
  for sticks and triggers (2.3), so a VR control counts as activity under
  the same rule as a real pad.
- Thresholds:
  - trigger as a button > 0.5: the game's own trigger press threshold
    (pad +0x04, 2.3), so a VR trigger fires where a pad trigger does;
  - analog grip (squeeze) as a button > 0.5, by the same rule;
  - touchpad centre zone \|x\| < 0.33 and \|y\| < 0.33 (project decision:
    the middle third of the pad in each axis, large enough to hit without
    looking, small enough to leave the upper, lower and edge regions
    usable);
  - "near the head" = right hand within 0.2 m of the head (5.10; project
    decision: about the distance from a hand held at the side of the head
    to the headset's centre, beyond the reach of a hand held in front of
    the chest);
  - weapon-switch and snap-turn deflection > 0.5 (project decision: half
    deflection, so drift or a light touch never acts; the reason is the
    one 05 §3.2c gives for snap turn).
- Every analog value written to the game is limited to [-1, 1] (0 to 1 for
  triggers), whether or not it was scaled, because the game's own pad never
  delivers more (a runtime can report a stick slightly past 1, or a
  square-gated stick on a diagonal). The move vector may be limited to
  length 1 in either scheme; in the touchpad scheme it must be (5.2).
- **No input focus**: while the runtime does not give the game input focus
  (the SteamVR dashboard or another overlay has it; OpenXR session not
  `FOCUSED`), every control reads as released / 0, so every VR action value
  is written as false / 0. Under SteamVR Input and OpenXR this follows from
  the runtime reporting every action inactive (5.8.2, 5.8.4) [sure for
  OpenXR, observed in game: `xrSyncActions` returned
  `XR_SESSION_NOT_FOCUSED` with the dashboard open; likely for SteamVR
  Input]. (An earlier mod kept controls latched while the Oculus dashboard
  had focus; this requirement fixes that.)
- **No input at all**: when a controller is missing, or the session is not
  running (OpenXR), or the runtime has asked the game to exit, all VR
  action state must be zero (not the last values read).
- No haptics in v1.

Touchpad region decoding (used by the touchpad scheme, on the right pad
with its click):

- horizontal = \|x\| > \|y\|
- centre = \|x\| < 0.33 and \|y\| < 0.33
- padDown = click and not horizontal and not centre and y < 0
- padUp = click and not horizontal and not centre and y > 0
- padCentre = click and centre

A click with horizontal true (left or right edge) drives none of the three.

### 5.2 Touchpad scheme (Vive wand)

Map 0:

| Action | Input |
|---|---|
| Move X / Y | L pad x / y (only while the L pad is touched), × 1.1, then limited to length 1 (below) |
| Sprint | L pad click |
| Crouch | L trigger > 0.5 |
| Aim | R grip (`Input.ToggleGrips`, 5.6) |
| Fire / throw / melee (both indices) | R trigger > 0.5 |
| Interact (both) | padDown |
| Radial menu | padCentre |
| Reload / activate (both) | padUp |
| Motion tracker | L grip and not padCentre (`Input.ToggleGrips`, 5.6) |
| Flashlight | padDown while the right hand is near the head |
| Map | L application-menu button |
| 0x27 / 0x28 (kept for parity, function unconfirmed, §11) | L grip / R grip, held (never toggled) |
| Previous / next weapon | never |
| Turn | see below |

Turn: when horizontal and not centre (of the R pad position), R.x × 0.85,
else 0. With `Input.SnapRotation` on, the value is only produced while the
R pad is clicked (so a resting thumb does not turn); with it off, touch is
enough (smooth turn through the game's own look).

The two scale factors are project decisions for parity with the observed
wand controls, and may be tuned: × 1.1 on movement lets a thumb reach full
walking speed a little inside the rim of the round pad, where it rarely
rests; × 0.85 on turning keeps smooth turning from the pad, which has no
spring return, a little below the full rate a stick at its stop gives.

After the × 1.1, the move keeps its direction and its length is the
smaller of its own length and 1. Project decision of spec r6: a round pad
at full deflection must give the same speed as a stick at full deflection
in every direction, so the × 1.1 only changes where full speed is reached
and a diagonal is never faster than straight ahead. Limiting each
component to [-1, 1] on its own does not meet this: on a diagonal near the
rim it leaves a move up to about 1.1 long.

Map 1:

| Action | Input |
|---|---|
| Confirm (both) | padDown or L pad click |
| Index 1 (kept for parity, function unconfirmed, §11) | padCentre |
| Pause | R application-menu button |
| Index 9 (kept for parity, function unconfirmed, §11) | L grip, held (never toggled) |
| Back (both) | padUp |
| Indices 0x0B / 0x0E (kept for parity, function unconfirmed, §11) | the flashlight value of map 0 |
| Analog 0 / 1 | L / R trigger value |
| Analog 2 / 3 | L pad x / y while touched (not scaled) |
| Analog 4 / 5 | R pad x / y while touched (not gated, not scaled) |

### 5.3 Thumbstick and face-button scheme (Oculus Touch; also Index, Quest, Pico, WMR and anything else)

v1 uses this scheme for every controller that is not a Vive wand, under
any runtime (a user requirement: under OpenVR an earlier mod treated every
controller as a wand).

Map 0:

| Action | Input |
|---|---|
| Move X / Y | L stick (not scaled) |
| Sprint | L stick click |
| Crouch | L trigger > 0.5 |
| Aim | R grip (`Input.ToggleGrips`) |
| Fire (both) | R trigger > 0.5 |
| Interact (both) | A (right lower face button) |
| Radial menu | R stick click |
| Reload / activate (both) | B (right upper face button) |
| Previous weapon | R stick: not horizontal and y < -0.5 |
| Next weapon | R stick: not horizontal and y > 0.5 |
| Motion tracker | L grip and not R stick click (`Input.ToggleGrips`) |
| Flashlight | Y, or A while the right hand is near the head |
| Map | X (left lower face button) |
| 0x27 / 0x28 (kept for parity, function unconfirmed, §11) | L grip / R grip, held (never toggled) |
| Turn | R stick x when horizontal and not centre, else 0 (no click needed, whatever the snap option) |

Map 1:

| Action | Input |
|---|---|
| Confirm (both) | A |
| Index 1 (kept for parity, function unconfirmed, §11) | R stick click |
| Pause | the menu button (Touch: left menu button); on Index, which has no menu button, the left trackpad press |
| Index 9 (kept for parity, function unconfirmed, §11) | L grip, held (never toggled) |
| Back (both) | B |
| Indices 0x0B / 0x0E (kept for parity, function unconfirmed, §11) | the flashlight value of map 0 |
| Analog 0 / 1 | L / R trigger |
| Analog 2 / 3 | L stick |
| Analog 4 / 5 | R stick |

On Index controllers the left A / B act as X / Y; the trackpads only carry
the pause press. Grip is the controller's grip button, or squeeze > 0.5
where the grip is analog.

### 5.4 Windows Mixed Reality

WMR controllers have a thumbstick, a touchpad, trigger, grip and menu, but
no A/B/X/Y. They use the **thumbstick scheme** (5.3) for the game actions;
the default bindings put the face-button roles on the touchpads:

- SteamVR Input (5.8.2): interact / reload on the right touchpad's lower /
  upper half (click), map / flashlight on the left touchpad's lower / upper
  half (click), pause on either menu button; sticks, triggers and grips as
  in 5.3.
- OpenXR (5.8.4): the touchpad click of each hand is the "primary" button,
  so interact is the right touchpad click (anywhere), map the left touchpad
  click; there is no reload and no flashlight button (flashlight only by
  interact near the head); pause on either menu button.

The glyph set and controls help entries are the thumbstick ones (5.5), so
on WMR they name face buttons for controls that are touchpad presses. Not
verified on WMR hardware.

### 5.5 Choosing the scheme and the glyph set

Two independent choices:

**Game-action scheme** (touchpad 5.2 or thumbstick 5.3), evaluated every
frame from what the runtime reports:

| Runtime | Touchpad scheme when | Otherwise |
|---|---|---|
| OpenVR, SteamVR Input | the controller bound to the game is a Vive wand (`vive_controller`); 5.8.2 item 6 gives allowed ways to tell | thumbstick |
| OpenVR, legacy (5.8.3) | always | - |
| OpenXR | the right hand's current interaction profile is `/interaction_profiles/htc/vive_controller` | thumbstick (also while no profile is bound) |

**Glyph set and controls help entries** (4.1, 5.7), chosen **once per run**:

| Runtime | At stereo device creation | Then, the first time the right-hand controller is identified |
|---|---|---|
| OpenVR, SteamVR Input | touchpad set and entries | `Prop_ControllerType_String` of the right-hand role's device: `vive_controller`: touchpad set and entries; `knuckles`: thumbstick set, Index variant of the entries; anything else (`oculus_touch`, `holographic_controller`, unknown): thumbstick set and entries |
| OpenVR, legacy | touchpad set and entries | no change |
| OpenXR | thumbstick set and entries | the first frame on which a right-hand profile is known: `htc/vive_controller`: touchpad set and entries; any other: thumbstick set and entries (including Index: under OpenXR its pause entry names the menu button) |

"Identified" under OpenVR: a hand's controller is identified when the
runtime first reports a valid device for that hand role, and again
whenever the device for the role changes; each identification is logged
with the hand and its `Prop_ControllerType_String`. The glyph set is
chosen from the first right-hand identification of the run. Under
OpenXR, the profile is read at each
`XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED` (5.8.4).

Re-choosing the glyph set when controllers change during a run is not
required in v1 (per-controller glyphs are a v2 item).

### 5.6 Option effects

- **Toggle grips** (`Input.ToggleGrips`, default off; its menu label must
  convey that the grips toggle aim and the motion tracker): while on,
  Aim flips on each rising edge of the right grip and Motion tracker flips
  on each rising edge of the left grip; the radial input does not suppress
  the toggled tracker. While off, Aim = right grip and Motion tracker = left
  grip and not radial, held. The edge detectors follow the grips on every
  controls write whatever the option; the two toggled states start off,
  change only while the option is on, and keep their value when the option
  is switched off and on again, and through focus loss (a grip still held
  when focus returns reads as a new press, since it read released while
  focus was away). The held grip actions (map 0 0x27 / 0x28, map 1 9) are
  never toggled.
- `Input.ToggleCrouch`, `Input.VRInputRelativeWalking`,
  `Input.VRInputStrafing`: stored and shown to the UI only; they change no
  input behaviour in v1 (the observed controls had no effect for them, and
  the keys stay so `mothervr.json` keeps its layout) and the menu hides
  them (06).
- `Input.SnapRotation` and `Input.SnapRotationSegments`: section 6.
  `Input.SnapRotation` also gates touchpad turning (5.2).

All options are read when used, so a change in the menu takes effect on
the next frame.

### 5.7 Controls help page

The options menu has a read-only controls help page that the mod fills
with entries for the current set (5.5). The entries are delivered to the
menu through the UI interface of 06 §4.3, one message per entry carrying
the function's text and the control's text, whenever the mod pushes its
settings to a menu. Required content:

- For every game function that has a default control in the current set
  (5.2 for the touchpad set, 5.3 for the thumbstick set; 5.4 for WMR,
  which uses the thumbstick entries), the page names that control.
- Touchpad set: the flashlight is reachable only by interact with the
  right hand near the head (5.10), and the page says so; weapon selection
  has no control and no entry.
- Thumbstick set: the flashlight has its own button and is also reachable
  by interact near the head; the page names both.
- Index variant (5.5): the thumbstick entries, with pause on the left
  trackpad press.
- From stereo device creation on, a set is always chosen (the first
  column of the table in 5.5), also before any controller is identified.
- Until a set is chosen in this run (5.5: runtime None, a runtime that
  failed to start, or before the stereo device is created), the page lists
  no controller entries; it may show one entry saying that the controls
  appear once VR is running. Showing a layout for controllers that are not
  in use would mislead the player.

The texts, the order and grouping of the entries and the page's layout are
the implementation's.

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

The runtime's action system is asked for inputs named by game function,
not by physical control, so that a player rebinding in the runtime's UI
rebinds a game function. The set must cover the inputs below. Each is
referred to in the rest of this document by the game function it drives
(2.4); these labels are descriptive only: the identifiers, display names,
localisation, order and the exact split into actions in the runtime's
files are the implementation's own, as long as the default bindings
reproduce 5.2 to 5.4 and the player can recognise each input's game
function from its display name in the runtime's binding UI.

| Type | Inputs (label: what it drives, 5.8.5) |
|---|---|
| 2D vector | Move: move X / Y. Turn: turn (x) and, in the thumbstick scheme, previous / next weapon (y) |
| 1D value, 0..1, pressed above 0.5 (so a trigger works as the default control) | Crouch: crouch. Fire: fire, throw, melee |
| boolean | Aim: aim (`Input.ToggleGrips`). Tracker: motion tracker (`Input.ToggleGrips`). Interact: interact, and the flashlight when the right hand is near the head. Reload: reload / activate, back in menus. Radial: radial menu. Flashlight: flashlight. Sprint: sprint. Map: map. Pause: pause. Turn engage: touchpad scheme only, engages turning while `Input.SnapRotation` is on (5.2) |
| pose, one per hand | Hand poses: not used by the game in v1 (5.11) |

#### 5.8.2 OpenVR: SteamVR Input

Required when the runtime is OpenVR and the `IVRInput` interface is
available (`VR_GetGenericInterface(IVRInput_Version)` returns non-null):

1. At runtime start (CreateDevice, after the compositor is ready, 03 §6),
   create a folder in the game directory named `mothervr_input` (next to `AI.exe`;
   a compatibility name the player guide refers to) and write into it one **default binding file
   per controller type** and the **action manifest**. Write them at every
   start, replacing the old files, so an updated mod updates its defaults.
   SteamVR keeps a player's own bindings separately, so rewriting the
   defaults does not lose them.
2. The manifest (SteamVR's public action manifest format) declares one
   action set (usage `single`); the inputs of 5.8.1 with their types
   (`boolean`, `vector1`, `vector2`, and `pose` for the hand poses); English localisation; and default bindings for the controller
   types `knuckles`, `oculus_touch`, `holographic_controller` and
   `vive_controller`, with the assignments in the table below.
3. A binding a player saved for an older version of the mod may lack
   inputs added later; those read inactive (5.11 logs the case for the
   hand poses). The implementation may mark binding files with a version
   so players can tell them apart in SteamVR's binding UI.
4. Call `IVRInput::SetActionManifestPath` with the manifest's absolute path
   (UTF-8), then get the set and action handles (`GetActionSetHandle`,
   `GetActionHandle`). Log the number of actions and the manifest path.
5. Each controls write (5.9): one `UpdateActionState` for the set (one
   `VRActiveActionSet_t`, no restriction); then `GetDigitalActionData` /
   `GetAnalogActionData` for each action, from any device
   (`k_ulInvalidInputValueHandle`). An action whose call fails or whose data
   is not `bActive` counts as false / 0; a boolean is `bActive and bState`;
   a 1D value is `x`; a 2D value is `(x, y)`. If `UpdateActionState` fails,
   every input reads false / 0 for that write (thumbstick scheme, nothing
   pressed).
6. The touchpad scheme applies when the controller bound to the game is
   a Vive wand (5.5). Under SteamVR Input one allowed way to tell is that
   the wand-only turning press reads `bActive` (only the wand default binds
   it); reading the right hand's controller type is another. In the
   touchpad scheme Turn is engaged while that press is held; in the
   thumbstick scheme Turn is always engaged.

Failure: if `IVRInput` is unavailable, or a file cannot be written (folder
creation failure, write error), or `SetActionManifestPath` returns an
error, log the reason and use the legacy path (5.8.3) for the rest of the
run.

Default bindings (`L` / `R` = left / right hand; SteamVR input source path
under `/user/hand/<hand>/input/`, binding mode, and the mode's input):

| Input | `knuckles` (Index) | `oculus_touch` | `holographic_controller` (WMR) | `vive_controller` (wands) |
|---|---|---|---|---|
| Move | L `thumbstick`, `joystick`, `position` | L `joystick`, `joystick`, `position` | L `joystick`, `joystick`, `position` | L `trackpad`, `trackpad`, `position` |
| Sprint | same source, `click` | same source, `click` | same source, `click` | same source, `click` |
| Turn | R `thumbstick`, `joystick`, `position` | R `joystick`, `joystick`, `position` | R `joystick`, `joystick`, `position` | R `trackpad`, `trackpad`, `position` |
| Turn engage | - | - | - | same source, `click` |
| Radial | same source as Turn, `click` | same source as Turn, `click` | same source as Turn, `click` | R `trackpad`, `dpad` (parameter `sub_mode` = `click`), `center` |
| Interact | R `a`, `button`, `click` | R `a`, `button`, `click` | R `trackpad`, `dpad` (`sub_mode` `click`), `south` | the R `dpad` source above, `south` |
| Reload | R `b`, `button`, `click` | R `b`, `button`, `click` | the R `dpad` source above, `north` | the R `dpad` source above, `north` |
| Map | L `a`, `button`, `click` | L `x`, `button`, `click` | L `trackpad`, `dpad` (`sub_mode` `click`), `south` | L `application_menu`, `button`, `click` |
| Flashlight | L `b`, `button`, `click` | L `y`, `button`, `click` | the L `dpad` source above, `north` | - (interact near the head) |
| Crouch | L `trigger`, `trigger`, `pull` | same | same | same |
| Fire | R `trigger`, `trigger`, `pull` | same | same | same |
| Tracker | L `grip`, `grab`, `grab` | L `grip`, `button`, `click` | L `grip`, `button`, `click` | L `grip`, `button`, `click` |
| Aim | R `grip`, `grab`, `grab` | R `grip`, `button`, `click` | R `grip`, `button`, `click` | R `grip`, `button`, `click` |
| Pause | L `trackpad`, `button`, `click` (Index has no menu button) | L `application_menu`, `button`, `click` (the Touch menu button) | L and R `application_menu`, `button`, `click` (two sources) | R `application_menu`, `button`, `click` |
| Left / Right hand pose | `/user/hand/<hand>/pose/grip` | `/user/hand/<hand>/pose/openxr_grip` | `pose/grip` | `pose/grip` |

The wand's right trackpad appears in two sources (`trackpad` mode for Turn
and Turn engage, `dpad` mode for the click regions); SteamVR allows this.
These reproduce 5.3 (Index, Touch), 5.4 (WMR) and 5.2 (wands).

Why these pose components: the hand poses are meant to match OpenXR's
`/input/grip/pose` (palm origin, handle orientation). SteamVR's own
OpenXR layer maps that path to the component `openxr_grip` where the
controller's profile defines it (Touch does); the Index, Vive and WMR
profiles do not, and there `grip` is the closest (it sits on the handle in
the palm with the handle's tilt) [likely, from SteamVR's controller
profiles and render models].

#### 5.8.3 OpenVR: legacy fallback

When `IVRInput` is not available (an old runtime) or set-up fails (5.8.2),
read the controllers through `IVRSystem`'s controller state and log that
the legacy path is used:

- the hands are `IVRSystem::GetTrackedDeviceIndexForControllerRole` for
  `TrackedControllerRole_LeftHand` / `_RightHand`, looked up on every
  write; their state comes from `IVRSystem::GetControllerState` (a hand
  with an invalid index reads all zero);
- every controller is treated as a Vive wand: the **touchpad scheme** of
  5.2. Axis 0 is the pad, axis 1 the trigger (its `x`); the touchpad button
  (`k_EButton_SteamVR_Touchpad`) pressed bit is the pad click and its
  touched bit the pad touch; `k_EButton_Grip` pressed is the grip;
  `k_EButton_ApplicationMenu` pressed is the menu button;
- Move is the left pad while it is touched (else 0); Turn the right pad
  while it is touched (else 0); Turn engage the right pad click; Sprint the
  left pad click; Interact / Reload / Radial the right pad click regions
  (padDown, padUp, padCentre of 5.1, from the right pad position whether
  touched or not); Map the left menu, Pause the right menu; Crouch / Fire
  the trigger values; Tracker / Aim the grips; no Flashlight button.

The glyphs are then the touchpad set (4.1).

#### 5.8.4 OpenXR: actions and suggested bindings

Required when the runtime is OpenXR (09), when the session is created
(before it begins):

- one action set (priority 0) whose actions, each with both hand
  subaction paths (`/user/hand/left`, `/user/hand/right`), together read
  every physical control the suggested bindings below name, each with an
  action of the matching type: the grip pose as `XR_ACTION_TYPE_POSE_INPUT`,
  trigger and squeeze values as `XR_ACTION_TYPE_FLOAT_INPUT`, the stick or
  trackpad position as `XR_ACTION_TYPE_VECTOR2F_INPUT`, clicks and buttons
  as `XR_ACTION_TYPE_BOOLEAN_INPUT`. How the controls are split into
  actions (per physical control, or per game function with the same
  suggested assignments), and their names, are the implementation's; what
  is required is which physical control drives which game function, per
  profile. No haptic output in v1;
- `xrSuggestInteractionProfileBindings` for each profile below (a profile
  the runtime rejects is logged and skipped; the others still apply), then
  `xrAttachSessionActionSets`; one action space per hand for the grip pose
  (`xrCreateActionSpace` with that hand's subaction path and the identity
  pose). If creating the set or an action, or attaching, fails: log it,
  destroy what was made and run with no VR input (the session still
  renders);
- the action spaces and set are destroyed with the session;
- each OpenXR frame, right after `xrBeginFrame` (09 §5.3):
  `xrSyncActions` for the set (one active set, no subaction filter), then
  each hand's states with `xrGetActionState*` and the grip pose located in
  the base space at the frame's `predictedDisplayTime`, with its linear
  velocity chained (`XrSpaceVelocity`). An inactive state, or a failed call,
  counts as false / 0; `XR_SESSION_NOT_FOCUSED` is a success code under
  which every state reads inactive, which zeroes all VR state as 5.1
  requires. If `xrSyncActions` fails, both hands read all zero and
  unlocated for that frame. Log the `xrSyncActions` result whenever it
  changes (observed in game: 0 while focused, 8 =
  `XR_SESSION_NOT_FOCUSED` with the SteamVR dashboard open);
- a grip pose is used only when both `XR_SPACE_LOCATION_ORIENTATION_VALID_BIT`
  and `XR_SPACE_LOCATION_POSITION_VALID_BIT` are set; its velocity only when
  `XR_SPACE_VELOCITY_LINEAR_VALID_BIT` is set. When a hand's grip cannot be
  located, log once per hand and per change of cause, with enough to tell
  an unbound pose action from a controller the runtime cannot track and
  from a failed call (11 owns the wording);
- on `XrEventDataInteractionProfileChanged`, read the right hand's profile
  with `xrGetCurrentInteractionProfile(/user/hand/right)` and log it, or
  log that no profile is bound (`XR_NULL_PATH` or a failure). The profile
  selects the scheme and glyph set (5.5). Observed in game with Index under
  SteamVR: several "no profile" events at session start, then
  `/interaction_profiles/valve/index_controller` within 20 ms.

Suggested bindings (paths under `/user/hand/<hand>/`; `L` left, `R` right,
both hands when not marked). Rows are physical controls; "primary" and
"secondary" are the lower and upper face buttons (A / X and B / Y):

| Control | `oculus/touch_controller` | `valve/index_controller` | `htc/vive_controller` | `microsoft/motion_controller` | `khr/simple_controller` |
|---|---|---|---|---|---|
| grip pose | `input/grip/pose` | same | same | same | same |
| trigger | `input/trigger/value` | same | same | same | `input/select/click` |
| squeeze | `input/squeeze/value` | `input/squeeze/value` | `input/squeeze/click` | `input/squeeze/click` | - |
| stick | `input/thumbstick` | `input/thumbstick` | `input/trackpad` | `input/thumbstick` | - |
| stick click | `input/thumbstick/click` | `input/thumbstick/click` | `input/trackpad/click` | `input/thumbstick/click` | - |
| primary | L `input/x/click`, R `input/a/click` | `input/a/click` | - | `input/trackpad/click` | - |
| secondary | L `input/y/click`, R `input/b/click` | `input/b/click` | - | - | - |
| menu | L `input/menu/click` | L `input/trackpad/force` | `input/menu/click` | `input/menu/click` | `input/menu/click` |

(The Index pause is the left trackpad force bound to a boolean action; the
runtime applies its own press threshold.)

Per-hand states to semantic inputs (grip = squeeze > 0.5):

| Input | Thumbstick scheme (every profile but the wand) | Touchpad scheme (`htc/vive_controller`) |
|---|---|---|
| Move | L stick | L stick (the trackpad position as the runtime reports it; no touch gate) |
| Sprint | L stick click | L stick click (L pad click) |
| Turn | R stick | R stick (R pad position) |
| Turn engage | always engaged | R stick click |
| Radial | R stick click | R pad click in the centre zone (padCentre) |
| Interact | R primary | R pad click, padDown region |
| Reload | R secondary | R pad click, padUp region |
| Map | L primary | L menu |
| Flashlight | L secondary | - |
| Pause | L menu or R menu | R menu |
| Crouch / Fire | L / R trigger | L / R trigger |
| Tracker / Aim | L / R grip | L / R grip |

#### 5.8.5 Semantic inputs to game actions

Whatever the runtime, the game action state of 5.2 and 5.3 is written from
the semantic inputs. With `turn` = Turn, `horizontal` =
\|turn.x\| > \|turn.y\|, `centre` = \|turn.x\| < 0.33 and \|turn.y\| < 0.33,
`touchpad` = the game-action scheme of 5.5, `engaged` = Turn engage (always
true in the thumbstick scheme), `near` = the near-head test (5.10) and
`flash` = Flashlight or (Interact and near):

Map 0:

| Game action | Value |
|---|---|
| Move X / Y (analog 0 / 1) | Move; in the touchpad scheme × 1.1, then limited to length 1 as a vector (5.2) |
| Sprint (0x08) | Sprint |
| Crouch (0x09) | Crouch > 0.5 |
| Aim (0x0A) | Aim, with `Input.ToggleGrips` (5.6) |
| Fire (0x0B, 0x0E) | Fire > 0.5 |
| Interact (0x0C, 0x0F) | Interact |
| Radial (0x0D) | Radial |
| Reload (0x10, 0x11) | Reload |
| Previous / next weapon (0x1A / 0x1B) | thumbstick scheme: not horizontal and turn.y < -0.5 / > 0.5; touchpad scheme: false |
| Motion tracker (0x1C) | toggle off: Tracker and not Radial; toggle on: the toggled state (5.6) |
| Flashlight (0x1D) | flash |
| Map (0x26) | Map |
| 0x27 / 0x28 (kept for parity, function unconfirmed, §11) | Tracker / Aim, held (never toggled) |
| Turn (analog 3) | 0 if `Input.SnapRotation` is on and not engaged; else, if horizontal and not centre: turn.x (× 0.85 in the touchpad scheme), else 0 |

Map 1:

| Game action | Value |
|---|---|
| Confirm (0, 0x10) | Interact, or (touchpad scheme only) Sprint, the left pad click |
| Index 1 (kept for parity, function unconfirmed, §11) | Radial |
| Pause (8) | Pause |
| Index 9 (kept for parity, function unconfirmed, §11) | Tracker, held (never toggled) |
| Back (0x0A, 0x0F) | Reload |
| 0x0B, 0x0E (kept for parity, function unconfirmed, §11) | flash |
| Analog 0 / 1 | Crouch / Fire values |
| Analog 2 / 3 | Move (not scaled) |
| Analog 4 / 5 | turn.x / turn.y (not gated, not scaled) |

Every other map 0 / map 1 entry, and all of map 2, is 0. The activity flag
and the clearing rules of 5.1 apply to these values.

### 5.9 When the controls reach the game

The game runs its simulation on the game thread and calls the stereo
device's begin frame (slot 5) and end frame (slot 6) on the render thread
(02, 03 §6). The VR state must reflect the controllers as of the newest
poses the runtime has delivered, before the game's next simulation step
reads them:

- **OpenVR**: at the end of slot 6, after both submits and after
  `IVRCompositor::WaitGetPoses` returns (and after the hand poses of 5.11
  are read), read the controllers (5.8.2 or 5.8.3) and write the VR state.
  **Fallback**: at slot 5, write the VR state only if no slot 6 has written
  it since the previous slot 5. This covers the frames before the first
  submit and any slot 6 that returns early without submitting (no eye
  texture), which also skips `WaitGetPoses`. Never both in one frame.
- **OpenXR**: at the end of slot 6, after the OpenXR frame call of 09 §5.3
  (end the previous frame, poll events, wait, begin, locate views, sync
  actions), write the VR state from the states synced at that
  `xrBeginFrame`. There is no read at slot 5: without a new OpenXR frame
  there is nothing new to read. Slot 5 instead checks whether the runtime
  has asked the game to exit (session `EXITING`) and, if so, sets the
  game's exit flag (06); a session or instance loss does not (09 §3). When slot 6 returns early (no session, no eye
  texture), the previous VR state stays.

Why: reading at slot 5 used a frame-old controller state for the frame's
own poses; handing the state over right after the runtime's frame wait
gives the game's next step the controllers sampled with the poses it will
render with (one frame less input latency).

### 5.10 Near-the-head test

"Near the head" is: the right hand's position is within **0.2 m** (runtime
metres, before the world scale of 03 is applied) of the head's position.
The poses used are the render thread's own, taken at the same time as the
controls they modify, never the head pose the game thread reads:

- **OpenVR**: `IVRCompositor::GetLastPoses` into a pose array owned by the
  controls write (the poses of the latest `WaitGetPoses`, standing
  universe as the compositor uses it); the hand is the device at the
  right-hand role's index (5.8.3), its raw device pose (not the grip pose
  action); the hand's `bPoseIsValid` must be set and its index valid,
  otherwise false. The head is device 0 of the same array.
- **OpenXR**: the right hand's grip pose (5.8.4) as located at the frame's
  `predictedDisplayTime` in the base space, which must be located (both
  valid bits), otherwise false; the head is the VIEW space located in the
  same base space at the same time (09 §5.3).

The test only affects Interact (it adds the flashlight) and is evaluated
on every controls write.

### 5.11 Hand poses for later use

v1 does not use hand poses in the game, but the mod must already read them
so v2 can (00 §1.1). Both runtimes provide, per hand, the grip pose (palm
origin) and its linear velocity, valid or not:

- **OpenVR**: right after each `WaitGetPoses` (before the controls write),
  `IVRInput::GetPoseActionDataForNextFrame` for the two pose actions
  (5.8.2), in the compositor's tracking universe
  (`IVRCompositor::GetTrackingSpace`, read once at start). A pose is valid
  when the call succeeds, the data is `bActive` and `bPoseIsValid`. When
  controller inputs are bound but a hand pose is not (a custom binding
  saved before the pose actions existed), log once that the binding lacks
  the hand poses and should be reset to the default. Without SteamVR Input
  no hand poses are available.
- **OpenXR**: the grip poses and velocities of 5.8.4, located with the
  frame's views and published together with them.

Converted to the game's convention like the head pose (03 §1: negate the X
and Y basis columns, negate the translation and multiply by the world
scale `s`), and velocities multiplied by `-s`. The poses cross from the
render thread to the game thread as one consistent pair (both hands, with
or without a lock-free sequence lock); a game-thread reader gets the hands
that belong to the head pose the game read for the frame it is building
(the hands published with the views of that head pose), or the newest when
no head pose was read since the last slot 6 (menus). Under OpenVR the head
pose the game reads comes from the compositor's last poses (03 §6.3),
which belong to the new `WaitGetPoses` before that call's hand poses are
read; a game-thread read in that window must still not pair the new head
pose with the hands of an earlier `WaitGetPoses`. How (for example by
publishing the head pose the game reads together with the hands) is the
implementation's choice.

With `DebugOptions` on (06), every 900 frames on the render thread, log
each hand's offset from the head in head coordinates (+x right, +y up, -z
forward, metres) or why it is untracked. This is how hand tracking is
checked from the log without asking the player.

### 5.12 States and edge cases

| Situation | Required input behaviour |
|---|---|
| Before the first frame | OpenVR: the slot 5 fallback writes the controls. OpenXR: VR state is zero until the first OpenXR frame begins |
| A repeated CreateDevice call (not expected: the game calls it once per process, 02 §6.1; a display reset does not call it) | the runtime and its input set-up are not re-created: no second manifest write, no second OpenXR action set; the pad hooks and remap state are unaffected; the next pad query with a raw code remaps if the game rebuilt its maps |
| Game window loses OS focus | VR controls and the gamepad: no change, the runtime, not the desktop window, decides input focus. While a headset is attached the focus loss does not pause the game (03 §9.6). Keyboard and mouse stop reaching the game until its window has the focus again, because Windows sends them only to the foreground window |
| SteamVR dashboard or another overlay takes input focus | all VR state zero (5.1) while it lasts; the toggled grip states keep their value |
| Headset taken off | input keeps being read as the runtime reports it (OpenVR keeps delivering controller input; OpenXR as long as the session stays focused). Automatic recenter requests wait until the headset is in use again (8) |
| OpenXR session leaves `FOCUSED` (`VISIBLE`) | states read inactive: all zero |
| OpenXR session not running (`STOPPING`, `IDLE`) or lost, or exit requested | all VR state zero; no further reads. The two toggled grip states (5.6) keep their value: the grips read released, which is not a rising edge |
| Controller turned off or lost | its inputs read inactive / zero; the other hand keeps working |
| Controller type changes during a run | the game-action scheme follows on the next write (5.5); the glyph set does not change (5.5) |
| VR runtime changed in the options menu | takes effect at the next launch (06 §4.5); nothing in input changes during the run |
| Real gamepad plugged in or removed | merged per action (3.3); the prompts follow the source with activity (4.2) |

## 6. Snap turn

Runs in the mod's per-frame update on the game thread (05 §3.2c), only
while the stereo device is running (02) and the active pad device exists.

Input: `turn` = the merged value (3.3) of map 0's turn action (analog
action 3, its virtual code derived from the map, 3.1), with `negate` 0, on
the active pad device: the larger-magnitude of the real right stick and
the VR turn value of 5.8.5.

Snap turn applies only while all of these hold: the first-person camera is
active (2.7, tracked through its activate and deactivate slots),
`Input.SnapRotation` is on, and the stereo mode is not 1 (no menu on the
virtual screen). Required results:

- **A step.** Each step adds `step` to the camera yaw (+0x204) when
  turn > 0.5, or subtracts it when turn < -0.5 (the 0.5 threshold and its
  reason are those of 05 §3.2c), wraps the result into [-1, 1] (above 1
  subtract 2, below -1 add 2), and writes the same value to +0x1FC, as the
  game's own yaw setter does when asked to set both (2.7). A positive turn
  value (stick or pad to the right) turns the player to the right [likely:
  same sign as the game's own stick look, which receives the same value
  when snap is off].
- **When steps happen.** Deflecting the stick past 0.5 steps at once. While
  it stays deflected past 0.5 with the camera active, a further step follows
  every 300 ms, measured with a monotonic clock (project decision: a held
  stick keeps turning at about three steps a second, slow enough to release
  after one extra step or to stop on the step wanted, fast enough to turn
  around in under two seconds at 30° steps; 05 §3.2c refers here). Returning
  inside 0.5 lets the next deflection step at once. While the camera is
  inactive, a held stick does not step, and releasing it re-arms snap turn
  as above. This matches 05 §3.2c.

Step size: yaw is in half-turn units, so **step = 2 / segments**, computed
in floating point. `segments` (`Input.SnapRotationSegments`) is one of
24, 12, 6, 4, 3, 2 (15°, 30°, 60°, 90°, 120°, 180°; 06 normalises other
values by rounding down to the next value in this list, and values below 2
to 2). Every value gives exactly its angle: an earlier mod turned 180° at
3 segments and failed below 2, which must not happen.

## 7. Game look enabling

The mod owns FollowBehavior +0x264 (look input enabled), written in its
per-frame update (05 §3.2a, d), only while a FollowBehavior is active:

- Stereo device not running (VR off, failed, or not yet tracking): set it
  to 1 so the game's normal look works.
- Otherwise set it to 1 when `Input.SnapRotation` is off or keyboard and
  mouse are in use, and to 0 otherwise.

Whether keyboard and mouse are in use follows 05 §3.2b: keyboard activity
(the keyboard's AnyKey, 2.2, while the keyboard device exists) or mouse
activity (the mouse's Active, while the mouse device exists) in a frame
makes it true, also when the pad or VR controllers show activity in the
same frame; pad or VR activity alone (the replaced HasActivity of 3.3)
makes it false; no activity leaves it; it starts false. With snap off, the
game's own smooth look runs from the right stick or the VR turn value;
with snap on, look only comes from the head (and the mouse while keyboard
and mouse are in use).

## 8. Recentering inputs

Recentering (taking the yaw-only reference pose, 03 §2) is triggered by:

- the chord: LB and RB held together on a real pad, read with the
  **stock** IsDown and raw codes 0x0A / 0x0B on the active pad device (the
  replaced method ignores raw codes), or left Ctrl and left Alt held
  together (KeyDown 0x1D and 0x38, whenever the keyboard device exists);
- a pending automatic request (03 §2.2: device created with the headset not
  in use, a loading screen closed), executed on the first update where the
  headset is in use (03 §2.3);
- not a recenter by the VR runtime: an OpenXR change of the `LOCAL`
  reference space only re-anchors the mod's base space (09 §6.3) and does
  not move the game's reference; OpenVR has no such request in v1.

The chord recenters once per press; holding it does not repeat; releasing
it re-arms it (05 §3.2e). A pending automatic request takes precedence over
the chord in the same frame. After any recenter the virtual screen is
re-placed in front of the player on the next two updates (05 §3.2e, 03 §7);
a new recenter during them starts the two re-placements again. The game's own
LB+RB recenter in its stereo update is not run (its update is replaced),
and the pause menu's "reset view" is routed to the same recenter (05 §8).

When OpenXR runs without a stereo device (virtual screen only, 09), Ctrl+Alt
is read from the OS keyboard state (`GetAsyncKeyState` of `VK_CONTROL` and
`VK_MENU`) on each Present and, on the press, recenters the OpenXR base
space (09 §6).

## 9. Known problems and robustness requirements

Problems observed with an earlier mod that this spec fixes:

| Observed effect | Required |
|---|---|
| Snap turn of 180° at 3 segments, failure below 2 | step = 2 / segments over the normalised set (6) |
| Controls stayed latched while the Oculus dashboard had focus | all VR state zero without focus (5.1) |
| Under OpenVR every controller played as a Vive wand | the thumbstick scheme for every non-wand controller (5.3, 5.4) through SteamVR Input default bindings; the wand treatment only for wands and as the legacy fallback (5.8.3) |
| Controls lagged the rendered poses by a frame | handed to the game right after the runtime's frame wait (5.9) |

Robustness requirements (user requirement: bounds safety):

- No pad method reads outside the mod's per-action state, whatever code
  arrives; an action of the wrong kind gets no VR contribution (3.3).
- `ControlToName` never fails, for any code (3.3).
- A binding record is added only to an action with fewer than 6 records
  (3.2).
- Codes and indices are derived from each map's counts, never assumed
  (3.1).
- Every analog value handed to the game stays within [-1, 1] (0 to 1 for
  triggers), scaled or not (5.1).
- The five pad slots, the connected flag and the left-stick redirect are
  applied together or not at all (3.3).
- The prompt redraw writes no game field through a null device pointer
  (4.2).
- The near-head test uses the render thread's own poses, never the head
  pose the game thread reads (5.10).
- The first-person camera is not written after its deactivation, since it
  may be freed (2.7).

## 10. Acceptance

Each check names what to do and what must be seen. "Log" means the mod's
log file; the wording and format of each logged event is defined in
`11-diagnostics.md`, this document only says which events are logged.

Start-up and game interface:

1. Runtime None: no pad hook log line; a real pad works and shows stock
   prompts.
2. Runtime OpenVR or OpenXR on the 2015 build: the log reports the pad
   vtable hooked, and within seconds of the title screen one line with the
   three maps' digital/analog counts, which reads 41/6, 19/6, 32/8.
3. Pull the XInput pad out (or play without one): VR controllers still
   drive the game (connected-flag patch).
4. Preflight: with one of the checked bytes altered (for example the
   `setne cl` at 0x60EE6F), or with a test build that expects a different
   function in one of the five pad slots (2.3), nothing is patched and the
   log names the failed check.

SteamVR Input (OpenVR):

5. The log names SteamVR Input with the number of actions the manifest
   declares (the inputs of 5.8.1, hand poses included) and the manifest's
   path; the folder holds the manifest and four binding
   files, rewritten (new modification time) at each launch.
6. SteamVR's binding UI lists the game with the inputs of 5.8.1 and
   shows the default binding for the connected controller.
7. Index: left stick moves, click sprints; right stick turns in 30° steps
   (default snap) with repeat about every 300 ms while held; right stick up
   / down selects next / previous weapon; right stick click opens the
   radial menu; A interacts; B reloads and goes back in menus; left A opens
   the map; left B toggles the flashlight; A with the right controller
   within 20 cm of the headset toggles the flashlight; left trigger
   crouches; right trigger fires; right grip aims; left grip shows the
   motion tracker; pressing the left trackpad opens the pause menu.
8. Prompts show the thumbstick set's glyphs (the right lower face button
   for interact, the left lower face button for map, the menu button for
   pause) and the controls help page shows the Index variant (pause on the
   left trackpad press). With a Vive wand as the first right-hand
   controller: the touchpad set's glyphs and entries.
9. Open the SteamVR dashboard while holding the trigger and pushing the
   stick: the player stops firing and moving at once, and nothing reacts
   until the dashboard closes.
10. Remove SteamVR Input (or make the manifest folder read-only): the log
    says why and that the legacy path is used; a wand still plays with the
    touchpad scheme.

OpenXR:

11. The log reports the actions attached, then the right hand's profile
    (`/interaction_profiles/valve/index_controller` with Index on SteamVR);
    `xrSyncActions` result 0 while playing and 8 with the dashboard open,
    each logged once per change.
12. Index under OpenXR plays as in check 7, except that the pause entry of
    the controls help page names the menu button.
13. Dashboard open: all controls released, as in check 9.

Common:

14. Toggle grips on: one squeeze of the right grip keeps aiming until the
    next squeeze; off: aim only while held. The left grip likewise for the
    tracker.
15. A real Xbox pad together with VR controllers: both act; using the pad
    switches prompts to the stock pad glyphs, using a VR controller
    switches them back (the prompts redraw).
16. `Input.SnapRotationSegments` 3: each step is 120°; 2: 180°; 24: 15°. No turning while
    a menu is on the virtual screen.
17. `Input.SnapRotation` off: the right stick turns smoothly and pitch stays with
    the head; snap on and the mouse moved: mouse look works until a pad or
    VR control is used.
18. LB+RB (real pad) or Ctrl+Alt: one recenter per press, logged; holding
    the chord does not repeat it; an open menu is placed in front of the
    new forward.
19. Wands (touchpad scheme), snap on: resting the thumb on the right pad
    does not turn; clicking its left or right edge turns once; clicking its
    lower half interacts, upper half reloads, centre opens the radial menu.
20. With `DebugOptions` on, every 900 frames the log shows both hands'
    head-local offsets, which change as the controllers move.
21. The interact prompt for "look" and hacking shows the left-stick text
    when movement is on the left stick (stock layout).
22. Wands (touchpad scheme): with the thumb at the rim of the left pad on a
    diagonal, the player walks no faster than with the thumb at the rim
    straight ahead; the move values the game receives never form a vector
    longer than 1.
23. With a headset attached, click on another desktop window during play:
    the VR controllers and a real pad keep driving the game; keyboard and
    mouse do nothing until the game window is clicked again (03 §9.6).
24. Runtime None, and runtime OpenVR with SteamVR not able to start: the
    controls help page shows no controller layout (5.7).
25. Runtime OpenVR with the options archive not in use (a `UI.PAK` the mod
    rejects, 14 §5.9): prompts show the game's own pad glyphs, never an
    empty button.
26. Keyboard and mouse in use, then a VR controller used: the prompts
    switch from keyboard prompts to pad-style prompts with VR glyphs in the
    same moment the controller is used, also when the mod's own activity
    check runs before the game's device selection in that frame (3.3).
27. In a test build that fails to write the fourth pad slot after the
    checks passed: none of the pad slots, the connected flag or the
    left-stick redirect is left applied, the log says the controls area is
    off, and a real pad plays as stock.

## 11. Open questions

- AI.exe names of the three maps, and the game function of the actions
  kept for parity with their function unconfirmed (map 0 0x27 / 0x28, map 1
  1, 9, 0x0B, 0x0E; 2.4). Until resolved they are driven as 5.8.5 says; once
  a function is known, the spec states it, or drops the action if it does
  nothing useful in VR. To resolve: string references near the map
  constructors (0x597750, 0x5978B0) or the input-mapping screen's labels.
- Meaning of the composite list and its +0x21 flag. To resolve: break on
  writes to map+0x1038 and inspect the codes of a composite entry with a
  pad preset loaded.
- The binding records' first dword, and the flag bits other than 0x40
  (2.4): what bit 0x80 does after a device reports a control down (the
  digital evaluation at 0x586D80 tests it), what the flags of analog
  records mean (the analog evaluation was not traced), and whether the game
  ever builds a pad record with bit 0x40 set (3.2 item 2). A lead
  [guess]: code between 0x595EB1 and 0x596190 ORs 0x40, 0x20, 0x80, 0x30
  and 0x10 into a byte at +7 of a record and may be where the game builds
  binding records from `DATA\INPUT.XML`; to resolve, confirm that and
  trace which bindings lead to each bit.
- In which order the game's device selection (2.5) and the mod's
  per-frame update (05 §3.2) run within a game frame. 3.3 requires VR
  activity to reach both in either order; to confirm in game, log the
  order of the two calls once.
- Whether any binding is evaluated through GetAxis with `negate` set; if so
  the VR value may need the same negation. Movement, look and menu axes
  behave correctly in game with the VR value not negated, so any such
  binding is not one VR drives; to resolve, log the `negate` argument per
  code once.
- Whether FollowBehavior +0x1CC is in world space like +0x1C0: slots 6
  and 8 take the `Head1` transform with a mode argument of 7 that was not
  decoded (2.7). To resolve: decode the transform getter's mode, or log
  +0x1C0 and +0x1CC together while standing still in game (a difference of
  about the eye height means both are world positions).
- Whether SteamVR Input reports actions inactive (rather than holding
  their last state) while the dashboard has focus, and what legacy
  `GetControllerState` returns then. To resolve: acceptance check 9 under
  each path; if the legacy path keeps state, gate it on
  `IVRSystem::IsInputAvailable`.
- 2020 build: every "?" above (device selection, IsMoveOnLeftStick, the
  look-apply slot, the camera update and yaw setter, map constructors,
  keyboard and mouse vtables), every 2020 value from the earlier mod's
  table, and every structure offset of section 2 (assumed equal to 2015)
  need checking against a 2020 `AI.exe`.
- WMR and Touch defaults are not verified on hardware; under OpenXR, WMR
  has no reload input (5.4), a candidate for a binding change once
  verified.
