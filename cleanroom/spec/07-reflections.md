<!-- Provenance: static analysis of AI.exe (2015 Steam build, re-checked against the binary for r2: reflection render and its single call, POV::SetEye including its FOV formula, renderer clear prologue, screen-constants upload); 2020 values were taken from the earlier mod's per-build address table (and, for inline sites, its installer) and have not been verified against a 2020 `AI.exe`; analysis of the game's level pixel shaders (LEVEL_SHADERS_DX11_BIN.PAK of the Torrens level); geometric derivation of the per-eye mapping by the spec side; observation in game by the spec side (an earlier mod showed reflections sliding against head motion; the per-eye render required here has not yet been seen running, because in every scene tested the reflection view never had both its active and render flags set). r4: some game facts (the side effects of POV::SetEye, mip generation by quality and the mip count at target creation, the values the graphics settings offer for setting 0x10) were located with the help of an analysis of another mod and verified in the 2015 `AI.exe` and the game's `ENGINE_SETTINGS.XML` by the spec side; the viewport rebuild of §8 item 1 re-read in the 2015 `AI.exe`; requirements of §4.3, §4.6, §6 and §7 restated as results (clean-room review round 2); capture gating aligned with 11 and 13. Section numbers are unchanged from r3. Revision: r4 -->

# 07: Planar reflections in stereo

Reflective floors (and other flat mirrors) are drawn by the game's **planar
reflection** system: a second camera view renders the scene mirrored about
the floor plane into a texture, and the floor's pixel shader reads that
texture in screen space. On a flat screen this is correct. On the
side-by-side stereo target it is not: an earlier mod showed reflections
that slid against head motion and twisted on roll. This document gives
the game facts and what the mod must do so that each eye sees its own correct
reflection.

Addresses are RVAs of the 2015 build (image base 0x400000). 2020 values
were taken from the earlier mod's per-build address table (and, for inline
sites, its installer) and have not been verified against a 2020 `AI.exe`.
Most 2020 addresses this fix needs are not known ("?" below); on that build
the fix stays off (01 §4.7), the log says so once, and reflections render as
the game does.

Confidence: **[sure]** the game code shows it directly; **[likely]** strong
inference; **[guess]** must be confirmed in game.

**Status:** the behaviour of §4 has **not yet been observed in game**: in
the scenes the spec side tested, the reflection view never had both flag
bits 0 and 4 set, so a per-eye render could not run, and neither its result
nor the viewport question (§8 item 1) has been checked. §4 is required
behaviour; §7 says exactly how to check it.

## 1. Game objects

| Object | 2015 | 2020 | Facts |
|---|---|---|---|
| Reflection manager | global pointer at 0x135835C | ? | `+0x04` reflection render target; `+0x56` byte, reflections usable; `+0x58`/`+0x5C` target width/height; `+0x60` quality = graphics setting 0x10 "Planar Reflections" (0 off; the game's `ENGINE_SETTINGS.XML` offers Off = 0 and On = 3); `+0x6C`/`+0x70` candidate reflection planes and their count; `+0x78` byte flag. [sure] |
| POV manager | global pointer at 0x1366A00 | 0x1376BB0 | Owns the POVs (camera views): count byte `+0x88`, POV pointers from `+0x08`. [sure] |
| POV | allocated by the POV manager | | `+0x000` view matrix (4x4 float, row vectors), `+0x040` projection, `+0x080` copy of the projection used for shader constants, `+0x0E0` vertical FOV (radians), `+0x0E4` aspect, `+0x0E8` type (0 main/other, 1 reflection), `+0x33C`/`+0x340` width/height of its render target, `+0x36B` flags (bit 0 active, bit 2 cleared by SetEye, bit 4 renders, bit 7 main view), `+0x378` eye (-1 none; selects the viewport half). [sure] |
| Stereo object | global pointer at 0x1366A2C | 0x1376BDC | `+0x04` stereo device; device `+0x04` state, 2 = stereo running (02). [sure] |
| Renderer eye owner | global pointer at 0x13583E4 | ? | The int at `+0x2C` is the renderer's current eye. [sure] |
| POV eye save area | 0x142F5E0 (view) and 0x142F620 (projection) | ? | Shared by every POV's `SetEye` (§2.4). [sure] |

The reflection POV is created at start-up (RVA 0x599540) with the name
"Reflection Map" and type 1. Its target is created only when setting 0x10 is
not 0, and is recreated when the setting changes (RVA 0x59AB30) and at every
display reset (02 §9.3).

**Target size.** `+0x58`/`+0x5C` are half of the game's back-buffer size
answer (display information at RVA 0x13588B0, `+0x14`/`+0x18`), which in
stereo is the side-by-side size `W x H` of the render-target size query
(02 §6.2). So the reflection target is `W/2 x H/2` and each eye half of it is
`W/4 x H/2`, the same per-eye density as on a flat screen. [sure for the
halving; likely that the display information holds the side-by-side size in
stereo]

## 2. How the game renders a planar reflection

### 2.1 Update: the reflection camera [sure]

`ReflectionPOV update` (0x5B41E0, thiscall, ECX = the reflection POV) runs
once per frame in the game update, from the POV manager update (0x5B45A0;
the call is at 0x5B4640, `e8 9b fb ff ff`, made for every POV of type 1). It
runs after the camera update has written the main view, and before any
render.

- It sets POV flag bit 0 ("active") to whether the reflection manager has
  any candidate plane (`+0x70` > 0), and does nothing more if not.
- It takes the main view (the first POV with flag bit 7): its view `V` and
  projection `P` at that time. These are the **centre** (mono) matrices: `P`
  is the game's flat camera projection, not a per-eye headset projection.
- It picks the best plane for `V·P` and the camera position (0x59B990),
  builds the 4x4 mirror matrix `R` about that plane, and sets the reflection
  view to `R·V` with the X column negated (the image is mirrored left-right
  so that triangle winding is kept).
- It sets the reflection projection to `P`, updates the frustum (§2.5), and
  sets aspect and FOV from `P`.

So the reflection camera already contains the head rotation (it is part of
the main view `V`), correctly mirrored. What it lacks is the eye: it is one
mono camera with the flat projection.

### 2.2 Render: once, mono [sure]

`ReflectionPOV render` (0x5B5C40, thiscall, ECX = POV, no stack arguments,
plain `ret`; prologue `55 8b ec 83 e4 f0 83 ec 2c`) is called from exactly
one place: 0x5B5D91 (`e8 aa fe ff ff`) in the POV manager's render loop
(0x5B5D40), for POVs of type 1. The loop runs in the render job, on the
render thread, before the main view.

It does nothing unless the POV has flag bits 0 and 4 set and the reflection
manager has `+0x56` and `+0x60` non-zero. Otherwise it:

1. begins rendering the POV (0x5B11D0): uploads view, projection and camera
   constants, sets the render target and the viewport (§2.6);
2. culls the scene for this POV (0x590020);
3. begins the pass on the reflection target (0x5B3A80 with 1, 0, 0), which
   **clears the whole target** (§2.3);
4. renders the reflected scene (0x5B1570, then 0x5B53A0);
5. ends the POV (0x5B1340), which for a type-1 POV builds the mip chain
   of the reflection target according to the quality (0x59C8B0): value 2
   uses the device context's mip generation, value 3 the game's blur per
   mip (0x59BD80), any other value builds nothing. The target is created
   with a full mip chain only at quality 2 or 3, and with a single mip
   otherwise (mip count at 0x5995A0).

There is no eye handling: `+0x378` stays -1, so the viewport is the whole
reflection target.

### 2.3 The clear in every render [sure]

Step 3 calls the pass setup (0x595170) and then 0x57B210 with (0, 0, 0),
which ends in the renderer clear 0x621770 with flags 7. The renderer clear is
`cdecl (renderer, flags, ARGB colour, float depth, stencil)`, flags 1
colour, 2 depth, 4 stencil; the caller pops the arguments. Its first six
bytes are `55 8b ec 83 ec 10` (`push ebp; mov ebp,esp; sub esp,0x10`),
position independent. A Direct3D 11 clear ignores the viewport, so this
erases the whole reflection target, both halves, on every render.

### 2.4 POV::SetEye and its side effects [sure]

`POV set eye` (0x5B38D0, thiscall, one int argument, `ret 4`) is the game's
per-eye setup for the main view. It does much more than set matrices:

- clears flag bit 2; does nothing more unless flag bits 0 and 4 are set;
- stores the eye in `+0x378`;
- if stereo is running (device state 2): for eye -1 it **saves** the POV's
  view and projection to the shared save area (§1) and sets `+0x378 = 0`;
  for any other eye it **restores** view and projection from that save
  area. Then it calls ApplyEyeTransform (§2.5) for `+0x378`, updates the
  frustum, sets aspect `+0x0E4 = P[1][1] / P[0][0]` and vertical FOV
  `+0x0E0 = 2·atan(1 / P[1][1])` (constants 1.0 and 2.0 in the code), and
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

- **ApplyEyeTransform** (0x5B86A0, thiscall on the stereo object, arguments
  `int eye, float view[16], float proj[16]`, `ret 0xC`): if the stereo
  device exists and its state is 2, it forwards to device slot 12 (02 §5.1),
  which replaces `proj` with the eye's projection built from the device's
  EyeFov and post-multiplies `view` by the translation by the eye's
  ViewAdjust. It adds no rotation. Otherwise it returns without change.
- **POV frustum update** (0x5B2FB0, thiscall on the POV, no stack
  arguments): recomputes the POV's culling frustum from its view and
  projection.

### 2.6 The viewport and device slot 15 [sure]

The POV viewport (0x5B0820, thiscall, one pointer to a `{left, top, right,
bottom}` int rectangle, `ret 4`):

- `+0x378 == -1`: the whole target, `{0, 0, width, height}` from
  `+0x33C`/`+0x340`;
- otherwise, with a stereo object: the stereo wrapper 0x5B8730 calls
  **device slot 15** with `(eye = +0x378, width / 2, height, &left, &top,
  &right, &bottom)` (thiscall, `ret 0x1C`) when the device state is 2.

The game's own slot 15 (0x5B9000) ignores the width and height it is given
and returns the device's eye rectangle for the **main** stereo target
(device `+0x8C`). For the reflection target, which is half the main size,
eye 1's rectangle then starts beyond the reflection target's right edge. The
rectangle is copied into the renderer (`+0xA88..+0xA94`) and consumers use
`right - left`.

### 2.7 How the floor samples the reflection [sure]

The reflection target is bound to pixel-shader slot **t7** (0x59B8F0; when
reflections are off a default texture is bound instead) for materials with
the reflective flag. In the level shader packs (`LEVEL_SHADERS_DX11_BIN.PAK`,
for example the Torrens level's), floor pixel shaders declare
`reflectionSamplerReg` at t7 and compute the lookup purely from the pixel
position:

- `u = 1 - SV_Position.x / ScreenResolution.x`,
  `v = SV_Position.y / ScreenResolution.y`;
- `ScreenResolution` is pixel constant buffer 2 (`cbDefaultPSC`) register
  60, written by 0x574F50 from the size of the bound render target;
- the sample is a `sample_l` with the level of detail taken from the
  surface roughness.

No reflection matrix is involved. `1 - x` undoes the left-right mirroring of
the reflection view.

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

When stereo is running (device present, state 2) and the reflection POV
would render (flag bits 0 and 4 both set), the reflection must be rendered
**once per eye, into the two halves of the reflection target**, instead of
once for the whole target. In every other case (flat play, no device,
planar reflections off, no plane, either flag bit clear, the fix not
installed) the game's render is called once, unchanged.

### 4.1 Which eye goes in which half

The floor shader reads `u = 1 - x / W`. The left eye's pixels (`x` in
`[0, W/2]`) therefore read `u` in `[0.5, 1]`, the **right half** (half 1);
the right eye reads the **left half** (half 0).

Because the reflection view is mirrored in X, applying eye `k`'s transform
to the reflection camera gives the reflection seen by the **other** eye:
mirroring swaps the sign of the eye's sideways offset. For a headset whose
two eye frusta and eye offsets are mirror images of each other (true for
current symmetric headsets, and the rendered frusta are symmetric and shared
by construction, 03 §3.2), this is exact. (Derivation by the spec side: the
true reflection view for eye `e` is `R·(V·T(a_e))·Fx`, which equals
`(R·V·Fx)·T(Fx·a_e)`, and `Fx·a_e = a_(1-e)` when the eye offsets `a_e` are
±IPD/2 in X; the same mirror argument holds for the projections.)

So: **half k is rendered with eye k's transform and eye k's viewport, and is
sampled by eye 1-k.** Within its half, each eye then sees a reflection with
the correct scale, centre, rotation and parallax. Headsets with canted
displays break the mirror identity only by the cant angle; this is accepted
for v1.

### 4.2 State at each pass

The reflection is rendered in two passes, one per half `k` (0 and 1). Each
pass is one call of the game's reflection render (§2.2). At the moment of
the call for half `k`, this must hold:

| Item | Required value |
|---|---|
| POV view `+0x000` and projection `+0x040` | the centre view and projection the update left this frame (§2.1), transformed for eye `k` exactly as ApplyEyeTransform (§2.5) transforms them; always derived from the centre matrices, never from the other pass's result |
| POV culling frustum | consistent with that view and projection (the frustum update of §2.5) |
| aspect `+0x0E4`, vertical FOV `+0x0E0` | `P[1][1] / P[0][0]` and `2·atan(1 / P[1][1])` of that projection (the game's own formulas, §2.4) |
| projection copy `+0x080` | equal to that projection |
| POV eye `+0x378` | `k`, so the viewport is half `k` (§2.6, §4.4) |
| renderer current eye (eye owner `+0x2C`) | `k`, when the eye owner pointer is not null |
| renderer clear (0x621770) | runs normally in the first pass; in the second it does not erase the first pass's half (§4.3) |

Orders forced by the game: the frustum, aspect, FOV and projection copy are
computed from the eye's transformed matrices, so they follow the transform;
and the pass whose clears run must come before the pass whose clears are
skipped, because each render clears the whole target (§2.3). Which half is
rendered first is free.

`SetEye` (§2.4) must not be called on the reflection POV: its side effects
belong to the main view.

The place to do this is the single call at 0x5B5D91, redirected to a
function with the same ABI (thiscall, POV in ECX, no stack arguments, plain
`ret`, callee-saved registers preserved).

### 4.3 The second pass keeps the first pass's half

The second pass must leave the half the first pass drew intact: the
reflection render's own clear of the reflection target (§2.3) must not take
effect during it. The game's clear in the first pass already empties both
halves, so nothing else needs clearing. Making every renderer clear
(0x621770) during the second pass return without clearing meets this, with
the caveat of open question 2; a narrower skip that still keeps the first
half is also acceptable. Outside the second pass every clear runs normally,
on every thread.

The mechanism is the implementation's choice. Game facts it can rely on: the
clear is `cdecl`, so a return without popping its arguments is correct; its
first six bytes (`55 8b ec 83 ec 10`) are whole instructions and position
independent; during the passes it is called on the render thread. Any
executable memory the mechanism uses must not stay writable and executable.

### 4.4 Slot 15 honours the target size

Device slot 15 must return the rectangle for the size it is given, not for
the main target: eye 1 `{halfWidth, 0, 2·halfWidth, height}`, any other eye
`{0, 0, halfWidth, height}` (02 §5). For the main stereo target this gives
the same rectangles as the device's eye viewports (03 §4), so the main view
is unaffected.

### 4.5 State restored

After both passes, these hold their values from before the first pass:
the POV's view, projection,
projection copy `+0x080`, FOV `+0x0E0`, aspect `+0x0E4` and eye `+0x378`,
and the renderer's current eye. The shared save area (§1) must not be
touched: the main view's eye-1 pass relies on it.

### 4.6 Not required

- The mip chain (with reflections on, the quality-3 blur of §2.2, since
  the settings offer only 0 and 3) is built at the end of each pass over
  the whole target, so at quality 3 the blur runs twice and crosses the
  seam between the halves by a few texels. This is acceptable. Building it
  only once, after the second pass, is allowed.
- Each eye must see a reflection rendered for its own eye position: the
  two eyes' reflections must differ by the parallax of the mirrored scene,
  so that the reflection appears below the floor at the mirrored depth. A
  single image shared by both eyes does not meet this.
- Switching planar reflections off in VR is not acceptable as the fix (the
  game's own Rift preset did; the mod must not, 02 §6.5).

## 5. Installation, checks and fallback

At load (with the rest of the patch set, 01 §2.3), on the 2015 build only,
both sites are verified before anything is written:

- the call site 0x5B5D91 holds `e8` whose target resolves to 0x5B5C40;
- the renderer clear 0x621770 starts with `55 8b ec 83 ec 10`.

| Call site | Clear prologue | Result |
|---|---|---|
| as expected | as expected | the fix is installed; the installation is logged once (§6) |
| differs | any | nothing is written for reflections; the game's mono render runs; the log says the fix is off and that the call-site check failed |
| as expected | differs | nothing is written for reflections (a per-eye render without the clear skip would erase half of the target); the game's mono render runs; the log says the fix is off and that the clear check failed |

The order of the writes is the implementation's, with one constraint: the
clear skip of §4.3 is in place before the redirected call can be reached,
because a per-eye render without it would erase the first pass's half.

On the 2020 build, or any build whose addresses are unknown, the fix is not
installed; the game's mono render runs, and the player can switch "Planar
Reflections" off in the game's options to avoid the wrong reflection (floors
then keep their environment-map reflection).

## 6. Diagnostics, threads and cost

**Logged events** (11-diagnostics owns the wording and format; each event
must convey what is listed):

- the installation result (§5): installed, or not installed and which check
  failed, or that the build's addresses are unknown;
- each new reflection POV and each change of its flag byte `+0x36B`: which
  POV, its new flags and the device state. This is the event that shows
  whether a scene has an active reflection (§7 step 3). Bounded so a run
  cannot flood the log; the bounds are the implementation's choice;
- the first per-eye render, once per run.

Nothing else is logged per frame.

**Reflection capture** (optional diagnostic, only while `DebugOptions` is
on and the per-eye render is in effect, as in 11 §4.4 and 13 §3.5; with
`DebugOptions` off the key does nothing and nothing is logged): on a key
press (the key is the implementation's choice), after each
of the two passes of that frame, save the render target bound at that moment
to a DDS file in the game directory, one file per pass, named so that it is
clear which pass it holds (names are the implementation's choice; 13 lists
the files the mod writes). Log each file's size and format, or that no
target was bound. It is one way to make the check of §7 step 5.

**Threads.** The reflection render, the per-eye function and the clear all
run on the render thread. The device state is read there with an atomic
load (02 §9.1). Whatever state decides that a clear is skipped is
written and read on the render thread only; a clear on another thread is
never skipped.

**Cost** (not yet measured, [likely]): pixel work about the same as the mono
render (two half viewports of the same target instead of one full one);
draw submission (CPU) for the reflection doubles, because the scene is culled
and drawn once per eye; the mip chain is built twice. Measure in a scene
with a reflective floor before optimising.

## 7. Acceptance: checking on the Torrens at the start of a new game

The Torrens level's shader pack contains floor shaders that sample the
reflection at t7 [sure], so its reflective floors are the first place to
check; whether its planes register at run time (reflection POV flag bit 0)
is [likely] and is exactly what step 3 shows.

1. Set "Planar Reflections" to on (any value other than Off) in the game's
   graphics options, and the VR runtime to OpenVR or OpenXR. Restart if the
   runtime changed.
2. Start a **New Game**. After the opening sequence the player has control
   aboard the Torrens. Walk into a lit corridor or room with a glossy floor
   and look down at the floor where lights, doorframes or Ripley's
   surroundings should be mirrored. Stand still for a few seconds.
3. Read the log: there must be a reflection-POV flags event whose new flags
   have both bit 0 (active) and bit 4 (renders) set, with device state 2, and
   the one-time event of the first per-eye render (§6). If no event ever shows both
   bits while on the Torrens floors, the scene has no active plane: record
   where this was tried and repeat on other glossy floors later in the game
   (the fix is still unverified).
4. In the headset, with both eyes: turn the head left and right, look up and
   down, and roll the head. The reflection must stay fixed to the floor (no
   sliding against head motion, no shear on roll) and fuse at the depth of
   the mirrored scene, below the floor. Closing each eye in turn, each eye's
   reflection must line up with the objects it mirrors.
5. While looking at the floor, inspect the reflection target's content
   after each pass, by any means that shows it (for example the optional
   capture of §6 with `DebugOptions` on). After both passes, half 0 holds
   the right eye's reflection and half 1 the left eye's (§4.1), and
   neither has been erased. If half 1 is not drawn where expected, the
   viewport origin is lost (§8 item 1).
6. Switch Planar Reflections off in the options: floors fall back to their
   environment reflection, and no further flags events with bit 4 appear.
7. With the runtime None, the game's render runs once per frame as in the
   unmodded game (no per-eye event in the log).

## 8. Open questions

1. **Viewport origin [guess].** The pass setup (0x595170) for some pass
   types rebuilds the viewport from the renderer rectangle as `{0, 0,
   right - left, bottom - top}` (0x574EE0), which would drop the left offset
   of half 1 (0x574EE0 is called from within 0x595170 and writes that
   rectangle from the renderer's `+0xA88..+0xA94`; re-read in the 2015
   `AI.exe`). Resolve with §7 step 5. If half 1 is not drawn where
   expected, the per-eye render must draw only into half k, and every
   full-screen pass inside the reflection render (the mip blur among them)
   must read and write only half k. Where to enforce this is the
   implementation's choice.
2. **Clears of intermediate targets in the second pass [guess].** Skipping
   every renderer clear during the second pass (§4.3) includes any clear
   of the mip blur's intermediate targets ("Reflection Mip Blur Target",
   "Intermediate Reflection Mip Blur Target", both named in the blur
   0x59BD80). If the blur fully overwrites them this is harmless. Resolve
   by comparing the target's mip levels after the second pass with those
   after the first at quality 3; if stale content shows, restrict the skip
   to clears of the reflection target itself.
3. **Which scenes have planes.** Where in the game the reflection POV first
   becomes active is unknown; resolve with §7 step 3 and record the
   location here.
4. **2020 addresses.** Every address of §1 to §5 except the stereo object
   and POV manager pointers is unknown for 2020. Resolve by locating the
   "Reflection Map" string reference and the type-1 render call in a 2020
   `AI.exe`, then checking the bytes given here.
