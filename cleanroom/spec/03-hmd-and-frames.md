<!-- Provenance: game analysis of AI.exe 2015 (device methods 11-16, relative head pose, render-target size query), OpenVR public API semantics, observation in game (test logs, fixes recorded in the earlier reconstruction's history: full resolution from the start, eye layout tied to the created texture, windowed swapchain, recenter when worn and when gameplay starts), behaviour of the reference mod as observed; DXGI public API semantics (fullscreen transitions); answers to implementation questions Q1, Q3, Q4 (ANSWERS.md). Revision: r2 -->

# 03 - Head tracking, projection, render targets, frames, virtual screen, mirror

This document specifies what the stereo device (02) does with the headset
runtime. Runtime calls are named by their public OpenVR API names
(`IVRSystem::...`, `IVRCompositor::...`, `IVROverlay::...`). Section numbers
of 02 are written "02 §n". The same behaviour on OpenXR is in 09, which
names the OpenXR equivalent of each OpenVR step used here.

Parameters used throughout:

| Symbol | Meaning | Source |
|---|---|---|
| `s` | world scale (metres scale), default 1.0 | command line `-vrscale <value>` only, accepted in [0.495, 1.505], otherwise 1.0 (06 §2). Never stored, never in the menu |
| `p` | pixel scale, default 1.0, range 0.5 to 4.0 | config file and options menu only (06 §1); no command-line override |
| eye 0 / eye 1 | the game's eye indices | eye 0 is the **left** eye: left half of the target, submitted as `Eye_Left`; eye 1 is the right eye |

## 1. Head pose convention (device slot 11)

The game consumes the head pose as a 3x4 row-major float matrix (12 floats,
row r = `m[r][0..3]`, column 3 = translation), produced by device slot 11 and
compared against the reference pose at manager +0x78 (02 §6.3).

Take the headset pose from `IVRCompositor::GetLastPoses`, element
`k_unTrackedDeviceIndex_Hmd`, field `mDeviceToAbsoluteTracking`, in the
tracking universe the application runs in (the compositor default,
`TrackingUniverseStanding`; the mod does not change it). Call its rotation
part `R` (3x3) and its translation `t` (metres). OpenVR space is
right-handed, +Y up, -Z forward.

The game expects, for each row r = 0, 1, 2:

    out[r] = ( -R[r][0], -R[r][1], +R[r][2], -s * t[r] )

That is: the head's local X and Y axes (columns 0 and 1) are negated, which
is a 180 degree roll about the head's own Z axis; the translation is negated
and multiplied by the world scale. Rows stay in OpenVR axis order. This
statement of the convention is derived from the requirement (it is what the
game's camera code needs to move the camera the way the head moves; verified
in game), not taken from any implementation.

Consequences:

- the "neutral" pose (no rotation, no translation) is rows
  (-1,0,0,0), (0,-1,0,0), (0,0,1,0); the manager's reference pose must start
  as this (02 §3.3);
- world scale scales positions only, never rotations;
- slot 11 also records that the game asked for a head-tracked pose this
  frame (section 6).

## 2. Reference (recenter) pose

### 2.1 What it is

The reference pose at manager +0x78 defines "forward" and the origin of
positional tracking: the game expresses the head pose relative to it (02
§6.3). It must be **yaw-only**:

1. from the same headset pose as in section 1, compute the heading
   `yaw = atan2(R[0][2], R[2][2])`;
2. build `Y`: rotation about +Y by `yaw`, i.e. rows
   `(cos yaw, 0, sin yaw)`, `(0, 1, 0)`, `(-sin yaw, 0, cos yaw)`, with the
   headset's full translation `t` (position is kept, pitch and roll are
   dropped);
3. convert `Y` with the formula of section 1 and write it to manager +0x78.

Keep the unconverted `Y` (OpenVR space, unscaled): it also places the virtual
screen (section 7).

Pitch and roll must be dropped so that recentering while looking down or
with the head tilted does not tilt the game world.

Taking the reference must not mark the frame as head-tracked (section 6);
this is why the recenter call sites must not use device slot 11 itself
(02 §6.4).

### 2.2 When to take it

| Trigger | Behaviour |
|---|---|
| CreateDevice succeeds (02 §6.1) | take it immediately; if the headset is not in use (2.3), also queue a retake |
| a queued retake | executed on the first per-frame update (04/05) where the headset is in use; until then the old reference stays |
| a loading screen closes (05) | queue a retake, so gameplay starts facing the way the player faces |
| manual recenter: pad LB+RB, keyboard Ctrl+Alt (04) | take it at once |
| the game's reset-orientation path (02 §6.4, RVA 0x5B8680) | take it at once |

After any recenter the menu's virtual screen in the game world is placed
again (05).

Background: taking the reference only at launch fails when the game is
started with the headset lying on the desk: the menus and the game's
forward then point wherever the headset was facing. Retaking it once the
headset is worn, and again when gameplay starts, fixes this without making
the menus follow the head.

### 2.3 "Headset in use"

The headset counts as in use when both hold:

- `IVRSystem::GetTrackedDeviceActivityLevel(k_unTrackedDeviceIndex_Hmd)`
  returns `k_EDeviceActivityLevel_UserInteraction`;
- the headset is upright: the world-Y component of its local up axis,
  `R[1][1]`, exceeds 0.7 (within about 45 degrees of vertical).

Logging each recenter (trigger, heading, position, activity level) is
recommended; it is the main tool for diagnosing "forward is wrong" reports.

## 3. Field of view

### 3.1 Per-eye tangents

For each eye e, `IVRSystem::GetProjectionRaw(eye, &l, &r, &t, &b)` returns
tangents in OpenVR's convention, where `t` is normally negative and `b`
positive. In image terms (D3D textures, v = 0 at the top) the extents are:

| Extent | Value |
|---|---|
| up (towards v = 0) | `up_e = b` |
| down | `down_e = -t` |
| left | `left_e = -l` |
| right | `right_e = r` |

(`b` is the upward extent: with OpenVR's projection composition, NDC y = +1,
the top of a D3D image, corresponds to the tangent `b`.) All four are
positive for real headsets.

### 3.2 The rendered FOV: symmetric, shared by both eyes

    maxV = max(up_0, down_0, up_1, down_1)
    maxH = max(left_0, right_0, left_1, right_1)

Both eyes are rendered with the symmetric FOV `{Up = maxV, Down = maxV,
Left = maxH, Right = maxH}`. Write it to device EyeFov (+0xAC, both eyes)
and to the descriptor's MaxEyeFov (+0x5C). Write each eye's real tangents to
DefaultEyeFov (+0x3C) as `{up_e, down_e, left_e, right_e}`.

The game's projection (02 §5.1) covers exactly these tangents in each eye's
half of the target. Symmetric frusta are required because game code that
only receives a vertical FOV and an aspect from slot 13 assumes a centred
frustum (likely; not every consumer was traced). The game's projection
itself would accept asymmetric tangents.

The pixel aspect of an eye half need not equal `maxH / maxV`; slot 12 sets
the X and Y scale from EyeFov, so the image is not distorted, only sampled
at slightly different densities horizontally and vertically. Sizing the
target to make pixels square is an allowed improvement.

### 3.3 Crop bounds per eye

Each eye's real FOV is a sub-rectangle of the symmetric image. For the
compositor (`vr::VRTextureBounds_t`, in UV of the whole side-by-side
texture, v = 0 at the top), eye e:

    uMin = 0.5 * e + 0.5 * (0.5 - 0.5 * left_e  / maxH)
    uMax = 0.5 * e + 0.5 * (0.5 + 0.5 * right_e / maxH)
    vMin = 0.5 - 0.5 * up_e   / maxV
    vMax = 0.5 + 0.5 * down_e / maxV

(The formulas are the standard ones for presenting a symmetric render of an
asymmetric eye; stated here because the result must match exactly.) The
desktop mirror's cropped mode uses the same bounds (section 8).

## 4. Render-target size and eye layout

### 4.1 The computed size

    (w, h) = IVRSystem::GetRecommendedRenderTargetSize()
    eye size = (trunc(w * p), trunc(h * p))
    W = 2 * trunc(w * p),  H = trunc(h * p)

Eye viewports (device +0x8C, `{x, y, w, h}`): eye 0 `{0, 0, W/2, H}`,
eye 1 `{W/2, 0, W/2, H}`. Render-target size at device +0x1EC / +0x1F0.

### 4.2 Full resolution from the first answer

The size must use the configured pixel scale from the **very first** query.
The game sizes its stereo render target and its swapchain back buffer from
the first answer to the render-target size query (02 §6.2), which happens
before stereo starts, and it does not rebuild them when stereo starts. An
answer at a reduced scale until the sensor starts (a behaviour of the
reference) leaves each eye rendering at that reduced resolution for the
whole session.

A changed pixel scale therefore takes effect at the next start.

### 4.3 The layout follows the texture actually created

When the game configures rendering (device slot 3), take the stereo
texture's real size from `ID3D11Texture2D::GetDesc` and set
+0x1EC/+0x1F0 and both viewports from it. Keep that layout until the game
configures a texture of another size. A size computed later (for example
after a pixel-scale change) must not move the viewports while the old
texture is in use; otherwise each eye shows a different slice of the image.

Size answers:

- the render-target size query (02 §6.2) answers the computed size (4.1);
- device slot 16 answers the configured texture's size once one exists,
  otherwise the computed size. It is called several times per frame and
  must stay cheap.

### 4.4 Slot 3 (configure rendering) in full

1. Look up the stereo target's texture and shader resource view through the
   renderer accessors (02 §8) with the index at +0x80 of the argument
   (manager +0x08); a null argument keeps the previous ones.
2. Set the eye layout from the texture (4.3); fill the two texture
   descriptors at +0x13C / +0x178 (API 5, size, viewport, texture, view).
3. Create the runtime's render resources (the virtual-screen overlay,
   section 7) once; repeated configuration must not create duplicates.
4. Create the mirror's resources (section 8).
5. Return true.

Slot 4 (shut down rendering) releases what step 3 created.

### 4.5 Keep the swapchain windowed

With a headset attached the back buffer is the side-by-side size (for
example 5376x2688), larger than any display mode, so a fullscreen swapchain
cannot be created or switched to. When the game creates its device and
swapchain (`D3D11CreateDeviceAndSwapChain`) while the stereo device is
attached (CreateDevice runs first, 02 §6.1), the mod must set
`DXGI_SWAP_CHAIN_DESC::Windowed = TRUE`. The window is only the desktop
mirror; DXGI scales the back buffer to its client area, whose size is the
window size recorded in 02 §6.2.

The swapchain must also **stay** windowed while the stereo device exists.
A later switch to fullscreen, whether the game asks for it (its fullscreen
video option calls `IDXGISwapChain::SetFullscreenState`) or DXGI's own
Alt+Enter handling does, must be refused:

- `IDXGISwapChain::SetFullscreenState` with `Fullscreen = TRUE` on the game's
  swapchain returns `DXGI_ERROR_NOT_CURRENTLY_AVAILABLE` without changing
  state (the documented result when a fullscreen transition cannot be made,
  which applications must already handle). `Fullscreen = FALSE` passes
  through.
- DXGI's automatic Alt+Enter toggle is switched off for the game window
  (`IDXGIFactory::MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER)`, on
  the factory that created the swapchain, obtained with `GetParent`).
- `IDXGISwapChain::ResizeTarget` passes through: in windowed mode it only
  resizes the window, which the mirror follows (section 8).
- With no stereo device (runtime None, or the runtime failed to start) none
  of this applies: the game's fullscreen option works as in the unmodded
  game.

[guess] How the game reacts to the refusal is not known: it may keep
showing "Fullscreen" in its video options. That is acceptable; the window
stays a working mirror. Confirm in game with the game's fullscreen option
on at start and switched on while stereo runs.

## 5. Eye offsets (ViewAdjust)

At every begin frame (slot 5), read both eyes' offsets from the head with
`IVRSystem::GetEyeToHeadTransform(Eye_Left / Eye_Right)` (translation column
only; the rotation part is ignored) and write:

| Device field | Value |
|---|---|
| ViewAdjust of eye 0 (+0x0F8) | `s *` translation of `Eye_Right` |
| ViewAdjust of eye 1 (+0x130) | `s *` translation of `Eye_Left` |

Verified in game: on a headset with 66 mm IPD this gives eye 0
(+0.033, 0, 0) and eye 1 (-0.033, 0, 0), and depth is correct. The cross-over
is not a mistake: ViewAdjust moves view-space points (02 §5.1), so the
left eye needs `+IPD/2` in X. For headsets whose eyes are mirror images in X
this equals `s * (-x, -y, +z)` of the eye's own offset, which is the
derivation from the game's view convention (X right, Y up, Z forward); the
two differ only in the sign of a vertical offset, which real headsets do not
have.

Reading them every frame keeps a changed IPD in effect at once.

## 6. Frame timing

The game drives one frame as: (game update, which asks slot 11 for the head
pose if the camera is head tracked) -> begin frame (slot 5, then 7, 7) ->
render both eyes -> end frame (8, 8, then slot 6).

| Step | Required behaviour |
|---|---|
| slot 11 | `IVRCompositor::GetLastPoses`, convert (section 1), set the "head pose requested" flag |
| slot 5 | poll controllers (04); refresh eye offsets (section 5); this frame is **head-tracked** if the flag is set, otherwise it is a **screen frame** |
| slot 6 | head-tracked: hide the virtual-screen overlay if shown, then `IVRCompositor::Submit(Eye_Left, ...)` and `Submit(Eye_Right, ...)`. Screen frame: section 7. Then `IVRCompositor::WaitGetPoses(nullptr, 0, nullptr, 0)`; then draw the mirror (section 8); then `IDXGISwapChain::Present(0, 0)` on the game's swapchain; then clear the flag |

Submit details:

- texture: the stereo target texture (device +0x158), `TextureType_DirectX`,
  `ColorSpace_Gamma` (the game writes display-ready values into a UNORM
  target);
- bounds: the per-eye crop bounds of section 3.3;
- flags: `Submit_TextureWithPose`, with the headset pose the frame was
  rendered with (the result of the last `GetLastPoses`).

`WaitGetPoses` after both submits blocks until the compositor wants the next
frame and makes the poses for it available to `GetLastPoses`; the next
frame's head pose is therefore fresh when the game asks for it. The desktop
Present uses sync interval 0 so that it never adds a second wait; frame
pacing comes from `WaitGetPoses`.

The game does not present by itself while stereo runs (02 §5, slot 6): if
slot 6 does not call Present, the desktop window freezes.

Runtime start-up (CreateDevice, 02 §6.1): `VR_Init(VRApplication_Scene)`,
then wait (bounded) until `IVRCompositor` is available and until
`IVRCompositor::WaitGetPoses` delivers a valid headset pose. On failure,
there is no device and the game runs flat; tell the user why (01, 06).

## 7. The virtual screen for screen frames

Screen frames are those where the game did not ask for a head-tracked pose:
menus, the title video, loading screens, cutscenes. Their camera ignores the
head, so they are shown on a flat, world-locked screen instead of as stereo
3D.

Each screen frame:

| Property | Value |
|---|---|
| mechanism | an `IVROverlay` overlay created in slot 3 (`IVROverlay::CreateOverlay`, key and name of the mod's choosing), destroyed in slot 4 |
| texture | `IVROverlay::SetOverlayTexture`: the stereo target texture, `TextureType_DirectX`, `ColorSpace_Gamma` |
| part of the texture | `IVROverlay::SetOverlayTextureBounds` `{uMin 0.0, vMin 0.25, uMax 0.5, vMax 0.75}`: eye 0's half, middle half vertically |
| width | `IVROverlay::SetOverlayWidthInMeters` 32/9 m (3.5556 m); height follows the region's pixel aspect |
| placement | `IVROverlay::SetOverlayTransformAbsolute` in `TrackingUniverseStanding`: the unconverted yaw-only matrix `Y` of the latest recenter (section 2.1), with its translation moved 1.5 m along `Y`'s forward axis (minus its third column) in X and Z only. The screen centre is 1.5 m in front of where the head was at the recenter, at that head height, facing it, level |
| visibility | `IVROverlay::ShowOverlay`; hidden again with `IVROverlay::HideOverlay` on the first head-tracked frame |
| eye layers | the headset shows black behind the screen. Both eyes are still submitted every frame (the compositor expects them), with content that is black (the reference submitted the eye texture with bounds `{0, 0, 1e-5, 1e-5}`, one texel that is black in these frames) |

The screen is world-locked: it moves only when the reference pose is retaken,
never with head motion.

At 1.5 m a 3.5556 m wide screen spans a half-angle tangent of about 1.19,
close to a typical eye's horizontal FOV, so the image appears at roughly the
angular size it was rendered for. The game's own menu quad (05) sits inside
the middle half of the frame, which the bounds keep.

## 8. Desktop mirror

Drawn in slot 6 after the submit, into the game's back buffer (W x H), which
DXGI then scales to the window (4.5). All placement is computed for the
**window's** aspect `A = window width / window height`, using the window size
recorded in 02 §6.2; because the scaling to the window is uniform per axis,
fractions of the back buffer equal fractions of the window.

### 8.1 What is shown

Settings (06): **mirror view** = both eyes (0), right eye (1, default) or
left eye (2); **symmetric** = on (default) or off.

Head-tracked frames show the eye image(s):

| Mirror view | Symmetric on: source region (texture UV) | Symmetric off: source region |
|---|---|---|
| left eye | `u 0..0.5, v 0..1` | eye 0 crop bounds (3.3) |
| right eye | `u 0.5..1, v 0..1` | eye 1 crop bounds |
| both eyes | `u 0..1, v 0..1` | eye 0 crop on the left half of the draw area, eye 1 crop on the right half |

Screen frames show what the virtual screen shows: the region
`u 0..0.5, v 0.25..0.75`, whole (letterboxed, never cropped). The reference
instead sampled whichever texture the game had left bound at pixel-shader
slot 0, assuming 16:9 content; that depends on game state and is not
required.

### 8.2 Fitting

The source region has pixel aspect
`a = (region width in UV * W) / (region height in UV * H)`.

"Both eyes" with symmetric off uses two crops whose vertical extents can
differ (each eye's `vMin`/`vMax` come from its own tangents, 3.3). Both
crops then use one **common vertical range**, the union of the two:
`v0 = min(vMin_0, vMin_1)`, `v1 = max(vMax_0, vMax_1)`; each keeps its own
horizontal range. So the two halves of the draw area show the same rows of
the target (a point at the horizon is at the same height in both), and
neither crop is stretched. For this case
`a = ((uMax_0 - uMin_0) + (uMax_1 - uMin_1)) * W / ((v1 - v0) * H)`, and eye
0's crop fills the left half of the draw area, eye 1's crop the right half,
each half as wide as its crop's share of the summed width. On headsets
whose eyes are mirror images (all current ones) the two vertical ranges are
equal and the union changes nothing.

- Eye images fill the window width. Their draw height, in units of the
  window height, is `k = A / a`, centred vertically: `k < 1` leaves black
  bars above and below, `k > 1` crops the top and bottom equally. (A single
  near-square eye in a 16:9 window is therefore cropped top and bottom.)
- The screen-frame image is fitted whole: centred, black bars on the sides
  or above and below as needed.
- Outside the draw area: black.

### 8.3 Shader behaviour (for new HLSL)

- **Vertex stage**: no vertex buffer or input layout. A 4-vertex triangle
  strip generated from the vertex id: `uv = (id & 1, id >> 1)`, clip
  position `(2 * uv.x - 1, 1 - 2 * uv.y, 0, 1)`, `uv` passed to the pixel
  stage. It covers the viewport, which is set to the whole back buffer.
- **Pixel stage**: constants give a draw rectangle in viewport UV (`d0`,
  `d1`) and a source rectangle in texture UV (`s0`, `s1`). If `uv` is inside
  the draw rectangle, sample the stereo texture's shader resource view
  (device +0x15C) at `s0 + (uv - d0) / (d1 - d0) * (s1 - s0)` with a linear,
  clamping sampler and output it with alpha 1; otherwise output opaque black.
  "Both eyes, cropped" uses two such rectangle pairs (two draws, or a branch
  on which half `uv` is in).
- State: no blending, default rasterizer state, the back buffer's render
  target view bound, viewport = full back buffer. Restoring the game's
  bindings afterwards is not needed (Present follows), but every reference
  to a view obtained from the context must be released.

The constants change only when the window size, texture, setting or frame
kind changes; recomputing them per frame is also fine.

## 9. Pixel scale

`p` multiplies the runtime's recommended per-eye size (4.1). Default 1.0,
range 0.5 to 4.0, set in the options (06). Larger values sharpen the image
at a GPU cost. Because the game creates its targets once (4.2), a change
applies after a restart; the options UI should say so.
