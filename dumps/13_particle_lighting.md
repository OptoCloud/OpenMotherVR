# 13 - Particle (alpha) lighting vs. head pose in VR

Target: AI.exe 2015 Steam build (image base 0x400000). **All addresses below are RVAs**
(VA - 0x400000), globals included (`LightObject` = RVA 0x1365C90, as in cleanroom 24 §1.1).
Confidence tags: **[sure]** read directly from code or shader, **[likely]** strong inference,
**[guess]** unverified.

Symptom (user, in the headset): the lighting on smoke, steam and fog is the same in both eyes, and
"the light moves through the smoke in the direction I angle my head": the lit region of a smoke
volume shifts when the head turns in yaw or pitch, as if head-locked.

## TL;DR

* Smoke and particle lighting is a **screen-space atlas**. Once per frame the game snapshots the main
  view as a camera, the "alpha-light camera" (`LightObject` +0x70 = its view-projection), projects
  every lit emitter's bounding box into that camera's NDC, and gives the emitter a tile of a
  512x512 target. Each light then draws into the tiles, at the world points the tile's NDC rectangle
  unprojects to. The particle shaders read the tile back at the particle's **own screen position**:
  `uv = ndc_draw.xy * OffsetScale.zw + OffsetScale.xy` (VS constant `AlphaLight_OffsetScale`,
  cb0[29]; PS texture `alphalightIntensitiesReg` at t9). **[sure]**
* For the sprite emitters, the CPU-side `OffsetScale` (0x57EB30) has a stereo branch: it asks the
  stereo device for the eye's NDC scale and offset (wrapper 0x5B86E0 → device slot 14, called at
  **0x57EC15**) and converts the eye's NDC to `ndc/scale - offset`, a **view tangent**. But the
  tiles were laid out in the **NDC of the snapshot projection**, which in stereo is the eye-0 FOV
  projection (slot 12 with eye -1), whose scale is the same `scale`, not 1. The two only agree for
  a 90° FOV (scale 1). **[sure for the code]**
* With the mod's symmetric render FOV (03 §3.2: both eyes and the snapshot share one FovPort, scale
  `s = 1/tan`, about 0.7 to 0.8 on current headsets), the lookup reads the tile at `ndc / s`: a
  radial magnification by 1/s about the centre of the view. A particle at screen position `n`
  shows the lighting computed for the point at `n / s`, and particles of an emitter that is not near
  the view centre read outside their own tile. Turning the head moves every particle's `n`, so the
  lit pattern slides by about `(1 - s)` of the head turn, in the direction of the turn, and lighting
  is only right where you look straight. Both eyes do the same, so it looks synced. **[likely]**
  for the effect (it matches the report exactly), **[sure]** that flat play is unaffected (the
  wrapper returns `(0, 0, 1, 1)` without a running device, which makes the lookup the identity).
* Fix: replace the call at 0x57EC15 so that, while stereo runs, the lookup receives the eye's scale
  and offset **relative to the snapshot's**: `scale = s_eye / s_centre`,
  `offset = s_centre * o_eye / s_eye - o_centre`, from the game's own slot 14 for the eye and for
  -1. With the mod's symmetric, shared FOV that is exactly `(0, 0, 1, 1)`, the flat-play value.
* This is not 24 §4 (the eye-1 light resubmission): that would differ between the eyes. It is also
  in MotherVR 0.8.1, which keeps the game's slots 12 to 14 (02 §5).

---

## 1. Objects

| RVA | What | Notes |
|---|---|---|
| 0x1365C90 | `LightObject` (= static object at 0x13583A0 + 0xD8F0) | `[0x1430468]` holds its address: the constructor 0x5DE6D0 ends with `mov [0x1830468 VA], esi` (`89 35 68 04 83 01` at 0x5DE8CB), and the owner's constructor 0x5AFA70 is called on 0x13583A0 at 0xD74F30, constructing `LightObject` at +0xD8F0 (call at 0x5AFB20). **[sure]** |
| `LightObject` +0x34 / +0x38 | the alpha-light clients (vector of object pointers, count) | added by 0x5D4A70 (thiscall, `push`, ret 4) at 0x3DC25C, 0x3DC26C, 0x3DC27C with the objects of 0x3C1990, 0x3C19E0, 0x3C1A30 (vtables 0xEF5254, 0xEF5288, 0xEF52BC). **[sure]** |
| `LightObject` +0x70 | the alpha-light camera's view x projection (4x4, row vectors) | 0x5C5F50 §2. **[sure]** |
| `LightObject` +0xB0 | `0x4BC550(+0x70)`, used to unproject tile NDC to world (§3.2) | inverse of +0x70 **[likely]** (matches the shaders' name `AlphaLight_CliptoWorldMatrix`) |
| `LightObject` +0xF0, +0x100, +0x110, +0x120 | the camera's axes in world space (columns 1, 0, 2 of the view matrix, the first negated) and its position | **[sure]** |
| `LightObject` +0x130 | the camera's vertical FOV | from slot 13 (-1) in stereo **[sure]** |
| `LightObject` +0x2A8 | the alpha-light texture, bound to PS t9 by 0x5D4C70 / 0x5D55B0 (`push 9`, call 0x627750) | **[sure]** |
| `LightObject` +0x32C | the 512x512 render target of the light passes (`push 0x200` twice in 0x5D4B50) | **[sure]** |

## 2. The alpha-light camera snapshot: 0x5C5F50 **[sure]**

Called once per frame from the game update's camera step 0x3B4090, at **0x3B4160**
(`b9 90 5c 76 01 e8 eb 1d 21 00`, ECX = `LightObject`), after the camera update (0x035520), the
stereo update (0x5BE2A0), camera to POV (0x032300) and the POV manager update (0x5B45A0). So it sees
the main POV's **centre** view of this game frame, which already contains the head rotation
(dumps/07 §2). Game thread.

```
pov  = GetMainPOV(POV manager [0x1366A00])          ; 0x5B0B40
+0x130 = pov.fov ; +0x120 = pov.camPos (+0xC0)
V = pov.view (+0x00) ; P = pov.proj (+0x40)         ; local copies (rep movsd)
if (stereo device && state == 2) {
    device slot 12 (-1, &V, &P)                    ; 0x5C5FEB: P := eye-0 FOV projection, offset 0
    device slot 13 (-1, &+0x130, &aspect)           ; 0x5C6014
}
+0x70  = V * P                                      ; 0x428220, 0x5C6027
+0xB0  = 0x4BC550(+0x70)                            ; 0x5C6040
+0xF0, +0x100, +0x110 = camera axes from V
clients[i]->slot 8 (+0x20) ; reset the atlas ; ++frame (+0x50)
```

Device slot 12 (0x5CD4C0) builds the projection, then calls slot 14 with the **same eye** and sets
`P[0][0], P[1][1]` to its scale and post-multiplies a translation by its offset (0x5CD5C5..0x5CD646).
Slot 14 (0x5B8F10) returns `(offset.x, offset.y, scale.x, scale.y)` from the eye's FovPort with
LibOVR's formula (0xFA8F30); for eye -1 it returns offset 0 and **eye 0's scale** (the `cmp eax, -1`
branch at 0x5B8F56). So in stereo the snapshot's NDC is `ndc_c = s_0 * tan` (tan = the view-space
tangent), with `s_0 = 2 / (left + right)` and `2 / (up + down)` of eye 0's FovPort. In flat play
`P` is the flat camera projection.

## 3. The tiles

### 3.1 Emitter rectangle in the snapshot's NDC: 0x589A20 **[sure]**

The sprite client's registration/update (reached through `[0x1430468]`; entry tables at
`LightObject` +0x40) takes the emitter's bounding box, moves corners behind the camera plane
(+0x110 forward, +0x120 position) in front of it (0x589D45..0x589DF3), transforms the eight corners
by **+0x70** (0x589E36..0x58A082, the matrix copied transposed to locals), divides by w and clamps to
[-1, 1] (0x58A097..), and stores in the entry (0x58A5FC..0x58A62A):

| Entry offset | Content |
|---|---|
| +0x1C, +0x20, +0x24 | centre of the rectangle in the snapshot's NDC (x, y, z) |
| +0x28, +0x2C | half extents in the snapshot's NDC |

0x5D4B20 then requests an atlas tile for it.

### 3.2 Light passes **[sure for the code]**

* `ALPHALIGHT_TASK` (job 0x3753C0) runs 0x5D52D0 on `LightObject`: client slot 10 (+0x28) writes
  each client's vertices; for the sprite client that is 0x57EDB0, which unprojects the rectangle's
  corners at the centre depth through **+0xB0** (0x57EF15.., matrix at `[0x1430468]` + 0xB0..0xEC)
  into world points, and slot 11 (+0x2C, 0x57FBA0) per light, through 0x5D5170, records which
  vertex range each light covers. The atlas level of a tile comes from the rectangle's size
  (`fsqrt` at 0x5D535C, 7 levels at +0x134 + 0x1C·k) [likely].
* In each eye's transparent pass 0x592E10 (24 §1.7): 0x5D4B50 binds the 512x512 target +0x32C and
  clears it; 0x5D4D10 per light uploads the light (VS buffer 0 reg 20, buffer 10 regs 5..10),
  picks the light shader by type (+0xA3) and flags (+0xA6) from +0x3CC..+0x3F0 and draws its range;
  0x5D4C70 restores the target and binds +0x2A8 to t9.

The tile therefore holds the lighting at the world points that the tile's NDC rectangle, **in the
snapshot camera**, unprojects to. Both eyes draw the same geometry, so both get the same tiles
[likely: the vertex ranges are written once per frame by the job].

### 3.3 The particle shaders **[sure]**

`DATA/ENV/PRODUCTION/BSP_TORRENS/RENDERABLE/LEVEL_SHADERS_DX11_BIN.PAK` (1816 DXBC blobs):

* 114 pixel shaders bind `alphalightIntensitiesReg_TEX/SMP` at t9/s9. The simplest (blob at file
  offset 0x63BFF0):
  ```
  div r0.xy, v2.xyxx, v2.zzzz
  sample_indexable(texture2d) r0.xyz, r0.xyxx, t9.xyzw, s9   ; lighting
  mul r0.xyz, r0.xyzx, v0.xyzx                                ; times the particle colour
  ```
* 366 vertex shaders use `AlphaLight_OffsetScale` (cbDefaultXSC = cb0, register 29). 297 of them
  build `v2` from the particle's clip position in the **drawn** `ViewProj` (cb0[0..3]), e.g. blob
  0x17F50:
  ```
  dp4 r2.x..w, r1.xyzw, cb0[0..3]                 ; clip = world * ViewProj
  mul r1.xyzw, r0.xyww, cb0[29].zwxy               ; (x*S.x, y*S.y, w*O.x, w*O.y)
  add o2.xy, r1.zwzz, r1.xyxx                      ; o2.xy = clip.xy * S + clip.w * O
  mov o2.z, clip.w
  ```
  so `uv = ndc_draw * OffsetScale.zw + OffsetScale.xy`. The other 69 take the tile coordinate from
  a vertex attribute or a constant (not screen-space).
* The pixel constant buffer also declares `AlphaLight_WorldtoClipMatrix` (cb2[0..3]),
  `AlphaLight_CliptoWorldMatrix` (cb2[4..7]) and `AlphaLight_ScaleParams` (cb2[42]); none of the
  particle shaders read them.

In stereo `ViewProj` is the eye's (POV::SetEye, 07 §2.4), and the eye's NDC with the mod's shared
symmetric FOV equals the snapshot's NDC up to the eye's small ViewAdjust translation.

## 4. The lookup: 0x57EB30 **[sure for the code]**

`OffsetScale` is uploaded to VS buffer 0 register 0x1D by 0x617E50 (call 0x621C20 at 0x617EC5),
reached from 0x618BB0 at 0x618C27, which takes it from **0x57EB30** for the sprite client (flag
bit 0 of byte +5 of the draw's flags clear) or from 0x57FDC0 (that bit set: one texel per emitter,
no camera). A second uploader, 0x60C370 (callers 0x60CF22, 0x60DE0E, 0x616136), uses 0x57FDC0 or
0x580DC0 (a tile mapping with no camera term, for the non-screen-space shaders [likely]).

0x57EB30 (cdecl `(float out[4], renderer*, const u16* alphaLightIndex)`):

```
tile = 0x577BA0(...)                      ; (t0, t1, t2, t3): the tile's origin and size in the atlas
a = E.cx - E.hx ;  c = 2 * E.hx            ; the rectangle of 3.1, x (likewise b, d for y)
eye = renderer->+0x2C                      ; the renderer's current eye (07 §2.4)
(O.x, O.y, S.x, S.y) = Stereo::EyeNdc(eye) ; 0x57EC15: e8 c6 9a 03 00 -> 0x5B86E0
out.x = t0 + (t2 / c) * (-O.x - a)         ; offset
out.z = (t2 / c) / S.x                     ; scale
(y the same with 1 - t1, -t3, b, d)
```

With the shader's `uv = ndc * out.zw + out.xy`:

```
uv.x = t0 + t2 * ( (ndc_draw.x / S.x - O.x) - a ) / c
```

i.e. the tile is addressed with `q = ndc_draw / S - O`, which must be a coordinate in the
**snapshot's NDC** (the rectangle `[a, a + c]` of 3.1).

`Stereo::EyeNdc` (0x5B86E0, thiscall on the stereo manager `[0x1366A2C]`, `(float out[4], int eye)`,
`ret 8`, returns `out` in EAX) writes `(0, 0, 1, 1)` and, if the device exists with state 2, calls
device slot 14 `(eye, out)`. It has one caller, 0x57EC15 (`8b 0d 2c 6a 76 01` loads ECX at 0x57EBB6;
`push edx` = eye, `push eax` = out). **[sure]**

## 5. Why it is head-locked in stereo **[likely]**

* Flat: the wrapper gives `(0, 0, 1, 1)`, `q = ndc_draw = ndc_c`. Correct.
* Stereo: slot 14 gives the eye's `(o_e, s_e)`; the snapshot's NDC is `s_c * tan + o_c` with
  `o_c = 0`, `s_c` = eye 0's scale. The correct coordinate is
  `ndc_c = s_c * (ndc_e - o_e) / s_e + o_c`; the game uses `ndc_e / s_e - o_e`.
  With the mod's symmetric FOV shared by both eyes and the centre (03 §3.2), `o_e = o_c = 0` and
  `s_e = s_c = s`, so the game samples the tile at `ndc_e / s` instead of `ndc_e`.
* `s = 1 / tan(half FOV)`: about 0.7 to 0.8 with the render tangents of current headsets (1.2 to
  1.4), so the lookup is magnified by 1.25 to 1.4 about the centre of each eye's view.
* A lit spot at world direction θ_L (small-angle, tangent units) appears on the particles at
  θ = s·θ_L + (1 - s)·ψ when the head points at ψ: it moves by `(1 - s)` of every head turn, in
  the direction of the turn (yaw and pitch alike), and is right only where the player looks
  straight at it. Emitters away from the centre read texels outside their own tile (other tiles,
  or the cleared atlas), so their lighting jumps as the head turns.
* Both eyes use the same tiles and the same magnification: the error is the same in both eyes.
* The game's DK2 code path never ran with a FOV whose scale is 1, so this was never right; a 2014
  DK2 FovPort (tangents about 1.06 to 1.33) already gives `s` of 0.75 to 0.93. MotherVR 0.8.1 keeps
  slots 12 to 14 and has the same lookup.

## 6. Fix

Redirect the call at **0x57EC15** (`e8 c6 9a 03 00`, verified with `calls(site, 0x5B86E0)`) to a
replacement with the wrapper's ABI, `float* __thiscall(manager, float out[4], int eye)`, `ret 8`,
returning `out`:

* stereo not running: call 0x5B86E0 as the game does;
* stereo running: `e = 0x5B86E0(eye)`, `c = 0x5B86E0(-1)` (the snapshot's own), and
  `out = (s_c.x * o_e.x / s_e.x - o_c.x, s_c.y * o_e.y / s_e.y - o_c.y, s_e.x / s_c.x, s_e.y / s_c.y)`
  so that `ndc_e / out.scale - out.offset = ndc_c`. A scale that is 0 or not finite keeps the
  game's value.

With the mod's FOV this is `(0, 0, 1, 1)` in both eyes. Render thread, no allocation; 2020 build:
the addresses are unknown, the fix stays off.

Not changed (and not the cause of this symptom):

* The tiles are built from the centre view, not per eye, so each eye still sees lighting placed
  with the centre camera: an error of the eye's ViewAdjust (half the IPD) at the emitter's distance,
  the same parallax the planar reflections had before 07.
* The snapshot is taken on the game thread for the frame being prepared, while the render thread
  draws the previous one (02 §9.1). If the job and the draw ever use snapshots of different
  frames, fast head turns would offset the lighting by one frame of motion [guess]; that would lag
  behind the head and settle when it stops, unlike the reported, position-dependent shift.
* The 0x60C370 / 0x580DC0 path has no stereo term; its shaders do not index by screen position
  [likely].

## 7. In-game check

1. A scene with steam or smoke lit by a nearby lamp (Sevastopol vents; Medical, Seegson
   Synthetics corridors). Look at the lit part of the smoke, then turn the head left/right and
   up/down while keeping the smoke in view: the lit region must stay fixed on the smoke and lamp,
   as in flat play, not drift with the head.
2. Smoke near the edge of view must be lit as it is when looked at directly (no dark or wrongly
   lit edges that change as the head turns).
3. Both eyes alike (close each in turn); flat play (runtime None) unchanged.
4. The log shows `[hook] particle lighting: in place on 1 sites`; with Dev. Debug Options, one
   `[diag] particle lighting: ...` line with the game's per-eye scale and the one used.

## 8. Address summary (2015 build)

| RVA | Item |
|---|---|
| 0x3B4160 | call of the alpha-light camera snapshot 0x5C5F50 in the camera step 0x3B4090 |
| 0x5C5F50 | snapshot: main POV view x (slot 12 (-1)) projection → `LightObject` +0x70, +0xB0 |
| 0x589A20 | sprite client: emitter bounding box → rectangle in the snapshot's NDC (entry +0x1C..+0x2C) |
| 0x57EDB0 | sprite client slot 10: light-pass vertices, unprojected through +0xB0 |
| 0x5D52D0 | `ALPHALIGHT_TASK` body (job 0x3753C0) |
| 0x592E10, 0x5D4B50, 0x5D4D10, 0x5D4C70 | light passes into the 512x512 atlas, bound to t9 |
| 0x617E50, 0x618C27 | `AlphaLight_OffsetScale` upload (VS cb0[29]) for the sprite path, and its call |
| 0x57EB30 | `OffsetScale` from the tile and the eye's NDC scale/offset |
| **0x57EC15** | **`call 0x5B86E0` (`e8 c6 9a 03 00`), the fix's site** |
| 0x5B86E0 | `Stereo::EyeNdc` wrapper: `(0,0,1,1)`, or device slot 14 when state is 2 |
| 0x5B8F10 | game's device slot 14 (kept by the mod) |
| 0x5CD4C0 | game's device slot 12 (kept by the mod) |
