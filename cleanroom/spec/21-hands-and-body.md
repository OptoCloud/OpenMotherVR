<!-- Provenance: game analysis of the 2015 AI.exe (PE timestamp 0x54E3199D) by the spec side with static analysis (the frame stage table and its stage names, the player getter, health fields, crouch methods, the character final-pose step and its job scheduling, the Havok pose layout, the game's limb IK, the active camera's pose fields, the aim ray update and its caller, the fire alignment gate, the current-item getter and the weapon type enumeration, the motion tracker scan, the rigid-body velocity helper, the XInput rumble output); the game's own data files (GBL_ITEM.XML item and weapon definitions, UI.TXT item names, the first-person skeleton bone groups and mirror files in GLOBAL/ANIMATION.PAK, the first-person model part names in each level's MODELS_LEVEL.BIN); public OpenVR and OpenXR API definitions (grip and aim poses, haptic output); proposals of this project, pending user confirmation where marked [proposal]. Game facts located with the help of an analysis of another mod and verified in the 2015 `AI.exe` by the spec side; requirements are proposals of this project, pending user confirmation where marked. Facts found without a lead (the frame stage names, the rumble output, the weapon type enumeration, the item data, the first-person model parts, the bone names) need no such note. 2020 values: none (v2 gives 2015 RVAs only; every 2020 value is `?`). r2: second clean-room review: setting keys, labels and feature names chosen afresh by this project; behaviours restated as required results with this project's reasons (eye forward offset during framed screens, tracker heading, firing with the weapon away from the head, arming a throw, melee by motion, wrist health colour); solution hints reduced to places to look; seated and standing mode and calibration referred to 22 (one owner, 20 §5); the master switch of 20 §1.3. r3 (spec r7 cross-check): cross-references corrected (the hand mapping's game conventions cite 03 §1 and the view matrix convention of 05 §1; the haptics thread row cites §14.3 of this document); the interaction and hiding state named as the logging of game script events of 11 §5. r4 (CLEANROOM r3, what a player sees may be stated exactly): the `Hands` and `Body` pages of the options page given exactly (item order, labels, control kinds, value names, ranges and steps, description lines, greying), as this project's texts in the style of the v1 page players know, under the page rules of 22 §12.1; no message box for standing mode and calibration (the page texts say it); the controls help rows while hands are on listed as open. r5 (user decision of 2026-10-16): no master switch; every key here applies on its own with its v1 value as default (20 §1.3); the `Hands` and `Body` pages are reached from the `MotherVR` page (22 §12.1). r6 (project decision: with the aim poses made only at runtime start, turning tracked hands on in the menu left aiming on the head until the next start): the aim pose actions are declared and bound whatever `Hands.Enabled` is, so the key applies at once as §15.1 says; a binding that lacks them is logged. r7 (static analysis of the 2015 `AI.exe` and the game's data by the spec side, without a lead): `FinalPoseStep`'s single return corrected (0x2A5553 belongs to the next function); the pose's hand-over to the model's render objects (`SkinSetPose` 0x28E9A0, its two calls in `FinalPoseStep`, which render objects receive which pose, and that it reads the pose without keeping it); the skeleton's name field and the model-space array's size; the crouch methods' condition byte `+0x3E2E` (corrects §2.2); the game's first-person camera offsets in `DATA\GLOBALCONSTANTS.TXT` (§22 item 14, the crouch drop); body visibility's method made exact (collapse at the nearest drawn joint, in a copy handed to the render objects; the held item's bone always drawn); real crouch's press through the v1 crouch control; room-scale kept off until the frame of the game's movement input and the body's yaw are established (new §22 items 25, 26); new open items 27 (parts posed from another skeleton) and 28 (the requested crouch byte in play). r8 (a tester's log: a value set in a menu was never applied): a skeleton that cannot be read is read again once a second until it can be (§10.2, project decision with its reason), and that is logged once per change with its reason (§18). r9 (game analysis of the 2015 `AI.exe` by the spec side, facts located with the help of an analysis of another mod and verified in the code): the character's model-to-world transform on the animation component and the bone world query `BoneWorldTransform` 0x2A2FB0 with its pose selector (§2.3); the final pose is entry 7 of the component's pose array; the held item's attachment query at 0x3B4E06 and what makes the item follow the edited hand (§4.2 item 6, §22 item 3); the game's limb IK is not run for the player's first-person skeleton while tracked hands are active (§4.2 item 7, §22 item 4); the game's CCD and two-joint IK solvers (§22 item 23); the grip to hand-bone mapping derived from each skeleton's own finger bones at run time (new §3.5, §22 item 2). Revision: r9 -->

# 21. Hands and body

The v2 specification for tracked hands and the player's body: the VR
controllers' poses drive the first-person character's arms and hands; the
weapon aims where the hand points; the motion tracker, the flashlight,
melee and throwing follow the hands; the body can be hidden, follows the
real head and height, crouches with the player, and is brought back into
agreement with the head after the game's own animations; health is shown
on the wrist; the controllers vibrate for game events. Seated and standing
play and the height calibration are 22's (22 §6); this document says how
the head pose, room-scale and real crouch use them.

Read 20 first (the v1 rule: every v2 feature off or v1-compatible by
default, and all v2 options off is exactly v1). This document builds on 02,
03, 04, 05, 09, 11 and 12 and names the sections it changes.

**Conventions.** Addresses are RVAs of the 2015 Steam `AI.exe` (image base
0x400000), written 0xNNNNNN; the 2020 build is `?` throughout and every
feature here stays off on it (20 §6). Names in backticks such as
`AimRayUpdate` are spec-side labels for game code the game leaves unnamed;
the implementation may rename them (01). Byte strings are the 2015
originals, checked before any use (12 §4). Confidence: **[sure]** read in
the code or the game's data; **[likely]** strong inference, to be confirmed
by the acceptance check named; **[guess]** plausible, not to be relied on
without the check. Requirements that are design choices, not facts, are
marked **[proposal]** with a one-line reason; the user confirms or changes
them. Tuning numbers are proposals with their reasons. Settings are named by
their `mothervr.json` key inside `Settings` (06 §1): `Hands.Enabled` is
`Settings.Hands.Enabled`. Text a player sees (the options page items of
§15.3) is stated exactly, in code spans (CLEANROOM r3).

## 1. Summary

| Feature | Section | Key that turns it on | v1 behaviour when off |
|---|---|---|---|
| Tracked hands: the character's arms reach the controllers | 4 | `Hands.Enabled` | arms play the game's animation; controllers are a gamepad (04) |
| Aiming from the hand, per weapon class | 5 | `Hands.AimFromHand` (with `Hands.Enabled`) | aim follows the head (the game's camera) |
| Motion tracker held and aimed with the left hand | 6 | `Hands.TrackerInHand` (with `Hands.Enabled`) | tracker faces where the body faces |
| Flashlight aimed by a hand | 7 | `Hands.FlashlightAim` (with `Hands.Enabled`) | flashlight follows the head |
| Melee by motion | 8 | `Hands.MeleeByMotion` (with `Hands.Enabled`) | melee on the fire control |
| Throwing by motion | 9 | `Hands.ThrowByMotion` (with `Hands.Enabled`) | the game's throw on the fire control |
| Body visibility | 10 | `Body.Visible` | the whole first-person body is drawn |
| Eye forward or back of the body | 11.2 | `Body.EyeForward` | 0 m |
| Room-scale walking | 11.3 | `Body.RoomScale` | the body stays put; the head may leave it up to the head-in-wall limit (05 §7.2, 08 §4) |
| Seated eye raise; the head pose in standing mode | 11.4 | `Body.SeatedEyeRaise`; the mode and calibration are 22's (`Comfort.PlayMode`, `Comfort.StandingEyeHeight`, 22 §6) | eye height relative to the height at the last recenter (03 §2.1) |
| Real crouch drives the game's crouch | 11.5 | `Body.RealCrouch` | crouch on the control of 04 only |
| Control hand-back after the game's animations | 12 | active whenever any of 4, 11.2 to 11.5 is on | nothing to restore |
| Health on the wrist | 13 | `Hands.WristHealth` | health only on the game's HUD |
| Haptics | 14 | `Hands.Haptics.Enabled` | no haptics (04 §5.1) |

All keys are in §15. With every key at its default (20 §1.1, §1.3),
nothing in this document is installed
or runs (20 §1.1): no game site of §2 is hooked, no per-frame work is spent
beyond reading the settings and the runtime's poses. The one exception is
the aim pose actions (§16), which exist whatever the keys are so that
`Hands.Enabled` applies at once (§15.1); with the key off nothing uses
them.

## 2. Game facts

### 2.1 The frame and its stages [sure for the names and functions; likely for the order]

The game registers its per-frame work as named stages, each a pair of a
name string and a function, in one table built by the code around 0x3D6370.
The stages this document uses, in the order they are registered:

| Stage name (the game's string) | Function | Use here |
|---|---|---|
| `START_NEXT_FRAME` | 0x374FA0 | the start of a frame |
| `PLAYER_CAMERA_UPDATE` | 0x374FC0 | the player's camera |
| `CAMERA_UPDATE` | 0x3B4090 | camera behaviours; it reaches the stereo update call of 05 §3.1 (0x3B4100) |
| `CHARACTER_BUILD_START_PRIMARY_ANIMATION` .. `CHARACTER_BUILD_FINALISE_ANIMATION` | 0x3C9F30, 0x3C9F70 | animation of all characters |
| `CHARACTER_IK_AND_RAGDOLL` | 0x3C9FB0 | the equipment update and the final-pose step of every character (§2.3) |
| `CONTROLLER_UPDATE` | 0x383430 | controller (input) processing |
| `MAIN_THREAD_CHARACTER_PROCESSING` | 0x3CD750 | per-character logic on the main thread |
| `HAVOK_POST_CHARACTER_UPDATE` | 0x3CD6E0 | after the physics character update |
| `CALCULATE_POVS` | 0x3752C0 | the points of view the frame renders |
| `RENDER_STEREO_EYE` | 0x375520 | stereo rendering |

The table is the order of registration; that the stages run in this order
each frame is [likely] (the names describe a pipeline in this order).
Acceptance item 1 confirms it with thread ids and a frame number per
stage in a debug build.

`CHARACTER_IK_AND_RAGDOLL` (0x3C9FB0, spec label `CharacterIkStage`) does,
per frame [sure]:

1. for every character whose byte `+0x2FA` is set and whose equipment
   component `+0x1100` is not null, the equipment update 0x382710
   (`thiscall` on the component; call site 0x3CA02E, `e8 dd 86 fb ff`);
2. then the pose jobs (0x2B2F60, call at 0x3CA040) and the final-pose pass
   0x2A55F0 (call at 0x3CA04B, `e8 a0 b5 ed ff`).

So held items are updated from the pose **before** the final-pose step of
the same frame (an order the game has, which §4.2 must account for).

### 2.2 The player character [sure]

| Fact | Where | What |
|---|---|---|
| `CharacterRegistry` | 0x12F0C88 | global pointer to the game's character registry |
| `PlayerGetter` pattern | 0x00F6B0 (bytes `a1 88 0c 6f 01 8b 40 08`) | `list = [[0x12F0C88] + 8]`; the list's count is the dword at `list + 0x84`, its data pointer at `list + 0x80`; the player character is element 0 when the count is not 0, else there is none |
| Health, current | character `+0x33C` | unsigned 16-bit |
| Health, base maximum | character `+0x33E` | unsigned 16-bit |
| Health, maximum factor | character `+0x340` | float; maximum health = `trunc(u16(+0x33E) * f32(+0x340))`. Getters: 0x37A240 (maximum) and 0x37A260 (current), `thiscall`, plain `ret`, result in AX |
| Percentage | 0x093FC5 and other sites | the game itself computes `current * 100 / maximum` |
| Control object | character `+0x31C` | pointer; byte `+0x48` of it is set for the player-controlled character (tested by the crouch methods and by the weapon component's caller of `AimRayUpdate`, §2.5) |
| Animation component | character `+0x3F0` | the final-pose step runs on it (§2.3) |
| Equipment component | character `+0x1100` | see §2.5 |
| `CharacterRequestCrouch` | 0x3CAE20 (`56 8b f1 80`) | `thiscall` on the character, no arguments, plain `ret`. It sets the requested-crouch byte `+0x3E3A` to 1 when byte `+0x3E2E` is set and to 0 when it is not (r7: `+0x3E2E` is tested first, `80 be 2e 3e 00 00 00`), then to the value of byte `+0x3E34` when that is not 0. For the player (control object byte `+0x48` set) that is all; for other characters it also sets byte `+0x2AF` and raises the game's `CROUCHED` event. Callers: 0x0987E2, 0x287BE6, 0x3CFECF; 0x287B60 handles an event of type 7 whose value 1 stands (unless byte `+0x2B4`) and 2 crouches [sure for the code; the meaning of `+0x3E2E`, most likely "crouching allowed", is a guess] |
| `CharacterRequestStand` | 0x3CAE90 (`56 8b f1 8a`) | the same shape; sets `+0x3E3A` to 0 (or `+0x3E34`) |

The player's requested crouch state is therefore byte `+0x3E3A` of the
player character [likely: the player's crouch request that locomotion then
follows; acceptance item 14 confirms it against the game's crouch].

A game routine at 0x2E60FB compares a character's health percentage with
20 [sure for the comparison; the routine's owner and purpose are not
identified, so nothing here depends on it].

### 2.3 Animation, pose and skeleton

**Final-pose step [sure].** `FinalPoseStep` 0x2A5040 is a `thiscall` on the
character's animation component (character `+0x3F0`), no stack arguments,
plain `ret` (one return, at 0x2A5467; r7: 0x2A5553 is in the next
function, which starts at 0x2A5470), first bytes
`64 a1 00 00 00 00 6a ff`. It is reached only through a `cdecl` thunk
(`FinalPoseThunk`) at 0x2A55E0 (`8b 4c 24 04 81 c1 f0 03 00 00 e9 51 fa ff ff`: takes the
character, adds 0x3F0, jumps to 0x2A5040). The final-pose pass 0x2A55F0
queues one job per character whose byte `+0xF4C` is set, each job running
that thunk (the thunk's address is pushed as an immediate at 0x2A5655,
`68 e0 55 6a 00`), and then waits for the jobs. **The step therefore runs on
the game's job threads**, several characters in parallel, and the pass
returns only when all have finished.

**The pose [sure for the fields read below; likely for the Havok names].**
The animation component holds a pointer to the character's pose at
component `+0x24C` (character `+0x63C`), read by `FinalPoseStep`. The pose
has the layout of the Havok animation SDK's pose object (public SDK): `+0x00`
the skeleton, `+0x04` the array of bone transforms in local (parent) space,
`+0x10` the array of bone transforms in model space, `+0x1C` the per-bone
flag array (bit 1: model-space transform out of date; bit 3: marked during
a sync). A bone transform is 0x30 bytes: translation (4 floats), rotation
quaternion (x, y, z, w), scale (4 floats). The skeleton's parent-index array
(signed 16-bit, -1 for a root) starts at the data pointer held at skeleton
`+0x0C`. The model-space getter 0x7B9E80 (`thiscall` on the pose, one
argument: the bone index) makes a bone's model-space transform current by
walking its parents, which shows these fields. The skeleton's bone names
are in the bone array that follows the parent indices in the public layout
(skeleton `+0x18`, 8 bytes per bone, a name pointer first) [likely];
acceptance item 2 logs the names the mod finds. r7: the bone array's count
is the dword at skeleton `+0x1C`, the model-space array's count at pose
`+0x14`, and the skeleton's own name pointer is at skeleton `+0x08`
[likely, the public layout]; a name pointer's lowest bit is a flag of the
string and is cleared before use [likely]. A model-space transform is read
with aligned 16-byte loads, so any array of them handed to the game is
16-byte aligned [sure].

**The pose's hand-over to the renderer [sure, r7].** After making the
model-space array current (0x7BA260 on the pose), `FinalPoseStep` hands it
to the character model's render objects through `SkinSetPose` 0x28E9A0
(`55 8b ec 83 e4 f0 81 ec 54 01 00 00`), `thiscall` on a render object,
two stack arguments (a pointer to the model-space bone transforms, the bone
count), `ret 8`. `SkinSetPose` only reads the array: it converts each bone
to a 4x4 matrix (through 0x68EBE0, which reads only) into the render
object's own storage, computes the object's bounding box from them and
places the object's bone attachments; it keeps no pointer to the array.
Its two calls in `FinalPoseStep`:

| Call | Bytes | When | Render objects and pose |
|---|---|---|---|
| 0x2A51D8 | `e8 c3 97 fe ff` | byte `+0x3CD` of the animation component clear and the pointer at `+0x3D4` not null | the one render object at component `+0x3D8`, with the character's own model-space array |
| 0x2A536A | `e8 31 96 fe ff` | byte `+0x3CD` set | each of up to six part objects at component `+0x3E4 + 12 k` (k = 0..5); part 3 receives the character's own array, the others an array mapped from another skeleton when 0xA2C100 on the pointer at `+0x328` gives one, else the character's own |

Its other callers (0x28EE02, 0x293799, 0x2A8CD6, 0x2AC794, 0x2ACE45) are
outside the frame's pose step [likely: model set-up]. So everything drawn
of a character (the main view, its reflections and its shadow, which use
the render object's matrices [likely; acceptance item 10]) is posed from
the array passed here, while the game's own pose, collision, animation and
camera never see that argument.

**The game's own limb IK [sure for the shape].** A function at 0x29ED80
(`stdcall`, four arguments, `ret 0x10`, first bytes `55 8b ec 83 e4 f0`)
adjusts limbs of a pose; it does nothing when the weight (float at `+0x68`
of its first argument) is not above 0. It is called from 0x2A7986,
0x2A79A1, 0x2A79BC and 0x2A7F30. §4.2 item 7 states what it must not undo.
Whether the game also contains general IK solvers an implementation could
use for §4.2 is open (§22 item 23); the required result does not depend on
them. r9: it does (below).

**Model to world [sure for the code, r9].** `BoneWorldTransform` 0x2A2FB0
(`55 8b ec 83 e4 f0 83 ec 54`) is a `thiscall` on the animation component
(character `+0x3F0`) with three stack arguments, in push order last to
first: the bone index, a pose selector, a pointer to a 0x30-byte output
transform; `ret 0xc`. It maps the selector through 0x2A1CF0 to an index
`k` (selectors 8 and above lose 8 first), takes the pose at component
`+0x230 + 4 k` (the skeleton of that pose at `+0x210 + 4 k`), makes the
bone's model-space transform current through the getter 0x7B9E80 when its
flag bit 1 is set, and returns it in world coordinates:

    world translation = T + rotate(Q, model translation)   (scaled by S [likely])
    world rotation    = Q * model rotation

with `T` the vector at component `+0x270`, `Q` the quaternion (x, y, z, w)
at component `+0x280` and `S` the vector at component `+0x290`; so the
character's model-to-world transform is the bone-transform-shaped record at
component `+0x270` (character `+0x660`, `+0x670`, `+0x680`), 16-byte
aligned. World to model is its inverse: subtract `T`, rotate by the
conjugate of `Q`, divide by `S`. The pose `FinalPoseStep` edits and hands
to the renderer (component `+0x24C`, §2.3) is entry 7 of the array at
`+0x230` (0x230 + 7 * 4) [sure for the arithmetic]; that selector 7 gives
`k` = 7 is [likely] (acceptance item 3 logs it).

**The game's IK solvers [likely, r9; the shapes match the public Havok
animation SDK's CCD and two-joint solvers].** 0x7C1DA0
(`f3 0f 10 44 24 08 8b c1`) constructs a CCD solver in place (`thiscall`,
an iteration count and a float gain, `ret 8`; it writes the solver's
vtable, RVA 0x1029BA4, at `+0`, the count at `+8` and the gain at `+0xC`);
0x7C1DD0 (`55 8b ec 83 e4 f0 81 ec d4 00 00 00`) solves with it; 0x7C2630
(`55 8b ec 83 e4 f0 a1 30 61 be 01`) is the two-joint solve, taking a setup
record, the pose and a result. The game keeps per-layer two-joint setups
for its characters (the layer table 0x288E90 maps a layer to its index)
[likely]. An implementation may use these solvers or solve the two-bone
chain itself; §4.2's required result is the same.

**First-person skeletons [sure, game data].** The player's first-person
character uses the skeleton definitions `FEMALEFP` and `SPACESUITFP`
(`DATA\SKELETONDEFS\*.BML` in `DATA\GLOBAL\ANIMATION.PAK`). Their bone
groups (`DATA\REFERENCESKELETONS\*_BONE_GROUPS.TXT`) name, per side, the
bones this document uses:

| Group | Bones (left side; the right side is the same with `Right`) |
|---|---|
| shoulder | `LeftShoulder` |
| arm | `LeftArm`, `LeftArmRoll`, `LeftForeArm`, `LeftForeArmRoll`, `LeftForeArmRoll1` |
| hand | `LeftHand` |
| fingers | `LeftHandIndex1..3`, `LeftHandMiddle1..3`, `LeftHandPinky1..3`, `LeftHandRing1..3`, `LeftHandThumb1..3`, `LeftWeaponBone` |
| body | `Hips`, `Pelvis`, `Spine`, `Spine1`, `Spine2`, `Neck`, `Head`, `Head1`, `LeftUpLeg`, `LeftLeg`, `LeftFoot`, `LeftToeBase` |

The hand groups also list hand-pose blend values (`pose_LeftHand_Open`,
`_Closed`, `_GunGrip`, `_GunHold`, `_Point`, ...). The weapon attaches to
`RightWeaponBone` [likely, from the name]. Bones are found **by name** in
the player's skeleton at run time, never by a fixed index, because the two
skeletons (and any further first-person skeleton) need not number them the
same way.

### 2.4 The active camera [sure]

| Fact | Where |
|---|---|
| Camera manager | `[0x12F0C88] + 0x58` |
| Active camera used for aiming | camera manager `+0x1DC` (read by `AimRayUpdate`, §2.5) |
| Camera orientation | camera `+0x2C`: quaternion (x, y, z, w) |
| Camera position | camera `+0x3C`: x, y, z (world, metres) |
| `CameraPosition` | 0x0295B0, `thiscall`, (vec4* out), `ret 4`; writes +0x3C..+0x44 and w = 0 |
| `CameraForward` | 0x0295D0, `thiscall`, (vec4* out), `ret 4`; writes the camera's local +Z axis rotated by the orientation quaternion, with w = 0 |
| Behaviour lookup | 0x0293B0, `thiscall` on the camera, (type name, instance name), `ret 8`; `AimRayUpdate` asks it for `FollowBehavior` / `Follow` |

So the game's camera looks along its local +Z, with +Y up (05 §1: the world
is left-handed, +Y up, metres). In first-person play the active camera
carries the head pose the stereo device returned (05 §6, 03 §1)
[likely; acceptance item 3].

### 2.5 Weapons and the aim ray

**Items in the game's data [sure, `DATA\GBL_ITEM.XML`, names in
`DATA\TEXT\ENGLISH\UI.TXT`].**

| Data name | Kind and `weapon_type` | Shown as |
|---|---|---|
| `pistol` | weapon, `PISTOL` (ammunition: standard, `dum_dum_pistol_mag`, `tazer_pistol_ammo`) | Revolver |
| `shotgun` | weapon, `SHOTGUN` (standard, incendiary and solid-slug shells) | Shotgun |
| `flamethrower` | weapon, `FLAMETHROWER` (`flamer_fuel`) | Flame Thrower |
| `boltgun` | weapon, `BOLTGUN` (`bolt`) | Bolt gun |
| `cattleprod` | weapon, `CATTLEPROD` (`charge`) | Stun Baton |
| `emergency_axe` | weapon, `MELEE`, slot `MELEE_WEAPON` | the melee tool [likely the maintenance jack; its display name was not checked] |
| `flare` | light, held as `flare_held`, thrown as `flare`, `PressToActivate` | Flare |
| `pipe_bomb`, `molotov_cocktail`, `emp_mine`, `smoke_bomb`, `flashbang`, `noisemaker` | IED, `consume_when="OnThrowOrPlace"`, each with a held and a thrown object | Pipe Bomb, Molotov, EMP Mine, Smoke Bomb, Flashbang, Noisemaker |
| `small_medikit` | medikit, `HoldToActivate` | Medikit (not thrown) |
| `chem_light`, `wide_area_chem_light`, `explosive_mine`, `incendiary_mine` | marked "to be removed" in the file | not in normal play [likely] |

The game has no separate revolver class: the revolver is the `pistol`
weapon. The motion tracker (`motiontracker`) and the flashlight are not
items in the hands' sense (§6, §7).

**Weapon type enumeration [sure].** The `weapon_type` string becomes an
integer through 0x4B5F80, which compares the string's hash with hashes set
up at start-up from these names: `NO_WEAPON` 0, `FLAMETHROWER` 1, `PISTOL`
2, `SHOTGUN` 3, `MELEE` 4, `BOLTGUN` 5, `CATTLEPROD` 6, anything else -1.
The weapon object returned by the current-item getter holds this value at
`+0xD4` (the game compares it with 5 at 0x3AD27C and with 6 at 0x3C9C81).

**Current-item getter [sure].** `CurrentItem` 0x3829E0 (`80 b9 10 15 00 00
00`), `thiscall` on the weapon component: if byte `+0x1510` is set and the
slot pointer at `component + 4 * dword(+0x150C)` is not null, it returns
that item's vtable slot 5 (`+0x14`) result (the weapon object, or null for
items that are not weapons); otherwise null.

**`AimRayUpdate` [sure].** 0x382210 (`55 8b ec 83 e4 f0 83 ec 54`),
`thiscall` on the weapon component, no stack arguments, plain `ret`. Called
once per frame per character from the weapon component update 0x3CCFB0 at
0x3CD410 (`e8 fb 4d fb ff`), only when the component's owner (pointer at
`+0x1528`) has a control object (`+0x31C`) whose byte `+0x48` is set, so
only for the player; other characters take 0x3AB0B0 instead. It writes the
player's aim ray into the component from the active camera (§2.4):

| Component field | Value written |
|---|---|
| `+0x70` | ray start: the camera position |
| `+0x60` | ray direction: the camera's forward, normalised when its squared length is above 0 |
| `+0x90`, `+0x50` | ray end: start + forward * range, where range is the float at `+0x230` of the camera's `FollowBehavior` |

If the camera or its `FollowBehavior` is missing, it writes nothing.

**Fire alignment gate [sure for the mechanics; likely for the meaning].**
In the weapon component's update 0x3AB560, for the player (byte `+0x1C` of
the owner's object at `[+0x1528] + 0x64` set) and an item kind
(`+0x150C`) other than 6 or 9, when the component's state byte `+0x104` is
1 or 4, the game computes a vector between a point of the held item
(through 0x3A0E50 on the component's `+0xB4` sub-object) and the aim ray's
start (`+0x70`), and sets byte `+0x153C` to 1 when its length is above 0.75
(the float at 0xDC6780), else to 0. The comparison is `0f 2f 05 80 67 1c
01` at 0x3ABD6C followed by `76 11` (`jbe`) at 0x3ABD73. In every other case
the byte is set to 1. The firing decision at 0x3C3B7C requires the byte to
be set while the state at `[+0x152C] + 4` is 6, 7 or 8; the update at
0x3CCFEC also accumulates a timer at `+0x1540` while it is set. So the game
withholds firing in some aiming states unless that length test passes.
What the length measures exactly (most likely whether the held weapon is far
enough from the head to be "out", or clear of the ray start) is open
(§22 item 6); §5.3 states the required result.

### 2.6 Motion tracker [sure]

`TrackerScan` 0x431E40 (`55 8b ec 83 e4 f0`), `thiscall` on the tracker
object, no stack arguments, plain `ret`, returns a bool in AL. Its caller
0x4327D0 (`thiscall`, one float: the frame time, `ret 4`) counts a timer at
`+0x124` down and runs the scan when it expires or byte `+0x10F` is set,
then restarts the timer at 0.5 (the float at 0xDBB27C); so the scan runs
about twice a second. The scan derives the tracker's heading from the
horizontal direction vector at tracker `+0xF0` (x) and `+0xF8` (z):
`atan2` of the two, in degrees (the factor 57.2957764 at 0xDC3440). It also
drives the UI with the method names `ClearMotionBlips` and
`setTrackerDisabled`. Which code writes `+0xF0` is not found (§22 item 7).

### 2.7 Throwables and physics velocity [sure for the helper; open for the throw]

`SetBodyVelocities` 0x3EF590 (`55 8b ec 83 e4 f0 83 ec 3c`), `cdecl`, three
arguments: a physics object, a pointer to a linear velocity (x, y, z,
world metres per second) and a pointer to an angular velocity. For each of
the two it compares the new value with the body's current one (`+0x1B0`
linear, `+0x1C0` angular) and, when they differ, calls the body
interface's vtable slot at `+0x40` (linear) or `+0x44` (angular) on the
object's `+0xE0` sub-object. Nine call sites (0x3F16C8, 0x3F196F, 0x3F1BA0,
0x3F235E, 0x3F4F7E, 0x3F50F8, 0x3F51E1, 0x3F52F9, 0x3F8CC8). Which of them,
if any, gives a thrown item its launch velocity is open (§22 item 8).

The prompts the game shows for throwables are the localisation ids
`AI_UI_IED_HOLD_TO_AIM`, `AI_UI_IED_PRESS_TO_THROW`,
`AI_UI_IED_HOLD_TO_ACTIVATE` and `AI_UI_IED_PRESS_TO_ACTIVATE` (strings in
`AI.exe`, with a pointer table at 0x12422F4). The game's player actions
include `ACT_PLAYER_THROW_READY_ITEM` and `ACT_PLAYER_PLACE_READY_ITEM`
(strings at 0xEB0314 and 0xEB03F8) [sure for the strings].

### 2.8 First-person models [sure, game data]

Each level's `RENDERABLE\MODELS_LEVEL.BIN` lists the first-person
character models and, after each model's path, the names of its parts
[likely: material or sub-mesh names]:

| Model | Parts |
|---|---|
| `CHARACTERS\RIPLEY_FP` | `COL_FEMALE`, `FLIGHTSUIT`, `ZIP_TAGS`, `SHIRT`, `TRAINERS`, `WATCH`, `FP_ARMS`, `GORY_HOLE`, `FP_ARMS_INTERACT`, `WATCH_INTERACT` |
| `CHARACTERS\SPACESUIT_RIPLEY_FP` | `FP_GLOVE_STRAP`, `FPARM`, `SPACESUIT_LACES08`, `SPACESUIT_CLOTH01`, `SHADOW`, `FPARM_INTERACT`, `FP_GLOVE_STRAP_INTERACT` |
| `CHARACTERS\SPACESUIT_YELLOW_RIPLEY_FP` | `FP_GLOVE_STRAP02` (as listed in `HAB_AIRPORT`) |
| `CHARACTERS\RIPLEY_NAKED_FP` | `NAKED02` |
| DLC: `DALLAS_FP`, `PARKER_FP`, `E_RIPLEY_FP`, `AXEL_FP`, `HUGHES_FP`, `LINGARD_FP`, `RANSOME_FP`, `RICARDO_FP` | listed in `DLC\CHALLENGEMAP1`; their part lists there are partial |

So Ripley's normal first-person model has a separate **watch** part
(`WATCH`, and `WATCH_INTERACT`) on its arm; the space-suit and DLC models
have none.

### 2.9 Rumble output [sure for the code; likely for the audio origin]

The game drives XInput pad vibration in two places:

- `RumbleSetMotors` 0xC09280 (`f3 0f 10 44 24 04`), slot 0 of the vtable at
  RVA 0x11C1840 (whose constructor 0xC09380 stores the XInput user index as
  a byte at `+0x04`), `thiscall`, two floats (the low-frequency and the
  high-frequency motor level, 0 to 1), `ret 8`. It maps each level through
  a piecewise-linear curve to 0..65535 and calls `XInputSetState` (import by
  ordinal 3 from `XINPUT1_3.dll`, through the thunk at 0xC08A82). Slot 1
  (0xC09340) reports whether the device is connected through
  `XInputGetCapabilities`. A factory at 0xC08CA0 creates this object or a
  DirectInput counterpart (vtable at 0x11C1850). The names `Rumble_Small`
  and `Rumble_Big` are registered with the game's sound system at 0x2B7710,
  and `AI.exe` holds the names `RumbleImpulse`, `RumbleMixBus` and
  `RumbleBusBase`: the game's rumble is authored as audio-driven motion
  [likely].
- The pad device's vtable entry at 0xFE7698 (slot `+0x28`, 0x60F280) sets
  zero vibration on XInput users 0 to 3.

Which game events produce rumble, and whether `RumbleSetMotors` is called
while no physical pad is connected, are open (§22 item 9).

### 2.10 Signals v1 already provides

| Signal | v1 source |
|---|---|
| the player's first-person camera behaviour is active or not | FollowBehavior slots 0 and 1 (05 §6.1) |
| a cutscene or level fade is in progress | UI entity messages (05 §6.2), and the inference of 05 §3.2g |
| a loading screen is up | menu objects (05 §5) |
| an interaction is in progress and its kind; hiding | the logging of game script events (11 §5.3) |
| eye and body pivot positions each frame | the arguments of HeadClipFade at 0x34E2C1 (08 §4.1) |
| head pose, recenter reference | device slot 11, manager `+0x78` (03 §1, §2) |
| grip poses and linear velocities of both hands, converted, handed to the game thread | 04 §5.11 |

## 3. Hand poses in the game's world

### 3.1 Mapping [required]

Every hand feature uses the same mapping, derived here by the spec side
from the game's conventions (03 §1; 05 §1) and the runtimes' public pose
definitions; it is stated as a short algorithm because the result must
match exactly.

Inputs, all from one prediction (04 §5.11: the hands published with the
head pose the game read for the frame it is building):

- `H` = the head pose (rotation `Rh`, position `ph`) and `G` = a hand's grip
  pose (`Rg`, `pg`), both in the runtime's tracking space (right-handed, +Y
  up, -Z forward, metres);
- the active camera's world orientation `Qc` and position `Pc` (§2.4), read
  on the game thread in the same frame;
- the world scale `s` (03).

Result, the hand's pose in game world coordinates:

    d    = Rh^T (pg - ph)                 hand offset in the head's frame (x right, y up, -z forward)
    d'   = (d.x, d.y, -d.z) * s           the same offset in the camera's frame (x right, y up, +z forward)
    Pw   = Pc + rotate(Qc, d')
    Rrel = M (Rh^T Rg) M,  M = diag(1, 1, -1)   hand rotation relative to the head, mirrored into the left-handed camera frame
    Rw   = Qc * Rrel

Hands are placed relative to the eye the player sees from, so any change
that moves the view (03 §2, 04 §6, §11 of this document) moves the hands
with it, and a hand in front of the eyes appears in front of the eyes. Hand
rotations are never scaled; positions scale with `s` like the head (03 §1).
The grip pose's axes are the runtime's: on OpenXR `/input/grip/pose`
(origin in the palm on the handle's axis; -Z along the handle in the
direction the index finger points when straightened; +X normal to the
palm, away from the palm for the left hand and into it for the right)
(OpenXR specification, standard pose identifiers); on OpenVR the pose
actions of 04 §5.8.2, which are chosen to match it.

The mapping from the grip pose to the game's hand bone (an offset and a
rotation per side and per first-person skeleton) is a constant per
skeleton, not a feel value; r9: §3.5 derives it from the skeleton itself.

### 3.2 Smoothing [proposal]

No smoothing by default (`Hands.Smoothing` 0): runtime poses are already
filtered and predicted, and any extra filter adds latency to aiming. With
`Hands.Smoothing` above 0, positions and rotations pass through an adaptive
low-pass filter whose cut-off rises with hand speed (the public "1 Euro"
filter is one such), so a still hand stops trembling and a moving hand does
not lag; the setting scales the minimum cut-off from 10 Hz at 0 to 1 Hz at
1 (reason: about 1 Hz removes visible tremor at rest; 10 Hz is effectively
off at 90 Hz). Velocities used for throwing (§9) are never smoothed by this
filter.

### 3.3 Controller lost [proposal]

A hand is **tracked** when its grip pose is valid (04 §5.11) and, on
OpenVR, the pose is not marked as running out of range. When a hand stops
being tracked:

- for 0.3 s its last valid pose relative to the head is held (reason:
  covers brief occlusions without a visible jump);
- then, over 0.3 s, the hand blends to the game's own animated pose for that
  arm, and that hand's features fall back to their v1 behaviour (aim from
  the head, tracker facing the body, flashlight on the head) until tracking
  returns (reason: the game stays playable, 20 §4);
- when tracking returns, the hand blends back to the tracked pose over
  0.15 s (reason: short enough to be ready to aim, long enough not to snap).

A controller turned off, the runtime's dashboard open (no input focus,
04 §5.1) and a session that is not focused (OpenXR) count as lost. Nothing
here is NaN: an invalid pose is never converted (03 §1 rule).

### 3.4 Crossing threads

The poses are produced on the render thread (04 §5.11) and consumed on the
game thread (aim, tracker, room-scale) and on the job threads (the
final-pose step, §2.3). The consumer reads one consistent set (both hands,
their velocities and validity, and the head pose of the same prediction)
through the torn-free hand-over of 04 §5.11 and 12 §3; the job threads read
a copy the game thread made for the frame (§17), never the render thread's
live data, so every character job of one frame sees the same hands.

### 3.5 Grip to hand bone [required method, r9; the numbers are proposals]

The hand bone's origin is the wrist; the grip pose's origin is inside the
closed hand on the handle's axis (§3.1). The offset and rotation between
them come from the player's skeleton, measured once per skeleton when it
is first seen (and again when it changes, §2.3), from the local-space
translations of the hand's finger bones (pose `+0x04`, §2.3), which are in
the hand bone's own frame and do not change with animation:

- `I`, `M`, `R`, `P`: the local translations of `<Side>HandIndex1`,
  `<Side>HandMiddle1`, `<Side>HandRing1`, `<Side>HandPinky1` (the
  knuckles), and `T` of `<Side>HandThumb1`, when each of them has the hand
  bone as its parent; otherwise the skeleton is unsupported for hands
  (§4.6, logged once);
- knuckle line `a = unit(I - P)`: the handle's axis runs across the closed
  fist from the little finger out past the index finger, the grip pose's
  -Z (§3.1);
- forward `f = unit(M)`: from the wrist to the middle knuckle;
- palm normal `n = unit(f x a)`, its sign chosen so that `n . T > 0` (the
  thumb is on the palm side), then made orthogonal to `a`;
- grip origin in the hand bone's frame: the mean of `I`, `M`, `R`, `P`
  moved 0.025 m along `n` (towards the palm) [proposal: the handle sits
  under the first finger joints, about half a finger's thickness inside the
  knuckle line];
- grip axes in the hand bone's frame: -Z along `a`; +X along `-n` for the
  left hand (away from the palm) and `+n` for the right (into the palm), as
  §3.1 states the grip's +X; +Y completes the frame with the handedness of
  the game's space (the mirror `M` of §3.1).

The hand bone is then placed so that this grip frame coincides with the
hand's target of §3.1 converted to model space (§2.3, model to world): the
bone's model rotation is the target rotation times the inverse of the grip
frame's rotation in the bone, and its origin is the target position minus
the grip origin rotated by the bone's model rotation. With the game's
skeletons this puts the wrist about 7 to 12 cm from the grip origin,
towards the elbow [likely; acceptance item 3 logs both lengths per side].
No setting adjusts this: the derived frame is the rule, and an error found
in game is corrected here in the spec.

## 4. Tracked hands

### 4.1 What the player experiences

In first-person play the character's arms reach for the controllers: the
hands are where the real hands are, turned as they are, and held items
(weapons, flare, IEDs, medikit) sit in the in-game right hand. The shoulders
stay attached to the body; the elbows bend naturally. When a hand is out of
reach of the arm, the arm points towards it fully extended.

### 4.2 Required results

While `Hands.Enabled` is on and the conditions of §4.4 hold, for the
player character only (§2.2), every frame:

1. **Hand pose.** After all of the game's own changes to the arm bones for
   this frame, and before the pose is used for rendering, the `LeftHand`
   and `RightHand` bones are at the target poses of §3.1 (with the grip to
   bone offset of §22 item 2), to within 5 mm and 2 degrees when the target
   is within reach [proposal: below what a player can see at arm's length].
2. **Reach.** When the target is beyond the arm's reach (the summed lengths
   of upper arm and forearm), the arm is straight and points at it, and the
   hand keeps the target rotation (reason: a stretched arm is less wrong
   than a hand detached from its wrist).
3. **Chain.** Only `Shoulder` (at most 15 degrees of extra rotation
   [proposal: enough to reach across the chest, not enough to dislocate the
   shoulder visually]), `Arm`, `ForeArm` and `Hand` of each side are
   rotated, and the twist of the hand about the forearm's long axis is
   spread over the forearm's roll bones (`ForeArmRoll`, `ForeArmRoll1`) so
   the forearm does not visibly wring. How the twist is spread is the
   implementation's choice; the result is that no roll bone turns more than
   half the hand's twist relative to its neighbour.
4. **Elbow.** The elbow points down and outwards, away from the body,
   in the plane given by the shoulder, the hand and a hint below and behind
   the elbow (reason: the natural pose for a held object; a hint is the
   usual way to settle a two-bone solve).
5. **Fingers.** Fingers keep the game's animated hand pose (grip shapes
   come from the game's animation for the held item).
6. **Held items.** The item in the right hand is drawn attached to the
   moved hand **in the same frame**. The game updates equipment before the
   final-pose step (§2.1), so the attachment as the game computes it
   belongs to the pose before the edit; how the result is reached is the
   implementation's choice (§22 item 3 says where to look). r9: the
   attachment reads the hand through `BoneWorldTransform` at 0x3B4E06
   with selector 6 (§22 item 3); reading the final pose (selector 7) and
   running the equipment update 0x382710 again for the player after the
   edit gives the result [likely].
7. **The game's own limb IK** (0x29ED80) does not move the arm chain away
   from the result while tracked hands are active (§22 item 4 says where
   to look). r9: for the player's first-person skeleton (`FEMALEFP`,
   `SPACESUITFP`, matched by the skeleton's name, §2.3) it is not run at
   all while tracked hands are active; for every other character and with
   hands off it runs as in v1.
8. **Other characters** are never edited.
9. **Model-space consistency.** After the edit the pose's local and model
   transforms agree (the flags of §2.3 are left so the game recomputes what
   it needs), so ragdoll, attachments and rendering see one pose.

Where the edit is made is the implementation's choice among the verified
sites: inside `FinalPoseStep` for the player (on its job thread, §17), or
at a point after the pass in `CHARACTER_IK_AND_RAGDOLL`. It must not add a
wait between jobs.

### 4.3 Parameters

| Parameter | Value | Reason |
|---|---|---|
| target precision | 5 mm, 2 degrees | §4.2 item 1 |
| extra shoulder rotation | up to 15 degrees | §4.2 item 3 |
| hold, blend times | §3.3 | §3.3 |
| grip to hand-bone offset | measured per skeleton | game fact, §22 item 2 |

### 4.4 When tracked hands are active

Active only while all hold: the stereo device runs (state 2); the player
character exists; a FollowBehavior is tracked and active (05 §6.1); no
cutscene or level fade is in progress (05 §3.2g); no loading screen; the
interaction state (11 §5.3) says no interaction, or an interaction of a
kind the hands may join (none in r1 [proposal: 23 decides which
interactions become physical]); the player is not hiding (§12); the player
is alive (health above 0, §2.2). Otherwise the arms play the game's
animation, with the blends of §3.3 on each change. A hand that is not
tracked follows §3.3.

### 4.5 Off

With `Hands.Enabled` off: no pose edit, no hook of §2.3, the arms are the
game's, exactly v1.

### 4.6 Edge cases

| Situation | Result |
|---|---|
| Menus, virtual screen, videos | not active (no FollowBehavior or the stereo mode is 1) |
| Cutscene, level fade, loading | not active; blends back after (§12) |
| Death | not active from the frame health reaches 0 |
| Interaction (lever, terminal, keypad, ladder, ...) | not active in r1 |
| Hiding (locker, under a desk) | not active |
| Space suit levels | active with `SPACESUITFP`; bone names are the same (§2.3) |
| DLC characters | active only when the skeleton has the bones of §2.3 by name; otherwise not active and logged once per skeleton |
| Headset off | as with it on; hands may be off (§3.3) |
| Runtime None | no stereo device: nothing (20 §1.2) |

## 5. Aiming from the hand

### 5.1 What the player experiences

The shot goes where the weapon in the hand points, not where the head
looks: the player can look one way and shoot another, aim around a corner
with only the hand out, or aim down the barrel by raising the weapon to the
eye. The game's own spread, recoil, damage and ammunition rules are
unchanged.

### 5.2 Required results

While `Hands.AimFromHand` and tracked hands (§4.4) are both active and the
right hand is tracked, for every frame in which `AimRayUpdate` (§2.5) runs
for the player, the component's aim ray after it, and before anything else
reads it that frame, is:

| Field | Value |
|---|---|
| start (`+0x70`) | the aim origin of the class (table below), in world coordinates |
| direction (`+0x60`) | the aim direction of the class, unit length |
| end (`+0x50`, `+0x90`) | start + direction * the same range the game uses (`FollowBehavior` `+0x230`) |

Per weapon class (the weapon type at `+0xD4` of `CurrentItem`'s result,
§2.5):

| Class (type) | Aim origin | Aim direction | Reason |
|---|---|---|---|
| `PISTOL` (2): the revolver | the barrel's muzzle point | the barrel's axis | the player aims along the sights |
| `SHOTGUN` (3) | the muzzle point | the barrel's axis | as above |
| `BOLTGUN` (5) | the muzzle point | the barrel's axis | as above |
| `FLAMETHROWER` (1) | the nozzle point | the nozzle's axis | the flame must leave the nozzle |
| `CATTLEPROD` (6): the stun baton | unchanged (the game's head ray) | unchanged | contact weapon: hits come from the game's melee (§8) [proposal] |
| `MELEE` (4) | unchanged | unchanged | §8 |
| no weapon (0, or null): throwables, flare, medikit | the right hand's aim pose origin | the right hand's aim pose forward | throws and placement follow the hand when §9 is off; nothing visible otherwise |

"Barrel's axis" and "muzzle point" are those of the **weapon model as drawn
in the hand** after §4.2: the weapon's own transform where the game
exposes it (§22 item 5), otherwise the right hand's **aim pose** (OpenXR
`/input/aim/pose`; OpenVR a pose action bound to the controller's pointing
pose [likely `/pose/tip` on Index, Vive and WMR and `/pose/openxr_aim` on
Touch; to be confirmed per controller]) with a per-class offset from the
hand to the muzzle along it. In both cases the visual barrel and the ray
must agree: a shot at a wall 5 m away lands within 5 cm of the barrel's
extension [proposal: about half a degree, below what a player notices with
iron sights].

### 5.3 Firing when the weapon is not aligned with the view

The game assumes the weapon and the view share one ray. With hand aiming
they do not, so the game's fire alignment gate (§2.5) must not withhold
firing because of where the hand is relative to the head. Required result:
while hand aiming is active, in the states where the game runs that test,
byte `+0x153C` of the player's weapon component reads 1 after the test,
unless the game would also have set it to 0 with the weapon held at the
head (§22 item 6 decides whether a further condition is needed). Nothing
else of the game's firing rules (ammunition, reload, the weapon being
raised, interaction states) changes.

How the result is reached is the implementation's choice under 12 §4: any
change at the test is installed only while `Hands.AimFromHand` is on, or is
inert when off (20 §1.1), and the bytes of the test at 0x3ABD6C (§2.5) are
checked before it is made.

### 5.4 Holding a weapon "up"

v1's Aim control (04 §5.8.5, right grip by default) keeps driving the
game's aim action; with hand aiming the player raises the weapon in the
game (the game's aimed state: steadier, slower movement) by holding it
[proposal: the grip is already "hold the gun" in VR, and the game's aimed
state carries its own accuracy rules that should stay]. Hip fire stays the
game's.

### 5.5 Off, and fallbacks

Off (or the right hand not tracked, §3.3): `AimRayUpdate` runs unchanged,
the gate is the game's, exactly v1. Hand aiming never applies to other
characters.

### 5.6 Edge cases

| Situation | Result |
|---|---|
| Weapon switch | the class is read each frame; the next frame aims by the new class |
| Reload, holster, ammunition empty | the ray still follows the hand; the game decides whether firing happens |
| Vents (crouched, narrow) | as normal; the weapon may clip through vent walls (§22 item 10) |
| Interaction, hiding, cutscene | tracked hands inactive (§4.4): the game's ray |
| Aim ray end inside geometry close to the muzzle | the game's own hit test applies from the new start |

## 6. Motion tracker in the left hand

### 6.1 What the player experiences

Raising the tracker (v1's Tracker control, left grip) puts it in the left
hand, where the controller is; the player reads it by looking at the hand,
and the tracker's cone points where the hand points.

### 6.2 Required results

While `Hands.TrackerInHand` and tracked hands (§4.4) are active and the left
hand is tracked:

- the tracker model is in the left hand through §4.2 (the game's own
  tracker animation places it in the left hand [likely]; §22 item 11);
- each tracker scan (§2.6) derives its heading from the left hand: the
  heading the scan computes equals the horizontal heading of the left
  hand's aim direction [proposal: the cone follows the hand the player
  holds out; vertical tilt is ignored because the game's tracker is a 2D
  display]. Where and when the facing the scan reads is set is the
  implementation's choice, provided no game write comes between it and the
  scan (§22 item 7);
- when that horizontal length is below 0.1 (the hand points nearly
  straight up or down), the previous facing is kept (reason: the heading is
  undefined there; no NaN).

Off, or the left hand not tracked: the scan runs unchanged (v1).

## 7. Flashlight aimed by a hand

### 7.1 What the player experiences

The flashlight beam follows the chosen hand: the player can light a corner
while looking elsewhere.

### 7.2 Required results

`Hands.FlashlightAim` is `"Head"` (v1), `"LeftHand"` or `"RightHand"`. With
a hand chosen, that hand tracked and tracked hands active (§4.4): the beam's
origin is the hand's grip position (§3.1) and its direction the hand's aim
direction. Default `"Head"`. With `"LeftHand"` while the tracker is raised
in the left hand (§6), the beam stays on the head [proposal: a hand holding
the tracker cannot also point a torch].

The game object that carries the beam has not been verified: the player's
torch is a script entity named `PlayerTorch` with a parameter
`TorchDynamicMovement` (strings in `AI.exe`) [sure for the names only];
where its light's transform is set is open (§22 item 12). Until that is
verified the setting is accepted, stored, and has no effect, and that is
logged once.

## 8. Melee by motion

### 8.1 What the player experiences

With the melee tool (`emergency_axe`, type `MELEE`) or the stun baton
(`CATTLEPROD`) in hand, a real swing of the right hand makes the character
swing; the game decides hits and damage as before.

### 8.2 Required results

While `Hands.MeleeByMotion` and tracked hands are active and the current
weapon type (§2.5) is `MELEE` or `CATTLEPROD`:

- a **swing** is the right hand's speed exceeding 2.0 m/s and then falling
  below 1.0 m/s within 0.5 s [proposal: a deliberate swing exceeds 2 m/s,
  walking hand motion stays below it; the fall marks the end of the swing];
- each swing produces exactly one press of the game's fire action (the
  action v1 drives with Fire, 04 §5.8.5), held for 100 ms [proposal: long
  enough for the game to register a press at any frame rate down to 20
  fps], in addition to the Fire control, which keeps working;
- swings are ignored for 0.6 s after one fired [proposal: about the length
  of the game's own melee swing; §22 item 13 replaces it with the game's
  action duration once that is known];
- a swing by hand hits what the same swing made with the Fire control
  would hit: the game's hit test, damage and timing decide, as for a press
  of Fire. Whether that requires the right arm to play the game's melee
  animation while the swing runs (tracked hands blending out and back as
  §3.3) depends on what the game's hit test follows, which is open (§22
  item 13).

Off: melee on the Fire control only (v1).

## 9. Throwing by motion

### 9.1 What the player experiences

With a throwable in hand (flare, pipe bomb, Molotov, EMP mine, smoke bomb,
flashbang, noisemaker; §2.5), the player throws it with a real throwing
motion: the item leaves the hand with the hand's speed and direction.

### 9.2 Required results

While `Hands.ThrowByMotion` and tracked hands are active and the current
item is one of the throwables of §2.5:

- **Arm** [proposal]. The throw is armed while a hold control is held and
  released when it is let go; the hold control is the Aim control (right
  grip by default). Reason: a held grip matches holding an object in the
  hand, and the game readies a throwable on its aim action (its prompt id
  `AI_UI_IED_HOLD_TO_AIM`, §2.7 [likely, from the id]), so no control
  changes meaning. Arming enters the game's aim-to-throw
  state through the game action the gamepad uses for it.
- **Release.** Letting go of the hold control throws when the right hand's speed
  at that moment is at least 1.5 m/s [proposal: a gentle lob is above it, a
  hand at rest is far below]; below it the throw is cancelled and the item
  stays in hand (reason: opening the hand by accident must not throw a
  pipe bomb at the player's feet).
- **Velocity.** The thrown object's initial linear velocity in the world is
  the release velocity of the hand, mapped as in §3.1 (rotation only:
  `rotate(Qc, M Rh^T v)` scaled by `s`), multiplied by
  `Hands.ThrowStrength`. The release velocity is the runtime's
  reported linear velocity of the right grip, the largest-magnitude sample
  over the last 50 ms before the release [proposal: the hand decelerates as
  the fingers open, so the peak just before release is what the player
  intended].
- **Where.** The object leaves from the hand's position, not from the
  game's usual throw point, when the game allows the spawn point to be set
  (§22 item 8); otherwise only the velocity is replaced.

The velocity is applied at the game's own launch of the object (the site
of §22 item 8, candidate helper `SetBodyVelocities`, §2.7), replacing the
game's launch velocity for that one object and for no other physics object.

Off, or no tracked right hand: the game's own throw on the Fire control
(v1). Placing (`ACT_PLAYER_PLACE_READY_ITEM`) is never replaced.

### 9.3 Parameters

| Parameter | Value | Reason |
|---|---|---|
| release threshold | 1.5 m/s | §9.2 |
| velocity window | 50 ms | §9.2 |
| `Hands.ThrowStrength` | default 1.0, range 0.5 to 2.0 [proposal] | players with little room, or seated, cannot swing fully and need a throw to carry as far as the game's own; 1.0 throws with the hand's real speed; 2.0 lets a short seated flick reach the distance of a full throw, and 0.5 tames a player who throws hard |

## 10. Body visibility

### 10.1 What the player experiences

The player can hide parts of the first-person body that would conflict with
their own: the body (legs, torso), the arms, or everything.

### 10.2 Required results

`Body.Visible`:

| Value | Drawn | Not drawn |
|---|---|---|
| `"Full"` (default, v1) | the whole first-person model | nothing |
| `"ArmsAndHands"` | arms and hands (and the held item) | legs, hips, torso, neck, head |
| `"HandsOnly"` | hands and the held item | everything else |
| `"None"` | the held item | the whole first-person model |

"Not drawn" means no visible geometry of those parts in either eye, in the
main view, its reflections (07) and its shadow [proposal: hiding a body but
keeping its shadow looks wrong; if the shadow cannot be kept consistent it
is hidden too]. Parts are identified through the skeleton's bone groups
(§2.3): a vertex belongs to the hidden set when its bones are all in hidden
groups. A permitted method [proposal: needs no table of the game's meshes]
is to collapse the hidden bones' model-space transforms to zero scale at
their parent's position after the final-pose step, which leaves the other
bones untouched; another is to skip the hidden parts' draws using the part
names of §2.8.

r7 makes the first method exact [proposal for each rule; reasons given]:

- **Where.** The collapse is made in a copy of the model-space array that
  is handed to `SkinSetPose` in place of the game's at its two calls in
  `FinalPoseStep` (§2.3), and only when the array is the player
  character's own (the same array and skeleton as the player's pose). The
  game's pose is never written [reason: animation, collision, the camera
  and the next frame's blending read the game's pose; the render objects
  are the only readers of the copy]. A part posed from another skeleton's
  array (§2.3, the mapped pose) is left as the game poses it and that is
  logged once (§22 item 27).
- **Groups.** Each bone takes the group listed for its name in §2.3, after
  removing a namespace prefix up to the last `:` (the game's skeletons name
  bones `FemaleFP:LeftHand` [likely, from the mirror files]); a bone not
  listed takes the group of its nearest listed ancestor, and a bone with
  none is body. The fingers take their hand's group. `LeftWeaponBone` and
  `RightWeaponBone` form a group of their own that is always drawn
  [reason: the held item is placed on it in every value of the table].
  Drawn per value: `Full` everything; `ArmsAndHands` shoulders, arms,
  hands, weapon bones; `HandsOnly` hands and weapon bones; `None` the
  weapon bones.
- **Collapse point.** A hidden bone is collapsed to the joint of its
  nearest drawn bone by the number of parent links between them (several at
  the same distance: the mean of up to four of them); the joint is the
  drawn bone's own position when the drawn bone is below the hidden one,
  and the position of the hidden bone that hangs from it when the drawn
  bone is above [reason: a vertex weighted between a drawn and a hidden
  bone then stays at the edge of the drawn part instead of being pulled
  across the body, so the cut is at the shoulder or the wrist]. With no
  drawn bone the whole skeleton collapses at its first root.
- **Transform.** A collapsed bone has the collapse point as translation,
  the identity rotation and a scale of 0.001 on each axis [reason: exactly
  zero gives the renderer singular normal matrices; a millimetre is not
  visible].
- **Required bones.** The skeleton must have, by name, `Spine`,
  `LeftShoulder`, `RightShoulder`, `LeftArm`, `RightArm`, `LeftHand` and
  `RightHand`; a skeleton without one of them, with more bones than the
  implementation can hold (at least 256), or whose parent indices are not
  a tree is drawn whole and that is logged once per skeleton.
- **When.** Only while the stereo device runs (20 §1.2); the plan is made
  on the game thread when the skeleton or the value changes (§17).
- **A skeleton not readable yet** [r8, project decision from a tester's
  log]. When the player's skeleton cannot be read (no bones, no bone or
  parent array, more bones than the implementation holds, or a bone
  without a name), the body is drawn whole and the skeleton is read again
  once a second until it can be read, then the plan is made as above.
  Required: a value set while the game has no readable player skeleton
  (the pause or main menu, a level load) or present in `mothervr.json` at
  start takes effect as soon as the skeleton can be read, without the
  player changing the setting again. A new skeleton, pose or value is read
  at once. [reason: the setting is most often changed in a menu, and a
  value read once at the moment of the change stayed whole for the rest
  of the session; one read a second costs a walk of at most a few hundred
  bones on the game thread and nothing per frame.]

Whichever method is used, it must work for `FEMALEFP` and
`SPACESUITFP` (and each DLC skeleton that has the bones by name), must not
change collision, animation or the game's own visibility tests, and must
not hide other characters.

Independent of `Hands.Enabled`: a seated player may hide the body without
tracked hands.

### 10.3 Edge cases

Cutscenes: the setting applies as in gameplay whenever the first-person
model is drawn [proposal: a body that appears in a cutscene but not in play
is more jarring than a consistent one]. Mirrors and reflective surfaces
(07): as §10.2. The `_INTERACT` parts (§2.8) follow their non-interact part.

## 11. Head and body

### 11.1 Model [required, the answer to 20 §10 item 3 from this document's side]

The game places the camera at the body's eye point plus the head pose
relative to the recenter reference (03 §1, §2; 02 §6.3); the body pivot and
the eye are what HeadClipFade compares (05 §7.2). Every feature of this
section changes only two things the mod already owns: **the head pose it
returns** (device slot 11, 03 §1) and **the reference** (manager `+0x78`,
03 §2), plus game input through the gamepad layer (04). None moves the
character by writing its physics state. The combination, for each frame:

    head pose returned = v1 pose (03 §1)
                         + eye forward offset along the reference's forward (11.2)
                         + height adjustment (11.4)
                         - room-scale origin shift (11.3)

in tracking space before the conversion of 03 §1, each term 0 when its
feature is off. The terms are summed in runtime metres and the sum is then
scaled by the world scale `s` (03 §1 scales the whole translation; 22
§6.3) [proposal: the shift and height are real-world metres; the eye
forward offset is a body-relative metre that should also scale with the
world]. Only the place of `s` matters; the order of the sum does not.

### 11.2 Eye forward or back of the body

`Body.EyeForward` metres (default 0, range -0.20 to +0.20 [proposal:
beyond 0.2 m the eye leaves the head of the game's body]): the eye is moved
by that distance along the reference's horizontal forward (positive moves
it forward of the body, so the player sees less of their own chest and the
weapon sits further back in the view). Hands follow (§3.1 places them
relative to the eye). The head-in-wall test (05 §7.2) sees the moved eye;
an offset within the range keeps the eye inside the 0.5 m of that test.

While an interaction whose screen the game frames for a fixed eye point is
running, the eye forward offset is not applied, so that screen is seen as
the game frames it [proposal: a moved eye would crop or misalign a screen
composed for the body's own eye point]. Which interaction kinds (11 §5.3)
these are is to be listed from in-game observation (§22 item 24); until
then the offset applies in every interaction.

### 11.3 Room-scale

**Player experience.** With `Body.RoomScale` on, walking in the play area
moves the character in the level; the game's walls stop the body, and
leaning into a wall the body cannot pass blacks the view out as the
head-in-wall rule says (05 §7.2, 08 §4).

**Required results.** Let `o` be the horizontal offset of the real head
from the body, in tracking metres: the head pose's horizontal translation
relative to the reference, minus the accumulated origin shift.

- **Follow.** When `|o|` exceeds 0.20 m [proposal: leaning and small steps
  stay pure head motion; a real step triggers a follow] and the player is
  not using the move stick (|Move| below 0.1), the body walks towards the
  head through the game's own movement: the mod writes a movement input on
  the move action (04 §5.8.5 map 0 analog 0 and 1), in the body's frame,
  in the direction of `o`, with magnitude rising linearly from 0.3 at
  0.20 m to 1.0 at 0.50 m [proposal: the game's own walk and collision
  apply, including its footstep noise, which is honest to a stealth game].
  It stops when `|o|` falls below 0.05 m [proposal: hysteresis so the body
  does not oscillate].
- **Shift.** While a follow is running, every metre the body moves in the
  world (the change of the body pivot of 08 §4.1 between frames, rotated
  into tracking space by the inverse of the body's yaw and divided by `s`)
  is added to the origin shift, so the eye stays where the real head is and
  does not move twice. Body movement while the player uses the stick is not
  added (the stick moves body and eye together, as in v1).
- **Blocked.** When the body cannot follow (it moved less than 10 % of the
  commanded distance over 0.5 s [proposal: a wall or a closed door]), the
  follow stops; `o` keeps growing as the player walks and the head-in-wall
  rule applies.
- **No vertical shift**: room-scale never changes height (that is 11.4 and
  11.5).

The origin shift is reset to 0 at each recenter (03 §2.2) and at each
control hand-back (§12).

**Off**: no follow, no shift; v1.

**Until two facts are established (r7).** The follow writes the move action
in the body's frame and the shift rotates by the body's yaw; neither the
frame in which the game reads the move action (camera, body or world;
§22 item 25) nor where the body's yaw in tracking space can be read (§22
item 26) is established. Until both are, `Body.RoomScale` on changes
nothing and the log says once that room-scale is off in this build and
why [reason: a follow in the wrong frame walks the body away from the
head, which is worse than no follow].

### 11.4 Seated and standing height

The mode (`Comfort.PlayMode`), the calibrated standing eye height `h`
(`Comfort.StandingEyeHeight`), when a mode change takes effect and how the
calibration is taken are 22's (22 §6.1, §6.2; one owner per setting, 20
§5). This document adds only:

- **Seated eye raise**: in seated mode the height adjustment of §11.1 is
  `Body.SeatedEyeRaise` metres added to the eye height (default 0, range
  -0.50 to +0.50 [proposal: covers a low chair to a high stool]); in
  standing mode it is 0. Seated play otherwise keeps v1's height rule (03
  §2.1, 22 §6.1).
- **Standing**: the height adjustment is what 22 §6.1 requires (the
  vertical offset the game receives is the head's height above the floor
  minus `h`); real crouch (§11.5) and the control hand-back (§12) use the
  same `h`. With no calibration, standing behaves as seated (22 §6.1) and
  real crouch is inactive.

### 11.5 Real crouch

**Player experience.** In standing mode with `Body.RealCrouch` on, crouching
for real makes the character crouch (quieter, lower, able to enter vents);
standing up makes it stand, where the game allows.

**Required results.** With `h = Comfort.StandingEyeHeight` (calibrated,
22 §6.2):

- the character is made to crouch when the head's height falls below
  `0.78 h` and to stand when it rises above `0.86 h` [proposal: a deliberate
  crouch lowers the eyes by a quarter or more, a lean or a peek by less
  than a seventh; the gap prevents chatter];
- "made to crouch / stand" means: when the player's requested crouch byte
  (§2.2, `+0x3E3A`) differs from the wanted state, the mod presses the
  game's crouch action once (04 §5.8.5 map 0 action 0x09) for 100 ms
  [proposal: the game's crouch is reached through its own input, so its
  rules (no standing up inside a vent) apply], and does not press again for
  0.5 s [proposal: lets the game act on the press before it is judged];
- the view height follows the real head through the transition: the game's
  own crouch lowers its eye point, so while the game's crouch is engaged by
  real crouch, the head height the game receives is raised by the
  difference between the game's standing and crouched eye heights (a game
  value to measure, §22 item 14), so the eye stays within 5 cm of where the
  real head is [proposal: no double drop];
- when the game refuses to stand (in a vent), the character stays crouched
  and the view is limited by the game as usual; the mod retries at most
  every 0.5 s while the player stands.

r7 details [proposal for each; reasons given]:

- **The press** is the v1 crouch control (04's left trigger, mapped to map
  0 action 0x09) held for the 100 ms, added to what the player presses
  (merged by OR) [reason: the game's crouch then behaves exactly as for the
  player's own press].
- **One press per crossing.** A crouch press is made once for each crossing
  of `0.78 h`; a stand press is retried every 0.5 s only after a press of
  the session was followed by the requested crouch byte [reason: a byte
  that does not follow the game's crouch must never make the mod toggle the
  crouch again and again]. A press the byte does not follow within 1 s is
  logged once (§22 item 28). A crouch the player makes with the control
  while standing for real is not undone until the next crossing.
- **The drop.** The game's data gives the first-person camera's vertical
  offset as `first_person_camera_offset_y` 0.01 standing and
  `first_person_camera_crouch_offset_y` -0.62 crouched (`#Camera` section
  of `DATA\GLOBALCONSTANTS.TXT`; the names are entry 0x14 onwards of the
  constants table at 0x123F7C0, 0x58 bytes per entry), so the game's crouch
  lowers the eye by 0.63 game metres [likely: the reader of the values was
  not located; acceptance item 13 measures it]. While the game's crouch was
  engaged by a press of real crouch, the head height the game receives is
  raised by 0.63 game metres (so 0.63 / `s` in runtime metres before the
  world scale of §11.1), eased in and out over 0.3 s [reason: the game
  moves its camera over a short time too; a step would jump].
- **No head height**, no presses: without a floor reference, without a
  valid head pose or while the headset is not in use (§19).

The v1 crouch control keeps working; a press of it while real crouch is on
toggles the game as before, and real crouch re-syncs at the next threshold
crossing [proposal: the player always has a manual override]. `Input.ToggleCrouch`
(06 §1, stored only in v1) keeps no effect.

Off, or seated: crouch by control only (v1).

## 12. When the game hands control back

### 12.1 What the game does

The game takes the player's body away from the player in several ways, each
seen through a v1 signal (§2.10):

| Episode | Start | End |
|---|---|---|
| cutscene or level fade | `cutscene_visibility_start`, `level_fade_start` (05 §6.2) | the finish messages, or the FollowBehavior active again (05 §3.2g) |
| loading (and death and reload, which load) | loading screen Open (05 §5) | loading screen Close |
| camera taken by another camera | FollowBehavior deactivated (05 §6.1) | a FollowBehavior activated |
| interaction (ladder, keypad, terminal, hacking, cutting, rewire, suit change, generic) | event tap `_start` (11 §5.3) | event tap `_finish` / `stop_interaction` |
| hiding | `hiding_visibility_start` | `hiding_visibility_finish` |

Scripted player animations that are none of these (a grab, a fall, a melee
kill) have no verified signal yet; the game's player actions such as
`ACT_PLAYER_MELEE_KILL`, `ACT_PLAYER_TRAP_VICTIM`, `ACT_PLAYER_GRAPPLE_BREAK`,
`ACT_PLAYER_IMPACT_STANDING` and `ACT_PLAYER_DEATH` exist as names in
`AI.exe`, and the script entity `CMD_PlayAnimation` has a parameter
`PlayerAnimDrivenView` [sure for the names only]; how to observe them is
open (§22 item 15).

### 12.2 Required results

During any episode: tracked hands, hand aiming, the hand-held tracker,
melee and throwing by motion, room-scale follow and real crouch presses are
inactive (§4.4); the eye forward offset stays applied except as §11.2 says.

At the end of each episode, before the first frame the player controls the
body again:

1. **Room-scale:** the origin shift is set so that `o` = 0 (the body is
   where the head is now) [proposal: the game may have moved the body; the
   player's real position is the new truth].
2. **Height:** unchanged in standing mode (calibration holds); in seated
   mode, unchanged.
3. **Crouch:** the requested crouch byte is compared with the real head
   height and re-synced by one press if they differ (§11.5), not before
   0.5 s after the end [proposal: the game often finishes an animation
   crouched or standing on its own; waiting lets it settle].
4. **Hands:** blend back to the tracked pose as §3.3 says for returning
   tracking (0.15 s).
5. **Forward:** the recenter reference is not changed by the episode's end,
   except where v1 already queues a recenter (a loading screen closing, 03
   §2.2) [proposal: a recenter at every cutscene end would turn the world
   under the player].

The goal: after any episode, the view is at the real head's position over
the body, the hands are in the controllers, and the character's crouch
matches the player's, within one second.

r7: an episode is any frame in which §4.4's activity (computed as if
`Hands.Enabled` were on) is not active for a reason other than the device
not running or a setting; it ends at the first active frame, and the end is
logged (DBG) with the reason it started with [reason: one definition of an
episode for every feature of this section]. Items 1 to 5 apply then; item 1
waits for room-scale (§11.3).

## 13. Health on the wrist

### 13.1 What the player experiences

Turning the left wrist up shows the character's health on a small display
on the wrist, without the HUD.

### 13.2 Required results

With `Hands.WristHealth` on and the left hand tracked:

- **Value**: the player's health fraction `current / maximum` (§2.2), read
  on the game thread each frame; 0 when there is no player.
- **Display** [proposal]: a small panel attached to the left controller,
  4 cm wide [proposal: the size of a watch face], over the inside of the
  wrist where the game's `WATCH` part is on `RIPLEY_FP` (§2.8), shown only
  while the panel faces the head within 40 degrees [proposal: a glance at
  the wrist, like reading a watch]. It shows the fraction as a bar whose
  colour moves continuously from green at full health to red at none
  [proposal: a continuous ramp needs no thresholds and reads at a glance],
  and the bar blinks at 1 Hz at or below one fifth [proposal: the last
  fifth is where a player should heal before the next hit; one blink per
  second is noticed in peripheral vision without being frantic].
- **Drawn by the runtime** [proposal: needs no change to the game's
  rendering and works with every first-person model, including those
  without a watch]: OpenVR, an `IVROverlay` overlay positioned relative to
  the left controller (`SetOverlayTransformTrackedDeviceRelative`); OpenXR,
  a quad composition layer in the left grip action space. Its pixels come
  from a small texture the mod draws (128x64 [proposal: legible at 4 cm]),
  updated only when the shown value or blink phase changes.
- Shown in first-person play only (not in menus, cutscenes, loading); not
  shown when the left hand is not tracked.

Whether health could be shown on the game's own `WATCH` part instead is
open (§22 item 16).

## 14. Haptics

### 14.1 What the player experiences

The controllers vibrate for the game's feedback: the game's own rumble
(whatever the game would play on a pad), being hurt, the motion tracker's
pings, firing, and the start and end of hacking.

### 14.2 Events

Each event names its game source, which hand vibrates, and a default
strength (amplitude 0 to 1, before `Hands.Haptics.Strength`). Strengths and
durations are proposals; their reason is to make each event
distinguishable without masking the others.

| Event | Game source | Hand | Pulse |
|---|---|---|---|
| game rumble | `RumbleSetMotors` (§2.9): the two motor levels, sampled each frame | both: low-frequency motor to the left, high-frequency to the right [proposal: keeps the game's two channels apart] | continuous, amplitude = the motor level, frequency 0 (runtime default) |
| damage | the player's current health (§2.2) falls between two frames | both | amplitude 0.3 + 2 * (lost fraction of maximum), capped at 1; 150 ms |
| death | current health reaches 0 | both | 1.0, 600 ms |
| tracker ping | each `TrackerScan` run (§2.6) while the tracker is raised | the hand holding the tracker (left) | 0.15, 20 ms [proposal: a tick per scan, felt but faint] |
| firing | the fire action pressed while a ranged weapon (types 1, 2, 3, 5) is current and the alignment byte allows firing (§2.5); replaced by a verified shot event when one is found (§22 item 17) | right | 0.6, 60 ms (flamethrower: 0.3, repeated while held) |
| hacking start, finish | event tap kinds hacking and cutting (11 §5.3) | both | 0.4, 80 ms |
| hacking steps | not verified (§22 item 18) | | |
| Alien close by | not verified in r1; candidates are the game's own rumble (if the game rumbles for it) and the tracker's blips (§22 item 19) | | |

When the game rumble is on, the damage and firing pulses are still played
[proposal: the game may not rumble for them; doubling is mild].

### 14.3 Required results

- **Output, OpenVR with SteamVR Input**: two vibration actions (left and
  right hand, or one action restricted per device) added to the action
  manifest of 04 §5.8.2, bound by default to `/user/hand/left/output/haptic`
  and `/user/hand/right/output/haptic` for every controller type there;
  played with `IVRInput::TriggerHapticVibrationAction` (start 0, the pulse's
  duration, frequency, amplitude, `k_ulInvalidInputValueHandle`).
  Continuous output is a series of short vibrations renewed each frame
  while the level is above 0.02.
- **Output, OpenVR legacy (04 §5.8.3)**: `IVRSystem::TriggerHapticPulse`
  on the hand's device index, axis 0, with a duration in microseconds of
  `amplitude * 3000` [proposal: the API's pulse length range is about 0 to
  4 ms per call; the call is made at most once per frame per hand].
- **Output, OpenXR**: one action of type `XR_ACTION_TYPE_VIBRATION_OUTPUT`
  with both hand subaction paths, suggested to
  `/user/hand/<hand>/output/haptic` for every profile of 04 §5.8.4; played
  with `xrApplyHapticFeedback` (`XrHapticVibration`: duration in
  nanoseconds, frequency `XR_FREQUENCY_UNSPECIFIED` unless given, amplitude)
  and stopped with `xrStopHapticFeedback` when a continuous level drops to
  0.
- **Thread**: game sources are recorded where they occur (the rumble hook
  may run on the game's audio thread [likely], the health and tracker on
  the game thread) as atomic values or a small lock-free queue; all runtime
  calls are made on the render thread right after the controls are read
  (04 §5.9), so haptics never call the runtime from a game thread [proposal:
  one thread for all runtime input and output, as v1 already has for
  input].
- **No focus, controllers lost, headset off**: no output while the runtime
  withholds input focus (04 §5.1); a lost hand gets none; output continues
  with the headset off.
- **Master and strength**: `Hands.Haptics.Enabled` off means no haptic
  action is added to the runtime's action set and nothing is hooked (v1);
  `Hands.Haptics.Strength` multiplies every amplitude (default 1.0, range 0
  to 1). Each event has its own on/off key (§15).
- **Saved bindings**: a player's saved SteamVR binding made before these
  actions existed reads them as unbound; that is logged once, like the hand
  poses (04 §5.11) [20 §10 item 2].
- **Real pad**: when a physical XInput pad is in use, the game rumble also
  reaches the pad as before (the game's call is always made).

## 15. Settings

New sections `Hands` and `Body` inside `Settings` of `mothervr.json` (06
§1 style). Unknown keys are kept on save (06 §3). Every default gives v1.
Keys are this project's own. "Applies" says when a change takes effect.
Each key applies on its own; there is no master switch (20 §1.3). The
seated and standing mode and the calibrated standing
eye height are 22's keys (22 §3.1); this document only reads them.

### 15.1 `Hands`

| Key | Type | Default | Range / values | Sanitise | Applies |
|---|---|---|---|---|---|
| `Hands.Enabled` | bool | false | | non-bool: false | at once |
| `Hands.AimFromHand` | bool | true | | non-bool: true | at once; effective only with `Hands.Enabled` |
| `Hands.TrackerInHand` | bool | true | | non-bool: true | at once; only with `Hands.Enabled` |
| `Hands.FlashlightAim` | string | `"Head"` | `"Head"`, `"LeftHand"`, `"RightHand"` | other: `"Head"` | at once; only with `Hands.Enabled` |
| `Hands.MeleeByMotion` | bool | true | | non-bool: true | at once; only with `Hands.Enabled` |
| `Hands.ThrowByMotion` | bool | true | | non-bool: true | at once; only with `Hands.Enabled` |
| `Hands.ThrowStrength` | number | 1.0 | 0.5 to 2.0 | outside or NaN: 1.0 | at once |
| `Hands.Smoothing` | number | 0.0 | 0.0 to 1.0 | outside or NaN: 0.0 | at once |
| `Hands.WristHealth` | bool | false | | non-bool: false | at once |
| `Hands.Haptics.Enabled` | bool | false | | non-bool: false | next launch (the action set is created at runtime start, 04 §5.8) |
| `Hands.Haptics.Strength` | number | 1.0 | 0.0 to 1.0 | outside or NaN: 1.0 | at once |
| `Hands.Haptics.GameRumble` | bool | true | | non-bool: true | at once |
| `Hands.Haptics.Damage` | bool | true | | non-bool: true | at once |
| `Hands.Haptics.Tracker` | bool | true | | non-bool: true | at once |
| `Hands.Haptics.Firing` | bool | true | | non-bool: true | at once |
| `Hands.Haptics.Hacking` | bool | true | | non-bool: true | at once |

The sub-keys of a feature default to on so that turning the feature's
master key on gives the whole feature; the master keys default off (20
§1.1). Turning `Hands.Enabled` on makes the hand-dependent sub-keys live.

### 15.2 `Body`

| Key | Type | Default | Range / values | Sanitise | Applies |
|---|---|---|---|---|---|
| `Body.Visible` | string | `"Full"` | `"Full"`, `"ArmsAndHands"`, `"HandsOnly"`, `"None"` | other: `"Full"` | at once |
| `Body.EyeForward` | number | 0.0 | -0.20 to 0.20 (metres) | outside: clamped; NaN: 0.0 | at once |
| `Body.RoomScale` | bool | false | | non-bool: false | at once |
| `Body.SeatedEyeRaise` | number | 0.0 | -0.50 to 0.50 (metres) | outside: clamped; NaN: 0.0 | at once; seated mode only |
| `Body.RealCrouch` | bool | false | | non-bool: false | at once; only in standing mode with a calibration (22 §6) |

### 15.3 Options page and console

Every key appears on the options page, on the `Hands` and `Body` pages,
reached from the `MotherVR` page (22 §12.1, which also gives the rules every v2
page follows: look, value display, description lines, greying and hidden
items). The calibration is 22's (22 §3.3, §12.2). The labels, values and
description lines below are exact; they are this project's texts, in the
style of the v1 page players know (00 §2.6) [proposal for each text: it
names the result the player gets, in the fewest words that fit the stock
description length, 22 §12.1].

`Hands`, in order:

| # | Label | Kind | Values shown | Key | Description line | Greyed while |
|---|---|---|---|---|---|---|
| 1 | `Tracked Hands` | toggle | on / off | `Hands.Enabled` | `Your tracked hands are used in the game, as set below` | |
| 2 | `Aim from Hand` | toggle | on / off | `Hands.AimFromHand` | `The weapon aims where your hand points, not where you look` | `Tracked Hands` off |
| 3 | `Motion Tracker in Hand` | toggle | on / off | `Hands.TrackerInHand` | `Hold up the motion tracker in your left hand and look at it to read it` | `Tracked Hands` off |
| 4 | `Flashlight Aim` | list | `Head`, `Left Hand`, `Right Hand` | `Hands.FlashlightAim` | `What the flashlight points with` | `Tracked Hands` off; shown greyed with `Not available yet` until §7.2's object is verified |
| 5 | `Melee by Swing` | toggle | on / off | `Hands.MeleeByMotion` | `Swing your hand to strike with a melee weapon` | `Tracked Hands` off |
| 6 | `Throw by Motion` | toggle | on / off | `Hands.ThrowByMotion` | `Thrown items fly with your hand's motion` | `Tracked Hands` off |
| 7 | `Throw Strength` | slider | 0.5 to 2 in steps of 0.1 | `Hands.ThrowStrength` | `Multiplies the speed of your throws` | `Tracked Hands` or `Throw by Motion` off |
| 8 | `Hand Smoothing` | slider | 0 to 1 in steps of 0.1 | `Hands.Smoothing` | `Steadies the hands. Higher values steady more but lag behind` | `Tracked Hands` off |
| 9 | `Wrist Health` | toggle | on / off | `Hands.WristHealth` | `Turn your left wrist up to see your health` | |
| 10 | `Haptics` | toggle | on / off | `Hands.Haptics.Enabled` | `Requires restart! Controllers vibrate for game events` | |
| 11 | `Haptics Strength` | slider | 0 to 1 in steps of 0.1 | `Hands.Haptics.Strength` | `Strength of every vibration` | `Haptics` off |
| 12 | `Vibrate on Game Rumble` | toggle | on / off | `Hands.Haptics.GameRumble` | `Vibrate whenever the game would rumble a gamepad` | `Haptics` off |
| 13 | `Vibrate on Damage` | toggle | on / off | `Hands.Haptics.Damage` | `Vibrate when you are hurt` | `Haptics` off |
| 14 | `Vibrate on Tracker Pings` | toggle | on / off | `Hands.Haptics.Tracker` | `A faint tick for each motion tracker ping` | `Haptics` off |
| 15 | `Vibrate on Firing` | toggle | on / off | `Hands.Haptics.Firing` | `Vibrate your right hand when you fire` | `Haptics` off |
| 16 | `Vibrate on Hacking` | toggle | on / off | `Hands.Haptics.Hacking` | `Vibrate when hacking or cutting starts and ends` | `Haptics` off |

`Body`, in order:

| # | Label | Kind | Values shown | Key | Description line | Greyed while |
|---|---|---|---|---|---|---|
| 1 | `Body` | list | `Full`, `Arms and Hands`, `Hands Only`, `None` | `Body.Visible` | `Which parts of your character's body you see` | |
| 2 | `Eye Forward` | slider | -0.2 to 0.2 in steps of 0.01 | `Body.EyeForward` | `Moves your view forward (positive) or back from the body, in metres` | |
| 3 | `Room-Scale` | toggle | on / off | `Body.RoomScale` | `Walking in your play space moves you in the game` | |
| 4 | `Seated Eye Raise` | slider | -0.5 to 0.5 in steps of 0.05 | `Body.SeatedEyeRaise` | `Raises (positive) or lowers the view when seated, in metres` | `Play Mode` is `Standing` (22 §12.2) |
| 5 | `Real Crouch` | toggle | on / off | `Body.RealCrouch` | `Crouch for real to crouch in the game. Needs Standing play and a calibration` | `Play Mode` is `Seated`, or no standing height is calibrated (22 §6.2) |

Console commands in the `mvr_*` family for these keys are the
implementation's choice of names (they are not compatibility names); 06
§4 lists them in its next revision.

## 16. Inputs while hands are on

| Semantic input (04 §5.8.1) | Hands off (v1) | Hands on |
|---|---|---|
| Aim (right grip) | game aim | game aim (§5.4); with a throwable: arm and release the throw (§9) |
| Tracker (left grip) | raise the tracker | raise the tracker into the left hand (§6) |
| Fire (right trigger) | fire, throw, melee | fire; throw and melee also by motion (§8, §9) |
| Crouch (left trigger) | crouch | crouch; with real crouch also by height (§11.5) |
| Move (left stick) | move | move; written by room-scale follow only while the stick is idle (§11.3) |
| Flashlight, Interact near the head (04 §5.10) | flashlight | unchanged |
| Hand poses | read, unused | used (§3) |
| aim pose (new) | declared, bound and read, unused | right and left hand aim poses (§5.2, §6.2): one new pose action per hand in 04 §5.8.2 and one pose action with both subaction paths in 04 §5.8.4 |

The aim poses are declared and bound whatever `Hands.Enabled` is
[proposal: an action the runtime is told about only at start would leave
a player who turns tracked hands on in the menu aiming from the head until
the next start, against §15.1's "at once"; declaring two pose actions
costs nothing visible]. A binding saved before they were always declared
lacks them: when a hand's grip pose is tracked but its aim pose action
reads inactive, the log says so once (04 §5.11), and aiming stays on the
game's head ray until the binding is reset to the default.
| haptic output (new) | | §14.3 |

One owner per input at a time (20 §5): room-scale writes Move only while
the player's stick is idle; melee and throwing by motion add presses to the
game actions v1 writes, merged by OR.

## 17. Threads

| Work | Thread |
|---|---|
| read controllers and hand poses, publish them with the head pose; all haptic runtime calls | render thread (04 §5.9; §14.3 of this document) |
| take the frame's hand set (with the head pose of that frame), the camera pose (§2.4), the player pointer; compute world targets (§3.1); aim ray (§5), tracker heading (§6), room-scale, height, crouch presses (§11), control hand-back (§12), health value, haptic event detection | game thread |
| edit the player's pose (§4.2), body visibility (§10) | the job thread running the player's final-pose step (§2.3), reading only the game thread's copy for this frame |
| body visibility's plan (which bones collapse where, per skeleton and value) | game thread, before the `CHARACTER_IK_AND_RAGDOLL` stage; published with the pose and skeleton it is for; the job uses it only for that pose |
| game rumble levels (§2.9) | whichever thread calls `RumbleSetMotors` [likely the audio thread]; stored as two atomic floats |

The game thread's copy for the job threads is written before the
`CHARACTER_IK_AND_RAGDOLL` stage of the frame and not changed until that
stage's jobs have finished (the pass waits for them, §2.3), so a plain
per-frame copy is safe if it is published before that stage; otherwise it
goes through the torn-free hand-over of 12 §3. No allocation, file I/O,
locking or logging on the render thread or in the job (12 §6).

## 18. Logging

11 owns the wording; a later revision of 11 adds these events (20 §10 item
10). Each must convey:

| Event | Level | Must convey |
|---|---|---|
| each v2 key of this document that is on, at session start | INF | the key and value |
| each game site of §20 checked for an enabled feature | INF / WRN | the site, the outcome; on a mismatch that the feature stays off |
| the player's skeleton seen for the first time | INF | the skeleton's name; whether all bones of §2.3 were found by name; their indices |
| tracked hands become active or inactive | INF (DBG when it is an episode of §12) | which; the reason (episode kind, hand lost, setting) |
| a hand lost or regained (§3.3) | INF | the hand; the reason |
| hand aiming: the class in use changes | DBG | the weapon type |
| fire gate overridden for the first time in a session | INF | that firing with a misaligned weapon is permitted |
| throw by motion | DBG | the release speed; whether thrown or cancelled |
| room-scale follow blocked | DBG | the remaining offset |
| real crouch press | DBG | crouch or stand; the head height and thresholds |
| control hand-back at an episode end | DBG | the episode kind; what was restored |
| body visibility: a value other than `Full` takes effect, or `Full` again | INF | the value; how many bones of which skeleton collapse |
| body visibility: the first frame drawn from the collapsed copy | INF | that the player's model is drawn from it |
| body visibility: a part posed from another skeleton (§10.2) | WRN, once | that it stays drawn |
| body visibility: the player's skeleton could not be read (§10.2) | WRN, once per change of the skeleton, pose, value or reason | the skeleton's name; why (the reason, the bone count read, the first bone without a name); that the body is drawn whole until it can be read and how often it is read again |
| real crouch: a press the byte did not follow (§11.5) | WRN, once | the time waited |
| `Body.RoomScale` on while §11.3's facts are open | INF, once | that room-scale is off in this build and why |

The calibration and the play mode are logged by 22 (22 §16).
| haptic actions created, or bound/unbound | INF / WRN | per hand; that a saved binding lacks them |
| `DebugOptions`: every 900 frames | VRB | each hand's world target and the hand bone's reached position, their distance; the aim ray start and direction; the head offset `o` of §11.3 |

## 19. Edge cases

| Situation | Hands, aim, tracker, melee, throw | Body visibility | Eye forward offset, height | Room-scale, real crouch | Haptics |
|---|---|---|---|---|---|
| menus, virtual screen (stereo mode 1) | inactive | as set | not applied (screen frames ignore the head, 03 §7) | inactive | game rumble passes (the game may rumble in menus) |
| loading screen | inactive | n/a | not applied | inactive | none from mod events |
| cutscene, level fade | inactive | as set (§10.3) | applied when the game asks for a head pose | inactive | game rumble passes |
| interaction, hiding | inactive (r1) | as set | applied, except the eye forward offset during the interactions of §11.2 | inactive | hacking pulses |
| death | inactive from health 0 | as set | applied | inactive | death pulse |
| headset off | as with it on | as set | as set | real crouch: no presses while the headset is not in use (03 §2.3) | continue |
| controllers lost | §3.3 | unchanged | unchanged | follow continues (it uses the head) | none for that hand |
| runtime dashboard (no input focus) | hands lost (§3.3) | unchanged | unchanged | no presses | none |
| runtime None or failed | nothing (20 §1.2) | nothing | nothing | nothing | nothing |
| OpenVR legacy input (no `IVRInput`) | no hand poses (04 §5.11): every hand feature stays inactive and that is logged once | works | works | works | legacy pulses (§14.3) |
| OpenXR | as specified | | | | |
| 2020 build | off (20 §6) | off | off | off | off |

## 20. AI.exe addresses used here

2015 RVAs, verified by the spec side in the 2015 `AI.exe`; 2020 is `?` for
every row. Names are spec labels.

| Name | 2015 | 2020 | Kind | Bytes / note | § |
|---|---|---|---|---|---|
| `CharacterRegistry` | 0x12F0C88 | ? | global | pointer | 2.2 |
| `PlayerGetter` (pattern) | 0x00F6B0 | ? | function | `a1 88 0c 6f 01 8b 40 08` | 2.2 |
| health fields | character +0x33C, +0x33E, +0x340 | ? | fields | u16, u16, float | 2.2 |
| `CharacterRequestCrouch` / `CharacterRequestStand` | 0x3CAE20 / 0x3CAE90 | ? | functions | `56 8b f1 80` / `56 8b f1 8a`; requested crouch byte `+0x3E3A` | 2.2 |
| `CharacterIkStage` | 0x3C9FB0 | ? | stage function | `CHARACTER_IK_AND_RAGDOLL` | 2.1 |
| equipment update call | 0x3CA02E | ? | call site | `e8 dd 86 fb ff` (to 0x382710) | 2.1 |
| final-pose pass call | 0x3CA04B | ? | call site | `e8 a0 b5 ed ff` (to 0x2A55F0) | 2.1 |
| `FinalPoseStep` | 0x2A5040 | ? | function | `64 a1 00 00 00 00 6a ff` | 2.3 |
| `SkinSetPose` | 0x28E9A0 | ? | function | `55 8b ec 83 e4 f0 81 ec 54 01 00 00`; `thiscall`, (bones*, count), `ret 8` | 2.3, 10.2 |
| model render object call | 0x2A51D8 | ? | call site | `e8 c3 97 fe ff` (to 0x28E9A0) | 2.3, 10.2 |
| part render objects call | 0x2A536A | ? | call site | `e8 31 96 fe ff` (to 0x28E9A0) | 2.3, 10.2 |
| constants table | 0x123F7C0 | ? | data | 0x58 bytes per entry; entry 0x14 `first_person_camera_offset_y` | 11.5 |
| `FinalPoseThunk` | 0x2A55E0 | ? | function | `8b 4c 24 04 81 c1 f0 03 00 00 e9 51 fa ff ff`; its address is pushed at 0x2A5655 (`68 e0 55 6a 00`) | 2.3 |
| game limb IK | 0x29ED80 | ? | function | `55 8b ec 83 e4 f0`; `stdcall`, `ret 0x10` | 2.3 |
| pose model-space getter | 0x7B9E80 | ? | function | shows the pose layout | 2.3 |
| `BoneWorldTransform` | 0x2A2FB0 | ? | function | `55 8b ec 83 e4 f0 83 ec 54`; `thiscall` on component, (bone, selector, out*), `ret 0xc`; model-to-world at component +0x270 / +0x280 / +0x290 | 2.3 |
| held item attachment query | 0x3B4E06 | ? | call site | `e8 a5 e1 ee ff` (to 0x2A2FB0); selector pushed at 0x3B4DFD `6a 06` | 4.2, 22 |
| CCD solver construct / solve | 0x7C1DA0 / 0x7C1DD0 | ? | functions | `f3 0f 10 44 24 08 8b c1` / `55 8b ec 83 e4 f0 81 ec d4 00 00 00` | 2.3 |
| two-joint solve | 0x7C2630 | ? | function | `55 8b ec 83 e4 f0 a1 30 61 be 01` | 2.3 |
| `CameraPosition` / `CameraForward` | 0x0295B0 / 0x0295D0 | ? | functions | camera +0x3C / quaternion +0x2C | 2.4 |
| `AimRayUpdate` | 0x382210 | ? | function | `55 8b ec 83 e4 f0 83 ec 54` | 2.5 |
| aim ray call | 0x3CD410 | ? | call site | `e8 fb 4d fb ff` | 2.5 |
| `CurrentItem` | 0x3829E0 | ? | function | `80 b9 10 15 00 00 00` | 2.5 |
| weapon type parse | 0x4B5F80 | ? | function | enum of §2.5; type at weapon +0xD4 | 2.5 |
| fire alignment test | 0x3ABD6C | ? | site | `0f 2f 05 80 67 1c 01 76 11` | 2.5, 5.3 |
| `TrackerScan` | 0x431E40 | ? | function | `55 8b ec 83 e4 f0`; facing at tracker +0xF0 / +0xF8 | 2.6 |
| `SetBodyVelocities` | 0x3EF590 | ? | function | `55 8b ec 83 e4 f0 83 ec 3c` | 2.7 |
| `RumbleSetMotors` | 0xC09280 | ? | function, vtable slot at 0x11C1840 | `f3 0f 10 44 24 04` | 2.9 |
| HeadClipFade call (v1) | 0x34E2C1 | (as 08) | call site | `e8 1a 8b ff ff` | 2.10 |

## 21. Acceptance

In game on the 2015 build, headset worn and controllers held unless noted,
with OpenVR and with OpenXR. "Log" is `mothervr.log` with `DebugOptions` on
where a VRB line is needed.

1. Debug build: each stage of §2.1 logs its thread id and frame number once
   per second for ten seconds: the order matches §2.1 and the final-pose
   step runs on job threads.
2. Load a level with `Hands.Enabled`: the log names the player's skeleton
   (`FEMALEFP`), finds every bone of §2.3 by name, and gives their indices.
   Repeat in a space-suit section (`SPACESUITFP`).
3. Hands on, looking straight ahead with both hands held out: the VRB line
   shows each hand's world target within 5 mm of the hand bone's reached
   position (§4.2 item 1). r9: once per skeleton the log gives the grip
   frame of §3.5 per side (the wrist to grip distance, expected 7 to 12 cm)
   and the `Head` bone's world position from `BoneWorldTransform`'s
   transform next to the active camera's position (expected within
   0.25 m, which confirms the model-to-world record and selector 7). Turning the head leaves the in-game hands where the real
   hands are. Snap turn and recenter do not detach them.
4. Reach above the head, behind the back and across the chest: the arms
   follow without the elbow flipping; out of reach, the arm is straight
   towards the hand.
5. Equip the revolver, shotgun, bolt gun, flamethrower: shots (or the
   flame) go where the barrel points with the head turned 60 degrees away;
   bullet holes at 5 m are within 5 cm of the barrel's line. With hand
   aiming, firing works at every hand position relative to the head that
   the player can reach.
6. With `Hands.AimFromHand` off but hands on: shots follow the head again.
7. Raise the tracker with the left grip: it is in the left hand; turning
   the hand turns the tracker's cone; with hands off it faces the body.
8. Melee tool and stun baton: a real swing swings in game; a slow movement
   does not; the Fire control still works.
9. Throw a flare, a noisemaker and a pipe bomb overhand, underhand and
   gently: they fly with the hand's motion; opening the grip without moving
   keeps the item in hand.
10. Each `Body.Visible` value: the hidden parts are gone in both eyes, in a
    mirror (07) and in the shadow; other characters are unaffected. The
    log names the skeleton, its bone count and the indices of the required
    bones, then how many bones collapse and that the model is drawn from
    the collapsed copy; no warning that a part is posed from another
    skeleton. In `None`, the held weapon and a held flare are still drawn
    in the hand's place. Repeat in a space suit.
11. `Body.EyeForward` +0.15: the view moves forward of the body; the
    head-in-wall fade does not trigger standing in the open.
12. Room-scale: walk 1 m forward in the play area: the character walks 1 m
    in the level, the view does not move twice; walk into a wall: the body
    stops, the view blacks out after 08 §4's delay; step back: it clears.
13. Standing mode, calibrated (22 §6), `Body.RealCrouch` on: standing
    upright gives the game's normal eye height; crouching to 70 % of the standing height crouches the character
    (log: the press, the heights) and enters a vent; standing inside the
    vent stays crouched; leaving the vent and standing stands. While
    crouched by real crouch the view stays at the real head (within 5 cm;
    no second drop); this measures §11.5's 0.63.
14. The requested crouch byte (`+0x3E3A`) follows the game's crouch in
    the log when crouching by the control.
15. After a cutscene, a ladder, a hiding place, a death and reload: within
    one second hands are back in the controllers, the view is over the
    body, and the crouch state matches the player's (log: control
    hand-back lines).
16. Wrist health: take damage: the panel's bar and colour change; at one
    fifth it blinks; it is hidden when the wrist faces away.
17. Haptics: damage, tracker scans, firing and hacking pulse on the
    hands named in §14.2; with a physical pad connected, the pad still
    rumbles; with the dashboard open, nothing vibrates.
18. All keys of §15 at their defaults: the log shows no v2 site checked or
    hooked and no added actions; behaviour equals v1 (20 §1.1).
19. The `Hands` and `Body` pages show the labels, values, order and
    description lines of §15.3; with `Tracked Hands` off, items 2 to 8 of
    `Hands` are greyed; in seated play `Real Crouch` is greyed.

## 22. Open questions

1. **Stage order and threads** (§2.1): confirm the execution order and that
   the final-pose step runs on job threads (acceptance 1).
2. **Grip to hand-bone mapping**: r9: derived per skeleton at run time
   from the hand's finger bones (§3.5); acceptance item 3 confirms the
   result and the wrist distance.
3. **Held item after the edit** (§4.2 item 6): r9: the attachment query
   is `BoneWorldTransform` at 0x3B4E06 with selector 6 (§2.3); that
   selector 7 is the edited pose and that a second equipment update after
   the edit carries the item along are [likely], confirmed by acceptance
   item 5 (the weapon stays in the hand while the hand moves fast).
4. **The game's limb IK** (0x29ED80): r9: not run for the player's
   first-person skeleton while tracked hands are active (§4.2 item 7);
   which limbs its four call sites serve stays open and no longer blocks
   anything.
5. **Weapon transform**: where the drawn weapon's muzzle and barrel axis
   can be read (a candidate is the function at 0x3A07D0 on the weapon; its
   meaning is not verified).
6. **Fire alignment gate**: what the 0.75 m test measures (§2.5), and
   whether it also guards something the hand must still respect (a weapon
   pushed into a wall). Look at 0x3A0E50's result and the state values 6
   to 8 at `[+0x152C] + 4`.
7. **Tracker facing writer**: which code writes tracker `+0xF0`/`+0xF8`, so
   the mod's value is not overwritten between its write and the scan; and
   whether the scan runs while the tracker is lowered.
8. **Throw launch**: which of the nine `SetBodyVelocities` callers (§2.7),
   or another path, launches a thrown item; whether the game derives the
   throw direction from the aim ray; whether the spawn point can be set.
9. **Rumble**: which game events rumble; whether `RumbleSetMotors` is
   called with no physical pad connected (the pad is virtual in v1, 04 §3);
   and on which thread.
10. **Clipping**: whether hands and weapons may pass through walls or must
    be stopped at geometry (and how, without writing physics).
11. **Tracker model**: whether the game's tracker animation holds the
    tracker in the left hand, so that §4.2 alone places it.
12. **Flashlight**: where the player torch's light transform is set (the
    `PlayerTorch` entity), so a hand can drive it.
13. **Melee**: how long the game's melee action owns the arm (the
    `ACT_PLAYER_MELEE` action), to replace the 0.6 s of §8.2; and whether
    the game's melee hit test follows the arm's animation (then the arm
    must play it during a swing by motion, §8.2) or the camera or body.
14. **Crouch eye height**: the game's standing and crouched eye heights
    (or the camera's height change on crouch), for §11.5. r7: 0.63 m from
    the game's camera constants [likely] (§11.5); the code that reads them
    is not located; acceptance item 13 confirms.
15. **Scripted player animations** outside the signals of §12.1: how to
    observe that an animation drives the player's body and view
    (`CMD_PlayAnimation` and its `PlayerAnimDrivenView`, the
    `ACT_PLAYER_*` actions); the mission 4 android encounter of 05 §16 is
    one case.
16. **Watch part**: whether the in-game watch (`RIPLEY_FP`'s `WATCH`
    part, §2.8) could show health instead of the runtime panel of §13.2,
    and what that would need.
17. **A verified shot event** for the firing pulse (ammunition decrement
    or the weapon's fire function).
18. **Hacking steps**: a per-step signal of the hacking device (correct and
    wrong inputs).
19. **Alien proximity**: a game value for the Alien's distance or threat
    (the tracker's blip list, the music or threat state) that may be read
    each frame.
20. **DLC first-person skeletons**: which skeletons `DALLAS_FP`,
    `PARKER_FP` and the others use, and whether they have the bones of §2.3.
21. **Left-handed players**: whether a weapon in the left controller should
    drive the character's right hand (crossing arms) or be unsupported.
22. (Resolved in r2: seated and standing play and the calibration are
    22's, §11.4.)
23. **Game IK solvers**: r9: CCD and two-joint solvers at 0x7C1DA0,
    0x7C1DD0 and 0x7C2630 (§2.3) [likely]; using them is optional.
24. **Screens framed for a fixed eye point** (§11.2): which interaction
    kinds of 11 §5.3 show a screen the game composes for the body's own
    eye point. To resolve: with `Body.EyeForward` at +0.2, use each kind
    and note the screens that are cropped or misaligned.
25. **Frame of the move action** (§11.3): whether the game reads map 0
    analog 0 and 1 relative to the camera's yaw, the body's or the world's.
    To find: the player locomotion's reader of the move action; or in game,
    with the head turned 90 degrees from the body, a forward stick and the
    direction walked.
26. **The body's yaw** (§11.3): a field of the player character or of its
    camera's `FollowBehavior` that gives the body's heading each frame, so
    the body's movement can be rotated into tracking space.
27. **Parts posed from another skeleton** (§2.3, §10.2): which first-person
    models take `FinalPoseStep`'s part path with a mapped array, and from
    which skeleton. To find: acceptance item 10's warning; then the mapping
    0xA2C100 and the pointer at component `+0x328`.
28. **The requested crouch byte in play** (§2.2, §11.5): that `+0x3E3A`
    follows the game's crouch for the player, including crouches the game
    makes itself (vents, scripted). Acceptance item 14.

Answered by r7 from the open points the earlier revisions left inside §2:
the final-pose step's return and the pose's way to the renderer (§2.3,
§22 item 1 keeps only the in-game order check); the bone groups' bones
and how bones are matched by name (§2.3, §10.2); the first-person model
parts are not needed by the method of §10.2 (§2.8 stays for the skipping
method).

## 23. Changes to v1 and v2 documents

Not made here; for the next revisions:

- `04-input.md` §2.3: the pad vtable slot `+0x28` (0x60F280) sets zero
  vibration on XInput users 0 to 3 through `XInputSetState`; "scans XInput
  users" is inaccurate.
- `04-input.md` §5.1 ("No haptics in v1") and §5.8.4 ("No haptic output in
  v1"): add that v2 adds haptic outputs when `Hands.Haptics.Enabled` is on
  (21 §14.3).
- `04-input.md` §5.8.1, §5.8.2, §5.8.4: new inputs: an aim pose per hand,
  always (made in 04 r9), and the haptic outputs when they are on (21
  §16), with their default bindings; the saved-binding notice of §5.11
  extends to them (made in 04 r9 for the aim poses).
- `04-input.md` §5.8.5: Move may be written by room-scale follow while the
  stick is idle (21 §11.3); Crouch may be pressed by real crouch (21 §11.5);
  Fire may be pressed by a melee swing (21 §8).
- `04-input.md` §5.11: hand poses are used in v2 (21 §3); the hand-over
  must also carry each hand's tracking-result state (21 §3.3) and the aim
  poses.
- `03-hmd-and-frames.md` §1 and §2.1: the head pose returned by slot 11 may
  include the terms of 21 §11.1 (eye forward offset, height, room-scale
  shift); in standing mode the recenter keeps the calibrated height
  instead of the head's (22 §6.1).
- `05-game-patches.md` §3.2: the stereo update also runs room-scale follow,
  real crouch and the control hand-back (21 §11, §12) when their keys are
  on.
- `05-game-patches.md` §7: list the fire alignment patch of 21 §5.3 as a
  v2 optional patch.
- `06-options-and-config.md` §1: the `Hands` and `Body` sections (21 §15);
  §4: console commands for them and the calibration command.
- `04-input.md` §5.8.4 and `09-openxr.md` §5.2: the vibration action and
  its calls (21 §14.3); the left grip space also places the wrist panel as
  a quad layer (21 §13).
- `11-diagnostics.md`: the events of 21 §18.
- `12-threads-and-robustness.md` §2.2, §3.2: the job-thread work of 21 §17
  and the copy it reads; the audio-thread rumble values.
- `13-distribution.md`: no new message box; that real crouch needs
  standing play and a calibration is said by the `Real Crouch`
  description and the calibration's texts on the options page (§15.3, 22
  §12.2). The player guide lists the `Hands` and `Body` pages.
- `04-input.md` §5.7: which controls help rows change while tracked hands
  are on (for example `Aim` no longer on the right grip with
  `Hands.AimFromHand`) is to be given with the inputs of §16 [open: the
  rows follow once §16's ownership table is settled, 20 §10 item 1].
