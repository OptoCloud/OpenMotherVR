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

Set `"Hands": {"Enabled": true}` before starting the game (the aim poses
are registered when the runtime starts). Try on OpenVR and OpenXR.

- [ ] **Defaults.** With hands off: no `[hook] hands` lines; on OpenVR the
      log reads `+ 2 hand poses`.
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
- [ ] **Turned on mid-session.** The sites install and `no aim pose` is
      logged (the aim poses need a restart).
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

## Options pages

Options > MotherVR has four new entries, Comfort, Controls, Hands and
Body, for every key of those sections. The menus' `GameMenu` class is
recompiled to add them, so also watch for any menu that stops working.

- [ ] **Archive rebuilt.** First start of the new DLL: `ui patch: built
      ... (522 entries)`; the next start: `payload: ... is current`.
- [ ] **Entries.** Front end and pause menu: Options > MotherVR lists VR
      Runtime, Rendering, Input, Comfort, Controls, Hands, Body, Dev. Debug
      Options, Skip Startup Intro; each new page's title is its entry's
      label. The game-over menu still has no MotherVR entry.
- [ ] **Greyed without VR.** With `-vrforcenone` the four new entries are
      greyed like Rendering and Input.
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
      No Turning During Interactions) fit the description area.
- [ ] **Stock menus intact.** Options > Game, Video (greyed in VR as
      before), Audio, Load Game, the controls help (Input > View Controls)
      and, with Dev. Debug Options, Select Level and Trigger Debug
      Checkpoint behave as before; `Use OpenXR` still reads so.

## Builds and runtimes

- [ ] Every item above on both OpenVR and OpenXR where it applies.
- [ ] The December 2020 game build (Epic, GOG, newer Steam installs):
      starts, VR works, and the features without 2020 addresses log that
      they stay off.
