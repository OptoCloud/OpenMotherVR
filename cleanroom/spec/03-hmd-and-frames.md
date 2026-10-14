<!-- Provenance: game analysis of AI.exe 2015 (device methods 3 to 16, relative head pose, render-target size query, render-target rebuild flag, renderer accessors, order of renderer initialisation); OpenVR, DXGI, Win32 and LibOVR 0.4 public API semantics (fullscreen transitions, window placement, interface versions, DK2 descriptor values); observation in game by the spec side on SteamVR with a Valve Index (full resolution from the start, eye layout tied to the created texture, windowed swapchain, mirror back buffer at window size, mirror window fitted to the monitor, recenter when worn and when gameplay starts, running start with pose age 0 over 600 frames, render-target sizes up to 13440x6720 and the 16384 texture limit, pixel-scale changes applied in the same run through the rebuild flag, head motion moving the camera correctly); performance measurements in game (mirror and back-buffer cost, controller latency); answers to implementation questions Q1, Q3, Q4 (ANSWERS.md); user requirements and project decisions (SteamVR need not be running, invalid poses, bounded start-up waits). 2020 values were taken from the earlier mod's per-build address table (and, for inline sites, its installer) and have not been verified against a 2020 `AI.exe`. Revision: r4 -->

# 03 - Head tracking, projection, render targets, frames, virtual screen, mirror

This document specifies what the stereo device (02) does with the headset
runtime on OpenVR, and how the mod treats the game's swapchain and window
while a headset is attached (for both runtimes). Runtime calls are named by
their public OpenVR API names (`IVRSystem::...`, `IVRCompositor::...`,
`IVROverlay::...`). Section numbers of other documents are written "02 §n".
The same behaviour on OpenXR is in 09, which names the OpenXR equivalent of
each OpenVR step used here; §4, §8 and §9 apply to both runtimes.
2020 values were taken from the earlier mod's per-build address table (and,
for inline sites, its installer) and have not been verified against a 2020
`AI.exe`.

Parameters used throughout:

| Symbol | Meaning | Source |
|---|---|---|
| `s` | world scale (metres scale), default 1.0 | command line `-vrscale <value>` only, read once when the stereo device is first created, accepted in [0.495, 1.505], otherwise 1.0 (06 §2). Never stored, never in the menu |
| `p` | pixel scale, default 1.0, range 0.5 to 4.0 | config file and options menu only (06 §1); no command-line override. The effective value is capped (§4.1) |
| eye 0 / eye 1 | the game's eye indices | eye 0 is the **left** eye: left half of the target, submitted as `Eye_Left`; eye 1 is the right eye |
| `W`, `H` | the side-by-side render target's size | §4 |
| `A` | the desktop window's aspect, width / height of the size the game asked for (§4.1, §8) | §8 |

Threads (game facts, observed in test logs [sure]): the game has a **game
thread** (simulation, camera, the per-frame stereo update of 05 §3, window
messages; it also runs renderer initialisation, CreateDevice and swapchain
creation) and a **render thread** (device slots 3 to 9 and 12 to 16, the
swapchain's `Present` and `ResizeBuffers`). The game thread asks for the head
pose (device slot 11) once per frame, before the render thread begins that
frame; it was never seen calling slot 11 between a frame's slot 5 and slot
6 (0 of 600 frames, both runtimes). Job threads run the culling code of 08
and read the device state. §11 lists what crosses threads.

## 1. Head pose convention (device slot 11)

The game consumes the head pose as a 3x4 row-major float matrix (12 floats,
row r = `m[r][0..3]`, column 3 = translation), produced by device slot 11 and
compared against the reference pose at manager +0x78 (02 §6.3). Slot 11 is
`thiscall(device, float* out)`, `ret 4`, returns `out` in EAX (02 §5).

Take the headset pose from `IVRCompositor::GetLastPoses` (all tracked
devices, or at least `k_unTrackedDeviceIndex_Hmd`), element
`k_unTrackedDeviceIndex_Hmd`, field `mDeviceToAbsoluteTracking`, in the
tracking universe the application runs in (the compositor default,
`TrackingUniverseStanding`; the mod does not change it). This is the pose
the last `WaitGetPoses` (§6) predicted for the frame the game is about to
render. Call its rotation part `R` (3x3) and its translation `t` (metres).
OpenVR space is right-handed, +Y up, -Z forward.

The game expects, for each row r = 0, 1, 2:

    out[r] = ( -R[r][0], -R[r][1], +R[r][2], -s * t[r] )

That is: the head's local X and Y axes (columns 0 and 1) are negated, which
is a 180 degree roll about the head's own Z axis; the translation is negated
and multiplied by the world scale. Rows stay in OpenVR axis order. This is
the convention the game's camera code consumes (verified in game: head
motion moves the camera correctly). [sure]

Invalid poses: when the headset's pose is not valid (`bPoseIsValid` of its
`TrackedDevicePose_t` is false; tracking lost, sensors covered, runtime
still starting), slot 11 returns the last valid converted pose, or the
neutral pose (below) if there has been none. An invalid matrix is never
converted or returned, and nothing slot 11 returns contains NaN. When the
pose is valid but the runtime reports that position tracking is degraded
(`eTrackingResult` other than `TrackingResult_Running_OK`), the new
orientation is used with the last valid position (project decision, the
same rule as 09 §5.5: freezing the whole view while the head turns is worse
for comfort than holding the position).

Consequences:

- the "neutral" pose (no rotation, no translation) is rows
  (-1,0,0,0), (0,-1,0,0), (0,0,1,0); the manager's reference pose must start
  as this (02 §3.3);
- world scale scales positions only, never rotations;
- slot 11 also records that the game asked for a head-tracked pose this
  frame (§6), and the pose it returned is the pose that frame is submitted
  with (§6.3).

## 2. Reference (recenter) pose

### 2.1 What it is

The reference pose at manager +0x78 defines "forward" and the origin of
positional tracking: the game expresses the head pose relative to it (02
§6.3). It must be **yaw-only**:

- source: the latest headset pose as in §1 (`GetLastPoses`), and only
  when it is valid (`bPoseIsValid`);
- heading: `yaw = atan2(R[0][2], R[2][2])`;
- `Y` is the rotation about +Y by `yaw`, rows `(cos yaw, 0, sin yaw)`,
  `(0, 1, 0)`, `(-sin yaw, 0, cos yaw)`, with the headset's full
  translation `t` (position is kept, pitch and roll are dropped);
- manager +0x78 holds `Y` converted with the formula of §1.

Keep the unconverted `Y` (OpenVR space, unscaled): it also places the virtual
screen (§7).

Pitch and roll must be dropped so that recentering while looking down or
with the head tilted does not tilt the game world.

Looking straight up or down: when the horizontal length of the head's
forward axis, `sqrt(R[0][2]^2 + R[2][2]^2)`, is below 1e-4, the heading is
undefined; the previous reference's yaw is kept (0 before any reference),
with the new translation. A recenter never produces NaN or an arbitrary
heading. (1e-4 is a project decision: it is within about 0.006 degrees of
vertical, which no worn headset holds, and well above float rounding.)

Invalid pose: a recenter is never taken from an invalid pose. An immediate
recenter (§2.2) that meets an invalid pose keeps the old reference and
leaves a queued retake, which runs on the first frame with a valid pose
while the headset is in use.

Taking the reference must not mark the frame as head-tracked (§6);
this is why the recenter call sites must not use device slot 11 itself
(02 §6.4). Each recenter is logged once (11 §2.4) with what is needed to
diagnose "forward is wrong" reports: the head orientation and position it
was taken from, the resulting heading, and whether the headset was in use
(§2.3).

### 2.2 When to take it

| Trigger | Behaviour |
|---|---|
| the first CreateDevice that attaches a device (02 §6.1) | take it immediately; if the headset is not in use (§2.3) at that moment, also queue a retake |
| a queued retake | executed by the per-frame stereo update on the game thread (05 §3.2) on the first frame where the headset is in use and its pose is valid; until then the old reference stays. The update runs only while the device state is 2 |
| a loading screen closes (05 §5) | queue a retake, so gameplay starts facing the way the player faces |
| manual recenter: pad LB+RB, keyboard left Ctrl + left Alt (04 §8) | take it at once (once per press) |
| the game's reset-orientation path (02 §6.4, RVA 0x5B8680) | take it at once |

A queued retake is a single flag: queuing it again before it runs changes
nothing. A later CreateDevice (display reset, §6.5) does not take or queue
a reference. Taking the headset off and putting it on again later does not
by itself queue a retake; only the triggers above do.

After any recenter the menu's virtual screen in the game world is placed
again (05 §4.2), and the runtime-side virtual screen (§7) follows the new
`Y`.

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
  `R[1][1]` of the latest valid headset pose read by slot 11 or by a recenter,
  exceeds 0.7 (within about 45 degrees of vertical).

The test runs on the game thread.

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
positive for real headsets. [sure]

### 3.2 The rendered FOV: symmetric, shared by both eyes

    maxV = max(up_0, down_0, up_1, down_1)
    maxH = max(left_0, right_0, left_1, right_1)

Both eyes are rendered with the symmetric FOV `{Up = maxV, Down = maxV,
Left = maxH, Right = maxH}`. Write it to device EyeFov (+0xAC, both eyes)
and to the descriptor's MaxEyeFov (+0x5C, both eyes). Write each eye's real
tangents to DefaultEyeFov (+0x3C) as `{up_e, down_e, left_e, right_e}`. The
game reads EyeFov only (slots 12 to 14, 02 §5.1); MaxEyeFov and
DefaultEyeFov are read only by the mod (the crops of §3.3 and §8), so the
order inside DefaultEyeFov matters only to the mirror on a vertically
asymmetric headset. The order is the game's FovPort convention, `{Up, Down,
Left, Right}` with Up the tangent towards the top of the image: slot 14
turns a FovPort into an NDC scale and offset with the LibOVR formula, whose
vertical offset is `(Up - Down) / (Up + Down)` towards +Y (the top), and
slot 12 builds the vertical FOV as `atan(Up) + atan(Down)` (02 §5.1)
[sure for the formula; the game only ever receives symmetric EyeFov, so
the sign was not observed in an image]. With OpenVR's raw tangents that is
Up = `b`, Down = `-t` (§3.1). Storing `-t` as Up is wrong.

Also fill, at CreateDevice: ProductName (+0x18) with the headset's
`Prop_ModelNumber_String` and Manufacturer (+0x1C) with its
`Prop_ManufacturerName_String` (strings that stay valid for the process),
and the rest of the descriptor header (02 §4) as a LibOVR 0.4 DK2 would
report it, using the public LibOVR 0.4 values:

| Field | Value | Meaning (LibOVR 0.4) |
|---|---|---|
| Handle (+0x10) | 0 | no LibOVR handle |
| Type (+0x14) | 6 | `ovrHmd_DK2` |
| HmdCaps (+0x20) | 0x83 | `ovrHmdCap_Present` \| `ovrHmdCap_Available` \| `ovrHmdCap_LowPersistence` |
| SensorCaps (+0x24) | 0x70 | `ovrTrackingCap_Orientation` \| `ovrTrackingCap_MagYawCorrection` \| `ovrTrackingCap_Position` |
| DistortionCaps (+0x28) | 2 | `ovrDistortionCap_TimeWarp` |

None of these five is known to be read by the game outside its own device
methods (02 §4); any consistent DK2 values would do, and these are written
so that the descriptor describes a plausible headset of the API the game
was built for. Log the manufacturer, model and the two render tangents
(11 §2.4).

The game's projection (02 §5.1) covers exactly these tangents in each eye's
half of the target. Symmetric frusta are required because game code that
only receives a vertical FOV and an aspect from slot 13 assumes a centred
frustum [likely; not every consumer was traced]. The game's projection
itself would accept asymmetric tangents.

The pixel aspect of an eye half need not equal `maxH / maxV`; slot 12 sets
the X and Y scale from EyeFov, so the image is not distorted, only sampled
at slightly different densities horizontally and vertically. Sizing the
target to make pixels square is not done in v1 (the size is the runtime's
recommendation, §4.1).

Observed on Index (2688x2688 per eye recommended): render tangents of about
`v = 1.21`, `h = 1.15`.

### 3.3 Crop bounds per eye

Each eye's real FOV is a sub-rectangle of the symmetric image. For the
compositor (`vr::VRTextureBounds_t`, in UV of the whole side-by-side
texture, v = 0 at the top), eye e:

    uMin = 0.5 * e + 0.5 * (0.5 - 0.5 * left_e  / maxH)
    uMax = 0.5 * e + 0.5 * (0.5 + 0.5 * right_e / maxH)
    vMin = 0.5 - 0.5 * up_e   / maxV
    vMax = 0.5 + 0.5 * down_e / maxV

(The formulas are the standard ones for presenting a symmetric render of an
asymmetric eye; stated here because the result must match exactly.) They
are computed once, at CreateDevice. The desktop mirror's cropped mode uses
the same bounds (§8).

## 4. Render-target size and eye layout

### 4.1 The computed size

    (w, h) = IVRSystem::GetRecommendedRenderTargetSize()      (per eye)
    pmax   = min(16384 / (2 * w), 16384 / h)
    p'     = min(p, pmax)                                     (effective pixel scale)
    eye size = (trunc(w * p'), trunc(h * p'))
    W = 2 * trunc(w * p'),  H = trunc(h * p')

(Products and quotients in single-precision float; `trunc` towards zero.)
The cap keeps the side-by-side target inside Direct3D 11's 16384 texel
limit: without it, a pixel scale of about 3.05 or more on a 2688-wide eye
makes the game's target creation fail (observed: a session ramped up to
13440x6720 before failing). [sure]

Eye viewports (device +0x8C, `{x, y, w, h}`): eye 0 `{0, 0, W/2, H}`,
eye 1 `{W/2, 0, W/2, H}` (integer division). Render-target size at device
+0x1EC / +0x1F0.

Log each new `W x H` once (11 §2.4), with the effective pixel scale (so a
capped value is visible) and whether a texture of another size is still in
use.

### 4.2 Full resolution from the first answer

The size must use the configured pixel scale from the **very first** query,
in every device state. The game builds its stereo render target from device
slot 16's answer inside its configure-rendering wrapper (02 §6.7), first at
start-up in state 1, before stereo starts, and does not rebuild it when
stereo starts. An answer at a reduced scale before stereo starts therefore
leaves each eye rendering at that reduced resolution for the whole session.
[sure]

The game asks again, and recreates its stereo target and back buffer, at
every display reset (02 §9.3): when the byte `RenderTargetResizePending`
(RVA 0x1358470, 2020: 0x1368620) is 1 after an end frame (resolution
change, window restore, some display settings), and when the mod raises it
because the pixel scale or a mirror setting changed (06 §4). A changed pixel
scale therefore takes effect in the same run, without a restart, at that
reset (observed: a pixel-scale change in game produced the new target size
and a new stereo texture in the same session). [sure for the 2015 build]

### 4.3 The layout follows the texture actually created

When the game configures rendering (device slot 3), take the stereo
texture's real size from `ID3D11Texture2D::GetDesc` and set
+0x1EC/+0x1F0 and both viewports from it (§4.1 layout with `W` = texture
width, `H` = texture height). Keep that layout until the game configures a
texture of another size. A size computed later (for example after a
pixel-scale change, before the rebuild) must not move the viewports while
the old texture is in use; otherwise each eye shows a different slice of the
image.

Size answers:

- the render-target size query (02 §6.2): first record the incoming
  `*w, *h` (the window size the game derived from its resolution setting;
  the desktop mirror and the back buffer use it, §8, §9), then write the
  computed `W, H` (§4.1). With no device attached, leave both values and
  record nothing;
- device slot 16 (`thiscall(device, int* w, int* h)`, `ret 8`): always
  writes the computed `W, H` (02 §7.2: with per-eye sizes taken separately,
  `W` is the sum of the two eye widths and `H` the larger of the two eye
  heights; all current headsets report equal eyes).
  It must not answer the configured texture's size instead: the game builds
  the next stereo target from this answer, so a reset would never pick up a
  new pixel scale. It updates +0x1EC/+0x1F0 and the viewports only while no
  texture has been configured yet, or when the computed size equals the
  configured texture's size. It is called several times per frame (by the
  game's slots 12 and 13) and must stay cheap (no allocation, no logging
  except on a change of the computed size).

Slots 12 and 13 use slot 16's answer only for the aspect `(w / 2) / h`,
which is the same for the computed size and the configured texture's size
up to rounding.

### 4.4 Slot 3 (configure rendering): required results

Slot 3 is `thiscall(device, void* stereoTarget)`, `ret 4`, returns a bool in
AL (true). It runs on the render thread, at start-up after the swapchain
exists and again after every render-target rebuild (§4.2). When it returns:

| State | Required value |
|---|---|
| stereo texture and its shader resource view | those of the argument's stereo target, found through the renderer accessors (02 §8) with the 16-bit index at +0x80 of the argument (manager +0x08); a null argument keeps the previous ones |
| eye layout (+0x1EC/+0x1F0, both viewports) | from the texture's real size (§4.3), when a texture is known |
| eye 0 descriptor (+0x13C) and eye 1 descriptor (+0x178) | API id 5, the layout's size, the eye's own viewport, the texture and view above |
| runtime render resources | exist exactly once, however often slot 3 runs. OpenVR: the virtual-screen overlay (§7). OpenXR: the session (09 §2). If OpenXR cannot create its session, the device is detached and VR is off for this run (09 §7); the call in progress still leaves the other rows of this table as stated |
| mirror (§8) | its resources exist and its mapping matches the new layout and the recorded window size |
| log | the side-by-side size and the texture in use (11 §2.4) |

The layout is derived from the texture, and the mirror mapping from the
layout, so each needs the one before it.

Slot 4 (shut down rendering, no arguments, returns true) destroys the
overlay (OpenVR); OpenXR keeps its session.

## 5. Eye offsets (ViewAdjust)

At every begin frame (slot 5), read both eyes' offsets from the head with
`IVRSystem::GetEyeToHeadTransform(Eye_Left / Eye_Right)` (translation column
only; the rotation part is ignored) and write:

| Device field | Value |
|---|---|
| ViewAdjust of eye 0 (+0x0F8) | `s *` translation of `Eye_Right` |
| ViewAdjust of eye 1 (+0x130) | `s *` translation of `Eye_Left` |

Verified in game: on a headset with 66 mm IPD this gives eye 0
(+0.033, 0, 0) and eye 1 (-0.033, 0, 0), and depth is correct. [sure] The
cross-over is not a mistake: ViewAdjust moves view-space points (02 §5.1),
so the left eye needs `+IPD/2` in X. For headsets whose eyes are mirror
images in X this equals `s * (-x, -y, +z)` of the eye's own offset, which is
the derivation from the game's view convention (X right, Y up, Z forward);
the two differ only in the sign of a vertical offset, which real headsets
do not have.

Reading them every frame keeps a changed IPD in effect at once. Log the two
ViewAdjust vectors on the first begin frame only (11 §2.4), precise enough
to tell millimetres apart.

## 6. Frame timing

### 6.1 The game's frame

The game drives one frame as:

1. game thread: game update, which asks slot 11 for the head pose if the
   camera is head tracked (first-person gameplay; not in menus, videos,
   loading screens or cutscenes);
2. render thread: begin frame (slot 5, then slot 7 for eye 0 and eye 1);
3. render thread: render both eyes into the side-by-side target;
4. render thread: end frame (slot 8 for eye 0 and eye 1, then slot 6).

Slots 5 and 6 take no arguments and return nothing (`ret`); slots 7 and 8
take `int eye` (`ret 4`) and do nothing. Slot 10 ("device presents") returns
true, so the game's present step leaves presenting to slot 6 (02 §6.8).

### 6.2 What each slot does (OpenVR)

| Step | Required behaviour |
|---|---|
| slot 11 (game thread) | `IVRCompositor::GetLastPoses`, convert (§1; the last valid pose when this one is invalid), set the "head pose requested" flag, keep the pose as this frame's render pose |
| slot 5 (render thread) | if no controller state was handed to the game since the last slot 5 (before the first frame, or a frame that submitted nothing), read the controllers now (04 §5.9); refresh eye offsets (§5); this frame is **head-tracked** if the "head pose requested" flag is set, otherwise it is a **screen frame** |
| slot 6 (render thread) | if no stereo texture is known yet, skip to the mirror. Head-tracked: hide the virtual-screen overlay if it is visible, then `IVRCompositor::Submit(Eye_Left, ...)` and `Submit(Eye_Right, ...)`. Screen frame: §7. Then `IVRCompositor::WaitGetPoses(nullptr, 0, nullptr, 0)`; then read the controllers and hand them to the game (04 §5.9); then draw the mirror (§8); then `IDXGISwapChain::Present(0, 0)` on the game's swapchain (accessor, 02 §8); then clear the "head pose requested" flag |

The order inside slot 6 is required where the API or the game needs it:
both submits before `WaitGetPoses` (the compositor takes the frame, then
paces the next one: the running start of §6.3); the controllers after
`WaitGetPoses` (so they belong to the next frame's prediction); the mirror
after the submits and before `Present` (it draws into the back buffer that
`Present` shows); the flag cleared after the mirror, which uses it.

Submit details:

- texture: the stereo target texture (device +0x158), `TextureType_DirectX`,
  `ColorSpace_Gamma` (the game writes display-ready values into a UNORM
  target);
- bounds: the per-eye crop bounds of §3.3;
- flags: `Submit_TextureWithPose`, with `mDeviceToAbsoluteTracking` set to
  the headset pose the frame was rendered with (the render pose of slot 11).

### 6.3 Running start: which pose the game renders with

`WaitGetPoses` after both submits blocks until the compositor wants the next
frame and makes the poses predicted for that frame's display available to
`GetLastPoses`. The game thread's next slot 11 therefore reads a pose
predicted for the display time of the very frame it is about to render, and
that frame is submitted with that same pose. Observed: in 600 of 600 frames
the rendered pose was predicted for the frame it was displayed in (pose age
0). [sure]

Controller state is read right after `WaitGetPoses`, so the game's next
simulation step uses controller data of the same prediction as the head
(04 §5.9). [sure]

The desktop Present uses sync interval 0 so that it never adds a second
wait; frame pacing comes from `WaitGetPoses`. The game does not present by
itself while stereo runs (02 §5, slot 6): if slot 6 does not call Present,
the desktop window freezes.

No allocation, file I/O or logging per frame on the render thread; logging
only on a change, or at a bounded periodic rate behind `DebugOptions`
(11 §3).

### 6.4 Runtime start-up (CreateDevice, OpenVR)

CreateDevice is the manager's `Init(int type)` (02 §6.1, `thiscall`,
`ret 4`), called on the game thread during renderer initialisation, before
the render-target size query and before the D3D11 device exists. For
`type == 1`, and only the first time it is called, the OpenVR start-up
must meet these requirements (the runtime chosen per 06 §1, logged per
11 §2.3):

| Topic | Requirement |
|---|---|
| finding the runtime | the OpenVR runtime root is the first string of the `"runtime"` array in `%LOCALAPPDATA%\openvr\openvrpaths.vrpath` (JSON, written by SteamVR), usable when `<root>\bin\win32\openvr_api.dll` exists |
| the loader | `openvr_api.dll`, x86, from a public OpenVR SDK release that serves the interface versions below: the copy deployed next to the game (13), else `<root>\bin\win32\openvr_api.dll`. Exports used, all `cdecl`: `VR_InitInternal2`, `VR_ShutdownInternal`, `VR_GetGenericInterface`, `VR_GetVRInitErrorAsEnglishDescription`; any missing is a failure |
| DLL search path | while the loader loads and while `VR_InitInternal2` runs, `<root>\bin\win32` is on the DLL search path, because the loader loads `vrclient.dll` and its dependencies from there (without it the result is `VRInitError_Init_VRClientDLLNotFound`, 102); the previous search path is restored afterwards. When `vrclient.dll` or one of its dependencies cannot be loaded, the log identifies which file is missing |
| SteamVR need not be running | `VR_InitInternal2(&err, VRApplication_Scene, nullptr)` starts SteamVR if it is not running; the mod makes no check for a SteamVR process beforehand. An init error (including no headset, such as `VRInitError_Init_HmdNotFound`) is a failure, reported with the runtime's English description and the error number |
| interface versions | `IVRSystem_017`, `IVRCompositor_021` and `IVROverlay_016` (OpenVR SDK 1.0.10), or later versions of the same interfaces when the implementation is built against a later SDK; every call used here exists in all of them. Obtained with `VR_GetGenericInterface`. A missing interface is a start-up failure with the OpenVR error code and says which interface was missing. The versions obtained are logged once |
| bounded wait | on a cold start SteamVR may serve the compositor (and report the headset) only some seconds after `VR_InitInternal2` returns. Waiting for the compositor interface and then for a valid headset pose (`bPoseIsValid` of the headset's pose from `WaitGetPoses`) is bounded to about 10 s in total (project decision: long enough for a cold SteamVR start on current PCs, short enough that the game does not appear hung). No compositor within the bound is a failure; no valid pose within it is not (§1 covers invalid poses, and the reference is then queued, §2.1) |
| logged once | the recommended per-eye size and the IPD (right eye X minus left eye X of `GetEyeToHeadTransform`) |
| on success | the descriptor and EyeFov filled (§3.2), device state 1, the device attached at manager +0x04, the reference taken (§2.2) |

Order is required only where the API forces it: the loader is loaded
before `VR_InitInternal2`, the runtime is initialised before its
interfaces are obtained, and the compositor exists before `WaitGetPoses`;
the device is attached last, so the game never sees a half-filled device.

Failure: shut the runtime down if it was initialised, leave manager +0x04
null and the device state 0, so the game continues on the stereo path's
no-device behaviour (02), and tell the player once (13 §5). The message must
convey that SteamVR could not be started, the runtime's error text and
number, and what the player can do next (the game continues without VR in
this run). For error 117 (`VRInitError_Init_AnotherAppLaunching`) it must
also convey that SteamVR Desktop Game Theatre is the likely cause and that
Steam's option to use Desktop Game Theatre while SteamVR is active must be
turned off in the game's Steam properties. The runtime is not retried in
this run.

### 6.5 Display reset and later CreateDevice calls

The game does not call CreateDevice again at a display reset (02 §9.3),
and no second call was traced; should one happen anyway, it must not start
the runtime again (a second `VR_Init` or overlay
would fail): it attaches the existing device again if the first call
produced one (state not 0), else leaves manager +0x04 null, and logs that
it was a repeated call with the outcome (11 §2.4). It takes no reference
pose.

A display reset (§4.2, 02 §9.3) runs on the render thread after an end
frame: slot 4 (the overlay is destroyed), the size query (§4.3), the
swapchain's `ResizeBuffers` (§9.2), then new stereo targets from slot 16's
answer and slot 3, which recreates the overlay and re-derives the layout
and the mirror constants. The device stays in state 2 throughout. A rebuild stalls the
game for about 0.2 s (observed); the mod must raise the rebuild flag only
when the pixel scale or a mirror setting actually changed (06 §4).

### 6.6 Headset removed, runtime idle, focus

Nothing in this document changes when the headset is taken off or the
runtime shows its dashboard: frames keep being submitted and paced by
`WaitGetPoses`, the reference pose is kept, and the game keeps running.
(Controller state while the game lacks input focus is 04's.) The game window
losing desktop focus changes nothing either.

## 7. The virtual screen for screen frames

Screen frames are those where the game did not ask for a head-tracked pose:
menus, the title video, loading screens, cutscenes. Their camera ignores the
head, so they are shown on a flat, world-locked screen instead of as stereo
3D.

Required in every screen frame:

| Property | Value |
|---|---|
| mechanism | an `IVROverlay` overlay (public OpenVR API), created with the runtime render resources (§4.4) and destroyed in slot 4; its key and name are the implementation's choice. If creation fails, log the error once and continue: screen frames then show black in the headset, the mirror still works |
| content | the stereo target texture (`TextureType_DirectX`, `ColorSpace_Gamma`), region `u 0.0..0.5`, `v 0.25..0.75`: eye 0's half, middle half vertically |
| width | 32/9 m (3.5556 m); height follows the region's pixel aspect (`H / W` of the target times the width, 1.778 m for square eyes) |
| placement | absolute, in `TrackingUniverseStanding`: the unconverted yaw-only matrix `Y` of the latest recenter (§2.1), with its translation moved 1.5 m along `Y`'s forward axis (minus its third column) in X and Z only. The screen centre is 1.5 m in front of where the head was at the recenter, at that head height, facing it, level. Before any recenter `Y` is identity |
| visibility | shown in screen frames; hidden on the first head-tracked frame |
| around the screen | the headset shows black around the screen. The compositor keeps receiving eye frames in screen frames too (it expects them every frame), by any means that shows black |

A recenter, or a new stereo texture after a rebuild, must show on the next
screen frame. The screen is world-locked: it moves only when the reference
pose is retaken, never with head motion.

At 1.5 m a 3.5556 m wide screen spans a half-angle tangent of about 1.19,
close to a typical eye's horizontal FOV, so the image appears at roughly the
angular size it was rendered for. The game's own menu quad (05 §4.2) sits
inside the middle half of the frame, which the bounds keep.

## 8. Desktop mirror

Drawn in slot 6 after the submit (both runtimes), into the game's back
buffer, which has the size of the desktop window the game asked for (§9.2),
not the stereo size. DXGI presents it to the window. All placement is
computed for the window's aspect `A = width / height` of the size recorded
by the render-target size query (§4.3); fractions of the back buffer equal
fractions of the window.

### 8.1 What is shown

Settings (06 §1): **mirror view** = both eyes (0), right eye (1, default) or
left eye (2); **symmetric** = on (default) or off.

Head-tracked frames show the eye image(s):

| Mirror view | Symmetric on: source region (texture UV) | Symmetric off: source region |
|---|---|---|
| left eye | `u 0..0.5, v 0..1` | eye 0 crop bounds (§3.3) |
| right eye | `u 0.5..1, v 0..1` | eye 1 crop bounds |
| both eyes | `u 0..1, v 0..1` | eye 0 crop on the left half of the draw area, eye 1 crop on the right half |

Screen frames show what the virtual screen shows: the region
`u 0..0.5, v 0.25..0.75`, whole (letterboxed, never cropped). Showing some
other texture the game happens to have bound at that moment is not
required, because what is bound there depends on game state (open
question 3).

The frame kind for the mirror is decided in slot 6 from the "head pose
requested" flag, before it is cleared.

### 8.2 Fitting

The source region has pixel aspect
`a = (region width in UV * W) / (region height in UV * H)`.

"Both eyes" with symmetric off uses two crops whose vertical extents can
differ (each eye's `vMin`/`vMax` come from its own tangents, §3.3). Both
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

Mistakes to avoid: computing `k` in
the cropped modes without the crop's height (that stretches a vertically
cropped image by `1 / (vMax - vMin)`); writing DefaultEyeFov with up and
down exchanged (that mirrors the vertical crop about the centre for a
vertically asymmetric eye); for "right eye, symmetric off", taking the crop
inside the **left** half (that shows the left eye's image).

### 8.3 Required pixel result

The mirror is drawn with the implementation's own shaders (written in HLSL
from this section). How the geometry, constants and sampler are set up is
the implementation's choice; the result must be:

- **Coverage**: every pixel of the back buffer is written, opaque
  (alpha 1), with no blending against what was there.
- **Mapping**: a back-buffer pixel inside the draw area (§8.2) shows the
  stereo texture (shader resource view at device +0x15C) at the
  corresponding position of the source region (§8.1): the map from draw
  area to source region is linear in each axis, with the draw area's top
  left at the region's top left (texture v = 0 is the top). "Both eyes,
  cropped" maps each half of the draw area to its own eye's crop (§8.2).
- **Filtering**: bilinear (linear filtering in both axes), so a mirror
  smaller or larger than the source is neither blocky nor aliased beyond
  what bilinear sampling gives.
- **Clamping**: sampling is clamped at the texture edges; nothing wraps
  around, so a crop at a texture border does not pick up texels from the
  opposite side.
- **Outside the draw area**: opaque black.
- **Target**: the game's back buffer (the render target the game has bound
  for output), no depth buffer. Clearing it first is allowed. Restoring the
  game's bindings afterwards is not needed, because `Present` follows
  (§6.2). Every reference to a view or resource obtained from the context
  must be released, because a reference kept per frame leaks one object per
  frame.

The mapping must be current for the layout and the recorded window size
after every configuration (slot 3, so after every rebuild) and for the
current mirror settings. A mirror setting changed in the menu reaches it
through the rebuild the mod requests (§6.5, 06 §4); recomputing it per frame
or on any change is also fine.

## 9. The game's swapchain and window while a headset is attached

"Headset attached" below means the stereo device is attached at manager
+0x04 at the moment of the call (either runtime). With no device (runtime
None, the runtime failed to start, or the OpenXR session failed, 09 §7)
none of this applies and the game's swapchain and fullscreen option work as
in the unmodded game.

### 9.1 Which swapchain

The mod intercepts the game's `D3D11CreateDeviceAndSwapChain` (01 §1) and
patches the swapchain's `Present`, `SetFullscreenState` and
`ResizeBuffers`. Those methods may be shared with other swapchains in the
process (an OpenXR runtime or an overlay may create its own in-process);
the behaviour below applies only to the game's swapchain, the one most
recently created through the intercepted call. Others pass through
unchanged.

### 9.2 Created windowed, back buffer at window size

With a headset attached, the stereo back buffer the game asks for is the
side-by-side size (for example 5376x2688 at pixel scale 1.0 on Index), larger
than any display mode, and only the mirror draws into it. When the game
creates its device and swapchain (`D3D11CreateDeviceAndSwapChain`, after
CreateDevice, 02 §6.1), the mod must:

- set `DXGI_SWAP_CHAIN_DESC::Windowed = TRUE` if the game asked for
  fullscreen (logged, 11 §2.2);
- replace `BufferDesc.Width` and `Height` with the window size recorded by
  the render-target size query (§4.3) when the game asked for a non-zero
  size that differs from it (log the size used).

Every `ResizeBuffers` on the game's swapchain gets the same size
replacement: a non-zero width and height that differ from the recorded
window size are replaced by it; a zero size ("use the window's client
size") passes through. Log each `ResizeBuffers` with the size the game
asked for and the size used (11 §2.2).

Why: a stereo-sized back buffer only costs. Measured at 8064x4032 (pixel
scale 1.5 on Index): 260 MB of VRAM for two back buffers, 0.3 ms of GPU for
the mirror against 0.03 ms at window size, plus the desktop compositor's
scaling. [sure]

### 9.3 The mirror window must fit the monitor

The game sizes its window for the display mode it picked (the monitor's full
resolution, or the stereo size). Forced windowed, a client area of the
monitor's size plus a title bar and borders runs past the screen
(project decision, ANSWERS r4: such a window is shrunk and centred).
Required, after the game's swapchain is first created and after every
successful `ResizeBuffers` on it, while a headset is attached and the
swapchain is windowed, for its output window:

- **Area**: the monitor the window is on (the nearest one if none); its work area if the
  window has a title bar (so the title bar and the taskbar stay visible),
  else the whole monitor.
- **Left alone**: a window that already fits the area (including one the
  player sized), a maximised or minimised window, and any window whose
  size or monitor cannot be read or whose client area is empty.
- **Otherwise**: the window is shrunk so that it fits the area, its client
  area keeping its aspect and being as large as fits, and centred in the
  area.
- **Applied without blocking**: the window belongs to the game thread, and
  this runs on the render thread, which the game thread may be waiting on,
  so the move must not wait for the game thread (`SetWindowPos` with
  `SWP_ASYNCWINDOWPOS`); it changes neither the Z order nor activation.
- **Logged** once per change, with the old and the new window size
  (11 §2.2).

The window's new client size then reaches the game through its own window
handling; the back buffer stays at the recorded size and DXGI scales it to
the client area.

### 9.4 Fullscreen is refused

The swapchain must **stay** windowed while a headset is attached. A later
switch to fullscreen, whether the game asks for it (its fullscreen video
option calls `IDXGISwapChain::SetFullscreenState`) or DXGI's own Alt+Enter
handling does, must be refused:

- `IDXGISwapChain::SetFullscreenState` with `Fullscreen = TRUE` on the game's
  swapchain returns `DXGI_ERROR_NOT_CURRENTLY_AVAILABLE` without changing
  state (the documented result when a fullscreen transition cannot be made,
  which applications must already handle); logged once per process.
  `Fullscreen = FALSE` passes through.
- DXGI's automatic Alt+Enter toggle is switched off for the game window
  (`IDXGIFactory::MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER)`, on
  the factory that created the swapchain, obtained with `GetParent`)
  (ANSWERS Q4).
- `IDXGISwapChain::ResizeTarget` passes through: in windowed mode it only
  resizes the window, which the mirror follows.

[guess] How the game reacts to the refusal is not known: it may keep
showing "Fullscreen" in its video options. That is acceptable; the window
stays a working mirror.

### 9.5 Present

The game's own `Present` calls pass through, with the game's sync interval,
except on the OpenXR path without a stereo device (09 §8). While stereo runs
the game does not call `Present` itself; slot 6 does, with `(0, 0)` (§6.2).

## 10. Pixel scale and the runtime's supersampling

`p` multiplies the runtime's recommended per-eye size (§4.1). That size
already includes the runtime's own resolution setting (SteamVR's per-app or
global render resolution, both for OpenVR and for SteamVR's OpenXR runtime).
So `p` is a second, per-axis multiplier on top of it: 1.5 means 2.25 times
the runtime's pixels, 2.0 four times. Observed on Index: GPU time in the
same scene 3.1 to 3.6 ms at 1.0 and 7.3 to 7.7 ms at 1.5; a heavy scene at
1.5 missed 90 Hz. [sure]

Required: default 1.0; the options page and the player guide describe it as
a multiplier of the runtime's (SteamVR's) resolution and warn that raising
both multiplies (06 §5); the cap of §4.1 applies to any value; a change
applies at the next render-target rebuild (§4.2), which the mod requests
when the value changes.

## 11. Data crossing threads

Requirements, whatever the mechanism:

| Data | Written by | Read by | Requirement |
|---|---|---|---|
| "head pose requested" flag | game thread (slot 11) | render thread (slots 5 and 6, which clears it) | a set made before the frame's slot 5 is seen there |
| the render pose (the headset pose slot 11 returned) | game thread | render thread (submit pose) | the submit uses exactly the pose the frame was rendered with; never a torn matrix |
| unconverted yaw-only reference `Y` | game thread (recenter) | render thread (virtual screen placement) | never torn; a recenter shows on the next screen frame |
| controller poses and states | render thread (after `WaitGetPoses`) | game thread | handed over whole (no mix of two frames' data in one read), 04 |
| device pointer at manager +0x04 and device state (+0x04 of the device) | game thread | render thread and job threads | aligned 32-bit stores and loads that are never torn; a value one frame stale is harmless |
| recorded window size | render-target size query | swapchain hooks, slot 3 | read after it is written in the same rebuild |

No locks that the render thread can wait on for long; no allocation, file
I/O or logging per frame on the render thread (11 §3).

## 12. AI.exe addresses used here

2015 RVAs were verified against the Steam 2015 binary. 2020 values were
taken from the earlier mod's per-build address table (and, for inline
sites, its installer) and have not been verified against a 2020 `AI.exe`;
"?" means unknown. Names are spec labels (01 §4); the implementation may
rename them. Bytes at the patched sites are in 01 §4 and 02 §6.

| Name | 2015 | 2020 | Use here |
|---|---|---|---|
| `StereoObjectPtr` | 0x1366A2C | 0x1376BDC | global holding the manager (02 §3) |
| `StereoInitCall` (CreateDevice site) | 0x613941 | 0x565E11 | §6.4, `thiscall(manager, int)`, `ret 4` |
| renderer initialisation | 0x6138F0 | ? | order of CreateDevice, size query, swapchain (01 §2.1) |
| back-buffer size query call | 0x613AE8 (into 0x60FC40) | ? | §4.2 |
| `StereoRenderTargetSizeCall` | 0x60FD64 | 0x562244 | §4.3, `thiscall(manager, int* w, int* h)`, `ret 8` |
| `StereoDisplaySizeCall` | 0x60FE5D | 0x56233D | leaves the window size alone (02 §6.2) |
| D3D11 device creation call | 0x613CB8 (into 0x626FA0) | ? | §9.2 |
| `RenderTargetResizePending` | 0x1358470 | 0x1368620 | §4.2, byte (display object +0x88), tested after each end frame (02 §9.3) |
| `RecenterCallUpdate` | 0x5BE2E8 | 0x5104A8 | §2.2 (02 §6.4) |
| `RecenterCallMenu` | 0x5B8691 | 0x50A831 | §2.2, reset-orientation method 0x5B8680 (2020: ?) |
| `DeviceEyeTransform` (slot 12, kept) | 0x5CD4C0 | 0x51F630 | §3, §5 |
| `DeviceFovAspect` (slot 13, kept) | 0x5B8E70 | 0x50B010 | §3, §4.3 |
| `DeviceEyeScaleOffset` (slot 14, kept) | 0x5B8F10 | 0x50B0C0 | §3 |
| `RendererPtr` | 0x17DF5CC | 0x17EF77C | 02 §8 |
| `RendererDevice` | 0x621E20 | 0x574310 | §4.4 |
| `RendererSwapChain` | 0x621E30 | 0x574320 | §6.2 Present |
| `RendererContext` | 0x621E40 | 0x574330 | §8 |
| `RendererRtTexture` | 0x6232D0 | 0x575800 | §4.4 |
| `RendererRtSrv` | 0x6232F0 | 0x5757C0 | §4.4, §8 |

## 13. Acceptance

Checks with OpenVR (SteamVR) unless noted; the log content is that of 11
(wording is the implementation's).

1. Start with a headset worn: the log shows the runtime chosen, the OpenVR
   start-up steps succeeding, the manufacturer, model and render tangents,
   the first render-target size equal to `2 * trunc(w * p')` by
   `trunc(h * p')` for the logged recommended size, the configuration with
   the same size, and stereo starting. In game the image is sharp (each eye at the
   recommended resolution times the pixel scale).
2. Turn the head
   quickly in gameplay: no lag or swimming beyond the compositor's normal
   reprojection.
3. Set pixel scale 2.0 in the menu during play: within a second the log shows
   a new render-target size (twice the previous per axis), a
   `ResizeBuffers` at the window size, and a new configuration; the eyes
   stay aligned. Set 4.0 on a 2688-wide eye: the logged effective pixel
   scale is about 3.05 (width at most 16384) and the game keeps running.
4. Start with the headset lying on the desk, then put it on: the log shows
   one recenter with activity level "user interaction" and an upright head;
   the menu screen is level and in front.
5. Load a level: when the loading screen closes, one recenter is logged and
   gameplay forward is where the player faces.
6. LB+RB (or left Ctrl + left Alt) recenters once per press; the virtual
   screen re-places in front.
7. Menus, videos and loading screens appear on a level screen about 3.56 m
   wide, 1.5 m away, black around it; it does not follow the head. Entering
   gameplay hides it.
8. IPD 66 mm: the first begin frame logs ViewAdjust eye 0 (+0.033, 0, 0) and
   eye 1 (-0.033, 0, 0) (scaled by `-vrscale` when given); objects at arm's
   length look right.
9. Desktop window: with the game set to fullscreen at the monitor's
   resolution, the log shows that the swapchain was forced windowed, the
   back-buffer size used equal to the window size, and the window shrunk
   with its old and new size; the window sits centred on the monitor inside the
   work area with its title bar visible. A window resized by the player to
   fit is not moved again.
10. Turning on the game's fullscreen option while stereo runs: the log shows
    the refusal once; the window stays a working mirror. Alt+Enter
    does nothing.
11. Mirror views: right eye (default) fills the window width, cropped top
    and bottom in a 16:9 window; left eye likewise; both eyes side by side
    with bars above and below; symmetric off shows each eye cropped to its
    real FOV, never stretched; menus show the virtual-screen region whole.
12. With runtime None, or SteamVR not installed with OpenVR forced: no
    stereo, the game's fullscreen option works, no swapchain changes are
    logged.
13. SteamVR running with Desktop Game Theatre active: the message names
    error 117 and Desktop Game Theatre; the game continues without VR.
14. Start the game with SteamVR not running and the headset connected:
    SteamVR starts, the log shows the interface versions obtained, and VR
    works without a restart. With the headset unplugged: the failure
    message appears within about 10 s and the game runs flat.
15. In gameplay, cover the headset's tracking (or step out of tracking):
    the image holds the last pose, never flips or blanks with invalid
    values. Press the recenter chord while tracking is lost: the reference
    does not change until tracking returns, then the recenter happens once.
    Recentering while looking straight up or down keeps the previous
    forward direction.

## 14. Open questions

1. **2020 build.** The 2020 RVAs above were taken from the earlier mod's
   table and have not been verified against a 2020 `AI.exe`; renderer initialisation, the
   size query call, the D3D11 creation call, the rebuild test and the
   reset-orientation method have no 2020 address. To resolve: find the
   2015 byte patterns of 01 §4 in the 2020 binary.
2. **Game reaction to the fullscreen refusal** (§9.4): whether the video
   options keep showing "Fullscreen" and whether the game retries. To
   resolve: turn fullscreen on at start and while stereo runs and read the
   options page and the log.
3. **What the game has bound at pixel-shader slot 0 when slot 6 runs in a
   screen frame** (§8.1). If it is always the 1280x720 UI target, sampling
   it would give a sharper flat menu on the desktop than the virtual-screen
   region. To
   resolve: capture a menu frame and inspect the bound resource at slot 6.
4. **Which game code consumes slot 13's vertical FOV and aspect** (§3.2):
   symmetric frusta are required on the assumption that such code expects
   a centred frustum. To resolve: trace the callers of the slot 13 wrapper
   (RVA 0x5B86C0).
