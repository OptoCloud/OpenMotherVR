<!-- Provenance: game analysis of AI.exe 2015 (PE timestamp 0x54E3199D) by the spec side: the input action name table and its indices, the game's UI object lookup and the HUD movies' hide and show methods, the hiding place's breathing-game messages passed to the script event tap, the peek action and the CameraPeek tuning names; the game's own data files (`DATA\INPUT.XML` default bindings, `DATA\UI.PAK` movies that define the HUD hide functions); game facts of the v1 documents 02 to 14, cited by section; game facts located with the help of an analysis of another mod and verified in the 2015 `AI.exe` by the spec side; requirements are proposals of this project, pending user confirmation where marked [proposal]; user requirements of the v2 roadmap (smooth turning, comfort options, seated and standing play, gestures, per-controller glyphs, OpenXR as the default runtime, a way to turn VR off, crash-safe logging); public OpenVR, OpenXR, D3D11 and Win32 documentation. 2020 values: none, every 2020 address is `?` and every feature here stays off on the 2020 build; r2: v1's crash record (11 r3 §6.1) taken as the base of the crash-safe log's crash event (project decision of spec r6); r3: second clean-room review: the crash event stated as the state that must hold, at the level of v1's crash record; the gestures and d-pad emulation stated as results with this project's reasons; the HUD game facts of 2.4 re-read by the spec side in the 2015 `AI.exe` and reconciled with 23 (the pending flag of each HUD object, the objects the game's own hide and show touch); this document is the one owner of seated and standing play and the calibration (21 reads them); `EnhancedMode` gates 21 to 24 (20 §1.3; removed in r6); lever interactions own the grips, the move action and turning (23 §3.2). r4 (spec r7 cross-check): the game's event tap function named `ScriptEventTap` and the tracked state the logging of game script events, as 11 §5 now does; the flush thread's exit rule follows 12 §7 (at process exit only the detach line is logged); the change listed for 11 states the crash event's level and timing as §14 does. r5 (CLEANROOM r3, what a player sees may be stated exactly): the v2 options pages given exactly (the `Enhanced Mode` entry and its place on the `MotherVR` page, replaced in r6 by one entry per page, one sub-page per settings section, item order, labels, control kinds, value names, display formats, description lines, greying and hiding rules, the calibration's texts), the glyph designs per controller family in v1's look with a hand mark, the controls help rows per family and for v2 features, and the welcome text once OpenXR is the default, each a project text in the style of the v1 page players know, with its reason; the game's own Camera Speed label and description and its longest stock menu description read by the spec side from `DATA\TEXT\ENGLISH\UI.TXT` and the stock menu movies of the player's `UI.PAK`; the VR-off hotkey's page state stated as v1's (no new text). r6 (user decision of 2026-10-16): no master switch: the `EnhancedMode` key, its toggle, the `Enhanced Mode` entry and page and the greying that depended on it are removed; each key applies on its own with its v1 value as default (20 §1.3); the `MotherVR` page gains the seven v2 sub-page entries directly, after `Input` and before `Dev. Debug Options` (12.1). r7: Valve's public Steam Frame input documentation (a controller with a physical d-pad); a tester's report on a Steam Frame (its d-pad toggled the flashlight instead of moving through menus); project decisions with their reasons inline: a controller's own d-pad drives the d-pad actions whatever `Controls.DpadEmulation` is (04 §5.8.1); the d-pad modifier is Sprint's control rather than an input of its own. r8: the breathing game's tutorial texts and the head-tracking option text read by the spec side from `DATA\TEXT\ENGLISH\UI.TXT` (the inputs that lean and hold breath in a hiding place, 2.3), so 9.2 and 9.3 are no longer kept off; project decisions with their reasons inline: the axes the head's lean drives (9.2), a holster press while holsters are off, an interaction beginning during a holster hold, the recenter gesture not counting a grip a holster holds (8.2, 8.4); acceptance items 18 and 19. r9: the Steam Frame as a glyph family (10.1 to 10.3: its identification, its prompts' designs and the bumper and View designs, its controls help rows, from 04 r9's Frame controls); project decisions with their reasons inline: the per-hand rule for two families that put a function on different hands and for a hand not identified yet, the d-pad glyphs of 7 with `Controls.ControllerGlyphs` off, a controller's own d-pad keeping the game's d-pad glyphs, the family change logged; the hiding rows of 10.3 no longer wait for 9.2 and 9.3 (r8). r10 (spec-side game analysis, static, 23 r6): 8.3 tells the game's own HUD hide from the mod's by wrapping the scripted HUD entity's methods. r11 (a tester's log, OpenXR with Index controllers): the chest and left-hip holster zones of 8.4 moved off the hands' ordinary hold and rest positions (project decision, reason in 8.4); acceptance item 11 names those positions. r12 (a comparison of this document's controls and comfort with the behaviour of another VR mod for the game, whose game facts were verified in the 2015 `AI.exe` by the spec side): no requirement changes; the alternatives considered and the reasons this document keeps its own choices (new 8.5); the look stick's Y during interactions now in 04 r10 §5.8.5, which the lever mapping of 23 §3.2 and the d-pad emulation of 7 leave as they are (section 20). Revision: r12 -->

# 22. Controls and comfort (v2)

The v2 controls and comfort features: smooth turning and the snap-turn
options, a comfort vignette, seated and standing play with a height
calibration, world scale as a setting, d-pad emulation, gestures, turning
that respects interactions, leaning and holding breath while hiding,
button glyphs per controller type, OpenXR as the default runtime, the v2
part of the options page, a hotkey that turns VR off, and crash-safe
logging.

This document builds on 03, 04, 05, 06, 09, 11, 12 and 14 and follows
`20-v2-overview.md` §1.1: every feature has its own `mothervr.json` key,
its default gives exactly v1 behaviour, and with every key here at its v1
value the mod behaves as v1 specifies.

Text a player sees is stated exactly (CLEANROOM r3): the v2 part of the
options page with its labels, values and description lines (section 12),
the glyph designs (10.2), the controls help rows v2 adds (10.3) and the
welcome text once OpenXR is the default (section 11). These
are this project's texts, written in the style of the v1 page the player
already knows (06 §5, 00 §2.6): title-case labels, the game's own item
kinds, `Off` / `On` and `Low` / `High` as the game names such values, and
`Requires restart!` at the start of a description where a change applies
at the next launch, as v1's runtime buttons say it. A label or description
in a code span below is the exact text; code spans of keys and game names
are not shown to the player.

Addresses are RVAs (image base 0x400000) of the 2015 `AI.exe` (PE
timestamp 0x54E3199D). The 2020 column is `?` throughout; every feature of
this document that needs a game address stays off on the 2020 build (20
§6). Labels for game functions and sites are spec labels (01); the
implementation may rename them. Every site is verified before it is used
(12 §4); a v2 feature is an optional fix, so a mismatch turns that feature
off alone and is logged.

Confidence: **[sure]** read from the game's code or data, or observed in
game; **[likely]** strong inference; **[guess]** plausible, must be
confirmed. Requirements that are design choices rather than facts are
marked **[proposal]** with a one-line reason.

## 1. Summary

| Feature | Key (section 3) | v1 value | Section |
|---|---|---|---|
| smooth turning at a set speed, time-based | `Comfort.SmoothTurnSpeed` | 0 (the game's own look, as v1) | 4.2 |
| snap-turn repeat interval | `Comfort.SnapTurnRepeatMs` | 300 | 4.3 |
| a short fade at each snap step | `Comfort.SnapTurnFade` | false | 4.3 |
| no turning during interactions | `Controls.InteractionAwareTurning` | false | 4.4 |
| comfort vignette | `Comfort.Vignette` | 0 (off) | 5 |
| seated or standing play | `Comfort.PlayMode` | `"Seated"` | 6.1 |
| calibrated standing eye height | `Comfort.StandingEyeHeight` | 0 (not calibrated) | 6.2 |
| world scale | `Comfort.WorldScale` | 1.0 | 6.3 |
| d-pad emulation | `Controls.DpadEmulation` | false | 7 |
| two-handed recenter gesture | `Controls.RecenterGesture` | false | 8.2 |
| HUD toggle gesture | `Controls.HudToggleGesture` | false | 8.3 |
| body-relative holster gestures | `Controls.HolsterGestures` | false | 8.4 |
| lean while hiding by moving the head | `Controls.HidingLean` | false | 9.2 |
| hold breath by covering the mouth | `Controls.HoldBreathGesture` | false | 9.3 |
| glyphs per controller type, following changes | `Controls.ControllerGlyphs` | false | 10 |
| OpenXR as the default runtime | (default of `VRRuntime`) | `"OpenVR"` | 11 |
| v2 settings on the options page | (no key) | v1 page | 12 |
| VR-off hotkey | `Controls.VrOffHotkey` | false | 13 |
| crash-safe logging | `Controls.CrashSafeLog` | false | 14 |

## 2. Game facts

### 2.1 The input action names and their indices [sure for the table]

The game names the actions of its three input maps (04 §2.4) in one static
table in `.data` at RVA **0x124F120** (2020: ?). Each entry is 12 bytes:
a pointer to the action's NUL-terminated name, the action's index in its
map, and a kind (0 digital, 1 analog axis, 2 slider; sliders are analog
actions of the map). The entries run map by map: player map (map 0)
digital, then analog, then the menu map (map 1, from RVA 0x124F358), then
the debug map (map 2, from RVA 0x124F490). These are the names
`DATA\INPUT.XML` binds controls to (2.2).

The entry counts match the counts observed in each map at the first remap
(04 §2.4: 41/6, 19/6, 32/8), and every action function 04 §2.4 inferred from
play matches the name at its index, so the table index is the map entry
index [likely: the counts and all observed functions agree; to be
confirmed by the logging of 19 item 1].

Map 0 (player), digital:

| Index | Name | Index | Name | Index | Name |
|---|---|---|---|---|---|
| 0x00 | `move_forward` | 0x0E | `melee` | 0x1C | `select_motion_tracker` |
| 0x01 | `move_back` | 0x0F | `button_mash` | 0x1D | `select_torch` |
| 0x02 | `move_left` | 0x10 | `reload_weapon` | 0x1E | `torch_beam` |
| 0x03 | `move_right` | 0x11 | `activate_item` | 0x1F..0x24 | `select_item_1` .. `select_item_6` |
| 0x04 | `turn_left` | 0x12 | `peek` | 0x25 | `change_dof_focus` |
| 0x05 | `turn_right` | 0x13..0x17 | `select_weapon_1` .. `select_weapon_5` | 0x26 | `hud_menu` |
| 0x06 | `look_up` | 0x18 | `prev_weapon` | 0x27 | `tab_left` |
| 0x07 | `look_down` | 0x19 | `next_weapon` | 0x28 | `tab_right` |
| 0x08 | `sprint` | 0x1A | `prev_main_weapon` | | |
| 0x09 | `crouch` | 0x1B | `next_main_weapon` | | |
| 0x0A | `aim` | | | | |
| 0x0B | `fire_or_throw` | | | | |
| 0x0C | `use` | | | | |
| 0x0D | `back_close` | | | | |

Map 0, analog: 0 `x_axis`, 1 `y_axis`, 2 `cam_vertical_rot`, 3
`cam_horizontal_rot` (the turn action of 04 §6), 4 `analog_aim`, 5
`analog_shoot` (sliders).

Map 1 (menus), digital: 0 `ok`, 1 `cancel`, 2 `up`, 3 `down`, 4 `left`, 5
`right`, 6 `start`, 7 `select`, 8 `open_menu`, 9 `craft_item`, 0x0A
`alt_1`, 0x0B `alt_2`, 0x0C `scroll_up`, 0x0D `scroll_down`, 0x0E
`minigame_top`, 0x0F `minigame_middle`, 0x10 `minigame_bottom`, 0x11
`leaderboard`, 0x12 `player_info`. Analog: 0 `zoom_in`, 1 `zoom_out`
(sliders), 2 `x_axis`, 3 `y_axis`, 4 `x_axis_right`, 5 `y_axis_right`; the
table has a seventh, `scroll` (6), beyond the six the map uses.

This resolves names 04 §2.4 left open: map 0 0x27 / 0x28 are `tab_left` /
`tab_right`, map 1 index 1 is `cancel`, 9 `craft_item`, 0x0B and 0x0E
`alt_2` and `minigame_top` (section 20).

### 2.2 Default bindings that matter here (`DATA\INPUT.XML`) [sure]

From the game's own `DATA\INPUT.XML` (20,802 bytes, Steam 2015; 14 §2.10):

| Control | Player map, default gamepad device | Menu map, gamepad |
|---|---|---|
| `dpad_left` | `prev_main_weapon` (every preset) | `left` |
| `dpad_right` | `next_main_weapon` (every preset) | `right` |
| `dpad_up` | none (default and presets 1 to 3) | `up` |
| `dpad_down` | none (default and presets 1 to 3); `crouch` in the remote-play presets 4 and 5 | `down` |
| `lb` | `peek` and `tab_left` (default device) | `craft_item` |
| `lcontrol` (keyboard) | `peek` | |

So the game's d-pad functions are weapon cycling in play and item
navigation in menus. The v1 thumbstick scheme already drives
`prev_main_weapon` / `next_main_weapon` (04 §5.8.5: right stick down / up,
indices 0x1A / 0x1B); v1 drives none of the menu map's `up`, `down`,
`left`, `right`.

The file's default player bindings (for example `aim` on `lt_button`,
`select_motion_tracker` on `rb`, `crouch` on `rstick_button`) differ from
the stock pad layout 04 §2.4 states from the prompts. Which bindings the
2015 PC build actually loads for the pad is an open question (19 item 2).
Nothing in this document depends on the answer: every feature here drives
actions through their index or their original pad control as recorded at
virtualisation (04 §3.2), never through an assumed preset.

### 2.3 Peek, hiding and the breathing game [sure for the strings]

- `peek` (map 0 digital 0x12) is the game's lean action. The game's
  camera code has a `CameraPeek` behaviour (string at RVA 0xDC3948,
  referenced from code at 0x01E5DD, 0x3462F1 and 0xD48791) and a peek
  system with tuning names such as `max_vertical_peek`,
  `max_horizontal_peek`, `max_peek_control_velocity` and
  `peek_control_scaling` (strings around RVA 0x12531F8), which suggests
  that while peeking a stick moves the lean [likely]. v1 drives no VR
  input to `peek`.
- A script entity that is the hiding place [likely, from its names] has methods named in a table at RVA
  0xF12040: `disallow_interrupt`, `Get_In`, `Add_NPC`,
  `Start_Breathing_Game`, `Breathing_Game_Started`, `End_Breathing_Game`,
  `Breathing_Game_Ended`, `bind_all` and their `_finished` forms. Its
  handler passes `Start_Breathing_Game` (string at RVA 0xF067B0) and
  `End_Breathing_Game` (RVA 0xF06764) to `ScriptEventTap` (11 §5.1,
  RVA 0xA1BCC0): the `push imm32` of each string is at 0x51070C and
  0x510765, and the `call rel32` to the tap at 0x510714 and 0x51076D. So
  the tap that v1 already installs sees the start and the end of the
  breathing game [sure for the calls; likely that the pair brackets the
  game's hold-breath minigame while hiding].
- The game also has a `CHR_HoldBreath` script entity (string at RVA
  0xFD6838) and UI texts for the breathing game's prompt and tutorial
  (`show_breathing_game_prompt`, `set_breathing_game_prompt_progress`,
  `display_breathing_game_tutorial`, strings around RVA 0xF0073C).
- `hiding_visibility_start` / `hiding_visibility_finish` reach the tap
  as v1 already uses them (11 §5.3).

- The game's own tutorial texts for the breathing game, in
  `DATA\TEXT\ENGLISH\UI.TXT` (UTF-16; each `#name#` is shown as the
  glyph of the control bound to that action, 2.1), name the inputs of a
  hiding place [sure for the texts]:

  | Text id | Text |
  |---|---|
  | `AI_UI_TUTORIAL_BREATHING_TUTORIAL_1` | `Press #button_mash# to exit` / `#move# to peek` |
  | `AI_UI_TUTORIAL_BREATHING_TUTORIAL_2` | `Hold #aim# to hold breath` |
  | `AI_UI_TUTORIAL_BREATHING_TUTORIAL_3` | `Hold #move# down to lean back` |
  | `AI_UI_TUTORIAL_BREATHING_TUTORIAL_3_KB` | `Hold #move_back# to lean back` |

  So in a hiding place the movement stick (map 0 analog `x_axis` and
  `y_axis`; `move_back` on the keyboard) peeks and leans, down leaning
  back, and `aim` (map 0 digital 0x0A) holds the breath [likely: the game's
  own instructions to the player; to be confirmed in play, 19 items 3 and
  4]. The same file's `AI_UI_HEAD_TRACKING_TUTORIAL` says the game's own
  head tracking option applies tracked head movement to the character
  "when in peek mode", for "peeking over objects and around corners".

### 2.4 The HUD movies and how the game hides them [sure]

| Label | RVA | What is there |
|---|---|---|
| `UiManagerPtr` | 0x134A78C | global pointer to the game's UI manager |
| `UiFindObject` | 0x410420 | thiscall on the UI manager, one argument `const char* name`, `ret 4`; returns the UI object of that name or null. It takes the manager's critical section, so it is called on the game thread. Prologue `83 ec 08 53 55 56 8b f1`. 515 call sites |
| `UiInvoke` | 0x4081E0 | thiscall on a UI object, one argument `const char* function`, `ret 4`: calls that public function of the object's movie. SEH prologue `6a ff 68 <imm32> 64 a1 00 00 00 00` |
| `WeaponHudHide` | 0x40C220 | thiscall on the `weaponstuff` UI object, no arguments, plain `ret`: when the movie (+0x68) exists, invokes `force_hud_hidden`, else sets the pending byte +0x129 to 1; either way sets +0x40 to 1. Bytes `56 8b f1 83 7e 68 00` |
| `WeaponHudShow` | 0x40C250 | the same object; invokes `force_hud_visible` when the movie exists, else sets +0x129 to 1; either way sets +0x40 to 0. Bytes `56 8b f1 83 7e 68 00` |
| `PickupHudHide` | 0x40ADC0 | thiscall on the `pickupOverlay` UI object, no arguments, plain `ret`: invokes `force_hud_hidden`, sets +0x40 to 1. Bytes `56 68`, then the address of the string `force_hud_hidden` |
| `PickupHudShow` | 0x40ADE0 | the same, invokes `force_hud_visible`, sets +0x40 to 0. Bytes `56 68`, then the address of `force_hud_visible` |
| names | 0xDC6CE8, 0xDC3934, 0xE0451C | the strings `weaponstuff`, `pickupOverlay`, `reaction_game` that the game passes to `UiFindObject` |
| function names | 0xEFB2D8, 0xEFB2EC | `force_hud_hidden`, `force_hud_visible` |

In the player's `UI.PAK`, three movies define `force_hud_hidden` and
`force_hud_visible`: `WEAPONSTUFF.GFX` (the HUD: weapon, ammunition,
prompts), `PICKUPOVERLAY.GFX` and `REACTION_GAME.GFX`.

The +0x40 byte is the game's own "HUD forced hidden" state of each of the
two objects [sure for its writers: the four methods above and the two
re-apply sites below]. Each object re-applies that state when its movie
has been (re)loaded [sure for the code; likely for the trigger]:

- `weaponstuff`: an update at RVA 0x44D020, at 0x44D415, finds the
  pending byte +0x129 set and invokes `force_hud_hidden` or
  `force_hud_visible` according to +0x40 when the movie exists [likely
  that this update belongs to `weaponstuff`: it reads the same pending
  byte and +0x68].
- `pickupOverlay`: an update at RVA 0x44BBE0, at 0x44C0A1, finds its
  pending byte +0x408 set and the movie present, clears +0x408 and
  invokes the function matching +0x40 [likely that this update belongs to
  `pickupOverlay`: it reads the same fields]. +0x408 is set to 1 by the
  method at 0x40AE00, which in one path also sets +0x40 to 0 [sure for the
  writes; likely: the movie's load], so a reload of this movie can reset
  the game's state to visible.

The game hides its HUD for its own reasons through these methods: a
scripted entity's methods at RVA 0x1094A0 (hide) and 0x1095A0 (show)
[likely: the levels' HUD visibility entity] look up `weaponstuff`,
`pickupOverlay` and `radial_menu` (the show method also `SCANNER`); hide
calls `PickupHudHide` and `WeaponHudHide`, show calls `PickupHudShow` and
`WeaponHudShow`, and each also calls a method of the other objects it
found. 23 §2.7 cites this section for these facts.

### 2.5 v1 game facts used here

- First-person camera (FollowBehavior): yaw at +0x204 with its mirror at
  +0x1FC, ±1.0 = ±180°, wrapped to [-1, 1]; look input byte +0x264; written
  only while the camera is tracked and active (04 §2.7, 05 §6.1).
- The per-frame stereo update on the game thread (05 §3.2), where snap
  turn and the look gate run.
- Cutscene and level-fade state from the UI entity messages (05 §6.2,
  §3.2g).
- Interaction and hiding state from the logging of game script events
  (11 §5.3).
- Recentering (03 §2, 04 §8, 09 §6) and the near-the-head test (04 §5.10).
- The stereo mode at stereo object +0x60: 1 while a menu is on the virtual
  screen (05 §1).
- Detaching the stereo device after a late failure (02 §7.4).
- The keyboard key test (04 §2.2, 05 §1).

## 3. Settings

### 3.1 Keys

New keys go in two new objects inside `Settings`: `Comfort` and
`Controls`. The key names are this project's own. Reading, writing and sanitising follow 06 §3: each key is
read on its own, a missing key or one of the wrong type gives its default,
unknown keys are kept on save, and `Comfort` or `Controls` that is not an
object is replaced by an object on the next write. The file is still not
rewritten at start-up (06 §3.1), so an upgrade from v1 leaves the file
untouched until the player changes a setting; the first write adds the new
keys with their values.

| Key | JSON type | Default (v1 value) | Values / range, sanitise | Takes effect |
|---|---|---|---|---|
| `Comfort.SmoothTurnSpeed` | number | 0 | 0, or 30 to 360 (degrees per second); a value in (0, 30) becomes 30, above 360 becomes 360, NaN or negative becomes 0 | at once |
| `Comfort.SnapTurnRepeatMs` | integer | 300 | 0 (no repeat), or 150 to 1000; a value in (0, 150) becomes 150, above 1000 becomes 1000, negative becomes 300 | at once |
| `Comfort.SnapTurnFade` | bool | false | | at once |
| `Comfort.Vignette` | number | 0 | 0 to 1 (0 off); outside or NaN becomes 0 | at once |
| `Comfort.PlayMode` | string | `"Seated"` | `"Seated"`, `"Standing"` (case-sensitive); anything else is `"Seated"` | at the next recenter (6.1) |
| `Comfort.StandingEyeHeight` | number | 0 | 0 (not calibrated) or 1.0 to 2.2 (metres); anything else becomes 0 | at once |
| `Comfort.WorldScale` | number | 1.0 | 0.5 to 1.5; outside or NaN becomes 1.0 | at once (6.3) |
| `Controls.InteractionAwareTurning` | bool | false | | at once |
| `Controls.DpadEmulation` | bool | false | | at once |
| `Controls.RecenterGesture` | bool | false | | at once |
| `Controls.HudToggleGesture` | bool | false | | at once |
| `Controls.HolsterGestures` | bool | false | | at once |
| `Controls.HidingLean` | bool | false | | at once |
| `Controls.HoldBreathGesture` | bool | false | | at once |
| `Controls.ControllerGlyphs` | bool | false | | at the next controller identification (10) |
| `Controls.VrOffHotkey` | bool | false | | at once |
| `Controls.CrashSafeLog` | bool | false | | next launch (14) |

There is no master switch (20 §1.3, user decision of 2026-10-16): each
key of this table applies on its own, as stored, and its default is its
v1 value, so a file without these keys, or with each at its default,
gives v1, and a v2 problem is told from a v1 one by setting the keys
concerned back to their defaults. (`VRRuntime`'s default, section 11, is
not a v2 key.)

`Comfort.PlayMode` and `Comfort.StandingEyeHeight` are the only keys for
seated and standing play and the calibration; 21 §11 reads them for its
height adjustment, real crouch and room-scale and defines no keys of its
own for them (one owner per setting, 20 §5).

Reasons for the ranges [proposal for each]:

- `SmoothTurnSpeed` 30 to 360 °/s: below 30 °/s turning a full circle
  takes over 12 s, too slow to react to the Alien; above 360 °/s the turn
  is no smoother than a snap and much worse for comfort. 0 keeps v1's turn
  through the game's own look.
- `SnapTurnRepeatMs`: 300 ms is v1's value (04 §6, 05 §3.2c), confirmed as
  the default. Below 150 ms a held stick spins faster than a player can
  stop on the step wanted; above 1000 ms a held stick feels unresponsive.
  0 means one step per deflection, for players who prefer to flick.
- `StandingEyeHeight` 1.0 to 2.2 m: covers seated-height children to tall
  adults standing; a value outside cannot be a calibrated eye height.
- `WorldScale` 0.5 to 1.5: the range `-vrscale` accepts (06 §2, kept for
  compatibility), so the setting and the switch agree.

### 3.2 Content

With the v2 defaults the two new objects parse to:

```json
"Comfort": {
    "PlayMode": "Seated",
    "SmoothTurnSpeed": 0,
    "SnapTurnFade": false,
    "SnapTurnRepeatMs": 300,
    "StandingEyeHeight": 0,
    "Vignette": 0,
    "WorldScale": 1.0
},
"Controls": {
    "ControllerGlyphs": false,
    "CrashSafeLog": false,
    "DpadEmulation": false,
    "HidingLean": false,
    "HoldBreathGesture": false,
    "HolsterGestures": false,
    "HudToggleGesture": false,
    "InteractionAwareTurning": false,
    "RecenterGesture": false,
    "VrOffHotkey": false
}
```

inside `Settings`. Formatting and order are the implementation's choice
(06 §3.2).

### 3.3 Commands and pushes

Each key gets a mod command (06 §4.1, §4.2) and a push to the options page
(06 §4.3), following v1's rules: a command is one string beginning with
`mvr_`, its value decimal with `.` as the decimal point; a bad or
out-of-range value changes nothing (06 §4.2 "one rule for bad values"); a
change is saved (06 §3.1) and logged (11 §2.5). The command names and the
page's functions are the implementation's own, under one constraint: each
begins with `mvr_` so that 06 §4.1 recognises it as a mod command, and none
equals a v1 command name. Two commands carry no setting value:

- **calibrate height**: takes the standing eye height now (6.2);
- (no command turns VR off; that is the hotkey of 13 only.)

A pushed value never makes the page send a command back (06 §4.3).

## 4. Turning

### 4.1 Which turning applies

The turn value is v1's: the merged value of map 0's turn action (04 §6:
the larger-magnitude of the real right stick and the VR turn value of 04
§5.8.5). Exactly one of three turnings applies, chosen every update:

| `Input.SnapRotation` | `Comfort.SmoothTurnSpeed` | Turning |
|---|---|---|
| on | any | **snap turn** (04 §6), with the v2 repeat and fade of 4.3 |
| off | 0 | **game look**: the game's own stick look turns smoothly (v1, 04 §7), scaled by the game's Camera Speed (4.5) |
| off | above 0 | **smooth turn** written by the mod (4.2) |

The look byte (FollowBehavior +0x264, 04 §7, 05 §3.2d) becomes: 1 while
the stereo device is not running; otherwise 1 when keyboard/mouse is in use
(05 §3.2b) or the turning is game look; 0 otherwise. So with smooth turn
the game's own stick look is off, as with snap turn: pitch comes from the
head only, and the stick does not turn twice. With every v2 key at its v1
value the table and this rule reduce to v1's.

All turning (snap and smooth) applies only while the conditions of 04 §6
hold (stereo running, the active pad device exists, a FollowBehavior is
tracked and active, stereo mode not 1), and, with
`Controls.InteractionAwareTurning` on, while 4.4 allows it. Independently
of that key, no turning happens while 23's lever mapping is active (a
lever or junction with `Interaction.PhysicalLevers` on, 23 §3.2), because
the mapping then owns the grips and the move and look actions (one owner
per input, 20 §5).

### 4.2 Smooth turn

Required result: while the turn value is deflected past the centre zone,
the player's heading changes continuously at a rate set in degrees per
second, the same at any frame rate.

- **Rate.** With `v` the turn value, `d = (|v| - 0.33) / (1 - 0.33)`
  clamped to [0, 1], the yaw changes at `SmoothTurnSpeed * d` degrees per
  second, to the right for `v > 0` (the sign of 04 §6's snap step). [proposal:
  0.33 is the centre zone 04 §5.8.5 already applies to the VR turn value,
  applied here to the merged value too so a real stick at rest never
  drifts; the linear ramp gives fine control near the zone and full speed
  at full deflection.]
- **Time base.** Each update adds `rate * dt` to the yaw (+0x204, in
  half-turn units: degrees / 180) and writes the same value to +0x1FC,
  wrapped into [-1, 1] as 04 §6 does. `dt` is the time since the previous
  update in which smooth turn applied, from a monotonic clock, at most 50
  ms. The first update after smooth turn becomes applicable (after a menu,
  a cutscene, an interaction, a camera change) uses `dt = 0`. [proposal: the
  cap keeps a hitch or a long frame from turning the player by a large
  jump; below 20 frames per second the turn slows down instead.]
- Accumulated in floating point; no step quantisation.
- The game's frame-time argument of the stereo update (05 §3.1) may be used
  instead of the clock if it is the real time between updates; whether it
  is is not verified, so the clock is the reference.

Edge cases: the stick held while a menu opens does nothing until the menu
closes, then turning resumes with `dt = 0`; focus loss zeroes the VR turn
value (04 §5.1) so smooth turn stops at once; a real pad's right stick
turns the same way as the VR stick.

### 4.3 Snap turn: repeat and fade

**Repeat.** v1's snap turn (04 §6, 05 §3.2c) with the repeat interval
`Comfort.SnapTurnRepeatMs` instead of the fixed 300 ms. 0 means a held stick
steps once and steps again only after returning inside 0.5. Default 300:
v1's value is confirmed (its reason, 05 §3.2c, still holds).

**Fade** (`Comfort.SnapTurnFade`). Required result: each snap step is
hidden behind a brief darkening, so the player never sees the view jump.

- The first head-tracked frame rendered with the new yaw is shown fully
  black in both eyes; the following frames return from black to the
  normal image over 100 ms. [proposal: a fade-in only adds no input delay
  (the step is not held back), and 100 ms is short enough not to hide the
  Alien moving while long enough that the cut is not seen.]
- A further step during the fade-in restarts it from black.
- The darkening is drawn into each eye's image exactly like the vignette
  (section 5) with full strength over the whole eye.
- Screen frames (menus) are never darkened; with the fade off nothing is
  drawn.

Thread note: the step is made on the game thread; the frame that carries
it is identified by the render pose the game read for it (03 §6.3) or by a
step counter published with an atomic store and read by the render thread
at that frame's slot 6 (section 15).

### 4.4 Interaction-aware turning

With `Controls.InteractionAwareTurning` on, no turning (snap, smooth, or
game look from the stick; mouse look stays) happens while any of these
holds:

- the interaction state is "in interaction" (11 §5.3), any kind;
- the breathing game is in progress (9.1);
- a cutscene or level fade is in progress (05 §3.2g);
- the first-person camera is not tracked and active (already v1: the game
  owns the camera, 05 §6.1).

While turning is blocked, a held snap deflection does not step and does not
repeat; after the block ends, the stick must return inside 0.5 before the
next snap step (as v1 does when the camera becomes inactive, 04 §6), and
smooth turn resumes with `dt = 0`. The look byte stays 0 during the block
when the turning is game look and pad or VR input is in use, so the
game's own stick look cannot turn either. [proposal: during keypads,
terminals, levers and hacking the stick drives the interaction; turning
the body at the same time moves the interaction prompt out of view.]

Hiding (11 §5.3 "hiding") does not by itself block turning: the game limits
the camera in a hiding place itself [likely; to confirm in game, 19 item 7].

With the key off, turning is v1's: the interaction state is ignored.

### 4.5 The game's Camera Speed

The stock Game page's Camera Speed (`_cameraSensitivityItem`, sends
`set_gamepad_speed`; 06 §5.3) scales the game's own stick look. Its English
label is `Controller Sensitivity`, with the description line `Adjust the
camera sensitivity when using the controller` [sure: the stock menu
movies build `_cameraSensitivityItem` as a slider with the text key
`AI_UI_MENU_CAMERA_SPEED`, and `DATA\TEXT\ENGLISH\UI.TXT` gives that key
and its `_DESC` key these texts]. It therefore scales game look (4.1)
only. Snap turn ignores it (v1), and so does smooth turn: its speed is
`SmoothTurnSpeed` alone [proposal: one setting with a unit the player can
reason about; Camera Speed keeps its stock meaning for flat play]. Camera
Speed stays enabled in VR (06 §5.3). The options page's description of
`SmoothTurnSpeed` (12.2) names it by its label, so the player finds the
item it means.

## 5. Comfort vignette

**What the player experiences.** While moving with the stick or turning
smoothly, the edge of the view darkens, narrowing the visible field; it
clears when the movement stops. The stronger the setting, the narrower the
clear area.

**When it applies.** The vignette's target strength is
`Comfort.Vignette` while, in the current game frame:

- the merged movement value (map 0 analog actions 0 and 1, VR and real
  pad) has a magnitude above 0.1, or
- smooth turn (4.2) changed the yaw, or the game look turned with the
  stick (4.1, turn value past 0.33);

and 0 otherwise. Snap steps do not trigger it (they use the fade, 4.3).
Keyboard movement does not trigger it [proposal: keyboard players are not
in the headset's comfort case the setting addresses; to revisit]. The
shown strength moves towards the target at a rate that takes 0 to full in
150 ms and full to 0 in 300 ms. [proposal: 0.1 ignores stick noise; a fast
onset covers the start of motion, where discomfort begins, and a slower
release avoids flicker when the stick is briefly released.] It is 0 in
screen frames and while the stereo device is not running.

**Shape** (required result). In each eye the vignette is a function of the
angle from that eye's forward axis: pixels whose angle from the axis is
below an inner radius are untouched; beyond an outer radius they are black;
in between they darken smoothly. With strength `k` in (0, 1], the inner
radius is `60° - 35° * k` and the outer radius is the inner radius plus
15°. [proposal: at full strength a clear cone of 25° around the gaze
centre, roughly the common "strong" comfort setting; at low strength the
darkening stays outside most headsets' sweet spot.] The shape is the same
in both eyes in angle, so the darkened ring fuses and does not appear
doubled.

**Rendering approach** (required result, both runtimes). The vignette is
drawn into each eye's image itself, so it is part of what the runtime
composites and reprojects:

- It is drawn in slot 6 of a head-tracked frame, on the render thread,
  into the stereo render target (device +0x158), after the game has
  finished the frame and before the frame is handed to the runtime:
  before `IVRCompositor::Submit` on OpenVR (03 §6.2), before the copy into
  the eye swapchain images on OpenXR (09 §5.2).
- Each eye half is darkened about its own forward axis. The game renders
  each eye with the symmetric FOV of 03 §3.2 (tangents `maxH`, `maxV`), so
  the eye's forward axis is at the centre of its half, and a pixel at
  normalised position `(u, v)` in the half has direction tangents
  `((2u - 1) * maxH, (1 - 2v) * maxV)`; its angle from the axis is
  `atan(sqrt(tx^2 + ty^2))`.
- The pass multiplies the colour by the darkening factor (blend to black);
  it never brightens and never touches pixels with factor 1.
- The pass leaves the game's D3D11 pipeline state as it found it (the same
  requirement as the mirror draw, 03 §8), uses resources created when the
  target size changes, and allocates nothing per frame (12 §6.1).
- Nothing is drawn while the shown strength is 0 and no snap fade runs, so
  the feature off costs one comparison per frame.
- The desktop mirror shows the vignette too, since it samples the same
  target [proposal: the mirror shows what the player sees].

**Cost.** One full-target pass while active: one draw per eye half over at
most 2 x the eye pixel count (for example about 14.5 megapixels at 2688 x
2688 per eye), a single multiply per pixel. It must stay within the
render-thread rule of 12 §6.1 (no wait, no readback) and is measured in
game (18 item 4); the target is well under 0.2 ms of GPU time at that size
on current hardware [proposal: the bound to check against].

**Edge cases.** Menus and videos (screen frames): no vignette. Cutscenes:
the FollowBehavior is inactive, so no movement or turn drives it. Death and
reload: the strength falls to 0 with the movement. Focus loss: VR movement
reads 0, so it fades out. Runtime None or no device: nothing is drawn.

## 6. Seated and standing play, world scale

### 6.1 Seated and standing modes

How v1 places the head (03 §1, §2.1): the game receives the head pose
relative to the reference pose, and the reference keeps the head's full
position at the latest recenter, height included. So at every recenter the
player's eye is put at the game character's eye height, whatever the
player's real height or posture. That is the right behaviour seated, and
it is v1.

| `Comfort.PlayMode` | Vertical part of the reference | Result |
|---|---|---|
| `"Seated"` (v1) | the head's height at the latest recenter (03 §2.1) | the eye is at the character's eye height after every recenter |
| `"Standing"`, calibrated | the calibrated standing eye height `h` (6.2), measured from the floor, never changed by a recenter | standing straight, the eye is at the character's eye height; ducking or kneeling lowers the view by the real amount, whenever the last recenter happened |
| `"Standing"`, not calibrated | as seated, until a calibration is taken (6.2) | |

Required result for standing: the vertical offset the game receives is
`(current head height above the floor - h) * s` (s the world scale, 6.3),
while the horizontal part and the yaw keep v1's recenter behaviour. The
recenter that a loading screen or putting the headset on triggers (03 §2.2)
then no longer resets the height when the player happens to be crouching.

Floor height comes from the runtime:

- OpenVR: the standing tracking universe the mod already uses (03 §1): its
  Y is the height above the calibrated floor.
- OpenXR: the `STAGE` reference space when the runtime supports it, else
  `LOCAL_FLOOR` when the runtime offers it (core in OpenXR 1.1, or the
  `XR_EXT_local_floor` extension); the head is located in that space at the
  same time as the frame's views (09 §5.3). If neither is available,
  standing mode is unavailable on this runtime: play is seated and this is
  logged once [proposal: a height without a floor reference drifts with
  each re-anchor of the base space, 09 §6.3].

Who uses the mode: 21 (its height adjustment, real crouch and room-scale,
21 §11.4) reads it; this document owns the setting and the calibration
(20 §4).

The mode takes effect at the next recenter, which a change from the menu
requests (as a pending automatic request, 05 §3.2e) [proposal: switching
the reference mid-play would jump the view].

### 6.2 Height calibration

Required result: `Comfort.StandingEyeHeight` holds the player's standing
eye height in runtime metres (before world scale), measured from the
floor.

- **Taking it.** On the calibrate command (3.3) from the options page, the
  mod waits 3 s (shown on the page as a countdown, 12.2, so the player can
  take off the controller and stand straight), then takes the head's height
  above the floor from the first valid head pose while the headset is in
  use (03 §2.3, 09 §6.2), stores it, saves the file and logs it.
  [proposal: 3 s is time to stand up straight after pressing the button.]
- **Automatic first calibration.** In standing mode with no calibration
  (0), the first recenter taken while the headset is in use and upright
  also takes the calibration. [proposal: a player who picks standing and
  never calibrates still gets a sensible height; it is the head height at
  the moment play starts, which is usually standing.]
- A height outside 1.0 to 2.2 m is not taken (logged; the old value
  stays).
- An invalid pose never calibrates; the request stays pending until a
  valid pose arrives, like a recenter (03 §2.1).

### 6.3 World scale

`Comfort.WorldScale` is the world scale `s` of 03 (metres scale of head and
eye positions, hand positions, 04 §5.11), as a setting. The `-vrscale`
switch (06 §2) still wins for its run: when it is present and accepted, `s`
is its value and the setting is ignored (the page shows the switch's value
and says the command line sets it, 12.2). Without the switch `s` is the
setting.

Changes from v1 (03 parameter table, 06 §1): `s` is no longer read once
when the device is created. A change from the menu applies from the next
frame: the head pose (03 §1), the eye offsets (03 §5, refreshed every slot
5), the reference pose (re-converted with the new `s`, so the view does not
jump: the head stays where it is relative to the reference) and the hand
poses all use the new value. Runtime layer poses (09 §5.2) never use `s`.

Order with standing mode and real crouch (20 §10 item 4): positions are
taken relative to the reference in runtime metres first, then multiplied by
`s`. The standing height `h` and 21's crouch threshold are in runtime
metres, before `s` [proposal: they describe the player's body, which does
not change with the world's scale].

## 7. D-pad emulation

**What the player experiences.** On VR controllers, which mostly have no
d-pad, the game's d-pad functions (2.2) can still be used: weapon cycling
in play and item navigation in menus and interaction screens. A controller
that has a d-pad (the Steam Frame's left hand) uses it as a pad's d-pad,
with or without the emulation.

**Required results.** With `Controls.DpadEmulation` on:

- every game function the player's bindings put on a d-pad direction can
  be reached from the VR controllers, in play, in menus and in
  interaction screens;
- one physical input never drives both walking and a d-pad press at the
  same moment;
- a prompt for such a function shows the control the player must use for
  it (section 10).

**Source** [proposal]. The left stick's direction gives the d-pad press,
while any of these holds:

- an interaction is in progress (11 §5.3), or the breathing game (9.1),
  except while 23's lever mapping is active (23 §3.2), which owns the move
  action then;
- a menu is on the virtual screen (stereo mode 1);
- the player holds the "d-pad modifier": the control of Sprint (04
  §5.8.1), the left stick click in every default binding, held for 0.3 s;
  on wands Sprint's left touchpad click on one of its four edges (outside
  the centre zone of 04 §5.1), at once, with the direction of the edge.
  While the modifier is engaged Sprint reads released; a shorter click, or
  a wand's click in the pad's centre, stays sprint.

Reasons: the left stick is the one control every supported controller has
under the left thumb, where a d-pad sits on a pad, so no new reach is
learned; during interactions and in menus it does not walk the player, so
its direction is free; elsewhere a deliberate modifier keeps walking
unaffected, and holding the click (rather than a click alone, which is
sprint) tells the two apart. The modifier is Sprint's control rather than
a runtime input of its own [proposal: one physical control does both, so a
second input would need a second binding source on the same control in
every default binding, and a player who moves Sprint in the runtime's
binding UI moves the modifier with it].

**Direction.** With the left stick at `(x, y)`: a direction is pressed when
the stick is beyond 0.6 along that axis and the larger of `|x|`, `|y|`
(one direction at a time); it is released below 0.4. [proposal: hysteresis
so a stick at the threshold does not chatter; one direction at a time as on
a d-pad's common use.] While a d-pad direction is pressed from the stick,
the movement value read from that stick is 0, which meets the "never both"
result above.

**What it drives** (required result): while a direction is pressed, from
the emulation or from a controller's own d-pad (the four d-pad inputs of
04 §5.8.1), every digital action of maps 0 and 1 whose **original pad
control** (04 §3.2) is that d-pad direction (0x04 DPAD_UP, 0x05 DPAD_DOWN,
0x06 DPAD_LEFT, 0x07 DPAD_RIGHT) has its VR digital state set, merged with
v1's state for that action by OR. A controller's own d-pad may press two
directions at once, as a pad's may; the emulation presses one. Under the default bindings this is `prev_main_weapon` /
`next_main_weapon` (map 0, left / right) and `up`, `down`, `left`, `right`
(map 1). Driving by original control follows whatever the player bound to
the d-pad and needs no table of actions. Activity (04 §5.1) counts these
values like any other.

**Glyphs.** As a consequence of section 10's rule that a prompt shows the
control that drives it, with d-pad emulation on, prompts for actions whose
original control is a d-pad direction show the emulated control (the left
stick direction, or the left touchpad edge on wands) instead of the stock
d-pad glyph; the designs are the d-pad rows of 10.2, and the controls help
gains the `D-Pad` row of 10.3.

**With the key off** no emulated d-pad state is written (v1). A
controller's own d-pad still drives the d-pad actions [proposal: it is a
physical d-pad doing what a pad's d-pad does, and the controllers v1 knows
have none bound, 04 §5.8.2, so their v1 behaviour is unchanged].

Whether some screens read the pad's d-pad without an action binding, and
which interaction screens need the d-pad at all, is open (19 item 5).

## 8. Gestures

### 8.1 Common rules

- Gestures use the hand and head poses of the render thread's controls
  write (04 §5.9, §5.10), taken at the same time as the controls they
  modify, in runtime metres before world scale. A hand without a valid
  pose never takes part in a gesture.
- Distances are measured from the head's position (the HMD pose, as 04
  §5.10); "in head frame" means relative to the head, rotated by the
  inverse of the head's yaw only, with x right, y up, z back.
- A gesture fires once per performance: after firing it needs its trigger
  released before it can fire again.
- No gesture fires while the runtime has no input focus (04 §5.1: every
  input reads released), or in the first 0.5 s after focus returns
  [proposal: a hand held still while the dashboard closes must not fire
  anything].
- A gesture's result that touches the game (a recenter, the HUD, an action)
  is carried out on the game thread (section 15).
- Each gesture is logged when it fires (16), and, with `DebugOptions` on,
  when it was suppressed and why.

### 8.2 Two-handed recenter

**Gesture.** Both grips pressed (the Aim and Tracker semantic inputs, 04
§5.8.1) while both hands are within 0.30 m of the head, held together for
0.75 s. [proposal: both hands at the face with both grips held for most of
a second does not happen in play; the hold time and the second hand rule
out aiming with the tracker raised. Both hands are required, rather than
one hand or a button, so that a single raised hand during play never
triggers it, and both grips are used because they are the controls every
supported controller has and the player can find without looking; v1's
recenter chord (04 §8) stays for players who prefer buttons.]

**Result.** A manual recenter as the v1 chord gives (04 §8): taken at once
(once per gesture), never from an invalid pose (03 §2.1), the virtual
screen re-placed (05 §3.2e). It is logged with the trigger "gesture".

**False triggers.** Not while Fire is pressed; the timer restarts whenever
either condition breaks. The grips still drive Aim and the tracker while
the gesture is held (v1 behaviour of the grips is not changed). A grip held
by a holster (8.4) does not count as pressed here [proposal: one owner per
input at a time, 20 §5].

### 8.3 HUD toggle

**Gesture.** The left hand within 0.20 m of the head (the near-head
distance of 04 §5.10, used for the left hand) and a press of the Map input
(04 §5.8.1; the left lower face button by default). While the left hand is
near the head and this gesture is on, the Map input drives the HUD toggle
and not the map. [proposal: a deliberate "touch the side of the helmet"
movement. The near-head distance is the one v1 already uses for the
flashlight (04 §5.10), so the player learns one distance. The Map input is
chosen because it is on the left hand, which is free when the right holds
a weapon, and because opening the map is never wanted with the hand
raised to the head, so taking it there costs nothing; the map is never
opened by accident because Map does one thing at a time.]

**Result.** Each gesture flips a "HUD hidden by the player" state, which
starts visible each session:

- hidden: the `weaponstuff` and `pickupOverlay` HUD movies are hidden
  through the game's own methods (2.4: `WeaponHudHide`, `PickupHudHide`,
  each on the object `UiFindObject` returns for its name, only when the
  object exists), on the game thread;
- visible: they are shown again through `WeaponHudShow` and
  `PickupHudShow`, **unless the game itself hides the HUD at that moment**
  (the game's own hide, for example the one at 0x1094A0, leaves the +0x40
  byte set and the mod does not override it).

Required: while the player's state is hidden, the HUD stays hidden even
when the game shows it again for its own reasons (a cutscene ending): the
mod re-applies the hide on the next update in which it finds the object's
+0x40 byte at 0. When the player shows the HUD, the mod does not show a HUD
the game is hiding for its own purpose: it restores visibility only if it
was the mod that hid it. Interaction prompts and objective messages are in
these movies, so hiding the HUD hides them too; the item's description on
the options page says so (12.2). The virtual screen and menus are not
affected.

Before first use the six sites of 2.4 are verified (prologue bytes, the
`push` operands pointing at the named strings). A mismatch turns the HUD
toggle off and is logged.

The game's own hide is told apart from the mod's [r10, from 23 §2.7 r6]:
`+0x40` is set by the same methods whichever side calls them, so the mod
notes the game's intent where the game expresses it, in the scripted HUD
entity's four methods (23 §2.7: each is wrapped in its method-table slot,
records "the game hides" or "the game shows", then runs as before). The
mod shows a movie only when it hid it and the game's last intent is not
"hide". The 2.4 lookup is made only while the game's UI-ready byte (23
§2.7) is set.

### 8.4 Holster gestures

**Gesture** [proposal]. A grip press that begins while the hand is in one
of these zones (head frame, metres):

| Zone | Where | Drives (map 0 action) |
|---|---|---|
| right shoulder | right hand, above the head's height minus 0.25, behind the head (z above 0.05) | `next_main_weapon` (0x1B) |
| left shoulder | left hand, same zone mirrored | `prev_main_weapon` (0x1A) |
| left hip | left hand, below the head's height minus 0.60, left of centre by more than 0.18 (x below -0.18), not more than 0.10 in front of the head (z above -0.10) | `select_torch` (0x1D) |
| chest | either hand, between 0.20 and 0.45 below the head's height, within 0.15 of centre, from 0.20 in front of the head to 0.05 behind it (z from -0.20 to 0.05) | `select_motion_tracker` (0x1C) |

[proposal: reaching over the shoulder for a weapon and to the hip for the
torch are familiar VR conventions; the zones do not overlap each other or
the near-head zone of 8.3; these are starting values to tune in play.]

[r11, project decision from a tester's log (OpenXR, Index controllers):
the hands' ordinary hold in play is about 0.3 to 0.4 m in front of the
head and 0.5 m below it, and hands rest in the lap about 0.6 m below it.
The chest zone of r10 had no limit in front, so that hold lay in it and
nearly every grip selected the tracker instead of aiming. The chest zone
is now the space against the chest only; the hip zone starts lower and
further left and ends at the front of the body. Required: a grip press
with the hands at that hold, in the lap, or raised to aim begins in no
zone, also with the head pitched up to 30 degrees down (the head frame
turns with the yaw only, 8.1, so pitching the head moves a hand's
position in it).]

**Result.** The action is pressed for as long as the grip is held, as if
its button were held; when the grip is released it is released. A grip
press consumed by a holster does not also drive Aim or the tracker (and is
not counted by `Input.ToggleGrips`) until it is released [proposal: one
owner per input at a time, 20 §5]. Grip presses that begin outside every
zone behave as v1.

Holsters are off during interactions and in mode 1: a grip press that
begins in a zone then behaves as v1. When an interaction or a menu begins
while a holster holds its action, the action is released and the grip
stays the holster's until it is released [proposal: the action lets go at
once, and the grip does not turn into Aim or the tracker under the
player's hand].

### 8.5 Choices against alternatives [r12]

Alternatives that were considered for the features of this document, and
why this document keeps its own (project decisions; each reason is a
player-visible effect):

| Feature | Alternative | This document | Reason |
|---|---|---|---|
| Two-handed recenter (8.2) | both grips pressed with both hands within 0.25 m of the head, firing at once | within 0.30 m, held 0.75 s | a press that fires at once also fires while a player aims with the tracker raised near the face; the hold costs under a second once, and 0.30 m is easier to reach with a headset's strap in the way |
| HUD toggle (8.3) | the left upper face button near the head | the Map input near the head | the upper face button is v1's flashlight (04 §5.3); Map has no use with the hand at the head |
| Flashlight near the head | Interact near the head | already v1 (04 §5.10) | the same |
| D-pad (7) | the left grip held as a modifier with the left stick | the left stick during interactions and menus, else Sprint's click held 0.3 s | the left grip is the tracker (04 §5.3); taking it would stop the tracker being raised while walking |
| Smooth turn (4.2) | a fixed step per frame | degrees per second from a monotonic clock | a per-frame step turns faster at a higher frame rate and slows in a heavy scene |
| Snap repeat (4.3) | 500 ms | 300 ms (v1) | 05 §3.2c's reason holds; 150 to 1000 ms is a setting |
| Turning in interactions (4.4) | blocked always | a setting, default v1 | v1's behaviour is the default (20 §1.1) |
| Lean while hiding (9.2) | writing the pad's raw left stick values | the movement actions through the virtual pad | the same effect in the game (the game's tutorial names the move stick, 2.3); one route for every VR input keeps one owner per input (20 §5) |
| Button layout (04 §5.3) | interact A, reload B, map X, pause Y | v1's layout (Y flashlight, pause on the menu button) | v1's players know it; the runtime's binding UI lets a player move any function |

## 9. Hiding: lean and hold breath

### 9.1 State

The interaction state of 11 §5.3 gains one value, **breathing game**
(bool): set by `Start_Breathing_Game` and cleared by `End_Breathing_Game`
(2.3), both received through the event tap; also cleared by
`hiding_visibility_finish` and at every loading screen. This state is kept
whenever the tap is installed, so its log (11 §4.3) can confirm the
bracket in play before 9.3 relies on it.

### 9.2 Lean while hiding

**What the player experiences.** In a hiding place, physically leaning
(back into a locker, sideways or up from behind cover) makes the game's
character lean the same way, so the game's own rules about being seen
follow the player's real posture.

**Required result** (`Controls.HidingLean`). While hiding (11 §5.3) and
not in a menu: the head's offset from where it was when hiding began
(head frame of that moment, yaw only), when it exceeds 0.08 m along an
axis, produces the game's lean input in that direction; below 0.05 m it
produces none [proposal: a deliberate lean is well beyond 8 cm, a breath or
a nod stays within 5 cm; hysteresis avoids chatter]. The amount is
proportional up to 0.25 m, then full.

**Which game input leans.** The movement axes, map 0 analog 0 (`x_axis`)
and 1 (`y_axis`), as the game's tutorial says (2.3) [likely; acceptance 18
confirms it in play]. The head-frame offset drives them so [proposal: the
stick has two axes, and the game's own text makes "down" the lean back]:

- sideways (x) drives `x_axis`, right positive;
- forward and back (-z) drives `y_axis`, forward positive, so leaning back
  is the stick held down;
- a rise (+y) drives `y_axis` forward as well, the larger of the two
  applying, so rising from behind cover peeks out; a duck drives nothing
  (sitting lower in a hiding place is not a lean).

Each of the three has its own hysteresis (the 0.08 m and 0.05 m above).
The reference is taken at the first controls write with a valid head pose
after hiding begins and is dropped when hiding ends; a menu or a missing
head pose gives no lean and keeps the reference.

While the lean drives the movement axes, the left stick's own movement
value is added and the sum clamped to [-1, 1] (04 §5.1).

### 9.3 Hold breath

**What the player experiences.** When the game asks the hiding player to
hold their breath (the breathing game), covering the mouth with a hand
holds the breath, as well as the game's own control.

**Required result** (`Controls.HoldBreathGesture`). While the breathing
game is in progress (9.1): the game's hold-breath input is pressed while
either hand is within 0.12 m of the mouth point, defined as 0.09 m below
and 0.07 m in front of the head position in the head frame [proposal: a
point close to the mouth of an average adult wearing a headset; 12 cm is
"hand over mouth", not "hand near the face"]. It is released when the hand
moves beyond 0.18 m. The game's own control keeps working.

**Which game input holds breath.** `aim` (map 0 digital 0x0A), as the
game's tutorial says (2.3) [likely; acceptance 19 confirms it in play]. The
gesture presses it in addition to the right grip's Aim (04 §5.8.1),
merged by OR, so the grip keeps holding the breath too. Not while a menu
is on the virtual screen (mode 1): the game is paused.

## 10. Per-controller button glyphs

v1 chooses one glyph set per run, at the first right-hand identification,
from two sets (touchpad, thumbstick; 04 §4.1, §5.5). With
`Controls.ControllerGlyphs` on:

### 10.1 Families and the rule

**Sets.** One glyph set per controller family, each image showing that
controller's own control (its shape and its label):

| Family | Identified by (OpenVR SteamVR Input: `Prop_ControllerType_String`; OpenXR: interaction profile) | Notes |
|---|---|---|
| Index | `knuckles`; `/interaction_profiles/valve/index_controller` | A / B on both hands, trackpads, thumbsticks |
| Touch (Quest, Rift) | `oculus_touch`; `/interaction_profiles/oculus/touch_controller` | A / B right, X / Y left, menu button |
| WMR | `holographic_controller`; `/interaction_profiles/microsoft/motion_controller` | touchpad halves for the face-button roles (04 §5.4) |
| Vive wand | `vive_controller`; `/interaction_profiles/htc/vive_controller` | the touchpad set of 04 §4.1 |
| Steam Frame | `frame_controller`; `/interaction_profiles/valve/frame_controller_valve` | A / B right, View button, bumper and d-pad left, Menu right (04 §5.8.2) |
| generic | anything else, or `khr/simple_controller` | the thumbstick set of 04 §4.1 (v1 images) |

The glyph for a prompt is the one of the control that drives the
prompted function under the **default binding of that family** (04 §5.8.2
and §5.8.4 tables), per runtime. Under OpenXR the Index pause is the left
trackpad force (04 §5.8.4); under SteamVR Input it is the left trackpad
click; both show the trackpad press glyph of 10.2.

**Per hand.** Controls of the left hand use the left controller's family,
controls of the right hand the right one's [proposal: mixed controllers are
rare, but the rule costs nothing and is never wrong]. Where two families
put a function on different hands (Pause: the left hand on Index and
Touch, the right on the wand and the Frame), the left controller decides
every function its own default binding puts on the left hand, and the
right controller every other one [proposal: each controller drives the
game through its own binding, so the control on the left controller is the
one that works for a left-hand function]. A hand not identified yet counts
as holding the other hand's family; until either hand is identified, v1's
set applies (04 §5.5).

**Following changes.** The family is re-evaluated at every controller
identification (OpenVR: each change of the device in a hand role, 04
§5.5; OpenXR: each interaction profile change, at which both hands'
current profiles are read). `Controls.ControllerGlyphs` itself is read at
each identification (3.1): switched off, v1's set and rows return at the
next one. Each change of a family while the key is on is logged once,
with both hands' families and the runtime's input path (acceptance 12).
When a family changes, the
glyph names change on the render thread as v1 requires (04 §3.6: a complete
name, never a mix) and the prompts on screen redraw by the mechanism of 04
§4.2, without the player touching another device. The controls help page
(04 §5.7) is pushed with that family's rows (10.3) at the next push.

**d-pad and v2 inputs.** With d-pad emulation on, d-pad codes get the
emulated control's glyph (7), whatever `Controls.ControllerGlyphs` says:
with it off, from v1's set, the left stick's directions for the
thumbstick set and the left touchpad's quarters for the touchpad set
(10.2's d-pad row). A change of `Controls.DpadEmulation` changes these
glyphs at once, with the redraw above. A controller with a d-pad of its own
(the Steam Frame, 04 §5.8.1) keeps the game's d-pad glyphs, with or without
the emulation [proposal: the stock d-pad glyph already shows the control
that drives these functions there]. With holsters, gestures and hiding features,
no prompt changes (the game has no prompt for them); the controls help
lists them (10.3).

### 10.2 Designs

Every glyph keeps v1's look (04 §4.1, the look players already know):
32 x 32 pixels with transparency, the size and style of the game's own
Xbox glyphs, no colour; round glyphs near-white (about 224 grey), grip and
stick glyphs light grey (about 192 grey) with black letters. The designs:

| Design | What it shows |
|---|---|
| face button `A`, `B`, `X`, `Y` | v1's design: a white ring (a button outline) with the white capital letter inside |
| menu button | v1's design: a white ring with three thick horizontal white bars |
| left grip, right grip | v1's designs: the light-grey tab with a black `LG` (rounded on its left) or `RG` (rounded on its right) |
| right stick click | v1's design: a light-grey thumbstick seen from the side (a cap on a stem) with a black `R`, and a grey triangle pointing down above it |
| left stick up, down, left, right | a light-grey disc (a thumbstick seen from above) with a black `L` at its centre and a grey triangle on its edge pointing outward in that direction [proposal: seen from above, unlike the click design, so a direction is never read as a click] |
| touchpad centre, upper half, lower half | v1's designs: a thin white ring with a white dot at its centre, or with its top or bottom quarter filled white as a wedge whose point reaches the centre |
| touchpad left, right quarter | the same ring with its left or right quarter filled [proposal: the v1 wedge style for the two directions v1 did not need] |
| touchpad anywhere | the thin white ring filled white over its whole area [proposal: a press anywhere on the pad, unlike every region design] |
| trackpad press (Index) | an upright white rounded oblong, the shape of the Index trackpad, filled white [proposal: the Index pad is oblong, not round, and the press is anywhere on it] |
| left bumper (Steam Frame) | a light-grey tab like the left grip's but lower, rounded on its left, with a black `LB` [proposal: the bumper is a small button above the trigger, and the player sees no stock `LB` glyph while VR glyphs show, 04 §4.1] |
| View button (Steam Frame) | a white ring with two overlapping white rectangle outlines, the front one lower and to the right [proposal: the common "view" icon, unlike the menu button's bars] |
| unbound | 04 §4.1's design |

**Hand mark** [proposal]. Where one family uses the same design for a
control of each hand (Index's `A` and `B`, WMR's touchpad regions, the
wand's two menu buttons and two touchpads), each of those glyphs adds a
short horizontal white bar under the design, under its left half for the
left hand and under its right half for the right hand. Reason: a prompt
must never show a control the player cannot tell from the same control on
the other hand; a bar reads at 32 pixels where a letter `L` or `R` would
not fit beside a letter already in the ring.

Which design each prompt shows, by the stock pad control the game asks
about (04 §4.1):

| Stock pad control (function) | Index | Touch | WMR, SteamVR Input | WMR, OpenXR | Vive wand | Steam Frame | generic |
|---|---|---|---|---|---|---|---|
| A (interact) | `A`, right | `A` | touchpad lower half, right | touchpad anywhere, right | touchpad lower half, right | `A` | as v1 (04 §4.1, thumbstick set) |
| B (radial menu) | right stick click | right stick click | right stick click | right stick click | touchpad centre, right | right stick click | as v1 |
| X (reload / activate) | `B`, right | `B` | touchpad upper half, right | unbound (no reload control, 04 §5.4) | touchpad upper half, right | `B` | as v1 |
| Y (flashlight) | `B`, left | `Y` | touchpad upper half, left | the game's own name (only interact near the head, as 04 §4.1 does for wands) | the game's own name (04 §4.1) | left bumper | as v1 |
| BACK (map) | `A`, left | `X` | touchpad lower half, left | touchpad anywhere, left | menu button, left | View button | as v1 |
| START (pause) | trackpad press | menu button | menu button | menu button | menu button, right | menu button | as v1 |
| LB (motion tracker) | left grip | left grip | left grip | left grip | left grip | left grip | left grip |
| RB (aim) | right grip | right grip | right grip | right grip | right grip | right grip | right grip |
| d-pad up, down, left, right (only with 7 on) | left stick in that direction | the same | the same | the same | touchpad quarter in that direction, left | the game's own name (its own d-pad, 10.1) | left stick in that direction |
| every other pad code | the game's own name (04 §4.1) | | | | | | |

"right" and "left" in a cell are the hand marks above. Under OpenVR without SteamVR
Input (legacy, 04 §5.8.3) every controller is driven as a wand and shows
the wand column and rows.

**Resources.** 14 gains these images in the stock glyph format (14 §2.9),
drawn by this project from the designs above, and registers their input
ids in the 19 movies (14 §3.4), whatever `Controls.ControllerGlyphs` says
(the archive is linked at launch, the key applies at an identification).
Ids and file names are the implementation's choice under 04 §4.1's
constraints. When the patched archive is not in use, stock glyphs stay
(04 §4.2).

### 10.3 Controls help rows

The `View Controls` page (04 §5.7) keeps v1's row format: the function on
the left, the control on the right, exact text and order.

**Rows per family** (with `Controls.ControllerGlyphs` on; the list follows
the right hand's family [proposal: the page holds one list, and most rows
name right-hand controls]):

- Touch and generic: v1's thumbstick rows (04 §5.7).
- Vive wand: v1's touchpad rows (04 §5.7).
- Index: v1's thumbstick rows with these control texts, so that they name
  the buttons the Index shows: row 8 `Interact` | `Right A`; row 10
  `Reload / Activate` | `Right B`; row 11 `Flashlight` | `Left B`; row 12
  `Flashlight [Press Interact near Head]` | `Right A`; row 15 `Map Menu` |
  `Left A`; row 16 `Pause Menu` | `Left Trackpad Press` (as v1's Index
  variant).
- WMR, SteamVR Input: v1's thumbstick rows with row 8 `Interact` |
  `Right Touchpad Down`; row 10 `Reload / Activate` | `Right Touchpad Up`;
  row 11 `Flashlight` | `Left Touchpad Up`; row 12
  `Flashlight [Press Interact near Head]` | `Right Touchpad Down`; row 15
  `Map Menu` | `Left Touchpad Down`.
- WMR, OpenXR: v1's thumbstick rows with row 8 `Interact` |
  `Right Touchpad Click`; rows 10 and 11 left out (no such control, 04
  §5.4); row 12 `Flashlight [Press Interact near Head]` |
  `Right Touchpad Click`; row 15 `Map Menu` | `Left Touchpad Click`.
- Steam Frame: the Frame variant of 04 §5.7 (its `Left Bumper`,
  `View Button` and `Left D-Pad` rows), which already names the Frame's
  own buttons.
- Under OpenVR without SteamVR Input (legacy): the wand's rows, as the
  glyphs (10.2).

The control texts follow v1's styles: `Left` / `Right` written out in the
thumbstick families' rows, `L` / `R` in the wand's.

**Rows of v2 features** [proposal], whatever `Controls.ControllerGlyphs`
says: appended after the set's rows, in this order, each only while its
key is on (3.1). The function text follows v1's
`Flashlight [Press Interact near Head]` pattern (the condition in
brackets). The control text is given for the thumbstick style; the wand
style is in brackets.

| Key | Function | Control |
|---|---|---|
| `Controls.DpadEmulation` | `D-Pad` | `Hold Left Thumbstick Click + Left Thumbstick` (wand: `L Touchpad Edge Click`) |
| `Controls.RecenterGesture` | `Recenter [Both Hands near Head]` | `Hold Both Grips` |
| `Controls.HudToggleGesture` | `Hide/Show HUD [Left Hand near Head]` | the control text of the list's `Map Menu` row |
| `Controls.HolsterGestures` | `Next Weapon [Grip over Right Shoulder]` | `Right Grip` (wand: `R Grip`) |
| | `Previous Weapon [Grip over Left Shoulder]` | `Left Grip` (wand: `L Grip`) |
| | `Flashlight [Grip at Left Hip]` | `Left Grip` (wand: `L Grip`) |
| | `Motion Tracker [Grip at Chest]` | `Left or Right Grip` (wand: `L or R Grip`) |
| `Controls.HidingLean` | `Lean [While Hiding]` | `Lean Your Head` |
| `Controls.HoldBreathGesture` | `Hold Breath [While Hiding]` | `Hand over Mouth` |

**With the key off**, v1's choice of glyphs and rows applies exactly; the
d-pad glyphs of 7 (10.1) and the rows of v2 features still apply while
their keys are in effect.

## 11. OpenXR as the default runtime

This changes a v1 default (06 §1, ANSWERS r4) and is a **user decision**
pending (20 §9). Required when it is made:

- A **fresh** `mothervr.json` (first launch, 06 §3.1) is written with
  `VRRuntime` `"OpenXR"`, and a file without a `VRRuntime` key (or with an
  unknown value) means OpenXR.
- **Existing files keep their value.** A file that says `"OpenVR"`,
  `"OpenXR"`, `"Oculus"` or `"None"` is read as in v1 (06 §3.3); nothing
  rewrites it at start-up (06 §3.1). Every file v1 wrote holds the key, so
  an upgrade keeps the player's runtime. [proposal: changing a runtime the
  player chose would break a working setup; new players get the new
  default.]
- **Fallback** when the preferred runtime is not available and not forced
  (06 §1.1): the other runtime if available, OpenXR before OpenVR when
  neither was named (a fresh config). The table of 06 §1.1 is otherwise
  unchanged.
- The **welcome message** (13 §5.3; when and how it is shown are 13's and
  do not change) keeps the v1 box (caption `MotherVR`, information icon, OK
  only) and its first line, and its second paragraph says that the default
  is OpenXR, that SteamVR and other runtimes work through it, and how to
  choose SteamVR's own interface on the options page. Its text (quoted as
  03 §6.4 quotes box texts; `MotherVR` is plain text in the box):

  > Welcome to `MotherVR` for Alien: Isolation!
  >
  > VR starts with OpenXR, which works with SteamVR, the Meta runtime and other OpenXR runtimes. To use SteamVR directly or turn VR off, go to Options->`MotherVR` after the game starts up. This message is only shown on the first launch of `MotherVR`.

  It is v1's welcome (13 §5.3) with the runtime names of the second
  paragraph's first two sentences swapped and a clause on what OpenXR
  works with, because the default it names changed; the rest is unchanged.
  "Use SteamVR directly" points to the `Use SteamVR` button on the
  `VR Runtime` page (06 §5.2).
- What changes for SteamVR players on OpenXR: bindings are edited in
  SteamVR's OpenXR binding UI instead of the SteamVR Input files, and the
  `mothervr_input` folder is not written while OpenXR runs (04 §5.8.2
  applies to OpenVR only).
- Logging: the runtime decision event (11 §2.3) says when the default, not
  the file, chose OpenXR.

Before the decision: v1's default (OpenVR) stays.

## 12. v2 settings in the headset

### 12.1 Where they are and how they look

The `MotherVR` page (06 §5, 00 §2.6) gains seven sub-page entries, after
`Input` and before `Dev. Debug Options`, each with no description line,
like v1's other sub-page entries [proposal: they sit with the other
sub-pages and leave every v1 item in its place and order]. There is no
`Enhanced Mode` entry or toggle: each setting applies on its own (20
§1.3). The entries are shown in the front end and the pause menu, wherever
the `MotherVR` page is (06 §5.1). In order:

| # | Label | Kind | Values shown | Key | Description line |
|---|---|---|---|---|---|
| 1 | `Comfort` | sub-page entry | | | none |
| 2 | `Controls` | sub-page entry | | | none |
| 3 | `Hands` | sub-page entry (21 §15.3) | | | none |
| 4 | `Body` | sub-page entry (21 §15.3) | | | none |
| 5 | `Interaction` | sub-page entry (23 §8) | | | none |
| 6 | `HUD` | sub-page entry (23 §8) | | | none |
| 7 | `Graphics` | sub-page entry (24 §10) | | | none |

One sub-page per settings section of `mothervr.json` (20 §5) [proposal:
the player finds a setting where the file keeps it, and no page grows
longer than the stock Video page]. This answers 20 §10 item 6.

Rules for every v2 page (this document's and those of 21, 23 and 24):

- **Look.** The pages are the game's standard vertical menu lists in the
  same frame as the stock and v1 pages, with no new layout, headers or
  dividers, and every control is one of the game's own item kinds (toggle,
  list, slider, button, sub-page entry), as v1's page is (00 §2.6). Each
  page's title is the label of the entry that opened it.
- **Values.** A list shows the exact value names given. A slider shows the
  number beside the bar as v1's `Pixel Scale` does: a plain decimal with no
  unit and no trailing zeros (`0.5`, `1`, `1.25`), rounded to the slider's
  step; the unit is in the description line. Steps apply from the
  control's first use (06 §5.2). A stored value that is not one of a
  list's values or a slider's steps (written by hand in the file, or
  changed by sanitising, 3.1) is shown as the nearest one and is not
  rewritten until the player changes the control.
- **Description lines.** Every v2 item has the description line given
  [proposal: the settings are new to every player, and several depend on
  another setting; v1's page had descriptions only on the runtime
  buttons]. None is longer than 93 characters, the length of the longest
  description in the stock English menu text (the audio language item:
  `Change audio language for the game. Changes will be applied after
  loading the next new level.`) [sure for the stock text, read from
  `DATA\TEXT\ENGLISH\UI.TXT`], so each fits as the game's own longest one
  does. An item whose change applies at the next launch starts with
  `Requires restart!` as v1's runtime buttons do.
- **Greying.** The seven v2 sub-page entries are greyed while VR is not
  running, like `Rendering` and `Input` (06 §5.3). An item that has
  effect only with another setting is greyed while that setting is off,
  as each table says; its value is kept.
- **Hidden items.** An item for a feature that is reserved, or not yet
  confirmed by the user where its document says so (24 §10), is not
  shown; an item whose game input or site is not yet confirmed (9.2, 9.3,
  23 §5.2) is shown greyed with the description `Not available yet`
  [proposal: the player sees that the feature exists and that it is not a
  setting they got wrong].

### 12.2 The `Comfort` and `Controls` pages

`Comfort`, in order:

| # | Label | Kind | Values shown | Key | Description line |
|---|---|---|---|---|---|
| 1 | `Smooth Turn Speed` | list | `Off`, `30°/s`, `45°/s`, ... `360°/s` (steps of 15) | `Comfort.SmoothTurnSpeed` | `Needs Snap Rotation off. Controller Sensitivity applies only when this is Off` |
| 2 | `Snap Rotation Repeat` | list | `Off`, `150 ms`, `200 ms`, ... `1000 ms` (steps of 50) | `Comfort.SnapTurnRepeatMs` | `How often a held stick repeats a snap turn. Off: one turn per push` |
| 3 | `Snap Rotation Fade` | toggle | on / off | `Comfort.SnapTurnFade` | `Hides each snap turn behind a short fade from black` |
| 4 | `Comfort Vignette` | list | `Off`, `10%`, `20%`, ... `100%` | `Comfort.Vignette` (0.1 per 10 %) | `Darkens the edge of the view while you move or turn with the stick` |
| 5 | `Play Mode` | list | `Seated`, `Standing` | `Comfort.PlayMode` | `Standing: your real height sets the view height. Applies at the next recenter` |
| 6 | `Calibrate Standing Height` | button | | (6.2) | changes with the calibration, below |
| 7 | `World Scale` | slider | 0.5 to 1.5 in steps of 0.05 | `Comfort.WorldScale` | `Above 1 the world looks smaller, below 1 larger` |

The labels `Snap Rotation Repeat` and `Snap Rotation Fade` extend v1's
`Snap Rotation` item, so the player connects them with it. The degree
sign is U+00B0, as in v1's `Snap Rotation Segments` values. With
`Comfort.SnapTurnRepeatMs` at a value written by hand between steps, the
list shows the nearest step (12.1).

`Calibrate Standing Height`'s description line shows the calibration's
state (6.2) [proposal: the page is where the player pressed the button,
so the result is shown there, without a message box]:

| State | Description line |
|---|---|
| never calibrated | `Stand up straight, select, and keep still` |
| counting down | `Measuring in 3`, `Measuring in 2`, `Measuring in 1` |
| waiting for a valid head pose (6.2) | `Waiting for head tracking` |
| calibrated (now or before) | `Standing height: 1.72 m. Select to measure again` (the stored height, two decimals) |
| not taken: outside 1.0 to 2.2 m | `Not measured: height outside 1.00 to 2.20 m` |
| not taken: no floor reference (6.1) | `Not measured: no floor height on this runtime` |

While `-vrscale` sets the scale for this run (6.3), `World Scale` is
greyed, its slider shows the switch's value, and its description is
`Set by -vrscale on the command line for this session`.

`Controls`, in order:

| # | Label | Kind | Values shown | Key | Description line |
|---|---|---|---|---|---|
| 1 | `No Turning During Interactions` | toggle | on / off | `Controls.InteractionAwareTurning` | `The stick does not turn you at keypads, terminals, levers and other interactions` |
| 2 | `D-Pad Emulation` | toggle | on / off | `Controls.DpadEmulation` | `The left stick is the D-pad in menus and interactions, or while you hold its click` |
| 3 | `Recenter Gesture` | toggle | on / off | `Controls.RecenterGesture` | `Hold both grips with both hands at your face to recenter` |
| 4 | `HUD Toggle Gesture` | toggle | on / off | `Controls.HudToggleGesture` | `Left hand at your head and Map Menu hides or shows the HUD, prompts included` |
| 5 | `Holster Gestures` | toggle | on / off | `Controls.HolsterGestures` | `Grip over a shoulder: weapons. Left hip: flashlight. Chest: motion tracker` |
| 6 | `Lean While Hiding` | toggle | on / off | `Controls.HidingLean` | `Lean in a hiding place by leaning your head` |
| 7 | `Hold Breath Gesture` | toggle | on / off | `Controls.HoldBreathGesture` | `Cover your mouth with a hand to hold your breath while hiding` |
| 8 | `Per-Controller Glyphs` | toggle | on / off | `Controls.ControllerGlyphs` | `Button prompts show the buttons of the controllers you are holding` |
| 9 | `VR Off Hotkey` | toggle | on / off | `Controls.VrOffHotkey` | `Hold Right Ctrl + Right Shift + F10 for 1 second to turn VR off until restart` |
| 10 | `Crash-Safe Log` | toggle | on / off | `Controls.CrashSafeLog` | `Requires restart! Writes the log to disk at once, so it survives a system crash` |

`Map Menu` in item 4's description is the controls help's name for the
Map input (04 §5.7), so the player finds the button there.

### 12.3 State rules

State rules as 06 §5.3: with VR not running the v2 controls are disabled
(12.1); values pushed never send a command back; steps apply from first use
(06 §5.2). The calibration and the HUD state are not settings and are not
pushed except the stored height; the HUD state's value at session start
comes from 23's `Hud.Hidden` (23 §7.2). The page's v2 additions are part of
the menu change set of 14 (14 §3.1); where they go in the page map follows
06 §7.3 (new pages and items created after every stock one); the order in
which the player sees them is the order of the tables above.

Every key of 3.1 is reachable in the headset through these pages.

## 13. VR-off hotkey

**What the player experiences.** A key combination turns VR off for the
rest of the session without quitting: the headset stops showing the game
and the game continues flat on the monitor.

**Key** [proposal]: right Ctrl + right Shift + F10, held together for 1 s,
read with the game's keyboard key test (04 §2.2: DirectInput scan codes
0x9D right Ctrl, 0x36 right Shift, 0x44 F10) on the game thread while the
keyboard and mouse devices exist. [proposal: right-hand modifiers avoid
the Ctrl+Alt recenter chord (04 §8) and the game's own left-hand keys; F10
has no game binding in `DATA\INPUT.XML`; the hold avoids accidents.]

**Result** when it fires (only while the stereo device runs):

- the device is detached as after a late runtime failure (02 §7.4): state 0,
  manager +0x04 null, then the runtime released; the options page shows VR
  not running; **no** failure message is shown (the player asked for it);
- the game runs flat from the next frame (02 §7.5);
- the render-target rebuild flag (06 §4.6) is raised once so the game's
  targets return to the window's size [proposal: the stereo-sized targets
  would otherwise stay until the next resize; one 0.2 s freeze at the
  moment the player chose];
- the swapchain stays windowed for the rest of the session (03 §9 is not
  undone) [proposal: changing the window mode mid-session is a second
  risk for no gain];
- VR cannot be turned on again in this session (the runtime is started
  once per process, 03 §6.5). The page shows it as v1 shows a runtime that
  stopped during the session (06 §5.2): `VR Runtime Set` shows `None`,
  and the button of the stored runtime is visible, so choosing it (which
  quits, 06 §4.5) starts VR again at the next launch; the item's
  description (12.2) already says that VR stays off until a restart;
- logged once (16).

With OpenXR the session is ended in the order the API requires
(`xrRequestExitSession`, then on `XR_SESSION_STATE_STOPPING` `xrEndSession`)
before the instance is destroyed; the device is detached at the request.
v2 features stop with the device.

## 14. Crash-safe logging

v1 (11 §1.2) already writes each event with one `WriteFile` to a handle
opened once, with no buffering in the process; such data is in the
operating system's cache and survives a crash **of the process**, and v1
records a crash of the process in the log itself (11 §6.1). What is still
lost is what the cache had not written to disk when the whole system
crashes (a driver fault under GPU load), and v1's crash record says where
the crash happened but not what state the mod was in. With
`Controls.CrashSafeLog` on (read at attach, so it applies from the next
launch):

**Flush policy.**

- The log file is flushed to disk (`FlushFileBuffers`) after every `WRN`,
  `ERR` or `FTL` event written on the game thread or the loader thread, and
  at most once per second after other events, by a thread of the mod's own
  (thread name `mod`, 11 §1.4) that waits on a timer and an event.
- The render thread and job threads never flush and never wait for a
  flush: their events are written as in v1 and flushed by the mod's thread
  within a second. (12 §6.1: no file I/O beyond v1's single write per
  event, and only per state change.)
- The flush thread is created after attach (never in `DllMain`, 12 §2.3),
  holds no lock the render thread takes, and is not joined at exit (12 §7:
  at process exit the mod only logs the detach line).

**Crash event.** v1's crash record (11 §6.1) is unchanged. With the key
on, the same crash also produces one further event that conveys the last
v2 and v1 states that help to place the crash (device state, stereo mode,
interaction kind, whether a menu was open) and the thread name (11 §1.4).
By the time the exception leaves the mod's handling, this event and v1's
record are flushed to disk, because the process may end as soon as the
exception is passed on. The event is rendered without allocation and
written with one `WriteFile`, like every event. Only unhandled exceptions
produce it, never a first-chance exception the game handles itself
[proposal: only unhandled exceptions are crashes].

**With the key off**, logging is v1's exactly.

## 15. Threads and data crossing

| Data | Written by | Read by | Requirement (12 §3) |
|---|---|---|---|
| v2 settings | the command handler (game thread) | game, render threads | each a single word, atomic (06 §3.4) |
| turn value, movement value | pad replacements and the stereo update (game thread) | game thread | as v1 |
| smooth-turn, snap and turning-block decisions, yaw writes | game thread (stereo update) | game thread | as v1 (05 §13) |
| vignette target strength, snap-fade trigger (a step counter) | game thread | render thread (slot 6) | atomic 32-bit values |
| gesture results (recenter request, HUD toggle request, holster action state) | render thread (controls write) | game thread | atomic flags and per-action values, as the VR state of 04 §3.6 |
| d-pad VR state | render thread (controls write) | game thread (pad replacements) | per-action values as 04 §3.6 |
| breathing-game state | the tap's thread (11 §8) | any | atomic, as 11 §5.3 |
| standing height, play mode, world scale | game thread | render thread (head and hand conversion, 03 §11) | atomic; a change applies from the next frame whole (a frame never mixes two scales) |
| VR-off request | game thread | render thread | atomic; the device detach follows 02 §7.4's order |
| HUD hide and show calls into the game | game thread only (`UiFindObject` takes the UI critical section) | | never from the render thread |

No allocation, file I/O, locking or logging per frame on the render thread
(12 §6.1). The vignette pass and the fade allocate their resources when the
target size changes.

## 16. Logging

11 owns the wording and the format; a later revision of 11 adds these
events. Each is logged once when it happens or changes (11 §3):

| Level | Area | When | Must convey |
|---|---|---|---|
| INF | Config | session start (11 §1.7) | the v2 keys and their values in the settings event |
| INF | Input | the turning in use changes (snap, game look, smooth) | which; the speed or repeat interval |
| DBG | Input | turning blocked or unblocked by 4.4 (with `DebugOptions`) | why (interaction kind, breathing game, cutscene) |
| DBG | Input | the VR d-pad press of 7 changes (with `DebugOptions`) | each direction pressed or not; whether the emulation pressed it |
| INF | Stereo | play mode or calibration applied | the mode; the height taken, or why none was taken (invalid pose, out of range, no floor reference) |
| WRN | Stereo | standing mode unavailable on this runtime | the runtime; that play is seated |
| INF | Stereo | world scale changes | the new value; whether `-vrscale` overrides it |
| INF | Input | a gesture fired | which (recenter, HUD hidden or shown, holster and its action, the hiding lean starting and ending, the breath held and released) |
| WRN | UI | the HUD toggle cannot be installed | the site that failed its check (12 §4) |
| INF | Input | the controller family for a hand changes (with `Controls.ControllerGlyphs`) | the hand; the family; the glyph set chosen |
| INF | Diagnostics | the breathing game starts and ends (tap) | the state, as 11 §4.3 gated by `DebugOptions` |
| INF | Stereo | VR turned off by the hotkey | that it was the player's request; that a restart turns it on |
| ERR | Load | crash event (14) | the content of 14; the same level as v1's crash record (11 §6.1), since both describe one crash |

## 17. Edge cases

| Situation | Required behaviour |
|---|---|
| Menus, videos, loading (mode 1) | no turning, no vignette, no fade; d-pad emulation from the left stick is on (7); holsters off; HUD toggle still allowed (it changes no menu) |
| Cutscenes and level fades | FollowBehavior inactive: no turning (v1); vignette strength 0; the HUD state is kept and re-applied after (8.3) |
| Interactions | turning blocked with 4.4 on, and always while 23's lever mapping is active (4.1); d-pad from the left stick except during the lever mapping (7); holsters off |
| Hiding | lean and hold breath per 9 (when confirmed); turning as 4.4 |
| Death and reload | smooth-turn `dt` restarts at 0; vignette falls to 0; the HUD state is kept for the session [proposal: the player chose it]; the breathing-game state is cleared at the loading screen |
| Headset taken off | nothing changes; calibration waits for a pose while the headset is in use (6.2) |
| Controllers lost | gestures need valid hand poses (8.1); VR turn and movement read 0 (04 §5.1), so smooth turn and the vignette stop |
| Runtime focus lost (dashboard) | all VR input zero (04 §5.1); gestures blocked until 0.5 s after focus returns (8.1) |
| Runtime None, or VR failed | no v2 feature runs (20 §1.2); the page shows the v2 section greyed |
| OpenVR and OpenXR | every feature on both, except standing mode without a floor space on an OpenXR runtime (6.1) |
| Keyboard and mouse in use | mouse look as v1; smooth turn and snap still apply to the stick; the vignette ignores keyboard movement (5) |
| Display reset | vignette and fade resources are recreated with the target; settings unchanged |
| VR turned off by the hotkey | every v2 feature stops with the device; the game is flat |
| 2020 build | every feature of this document that needs a game address is off; settings are still read and kept |

## 18. Acceptance

On the 2015 build, headset worn, with OpenVR and with OpenXR unless noted.
"Log" is `mothervr.log`.

1. All keys of 3.1 at their defaults, or absent: every acceptance check
   of 04 and 05 passes unchanged, and after a menu change the file keeps
   the v2 values stored in it.
2. `SmoothTurnSpeed` 90 with snap off: holding the right stick fully turns
   a full circle in 4 s (±10 %), at 90 Hz and with the frame rate capped
   at 45 Hz alike; the stick at rest never drifts; Camera Speed at its
   minimum and maximum changes nothing. With speed 0 and snap off the turn
   is the game's own (v1).
3. `SnapTurnRepeatMs` 600: a held stick steps about every 0.6 s (half
   v1's rate); 0: one step per flick. With the fade on, no view jump is visible; the first
   frame after a step is black.
4. `Vignette` 1.0: walking with the stick narrows the view to a clear
   circle of about 25° around the centre of each eye; it clears within
   about 0.3 s of stopping; both eyes show the same ring (no doubled edge);
   menus never show it. With a performance overlay, the GPU time added
   while it is active is measured and stays under 0.2 ms at 2688 x 2688 per
   eye.
5. Standing mode, calibrated standing: ducking 30 cm lowers the view 30 cm
   in game; load a level while kneeling: after the loading screen the view
   is still low (not reset to standing). Seated: as v1. The log shows the
   height taken. On an OpenXR runtime without `STAGE` or `LOCAL_FLOOR` the
   log says standing is unavailable.
6. `WorldScale` 0.8 set in the menu during play: head motion is scaled at
   once without a jump; with `-vrscale 1.2` on the command line the page
   shows the override and 1.2 applies.
7. `InteractionAwareTurning` on: at a keypad and a terminal the right
   stick does not turn the player; after the interaction the next flick
   turns. Off: v1. With `DebugOptions` on, the log shows the block reasons.
8. `DpadEmulation` on: in a menu, the left stick moves the selection as the
   d-pad does on a pad; in play, holding the left stick click 0.3 s and
   pushing left / right cycles weapons; the movement stops while a d-pad
   direction is pressed, and a click shorter than 0.3 s still sprints.
   On a Steam Frame, with the key off, the left d-pad moves the selection
   in menus and cycles weapons in play.
9. Recenter gesture: both grips at the face for under 0.75 s does nothing;
   for 0.75 s recenters once (logged "gesture"); aiming with the tracker
   raised never recenters during 10 minutes of play.
10. HUD toggle: left hand at the helmet and the map button hides the HUD
    and does not open the map; again shows it; a cutscene ending does not
    bring a hidden HUD back.
11. Holsters: reaching over the right shoulder and squeezing selects the
    next weapon; the same squeeze with the hands held in front of the body
    as in play, or resting in the lap, aims or raises the tracker as v1
    and logs no holster.
12. `ControllerGlyphs` on: with Index, prompts show Index glyphs; switch to
    Touch controllers during play: the prompts redraw with Touch glyphs
    without pressing anything else, and the log shows the family change.
    With Index, the interact prompt shows the `A` ring with the right hand
    mark and the map prompt the `A` ring with the left one (10.2), and
    `View Controls` lists `Interact` | `Right A` and `Map Menu` | `Left A`
    (10.3). With the recenter gesture on, `View Controls` ends with
    `Recenter [Both Hands near Head]` | `Hold Both Grips`. With a Steam
    Frame, the flashlight prompt shows the bumper glyph, the map prompt
    the View glyph and weapon-cycling prompts the game's d-pad glyphs, and
    `View Controls` lists `Map Menu` | `View Button`. With the key off and
    `DpadEmulation` on, weapon-cycling prompts show the left stick
    direction glyphs.
13. OpenXR default (once decided): delete `mothervr.json`; the new file
    says `"OpenXR"` and OpenXR starts; a v1 file with `"OpenVR"` still
    starts OpenVR.
14. Every key of 3.1 can be changed on the options page in the headset,
    in the front end and the pause menu, and the change is saved. The v2
    sub-page entries on the `MotherVR` page and the `Comfort` and
    `Controls` pages show the labels, values, order and description lines
    of 12.1 and 12.2; with VR not running the sub-page entries are greyed;
    there is no `Enhanced Mode` entry; `Calibrate Standing Height` counts
    down `Measuring in 3` to `1` and then shows the height taken; with
    `-vrscale 1.2`, `World Scale` is greyed at `1.2`.
15. VR-off hotkey: holding the keys 1 s in gameplay leaves the game running
    flat on the monitor at window resolution after one short freeze; the
    headset shows the runtime's own environment; no message box; the log
    records it; pressing it again does nothing.
16. `CrashSafeLog` on, crash test build (11 §7 item 13): the log holds
    v1's crash record (module and offset matching Event Viewer) followed by
    the crash event of §14, and both are on disk after a forced system
    reset right after the crash; with a performance overlay no
    render-thread spike comes from flushing during 10 minutes of play.
17. With `DebugOptions` on, hide in a locker until the Alien searches it:
    the log shows `Start_Breathing_Game` and `End_Breathing_Game` around
    the breathing prompt (confirms 9.1).
18. `HidingLean` on, in a locker: leaning the head 15 cm sideways peeks
    that way and leaning back leans the character back, as the stick does;
    breathing and small nods lean nothing; the log shows the lean starting
    and ending (confirms 19 item 3).
19. `HoldBreathGesture` on, at the breathing prompt: a hand over the mouth
    holds the breath as the right grip does, and moving it away lets go;
    the log shows the breath held and released (confirms 19 item 4).

## 19. Open questions

1. **Action table index.** Confirm that the table at 0x124F120 gives each
   map's entry index (2.1). To resolve: at the first virtualisation, with
   `DebugOptions` on, log each action's index, name from the table and
   original pad code; the names must match the controls `DATA\INPUT.XML`
   binds (for example `use` on A).
2. **Which pad bindings the PC build loads.** `DATA\INPUT.XML`'s default
   gamepad device (2.2) and the stock layout of 04 §2.4 disagree (aim,
   tracker, crouch, peek). To resolve: the same log as item 1 shows each
   action's original pad code.
3. **The lean input while hiding** (9.2): [likely] the movement axes,
   from the game's tutorial text since r8 (2.3); acceptance 18 confirms
   it. If it fails, to resolve: with `DebugOptions`
   on, a real pad and the event tap (11 §5) installed, hide in a locker and
   lean with the pad as the game's prompt says; log, between
   `hiding_visibility_start` and `_finish`, every virtual code the game
   queries with a non-zero answer (IsDown, GetValue, GetAxis), once per
   code. The codes map to actions through item 1.
4. **The hold-breath input** (9.3): [likely] `aim`, from the game's
   tutorial text since r8 (2.3); acceptance 19 confirms it. If it fails,
   the same method between
   `Start_Breathing_Game` and `End_Breathing_Game`, pressing the control
   the breathing prompt shows.
5. **Screens that need the d-pad** (7): which interaction screens (keypads,
   terminals, rewiring, the hacking device) read map 1's `up` / `down` /
   `left` / `right` or any raw d-pad code. To resolve: the same query log
   during each interaction kind of 11 §5.3; and whether any game code calls
   the pad's IsDown with raw codes 0x04 to 0x07 (they answer false after
   virtualisation, 04 §3.3).
6. **Smooth-turn time base.** Whether the stereo update's float argument
   (05 §3.1) is the real time since the previous update (then it may
   replace the clock, 4.2), including during slow motion. To resolve: log
   it against the clock for 900 frames.
7. **Hiding and turning.** Whether the game itself limits the camera yaw in
   every hiding place (4.4), or snap and smooth turn can turn the player
   inside a locker. To resolve: turn in a locker, under a desk and in a
   cabinet with 4.4 off.
8. **The `reaction_game` HUD movie** defines `force_hud_hidden` too
   (2.4); which screen it is and whether the HUD toggle must include it.
9. **HUD state ownership.** Whether any game path other than the +0x40
   byte shows the HUD (for example the deferred re-apply through +0x129,
   or the `pickupOverlay` load at 0x40AE00 that resets +0x40 to visible,
   2.4); to resolve, toggle the HUD off, load a level, and check it stays
   hidden.
10. **2020 build.** Every RVA here; to resolve, find the action name table
    by its strings, `UiFindObject` by its call count, and the HUD methods by
    their string operands.

## 20. Changes to v1 documents

Made in 04 r10 (r12 of this document): map 0's look Y (`cam_vertical_rot`)
carries the VR look stick's Y while an interaction is in progress, and
the stick then does not cycle weapons (04 §5.8.5). It does not change 7
(the d-pad emulation takes the left stick, not the right) or 4.4 (turning
is the stick's X).

Not made here; to be applied in a later revision:

- `04-input.md` §2.4: name the map actions from the table of 2.1 (map 0
  0x27 / 0x28 `tab_left` / `tab_right`; map 1 1 `cancel`, 9 `craft_item`,
  0x0B `alt_2`, 0x0E `minigame_top`) and drop them from "kept for parity,
  function unconfirmed" in §2.4, §5.2, §5.3, §5.8.5 and §11; note the
  disagreement of the stock layout with `DATA\INPUT.XML` (19 item 2).
- `04-input.md` §5.5: the glyph set is re-chosen per hand and per change
  when `Controls.ControllerGlyphs` is on (10); §6: the repeat interval is
  `Comfort.SnapTurnRepeatMs`, the fade of 4.3, the turning block of 4.4
  and the block during 23's lever mapping (4.1); §7: the look byte rule of
  4.1; §5.8.1: none for the d-pad modifier, which is Sprint's control (7;
  04 r9 added the four d-pad inputs of a controller's own d-pad);
  §5.8.5: d-pad and holster VR state (7, 8.4) and the Map input consumed by
  the HUD toggle near the head (8.3); §5.10: the near-head test extended to
  the left hand for 8.3; §5.7: with `Controls.ControllerGlyphs` the rows
  follow the right hand's family, and the rows of v2 features are appended
  (10.3).
- `05-game-patches.md` §3.2c and §3.2d: turning selection and look gate of
  4.1; §3.2e: the gesture recenter trigger (8.2) and the recenter's
  vertical part in standing mode (6.1).
- `03-hmd-and-frames.md` parameter table and §1, §2.1, §5: world scale from
  `Comfort.WorldScale` applied live (6.3); the reference's vertical part in
  standing mode (6.1); §6.2: the vignette and fade pass in slot 6 before
  submission (5).
- `09-openxr.md` §5.2, §6: the vignette before the copy; `STAGE` or
  `LOCAL_FLOOR` located with the views for standing mode (6.1); the VR-off
  session end (13).
- `02-game-stereo-path.md` §7.4: a detach requested by the player, without
  a failure message (13).
- `06-options-and-config.md` §1, §3.2, §3.3, §4.2, §4.3, §5: the
  `Comfort` and `Controls` keys, their commands and pushes, the v2
  sub-page entries on the `MotherVR` page, after `Input` (12.1), and their
  pages (12); §1 and §1.1: `VRRuntime`'s default and
  fallback order once OpenXR is the default (11); §5.3: Camera Speed does
  not scale smooth turn (4.5).
- `11-diagnostics.md` §5.3: the breathing-game state (9.1) and its strings;
  §6.1: the v2 crash event, at `ERR` like v1's crash record and on disk
  with it (14), and the flush policy with `Controls.CrashSafeLog` (14); §2:
  the events of 16.
- `12-threads-and-robustness.md` §2.2 and §3.2: the mod's flush thread
  (14), the crossings of 15; §7: the VR-off hotkey.
- `13-distribution.md` §5.3: the welcome text with OpenXR as default
  (11), word for word; the player guide: the new settings, gestures and
  the hotkey.
- `14-ui-resources.md` §3.1: the v2 pages and items with their labels and
  description lines (12). (§2.9 and §3.4, the glyph images per controller
  family and for the emulated d-pad, were applied in 14 r6.)
- `ANSWERS.md`: record the user decisions this document leaves open
  (OpenXR default, the proposals marked here) and the user decision of
  2026-10-16 that there is no master switch (20 §1.3).
