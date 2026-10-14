# 07 - Floor reflections vs. head pose (planar reflections in VR)

Target: AI.exe 2015 Steam build (image base 0x400000; all addresses below are VAs, RVA = VA - 0x400000).
Confidence tags: **[sure]** read directly from code/shader, **[likely]** strong inference, **[guess]** unverified.

## TL;DR

* The floor reflections are the engine's **planar reflections** (graphics setting 0x10). A second POV called
  `"Reflection Map"` renders the scene mirrored about the floor plane, **once per frame** (mono), into a
  half-resolution texture. The floor shader samples that texture in **screen space**:
  `uv = (1 - SV_Position.x / ScreenResolution.x, SV_Position.y / ScreenResolution.y)` **[sure]**.
* The head rotation **is** in the reflection camera. It enters through the VR camera behaviour into the
  camera, then into POV[0]'s centre view, and the reflection view is `Mirror(plane) * POV[0].view` with X negated. **[likely/sure]**
  So the reflection camera is not missing the head rotation, and it is not mirrored the wrong way either.
* The bug is a **projection/layout mismatch**. The reflection texture is one mono image taken with
  POV[0]'s **centre (flat-game) projection**, stretched over the **whole double-wide side-by-side RT**. Each eye
  only covers half of the RT and uses a much wider per-eye FOV (`ApplyEyeTransform`). So for each eye,
  screen position maps to reflection direction with the wrong scale (about 2x) and the wrong centre (the
  reflection's centre is at the seam between the eyes). When the head rotates, the reflection content moves
  on screen about 2x faster than the world. Relative to the floor, that is a slide of about 1x the head motion in
  the **opposite** direction. Roll rotates the image about the wrong centre with anisotropic scale, which shows as shear/swing
  "distortion". **[likely]**
* The original MotherVR does not touch any of this (no patch in 06c is reflection related) **[sure]**, so 0.8.1 has the bug too.
* Real fix: render the reflection POV **per eye into the two halves of the reflection texture**, reusing the
  game's own `POV::SetEye` (0x9b38d0) machinery. Because the reflection view is X-mirrored, half *k*
  rendered with `SetEye(k)` holds exactly eye *(1-k)*'s correct reflection, and that is precisely the half that
  eye *(1-k)* samples (shader does `1 - u`). Hook site: the single call `0x9b5d91: call 0x9b5c40`.
  Fallback: Planar Reflections = Off (the setting itself, or a 2x3-byte patch).

---

## 1. Planar reflection subsystem (reflection manager, global `[0x175835c]`)

| Address | What | Notes |
|---|---|---|
| vtable 0x13e3dd0 | reflection manager class | slot1 0x99abf0 = OnSettingChanged |
| 0x99b850 | init | reads setting 0x10 (`0x99df70(0x1758a50,0x10)` → `0x9a4d00` → `+0x50`) into `+0x60`; calls 0x999540; `+0x58/+0x5c` = `[0x17588c4]/2`, `[0x17588c8]/2` (half of the main render resolution, which is the double-wide stereo RT in VR) |
| 0x999540 | creates the reflection POV | `POVManager::CreatePOV([0x1766a00], "Reflection Map" (0x13e3cb0), 0)` 0x9b43c0 → `+0x50`; sets POV `+0xe8 = 1` (POV type 1 = reflection), `+0x348 = 4`, `+0x36c \|= 8`, `+0x338 = 0x28`, `+0x36e = 0x72`, `+0x36b &= ~1` **[sure]** |
| 0x99abf0 / 0x99ab30 | setting 0x10 changed | `+0x60 = value`; (re)creates the reflection RT (0x99a0c0) only if value != 0 |
| 0x99b990 | pick best reflection plane | thiscall(`float4x4* viewProj, float3* camPos`), ret 8; iterates planes `+0x6c[+0x70]` |
| 0x99b8f0 | bind reflection texture | binds `[+4]` (reflection RT) if `+0x56 && +0x60`, else default `[0x1bdf60c]`, to **PS slot t7** (`0xa22e80(ctx,1,7,tex)`) **[sure]** |
| 0x99aae0 | register a reflective material | adds to list `+0x68` if enabled |
| 0xa0cf70, 0xa15178 | material draw paths | call 0x99b8f0 + 0x99aae0 when material flag `+0x36 & 0x10` |
| 0x99c8b0 → 0x99bd80 | mip blur of the reflection | "Reflection Mip Blur Target" / "Intermediate Reflection Mip Blur Target" |

### 1.1 Reflection camera: `ReflectionPOV::Update` 0x9b41e0 (thiscall, ebx = reflection POV) **[sure]**
Called from 0x9b4640 inside `POVManager::Update` 0x9b45a0 (for POVs with `+0xe8 == 1`), which runs in the
**game update** at 0x7b4119. That is right after the camera update 0x435520, MotherVR's `Stereo::Update` (0x9be2a0 site)
and camera→POV 0x432300, and **before** the render jobs that do the per-eye `SetEye`.

```
refl.flags36b.bit0 = (planeCount([0x175835c]+0x70) > 0)      ; "active"
if (!active) return
main = first POV with flags36b & 0x80                          ; main view = POV[0]
V = main.view (+0x00), P = main.proj (+0x40)                   ; centre (pre-eye) matrices
VP = V*P ; camPos = main+0xc0
plane = 0x99b990(VP, camPos) ; refl+0x30c = plane
R = 0x99e9e0(0x99ac20(plane))                                  ; 4x4 mirror matrix about the plane
view = R * V            (0x428220: this*arg, row vectors: p*R*V)
view[0][0], view[1][0], view[2][0], view[3][0] = -...          ; negate column 0 (x) = keeps winding, mirrors image L/R
refl.view (+0) = view ; 0x9b2fb0 (frustum) ; refl.proj (+0x40) = P
refl+0xe4 = P[1][1]/P[0][0] ; refl+0xe0 = fov from P[1][1]
```
So: **reflection view = `R · V_centre · Fx`, reflection proj = main POV's centre projection `P_c`** (the
game's flat camera projection from 0x432300; it is not the per-eye HMD projection). **[sure for code, likely that +0x40 holds the
flat proj at that time]**. At that point `+0x40` holds what camera→POV wrote, or the centre restored from
`0x182f620` by 0x9b5b70 at the end of last frame's eye 1. Either way it is not the per-eye proj.

### 1.2 Reflection render: once, mono **[sure]**
Render job 0x775400 → `POVManager::RenderPOVs` 0x9b5d40 loops over POVs. Type 0 → 0x9b5be0
(`SetEye(-1)` + full render). **Type 1 (reflection) → 0x9b5c40**, called only from **0x9b5d91**:
`0x9b11d0` (BeginRender: constants from +0/+0x40/+0x80, cam pos, viewport via 0x9b0820, RT via 0x974dd0),
`0x990020` (cull), `0x9b3a80(1,0,0)`, `0x9b1570(1,0)` (pass type 5 on the RT + draw), `0x9b53a0(1)`,
`0x9b1340` (end; mip blur). **There is no SetEye on the reflection POV**, and `+0x378` stays -1, so the viewport is
the full reflection RT (0x9b0820: `+0x378 == -1` → `{0,0,+0x33c,+0x340}`).
Eye 1 of the main POV is rendered by job 0x9b5dd0 → 0x9b5b70 (`SetEye(1)`, render, restore centre).

### 1.3 Floor shader **[sure]**
From `DATA/ENV/PRODUCTION/BSP_TORRENS/RENDERABLE/LEVEL_SHADERS_DX11_BIN.PAK`: shaders binding
`reflectionSamplerReg_TEX` at **t7** (offsets 0x6108b0, 0x7b8a50, ...; also a `VisorReflectionSamplerReg` variant).
PS at 0x7b8a50 (ps_5_0), reflection lookup:
```
div r0.xy, v6.xyxx, cb2[60].xyxx        ; v6 = SV_Position, cb2 = cbDefaultPSC, reg 60 (+0x3c0) = ScreenResolution
add r0.z, -r0.x, l(1.0)                 ; u = 1 - x/ScreenRes.x   (undoes the X-negated reflection view)
sample_l t7, (u, y/ScreenRes.y), lod r1.y (roughness)
```
No reflection matrix is uploaded; the lookup is **purely screen-space**. ScreenResolution (PS cb2 reg 0x3c) is
written by 0x974f60 (part of the RT/viewport setter 0x974dd0) from the **render-target size** (0xa27b90 on the bound
RT), with the viewport rect `renderer+0xa88..0xa94` only going into the next register. **[likely]**: in VR
ScreenResolution.x = full double-wide width W.

## 2. Where the head pose is, relative to POV[0] **[likely → effectively sure]**
* `GetRelativeHeadPose` 0x9be360 has a single caller, 0x74dea0 in VRCameraBehavior update 0x74de50. The result
  is folded into the behaviour's camera orientation/position (`+0xc..+0x18` quaternion, `+0x1c..` pos), which
  the camera system copies into the main POV view in 0x432300 (via GetMainPOV 0x9b0b40).
* `ApplyEyeTransform` 0x9cd4c0 adds **no rotation**. It builds the per-eye proj from EyeFov(+0xac) and post-multiplies
  the view by `T(ViewAdjust[eye])` (`view = view * T`, 0x9cd6f6). Eye -1 means no offset.
* `POV::SetEye(eye)` 0x9b38d0 (thiscall, ret 4) works **in place** and uses **shared globals**. For `eye == -1` it copies
  view/proj → `0x182f5e0`/`0x182f620` and sets `+0x378 = 0` (so "centre" renders as eye 0). Otherwise it restores
  view/proj from those globals and sets `+0x378 = eye`. Then it calls `ApplyEyeTransform(+0x378, &view, &proj)` (via
  0x9b86a0), 0x9b2fb0, copies proj→+0x80, sets +0xe4/+0xe0, and writes `[[0x17583e4]+0x2c] = +0x378`. It is gated on
  `flags36b & 1` and `& 0x10`.
* POV viewport 0x9b0820: `+0x378 != -1` and stereo → `GetEyeViewportRect(eye, w/2, h)`, i.e. the eye's half.

So POV[0].view (centre) already contains the head rotation, and the reflection camera inherits it,
correctly mirrored (yaw keeps its sign under the floor mirror; pitch and roll flip, as a real mirror camera does).

## 3. Why the symptom follows **[likely]**
Let W×H be the side-by-side RT. MotherVR uses symmetric per-eye frusta with half-tangents `te` (horizontal) and `tev`
(vertical), one eye per half. Let `tc`, `tcv` be the half-tangents of `P_c`, the game's flat camera projection.

* Left-eye pixel at RT x (0..W/2) shows the world direction with tangent `t = (4x/W - 1)·te`.
* The floor at that pixel shows the reflection at `u_c = x/W` of a texture rendered with `P_c` over its full width,
  i.e. the virtual-scene tangent `t' = (2x/W - 1)·tc`.
* Head yaw by Δ shifts all head-relative tangents. On screen the world moves at `(W/2)/(2te)` px per unit tangent,
  the reflection at `W/(2tc)`. Ratio `kx = 2te/tc`; vertically `ky = tev/tcv`. With HMD tangents (~1.2–1.4) vs. the
  flat camera (tcv ≈ 0.6–0.75, tc ≈ tcv·aspect), **kx ≈ ky ≈ 2**.
* With k ≈ 2 the reflection moves on screen at about twice the world's speed, in the same screen direction. Against the
  world-fixed floor, it therefore slides **opposite to the head motion**: turn left → the world moves right on screen, the
  reflection moves right twice as far → it "moves right". Look up → it "moves down".
* Roll φ rotates each eye image about the eye centre. The reflection mapping is a 2x-scaled copy centred on the
  RT centre `(W/2, H/2)`, the inner edge of each eye, with `kx ≠ ky` in general. Rotating through that mapping
  gives a translation that grows with the distance to the wrong centre, plus shear from the anisotropic scale. That is the "strange distortion". The
  two eyes also see non-stereo-consistent reflections (static misregistration of ~30° at each eye's centre).
* On a flat screen `P_c` is the projection actually used and the viewport is the full RT, so everything matches. This
  is why the game is fine without VR.

## 4. Fix proposals

### 4.1 Recommended (real fix): render the reflection per eye into the two RT halves
Key identity (row vectors, `Fx = diag(-1,1,1,1)`, `R` = plane mirror, `a_e` = ViewAdjust of eye e):
```
true reflection view for eye e = R · (V_c · T(a_e)) · Fx = (R · V_c · Fx) · T(Fx·a_e) = V_refl · T(a_{1-e})
```
because OpenVR ViewAdjust is ±IPD/2 in x (y/z equal) **[likely, true for symmetric HMDs]**, and MotherVR's per-eye
projections are symmetric and identical. So **`SetEye(k)` applied to the reflection POV yields eye (1-k)'s exact
reflection view and projection, with viewport = half k**. The shader samples `u = 1 - x/W`, so the left eye
(x ∈ [0, W/2]) reads u ∈ [0.5, 1] = **half 1** and the right eye reads half 0. Within a half, the mapping is
`u = 0.75 - t/(4te)` (left) on both sides, so yaw/pitch/roll and IPD parallax all become correct. Texture density per eye
stays (W/4)×(H/2), half-res per eye as on flat.

**Patch site:** `0x9b5d91` (RVA `0x5b5d91`), original bytes `e8 aa fe ff ff` (`call 0x9b5c40`), the **only** caller
of 0x9b5c40. Replace with `call <hook>` (rel32). Alternatively hook the entry of 0x9b5c40
(`55 8b ec 83 e4 f0 83 ec 2c` = 9 bytes, `push ebp; mov ebp,esp; and esp,-16; sub esp,0x2c`).
ABI: `void __thiscall ReflRender(POV* ecx)`, no stack args, plain `ret`, preserve ebx/esi/edi/ebp.

```cpp
// game functions
using RenderReflFn = void (__thiscall*)(void* pov);           // 0x9b5c40
using SetEyeFn     = void (__thiscall*)(void* pov, int eye);  // 0x9b38d0, ret 4
constexpr uintptr_t kEyeSaveGlobals = 0x182f5e0;              // view[16] @0x182f5e0, proj[16] @0x182f620
// POV fields: +0x000 view, +0x040 proj, +0x36b flags, +0x378 eye (-1 = none)
// renderer current-eye: [[0x17583e4] + 0x2c]

void __fastcall ReflRenderStereo(uint8_t* r, void*) {
    if (!stereo_tracking_active()) { RenderRefl(r); return; }   // [0x1766a2c]->dev && dev->state == 2
    std::array<uint8_t,0x80> savedG; std::memcpy(savedG.data(), (void*)kEyeSaveGlobals, 0x80);
    const uint8_t  savedFlags = r[0x36b];
    int* rendererEye = (int*)(*(uint8_t**)0x17583e4 + 0x2c); const int savedEye = *rendererEye;

    r[0x36b] |= 0x10;            // SetEye also needs bit0, which 0x9b41e0 already set (else 0x9b5c40 is a no-op anyway)
    SetEye(r, -1);               // saves refl centre into the globals, applies eye 0, +0x378 = 0 (left half)
    RenderRefl(r);               // half 0 <- RIGHT eye's reflection
    SetEye(r, 1);                // restores centre from the globals, applies eye 1, +0x378 = 1 (right half)
    RenderRefl(r);               // half 1 <- LEFT eye's reflection   (see the clear caveat)

    std::memcpy(r + 0x00, (void*)kEyeSaveGlobals,        0x40);   // restore centre view
    std::memcpy(r + 0x40, (void*)(kEyeSaveGlobals+0x40), 0x40);   // restore centre proj
    *(int*)(r + 0x378) = -1;  r[0x36b] = savedFlags;  *rendererEye = savedEye;
    std::memcpy((void*)kEyeSaveGlobals, savedG.data(), 0x80);    // main POV's eye-1 pass (0x9b5b70) needs its centre back
}
```
Caveats / to verify at runtime (RenderDoc capture of one frame):
1. **Clear of the reflection RT [guess]**: each 0x9b5c40 call runs `0x9b1570` → `0x995170(5, rt, 1.0, 0, 0)`
   (pass begin on the reflection RT). If that does a full-RT `ClearRenderTargetView`/`ClearDepthStencilView`, the second
   pass wipes half 0. Then the hook must reimplement 0x9b5c40 (15 calls, listed in 1.2) and skip the clear on
   the second pass. The game does the same for its own eye 1: 0x9b5b70 skips 0x9b14a0/0x9b1720, which 0x9b5be0 runs.
2. Mip blur (0x9b1340 → 0x99c8b0) runs twice. That should be harmless (it regenerates mips from mip 0), but blur kernels
   cross the half seam, which is the same seam bleed every SBS effect has. Calling the end step only once, after the second pass, is cleaner.
3. Canted-display HMDs (Pimax, which has non-parallel eyes) break the `Fx·a_e = a_{1-e}` identity only by the cant angle. Fine for v1.
4. Cost: reflection geometry is drawn twice (as for the main view).

### 4.2 Cheaper partial alternative (not recommended)
Change only the reflection proj in 0x9b41e0 (`0x9b436a`: `rep movsd` of local P into `refl+0x40`) to a projection
with horizontal half-tangent `2·te` and vertical `tev`. That fixes the *rate* (kx = ky = 1, so no sliding on
yaw/pitch), but each eye then sees the reflection offset by about `te` (~50°) horizontally, plus roll error. That is visibly worse statically. A
single mono render cannot match two side-by-side eyes through the shader's fixed `x/W` lookup.

### 4.3 Fallback v1: planar reflections off
Equivalent to the user choosing "Planar Reflections: Off". Floors keep the cubemap/env reflection; 0x99b8f0 binds
the default texture `[0x1bdf60c]` and the reflection pass early-outs (0x9b5c62: `[0x175835c]+0x56 && +0x60`). To force it in
code, make reflection-manager `+0x60` always 0:
* `0x99b86d` (RVA 0x59b86d): `8b 40 50` (`mov eax,[eax+0x50]`) → `33 c0 90` (`xor eax,eax; nop`), at init
* `0x99ac01` (RVA 0x59ac01): `8b 40 50` → `33 c0 90`, on setting change
Trade-off: reflective floors lose the mirrored scene (lamps, characters). This has zero risk and zero cost, and it actually saves GPU.
The menu option stays visible but does nothing.

## 5. Address summary (2015 build)
| VA | RVA | Item |
|---|---|---|
| 0x9b41e0 | 0x5b41e0 | ReflectionPOV::Update (builds `R·V·Fx`, copies main proj) |
| 0x9b4640 | 0x5b4640 | its call in POVManager::Update 0x9b45a0 (called 0x7b4119) |
| 0x9b5c40 | 0x5b5c40 | ReflectionPOV render (type 1), mono |
| **0x9b5d91** | **0x5b5d91** | **only call to 0x9b5c40 → hook site** (`e8 aa fe ff ff`) |
| 0x9b38d0 | 0x5b38d0 | POV::SetEye(int) thiscall ret 4 (shared save globals 0x182f5e0/0x182f620) |
| 0x9b5be0 / 0x9b5b70 | | main POV eye 0 / eye 1 render prep |
| 0x9b0820 | 0x5b0820 | POV viewport (eye half when `+0x378 != -1`) |
| 0x99b8f0 | 0x59b8f0 | bind reflection texture to t7 |
| 0x974f60 | 0x574f60 | ScreenResolution (PS cb2 reg 0x3c) upload |
| 0x99b86d, 0x99ac01 | 0x59b86d, 0x59ac01 | setting-0x10 reads (force-off patch) |
