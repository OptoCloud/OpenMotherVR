<!-- Provenance: static analysis of the 2015 `AI.exe` (PE timestamp 0x54E3199D, image base 0x400000) by the spec side: the final post chain and the anti-aliasing pass, the SMAA runner and its resources, the jitter matrix and the camera constants it feeds, the post-processing camera slots and the velocity pass, the main view's per-eye renders, the post-processing fade pair and its sources, the post-processing settings handler (anti-aliasing, depth of field, motion blur, chromatic aberration, film grain), the lens flare draw sites, the per-frame light submission of the transparent pass, the engine settings name table, the engine settings XML load, the shadow map and streaming heap reads; the stock game file `DATA\ENGINE_SETTINGS.XML` of the Steam 2015 install (option names, values and precedences read from the player's own copy); the levels' compiled shader packs (`RENDERABLE\LEVEL_SHADERS_DX11_BIN.PAK`, resource names read by the spec side). Some of these game facts (the AA path, the velocity pass and its per-eye matrices, the post-processing fade pair, the light submission site, the engine settings override point, state-setting wrappers) were located with the help of an analysis of another mod and verified in the 2015 `AI.exe` by the spec side; the rest (camera slot mapping, jitter matrix, lens flares, chromatic aberration and film grain fields, shadow map clamp, heap reads, setting id table, XML load path) were found by the spec side without a lead. Requirements are proposals of this project, pending user confirmation where marked [proposal]. Public API documentation: Direct3D 11, NVAPI variable rate shading for Direct3D 11, OpenVR, OpenXR. The engine settings override as a feature was known from that analysis; the additions of §6.2 are derived from this project's VR needs. Measurements quoted from 03 and 08. 2020 values: none (v2 gives 2015 RVAs only; every v2 feature here stays off on 2020). r2: second clean-room review: 2020 values removed; the pass set-up given as the RVA 0x595170 and re-read; the particle lighting, temporal AA, velocity and engine settings requirements stated as results with this project's reasons; the source of the particle shaders' resource name; the master switch of 20 §1.3 and the engine settings override after a failed runtime (20 §1.2). r3 (spec r7 cross-check): 07 §8 item 1 no longer names the viewport setter or a viewport constant, so the viewport setter 0x621350 (it builds one viewport and calls `ID3D11DeviceContext::RSSetViewports`), the constant writers 0x621C20 and 0x621C90 and the pass set-up's type argument are given as this document's own facts, verified by the spec side in the 2015 `AI.exe`, and the velocity shader question no longer assumes a viewport constant; the settings setter labelled `SettingsSetOption` as in 01 §4, and whether it reaches the saved settings left open as 02 §11 item 5 does; the 2D-only effects keep the player's saved settings as 02 §6.5 now requires of v1. r4 (CLEANROOM r3, what a player sees may be stated exactly): the `Graphics` page of the options page given exactly (item order, labels, control kinds, value names, steps, description lines, greying and the rule for showing an item), as this project's texts in the style of the v1 page players know, under the page rules of 22 §12.1; the game's English menu texts it names read by the spec side from `DATA\TEXT\ENGLISH\UI.TXT` and the anti-aliasing option names from `DATA\ENGINE_SETTINGS.XML`; the `Pixel Scale` item and the Video page's `Shadow Map Resolution` item named by their labels. r5 (user decision of 2026-10-16): no master switch; every key here applies on its own with its v1 value as default (20 §1.3); the `Graphics` page is reached from the `MotherVR` page (22 §12.1). r6 (user decision of 2026-10-18, a tester's request): the additions of §6.2 made exact: `ShadowMap4096` adds the option `4096` and `SharpReflections` the option `Alternative` with value 6 (replacing the proposed value 1), both ranked so that they never become the default, the heap additions' names, and an addition the player's file already offers is not made twice; how the Video page shows an option's name and that the C runtime's open of the engine settings file goes through `AI.exe`'s `CreateFileA` import (§1.9), the reflection quality compares in the reflection manager's code (§12 item 6) and the copy's file name (13 §2), read by the spec side in the 2015 `AI.exe`, `DATA\TEXT\ENGLISH\UI.TXT` and `DATA\ENGINE_SETTINGS.XML`. r7: static analysis of the 2015 `AI.exe` by the spec side (the chromatic aberration reader's flag read and the only readers of `+0x1B38`; the film grain reader 0x5F63D0, its only call, its read of the intensity and the only readers of `+0x608`; §1.6, §8); project decisions with their reasons (§8: a value written for a reader's call does not outlive it; a toggle's call is redirected at attach only for a key that is false then; §10: the three keys' "takes effect" column). Section numbers are unchanged from r6. r8 (user decision of 2026-10-18): the user confirmed the `Graphics` page's items of the features implemented (§10 items 5 to 9 and 11 to 13), which are now shown; the others stay hidden until their feature is implemented and confirmed; a change on the page reaches the 2D effects at once as §10's "takes effect" column says. r9 (a tester's report on OpenXR with a Valve Index: smoke turns with the roll of the head): new §14, upright sprites in stereo; static analysis of the levels' compiled shader packs (`RENDERABLE\LEVEL_SHADERS_DX11_BIN.PAK` of the Torrens and `TECH_HUB` levels, and the global packs under `DATA\SHADERS`) and of the 2015 `AI.exe` by the spec side (the sprite vertex shaders' corner offset in the view plane and its form; the camera-constant upload 0x57A100 and its `ViewMatrix` registers; the vertex constant writer 0x621C20, given to the spec side as an observation and confirmed in the binary); the roll-removal arithmetic derived by the spec side from the requirement and the game's view convention (05 §1); project decisions with their reasons (§14.1: a fix with no key; §14.3: the ease near the vertical and its length; §14.5: axis-locked sprites left as they are). Sections 1 to 13 are unchanged. r10 (a tester's report on OpenXR with a Valve Index, smoke near the floor unlit when looked at directly): the particle lighting lookup in stereo is specified as a v1 fix in 08 §8 (08 r7), pointed to from §4.1; §14.4 states that the upright sprites leave the lookup where it is, from the spec side's reading of the lit sprite shaders in the level packs. Section numbers are unchanged from r9. Revision: r10 -->

# 24. Rendering in v2

v1 renders the game correctly in stereo (02, 03, 07, 08). This document
specifies the v2 rendering features listed in 20 §3.4:

| § | Feature | `Graphics` key(s) | v1 value |
|---|---|---|---|
| 2 | Temporal SMAA T2x with per-eye data | `PerEyeTemporalAA` | false |
| 3 | A project temporal anti-aliasing mode | `TemporalAA` | `"Off"` |
| 4 | Steady smoke, fog and particle lighting | `SteadyParticleLighting` | false |
| 5 | The world behind the pause menu | `PauseWorldBackdrop` | false |
| 6 | Engine settings for VR | `EngineSettingsOverride`, `ShadowMap4096`, `SharpReflections`, `StreamingTextureHeapMB`, `StreamingModelHeapMB` | false, false, false, 0, 0 |
| 7 | Foveated rendering and resolution scale | `FoveatedRendering` | `"Off"` |
| 8 | 2D-only effects | `LensFlares`, `ChromaticAberration`, `FilmGrain` | true, true, true |
| 9 | D3D11 state cost | `StateCache` | false |
| 14 | Upright sprites in stereo | none: a fix of a stereo defect | (applies whenever a headset runtime is chosen) |

Every key is in a new section `Graphics` inside `Settings` of
`mothervr.json` (section 10). With every key at its v1 value the mod is
exactly v1 (20 §1.1): no patch of this document is installed and no work of
this document runs per frame. The exception is §14, a fix of a defect the
game shows only in stereo, which has no key and applies with v1 like the
shader fix of 08 §1.

Addresses are RVAs of the 2015 build (image base 0x400000), read from the
2015 `AI.exe`. The 2020 column is `?` throughout: every feature here stays
off on the 2020 build, and the log says so once (11).

Confidence: **[sure]** the game code or data shows it directly;
**[likely]** strong inference; **[guess]** must be confirmed in game.
Statements about game code without a tag are [sure].

Spec-side labels for game functions are introduced in section 1; 01 states
that the implementation may rename them.

## 1. Game facts

### 1.1 Objects and addresses

| Label | 2015 | 2020 | Facts |
|---|---|---|---|
| `PostFxGet` | 0x5F19C0 | ? | cdecl, no arguments, returns the post-processing object (bytes `b8` + 4-byte address + `c3`) (08 §3.1) |
| `RendererPtr` | global pointer at 0x13583E4 | ? | the renderer; the int at `+0x2C` is the current eye (07 §1) |
| `StereoObjectPtr` | global pointer at 0x1366A2C (01) | ? | stereo object; `+0x04` device, device `+0x04` state (2 = stereo running); `+0x64` scene visibility (02, 05 §3.2g) |
| `SettingsObject` | object at 0x1358A50 (01) | ? | the game's settings; `SettingsSetOption` at 0x5A8910 (01 §4, 02 §6.5); `SettingFind` at 0x59DF70, thiscall `(settings, int id)`, returns the setting object or null |
| `SettingCurrent` | 0x5A4D00 | ? | thiscall `(setting)`, returns the selected quality record (records 0x70 bytes apart): `+0x4C` float value, `+0x50` int value, `+0x54` bool value |
| `SettingNameTable` | 0x124FC20 | ? | array of string pointers indexed by setting id (table 1.2), followed by the paths of the engine settings files (1.9) |
| `JitterMatrixGet` | 0x61A360 | ? | `b8` + address + `c3`: returns the jitter matrix at 0x17DF540 (1.4) |
| `CameraConstants` | object at 0x1357E40 | ? | the frame's camera constants (1.4) |
| `LensFlareGet` | 0x5B61D0 | ? | `b8` + address + `c3`: returns the lens flare object at 0x124FFF4 (1.8) |
| `LightObject` | object at 0x1365C90 | ? | receives the frame's light lists (1.7) |

### 1.2 Setting ids [sure]

`SettingNameTable` gives the name of every setting id. The ids the game's
menu and the XML (1.9) share:

| Id | Name | Id | Name |
|---|---|---|---|
| 0x00 | Depth of Field | 0x0D | MotionBlur |
| 0x01 | Level of Detail | 0x0E | ChromaticAberrations |
| 0x02 | Deep Colour (30bpp) | 0x0F | FilmGrainIntensity |
| 0x03 | FullScreen Resolution 24bpp | 0x10 | Planar Reflections |
| 0x04 | FullScreen Resolution 30bpp | 0x11 | Field Of View |
| 0x05 | Windowed Resolution | 0x12 | Volumetric Lighting |
| 0x06 | Full Screen | 0x13 | Streaming Texture Heap Size |
| 0x07 | Vertical Sync | 0x14 | Streaming Model Heap Size |
| 0x08 | Particles | 0x15 | Screen Space Ambient Occlusion |
| 0x09 | AntiAliasing | 0x16 | Texture Filtering |
| 0x0A | Graphics (the group) | 0x17 | Multithreaded Shadow Rendering |
| 0x0B | ShadowMapResolution | 0x18 | Stereo Mode |
| 0x0C | ShadowMapping | 0x19 to 0x1E | Audio and its settings |

These agree with the ids 02 §6.5 uses.

### 1.3 The anti-aliasing path [sure]

Facts of 08 §3.1 (the AA pass 0x600210, the final post chain 0x60FF40 with
its stereo branch at 0x610144, the SMAA runner call at 0x600479) hold. In
addition:

- The AA pass runs the SMAA runner (0x61A4C0, cdecl, three arguments:
  colour target object 0x17D018C, a second target object 0x17D0238, `const
  bool*` temporal; prologue `55 8b ec 83 e4 f0 83 ec 64`) only for AA modes
  2 and 3; the temporal flag it passes is true only for mode 3.
- **SMAA state** is a set of globals: the SMAA target width and height at
  0x17DF200 and 0x17DF204, written by the SMAA set-up (0x61A380) from the
  post-processing object's `+0x04` and `+0x06` (16-bit) sizes, so in stereo
  they are the side-by-side size [likely: the post-processing sizes are the
  main render size]; a frame index (0 or 1) at 0x17DF208; two lookup
  textures at 0x17DF20C and 0x17DF210 (the runner does nothing if either is
  null).
- **History.** The set-up creates two pairs of full-size render target
  objects (0x84 bytes each): 0x17DF330 + i·0x84 and 0x17DF438 + i·0x84 for
  i = 0, 1. In a temporal run the runner binds the pair element for the
  current frame index and the one for the other index as inputs of its
  final resolve draw, then flips the frame index (`index = (index - 1) & 1`).
  There is one history for the whole target, not one per eye.
- **Jitter.** After a temporal run the runner writes a sub-pixel offset into
  the translation row (`+0x30`, `+0x34`) of the jitter matrix at
  0x17DF540, from a two-entry pattern of diagonal quarter-pixel offsets
  selected by the frame index and scaled by the SMAA target size; after a
  spatial run it writes zero there. This confirms the [likely] of 08 §3.1:
  spatial SMAA clears the jitter. The offsets are scaled for the whole
  side-by-side target, so in stereo the horizontal offset is half of a
  quarter pixel of each eye's image [likely].
- **Where the jitter applies.** The camera-constant setter 0x57A8A0
  (thiscall on `CameraConstants`, four stack arguments: context, view,
  projection, projection copy; `ret 0x10`) multiplies the view-projection
  and the projection by the jitter matrix and uploads them as vertex
  constants (buffer 0 registers 0, 8, 12; buffer 1 register 0); the
  unjittered view and the inverse view-projection are uploaded too (buffer 1
  registers 4 and 8, buffer 0 register 16). Every POV's begin-render
  (0x5B11D0) calls it, so both eyes of a frame use the same jitter.

### 1.4 Per-eye camera data and the velocity pass [sure]

The post-processing object keeps **three camera slots**, 0x170 bytes apart
from `+0x690`. The slot of an eye is given by 0x5F19A0 (cdecl, one int):
eye 0 gives slot 1, eye 1 slot 2, anything else (the centre, -1) slot 0.
Within a slot:

| Offset in slot | Content |
|---|---|
| `+0x690` | previous frame's view-projection (copied from `+0x710` before it is replaced) |
| `+0x6D0` | previous frame's second view-projection (from `+0x750`) |
| `+0x710` | this frame's view-projection |
| `+0x750` | this frame's view times projection copy |
| `+0x790` | inverse of `+0x710` |
| `+0x7F0` | camera position |

The slot update (0x5F6000, thiscall `(postfx, int slot, view, projection,
projection copy)`, `ret 0x10`, prologue `55 8b ec 83 e4 f0 81 ec 94`) is
called only from `POV set eye` (07 §2.4) at 0x5B3A12 (`e8 e9 25 04 00`), for
the main view, with the slot of **SetEye's argument**.

The **velocity pass** (0x5B1720, thiscall on the POV, no stack arguments;
prologue `56 8b f1 8a 86 6b 03 00 00`) runs when the POV has flag bits 0
and 4, and either motion blur is active (post-processing `+0x1B2D` set and
its amount `+0x1988` above 0) or the AA mode `+0x1B3C` is 3. It begins a
pass of type 0xB and calls the post-processing velocity draw (0x5FFDA0,
thiscall `(postfx, int slot)`, `ret 4`) at 0x5B17EE with the slot of the
POV's **`+0x378`**. The velocity draw uploads the slot's inverse
view-projection and the two previous view-projections as pixel constants
(buffer 2 registers 12, 16, 20) and the camera position (vertex buffer 0
register 22), and draws its full-screen effect. While stereo runs and the
POV is the main view, the velocity pass passes a scale of 0.5 to its pass
set-up instead of the POV's own `+0x31C`.

The main view's eye renders:

| Render | Function | Velocity pass |
|---|---|---|
| eye 0 (and the flat view) | 0x5B5BE0: `SetEye(-1)`, then the begin, the main render 0x5B3F10, and the velocity pass at 0x5B5C22 (`e8 f9 ba ff ff`) | yes |
| eye 1 | 0x5B5B70 (prologue `53 8b d9 8a 83 6b 03 00 00`): `SetEye(1)`, the begin, the main render, then the centre matrices restored from the save area (07 §2.4) | no |

Consequence in stereo [sure for the code, likely for the effect]: `SetEye(-1)`
sets `+0x378` to 0 (07 §2.4), so the eye-0 slot update writes slot 0 (from
the argument -1) while the eye-0 velocity pass reads slot 1, which no call
updates in stereo. Eye 1's slot 2 is updated every frame but no velocity is
drawn for it. So in stereo neither eye has correct velocity, and only one
history exists for both eyes (1.3). This is why v1 runs SMAA T2x spatially
(08 §3.2).

### 1.5 The post-processing fade pair [sure]

The post-processing object holds two floats at `+0x1934` and `+0x1938`,
read together by the getter 0x5F2510. At the start of each post-processing
frame (0x608C50, called at the top of the post-processing render 0x609390)
the object's settings block from `+0x190C` is copied from a global block at
0x1250A60 (so `+0x1934`/`+0x1938` come from 0x1250A88/0x1250A8C), then a
fade (mode `+0x1B44`: 1 set at once, 2 and 3 move toward the targets
`+0x1B50`/`+0x1B4C` by the rate `+0x1B48`, active while `+0x1B60` is set)
may replace them. Script code requests and releases these fades (requests
counted at `+0x1B54`; callers in the game's script entities). The stereo
output multiplies both values by the stereo scene visibility and uploads
them as pixel buffer 2 register 36 for the final draw (08 §3.1). When both
are 0 the stereo output is black.

### 1.6 Post-processing settings handler [sure]

The post-processing object's setting handler (0x5F6AC0, thiscall, one
argument, `ret 4`; prologue `57 8b f9 8b 4c 24 08 8b 01 83 f8 09`) stores:

| Setting | Field | Value |
|---|---|---|
| 0x09 AntiAliasing | `+0x1B3C` | 1 FXAA, 2 SMAA T1x, 3 SMAA T2x (by quality name), otherwise 0 |
| 0x00 Depth of Field | `+0x1B37` | the bool value |
| 0x0F FilmGrainIntensity | `+0x608` | the float value clamped to [0, 1] |
| 0x0D MotionBlur | `+0x1B2D` | int value 1 gives 1, otherwise 0 |
| 0x0E ChromaticAberrations | `+0x1B38` | the bool value |

Readers, on the render thread:

- **Chromatic aberration**: 0x5F1E00 (thiscall on the post-processing
  object, called at 0x6093E9 in the post-processing render) sets parameter 2
  of the post-processing material (`+0x62C`) to the aberration scale
  `+0x1A60` when `+0x1B38` is set and the scale is above 0, otherwise to
  -1.0.
  The flag is read only there (`80 be 38 1b 00 00 00`, `cmp byte ptr
  [esi+0x1B38], 0`, at 0x5F1E74); the only stores to it are the handler's
  (0x5F6C09) and one at 0x5FF71A [likely the object's set-up] [sure for a
  byte-pattern search of the code section].
- **Film grain**: the post-processing material parameter 9 is built from
  grain values scaled by `+0x608` (around 0x5F65A8). The reader is
  0x5F63D0 (thiscall on the post-processing object, no arguments, plain
  `ret`; prologue `83 ec 30 f6 05 cc`), which also sets parameters 10
  and 11; its only call is at 0x6005DF (`8b ce e8 ec 5d ff ff`, `mov ecx,
  esi; call`). It reads the intensity once, at 0x5F65A8 (`f3 0f 10 8e 08
  06 00 00`, `movss xmm1, [esi+0x608]`), and uses 0 instead when the byte
  `+0x1B2C` is clear. `+0x608` is read nowhere else; the only stores to it
  are the handler's (0x5F6BCA) and one at 0x5FF0FA [likely the object's
  set-up] [sure for a byte-pattern search of the code section].

### 1.7 Light submission of the transparent pass [sure for the code]

The main render (0x5B3F10) runs the transparent-object pass (0x5B0470,
thiscall on the POV, five stack arguments) with its fifth argument `eye != 1`.
That pass calls 0x594050, which calls 0x592EB0 with (true, `eye != 1`).
0x592EB0 then:

- at 0x592FF4 (`80 7c 24 14 00 74 07 8b cb e8 0e fe ff ff`): if its first
  argument is true, calls 0x592E10, which submits the frame's two light
  lists (20-byte entries; counts at `+0x504` and `+0xA508` of its object) to
  `LightObject` (begin 0x5D4B50, one call of 0x5D4D10 per light, end
  0x5D4C70) and rebinds the result;
- at 0x593002: if its second argument is true, runs 0x5DEEE0 on
  `LightObject`.

So the light lists are submitted again in **each** eye's render, while the
follow-up step runs only for eye 0 (and in flat play). A separate job,
`ALPHALIGHT_TASK` (registered at 0xD48D10, job function 0x3753C0), runs
0x5D52D0 on `LightObject` once per frame. The levels' compiled shader packs
bind a texture and a sampler named `alphalightIntensitiesReg_TEX` and
`alphalightIntensitiesReg_SMP` (72 shaders of each in the Torrens level's
`RENDERABLE\LEVEL_SHADERS_DX11_BIN.PAK`, read by the spec side) [sure for
the names]. That these lists and this job are the lighting of smoke, fog
and particles, read through that texture, is [likely]: the job name, the
shader resource name and the transparent pass agree; the link from
0x592E10 to that texture is not traced.

### 1.8 Lens flares [sure]

The lens flare object (0x124FFF4, from `LensFlareGet`) is set up at
0x5D47B0 from `DATA\LENS_FLARE_CONFIG.BIN` and `DATA\LENS_FLARE_ATLAS.BIN`
with the material `LENS_FLARE_MATERIAL`; lights carry `has_lens_flare`,
`lens_flare_brightness` and `lens_flare_colour` parameters. Its draw
(0x5D3970, thiscall `(lensflares, int pass)`, `ret 4`; does nothing unless
`+0x28` is set and `+0x0C` is non-null) is called at two sites:

| Site | Bytes | Pass | Runs |
|---|---|---|---|
| 0x609AD0 (in the post-processing render) | `6a 01 e8 f9 c6 fa ff 8b c8 e8 92 9e fc ff` | 1 | flat and stereo |
| 0x610215 (end of the final post chain, flat branch) | `6a 00 e8 b4 5f fa ff 8b c8 e8 4d 37 fc ff` | 0 | flat only (the stereo branch returns at 0x6101EE first) |

A preparation call (0x5D3950) precedes the first site. Flares are drawn in
screen space over the finished image.

### 1.9 Engine settings XML [sure]

- The game reads its option definitions from the loose file
  `DATA\ENGINE_SETTINGS.XML` (path string at 0xFE3FFC, pointer at
  `SettingNameTable` entry 0x124FC9C). The player's chosen values are a
  separate file, `DATA\engine_settings_savefile.bin` (or `.txt`).
- The XML is parsed once, in the display initialisation at start-up on the
  main thread: 0x613984 calls 0x5AFF50, which reaches the parse at 0x5AFCF0.
  The load site at 0x5AFD02 holds `a1 9c fc 64 01 6a 01 50 8d 4c 24 38 e8 ed
  27 eb ff` (load the path pointer, push 1 and the path, call the XML
  reader's load 0x462500). The reader opens the file with the C runtime's
  `fopen` (mode `rb`, statically linked) and parses from that stream. This
  runs after the stereo device is created (0x613941, 02 §6.1).
- The path the game holds is the relative `DATA\engine_settings.xml`
  (lower case). The reader passes it through the game's path alias list
  (0x4B0290) before the open [likely: unchanged when no alias matches].
  `fopen` (0x67472A) reaches the C runtime's narrow open, which calls
  `CreateFileA` through `AI.exe`'s import of `kernel32.dll` (the call at
  0x6849B7), for reading an existing file. So the game's only read of the
  file is an open through that import [sure for the code path].
- **Option names on the Video page.** The page shows each option by its
  `name` attribute. Where `DATA\TEXT\ENGLISH\UI.TXT` has a key
  `AI_UI_MENU_QUALITY_NAME_<NAME>` (the name upper-cased, spaces as `_`:
  `ON`, `OFF`, `ULTRA`, `HIGH`, `MEDIUM`, `LOW`, `VERY_LOW`, `STANDARD`,
  the texture filters) it shows that text in the player's language; names
  without a key (`512` to `2048`, `FXAA`, `SMAA T1x`, `SMAA T2x`, `HDAO`)
  are shown as written [likely: no key exists for those names, and the page
  shows them]. So an added option is shown by its name as written, in every
  language, and needs no change to the game's text files. The page lists a
  setting's options in the file's order [likely; to be confirmed in game,
  §6.4].
- The stock file of the Steam 2015 install defines (values as written in
  the player's file):

| Setting | Options (name: value, precedence) |
|---|---|
| ShadowMapResolution | 512: 512, 1; 1024: 1024, 2; 1536: 1536, 3; 2048: 2048, 4 (default needs GPU index 7.6) |
| ShadowMapping | Very Low 2, Low 3, Medium 4, High 5, Ultra 6 ("contact hardening") |
| Planar Reflections | Off 0, On 3 |
| Screen Space Ambient Occlusion | HDAO 1 (precedence 1), Standard 0 (precedence 2), Off 2 (precedence 0) |
| Streaming Texture Heap Size | Default 300 (precedence 1), Ultra 300 (precedence 2, needs 2200 MB main memory) |
| Streaming Model Heap Size | Default 128 |
| AntiAliasing | Off 0 (1), FXAA 1 (2), SMAA T1x 2 (4), SMAA T2x 3 (3) |
| ChromaticAberrations | Off, On (bool) |
| FilmGrainIntensity | 0.0 to 1.0 in steps of 0.1 |
| MotionBlur | Off 0, On 1 |
| Stereo Mode | Off only |

  Precedence orders the options; the highest precedence whose
  requirements hold is the default for a new settings file [likely]. So a
  fresh install defaults to SMAA T1x and Standard SSAO.
- **How the values are used:**
  - ShadowMapResolution and ShadowMapping are read in 0x611D00 (from the
    display setting handler, 0x612263, and 0x5F90D2): the resolution is
    clamped to [512, 4096] and the PCF value to [2, 5]. Without the
    setting the resolution is 512 or 1024 by a quality hint. So a 4096
    option is accepted by the engine without code changes.
  - Streaming Texture Heap Size (0x13) and Streaming Model Heap Size (0x14)
    are read as megabytes (`int << 20`) by 0x5F6E80 and 0x5F6D30 (both
    reached through a table at 0xFE7088 / 0xFE70F0); the texture heap takes
    the larger of the setting and a value at `+0x2090` of its object. When
    they run (start-up only or also on change) is [likely] start-up.
  - Planar Reflections quality: a value of 2 builds mips by hardware, 3 by
    a blur, anything else gives one mip and no blur (07 §2.2).

## 2. Per-eye temporal SMAA T2x (`Graphics.PerEyeTemporalAA`)

### 2.1 What the player experiences

v1 runs the game's SMAA T2x as T1x in stereo (08 §3.2). With this feature
on and SMAA T2x chosen in the game's options, edges are smoothed over two
frames in each eye, as on a flat screen: thin geometry, grates and
specular highlights shimmer less on head motion than with T1x, without
ghosting between the eyes or across the seam.

### 2.2 Required behaviour

While stereo runs, AA runs in stereo by v1's rules (`StereoAntiAliasing`
and the pixel scale, 08 §3.2), the AA mode `+0x1B3C` is 3 and the key is on,
these results hold every frame:

1. **Temporal.** The SMAA runner runs with its temporal flag true (the v1
   change at 0x600479 does not force it false).
2. **Velocity for both eyes.** After both eye renders, each half of the
   velocity target holds the motion of that eye's pixels: computed from that
   eye's own current and previous view-projection and that eye's depth,
   written within that eye's viewport only. Eye 0's velocity uses the eye-0
   data, not the stale slot 1 of 1.4; eye 1's half gets its velocity
   although the game draws none for it. How is the implementation's
   choice; eye 1's velocity must land in eye 1's half of the velocity
   target (§12 items 1 and 2 give the viewport and units questions).
3. **Previous matrices per eye.** "Previous" for an eye means the same
   eye's matrices of the previous rendered stereo frame. The two eyes never
   share a previous matrix.
4. **Jitter.** Each eye is jittered by the same sub-pixel pattern, two
   diagonal quarter-pixel offsets alternating every rendered frame,
   measured in pixels of the eye's own image (horizontally half the
   game's offset for the whole target, 1.3, or the game's offset if in game
   the eye image turns out to be jittered correctly). Both eyes of a frame
   use the same pattern entry; the frame index advances once per rendered
   frame, as the game's runner does.
5. **History per eye.** The temporal resolve combines each eye's pixels
   only with history from that eye's half. A reprojected position that
   falls outside the eye's half is clamped to its edge or rejected; it
   never samples the other eye. The game's history (1.3) is the whole
   target, so its halves are already per eye; only the reprojection near
   the seam needs this rule.
6. **History reset.** The first temporal frame after any of these runs
   spatially (no history used): stereo start; a display reset or stereo
   target rebuild (02 §9.3, pixel scale change); the first head-tracked
   frame after screen frames (menus, loading, cutscene cameras on the
   virtual screen, 03 §7); a recenter (03 §2); a camera cut the game makes
   (a jump of the camera position of more than 1 m in one frame
   [proposal: a step no walking or leaning produces at 90 Hz, so it marks
   a cut or teleport]).
7. **Blackout kept.** The head-in-wall scene visibility (register 36,
   08 §3.2) still darkens the image.

Otherwise (key off, AA mode not 3, AA not running in stereo, stereo not
running) the v1 behaviour of 08 §3.2 holds exactly: T2x runs spatially in
stereo.

### 2.3 Sites and checks

All checked before the first write; all of the feature or none (12 §4):

| Site | 2015 | Check |
|---|---|---|
| SMAA runner call (v1 site) | 0x600479 | `e8` to 0x61A4C0 |
| velocity pass | 0x5B1720 | prologue `56 8b f1 8a 86 6b 03 00 00` |
| eye-0 velocity call | 0x5B5C22 | `e8 f9 ba ff ff` |
| eye-1 render | 0x5B5B70 | prologue `53 8b d9 8a 83 6b 03 00 00` |
| slot update call in SetEye | 0x5B3A12 | `e8 e9 25 04 00` |
| velocity draw call | 0x5B17EE | `e8 ad e5 04 00` |
| jitter matrix getter | 0x61A360 | `b8 40 f5 bd 01 c3` (the address operand is relocated with the image; check the opcode bytes `b8` and `c3`) |

The v1 AA fix (08 §3) must be installed: this feature changes its temporal
rule and is off when it is not.

### 2.4 Cost

The velocity pass for eye 1 is one more full-screen pass over half the
target; the temporal resolve is the game's. Not measured [likely: below the
0.6 ms of SMAA itself at 8064x4032, 08 §3.2]. Required: measure in a heavy
scene at pixel scale 1.0 and record it in the commit (STYLEGUIDE §9).
Budget [proposal]: at most 0.5 ms GPU over v1's T1x-in-stereo at pixel
scale 1.0 on a Valve Index class headset, because 90 Hz leaves 11.1 ms and
v1 already measures 3.1 to 3.6 ms in a light scene (03 §10).

## 3. Project temporal anti-aliasing (`Graphics.TemporalAA`)

### 3.1 What the player experiences

A temporal AA stronger than SMAA T2x (more frames of history), for players
who see shimmer even with §2. [proposal: lower priority than §2; §2 uses
the game's own shaders and is much smaller. Only to be built if §2 in game
leaves visible shimmer.]

### 3.2 Required behaviour

With `"On"`, while stereo runs and AA runs in stereo (08 §3.2):

- the project's own TAA replaces the game's AA pass output (any game AA
  mode, including Off) and is written by this project in HLSL (CLEANROOM:
  shaders are made here);
- it uses the per-eye velocity, per-eye previous matrices, the per-eye
  history rule and the history resets of §2.2 items 2 to 7, with a jitter
  sequence of at least 8 positions [proposal: a low-discrepancy sequence
  of 8 or more gives a stable result; 2 positions is T2x];
- the image in each eye has no more shimmer than with §2; nothing that
  moves leaves a trail; no surface whose content changes (screens,
  displays) shows trails; transparent surfaces shimmer no more than
  without TAA; and the image is not visibly softer than with SMAA T1x
  [proposal: these are what a player notices of a temporal AA in a
  headset]. How (masks, jitter on transparent passes, sharpening) is the
  implementation's choice; §12 item 3 asks how changing surfaces can be
  recognised.

With `"Off"`: §2 or v1 applies. `"On"` is ignored (log once) while
`PerEyeTemporalAA` is the only temporal mode available in a build that does
not implement §3.

### 3.3 Cost

Not measured; budget as §2.4 plus the TAA pass itself [proposal: 1.0 ms
GPU at pixel scale 1.0].

## 4. Steady smoke, fog and particle lighting (`Graphics.SteadyParticleLighting`)

### 4.1 Problem

(r10) The lookup of the particle lighting in stereo, which placed the
lit part of smoke with the wrong scale so that it moved with the head, is
a v1 fix with no key, 08 §8; this section is about frame-to-frame and
eye-to-eye differences of the lighting itself.

[guess, to be confirmed in game, §4.4]: smoke, steam, fog and dust
particles may flicker in brightness from frame to frame or differ between
the two eyes, most visibly in dark areas lit by a few lights (for example
vents and corridors with steam). Candidate cause from the game code (1.7): the light lists are
submitted to `LightObject` again during eye 1's render, after eye 0's
follow-up step has run and before the `ALPHALIGHT_TASK` job of the next
frame, so the lighting state the particles read can belong to either
eye's submission depending on job timing [guess].

### 4.2 Required behaviour

While stereo runs and the key is on:

- particles, smoke and fog are lit from one light state per frame, the same
  for both eyes;
- with the camera and lights still, their brightness does not change from
  frame to frame;
- they still respond to lights the game turns on, off or moves (no frozen
  or constant lighting) [proposal: the game's dark scenes depend on
  particle lighting];
- in flat play and for eye 0, and with the key off, the game's submission
  runs as it does (v1).

Proposed means [proposal, to be confirmed by the in-game check of §4.4
step 2]: in stereo, eye 1's transparent pass does not submit the light
lists a second time (1.7, site 0x592FF4 with the bytes of 1.7), so both
eyes read the state built for eye 0.

If the check shows the flicker exists in flat play too (a game behaviour,
not a stereo one), the requirement still holds but the means is open
(§12 item 4).

### 4.3 Edge cases, threads

The submission runs on the render thread inside the eye renders; the
stereo state is read there with an atomic load (02 §9.1). Cutscenes and
screen frames: unchanged rule (it is per eye render). Loading: no effect.

### 4.4 Acceptance

1. Find a scene with steam or smoke in front of a few lights (Medical,
   Seegson Synthetics corridors); record it with the key off: describe the
   flicker in the log with `DebugOptions` (the light counts submitted per
   eye, logged every 900 frames, 11 §3).
2. Run the same scene flat (runtime None): does it flicker? Record.
3. Key on: no frame-to-frame flicker with the head still; both eyes equal
   (close each eye in turn); lights switching on and off still change the
   smoke.

## 5. The world behind the pause menu (`Graphics.PauseWorldBackdrop`)

### 5.1 Problem

In v1 the pause menu is a screen frame: it is shown on the virtual screen
and the headset is black around it (03 §7, 05 §5). Players lose their
place in the world and the black void is less comfortable than seeing
where they paused.

### 5.2 Game facts relevant here

- While the pause menu is open the game is in mode 1 (05 §5) and does not
  ask for a head pose (it is a screen frame, 03 §7); the game's camera
  does not follow the head [sure, from 03 §7 and 05].
- The final image of the game is scaled by the post-processing fade pair
  (1.5); a fade to 0 makes the final image black. Whether the game drives
  that pair (or another post-processing setting) to black when it pauses,
  so that the game's own frame behind the menu is black, is not known
  [guess]; §12 item 5 says how to find out.

### 5.3 Required behaviour

While stereo runs, the key is on and the pause menu is open (between its
Open and Close, 05 §5):

- the headset shows the last head-tracked stereo frame before the pause
  opened, world-locked (it does not move with the head: the runtime's
  reprojection or the mod's own placement keeps it where it was), darkened
  to 40 % brightness [proposal: dark enough that the menu reads clearly,
  bright enough to recognise the place], behind the virtual screen of
  03 §7, which stays where 05 §4.2 places it;
- the virtual screen shows the menu as in v1;
- when the head moves far from where the frame was taken (more than 0.5 m
  [proposal: beyond this, reprojection of a single frame shows large holes
  and stretched edges]) or turns more than 90 degrees away, the backdrop
  fades to black over 0.3 s [proposal: no hard pop], and returns when the
  head comes back;
- on Close the next head-tracked frame replaces the backdrop at once.

Means: the implementation keeps a copy of the stereo texture of the last
head-tracked frame, and its render pose, and submits that pair while the
menu is open (OpenVR: as the eye textures with the stored pose; OpenXR: as a
projection layer with the stored views) with the overlay or quad layer of
the menu in front. No game patch. The copy is made at most once per pause
(no per-frame copy in gameplay): the stereo texture is a game resource that
the game draws the paused frame into, so the copy is taken at the pause
Open from the frame before, which requires keeping the previous
head-tracked frame's texture: a second texture of the stereo target's size
and one `CopyResource` per head-tracked frame while the key is on
[proposal: the simplest way to have the pre-pause frame; cost to be
measured, §5.5].

Not shown as backdrop: loading screens (the world behind is half-loaded,
05 §3.2g), the front-end menu, the title video, cutscenes on the virtual
screen. Death and game-over screens: the backdrop applies only to the pause
menu.

With the key off: v1 (black around the virtual screen).

### 5.4 Edge cases

Pause opened from a screen frame (a cutscene paused): no head-tracked frame
of this scene exists; black as v1. Display reset while paused: the stored
copy is dropped; black until Close. Headset off while paused: nothing
special. Runtime None: no stereo, no feature.

### 5.5 Cost and threads

One full-target copy per head-tracked frame on the render thread while the
key is on [likely: a few tenths of a millisecond of GPU copy at 2x2688x2688;
to be measured]; no allocation per frame (the copy target is created with
the stereo target in slot 3 and released in slot 4, 03 §4.4).

## 6. Engine settings for VR

### 6.1 Rule: the player's own file, overridden in memory

The game's options come from the player's own `DATA\ENGINE_SETTINGS.XML`
(1.9). v2 never writes to that file and never ships a modified copy of it
(CLEANROOM; the same rule as for `UI.PAK`, 06 §7). With
`Graphics.EngineSettingsOverride` on, when the game parses the file at
start-up (1.9), the parse sees the player's file with the additions of 6.2
applied, and only those:

- every element and attribute of the player's file is kept as it is; the
  additions are new `Quality` elements inside existing `Setting` elements;
- if the player's file does not have the expected setting (a modified or
  unknown file), that addition is skipped and logged; the rest still
  applies;
- if the setting already offers the addition (an option of the same name,
  or, for an option that is never the default, of the same value; for a
  heap size, the setting's default already has that value), it is not
  added again and this is logged;
- if the player's file cannot be read or parsed as XML by the mod, nothing
  is added and the game reads its file as without the mod.

An added option goes after the setting's last option, on its own line, in
the indentation and line ending of the lines around it, so a player
comparing the copy with the original sees only the additions.

Means: the implementation's choice, within these constraints. The game
reads the file through the C runtime's `fopen` at the load site 0x5AFD02
(1.9), once at start-up. Acceptable: redirecting that load to a copy the
mod derives from the player's file at that moment, in the mod's own folder
outside `DATA` (named by 13), or serving the bytes from memory at the
file-read level. The copy, if any, is rebuilt every launch from the
player's current file, so a game update or a player's edit is picked up.

The override is decided at attach (it must exist before display
initialisation); a change of the key applies at the next launch.

### 6.2 Additions

Each addition answers a need of VR play that the stock options do not
meet:

- **Shadows** are seen closer and larger in a headset than on a monitor,
  so the stock maximum of 2048 shows blocky shadow edges; the engine
  accepts resolutions up to 4096 (1.9).
- **Reflections**: the blurred mips of the planar reflection cross the
  seam between the eye halves of the reflection target (07 §4.6), and a
  blurred mirror reads as a smear at headset resolution; the engine
  already has a mode with one mip and no blur (1.9, 07 §2.2).
- **Streaming heaps**: turning the head in VR reveals more of the level
  in a short time than turning a camera with a stick, so textures and
  models arrive late more often with the stock heap sizes.

| Key (all `Graphics.`) | Setting | Addition | Reason |
|---|---|---|---|
| `ShadowMap4096` | ShadowMapResolution | an option named `4096`, int 4096, no default requirements, precedence one below the setting's lowest (not below 0) | shadows above. Never chosen automatically: the game picks the highest precedence whose requirements hold as the default of a new settings file (1.9), so the lowest precedence keeps a fresh install on the stock choice; the player chooses 4096 on the Video page |
| `SharpReflections` | Planar Reflections | an option named `Alternative`, int 6, precedence one below the setting's lowest (not below 0) | reflections above. Value 6 is neither off (0) nor a mip mode (2, 3), so the engine renders one mip without blur (1.9, 07 §2.2); the reflection manager's code compares the quality only with 0, 2 and 3 (§12 item 6), so 6 behaves as 1 would. Name, value and rank: user decision of 2026-10-18 (tester request; the engine already supports value 6 and 4096); never the default for the same reason as 4096 |
| `StreamingTextureHeapMB` | Streaming Texture Heap Size | when not 0: an option named `<value> MB` (for example `600 MB`) with that int value, precedence one above the setting's highest | streaming heaps above. Range 300 to 1024 [proposal: 300 is the stock size; 1024 keeps a 32-bit process's address space safe, §12 item 7] |
| `StreamingModelHeapMB` | Streaming Model Heap Size | when not 0: as for the texture heap | streaming heaps above; range 128 to 512 [proposal: 128 is stock; 512 bounds address space] |

The names `4096` and `Alternative` are what the player sees on the Video
page (1.9): `Shadow Map Resolution` then offers `512`, `1024`, `1536`,
`2048`, `4096`, and `Planar Reflections` offers `Off`, `On`, `Alternative`.

The heap additions become the selected value only if the game selects by
precedence for these two settings (they have no menu control) [likely]; if
the game keeps a saved value instead, the requirement is that the value in
effect is the key's (§12 item 7).

No other option or precedence is changed [proposal]: precedence only picks
the default of a new settings file, which concerns flat play, and the
settings v1 controls in stereo (02 §6.5, 08 §3.2) need no XML change.

### 6.3 Edge cases

The game's saved settings file may name an option the override adds (for
example `4096` chosen last run) while the override is now off: the game then
finds no such option; what it does is [guess] (falls back to a default,
§12 item 8). Required: turning the override off must not stop the game
from starting; if the game would fail, the override must keep the added
options as long as a saved value names them.

### 6.4 Acceptance

Override on with `ShadowMap4096`: the Video page's `Shadow Map Resolution`
item offers `4096`, after `2048`. Override on with `SharpReflections`: its
`Planar Reflections` item offers `Alternative`, after `On`, and with it
chosen the reflections are sharp. A fresh settings file (none saved yet)
still starts on the stock choices. The
log shows the additions made; shadows are sharper; the player's
`DATA\ENGINE_SETTINGS.XML` is byte-identical to before. Override off: the
menu is stock. A deliberately broken XML (in a test copy of the game only):
the game starts as without the mod and the log says nothing was added.

## 7. Foveated rendering and resolution scale

### 7.1 Foveated rendering (`Graphics.FoveatedRendering`)

**Public API facts.** Direct3D 11 has no variable rate shading of its own.
On NVIDIA GPUs of the Turing generation and later, NVAPI offers variable
rate shading for Direct3D 11 (a shading-rate resource bound per viewport).
Other vendors offer no such Direct3D 11 interface the project can rely on
[likely; to be rechecked when implementing].

**Required behaviour** while stereo runs and the value is not `"Off"`, on a
GPU and driver that support it:

- the edges of each eye's view are shaded at a coarser rate during the
  game's opaque scene passes into the main stereo target; the centre of each
  eye stays at full rate;
- the full-rate region is centred on each eye's optical centre (the centre
  of its crop rectangle, 03 §3.3), not the centre of the eye's half of the
  target, because headsets with asymmetric eyes have their optical centre
  off the image centre;
- `"Low"`: full rate within 70 % of the half-width from the optical centre,
  2x2 beyond [proposal: barely visible on current lenses, where the outer
  third is blurred by the optics]; `"High"`: full rate within 50 %, 2x2 to
  70 %, 4x4 beyond [proposal: noticeable but large saving];
- never applied to post-processing, UI, the virtual screen, the mirror, the
  reflection target, shadow maps or the velocity pass (coarse shading there
  produces visible blocks or wrong data);
- without support (other vendors, older GPUs, NVAPI missing), the feature
  stays off, the log says why once, and v1 rendering runs.

Which game passes are "the opaque scene passes" must be identified (the
pass types the main view gives the pass set-up 0x595170, a thiscall
whose first stack argument is the pass type; the velocity pass gives it
type 0xB) before this is implemented (§12 item
9). Cost and gain to be measured; the feature is
worth keeping only if it saves at least 10 % GPU time at pixel scale 1.5
[proposal].

### 7.2 Resolution scale

v1 has one resolution multiplier, `Rendering.PixelScale` (03 §4.1, §10),
applied on top of the runtime's own resolution. v2 adds **no second static
scale** [proposal: two multipliers that multiply each other confuse players
(03 §10 already warns about the runtime's one)]. The pixel scale stays the
v1 options-page control it is (`Pixel Scale` on the `Rendering` page, 06
§5).

Dynamic resolution (changing the render size with load) is not offered
[proposal]: the game rebuilds its targets only through a display reset,
which stalls 177 to 232 ms (02 §9.3), so the size cannot follow the load
smoothly.

## 8. 2D-only effects

Effects designed for a flat screen that look wrong in stereo. For each: the
game fact, the problem, and the required toggle. All toggles default to the
game's behaviour (v1) and act only while stereo runs; flat play is never
changed, and the player's saved game settings are never changed by them
[proposal: the player who goes back to flat play keeps their own choices,
as v1 requires for the three settings it forces in stereo, 02 §6.5].

| Effect | Game fact | Problem in stereo | Key and required result |
|---|---|---|---|
| Lens flares | 1.8: drawn in screen space over the final image, pass 1 in both paths | a flare is positioned in the side-by-side image, not in each eye's view, so it does not fuse and floats at the wrong depth [likely; confirm in game] | `Graphics.LensFlares` (bool, default true). False: no flare is drawn while stereo runs (the draw at 0x609AD0 does nothing for the main view; the flat-only site 0x610215 is unaffected) |
| Chromatic aberration | 1.6: game setting 0x0E, material parameter 2 of the post-processing material, set by 0x5F1E00 | fringes are computed from the centre of the side-by-side image, so they grow toward the seam and differ between eyes [likely]; real lenses already add their own | `Graphics.ChromaticAberration` (bool, default true). False: while stereo runs, the parameter is set as when the setting is off (-1.0), whatever the player's setting |
| Film grain | 1.6: game setting 0x0F, `+0x608` scales material parameter 9 | grain identical in both eyes reads as a pattern on a pane in front of the eyes, not as image noise [likely] | `Graphics.FilmGrain` (bool, default true). False: while stereo runs, the grain is drawn as with intensity 0, whatever the player's setting |
| Cutscene letterboxing | not located in `AI.exe`: no letterbox, border or bar name in the executable or `UI.PAK` [sure for the search]. Pre-rendered videos may have bars in the video itself; those play on the virtual screen (03 §7), where bars are correct | none known for videos | no key in r1; §12 item 10 |
| Vignettes | the game has a vignette post-processing setting (`VignetteSettings`, `vignette_factor`, `vignette_chromatic_aberration_scale`) and a hiding vignette controller (`CUSTOM_HIDING_VIGNETTE_CONTROLLER`), names in `AI.exe` [sure]; where they are drawn is not traced | a vignette centred on the side-by-side image darkens the inner edges of each eye [guess] | no key in r1; §12 item 11 |
| Depth of field, motion blur, SSAO | v1 turns them off in stereo (02 §6.5, 05 §7.1) | | unchanged |

Means for the three toggles: the implementation's choice; the game facts
are the fields and the readers above. Chromatic aberration and film grain
must take effect from the next rendered frame after the key changes, by
overriding the value the reader uses (for example the field at the time
0x5F1E00 or the grain reader runs), never through `SettingsSetOption`
(whether a value set there reaches the player's saved settings is open,
02 §11 item 5, and these keys must never change them). The lens flare draw site is a `call` that can
be redirected to a function with the ABI of 1.8 that returns without
drawing while stereo runs and the key is false, and otherwise calls the
game's draw.

Sites checked before writing: 0x609AD0 with the bytes of 1.8 (lens flares);
0x5F1E00 prologue `83 ec 10 f3 0f 10 05 14` and the call at 0x6093E9 `e8 12
8a fe ff` (chromatic aberration), if the implementation patches there; for
film grain, if the implementation patches there, the call at 0x6005DF with
the bytes of 1.6 and the reader's prologue and intensity read (1.6).

A value written into a game field for the reader's call must not outlive
that call: the game's value is back in the field when the call returns,
except that a value the game's settings handler stored during the call is
kept [proposal: the handler may run on another thread, and its store must
never be lost].

A toggle's site is written at attach only when its key is false then
[proposal: with every key at its v1 value no patch is installed, 20 §1.1];
so a key that is true at attach and turned false later applies from the
next launch, while a key false at attach applies at once both ways.

Acceptance: in a lit area with flares (lights with flares are common on the
Torrens and in Solomons), with `LensFlares` false no flares in either eye;
with `ChromaticAberration` false no colour fringes at the image edges while
the game's own setting is On; with `FilmGrain` false no grain while the
game's intensity is 1.0; flat play (runtime None, then OpenVR with the
headset off is still stereo, so use None) shows the player's settings.

## 9. D3D11 state cost (`Graphics.StateCache`)

### 9.1 Measure first

The game sets render state through thin cdecl wrappers on its renderer (the
viewport setter 0x621350, cdecl `(context, rectangle)`, which turns an
integer rectangle and a depth range into one viewport and passes it to
`ID3D11DeviceContext::RSSetViewports` through the D3D11 context held at
the context's `+0x08`; the constant writers 0x621C20 and 0x621C90, cdecl,
which copy float4 constants into the renderer's copy of one constant-buffer
slot and mark the slot for upload; the clear 0x621770, 07 §2.3), many of them each draw. Whether
redundant calls (setting the state that is already set) cost measurable
render-thread time in stereo is **not known**. Required before any
implementation: a profile of the render thread in a heavy scene (Medical or
the Torrens at pixel scale 1.0) that counts `ID3D11DeviceContext` calls per
frame by method and how many of them set state equal to the current state,
and the time the render thread spends in them. The feature is implemented
only if redundant calls cost at least 0.5 ms of render-thread time per
frame [proposal: below that, the risk of a stale-state bug outweighs the
gain].

### 9.2 Required behaviour if implemented

- At the device-context level in the proxy (the game's immediate context,
  wrapped as 01 wraps the device), not by patching the game's wrappers:
  one place, no game addresses, and it cannot break when a game wrapper
  differs.
- A call that sets exactly the state the context already has (same
  objects, same values) may be skipped; every other call passes through in
  order. Draw, dispatch, clear, copy, map and query calls are never
  skipped or reordered.
- The cached state is invalidated whenever anything other than the game's
  own calls through the wrapped context may change it: `ClearState`, a
  display reset, the mod's own rendering (mirror, virtual screen, 03 §8)
  and the VR runtime's submit (which may use the same context, 03 §6), and
  any call the cache does not model.
- Applies only to the game's device's immediate context; deferred contexts
  and other devices pass through.
- With the key off: every call passes through unchanged (v1).

Acceptance: the same frame captured with the key off and on renders
identically (pixel compare of the stereo target in a still scene); the
render thread time drops by the measured amount; no flicker or missing
objects over a full level.

## 10. Settings

Keys are inside `Settings.Graphics` of `mothervr.json` (06 §1 style; a
missing key reads as its default; unknown keys are kept, 06 §3):

| Key | JSON type | Default | Values / range | Sanitise | Takes effect |
|---|---|---|---|---|---|
| `Graphics.PerEyeTemporalAA` | bool | false | | non-bool: default | at once (next frame; the first frame runs spatially, §2.2 item 6) |
| `Graphics.TemporalAA` | string | `"Off"` | `"Off"`, `"On"` | other: `"Off"` | at once |
| `Graphics.SteadyParticleLighting` | bool | false | | non-bool: default | at once |
| `Graphics.PauseWorldBackdrop` | bool | false | | non-bool: default | next pause menu Open |
| `Graphics.EngineSettingsOverride` | bool | false | | non-bool: default | next launch |
| `Graphics.ShadowMap4096` | bool | false | | non-bool: default | next launch (with the override) |
| `Graphics.SharpReflections` | bool | false | | non-bool: default | next launch (with the override) |
| `Graphics.StreamingTextureHeapMB` | integer | 0 | 0 (game's), or 300 to 1024 | outside: nearest bound; negative: 0 | next launch (with the override) |
| `Graphics.StreamingModelHeapMB` | integer | 0 | 0 (game's), or 128 to 512 | outside: nearest bound; negative: 0 | next launch (with the override) |
| `Graphics.FoveatedRendering` | string | `"Off"` | `"Off"`, `"Low"`, `"High"` | other: `"Off"` | at once where supported |
| `Graphics.LensFlares` | bool | true | | non-bool: default | at once; a key true at attach and turned false later, next launch (§8) |
| `Graphics.ChromaticAberration` | bool | true | | non-bool: default | at once; a key true at attach and turned false later, next launch (§8) |
| `Graphics.FilmGrain` | bool | true | | non-bool: default | at once; a key true at attach and turned false later, next launch (§8) |
| `Graphics.StateCache` | bool | false | | non-bool: default | next launch [proposal: the cache is set up with the context wrapper] |

All defaults are v1 values (20 §1.1). Each key applies on its own; there
is no master switch (20 §1.3). A key that applies at the next launch
follows its own value at that launch.

**Options page.** The keys have items on the `Graphics` page, reached
from the `MotherVR` page (22 §12.1, which also gives the rules every v2 page
follows: look, value display, description lines, greying and hidden
items). An item is shown only once its feature is implemented and the
user has confirmed it [proposal: several features here are conditional on
a measurement, §2.4, §3.1, §7.1, §9.1]. The user confirmed items 5 to 9
and 11 to 13 (user decision of 2026-10-18, for the engine settings additions
of §6 and the 2D effects of §8, which are implemented), so the page shows
those eight items, in the table's
order; items 1 to 4, 10 and 14 are not shown. Each item takes effect as
the table above says: items 5 to 9 at the next launch (their description
lines start with `Requires restart!`), items 11 to 13 at once, with the
exception the table gives for a key that was true at attach. The labels, values and description
lines are exact; they are this project's texts, in the style of the v1
page players know (00 §2.6) [proposal for each text: it names the result
the player gets, in the fewest words that fit the stock description
length, 22 §12.1]. Game names in them are the game's English menu texts
(`Anti-aliasing`, `Shadow Map Resolution`, `Planar Reflections`,
`Chromatic Aberration`, `Film Grain Intensity` in
`DATA\TEXT\ENGLISH\UI.TXT`) and the anti-aliasing option name `SMAA T2x`
of `DATA\ENGINE_SETTINGS.XML` [sure for the texts; likely that the Video
page shows the XML option name as the value].

`Graphics`, in order:

| # | Label | Kind | Values shown | Key | Description line | Greyed while |
|---|---|---|---|---|---|---|
| 1 | `Per-Eye Temporal AA` | toggle | on / off | `Graphics.PerEyeTemporalAA` | `With Anti-aliasing at SMAA T2x, smooths edges over two frames in each eye` | |
| 2 | `Temporal AA` | list | `Off`, `On` | `Graphics.TemporalAA` | `Stronger temporal anti-aliasing, for shimmer that SMAA T2x leaves` | |
| 3 | `Steady Smoke and Fog` | toggle | on / off | `Graphics.SteadyParticleLighting` | `Lights smoke, fog and particles the same in both eyes and every frame` | |
| 4 | `World Behind Pause Menu` | toggle | on / off | `Graphics.PauseWorldBackdrop` | `Shows the paused world, darkened, around the pause menu` | |
| 5 | `Engine Settings Override` | toggle | on / off | `Graphics.EngineSettingsOverride` | `Requires restart! Adds the VR options below to the game's graphics settings` | |
| 6 | `4096 Shadow Maps` | toggle | on / off | `Graphics.ShadowMap4096` | `Requires restart! Adds 4096 to Shadow Map Resolution in the Video options` | `Engine Settings Override` off |
| 7 | `Sharp Reflections` | toggle | on / off | `Graphics.SharpReflections` | `Requires restart! Adds a Planar Reflections option without blur` | `Engine Settings Override` off |
| 8 | `Texture Streaming Memory` | list | `Game`, `300 MB`, `400 MB`, ... `1000 MB` (steps of 100), `1024 MB` | `Graphics.StreamingTextureHeapMB` (`Game` is 0) | `Requires restart! Memory for streaming textures. Game: the game's own size` | `Engine Settings Override` off |
| 9 | `Model Streaming Memory` | list | `Game`, `128 MB`, `192 MB`, ... `512 MB` (steps of 64) | `Graphics.StreamingModelHeapMB` (`Game` is 0) | `Requires restart! Memory for streaming models. Game: the game's own size` | `Engine Settings Override` off |
| 10 | `Foveated Rendering` | list | `Off`, `Low`, `High` | `Graphics.FoveatedRendering` | `Renders the edges of the view at lower detail` | shown greyed with `Not supported by this graphics card` where §7.1's support is missing |
| 11 | `Lens Flares in VR` | toggle | on / off | `Graphics.LensFlares` | `Off: no lens flares in the headset` | |
| 12 | `Chromatic Aberration in VR` | toggle | on / off | `Graphics.ChromaticAberration` | `Off: no color fringes in the headset, whatever the Video setting` | |
| 13 | `Film Grain in VR` | toggle | on / off | `Graphics.FilmGrain` | `Off: no film grain in the headset, whatever the Video setting` | |
| 14 | `Graphics State Cache` | toggle | on / off | `Graphics.StateCache` | `Requires restart! Skips repeated graphics state changes` | |

Items 11 to 13 carry `in VR` because the game's Video page has its own
`Chromatic Aberration` and `Film Grain Intensity` items, which these do
not change (§8), so the two are never confused. A stored heap size between
the list's steps (for example 350) is shown at the nearest step (22
§12.1) and kept.

## 11. Common requirements

### 11.1 Edge cases for every feature

| Situation | Rule |
|---|---|
| runtime None | nothing of this document runs or is installed (20 §1.2) |
| runtime fails to start | no stereo; every feature inactive except the engine settings override, which still applies if on (the exception of 20 §1.2) [proposal: it is a game-settings feature, useful flat too, and is decided at attach before the runtime's outcome is known] |
| OpenVR and OpenXR | every feature works on both; §5 submits through each runtime's own API (03 §6, 09 §5) |
| screen frames (menus, loading, cutscenes on the virtual screen) | §2 and §3 reset history on the next head-tracked frame; §4, §8 act on whatever the game renders; §5 as stated |
| display reset | features re-acquire resources after slot 3 (03 §4.4); §2 and §3 reset history |
| headset off, controllers lost | no effect on rendering features |
| 2020 build | all off; logged once |
| a site check fails | that feature alone stays off and is logged (12 §4) |

### 11.2 Threads

All per-frame work of this document runs on the render thread (02 §9.1):
the AA path, the velocity passes, the light submission, the lens flare
draw, the backdrop copy and submit, the state cache. The stereo state is
read with atomic loads. Settings are read with atomic loads of values the
game or UI thread writes (06 §3.4). The engine settings override runs once
on the main thread at start-up. No allocation, file I/O, locking or logging
per frame on the render thread (12 §6.1); resources are created with the
stereo target (slot 3) or at install.

### 11.3 Logging (11 owns wording)

Each event must convey:

- per feature, at install: installed, or not and which check failed, or
  the build's addresses unknown;
- §2: the first frame with per-eye temporal SMAA, and each history reset
  with its cause, bounded (11 §3);
- §4: when the eye-1 submission is first skipped; with `DebugOptions`, the
  light counts submitted per eye every 900 frames;
- §5: backdrop shown, faded, dropped (and why);
- §6: each addition made or skipped, with the setting name; that the
  override is on; any read or parse failure;
- §7: whether foveated rendering is supported and on, and why not;
- §8: each toggle's state when stereo starts and when it changes;
- §9: the profile counts when profiling is on (a `DebugOptions` diagnostic);
  that the cache is on;
- §14: as §14.7.

## 12. Open questions

1. **Velocity pass viewport for eye 1.** Does the velocity pass's pass
   set-up (0x595170 called with type 0xB at 0x5B17D7) keep the eye's
   viewport origin, or rebuild
   it from `{0, 0, w, h}` (the same question as 07 §8 item 1)? Look for: a
   capture of the velocity target after both eyes with §2 on.
2. **Velocity shader units.** Does the game's velocity shader compute
   screen positions over the whole target or the bound viewport? Look for:
   the velocity effect's pixel shader in the post-processing shader pack
   (`DATA\SHADERS\UBERSHADER\POSTPROCESSINGSHADERS_DX11*.PAK`) and its use
   of any constant that carries the target's or the viewport's size or
   offset.
3. **Screen-like surfaces under TAA (§3).** Which materials draw terminal
   and tracker screens, and can they be recognised by shader, by render
   pass or by the interaction state (11 §5.3)?
4. **Particle flicker cause (§4).** Confirm in game (§4.4 steps 1 and 2)
   that the flicker is stereo-only and that skipping the eye-1 submission
   removes it. If not, trace 0x592E10's results to the particle shaders'
   `alphalightIntensitiesReg` texture and the `ALPHALIGHT_TASK` job
   (0x5D52D0).
5. **Pause black (§5).** Is the game's own frame behind the pause menu
   black, and by what? Look for: with `DebugOptions`, log the fade pair
   `+0x1934`/`+0x1938`, the fade mode `+0x1B44` and the global block values
   (0x1250A88) on pause Open and Close; capture the stereo target while
   paused. If the game draws the world while paused, §5 could show the
   game's own paused frame instead of a stored copy.
6. **Planar reflections value 6 (§6.2).** Partly answered (spec r6): in
   the reflection manager's code the quality `+0x60` is compared only with
   0 (the render, 07 §2.2, and the target creation), 2 and 3 (the mip
   count 0x5995A0, the mip build 0x59C8B0, the blur targets at 0x59A165).
   Still open: whether the settings handler or any other code reads
   setting 0x10's value directly and treats 6 differently. Look for: in
   game, the reflection with `Alternative` chosen.
7. **Heap settings in effect.** When are the heap sizes read (0x5F6E80,
   0x5F6D30) and does the saved settings file override the precedence
   choice for settings without a menu control? Look for: the callers of the
   table at 0xFE7088 and how the saved file is applied (0x5AFC60).
8. **Saved value naming a removed option (§6.3).** What the game does when
   its saved settings name an option the XML no longer has.
9. **Opaque scene passes for foveation (§7.1).** Which pass types given
   to the pass set-up 0x595170 belong to the main view's opaque scene
   passes (the main render 0x5B3F10 gives type 4 at 0x5B3F98; the function
   at 0x5B3A80 gives types 2 and 3; its relation to the main render is to
   be traced), and a point where the shading-rate state can be set and
   cleared around them.
10. **Letterboxing.** Do in-engine cutscenes draw bars, and how (a camera
    aspect, a UI movie, a post-processing mask)? Look for: a cutscene in
    game, captured; cinematic camera entities (`CINEMATIC_*` names in
    `AI.exe`).
11. **Vignettes.** Where the vignette and the hiding vignette are drawn
    (post-processing material parameters, or a UI movie), and whether they
    are centred per eye.
12. **Jitter scale.** Confirm in a capture whether each eye's jitter is a
    quarter pixel horizontally or half that (1.3).
13. **2020 build.** Every address of this document.

## 13. Changes to v1 documents

To be applied in the v1 revision that collects v2 changes (20 §10 item 11):

- `08-render-fixes.md` §3.2: "SMAA T2x runs as T1x in stereo" holds unless
  `Graphics.PerEyeTemporalAA` is on (24 §2); §7 item "Per-eye T2x" points to
  24. §3.1: the jitter is confirmed cleared by a spatial run (24 §1.3).
- `08-render-fixes.md` §7, `+0x1B2F` item: unchanged; add the velocity
  slot mismatch of 24 §1.4 as a known game fact.
- `07-reflections.md` §1: the reflection quality values (24 §1.9) and the
  optional sharp mode (24 §6.2).
- `02-game-stereo-path.md` §6.5: the setting id table of 24 §1.2 (ids 0x01
  to 0x18 named).
- `03-hmd-and-frames.md` §7: the pause backdrop exception (24 §5); §4.4
  slot 3 and slot 4: v2 resources created and released there (24 §5.5,
  §2).
- `06-options-and-config.md` §1: the `Graphics` section exists (24 §10);
  §5: the `Graphics` page (24 §10, under 22 §12.1).
- `11-diagnostics.md` §2: the events of 24 §11.3.
- `12-threads-and-robustness.md` §4.2: the features of this document are
  optional fixes.
- `13-distribution.md`: any file the engine settings override writes
  (24 §6.1).
- `01-loading.md` §4.7: the v2 addresses of this document, 2015 only.
- `08-render-fixes.md` §1: a second shader fix, for vertex shaders, is
  24 §14; `11-diagnostics.md` §2.2: its events (24 §14.7) and
  `CreateVertexShader` among the intercepted methods.

## 14. Upright sprites in stereo (a fix, no key)

### 14.1 Problem

A tester reported (OpenXR, Valve Index): smoke and steam turn with the
roll of the head. Tilting the head sideways turns every smoke sprite by
the same angle, so the smoke looks fixed to the face rather than standing
in the room. Flat play hardly shows it, because the game's own camera
rarely rolls; a headset rolls whenever the player tilts the head.

This is a defect of the game in stereo, not a matter of taste, so it is
fixed whenever a headset runtime is chosen, like the dither fix of 08 §1,
and has no key [proposal: project decision with the reason above]. With
no roll the result is the game's own (§14.3), so a player who keeps the
head upright sees no change.

### 14.2 Game facts: how the game turns a sprite to the camera [sure]

Found by the spec side in the levels' compiled shader packs
(`RENDERABLE\LEVEL_SHADERS_DX11_BIN.PAK`; shader model 5.0 vertex
shaders) and in the 2015 `AI.exe`:

- **The sprite vertex shaders.** Many vertex shaders draw a camera-facing
  sprite the same way (counted by the spec side: 396 of the 925 vertex
  shaders of the Torrens level's pack, 469 of the 1106 of the `TECH_HUB`
  level's; none in the global packs under `DATA\SHADERS`). They compute
  the clip position of the sprite's centre, `c = p · ViewProj` (vertex
  constant buffer 0, `cbDefaultXSC`, registers 0 to 3; in some shaders
  `SecondaryViewProj`, registers 12 to 15), and then add the corner's
  offset in the eye's view plane, scaled by the projection:
  `c.x += a · P[0][0]`, `c.y += b · P[1][1]`, where `P` is `ProjMatrix`
  (vertex constant buffer 1, `cbDefaultVSC`, register 0 component x and
  register 1 component y) and `(a, b)` is the corner's offset in view
  units after the sprite's own rotation (which comes from the particle's
  data and the material). Lit sprites then also compute the particle
  lighting lookup of `AlphaLight_OffsetScale` from `c` (§4).
- **Their form.** Each of these shaders holds exactly two instructions
  that read those projection entries: a `mad` whose destination is
  component x of a temporary, whose first source is one component of a
  temporary (`a`, possibly negated), whose second source is buffer 1
  register 0 component x and whose third source is a temporary component
  last written by a `dp4` with buffer 0 register 0 (or 12); and a `mad`
  of the same shape for component y with `b`, buffer 1 register 1
  component y and buffer 0 register 1 (or 13). Only `dp4` instructions lie
  between the two; the x one comes first in most shaders, the y one first
  in some. The spec side checked all 865 sites of the two packs against
  this form.
- **What that means.** The sprite's axes are the drawn eye's view x and y
  axes. When the head rolls, the eye's view rolls, and so does every
  sprite.
- **The view is available to these shaders.** Buffer 0 registers 4 to 7
  (`ViewMatrix`) hold the transposed view of the POV being drawn: the
  camera-constant setter 0x57A8A0 (§1.3) stores its view argument at
  `CameraConstants` `+0x280`, and the camera-constant upload 0x57A100
  (thiscall `(CameraConstants, renderer, int)`, `ret 8`) uploads that
  matrix's columns 0 to 3 as buffer 0 registers 4 to 7 through the vertex
  constant writer 0x621C20 (cdecl `(renderer, buffer, first register,
  data, register count)`; it copies the data into the renderer's copy of
  that buffer and marks the buffer for upload). No other code writes
  `+0x280`. Every POV's begin-render (0x5B11D0) calls 0x57A8A0 with the
  POV's own view (`+0x00`), and the eye renders run it after `SetEye`
  (07 §2.4), so in each eye's pass `ViewMatrix` is that eye's view.
  The sprite shaders declare buffer 0 with at least 23 registers. Some of
  the game's own particle shaders read `ViewMatrix` already
  (camera-relative emitters).
- **Up in the view.** With the game's view convention (05 §1: row
  vectors, columns 0 and 1 of the view are the camera's right and up axes
  in world space, the world's up is +Y), the world's up direction has the
  view coordinates `ux = V[1][0]` and `uy = V[1][1]`, which are component y
  of buffer 0 register 4 and component y of register 5.
- **Other sprite kinds are already upright.** Shaders that build a sprite
  in world space from its velocity and the direction to the camera
  position (buffer 0 register 22), or around an emitter axis, do not use
  the view's roll. A few shaders (the spec side found six in the Torrens
  pack) build an axis-locked sprite from the view's right axis in world
  space (register 4 alone) [likely]; they are not changed (§14.5).

The spec side also confirmed an observation given to it: 0x621C20 is the
renderer's writer of vertex constants, through which the camera matrices
reach buffers 0 and 1.

### 14.3 Required result

In every vertex shader of the form of §14.2 that the game creates while a
headset runtime is chosen, the corner offset `(a, b)` that the two
instructions read is replaced by `(a', b')`, the same offset turned in
the view plane so that the sprite's up axis is the world's up as the eye
sees it. The arithmetic below was derived by the spec side from this
requirement and the view convention of §14.2:

1. `u = (ux, uy)` from `ViewMatrix` (§14.2), `len = |u|`.
2. Near the vertical the direction of `u` is undefined, so it is eased
   back to the game's own orientation: `w = u + (0, max(0, L - len))`
   with `L = 0.1` (project decision: within about 6 degrees of looking
   straight up or down the sprites turn smoothly back to the view's own
   orientation instead of spinning with the smallest head turn; at
   `len >= L`, `w = u` exactly).
3. If `|w|^2 < 1e-12` (only reachable with the view upside down near the
   vertical) `w = (0, 1)`.
4. `n = w / |w|`, the sprite's up axis in view x and y; its right axis is
   `(n.y, -n.x)`.
5. `a' = a · n.y + b · n.x`, `b' = b · n.y - a · n.x`.

Properties the result must have:

- **No roll, no change.** With no roll `ux = 0` and `n = (0, 1)`, so
  `(a', b') = (a, b)`: flat play and an upright head give the game's own
  result (up to float rounding).
- **Still facing the eye, still centred.** Only the in-plane direction of
  the corner offset changes: the sprite stays in the eye's view plane, its
  size and its centre `c` are unchanged, and each eye uses its own view,
  so the two eyes agree.
- **Upright in the world.** Away from the vertical, the sprite's right
  axis has no world-up component and its up axis lies in the plane of the
  view direction and the world's up.
- **Defined everywhere.** Looking straight up or down gives the game's own
  orientation; nothing is NaN or infinite.
- **Only those values change.** The values the two instructions read as
  `a` and `b` are the only change to what the rest of the shader sees; the
  shader's inputs, outputs, signatures and other values stay as they are.
- **Container rules of 08 §1.** A shader whose container this fix does not
  understand (checksum, sizes, one program chunk), or whose instructions
  differ in any way from §14.2, is created unchanged. If the device
  refuses a changed shader the original is created.

Runtime None: nothing (20 §1.2). The shader is changed when it is created,
so the change applies to flat frames of a session with a headset runtime
too; with no roll that is the game's own result.

### 14.4 Edge cases

| Situation | Rule |
|---|---|
| looking straight up or down | the game's own orientation, eased in within `L` (§14.3 item 2) |
| planar reflections (07) | the reflection view's up may be mirrored, so sprites in a floor reflection may appear as the mirror image of upright sprites [guess]; acceptable, as a real reflection shows them so |
| the light passes of the particle lighting (§1.7) | not sprite shaders of §14.2; unchanged |
| the particle lighting lookup (08 §8) | unchanged: the lit sprite shaders form the lookup from the final clip position, after the corner offset, so each pixel still reads the lighting at its own position in the eye's view [sure from the shaders] |
| a shader changed by another tool | its checksum or form no longer matches; created unchanged |
| 2020 build | the form is a property of the shader packs, not of `AI.exe` addresses; it applies if the packs match [guess] |

### 14.5 Not covered

- The few axis-locked sprites built from the view's right axis in world
  space (§14.2). Their vertical axis is the world's already; only their
  width axis tilts with roll. Left as they are (project decision: few
  shaders, a smaller effect, a different form).
- Sprites drawn by shaders outside the level packs: none of the global
  packs has the form of §14.2.

### 14.6 Cost and threads

About 16 instructions more per sprite vertex. The shader is examined when
the game creates it, on whichever thread creates it: a cheap byte search
for the second-source operand of the x instruction (buffer 1 register 0
component x) skips most shaders before any checksum is computed (08 §1's
concern for shader-creation cost). Nothing runs per frame.

### 14.7 Logging

- At install: that vertex shader creation is intercepted for this fix, or
  that it could not be (11 §2.2's interception failure, method
  `CreateVertexShader`).
- When the fix changed a shader: at the first one, then at the bounded
  rate of 08 §1's event: how many vertex shaders the game created so far,
  how many were changed, how many changed ones the device refused.
  Template:
  `Upright sprites: {Patched} of {Created} vertex shaders changed so far; {Rejected} refused by the device`.
- A refused shader: a few at most per process, as for 08 §1.

### 14.8 Acceptance

1. Near smoke or steam (Sevastopol vents, the Torrens), tilt the head to
   either side: the sprites stay upright in the room, as they do with an
   upright head; nod and turn: unchanged from before.
2. Look straight down into steam and up into smoke: no spinning or
   flicker; the sprites turn smoothly back to the game's own orientation
   near the vertical.
3. Both eyes alike (close each in turn); the lit parts of the smoke stay
   where the particle lighting puts them.
4. Runtime None: unchanged.
5. The log shows the first `Upright sprites` event after a level loads,
   with a nonzero changed count and none refused.
