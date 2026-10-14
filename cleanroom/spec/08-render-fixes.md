<!-- Provenance: static analysis of AI.exe (2015 Steam build, verified against the binary) and of the game's pixel shaders (DXBC from the level and global shader packs); analyses of other VR mods used only to learn which problems exist; in-game testing of an earlier rebuild in a headset (section 1 and section 5 confirmed working); user requirements. Revision: r1 -->

# 08: Rendering fixes

Five problems in how the game renders in stereo, beyond the reflections of
07: lighting that differs between the eyes, objects popping in and out,
anti-aliasing switched off, an abrupt head-in-wall blackout, and the size of
the desktop mirror. For each: what the game does, why it fails in VR, and
what the mod must do.

Addresses are RVAs of the 2015 build (image base 0x400000). The 2020
addresses are not known; fixes that need a game address stay off on that
build. Section 1 needs no game address and applies to both builds.

Confidence: **[sure]** the game code or shader shows it directly;
**[likely]** strong inference; **[guess]** must be confirmed in game.

| § | Problem | Status |
|---|---|---|
| 1 | Lighting differs between the eyes | required; implemented; **confirmed in game** |
| 2 | Object pop-in | required; not implemented |
| 3 | Anti-aliasing off in stereo | required; not implemented |
| 4 | Head-in-wall blackout too abrupt | required; not implemented |
| 5 | Desktop mirror smaller than the window | required; implemented; **confirmed in game** |

## 1. Position-based dither in the game's pixel shaders

### 1.1 What the shaders do [sure]

Several hundred of the game's pixel shaders (716 of the 2178 distinct pixel
shaders in one level's `LEVEL_SHADERS_DX11_BIN.PAK`, plus some global
shaders) add a small anti-banding dither to a lighting or reflectance
factor. The dither is a hash of the pixel's **absolute** position. In the
compiled shaders (shader model 5.0) it is always this sequence, where `vP`
is the `SV_Position` input, `vT` another input (a texture coordinate) and
`rA`, `rB`, `rC`, `rD` temporaries:

| # | Instruction (as the game's shaders contain it) | Value |
|---|---|---|
| 1 | `mul rA.xy, vP.xyxx, l(0.001, 0.001, 0, 0)` | `a = 0.001·p` |
| 2 | `mad rB.zw, vP.xxxy, l(0, 0, 0.001, 0.001), -vT.xxxx` | `b = 0.001·p - t` |
| 3 | `dp2 rC.w, rB.zwzz, rA.xyxx` | `d = b·a` |
| 4 | `exp rC.w, rC.w` | `2^d` |
| 5 | `mul` by 10000, `frc`, `mul` by 1400, `frc` | hash `h` in `[0, 1)` |
| 6 | `mad rD.w, rC.w, l(0.0025), rD.w` | factor += `0.0025·h` |
| 7 | `min rD.w, rD.w, l(1.0)` | factor clamped to 1 |

Register numbers and the input numbers vary between shaders; instructions
1 and 2 are always adjacent and in this order, with the immediates exactly
`0.001` (bit pattern 0x3A83126F). The factor then scales an output colour
(emissive or environment light).

### 1.2 Why it fails on a side-by-side target [likely]

`p` is in pixels of the bound target. On the side-by-side target the right
eye's `x` is offset by the eye width, so its `p` is much larger than the left
eye's, and the larger the render resolution the larger both are. `d` grows
roughly as `(0.001·|p|)²`:

- once `2^d · 10000` passes 2^24, `frc` returns 0 and the dither vanishes;
  before that it degrades into coarse patterns;
- with large or negative tiled texture coordinates `t`, `d` passes 128,
  `2^d` overflows to infinity, `frc` gives NaN, and under Direct3D's rules
  `min(NaN, 1)` returns 1: the factor is forced to 1 and the surface is
  fully lit or fully reflective.

The result is surfaces lit differently in the two eyes, first in the right
eye and worse at higher resolution (MotherVR issue #253). Players described
it as reflections moving against the head; it is lighting, not the planar
reflection of 07.

### 1.3 What the game provides [sure]

Pixel constant buffer 2 (`cbDefaultPSC`) register 39 holds
`(W, H, 1/W, 1/H)` of the main view's render target. It is written in the
post-processing setup (0x6007B0..0x600811: a size query 0x627B90, then the
constant upload 0x621C90 with buffer 2, register 0x27, count 1). The
shaders' reflection data names it `SourceResolution`. It differs from
register 60 (`ScreenResolution`, 07 §2.7).

### 1.4 Required change

Every pixel shader the game creates is checked, at
`ID3D11Device::CreatePixelShader`, for the instruction pair 1-2 of §1.1.
At each occurrence the dither must use the position **relative to the
render target** `n = p · (1/W, 1/H)` (each component in `[0, 1]`) instead of
the absolute position:

- instruction 1 computes `n`: `vP.xy` times register 39's `zw` components,
  in place of the immediate `0.001`;
- instruction 2 computes `0.001·n - t`, taking `n` from instruction 1's
  result instead of from `vP`.

Numeric effect: the exponent becomes `d = n·(0.001·n - t)`, that is about
`-t·(n.x + n.y)` plus at most 0.002. It no longer depends on the target size
or on which half of the target the eye occupies, stays far from overflow for
the texture coordinates the game uses, and `a = n` is what instruction 1
produced on a flat screen about 1000 pixels wide. The dither keeps its
amplitude (at most 0.0025) in both eyes.

Constraints on the patched shader:

- **Same length.** The patched instruction stream must keep every other
  instruction at its offset and the shader's total size: replacing the
  4-component immediate operand by a constant-buffer operand shortens
  instruction 1, and the freed space is filled with `nop` instructions.
- **Constant buffer declaration.** Register 39 must be inside the shader's
  declared size of constant buffer 2: if the declaration is smaller than 40
  registers it is raised to 40. A shader that has no declaration of
  constant buffer 2 is left unchanged.
- **Checksum.** The DXBC container's 16-byte checksum (bytes 4..19) must be
  recomputed over the patched container (the standard DXBC checksum, an MD5
  variant over bytes 20 to the end). The stored checksum of the original is
  checked first; a shader whose checksum does not verify is left alone.
- **Conservative.** Any shader whose structure is not as expected is left
  unchanged. If `CreatePixelShader` rejects a patched shader, the original
  bytecode is used.
- Shaders without the pair are passed through untouched.

This runs in flat play too; it changes nothing visible there beyond the
dither pattern.

**Observed:** in the headset, surfaces that lit up in the right eye only are
lit the same in both eyes after the change.

## 2. Object pop-in

### 2.1 The game's per-object visibility test [sure]

The scene's object lists are culled by one function (0x61FD20, cdecl:
`(cull context*, per-object flags*, bounds[] with stride 0x40, items[],
count, out[])`), whose only call is at 0x5E984B. For each object it:

1. tests masks: the object's mask byte against context `+0x54`, its layer
   word against context `+0x58`; flags `& 0x19` skip it; flags `& 2` keep it
   unconditionally;
2. unless flags `& 4`: a **distance/LOD test** (0x619A50, cdecl
   `(bounds*, const vec3* camera position = context + 0x40, float s)`)
   with `s` = the object's 16-bit LOD value times context `+0x50`; it culls
   when `|centre - camera|² · s² > radius²` (centre at bounds `+0x0C`,
   `+0x1C`, `+0x2C`; radius at `+0x3C`);
3. a **frustum test** (0x619030, `(bounds*, context*)`);
4. if context `+0x5C` is set, an extra plane test (0x619860); if context
   `+0x60` is set, another (0x619920, with context `+0x70`). These are
   occluder/portal tests [likely];
5. appends the item to `out[]`.

The same function serves the main view and the shadow-caster lists.

### 2.2 Why it fails in VR [likely]

The distance scale `+0x50` is derived from the field of view and
resolution of the single centre view, and the extra tests of step 4 are
built from that centre view. In a headset each eye has a much wider field of
view and its own position. Small props and decals vanish or appear at
distances that do not match what the player sees, and objects near the
edges of each eye are dropped by tests made for the centre view (worse on
wide-FOV headsets). Shadows of such objects pop as well (MotherVR issue
#194).

### 2.3 Required behaviour

While stereo runs:

- the distance/LOD test (step 2) must not remove objects: treat it as
  passed, for the main view and for shadow casters;
- for the main view, the extra plane tests of step 4 must not remove an
  object that is visible in either eye; skipping them for the main view is
  acceptable;
- the frustum test stays, but the frustum it uses for the main view must
  contain both eyes' frusta (to verify: which frustum the main view's cull
  uses while stereo runs);
- in flat play the game's behaviour is unchanged.

The cost is more objects drawn; this is accepted. Confirm in game: decals
and small props no longer appear or vanish while walking, including at the
edges of the view.

## 3. Anti-aliasing in the stereo output path

### 3.1 Where the game skips it [sure]

Graphics setting 0x09 is anti-aliasing. Its handler (0x5F6AC0, thiscall,
one argument, `ret 4`) stores the mode at post-processing object `+0x1B3C`
(the object is returned by 0x5F19C0): 1 FXAA, 2 SMAA T1x, 3 SMAA T2x,
otherwise 0.

The final post chain (0x610040..0x6101FA) runs the post-processing pass
(0x600490) and then, after debug-view checks, tests whether stereo is
running (0x610144: 0x01E150 on the object at 0x13583A0, true when the
stereo device state is 2):

- **flat:** AA pass (0x600210), then the post-processing pass (0x600490)
  again;
- **stereo:** the stereo output (0x5FC560), which sets pixel constant
  buffer 2 register 36 to two post-processing values multiplied by the
  stereo object's scene visibility (`+0x64`, the head-in-wall blackout), and
  0x5A0980. **The AA pass is never called.**

For SMAA T2x the velocity pass (0x5B1720) runs on the eye-0/centre path
only; the main view's eye-1 preparation (0x5B5B70) skips it.

### 3.2 Required behaviour

The anti-aliasing chosen in the game's options must apply in stereo:

- **FXAA and SMAA T1x** (spatial): the AA pass runs on the side-by-side
  image in stereo, in the same order relative to post-processing as in
  flat play. Bleeding of one or two pixels across the seam between the eyes
  is acceptable; keeping each eye's filter inside its half is better.
- **SMAA T2x** (temporal): each eye needs its own velocity (the velocity
  pass must also run for eye 1, into eye 1's half) and its own
  previous-frame view-projection history; reprojection must not fetch
  history from the other eye's half. If this cannot be met, T2x in stereo
  must behave as T1x (no temporal component) rather than ghost in one eye.
- The scene visibility (blackout) the stereo output applies through
  register 36 must keep working with AA on.
- Flat play is unchanged.

Confirm in game for each mode: edges are smoothed in both eyes, no
ghosting on head motion, and the head-in-wall blackout still darkens the
view.

## 4. Head-in-wall blackout delay

### 4.1 Game facts [sure]

The head-in-wall fade (HeadClipFade, 0x346DE0, stdcall
`(const vec3* eye, const vec3* pivot)`, `ret 8`, result on the x87 stack)
returns the scene visibility, 1 visible down to 0 black; its caller
(the call at 0x34E2C1 in the VR camera behaviour update) stores it in
stereo object `+0x64`. The fade itself and the clipping-blinders option are
described in 05 §7.2.

### 4.2 Problem and required behaviour

The view blacks out on the first frame the head touches geometry, so brief
contacts (leaning to look around a corner, a quick head movement near a
wall) flash the view to black.

Required: a **grace period of 0.6 s**. When the visibility the game
computes is below 1 (more precisely below 0.99999), the mod lets visibility
1.0 through instead, until 0.6 s have passed since the last frame in which
the game's value was 1; after that the game's value passes through
unchanged, so the fade then proceeds as the game computes it. Leaving the
wall resets the timer. The grace period is measured in real time, not in
frames. It applies whether the clipping-blinders option is on or off, and it
never shows the scene during cutscenes or loading where the mod itself
forces black (05).

## 5. Desktop mirror scale

The mirror (03 §8) must fill the window as specified in 03 §8.2, every
frame. MotherVR drew the mirror image scaled by 1.0 on the first frame and
by `0.5 / pixel scale` afterwards. That factor only matched its own
behaviour of starting the eyes at half size (00 §4 item 1); with eyes at full
size from the start it shrinks the image into a corner of the window. v1
uses no such factor.

The mirror draw must also set its viewport to the whole back buffer
itself: the game leaves whatever viewport it last used bound.

**Observed:** after both changes the mirror fills the window.
