<!-- Provenance: format analysis of the Steam 2015 DATA/UI.PAK (PAK2 container, entry table, padding, names), its Scaleform movies (GFX header and tag stream, PlaceObject records, DefineEditText bounds, DoABC2 tags, ABC files: pools, namespaces, classes, traits, method bodies, scope depths, opcode set) and its DDS glyphs, read with the spec side's own tools; a survey of every root movie of the stock archive (stage size, named placements, their bounds and whether they hold text, later-frame matrices) and of every stock `resizeListener`, which is the source of the layout selection in 3.6; the stock game's ActionScript classes in those movies (ImageSubstitution, frontend.GameMenu and its subclasses, frontend.menu.Menu and MenuPage, NewFrontendMenu, LoadingScreen, CraftingScreen, CreditsScreen, PickupOverlay, PopupOverlay, TRCSystemOverlay, WeaponStuff, ChallengeMapOverlay, RadialMenuScreen, Reaction_Game); the game's default input bindings in DATA\INPUT.XML (source of the prompt actions in 3.4); AI.exe 2015 (UI archive path pointer, on_page_changed registration site); 2020 values were taken from the earlier mod's per-build address table (and, for inline sites, its installer) and have not been verified against a 2020 `AI.exe`; r1 used a comparison of the stock archive with the earlier mod's archive to find the movies to touch, and r2 replaces every selection that came from it with the stock survey and the game's binding data above (no value, string, name, code or image is taken from that archive); viewing geometry of 03 and 05 for the layout requirements; observation in game by the spec side (the game opens a UI archive through a redirected path, and an archive whose names are in sorted order loads); user requirements (archive built at launch from the player's own UI.PAK through a compiled bundle, no game or mod asset shipped, players need no tools, unknown UI.PAK leaves the game running). r3 (spec r7, second clean-room review): the key file's name left to the implementation; the in-game observations added to these sources; the crosshair placement chosen by the rules of 3.6.2 from the spec side's own survey of `WEAPONSTUFF` (root placement and script uses of `crosshairs_mc`); the redirect string stated by its form; the fast-path cost reason given as an estimate; the DoABC2 count recounted (230 in the archive, all decoded by the spec side's own reader); the movies' change sets listed per movie; smaller consistency fixes. r4 (spec r7 cross-check): the linking runs only in the game's process on a recognised build whose checks passed (01 §2.4, §3.1), with an acceptance check. r5 (CLEANROOM r3, what a player sees): the labels, help rows and glyph designs a player sees are those the spec states (06 §5, 04 §4.1, §5.7), and the constraint against using another mod now covers its files only; the layout factors fixed at the menu factor players of the earlier mod saw (1.22) and, for the HUD, the smallest factor meeting the project's 80% floor (1.38, project decision), from what players of the earlier mod saw, read by the spec side (menus 1.22, HUD 1.33, subtitles 1.25, crosshair 3.16, the other moves of 3.6.6); what the player sees with skip intro; the UI.PAK message named by its first line. No game fact was added. Revision: r5 -->

# 14. UI resources: the options archive

How the mod's menu changes, VR glyphs and HUD layout changes reach the
game: a bundle compiled from the project's own sources at development time
is embedded in the DLL, and at launch the DLL links it into the player's
own stock movies to produce `DATA\MOTHER.PAK`, which the game then opens
instead of `DATA\UI.PAK`.

What the options page must do, the commands it sends and the values pushed
to it are in 06, which also gives its labels, values and description lines
exactly. Why VR glyphs exist, which VR controls get one and what each
glyph shows are in 04 §4.1; the rows of the controls help page are in 04
§5.7. Where the HUD and the virtual screen sit in the headset is in 05.
When this work runs relative to the rest of start-up is in 01 §2 and 12
§2.3. Which events are logged and what each conveys is in 11 §2.1 and
§2.5; the messages to the player are in 13 §5.3.

Addresses are RVAs (image base 0x400000) in the Steam 2015 `AI.exe` (PE
timestamp 0x54E3199D). 2020 values were taken from the earlier mod's
per-build address table (and, for inline sites, its installer) and have not
been verified against a 2020 `AI.exe`. All archive facts are from the Steam
2015 `DATA\UI.PAK` unless marked. Names such as `UiPakPathPtr` are spec
labels (01); the implementation may rename them.

Markers: [sure] verified on the 2015 files; [likely] strong evidence, not
proven in game; [guess] needs checking.

## 1. Goal and clean-room constraints

### 1.1 Goal

The game's UI is a set of Scaleform movies inside one archive. A VR options
page, VR controller glyphs, prompt fixes and readable HUD and menu layout
can only be added by changing those movies. The project must do that
without shipping any game file and without using another mod's files.

### 1.2 Constraints

1. Nothing the project ships contains bytes of a game file: no `UI.PAK`, no
   movie, no part of a movie, no game image, no binary delta against a game
   file. A delta from the stock archive to any modified archive is excluded
   too, because it encodes the modified movies.
2. Nothing the project ships or builds from comes from another mod's
   files: no archive, movie, ActionScript, bytecode or image. The menu is
   authored here from 06 and this document. The labels, help texts, glyph
   designs and layout factors a player sees are the ones the spec states
   (06 §5, 04 §4.1 and §5.7, 3.6 here); many of them are what players of
   the earlier mod saw, which the spec may state exactly (CLEANROOM r3),
   and the implementation takes them from the spec, never from a mod's
   files.
3. The developer build never reads `UI.PAK`, `MOTHER.PAK` or any mod's
   binary. It works from the project's sources and from the facts in this
   document (class, member and namespace names, signatures, expected sizes
   and hashes). A build machine without the game builds the same bytes.
4. Players need no Java, decompiler, Python or compiler. Everything that
   runs on the player's machine is inside the DLL.
5. `DATA\UI.PAK` is never written, renamed, locked for writing or deleted.
   Removing the DLL makes the game open `UI.PAK` again, and Steam's file
   verification never finds a changed game file.
6. If the player's `UI.PAK` is not one the bundle was made for, the mod
   changes nothing in the UI and the game runs (section 5.9).

Why this design: the movies are the game's copyrighted data and must come
from the player's own installation; the project's changes are its own
expression and must be written here; linking at launch is the only point
where the two meet, and it happens on the player's machine, from the
player's files.

### 1.3 Two change sets

The bundle's changes are grouped. Which groups are linked depends on the
runtime chosen for this run (01 §2.3, 06 §1):

| Set | Linked when | Contents |
|---|---|---|
| Menu set | always (runtime None, OpenVR or OpenXR) | the VR options page and its push receivers (3.1), debug-item visibility (3.1, 3.2), the skip-intro function (3.3) |
| VR set | runtime OpenVR or OpenXR | everything in the menu set, plus VR glyph loading and prompt resolution (3.4), the crafting prompt (3.5), layout changes (3.6) and the glyph images (2.9) |

Why: with runtime None flat play must be the unmodded game except for the
options page (01 §2.3). Layout changes, prompt changes and the crafting
prompt change what a flat player sees, so they are only linked when a VR
runtime is configured. The set is part of the staleness key (5.1): changing
the runtime (which quits the game; the new runtime applies at the next
launch, 06 §4.5) rebuilds the archive once, at that launch.

## 2. Game facts

### 2.1 How the game opens its UI archive

- A pointer at RVA **0x124F748** (2020: 0x125F978), `UiPakPathPtr` in 01,
  holds the VA 0x13E3C34 of the NUL-terminated string `data\ui.pak` [sure].
  The game opens its UI archive through this pointer on the game thread
  during start-up, before the front end shows (01 §2.1). Redirecting the
  pointer to another NUL-terminated string of the same form makes the game
  open that file instead [sure: observed in game by the spec side with a
  redirected path].
- The path is relative. It resolves against the process's current
  directory, which Steam sets to the game folder. The mod's own directory
  (the folder of the proxy DLL) is the game folder in every supported
  launch [likely]; 5.2 says what to do when they differ.
- Movies load images by relative, lower-case names (the stock glyph loader
  appends `.dds` and lower-cases, 2.7.1) while archive names are upper-case,
  so the game's archive lookup ignores case [sure: stock glyphs load].
- The archive is opened once and kept for the session [likely]. The mod
  must not hold the output file open for writing after attach.

### 2.2 Archive format (PAK2)

Little-endian [sure]:

| Offset | Size | Content |
|---|---|---|
| 0 | 4 | magic `PAK2` |
| 4 | 4 | u32 `namesSize`: bytes of the name block |
| 8 | 4 | u32 `count`: number of entries |
| 12 | 4 | u32 `alignment` |
| 16 | `namesSize` | name block: `count` NUL-terminated names in entry order, packed; `namesSize` is exactly the sum of the name lengths plus one NUL each |
| 16 + `namesSize` | 4 x `count` | u32 `end[i]`: absolute file offset where entry *i* ends |
| after the table | | data: entry 0 starts right after the offset table, entry *i* > 0 starts at `end[i-1]`; each start is rounded up to `alignment`, the gap is zero bytes and not part of any entry; the last entry ends at the end of the file |

The Steam 2015 `DATA\UI.PAK` [sure]:

| Property | Value |
|---|---|
| File size | 17,521,556 bytes |
| SHA-256 | `a8a0a68ac597b853133fd206f70b0acc7d5718949cf21a4a26aca1bd07524b43` |
| CRC-32 (whole file) | AA7CF346 |
| Entries | 505 |
| Name block | 16,480 bytes |
| Alignment | 4 |
| First entry offset | 18,516 (= 16 + 16,480 + 4 x 505, no padding) |
| Padded entry starts | 124, all padding bytes zero |
| Name order | ascending byte order, no duplicates |
| Folders | `DATA/UI/` 236, `DATA/UI/SCREENS/` 114, `DATA/UI/MISSING_PERSONS/` 79, `DATA/UI/MAPS/` 51, `DATA/UI/DLC/` 15, `DATA/UI/DATA/` 8, `DATA/UI/LOADING_IMAGES/` 2 |
| Types | 337 `.DDS`, 158 `.GFX`, 8 `.HTML`, 2 `.PSD` |

Names are upper-case with forward slashes (`DATA/UI/PAUSEMENU.GFX`). No
compression: entries are stored as they are used.

Whether the game's lookup needs the sorted order is not known (it may
bisect). An archive that keeps the order loads [sure: observed in game by
the spec side], so the output keeps it (5.7).

The 2020 build's `UI.PAK` is not characterised. Until it is, the bundle
accepts no 2020 identity and the 2020 build gets no options archive (5.9).

### 2.3 Touched entries

The movies the bundle changes, with their identity in the Steam 2015
archive [sure] (index in the entry table, data offset, size, CRC-32 with
the zlib / IEEE 802.3 polynomial over the entry's bytes without padding,
SHA-256 of the same bytes):

| Entry | Index | Offset | Size | CRC-32 | SHA-256 |
|---|---|---|---|---|---|
| `DATA/UI/CHALLENGELEADERBOARDCOMPONENT.GFX` | 7 | 136484 | 49770 | B1149B90 | `ab3be2248646ac19cc4e60c124fc6d0acf1dc7802ed7c4990f2b5710d7a82bf7` |
| `DATA/UI/CHALLENGEMAPOVERLAY.GFX` | 8 | 186256 | 100580 | DF061961 | `840dc63a9a83775d6f0afac133095e0abc1635e1c10f31d394ddc39d9d0464bd` |
| `DATA/UI/CRAFTINGSCREEN.GFX` | 20 | 758372 | 76681 | 560CE12C | `9e013d27353f3387df26f4a6adf41915359c7826c8fe4bf471f6e67b39498e90` |
| `DATA/UI/CREDITSSCREEN.GFX` | 21 | 835056 | 8330 | F49C3E09 | `e63b6a4afe1f93eb1bb22749fe3e36c4d83db4a591ddd0ebfffca1c040127131` |
| `DATA/UI/GAMEOVERMENU.GFX` | 51 | 2112480 | 136327 | 2A6BDA7C | `343d4705ddc2c17c85767619f61b21f2a088321cc95f3c93ccef2a4e357d2ede` |
| `DATA/UI/GAMMASCREEN.GFX` | 52 | 2248808 | 66630 | 0BDF6322 | `088acbd30caa5317674ad67f17f2a8448a416ea5629abc56683a105e3ff26a85` |
| `DATA/UI/HUB.GFX` | 85 | 2654780 | 47269 | 95B354E0 | `538202f8a9f1eb7379fe421b62e2fe569d8f689fe5e26cd438861eb3e0831b95` |
| `DATA/UI/HUB_INVENTORY.GFX` | 101 | 3059364 | 71608 | 9EE59720 | `afaa084a7cb116ea939068447c44673b6aabf675aa3809d430a717c821544ec1` |
| `DATA/UI/HUB_JOURNAL.GFX` | 108 | 3662172 | 59383 | 35A99F8F | `bd49c75429f2ed6f47122a761999391bf8277521816589d379bd39a9fe880c26` |
| `DATA/UI/HUB_LOGS.GFX` | 111 | 3787348 | 54924 | 322E896B | `51cf04cde2f9f8caf6e38e13fbee79262a6f45365cd3e0844210d6ed1776ec4f` |
| `DATA/UI/HUB_MAP.GFX` | 112 | 3842272 | 107652 | 8AA8936B | `e0c96ab3c1ed1c44fc3c5edccf9faa2b763b8d030e290d8f3d74ef956b7f4cca` |
| `DATA/UI/HUB_MISSING.GFX` | 113 | 3949924 | 78654 | 1CCF1FC7 | `f61b4ae507dc48fb27798f319549b67f4cd1ee85ae66036fd06c44d658d3ebc6` |
| `DATA/UI/LOADINGSCREEN.GFX` | 122 | 4140740 | 19585 | CDF572A3 | `fdde0d4adab50903dfbfde70636b711ad0812f3f799060542a2049a8d5d5ded7` |
| `DATA/UI/MOVIEPLAYER.GFX` | 288 | 13593988 | 5676 | 6FDE455B | `bd936f7fe5f35b7023f5b47958fda0b51849af060de70ab58a0794b1f65f85ff` |
| `DATA/UI/NEWFRONTENDMENU.GFX` | 290 | 13604560 | 220324 | 3BCF6656 | `07efd64be02095721faea0e88b6eee4f0f02e5d6b8fc583274208f50ba9eb672` |
| `DATA/UI/PAUSEMENU.GFX` | 299 | 13899636 | 134679 | 36912009 | `56f8b0f3d0dbf44aa4abb5f32bc840182236caf13dc0f82baf48b6feea5e79f3` |
| `DATA/UI/PICKUPOVERLAY.GFX` | 300 | 14034316 | 65768 | EBCA7423 | `abac623189a72180e84b4fd79af5fe11d3f13b6512e8a630504d2a9dde2594e9` |
| `DATA/UI/POPUPOVERLAY.GFX` | 304 | 14117364 | 46794 | 7C3694A6 | `067c9cd5e6bad730bbc422d55f133893a3ba55df9c4adfc5fba8d9b1c5a0994c` |
| `DATA/UI/RADIALMENU.GFX` | 307 | 14168512 | 163252 | 399AA2BC | `db145fe5286614d95bee704614dedd646e20d81ea4a21fcd4f027e790847b1c6` |
| `DATA/UI/REACTION_GAME.GFX` | 309 | 14364660 | 48255 | 62101EDD | `62b4a8574ed9cb083845a02d837c5069bae524149092543e05117c53d0fe0aa9` |
| `DATA/UI/SUBTITLES.GFX` | 480 | 16666164 | 2490 | 068713A4 | `937abb78b1269ed920e2e868e6b786d207388d89dc7823558f5bb684855db838` |
| `DATA/UI/TERMINALINTERACTION.GFX` | 481 | 16668656 | 60163 | CB7C7FE5 | `9a23469ba0a6f5f4771cda0d504d1e4eca14ab7b786044e171413f66cdb3d88e` |
| `DATA/UI/TRCSYSTEMOVERLAY.GFX` | 484 | 16748924 | 30737 | AEFCF9C5 | `0daa26b498c891ad0a418ee6f526d66cb22d1bf0f1bf3592dafbb36d1199fee5` |
| `DATA/UI/WEAPONSTUFF.GFX` | 504 | 17449868 | 71688 | 09984F4E | `b6d0b4bbfd262a84043de0efeeeaf74796b76fe5ae535a2942e63c1227f59e17` |

Why these 24: each carries at least one change of section 3. Per movie
(3.1 to 3.3 are the menu set, 3.4 to 3.6 the VR set):

| Movie | Changes |
|---|---|
| `NEWFRONTENDMENU` | 3.1, 3.2, 3.4, 3.6.2 |
| `PAUSEMENU`, `GAMEOVERMENU` | 3.1, 3.4, 3.6.2 |
| `LOADINGSCREEN` | 3.3, 3.6.2 |
| `CRAFTINGSCREEN` | 3.4, 3.5 |
| `WEAPONSTUFF` | 3.4, 3.6.2, 3.6.3, 3.6.4 |
| `PICKUPOVERLAY`, `TRCSYSTEMOVERLAY` | 3.4, 3.6.3 |
| `POPUPOVERLAY` | 3.6.3 |
| `CREDITSSCREEN`, `MOVIEPLAYER`, `SUBTITLES` | 3.6.2 |
| `CHALLENGELEADERBOARDCOMPONENT`, `CHALLENGEMAPOVERLAY`, `GAMMASCREEN`, `HUB`, `HUB_INVENTORY`, `HUB_JOURNAL`, `HUB_LOGS`, `HUB_MAP`, `HUB_MISSING`, `RADIALMENU`, `REACTION_GAME`, `TERMINALINTERACTION` | 3.4 |

The menu set touches `NEWFRONTENDMENU`, `PAUSEMENU`, `GAMEOVERMENU` and
`LOADINGSCREEN`; the VR set touches all 24.

New glyph entries (2.9) are inserted at their sorted positions; their
names must not equal any stock entry name.

### 2.4 Movie container (GFX)

Every `.GFX` entry is an uncompressed Scaleform movie [sure]:

| Offset | Content |
|---|---|
| 0 | signature `GFX` (3 bytes; no compressed `CFX` movie occurs in the archive) |
| 3 | version byte: 11 in all touched movies (10 occurs in other movies) |
| 4 | u32 total file length, equal to the entry size |
| 8 | frame size: a SWF `RECT` (5-bit `Nbits`, then four signed `Nbits`-bit fields, padded to a byte); `Nbits` is 13 to 17 in the archive, 16 in most touched movies |
| then | u16 frame rate (8.8 fixed, 24.0 in touched movies), u16 frame count |
| then | the tag stream to the `End` tag (code 0), which is the last two bytes |

Each tag starts with a u16 `code << 6 | length`. A length of 0x3F means a
u32 length follows (long form). The stock movies use the long form for many
tags shorter than 63 bytes (for example 1,227 `PlaceObject2` tags), so a
writer must not assume short form for short tags.

Tags that matter here:

| Code | Tag | Use |
|---|---|---|
| 82 | `DoABC2` | u32 flags (1 = lazy initialise in every stock tag), NUL-terminated name, then an ABC file (2.6) |
| 76 | `SymbolClass` | character id to class name; unchanged by the bundle |
| 39 | `DefineSprite` | u16 sprite id, u16 frame count, then a nested tag stream ending in `End`; its length covers the nested tags |
| 26 | `PlaceObject2` | display-list placement (2.5) |
| 70 | `PlaceObject3` | placement with a class name (2.5) |
| 37 | `DefineEditText` | u16 character id, then a `RECT` (the field's bounds in twips), then the field's flags and the rest of the record (2.5) |
| 1000 | Scaleform exporter info | carries the movie's name; unchanged |
| 69, 77, 9, 86, 71 | `FileAttributes`, `Metadata`, `SetBackgroundColor`, `DefineSceneAndFrameLabelData`, `ImportAssets2` | unchanged |

DoABC2 tags per touched movie [sure]: one tag holding every class in 21 of
the 24; `RADIALMENU.GFX` has 69 (one class each, named after the class:
`ImageSubstitution` is the first); `TRCSYSTEMOVERLAY.GFX` has 10
(`ImageSubstitution` in the first, `TRCSystemOverlay` in the sixth);
`WEAPONSTUFF.GFX` has 30 (`ImageSubstitution` in the first, `WeaponStuff`
in the twelfth). The linker finds a class by its qualified name, never by
tag position, and requires exactly one definition of it across the movie's
DoABC tags.

### 2.5 Placements and text field bounds

The layout edits of 3.6 change the transform of a placed display object or
the bounds of a text field. Record layout (SWF 10 file format, as used by
these movies):

- `PlaceObject2`: u8 flags (bit 0 move, bit 1 has character, bit 2 has
  matrix, bit 3 has colour transform, bit 4 has ratio, bit 5 has name,
  bit 6 has clip depth, bit 7 has clip actions), u16 depth, then in order:
  character id (u16), `MATRIX`, `CXFORMWITHALPHA`, ratio (u16), name
  (NUL-terminated), clip depth (u16), clip actions.
- `PlaceObject3`: u8 flags as above, u8 flags2 (bit 0 has filter list,
  bit 1 has blend mode, bit 2 has cache as bitmap, bit 3 has class name,
  bit 4 has image, bit 5 has visible, bit 6 opaque background), u16 depth,
  then class name (NUL-terminated, when bit 3 of flags2 is set, or bit 4
  and the character bit), character id, `MATRIX`, `CXFORMWITHALPHA`, ratio,
  name, clip depth, filter list, blend mode, cache flag, visible and
  background colour, clip actions, each when its flag is set.
- `MATRIX` is bit-packed from the most significant bit: 1 bit has-scale
  (then 5-bit `n` and two signed `n`-bit 16.16 fixed values ScaleX,
  ScaleY), 1 bit has-rotate (then 5-bit `n` and two signed `n`-bit 16.16
  values RotateSkew0, RotateSkew1), 5-bit `n` and two signed `n`-bit
  translate values in twips (1/20 pixel); then padding to a byte.
- `DefineEditText`: u16 character id, `RECT` bounds (twips), padded to a
  byte, then two flag bytes (first byte, from the most significant bit:
  has text, word wrap, multiline, password, read-only, has colour, has
  max length, has font) and the rest of the record. A placement's
  registration point is the field's local origin; the bounds are relative
  to it.

A placement is identified by its container (the root timeline or a
`DefineSprite` id), its depth, and its character id or class name and
instance name. A placement whose later frames carry the move flag with no
matrix keeps the matrix of its first placement, so one edit holds in every
frame; a later frame that carries a matrix replaces it. Coordinates below
are in pixels of the movie's stage (twips / 20). Scaling a placement scales
its content about its registration point (its translate).

### 2.6 ActionScript bytecode (ABC)

All touched classes are ActionScript 3 in ABC files version **46.16**
(major 46, minor 16) [sure]. The parts the linker reads and writes, in file
order (u30 is the variable-length unsigned integer of the ABC format;
pools index from 1, entry 0 is implicit):

1. `minor_version` u16, `major_version` u16.
2. Constant pools: `int` (s32 as u30-encoded), `uint`, `double` (8 bytes),
   `string` (u30 length plus UTF-8), `namespace` (u8 kind plus string
   index), `ns_set` (count plus namespace indices), `multiname` (u8 kind
   plus fields).
3. `method_info[]`: param count, return type (multiname index, 0 = any),
   param types, name (string index; 0 in every stock method), flags,
   optional values (when flag 0x08), param names (when flag 0x80).
4. `metadata_info[]`.
5. `instance_info[]` and `class_info[]` (same count), then
   `script_info[]`.
6. `method_body_info[]`: method index, `max_stack`, `local_count`,
   `init_scope_depth`, `max_scope_depth`, code, exception table, traits.

Namespace kinds in the touched classes: package (0x16), private (0x05),
protected (0x18), static protected (0x1A), package internal (0x17),
explicit (0x19), namespace (0x08). Multiname kinds seen: QName (0x07),
Multiname (0x09), MultinameL (0x1B), RTQName and TypeName (0x1D, for
`Vector.<T>`).

Facts about the touched classes [sure]:

- Instance flags are 0x09 (sealed, has protected namespace) for every
  touched class.
- Every slot and const trait has `slot_id` 0 and every method trait has
  `disp_id` 0, so the AVM assigns slots in trait order. Appending traits
  at the end of a class's instance traits keeps every existing slot
  number.
- Private members of a class use one private namespace per class. Its name
  differs between movies: empty in most, `ImageSubstitution` for that
  class in `HUB_MAP`, `HUB_MISSING`, `RADIALMENU`, `TRCSYSTEMOVERLAY` and
  `WEAPONSTUFF`. Private namespaces are matched by the class's own traits,
  never by name (5.4).
- Method body scope depths (`init_scope_depth`, `max_scope_depth`) are
  fixed per class:

| Class | Instance methods | `iinit` | `cinit` | Script init |
|---|---|---|---|---|
| `ImageSubstitution` (all 19 movies) | 4, 5 | 4, 5 | 3, 4 | 1, 3 |
| `frontend.GameMenu` | 11, 12 | 11, 12 | 10, 11 | 1, 10 |
| `frontend.NewFrontendMenu` | 12, 13 (two methods 12, 16) | 12, 13 | 11, 12 | 1, 11 |
| `LoadingScreen` | 10, 11 | 10, 11 | 9, 10 | 1, 9 |
| `CraftingScreen` | 11, 12 (one method 11, 15) | 11, 12 | 10, 11 | 1, 10 |
| `PickupOverlay`, `TRCSystemOverlay`, `WeaponStuff` | 10, 11 | 10, 11 | 9, 10 | 1, 9 |
| `PopupOverlay` | 11, 12 | 11, 12 | 10, 11 | 1, 10 |

- The stock code uses 111 distinct opcodes, all from the documented AVM2
  set plus `findpropglobalstrict` (0x5B) and `findpropglobal` (0x5C),
  including `lookupswitch` and the debug opcodes `debug` (0xEF),
  `debugline` (0xF0) and `debugfile` (0xF1). The archive holds 230
  top-level DoABC2 tags in 124 movies (69 in `RADIALMENU`, 30 in
  `WEAPONSTUFF`, 10 in `TRCSYSTEMOVERLAY`, one in each other movie that has
  any; 130 of them in the 24 touched movies) and no `DoABC` (code 72) tag;
  every one decodes completely with these operand formats, and their
  method bodies together use exactly these 111 opcodes [sure].
- The same class is a separate copy in each movie, with its own pool
  indices. Copies of `ImageSubstitution` differ slightly between movies
  (some add mappings such as `leaderboard` and `player_info`, some have
  different code size); this is why changes are linked as hooks that call
  the movie's own original (section 3) rather than as replacement bodies.

### 2.7 Stock classes and members involved

Names below are those in the game's movies. Types are fully qualified
(`flash.display::BitmapData`); `*` means untyped. "ns" is the namespace
kind of the trait. These tables are what the project's declaration files
(4.2) are written from.

#### 2.7.1 `ImageSubstitution` (package-level, in 19 movies)

The 19 movies: `CHALLENGELEADERBOARDCOMPONENT`, `CHALLENGEMAPOVERLAY`,
`CRAFTINGSCREEN`, `GAMEOVERMENU`, `GAMMASCREEN`, `HUB`, `HUB_INVENTORY`,
`HUB_JOURNAL`, `HUB_LOGS`, `HUB_MAP`, `HUB_MISSING`, `NEWFRONTENDMENU`,
`PAUSEMENU`, `PICKUPOVERLAY`, `RADIALMENU`, `REACTION_GAME`,
`TERMINALINTERACTION`, `TRCSYSTEMOVERLAY`, `WEAPONSTUFF`.

| Member | ns | Signature | Stock behaviour |
|---|---|---|---|
| `load_image_files` | public | `(String):void` | switch on the platform string: `"PC"` and `"360"` load the `glyph_360_*` set (23 calls, ids `a`, `b`, `back`, `dpad`, `dpad_down`, `dpad_left`, `dpad_right`, `dpad_up`, `dpad_x`, `dpad_y`, `lb`, `lstick_button`, `lstick`, `lt_button`, `lt`, `rb`, `rstick_button`, `rstick`, `rt_button`, `rt`, `start`, `x`, `y`); `"DURANGO"`, `"ORBIS"`, `"PS3"` load other sets |
| `load_image_file` | public | `(String,String):void` | `(file, inputId)`: appends `.dds`, lower-cases, records `inputId -> file`, counts one more pending image and starts an asynchronous `flash.display::Loader` on the relative URL |
| `on_all_images_loaded` | private | `():void` | called when every pending image has completed or failed; adds all text substitutions (action mappings, `$id$` button tokens, `@id@` UI tokens), sets `m_initialised`, calls the registered loaded callback |
| `initialise` | public | `(String,Boolean,Boolean,uint=):void` (3 optional) | `(platform, keyboard, forceUpdate, flags)`; loads images the first time, otherwise re-runs `on_all_images_loaded` |
| `Add_Substitution` | public | `(String,flash.display::BitmapData,String):void` | appends `{subString, image, ...}` to `substitutions` |
| `Add_Substitution_From_Mapping` | public | `(String,String):*` | `#id#` token for a game action, image of the bound control (none with keyboard) |
| `Add_Substitution_For_Button` | public | `(String):*` | `$id$` token showing the image of input id `id` |
| `Add_Substitution_For_UI` | public | `(String,String,String):*` | `(id, keyName, inputId)`: `@id@` token, localised key name, image of `inputId` (none with keyboard) |
| `find_button_object_from_name` | public | `(String):flash.display::BitmapData` | image for an input id, or null |
| `get_button_from_action_mapping` | public | `(String):String` | `ExternalInterface.call("get_button_from_action_mapping", action)`: the input id of the control bound to a game action (the native side answers through `ControlToName`, 04 §4) |
| `get_localised_button_name` | public | `(String):String` | `ExternalInterface.call("get_localised_button_name", key)` |
| `using_keyboard` | public | `():Boolean` | |
| fields | private | `substitutions:Array`, `button_to_file_mapping:Object`, `file_to_image_mappings:Object`, `text_to_bitmapdata:Object`, `all_images_count:int`, `loaded_images_count:int`, `m_loaded_callback:Function`, `current_platform:String`, `current_baseline:Number`, `m_initialised:Boolean`, `m_keyboard:Boolean`, `m_force_update:Boolean`, `m_substitution_flags:uint` | |

In the stock `on_all_images_loaded`, the button tokens `$a$`, `$b$`, `$x$`,
`$y$` are added through `Add_Substitution_For_Button` with the fixed input
ids `a`, `b`, `x`, `y`, and the UI tokens `@confirm@` and `@cancel@`
through `Add_Substitution_For_UI("confirm", "return", "a")` and
`("cancel", "escape", "b")` (swapped on PS3 only). So these prompts always
show the pad's A / B / X / Y image, whatever control the action is bound
to.

The stock action mappings (`Add_Substitution_From_Mapping`) ask the native
side for actions of both of the game's input maps of 2.10: player actions
(`use`, `back_close`, `reload_weapon`, `select_torch`, ...) and menu actions
(`cancel`, `alt_1`, `alt_2`, `craft_item`, `start`, `zoom_in`, ...) [sure].
So `get_button_from_action_mapping` answers for actions of either map
[likely: the stock movies rely on it].

#### 2.7.2 `frontend.GameMenu` (in `NEWFRONTENDMENU`, `PAUSEMENU`, `GAMEOVERMENU`)

Extends `display.MovieClipEx`. Constructor `(String)` (the
ExternalInterface prefix). Protected namespace name `frontend:GameMenu`.

| Member | ns | Signature or type | Use |
|---|---|---|---|
| `_optionsItem`, `_gameOptionsItem`, `_videoOptionsItem`, `_audioOptionsItem` | protected | `frontend.menu::MenuItemExpandable` | the Options page and its three stock entries |
| `_loadGameItem`, `_debugCheckpointsItem` | protected | `frontend.menu::MenuItemExpandable` | Load Game and "Trigger Debug Checkpoint" |
| the Video items listed in 06 §5.3 (`_fieldOfViewItem`, `_deepColourItem`, ..., `_stereoModeItem`) | protected | `frontend.menu::MenuItemOptionSlider` or `MenuItemOptionOptionStepper` | greyed in VR (06 §5.3) |
| `set<Item>Data` for each of those items (`setFieldOfViewData`, `setDeepColourData`, ..., `setStereoModeData`) | public | `(frontend.menu::MenuItemDataSequence):void` | the game's data pushes; each replaces the item's `MenuItemData`, so `enabled` set earlier is lost (06 §7.5) |
| `setAdvancedVideoOptionsVisible` | public | `(Boolean):void` | shows or hides the advanced Video items (06 §7.5) |
| `_imageSubstitution` | protected | `ImageSubstitution` | the movie's prompt helper |
| `menu` | public | `frontend.menu::Menu` | |
| `call` | protected | `(String, ...rest):*` | `ExternalInterface.call(prefix + name, ...)` |
| `setIds`, `setDescriptions`, `setTransitions`, `setVisibility` | protected | `():void` | set-up methods the constructor calls in this order after attaching every item; each is overridden by `NewFrontendMenu`, `PauseMenu` and `GameOverMenu`, and every override calls `super` first |
| `menuPageChangeHandler` | protected | `(frontend.events::MenuEvent):void` | page-change listener: `call("on_page_changed", event.page.id.value)` then `call("refresh_profile_picture")` |
| `videoOptionsBeforeBackHandler` | protected | `(frontend.events::MenuPageEvent):void` | stock refresh on leaving Video |
| `setItemVisible`, `setItemEnabled` | public | `(int,Boolean):void` | by item id |

Stock construction order [sure]: the subclass constructor builds its own
items, then calls `super(prefix)`. The `GameMenu` constructor builds every
`GameMenu` item, attaches Game, Video and Audio to `_optionsItem.page`,
builds the sub-pages, then calls `setIds()`, `setDescriptions()`,
`setTransitions()`, `setVisibility()`, then adds the `BEFORE_BACK`
listeners and the menu's `PAGE_CHANGE` and `INDEX_CHANGE` listeners. Back
in the subclass constructor, after `super`, the subclass creates its root
`MenuPage`, sets its id to the fixed `MenuPageId.ROOT`, fills it, and
assigns `menu.rootPage`. `GameMenu.setVisibility` hides, among others,
`_debugCheckpointsItem`.

Ids and maps (game facts, also in 06 §7.3):

- `frontend.enum.MenuPageId` has fixed ids 0 to 13 (`OPTIONS` = 1,
  `VIDEO_OPTIONS` = 3, `LOAD_GAME` = 8, `DEBUG_CHECKPOINTS` = 10,
  `SELECT_LEVEL` = 13) and `MenuItemId` fixed ids such as
  `DEBUG_CHECKPOINTS` = 12 and `DEBUG_LOAD_LEVEL` = 15. Every `MenuPage`
  (including the page every `MenuItemExpandable` creates) and every
  `MenuItemData` takes the next automatic id (300 up, `getNextAutoId()`)
  when it is constructed, from a counter per movie, in creation order.
- The only stock objects the three constructors create after
  `setVisibility` has run are the subclass's root `MenuPage`, whose
  automatic id is replaced at once by `MenuPageId.ROOT` [sure]. Objects
  created in a `setVisibility` hook therefore take automatic ids after
  every stock page and item that keeps its automatic id.
- Items the game adds later at run time take later automatic ids:
  debug checkpoints (`addDebugCheckpoint`, not added to the item map),
  input-mapping rows (`addInputMappingItem`, addressed through their
  `data` index) and mission checkpoints (`addMissionCheckpoint`, which
  sets a fixed id from native code and calls `menu.addItemToMap`). Their
  automatic ids shift by the number of objects the mod creates, but the
  game addresses none of them by automatic id [sure for the script side].
- Assigning `menu.rootPage` builds the page and item maps
  (`rebuildItemMap`, `rebuildPageMap`). The game's `setCurrentPage(pageId,
  index)`, `setItemVisible`, `setItemEnabled` and `setItemDescription`
  find pages and items only through those maps. The stock front end
  re-assigns `menu.rootPage = menu.rootPage` to rebuild them after
  changing pages on console platforms, and `menu.addItemToMap(item)` adds
  one item.

#### 2.7.3 Subclasses

| Class | Movie | Members used |
|---|---|---|
| `frontend.NewFrontendMenu` | `NEWFRONTENDMENU` | `_selectLevelItem` (private, `MenuItemExpandable`, label "Select Level", item id `DEBUG_LOAD_LEVEL` = 15, on the Campaign page), `_campaignItem` (private), `campaignHandler` (private, `(frontend.events::MenuPageEvent):void`, the Campaign expandable's open handler; its stock body is empty); its `setVisibility` override calls `super` and then sets `_selectLevelItem.visible = false` |
| `frontend.PauseMenu` | `PAUSEMENU` | none beyond `GameMenu` |
| `frontend.GameOverMenu` | `GAMEOVERMENU` | none beyond `GameMenu` |

#### 2.7.4 `LoadingScreen` (in `LOADINGSCREEN`)

| Member | ns | Signature or type | Use |
|---|---|---|---|
| `ShowBootup` | public | `(Boolean,int):*` | `(isPC, language)`: sets `playlist` to one of the four startup playlists (PC or console, French or not), sets up the video stream, then calls `NextEntry()` |
| `NextEntry` | private | `():*` | plays the next playlist entry; when the playlist is exhausted calls `ExternalInterface.call("FinishedBootup", 0)` |
| `startup_playlist_pc`, `startup_playlist_pc_french`, `startup_playlist_console`, `startup_playlist_console_french`, `playlist` | private | `Array` | flat lists of (kind, value) pairs; kinds are the internal consts `VIDEO` = 0, `MOVIECLIP` = 1, `DELAY` = 2 (value in milliseconds), `IMAGE` = 3. Every stock playlist separates its entries with `DELAY, 500` |

#### 2.7.5 `CraftingScreen` (in `CRAFTINGSCREEN`)

`init_image_substitution(String,Boolean,Boolean):void` (public):
`(platform, keyboard, forceUpdate)`. It stores `keyboard` in
`_usingKeyboard` (private, Boolean), initialises `_imageSubstitution`, then
shows the craft button in its mouse form (localisation key
`AI_UI_CRAFTING_CRAFT_MOUSE`, button not selected) when `keyboard` is true,
or its pad form (`AI_UI_CRAFTING_CRAFT_GAMEPAD`, selected) otherwise.

#### 2.7.6 HUD layout methods

The stock archive has a public `resizeListener()` in exactly seven movies
[sure: no other movie contains the name]. The game calls it on the movie's
root class. Each computes `k = min(stageWidth / Wref, stageHeight / Href)`
from two reference sizes pushed at the start of the method (each with
`pushshort`, 3 bytes: opcode 0x25 and a 2-byte u30), then sets elements'
`width`, `height`, `x` and `y` as multiples of `k`, or positions some
elements only:

| Movie, class | `Wref`, `Href` | Root class? | Elements it sizes and places (design box `w x h`, position) |
|---|---|---|---|
| `WEAPONSTUFF`, `WeaponStuff` | 1280, 720 | yes | `mc_healthbarroot` 280 x 28 at `(96k, H - 74k)`, `ammo_mc` 280 x 28 at `(96k, H - 78k)`; positions only: `crosshairs_mc` at the centre, `grapplePrompt_mc` and `grapplePrompt_scripted_mc` at `(W/2, H/2 + 146)`, `BreathingGame_mc` at `(W/2 - 24, H/2 + 69)` (stage pixels, not `k`) |
| `PICKUPOVERLAY`, `PickupOverlay` | 1280, 720 | yes | eleven text-bearing elements, listed with their results in 3.6.3; `breathingtutorial_mc` positioned only, at `(W - 40, H - 40)` |
| `TRCSYSTEMOVERLAY`, `TRCSystemOverlay` | 1280, 720 | yes | movie-skip prompt 376 x 28 at `(W - 130k, H - 35k)`, movie-loading icon, saving and loading icons near the top right |
| `POPUPOVERLAY`, `PopupOverlay` | 1280, 720 | yes | the current dialog popup, `1280k x 1300k` at the stage centre; the second 1280 is the popup symbol's design width (its full-stage backdrop is 1280 units wide, so its scale is `k`) |
| `CHALLENGEMAPOVERLAY`, `ChallengeMapOverlay` | 1280, 720 | yes | the Survivor-mode HUD and result screens (objectives, timers, title card, result panels, `continuePrompt_mc` 1112 x 68), and passes `k` to the leaderboard component |
| `RADIALMENU`, `RadialMenuScreen` | **1290, 730** | yes | `radial_menu_mc` 1280 x 620 at the centre, `holdToCraft_mc` 288 x 60 at `(W - 80k, H - 95k)`, `decoration_counter_mc` positioned only |
| `REACTION_GAME`, a copy of `PickupOverlay` | 1280, 720 | **no**: the movie's root class is `Reaction_Game` (an 820 x 720 device minigame), and no symbol uses `PickupOverlay` | never runs [likely] |

Elements are placed by their registration point: for example the health
bar's registration point is its top-left corner and the ammo counter's is
its bottom-left corner [sure: from their placement bounds].

#### 2.7.7 Native interface the movies use

- Invoke by name: the game calls public functions of a menu movie's root
  class through the script invoke methods of 06 §4.3. New public methods
  added to `GameMenu` are reachable the same way.
- `ExternalInterface.call` names relevant here: from `GameMenu.call` with
  the movie prefix (`"fe_"`, `""`, `"go_"`, 06 §7.4): `on_page_changed`,
  `on_item_selected`, `update_debug_checkpoints`; without prefix, from
  `ImageSubstitution`: `get_button_from_action_mapping`,
  `get_localised_button_name`, `is_cross_and_circle_swapped`; from
  `LoadingScreen`: `FinishedBootup`, `StartedNewMovie`, `PlayMovieSound`.
  The mod's own commands go straight to `ExternalInterface.call` (06 §4.1).
- The registration of `on_page_changed` in `AI.exe`: at RVA 0x4503B0 the
  instruction `68 A4 0B 30 01` pushes VA 0x1300BA4, the string
  `on_page_changed` [sure for the push; the handler it registers is not
  traced].

### 2.8 Strings

Stock labels are localisation keys (`AI_UI_MENU_OPTIONS`,
`AI_UI_MENU_VIDEO`, ...), resolved by the game's text system, which is not
in `UI.PAK`. A literal string in a label is shown as it is: the stock "Select
Level" and "Trigger Debug Checkpoint" items use literal English. The
project's labels, values and description lines are therefore literal
English strings, exactly as 06 §5 gives them (the labels players of the
earlier mod knew, where the behaviour is unchanged); the entry's label is
`MotherVR` so existing guides apply, and the runtime value 1 is shown as
"OpenXR" (06 §5.2). The controls help rows are literal strings sent by the
DLL (04 §5.7). No localisation entry is added.

### 2.9 Glyph images

All 21 stock pad glyphs (`DATA/UI/GLYPH_360_*.DDS`) have one format [sure]:

| Field | Value |
|---|---|
| File size | 1152 bytes (128-byte header, 1024 bytes of data) |
| Magic, header size | `DDS `, 124 |
| Flags | 0x00081007 (caps, height, width, pixel format, linear size) |
| Width x height | 32 x 32 |
| Linear size | 1024 |
| Depth, mip count | 0, 0 (one level) |
| Pixel format | flags 0x4 (FourCC), FourCC `DXT5` (BC3) |
| Caps | 0x1000 (texture) |
| Reserved words | contain an encoder tag in the stock files; not read by the game [likely] |

The game loads glyphs through the movie (2.7.1), so a new glyph is a new
archive entry under `DATA/UI/`, named after the file the movie asks for,
upper-cased, with the `.DDS` extension. The VR set adds one glyph per
glyph id of 04 §4.1 (one for each VR control a default binding gives a
prompted function, plus the unbound glyph, and any further glyphs the
implementation provides), in the stock format above. File names and input
ids are the implementation's choice under these constraints:

- each input id is registered in the movie in lower case (the stock
  loader lower-cases file names, and `ControlToName` names are looked up
  case-insensitively, 04 §4.1);
- no input id equals a stock input id (`a`, `lb`, ...), and no archive
  name equals a stock entry name;
- the native side (04 §4) returns exactly the input ids the movies
  register.

The images are drawn by this project (4.2) to the designs of 04 §4.1:
32 x 32 pixels with transparency, colourless, in the size and style of the
stock glyphs. Call the number of glyphs `G`;
the VR-set output has 505 + `G` entries (5.7).

### 2.10 Default input bindings (`DATA\INPUT.XML`)

The game's default bindings are a plain XML file in the game folder,
`DATA\INPUT.XML` (20,802 bytes in the Steam 2015 install) [sure]. Its
`<player_input>` section holds the gameplay map: the default gamepad
device and alternates (presets 1 to 5, including left-handed and remote
play). Its `<menu_input>` section holds the menu map. Face buttons of the
default gamepad device:

| Pad control | Player map (default and every preset) | Menu map |
|---|---|---|
| A | `use` (also `button_mash`) | `ok` (also `minigame_bottom`) |
| B | `back_close` | `cancel` |
| X | `reload_weapon` (also `activate_item`) | `alt_1` (also `minigame_middle`) |
| Y | `select_torch` | `alt_2` (also `minigame_top`) |

The keyboard part of the menu map binds `ok` to `return` and `space` and
`cancel` to `escape`, the key names the stock `@confirm@` and `@cancel@`
tokens show (2.7.1). This is why those tokens are the menu map's `ok` and
`cancel`.

## 3. Required menu and UI changes

Each change names the stock class and member it modifies and how. Kinds:

- **hook**: the stock method's trait is pointed at the project's method;
  the stock method is kept, unchanged, under an **alias** (a new trait of
  the same class, same namespace kind, a name of the project's choosing
  that collides with nothing), and the hook calls it through the alias
  where the stock behaviour is to be preserved;
- **add**: a new method or field trait appended to a stock class;
- **layout**: a checked edit of a placement transform, of a text field's
  bounds or of a constant operand (3.6).

The names of new members, of the messages between the menu and the DLL,
of the skip-intro function and of glyph files and input ids are the
implementation's choice (06 §4 says what each message carries).

### 3.1 `frontend.GameMenu` (menu set; three movies)

| Change | Kind | Required behaviour |
|---|---|---|
| VR page construction | hook `setVisibility` | call the alias first (stock visibility, including the hidden debug checkpoint item), then build the VR entry and its page as 06 §5.1 to §5.4 require and attach it to `_optionsItem.page` after the stock Audio entry, hidden until the first push (06 §5.1) |
| Video greying | hooks `menuPageChangeHandler`, each `set<Item>Data` of the Video items listed in 06 §5.3, and `setAdvancedVideoOptionsVisible` | each hook calls its alias first (so `on_page_changed`, `refresh_profile_picture` and the game's data still reach their stock code); then, when the Video page is current or the item's data was just replaced, set `enabled` on the Video items listed in 06 §5.3 from the last pushed "VR running" value |
| Push receivers | add (public methods) | one public method per value pushed by 06 §4.3, each storing the value and updating the page's controls without sending a command (06 §5.2: controls send only when the player changes them); the first push shows the VR entry |
| Pushed state | add (fields) | fields holding the pushed values (VR running, runtime, `DebugOptions`, ...) with constant defaults of 06 §5.4; objects (items, arrays) are created in the hook, not as field defaults |
| Debug checkpoint visibility | in the `DebugOptions` push receiver and the `DebugOptions` control's handler | in the pause menu, `_debugCheckpointsItem.visible` follows `DebugOptions` (06 §5.5); in the front end it is never shown (it stays as the stock set-up left it, hidden); the game-over menu receives no push, so it stays hidden there too |
| Commands | in the VR page's handlers | `ExternalInterface.call("<command> <value>")` directly, not through `call` (06 §4.1) |

Why `setVisibility` for the page (game facts of 2.7.2):

- it runs once per menu instance, after every stock `GameMenu` item and
  every subclass item built before `super` exist, and before the stock
  listeners are added; the three subclass overrides call `super` first, so
  the page is built in all three movies;
- every stock page and item that keeps its automatic id was created
  before it, so the VR objects take later ids and **no stock id is
  renumbered** (06 §5.1);
- it runs before the subclass assigns `menu.rootPage`, so the VR pages
  are in the page and item maps and the game can restore them with
  `setCurrentPage` (06 §5.1).

Also required: no stock item's construction is moved or reordered, and
no stock page or item is created by the project's code.

The VR entry exists in all three movies so that their item ids stay
alike, and it becomes visible at the first push. The front-end and pause
menus receive pushes; the game-over menu receives none (06 §4.4), so the
VR page is hidden there and its Video items stay as in the stock game (06
§5.1, §5.3).

Why greying is hooked on the data pushes too: the game replaces an item's
`MenuItemData` in its `set<Item>Data` methods, which resets `enabled`
(2.7.2). Re-applying the rule on entry to the Video page alone would hold
only if the game never pushed data while that page is open, which is not
known; re-applying it after each replacement and after the advanced items
are shown or hidden makes the rule of 06 §5.3 hold in every case.

### 3.2 `frontend.NewFrontendMenu.campaignHandler` (menu set)

Required (06 §5.5): in the front end, whenever the Campaign page is shown,
"Select Level" (`_selectLevelItem`) is visible exactly when `DebugOptions`
is on, using the value pushed into the field of 3.1.

Hook point: `campaignHandler`, the Campaign expandable's open handler.
Why there: the stock `NewFrontendMenu.setVisibility` hides
`_selectLevelItem` after its call to `super`, so a hook on
`GameMenu.setVisibility` runs too early to set it, and the open handler
runs every time the page that holds the item opens. The hook calls the
alias (empty in stock, kept so a later game update that fills it still
runs), then sets the item's visibility. "Trigger Debug Checkpoint" needs
nothing here: the front end never shows it (3.1).

### 3.3 `LoadingScreen`: the skip-intro function (menu set)

Add a public method to `LoadingScreen` with the `ShowBootup` signature
`(Boolean,int):*` (`isPC`, `language`) under a name of the
implementation's choosing: the skip-intro function. The patch of 06 §6
points the game's `ShowBootup` push at a NUL-terminated copy of that name
owned by the DLL, so the DLL and this movie must use the same name.

Required behaviour: the same set-up as `ShowBootup` with a playlist that
holds only one `DELAY` entry of the stock gap length (500 ms, the gap
between entries in every stock startup playlist, 2.7.4), so that nothing
is played and `FinishedBootup` reaches the game about half a second later,
as it does after the last entry of a stock playlist. `ShowBootup` itself
is not changed. What the player sees: about half a second of black
instead of the logo videos and the legal screen, then the title screen
(05 §11), as players of the earlier mod saw it with the same option.

### 3.4 `ImageSubstitution` (VR set; 19 movies)

| Change | Kind | Required behaviour |
|---|---|---|
| Load VR glyphs | hook `load_image_files` | call the alias; then, when the platform string is `"PC"` or `"360"`, register every glyph of 2.9 with `load_image_file(file, inputId)`. The stock loader counts these with its own images, so `on_all_images_loaded` still runs once, after the last one [likely, 7]; a missing glyph file only counts as a failed load |
| Button tokens | hook `Add_Substitution_For_Button` | for `a`, `b`, `x`, `y`: add the token `$a$` (and so on) with the image of the control bound to the face button's player-map action of 2.10 (`use`, `back_close`, `reload_weapon`, `select_torch`; input id from `get_button_from_action_mapping`, image from `find_button_object_from_name`); if that yields no image, call the alias (stock A / B / X / Y image). Any other id: call the alias |
| UI tokens | hook `Add_Substitution_For_UI` | for `confirm` and `cancel`: the same, with the menu-map actions `ok` and `cancel` (2.10), keeping the stock rule that keyboard mode shows no image and the stock localised key name; any other id: call the alias |

Why hooks at these three methods: they are public, have the same signature
in all 19 copies, and are called by every copy's own
`on_all_images_loaded`, so the per-movie differences (2.6) are kept.

Why these actions: under the game's default bindings (2.10) each is the
action on that face button, so the token keeps meaning "the button this
prompt has always named", and once VR controllers drive the pad through
virtual control codes (04) the native side answers with the VR glyph of
the control that drives it. The fixed A / B / X / Y image would name a pad
button the player in the headset does not hold.

[guess] Two choices are not yet confirmed in game:

- `$y$` and the player map: Y is `select_torch` in the player map and
  `alt_2` in the menu map. Under the default bindings both give the same
  pad control, so the glyph is the same; they differ only after the player
  rebinds. What to check: in which screens the game's texts use `$y$` (the
  game's text databases), and whether a menu-map action fits them better.
- `ok`: no stock movie asks for it (2.7.1). What to check: that
  `get_button_from_action_mapping("ok")` returns the A input id; if it
  returns nothing, use the player-map `use`, which the default bindings
  also put on A.

### 3.5 `CraftingScreen.init_image_substitution` (VR set)

Hook. Call the alias with the arguments unchanged, then show the pad form
of the craft button (`AI_UI_CRAFTING_CRAFT_GAMEPAD`, selected) whatever
`keyboard` was. Why: VR controllers arrive as the pad, but the keyboard
flag can be set from earlier keyboard or mouse use; the mouse form then
names a mouse action the player in the headset cannot see.

### 3.6 Layout (VR set)

#### 3.6.1 Why and how much

The 1280 x 720 UI is shown smaller in the headset than on a monitor:

| Surface | Angle across 1280 units | Degrees per unit |
|---|---|---|
| Monitor reference: 27-inch 16:9 (0.598 m wide) at 0.6 m | 53.0° | 0.0414 |
| Virtual screen (menus, loading, 05 §4.2): 1.0 m wide at 1.5 m | 36.9° | 0.0288 |
| HUD panel (05 §4.3): 0.55 m wide at 1.0 m | 30.8° | 0.0240 |

Headsets also resolve fewer pixels per degree than a monitor. Required: the
elements selected in 3.6.2 are enlarged so that they subtend at least 80%
of their monitor-reference angle:

- on the virtual screen: scale factor `s_menu` from 1.15 to 1.44
  (0.8 x 0.0414 / 0.0288 = 1.15; 1.44 gives the monitor size);
- on the HUD panel: scale factor `s_hud` from 1.38 to 1.72 (0.8 x 0.0414 /
  0.0240 = 1.38; 1.72 gives the monitor size).

The values v1 uses:

- `s_menu` = **1.22**. Players of the earlier mod saw the menu lists, item
  descriptions, button legends, the loading and death-screen tips and the
  credits 1.22 times their stock size; that factor lies in the range (85%
  of the monitor-reference angle), so v1 keeps the size players knew.
- `s_hud` = **1.38** (project decision). Players of the earlier mod saw
  the health bar, ammo counter and HUD overlays about 1.33 times their
  stock size and the in-game subtitles 1.25 times, which on the HUD panel
  is 77% and 73% of the monitor-reference angle, just under the floor
  above; 1.38 is the smallest factor that meets the floor, so it stays
  closest to the size players knew.

Either value is changed, within its range, only if the headset check
(acceptance 13) fails at it; the values used are those of the layout file
(4.2). Bounds, after scaling:

- every selected element stays fully inside its movie's stage;
- the selected elements of one screen do not overlap each other or the
  screen's other visible elements: the menu list on the deepest stock page
  with the most items (Video, advanced options visible), the description
  and extras panels, the front-end title graphics;
- an element is moved only as far as needed for that, keeping its stock
  anchor (left column, bottom-right panel, centred prompt);
- on the HUD panel, elements end at least 5% of the stage width (64 px)
  from the left and right edges and 5% of the stage height (36 px) from the
  bottom edge, because the HUD panel's edges lie where lenses blur;
- a word-wrapped text field whose scaled width would break these bounds is
  narrowed (its `DefineEditText` bounds, 2.5) so that its scaled width
  fits, and keeps its stock horizontal centre; its text wraps into more
  lines, which the stock fields already allow (multiline, word wrap, and
  `scrollV` scrolling in `SUBTITLES`) [sure for the flags].

Where a selected element cannot meet the bounds at the chosen factor, it
gets the largest factor at or below it that meets them.

#### 3.6.2 Selection rule and the survey of the stock screens

**Rule.** A placement is edited when all hold:

1. Its movie is shown on the virtual screen or the HUD panel, so its
   stage is the 1280 x 720 UI (movies with another stage size are drawn
   on in-world device and terminal screens and are left alone [likely]).
2. It carries text the player reads to play or to navigate (menu items,
   item descriptions, button legends, prompts, tips, subtitles, credits),
   either in the movie or added at run time by the movie's own script.
3. No stock resize method sets its `width` or `height`: such a method
   overwrites the placement's scale on every resize, so those elements are
   scaled through 3.6.3 instead. (An element the method only positions
   keeps its placement scale, so it is edited here.)
4. It is the outermost placement that holds that text, has its
   registration point at the element's anchor (so scaling keeps the
   anchor), and holds no full-stage background. Where the outermost one
   fails this, the edit goes one level in.
5. Its first placement carries its matrix and later frames move it without
   one (2.5), so one edit holds in every frame.

**Survey.** Every root movie of the stock archive under `DATA/UI/` was
read (stage size, named placements on the root timeline and their nested
sprites, bounds, text, later-frame matrices) [sure for the values below].
Selected placements (stock values; the edit is rejected if the stock record
differs, 5.5):

| Movie | Container | Depth | Character or class | Instance | Stock scale | Stock translate (px) | Factor | Why this level |
|---|---|---|---|---|---|---|---|---|
| `NEWFRONTENDMENU` | root | 20 | character 82 | `menuParent` | none | 110, 490 | `s_menu` | menu list (filled by script) |
| `NEWFRONTENDMENU` | root | 8 | `MenuAdditionalDescription` | `description` | none | 114.8, 624.25 | `s_menu` | item description |
| `NEWFRONTENDMENU` | root | 9 | `MenuExtrasPanel` | `extras` | none | 1060, 625 | `s_menu` | button legend |
| `NEWFRONTENDMENU` | sprite 84 (root depth 5, `splashText`) | 1 | character 83 | `textField` | none | 112.9, 548.05 | `s_menu` | title-screen prompt; `splashText` is placed at the stage origin, so scaling it would move the prompt (rule 4) |
| `PAUSEMENU` | root | 6 | character 1 | `menuParent` | none | 110, 490 | `s_menu` | menu list |
| `PAUSEMENU` | root | 4 | `MenuAdditionalDescription` | `description` | none | 115, 640 | `s_menu` | item description |
| `PAUSEMENU` | root | 5 | `MenuExtrasPanel` | `extras` | none | 1060, 640 | `s_menu` | button legend |
| `GAMEOVERMENU` | root | 8 | character 1 | `menuParent` | none | 104, 550 | `s_menu` | menu list |
| `GAMEOVERMENU` | root | 6 | `MenuAdditionalDescription` | `description` | none | 106, 655 | `s_menu` | item description |
| `GAMEOVERMENU` | root | 7 | `MenuExtrasPanel` | `extras` | none | 1100, 650 | `s_menu` | button legend |
| `GAMEOVERMENU` | root | 2 | character 4 | `tipText` | none | 587.05, 505.2 | `s_menu` | death-screen tip; later frames move it without a matrix |
| `LOADINGSCREEN` | sprite 18 (`gameplaytip_mc` in `loading_mc`) | 2 | character 16 | `transanim` | none | 0, 0 | `s_menu` | loading tip; `gameplaytip_mc` also holds a full-width band (characters 14 and 17, 1280 wide), so the edit goes to the text (rule 4) |
| `LOADINGSCREEN` | root | 22 | character 44 | `subtitlebox_mc` | none | 646.45, 640.95 | `s_menu` | loading-screen subtitles |
| `CREDITSSCREEN` | root | 3 | character 5 | `textFieldParent` | none | 265, 0 | `s_menu` | credits (the script creates the lines in it, each as wide as `textFieldExample`, character 7) |
| `MOVIEPLAYER` | root | 5 | character 5 | `subtitlebox_mc` | none | 640.45, 667.4 | `s_menu` | cutscene-video subtitles |
| `SUBTITLES` | root | 1 | character 1 (`DefineEditText`) | `txt_subtitlebox` | none | 144.95, 529.95 | `s_hud` | in-game subtitles on the HUD panel |
| `WEAPONSTUFF` | root | 13 | character 87 | `grapplePrompt_mc` | none | 640, 507 | `s_hud` | grapple and struggle prompt (positioned only by the resize method) |
| `WEAPONSTUFF` | root | 20 | character 87 | `grapplePrompt_scripted_mc` | 1.48621 | 640, 506.8 | `s_hud` times the stock scale | as above |
| `WEAPONSTUFF` | root | 27 | character 90 | `BreathingGame_mc` | none | 616.7, 429.45 | `s_hud` | hold-breath prompt (positioned only) |

Stock scale values are the 16.16 fixed values 0x00017C78 (1.48621) for
`grapplePrompt_scripted_mc` and none (no scale field) elsewhere [sure]; the
check compares the fixed-point bits, not rounded decimals.

Text fields that the bounds of 3.6.1 force to be narrowed (stock widths
from their `DefineEditText` bounds) [sure for the widths]:

| Field | Stock width (units) | Fits at `s` up to | Narrowed when |
|---|---|---|---|
| `SUBTITLES` character 1 (`txt_subtitlebox`) | 1008 | 1152 / 1008 = 1.14 (HUD 5% margins) | always, since `s_hud` is at least 1.38 |
| `MOVIEPLAYER` character 4 (in `subtitlebox_mc`) and `LOADINGSCREEN` character 43 (in `subtitlebox_mc`) | 1008 | 1280 / 1008 = 1.27 (stage) | `s_menu` above 1.27 |
| `CREDITSSCREEN` character 7 (`textFieldExample`; the script copies its width to every credits line and to the scroll rectangle) | 1009 | about 1.01 at its stock left edge of 265 px; 1.27 if the column moves left | `s_menu` above 1.27, and the column moves left as far as needed |

**Screens left unchanged**, with the reason:

| Movie(s) | Reason |
|---|---|
| `HUB`, `HUB_INVENTORY`, `HUB_JOURNAL`, `HUB_LOGS`, `HUB_MAP`, `HUB_MISSING`, `CRAFTINGSCREEN` (and its library `CRAFTINGCOMPONENTS`) | the in-game hub screens fill the stage with text across most of its width and height (for example the journal's `glyphs` row spans 35 to 1244 px, the map legend 64 to 1217 px, the controller prompts 82 to 1228 px); no factor of at least 1.15 keeps them inside the stage, so enlarging them needs a re-layout of each screen, not a factor (project decision: unchanged in v1, open in 7) |
| `CHALLENGEMAPOVERLAY`, `CHALLENGELEADERBOARDCOMPONENT`; `characterSelectionParent` and `mapSelectionParent` in `NEWFRONTENDMENU` | Survivor mode: the selection screens and result panels already span most of the stage (`mapSelectionParent` reaches 729 px, past the stage bottom, in stock), the overlay passes `k` to the leaderboard component of another movie, and several elements leave the stage at 1.38 (3.6.3); project decision: unchanged in v1, open in 7 |
| `network_notification_mc` in `NEWFRONTENDMENU` | an online-service notice shown with the Survivor screens [guess: check when it appears]; left with them |
| `userProfilePanel` in `NEWFRONTENDMENU` and `PAUSEMENU` | shows the platform profile name, which the player does not need to navigate; in the front end a later frame re-places it with its own matrix, so one edit would not hold (rule 5) |
| `logoIsolation`, `logoAlien` in `NEWFRONTENDMENU` | not text; moved only if the overlap check of 3.6.1 shows the enlarged menu list or prompt touching them (the stock logos end at 514 px and the title-screen prompt starts at 546 px) |
| `legal_guff_mc` in `LOADINGSCREEN` | the legal screen of the start-up sequence, which the player does not need to read to play; at 1.15 it would reach past the stage bottom |
| `GAMMASCREEN` | the one-time brightness calibration; its labels are aligned to the reference images (one label already sits outside the stage in stock); project decision: unchanged |
| `TERMINALINTERACTION`, `TERMINALINTERACTIONATTRACT` (960 x 720), `HACKINGV4`, `HACKINGUPGRADE` (840 x 720), `REACTION_GAME` (820 x 720), `REWIREV2` (1920 x 720), `MINIGAMES` (960 x 720), `KEYPAD_*` (171 x 128), `MOTIONTRACKER`, `CCTVLASER`, `SCREEN*` (128 to 256 units) | stage sizes other than 1280 x 720: terminal, device and in-world screens (rule 1) |
| `RELOADING` | reload-progress overlay for two weapons [guess: what it shows and on which surface]; at 1.38 it would leave the stage on the right; open in 7 |
| `ANNOUNCEEND`, `ATTRACTSCREEN`, `SPLASHSCREEN`, `INSTALLATIONSCREEN`, `MAINMENU`, `DEBUGTEXT` | not shown in the PC game's normal flow [guess: check `ANNOUNCEEND` and `SPLASHSCREEN`]; `DEBUGTEXT` is developer output |
| `RUNTIMESHAREDCOMPONENTS` | the library of shared menu symbols; the menus' own placements of them are edited instead, so the library's symbols stay as every movie expects |
| `PICKUPOVERLAY`, `TRCSYSTEMOVERLAY`, `POPUPOVERLAY`, `RADIALMENU`; `mc_healthbarroot` and `ammo_mc` in `WEAPONSTUFF` | sized by a stock resize method (rule 3): see 3.6.3 |
| `AGGREGATE`, `CONTROLLERDIAGRAMS`, `FONTS_*`, `GFXFONTLIB`, `HUB_CONSTRUCT`, `HUB_INFO`, `KEYPAD_DISPLAY`, `LOADINGSCREENSTILL`, `MOUSECURSOROVERLAY`, `MOVIEPLAYER_RTT`, `SCREENREWIREMAP`, `VHS_EFFECTS` | no text-bearing placement on the 1280 x 720 stage |

#### 3.6.3 HUD resize methods

**Technique.** In each included method, the two reference constants are
edited: `Wref` to `round(Wref / s_hud)` and `Href` to `round(Href /
s_hud)` (the first two `pushshort` operands of the method, 2.7.6). This
multiplies `k` by `s_hud` for every element the method sizes, and each
element keeps its anchor because its position is computed from `k` and the
stage size. Why this lever rather than another:

- `k` is the single factor every element of the method derives its size
  and edge distance from, so one checked two-operand edit per method
  scales all of them about their own anchors;
- a scale on the movie's root would multiply positions too, about the
  stage origin, pushing right- and bottom-anchored elements off the stage;
- a larger HUD panel (05 §4.3) would enlarge every HUD movie, including the
  crosshair and full-stage effects, and cover more of the view, which 05
  chose against;
- editing every element's own constants would take many more edits for the
  same result. Element constants are still edited where an element would
  break the bounds (below).

**Survey of the seven methods** (2.7.6):

| Method | Included | Why |
|---|---|---|
| `WeaponStuff` | yes | HUD panel: health bar and ammo counter carry text |
| `PickupOverlay` (`PICKUPOVERLAY`) | yes | HUD panel: pickup, tutorial, hold-to-throw, audio-log and upgrade prompts |
| `TRCSystemOverlay` | yes | movie-skip prompt and saving / loading indicators, over gameplay and videos |
| `PopupOverlay` | yes | dialog popups over gameplay and menus; with `s_hud` its dialog stays well inside the stage on the virtual screen too (content 201 to 1077 px at 1.72) |
| `ChallengeMapOverlay` | no | Survivor mode (3.6.2) |
| `RadialMenuScreen` | no | its radial menu is 1280 design units wide at the stage centre, so any factor above 1 takes it past the stage edges [sure: from its bounds]; the corner prompts would need their own constant edits; open in 7 |
| `PickupOverlay` in `REACTION_GAME` | no | never runs: not the movie's root class (2.7.6) |

The second `1280` in `PopupOverlay` is the popup symbol's design width,
not a reference size, and is not changed.

**Results at the ends of the `s_hud` range**, computed from each element's
stock bounds and the method's formulas (stage 1280 x 720; L, R, B: breaks
the left, right or bottom 5% margin; S: leaves the stage) [sure for the
arithmetic; the bounds are those of the elements' first frames]:

| Element (method) | Design box, position | At 1.38 | At 1.72 |
|---|---|---|---|
| `mc_healthbarroot` (`WeaponStuff`) | 280 x 28, `(96k, H - 74k)`, top-left anchor | 132 to 519 px, bottom 656 | 165 to 647 px, bottom 641 |
| `ammo_mc` (`WeaponStuff`) | 280 x 28, `(96k, H - 78k)`, bottom-left anchor | 132 to 519 px, bottom 612 | 165 to 647 px, bottom 586 |
| `holdThrowPrompt_mc` | 385 x 64, `(96k, H - 132k)` | inside | inside |
| `audiolog_popup_mc` | 356 x 90, right-anchored at `W - 60k` | inside | inside |
| `upgradeNotifier_mc` | 411 x 93, centred, `y = 110k` | inside | inside |
| `container_mc` | 690 x 100, stage centre | inside | L, R (46 to 1233 px) |
| `staticPrompt_mc` | 750 x 80, centred, `y = H/2 + 85k` | inside | L, R, S |
| `mc_tutorial_message_root` | 479 x 75, centred, `y = H/2 + 160k` | inside | B (bottom 700) |
| `rewirePopUp_mc` | 725 x 370, stage centre | inside | L, R, B |
| `scriptedMessage_mc` | 853 x 51, centred, `y = 160k` | L, R (51 to 1229 px) | L, R, S |
| `preNames_mc` | 540 x 80, centred, `y = 590k` | S (centre at 814) | S |
| `mc_tutorial_overlay`, `mc_tutorial_overlay_kb` | 1175 x 30, centred, `y = H - 65k` | L, R, S | L, R, S (already L, R at the stock size) |
| movie-skip prompt, loading and saving icons (`TRCSystemOverlay`) | right- and bottom-anchored by multiples of `k` | inside | inside |
| dialog content (`PopupOverlay`) | centred | inside | inside |

So the health bar and ammo counter need no edge-offset edit: an element
placed at a multiple of `k` from an edge keeps a gap that grows with `k`.
Elements that break the bounds at the chosen `s_hud` are corrected with
constant edits of their own, under the rule of 3.6.1:

- a centred element too wide or too tall: its width and height constants
  are edited in the same proportion, so it gets the largest uniform factor
  that fits;
- an element positioned at a multiple of `k` from the top that would move
  off the stage (`preNames_mc`'s `590`): its position constant becomes
  `round(c / s_hud)`, so it keeps its stock position and grows about it;
- the tutorial overlays: their background is resized to the text width at
  run time (`display_alt_tutorial_overlay`), so their first-frame width
  overstates them [likely]; their width and height constants are reduced
  until the widest stock tutorial line fits within the 5% margins, checked
  in the headset (acceptance 13).

New operand values must keep the instruction's encoded length (`pushshort`
operands from 128 to 16383 take two bytes; `pushbyte` operands are -128 to
127), so no branch moves.

#### 3.6.4 Crosshair

Required: the crosshair appears `s_hud` times its stock size, centred
where it is in stock. Why: it is a symbol the player must find and aim
with, and the reason of 3.6.1 (the HUD panel shows it at 0.0240° per unit
against 0.0414° on the monitor reference, in a display with fewer pixels
per degree) applies to it as it does to text; at the stock size it
subtends about 0.58° in the headset against 0.99° on the monitor
reference.

Which placement carries the scale follows rules 3 to 5 of 3.6.2 (rule 2,
text, does not apply: the crosshair is a symbol, and the reason above takes
its place). Survey of `WEAPONSTUFF` [sure]:

- `crosshairs_mc` is placed once on the root timeline, at depth 1 in the
  first frame (character 62, `WeaponStuff_fla.mcCrosshairAnims_1`), by a
  `PlaceObject2` with no scale and translate (640, 360), the stage centre;
  none of the root timeline's later frames (103 in all) places depth 1
  again (rule 5).
- The stock script sets only its `x` and `y` (in the resize method, from
  the stage size) and otherwise only plays and stops its frames and those
  of its child `mc` (showing, hiding and accuracy states); nothing sets its
  `width`, `height` or scale (rule 3).
- Inside it, character 61 (`WeaponStuff_fla.crosshairs_mc_2`, instance
  `mc`) is placed at translate (0, 0) with no scale, and later frames of
  character 62 change only its colour transform. So the registration point
  of the root placement is the crosshair's own origin, and scaling it keeps
  the crosshair centred (rule 4). The crosshair is about 23 x 24 units.

So the edit is a scale of `s_hud` in the matrix of the root placement of
`crosshairs_mc`, translate unchanged; the stock record must match the
values above (5.5). It is the outermost placement and passes rules 3 to 5,
so no inner placement is edited.

Players of the earlier mod saw the crosshair about 3.16 times its stock
size (about 1.8° across in the headset) and the grapple and struggle
prompt about 4.46 times. v1 gives both `s_hud` (project decision, pending
user confirmation: at 1.38 the crosshair subtends about 0.8°, 80% of its
monitor-reference angle like the HUD text, which is enough to find and aim
with, while a crosshair more than twice that covers more of a distant
target; the prompt is
text and is sized like the other HUD text, 3.6.1).

#### 3.6.5 Not changed

The background video placement (`moviePlayer`) and colour transforms are
not changed. `CREDITSSCREEN`, `MOVIEPLAYER` and `SUBTITLES` get layout
edits only.

#### 3.6.6 Compared with the layout players knew

Players of the earlier mod saw these layout changes besides the factors of
3.6.1 and 3.6.4. v1 does not copy them as offsets; it reaches the same aim
(text inside the clear part of the view, nothing overlapping) through the
bounds of 3.6.1, which move an element only as far as needed:

| Screen | Seen by players of the earlier mod | v1 |
|---|---|---|
| Front-end and pause menus | menu list 140 px (of 720) lower | stock anchor, moved only as the bounds require [guess: check in the headset whether the list at 1.22 meets the logos or the description; open in 7] |
| Front end | the two title logos 1.22 times larger, about 31 px to the right | unchanged unless the overlap check needs them moved (3.6.2) |
| Game-over menu | menu list 25 px higher, tip text 108 px to the right | stock anchors, moved only as the bounds require |
| Loading screen | tip text 21.5 px lower; subtitles at the stock size | tip and subtitles at `s_menu` |
| Cutscene videos | subtitles 1.25 times larger | `s_menu` (1.22) |
| In-game subtitles | 1.25 times larger, moved left (x 145 to 23) and up (y 530 to 500) | `s_hud`, text field narrowed to the margins (3.6.2) |
| HUD health bar, ammo counter, pickup hold-to-throw prompt | moved further in from the left edge (96 to 144 units) | further in by the factor of `k` (3.6.3: 96 x 1.38, about 132 px) |

## 4. Developer build

### 4.1 Overview

A resource build, run by developers and by CI, turns the project's UI
sources into one binary **bundle**. The DLL build embeds the bundle as a
Win32 resource (type `RCDATA`, a fixed id) and never runs the resource
tools. The generated bundle is checked in; the DLL build uses the checked-in
file, and a CI job regenerates it and fails if the bytes differ. So a DLL
build needs only the C++ toolchain, and the bundle is reproducible.

Developer tools (any equivalent is acceptable if it produces the same
bundle): Python 3.12 or later with Pillow, Java 21, and FFDec 26.x used as
an ActionScript 3 compiler from the command line, or another compiler that
emits ABC 46.16. Tool versions are pinned in the build and recorded in the
bundle's info section.

### 4.2 Inputs

All authored in this project:

| Input | Content |
|---|---|
| Declaration files | ActionScript source declaring each stock class of 2.7 with the members the project's code uses (name, namespace kind, signature or type, superclass chain up to a Flash class), with empty or throwing bodies. Written from this document, never extracted from a movie |
| Project sources | for each target class, the project's new methods and fields, written in a source file that also contains that class's declarations, so they compile as members of the class and may use its protected and private members |
| Binding file | text (for example JSON): for each change of section 3, the target movies, class, kind (hook, add method, add field), stock member, project member, alias name, change set (menu or VR), and the expected overriders of a hooked method (2.7.2) |
| Layout file | text: each edit of 3.6 with its expected stock record values and its new values (`s_menu`, `s_hud` and derived numbers) |
| Glyph sources | one 32 x 32 RGBA PNG per glyph of 2.9, drawn for this project to the designs of 04 §4.1 |

### 4.3 Process

1. Compile each project source with its declarations into an ABC file.
2. From each ABC take only the members the binding file names as project
   members (their `method_info`, `method_body_info`, slot traits and
   everything they reference); stub bodies are discarded.
3. Check every extracted body (rules of 5.4.4: opcodes, branches,
   exceptions, no debug opcodes, stack and local limits). The build fails
   on any violation, naming the member.
4. Record, for every stock member the extracted code references (by
   multiname), a **requirement**: class, name, namespace kind, trait kind
   and type or signature, taken from the declarations. The linker checks
   them against the player's movies (5.4.2).
5. Encode the glyph PNGs as DDS in the format of 2.9 (BC3, one level). The
   encoder is the project's own BC3 encoder or a pinned library version;
   it must be deterministic (same PNG, same bytes).
6. Collect the layout edits, the accepted source identities (2.2, 2.3) and
   the expected per-entry identities of every touched entry.
7. Write the bundle (4.4).

### 4.4 Bundle format

Little-endian. All offsets from the start of the bundle. All sections
4-byte aligned with zero padding.

Header (64 bytes):

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | magic `OMVRUIB` followed by one NUL byte |
| 8 | 2 | format major (1); a linker rejects another major |
| 10 | 2 | format minor (0); higher minors only add sections a linker may ignore |
| 12 | 4 | header size (64) |
| 16 | 4 | section count |
| 20 | 4 | total bundle size |
| 24 | 8 | reserved, zero |
| 32 | 32 | SHA-256 of bytes 64 to the end |

Section table, right after the header, one 16-byte record per section,
sorted by type: u32 type, u32 offset, u32 size, u32 CRC-32 of the section.

| Type | Section | Content |
|---|---|---|
| 1 | Strings | u32 count, then per string u32 byte length and UTF-8 bytes. Every name in later sections is an index into this table |
| 2 | Sources | accepted whole-archive identities: u32 count, each u64 size, 32-byte SHA-256, string index of a label (`Steam 2015`); then expected entries: u32 count, each string index of the entry name, u32 size, u32 CRC-32, 32-byte SHA-256 |
| 3 | Units | u32 count; each unit: string index of the target class (qualified), u32 ABC size, ABC bytes (a minimal ABC 46.16 file holding only the extracted members, their pools and their requirements' multinames) |
| 4 | Bindings | u32 count; each record: u8 kind (1 hook, 2 add method, 3 add field), u8 set mask (bit 0 menu set, bit 1 VR set), u16 zero, u32 unit index, u32 movie list (string indices, count first), stock member (u8 namespace kind, string index of name), project member (same), alias name (string index, hooks only), expected overriders (string indices, count first) |
| 5 | Requirements | u32 count; each: unit index, class, member name, namespace kind, trait kind, type or signature (param types, optional count, rest flag, return type, all as qualified names) |
| 6 | Layout | u32 count; each: u8 kind (1 placement matrix, 2 constant operand, 3 text field bounds), set mask, movie (string index); kind 1: container sprite id (0 root), depth, character id or class name, instance name, expected matrix bytes, new matrix fields; kind 2: class, method (namespace kind and name), opcode, occurrence (1-based), expected operand, new operand; kind 3: character id, expected `RECT` bytes, new bounds |
| 7 | Files | u32 count; each: entry name (string index), set mask, u32 size, u32 CRC-32, bytes |
| 8 | Info | strings: project version, source commit, tool names and versions. Logged, never used for decisions |

The bundle contains compiled project methods, binding instructions, layout
values and project glyphs, and expected sizes and hashes of game entries.
It contains no game movie, no part of one, and no game image.

### 4.5 Determinism

The resource build is a pure function of its inputs and pinned tools: no
time stamps, no absolute paths, no host names, sorted maps, stable
compiler settings (debug information off). The CI job of 4.1 enforces it.
The linker is deterministic too (5.6), so one `UI.PAK`, one bundle and one
change set always give the same `MOTHER.PAK` bytes.

### 4.6 Developer-only checks

A command-line host of the DLL's linker (same code, not shipped) links the
bundle against a developer's own `UI.PAK` into a scratch folder for
inspection with any SWF tool. It never runs in CI (CI has no game files)
and its output is never committed.

## 5. Install-time linking

### 5.1 When, where, and the staleness key

- Runs at `DLL_PROCESS_ATTACH`, on the loader thread, after the config file
  and command line are read (the change set depends on the runtime) and
  before attach returns (01 §2.2, 12 §2.3). There is no later hook early
  enough: the game opens its UI archive during start-up.
- Runs only in the game's process, on a recognised build whose checks
  passed (01 §2.4, §3.1). In a host that is not the game, on an unknown
  build or after a failed preflight, nothing of this section runs: no
  file is read or written and `UiPakPathPtr` is not touched, because the
  redirect could not be applied and the work would only cost start-up
  time.
- Game folder = the folder of the proxy DLL. Input `DATA\UI.PAK`, output
  `DATA\MOTHER.PAK`, and a **key file** in `DATA` next to the output, under
  a name the implementation chooses that no game file uses. The redirect
  string is a relative path to the output in the same form as the stock
  `data\ui.pak` (folder `data`, a backslash, the file `MOTHER.PAK`; case
  does not matter, 2.1), NUL-terminated, in storage that lives for the
  whole process (01 §2.2).
- **Key**: (`UI.PAK` SHA-256, bundle SHA-256, linker version, change set).
  The output is a pure function of the key, so an existing output with a
  matching key is reused.
- **Key file**, 152 bytes; a key file of any other size or magic is no
  match:

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | magic `OMVRKEY1` |
| 8 | 4 | key file version (1) |
| 12 | 4 | change set (1 menu, 2 VR) or 0xFFFFFFFF for "rejected" (5.9) |
| 16 | 8 | `UI.PAK` size |
| 24 | 8 | `UI.PAK` last-write time (FILETIME) |
| 32 | 32 | `UI.PAK` SHA-256 |
| 64 | 32 | bundle SHA-256 |
| 96 | 4 | linker version |
| 100 | 8 | `MOTHER.PAK` size |
| 108 | 8 | `MOTHER.PAK` last-write time (FILETIME) |
| 116 | 32 | `MOTHER.PAK` SHA-256 |
| 148 | 4 | CRC-32 of bytes 0 to 147 |

- Output file times are whatever the file system sets when the file is
  written; the mod does not stamp them. They are recorded in the key file
  only to detect a later change.

### 5.2 Fast path (reuse)

Reuse `MOTHER.PAK` without reading `UI.PAK` when all hold:

1. The key file is 152 bytes, its magic, version and CRC-32 are right, and
   its change set is the one for this run.
2. Its bundle SHA-256 equals the SHA-256 in the embedded bundle's header
   and its linker version equals the DLL's.
3. `UI.PAK`'s current size and last-write time equal the recorded ones.
4. `MOTHER.PAK`'s current size and last-write time equal the recorded
   ones.
5. `MOTHER.PAK` opens, its header is `PAK2` with alignment 4, its entry
   count is the stock count plus the bundle's new entries for this set,
   its names are sorted and unique, its `end` offsets increase and the
   last equals the file size (about 18 KB read).

Then redirect `UiPakPathPtr` and log the reuse (11 §2.1). Any failure means
rebuild (5.3). If the process's current directory is not the game folder,
log it once as a warning and still redirect (the stock `data\ui.pak` is
relative in the same way).

Why metadata and not a hash on the fast path: hashing both archives
(about 35 MB) at every launch would add tens of milliseconds under the
loader lock at typical SHA-256 speeds (an estimate, not a measurement),
against the 10 ms limit of 5.10. The full hashes are taken whenever
anything changes, and a file whose size or last-write time changed is
caught.

### 5.3 Rebuild: validation

1. `UI.PAK` missing: fail with reason `missing` (5.9).
2. Read it whole into memory (one read). Compute its SHA-256 with the
   DLL's own SHA-256 code (no system crypto library is loaded under the
   loader lock).
3. Parse it as 2.2. Any structural error (magic, sizes beyond the file,
   non-increasing offsets, names not NUL-terminated): fail with reason
   `format`. Non-zero padding bytes are tolerated; the output pads with
   zeros.
4. If the SHA-256 and size match an accepted identity of the bundle: the
   archive is known stock.
5. Otherwise check, for every entry the selected change set touches, that
   it exists and that its size, CRC-32 and SHA-256 equal the bundle's
   expected values, and that no entry the set adds already exists. If all
   hold, continue and log the source as `modified` (another tool changed
   entries the mod does not touch; those are carried over unchanged). If
   any fails: fail with reason `unknown_version` and name the first entry
   that differs.
6. In both cases, each touched entry's size, CRC-32 and SHA-256 are
   compared before it is changed (step 5 does this for 4 as well, since a
   known whole file implies them).

### 5.4 Rebuild: linking

For each touched movie, for each DoABC tag holding a target class, the
linker parses the ABC file, applies the bindings for this change set, and
writes the ABC back. Any failed check below aborts the whole build (5.9,
reason `link`, naming the movie, class, member and check). No partial
output is ever written.

#### 5.4.1 Round trip

Before changing a tag, re-encode the parsed ABC without changes and compare
with the original bytes. They must be identical; if not, the linker does
not understand this ABC and stops. Edits then only append: no existing
pool entry, `method_info`, body, trait or class is moved, renumbered or
removed, so every existing index in the stock code stays valid.

#### 5.4.2 Requirements and target lookup

- The target class (qualified name) must be defined exactly once in the
  movie's DoABC tags.
- Every requirement of the unit must hold in the target ABC: the member
  exists on the named class (or the superclass chain for inherited
  members), with the stated trait kind and namespace kind, and its type or
  signature resolves to the same qualified names.
- For a hook, the overriders found in the movie (subclasses in the same
  ABC with a method trait of the same name and the override attribute)
  must be exactly the bundle's expected list, and each must call the hooked
  method on `super` (its body contains `callsuper` or `callsupervoid` with
  that name). Otherwise the hook would not run for that subclass.

#### 5.4.3 Namespace mapping

Every namespace the unit's code uses is mapped to the target ABC:

| Unit namespace | Maps to |
|---|---|
| package (public, any package name) | the target namespace of the same kind and name; appended if absent |
| protected or static protected of a target class | the target class's protected namespace (named in its `instance_info`), or the static one of the same name |
| private of a target class | the single private namespace used by that class's own private traits in the target; if the class has no private trait, or its private traits use more than one namespace, and the unit references a private member: fail |
| package internal | the target's internal namespace of that package name, which must exist |
| explicit, namespace (0x08) | same kind and name, appended if absent |

Namespace sets are mapped element by element and appended as new sets.

#### 5.4.4 Code checks

Each unit method body must, before remapping:

- decode completely with the AVM2 operand formats (2.6);
- use none of: `newfunction`, `newclass`, `newactivation`, `newcatch`,
  `getslot`, `setslot`, `getglobalslot`, `setglobalslot`,
  `getscopeobject`, `getouterscope`, `dxns`, `dxnslate`, `debug`,
  `debugline`, `debugfile`, `findpropglobal`, `findpropglobalstrict`, or
  any undocumented opcode. Why: these depend on activation objects, outer
  scopes, slot layouts or debugging state the linker cannot rebase. (So
  project code has no closures, no `try`/`catch` and no nested
  functions; handlers are methods of the class, passed as method
  closures.)
- have every branch target (`jump`, the `if...` family: offset from the end
  of the instruction) and every `lookupswitch` target (offset from the
  start of the instruction) on an instruction boundary inside the body,
  and end with `returnvoid`, `returnvalue`, `jump` or `throw`;
- have an empty exception table;
- have `max_stack` and `local_count` at most 64 (the build has already
  checked them against a stack-height pass, 4.3).

#### 5.4.5 Pool merging and operand remapping

Append to the target's pools every pool entry the unit references, reusing
an existing target entry when it is identical (int, uint and double by
value, string by bytes, namespace by the mapping of 5.4.3, multiname by
kind and mapped fields). Then rewrite every pool operand of every unit
instruction to the target index: string (`pushstring`), int (`pushint`),
uint (`pushuint`), double (`pushdouble`), namespace (`pushnamespace`) and
multiname (`getlex`, `findproperty`, `findpropstrict`, `getproperty`,
`setproperty`, `initproperty`, `deleteproperty`, `getsuper`, `setsuper`,
`getdescendants`, `callproperty`, `callpropvoid`, `callproplex`,
`callsuper`, `callsupervoid`, `constructprop`, `coerce`, `astype`,
`istype`). An operand's u30 length can change when its index grows, so
the code is re-encoded and every branch and `lookupswitch` offset is
recomputed from the instruction map; the result is checked again as in
5.4.4. Also remap method signatures (param, return and optional value
indices) and trait names and types.

#### 5.4.6 Applying the bindings

- **Hook**: the hook's `method_info` and body are appended. Its signature
  must equal the stock method's: same param count, param types, optional
  count and default values, rest flag (`NEED_REST` 0x04 /
  `NEED_ARGUMENTS` 0x01 identical) and return type, all compared as
  qualified names. Then the stock trait's method index is set to the
  hook's, and an alias method trait is appended to the class: same trait
  kind and namespace as the stock trait, the alias name, pointing at the
  stock `method_info`. The alias name must not exist on the class, its
  superclasses or its subclasses in that ABC. Hooks on `iinit`, `cinit`,
  script init, getters or setters are not allowed.
- **Add method**: append the `method_info`, body and a method trait; the
  name must not exist on the class, its superclasses or subclasses.
- **Add field**: append a slot trait (`slot_id` 0) with a mapped type and a
  constant default only (none, null, undefined, Boolean, int, uint,
  double or string).
- **Scope**: every appended body gets the class's instance-method depths
  of 2.6 (`init_scope_depth` = the class's value; `max_scope_depth` =
  that plus the unit body's own `max_scope_depth - init_scope_depth`,
  at least 1). The checks of 5.4.4 make this rebasing safe.
- Bodies are appended at the end of `method_body_info[]`; `method_info.name`
  is 0 as in the stock code.

### 5.5 Rebuild: layout edits

- **Placement**: find the container (root or the `DefineSprite` with the
  given id), the placement tag at the depth with the character id or class
  name and the instance name; its current `MATRIX` bytes must equal the
  expected ones. Re-encode the record with the new matrix (smallest bit
  widths), keep every other field, rewrite the tag header (long form if the
  stock tag was long or the length is 63 or more), and fix the enclosing
  `DefineSprite` length.
- **Text field bounds**: find the `DefineEditText` with the given
  character id; its current `RECT` bytes must equal the expected ones.
  Re-encode the `RECT` with the new bounds, keep every other byte of the
  record, and rewrite the tag header and any enclosing `DefineSprite`
  length as for a placement.
- **Constant operand**: find the method by namespace kind and name; decode
  it; the given occurrence of the opcode must exist and have the expected
  operand; write the new operand. Its encoded length must equal the old
  one, so no other byte moves.

A missing target or a different expected value aborts the build (reason
`layout`).

### 5.6 Rebuild: writing the movies

Every tag that no binding or edit touches is copied byte for byte. A
changed DoABC2 tag keeps its flags and name and gets the new ABC; its tag
header is rewritten (long form). Then the movie's file length field (offset
4) is set to the new size. The linker parses the written movie again
(header, tag stream to `End`, each changed ABC) as a final check.

### 5.7 Rebuild: assembling MOTHER.PAK

- Every stock entry is copied, in stock order, byte for byte unless
  changed; changed entries keep their names.
- The change set's new entries (the `G` glyphs of 2.9, VR set only) are
  inserted at their sorted positions. Names stay sorted and unique.
- Alignment 4, zero padding, `namesSize` and every `end` recomputed per
  2.2. The output for the 2015 archive has 505 entries (menu set) or
  505 + `G` (VR set).
- Every copied entry's size and CRC-32 in the output are checked against
  the input (a copy error is caught before the file is used).

### 5.8 Rebuild: writing atomically

Required results:

- The game never opens a partly written `MOTHER.PAK`: the archive is
  written in full under a temporary name in `DATA`, flushed
  (`FlushFileBuffers`), and then moved over `MOTHER.PAK` with
  `MoveFileExW(..., MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`;
  the key file is replaced the same way, through its own temporary name
  (the temporary names are the implementation's choice). Either
  file may land first: the fast path compares the archive's size and
  last-write time with the key (5.2), so a key and an archive from
  different builds never match.
- The output SHA-256 recorded in the key file is that of the bytes written.
- `UiPakPathPtr` is redirected only after both files are in place, and the
  build is logged with the entry count and source (`stock` or `modified`,
  11 §2.1).

A process killed at any point leaves either the old pair (rebuilt next
time, since the key no longer matches) or a new archive with an old key
(also rebuilt). A leftover temporary file is deleted before writing. A
stale or corrupt `MOTHER.PAK` is never patched in place; it is replaced.

Concurrency: a second game process (for example one that Steam starts
while the first is still starting or exiting) can reach this work while
another process is still in it, and the two must not write the same files
at once. The whole of 5.2 to 5.8 runs under a named mutex
(`Local\` namespace, a project-specific name). Wait at most 5 s (project
decision: longer than a rebuild's 500 ms limit with margin, short enough
that a stuck process does not hold up the game's start noticeably); on
timeout, use the archive only if the fast path (5.2) succeeds, otherwise
leave the pointer alone and log it.

### 5.9 Failure behaviour

In every failure: `UiPakPathPtr` is left alone (the game opens `UI.PAK`),
the skip-intro patch is not applied (06 §6), the game uses its stock
prompts (04 §4.2 stays in stock-glyph mode), VR, patches and the config
file are unaffected, settings are changed in `mothervr.json` only. An
existing `MOTHER.PAK` is not used and not deleted.

| Cause | Reason | Logged (11 §2.1) | Player (13 §5.3) |
|---|---|---|---|
| `UI.PAK` missing | `missing` | error, every launch | the message whose first line is `Failed to find UI.PAK!` (13 §5.3) at every launch: the game cannot show its UI without it, so the player must repair the installation |
| `UI.PAK` not PAK2 or damaged | `format` | error, every launch | the UI-archive message of 13 §5.3, once per distinct `UI.PAK` (size, last-write time) |
| unknown version (5.3 step 5) | `unknown_version` (with the entry) | error, every launch | as for `format` |
| link or layout check failed (bundle and game disagree) | `link` / `layout` (with movie, class, member, check) | error, every launch | as for `format` |
| write failed (read-only folder, disk full, access denied) | `write` (with the Windows error) | error, every launch | as for `format` |
| mutex timeout without a usable archive | `busy` | warning | none |

"Once per distinct `UI.PAK`": after the message the mod writes a key file
with change set 0xFFFFFFFF and the rejected size and last-write time; the
next launch with the same file logs the failure but shows no message.
Messages decided at attach are shown as 12 §2.4 requires.

### 5.10 Cost limits

| Path | Limit |
|---|---|
| Fast path (5.2) | 10 ms at attach (two file stats, one 152-byte read, one 18 KB read) |
| Rebuild | 500 ms on an SSD for the 2015 archive (read 17.5 MB, two SHA-256 passes over about 35 MB, link 24 movies, write about 17.7 MB) |
| Memory | at most 64 MB extra during a rebuild, freed before attach returns |
| Disk | one archive of about 17.7 MB plus the key file |

The limits are project decisions: attach runs under the loader lock before
the game shows anything, so the fast path must be imperceptible, and a
rebuild, which happens only after an install, an update or a runtime
change, must stay well under a second.

No thread is created for this work, no DLL is loaded, no game function is
called (01 §2.2).

## 6. Acceptance

"Log" is `mothervr.log` (11). Unless stated, on the Steam 2015 build.

1. Delete `MOTHER.PAK` and the key file, start with runtime OpenVR:
   the log shows a build from source `stock` with 505 + `G` entries;
   the key file is 152 bytes. Start again: the log shows a reuse.
2. Delete both again and rebuild: the new `MOTHER.PAK` has the same SHA-256
   as before (determinism). Do the same on a second machine with the same
   DLL: same SHA-256.
3. Set runtime None in the menu (the game exits), start: rebuilt with 505
   entries (menu set). In the front end, the HUD and prompts look exactly
   as with the DLL removed; the Options page has the `MotherVR` entry.
4. With runtime OpenVR, the VR page exists in the front-end and pause
   menus and 06 §8's checks pass; the game-over menu shows no VR page and
   its Video items behave as in the stock game.
5. `SkipStartupIntro` on (`Skip Startup Intro` on the options page): no
   logo video and no legal screen, about half a second of black, then the
   title screen; the log shows the intro skipped (06 §6).
6. Truncate `MOTHER.PAK` by one byte: next start rebuilds. Replace it with
   a 0-byte file: rebuilds. Write garbage into its first 4 bytes and
   restore its last-write time: rebuilds (header check).
7. Touch `UI.PAK`'s last-write time: next start rehashes, finds the same
   identity, and rebuilds an identical archive.
8. Replace `PAUSEMENU.GFX` in a copy of `UI.PAK` with different bytes and
   install it: the log names `DATA/UI/PAUSEMENU.GFX` and `unknown_version`;
   one message; the game runs with the stock menus and VR; the next start
   logs again but shows no message. Restore `UI.PAK`: rebuilt, message
   gone.
9. Change a byte inside an untouched entry (for example a loading image)
   in a copy of `UI.PAK`: built with source `modified`; that entry is in
   the output unchanged.
10. Rename `UI.PAK` away: message at every launch; restoring it brings
    the options page back.
11. Make `DATA` read-only: reason `write`, game runs, one message.
12. Kill the process while it writes the temporary archive (for example
    with a debugger break at the write): the next start rebuilds and the
    game uses a complete archive.
13. In the headset, with runtime OpenVR: menu text, item descriptions,
    the loading tip, subtitles, the crosshair and HUD prompts are readable
    without leaning in at `s_menu` 1.22 and `s_hud` 1.38 (3.6.1); the crosshair is enlarged and stays at the centre
    of the view while aiming, after the weapon changes and after an
    accuracy change; nothing is cut off at the stage edges on the Video
    page with advanced options; the health bar, ammo counter, tutorial
    overlays and subtitles are inside the clear part of the view. Prompts
    show VR glyphs drawn as 04 §4.1 describes, including the confirm and back prompts of menus and the
    `$a$`-style tokens in tutorial text (04 §4.1). The crafting screen
    shows the pad form of the craft prompt after the keyboard was used.
14. Page ids: with the logging of game script events (11 §5) on, visit every stock
    page of the front-end, pause and game-over menus once in a run where
    the options archive is not used (for example the read-only case of
    check 11) and once with `MOTHER.PAK`. The `on_page_changed` ids of the
    stock pages are identical in both runs, and opening a VR page and
    leaving the menu restores it as the game restores stock pages.
15. Video greying: with VR running, open the Video page, show and hide
    the advanced options, change a stock Video setting that makes the game
    push the items' data, and return to the page: the Video items listed
    in 06 §5.3 stay disabled throughout.
16. `UI.PAK`'s SHA-256 and last-write time are unchanged after all of the
    above. Removing the DLL: the game opens `UI.PAK` and is stock.
17. Resource build: the CI job regenerates the bundle from a clean
    checkout without game files and gets the checked-in bytes. A project
    method that uses `try`/`catch`, a nested function, or a hook whose
    signature differs from the declaration makes the build fail with the
    member named.
18. Two launches close together (start the game, and start it again from
    Steam while the first is still on its start-up screen) with
    `MOTHER.PAK` and the key file deleted: one archive is built, neither
    process uses a partly written one, and the log of the second shows a
    reuse or a wait (5.8).
19. With `MOTHER.PAK` and the key file deleted, load the DLL from a
    program that is not the game (a test harness in the game folder), and
    start a game build the mod does not recognise (or one whose preflight
    fails): neither run creates `MOTHER.PAK`, the key file or a temporary
    file in `DATA` (5.1).

## 7. Open questions

- **A camera disturbance in the front end.** Open: whether the VR page
  (or anything else a menu rebuild changes) disturbs the front end's
  background camera or view when entering or leaving it. Candidate
  causes, most likely first [guess]:
  1. Renumbered ids. The native handler for `on_page_changed` receives
     page ids, and the game restores pages with `setCurrentPage(pageId,
     index)` (2.7.2). Pages created earlier than the stock ones, or a moved
     stock construction, renumber stock pages; a page missing from the
     page map cannot be restored, and the menu falls back to the root page.
     3.1 rules this out; acceptance 14 checks it. Also trace the function
     registered with the `on_page_changed` string (pushed at RVA 0x4503B0,
     2.7.7) and what it does with an id it does not know (the VR page's
     automatic id), and watch the front-end background while entering and
     leaving the VR page and its sub-pages.
  2. Camera Speed (`_cameraSensitivityItem`, Game page): a slider from 0 to
     2 that sends `set_gamepad_speed`, pushed by the game with
     `setGamepadSpeed` and saved when the Game page is left; with VR
     controllers arriving as the pad (04) it scales smooth turning on the
     stick (06 §5.3).
  3. Field of view greying lost: the game's `setFieldOfViewData` replaces
     the item's data and re-enables it (2.7.2); 3.1 re-applies greying
     after each replacement; acceptance 15 checks it.
  4. `setHeadTracking`: the game shows the hidden `_headTrackingItem`
     (eye-tracker head tracking) whenever it pushes it; the VR page's
     set-up must not change that item's visibility.
  Transitions are a lower risk: Options, Game and Video use the
  `test_transition` label, Audio has none, and a new page without a label
  plays none, as Audio does. An acceptance check is added when the cause
  is known.
- 2020 build: its `UI.PAK` size, SHA-256 and touched entries; until then
  no options archive on 2020.
- Whether `Loader` completions inside the game's Scaleform are always
  asynchronous. If one completed inside `load()`, `on_all_images_loaded`
  could run before the VR glyph loads are counted (3.4). The stock loader
  has the same exposure, so [likely] they are asynchronous; confirm by
  checking that prompts show VR glyphs in the first HUD movie that loads.
- The two prompt-action choices of 3.4 (`$y$` and `ok`).
- Whether the game's archive lookup needs sorted names (2.2), and whether
  it accepts an absolute path in `UiPakPathPtr` (which would remove the
  current-directory caveat of 5.2).
- The layout players of the earlier mod knew (3.6.6): whether v1 should
  also move the front-end and pause menu lists lower (140 px there) and
  enlarge the title logos, once the headset check shows how the lists at
  1.22 sit against the logos and descriptions; and whether the user wants
  the larger crosshair players knew (3.6.4).
- Screens left at the stock size (3.6.2, 3.6.3): the hub and crafting
  screens, Survivor mode, the radial menu's corner prompts and
  `RELOADING`; each needs a re-layout or its own constant edits, and the
  in-headset check decides which matter.
- The size of the UI render target in stereo, which sets `k` in the HUD
  layout methods (2.7.6); 3.6 assumes the stage is 1280 x 720 or a uniform
  multiple of it.
- Whether FFDec's compiler is byte-deterministic across runs and machines
  at a pinned version (4.5); if not, the project needs another compiler.
