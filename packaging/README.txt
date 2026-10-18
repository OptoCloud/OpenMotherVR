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
  Dev. Debug Options on), openmothervr_engine_settings.xml, and
  DATA\MOTHER.PAK with DATA\MOTHER.PAK.src.
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
- Clip Blinding (Options > MotherVR): the view fades to black when you
  move your real head into a wall, or more than about half a metre from
  your body, after half a second. Only in normal play: cutscenes,
  interactions (hacking, levers, ladders...) and hiding never black out,
  however you move your head.
- VR controllers (Index, Quest/Touch, WMR, Vive wands, Steam Frame) have
  default layouts. Index/Touch: left stick move (click: sprint), right
  stick turn (up/down: weapons, click: radial menu), A interact (hand near
  your head: flashlight), B reload/back, X map, Y flashlight, left
  trigger crouch, right trigger fire, left grip motion tracker, right
  grip aim. Pause: menu button (Index: press the left trackpad).
  Steam Frame: as Touch, except the left d-pad works like a gamepad's
  d-pad (moves through menus; in play left/right pick the previous/next
  weapon), the View button opens the map, the left bumper toggles the
  flashlight, and the right Menu button pauses. If an older custom binding
  for the game puts the flashlight on the d-pad, reset it to the default.
  Rebind anything in SteamVR > Settings > Controllers > Manage
  controller bindings > Alien: Isolation.
- An Xbox controller works too.
- Settings: in game, Options > MotherVR. Or edit mothervr.json while the
  game is closed. The Comfort, Controls, Hands and Body pages there hold
  the settings of the same names below; a setting marked "Requires
  restart!" applies from the next launch.
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
- Comfort (preview; Options > MotherVR > Comfort), in a "Comfort"
  object under "Settings":
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
  - "PlayMode": "Seated" (the default) puts your eyes at the character's
    eye height at every recenter. "Standing" uses your real height: stand
    straight for the normal view, duck or kneel to go lower, even when a
    recenter happens while you crouch. Applies at the next recenter.
  - "StandingEyeHeight": your standing eye height in metres (1.0 to 2.2)
    for "Standing". 0 (the default) measures it at the first recenter
    while you wear the headset in standing mode, and saves it.
  - "WorldScale": 0.5 to 1.5 scales your head movement in the game
    (default 1.0). -vrscale on the command line overrides it for that run.
- Body (preview; Options > MotherVR > Body), in a "Body" object under
  "Settings":
  - "SeatedEyeRaise": -0.5 to 0.5 metres moves your eyes up (or down) in
    seated play, for a low chair or a high stool. Default 0.
  - "EyeForward": -0.2 to 0.2 metres moves your eyes forward of (or back
    from) the character's body. Default 0.
  - "Visible": which parts of your character you see: "Full" (the
    default), "ArmsAndHands", "HandsOnly" or "None" (only the item in your
    hand). Hidden parts are also gone from reflections and shadows.
  - "RealCrouch": true, in standing play with a calibrated height: crouch
    for real to make the character crouch, stand up to stand (the game
    still keeps you down in a vent). Your view stays at your real head.
    The crouch button keeps working. Default false.
  - "RoomScale": not available yet; turning it on changes nothing.
- Hands (preview; Options > MotherVR > Hands), in a "Hands" object
  under "Settings":
  - "Enabled": true uses your tracked controllers in the game (default
    false). It applies at once, also when turned on in the menu (a custom
    SteamVR binding for the game made with an older version may lack the
    pointing pose: the log then says so, and a reset to the default binding
    fixes it). The in-game arms still play the game's own animation for
    now.
  - "AimFromHand": with Enabled, the revolver, shotgun, bolt gun and
    flamethrower aim where your right controller points, not where you
    look; the drawn weapon does not follow your hand yet. Melee weapons
    keep the game's aim. Default true.
  - "TrackerInHand": with Enabled, the motion tracker's cone points where
    your left controller points. Default true.
  - "Smoothing": 0 to 1 steadies the hands; higher steadies more but lags
    behind. 0 (the default) is off.
  - "Haptics": an object of its own inside "Hands" for controller
    vibration; works with or without "Enabled":
    - "Enabled": true makes the controllers vibrate for the game
      (default false). Set it before starting the game; a custom SteamVR
      binding for the game made before needs a reset to the default.
    - "Strength": 0 to 1 scales every vibration (default 1).
    - "GameRumble": the game's own pad rumble, low motor in the left
      hand and high motor in the right. "Damage": being hurt and dying.
      "Tracker": a tick in the left hand per scan while the motion
      tracker is up. "Firing": a buzz in the right hand per shot.
      "Hacking": the start and end of hacking and cutting. Each
      default true.
- Controls (preview; Options > MotherVR > Controls), in a "Controls"
  object under "Settings":
  - "InteractionAwareTurning": true stops the right stick turning you
    while you use a keypad, terminal, lever or other interaction, during
    the breathing game while hiding, and in cutscenes; mouse look still
    works. Default false.
  - "DpadEmulation": true makes the left stick a d-pad in menus, during
    interactions and the breathing game while hiding, and in play while
    you hold the left stick click (sprint) for a moment; on Vive wands,
    click the left touchpad on an edge. Walking stops while a d-pad
    direction is pressed. A controller with its own d-pad (Steam Frame)
    uses it whatever this says. Default false.
  - "VrOffHotkey": true lets you turn VR off for the rest of the session
    by holding right Ctrl + right Shift + F10 for one second; the game
    goes on flat on the monitor. VR comes back at the next launch.
    Default false.
  - "CrashSafeLog": true writes mothervr.log to disk as it goes, and adds
    the mod's state to a crash record, so the log survives even a PC
    crash. Applies from the next launch. Default false.
  - "RecenterGesture": true recenters when you hold both grips with both
    hands at your face for about a second (not while you hold the
    trigger). Default false.
  - "HolsterGestures": true: squeeze a grip over your right shoulder for
    the next weapon, over your left shoulder for the previous one, at
    your left hip for the flashlight, and in front of your chest for the
    motion tracker. A squeeze anywhere else aims or raises the tracker as
    usual. Off in menus and interactions. Default false.
  - "HidingLean": true: in a locker or other hiding place, lean your head
    sideways or back to peek and lean. Small movements do nothing.
    Default false.
  - "HoldBreathGesture": true: when the game asks you to hold your breath
    while hiding, cover your mouth with a hand (the grip still works).
    Default false.
  - "HudToggleGesture": true: hold your left hand at the side of your
    head and press Map to hide or show the HUD. It never shows a HUD the
    game is hiding, for example in a scene. Default false.
  - "ControllerGlyphs": true makes button prompts and View Controls show
    the buttons of the controllers you hold (Index, Quest/Touch, WMR, Vive
    wands, Steam Frame), per hand. It applies the next time a controller
    is detected (switch it off and on, or restart). Default false.
- Hud (preview; Options > MotherVR > HUD), in a "Hud" object under
  "Settings":
  - "Crosshair": "On" (default), "Off", or "Auto" (hidden while you aim
    with the hand). The ammo count stays.
  - "Hidden": true starts with the HUD hidden (health, ammo, prompts,
    objectives); subtitles stay. Default false.
  - "ItemHighlight": false: items you can pick up do not glow; prompts
    still work. Default true.
  - "Placement": "HeadLocked" (default) or "Follow" (level, catches up
    when you turn more than about 20 degrees); "Distance" 0.6 to 2.0 m,
    "Width" 0.3 to 1.2 m (a smaller HUD has smaller text), "Pitch" -30 to
    10 degrees (negative: below your view).
- Interaction (preview; Options > MotherVR > Interaction), in an
  "Interaction" object under "Settings":
  - "TerminalTextScale": 1 (default), 1.25, 1.5 or 1.75 (Terminal Text
    Size 100% to 175%) makes the text of terminal documents larger; long
    documents still scroll to the end. Entry titles and folder names grow
    as far as their boxes allow. Applies at the next launch.
  - "PhysicalLevers", "CameraTakeovers", "TerminalDistance" and
    "RewireDistance" are saved but do nothing in this version; their
    items are greyed with "Not available yet".
- Graphics (preview; Options > MotherVR > Graphics), in a "Graphics"
  object under "Settings". The engine settings apply at the next launch;
  the three effects in the headset at once:
  - "EngineSettingsOverride": true adds the options below to the game's
    Video options. The game's own DATA\ENGINE_SETTINGS.XML is never
    changed: each launch the mod writes openmothervr_engine_settings.xml
    in the game folder from it, and the game reads that instead.
    Default false.
  - "ShadowMap4096": true makes Video > Shadow Map Resolution offer 4096
    (sharper shadows, at a GPU cost).
  - "SharpReflections": true makes Video > Planar Reflections offer
    Alternative: sharp, mirror-like reflections without blur.
  - "StreamingTextureHeapMB": 0 keeps the game's size, or 300 to 1024.
  - "StreamingModelHeapMB": 0 keeps the game's size, or 128 to 512.
  - "LensFlares", "ChromaticAberration", "FilmGrain": false turns that
    effect off in the headset only, whatever the game's Video setting;
    flat play and your saved game settings are unchanged. Default true.
    Turning one back on applies at once; turning off one that was on
    when the game started applies at the next launch.
  - Shadow maps, reflections and the two memory sizes are greyed on the
    page while Engine Settings Override is off.

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
