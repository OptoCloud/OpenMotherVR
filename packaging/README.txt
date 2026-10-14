OpenMotherVR
============

A rebuild of the MotherVR VR mod for Alien: Isolation that works on
Windows 10 and 11 with SteamVR or an OpenXR runtime.
Source, releases and issues: https://github.com/OptoCloud/OpenMotherVR

Requirements
- Alien: Isolation on Steam (the build Steam installs; GOG/Epic not tested)
- SteamVR, with your headset and controllers working in SteamVR Home
- Nothing else: no Visual C++ runtime, no registry fix, no original
  MotherVR needed

Install
1. Copy d3d11.dll into the game folder, next to AI.exe.
   (Steam: right-click Alien: Isolation > Manage > Browse local files)
2. Start SteamVR, then start the game from Steam.
   On first start the mod builds DATA\MOTHER.PAK (its options menu)
   from the game's own UI.PAK, and writes openvr_api.dll,
   mothervr.json and the mothervr_input folder (controller bindings).

Uninstall
- Delete d3d11.dll, openvr_api.dll, mothervr.json, mothervr.log, the
  mothervr_input folder and DATA\MOTHER.PAK from the game folder.

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
- "Pixel Scale" multiplies the resolution SteamVR (or your OpenXR runtime)
  asks for, which already includes SteamVR's own resolution slider. Keep it
  at 1.0 and use SteamVR's slider instead; 1.5 renders 2.25 times the
  pixels and roughly doubles the GPU cost.
- "StereoAntiAliasing" in mothervr.json: "Auto" (the default) smooths edges
  at Pixel Scale 1.3 or below and skips it above, where supersampling
  already does the job; "On" or "Off" force it.

If something goes wrong
- Send mothervr.log from the game folder.
- Crashes: also check Windows Event Viewer > Windows Logs > Application
  for an "AI.exe" error and send its details.

Known issues
- No hand tracking yet (controllers act as a gamepad).
- Please report anything odd as an issue on GitHub, with mothervr.log.
