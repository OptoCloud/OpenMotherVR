<!-- Provenance: user requirements; in-game observation of the earlier mod's last release on the 2015 build; AI.exe 2015 analysis; 2020 values were taken from the earlier mod's per-build address table (and, for inline sites, its installer) and have not been verified against a 2020 `AI.exe`; in-game observation by the spec side in a headset (per-eye lighting and mirror fixes confirmed; OpenXR stereo confirmed on SteamVR with Index; controls on Index confirmed); measurements by the spec side of render-thread and GPU cost in game (pixel scale, culling, mirror size, anti-aliasing, display resets, shader patching, logging, OpenXR frame loop); user decisions of spec r3 (runtimes None / OpenVR / OpenXR, flat play with runtime None is the unmodded game, "Oculus" means OpenXR), later (OpenVR is the default runtime; the options page is project-authored and built into the player's own UI archive; the project's log format; no other mod named in the spec) and r5 (SteamVR need not be running; invalid head poses; no elevation, registry change or folder rename; settings named by their key; known problems stated as observed effects); a tester's report on SteamVR with a Steam Frame headset and project decisions of spec r6 (borderless mirror window, focus loss does not pause, the options page shows the runtime this run drives, the headset-not-showing-the-game warning, the crash record). Revision: r6 -->

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

- **v1** reproduces the user-visible behaviour of the earlier community VR
  mod for this game (the first to use this path, in its last release), on
  Windows 10 and 11 and current runtimes, and fixes that mod's known
  problems and the further problems found in game testing (§4). v1 adds no
  gameplay features beyond those fixes.
- **v2** (later, separate spec revision) adds enhancements: tracked hands
  and hand aiming, smooth turning, comfort options, haptics, per-controller
  glyphs in more places, OpenXR as the default runtime. Nothing in v1 may
  make these harder, but v1 does not specify them. (OpenXR itself is a v1
  runtime, §3; only making it the default is v2.)

## 2. What the player sees in v1

### 2.1 Gameplay

- Full stereo 3D with 6-DoF head tracking: the first-person view follows
  the headset's orientation and position on top of the game's own camera.
  Each eye renders at the runtime's recommended resolution times the
  configured pixel scale, from the first frame (03 §4).
- Lost tracking never breaks the view: while the runtime reports the head
  pose invalid, the last valid pose is used (the neutral pose if there has
  been none yet), and looking straight up or down keeps the previous yaw;
  no pose or yaw is ever NaN (03 §2).
- The game's own camera sway, breathing motion and DK2-era neck correction
  are off; they cause discomfort in a headset (05 §7).
- Screen-space ambient occlusion is off (it bleeds across the two eyes).
- The HUD (motion tracker overlay, prompts, inventory, objectives) is
  shown on a flat panel locked to the head, 0.55 m wide, 1.0 m in front of
  the eyes (05 §4.3).
- Lighting is the same in both eyes (08 §1); small objects do not pop in
  and out at the edges of the view (08 §2); planar reflections are rendered
  per eye (07); the game's FXAA and SMAA run in stereo (08 §3).
- Head-in-wall blackout ("clipping blinders"): when the head moves into
  level geometry the view fades to black, after 0.6 s of contact so that
  walking through a doorway does not flash black (08 §4). An option weakens
  the trigger so that leaning near walls does not set it off.
- Cutscenes and level fades play in stereo and are never blacked out by
  the mod.

### 2.2 Menus, videos and loading

- The front-end menu, the pause menu, loading screens and the title/attract
  video are shown on a **virtual screen**: a flat, level screen 1.5 m in
  front of the player at eye height, fixed in the world (03 §7, 05 §4.2,
  09). It is placed in front of the player's facing when the screen opens
  and after each recenter.
- While a loading screen is up the 3D scene behind it is black.
- The menus use focus navigation (pad style). The mouse cursor, when used,
  maps into the menu's own 1280x720 space, not the stereo back buffer.

### 2.3 Input

- VR controllers drive the game through its gamepad: to the game there is
  always a connected Xbox-style controller, whose buttons and sticks come
  from the VR controllers and from a real gamepad if one is attached.
  Keyboard and mouse keep working. Default bindings exist for Valve Index,
  Oculus Touch (Quest, Rift), Windows Mixed Reality and Vive wand
  controllers, rebindable in SteamVR (04 §5.8).
- Controller prompts in the UI show VR controller glyphs.
- **Snap turn** (on by default): pushing the turn stick past half deflection
  turns the player by exactly 360°/N (N = 12 by default, so 30°; N from
  {2, 3, 4, 6, 12, 24}, so 3 gives 120°). Holding the stick repeats the
  turn every 300 ms. Snap turn only acts in first-person gameplay, not on
  the virtual screen. When snap turn is off, or while keyboard and mouse
  are in use, the game's own smooth stick and mouse look applies (04 §6,
  §7).
- **Recentering** sets the player's forward direction to where the head is
  facing (yaw only) (04 §8, 03 §2):
  - by hand: LB+RB on a gamepad, Ctrl+Alt on the keyboard, or the game's
    own "reset view" menu item;
  - automatically when the headset is put on (the headset is in use and
    upright), so a game started with the headset lying on the desk still
    comes up level and facing the right way;
  - automatically when a loading screen closes, so gameplay starts facing
    where the player faces;
  - when the OpenXR runtime recenters its space.
  A recenter is never taken from an invalid head pose; it stays pending
  until a valid pose arrives. The virtual screen is re-placed after every
  recenter.

### 2.4 Desktop

The game window shows a mirror of the headset view: the right eye by
default, or the left eye or both eyes; optionally symmetric (03 §8). The
window is only a mirror: the game renders to a side-by-side target sized
for the headset, independent of the window size. The window's back buffer
has the window's size, not the headset's (03 §9.2); the window stays
windowed while a headset is in use (fullscreen is refused, 03 §9.4) and
becomes borderless over its whole monitor when the size the game picked
would not fit, so it looks as fullscreen did and the menus and the cursor
line up (03 §9.3). Another application taking the desktop focus (SteamVR,
a streaming client) does not pause the game while a headset is in use;
keyboard and mouse then wait until the game window is clicked, the
controllers keep working (03 §9.6).

### 2.5 Audio

On runtimes that report the headset's audio device (Meta's OpenXR runtime),
the game's sound plays through the headset whatever the Windows default
device is. On SteamVR the game follows Windows' default device, which
SteamVR can switch itself (10).

### 2.6 Options menu page

The game's Options menu gains a VR page. Its settings take effect
immediately unless noted, and are saved to the config file (06). Each
setting is named below by its meaning and its `mothervr.json` key (06 §1
has them all); the labels on the page are the implementation's own wording
(06 §5).

- VR runtime (`VRRuntime`): None, OpenXR or OpenVR (SteamVR). Changing it
  quits the game; the choice applies at the next start. Value 1, formerly
  "Oculus", selects OpenXR and is labelled OpenXR (compatibility with
  existing config files, 06).
- Which eye the desktop mirror shows (both, left, right;
  `MirrorViewType`) and whether it is symmetric (`SymmetricMirrorView`).
- Pixel scale (render resolution relative to the runtime's recommendation,
  which already includes SteamVR's own resolution setting; capped so the
  side-by-side target stays within 16384 pixels in each direction, D3D11's
  texture size limit).
- Head-in-wall blackout on/off (`ClippingBlinders`).
- Snap turn on/off (`SnapRotation`) and the number of snap turn segments
  (`SnapRotationSegments`).
- VR input options passed to the game's scripts: toggle grips (and the
  stored-only relative walking, strafing and toggle crouch, 06 §1).
- Skipping the start-up logo and intro sequence (`SkipStartupIntro`, from
  the next launch).
- Developer diagnostics (`DebugOptions`): shows the game's hidden "Select
  Level" and debug checkpoint entries in the front end, and turns on extra
  diagnostics in the log (06 §1.3, 11 §4).

Pixel scale and mirror changes apply without a restart: the mod asks the
game to rebuild its render targets, a pause of about 0.2 s, and only when
one of those settings actually changed (opening a menu never causes it,
12 §6.3).

The page and its glyph images are the project's own work. They are built
at install time into the player's own copy of the game's UI archive
(`DATA\UI.PAK`), producing `DATA\MOTHER.PAK`; no game file and no other
mod's UI file is shipped (06, 14).

### 2.7 Config file

`mothervr.json` in the game directory holds every option of §2.6, plus the
stereo anti-aliasing policy (`Auto` by default: anti-aliasing in stereo only
at pixel scale 1.3 or below, where the eyes are not already supersampled;
`On`; `Off`), which has no menu control. It is read at start-up; missing or
invalid entries take their defaults and out-of-range values are sanitised;
it is rewritten when an option changes, keeping keys it does not know. On
the first start without a config file the player is told once where the VR
options are, and the file is written only after that message is closed,
so that a process that exits first (Steam relaunching the game) leaves the
one-time message for the next launch (13). The format, the command-line
switches (`-vrforcenone`, `-vrforceopenvr`, `-vrforceopenxr`,
`-vrforceoculus`, `-vrscale`) and the `mvr_*` console commands are in 06.

### 2.8 Which runtime runs

- A fresh config file selects **OpenVR** (SteamVR).
- **None** is the unmodded game: no stereo device, no game patch, no shader
  patch, no D3D11 wrapping beyond the pure forwarders; only the UI set that
  puts the VR page in the Options menu, so VR can be turned back on (01
  §2.3).
- When the configured runtime is not installed, the mod falls back to the
  other installed one (OpenVR first, then OpenXR); a runtime forced on the
  command line never falls back. With nothing to run, the game runs flat on
  the stereo path's no-device behaviour (02 §7.5) and the log says why.
- SteamVR need not be running when the game starts: starting OpenVR may
  launch it, and the mod does not look for its process. OpenVR fails to
  start only if the runtime cannot start or no headset appears within a
  bounded wait of about 10 s (project decision: long enough for SteamVR to
  come up, short enough that a player without a headset is not kept
  waiting) (03, 12 §7).
- When the runtime is installed but fails to start (no headset, runtime
  error), the player is told once with the runtime's error, VR stays off
  for the session, and the game continues on the monitor at its normal
  resolution, never stuck in stereo (12 §7). The options page then shows
  runtime None while the stored choice is kept, so the player can retry
  it or pick the other runtime; an OpenXR failure message also says how to
  go back to SteamVR (06 §5.2, 13 §5.3).
- When SteamVR does not show the game in the headset although VR started
  (its Desktop Game Theatre is the usual cause), the player is warned once,
  without the game stopping, and told which Steam setting to turn off
  (03 §6.7, 09 §3.1).
- If the player's `UI.PAK` is missing or not the expected one, VR works
  without the options page (06, 14).
- The mod never needs elevation, a registry change or a renamed game
  folder. If the game folder is unknown or not writable, the game still
  starts and each missing file degrades on its own: no options page
  without `MOTHER.PAK`, the runtime's own loader without a local
  `openvr_api.dll`, log output to the debugger if `mothervr.log` cannot be
  opened (12, 13).
- If `AI.exe` is not a supported build, or its code at any patched place is
  not what the mod expects, the mod patches nothing and the game runs
  unmodified; an unknown build is reported once (01 §2.4, §2.5).

### 2.9 Diagnostics

The mod writes `mothervr.log` next to the game: one session per process,
every decision and failure once, nothing per frame (11). A crash is
recorded in the log too (where it happened, the registers and the
callers), and Windows still records it as without the mod. Players send
this file with a bug report (11 §6, 13).

## 3. Platform

| Item | v1 |
|---|---|
| OS | Windows 11 and Windows 10 |
| Game | Alien: Isolation, `AI.exe` 2015 build (PE timestamp 0x54E3199D), the primary target. The 2020 build (0x5FD25676; Steam, GOG, Epic) is recognised and patched from its own RVAs after the same preflight. 2020 values were taken from the earlier mod's per-build address table (and, for inline sites, its installer) and have not been verified against a 2020 `AI.exe`; the build is not verified in game, and every feature whose 2020 address is unknown stays off there (01 §3.2) |
| Graphics API | Direct3D 11, as the game uses |
| Entry | a 32-bit `d3d11.dll` proxy in the game directory (01). A `dxgi.dll` proxy is not loaded from the game directory on Windows 11 (§4.1 item 1) |
| VR runtime | OpenVR (SteamVR, 03) and OpenXR (09), chosen in the options (§2.8). The Oculus native (LibOVR) runtime is not supported; Meta headsets work through OpenXR (Meta's runtime is an OpenXR runtime) or SteamVR. A config file or command line naming "Oculus" selects OpenXR (06) |
| Controllers | read through SteamVR Input (legacy OpenVR input as a fallback) or OpenXR actions, with default bindings per controller type (04 §5.8) |
| Dependencies | none for the player: no Visual C++ runtime, no elevation, no registry change, no folder rename; SteamVR need not be running; the mod writes the files 13 lists |

## 4. Known problems that v1 fixes

### 4.1 Problems of the earlier mod

Each item states the effect observed in game and the requirement v1 meets.

1. **Half-resolution eyes.** Each eye showed half the headset's
   resolution. The game builds its stereo render target from device slot
   16's answer inside its configure-rendering wrapper (02 §6.7), once at
   start-up, and also takes its display mode from that size; it does not
   rebuild the target when stereo starts. If slot 16 reports anything but
   the full configured size at that first query, the eyes stay at that
   size for the session. v1 answers with the full configured size from the
   first query, and keeps the swapchain windowed while a headset is in use
   (the side-by-side target is larger than any display mode) (03 §4.2).
2. **Does not load on Windows 11.** A `dxgi.dll` proxy in the game
   directory is ignored by Windows 11. v1 is a `d3d11.dll` proxy (01 §1).
3. **Eye layout mismatch.** Each eye could show a different slice of the
   image, which happens when the eye viewports are laid out for a size
   other than that of the texture the game created. v1 derives the eye
   layout from the actual texture and keeps it until the game creates a
   texture of a new size (03 §4.3).
4. **Upside-down or tilted menus when the headset is not worn at launch.**
   The view was centred once at launch, whatever the headset was doing.
   v1 recenters automatically when the headset is put on and when gameplay
   starts (§2.3).
5. **Gamepad dead while the mod is loaded**, and controller prompts missing.
   v1 keeps a real gamepad working alongside the VR controllers (04 §3).
6. **Floor reflections in stereo.** The game renders planar reflections
   once, mono, with the flat camera's projection, and samples them in
   screen space over the whole side-by-side target, so reflections slid
   against head motion and twisted on roll. v1 renders the reflection per
   eye (07).
7. **Controls fitted only Vive wands under OpenVR.** v1 maps Valve Index,
   Oculus Touch and Windows Mixed Reality controllers' thumbsticks and
   buttons properly, through SteamVR Input with default bindings (04).
8. **Lighting that differs between the eyes.** Several hundred of the
   game's pixel shaders add a dither computed from the absolute pixel
   position; on the right half of the side-by-side target it overflows and
   forces surfaces fully lit, worse at high resolution (it looked like
   reflections moving against the head). v1 patches those shaders to use
   the position relative to the target (08 §1).
9. **Object pop-in.** The game's per-object distance/LOD test and its
   occlusion test are computed for the single centre view, so small props,
   decals and shadows appear and vanish in the headset. v1 keeps every
   object that either eye can see (08 §2).
10. **No anti-aliasing in VR.** The game's stereo output path never calls
    its AA pass, whatever the AA option says. v1 runs FXAA and SMAA T1x in
    stereo; SMAA T2x runs as T1x, because the temporal mode has no per-eye
    history (08 §3).
11. **Abrupt head-in-wall blackout.** The view goes black on the first
    frame the head touches geometry. v1 allows 0.6 s of contact before the
    blackout starts (08 §4).
12. **Desktop mirror in a corner.** With the eyes at full size the mirror
    image filled only a corner of the window. The mirror must scale from
    the actual eye size, so the image fills the window (03 §8, 08 §5).
13. **Stale controller input when the runtime's dashboard has focus.** v1
    reads the controllers as released while the game lacks input focus
    (04 §5.12).
14. **Settings changed from the menu did not reach the renderer** until a
    restart. v1 rebuilds the render targets when pixel scale or mirror
    settings change (§2.6).
15. **A console command with arguments that is not one of the mod's lost
    its arguments** before reaching the game. v1 passes such commands on
    unchanged (06 §4.2).

### 4.2 Robustness required beyond the earlier mod

16. **Calling conventions of replaced game methods.** The game calls its
    gamepad, stereo device and UI methods as `thiscall` and relies on each
    callee popping exactly its own stack arguments. Any replacement that
    pops a different number of bytes, or reads arguments from outside its
    own frame, corrupts the game's stack. v1 matches the game's ABI for
    every method it replaces (02 §2, 12 §5).
17. **No leaked D3D11 references.** Every D3D11 reference the mod obtains
    is released; nothing leaks per frame.
18. **Input bounds.** Every index into the mod's own input state is
    bounds-checked, and the mod never adds more bindings to a game action
    than the game's action maps hold (six per action) (04 §9).
19. **Verify before writing.** Every patch site is checked before anything
    is written; a mismatch in a core site patches nothing, an optional fix
    with a mismatch stays off on its own (12 §4, 01 §2.5).
20. **No heavy work under the loader lock.** The system `d3d11.dll` is
    loaded on the first forwarded call, not at attach. A local
    `openvr_api.dll` is in place before OpenVR starts, whichever way OpenVR
    was reached (configured, by fallback, or chosen later); its freshness
    check does not read the whole file on every launch. A message box
    raised at attach is never shown under the loader lock, appears in front
    of the game window, and the game's device creation does not complete
    until it is closed; how that is arranged is the implementation's
    choice. Only the build of `MOTHER.PAK`, which must be ready before the
    game opens its UI archive, runs at attach (12 §2.3, 13).
21. **Paths outside the ANSI code page** (install folder, user name) work
    everywhere; nothing throws on them (11 §1.1).
22. **Runtime failure leaves the game playable**: the device detaches, the
    runtime is shut down, the player is told once, and the game continues
    flat; a display reset never starts the runtime twice (12 §7).
23. **Data crossing threads is never torn**: whenever the game thread,
    the render thread or a job thread reads head poses, views, hand poses
    or the device state written by another thread, it sees one complete,
    consistent value (12 §3).
24. **No work at process exit**: no runtime shutdown or D3D release while
    the process ends (12 §7).
25. **Hooks on shared DXGI and D3D11 vtables act only on the game's own
    swapchain** (12 §4.4).
26. **Files written completely or not at all**: a partly written
    `openvr_api.dll` is removed and never treated as current; `MOTHER.PAK` is
    rebuilt when the player's `UI.PAK` changes (13, 14).
27. **The first-person camera pointer is forgotten when the camera is
    deactivated**, so no write goes to a freed object after a level unload
    (05 §6.1).

### 4.3 Performance required

Measured with the eyes at 2688x2688 on SteamVR at 90 Hz; each is required
behaviour.

28. **Pixel scale cap**: the side-by-side target stays within D3D11's 16384
    texture limit; the log states the megapixels (03 §10).
29. **Culling bypass limited to what stereo needs**: the distance/LOD test
    passes in every view, the occlusion test passes only for the main view
    (the only one that has it); the frustum test and the shadow lights'
    view-volume test stay the game's (08 §2). Disabling every test cost
    about 1 ms of render-thread time.
30. **Mirror back buffer at window size**, not at the side-by-side size
    (about 0.3 ms GPU and 250 MB saved at 8064x4032) (03 §9.2).
31. **Stereo anti-aliasing policy**: `Auto` skips AA above pixel scale 1.3,
    where it cost about 0.6 ms GPU for little gain (08 §3, §2.7).
32. **Display reset only on a change** of pixel scale or mirror settings,
    not on every menu, pause or loading screen (each reset is a 0.2 s
    freeze) (12 §6.3).
33. **OpenXR views predicted for the right frame**: the views the game
    renders with are predicted for the display time of the frame they are
    submitted in, not one display period (11 ms at 90 Hz) later; this
    needs the next frame to be waited, begun and its views located before
    the game starts rendering it, so it happens as soon as the previous
    frame has been submitted. The eye images are copied and flushed before
    `xrEndFrame`, because OpenXR requires swapchain images to be released
    before the frame that uses them ends (09 §5.3).
34. **Fresh controller state**: the game receives the controller state
    sampled right after the runtime's wait (OpenVR `WaitGetPoses`, OpenXR
    frame begin), so its input is from the same runtime frame as its head
    pose rather than a frame older (04 §5.9).
35. **Shader patch pre-filter**: only shaders containing the dither's
    constant are hashed and parsed (about 1 s of game-thread time saved per
    session) (08 §1, 12 §6.2).
36. **Log opened once**, one append per event; no periodic render-thread
    line (11 §1.2, §3).
37. **No double wait on the OpenXR virtual-screen path**: when the runtime
    paced the frame, the game's `Present` runs without vsync (09).

## 5. Glossary

- **Rift mode**: the game state where graphics setting 0x18 "Stereo Mode"
  equals "Rift". The game's stereo path runs only in Rift mode; the mod
  makes every test of it succeed (02 §6.6).
- **Stereo object**: the game's 0xB0-byte object that owns stereo
  rendering: device pointer, UI render targets, virtual-screen matrix,
  stereo mode and mode stack, recenter reference, fade state. Layout in 02.
- **Stereo device**: the 21-method HMD interface the stereo object calls
  (originally a LibOVR wrapper). The mod supplies its own implementation in
  the game's layout (02).
- **Side-by-side render target** (SBS target): the single target the game
  renders both eyes into, left eye in the left half and right eye in the
  right half. Its size is what the stereo device reports.
- **Eye viewport**: the rectangle of the SBS target one eye renders to and
  the runtime reads from.
- **Stereo mode**: 0 = world (3D scene with the HUD panel), 1 = virtual
  screen (2D UI on a world-fixed quad).
- **Stereo mode stack**: the stack of stereo modes the game pushes and pops
  as screens open and close. In v1 it is at most one deep (05 §4.4).
- **Virtual screen**: the world-fixed flat screen that shows menus, videos
  and loading screens in stereo mode 1.
- **HUD panel**: the head-locked quad that shows the game's 2D UI layer in
  stereo mode 0.
- **Recenter**: taking the current head yaw (and position) as the new
  reference forward and origin.
- **Blackout** / **scene visibility**: the stereo object's multiplier on
  the 3D scene (1 visible, 0 black), used by the head-in-wall fade and
  during loading.
- **Clipping blinders**: the head-in-wall fade, and the option
  (`ClippingBlinders`) that sets its distance threshold.
- **FollowBehavior**: the game's first-person camera behaviour (the name
  its `GetName` returns). Owns yaw, pitch and the look-input gate.
- **POV**: a game camera view (main view, reflection view, ...), managed
  by the POV manager. POV 0 starts with the world-to-view matrix.
- **Action map**: one of the game's three tables mapping game actions
  (move, turn, crouch, confirm, ...) to device controls, held by the input
  manager.
- **Action routing**: VR controller state reaches the game per game
  action, through the game's gamepad device, merged with a real pad (04).
  How a pad query is attributed to an action is the implementation's
  choice; any code it puts in the game's action maps lies outside the
  game's own pad codes (0x00..0x17), is not -1, fits in a signed 16-bit
  value, and identifies its action map and action (04 §3).
- **Pixel scale**: multiplier on the runtime's recommended per-eye size.
- **Snap turn segments**: the number N of equal steps in a full turn.
- **Mirror**: the desktop window's copy of the headset view.
- **Display reset**: the game rebuilding its swapchain buffers and render
  targets, requested through its display-reset flag.
- **Game thread**, **render thread**, **job threads**: the game's threads
  the mod runs on (12 §2).
- **Core patch** / **optional fix**: a patch without which the stereo
  hand-off cannot work (all or nothing), and a fix the stereo hand-off
  does not depend on, which turns off on its own when its site differs
  (12 §4.2).
- **RVA**: an address relative to `AI.exe`'s image base (01 §3.3).
- **Site**: an instruction in `AI.exe` that the mod patches or redirects.

## 6. Documents

See `README.md` for the list. Order of reading: 00, 01, 02, 12, then 03 to
14 as needed. `ANSWERS.md` answers the implementation side's questions and
names the sections each answer changed.
