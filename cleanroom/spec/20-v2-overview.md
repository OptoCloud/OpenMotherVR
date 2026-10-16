<!-- Provenance: project goals (roadmap, v2 section); user requirements (v1 = the earlier mod's behaviour with its problems fixed, v2 = enhanced mode on top of it; every v2 feature off or v1-compatible by default); proposals of this project, pending user confirmation where marked [proposal]. This document states no new game facts: game facts used here are cited from the v1 documents 00 to 14, and those for v2 are in 21 to 24, where game facts were located with the help of an analysis of another mod and verified in the 2015 `AI.exe` by the spec side; requirements are proposals of this project, pending user confirmation where marked. 2020 values: none (v2 gives 2015 RVAs only). r2: project decisions of the second clean-room review of 20 to 24 (the master switch covers 21 to 24, removed in r4, one owner for seated and standing play, the engine settings override and a failed runtime, where v2 addresses are listed, this project's own feature names). r3 (CLEANROOM r3, what a player sees may be stated exactly): where the v2 text a player sees is stated (22 §12.1 for the page rules, each document for its items); v2 adds no message box; the options page space question answered by an `Enhanced Mode` entry and its pages (replaced in r4: one entry per settings section on the `MotherVR` page). r4 (user decision of 2026-10-16): no master switch: `EnhancedMode` and its gating are removed; each v2 key applies on its own and defaults to its v1 value, so the v1 rule alone gives an upgrade v1 and lets a v2 problem be told from a v1 one (§1.3, §9); the v2 options pages are reached from the `MotherVR` page directly, one entry per settings section (§3.2, §10 item 6, 22 §12.1). Revision: r4 -->

# 20. v2 overview: enhanced mode

## 1. Goal

v1 (00 to 14) plays the whole game in a current PC VR headset as 00 to 14
specify. It drives the game
through its gamepad: the VR controllers are buttons and sticks, the head
is tracked, and the hands are read (04 §5.11) but not used.

**v2 is an enhanced mode on top of v1.** It makes the game feel built for
VR: the hands are tracked in the game and aim, hold the motion tracker and
the flashlight; the body follows the player's real head and height; turning
and moving offer comfort choices; interactions such as levers and valves
can be done by hand; the HUD and rendering are adapted to a headset. v2
adds no gameplay of its own: the game's rules, AI, scripting and saves
stay the game's.

### 1.1 The v1 rule

These two requirements bind every v2 document:

1. **Off or v1-compatible by default.** Every v2 feature is turned on by
   its own `mothervr.json` key, and its default is off, or a value that
   gives exactly v1's behaviour, unless a user decision recorded in that
   document says otherwise, with its reason.
2. **All v2 options off is exactly v1.** With every v2 key at its v1 value
   (or absent), the mod behaves as v1 specifies, observably and in what it
   writes to the game: no extra game patch is applied, no extra runtime
   action changes what the controls do, no extra per-frame cost is spent
   beyond a check that a feature is off. A v2 feature that needs a patch
   installs it only while it is on, or installs it in a form that is
   provably inert while off (the document states which, and why). v2 may
   read state v1 already keeps (hand poses, 04 §5.11; the interaction
   state, 11 §5.3).

Consequences:

- A player who upgrades and changes nothing gets v1. [proposal: the safe
  upgrade path; users who want v2 turn it on.]
- A bug report can always be checked against v1 by turning the v2 options
  off; the log records which v2 options are on (11 owns wording).
- A v2 feature whose game sites fail their check (12 §4) stays off on its
  own and the rest of the mod, v1 included, runs (12 §4.2: v2 features are
  optional fixes in that sense, never core patches).

Defaults that may break rule 1 only by user decision (§9): OpenXR as the
default runtime (22 §11), and whether tracked hands are on for a fresh
config. Either decision is also an exception to rule 2: once made, a fresh
config with every v2 key absent no longer gives v1's runtime or v1's
controls, and the document that owns the default says so.

### 1.2 Runtime None and VR failure

With runtime None the game is the unmodded game (01 §2.3); no v2 feature
runs and no v2 patch is applied. When the runtime fails to start, the game
runs flat as v1 says (12 §7) and every v2 feature is inactive. v2 features
exist only while the stereo device runs, with one exception: the engine
settings override of 24 §6, when it is on, is decided at attach and still
applies after a runtime failure, because it changes the game's own option
list before the runtime's outcome is known and concerns flat play as much
as stereo [proposal: undoing it after the parse is not possible, and a
player who turned it on wants it in flat play too].

### 1.3 No master switch [user decision of 2026-10-16]

There is no single switch for v2. Each v2 key of 21, 22, 23 and 24 applies
on its own, as stored, and its default is its v1 value (§1.1); no key
gates another document's keys, and the options page has no entry that
turns v2 on or off as a whole. Reason: the per-key v1 defaults already
give what a master switch would: a player who upgrades and changes nothing
gets v1 (§1.1), and a v2 problem is told from a v1 one by setting the v2
keys back to their v1 values, which the log's settings event lists (22
§16). A key that applies only at the next launch (for example 24 §6)
follows its own value at that launch.

## 2. Documents

| Document | Covers |
|---|---|
| `20-v2-overview.md` | this document: goals, the v1 rule, document set, what the player sees, modes, platform, phasing, glossary, cross-cutting questions |
| `21-hands-and-body.md` | tracked hands in the game; aiming with the hand; the motion tracker, flashlight and melee by hand; hiding the player's body where it conflicts with the real one; the eye forward or back of the body; room-scale movement; the seated eye raise; real crouch driving the game's crouch; control hand-back after the game's animations; throwing by motion; health on a wrist display; haptics |
| `22-controls-and-comfort.md` | smooth turning; snap-turn repeat; a comfort vignette; seated and standing modes and their calibration (the one owner of both); world scale as a setting; d-pad emulation; gestures (body-relative holsters); turning that respects interactions; holding breath and leaning by head movement; glyphs per controller type; OpenXR as the default runtime; settings in the headset; a hotkey that turns VR off; crash-safe logging |
| `23-interactions-and-hud.md` | physical levers, valves, the rewiring tool and terminals; fewer camera takeovers during interactions; activation distances for hands; the terminal font in a headset; HUD placement and crosshair options; highlighting items that can be picked up |
| `24-rendering-v2.md` | temporal anti-aliasing and a real SMAA T2x with per-eye history; smoke and fog lighting in stereo; the pause menu over the live world; the game's engine settings; foveated rendering and a resolution scale; 2D-only effects (lens flare, chromatic aberration, film grain, cutscene letterboxing); the cost of D3D11 state changes |

21 to 24 hold the details: game facts with 2015 RVAs, settings with their
keys and defaults, edge cases, threads, acceptance checks and open
questions. This document does not repeat them and does not define any key.
Where this document and one of them disagree, the detailed document wins
and this one is corrected.

Reading order for v2: 00 to 14 as for v1 (v2 builds on all of them), then
20, then 21 to 24 as needed.

## 3. What the player sees in v2

By area. Each item is a goal; the document named holds what is required.

### 3.1 Hands and body (21)

- The player's hands appear where the controllers are. The weapon in hand
  aims where the hand points, not where the head looks.
- The motion tracker is held in one hand and read by looking at it; the
  flashlight shines where the hand (or the head, by choice) points. Melee
  swings follow the hand.
- The view follows the real head: leaning, ducking and stepping move the
  head without the game's body snapping after it, within limits that keep
  the game's collision honest (free head, room-scale).
- In standing mode the player's real height sets the eye height, and
  crouching for real crouches in the game. In seated mode the eye height is
  that of the standing character.
- After the game plays an animation that moves the body (a ladder, a vent,
  a hiding place), body and head are back in agreement when control
  returns.
- Thrown items fly with the hand's motion.
- Health is shown on a wrist display.
- Haptics: the controllers vibrate for the game's own feedback (for
  example the Alien close by, tracker pings, hacking).

### 3.2 Controls and comfort (22)

- Smooth turning at a chosen speed, as an alternative to snap turn; snap
  turn gains a choice of repeat while the stick is held.
- A comfort vignette narrows the view while moving with the stick or
  turning.
- Seated and standing modes, with a height calibration.
- World scale as a setting (v1 has it only as `-vrscale`, 06 §2).
- Body-relative gestures (reach over the shoulder, to the hip, to the
  chest) select weapons, the tracker and the flashlight; a d-pad is
  emulated for controllers without one.
- Turning is suppressed while an interaction needs the stick.
- Holding breath and leaning while hiding follow the real head.
- Button prompts show the glyphs of the controller actually in use, and
  change when it changes.
- OpenXR may become the default runtime (pending user decision, §9).
- The new settings can be changed in the headset: the `MotherVR` options
  page gains one entry per area whose page holds that area's v2
  settings (22 §12).
- A hotkey turns VR off without quitting; log lines also survive a crash
  of the whole system, and the crash record gains the mod's state (22
  §14).

### 3.3 Interactions and HUD (23)

- Levers, valves, the rewiring tool and terminals can be worked by hand.
- Fewer interactions take the camera away from the player's head.
- Interactions start at hand-reach distances suited to VR.
- Terminal text is readable in the headset.
- HUD placement and a crosshair are options; items that can be picked up
  can be highlighted.

### 3.4 Rendering (24)

- Temporal anti-aliasing with per-eye history, so SMAA T2x is real rather
  than running as T1x (00 §4.1 item 10).
- Smoke and fog are lit consistently in both eyes.
- The pause menu shows the world behind it instead of black.
- Selected engine settings, foveated rendering and a resolution scale.
- 2D-only effects (lens flare, chromatic aberration, film grain, cutscene
  letterboxing) are fixed for stereo or can be turned off.
- Less render-thread cost from redundant D3D11 state changes.

### 3.5 What stays exactly as v1

Unless a v2 option the player turned on changes it:

- loading, build identification, the preflight and patch sets (01), and
  runtime None as the unmodded game;
- the stereo path, frame timing, the virtual screen for menus, videos and
  loading screens, the mirror and the swapchain rules (02, 03, 09);
- recentering (03 §2, 04 §8), the near-the-head test (04 §5.10), invalid
  pose handling;
- the v1 render fixes (07, 08), audio (10);
- the log format and its required events (11), threads and robustness
  rules (12), the files the mod writes and the messages to the player (13;
  the welcome's text changes only once OpenXR is the default, 22 §11);
- every v1 `mothervr.json` key, its meaning and default (06 §1).

Menus, videos and loading screens stay on the virtual screen in every v2
mode [proposal: no v2 goal concerns them; the pause menu change of 24 is
the only exception and is an option].

## 4. Modes

Two independent choices define how v2 plays. Each is a setting of the
document named.

| | Hands off | Hands on |
|---|---|---|
| **Seated** | v1 controls, plus the comfort options the player chose; eye height of the standing character | hands aim and hold; gestures within seated reach; crouch by button |
| **Standing** | v1 controls; eye height from the calibrated real height; real crouch drives the game's crouch | everything of 21: hands, real crouch, room-scale |

- **Seated or standing** (22 §6, which owns the mode key and the height
  calibration; 21 §11 reads them for real crouch and room-scale): sets
  where eye height comes from and whether real crouch is used. A fresh
  config is seated
  [proposal: matches v1, where the head offset is the runtime's own and no
  calibration is needed].
- **Hands on or off** (21): with hands off, the controllers drive the game
  through its gamepad exactly as in v1 (04), including aim on the grip.
  With hands on, aiming, the tracker and the flashlight follow the hands
  and the inputs those take over are freed (21 states which, and the v1
  sections it changes, for example 04 §5.6).
- Comfort, HUD and rendering options apply in every mode.

When the hands are lost (controllers off, out of tracking, the runtime's
dashboard open) hand features fall back to the head and the v1 bindings
until tracking returns [proposal: the game stays playable without hands;
21 states the details].

## 5. Common requirements for every v2 feature

These restate v1 rules as they apply to v2; 21 to 24 apply them per
feature.

- **Settings** are new keys in new sections inside `Settings` of
  `mothervr.json`, one section per area, named by the document that owns
  it (`Hands` and `Body` in 21, `Comfort` and `Controls` in 22,
  `Interaction` and `Hud` in 23, `Graphics` in 24); there is no master
  key (§1.3). Each key has a type, default, range and sanitise rule as in 06
  §1. A setting has exactly one key, in the document that owns it; other
  documents refer to that key. Unknown keys are kept on save (06 §3); v1
  keys keep their layout.
- **Game addresses** are 2015 RVAs, listed in the address table of the
  document that uses them (21 §20, 22 §2, 23 §2, 24 §1.1 and §2.3); the
  2020 column is `?` (§6). 01 §4 collects them in a later revision (§11).
  Every site is verified before it is written (12 §4);
  a v2 feature is an optional fix: a mismatch turns that feature off alone
  and is logged.
- **Threads** follow 12: hand and head poses reach the game thread through
  the consistent hand-over of 04 §5.11 and 03 §11; nothing is torn; no
  allocation, file I/O, locking or logging per frame on the render thread.
- **Edge cases** each document covers: cutscenes, menus and the virtual
  screen, loading, death and reload, interactions (11 §5.3), hiding,
  headset off, controllers lost, runtime None, OpenVR and OpenXR.
- **Logging** is in 11's format; each v2 document says what each event
  must convey and 11 (in a later revision) owns the wording.
- **Messages to the player** are owned by 13. v2 adds no message box
  [proposal: a v2 result is shown where the player asked for it, on the
  options page, for example the height calibration, 22 §12.2].
- **Text a player sees** (options page items, glyph designs, controls
  help rows) is stated exactly (CLEANROOM r3) by the document that owns
  the feature, as this project's text in the style of the v1 page players
  know (00 §2.6); 22 §12.1 gives the rules every v2 page follows.
- **Both runtimes**: a v2 feature works on OpenVR and on OpenXR, or the
  document says why it cannot and what the player sees on the other.
- **Input**: a v2 input feature names the semantic inputs (04 §5.8.1) it
  reads and the game actions it drives, and states how it shares an input
  with a v1 binding (only one owner per input at a time).
- **Acceptance** is checked in game on the 2015 build, with the headset
  worn where the feature is about the headset.

## 6. Platform

| Item | v2 |
|---|---|
| OS, graphics API, entry, dependencies | as v1 (00 §3) |
| Game | the 2015 `AI.exe` (PE timestamp 0x54E3199D) only. On the 2020 build (0x5FD25676) every v2 feature stays off and v1 runs as 00 §3 says |
| VR runtime | OpenVR and OpenXR as in v1; OpenXR becomes the default only by user decision (§9) |
| Controllers | as v1 (04 §5.8), plus haptic outputs and any actions 21 and 22 add, with default bindings per controller type |

### 6.1 Supporting the 2020 build

Supporting the 2020 build is itself a v2 goal. It takes:

1. A 2020 `AI.exe` available to the spec side.
2. Verifying every v1 address and byte string against it. Today no 2020
   value in v1 has been verified against a 2020 `AI.exe` (01 §3.2 gives
   their source). Each one is confirmed or corrected, and each `?` is found
   or the feature it serves stays off there.
3. Finding every v2 address in it (21 to 24 give 2015 RVAs only), with the
   same verification as for 2015.
4. Checking the game data the mod depends on: the UI archive and its movies
   (06 §7, 14), the shaders the v1 and v2 render fixes patch (08, 24), and
   the game's animation and level data that v2 reads.
5. Running the acceptance checks of 00 to 14 and of 21 to 24 in game on the
   2020 build.

Until a feature has passed all of that, it stays off on 2020 (01 §3.2:
every feature whose 2020 address is unknown stays off).

## 7. Priorities and phasing [proposal]

Ordered by what players ask for most, by dependency, and by risk. Each
phase leaves the mod releasable with all v2 options off equal to v1.

| Phase | Contents | Why here |
|---|---|---|
| A. Foundations | v2 settings sections and their place on the options page (22); input ownership between v1 bindings and v2 features (§5); the VR-off hotkey and crash-safe logging (22) | everything later needs the settings and the input rules; the hotkey and the logging make every later test safer and its failures readable |
| B. Hands and turning | tracked hands and hand aiming, the tracker and flashlight by hand (21); smooth turning and snap repeat, comfort vignette (22) | the most requested features; hand aiming builds on hand poses v1 already reads (04 §5.11); smooth turning is small and independent |
| C. Body and modes | seated and standing modes with calibration (22), real crouch, free head, room-scale, control hand-back after animations (21), world scale (22); haptics (21) | depends on B's hands and on a settled seated/standing model; body work touches the game's camera and collision and is the riskiest gameplay change |
| D. Controls polish | gestures and d-pad emulation, interaction-aware turning, hold breath and lean, per-controller glyphs, in-headset settings for everything so far, OpenXR as default (22) | needs B and C to know which inputs are free; glyphs and the options page change the UI archive once for all |
| E. Interactions and HUD | physical levers, valves, rewiring, terminals; fewer camera takeovers; activation distances; terminal font; HUD, crosshair, item highlight; melee and throwing by hand (21, 23) | each interaction is its own game study; they need hands (B) and the interaction state (11 §5.3) |
| F. Rendering | per-eye temporal AA and SMAA T2x, smoke and fog, pause menu over the world, engine settings, foveated rendering and resolution scale, 2D-only effects, D3D11 state cost (24) | v1 already renders correctly; these improve quality and cost and are independent of gameplay, so they can also run in parallel by a separate implementer |
| G. 2020 build | §6.1 | needs a 2020 binary; can start whenever one is available and repeats for each feature added after |

A feature may move earlier when it is cheap and independent (for example
2D-only effect toggles), provided its document is complete.

## 8. Glossary

v1 terms are in 00 §5. v2 adds:

- **v2 feature**: a behaviour specified in 21 to 24, turned on by its own
  `mothervr.json` key.
- **v1 value**: the value of a v2 key that gives exactly v1 behaviour; the
  default unless a user decision says otherwise.
- **Hands on / hands off**: whether the tracked hands drive aiming and held
  items (21), or the controllers act only as a gamepad (v1).
- **Seated / standing mode**: where the eye height comes from (the
  standing character's height, or the player's calibrated real height) and
  whether real crouch is used (22).
- **Height calibration**: taking the player's standing eye height as the
  reference for the game character's eye height (22).
- **Real crouch**: lowering the real head below a threshold under the
  calibrated height makes the game character crouch (21).
- **Hand aiming**: the weapon's aim follows the tracked hand, not the head.
- **Free head**: the view follows the real head's position within limits
  while the character's body stays where the game put it (21 §11).
- **Eye forward offset**: the eye moved forward or back of the game body
  by a set distance (21 §11.2).
- **Room-scale**: the player's real walking in the play space moves the
  game character, through the game's own movement and collision (21).
- **Control hand-back**: when the game returns control of the character
  after one of its own animations moved it, the view, hands and crouch
  agree with the player again (21 §12).
- **Body hiding**: not drawing parts of the game character's body that
  would conflict with the player's real body (21).
- **Smooth turning**: continuous yaw from the turn stick at a set speed, as
  an alternative to snap turn (04 §6) (22).
- **Snap repeat**: the interval at which a held stick repeats a snap step
  (fixed at 300 ms in v1, 04 §6).
- **Comfort vignette**: darkening the edge of the view during artificial
  movement or turning (22).
- **World scale**: the metres scale `s` of 03; a setting in v2, the
  `-vrscale` switch in v1.
- **Gesture** / **holster**: a body-relative zone (shoulder, hip, chest)
  where a hand action selects an item (22).
- **D-pad emulation**: deriving the game's d-pad inputs from another
  control on controllers without a d-pad (22).
- **Interaction-aware turning**: turning suppressed or changed while an
  interaction (11 §5.3) uses the stick (22).
- **Physical interaction**: working a lever, valve, rewiring panel or
  terminal by moving the hand rather than pressing a button (23).
- **Camera takeover**: an interaction during which the game moves the
  camera away from the player's control (23).
- **Activation distance**: how close the hand or head must be for an
  interaction to start (23).
- **Item highlight**: marking items that can be picked up (23).
- **Per-eye history**: the previous frame's image and data kept separately
  for each eye, which temporal anti-aliasing needs (24).
- **Foveated rendering**: rendering the edge of each eye's view at a lower
  shading rate (24).
- **Resolution scale**: a v2 scale on the rendered size; 24 states how it
  relates to the v1 pixel scale (03 §10).
- **2D-only effect**: a screen effect designed for a flat screen (lens
  flare, chromatic aberration, film grain, letterboxing) (24).
- **Haptics**: vibration of a VR controller driven by game events (21).
- **In-headset settings**: v2 settings on their own pages of the
  options page, one per settings section, reachable in the headset (22 §12, 14).
- **VR-off hotkey**: a key that turns the stereo device off for the rest of
  the session without quitting (22).
- **Crash-safe logging**: log lines flushed to disk so that they also
  survive a crash of the whole system, and the crash record extended with
  the mod's state (22 §14; v1 already records a crash of the process,
  11 §6).

## 9. Decisions the user must make

| Decision | Options | Proposal |
|---|---|---|
| Default runtime for a fresh config | OpenVR (v1 decision r4) or OpenXR (roadmap) | OpenXR once phase D has verified v2 on both runtimes; existing config files keep their value |
| Hands on for a fresh config | off (v1 rule) or on | off: the v1 rule; the options page makes it one switch |
| A single switch for all of v2 | none (each feature its own key) or one master key that forces every v2 key to its v1 value while off | decided (user decision of 2026-10-16): none; each v2 key applies on its own and defaults to its v1 value (§1.3) |
| Seated or standing for a fresh config | seated or standing | seated (no calibration needed) |
| Phasing | §7 | as §7 |
| 2020 build in v2 | off until verified, or best effort | off until §6.1 is done for each feature |

## 10. Open questions across 21 to 24

1. **Input ownership.** Hands on, gestures, d-pad emulation,
   interaction-aware turning, physical interactions and smooth turning all
   take inputs v1 maps to the gamepad (04 §5.8.5). One table must say, per
   mode, which feature owns each semantic input. Where does it live (21 or
   22) and does 04 §5.8 need new semantic inputs?
2. **Action manifest changes.** Haptics and new actions change the SteamVR
   Input manifest and OpenXR action set (04 §5.8.2, §5.8.4). A player's
   saved custom binding lacks them. Required: detect that and say so, as
   04 §5.11 does for pose actions; which document owns it?
3. **Camera ownership.** Free head, room-scale, real crouch, control
   hand-back, lean and fewer camera takeovers all change how the game's
   first-person camera and the stereo view are combined (03 §1, 05 §3, 05
   §6). One model is needed so they do not fight; 21 proposes it, 22 and
   23 use it.
4. **World scale and height.** World scale (22), seated and standing
   height (22), the seated eye raise (21) and real crouch all scale or
   offset the head position. Answered: positions are taken relative to the
   reference in runtime metres, the offsets are summed there and the sum
   is scaled by `s`; the standing height and the crouch threshold are in
   runtime metres, before `s` (22 §6.3, 21 §11.1).
5. **Interaction state.** 11 §5.3 keeps whether an interaction is running
   and its kind. Is it enough for 21 to 23 (hand features off during an
   interaction, interaction-aware turning, physical interactions), or do
   they need the interaction's object and progress?
6. **Options page space.** Answered [proposal]: the `MotherVR` page gains
   one sub-page entry per settings section (22 §12.1); new pages and items
   are created after every stock one (06 §7.3).
7. **Per-frame budget.** v2 adds render-thread and job-thread work (hands,
   per-eye history, foveation). What total budget over v1 is acceptable,
   measured as in 00 §4.3?
8. **Death, reload and checkpoints.** Which v2 state (calibration, held
   item, toggles) survives a death and reload; none is written to the
   game's saves.
9. **Cutscenes.** Hands and body features during cutscenes: hidden, frozen,
   or shown? The v1 rule (cutscenes in stereo, never blacked out) holds.
10. **Logging.** Each of 21 to 24 lists its events; 11 needs a revision
    that adds them, with the v2 options on in the session start lines.
11. **v1 documents.** The changes each of 21 to 24 lists in its "Changes to
    v1 documents" section must be collected into one v1 revision so that
    v1 documents state where v2 changes them.

## 11. Changes to v1 documents

Not made here; to be applied in a later revision:

- `00-overview.md` §1.1: point to this document for v2.
- `README.md`: add 20 to 24 to the index and the v2 reading order (§2).
- `06-options-and-config.md` §1: note that v2 sections exist and that
  `Input.VRInputRelativeWalking`, `Input.VRInputStrafing` and
  `Input.ToggleCrouch` (stored only in v1) may gain effect in v2 if 21 or
  22 says so.
- `01-loading.md` §4: the v2 addresses of 21 §20, 22 §2, 23 §2 and 24
  §1.1 and §2.3, 2015 only, as optional-fix sites.
- `11-diagnostics.md`: v2 events and the v2 options in the session start
  (§10 item 10).
- `06-options-and-config.md` §5 and `00-overview.md` §2.6: the v2
  sub-page entries after `Input` and their pages (22 §12.1).
- `ANSWERS.md`: record the user decisions of §9 once made, and the
  decisions of this revision (§1.2 exception, §1.3 no master switch, one owner
  for seated and standing play, where v2 addresses are listed).
