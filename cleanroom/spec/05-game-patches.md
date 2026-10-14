<!-- Provenance: static analysis of AI.exe (2015 Steam build, verified against the binary; 2020 RVAs from the per-build address analysis, not verified against a 2020 binary), required behaviour restated from observing MotherVR 0.8.1 in game, in-game testing of an earlier rebuild, user requirement (automatic recenter on headset-on and gameplay start); AI.exe 2015 analysis of the mode-stack container (answer to Q5, ANSWERS.md). Revision: r2 -->

# 05: Changes to game behaviour

Every change the mod makes to how `AI.exe` behaves, other than creating the
stereo device (`02-game-stereo-path.md`), sizing and submitting frames
(`03-hmd-and-frames.md`) and feeding input (`04-input.md`). For each change:
what the game does, what must happen instead, and why.

Addresses are RVAs (image base 0x400000), given as **2015 / 2020**. "2020: ?"
means unknown. Bytes written inline are the 2015 originals and were checked
in the 2015 `AI.exe`; the 2020 build is assumed to hold the same instruction
at its RVA, and the implementation must check the original bytes before
writing and skip (and log) a site that does not match.

Confidence: **[sure]** the game code shows it directly; **[likely]** strong
inference; **[guess]** plausible, must be confirmed in game. Each [guess] and
[likely] item says what to look for.

## 1. Game objects and conventions used below

| Object | Where | Facts |
|---|---|---|
| Stereo object | global pointer at 0x1366A2C / 0x1376BDC | Layout in `02`. Fields used here: `+0x04` device pointer, `+0x20` 4x4 virtual-screen world matrix, `+0x60` current stereo mode (int), `+0x64` scene visibility (float, 1 visible, 0 black), `+0x68` mode stack data pointer, `+0x6C` mode stack count, `+0x78` 3x4 reference (recenter) transform, `+0xA8` fade target (bool), `+0xAC` fade alpha (float). |
| Stereo device | stereo `+0x04` | `+0x04` state: 0 none, 1 initialised, 2 tracking running. See `02`. |
| POV manager | global pointer at 0x1366A00 / 0x1376BB0 | `[mgr+8]` points to the main view (POV 0); its first 64 bytes are the world-to-view matrix. |
| Input manager | global pointer at 0x1359B44 / 0x1369CF4 | `+0x4A70` mouse, `+0x4A74` keyboard, `+0x4A7C` gamepad device (may be null). See `04`. |
| Keyboard key test | 0x60EB20 / 0x560FE0 | thiscall on the keyboard object, one int arg (DirectInput scan code), `ret 4`, returns bool. |
| Keyboard any-key | 0x60EE00 / 0x5612D0 | thiscall, no args, returns bool: some key is down. |
| Mouse active | 0x9BBE70 / 0x4C87E0 | thiscall, no args, returns bool: the mouse moved or a button is down. |

**Stereo modes.** Mode 0: the 3D world, with the 2D UI (HUD) drawn on a quad
locked to the head. Mode 1: the "virtual screen", the whole 2D UI (menus,
videos) drawn on a quad placed in the world, with the 3D scene behind it.

**View matrix convention [sure].** The game's world-to-view matrix is 16
floats, row-vector (D3D) convention, left-handed, units in metres. Columns
0, 1, 2 (elements [0,4,8], [1,5,9], [2,6,10]) are the camera's right, up and
forward axes in world space; row 3 holds the view translation. The camera's
world position is therefore `-(row3 · column0, row3 · column1, row3 · column2)`
(row3 dotted with the first three elements of rows 0, 1, 2 respectively).
World up is +Y.

A UI quad "world matrix" uses the same convention: rows 0, 1, 2 are the quad's
right, up and normal axes in world space, already scaled; row 3 is its centre.

**FollowBehavior [sure].** The first-person camera behaviour (vtable at
0xDC6600 / 0xDDD6E8; its name method, slot 14, returns "FollowBehavior").

| Field / slot | Meaning |
|---|---|
| `+0x204` | yaw, normalised: ±1.0 = ±180°, wraps at ±1 |
| `+0x1FC` | mirror of the yaw; must be written with the same value |
| `+0x208` | pitch, clamped by the game |
| `+0x260`, `+0x261` | look-integration enable bytes (see 7.4) |
| `+0x264` | look input enabled: the game applies stick/mouse look deltas (slot 11, 0x2D200) only while this byte is set |
| slot 0 (vtable +0) | OnActivate(arg): 0x2C930 / 0x3C1A0, thiscall, one stack arg, `ret 4` |
| slot 1 (vtable +4) | OnDeactivate(arg): 0x2BBF0 / 0x3B340, same ABI |

## 2. Force the eight "Stereo Mode == Rift" checks true [sure]

**Game.** Graphics setting 0x18 is "Stereo Mode". The game's dormant HMD
path (`02`) runs only where the current value equals the "Rift" option. Each
check has the same shape: fetch setting 0x18 (settings lookup at 0x5A4D00),
load its current value index into a register, call the stereo object's
"index of the Rift option" method (0x5BE350, thiscall, returns it in EAX),
then a 2-byte register-to-register compare `cmp reg, eax`, followed by
`jne`/`je`/`sete`.

| # | 2015 | 2020 | Original bytes (2015) | What it gates |
|---|---|---|---|---|
| 0 | 0x336B6 | 0x42F96 | `3b f8` | `sete bl`: a stereo-on flag outside the renderer |
| 1 | 0x60FD58 | 0x562238 | `3b d8` | the render-target size query (call at 0x60FD64) |
| 2 | 0x60FE50 | 0x562330 | `3b f0` | the display size query (call at 0x60FE5D) |
| 3 | 0x60FEBD | 0x56239D | `3b f0` | display resolution setup |
| 4 | 0x60FF15 | 0x5623F5 | `3b f0` | display resolution setup |
| 5 | 0x611740 | 0x563C10 | `3b f0` | renderer setup |
| 6 | 0x61378A | 0x565C5A | `3b f0` | stereo device creation (`02`) |
| 7 | 0x613C80 | 0x566150 | `3b d8` | `sete al` stereo flag in renderer setup |

**Change.** Make every compare report "equal" whatever the setting holds, for
example by replacing the two bytes with `39 c0` (`cmp eax, eax`), which sets
the flags as for equality and is the same length.

**Why.** The setting cannot select a modern headset, and the whole stereo
path (double-wide render target, per-eye rendering, the UI overlay, the mode
stack) hangs off these eight checks. Forcing them is what turns the path on.

## 3. Per-frame stereo update

### 3.1 What the game does [sure]

The stereo object's update (0x5BE2A0, thiscall, one float arg = frame time,
`ret 4`) is called once per frame from 0x3B4100 / 0x3C4FB0 (a 5-byte
`call rel32`). It:

1. if the device state is 2 and the gamepad's LB and RB are both held, calls
   device slot 11 (vtable `+0x2C`) with a pointer to stereo `+0x78`, which in
   the DK2 implementation takes the head yaw and writes the reference
   transform there (the "recenter");
2. animates the fade alpha `+0xAC` towards `+0xA8` by the frame time,
   clamped to [0, 1].

### 3.2 Required replacement

Replace the call at the site above with the mod's own update (same ABI:
thiscall, ECX = stereo object, one float on the stack, callee pops 4 bytes).
The game's update is not called. The frame-time argument is not needed and
the fade alpha `+0xAC` is left at its initial value (`02`).

The mod tracks the active FollowBehavior (section 6.1) and keeps this state
across frames: snap latch and its timestamp, keyboard/mouse-in-use flag,
"recentered during this chord" flag, a re-place counter, a pending
automatic-recenter request, the loading-blackout flag (section 5), and the
cutscene flags (section 6.2).

In order, every frame:

**a. Not running.** If the stereo object has no device, or its state is not 2:
set the tracked FollowBehavior's `+0x264` to 1 (if one is tracked) and do
nothing else. With no headset tracking, the game's own look must work.

**b. Read inputs.** If the gamepad device exists: read LB (button 10), RB
(button 11) and R3 (button 15); if the VR controllers produced any input this
frame, clear the keyboard/mouse-in-use flag; read the turn axis (the axis the
VR controllers drive for turning, see `04`). If both keyboard and mouse
devices exist: read Left Ctrl (scan code 0x1D) and Left Alt (0x38) with the
key test; if any key is down or the mouse is active, set the
keyboard/mouse-in-use flag.

**c. Snap turn.** The axis is "deflected" when its value is below -0.5 or
above 0.5. When all of these hold: a FollowBehavior is active, the snap
rotation option is on (`06`), the axis is deflected, the current stereo mode
is not 1, and the snap latch is clear: add one step to the FollowBehavior yaw
(`+0x204`), subtracting for a negative axis and adding for a positive one,
wrap the result into [-1, 1] (add or subtract 2.0), write it to both `+0x204`
and `+0x1FC`, set the latch and record the time. The step is
`1 / (segments / 2)` in yaw units, so `segments` steps make a full turn
(`segments` is the snap-segments option, default 12, giving 30° steps; guard
against `segments / 2 == 0`). The latch clears when the axis returns inside
±0.5, or when it has been held deflected for more than 300 ms (a FollowBehavior
being active), which repeats the step about every 300 ms while held.

Why: smooth yaw from a stick in VR causes motion sickness; the yaw field is
the game's own first-person heading, so turning through it keeps movement,
aiming and animation consistent. No turning while a menu is on the virtual
screen.

**d. Game look.** Set the tracked FollowBehavior's `+0x264` to 1 if snap
rotation is off or the keyboard/mouse-in-use flag is set, otherwise 0.

Why: the game look (stick/mouse) adds yaw and pitch to the camera on top of
the headset. With snap turn on and VR controllers in use it must be off
(pitch comes from the head only; yaw from snap steps). With snap off, the
stick turns smoothly through the game's look. With mouse and keyboard the
player needs mouse look. See also 7.6.

**e. Recenter.** The chord is LB+RB held together, or Left Ctrl + Left Alt.
A recenter writes a yaw-only reference transform to stereo `+0x78`
(`03`/`04` define the transform).

- If an automatic request is pending and the headset is in use (below):
  recenter, clear the request, set "recentered", reset the re-place counter.
- Otherwise, if the chord is held and "recentered" is clear: recenter and set
  "recentered" (one recenter per press of the chord).
- Otherwise, if "recentered" is set and fewer than two re-places have been
  done since: rebuild the virtual-screen matrix (`+0x20`) with the placement
  of section 4.2 (inverse aspect 0.5625), and count it. This makes an open
  menu face the new forward; it is done on two consecutive updates because
  the new reference reaches the view matrix a frame later [likely].
- Otherwise, if the chord is not held: clear "recentered" and the counter.

R3 (button 15) held with the chord has no visible effect in MotherVR and need
not be implemented.

**Automatic recenter requests** (OpenMotherVR requirement, beyond MotherVR,
which only recenters once when the device is created, whatever the headset is
doing):

- when the device is created: recenter at once; if the headset is not in use
  at that moment, also leave a request pending, so the reference is taken
  again when the player puts it on;
- when a loading screen closes (section 5): leave a request pending, so
  gameplay starts with forward where the player faces;
- if the VR runtime itself asks for a recenter, treat it as a pending request.

"Headset in use": the runtime reports the user interacting with the headset
(OpenVR: `GetTrackedDeviceActivityLevel` for the HMD is
`k_EDeviceActivityLevel_UserInteraction`) and the head is upright (the world-up
component of the head's up axis greater than 0.7, about 45°). A pending
request waits as long as needed; it never fires on a headset lying on a desk.

**f. Mode stack bookkeeping.** If stereo `+0x6C` (stack count) differs from
`+0x60` (current mode), bring it to the current mode. With the stack kept one
deep (section 4.4), this keeps the count at 0 in mode 0 and 1 in mode 1 even
after unbalanced game pushes or pops.

The stack is a growable array of 32-bit ints owned by the game [sure]:
`+0x68` data pointer, `+0x6C` count, `+0x70` capacity. It starts empty (data
null, capacity 0). The game grows it only in its append routine, 0x294540 /
2020: ? (the `call` at PushMode + 0x53, 0x5D9C53), thiscall with ECX =
stereo `+0x68` and one argument, a pointer to the int to append, `ret 4`;
returns the element's address in EAX (null if allocation failed). When the
count equals the capacity it allocates a new array of twice the capacity (8
entries the first time) through the game's allocator, copies the old
entries and frees the old array.

- **Lowering** the count (1 to 0): write `+0x6C` directly.
- **Raising** it (0 to 1): append the value 1 through the game's append
  routine, never by writing the count alone: when the game entered mode 1
  without a push, the array may never have been allocated. Do it on the
  game's main thread (the per-frame update is), where the game itself
  pushes. The game's PushMode holds a lock around its append; the
  per-frame update runs on the same thread as every push and pop, so no
  further locking is needed [likely].
- If the 2020 address of the append routine is unknown, raise the count
  only when data is non-null and the capacity is at least 1 (write element
  0 = 1, then the count), otherwise leave it and log once.

**g. Scene visibility.** The game writes `+0x64` every frame from the
head-in-wall test (7.2). After that:

- While a cutscene or level fade is running (section 6.2) and the mode is not
  1, set `+0x64` to 1.0. Cutscene cameras are not where the player's body is,
  so the head-in-wall test would black the view out [likely].
  While the flag is set, if no FollowBehavior is active, note that the
  cutscene has taken the camera; once a FollowBehavior is active again after
  that, clear both flags (the cutscene ended even if no finish message came).
- While the loading-blackout flag is set, set `+0x64` to 0.0 (black scene
  behind the loading screen; this wins over the cutscene rule).

## 4. Placing the UI quads, and the stereo mode stack

### 4.1 The game's quad builder [sure]

0x5BE550 / 0x510710 builds a UI quad matrix:
`Matrix44* builder(Matrix44* out, float size, float invAspect, float dist)`,
**cdecl** (caller pops 0x10), returns `out` in EAX. The callers copy the 64
bytes **through the returned pointer**, so a replacement must return `out`.
The game builds: row 0 = camera right × size, row 1 = camera up × size ×
invAspect, row 2 = camera forward, row 3 = camera position + forward × dist,
using the full camera orientation from POV 0.

It is called from two sites, both 5-byte `call rel32`:

| Site | 2015 | 2020 | Game arguments | Result stored in |
|---|---|---|---|---|
| SetMode(1) (0x5CD0A0): the virtual screen | 0x5CD132 | 0x51F2A2 | (3.5, UI target height / width, 3.0) | stereo `+0x20`, once per entry into mode 1 |
| overlay draw (0x5CD170), mode 0: the HUD | 0x5CD2EE | 0x51F45E | (0.8, inverse aspect, 1.0) | a local, rebuilt every frame |

Redirect both calls to mod functions with the same ABI. They ignore `size` and
`dist`, use `invAspect` as given, read the current view matrix of POV 0, and
must cope with a null POV manager (return identity).

The quad's size in metres is taken here as the length of row 0. [likely:
confirm in game that a row-0 length of 1.0 gives a quad 1 m wide; if the
quad's local extent is not a unit square, scale the sizes below so the
observed sizes match.]

### 4.2 Virtual screen (mode 1): level, yaw-only, world-fixed

- Forward: the camera forward projected onto the horizontal plane
  (`(fx, 0, fz)` normalised); if its length is near zero, use `(0, 0, 1)`.
- Up: world up `(0, 1, 0)` × invAspect. Right: `up × forward` for left-handed
  axes, i.e. `(fz, 0, -fx)` (unit length).
- Width 1.0 m, height = invAspect m (0.5625 m for a 16:9 UI).
- Centre: camera position + horizontal forward × 1.5 m, at camera (eye) height.

The matrix is built when the game enters mode 1 and when a recenter re-places
it (3.2e), and is not updated while the menu is up, so the screen stays fixed
in the world while the head moves.

Why: the game's placement copies the full head orientation at the moment the
menu opens (tilted if the player looked down) and is 3.5 units wide at 3 m. A
level screen at reading distance, facing where the player looks, is
comfortable and does not follow the head.

### 4.3 HUD panel (mode 0): head-locked

- Axes from the current view matrix: up = column 1, forward = column 2,
  right = up × forward (left-handed).
- Row 0 = right × 0.55, row 1 = up × 0.55 × invAspect, row 2 = forward,
  row 3 = camera position + forward × 1.0.
- So a panel 0.55 m wide, 1 m in front of the point between the eyes, that
  follows head yaw, pitch and roll. Rebuilt every frame (by the game's call).

Why: 0.8 at 1 m covers too much of the view in a modern headset; the HUD
elements (motion tracker prompts, interaction hints, subtitles) must be
readable without filling the field of view.

### 4.4 Stereo mode stack [sure for the game side]

Game functions (both thiscall on the stereo object):

- PushMode(int mode) 0x5D9C00 / 0x52BE00, `ret 4`: calls SetMode(mode)
  (0x5CD0A0), then appends `mode` to the stack (`+0x68` data, `+0x6C` count)
  under a lock.
- PopMode() 0x5D9C80 / 0x52BE80, plain `ret`: if the count is non-zero,
  removes the top and calls SetMode with the new top, or 0 if empty.

Call sites, each a 5-byte `call rel32` with ECX = the stereo object:

| Kind | 2015 | 2020 | Context |
|---|---|---|---|
| push | 0x40EB8E | 0x41FD4E | pushes 1 |
| push | 0x43EECA | 0x45008A | pushes 1 (a hub-type menu screen) |
| push | 0x44197A | 0x452B2A | front-end menu Open |
| pop | 0x413DDA | 0x42508A | front-end menu Close |
| pop | 0x416382 | 0x4275D2 | (owner not identified) |
| pop | 0x424C2A | 0x435DCA | (owner not identified) |

**Required.** Redirect the three push sites to a function with PushMode's ABI
that: if the requested mode is 1 and the current mode is 0, calls the game's
PushMode(1); otherwise calls the game's PopMode() and then the game's
PushMode(mode). Redirect the three pop sites to a function with PopMode's ABI
that calls the game's PopMode() only when the current mode is 1.

Result: the stack never grows beyond one entry, every push re-runs SetMode(1)
and so re-places the virtual screen in front of the current view, and a pop
in mode 0 does nothing. Together with 3.2f and the menu wrappers (section 5),
the game is in mode 1 exactly while a full-screen menu, loading screen or
title video is up.

Why: the game's menus push and pop unevenly (nested screens, pops on resume
without a matching push); with the stack as written the game could stay in
the virtual screen during gameplay or show a menu in mode 0.

**Two stray stack operations are removed:**

| Site | 2015 | 2020 | Original (2015) | Change |
|---|---|---|---|---|
| PopMode in the "Resume_game" handler (function at 0x4535D0) | 0x453682 | 0x4649B2 | 11 bytes: `mov ecx,[stereo object]` + `call PopMode` (`8b 0d 2c 6a 76 01 e8 f3 65 18 00`) | skip both instructions (e.g. a short jump over them) |
| PushMode(1) in the pause menu's "Pause" handler (pause-menu vtable slot 3, 0x43E520) | 0x43E545 | 0x44F705 | 13 bytes: `mov ecx,[stereo object]`, `push 1`, `call PushMode` (`8b 0d 2c 6a 76 01 6a 01 e8 ae b6 19 00`) | skip all three |

Why: the pause and front-end wrappers (section 5) now own entering and
leaving mode 1 for these screens; the game's own calls would unbalance it.

## 5. Menu objects

All methods below are thiscall with no stack arguments and a plain `ret`.
Replace the vtable slot with a function of the same ABI that calls the
original with the same ECX first, then does the added work. Vtables are in
`.rdata`; the slot must be made writable to patch it.

| Menu | Vtable 2015 / 2020 | Slot 0 Open (original) | Slot 2 Close (original) | Recognisable by |
|---|---|---|---|---|
| Front-end menu | 0xF000E0 / 0xF17840 | 0x441950 / 0x452B00 | 0x413DD0 / 0x425080 | movie methods playBackgroundMovies, setLanguage, stop_front_end_music |
| Pause menu | 0xF00180 / 0xF178E0 | 0x441D90 / 0x452F40 | 0x4155E0 / 0x426890 | setItemVisible, setChallengeOrPreorderEnabled; slot 3 handles "Pause" |
| Loading screen | 0xF00218 / 0xF17978 | 0x441EA0 / 0x453050 | 0x415670 / 0x4268D0 | setGameplayTip, setState |
| Attract / title video | 0xEFCC80 / 0xF143E0 | 0x40B3A0 / 0x41C610 | 0x669C90 / ?: a shared empty method | playAttractVideo |

"Enter mode 1" below means: if the current mode is not 0, call the game's
PopMode(); then call the game's PushMode(1). "Leave mode 1" means: if the
current mode is 1, call the game's PopMode(). (These call the game functions
directly, not the redirected sites.)

| Menu | On Open (after the original) | On Close (after the original) |
|---|---|---|
| Front-end | push the mod's settings into the menu's UI (`06`). Mode 1 is entered by the game's own push at 0x44197A (4.4). | leave mode 1 |
| Pause | enter mode 1; push the settings (`06`) | leave mode 1 |
| Loading screen | enter mode 1; set the loading-blackout flag; push the settings (`06`) | clear the loading-blackout flag; leave mode 1; leave an automatic recenter request (3.2e) |
| Attract / title video | enter mode 1 | do not call the original (it is empty and shared with other classes); leave mode 1 |

Effect: menus, the loading screen and the title video appear on the level
virtual screen 1.5 m ahead, placed when they open; behind a loading screen
the 3D scene is black, because the world behind it is half-loaded and the
camera may jump. The settings push lets the options page show the mod's
current values (`06`).

## 6. Watching the game's camera and cutscenes

### 6.1 Tracking the active FollowBehavior [sure]

Replace FollowBehavior vtable slots 0 and 1 (2015: the dwords at 0xDC6600 and
0xDC6604; 2020: at 0xDDD6E8 and 0xDDD6EC) with functions of the same ABI
(thiscall, one stack argument, `ret 4`, return value passed through) that
record `this` as the tracked FollowBehavior, set "active" (OnActivate) or
clear it (OnDeactivate), and call the original with the same argument.

Why: the stereo update needs the player's camera behaviour to turn it, to
gate the game's look, and to tell gameplay from cutscenes. There is one
first-person camera, so the last one activated or deactivated is the one
that matters.

### 6.2 Cutscene and level-fade messages [sure]

The UI entity class's message handler is vtable slot 2: the slot is at
0xDFCE18 / 0xE13F38 (vtable at 0xDFCE10 in 2015), original handler 0x51BD70 /
0x6241A0. ABI: thiscall(this, const ShortGuid* message, a, b, c), `ret 0x10`;
the first dword the message pointer points to is the hashed message name.

Four globals hold the names' hashes. They are computed at start-up by static
initialisers, so read them when comparing, not at install time:

| Message | Global 2015 | Global 2020 |
|---|---|---|
| `cutscene_visibility_start` | 0x134DC0C | 0x17F6B84 |
| `cutscene_visibility_finish` | 0x134DC18 | 0x17F6B90 |
| `level_fade_start` | 0x134DD1C | 0x17F6C94 |
| `level_fade_finish` | 0x134DD28 | 0x17F6CA0 |

Replace the slot with a function of the same ABI that sets the
cutscene-or-fade flag on either start message, clears it on either finish
message, and always calls the original with all arguments, returning its
result. Use the handler's own first argument as the message.

Why: during cutscenes and level fades the camera is driven by the game, not
by FollowBehavior, and the head-in-wall test (7.2) would black out the view
(3.2g).

## 7. Byte patches

### 7.1 SSAO never runs [likely]

The SSAO setting handler (0x575400, thiscall, one arg = setting event,
`ret 4`) handles setting 0x15 "Screen Space Ambient Occlusion": for the value
index `v` it stores `+0xD80` (which AO texture the resolve uses) and `+0xD84`
(byte: run the AO compute pass). Originally v=1 → (1, 1), v=0 → (0, 1),
v=2 → (2, 0).

**Change.** At 0x575421 / 0x4C71D1, the `jne` after the `v == 1` compare
(`75 10`) becomes an unconditional jump to the v=2 store (`eb 29`), which
stores `+0xD80 = v` and `+0xD84 = 0` for every value.

**Why.** Screen-space AO computed on the double-wide stereo target bleeds
across the eye boundary and differs between eyes, and it is costly at
headset resolutions. Verify in game: no AO darkening in corners, whatever the
option says.

### 7.2 Head-in-wall blackout follows the "clipping blinders" option [sure for the mechanics]

The VR camera behaviour ("VRCameraBehavior") computes a scene visibility from
the eye position and a body pivot (0x346DE0, stdcall(const vec3* eye,
const vec3* pivot), `ret 8`, result on the x87 stack); its caller (0x34E2C1)
stores it in stereo `+0x64`. Visibility is 1 when the eye is within 0.5 m of
the pivot, falling to 0 at 0.6 m; and if the distance exceeds a threshold, a
probe along the pivot-to-eye direction scales it down when geometry is in the
way. The threshold is a constant 1e-5 (bits 0x3727C5AC), read by
`comiss xmm0, [abs32]` at 0x346E94 / 0x35794E (`0f 2f 05` + the 4-byte
address, which starts at +3).

**Change.** Point the 4-byte address at +3 to a float the mod owns: 1e-5
(bit-identical to the game's) when the clipping-blinders option (`06`) is on,
1.0 when it is off. Update it when the option changes.

**Why.** With blinders on, the view goes black when the player's real head
moves into a wall, which prevents seeing through geometry. Some players
prefer no blackout; with 1.0 the wall probe only runs at distances where the
0.5-0.6 m falloff has already blacked the view out, so leaning into walls no
longer darkens the view. Verify in game with the option off and on.

### 7.3 No procedural camera sway or breathing [likely]

The camera update (0x260320) calls a function (0x24BF50) that blends in two
procedural noise channels from the character's animation component and writes
the camera's offset fields (`+0xE0`..`+0xEC`). The call is
`mov ecx, esi; call rel32` at 0x2603B1 / 0x270C61 (7 bytes:
`8b ce e8 98 bb fe ff`).

**Change.** Skip both instructions (e.g. `eb 05` over the remaining 5 bytes),
so the offsets stay at zero.

**Why.** Idle sway, breathing and handheld camera noise move the view without
the player moving their head, which causes nausea in VR. Verify: the view is
still when standing still with the headset still.

### 7.4 Look-integration branch kept on [guess]

FollowBehavior's state update (0x2D600) recomputes the bytes `+0x258`..`+0x263`.
At 0x2D83C / 0x3D0BC it sets `+0x261` from a saved earlier state
(`mov cl, [esp+0xF]`, bytes `8a 4c 24 0f`, followed by the store to `+0x261`).
The look/aim integrator (0x2BC50, called on activate and update) treats
"`+0x260` or `+0x261` set" as permission for one of its integration branches.

**Change.** Make the value stored to `+0x261` always 1 (replace the 4-byte load
with a load of the constant 1 into CL padded to 4 bytes).

**Why (guessed).** Probably "free look not locked by animation": keeps the
camera following the player's look state when the game would otherwise lock it
during some animations, which in VR would fight the head pose. Verify by
comparing with and without the patch in vents, ladders, hiding in lockers and
scripted grabs.

### 7.5 No DK2-era camera correction [likely]

VRCameraBehavior's update (0x34DE50) calls a correction function (0x34A050,
thiscall, four stack args: a value, two `vec3*` positions and `vec3* out`;
`ret 0x10`) at
0x34E1B9. It zeroes `out`, then computes a correction vector only if two
positions differ vertically by at most 0.999: the `jbe rel32` at
0x34A0CE / 0x35ACCE (`0f 86 2b 03 00 00`) skips to its epilogue.

**Change.** Make the branch unconditional: `e9 2c 03 00 00 90` (`jmp` to the
same target, then `nop`), so `out` is always (0, 0, 0).

**Why.** The game pushes the camera back against head displacement (a DK2
body/neck model). With modern 6-DoF tracking this moves the world against the
player's real head motion. Verify: leaning moves the view by exactly the head
motion.

### 7.6 FollowBehavior activation does not enable look input [sure]

OnActivate (0x2C930 / 0x3C1A0) sets `+0x264 = 1` with a 6-byte store at +8
(0x2C938: `88 86 64 02 00 00`, `mov [esi+0x264], al`). The constructor and
OnDeactivate clear it.

**Change.** Remove that store (six `nop`s), so the look-enabled byte is
controlled only by the stereo update (3.2a and 3.2d).

**Why.** Otherwise each activation (every cutscene end, every respawn)
re-enables stick/mouse look until the next stereo update, and the stick would
turn the view smoothly on top of snap turn.

### 7.7 Mouse cursor in a 1280x720 UI space [sure for 2015; 2020 byte likely]

The UI mouse update (0x443520) centres the cursor with
`movss xmm2, [W]` and `movss xmm2, [H]`, where W and H are the back buffer
width and height at `+0x2C` and `+0x30` of a display structure at 0x13588B0
(2015). The same structure holds the UI reference size 1280.0 and 720.0 at
`+0x3C` and `+0x40` (set at 0x610360).

**Change.** Make the two loads read `+0x3C` and `+0x40` instead: change the
low byte of the 4-byte address (at +4 of the instruction).

| Load | Site 2015 / 2020 | 2015: original → new | 2020 new byte |
|---|---|---|---|
| width | 0x443564 / 0x454704 | `dc` → `ec` | `9c` |
| height | 0x4435C2 / 0x454762 | `e0` → `f0` | `a0` |

The 2020 build's structure is assumed to have the same layout; check that its
original bytes are `8c` and `90` before writing.

**Why.** The back buffer in VR is the double-wide stereo target, so the cursor
would be off-centre and mis-scaled on the virtual screen.

### 7.8 Menus use focus (pad) navigation [guess]

A menu input-mode handler (0x43EB00, thiscall, two stack args, `ret 8`), for
input event 0x12, switches the menu to state 2 (pad/focus navigation, via
0x434E90) unless the keyboard/mouse device is present with two of its flags
(`+0x10`, `+0x11`) set. Events 0x15 and 9 switch to state 0 (pointer) and show
the cursor.

**Change.** At 0x43EB3A / 0x44FCFA, the `je` (`74 0c`) becomes `jmp`
(first byte `eb`), so event 0x12 always selects state 2.

**Why (guessed).** VR controllers reach the game as a gamepad (`04`), and
menus must keep a focused item to navigate with. Verify: with a keyboard and
mouse attached, menus still show a focused item and respond to the
controllers.

### 7.9 No 30 Hz throttle in the loading loop [likely]

A state loop (0x43AF50) in its state-3 branch measures the time since the last
pass and calls `SleepEx(33 - min(elapsed, 33), TRUE)`. The sleep time is
pushed with `push ecx` (`51`) at 0x43B01C / 0x44C1CC.

**Change.** Push a zero instead. EBX is 0 at this point in the loop, so the
byte `51` becomes `53` (`push ebx`), giving `SleepEx(0, TRUE)`. [Confirm EBX
is zero on the 2020 build too.]

**Why.** This loop appears to run while loading; capped at 30 Hz the headset
would get frames far below its refresh rate and the compositor would show
stutter or its own loading view. Verify: the loading screen stays smooth in
the headset.

### 7.10 The gamepad is always connected [sure]

The XInput pad's poll (0x60EE30) stores `+0x289 = (XInputGetState result !=
ERROR_DEVICE_NOT_CONNECTED)` with `setne cl` at 0x60EE6F / 0x56133F
(`0f 95 c1`) followed by the store.

**Change.** Store 1 always (replace the 3 bytes with a load of the constant 1
into CL, padded to 3 bytes).

**Why.** VR controller input reaches the game through the gamepad device's
methods (`04`); with no physical pad the game would treat the device as absent
and never call them.

### 7.11 Left-stick control prompts [sure]

The control-prompt name lookup (0x42CFF0, cdecl, two args) asks whether
movement is on the left stick (0x57DCE0, thiscall, ECX = the action map, no
args, returns bool in AL) to choose "lstick" or "rstick" for the prompts
`look_controls`, `hack_left_stick_controls` and `hack_right_stick_controls`.
That method finds the first enabled binding of the first analog action and
returns whether its code is 0x12 (left stick X); with no binding it returns
true. It is called from two sites: 0x42D0A0 and 0x42D18C (2015);
0x43E1F0 and 0x43E2DC (2020, the second assumed at the same +0xEC offset).

**Change.** Redirect both calls to a function with the same ABI that answers
from the player's original binding of the first analog action, as it was
before the mod remapped the action map (`04`).

**Why.** The mod rewrites the action map's bindings for VR controllers, so the
game's own check reads the mod's codes and shows the wrong stick in prompts.

## 8. Recenter from the game's own menu

Besides the update (section 3, replaced), the game recenters from the menu's
"reset view" through a small function (0x5B8680) that calls device slot 11
(vtable `+0x2C`) with a pointer to stereo `+0x78`, at 0x5B8691 / 0x50A831
(`8b 42 2c ff d0`: load the slot, call it).

**Required.** That call must perform the same yaw-only recenter as section
3.2e, writing `+0x78` (thiscall, one stack arg, `ret 4`). The device's slot 11
must either implement exactly that, or the 1-byte slot offset at +2 of the
site must be pointed at a device slot that does (the device's layout is the
mod's own; see `02`).

## 9. Deliberately not done

- **Audio endpoint redirect.** MotherVR redirects the game's audio to the
  headset's audio device, but only for the Oculus runtime. Under OpenVR (and
  OpenXR) the user chooses the default audio device; the mod does not touch
  audio.
- **Control-prompt name lookup.** MotherVR also redirects the call of the
  prompt-name lookup at 0x43AB40 / 0x44BCF0 to a function that only calls the
  original. It has no effect and is not needed.
- **R3 with the recenter chord.** No observable effect (3.2e).
- **The game's fade animation** (`+0xA8`/`+0xAC`) is not run (3.2).
