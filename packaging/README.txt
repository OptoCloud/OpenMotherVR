OpenMotherVR
============

A rebuild of the MotherVR VR mod for Alien: Isolation that works on
Windows 10 and 11 with SteamVR or an OpenXR runtime.
Source, releases and issues: https://github.com/OptoCloud/OpenMotherVR

Requirements
- Alien: Isolation on Steam (the build Steam installs; GOG/Epic not tested)
- SteamVR, with your headset and controllers working in SteamVR Home;
  or, for the OpenXR mode, an OpenXR runtime that supports 32-bit
  games (the game is 32-bit)
- Nothing else: no Visual C++ runtime, no registry fix, no admin rights,
  no original MotherVR needed

Install
1. Copy d3d11.dll into the game folder, next to AI.exe.
   (Steam: right-click Alien: Isolation > Manage > Browse local files)
2. In Steam, open the game's Properties > General and uncheck "Use
   Desktop Game Theatre while SteamVR is active". Left on, SteamVR shows
   the game as a flat screen instead of in VR.
3. Start SteamVR, then start the game from Steam.
   On first start the mod builds DATA\MOTHER.PAK (its options menu)
   from the game's own UI.PAK, and writes openvr_api.dll,
   mothervr.json and the mothervr_input folder (controller bindings).

Uninstall
- Delete d3d11.dll from the game folder. That alone restores the
  unmodded game.
- To remove everything the mod wrote, also delete from the game folder:
  openvr_api.dll, mothervr.json, mothervr.log and mothervr.log.1 (the
  older log, if present), the mothervr_input folder, any
  mothervr_reflection_*.dds files (developer debug dumps, only made with
  Dev. Debug Options on), and DATA\MOTHER.PAK with DATA\MOTHER.PAK.src.
  A game that was closed while saving can leave mothervr.json.tmp or
  DATA\MOTHER.PAK.tmp behind; delete those too.
  Never delete DATA\UI.PAK: it is the game's own file.
- Steam's "Verify integrity of game files" is not needed and does not
  remove these files.

Playing
- Menus, videos and loading screens appear on a screen in front of you;
  gameplay is full 3D with head tracking.
- Recenter: Ctrl+Alt, or LB+RB on a gamepad. The view also recenters by
  itself when you put the headset on and when a level finishes loading.
- VR controllers (Index, Quest/Touch, WMR, Vive wands) have default
  layouts. Index/Touch: left stick move (click: sprint), right stick
  turn (up/down: weapons, click: radial menu), A interact (hand near
  your head: flashlight), B reload/back, X map, Y flashlight, left
  trigger crouch, right trigger fire, left grip motion tracker, right
  grip aim. Pause: menu button (Index: press the left trackpad).
  Rebind anything in SteamVR > Settings > Controllers > Manage
  controller bindings > Alien: Isolation.
- An Xbox controller works too.
- Settings: in game, Options > MotherVR. Or edit mothervr.json while the
  game is closed.
- VR off (runtime None, or -vrforcenone): the game plays as unmodded,
  with only the MotherVR options page added, so VR can be turned back
  on from there. The mod then changes no shaders, window, SSAO, camera
  or pad behaviour.
- Launch options (Steam: Properties > General > Launch Options), for
  this run only: -vrforceopenvr (SteamVR), -vrforceopenxr (OpenXR),
  -vrforcenone (no VR), -vrscale <0.5 to 1.5> (world scale). If several
  -vrforce options are given, the last one wins.
- "Pixel Scale" multiplies the resolution SteamVR (or your OpenXR runtime)
  asks for, which already includes SteamVR's own resolution slider. Keep it
  at 1.0 and use SteamVR's slider instead; 1.5 renders 2.25 times the
  pixels and roughly doubles the GPU cost.
- "StereoAntiAliasing" in mothervr.json: "Auto" (the default) smooths edges
  at Pixel Scale 1.3 or below and skips it above, where supersampling
  already does the job; "On" or "Off" force it.
- "DiscordRichPresence" in mothervr.json: true (the default) shows
  "Playing Alien: Isolation VR" on your Discord profile, with the area
  you are in and what you are doing (loading, paused, hiding, hacking...);
  hover the picture for the VR runtime. false turns it off. With VR
  disabled it shows only "VR off".
- Comfort (preview, mothervr.json only for now), in a "Comfort" object
  under "Settings":
  - "SmoothTurnSpeed": 30 to 360 degrees per second turns smoothly with
    the right stick when Snap Rotation is off (the further you push, the
    faster). 0 (the default) keeps the game's own stick turning, scaled
    by Controller Sensitivity.
  - "SnapTurnRepeatMs": how often a held stick repeats a snap turn, 150
    to 1000 ms (default 300). 0 turns once per push.
  - "SnapTurnFade": true hides each snap turn behind a quick fade from
    black (about 0.1 s). Default false.
  - "Vignette": 0 to 1 darkens the edge of the view while you move with
    the stick or turn smoothly, and clears it when you stop; higher
    narrows the clear area more. 0 (the default) is off.
- Hands (preview, mothervr.json only for now), in a "Hands" object under
  "Settings":
  - "Enabled": true uses your tracked controllers in the game (default
    false). Set it before starting the game: aiming needs a pointing pose
    that is asked of SteamVR or OpenXR at start (a custom SteamVR binding
    for the game made before needs a reset to the default to get it). The
    in-game arms still play the game's own animation for now.
  - "AimFromHand": with Enabled, the revolver, shotgun, bolt gun and
    flamethrower aim where your right controller points, not where you
    look; the drawn weapon does not follow your hand yet. Melee weapons
    keep the game's aim. Default true.
  - "TrackerInHand": with Enabled, the motion tracker's cone points where
    your left controller points. Default true.
  - "Smoothing": 0 to 1 steadies the hands; higher steadies more but lags
    behind. 0 (the default) is off.
- Controls (preview, mothervr.json only for now), in a "Controls" object
  under "Settings":
  - "InteractionAwareTurning": true stops the right stick turning you
    while you use a keypad, terminal, lever or other interaction, during
    the breathing game while hiding, and in cutscenes; mouse look still
    works. Default false.
  - "VrOffHotkey": true lets you turn VR off for the rest of the session
    by holding right Ctrl + right Shift + F10 for one second; the game
    goes on flat on the monitor. VR comes back at the next launch.
    Default false.
  - "CrashSafeLog": true writes mothervr.log to disk as it goes, and adds
    the mod's state to a crash record, so the log survives even a PC
    crash. Applies from the next launch. Default false.

If something goes wrong
- The headset shows the game as a flat screen (Theatre), or the game
  starts in a small window: uncheck the Desktop Game Theatre option
  (Install, step 2), then restart SteamVR and the game. After a crash,
  check that the Theatre is not still open before starting again.
- OpenXR fails to start: choose "Use SteamVR" on the MotherVR options
  page, or start the game with -vrforceopenvr. After a failed start the
  page shows the runtime as None and offers both runtimes.
- Report problems as an issue at
  https://github.com/OptoCloud/OpenMotherVR/issues and attach
  mothervr.log from the game folder (and mothervr.log.1 if present).
- Crashes: also check Windows Event Viewer > Windows Logs > Application
  for an "AI.exe" error and send its details.

Known issues
- No hand tracking yet (controllers act as a gamepad).
- Please report anything odd as an issue on GitHub, with mothervr.log.

Licence and source
- OpenMotherVR is free software under the GNU GPL 3.0 (LICENSE.txt);
  THIRD_PARTY_LICENSES.txt lists the libraries it includes. The complete
  source of this version, with its build scripts, is the release's tag
  at https://github.com/OptoCloud/OpenMotherVR.
