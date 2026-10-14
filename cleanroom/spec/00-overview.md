<!-- Provenance: user requirements; in-game observation of MotherVR 0.8.1 on the 2015 build; AI.exe 2015 analysis; in-game testing of an earlier rebuild in a headset (per-eye lighting and mirror fixes confirmed; OpenXR stereo confirmed on SteamVR with Index); user decisions of spec r3 (runtimes None / OpenVR / OpenXR, flat play is the unmodded game). Revision: r3 -->

# 00. Overview

## 1. Goal

Play the whole of Alien: Isolation, from the title screen to the end
credits, in a current PC VR headset, by driving the game's own dormant
Oculus Rift DK2 path.

The 2015 game contains a complete stereo renderer written for LibOVR 0.4
(the DK2 SDK), statically linked into `AI.exe`. It is switched off: it only
runs when graphics setting 0x18 "Stereo Mode" is "Rift", the menus never
offer that value, and the DK2 runtime it was written for no longer exists.
The mod turns that path on and puts a modern VR runtime underneath it. The
game then renders both eyes, places its UI for a headset, and keeps
running its own gameplay, AI, audio and scripting unchanged.

### 1.1 Versions

- **v1** reproduces the user-visible behaviour of MotherVR 0.8.1 (the
  community mod that first used this path) on Windows 11 and current
  runtimes, and fixes that mod's known problems (§4). v1 adds no features
  beyond those fixes.
- **v2** (later, separate spec revision) adds enhancements: tracked hands
  and hand aiming, smooth turning, comfort options, haptics, per-controller
  glyphs, OpenXR as the default runtime. Nothing in v1 may make these
  harder, but v1 does not specify them. (OpenXR itself is a v1 runtime,
  §3; only making it the default is v2.)

## 2. What the player sees in v1

### 2.1 Gameplay

- Full stereo 3D with 6-DoF head tracking: the first-person view follows
  the headset's orientation and position on top of the game's own camera.
  Each eye renders at the runtime's recommended resolution times the
  configured pixel scale.
- The game's own camera sway, breathing motion and DK2-era neck correction
  are off; they cause discomfort in a headset.
- Screen-space ambient occlusion is off (it bleeds across the two eyes).
- The HUD (motion tracker overlay, prompts, inventory, objectives) is
  shown on a flat panel locked to the head, 0.55 m wide, 1.0 m in front of
  the eyes.
- Floor and other planar reflections are correct in each eye.
- Head-in-wall blackout ("clipping blinders"): when the head moves into
  level geometry the view fades to black. An option weakens this so that
  leaning near walls does not trigger it.
- Cutscenes and level fades play in stereo and are never blacked out by
  the mod.

### 2.2 Menus, videos and loading

- The front-end menu, the pause menu, loading screens and the title/attract
  video are shown on a **virtual screen**: a flat, level screen 1.0 m wide
  and 0.5625 m high (16:9), 1.5 m in front of the player at eye height,
  fixed in the world. It is placed in front of the player's current
  facing each time such a screen opens and after each recenter.
- While a loading screen is up the 3D scene behind it is black.
- The menus use focus navigation (pad style). The mouse cursor, when used,
  maps into the menu's own 1280x720 space, not the stereo back buffer.

### 2.3 Input

- VR controllers drive the game through its gamepad: to the game there is
  always a connected Xbox-style controller, whose buttons and sticks come
  from the VR controllers and from a real gamepad if one is attached.
  Keyboard and mouse keep working.
- Controller prompts in the UI show VR controller glyphs.
- **Snap turn** (on by default): pushing the turn stick past half deflection
  turns the player by 360°/N (N = 12 by default, so 30°; N from {2, 3, 4,
  6, 12, 24}). Holding the stick repeats the turn every 300 ms. Snap turn
  only acts in first-person gameplay, not on the virtual screen. When snap
  turn is off, or while keyboard and mouse are in use, the game's own
  smooth stick and mouse look applies.
- **Recentering** sets the player's forward direction to where the head is
  facing (yaw only):
  - by hand: LB+RB on the virtual gamepad, Ctrl+Alt on the keyboard, or
    the game's own "reset view" menu item;
  - automatically when the headset is put on (the runtime reports a user
    present and the head is upright), so a game started with the headset
    lying on the desk still comes up level and facing the right way;
  - automatically when a loading screen closes, so gameplay starts facing
    where the player faces.
  The virtual screen is re-placed after every recenter.

### 2.4 Desktop

The game window shows a mirror of the headset view (both eyes, or the left
or right eye; optionally symmetric, see 06). The window
is only a mirror: the game renders to a side-by-side target sized for the
headset, independent of the window size.

### 2.5 Options menu page

The game's Options menu gains a VR page. Its settings take effect
immediately unless noted, and are saved to the config file:

- VR runtime: None, OpenXR or OpenVR (SteamVR). The original's "Oculus"
  entry selects OpenXR and is labelled so. Takes effect after a restart.
- Mirror view (both eyes, left, right) and symmetric mirror.
- Pixel scale (render resolution relative to the runtime's recommendation).
- Clipping blinders on/off.
- Snap turn on/off and snap turn segments.
- VR input options passed to the game's scripts: relative walking,
  strafing, toggle grips, toggle crouch.
- Skip intro: skips the start-up logo and intro sequence (next launch).
- Dev. debug options: shows the game's hidden "Select Level" and debug
  checkpoint entries in the front end.

The VR page is produced by patching the player's own `UI.PAK` (06); no
modified game archive is shipped.

### 2.6 Config file

A JSON file in the game directory holds every option of §2.5. It is read
at start-up; missing or invalid entries take their defaults; it is
rewritten when an option changes. On the first start without a config
file the player is told once that VR options are under Options. The format
is in 06.

### 2.7 When VR is not available

With the runtime set to None (the default), flat play is the unmodded
game: no stereo device is created and no game patch is applied; only the
Options menu has the VR page, so VR can be turned on (01 §2.3).

If the runtime is OpenVR or OpenXR but absent or fails to start, the game
stays playable on the monitor, and the reason is logged and shown once. 02
defines how the stereo path behaves without a headset. If the player's
`UI.PAK` is not the expected one, VR works without the options page (06
§7.5). If `AI.exe` is not a supported build, the game runs completely
unmodified (01).

## 3. Platform

| Item | v1 |
|---|---|
| OS | Windows 11 (Windows 10 expected to work) |
| Game | Alien: Isolation, `AI.exe` 2015 build (PE timestamp 0x54E3199D). The 2020 build (0x5FD25676) is recognised; it is enabled only once its addresses are verified (01 §3.2) |
| Graphics API | Direct3D 11, as the game uses |
| Entry | A 32-bit `d3d11.dll` proxy in the game directory (01) |
| VR runtime | OpenVR (SteamVR, 03) and OpenXR (09), chosen in the options. The Oculus native (LibOVR) runtime is not supported; Meta headsets work through OpenXR (Meta's runtime is an OpenXR runtime) or SteamVR. A config file or command line naming "Oculus" selects OpenXR (06) |
| Controllers | read through SteamVR Input or OpenXR actions with default bindings per controller type (04 §5.8) |

## 4. Known problems of MotherVR 0.8.1 that v1 fixes

1. **Half-resolution eyes.** The game sizes its back buffer and render
   targets from the first render-target size it is told and never rebuilds
   them. MotherVR answered that first question with half scale (stereo was
   not yet running) and full scale later, so each eye rendered at half the
   headset's resolution. v1 answers with the full configured size from the
   first query, and keeps the swap chain windowed while a headset is in
   use (the side-by-side back buffer is larger than any display mode).
2. **Eye layout mismatch.** Because of the same resize, MotherVR laid the
   eye viewports out for a size other than that of the texture the game had
   created, so each eye showed a different slice of the image. v1 derives
   the eye layout from the actual texture and keeps it until the game
   creates a texture of a new size.
3. **Upside-down or tilted menus when the headset is not worn at launch.**
   MotherVR's only automatic recenter happened at launch, whatever the
   headset was doing; with the headset on the desk, the menus and the
   game's forward direction stayed wrong. v1 recenters automatically when
   the headset is put on and when gameplay starts (§2.3).
4. **Calling-convention mismatches in game-method replacements.** The game
   calls its gamepad, stereo device and UI methods as `thiscall` and relies
   on each callee popping exactly its arguments. MotherVR had a device
   method that popped 8 bytes where the game's original pops none, and a UI
   message hook that read its argument from the caller's stack frame
   instead of its own. v1 matches the game's ABI for every method it
   replaces (01 lists each one's convention).
5. **Floor reflections in stereo.** The game renders planar reflections
   once, mono, with the flat camera's projection, and samples them in
   screen space over the whole side-by-side target; in MotherVR reflections
   slid against head motion and twisted on roll. v1 renders the reflection
   per eye (07).
6. **Render-target-view leak.** MotherVR leaked one render-target view
   reference every frame. v1 releases every reference it obtains.
7. **Out-of-range reads in the input layer.** MotherVR read its per-action
   VR state tables with unchecked indices (an analog code could index the
   digital table and the other way round) and could append a seventh
   binding to an action that holds six. v1 bounds-checks every such index
   and never exceeds the game's binding capacity (04).
8. **Controls fitted only Vive wands under OpenVR.** v1 maps Valve Index,
   Oculus Touch and Windows Mixed Reality controllers' thumbsticks and
   buttons properly (04).
9. **Lighting that differs between the eyes.** Several hundred of the
   game's pixel shaders add a dither computed from the absolute pixel
   position; on the right half of the side-by-side target it overflows and
   forces surfaces fully lit, worse at high resolution (MotherVR issue
   #253; it looked like reflections moving against the head). v1 patches
   those shaders to use the position relative to the target (08 §1).
10. **Object pop-in.** The game's per-object distance/LOD test and its
    extra occluder tests are computed for the single centre view, so small
    props, decals and shadows appear and vanish in the headset (MotherVR
    issue #194). v1 keeps every object visible in either eye (08 §2).
11. **No anti-aliasing in VR.** The game's stereo output path never calls
    its AA pass, whatever the AA option says. v1 runs the chosen AA mode in
    stereo, with per-eye history for the temporal mode (08 §3).
12. **Abrupt head-in-wall blackout.** The view goes black on the first
    frame the head touches geometry. v1 allows 0.6 s of contact before the
    blackout starts (08 §4).
13. **Desktop mirror in a corner.** MotherVR's mirror scale only fitted
    eyes that start at half size; with full-size eyes (item 1) the image
    filled a corner of the window. v1 fills the window (08 §5, 03 §8).

## 5. Glossary

- **Rift mode**: the game state where graphics setting 0x18 "Stereo Mode"
  equals "Rift". The game's stereo path runs only in Rift mode; the mod
  makes every test of it succeed (01 §4.1).
- **Stereo object**: the game's 0xB0-byte object that owns stereo
  rendering: device pointer, UI render targets, virtual-screen matrix,
  stereo mode and mode stack, recenter reference, fade state. Global
  pointer `StereoObjectPtr` (01). Layout in 02.
- **Stereo device**: the 21-method HMD interface the stereo object calls
  (originally a LibOVR wrapper). The mod supplies its own implementation in
  the game's layout (02).
- **Side-by-side render target** (SBS target): the single back buffer the
  game renders both eyes into, left eye in the left half and right eye in
  the right half. Its size is what the stereo device reports.
- **Eye viewport**: the rectangle of the SBS target one eye renders to and
  the runtime reads from.
- **Stereo mode**: 0 = world (3D scene with the HUD panel), 1 = virtual
  screen (2D UI on a world-fixed quad).
- **Stereo mode stack**: the stack of stereo modes the game pushes and pops
  as screens open and close. In v1 it is at most one deep (05).
- **Virtual screen**: the world-fixed flat quad that shows menus, videos
  and loading screens in stereo mode 1.
- **HUD panel**: the head-locked quad that shows the game's 2D UI layer in
  stereo mode 0.
- **Recenter**: taking the current head yaw (and position) as the new
  reference forward and origin.
- **Blackout** / **scene visibility**: the stereo object's multiplier on
  the 3D scene (1 visible, 0 black), used by the head-in-wall fade and
  during loading.
- **Clipping blinders**: the head-in-wall fade, and the option that sets
  its distance threshold.
- **FollowBehavior**: the game's first-person camera behaviour (the name
  its `GetName` returns). Owns yaw, pitch and the look-input gate.
- **POV**: a game camera view (main view, reflection view, ...), managed
  by the POV manager. POV 0 starts with the world-to-view matrix.
- **Action map**: one of the game's three tables mapping game actions
  (move, turn, crouch, confirm, ...) to device controls, held by the input
  manager.
- **Virtual control code**: a gamepad control code of 0x100 or above that
  the mod assigns to each action, so that the gamepad hooks can tell which
  action the game is asking about and merge VR controller state into it
  (04).
- **Pixel scale**: multiplier on the runtime's recommended per-eye size.
- **Snap turn segments**: the number N of equal steps in a full turn.
- **Mirror**: the desktop window's copy of the headset view.
- **RVA**: an address relative to `AI.exe`'s image base (01).
- **Site**: an instruction in `AI.exe` that the mod patches or redirects.

## 6. Documents

See `README.md` for the list. Order of reading: 00, 01, 02, then 03 to 09
as needed. `ANSWERS.md` answers the implementation side's questions and
names the sections each answer changed.
