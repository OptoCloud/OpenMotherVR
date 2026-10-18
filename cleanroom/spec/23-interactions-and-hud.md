<!-- Provenance: game analysis of AI.exe 2015 (UI entity message handler and the interaction names it passes to the script event tap; the Interaction entity's messages; RTTI class names of the game's interaction and camera entities; StealCamera and CameraResource methods, the camera director's behaviour lookup, the follow camera's steer, re-align and pitch-level methods, the camera manager's reset request; CMD_PlayAnimation's parameter names and its PlayerAnimDrivenView getter; the interaction icon update loop and its range float; UI_Icon's parameter getters and defaults; the HUD movies' forced-hidden methods and the crosshair update); the stock game data (DATA\TEXT\ENGLISH\UI.TXT prompt texts, DATA\GLOBAL\ANIMATION.PAK and the levels' COMMANDS.PAK names, DATA\INPUT.XML); the stock DATA\UI.PAK movies WEAPONSTUFF, PICKUPOVERLAY, TERMINALINTERACTION and TERMINALINTERACTIONATTRACT (text field records and their scripts), read with the spec side's own tools; v1 documents 04, 05, 06, 08, 11, 14 and 20. Game facts located with the help of an analysis of another mod and verified in the 2015 `AI.exe` by the spec side; requirements are proposals of this project, pending user confirmation where marked. The lever, junction and terminal-text facts were verified by the spec side in the game's data; the existence of a terminal font change and of lever input emulation was known from the analysis of another mod. The camera takeover sites of 2.4 were located with the help of that analysis and verified. 2020 values: none (v2 gives 2015 RVAs only; 2020 is `?` throughout). r2: second clean-room review: terminal text steps derived from this project's own legibility criterion; the reduced animation set defined by a criterion; facts with no use removed; item highlight stated as a result; the HUD game facts cited from 22 §2.4 (one description); the tap's thread left open as 11 §8 does; lever interactions own turning and the move action (22 §4.1, §7); the master switch of 20 §1.3. r3 (spec r7 cross-check): the controls help cited from 04 §5.7 (it is not in 06); the interaction names passed to `ScriptEventTap` (11 §5.1). r4 (CLEANROOM r3, what a player sees may be stated exactly): the `Interaction` and `HUD` pages of the options page given exactly (item order, labels, control kinds, value names, ranges, steps and display formats, description lines, greying), and the controls help rows of the lever mapping, as this project's texts in the style of the v1 page players know, under the page rules of 22 §12.1; the activation distances' suggested values move from a first offered value to the item's description. r5 (user decision of 2026-10-16): no master switch; every key here applies on its own with its v1 value as default (20 §1.3); the `Interaction` and `HUD` pages are reached from the `MotherVR` page (22 §12.1). r6 (spec-side game analysis of the 2015 `AI.exe`, static, while implementing 5 to 7): the icon highlight update and its "highlight disabled" bit (2.6; §12 item 7 resolved for the update, the receiver of its messages left open); the crosshair update's two invoke sites and the movie invoke's missing null check (2.7); the scripted HUD entity's four methods and the six table slots they fill, and the UI-ready byte the game tests before each lookup (2.7, §12 item 10 in part); the terminal text steps built as tagged variant entries of the UI patch (6.2). r7 (spec-side analysis of the stock `TERMINALINTERACTION.GFX`, `FONTS_EN.GFX`, `DATA\FONT_CONFIG.XML` and `DATA\TEXT\ENGLISH\*.TXT`, static, while building the terminal text steps): the containers of the entry titles and folder names, the fonts' line boxes, and the sizes the steps give them (2.8, 6.2); the stock English texts carry no font size (§12 item 9 resolved for English); the scroll range compared with the line pitch the font gives (§12 item 13, new); `Levers by Hand` and `Camera Takeovers` shown greyed while their features have no effect (section 8, under 22 §12.1's rule). Revision: r7 -->

# 23. Interactions and HUD

v2 features for the moments when the player works something in the world
(levers, valves, terminals, the rewiring and hacking devices, keypads,
crafting, ladders, vents, doors) and for the HUD in a headset: working
levers and valves by hand, fewer interactions that take the camera away
from the player's head, activation distances suited to standing in front
of a panel, readable terminal text, and HUD options (crosshair, HUD off
and its toggle, item highlight, HUD placement).

v2 rules (20 §1.1, §5) apply to every feature here: each is turned on by
its own key under `Settings.Interaction` or `Settings.Hud`, its default
gives v1 behaviour exactly, there is no master switch over the keys (20
§1.3), and with runtime None or a
failed runtime none of it runs and none of its patches is applied.

Addresses are RVAs in the 2015 `AI.exe` (PE timestamp 0x54E3199D, image
base 0x400000). 2020 is `?` throughout, and every feature here stays off
on the 2020 build (20 §6). Names such as `CrosshairUpdate` are spec labels
for code the game leaves unnamed; the implementation may rename them.
Strings and class names the game contains are given exactly, and so is
the text a player sees on the options page (section 8; CLEANROOM r3).
Before any
site is written it is checked as 12 §4 requires; every feature here is an
optional fix (12 §4.2): a mismatch turns that feature off alone and is
logged.

Markers: **[sure]** read in the binary or the game files by the spec side;
**[likely]** strong inference; **[guess]** to be confirmed in game. Each
[likely] and [guess] says what to look for. **[proposal]** marks a design
choice of this project, with its reason, for the user to confirm.

## 1. Summary

| § | Feature | Key (section 8) | Default (v1 value) |
|---|---|---|---|
| 3 | Levers and junction valves worked by hand | `Interaction.PhysicalLevers` | false |
| 4 | Fewer camera takeovers during interactions | `Interaction.CameraTakeovers` | `"Game"` |
| 4.6 | Shorter interaction animations | `Interaction.FastAnimations` | false |
| 5 | Activation distance for terminals and rewire stations | `Interaction.TerminalDistance`, `Interaction.RewireDistance` | 0 (the game's own) |
| 6 | Terminal text size | `Interaction.TerminalTextScale` | 1.0 |
| 7.1 | Crosshair | `Hud.Crosshair` | `"On"` |
| 7.2 | HUD off at start (the toggle gesture is 22 §8.3) | `Hud.Hidden` | false |
| 7.3 | Item highlight | `Hud.ItemHighlight` | true |
| 7.4 | HUD placement | `Hud.Placement`, `Hud.Distance`, `Hud.Width`, `Hud.Pitch` | `"HeadLocked"`, 1.0, 0.55, 0 |

## 2. Game facts

### 2.1 Interaction events [sure]

The UI entity's message handler (05 §6.2: slot 2 of the vtable at
0xDFCE10, handler 0x51BD70) receives, among others, these messages, and
for each passes its name to `ScriptEventTap` (11 §5.1, 0xA1BCC0) when it
arrives:

| Prefix | `_start` | `_finish` |
|---|---|---|
| `cutting_panel` | yes | yes |
| `keypad_interaction` | yes | yes |
| `traversal_interaction` | yes | no finish name exists in `AI.exe` |
| `suit_change_interaction` | yes | yes |
| `terminal_interaction` | yes | yes |
| `rewire_interaction` | yes | yes |
| `hacking_interaction` | yes | yes |
| `ladder_interaction` | yes | yes |
| `button_interaction` | yes | yes |
| `lever_interaction` | yes | yes |

The same handler passes `level_fade_*`, `cutscene_visibility_*` and
`hiding_visibility_*` (05 §6.2, 11 §5.3). Each name is compared with a
hash computed by a static initialiser, as 05 §6.2 describes for the
cutscene messages (the `lever_interaction_start` comparison is at
0x51C386, the hash global at 0x134DB64).

The game's `Interaction` entity (class `CATHODE::Interaction`, vtable at
0xFB6D84; its message method, slot 126, at 0x51D270) passes
`start_interaction`, `stop_interaction`, `allow_interrupt` and
`disallow_interrupt` to the same tap. So the game has a notion of an
interaction that may or may not be interrupted [sure for the names;
likely: `allow_interrupt` marks the part of an interaction the player may
leave].

The name `force_disable_highlight` is also passed to the tap, by a message
method at about 0x518861 [sure for the name; likely: the `UI_Icon`
entity's handler, whose slot 126 is 0x5184D0].

11 §5.3 tracks none of `button_interaction`, `lever_interaction` or
`traversal_interaction` today (section 10 below).

### 2.2 Interaction classes, prompts and controls [sure]

The game's own prompts (`DATA\TEXT\ENGLISH\UI.TXT`, tokens such as
`#aim#` naming player-map actions of `DATA\INPUT.XML`, 14 §2.10) say how
each mechanism is driven on a pad:

| Mechanism | Game data | Prompt keys and texts | Pad input |
|---|---|---|---|
| One-handed lever | prompt key in the levels' scripts; entity kinds `PULL_LEVER`, `ROTATE_LEVER` (from the class names `CATHODE::DeletePullLever`, `CATHODE::DeleteRotateLever` and the strings `DELETE_PULL_LEVER`, `DELETE_ROTATE_LEVER`, `LEVER_TYPE`, `USE_LEVER`); lever models `CONTROLS_LEVER` (animations `LEVER`) in `ANIMATION.PAK` | `AI_UI_ONE_HAND_LEVER_GRAB` "Hold `#aim#`", `AI_UI_ONE_HAND_LEVER_PULL` "`#move#` down", keyboard `..._PULL_KB` "Press `#move_back#`" | hold `aim`, then the move stick down |
| Two-handed lever | `CONTROLS_LEVER_2` (animations `LEVER_2`) | `AI_UI_TWO_HAND_LEVER_GRAB` "Hold `#aim#` and `#fire_or_th#`", `AI_UI_TWO_HAND_LEVER_PULL` "`#move#` down and `#look#` down" | hold `aim` and `fire_or_throw`, then move stick and look stick down |
| Junction (valve-like wheel) | | `AI_UI_JUNCTION_GRIP` "Hold `#aim#` and `#fire_or_th#`", `AI_UI_JUNCTION_Turn` "`#move#` left" | hold both, then the move stick left |
| Electrical junction | | `AI_UI_JUNCTION_GRIP`, `AI_UI_ELEC_JUNCTION_Turn` "`#move#` right" | hold both, then the move stick right |
| Terminal | `CATHODE::AccessTerminal`, `TerminalContent`, `TerminalFolder`; movies `DATA/UI/TERMINALINTERACTION.GFX`, `TERMINALINTERACTIONATTRACT.GFX` (stage 960 x 720); animations `USE_TERMINAL_INTRO`, `USE_TERMINAL_BREATHE`, `USE_TERMINAL_EXIT` | `AI_UI_INTERACT_ACCESS_TERMINAL` | interact, then menu navigation |
| Rewire station | `CATHODE::Rewire`, `RewireSystem`, `RewireLocation`, `RewireAccess_Point`; movie `REWIREV2.GFX` (1920 x 720); animations `REWIRE_BOX_OPEN_PLAYER`, `REWIRE_BOX_USE_PLAYER`, `REWIRE_BOX_CLOSE_PLAYER` (and `_PROP` for the box) | | interact, then menu navigation |
| Hacking device (access tuner) | `CATHODE::HackingGame`; movies `HACKINGV4.GFX`, `HACKINGUPGRADE.GFX` (840 x 720) | `AI_UI_USE_HACKING_TOOL` | sticks and buttons of the minigame |
| Keypad | `CATHODE::UI_Keypad`; movies `KEYPAD_CONTROL.GFX`, `KEYPAD_DISPLAY.GFX` | | menu navigation |
| Cutting torch panel | `cutting_panel` messages; models `CUTTINGPANEL_*` | | interact, hold |
| Portable computer | animation `PORTABLE_COMPUTER_USE_01` | | interact |
| Ladder, vent | `CATHODE::TRAV_ContinuousLadder`, `TRAV_1SHOT_VENT_ENTRANCE`, `TRAV_1SHOT_VENT_EXIT`, `TRAV_1SHOT_FLOOR_VENT_*`; animations such as `ENTER_VENT`, `EXIT_FLOOR_VENT`, `LADDER_*` | | interact, then movement along the path |
| Door button | `AI_UI_INTERACTIONS_OPEN_DOOR`, `AI_UI_INTERACTIONS_CLOSE_DOOR` | | interact |
| Crafting | `RADIALMENU.GFX`, `CRAFTINGSCREEN.GFX` (14) | | radial menu, hold to craft |

So levers and junctions are already analog interactions in the game: the
player grabs with one or two held buttons and drives the handle with a
stick. No other class in the table is driven by an analog motion of the
player.

### 2.3 Animations that drive the camera [sure]

`CMD_PlayAnimation` (class `CATHODE::CMD_PlayAnimation`) is the scripted
"play this animation on a character" command. Its parameter names, hashed
by static initialisers near 0xD58000, include `AnimationSet`, `Animation`,
`PlaySpeed`, `AllowInterruption`, `ConvergenceTime`,
`LocationConvergence`, `OrientationConvergence`, `UseExitConvergence`,
`ExitConvergenceTime`, `FullCinematic` and `PlayerAnimDrivenView`.

- `PlayerAnimDrivenView` is read by a getter at 0x4E2030 (thiscall, one
  stack argument, `ret 4`, returns a bool in AL; prologue
  `55 8b ec 51 8b 55 08 8d`) whose default, when the entity does not set
  the parameter, is **true**. Its one caller is in the command's
  parameter build (function at 0x12FA10, call at 0x130080), which stores
  the value as bit 6 (0x40) of the request's flag byte `+0x55`.
  [likely, from the name and the flag's use: when set, a first-person
  animation moves the camera with the animated head.]
- The convergence parameters move and turn the player to the animation's
  marker before it plays (the game's own error strings say "told to use
  convergence but has no MARKER attached").

### 2.4 Camera takeovers [sure for the code; likely for the situations]

The first-person camera is the FollowBehavior (05 §1). Game code reaches it
through the camera director's behaviour lookup at 0x293B0 (thiscall,
ECX = the director at `[[0x12F0C88]+0x58]+0x1DC`, two stack arguments: the
type name `"FollowBehavior"` and the instance name `"Follow"`, `ret 8`,
returns the behaviour or null). It has 29 call sites. Those that change
where the player looks:

| Label | Where | What it does to the player's view |
|---|---|---|
| `FollowSteer` | 0x2DD50, thiscall, four stack arguments (a mode, a target position `vec3*`, a byte, a float), `ret 0x10`; prologue `56 8b f1 8b 46 34 f3 0f` | sets a target point (`+0x1B4`) and steering flags (`+0x265` to `+0x26A`) so the follow camera turns towards that point, and rewrites the yaw `+0x1FC` / `+0x204`; mode 2 also sets the pitch `+0x200` |
| `StealCamera` start | 0x1C620, entry in the method table at 0xDC231C; `ret 8`; prologue `83 ec 40 56 57 8b 7c 24` | the level's `StealCamera` entity: looks up the follow camera and calls `FollowSteer` (call at 0x1C6E8), turning the player's view to a point of interest |
| `CameraResource` start | 0x24C80, table entry at 0xDC5A74 (also referenced at 0x3D17F7); `ret 8` | activates a level camera through the camera manager's activate method 0x32E10 (call at 0x24CF6): the view leaves the player's head for a fixed or animated camera |
| `CameraResource` stop | 0x245F0, table entry at 0xDC5A78; `ret 8` | returns to the player; reads `reset_player_camera_on_exit` (getter 0x4DDC20, default false, call at 0x2472B) and calls `FollowSteer` (call at 0x2471C) |
| `CameraResetRequest` | 0x33540, thiscall, two stack arguments, `ret 8`, 26 call sites between 0x250ABA and 0x2763A5 (the player's action code [likely]); prologue `55 8b ec 83 e4 c0 83 ec` | for mode 6: looks up the follow camera, re-aligns it to the body heading (method 0x2C6F0), levels its pitch (method 0x2C730 with 0.0) and clears its steering (method 0x2DF30) |
| player state entry | call at 0x281132 | the same lookup, then re-align to the body and clear steering |

These sites were located with the help of an analysis of another mod and
verified by the spec side. Each is listed for the effect on the view in
its last column, which is what §4.2 keeps or reduces; the other callers of
the lookup are not needed by this document.

Because the mod adds the head pose on top of the follow camera (02, 03),
any change of the follow camera's yaw turns the whole world around the
player in the headset, and an animation that drives the view moves the
viewpoint without the head moving. A level camera (`CameraResource`)
replaces the player's viewpoint altogether; the head pose still applies
on top of it (02).

### 2.5 The interaction icon update and its range float [sure]

A per-frame function at 0x3C51E0 (`ret 4`) walks 1024 interaction icon
records (stride 0x9C). For a record with a flag bit set (bit 0 of the
record's byte `+0x96`) it computes the distance from the camera position
of POV 0 (05 §1) to the record's position and compares it with the float
at **0x12411E4** (`comiss` at 0x3C5582, the load at 0x3C557A): within that
distance the record becomes a candidate for the prompt. In the shipped
`AI.exe` that float is 0.0 in `.data`, no instruction writes it, and no
other code or data refers to it (searched by its address in every section).
So in the stock game this test never passes, and the stock activation
range of terminals and rewire stations is decided elsewhere [likely: by
the level's trigger volumes and `Interaction` entities; to be found, §12].

### 2.6 `UI_Icon` parameters [sure]

`CATHODE::UI_Icon` (vtable at 0xFC1E24) is the entity that shows an
interaction icon and its prompt. Its parameter names are hashed into
globals by static initialisers near 0xD63100; among them
`highlight_geometry`, `target_pickup_item`, `highlight_distance_threshold`
(global 0x1353428), `interaction_distance_threshold` (global 0x135342C),
`icon_keyframe`, `can_be_used`, `category` and `push_hold_time`. Its
messages include `highlight` and `unhighlight`.

| Parameter | Getter | Default | Where the value goes |
|---|---|---|---|
| `highlight_distance_threshold` | 0x4F0650, thiscall, one stack argument, `ret 4`, float on the x87 stack; prologue `55 8b ec 51 8b 55 08 d9` | **3.15** (float at 0xFC21E8, bits 0x4049999A) | the icon record's `+0x8C`, written once when the icon starts (the getter's one caller is at 0x16B5C0, in the start method at 0x16B530) |
| `push_hold_time` | 0x4F0590 | 0.0 | the record's `+0x90` |
| `interaction_distance_threshold` | none: its hash global is written by its initialiser and read by no instruction | | not used by `AI.exe` |

**Highlight update** [sure, r6]. `IconHighlightUpdate` at **0x3A3420**
(thiscall on the icon manager, one stack argument, the icon record;
`ret 4`; prologue `55 8b ec 83 e4 f0 6a ff`) reads `+0x8C`. Its two
callers are in the icon update 0x3C51E0, at **0x3C586C** and **0x3C594C**;
neither uses its return value. It decides "highlighted" only when the
record's byte `+0x96` has bits 1 and 2 set, its byte `+0x97` has bit 4 set
and **bit 3 clear**, its geometry list (`+0x74`) is not empty and the
manager's byte `+0x27065` is not 0 (set to 1 in the manager's constructor
only); then either the squared distance from the camera to the record's
position (`+0x4C` to `+0x54`) is at most `+0x8C` squared, or the player's
flag at `+0x2BA` is set, the record is within a view cone (0.2513, float
at 0x12F4B58) and at most 85.38 squared metres away (0x12F4B54). It
compares the result with the record's cached state (`+0x97` bit 5) and,
only on a change, sends message 0x200036 (highlight) or 0x200037
(unhighlight) for the record's entity (`+0x58`) to the object at
0x174EF40 and updates bit 5; it writes nothing else of the record. The
game itself sets `+0x97` bit 3 at 0x16B3C3 (in `UI_Icon`'s code) to
disable an icon's highlight. So an icon whose bit 3 is set when the
update runs is found not highlighted, and an icon that was highlighted is
unhighlighted by the game's own message. [The receiver of the two
messages and what it draws are left open, §12 item 7.]

### 2.7 HUD movies [sure]

**Forced hidden.** Two stock HUD movies define public `force_hud_hidden()`
and `force_hud_visible()`: `WeaponStuff` (`WEAPONSTUFF.GFX`: sets the root
`visible`, so the health bar, ammo counter, crosshair and grapple prompts
go) and `PickupOverlay` (`PICKUPOVERLAY.GFX`: hides `container_mc`,
`staticPrompt_mc`, `holdThrowPrompt_mc`, `mc_tutorial_message_root`; on
visible it also resets `staticPrompt_mc` to its `off` frame). Both keep
a `m_hud_forced_hidden` field. The game calls them through its movie
wrapper objects (the movie at `+0x68`, null until loaded) with the
movie-invoke method 0x4081E0 (thiscall, one stack argument, the function
name, `ret 4`). The wrappers' hide and show methods, their "HUD forced
hidden" byte `+0x40`, how each wrapper re-applies it when its movie
(re)loads, and the scripted entity that hides and shows the HUD (methods
at 0x1094A0 and 0x1095A0, entries of the method table at 0xDFD014 to
0xDFD028) are described once, in 22 §2.4, with its labels
(`WeaponHudHide`, `WeaponHudShow`, `PickupHudHide`, `PickupHudShow`,
`UiFindObject` 0x410420, `UiManagerPtr` 0x134A78C).

**Crosshair.** `CrosshairUpdate` at **0x4371F0** (thiscall on the
`weaponstuff` wrapper, no stack arguments, plain `ret`; prologue
`51 53 56 57 6a 00 8b f9`) decides each frame whether the crosshair should
show [likely: from the player's equipped weapon, a flag at player
`+0x2C3` and a blocking state] and, only when the decision differs from the
cached byte `+0xE4`, invokes `ShowCrosshairs` (then `ShowAmmo` in some
states) or `HideCrosshairs` and `HideUI` on the movie, and stores the new
decision in `+0xE4`. Its one caller is the HUD update at 0x437340 (call at
0x437345), called from 0x44D3C9. In the movie, `ShowCrosshairs` plays the
crosshair's `in` label and `HideCrosshairs` its `out` label; `HideUI` is
empty. 14 §3.6.4 describes the crosshair symbol.

The two invokes [sure, r6]: `ShowCrosshairs` is `push` of the string at
**0xEFEA94** at 0x4372BF and the call of 0x4081E0 at **0x4372C4**;
`HideCrosshairs` is `push` of the string at **0xEFEA84** at 0x437313 and
the call at **0x437318**. The movie invoke dereferences the movie
(`+0x68`) without testing it, so the movie must be loaded before it is
called on this object.

**Scripted HUD entity** [sure, r6]. The slots 0xDFD014 to 0xDFD028 of its
method table hold four methods: 0x1094A0 (in two slots) and 0x109680 hide
the HUD, 0x1095A0 and 0x109770 (in two slots) show it. All four are
thiscall with two stack arguments (`ret 8`), are referred to only from
those slots, and are the only callers of `WeaponHudHide`,
`WeaponHudShow`, `PickupHudHide` and `PickupHudShow` (the calls at
0x109514, 0x109527, 0x10965F, 0x109671, 0x109723, 0x109735, 0x1097F4 and
0x109807). Before each `UiFindObject` of these objects the game tests the
byte at **0x1240F27** (1 at start). So the game's own intent to hide or
show the HUD is the calls of these four methods, while `+0x40` reads the
same whether the game or the mod hid the movie.

### 2.8 Terminal text [sure]

`TERMINALINTERACTION.GFX` (stock identity in 14 §2.3, entry 481; stage
960 x 720) holds the terminal screens. Its text fields (`DefineEditText`,
sizes in stage units, font height in pixels):

| Character | Instance | Size | Font, height | Holds |
|---|---|---|---|---|
| 38 | `txt_description_text` (in sprite 40, `description_box_mc`) | 620 x 463, word wrap, multiline, HTML | `$JixellationReg`, 20 (`<font size="20">` in its initial HTML) | the document body the player reads |
| 33 | `txt_content_text` (sprite 34) | 453 x 38 | `$JixellationBold`, 23 | entry titles |
| 49 | `txt_foldername` (sprite 50) | 185 x 106, word wrap | `$JixellationBold`, 20 | folder names |
| 57, 64 | `folders_txt`, `headingtext` | 201 x 28, 391 x 28 | `$JixellationBold`, 18 | headings |
| 24, 27 | `txt_playlog` | 343 x 32, 279 x 32 | `$JixellationBold`, 21 | audio-log controls |
| 67, 79, 91 | none | | `$JixellationBold`, 14 to 18 | branding lines |
| 9, 11 to 20, 43, 45, 82, 84 | | | 23 to 96 | keypad digits, lock and title screens |

Containers and fonts [sure, read in the movie and the fonts by the spec
side]: each entry title (character 33, placed in sprite 34 at -285, -18.4)
sits on its entry's background bar (sprite 32 in sprite 34, 50.5 units
tall, from -27.45 to 23.05 in the entry); the two entries are 58.4 units
apart. Each folder name (character 49, in sprite 50, placed in sprite 55 at
27.1, -12.7) sits in its folder tile (shapes 48, 51 and 53 of sprite 55,
202 x 122 units, -74 to 128 by -74 to 48); the four tiles are 128.6 units
apart. The records give each field 2 px of leading and 2 px of gutter on
each side. `DATA\FONT_CONFIG.XML` maps `$JixellationBold` and
`$JixellationReg` to `Jixellation` bold and normal, whose `FONTS_EN.GFX`
records give a line box (ascent plus descent) of 1.311 and 1.223 times the
font height. So the stock folder field (106.5 units) holds three lines of
its 20 px text, and the body's lines are about `1.223 h + 2` px apart.

`TerminalContentPage.set_description_text` sets the body field's height to
5000 and fills its `htmlText`; scrolling moves the field's `y`, and
`scroll_up` clamps it with `numLines * LINE_HEIGHT - TEXT_PAGE_HEIGHT`,
where `LINE_HEIGHT` is an internal const 20 and `TEXT_PAGE_HEIGHT` 10. So
the scroll range assumes 20 px lines. `TERMINALINTERACTIONATTRACT.GFX`
(the idle screen) has a 78 px prompt and a 23 px version line.

The terminal screens are drawn on the terminal's in-world display, not on
the HUD panel or the virtual screen [likely, 14 §3.6.2 rule 1: a stage
other than 1280 x 720; check in game].

## 3. Levers and junction valves by hand

### 3.1 What the player experiences

With `Interaction.PhysicalLevers` on, when the game offers a lever or a
junction, the player closes a hand (or both, for a two-handed lever or a
junction) and pulls down or twists, and the handle in the game follows.
Letting go releases the handle as releasing the pad buttons would. The
game's own prompts still show; while the key is in effect, the controls
help (04 §5.7) ends with two rows, after those of 22 §10.3 [proposal: in
the bracketed style of v1's `Flashlight [Press Interact near Head]`]:
`Pull Lever [Grip the Handle]` | `Grip and Pull Down`, and
`Turn Valve [Grip with Both Hands]` | `Twist Both Hands`.

### 3.2 Required behaviour

The mod does not move the lever itself: it produces, from the hands, the
pad inputs the game already reads for these mechanisms (2.2), through the
virtual pad of 04 §3. While the interaction kind (section 10, 11 §5.3) is
**lever** or the prompt is a junction prompt [see §12: how the mod knows a
junction is running]:

| Hands | Game input produced |
|---|---|
| one grip held (either hand) | `aim` held |
| both grips held | `aim` and `fire_or_throw` held |
| a held hand moved down from where its grip closed | move stick Y negative, proportional to the distance (full deflection at the pull distance below) |
| both grips held and both hands moved down | move stick Y and look stick Y negative, each from its hand's distance (the right hand drives look, the left drives move) |
| both grips held and the hands rotated together about the forward axis (a twist) | move stick X, left for an anticlockwise twist, right for a clockwise one, proportional to the angle |
| a grip released | that hand's input released; the stick values it drove return to 0 at once |

All other v1 controls keep working, except the grips: while this mapping
is active the grips belong to it (04 §5.8.5: in v1 the right grip is Aim
and the left grip the Tracker; one owner per input, 20 §5). The face
buttons, `back_close` included, keep their v1 meaning, so the player can
still leave an interaction the way the game allows.

Distances are measured in the playspace, from each hand's grip pose (04
§5.11), relative to the pose it had when its grip closed, so the mapping
needs no knowledge of where the handle is.

**Aborting.** Releasing the grip before the pull completes is the same as
releasing `aim` on a pad. What the game does then (the lever springs back,
or stays where it is) is the game's own behaviour and is not changed
[guess: check both lever kinds in game]. Turning, snap or smooth, is
suppressed while this mapping is active, whatever
`Controls.InteractionAwareTurning` says, and 22's d-pad emulation does not
take the left stick then (22 §4.1, §7) [proposal: a pull made while the
world turns is lost and uncomfortable; the mapping owns the grips and the
move and look actions, one owner per input, 20 §5].

**What stays the game's.** The interaction's start (walking to the marker,
the grab animation), the lever's travel and its result are the game's.
Terminals, rewire stations, the hacking device, keypads, the cutting
torch, crafting, ladders, vents and door buttons keep their v1 controls
in r1 [proposal: their stock input is menu navigation or a single press,
so a hand motion would only replace one button press with a gesture;
touching a screen by hand needs the in-world placement of each screen,
which is not known (§12)]. They are kept as the game's animations, subject
to section 4.

### 3.3 Parameters

| Parameter | Value | Reason |
|---|---|---|
| Pull distance for full deflection | 0.25 m [proposal] | a comfortable forearm pull from chest to waist height without stepping; less makes the stick saturate on a small motion, more tires the arm |
| Dead zone | 0.03 m [proposal] | hand tremor and tracking noise stay below it; the game sees no stick input until the hand clearly moves |
| Twist for full deflection | 60° [proposal] | a wrist turn the player can make with both hands on a wheel without releasing |
| Twist dead zone | 8° [proposal] | as the distance dead zone |
| Stick value | clamped to [-1, 1]; mapped linearly beyond the dead zone | the game's stick range (04 §2.3) |

### 3.4 Off

With the key false nothing here runs: the grips are Aim and Tracker as in
v1, and levers are worked with the trigger, grip and stick as 04 maps
them.

### 3.5 Edge cases

- **Hands lost** (controller off, untracked, dashboard): the affected
  hand's grip counts as released (20 §4: fall back to v1). If both are
  lost the v1 bindings apply until tracking returns.
- **Interaction ends** (any `_finish` or `stop_interaction`, 11 §5.3):
  the mapping ends; held inputs are released in the same frame.
- **Cutscene, menu, loading, death:** the mapping is inactive (no
  interaction kind, or the pause menu is up, 05 §3.2).
- **Runtime None, failed runtime, hands off (21):** inactive. The feature
  needs hand poses but not 21's hands-on mode [proposal: it is useful in
  seated play with hands off, since it only reads poses].
- **OpenVR and OpenXR:** the same; both provide grip poses (04 §5.11).
- **Keyboard and mouse in use** (05 §3.2b): inactive; the game's keyboard
  prompts apply.

## 4. Fewer camera takeovers

### 4.1 What the player experiences

With `Interaction.CameraTakeovers` set to `"Reduced"`, short interactions
(buttons, levers, terminals, rewire stations, keypads, the portable
computer) no longer swing the view or turn the world; the view stays
where the player's head points, and the hands and the device still move
as the game plays them. Scripted moments that point the player's view at
something (`StealCamera`) no longer turn the world. Ladders, vents,
grabs, deaths and cutscenes keep the game's camera.

### 4.2 Which takeovers are kept and which are reduced

| Takeover (2.3, 2.4) | `"Reduced"` | Reason |
|---|---|---|
| Animation-driven view (`PlayerAnimDrivenView`) for the **short device animations**: the player's first-person animations played while the player stands at an interaction marker in front of a device and does not travel (for example the terminal and rewire box animations named in 2.2); the set is enumerated from `ANIMATION.PAK` by this rule (§12 item 12) | cleared: the animation plays, the view does not follow the animated head | these animations move the viewpoint by small amounts while the player stands still; in a headset that is felt as the body being pushed. The player looks at the device anyway |
| Animation-driven view for traversal (ladders, vents), grabs and kills, `FullCinematic` animations and anything not in the list | kept | the viewpoint really travels along a path the player cannot walk, or the scene is a staged moment; a fixed view would show the inside of geometry [proposal] |
| `StealCamera` | the steer is not applied: the follow camera's yaw and pitch are unchanged [proposal] | a scripted turn of the view is a turn of the world in a headset, the classic cause of sickness; the point of interest is still announced by sound and the scene |
| `CameraResetRequest` (mode 6) and the state entry at 0x281132 re-aligning yaw to the body | the yaw re-align is not applied; levelling the pitch still is | a yaw snap at the end of an interaction rotates the world; pitch comes from the head in VR (05 §3.2d) so levelling it is invisible |
| `CameraResource` (level cameras) | kept | a level camera is a cutscene-like shot; v1 already shows it in stereo with the head pose on top; removing it would break scenes that depend on the shot |
| `reset_player_camera_on_exit` steer when a level camera ends | the yaw part is not applied [proposal] | as for `StealCamera` |
| Convergence (walk-to-marker) | kept | the game needs the body at the marker for the animation to line up with the device; it moves the body, not the view's yaw, once `OrientationConvergence` turns are covered by the rows above [likely; check that orientation convergence does not rotate the view] |

With `"Game"` (default) every takeover is the game's, as in v1.

Required results, not a mechanism: while `"Reduced"` is in effect, (a) a
`CMD_PlayAnimation` request for one of the short device animations has
bit 0x40 of its flag byte `+0x55` clear when the request is built,
(b) no call of `FollowSteer` from the `StealCamera` start or the
`CameraResource` stop changes the follow camera's yaw `+0x1FC` / `+0x204`
or pitch, and (c) the re-align to the body of `CameraResetRequest` (mode
6) and of the state entry at 0x281132 leaves the yaw unchanged. How each
is achieved is the implementation's choice under 12 §4, after checking the
listed prologue or call target. The animation set is matched by the
animation's name as the
game identifies it [open, §12: how `CMD_PlayAnimation` identifies the
animation in the request (name string or hash)].

### 4.3 Off

`"Game"`: nothing here is installed (20 §1.1 rule 2).

### 4.4 Edge cases

- **Cutscenes** (05 §3.2g): every takeover is kept during a cutscene or
  level fade.
- **Snap and smooth turn:** the follow camera's yaw remains the player's
  own; only scripted writes are suppressed. A player who then faces away
  from a device can turn as usual.
- **Animation ends with the body facing elsewhere:** the game may expect
  the view to face the body after an interaction. With the yaw re-align
  suppressed, the view keeps its heading and the body turns under it at
  the next movement [guess: check that movement direction follows the
  view after a terminal and a lever].
- **Death and reload:** nothing persists; the next level starts as the
  game sets it.
- **Hands on (21):** 21's control hand-back after animations (21 §12)
  still runs; this section does not depend on it.

### 4.5 Comfort reason

Rotating or translating the rendered world without the player's own head
motion is the main source of VR discomfort; small viewpoint motions during
short interactions buy nothing in a headset because the player already
looks where they want. Long traversals keep the game's camera because the
alternative (a fixed view while the body moves through a vent) puts the
eyes inside walls.

### 4.6 Shorter interaction animations [proposal]

With `Interaction.FastAnimations` on, the short device animations of
4.2's first row play faster by a factor of 1.5 [proposal:
the terminal intro and the rewire box open and close are long when the
player is standing at the device already; 1.5 keeps the device's
animation readable]. The game's parameter for it is `PlaySpeed` (2.3);
where the request's play speed is stored and whether the device's own
animation (`_PROP`) must be sped up with it to stay aligned are not yet
known (§12). Until they are, this key is reserved and has no effect, and
the options page does not show it.

## 5. Activation distances

### 5.1 What the player experiences

With `Interaction.TerminalDistance` or `Interaction.RewireDistance` set,
the terminal or rewire prompt appears when the player's head is within
that distance of the device, so the interaction starts from where a
standing player naturally stands, not from further away with a long walk
to the marker or from so close that the head is inside the screen.

### 5.2 Required behaviour

The game's own activation for these devices is not the dormant test of
2.5 (it never passes in the stock game) and has not been located (§12).
Required result, once it is: with a value `d` > 0, the prompt for a
terminal (respectively a rewire station) is offered exactly when the
distance from the camera position of POV 0 to the device's icon position
is at most `d` and the game's other conditions for offering it hold; with
0 the game's own rule applies unchanged. The implementation may use the
dormant test of 2.5 only after showing in game that a record of these
devices reaches it with the flag bit set; using it for other icons is not
required and not allowed [proposal: other prompts keep the game's ranges,
which work at standing height].

### 5.3 Parameters

| Key | Range | Proposal | Reason |
|---|---|---|---|
| `Interaction.TerminalDistance` | 0, or 0.6 to 2.0 m | 1.0 m when the player turns it on | arm's length plus the distance a person stands from a wall screen; below 0.6 m the head would be at the screen surface |
| `Interaction.RewireDistance` | 0, or 0.6 to 2.0 m | 1.2 m when the player turns it on | the rewire box opens towards the player and needs room for it |

The default of both keys is 0 (section 8); the "Proposal" column is the
value the item's description line suggests (section 8).

### 5.4 Off, edge cases

0 (default): v1 behaviour. Cutscenes, menus and loading: no prompts in any
case. Hiding: unchanged. Runtime None: off.

## 6. Terminal text size

### 6.1 What the player experiences

Terminal documents are readable in the headset without leaning in: the
body text is larger, and long documents still scroll to their end.

### 6.2 Required behaviour

`Interaction.TerminalTextScale` `t` enlarges the text of
`TERMINALINTERACTION.GFX` (2.8) through the UI build of 14, as a layout
change of the VR set (14 §1.3, §3.6):

- the body field (character 38): its font height and the size in its
  initial HTML become `round(20 x t)`, and the body's text is shown at that
  size whatever HTML the game puts into it [proposal: by giving the field
  a default format of that size, so text set later inherits it; the
  stock HTML from the game carries no size of its own, check in game];
- its width stays 620 units, so the text wraps into more lines, which the
  field already allows (word wrap, multiline);
- the scroll range uses the new line height: `LINE_HEIGHT` (an internal
  const of `TerminalContentPage`) becomes `round(20 x t)`, so `scroll_up`
  still reaches the last line;
- entry titles (character 33) and folder names (character 49) scale by
  `t` as far as their containers allow without overlapping (14 §3.6.1
  bounds), and are narrowed and wrapped as 14 §3.6.1 says when needed.
  With the containers of 2.8 [project decision, from those bounds]: a
  title's font height is `round(23 x t)`, at most 34 px, and its field
  grows about its stock vertical centre to hold one line (line box plus
  gutters), staying inside its bar, whose room about that centre allows
  49 units, so 34 px; its width stays 453. A folder name's font height is
  `round(20 x t)`, at most 26 px, and its field keeps its top, grows down
  to the tile's bottom (113.95 units) and widens about its centre to 200
  units, so it still holds the three lines it holds in stock; its text
  wraps into those lines as before. So the titles are 29, 34 and 34 px and
  the folder names 25, 26 and 26 px at the three steps. A title too long
  for its field at the larger size is cut at the field's right edge, as in
  stock [guess: titles are short; check in game];
- keypad digits, lock screens, branding lines and the attract movie are
  unchanged (they are already large or not read).

| Parameter | Values | Reason |
|---|---|---|
| `t` | 1.0, 1.25, 1.5, 1.75 [proposal] | the stock 20 px body text on a terminal seen at about 0.6 m in a headset is small; steps of a quarter add 5 px each (25, 30, 35 px), a change the player sees at once; 1.75 is the largest because a body line in the 620-unit field must still hold about 30 characters (a character is about half the font height wide), below which prose reads in fragments and a typical log needs many screens |

Takes effect on the next launch: the options archive is built at launch
(14 §5), and `t` becomes part of its staleness key (section 13).

How the build carries the steps [r6, project decision]: the embedded UI
patch (14 §5) holds, after the entry it replaces, one entry per step
other than 1.0, named as that entry with `|text125`, `|text150` or
`|text175` appended. The build takes the entry whose tag matches `t` in
place of the earlier one and drops the others; with `t` 1.0, or with a
patch that has no entry for the tag (logged), the movie is the VR set's
as before. The step's index (0 to 3) is added to the staleness key. Reason:
one patch file serves every step and the archive stays one file.

### 6.3 Off, edge cases

1.0: the movie is linked exactly as v1 links it (only the 3.4 prompt
changes of 14). Runtime None: the menu set only (14 §1.3), so no change.
A `UI.PAK` that is not the stock one: 14 §5.9 applies; nothing changes.

## 7. HUD options

### 7.1 Crosshair

**Player.** `Hud.Crosshair` `"Off"` removes the crosshair; `"Auto"`
removes it while 21's hand aiming is on, because a head-centred crosshair
then points somewhere the weapon does not; `"On"` (default) is v1.

**Required.** While the crosshair is to be hidden, the game never shows it:
whenever `CrosshairUpdate` (2.7) would show it, it is not shown, and if it
is showing when the setting changes it is hidden within one HUD update.
The rest of `CrosshairUpdate` (the ammo counter's `ShowAmmo`) keeps the
game's behaviour [proposal: hiding the crosshair must not hide the ammo
count]. The cached byte `+0xE4` stays consistent with what the movie
shows, so turning the setting back on shows the crosshair at the next
update in which the game wants it. Site checks: the prologue of 2.7 and
the call at 0x437345 targeting 0x4371F0.

**Edge cases.** Menus and cutscenes: the game hides it already. Death and
reload: re-evaluated by the game's update. 14 §3.6.4's scale applies
whenever it shows.

### 7.2 HUD off and the HUD toggle

**Player.** `Hud.Hidden` true starts every session with the HUD hidden
(the `weaponstuff` and `pickupOverlay` movies: health, ammo, crosshair,
interaction prompts, objective messages), for players who play without a
HUD [proposal: a setting rather than only a gesture, so the choice
survives a restart]. The HUD toggle gesture, its key
(`Controls.HudToggleGesture`) and its result are 22 §8.3's; this document
does not redefine them.

**Required.**

- The "HUD hidden by the player" state of 22 §8.3 starts each session as
  `Hud.Hidden` says (22 §8.3 starts it visible; with `Hud.Hidden` false
  that is unchanged). The gesture then flips it as 22 §8.3 says.
- Hiding and showing, whether from this setting or the gesture, have
  exactly 22 §8.3's results: through the game's own hide and show methods
  (2.7; 22 §2.4 labels them), on the game thread, only when the movie's
  object exists; a HUD the game itself hides is never shown by the mod;
  while the player's state is hidden the HUD stays hidden after the game
  shows it again.
- Changing `Hud.Hidden` from the options page sets the player's state at
  once (the page is in the pause or front-end menu; the HUD movies apply
  it when gameplay resumes or the level loads).
- Subtitles (`SUBTITLES.GFX`) and the motion tracker are not hidden
  [proposal: subtitles carry dialogue; the tracker is a held device].

**Off.** `Hud.Hidden` false (default): the session starts with the HUD
shown, as v1 and 22 §8.3.

**Edge cases.** Level load: the movies reload; the game re-applies its own
state (2.7) and 22 §8.3 re-applies the player's once the objects exist.
`PickupOverlay`'s `force_hud_visible` resets `staticPrompt_mc` to `off`,
so a prompt that was up when the HUD is shown again appears at its next
update [likely]. Menus and the virtual screen are not affected.

### 7.3 Item highlight

**Player.** `Hud.ItemHighlight` false removes the highlight of nearby
items, so items are found by looking, not by their glow [proposal: wanted
by players for immersion].

**Required.** With false, no item, prop or icon is drawn highlighted,
while prompts and the ability to interact work as in v1. With true, the
highlight is the game's, as in v1. How is the implementation's choice under
12 §4; the game facts of 2.6 (the `highlight_distance_threshold`
parameter, its getter and the icon record field it fills) and the open
question of §12 item 7 (what reads that field and what it highlights)
apply. A change may take effect only for icons started after it, for
example from the next level load [proposal: acceptable because the item's
description line says so, section 8].

### 7.4 HUD placement

**Player.** The v1 HUD panel is head-locked, 0.55 m wide, 1 m ahead (05
§4.3). v2 lets the player move it lower, nearer or further, and choose a
panel that follows the head lazily instead of being fixed to it.

**Required.** The HUD quad builder replacement of 05 §4.1 builds the mode
0 panel from these settings:

| Key | Values | Default (v1) | Effect |
|---|---|---|---|
| `Hud.Placement` | `"HeadLocked"`, `"Follow"` | `"HeadLocked"` | head-locked as 05 §4.3, or level and lazily following (below) |
| `Hud.Distance` | 0.6 to 2.0 m | 1.0 | centre distance from the point between the eyes |
| `Hud.Width` | 0.3 to 1.2 m | 0.55 | panel width; the height follows the UI aspect as in 05 §4.3 |
| `Hud.Pitch` | -30° to +10° | 0 | the panel's centre direction below (negative) or above the view direction |

- **HeadLocked** with these values: as 05 §4.3, with the panel's normal
  rotated about the head's right axis by `Hud.Pitch` and the panel facing
  the eyes.
- **Follow** [proposal]: the panel is level (no roll, its normal
  horizontal before the pitch offset), at `Hud.Distance` in the horizontal
  heading it last settled on and at eye height plus the pitch offset; when
  the head's heading differs from it by more than 20°, it moves to the
  head's heading over 0.3 s with an ease-in-out, and it follows the head's
  position every frame. Reasons: a panel that does not roll or bob with
  every head motion is easier to read; 20° keeps it still during normal
  glances; 0.3 s is fast enough to stay in view when turning, slow enough
  not to drag the eyes. Snap turns (05 §3.2c) re-place it at once.
- Every value keeps the HUD angular size of 14 §3.6.1 within its stated
  ratio only at the default `Width / Distance`; a player who shrinks it
  shrinks the text with it. The `HUD Width` item's description says so
  (section 8).
- The quad matrix is never NaN; looking straight up or down keeps the last
  heading (03 §2.1 threshold).

Off: the defaults give exactly 05 §4.3.

**Edge cases.** Mode 1 (virtual screen) is not affected. Recenter (05
§3.2e): Follow re-places at once. Cutscenes: the HUD keeps its placement.
Headset off: no change.

## 8. Settings

Inside `Settings`, in two new objects `Interaction` and `Hud` (20 §5).
Read, written and preserved as 06 §3 says for v1 keys: each key read on
its own, a wrong type gives the default, unknown keys are kept.

| Key | JSON type | Default | Values / range | Sanitise | Takes effect |
|---|---|---|---|---|---|
| `Interaction.PhysicalLevers` | bool | false | | | at once |
| `Interaction.CameraTakeovers` | string | `"Game"` | `"Game"`, `"Reduced"` | any other string or type: `"Game"` | at once; an interaction in progress keeps the camera rule it started with |
| `Interaction.FastAnimations` | bool | false | | | reserved (4.6) |
| `Interaction.TerminalDistance` | number | 0 | 0, or 0.6 to 2.0 | values in (0, 0.6) become 0.6, above 2.0 become 2.0, negative or NaN become 0 | at once (once 5.2 is available) |
| `Interaction.RewireDistance` | number | 0 | as above | as above | as above |
| `Interaction.TerminalTextScale` | number | 1.0 | 1.0, 1.25, 1.5, 1.75 | the nearest listed value; NaN gives 1.0 | next launch (14) |
| `Hud.Crosshair` | string | `"On"` | `"On"`, `"Off"`, `"Auto"` | other: `"On"` | at once |
| `Hud.Hidden` | bool | false | | | at once (7.2) and at each session start |
| `Hud.ItemHighlight` | bool | true | | | icons started after the change (7.3) |
| `Hud.Placement` | string | `"HeadLocked"` | `"HeadLocked"`, `"Follow"` | other: `"HeadLocked"` | at once |
| `Hud.Distance` | number | 1.0 | 0.6 to 2.0 | clamped; NaN gives 1.0 | at once |
| `Hud.Width` | number | 0.55 | 0.3 to 1.2 | clamped; NaN gives 0.55 | at once |
| `Hud.Pitch` | number (degrees) | 0 | -30 to 10 | clamped; NaN gives 0 | at once |

Strings are case-sensitive, as in 06 §3.3. Every key is persisted when the
player changes a setting (06 §3.1), and never written at start-up.

**Options page.** Each key gets a control on the `Interaction` or `HUD`
page, reached from the `MotherVR` page (22 §12.1, which also gives the rules
every v2 page follows: look, value display, description lines, greying
and hidden items). The labels, values and description lines below are
exact; they are this project's texts, in the style of the v1 page players
know (00 §2.6) [proposal for each text: it names the result the player
gets, in the fewest words that fit the stock description length, 22
§12.1]. `Interaction.FastAnimations` gets no item while reserved (4.6).

`Interaction`, in order:

| # | Label | Kind | Values shown | Key | Description line |
|---|---|---|---|---|---|
| 1 | `Levers by Hand` | toggle | on / off | `Interaction.PhysicalLevers` | `Grip levers and valves and pull or twist them with your hands` |
| 2 | `Camera Takeovers` | list | `Game`, `Reduced` | `Interaction.CameraTakeovers` | `Reduced: fewer interactions take the view away from your head` |
| 3 | `Terminal Distance` | list | `Off`, `0.6 m`, `0.7 m`, ... `2.0 m` (steps of 0.1) | `Interaction.TerminalDistance` | `How close you must be for a terminal's prompt. Off: the game's. Suggested: 1.0 m` |
| 4 | `Rewire Distance` | list | `Off`, `0.6 m`, `0.7 m`, ... `2.0 m` (steps of 0.1) | `Interaction.RewireDistance` | `How close you must be for a rewire prompt. Off: the game's. Suggested: 1.2 m` |
| 5 | `Terminal Text Size` | list | `100%`, `125%`, `150%`, `175%` | `Interaction.TerminalTextScale` | `Requires restart! Larger text on terminal screens` |

Items 3 and 4 are shown greyed with `Not available yet` until the game's
activation for these devices is located (5.2; 22 §12.1). Items 1 and 2
are shown greyed with `Not available yet` while their features have no
effect, that is until §12 items 2 to 4 (levers) and 5, 6 and 12 (camera
takeovers) are resolved [project decision of r7, under 22 §12.1's rule
for an item whose game input or site is not yet confirmed: a control that
changes nothing must not look like a setting that works]. Their keys are
kept and still set from the file. The distances
show one decimal and the unit (`1.0 m`) [proposal: a list value is read as
a measure, so it keeps its unit, while a slider's number does not, 22
§12.1].

`HUD`, in order:

| # | Label | Kind | Values shown | Key | Description line |
|---|---|---|---|---|---|
| 1 | `Crosshair` | list | `On`, `Off`, `Auto` | `Hud.Crosshair` | `Auto: hidden while the weapon aims from your hand` |
| 2 | `Hide HUD` | toggle | on / off | `Hud.Hidden` | `Starts with the HUD hidden, prompts and objectives included` |
| 3 | `Item Highlight` | toggle | on / off | `Hud.ItemHighlight` | `Glow on items you can pick up. A change may apply only from the next level` |
| 4 | `HUD Placement` | list | `Head-Locked`, `Follow` | `Hud.Placement` | `Follow: the HUD stays level and catches up when you turn` |
| 5 | `HUD Distance` | slider | 0.6 to 2 in steps of 0.1 | `Hud.Distance` | `Distance of the HUD from your eyes, in metres` |
| 6 | `HUD Width` | slider | 0.3 to 1.2 in steps of 0.05 | `Hud.Width` | `Width in metres. A smaller HUD also makes its text smaller` |
| 7 | `HUD Pitch` | slider | -30 to 10 in steps of 5 | `Hud.Pitch` | `Moves the HUD below (negative) or above your view, in degrees` |

`Crosshair` uses the game's own value names `On` and `Off` (the game's
menus name such values so) and `Auto` for the third. The HUD toggle
gesture's item is on the `Controls` page (22 §12.2).

**Commands.** Each key gets a value pushed to the page as 06 §4.3 does,
and a command from the page in the 06 §4.2 form (a name beginning `mvr_`,
one value, the 06 §4.2 rule for bad values). The new command names are the
implementation's choice (they are not compatibility names), must not
collide with 06 §4.2's, and are listed in the player guide.

## 9. Threads

| Work | Thread |
|---|---|
| Lever mapping (3): reading hand poses and grips, writing the virtual pad | the game thread, in the virtual pad's poll (04 §3.6), from the hand poses handed over by 04 §5.11 |
| Interaction kind (11 §5.3) | written by the tap on whichever thread calls it (open, 11 §8); read as single atomics, so the reader's thread does not matter |
| Camera takeover suppression (4), crosshair (7.1), HUD hide and show (7.2), item highlight (7.3) | the game thread (the game's camera, entity and HUD updates) [likely: all reached from the game's update loop, 05 §13] |
| HUD placement (7.4) | the thread that runs the overlay draw (05 §13), reading the settings as single words (06 §3.4) |
| Settings changes | the game thread (06 §3.4) |

No allocation, file I/O, locking or logging per frame in any of these.
The Follow placement's state (heading, transition time) lives with the
quad builder and uses a monotonic clock.

## 10. Logging

11 owns the wording; each event conveys:

- once at start-up, per feature: on or off, and whether its sites passed
  their checks (on a mismatch: the site and that the feature is off);
- the interaction kinds of section 13's change to 11 §5.3, as 11 §4.3
  already logs them with `DebugOptions`;
- with `DebugOptions`: each change of the lever mapping (active,
  inactive, which hands held), each suppressed takeover (which kind), each
  HUD toggle and the resulting hidden movies;
- once per settings change: the new values (06's settings entry).

## 11. Acceptance

Checked in game on the 2015 build, headset worn, with OpenVR and with
OpenXR unless stated.

1. All keys absent: the log shows every feature here off and no site of
   this document patched; levers, terminals, HUD and crosshair behave as
   in v1 (05, 14 acceptance).
2. `Interaction.PhysicalLevers` on, mission 1 or 2 one-handed lever: close
   one hand and pull down 0.25 m: the lever is pulled; releasing halfway
   gives the game's release behaviour; the log (`DebugOptions`) shows the
   mapping active only during the interaction.
3. A two-handed lever: one hand only does not pull; both hands pulling
   down do. A junction: both hands twisting turn it in the prompted
   direction. No snap turn happens while gripping.
4. Lose tracking of one controller during a pull: the pull stops; the v1
   grips work again once the interaction ends.
5. `Interaction.CameraTakeovers` `"Reduced"`: use a terminal, a rewire
   station, a door button: the view does not swing or snap at start or
   end; climbing a ladder and entering a vent still move the view along
   the path; a scripted look-at (`StealCamera`) does not turn the world.
   `"Game"`: all as v1.
6. `Interaction.TerminalTextScale` 1.5, restart: terminal body text is
   larger, a long log scrolls to its last line; 1.0 restores the stock
   look.
7. `Hud.Crosshair` `"Off"`: no crosshair with any weapon raised; the ammo
   counter still appears. Back to `"On"`: the crosshair returns at the
   next raise.
8. `Hud.Hidden` true, restart: gameplay starts without health, ammo and
   prompts; subtitles still show; with 22 §8.3's gesture on, the gesture
   shows the HUD, and during a scene in which the game hides the HUD the
   gesture does not show it.
9. `Hud.ItemHighlight` false, then load a save: items near the player are
   not highlighted; prompts still appear and work.
10. `Hud.Placement` `"Follow"`, `Hud.Pitch` -15: the HUD sits below the
    view, does not roll with the head, stays put for small glances and
    follows a turn beyond about 20° smoothly.
11. With every key at its default, the frame time on a terminal and in
    gameplay equals v1 within measurement noise (20 §1.1 rule 2).
12. The `Interaction` and `HUD` pages show the labels, values, order and
    description lines of section 8; `Levers by Hand`, `Camera Takeovers`,
    `Terminal Distance` and `Rewire Distance` are greyed with `Not
    available yet` while their features have no effect (section 8);
    with `Levers by Hand` on, `View Controls` ends with the two rows of
    3.1.

## 12. Open questions

1. **Activation range.** What decides when a terminal or rewire prompt
   appears in the stock game (trigger volumes, `Interaction` entities, the
   second loop of 0x3C51E0, `UI_Icon`'s `lock_interaction`), and whether
   the dormant range test of 2.5 is reached by those icons. Look for:
   which icon records carry bit 0 of `+0x96`, and what the second loop of
   0x3C51E0 compares.
2. **Junction detection.** The UI entity sends no `junction` message
   (2.1). Which message or state marks a junction interaction (perhaps
   `button_interaction` or `start_interaction` with the junction prompt
   up). Look for: the tap's log (11 §4.3) during a junction.
3. **Lever release.** What a lever does when `aim` is released mid-pull,
   for both lever kinds.
4. **Two-handed lever with one grip.** Whether holding `fire_or_throw`
   alone or `aim` alone during a two-handed lever has any side effect
   (firing a raised weapon).
5. **Animation identity.** How `CMD_PlayAnimation` identifies the
   animation in its request (string, hash, resource), so the list of 4.2
   can be matched; and where `PlaySpeed` is stored (4.6).
6. **Orientation convergence.** Whether `OrientationConvergence` turns the
   follow camera's yaw (4.2, last row).
7. **Highlight reader.** (Resolved in r6 for the reader: 2.6's
   `IconHighlightUpdate`.) Still open: the receiver of its messages
   0x200036 and 0x200037, what it draws, and the receiver of
   `force_disable_highlight`.
8. **Terminal screen placement.** Whether the terminal UI is drawn on the
   in-world display (2.8), where that display is, and whether a touch or
   pointer input to it is feasible (3.2).
9. **Body text format.** (Resolved in r7 for English: the `<font>` tags
   of `DATA\TEXT\ENGLISH\*.TXT` carry only a colour, and the movie's own
   recolouring adds only colours, so the body is shown at the field's own
   size.) Still open: the other languages' texts.
10. **The scripted HUD entity.** (Its four methods: 2.7, r6.) The class
    owning the method table at 0xDFD014 (2.7), and whether other HUD movies (objectives, subtitles)
    are hidden by other means in scripted scenes.
11. (Resolved in r2: during the lever mapping this document owns the
    grips and the move and look actions; 22 §4.1 blocks turning and 22 §7
    leaves the left stick to the mapping.)
12. **Short device animations** (4.2): the names in `ANIMATION.PAK` of
    the player's first-person animations played at an interaction marker
    in front of a device without travel. To resolve: list the
    `1ST_PERSON` animation set's names and keep those whose device class
    (2.2) is not traversal; confirm with the tap's log (11 §4.3) which of
    them play during each interaction kind.
13. **Scroll range and line pitch.** `scroll_up` stops at `numLines x
    LINE_HEIGHT - 10` (2.8), while the body's lines are about `1.223 h +
    2` px apart (2.8: 26.5 px in stock, 44.8 px at 35 px). If the game's
    lines are that far apart, the range ends before the last line once a
    document has more lines than about four times a page (17 lines in
    stock, 10 at 1.75), in stock as with the steps of 6.2. Look for: a
    long log (over 40 lines at 1.75) scrolled to its end; if the last line
    is not reached, `LINE_HEIGHT` becomes the measured pitch.

## 13. Changes to v1 documents

Not made here; for the v1 revision that collects v2 changes (20 §10 item
11):

- `11-diagnostics.md` §5.3: add the prefixes `lever_interaction` (kind
  lever), `button_interaction` (kind button) and `traversal_interaction`
  (kind traversal; it has no `_finish`, so it ends with
  `stop_interaction` or the next start) to the tracked kinds, and the
  matching values of "interaction kind" (game fact 2.1).
- `11-diagnostics.md` §2: the start-up and settings events of section 10.
- `06-options-and-config.md` §1, §3.2, §4.2, §4.3, §5.2: the `Interaction`
  and `Hud` sections, their pushes, commands and the `Interaction` and
  `HUD` pages (section 8, under 22 §12.1).
- `04-input.md` §5.7: the two lever rows of 3.1 after the rows of 22
  §10.3.
- `05-game-patches.md` §4.3: the HUD panel's size, distance and
  orientation come from `Hud.*` (7.4); the defaults are the values given
  there. §12: the events of 2.1 that v2 reacts to.
- `04-input.md` §5.8.5 and §5.6: during a lever or junction interaction
  with `Interaction.PhysicalLevers` on, the grips drive 3.2's mapping
  instead of Aim and Tracker; hand poses (5.11) gain a game-thread reader;
  §6: no turning while the mapping is active (22 §4.1).
- `14-ui-resources.md` §1.3, §3.6, §5.1: the terminal text change of 6.2
  as part of the VR set (the movie is already a touched entry, §2.3); the
  staleness key includes `Interaction.TerminalTextScale`; §3.6.2's
  "unchanged" row for `TERMINALINTERACTION` gains "except 23 §6".
- `00-overview.md` §1.1 and `README.md`: list this document (20 §11).
