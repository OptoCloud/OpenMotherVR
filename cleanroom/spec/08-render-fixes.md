<!-- Provenance: static analysis of AI.exe (2015 Steam build; every site and byte below re-read from the binary for r2; for r4 the spec side re-read the cull function 0x61FD20 and its distance test, the register 39 upload and the head-in-wall fade 0x346DE0) and of the game's pixel shaders (DXBC from the level and global shader packs; for r4 the spec side counted the pixel shaders holding the dither's immediates in three level packs); the public DXBC container layout and checksum; game facts located with the help of an analysis of another mod and verified in the 2015 `AI.exe` and shader packs by the spec side (the dither sequence and how it fails, register 39, the cull function's tests); that analysis served only as a lead to which problems exist, and the required results of sections 1, 2 and 4 are stated as this project's own properties; in-game observation by the spec side in a headset (section 1 and section 5 confirmed working); measurements in game by the spec side (draw-submission cost with the distance, occlusion and view-volume tests all bypassed; AA cost; shader-creation cost over the first 3500 shaders of a session); user requirements (anti-aliasing choice with an automatic mode, no MD5 of every shader, shaders changed by other tools pass through); project decisions, each with its reason inline (the head-in-wall delay D, chosen afresh for r4; the bound on the dither's input). r4: clean-room review round 2 applied (section 1 as properties, section 2.1 restated from the spec side's own reading, the head-in-wall delay and its "clear" test re-derived from the game, measurement conditions stated). Section numbers are unchanged from r3. Revision: r4 -->

# 08: Rendering fixes

Five problems in how the game renders in stereo, beyond the reflections of
07: lighting that differs between the eyes, objects popping in and out,
anti-aliasing switched off, an abrupt head-in-wall blackout, and the size of
the desktop mirror. For each: what the game does, why it fails in VR, and
what the mod must do.

Addresses are RVAs of the 2015 build (image base 0x400000), read from the
2015 `AI.exe`. The 2020 addresses of sections 2 to 4 are not known (2020: ?);
those fixes stay off on that build and the log says so once. Section 1 needs
no game address and applies to both builds.

Confidence: **[sure]** the game code or shader shows it directly, or it was
observed in game; **[likely]** strong inference; **[guess]** must be
confirmed in game.

| § | Problem | Status |
|---|---|---|
| 1 | Lighting differs between the eyes | required; **confirmed in game** |
| 2 | Object pop-in | required; specified; **not yet confirmed in game** |
| 3 | Anti-aliasing off in stereo | required; specified; **not yet confirmed in game** |
| 4 | Head-in-wall blackout too abrupt | required; specified; **not yet confirmed in game** |
| 5 | Desktop mirror smaller than the window | required; **confirmed in game** |
| (07) | Planar reflections per eye | required; specified in 07; **not yet confirmed in game** (no planar reflection surface in the tested scene) |

## 0. Applying the fixes

- None of these fixes is applied with runtime None (01 §2.3).
- Sections 2 to 4 are independent of each other and of the patch set of 01,
  02 and 05: each checks its own sites (the bytes given in its section)
  before writing. If one of its checks fails, that fix alone stays off, the
  log names the fix and says it was not applied, and everything else works.
  None of them is half-installed: all of a fix's sites are checked before the
  first of them is written.
- Sections 2 to 4 act only while stereo runs (stereo device present and in
  state 2, 05 §1); otherwise the game's own code runs unchanged.
- "Stereo running" is read on the game's job threads (section 2) and on the
  render thread (section 3). The device pointer and state must be read with
  single aligned (atomic) loads; a value one frame stale is acceptable.
- No allocation, file I/O or logging per frame or per call; log once per
  change (for example the first stereo frame with each AA mode). This
  document says which events are logged; the wording and format are those of
  `11-diagnostics.md`.

## 1. Position-based dither in the game's pixel shaders

### 1.1 What the shaders do [sure]

Several hundred of the game's pixel shaders add a small anti-banding
dither to a lighting or reflectance factor (counted by the spec side: 353
of the 887 distinct pixel shaders in the Torrens level's
`LEVEL_SHADERS_DX11_BIN.PAK`, 443 of 1068 in the `TECH_HUB` level's and 434
of 1046 in the `SCI_HOSPITALUPPER` level's contain both immediates of
instructions 1 and 2 below; some global shaders contain them too). The
dither is a hash of the pixel's **absolute** position. In the compiled
shaders (shader model 5.0) it is always this sequence, where `vP` is the
`SV_Position` input, `vT` another input (a texture coordinate) and `rA`, `rB`,
`rC`, `rD` temporaries:

| # | Instruction (as the game's shaders contain it) | Value |
|---|---|---|
| 1 | `mul rA.xy, vP.xyxx, l(0.001, 0.001, 0, 0)` | `a = 0.001·p` |
| 2 | `mad rB.zw, vP.xxxy, l(0, 0, 0.001, 0.001), -vT.xxxx` | `b = 0.001·p - t` |
| 3 | `dp2 rC.w, rB.zwzz, rA.xyxx` | `d = b·a` |
| 4 | `exp rC.w, rC.w` | `2^d` |
| 5 | `mul` by 10000, `frc`, `mul` by 1400, `frc` | hash `h` in `[0, 1)` |
| 6 | `mad rD.w, rC.w, l(0.0025), rD.w` | factor += `0.0025·h` |
| 7 | `min rD.w, rD.w, l(1.0)` | factor clamped to 1 |

Register numbers, write masks and input numbers vary between shaders;
instructions 1 and 2 are always adjacent and in this order. Instruction 1's
immediate is exactly `(0.001, 0.001, 0, 0)` and instruction 2's exactly
`(0, 0, 0.001, 0.001)`, with 0.001 having the bit pattern 0x3A83126F; the
second source of instruction 2 is an input with a negate modifier. The
factor then scales an output colour (emissive or environment light).

### 1.2 Why it fails on a side-by-side target [likely]

The hash input is `d = (0.001·p - t)·(0.001·p)`, with `p` in pixels of
the bound target, so for texture coordinates near 0 it is about the square
of the pixel's distance from the target's top left corner, in thousands of
pixels. The spec side's evaluation of the sequence:

- A float of 2^24 or more has no fractional bits, so `frc` of
  `2^d · 10000` is 0 once `d` exceeds about 10.7, that is beyond about 3300
  pixels from the corner; on the way there the hash loses precision and
  shows coarse patterns. On a flat 1920x1080 target no pixel is that far,
  so the game is correct there. On a side-by-side target of 5376x2688
  (Index at pixel scale 1.0) the right eye's pixels start 2688 pixels from
  the left edge, so the dither is gone from most of the right eye and only
  from the lower right corner of the left eye; a larger render size spreads
  this over both eyes.
- A large or negative tiled coordinate `t` adds `-t·0.001·p`, which can
  push `d` past 128. `2^d` is then infinite, `frc` of infinity is NaN, and
  Direct3D's `min` returns its non-NaN operand, so the factor becomes 1
  and the surface is lit fully or reflects fully.

The result is surfaces lit differently in the two eyes, first in the right
eye and worse at higher resolution (an earlier mod showed this). Players
described it as reflections moving against the head; it is lighting, not
the planar reflection of 07.

### 1.3 What the game provides [sure]

Pixel constant buffer 2 (`cbDefaultPSC`) register 39 holds
`(W, H, 1/W, 1/H)` of the main view's render target. It is written in the
post-processing setup (0x6007B0..0x600811: a size query 0x627B90, then the
constant upload 0x621C90 with buffer 2, register 0x27, count 1). The
shaders' reflection data names it `SourceResolution`. It differs from
register 60 (`ScreenResolution`, 07 §2.7).

### 1.4 Required result

At each occurrence of the pair 1-2, the values instruction 3 reads (the two
operands of the dot product that gives `d`) are replaced so that the hash
works in both eyes at any render size. Any arithmetic is acceptable that
has these properties:

- **Independent of the target.** The input no longer grows with the
  pixel's absolute position: it is computed from the position relative to
  the bound target, `p · (1/W, 1/H)` (register 39's `zw` components, §1.3),
  and the texture coordinate `t` of instruction 2, so it does not depend
  on the target's size or the pixel scale.
- **Bounded.** For every pixel of the target and every texture coordinate
  with `|t|` up to 1000, `|d|` stays at or below 8 (project decision:
  `2^8 · 10000` is below 2^24, so `frc` keeps the hash's fractional bits
  and nothing overflows; texture coordinates in the game's materials stay
  far below 1000 in magnitude [likely]). Never NaN or infinite.
- **Still a dither.** `d` still varies between neighbouring pixels for the
  texture coordinates the materials use, so the hash still breaks up
  banding, and its amplitude is unchanged: instructions 4 to 7 are kept,
  and at most 0.0025 is added in either eye.
- **Only those values change.** The values instruction 3 reads are the
  only change to what the rest of the shader sees (see "No live value
  overwritten" below).

A result with these properties generally needs more instructions than the
game's pair (the target size is not an immediate), so the patched program
may be longer than the original. The rewrite is subject to these required
results:

- **Exact match only.** A site is patched only if instructions 1 and 2 have
  exactly the form of §1.1: the operand kinds, the `vP` components read, the
  two immediates, and a negated input as the second source of instruction
  2. Any other form is left unchanged. How the match is decided is the
  implementation's choice.
- **No live value overwritten.** The patched program must not change any
  value the rest of the shader reads, other than the two results above.
  Whether to reuse registers or declare an extra temporary is the
  implementation's choice. A site that cannot be patched under this rule is
  left unchanged.
- **Register 39 declared.** Register 39 must be inside the shader's declared
  size of constant buffer 2 (at least 40 registers); a smaller declaration
  is raised to 40. A shader that does not declare constant buffer 2 at all
  is left unchanged: the game is only known to bind buffer 2 for shaders
  that declare it [likely].
- **Containers understood or untouched.** A DXBC container the
  implementation does not fully understand, by the public DXBC layout, is
  left unchanged; so is one whose stored checksum (bytes 4..19, the standard
  DXBC checksum, an MD5 variant over bytes 20 to the end) does not verify,
  since it is not a container the patcher understands. A patched container
  must be valid: when the program grows, the program's length token, the
  chunk's size, the container's total size (bytes 24..27) and the offsets of
  every chunk after the program chunk are updated, and the checksum is
  recomputed over the result.
- **Fallback.** Any structure not as expected leaves the whole shader
  unchanged. If `CreatePixelShader` rejects a patched shader, the original
  bytecode is created instead.
- **Cheap pre-filter.** Before any hashing or parsing, a shader that does not
  contain two consecutive dwords 0x3A83126F anywhere in its bytes (the
  immediate of instruction 1) is passed through untouched. Most shaders are
  skipped this way. Measured in game by the spec side over the first 3500
  shaders created in one session: without the pre-filter, checksumming
  every shader costs about 20 µs each on the thread that creates shaders
  (the game's main thread), about 56 % on top of shader creation with a
  warm driver cache, or about 1.3 s per session; with it, almost none.

**Shaders not as expected.** Every shader that does not match the exact
pattern of §1.1 passes through unchanged. This includes shader packs that
other tools changed: the game still runs, and the log reports how many
shaders were patched. Coexisting with other graphics mods is not required
beyond that.

**Where.** The check runs at `ID3D11Device::CreatePixelShader` of the game's
device (the device the game creates through the proxied D3D11 creation
export, 01). Shaders of other devices in the process (a VR runtime's or an
overlay's) and the mod's own shaders must reach the driver unchanged. If the
hook is installed on the device class (a shared vtable), the pattern test is
what keeps other devices' shaders unchanged; the mod's own shaders must not
contain the pattern.

**Logging** (wording and format in `11-diagnostics.md`). Log when the hook
is installed. Log the running counts of shaders created, patched and
rejected occasionally, never per shader. Log a rejected patched shader with
its HRESULT, for a few rejections only, so a driver that rejects every one
cannot flood the log.

This runs in flat play with runtime OpenVR or OpenXR too (it changes nothing
visible there beyond the dither pattern), but not with runtime None.

**Observed [sure]:** in the headset, surfaces that lit up in the right eye
only are lit the same in both eyes after the change.

## 2. Object pop-in

### 2.1 The game's per-object visibility test [sure]

One function builds every culled object list of the scene (0x61FD20,
cdecl `(cull context*, per-object records*, bounds[] with stride 0x40,
items[], count, out[])`, the caller pops 0x18); its only call is at
0x5E984B, and it runs on the game's job threads. Each object has an 8-byte
record: a 32-bit layer word, a 16-bit LOD value, a flags byte and a mask
byte. An object reaches `out[]` (room for 128 entries; a null follows the
last kept item when there is room) only if no row below drops it:

| Test | Applies | Drops the object when |
|---|---|---|
| mask | always | its mask byte shares no bit with context `+0x54` |
| layer | always | its layer word shares a bit with context `+0x58` |
| flags | always | flags bit 0, 3 or 4 is set. With none of those and bit 1 set, the object is kept without any test below |
| **distance/LOD** (0x619A50) | unless flags bit 2 is set | `|centre - camera|² · s² > radius²`, with the camera at context `+0x40`, `s` the object's LOD value times context `+0x50`, the centre at bounds `+0x0C`, `+0x1C`, `+0x2C` and the radius at `+0x3C`. cdecl `(const bounds*, const vec3* camera, float s)`, result in AL, true = keep |
| **frustum** (0x619030) | always | it is outside the view's frustum; `(bounds*, context*)` |
| **occlusion** (0x619860) | context `+0x5C` not null | it is hidden in the occlusion buffer at `+0x5C`; cdecl `(const bounds*, context*, buffer*)`, result in AL |
| **view volume** (0x619920) | context `+0x60` not null | it is outside that volume; cdecl, arguments bounds, context `+0x70`, context `+0x60`; result in AL |

Which tests exist depends on the view being culled [likely: from which views
fill the context fields; POV `+0x314` is the view's render-list group, the
second argument of the game's POV creation, and POV `+0x310` its occlusion
buffer]:

| Group | Name in the game | Views | `+0x5C` occlusion | `+0x60` view volume |
|---|---|---|---|---|
| 2 | `BUILD_MAIN_RENDER_LIST` | the main view | the software occlusion buffer, drawn from the single centre view | no |
| 1 | `BUILD_SHADOWS_RENDER_LISTS` | shadow-casting lights | no | yes |
| 0 | `BUILD_PRE_MAIN_RENDER_LISTS` | the reflection view | no | no |

So the occlusion test only ever runs for the main view.

Sites (2015; check every byte before writing):

| Site | 2015 | 2020 | 2015 bytes |
|---|---|---|---|
| distance test call | 0x61FD95 | ? | `e8 b6 9c ff ff` (call 0x619A50), preceded by `f3 0f 11 04 24 8d 46 40 50 53` (scale to `[esp]`, push camera position, push bounds) and followed by `83 c4 0c 84 c0` |
| occlusion test call | 0x61FDB9 | ? | `e8 a2 9a ff ff` (call 0x619860), preceded by `8b 46 5c 85 c0 74 0f 50 56 53` (load context `+0x5C`, skip when null, push it, the context and the bounds) and followed by `83 c4 0c 84 c0` |

### 2.2 Why it fails in VR [likely]

The distance scale `+0x50` belongs to the single centre view [likely that
it is derived from that view's field of view and resolution; its writer
was not read, §7], and the occlusion buffer is drawn from that centre
view. In a headset each eye has a much wider field of view and its own
position. Small props and decals vanish or appear at distances that do not
match what the player sees, and objects near the edges of each eye are
dropped by an occlusion test made for the centre view (worse on wide-FOV
headsets). Shadows of such objects pop as well, since shadow-caster lists go
through the same distance test (an earlier mod showed this pop-in).

### 2.3 Required behaviour

While stereo runs:

- the distance/LOD test (step 2) must report "keep" for every object, in
  every group (main view, shadows, reflection);
- the occlusion test (step 4, main view only) must report "keep";
- the frustum test (step 3) and the shadow lights' view-volume test stay the
  game's;
- otherwise (flat, or stereo not running) both tests are the game's.

The two calls above are redirected to functions with the same ABI (cdecl,
three arguments, result in AL) that report "keep" while stereo runs and
otherwise call the game's test with the same arguments and return its
result. They run on job threads (0.1 to 4 thousand calls per frame) and must
do nothing but the atomic "stereo running" read and the forward.

**Cost [sure; measured in game by the spec side with the distance,
occlusion and view-volume tests all bypassed, so an upper bound for this
fix].** About 1 ms of render-thread CPU per frame in the main
menu scene (draw submission 2.7 to 3.2 ms with the game's tests, 3.5 to 4.4
ms without), and 0 to 0.4 ms GPU; more is expected in levels with many
occluded rooms. Accepted.

**Log** once at install that the fix is in (and the two sites).

**Acceptance (in game, not yet done).** Walk through a level with many small
props, decals and shadows (for example the Medical wing): nothing appears or
vanishes while walking or turning the head, including at the outer edges of
each eye's view, compared with the same walk with the fix off. Frame time
stays within the headset's budget at pixel scale 1.0.

## 3. Anti-aliasing in the stereo output path

### 3.1 Where the game skips it [sure]

Graphics setting 0x09 is anti-aliasing. Its handler (0x5F6AC0, thiscall, one
argument, `ret 4`) stores the mode at post-processing object `+0x1B3C`: 1
FXAA, 2 SMAA T1x, 3 SMAA T2x, otherwise 0. The post-processing object is a
static object returned by 0x5F19C0 (cdecl, no arguments; bytes `b8` +
4-byte address + `c3`, `mov eax, imm32; ret`).

The final post chain (0x610040..0x6101FA) runs the post-processing pass
(0x600490) and then, after debug-view checks, tests whether stereo is
running (0x610144: a call to 0x01E150 on the object at 0x13583A0, true when
the stereo device state is 2):

- **flat:** the AA pass (0x600210, thiscall on the post-processing object, no
  arguments; prologue `55 8b ec 83 e4 f0`), which draws the bound final
  effect into an intermediate target and then runs FXAA or SMAA from it into
  the output; then the post-processing pass again;
- **stereo:** the stereo output (0x5FC560), which binds the stereo final
  effect, sets pixel constant buffer 2 register 36 to two post-processing
  values multiplied by the stereo object's scene visibility (`+0x64`, the
  head-in-wall blackout), and draws it straight into the output with the
  full-screen draw (0x5A0980, cdecl, no arguments) called at 0x6101DC (`e8 9f
  07 f9 ff`). **The AA pass is never called.**

Inside the AA pass, SMAA runs through a call at 0x600479 (`e8 42 a0 01 00`)
to the SMAA runner 0x61A4C0 (cdecl, three arguments: colour input, second
input, `const bool*` temporal; `83 c4 0c` follows). With the temporal flag
false it runs spatial SMAA and also zeroes the projection jitter that the
temporal mode applies to the next frame [likely]. The AA pass draws with its
own mesh instead of the full-screen triangle when the byte at
post-processing object `+0x1B2F` is set [likely].

For SMAA T2x the velocity pass (0x5B1720) runs on the eye-0/centre path
only; the main view's eye-1 preparation (0x5B5B70) skips it, and the game
keeps one previous-frame view-projection, not one per eye.

### 3.2 Required behaviour

A config setting `StereoAntiAliasing` (`06`) decides whether the game's AA
runs in stereo: **On**, **Off**, or **Auto** (default): on when the pixel
scale is at most 1.3, off above. Above 1.3 the eyes are already
supersampled by at least 1.69 times and SMAA adds about 0.6 ms GPU (measured
0.52 to 0.76 ms at 8064x4032) for little visible gain.

While stereo runs, the AA mode at `+0x1B3C` is 1, 2 or 3, and the setting
says AA runs:

- the stereo output's full-screen draw (the call at 0x6101DC) is replaced by
  the AA pass, called with the post-processing object as `this`; the stereo
  final effect, with the blackout already in register 36, then feeds FXAA or
  SMAA exactly as the flat final effect does. During that call the byte
  `+0x1B2F` must read 0 (so the pass uses the full-screen triangle, as the
  stereo path does) and is restored afterwards;
- **FXAA and SMAA T1x** run on the side-by-side image as they are. Only the
  pixel column on each side of the seam between the eyes can mix, at the
  outer edge of each eye's view; this is accepted;
- **SMAA T2x** runs as SMAA T1x: the SMAA runner (call at 0x600479) is given
  a temporal flag of false while stereo runs. Running it temporally would
  reproject the right eye with the left eye's motion and history, because the
  game keeps velocity and history for one eye only.

Otherwise (AA mode 0, setting off, or stereo not running) the game's own code
runs: the plain full-screen draw in stereo, the temporal flag as given.

Both sites (0x6101DC and 0x600479) are patched together or not at all; check
also the getter's bytes and the AA pass prologue.

The blackout (register 36) keeps working because it is applied before the AA
pass. Flat play is unchanged.

**Log** (wording and format in `11-diagnostics.md`) once at install, and
once per change of the AA mode in effect in stereo, naming the mode and
saying when temporal SMAA is run spatially.

**Acceptance (in game, not yet done).** For FXAA, SMAA T1x and SMAA T2x at
pixel scale 1.0 with `StereoAntiAliasing` Auto: edges are smoothed in both
eyes, the image does not ghost or shimmer on head motion, and the
head-in-wall blackout still darkens the view. At pixel scale 1.5 with Auto,
and with Off, no AA runs in stereo (log). With On at pixel scale 1.5, AA runs.

## 4. Head-in-wall blackout delay

### 4.1 Game facts [sure]

The head-in-wall fade (HeadClipFade, 0x346DE0, stdcall `(const vec3* eye,
const vec3* pivot)`, `ret 8`, result on the x87 stack) returns the scene
visibility, 1 visible down to 0 black. Its caller, the VR camera behaviour
update (0x34DE50) on the game's main thread, calls it once per frame at
0x34E2C1 (`e8 1a 8b ff ff`) and stores the result in stereo object `+0x64`.
The fade itself and the `ClippingBlinders` threshold are described in 05
§7.2.

### 4.2 Problem and required behaviour

The view blacks out on the first frame the head touches geometry, so brief
contacts (walking through a door, leaning to look around a corner, a quick
head movement near a wall) flash the view to black.

Required, while stereo runs: redirect the call at 0x34E2C1 to a function
with HeadClipFade's ABI (stdcall, two pointer arguments, pops 8, result in
ST0) that calls the game's HeadClipFade with the same arguments, so that:

- the scene visibility used is 1.0 until the game's fade has stayed below
  1.0 for a delay **D = 0.5 s** of real time without a break; from then
  on, and for as long as it stays below 1.0, it is the game's value. D is a
  project decision: a contact that ends within half a second (the head
  passing a door frame or brushing a wall while walking, a quick lean) is
  incidental, while a head kept in geometry for longer is being held
  there and must still black out promptly, well under 1 s. The value is
  to be confirmed in game by the acceptance check below, which records the
  longest incidental contact seen; D is raised only if incidental contacts
  still darken the view;
- "clear" is the game's value exactly 1.0, with no tolerance. Game fact
  [sure] (HeadClipFade, read by the spec side): the fade is the product of
  two powers of clamped terms, one from the eye's distance to the pivot and
  one from a probe of the geometry ahead of the eye; each term is exactly 1
  when its clamp saturates (nothing near), and a power of 1 is exactly 1,
  so a head near nothing gives exactly 1.0 and any smaller value means
  geometry is near;
- any value of exactly 1.0 restarts the delay, so leaving the wall resets
  it;
- if stereo starts with the head already in geometry (no clear value seen
  yet), the game's fade applies at once: the delay exists to hide brief
  contact during movement, and a head that starts inside geometry is not
  one;
- time is measured with a monotonic clock, not in frames, so the delay is
  the same at any frame rate.

So contact shorter than D never darkens the view, and longer contact
fades exactly as the game computes, from the moment D is reached. It applies whether
`ClippingBlinders` is on or off. When stereo is not running the game's
value passes through unchanged.

The mod's own scene-visibility rules (05 §3.2g: forced 1 during cutscenes,
forced 0 behind a loading screen) are applied after this and win over it.

**Log** once at install that the fix is in.

**Acceptance (in game, not yet done).** With `ClippingBlinders` on: walking
through doorways, leaning around corners and brushing a wall with the head
at normal speed never flashes black; note the longest of these contacts
(the time the game's fade stays below 1.0, by any means, for example a
debug log line behind `DebugOptions`) and record it in §7 as the
measurement behind D. Holding the head inside a wall turns the view black
after about D (0.5 s), with the game's fade; pulling the head out restores
the view at once. In the mission
4 android encounter the view never goes black from head clipping (05 §15
item 18, and its open question in 05 §16).

## 5. Desktop mirror scale

The mirror (03 §8) must fill the window as specified in 03 §8.2, every
frame: the mirror image is drawn at scale 1.0 every frame, and no factor
depends on the pixel scale (a factor that did would shrink the image into
a corner of the window at pixel scales above 1).

The mirror draw must also set its viewport to the whole back buffer
(`{0, 0, width, height, 0, 1}` from the back buffer's own description)
itself: the game leaves whatever viewport it last used bound.

**Observed [sure]:** after both changes the mirror fills the window.

## 6. Acceptance summary

| § | Check | Status |
|---|---|---|
| 1 | Floors and walls lit the same in both eyes at pixel scale 2.0; the log reports several hundred patched shaders per level and no rejections | confirmed |
| 2 | 2.3 acceptance | to do |
| 3 | 3.2 acceptance | to do |
| 4 | 4.2 acceptance | to do |
| 5 | Mirror fills the window for each mirror type, symmetric on and off | confirmed |
| (07) | 07 §5 | to do |

## 7. Open questions

- **Main-view frustum in stereo (§2).** Whether the frustum the main view is
  culled with while stereo runs contains both eyes' frusta (it is the centre
  view's; with symmetric per-eye frusta offset by half the eye distance, the
  outer edges of each eye may fall just outside it). Look for: objects at the
  far left of the left eye or far right of the right eye vanishing; the
  frustum the main POV's culling context is built from (POV frustum update
  0x5B2FB0).
- **View groups (§2.1).** Confirm that only group 2 fills context `+0x5C`.
  Look for: the writers of POV `+0x310` and of context `+0x5C`.
- **Distance scale (§2.2).** Confirm that context `+0x50` comes from the
  centre view's field of view and resolution. Look for: the writer of
  context `+0x50` before the call at 0x5E984B.
- **Head-in-wall delay (§4.2).** Record the measured longest incidental
  contact and confirm or adjust D.
- **`+0x1B2F` and the jitter (§3.1).** Confirm their meaning. Look for: the
  AA pass's branch on `+0x1B2F` and the SMAA runner's write of the jitter.
- **Per-eye T2x.** True temporal SMAA in stereo (a velocity pass for eye 1
  and per-eye history) is not required in v1.
- **2020 build.** Every address of sections 2 to 4.
