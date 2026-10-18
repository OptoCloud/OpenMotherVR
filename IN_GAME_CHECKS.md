# In-game checks

What is built and unit-tested but not yet seen working in the game. Each
item says what to do and what should happen. Play with **Dev. Debug
Options** on (`"DebugOptions": true` in `mothervr.json`), then hand over
`mothervr.log`: most items name the log line that confirms them.

Settings go under `"Settings"` in `mothervr.json`, with the game closed.
Keys of one section share one object, for example
`"Comfort": {"SmoothTurnSpeed": 120, "Vignette": 1.0}`.

Tick an item once it has been seen working, with the date, runtime and
headset; move it to the roadmap in `README.md` as verified.

## Next session: haptics and gestures

A focused run for the items under Haptics and Gestures and hiding below.
Before starting: Dev. Debug Options on, `"Hands": {"Haptics": {"Enabled":
true}}`, and in `"Controls"` set `RecenterGesture`, `HolsterGestures`,
`HidingLean`, `HoldBreathGesture` and `HudToggleGesture` to true.

What each vibration should feel like (amplitude 0 to 1 before
`Strength`), so the events can be told apart:

| Do this | Feel | Controller | Log (`[diag] haptics: ...`) |
|---|---|---|---|
| One revolver or shotgun shot | one sharp short tap (0.60, 60 ms) | right | `firing pulse, amplitude 0.60, 60 ms` |
| Hold the flamethrower | a soft buzz (0.30) for as long as it fires | right | `firing pulse, amplitude 0.30` repeating |
| Tracker raised | a faint tick per scan (0.15, 20 ms) | left | `tracker pulse` |
| A small hit | a medium thump (about 0.4 to 0.5, 150 ms) | both | `damage pulse, amplitude 0.4x, 150 ms` |
| A big hit | a stronger thump, more health lost is stronger | both | higher amplitude |
| Death | one long strong buzz (1.00, 600 ms) | both | `death pulse, amplitude 1.00, 600 ms` |
| Hacking device or cutting torch starts and ends | a light tap (0.40, 80 ms) each time | both | `hacking pulse, amplitude 0.40, 80 ms` |
| Explosions, heavy doors, the Alien's attacks | follows the game's pad rumble, ends within 0.5 s of it | left low motor, right high | `game rumble set N times ...` |

Report per row: whether it happened, whether it came from the right
controller, whether it felt too weak, too strong or wrong in kind (a buzz
where a tap belongs), and any vibration with no event or one that stayed
on. Hacking steps and the Alien close by have no vibration yet (the log
says so at start-up).

Gestures, per item under Gestures and hiding: did it fire, how easy was
it, did it ever fire by accident. Also the HUD toggle: left hand at the
side of the head and Map hides or shows the HUD (`[diag] hud: ...`).

## v1: still unconfirmed

- [ ] **Desktop Game Theatre warning.** With SteamVR's Desktop Game Theatre
      on for the game, a warning tells you to turn it off.
- [ ] **Runtime None.** With the runtime set to None (or `-vrforcenone`),
      the game plays as unmodded with only the MotherVR options page added.
- [ ] **Crash record.** After a crash, `mothervr.log` has a `crash:` record
      with module, offset and callers.
- [ ] **Unexplained reports:** B in the pause menu crashing; the old menu
      lingering over the Continue video; the view sliding on snap turns
      while walking.
- [ ] **Two-handed lever (04 §5.8.5, item 30).** Thumbstick controllers,
      Dev. Debug Options on. At a two-handed lever (the game prompts "Hold
      aim and fire", then the move and look sticks down): hold the right
      grip and right trigger, push both sticks down: the lever is pulled,
      and the weapon does not change. Away from the lever the right stick
      up or down still cycles weapons. Log: `[events] "start_interaction"`
      with `interaction true` at the lever, then `stop_interaction` with
      `interaction false`.

## Blackouts and rendering

- [ ] **No wall blackout while the game drives the view.** Cutscenes,
      level fades, a level camera taking over, interactions (hacking,
      levers, ladders) and hiding: the view never goes black, however you
      move your head. Log: `[diag] head-in-wall fade ... ignored (the game
      drives the view or the head is still)`.
- [ ] **Wall blackout still works in play.** Put your real head into a wall
      and hold it: black after about half a second. Brushing past door
      frames never flashes black.
- [ ] **What darkens the view.** If the view ever goes black unexpectedly,
      the `[diag] scene visibility ...` and `[diag] head-in-wall fade ...`
      lines say which part did it.
- [ ] **Smoke lighting stays on the smoke.** Find steam or smoke lit by a
      nearby lamp (vents, Medical, Seegson corridors). Turn your head
      left, right, up and down: the lit patch stays put instead of sliding
      with your head. Smoke at the edge of view looks as it does straight
      ahead; both eyes match. Log: `[hook] particle lighting: in place on 1
      sites` and one `[diag] particle lighting: eye N ...` line per eye.

- [ ] **Sharper reflections and 4096 shadows.** `"Graphics":
      {"EngineSettingsOverride": true, "ShadowMap4096": true,
      "SharpReflections": true}`, then Options > Video: Shadow Map
      Resolution lists 512, 1024, 1536, 2048, 4096; Planar Reflections
      lists Off, On, Alternative. `DATA\ENGINE_SETTINGS.XML` is unchanged.
      Log: `[hook] engine settings: on; ... 2 addition(s)`, two `[render]
      engine settings: added ...` lines and `[render] engine settings: the
      game reads ...openmothervr_engine_settings.xml`.
- [ ] **Alternative renders sharp.** Choose it on a level with a reflective
      floor (Torrens start): the reflection is sharp, present in both eyes
      and does not smear at the eye seam. Log: the `reflection POV` line
      shows `quality 6`. 4096 shadows: sharper edges; note the GPU frame
      time.
- [ ] **Defaults and fallbacks.** Without a saved engine settings file the
      defaults are still the stock choices. Choose 4096 and Alternative,
      quit, set `EngineSettingsOverride` false: the game still starts (note
      what Video shows for the two). A broken XML in a test copy: the game
      starts stock, log `is not well-formed XML ... nothing added`.

## Virtual screen at start-up (OpenVR)

- [ ] **Start-up videos in front.** Headset worn, Skip Startup Intro off:
      every logo video, the legal screen, the title video and the main
      menu on one level screen 1.5 m ahead at head height. Log: `openvr:
      recenter: ...`, then `openvr: frames now on the screen` before the
      first video and `openvr: frames now tracked` at the first gameplay
      frame. Tester logs (Quest): if the screen is misplaced, check
      whether an event line below coincides with it; if none, the runtime
      re-anchors without telling SteamVR apps.
- [ ] **Tracking space reset.** On the title screen, reset the seated or
      standing zero pose in SteamVR (Quest: hold the Oculus button): a
      moment of black, then the screen in front of where you face. Log:
      `openvr: ... zero pose reset: the tracking space moved; ...` (or
      `chaperone universe changed`) and one `openvr: recenter:` line. With
      the headset on the desk during the reset, the screen stays black
      until it is worn. In play the same reset re-centres forward.
- [ ] **Headset on and off.** Log `openvr: headset in use (event 103)` /
      `openvr: headset idle (event 104)`, and no recenter from that alone.

## Turning and comfort

- [ ] **Smooth turn.** Snap Rotation off, `"Comfort": {"SmoothTurnSpeed":
      90}`: the right stick turns smoothly, a full circle in about 4 s held
      fully over; the stick at rest never drifts. Log: `[input] turning:`
      line at stereo start and on each change.
- [ ] **Snap repeat.** Snap Rotation on, `"SnapTurnRepeatMs": 0`: one step
      per flick; 600 makes a held stick repeat slower.
- [ ] **Snap fade.** `"SnapTurnFade": true`: no visible jump at a snap
      step, a short fade from black instead. Watch that the black frame
      is not one frame early (the old view showing darkened).
- [ ] **Vignette.** `"Vignette": 1.0`: walking with the stick narrows the
      view to a clear circle of about 25 degrees in each eye, the ring the
      same in both eyes; it clears about 0.3 s after stopping; never in
      menus, videos or cutscenes; the desktop mirror shows it. Try on
      OpenVR and OpenXR. No rendering glitches in the frames after it
      draws, and it still draws after a display-mode change.
- [ ] **No turning during interactions.** `"Controls":
      {"InteractionAwareTurning": true}`: the right stick does not turn
      you at a keypad or terminal; the next flick after it turns. Log:
      `[input] turning blocked: interaction (keypad)`, then `unblocked`.
- [ ] **Breathing game.** Hide in a locker until the breath prompt: the
      log shows `Start_Breathing_Game` / `End_Breathing_Game` with
      `breathing true` / `false`, and turning is blocked while it runs.

## D-pad and the Steam Frame

With Dev. Debug Options on, every change of the d-pad press is logged:
`[input] d-pad up true down false left false right false (emulated false)`.

- [ ] **Frame d-pad navigates menus.** Steam Frame, OpenVR: the log has
      `openvr input: SteamVR Input with 18 actions + 4 hand poses`. In the
      main menu and the pause menu the left d-pad moves the selection up,
      down, left and right (the `[input] d-pad` lines with `emulated
      false`); it never toggles the flashlight. In play left / right pick
      the previous / next main weapon. If the d-pad still toggles the
      flashlight, the binding in use is an old custom one: reset it in
      SteamVR to the default and check again.
- [ ] **Frame d-pad on OpenXR.** Same on OpenXR: the log has `openxr input:
      Steam Frame controller bindings suggested` and `openxr input:
      controller profile /interaction_profiles/valve/frame_controller_valve`
      (if the runtime does not offer `XR_VALVE_frame_controller_interaction`
      the first line is missing and the Frame runs on a converted binding:
      report it).
- [ ] **Map and Flashlight on the Frame.** The left View button opens the
      map, the left bumper toggles the flashlight, A near the head still
      toggles it, the right Menu button pauses, right A / B interact and
      reload. Options > MotherVR > Input > View Controls shows `Map Menu`
      `View Button`, `Flashlight` `Left Bumper` and `Menu Navigation`
      `Left D-Pad`.
- [ ] **Frame hand poses (OpenVR).** The Frame binding uses the pose
      components `grip` and `tip`, which are not confirmed for the Frame:
      with Dev. Debug Options the hands lines every 900 frames show both
      hands tracked (not `untracked`), and with Tracked Hands on there is no
      `hand pose actions unbound` or `aim pose actions unbound` line.
- [ ] **Other controllers unchanged.** Index, Touch, WMR and wands play as
      before; their controls help is unchanged.
- [ ] **D-pad emulation in menus.** `"Controls": {"DpadEmulation": true}`:
      in a menu the left stick moves the selection one step per push, as a
      pad's d-pad does (`[input] d-pad ... (emulated true)`), without
      skipping items at the 0.6 threshold.
- [ ] **D-pad emulation in play.** Hold the left stick click about 0.3 s,
      then push left / right: previous / next weapon, and you stop walking
      while the direction is held. A short click still sprints. At a
      keypad or terminal, or in the breathing game, the left stick is the
      d-pad without the click. On wands: click the left pad on an edge.
- [ ] **D-pad emulation off.** With the key off the left stick only walks
      and moves menus as before; no `emulated true` lines.

## Seated and standing

Room-scale (`Body.RoomScale`) and real crouch (`Body.RealCrouch`) are not
built yet: those keys do nothing.

- [ ] **Defaults are v1.** No `Comfort` or `Body` keys: no `[stereo] play
      mode` or `[stereo] world scale` lines, and the view height resets at
      each recenter as before.
- [ ] **Standing, calibrated.** `"Comfort": {"PlayMode": "Standing",
      "StandingEyeHeight": 1.70}` (your own eye height): standing straight
      gives the game's normal eye height; ducking 30 cm lowers the view
      30 cm. Kneel and load a level: after the loading screen the view is
      still low. Log: `[stereo] play mode Standing (standing eye height
      1.70 m)` at the first recenter.
- [ ] **Automatic first calibration.** `"PlayMode": "Standing"` with no
      `StandingEyeHeight`: stand straight when the game starts. Log:
      `standing eye height calibrated: N m (automatic, ...)`, and the
      value is saved in `mothervr.json`.
- [ ] **Mode change waits for a recenter.** Switch Seated and Standing in
      play: nothing jumps until the recenter right after, then the new
      mode's line is logged.
- [ ] **OpenXR floor.** On OpenXR the log has `openxr: floor reference
      STAGE` (or `LOCAL_FLOOR`). On a runtime with neither: `openxr: no
      STAGE or LOCAL_FLOOR space` and, in standing mode, `WARNING standing
      play unavailable on OpenXR` (play is seated).
- [ ] **Seated eye raise.** `"Body": {"SeatedEyeRaise": 0.2}`: the view is
      20 cm higher, seated only; the head-in-wall fade does not start in
      the open.
- [ ] **Eye forward.** `"Body": {"EyeForward": 0.15}`: the view moves
      forward of the body, less of the chest shows; the hands stay in the
      controllers; the head-in-wall fade does not start standing in the
      open.
- [ ] **World scale.** `"Comfort": {"WorldScale": 0.8}` in play (once the
      options page sets it): head motion is scaled from the next frame
      with no jump; depth looks right in both eyes. Log: `[stereo] world
      scale 0.80`. With `-vrscale 1.2` on the command line: `world scale
      1.20 (set by -vrscale ...)` and the setting does nothing.
- [ ] **Calibrate command** (once the options page has it): select it, put
      the controller down, stand straight: after 3 s the log says
      `standing eye height calibrated: N m`; with the headset off, it
      waits (`height calibration waiting`) until it is put on.

## Hands

Set `"Hands": {"Enabled": true}`, before starting or in the menu (the aim
poses are always declared). Try on OpenVR and OpenXR.

- [ ] **Defaults.** With hands off: no `[hook] hands` lines; on OpenVR the
      log reads `+ 4 hand poses` (the aim poses are always declared).
- [ ] **Installed.** With hands on: `+ 4 hand poses` (OpenVR), `[hook]
      hands, aim from hand: in place`, `[hook] hands, motion tracker in
      hand: in place`, the line that the arms stay animated, then `tracked
      hands active` in play.
- [ ] **Aim from the hand.** Revolver, shotgun, bolt gun, flamethrower with
      the head turned about 60 degrees away: shots and flame follow the
      right controller. `[diag] hands: aim ray` lines start near the hand.
- [ ] **Firing away from the view.** Firing works with the hand held away
      from the head.
- [ ] **Head aim kept where it should be.** Melee and stun baton aim with
      the head; `"AimFromHand": false` puts all aiming back on the head.
- [ ] **Motion tracker in the left hand.** Raise the tracker: blips turn
      with the left controller's heading; nothing breaks with the
      controller pointing straight up.
- [ ] **Controller lost.** Cover a controller: `right hand lost`, aim holds
      0.3 s then returns to the head, and blends back when tracking
      returns. With the dashboard open, aim follows the head.
- [ ] **Scenes.** Cutscene, ladder, locker, death: aim goes back to the
      head.
- [ ] **Turned on mid-session.** Start with hands off, turn Tracked Hands
      on in the menu during play: the sites install, aiming follows the
      right controller at once, and the log has no `the runtime gives no
      aim pose for the right hand`. On both runtimes.
- [ ] **Old binding without aim poses.** With a custom SteamVR binding for
      the game saved by an older version: `openvr input: hand aim pose
      actions unbound; ... (reset it to the default binding)` once; under
      OpenXR a profile without an aim pose logs `openxr input: right hand
      aim pose not bound by the current profile` once.
- [ ] **Smoothing.** `"Smoothing": 1`: steadier hands.
- [ ] **Threads.** No crash or stutter when the weapon update runs (it is
      assumed to run on the game thread).

## Haptics

Set `"Hands": {"Haptics": {"Enabled": true}}` before starting the game (the
vibration outputs are registered when the runtime starts). Hands need not
be on. Try on OpenVR and OpenXR, with Dev. Debug Options on.

- [ ] **Defaults.** With haptics off: no `haptics` lines, no `[hook]
      haptics` lines, no `haptic output` lines from the runtime.
- [ ] **Installed.** With haptics on: `haptics: on (...)`, `[hook]
      haptics, game rumble: in place`, `... damage and death: in place`,
      `... firing: in place`, `[hook] hands, motion tracker in hand: in
      place` (the tracker scan's call, shared). OpenVR: `haptic output
      actions haptic_left and haptic_right declared`, then once a
      controller is bound `haptic outputs bound: left true, right true`.
      OpenXR: `haptic output action created` and, per controller profile,
      `haptic output bound to N source(s)`.
- [ ] **Damage and death.** Take a hit: both controllers buzz briefly,
      harder for bigger hits (`[diag] haptics: damage pulse, amplitude
      ...`). Die: one long strong buzz (`death pulse`). Healing with a
      medikit and reloading a save give none.
- [ ] **Tracker.** Hold the tracker up: a faint tick in the left
      controller each scan (about twice a second); none with it lowered.
      With `"Input": {"ToggleGrips": true}`, ticks while toggled up.
- [ ] **Firing.** Revolver, shotgun, bolt gun: one buzz in the right
      controller per pull (`firing pulse, amplitude 0.60`); the
      flamethrower buzzes softly for as long as it fires. No buzz for melee,
      the stun baton or throwables, or while the game will not let the
      weapon fire.
- [ ] **Hacking and cutting.** Both controllers buzz when the hacking device
      or the cutting torch starts and again when it ends.
- [ ] **The game's rumble.** Wherever a pad would rumble (explosions, the
      Alien's attacks, heavy doors): the left controller follows the pad's
      low motor, the right the high motor. Log: `haptics: the game set its
      rumble for the first time (thread ...)` names the thread, and with
      Debug Options `[diag] haptics: game rumble set N times ...` every
      900 frames says how often the game sets it (21 §22 item 9: whether it
      rumbles at all without a pad connected). A level the game stops
      renewing ends after 0.5 s: no buzz stays on in the pause menu.
- [ ] **Real pad.** With an Xbox pad connected the pad still rumbles.
- [ ] **No focus, lost hand.** With the SteamVR dashboard or the Quest menu
      open nothing vibrates; a covered controller gets nothing.
- [ ] **Strength and keys.** `"Strength": 0.3` makes every buzz weaker,
      `0` silences them; each of `"GameRumble"`, `"Damage"`, `"Tracker"`,
      `"Firing"`, `"Hacking"` set to false stops its own buzz at once.
- [ ] **Frequency.** On OpenVR the vibrations ask for frequency 0 (the
      runtime's default): if a controller does not vibrate at all on
      OpenVR but does on OpenXR, report it with the controller type.
- [ ] **Legacy input.** If the log says `IVRInput unavailable, using legacy
      input`, the same events still give short pulses.

## VR off and logging

- [ ] **VR-off hotkey.** `"Controls": {"VrOffHotkey": true}`; hold right
      Ctrl + right Shift + F10 for 1 s: the headset stops showing the game,
      the game carries on flat on the monitor at the window's size, the
      options page shows `None` with the stored runtime's button. On
      OpenXR the log shows no 1 s timeout line; on SteamVR the headset
      leaves the game picture.
- [ ] **Crash-safe log.** `"CrashSafeLog": true` (applies at the next
      launch): `log: crash-safe logging on` at start, no stutter from
      logging; after a crash the `crash: mod state:` line is in the file.

## Gestures and hiding

Controls keys in `mothervr.json`, Dev. Debug Options on.

- [ ] **Recenter gesture.** `"RecenterGesture": true`. Both hands at the
      face, both grips held under 0.75 s: nothing; over 0.75 s: one
      recenter. Aiming with the tracker raised for minutes never fires it.
      Log: `[input] gesture: recenter (both hands at the head, grips held)`
      once, then the runtime's recenter line.
- [ ] **Holsters.** `"HolsterGestures": true`. Squeeze over the right
      shoulder: next weapon; with a hand against the chest: the tracker;
      low at the left hip: the flashlight. Squeezes with the hands held in
      front of the body as in play (about 0.3 to 0.4 m forward, 0.5 m
      down), resting in the lap or raised to aim: aim and raise the
      tracker as before, and log no `holster` line. At a keypad the
      shoulder squeeze does nothing. Log: `[input] gesture: holster right
      shoulder (right hand): next_main_weapon`, `[input] gesture: holster
      chest (left hand): select_motion_tracker` only for the chest
      squeeze; at the keypad `... suppressed: interaction`.
- [ ] **Lean while hiding.** `"HidingLean": true`. In a locker lean your
      head about 15 cm sideways, then back: the character peeks, then leans
      back. Log: `[input] gesture: hiding lean reference taken`, `[input]
      gesture: hiding lean (side ..., forward ...)`, `... released`.
- [ ] **Hold breath.** `"HoldBreathGesture": true`. At the breathing prompt,
      a hand over the mouth holds the breath; taking it away lets go. Log:
      `[input] gesture: hold breath (hand over the mouth)` and `... released`
      between the `Start_Breathing_Game` and `End_Breathing_Game` lines.
      The game inputs for lean and breath come from its tutorial texts
      ([likely]); this check confirms them.

## Controller glyphs

- [ ] **Prompts are never blank.** First start of this build: `MOTHER.PAK`
      rebuilds and every button prompt shows an image, with Controller
      Glyphs off and on. Blank prompts mean the glyph names' case does not
      match what the game looks up.
- [ ] **Per-controller glyphs.** `"ControllerGlyphs": true` with Index
      controllers: interact shows the A ring with a bar under its right
      half, map the A ring with a bar under its left half; View Controls
      lists `Interact | Right A`, `Map Menu | Left A`. Log: `[input]
      controller glyphs: left Index, right Index (SteamVR Input)`. Switch to
      Touch in play: prompts redraw without pressing anything (`left Touch,
      right Touch`). On OpenXR the line ends `(OpenXR)`. Steam Frame: the
      flashlight prompt shows the LB tab, the map prompt the View icon.
- [ ] **D-pad glyphs.** Controller Glyphs off, D-pad Emulation on: weapon
      cycling prompts show the grey L disc with a triangle at once.

## HUD

- [ ] **Defaults.** No `Hud` keys: the log has the `hud:` and
      `interaction:` start-up lines and no `[hook] hud ...` group lines;
      HUD and crosshair as in v1.
- [ ] **Crosshair off.** `"Hud": {"Crosshair": "Off"}`: raise the revolver,
      no crosshair, ammo count shown. Log: `[hook] hud crosshair: in place
      (3 sites)`, `hud: crosshair hidden (Crosshair Off)`. Back to On
      (`mvr_crosshair 0`): the crosshair returns at the next raise.
      `"Auto"` with hand aiming: no crosshair while aiming from the hand.
      Watch for a crash when the setting is changed in play: the sites are
      written while the game runs.
- [ ] **HUD hidden.** `"Hidden": true`, restart: no health, ammo or
      prompts, subtitles shown. Log: `[hook] hud visibility: in place (6
      sites)`, `hud: HUD hidden (Hud.Hidden)`.
- [ ] **HUD toggle gesture.** `"Controls": {"HudToggleGesture": true}`:
      left hand at the head, press Map: the HUD flips and the map stays
      shut (`hud: toggle gesture fired`). In a scene where the game hides
      the HUD the gesture does not show it; after the scene a HUD the
      player hid stays hidden.
- [ ] **Item highlight off.** `"ItemHighlight": false`: nearby items do not
      glow, prompts still work (`[hook] hud highlight: in place (2
      sites)`). Note whether the glow goes at once or after a reload.
- [ ] **Follow placement.** `"Placement": "Follow", "Pitch": -15`: the HUD
      sits below the view, level, still for small glances, follows turns
      over about 20 degrees smoothly, and jumps at once on a snap turn or
      recenter.
- [ ] **Unfinished keys.** `"Interaction": {"PhysicalLevers": true}` or
      `"CameraTakeovers": "Reduced"`: only a one-time "has no effect"
      line.

## Terminal text size

- [ ] **Each size.** Options > MotherVR > Interaction > Terminal Text Size
      at 125%, 150% and 175% in turn, restarting each time: the start logs
      `ui patch: variant text125 built in` (text150, text175) and `ui
      patch: built ... (556 entries)`. At a terminal with a long document
      (for example a personal log in Seegson Systems): the body text is
      larger at each step and wraps inside the same box; scrolling down
      reaches the last line, and does not run on into an empty box far past
      it (note how far past the last line it can scroll at 175%).
- [ ] **Titles and folders.** The entry titles (29, 34, 34 px) stay on
      their bars and are not cut at the right; the folder names (25, 26, 26
      px) stay inside their tiles in at most three lines. Note any title or
      name that is cut.
- [ ] **Back to 100%.** Restart: no `variant` line, and the terminal looks
      as in v1. Runtime None (`-vrforcenone`): no `variant` line whatever
      the setting.

## Body

- [ ] **Body visibility.** `"Body": {"Visible": ...}` each of
      `ArmsAndHands`, `HandsOnly`, `None`, in a normal level and in a space
      suit section: the hidden parts are gone in both eyes, in reflective
      floors and in the shadow; the held item stays in `None`; other
      characters are whole. Log: `[hook] body visibility: in place
      (SkinSetPose calls RVA 002A51D8, 002A536A)`, `[body] player skeleton
      FEMALEFP: N bones; ...`, `[body] visibility HandsOnly: K of N bones
      of FEMALEFP collapsed`; no `[body] WARNING a part of the player's
      model is posed from another skeleton's pose`. Back to `Full`:
      `[body] visibility Full: the whole body is drawn`. With no `Body`
      keys: no `body visibility` line, and the body shows as in v1.
- [ ] **Body visibility set in a menu, and from the file.** Set `Body`
      to `Arms and Hands` on the Body options page in the pause
      menu and again in the main menu, then load a level; also start the
      game with `"Body": {"Visible": "HandsOnly"}` and load a level. In
      the level the hidden parts are gone without touching the setting
      again. Log: if the skeleton cannot be read yet, one `[body] WARNING
      the player's skeleton NAME could not be read (REASON, N bones); the
      body is drawn whole until it can be, read again every 1 s` per
      change (not repeated every second), then `[body] visibility
      ArmsAndHands: K of N bones of FEMALEFP collapsed` once the player is
      in the level. If the warning stays and the visibility line never
      comes, send the log: REASON says what the skeleton lacks.
- [ ] **Bone names from the first-person table (21 §2.3, r10).** With
      `"Body": {"Visible": "HandsOnly"}`, load a normal level, then a space
      suit section. Log, once per skeleton: `[body] player skeleton
      FEMALEFP: 69 bones; LeftShoulder 3, RightShoulder 26, LeftArm 4,
      RightArm 27, LeftHand 9, RightHand 32, Spine 59; names from the
      first-person bone table (21 §2.3): bone count and parents match`, the
      same for `SPACESUITFP`, then `[body] visibility HandsOnly: K of 69
      bones of FEMALEFP collapsed`; no `a bone without a name` warning
      (the tester's log before the fix had `could not be read (a bone
      without a name: bone 0 of 69)`). Only the hands and the held item
      are drawn. A `[body] WARNING player skeleton FEMALEFP: rejected, N
      bones and the first bone that differs from the first-person bone
      table ...` instead means the skeleton is not the table's (the 2020
      build's data, or another mod's skeleton): send the log.
- [ ] **Real crouch.** Standing, calibrated, `"RealCrouch": true`: crouch
      for real, enter a vent, try to stand inside, leave and stand. The
      character follows, stays down in the vent, and the view stays at
      your real head (no double drop: the 0.63 m drop is right). Log:
      `[body] real crouch: crouch (head ...)` and `stand`; no `[body]
      WARNING real crouch: the game's requested crouch did not follow a
      press` (that warning would mean the game's crouch is hold-to-crouch).
- [ ] **Hand-back.** After a cutscene, ladder, hiding and a death, crouch
      for real during or just after: within a second the crouch matches.
      Log: `[body] control hand-back after Cutscene: crouch re-synced after
      0.5 s`.
- [ ] **Room-scale.** `"RoomScale": true`: nothing changes; log once
      `[body] room-scale: off in this build; ...`.

## 2D-only effects

- [ ] **Lens flares.** `"Graphics": {"LensFlares": false}`, stand near
      flared lights (Torrens, Solomons): no flares in either eye. Log:
      `[hook] lens flare toggle: in place on 1 sites`, `[render] 2D effects
      in VR: lens flares off, chromatic aberration game's, film grain
      game's`.
- [ ] **Chromatic aberration.** Video setting On, `"ChromaticAberration":
      false`: no colour fringes at the edges; with runtime None the fringes
      are back and the Video setting is still On.
- [ ] **Film grain.** Film Grain 1.0, `"FilmGrain": false`: no grain in the
      headset; runtime None shows it again.
- [ ] **Defaults.** All three true: three `... toggle: off; Graphics.X is
      true, the game's effect is kept in VR` lines and no `[render] 2D
      effects` line.

## Upright sprites

- [ ] **Smoke and steam stay upright (24 §14).** Near smoke or steam
      (Sevastopol vents, Torrens), tilt the head to each side: the sprites
      stay upright in the room instead of turning with the head; nodding
      and turning look as before. Look straight down into steam and up into
      smoke: no spinning or flicker. Close each eye in turn: both alike.
      Check a planar reflection with smoke in it. Runtime None: unchanged.
      Log: `proxy: CreateVertexShader hooked (upright sprites)` at start-up,
      and after a level loads `proxy: upright sprites: N of M vertex shaders
      changed so far; 0 refused by the device` with N > 0.

## Options pages

Options > MotherVR has seven new entries, Comfort, Controls, Hands,
Body, Interaction, HUD and Graphics, for every key of those sections that
has an item. The menus' `GameMenu` class is recompiled to add them, so
also watch for any menu that stops working.

- [ ] **Archive rebuilt.** First start of the new DLL: `ui patch: built
      ... (556 entries)`; the next start: `payload: ... is current`.
- [ ] **Entries.** Front end and pause menu: Options > MotherVR lists VR
      Runtime, Rendering, Input, Comfort, Controls, Hands, Body,
      Interaction, HUD, Graphics, Dev. Debug Options, Skip Startup Intro;
      each new page's title is its entry's label. The game-over menu still
      has no MotherVR entry.
- [ ] **Greyed without VR.** With `-vrforcenone` the seven new entries are
      greyed like Rendering and Input.
- [ ] **Interaction page.** Levers by Hand, Camera Takeovers (`Game`,
      `Reduced`), Terminal Distance and Rewire Distance (`Off`, `0.6 m` ...
      `2.0 m`) are greyed with `Not available yet`; Terminal Text Size
      shows `100%` to `175%` with `Requires restart! Larger text on
      terminal screens`, and changing it logs
      `[script_bridge] Interaction.TerminalTextScale changed to 1.50 by
      command`.
- [ ] **HUD page.** Crosshair (`On`, `Off`, `Auto`), Hide HUD, Item
      Highlight, HUD Placement (`Head-Locked`, `Follow`), and the sliders
      HUD Distance (`0.6` to `2`), HUD Width (`0.3` to `1.2`) and HUD
      Pitch (`-30` to `10`), each with its description line. Changing
      HUD Pitch logs `Hud.Pitch changed to ...` and moves the HUD when
      play resumes; Crosshair logs `hud: crosshair ...` as in the HUD
      checks.
- [ ] **Graphics page.** Engine Settings Override, 4096 Shadow Maps, Sharp
      Reflections, Texture Streaming Memory (`Game`, `300 MB` ... `1000
      MB`, `1024 MB`), Model Streaming Memory (`Game`, `128 MB` ... `512
      MB`), Lens Flares in VR, Chromatic Aberration in VR, Film Grain in
      VR, and no other item. The four items after the override are greyed
      while it is off. With `"Graphics": {"FilmGrain": false}` at start,
      turning Film Grain in VR on brings the grain back at once (`[render]
      2D effects in VR: ... film grain game's`) and off again removes it;
      with it true at start, turning it off logs `Graphics.FilmGrain
      changed to false by command` and the grain goes at the next launch.
- [ ] **Values shown.** With `"Comfort": {"SmoothTurnSpeed": 37,
      "Vignette": 0.34}` and `"Body": {"EyeForward": 0.123}` written by
      hand, the pages show `30°/s`, `30%` and `0.12`, and `mothervr.json`
      is not rewritten until a control is changed.
- [ ] **Changing a value.** Each control logs `[script_bridge] <key path>
      changed to <value> by command` (for example `Comfort.Vignette changed
      to 0.30`) and saves `mothervr.json`; reopening the menu shows the new
      value. Opening a menu logs no `by command` line (a pushed value never
      sends one back).
- [ ] **Sliders.** World Scale, Throw Strength, Hand Smoothing, Haptics
      Strength, Eye Forward and Seated Eye Raise show a plain decimal
      beside the bar (`0.55`, `-0.2`, `1`), one step per left or right.
- [ ] **Greying.** Hands: with Tracked Hands off, Aim from Hand, Motion
      Tracker in Hand, Melee by Swing, Throw by Motion, Throw Strength and
      Hand Smoothing are greyed; Throw Strength also with Throw by Motion
      off; the five Vibrate items and Haptics Strength with Haptics off.
      Flashlight Aim, Lean While Hiding and Hold Breath Gesture are always
      greyed with `Not available yet`. Body: Seated Eye Raise greyed in
      Standing; Real Crouch greyed unless Standing with a calibrated
      height. Turning a master toggle greys or frees its items at once.
- [ ] **Calibrate Standing Height.** Without a height its description is
      `Stand up straight, select, and keep still`. Select it: `Measuring in
      3`, `2`, `1`, then `Waiting for head tracking`; the log has `standing
      height calibration requested by command` and the calibration's own
      lines. Reopen the menu: `Standing height: 1.72 m. Select to measure
      again` (your height). With the headset off it stays `Waiting for head
      tracking`; on a runtime without a floor: `Not measured: no floor
      height on this runtime`.
- [ ] **-vrscale.** With `-vrscale 1.2`: World Scale greyed at `1.2`, its
      description `Set by -vrscale on the command line for this session`.
- [ ] **Texts fit.** The longest description lines (D-Pad Emulation,
      No Turning During Interactions, Engine Settings Override, Texture
      Streaming Memory) fit the description area.
- [ ] **Stock menus intact.** Options > Game, Video (greyed in VR as
      before), Audio, Load Game, the controls help (Input > View Controls)
      and, with Dev. Debug Options, Select Level and Trigger Debug
      Checkpoint behave as before; `Use OpenXR` still reads so.

## Builds and runtimes

- [ ] Every item above on both OpenVR and OpenXR where it applies.
- [ ] The December 2020 game build (Epic, GOG, newer Steam installs):
      starts, VR works, and the features without 2020 addresses log that
      they stay off.
