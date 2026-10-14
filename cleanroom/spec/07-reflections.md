<!-- Provenance: static analysis of AI.exe (2015 Steam build, verified against the binary) and of the game's level pixel shaders; geometric derivation of the per-eye mapping by the spec side; in-game testing of an earlier rebuild (the per-eye render has not yet been seen running: the tested scene had no planar reflection surface); MotherVR 0.8.1 behaviour observed in game. Revision: r1 -->

# 07: Planar reflections in stereo

Reflective floors (and other flat mirrors) are drawn by the game's
**planar reflection** system: a second camera view renders the scene
mirrored about the floor plane into a texture, and the floor's pixel shader
reads that texture in screen space. On a flat screen this is correct. On
the side-by-side stereo target it is not, and MotherVR 0.8.1 left it
broken: reflections slid against head motion and twisted on roll. This
document gives the game facts and what the mod must do so that each eye
sees its own correct reflection.

Addresses are RVAs of the 2015 build (image base 0x400000). The 2020
addresses are not known; on that build the fix stays off (01 §4.7) and
reflections render as the game does.

Confidence: **[sure]** the game code shows it directly; **[likely]** strong
inference; **[guess]** must be confirmed in game.

**Status:** the required behaviour of §4 is implemented in the rebuild but
**not yet observed in game**: in the scene tested so far the reflection
render never ran (no reflection plane was active), so neither the result nor
the viewport question of §5 has been checked. Treat §4 as required, and §5
as the list of things to confirm in the first scene that has a reflective
floor.

## 1. Game objects

| Object | Where | Facts |
|---|---|---|
| Reflection manager | global pointer at 0x135835C | `+0x04` reflection render target; `+0x56` byte, reflections usable; `+0x58`/`+0x5C` target width/height = half the main render resolution; `+0x60` quality = graphics setting 0x10 "Planar Reflections" (0 off; the menu offers 0 and 3); `+0x6C`/`+0x70` candidate reflection planes and their count; `+0x78` byte flag. [sure] |
| POV manager | global pointer at 0x1366A00 | Owns the POVs (camera views): count byte `+0x88`, POV pointers from `+0x08`. [sure] |
| POV | allocated by the POV manager | `+0x000` view matrix (4x4 float, row vectors), `+0x040` projection, `+0x080` copy of the projection used for shader constants, `+0x0E0` vertical FOV (radians), `+0x0E4` aspect, `+0x0E8` type (0 main/other, 1 reflection), `+0x33C`/`+0x340` width/height of its render target, `+0x36B` flags (bit 0 active, bit 2 cleared by SetEye, bit 4 renders, bit 7 main view), `+0x378` eye (-1 none; selects the viewport half). [sure] |
| Stereo object | global pointer at 0x1366A2C | `+0x04` stereo device; device `+0x04` state, 2 = tracking running (02). [sure] |
| Renderer eye | global pointer at 0x13583E4 | The int at `+0x2C` is the renderer's current eye. [sure] |
| POV eye save area | 0x142F5E0 (view) and 0x142F620 (projection) | Shared by every POV's `SetEye` (§2.4). [sure] |

The reflection POV is created at start-up with the name "Reflection Map"
and type 1. Its target is created only when setting 0x10 is not 0.

## 2. How the game renders a planar reflection

### 2.1 Update: the reflection camera [sure]

`ReflectionPOV update` (0x5B41E0, thiscall, ecx = the reflection POV) runs
once per frame in the game update, from the POV manager update
(0x5B45A0; the call is at 0x5B4640, made for every POV of type 1). It runs
after the camera update has written the main view, and before any render.

- It sets POV flag bit 0 ("active") to whether the reflection manager has
  any candidate plane (`+0x70` > 0), and does nothing more if not.
- It takes the main view (the first POV with flag bit 7): its view `V` and
  projection `P` at that time. These are the **centre** (mono) matrices:
  `P` is the game's flat camera projection, not a per-eye headset
  projection.
- It picks the best plane for `V·P` and the camera position
  (0x59B990), builds the 4x4 mirror matrix `R` about that plane, and sets
  the reflection view to `R·V` with the X column negated (the image is
  mirrored left-right so that triangle winding is kept).
- It sets the reflection projection to `P`, updates the frustum (§2.5),
  and sets aspect and FOV from `P`.

So the reflection camera already contains the head rotation (it is part of
the main view `V`), correctly mirrored. What it lacks is the eye: it is one
mono camera with the flat projection.

### 2.2 Render: once, mono [sure]

`ReflectionPOV render` (0x5B5C40, thiscall, ecx = POV, no stack arguments,
plain `ret`; prologue `55 8b ec 83 e4 f0 83 ec 2c`) is called from exactly
one place: 0x5B5D91 (`e8 aa fe ff ff`) in the POV manager's render loop
(0x5B5D40), for POVs of type 1. The loop runs in the render job before the
main view.

It does nothing unless the POV has flag bits 0 and 4 set and the reflection
manager has `+0x56` and `+0x60` non-zero. Otherwise it:

1. begins rendering the POV (0x5B11D0): uploads view, projection and camera
   constants, sets the render target and the viewport (§2.6);
2. culls the scene for this POV (0x590020);
3. begins the pass on the reflection target (0x5B3A80 with 1, 0, 0), which
   **clears the whole target** (§2.3);
4. renders the reflected scene (0x5B1570, then 0x5B53A0);
5. ends the POV (0x5B1340), which builds the mip chain of the reflection
   target according to the quality: value 2 uses hardware mip generation,
   value 3 a Gaussian blur per mip, any other value gives a single mip and
   no blur.

There is no eye handling: `+0x378` stays -1, so the viewport is the whole
reflection target.

### 2.3 The clear in every render [sure]

Step 3 calls the pass setup (0x595170) and then 0x57B210 with (0, 0, 0),
which ends in the renderer clear 0x621770 with flags 7. The renderer clear
is `cdecl (renderer, flags, ARGB colour, float depth, stencil)`, flags
1 colour, 2 depth, 4 stencil; the caller pops the arguments. Its first six
bytes are `55 8b ec 83 ec 10` (`push ebp; mov ebp,esp; sub esp,0x10`),
position independent. A Direct3D 11 clear ignores the viewport, so this
erases the whole reflection target, both halves, on every render.

### 2.4 POV::SetEye and its side effects [sure]

`POV set eye` (0x5B38D0, thiscall, one int argument, `ret 4`) is the
game's per-eye setup for the main view. It does much more than set
matrices:

- clears flag bit 2; does nothing more unless flag bits 0 and 4 are set;
- stores the eye in `+0x378`;
- if stereo tracking is running: for eye -1 it **saves** the POV's view and
  projection to the shared save area (§1) and sets `+0x378 = 0`; for any
  other eye it **restores** view and projection from that save area. Then
  it calls ApplyEyeTransform (§2.5) for `+0x378`, updates the frustum, sets
  aspect `+0x0E4 = P[1][1] / P[0][0]` and FOV `+0x0E0` from `P[1][1]`, and
  copies the projection to `+0x080`;
- always writes `+0x378` to the renderer's current eye;
- for the main view (bit 7) it also updates the post-processing camera
  matrices (0x5F6000);
- for eye ≠ 1 it updates the light lists (0x5E7370), **resets the
  reflection manager's plane count to 0** (0x59AB10), sets reflection
  manager `+0x78 = 1`, and makes this POV the lighting system's current
  view (0x5BDE30 on the object at global 0x1366A44);
- for every eye it runs the POV's begin-render (0x5B11D0) again.

Eye 1 of the main view is prepared by 0x5B5B70, which calls `SetEye(1)`,
renders, and restores the centre matrices from the shared save area. So the
save area must still hold the main view's centre when that runs.

These side effects are meant for the main view once per frame. Calling
`SetEye` on the reflection POV overwrites the main view's saved centre,
switches the lighting system to the reflection POV and empties the
reflection plane list. It must not be used for the reflection.

### 2.5 ApplyEyeTransform and the frustum [sure]

- **ApplyEyeTransform** (0x5B86A0, thiscall on the stereo object,
  arguments `int eye, float view[16], float proj[16]`, `ret 0xC`): if the
  stereo device exists and its state is 2, it forwards to device slot 12
  (02), which replaces `proj` with the eye's projection built from the
  device's EyeFov and post-multiplies `view` by the translation by the
  eye's ViewAdjust. It adds no rotation. Otherwise it returns without
  change.
- **POV frustum update** (0x5B2FB0, thiscall on the POV, no stack
  arguments): recomputes the POV's culling frustum from its view and
  projection.

### 2.6 The viewport and device slot 15 [sure]

The POV viewport (0x5B0820, thiscall, one pointer to a
`{left, top, right, bottom}` int rectangle, `ret 4`):

- `+0x378 == -1`: the whole target, `{0, 0, width, height}` from
  `+0x33C`/`+0x340`;
- otherwise, with a stereo object: the stereo wrapper 0x5B8730 calls
  **device slot 15** with `(eye = +0x378, width / 2, height, &left, &top,
  &right, &bottom)` (thiscall, `ret 0x1C`) when the device state is 2.

The game's own slot 15 (0x5B9000) ignores the width and height it is given
and returns the device's eye rectangle for the **main** stereo target
(device `+0x8C`). For the reflection target, which is half the main size,
eye 1's rectangle then starts beyond the reflection target's right edge.
The rectangle is copied into the renderer (`+0xA88..+0xA94`) and consumers
use `right - left`.

### 2.7 How the floor samples the reflection [sure]

The reflection target is bound to pixel-shader slot **t7** (0x59B8F0; when
reflections are off a default texture is bound instead) for materials with
the reflective flag. In the level shader packs
(`LEVEL_SHADERS_DX11_BIN.PAK`), floor pixel shaders declare
`reflectionSamplerReg` at t7 and compute the lookup purely from the pixel
position:

- `u = 1 - SV_Position.x / ScreenResolution.x`,
  `v = SV_Position.y / ScreenResolution.y`;
- `ScreenResolution` is pixel constant buffer 2 (`cbDefaultPSC`) register
  60, written by 0x574F50 from the size of the bound render target;
- the sample is a `sample_l` with the level of detail taken from the
  surface roughness.

No reflection matrix is involved. `1 - x` undoes the left-right mirroring
of the reflection view.

## 3. Why it breaks in stereo [likely]

In stereo the bound target is the side-by-side target `W x H`, so
`ScreenResolution.x = W`. The reflection is one mono image taken with the
flat camera's projection, but each eye covers only half of `W` and uses a
much wider per-eye field of view. For each eye, screen position maps to
reflection direction with the wrong scale (about 2x) and the wrong centre
(the reflection's centre lies on the seam between the eyes). When the head
turns, the reflection moves on screen about twice as fast as the world, so
against the floor it slides opposite to the head motion; on roll it rotates
about the wrong centre with unequal horizontal and vertical scale, which
shows as shear. The two eyes also see reflections that do not fuse. On a
flat screen the projection and the full-target viewport match, so the game
is correct there.

## 4. Required behaviour

When stereo tracking is running and the reflection POV would render (flag
bits 0 and 4 set), the reflection must be rendered **once per eye, into the
two halves of the reflection target**, instead of once for the whole
target. Elsewhere (flat play, no planar reflection, setting off) the game's
render runs unchanged.

### 4.1 Which eye goes in which half

The floor shader reads `u = 1 - x / W`. The left eye's pixels
(`x` in `[0, W/2]`) therefore read `u` in `[0.5, 1]`, the **right half**
(half 1); the right eye reads the **left half** (half 0).

Because the reflection view is mirrored in X, applying eye `k`'s transform
to the reflection camera gives the reflection seen by the **other** eye:
mirroring swaps the sign of the eye's sideways offset. For a headset whose
two eye frusta and eye offsets are mirror images of each other (true for
current symmetric headsets), this is exact. (Derivation by the spec side:
the true reflection view for eye `e` is `R·(V·T(a_e))·Fx`, which equals
`(R·V·Fx)·T(Fx·a_e)`, and `Fx·a_e = a_(1-e)` when the eye offsets `a_e` are
±IPD/2 in X; the same mirror argument holds for the projections.)

So: **half k is rendered with eye k's transform and eye k's viewport, and is
sampled by eye 1-k.** Within its half, each eye then sees a reflection with
the correct scale, centre, rotation and parallax. Each half is
`(W/4) x (H/2)`, the same per-eye density as on a flat screen. Headsets with
canted displays break the mirror identity only by the cant angle; this is
accepted for v1.

### 4.2 One pass, for k = 0 then 1

1. Start from the reflection POV's centre view and projection as the
   update (§2.1) left them.
2. Apply eye `k`'s transform with ApplyEyeTransform (§2.5) to the POV's
   view and projection.
3. Update the frustum (§2.5), set aspect `P[1][1] / P[0][0]` and the
   vertical FOV `2·atan(1 / P[1][1])` [likely for the FOV formula], copy the
   projection to `+0x080`.
4. Set `+0x378 = k`, so that the viewport (§2.6) selects half k, and set the
   renderer's current eye to `k`.
5. Call the game's reflection render (§2.2).

This is the matrix part of `SetEye` without any of its side effects
(§2.4); `SetEye` itself must not be called on the reflection POV.

The natural place to do this is the single call at 0x5B5D91, which can be
redirected to a function with the same ABI (thiscall, POV in ecx, no stack
arguments, plain `ret`, callee-saved registers preserved).

### 4.3 Clears skipped on the second pass

The clear in every render (§2.3) would erase half 0 during the second pass.
During the **second** pass (k = 1) every renderer clear (0x621770) must be
skipped; the first pass's clear has already cleared the whole target. Any
mechanism is acceptable (for example a gate at the clear's entry that
returns at once while a flag is set; the clear is cdecl, so returning
without popping is correct). The flag must be off outside that pass. If the
clear's first six bytes are not the expected ones, the whole per-eye
reflection fix stays off and the game's mono render is used.

### 4.4 Slot 15 honours the target size

Device slot 15 must return the rectangle for the size it is given, not for
the main target: eye 0 `{0, 0, halfWidth, height}`, eye 1
`{halfWidth, 0, 2·halfWidth, height}`. For the main stereo target this
gives the same rectangles as the device's eye viewports (03 §4), so the
main view is unaffected.

### 4.5 State restored

After both passes, restore what was changed: the POV's view, projection,
projection copy `+0x080`, FOV `+0x0E0`, aspect `+0x0E4` and eye `+0x378`,
and the renderer's current eye; recompute the frustum from the restored
matrices. The shared save area (§1) must not be touched. The main view's
eye-1 pass relies on it.

### 4.6 Not required

- The mip chain is built at the end of each pass over the whole target, so
  it runs twice and the blur crosses the seam between the halves by a few
  texels. This is acceptable. Building it only once, after the second pass,
  is allowed.
- Rendering the reflection once and making both eyes read the same half
  (a shader change) is not acceptable: it removes the reflection's stereo
  parallax, so the reflection reads as being at infinity.

## 5. To confirm in game

The first scene with an active reflective floor must show:

1. The log shows the per-eye path running (the reflection POV has flag bits
   0 and 4 set while stereo runs).
2. A capture of the reflection target after the first pass holds content in
   the left half only, and after the second pass both halves hold content
   (half 0 is not erased).
3. **Viewport origin [guess].** The pass setup (0x595170) for some pass
   types rebuilds the viewport from the renderer rectangle as
   `{0, 0, right - left, bottom - top}` (0x574EE0), which would drop the
   left offset of half 1. If the second pass lands in the left half, the
   viewport offset must be enforced for the reflection passes (for example
   at the renderer's viewport setter, 0x621350, `cdecl (context,
   int vp[6] = {x, y, w, h, minZ, maxZ})`), and the vertex-shader screen-UV
   constant (constant buffer 1, register 21, `{vpW/rtW, vpH/rtH,
   vpLeft/rtW, vpTop/rtH}`, uploaded by 0x574F50) must describe the same
   half, so that full-screen passes inside the reflection render, such as
   the mip blur, stay within it.
4. In the headset, with the head turning, pitching and rolling, the
   reflection stays fixed to the floor, and the two eyes fuse it at the
   depth of the mirrored scene.

## 6. Fallback

If any site does not match, or on a build whose addresses are not known,
the game's own mono render runs. The player can switch "Planar Reflections"
off in the game's options to avoid the wrong reflection; floors then keep
their environment-map reflection.
