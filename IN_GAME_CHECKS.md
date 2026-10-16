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

## Builds and runtimes

- [ ] Every item above on both OpenVR and OpenXR where it applies.
- [ ] The December 2020 game build (Epic, GOG, newer Steam installs):
      starts, VR works, and the features without 2020 addresses log that
      they stay off.
