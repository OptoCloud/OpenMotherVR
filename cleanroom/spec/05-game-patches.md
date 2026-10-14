<!-- Provenance: static analysis of AI.exe (2015 Steam build, every original byte below re-read from the binary for r3), required behaviour restated from observing the earlier mod in game, in-game observation by the spec side in a headset (head tracking, stereo, menus, HUD, loading screens confirmed), measurements in game by the spec side (render-target rebuild stall, per-menu-open freeze of an earlier build), user requirements (automatic recenter on headset-on and gameplay start, no 0.2 s freeze on every menu open), project decisions (snap-turn threshold and repeat, each with its reason inline), AI.exe 2015 analysis of the mode-stack container (answer to Q5, ANSWERS.md), a head-in-wall blackout in the mission 4 android scene noted in another mod's analysis (acceptance item and open question only). 2020 values were taken from the earlier mod's per-build address table (and, for inline sites, its installer) and have not been verified against a 2020 `AI.exe`. Revision: r4 -->

# 05: Changes to game behaviour

Every change the mod makes to how `AI.exe` behaves, other than creating the
stereo device (`02-game-stereo-path.md`), sizing and submitting frames
(`03-hmd-and-frames.md`), feeding input (`04-input.md`) and the rendering
fixes (`07-reflections.md`, `08-render-fixes.md`). For each change: what the
game does, what must happen instead, and why.

Addresses are RVAs (image base 0x400000), given as **2015 / 2020**. Every
2015 address and every 2015 byte below was read from the 2015 `AI.exe`
(PE timestamp 0x54E3199D). 2020 values were taken from the earlier mod's
per-build address table (and, for inline sites, its installer) and have not
been verified against a 2020 `AI.exe`. The inline sites are the eight
Stereo Mode checks (§2), the second prompt call site (§7.11) and the
replacement bytes of §7.7. "2020: ?" means unknown, and a feature that needs
an unknown address stays off on that build (01 §3.2). Before writing any
site the implementation checks the original bytes given here; what
happens on a mismatch is in 01 §2.4 (no patch at all, log the first
mismatching site, game unmodified).

These changes are applied only when the runtime for this run is OpenVR or
OpenXR. With runtime None only the UI set of 01 §2.3 is applied; from this
document that is the settings push in the front-end and pause Open wrappers
(§5, without the stereo-mode handling) and skip intro (§11).

Settings are named by their `mothervr.json` key (`06` §1).

Confidence: **[sure]** the game code shows it directly, or it was observed in
game; **[likely]** strong inference; **[guess]** plausible, must be confirmed
in game. Each [guess] and [likely] item says what to look for.

## 1. Game objects and conventions used below

| Object | Where | Facts |
|---|---|---|
| Stereo object | global pointer at 0x1366A2C / 0x1376BDC | Layout in `02` §3.3. Fields used here: `+0x04` device pointer (null without a device), `+0x20` 4x4 virtual-screen world matrix, `+0x60` current stereo mode (int), `+0x64` scene visibility (float, 1 visible, 0 black), `+0x68` mode stack data pointer, `+0x6C` mode stack count, `+0x70` mode stack capacity, `+0x78` 3x4 reference (recenter) transform, `+0xA8` fade target (bool), `+0xAC` fade alpha (float). |
| Stereo device | stereo `+0x04` | `+0x04` state: 0 none, 1 initialised, 2 tracking running ("stereo running" below means device present and state 2). See `02` §4. |
| POV manager | global pointer at 0x1366A00 / 0x1376BB0 | `[mgr+8]` points to the main view (POV 0); its first 64 bytes are the world-to-view matrix. The manager pointer, and POV 0, can be null (before the first level, during teardown). |
| Input manager | global pointer at 0x1359B44 / 0x1369CF4 | `+0x4A70` mouse, `+0x4A74` keyboard, `+0x4A7C` active pad device (each may be null). See `04` §2. |
| Keyboard key test | 0x60EB20 / 0x560FE0 | thiscall on the keyboard object, one int arg (DirectInput scan code), `ret 4`, returns bool in AL. |
| Keyboard any-key | 0x60EE00 / 0x5612D0 | thiscall, no args, plain `ret`, returns bool in AL: some key is down (byte `+0x224`). |
| Mouse active | 0x9BBE70 / 0x4C87E0 | thiscall, no args, plain `ret`, returns bool in AL: the mouse moved or a button is down (`+0x54` non-zero). |
| Stereo PushMode | 0x5D9C00 / 0x52BE00 | thiscall (int mode), `ret 4` (§4.4). |
| Stereo PopMode | 0x5D9C80 / 0x52BE80 | thiscall, no args, plain `ret` (§4.4). |

**Stereo modes.** Mode 0: the 3D world, with the 2D UI (HUD) drawn on a quad
locked to the head. Mode 1: the "virtual screen", the whole 2D UI (menus,
videos) drawn on a quad placed in the world, with the 3D scene behind it.

**View matrix convention [sure].** The game's world-to-view matrix is 16
floats, row-vector (D3D) convention, left-handed, units in metres. Columns
0, 1, 2 (elements [0,4,8], [1,5,9], [2,6,10]) are the camera's right, up and
forward axes in world space; row 3 (elements 12, 13, 14) holds the view
translation. The camera's world position is therefore
`-(row3 · column0, row3 · column1, row3 · column2)`, that is
`x = -(m12·m0 + m13·m1 + m14·m2)`, `y = -(m12·m4 + m13·m5 + m14·m6)`,
`z = -(m12·m8 + m13·m9 + m14·m10)`. World up is +Y.

A UI quad "world matrix" uses the same convention: rows 0, 1, 2 are the quad's
right, up and normal axes in world space, already scaled; row 3 is its centre
(element 15 is 1, elements 3, 7, 11 are 0).

**FollowBehavior [sure].** The first-person camera behaviour (vtable at
0xDC6600 / 0xDDD6E8; its name method, slot 14, returns "FollowBehavior").

| Field / slot | Meaning |
|---|---|
| `+0x204` | yaw (float), normalised: ±1.0 = ±180°, wraps at ±1 |
| `+0x1FC` | mirror of the yaw; must be written with the same value |
| `+0x208` | pitch, clamped by the game |
| `+0x260`, `+0x261` | look-integration enable bytes (see 7.4) |
| `+0x264` | look input enabled (byte): the game applies stick/mouse look deltas (slot 11, 0x2D200) only while this byte is set |
| slot 0 (vtable +0) | OnActivate(arg): 0x2C930 / 0x3C1A0, thiscall, one stack arg, `ret 4`, returns a value in EAX |
| slot 1 (vtable +4) | OnDeactivate(arg): 0x2BBF0 / 0x3B340, same ABI; clears `+0x264` |

A FollowBehavior object can be destroyed after it is deactivated (a level
unload) [likely]; see §6.1 for what the mod may write through it.

## 2. Force the eight "Stereo Mode == Rift" checks true [sure]

**Game.** Graphics setting 0x18 is "Stereo Mode". The game's dormant HMD
path (`02`) runs only where the current value equals the "Rift" option. Each
check has the same shape: fetch setting 0x18 (settings lookup at 0x5A4D00),
load its current value index into a register, call the stereo object's
"index of the Rift option" method (0x5BE350, thiscall, returns it in EAX),
then a 2-byte register-to-register compare `cmp reg, eax`, followed by
`jne`/`je`/`sete`. What each one decides is in 01 §4.1.

| # | 2015 | 2020 | Original bytes (2015) | What it gates |
|---|---|---|---|---|
| 0 | 0x0336B6 | 0x042F96 | `3b f8` | on a Stereo Mode change, enables each camera's VRCameraBehavior |
| 1 | 0x60FD58 | 0x562238 | `3b d8` | the render-target size query (call at 0x60FD64) |
| 2 | 0x60FE50 | 0x562330 | `3b f0` | the display size query (call at 0x60FE5D) |
| 3 | 0x60FEBD | 0x56239D | `3b f0` | present sync interval |
| 4 | 0x60FF15 | 0x5623F5 | `3b f0` | swap-chain buffer count |
| 5 | 0x611740 | 0x563C10 | `3b f0` | back-buffer pixel format |
| 6 | 0x61378A | 0x565C5A | `3b f0` | output selection and stereo device creation (`02`) |
| 7 | 0x613C80 | 0x566150 | `3b d8` | `sete al` stereo flag in renderer setup |

**Check.** Each site is opcode `3b` with a ModRM byte whose mod field is 3
and whose r/m field is EAX (`ModRM & 0xC7 == 0xC0`).

**Change.** Write `39 c0` (`cmp eax, eax`) over the two bytes, which sets the
flags as for equality and is the same length.

**Why.** The setting cannot select a modern headset, and the whole stereo
path (double-wide render target, per-eye rendering, the UI overlay, the mode
stack) hangs off these eight checks. Forcing them is what turns the path on.

## 3. Per-frame stereo update

### 3.1 What the game does [sure]

The stereo object's update (0x5BE2A0, thiscall, one float arg = frame time,
`ret 4`) is called once per frame from 0x3B4100 / 0x3C4FB0 (a 5-byte
`call rel32`, 2015 bytes `e8 9b a1 20 00`). It:

1. if the device state is 2 and the gamepad's LB and RB are both held, calls
   device slot 11 (vtable `+0x2C`) with a pointer to stereo `+0x78`, which in
   the DK2 implementation takes the head yaw and writes the reference
   transform there (the "recenter");
2. animates the fade alpha `+0xAC` towards `+0xA8` by the frame time,
   clamped to [0, 1].

### 3.2 Required replacement

Redirect the call at the site above to the mod's own update (same ABI:
thiscall, ECX = stereo object, one float on the stack, callee pops 4 bytes;
the return value is not used). The game's update is not called. The
frame-time argument is not needed and the fade alpha `+0xAC` is left at its
initial value 1.0 (`02` §3.3).

It runs on the game's main thread, the thread that also runs the game's
pushes and pops, the menu Open/Close methods, the FollowBehavior activate and
deactivate methods and the UI message handler [likely: all are called from
the game's single update loop; confirm by logging thread ids once].

The rules a to g below are outcomes that must hold for each frame (each run
of the update). The lettering is for reference only, not an order. The game
requires one order: rule g is applied after the game has written `+0x64`
for this frame, because it overrides that value. What the implementation
remembers between frames, and how, is its own choice.

**a. Without a running headset.** While the stereo object has no device, or
the device state is not 2, the update leaves the game alone except for one
thing: a tracked, active FollowBehavior (§6.1) has its look byte `+0x264`
set to 1. Rules b to g apply only while stereo runs.

Why: with no headset tracking, the game's own stick and mouse look must
work, and 7.6 removed the game's own enabling of that byte.

**b. Which input is in use.** Rule d needs to know whether the player is
using keyboard and mouse or a pad (a real pad or the VR controllers). Per
frame:

| Activity this frame | Keyboard/mouse in use afterwards |
|---|---|
| keyboard or mouse activity: both devices exist (input manager `+0x4A74`, `+0x4A70`) and the keyboard's any-key test or the mouse-active test (§1) is true | yes, also when the pad reports activity in the same frame |
| pad activity only: the active pad device (`+0x4A7C`) exists and its HasActivity method, as replaced by the mod (`04` §7), reports activity (a real pad **or** the VR controllers) | no |
| neither | unchanged |

At start-up keyboard/mouse is not in use.

Why: a hand can rest on the mouse while the controllers are used, and the
reverse; keyboard and mouse win a tie so that mouse look is not lost while
both are touched.

**c. Snap turn.** The turn value is `04` §6's, read from the active pad
device. It is **deflected** when it is below -0.5 or above 0.5 (project
decision: half deflection, so stick drift or a light touch never turns).

While stereo runs, the active pad device exists, a FollowBehavior is
tracked and active, `SnapRotation` is on and the current stereo mode
(`+0x60`) is not 1:

- deflecting the stick turns the player by one step at once: one step added
  to the FollowBehavior yaw `+0x204` for a positive value, subtracted for a
  negative one;
- holding it deflected repeats a step every 300 ms (project decision: slow
  enough that a held stick can be released after one extra step, fast
  enough to turn around in under two seconds at 30° steps);
- returning it inside ±0.5 allows the next deflection to step at once.

After each step the yaw is wrapped into [-1, 1] (above 1 subtract 2.0,
below -1 add 2.0; 2.0 is one full turn) and the same value is written to
both `+0x204` and `+0x1FC`. The step is computed in floating point as
`step = 2.0 / segments` in the game's normalised yaw units (±1.0 = ±180°),
so `segments` steps make a full turn (24: 15°, 12: 30°, 6: 60°, 4: 90°, 3:
120°, 2: 180°); `segments` is `SnapRotationSegments` (one of 24, 12, 6, 4,
3, 2; default 12, giving 30° steps; `06` §1). The step must not be derived
with integer division anywhere (that turns 180° for 3 segments); see `04`
§6. Timing uses a monotonic clock.

Why: smooth yaw from a stick in VR causes motion sickness; the yaw field is
the game's own first-person heading, so turning through it keeps movement,
aiming and animation consistent. No turning while a menu is on the virtual
screen.

**d. Game look.** While stereo runs and a FollowBehavior is tracked and
active, its look byte `+0x264` is 1 when `SnapRotation` is off or
keyboard/mouse is in use (rule b), and 0 otherwise.

Why: the game look (stick/mouse) adds yaw and pitch to the camera on top of
the headset. With snap turn on and VR controllers in use it must be off
(pitch comes from the head only; yaw from snap steps). With snap off, the
stick turns smoothly through the game's look. With mouse and keyboard the
player needs mouse look. See also 7.6.

**e. Recenter.** The chord is LB and RB held on the active pad device (pad
buttons 10 and 11, read with the game's own button test and raw codes, `04`
§8), or Left Ctrl (scan code 0x1D) and Left Alt (0x38) held, read with the
key test while both keyboard and mouse devices exist. A recenter makes the
stereo device write the yaw-only reference transform to stereo `+0x78` (`02`
§6.4, `03` §2: heading and position, no pitch or roll). Required:

- a pending automatic request (below) recenters on the first update where
  the headset is in use and its pose is valid; until then it stays pending.
  A recenter is never taken from an invalid pose (`03` §2.1);
- holding the chord recenters once per press; releasing it re-arms it;
- after any recenter, an open virtual screen (mode 1) is re-placed (§4.2,
  rebuilding stereo `+0x20`) on each of the next two updates, with the same
  inverse aspect the game's SetMode passes (UI target height / width, 0.5625
  for the game's 16:9 UI). This makes an open menu face the new forward; two
  updates are needed because the new reference reaches POV 0's view matrix
  only on the next frame [likely; observed: one re-place was not always
  enough]. In mode 0 the matrix is unused and the next entry into mode 1
  places it anyway, so re-placing it then is harmless.

**Automatic recenter requests** (user requirement: forward must be where the
player faces when the headset goes on and when gameplay starts; the
triggers are `03` §2.2):

- when the device is created (`02` §6.1, first creation only, device state
  1): recenter at once; if the headset is not in use at that moment, also
  leave a request pending, so the reference is taken again when the player
  puts it on;
- when a loading screen closes (section 5): leave a request pending, so
  gameplay starts with forward where the player faces.

A request that is already pending stays pending (requests do not queue). A
recenter in the VR runtime's own UI does not create a request: on OpenXR it
re-anchors the mod's base space only (`09` §6.3); OpenVR reports none in v1.

"Headset in use" (`03` §2.3, `09`): OpenVR: `GetTrackedDeviceActivityLevel`
for the HMD is `k_EDeviceActivityLevel_UserInteraction` and the world-up
component of the head's up axis is greater than 0.7 (about 45°); OpenXR: the
session is running, the frame's view poses are valid, and the same uprightness
test holds on the head orientation. A pending request waits as long as needed;
it never fires on a headset lying on a desk.

**f. Mode stack count.** At the end of each update the stack count (stereo
`+0x6C`) equals the current mode (`+0x60`): 0 in mode 0, 1 in mode 1, even
after unbalanced game pushes or pops. With the stack kept at most one deep
(section 4.4) this is the only state the stack can be in that agrees with
the mode.

The stack is a growable array of 32-bit ints owned by the game [sure]:
`+0x68` data pointer, `+0x6C` count, `+0x70` capacity. It starts empty (data
null, capacity 0). The game grows it only in its append routine, 0x294540 /
2020: ? (called from PushMode at 0x5D9C53, bytes `e8 e8 a8 cb ff`), thiscall
with ECX = stereo `+0x68` and one argument, a pointer to the int to append,
`ret 4`; returns the element's address in EAX (null if allocation failed).
When the count equals the capacity it allocates a new array of twice the
capacity (8 entries the first time) through the game's allocator, copies the
old entries and frees the old array.

- **Lowering** the count (1 to 0): write `+0x6C` directly.
- **Raising** it (0 to 1): append the value 1 through the game's append
  routine, never by writing the count alone: when the game entered mode 1
  without a push, the array may never have been allocated. Do it from the
  per-frame update, on the thread where the game itself pushes. The game's
  PushMode holds a lock around its append; since every push and pop runs on
  that same thread, no further locking is needed [likely].
- If the append routine's address is unknown (2020), raise the count only
  when data is non-null and the capacity is at least 1 (element 0 = 1, then
  the count), otherwise leave it and log once.

**g. Scene visibility.** The game writes `+0x64` every frame from the
head-in-wall test (7.2, filtered by 08 §4). The value that stands after the
update:

| Situation | `+0x64` |
|---|---|
| a loading screen is up (between its Open and Close, section 5) | 0.0, whatever else applies |
| otherwise, a cutscene or level fade is in progress (below) and the mode is not 1 | 1.0 |
| otherwise | the game's value |

Why: behind a loading screen the world is half-loaded and the camera may
jump, so the scene is black. Cutscene cameras are not where the player's
body is, so the head-in-wall test would black the view out [likely].

A cutscene or level fade is in progress from a start message to the next
finish message (section 6.2). It also counts as finished, without a finish
message, once the player's FollowBehavior has been inactive for at least
one update after the start message and is active again: the camera is back
with the player, so the cutscene is over even if its finish message never
came.

The update writes no log line per frame.

## 4. Placing the UI quads, and the stereo mode stack

### 4.1 The game's quad builder [sure]

0x5BE550 / 0x510710 builds a UI quad matrix:
`Matrix44* builder(Matrix44* out, float size, float invAspect, float dist)`,
**cdecl** (plain `ret`, caller pops 0x10), returns `out` in EAX. The callers
copy the 64 bytes **through the returned pointer**, so a replacement must
return `out`. The game builds: row 0 = camera right × size, row 1 = camera up
× size × invAspect, row 2 = camera forward, row 3 = camera position + forward
× dist, using the full camera orientation from POV 0.

It is called from two sites, both 5-byte `call rel32` to 0x5BE550; check
that both sites call the same target before redirecting:

| Site | 2015 | 2020 | 2015 bytes | Game arguments | Result stored in |
|---|---|---|---|---|---|
| set-stereo-screen method 0x5CD0A0, screen 1: the virtual screen | 0x5CD132 | 0x51F2A2 | `e8 19 14 ff ff` | (3.5, UI target height / width, 3.0) | stereo `+0x20`, once per entry into the virtual screen |
| overlay draw (0x5CD170), mode 0: the HUD | 0x5CD2EE | 0x51F45E | `e8 5d 12 ff ff` | (0.8, inverse aspect, 1.0) | a local, rebuilt every frame |

Redirect both calls to mod functions with the same ABI. They ignore `size` and
`dist`, use `invAspect` as given, read the current view matrix of POV 0, and
return identity (in `out`) when the POV manager or POV 0 is null.

The quad's size in metres is taken here as the length of row 0. [likely:
menus and HUD were seen at a comfortable size in game, but the metre size was
not measured; see §16.]

### 4.2 Virtual screen (mode 1): level, yaw-only, world-fixed

From the view matrix `m` of POV 0:

- Forward: the camera forward projected onto the horizontal plane,
  `(fx, fz) = (m2, m10)` normalised. When the camera looks almost straight
  up or down (horizontal length below 1e-4, the threshold `03` §2.1 uses for
  the same case), the heading is undefined: any level heading is acceptable
  (the previous placement's, or a fixed one), but the matrix must never
  contain NaN.
- Row 0 (right) = `(fz, 0, -fx)`; row 1 (up) = `(0, invAspect, 0)`; row 2
  (normal) = `(fx, 0, fz)`.
- Row 3 (centre) = `(cx + 1.5·fx, cy, cz + 1.5·fz)`, where `c` is the camera
  position (§1): 1.5 m ahead at eye height.
- So the screen is 1.0 m wide and `invAspect` m high (0.5625 m for a 16:9 UI).

The matrix is built when the game enters mode 1 (every SetMode(1), which
§4.4 makes happen on every push) and when a recenter re-places it (3.2e), and
is not updated while the menu is up, so the screen stays fixed in the world
while the head moves.

Why: the game's placement copies the full head orientation at the moment the
menu opens (tilted if the player looked down) and is 3.5 units wide at 3 m. A
level screen at reading distance, facing where the player looks, is
comfortable and does not follow the head.

### 4.3 HUD panel (mode 0): head-locked

From the view matrix of POV 0:

- up = column 1 `(m1, m5, m9)`, forward = column 2 `(m2, m6, m10)`,
  right = up × forward (left-handed cross product).
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
  (0x5CD0A0), then appends `mode` to the stack under a lock.
- PopMode() 0x5D9C80 / 0x52BE80, plain `ret`: if the count is non-zero,
  removes the top and calls SetMode with the new top, or 0 if empty.

Call sites, each a 5-byte `call rel32` with ECX = the stereo object. Check
that each push site calls 0x5D9C00 and each pop site 0x5D9C80 (2020: the
2020 PushMode and PopMode RVAs):

| Kind | 2015 | 2020 | 2015 bytes | Context |
|---|---|---|---|---|
| push | 0x40EB8E | 0x41FD4E | `e8 6d b0 1c 00` | pushes 1 |
| push | 0x43EECA | 0x45008A | `e8 31 ad 19 00` | pushes 1 (a hub-type menu screen) |
| push | 0x44197A | 0x452B2A | `e8 81 82 19 00` | front-end menu Open |
| pop | 0x413DDA | 0x42508A | `e8 a1 5e 1c 00` | front-end menu Close |
| pop | 0x416382 | 0x4275D2 | `e8 f9 38 1c 00` | (owner not identified) |
| pop | 0x424C2A | 0x435DCA | `e8 51 50 1b 00` | (owner not identified) |

**Required.** Redirect the three push sites to a function with PushMode's
ABI and the three pop sites to a function with PopMode's ABI, with these
results:

| Call | Result when it returns |
|---|---|
| push of mode `m` | the game is in mode `m`, the stack holds at most one entry, and the game's SetMode(`m`) ran during the call, so for `m` = 1 the virtual screen is placed again in front of the current view (§4.2) |
| pop in mode 1 | the game is in mode 0 with an empty stack, through the game's SetMode(0) |
| pop in mode 0 | nothing changes |

The game's own PushMode and PopMode, called directly (not through a
redirected site), are the means: they keep the game's lock and its SetMode
call. Together with 3.2f and the menu wrappers (section 5), the game is in
mode 1 exactly while a full-screen menu, loading screen or title video is
up.

Why: the game's menus push and pop unevenly (nested screens, pops on resume
without a matching push); with the stack as written the game could stay in
the virtual screen during gameplay or show a menu in mode 0.

**Two stray stack operations are removed:**

| Site | 2015 | 2020 | Original (2015) | Check | New bytes |
|---|---|---|---|---|---|
| The pop in the "Resume_game" handler (front-end method at 0x4535D0) | 0x453682 | 0x4649B2 | 11 bytes: `8b 0d 2c 6a 76 01` (`mov ecx,[StereoObjectPtr]`) and `e8 f3 65 18 00` (`call` to the pop) | `8b 0d` at +0, a `call` to the pop at +6 | `eb 09` then nine `90` |
| The push of 1 in the pause menu's "Pause" handler (pause-menu vtable slot 3, 0x43E520) | 0x43E545 | 0x44F705 | 13 bytes: `8b 0d 2c 6a 76 01`, `6a 01` (`push 1`), `e8 ae b6 19 00` (`call` to the push) | `8b 0d` at +0, `6a 01` at +6, a `call` to the push at +8 | `eb 0b` then eleven `90` |

(The 4-byte operand `2c 6a 76 01` is the relocated address of
`StereoObjectPtr`; it differs when the image is rebased and in 2020, so check
only the opcode bytes.)

Why: the pause and front-end wrappers (section 5) now own entering and
leaving mode 1 for these screens; the game's own calls would unbalance it.

## 5. Menu objects

All methods below are thiscall with no stack arguments and a plain `ret`.
Replace the vtable slot with a function of the same ABI that calls the
original with the same ECX and adds the results below. Vtables are in
`.rdata`; the slot must be made writable to patch it and its protection
restored. Before patching, check that each slot to be replaced holds an
address inside `AI.exe`, and that the front-end vtable's slot 0 holds the
front-end Open RVA below.

| Menu | Vtable 2015 / 2020 | Slot 0 Open (original) | Slot 2 Close (original) | Recognisable by |
|---|---|---|---|---|
| Front-end menu | 0xF000E0 / 0xF17840 | 0x441950 / 0x452B00 | 0x413DD0 / 0x425080 | movie methods playBackgroundMovies, setLanguage, stop_front_end_music |
| Pause menu | 0xF00180 / 0xF178E0 | 0x441D90 / 0x452F40 | 0x4155E0 / 0x426890 | setItemVisible, setChallengeOrPreorderEnabled; slot 3 handles "Pause" |
| Loading screen | 0xF00218 / 0xF17978 | 0x441EA0 / 0x453050 | 0x415670 / 0x4268D0 | setGameplayTip, setState |
| Attract / title video | 0xEFCC80 / 0xF143E0 | 0x40B3A0 / 0x41C610 | 0x669C90 / 2020: ?, a shared empty method | playAttractVideo |

"Mode 1, placed now" below means: the game is in mode 1 with a stack one
entry deep, and the virtual screen was placed (§4.2) during this Open, in
front of the current view. "Mode 0" means the game is in mode 0 with an
empty stack. Both are reached through the game's own PushMode and PopMode
called directly, as in §4.4.

| Menu | After Open | After Close |
|---|---|---|
| Front-end | the mod's settings are pushed into the menu's UI (`06` §4.4); mode 1 comes from the game's own push at 0x44197A (redirected, 4.4) | mode 0 |
| Pause | mode 1, placed now; the settings are pushed | mode 0 |
| Loading screen | mode 1, placed now; the scene is black (3.2g); the settings are pushed | the scene is no longer forced black, mode 0, and an automatic recenter request is pending (3.2e); the first gameplay frame after the loading screen is not black |
| Attract / title video | mode 1, placed now | mode 0; the original Close is **not** called (it is an empty method shared with other classes) |

Order: on Open, the original runs before the settings are pushed, because
they are pushed into the menu's movie, which the original Open sets up
[likely]. No other order relative to the original is required.

With runtime None the front-end and pause Open wrappers do only the settings
push (01 §2.3); the other slots are not touched.

Effect: menus, the loading screen and the title video appear on the level
virtual screen 1.5 m ahead, placed when they open; behind a loading screen
the 3D scene is black, because the world behind it is half-loaded and the
camera may jump. The settings push lets the options page show the mod's
current values (`06`).

## 6. Watching the game's camera and cutscenes

### 6.1 Tracking the active FollowBehavior [sure]

Replace FollowBehavior vtable slots 0 and 1 (the dwords at 0xDC6600 and
0xDC6604; 2020: at 0xDDD6E8 and 0xDDD6EC) with functions of the same ABI
(thiscall, one stack argument, `ret 4`, return value passed through).
Before patching, check that slot 0 holds the OnActivate RVA of §1 and slot 1
an address inside `AI.exe`.

Required results:

- from a FollowBehavior's activation until its deactivation it is the
  tracked, active one (the last one activated, since there is one
  first-person camera);
- once its deactivation has begun the mod holds no usable reference to it;
  nothing is written through it after the original OnDeactivate returns,
  because the object may then be freed;
- both originals are called with the same `this` and argument, and their
  return value is passed through.

The mod writes FollowBehavior fields (yaw `+0x204`, `+0x1FC`, look byte
`+0x264`) **only while a FollowBehavior is tracked and active**. The object
may be freed after its deactivation (a level unload), and a write through it
would corrupt the heap.

Why: the stereo update needs the player's camera behaviour to turn it, to
gate the game's look, and to tell gameplay from cutscenes. There is one
first-person camera, so the last one activated is the one that matters.

### 6.2 Cutscene and level-fade messages [sure]

The UI entity class's message handler is vtable slot 2: the slot is at
0xDFCE18 / 0xE13F38 (vtable at 0xDFCE10 in 2015), original handler 0x51BD70 /
0x6241A0. ABI: thiscall(this, const ShortGuid* message, a, b, c), `ret 0x10`,
returns a value in EAX; the first dword the message pointer points to is the
hashed message name. Before patching, check that the slot holds an address
inside `AI.exe`.

Four globals hold the names' hashes. They are computed at start-up by static
initialisers, so read them when comparing, not at install time:

| Message | Global 2015 | Global 2020 |
|---|---|---|
| `cutscene_visibility_start` | 0x134DC0C | 0x17F6B84 |
| `cutscene_visibility_finish` | 0x134DC18 | 0x17F6B90 |
| `level_fade_start` | 0x134DD1C | 0x17F6C94 |
| `level_fade_finish` | 0x134DD28 | 0x17F6CA0 |

Replace the slot with a function of the same ABI so that either start
message begins a cutscene or fade (3.2g) and either finish message ends it,
and the original is always called with all arguments and its result
returned. The message is the handler's first stack argument; a null message
pointer matches nothing.

Why: during cutscenes and level fades the camera is driven by the game, not
by FollowBehavior, and the head-in-wall test (7.2) would black out the view
(3.2g).

## 7. Byte patches

Each row gives the site, the original 2015 bytes (checked before writing),
and the bytes written. Writes go through a temporary change of page
protection and an instruction-cache flush.

| § | Site 2015 / 2020 | Original (2015) | Check | Written |
|---|---|---|---|---|
| 7.1 | 0x575421 / 0x4C71D1 | `75 10` | first byte `75` | `eb 29` |
| 7.2 | 0x346E94 / 0x35794E | `0f 2f 05 a8 68 1c 01` | `0f 2f`, then ModRM with `& 0xC7 == 0x05` | 4-byte address at +3 replaced |
| 7.3 | 0x2603B1 / 0x270C61 | `8b ce e8 98 bb fe ff` | `8b`, register-form operand byte (`& 0xC0 == 0xC0`), `e8` at +2 | `eb 05` at +0 (the other 5 bytes left) |
| 7.4 | 0x02D83C / 0x03D0BC | `8a 4c 24 0f` | `8a 4c 24` | an instruction that sets CL to 1, padded to 4 bytes (for example `b1 01 90 90`) |
| 7.5 | 0x34A0CE / 0x35ACCE | `0f 86 2b 03 00 00` | all six bytes | `e9 2c 03 00 00 90` |
| 7.6 | 0x02C938 / 0x03C1A8 | `88 86 64 02 00 00` | `88`, ModRM `& 0xF8 == 0x80`, dword `0x264` at +2 | six `90` |
| 7.7 | 0x443568 / 0x454708 | `dc` (2020: `8c`, inferred, see 7.7) | the load at 0x443564 is `f3 0f 10` with ModRM `& 0xC7 == 0x05` | original + 0x10: `ec` (2020: `9c`) |
| 7.7 | 0x4435C6 / 0x454766 | `e0` (2020: `90`, inferred, see 7.7) | the load at 0x4435C2, same form | original + 0x10: `f0` (2020: `a0`) |
| 7.8 | 0x43EB3A / 0x44FCFA | `74 0c` | first byte `74` | `eb` at +0 |
| 7.9 | 0x43B01C / 0x44C1CC | `51` | `51` | `53` |
| 7.10 | 0x60EE6F / 0x56133F | `0f 95 c1` | all three bytes | `b1 01 90` |

The 7.6 site is FollowBehavior OnActivate + 8; check also that the vtable's
slot 0 points at OnActivate (6.1). The 7.7 sites are the low byte of each
load's 4-byte address, at +4 of the instruction (01 §3.3 explains why the
low byte survives relocation).

### 7.1 SSAO never runs [likely]

The SSAO setting handler (0x575400, thiscall, one arg = setting event,
`ret 4`) handles setting 0x15 "Screen Space Ambient Occlusion": for the value
index `v` it stores `+0xD80` (which AO texture the resolve uses) and `+0xD84`
(byte: run the AO compute pass). Originally v=1 gives (1, 1), v=0 gives
(0, 1), v=2 gives (2, 0).

**Change.** The `jne` after the `v == 1` compare becomes an unconditional jump
to the v=2 store (`eb 29`, target 0x57544C), which stores `+0xD80 = v` and
`+0xD84 = 0` for every value. The stereo object's setting handler also sets
SSAO to "Off" when stereo starts (`02` §6.5).

**Why.** Screen-space AO computed on the double-wide stereo target bleeds
across the eye boundary and differs between eyes, and it is costly at
headset resolutions. Look for: no AO darkening in corners and under objects,
whatever the option says.

### 7.2 Head-in-wall blackout follows `ClippingBlinders` [sure for the mechanics]

The VR camera behaviour ("VRCameraBehavior") computes a scene visibility from
the eye position and a body pivot (HeadClipFade, 0x346DE0, stdcall(const
vec3* eye, const vec3* pivot), `ret 8`, result on the x87 stack); its caller
(the call at 0x34E2C1) stores it in stereo `+0x64`. With `d = |eye - pivot|`
in metres: a distance factor `(1 - clamp((d - 0.5)·10, 0, 1))^5`, which is 1
below 0.5 m and 0 above 0.6 m; and, only if `d` exceeds a threshold, a probe
from the pivot towards the eye that scales the factor down when geometry is
in the way. The threshold is a constant 1e-5 (bits 0x3727C5AC, at RVA
0xDC68A8), read by `comiss xmm0, [abs32]` at the site in the table above
(the 4-byte address starts at +3).

**Change.** Point the 4-byte address at +3 to a float the mod owns and keeps
for the life of the process: 1e-5 (bit-identical to the game's, 0x3727C5AC)
when `ClippingBlinders` (`06` §1) is on, 1.0 when it is off. Set it at
install from the config and update it as soon as the option changes (the
`mvr_clipblinding` command, `06` §4.2); the game reads it on its next camera
update. A 4-byte aligned float written in one store needs no further
synchronisation.

**Why.** With `ClippingBlinders` on, the view goes black when the player's real head
moves into a wall, which prevents seeing through geometry. Some players
prefer no blackout; with 1.0 the wall probe only runs at distances where the
0.5 to 0.6 m falloff has already blacked the view out, so leaning into walls
no longer darkens the view (stepping more than 0.5 m away from the body still
does). The 0.6 s grace period of 08 §4 applies on top, in both settings.

### 7.3 No procedural camera sway or breathing [likely]

The camera update (0x260320) calls a function (0x24BF50) that blends in two
procedural noise channels from the character's animation component and writes
the camera's offset fields (`+0xE0`..`+0xEC`) as amplitude × channel × blend.
The call is `mov ecx, esi; call rel32` (7 bytes).

**Change.** `eb 05` over the first two bytes jumps over the call, so the
offsets stay at zero.

**Why.** Idle sway, breathing and handheld camera noise move the view without
the player moving their head, which causes nausea in VR. Look for: the view
is still when standing still with the headset still.

### 7.4 Look-integration branch kept on [guess]

FollowBehavior's state update (0x2D600) recomputes the bytes `+0x258`..`+0x263`.
At the site it sets `+0x261` from a saved earlier state (`mov cl, [esp+0xF]`,
the previous `+0x25E`, followed by `88 8e 61 02 00 00`, the store to
`+0x261`). The look/aim integrator (0x2BC50, called on activate and update)
treats "`+0x260` or `+0x261` set" as permission for one of its integration
branches.

**Change.** Replace the 4-byte load with an instruction that sets CL to 1,
padded to 4 bytes (for example `b1 01 90 90`), so the value stored to
`+0x261` is always 1. Only the result matters: CL is 1 at the store.

**Why (guessed).** Probably "free look not locked by animation": keeps the
camera following the player's look state when the game would otherwise lock it
during some animations, which in VR would fight the head pose. Look for: with
and without the patch, compare vents, ladders, hiding in lockers and scripted
grabs.

### 7.5 No DK2-era camera correction [likely]

VRCameraBehavior's update (0x34DE50) calls a correction function (0x34A050,
thiscall, four stack args: a value, two `vec3*` positions and `vec3* out`;
`ret 0x10`) at 0x34E1B9. It zeroes `out`, then computes a correction vector
only if the two positions differ vertically by at most 0.999: the `jbe rel32`
at the site skips to its epilogue at 0x34A3FF.

**Change.** `e9 2c 03 00 00 90` (an unconditional `jmp` to the same target,
then `nop`), so `out` is always (0, 0, 0).

**Why.** The game pushes the camera back against head displacement (a DK2
body/neck model). With modern 6-DoF tracking this moves the world against the
player's real head motion. Look for: leaning moves the view by exactly the
head motion.

### 7.6 FollowBehavior activation does not enable look input [sure]

OnActivate (0x2C930 / 0x3C1A0) sets `+0x264 = 1` with a 6-byte store at +8
(`mov [esi+0x264], al`, AL = 1). The constructor and OnDeactivate clear it.

**Change.** Six `nop`s over that store, so the look-enabled byte is
controlled only by the stereo update (3.2a and 3.2d).

**Why.** Otherwise each activation (every cutscene end, every respawn)
re-enables stick/mouse look until the next stereo update, and the stick would
turn the view smoothly on top of snap turn.

### 7.7 Mouse cursor in a 1280x720 UI space [sure for 2015; 2020 byte likely]

The UI mouse update (0x443520) centres the cursor as
`(mouse.x - W·0.5, -(mouse.y - H·0.5))` with `movss xmm2, [W]` and
`movss xmm2, [H]`, where W and H are the back buffer width and height at
`+0x2C` and `+0x30` of a display structure at RVA 0x13588B0 (VA 0x17588B0).
The same structure holds the UI reference size 1280.0 and 720.0 at `+0x3C` and
`+0x40` (set at 0x610360).

**Change.** Make the two loads read `+0x3C` and `+0x40` instead, by adding
0x10 to the low byte of each 4-byte address (table above).

2020: the reference size is assumed to sit 0x10 bytes past the back-buffer
size there too. The new low bytes `9c` and `a0` are the earlier mod's
installer's values; the originals `8c` and `90` are inferred from them by
subtracting 0x10. Neither was read from a 2020 binary. On 2020, read each
load's displacement, check that its low byte is the inferred original, and
write that byte plus 0x10.

**Why.** The back buffer in VR is the double-wide stereo target, so the cursor
would be off-centre and mis-scaled on the virtual screen.

### 7.8 Menus use focus (pad) navigation [guess]

A menu input-mode handler (0x43EB00, thiscall, two stack args, `ret 8`), for
input event 0x12, switches the menu to state 2 (pad/focus navigation, via
0x434E90, which sets `+0xD0 = 2` and calls the movie's `setState(2)`) unless
the keyboard device (input manager `+0x4A74`) is present with two of its
flags (`+0x10`, `+0x11`) set. Events 0x15 and 9 switch to state 0 (pointer)
and show the cursor.

**Change.** The `je` becomes `jmp` (first byte `eb`), so event 0x12 always
selects state 2.

**Why (guessed).** VR controllers reach the game as a gamepad (`04`), and
menus must keep a focused item to navigate with. Look for: with a keyboard and
mouse attached, menus still show a focused item and respond to the
controllers.

### 7.9 No 30 Hz throttle in the loading loop [likely]

A front-end state loop (0x43AF50) in its state-3 branch measures the time
since the last pass and calls `SleepEx(33 - min(elapsed, 33), TRUE)`. The
sleep time is pushed with `push ecx` at the site.

**Change.** `push ebx` (`53`) instead. EBX is 0 at this point in the loop
(2015, read from the code), giving `SleepEx(0, TRUE)`. [Confirm EBX is zero
on the 2020 build too, by reading the loop.]

**Why.** This loop appears to run while loading; capped at 30 Hz the headset
would get frames far below its refresh rate and the compositor would show
stutter or its own loading view. Look for: the loading screen stays smooth in
the headset.

### 7.10 The gamepad is always connected [sure]

The XInput pad's poll (0x60EE30) stores `+0x289 = (XInputGetState result !=
ERROR_DEVICE_NOT_CONNECTED)` with `setne cl` at the site, followed by the
store `88 8e 89 02 00 00`.

**Change.** `b1 01 90` (`mov cl, 1` padded to 3 bytes), so the flag is
always 1.

**Why.** VR controller input reaches the game through the gamepad device's
methods (`04`); with no physical pad the game would treat the device as absent
and never call them.

### 7.11 Left-stick control prompts [sure]

The control-prompt name lookup (0x42CFF0, cdecl, two args) asks whether
movement is on the left stick (0x57DCE0, thiscall, ECX = the action map, no
args, plain `ret`, returns bool in AL) to choose "lstick" or "rstick" for the
prompts `look_controls`, `hack_left_stick_controls` and
`hack_right_stick_controls`. That method finds the first enabled binding of
the first analog action and returns whether its code is 0x12 (left stick X);
with no binding it returns true. It is called from two sites, 5-byte
`call rel32` each: 0x42D0A0 (`e8 3b 0c 15 00`) and 0x42D18C (`e8 4f 0b 15
00`), 2015; 0x43E1F0 and 0x43E2DC (2020, the second at the same +0xEC
offset). Check that both call the same target.

**Change.** Redirect both calls to a function with the same ABI that answers
from the player's original binding of the first analog action of map 0, as it
was before the mod remapped the action map (`04` §3): true when that original
code is 0x12.

**Why.** The mod rewrites the action map's bindings for VR controllers, so the
game's own check reads the mod's codes and shows the wrong stick in prompts.

## 8. Recenter from the game's own menu

Besides the update (section 3, replaced), the game recenters from the menu's
"reset view" through a small function (0x5B8680, thiscall, no args) that
calls device slot 11 (vtable `+0x2C`) with a pointer to stereo `+0x78`, at
0x5B8691 / 0x50A831 (`8b 42 2c ff d0`: load the slot, call it). The game's
own update has the same call at 0x5BE2E8 / 0x5104A8 (`8b 40 2c`).

**Required.** These calls must perform the same yaw-only recenter as section
3.2e, writing `+0x78` (thiscall, one stack arg, `ret 4`, returns `out`),
immediately (no "headset in use" wait; an invalid pose keeps the old
reference and leaves a request pending, `03` §2.1). Slot 11 cannot be used
for it (it is the per-frame head pose, `02` §6.4). Point the 1-byte slot
offset at +2 of both sites at a slot of the mod's device vtable that the
game never calls: any slot beyond the game's 21, at a byte offset that fits
the signed 8-bit displacement (at most 0x7C). Which slot is the
implementation's choice. Check: `8b`, ModRM with mod field 1, then `2c`.
After a menu recenter the virtual screen is re-placed by the next
SetMode(1) (the menu is open) or the next push.

## 9. Deliberately not done

- **Audio output.** Moving the game's audio to the headset's audio device (OpenXR
  runtimes that report one) is specified in `10-audio.md`.
- **R3.** R3 has no function in v1, with or without the recenter chord.
- **The game's fade animation** (`+0xA8`/`+0xAC`) is not run (3.2).
- **Display reset on every menu open.** An earlier build froze for about
  0.2 s on every pause or loading-screen open; v1 raises the render-target
  rebuild flag only when a setting that needs it changed (section 10).

## 10. Render-target rebuild when a target setting changes

**Game [sure for the effect, likely for the name].** A byte at 0x1358470 /
0x1368620 (`.data`) is the renderer's "display changed" flag. The window
resize handler (0x611240) and the restore-from-minimised path (0x611CB0) set
it. The frame loop tests it at 0x3903B2 and then rebuilds the back buffer and
all render targets: it asks the stereo object for the render-target size again
(`02` §6.2), resizes the swap chain (`IDXGISwapChain::ResizeBuffers`),
re-creates its targets and re-runs the stereo device's configure-rendering
slot (`03` §4.4). Writing 1 to the byte from the mod makes the game do this
on its next frame. (01 lists it as `RenderTargetResizePending`.)

**Cost [sure, measured].** `ResizeBuffers` itself takes 3.4 to 5.0 ms, and the
frame after a rebuild takes 177 to 232 ms; in the headset the compositor shows
a stale frame for about 0.2 s, and hundreds of MB of VRAM are re-allocated.

**Required.** Three settings are baked into the render targets or the mirror
set up with them: pixel scale, mirror view type and symmetric mirror view
(`06` §1). The mod records their values at start-up (what the targets are
first built with). After each options command (`06` §4.2) and at each
settings push, it compares the current values with the record; if any
differs, it updates the record and writes 1 to the flag, once. It never
raises the flag otherwise: not on a menu open, not for any other setting.
With runtime None the flag is never written (01 §2.3).

An implementation that refreshes the mirror's parameters (`03` §8) without a
rebuild may leave out the raise for a change of the two mirror settings alone;
a pixel-scale change always needs it.

Why: a new pixel scale takes effect only when the game re-sizes its targets,
and a rebuild freezes the view for about 0.2 s (measured above), so raising
the flag on every menu open would freeze the game each time the pause menu
or a loading screen opened.

## 11. Skip intro

**Game [sure].** At 0x441AED / 0x452C9D a front-end method pushes the address
of the string `ShowBootup` (`68` + 4-byte address, 2015 bytes `68 78 e2 2f
01`, string at RVA 0xEFE278) as the name of the ActionScript function that
plays the start-up logos and intro.

**Required.** When `SkipStartupIntro` is on at start-up (`06` §1, §6),
check that the byte at the site is `68` and that its operand points at the
NUL-terminated string `ShowBootup` inside `AI.exe` (case-insensitive
compare), then point the operand (4 bytes at +1) at the NUL-terminated name
of the skip-intro function, held by the mod for the life of the process. The
skip-intro function is defined by the project's own movie (`14`) and goes
straight to the title screen; its name is the implementation's choice and
only has to agree between the movie and this operand. This is part of the
UI set and is also applied with runtime None. Changing the option takes
effect on the next launch.

## 12. Game events the mod reacts to

| Event | Source | Reaction |
|---|---|---|
| FollowBehavior activated / deactivated | vtable slots 0, 1 (6.1) | which camera is tracked and active |
| `cutscene_visibility_start`, `level_fade_start` | UI entity messages (6.2) | a cutscene or fade is in progress (3.2g) |
| `cutscene_visibility_finish`, `level_fade_finish` | UI entity messages (6.2) | it has finished |
| the player's FollowBehavior active again after being inactive during a cutscene or fade | 3.2g | it has finished, without a finish message |
| front-end, pause, loading, title video Open / Close | menu vtables (5) | stereo mode, loading blackout, settings push, recenter request |
| device created | `02` §6.1 | recenter, request if not worn |
| headset put on (in use) | runtime, polled by the update | a pending request fires |
| options command | `06` §4.2 | `ClippingBlinders` threshold (7.2), render-target rebuild (10) |

**Script event strings (diagnostic) [sure for the function].** The game's
scripts pass debug strings to a retail no-op at 0xA1BCC0 / 2020: ? (bytes
`8b c1 c2 04 00`, `mov eax, ecx; ret 4`: thiscall, one argument `const
char*`, returns `this`; 683 call sites in 2015). Interactions announce their
start and end there (`start_interaction`, `stop_interaction`,
`<kind>_start` / `<kind>_finish` for `suit_change_interaction`,
`keypad_interaction`, `hacking_interaction`, `cutting_panel`,
`terminal_interaction`, `ladder_interaction`, `rewire_interaction`), as does
hiding (`hiding_visibility_start`, `hiding_visibility_finish`). Most traffic is
per-frame engine chatter (strings starting with `C` or `on_`). No v1 behaviour
depends on these. With `DebugOptions` on (`06` §1) the mod may hook the
function (detour after checking its five bytes, calling the original with the
same arguments) and log each distinct string once and every change of the
derived interaction/hiding state; this is how new event names are found. The
hook must not allocate or log per call when the option is off.

## 13. Threads

| Code | Thread |
|---|---|
| Stereo update (3), push/pop redirects (4.4), menu wrappers (5), FollowBehavior slots (6.1), UI message handler (6.2), options commands and settings push (`06`), HeadClipFade (7.2, 08 §4) | the game's main thread [likely: all are reached from the game's update loop] |
| Quad builders (4.1) | the thread that runs SetMode and the overlay draw [likely the main thread] |

All state of this document is written and read on the main thread, so it needs
no synchronisation. The stereo device's state and pointer are also read on job
threads by the cull fix (08 §2); the mod's writes to them must be single
aligned stores (an atomic write) so those reads see either the old or the new
value. No allocation, file I/O or logging happens per frame in any of these
functions; logging is once per change. This document says which events are
logged; the wording and format are those of `11-diagnostics.md`.

## 14. States and edge cases

- **Install order.** All checks of this document (and of 01, 02) run before
  the first write; on any mismatch nothing is written (01 §2.4). The 07 and 08
  fixes are checked and applied separately (08 §0).
- **No device, or device not running** (runtime missing, failed, headset not
  yet tracking): 3.2a only; everything else is idle; menus still push and pop
  through the redirected sites and wrappers (harmless without a device: the
  game draws flat).
- **Headset taken off.** The device stays in state 2 and the update keeps
  running; snap turn and look gating continue; pending recenter requests wait.
  No recenter happens when it is put back on unless a request is pending.
- **Focus lost** (dashboard open): VR controller state is zeroed (`04`), so
  the turn value is 0, so the next deflection after focus returns steps at
  once; nothing else changes here.
- **Display reset** (window resize, restore, section 10): the stereo object
  and its fields, the mode stack and the tracked camera are unaffected; the
  game re-asks the stereo device for its sizes (`02`, `03`).
- **Runtime changed in the menu:** the config is saved and the game quits
  (`06` §4.5); the new runtime applies on the next launch.
- **Level load:** the loading screen enters mode 1 and blacks the scene; the
  FollowBehavior is deactivated (and forgotten) and a new one activated; on
  close, mode 0 and a recenter request.
- **Cutscene without a finish message:** treated as finished by 3.2g once
  the player's camera is active again.

## 15. Acceptance

Each item is checked in game on the 2015 build with OpenVR and with OpenXR,
headset worn unless stated. "Log" means the mod's log file.

1. The log records, once at start-up, which groups of this document were
   applied (Rift checks, stereo hooks, menu wrappers, byte patches), and on a
   modified `AI.exe` the first mismatching site and that nothing was patched
   (wording is the implementation's).
2. The front-end menu appears on a level screen about 1 m wide, 1.5 m ahead,
   at eye height, facing the direction the head faced when it opened; moving
   the head does not move it. Opening it while looking down still gives a
   level screen.
3. In gameplay the HUD (prompts, objectives, motion tracker overlay) sits on
   a head-locked panel about 0.55 m wide, 1 m ahead, and moves with head roll.
4. Pause in gameplay: menu on the virtual screen; resume: back to 3D with the
   HUD panel, never stuck in the virtual screen; repeat ten times, also from
   nested option pages.
5. Load a save: behind the loading screen the world is black; after it closes
   the player faces where the head faces (log: one recenter after the loading
   screen closed, once the headset is in use).
6. Start the game with the headset lying on the desk, put it on at the title
   screen: the menu re-places level in front of the player within a second
   (log: recenter on headset-on).
7. LB+RB on a pad, Ctrl+Alt on the keyboard, and the pause menu's reset view:
   each recenters once per press; the open menu re-faces the new forward.
8. Snap turn on (default): a turn-stick flick turns exactly 30° (12
   segments); holding repeats about every 0.3 s; set 3 segments: 120° per
   step; no turning while the pause menu is up. Snap turn off: the stick turns
   smoothly. With mouse and keyboard in use, mouse look works.
9. During a cutscene, moving the head into the cutscene camera's geometry does
   not black the view out.
10. With `ClippingBlinders` on, pushing the head into a wall darkens the
    view (after 08 §4's delay); with it off, leaning into a wall does
    not, but stepping more than 0.6 m from the body still does.
11. Standing still with the headset still, the view does not sway or breathe;
    leaning moves the view by exactly the head motion.
12. Screen-space AO is absent with every SSAO setting.
13. With a mouse, the cursor on the virtual screen sits under the mouse
    position mapped into 1280x720, centred.
14. Without a physical pad, VR controllers still drive the game; prompts name
    the correct stick.
15. Opening and closing the pause menu with unchanged settings causes no
    render-target rebuild (no swap-chain resize in the log, no 0.2 s
    freeze); changing the pixel scale in the menu causes exactly one rebuild
    and the new resolution appears without a restart.
16. With `SkipStartupIntro` on, the next launch goes straight to the title
    screen.
17. Load a different level ten times in one session: no crash (no writes
    through a freed camera behaviour).
18. In the mission 4 android encounter (a scripted player animation), the
    view never goes black from head clipping,
    with `ClippingBlinders` on. Whether 3.2g covers it depends on the open
    question below.

## 16. Open questions

- **Quad size in metres.** Whether a row-0 length of 1.0 gives a quad 1 m
  wide (§4.1). Look for: the UI overlay draw's vertex positions (0x5CD170) or
  measure in game against a known object at 1.5 m.
- **7.4 meaning.** What `+0x261` gates. Look for: A/B in vents, ladders,
  lockers and scripted grabs.
- **7.8.** Whether pad navigation is needed with keyboard and mouse present.
  Look for: menus with the patch off and a mouse attached, using VR
  controllers.
- **7.9.** Which screens run the throttled loop; whether EBX is 0 there in
  2020. Look for: frame times on the loading screen with the patch off; read
  the 2020 loop.
- **Threads (13).** Confirm that the menu methods, the UI message handler and
  the camera update all run on the thread that calls the stereo update. Look
  for: one thread id per hook in the log.
- **Pop sites 0x416382 and 0x424C2A.** Their owners. Look for: the functions
  containing them.
- **Mission 4 android encounter (acceptance 18).** Whether that sequence
  sends `cutscene_visibility_start` (or `level_fade_start`), so that 3.2g
  forces the scene visible. If it does not, the head-in-wall fade must also
  be forced clear while the player is in a scripted full-body animation;
  how to recognise that state in `AI.exe` is not known yet and must be
  found and verified in the binary before it is specified. Look for:
  whether the UI message handler (6.2) receives a start message during that
  sequence.
- **2020 build.** Every 2020 value here is from the earlier mod's table (and,
  for inline sites, its installer); the mode-stack append, the skip-intro
  string and the event tap are unknown.
