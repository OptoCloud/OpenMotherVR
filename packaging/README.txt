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
  "Playing Alien: Isolation VR" on your Discord profile, with whether VR
  is on and which runtime runs it; false turns it off.

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
