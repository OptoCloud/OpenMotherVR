<!-- Provenance: game analysis of AI.exe 2015 (device methods 3 to 16, relative head pose, render-target size query, render-target rebuild flag, renderer accessors, order of renderer initialisation); OpenVR, DXGI, Win32 and LibOVR 0.4 public API semantics (fullscreen transitions, window placement, window subclassing, interface versions, the LibOVR descriptor fields); observation in game by the spec side on SteamVR with a Valve Index (full resolution from the start, eye layout tied to the created texture, windowed swapchain, mirror back buffer at window size, recenter when worn and when gameplay starts, running start with pose age 0 over 600 frames, render-target sizes up to 13440x6720 and the 16384 texture limit, pixel-scale changes applied in the same run through the rebuild flag, head motion moving the camera correctly, the game's threads per device slot); observation of the SteamVR install by the spec side (where the x86 `vrclient.dll` and its libraries are); performance measurements in game by the spec side (mirror and back-buffer cost, controller latency); answers to implementation questions Q1, Q3, Q4 (ANSWERS.md); user requirements and project decisions (SteamVR need not be running, invalid poses, bounded start-up waits). 2020 values were taken from the earlier mod's per-build address table (and, for inline sites, its installer) and have not been verified against a 2020 `AI.exe`. For spec r6: AI.exe 2015 analysis by the spec side (the game's window procedure, RVA 0x612850, and its focus, activation and size handling); OpenVR public API (IVRApplications, the scene application's process id); observation in game by the spec side (a mirror window shrunk to fit its monitor, as the spec r4 decision required, put the menus and the cursor off centre); project decisions of spec r6, prompted by a tester's report on SteamVR with a Steam Frame headset (the game paused or held at a loading screen when another window took the focus; the game not shown in the headset with Desktop Game Theatre on), each with its reason inline (a mirror window that does not fit becomes borderless over its monitor, replacing the spec r4 shrink-and-centre decision; focus loss is kept from the game while a headset is attached; the scene-application check after about 3 s). For this document's r6 (after spec r6): clean-room review round 2 applied (results instead of mechanisms in §6.7, §9.3 and §9.6; the window-size behaviour traced in the game; the screen-frame mirror question restated from the game; descriptor header values left free); coverage review (late detach on the render thread, one interception failing, mirror resources failing, mirror shaders built at build time, the `vrclient.dll` location, failure text without a runtime error, acceptance checks for invalid poses, cropped mirror modes and the pose handover). Section numbers are unchanged from r5. For r7: cross-document alignment after the second fix pass (CreateDevice labelled with its RVA, and its single call site re-verified by the spec side in the 2015 `AI.exe`; required and optional OpenVR interfaces and the loader release as in 13 §3.2; the reason the mirror's shaders must not contain the dither pattern; the back-buffer view the mirror binds is not created per frame; references to 06 §4.6 and 05 §10 for the rebuild flag; the pose-handover check no longer points at an event 11 does not define). Section numbers are unchanged from r6. For r8, under the clean-room process r3 (what a player sees may be stated exactly): the SteamVR failure box of §6.4 given exactly (caption, icon, button, text: the earlier mod's text, whose behaviour is unchanged, with this project's caption `MotherVR` without a version, the `unknown error` fallback and this project's descriptions for failures the earlier mod did not report) and the headset-not-showing-the-game warning of §6.7 (this project's text, a check the earlier mod did not have); the options-page labels of the mirror and pixel-scale settings (§8.1, §10, parameters table); what the player sees of the virtual screen (§7); acceptance checks 3, 13, 14 and 16 naming the boxes and labels. No game fact was added. Section numbers are unchanged from r7. For r9: project decisions prompted by a tester's report on SteamVR with a Quest 2 headset (on the first launch the first start-up video appeared low and behind the player, the next one in front but far too high, the main menu then in place; one recenter at CreateDevice with the headset in use, none after it until a level loaded), each with its reason inline: the runtime moving its tracking space under the mod (a new chaperone universe, a seated or standing zero-pose reset; OpenVR public API events) queues a retake and hides the virtual screen until it (new §6.8, a row in §2.2, §6.2, §11); the virtual screen is placed in the universe the poses are in, as `IVRCompositor::GetTrackingSpace` reports it, not a fixed one (§1, §7); no virtual screen is shown before the first valid reference (§7, replacing "before any recenter `Y` is identity"); the frame kind of the first frames logged whatever `DebugOptions` says (§6.2, 11 §4.5). Game analysis by the spec side in the 2015 `AI.exe` (the start-up sequence is started by a front-end method of its own at 0x441AA0, not by the front-end menu's Open at 0x441950, which ends at 0x441A90) and the existing fact of 02 §6.3 (slot 11's only per-frame caller is the VR camera behaviour of a head-tracked camera) put the start-up videos and the legal screen in screen frames [likely] (§7). New section §6.8 and acceptance checks 19 to 21; other section numbers are unchanged from r8. For r10: game analysis by the spec side in the 2015 `AI.exe` (the window procedure's focus byte at RVA 0x1251298, set on `WM_SETFOCUS` and cleared first on `WM_KILLFOCUS` at RVA 0x612917; the input update that drops keyboard and mouse state while it is 0; raw input registered with `RIDEV_INPUTSINK` at RVA 0x61095B), prompted by the user's report that keyboard and mouse input reached the game while another window had the focus: §9.6 now requires the byte cleared on a focus loss kept from the game, and its closing note no longer says Windows sends input only to the foreground window. Section numbers are unchanged from r9. Revision: r10 -->

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
| `p` | pixel scale, default 1.0, range 0.5 to 4.0 | config file and the options page's `Pixel Scale` slider only (06 §1, §5); no command-line override. The effective value is capped (§4.1) |
| eye 0 / eye 1 | the game's eye indices | eye 0 is the **left** eye: left half of the target, submitted as `Eye_Left`; eye 1 is the right eye |
| `W`, `H` | the side-by-side render target's size | §4 |
| `A` | the desktop window's aspect, width / height of the size the game asked for (§4.1, §8) | §8 |

Threads (game facts, observed in game by the spec side [sure]): the game has a **game
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
`TrackingUniverseStanding`; the mod does not change it), as
`IVRCompositor::GetTrackingSpace` reports it once the runtime is up
(§6.4). Everything the mod places absolutely, the virtual screen of §7,
is placed in that same universe: a pose and a placement in different
universes put the screen at the right coordinates of the wrong origin. This is the pose
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
| the runtime moved its tracking space (§6.8) | queue a retake |

A queued retake is a single flag: queuing it again before it runs changes
nothing. A repeated CreateDevice (not expected, §6.5) does not take or
queue a reference. Taking the headset off and putting it on again later does not
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
  exceeds 0.7, within about 45 degrees of upright (project decision: a
  headset lying on a desk or hanging from a hook is tilted further than
  that, while a player looking well up or down still counts as wearing
  it).

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

The other fields of the descriptor header (02 §4: Handle, Type, ProductName,
Manufacturer and the three caps fields) is not read by game code outside
the device slots the mod replaces, so any values are acceptable (zero, or
any consistent description); a non-null ProductName or Manufacturer
points at a string that lives as long as the device. Log the headset's
manufacturer and model as the runtime reports them, and the two render
tangents (11 §2.4).

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
because the pixel scale or a mirror setting changed (06 §4.6, 05 §10). A changed pixel
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
| slot 6 (render thread) | if no stereo texture is known yet, skip to the mirror. Head-tracked: hide the virtual-screen overlay if it is visible, then `IVRCompositor::Submit(Eye_Left, ...)` and `Submit(Eye_Right, ...)`. Screen frame: §7. Then read the runtime's events (§6.8); then `IVRCompositor::WaitGetPoses(nullptr, 0, nullptr, 0)`; then hand a tracking space change read before it to the game thread (§6.8); then read the controllers and hand them to the game (04 §5.9); then draw the mirror (§8); then `IDXGISwapChain::Present(0, 0)` on the game's swapchain (accessor, 02 §8); then clear the "head pose requested" flag |

Each change between screen frames and head-tracked frames is logged once
per change, on either runtime (11 §4.5): the first frame's kind and each
change up to and including the first head-tracked frame whatever
`DebugOptions` says, later changes only with `DebugOptions` on. Reason
(project decision, r9): whether the start-up videos, the legal screen and
the front end are screen frames is the first question about a report of
a misplaced screen at start-up, and testers' logs rarely have
`DebugOptions` on; the always-on lines are a handful per launch.

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

CreateDevice is the manager's device creation (02 §6.1: RVA 0x5E0D40,
`thiscall(manager, int type)`, `ret 4`, called once per run from
`StereoInitCall`) on the game thread during renderer initialisation, before
the render-target size query and before the D3D11 device exists. For
`type == 1`, and only the first time it is called, the OpenVR start-up
must meet these requirements (the runtime chosen per 06 §1, logged per
11 §2.3):

| Topic | Requirement |
|---|---|
| finding the runtime | the OpenVR runtime root is the first string of the `"runtime"` array in `%LOCALAPPDATA%\openvr\openvrpaths.vrpath` (JSON, written by SteamVR), usable when `<root>\bin\win32\openvr_api.dll` exists |
| the loader | `openvr_api.dll`, x86, from a public OpenVR SDK release (13 §3.2: the loader passes each interface request on to the installed SteamVR, so its release need not match the interface versions below): the copy deployed next to the game (13), else `<root>\bin\win32\openvr_api.dll`. Exports used, all `cdecl`: `VR_InitInternal2`, `VR_ShutdownInternal`, `VR_GetGenericInterface`, `VR_GetVRInitErrorAsEnglishDescription`; any missing is a failure |
| DLL search path | while the loader loads and while `VR_InitInternal2` runs, `<root>\bin\win32` is on the DLL search path, because the loader loads the x86 `<root>\bin\vrclient.dll`, and SteamVR's other x86 libraries that it needs are in `<root>\bin\win32` (observed in a SteamVR install by the spec side); without that folder on the search path the result is `VRInitError_Init_VRClientDLLNotFound` (102). The previous search path is restored afterwards. When `vrclient.dll` or one of its dependencies cannot be loaded, the log names the file the loader tried and the Windows error (for example 126 for a missing dependency) rather than only error 102; how the mod finds this out is its choice (loading `<root>\bin\vrclient.dll` itself with the same search path before `VR_InitInternal2` is one way; a reference the mod takes is released when the runtime is shut down) |
| SteamVR need not be running | `VR_InitInternal2(&err, VRApplication_Scene, nullptr)` starts SteamVR if it is not running; the mod makes no check for a SteamVR process beforehand. An init error (including no headset, such as `VRInitError_Init_HmdNotFound`) is a failure, reported with the runtime's English description and the error number |
| interface versions | `IVRSystem_017`, `IVRCompositor_021` and `IVROverlay_016` (OpenVR SDK 1.0.10), or later versions of the same interfaces when the implementation is built against a later SDK; every call used here exists in all of them. Obtained with `VR_GetGenericInterface`. These three are the required interfaces (13 §3.2): a missing one is a start-up failure with the OpenVR error code and says which interface was missing. The versions obtained are logged once. Two more are optional, and the absence of either is logged and is not a failure: `IVRApplications` (any version that has `GetCurrentSceneProcessId`), for the check of §6.7 only, and `IVRInput`, without which the legacy controller path of 04 §5.8.3 applies |
| bounded wait | on a cold start SteamVR may serve the compositor (and report the headset) only some seconds after `VR_InitInternal2` returns. Waiting for the compositor interface and then for a valid headset pose (`bPoseIsValid` of the headset's pose from `WaitGetPoses`) is bounded to about 10 s in total (project decision: long enough for a cold SteamVR start on current PCs, short enough that the game does not appear hung). No compositor within the bound is a failure; no valid pose within it is not (§1 covers invalid poses, and the reference is then queued, §2.1) |
| logged once | the recommended per-eye size and the IPD (right eye X minus left eye X of `GetEyeToHeadTransform`) |
| on success | the descriptor and EyeFov filled (§3.2), device state 1, the device attached at manager +0x04, the reference taken (§2.2) |

Order is required only where the API forces it: the loader is loaded
before `VR_InitInternal2`, the runtime is initialised before its
interfaces are obtained, and the compositor exists before `WaitGetPoses`;
the device is attached last, so the game never sees a half-filled device.

Failure: shut the runtime down if it was initialised, leave manager +0x04
null and the device state 0, so the game continues on the stereo path's
no-device behaviour (02), and tell the player once with the message box
below (13 §5.3 lists it with the other messages; the text is the same
there). The runtime is not retried in this run.

The box is a standard Windows message box: caption `MotherVR`, error icon,
one OK button, in front of the game window; the game continues flat while
it is open and after it is closed. Its text is the one players of the
earlier mod saw for this failure (behaviour unchanged), with the
placeholders filled as the table below says. Quoted box texts in this
document are shown one box line per quoted line; a line ending in `\`
continues on the next box line without a blank line between, and an
empty quoted line is a blank line in the box. `MotherVR` is written as a
code span only to mark it as the compatibility name; the box shows it as
plain text.

For any error other than 117:

> Failed to initialize SteamVR!\
> [`<description>` | `<code>`]
>
> Look into the above error, and try again once it has been resolved.
>
> `MotherVR` will now start with VR disabled. Restart the game to try again.

For error 117 (`VRInitError_Init_AnotherAppLaunching`), whose usual cause
is SteamVR's Desktop Game Theatre starting for the game, two paragraphs
are added after the first one:

> Failed to initialize SteamVR!\
> [`<description>` | `<code>`]
>
> Look into the above error, and try again once it has been resolved.
>
> It's possible that this error is being caused by SteamVR Desktop Game Theatre.
>
> To use `MotherVR` with SteamVR, you need to uncheck 'Use Desktop Game Theatre while SteamVR is active' in the Steam Properties for Alien: Isolation, and make sure that VR Game Theatre isn't running when you start the game.
>
> `MotherVR` will now start with VR disabled. Restart the game to try again.

('Use Desktop Game Theatre while SteamVR is active' is Steam's own label
for the setting.)

| Failure | `<description>` | `<code>` |
|---|---|---|
| the runtime returned an error (`VR_InitInternal2`, or `VR_GetGenericInterface` for a required interface that reports an error) | the runtime's English description of it (`VR_GetVRInitErrorAsEnglishDescription`); `unknown error` when that text is empty | the OpenVR error number, decimal |
| the loader could not be loaded | `cannot load <path>`, with the full path of the copy tried last | the Windows error code, decimal (for example 126) |
| a loader export is missing | `openvr_api.dll has no <export>`, with the export's name | the Windows error code of the lookup, decimal (127) |
| a required interface is not served | `<interface> not available`, with the interface version requested (for example `IVRCompositor_021`) | the OpenVR error number the request returned, decimal |
| no compositor within the bounded wait | `no compositor after <n> s`, with the wait's length in whole seconds | the OpenVR error number of the last request, decimal |

The texts of the last four rows are this project's: the earlier mod gave
these cases no description of their own (a loader that could not be loaded
had a box of its own, which this row replaces, so that every SteamVR
start-up failure shows the same box). Each names what was missing, so a
player can report it, and the box never shows an empty description or a
bare 0. The
log records the same values (11 §2.3).

### 6.5 Display reset and later CreateDevice calls

The game calls CreateDevice once per run (game fact, 02 §6.1, verified by
the spec side in the 2015 `AI.exe`: its only call is `StereoInitCall`,
inside the display initialisation at RVA 0x6138F0, which has a single
caller at RVA 0x59C307, and no pointer to the method exists in the image);
in particular a display reset does not call it (02 §9.3). Should a second
call happen anyway, it must not start the runtime again (a second
`VR_Init` or overlay would fail): it attaches the existing device again
if the first call produced one (state not 0), else leaves manager +0x04 null, and logs that
it was a repeated call, which is not expected, with the outcome (11 §2.4).
It takes no reference pose.

A display reset (§4.2, 02 §9.3) runs on the render thread after an end
frame: slot 4 (the overlay is destroyed), the size query (§4.3), the
swapchain's `ResizeBuffers` (§9.2), then new stereo targets from slot 16's
answer and slot 3, which recreates the overlay and re-derives the layout
and the mirror constants. The device stays in state 2 throughout. A rebuild stalls the
game for about 0.2 s (observed); the mod must raise the rebuild flag only
when the pixel scale or a mirror setting actually changed (06 §4.6,
05 §10).

### 6.6 Headset removed, runtime idle, focus

Nothing in this document changes when the headset is taken off or the
runtime shows its dashboard: frames keep being submitted and paced by
`WaitGetPoses`, the reference pose is kept, and the game keeps running.
(Controller state while the game lacks input focus is 04's.) The game window
losing desktop focus must not pause the game or stop frames either (§9.6).

### 6.7 The headset shows another application (OpenVR)

The headset shows the frames of one application at a time, the one
SteamVR treats as its current scene application; if that is another
process, the game's frames are discarded although start-up succeeded and
no call fails. A tester on SteamVR with a Steam Frame headset reported
this with Steam's Desktop Game Theatre on for the game: the headset showed
the game's desktop window on a flat screen instead of the game's own
frames. (When the Theatre is still starting as the game initialises,
start-up fails with error 117 instead, §6.4.)

Required: once per process, about 3 s after the first submission (project
decision: late enough that a normal start has made the game the scene
application, early enough that the player is told while still looking at
the headset), the process the compositor reports as its current scene
application (`IVRApplications::GetCurrentSceneProcessId`) is compared with
the game's own process:

| Result | Required |
|---|---|
| the game's process | logged once (11 §2.4) |
| another process, or none (0) | logged once with that process's id and, when it can be read, its executable's file name (11 §2.4); the warning below is shown once per process, naming that executable when known. Nothing else changes: frames keep being submitted |
| the interface `IVRApplications` is not available | the check is skipped and that is logged; this is not a start-up failure, because the check is advisory |

The warning is a standard Windows message box (13 §5.3 lists it with the
other messages; the text is the same there): caption `MotherVR`, warning
icon, one OK button. Its text is this project's (the earlier mod had no
such check; the wording follows its Theatre advice of §6.4, with the
steps that fix the cause), quoted as in §6.4:

> SteamVR is not showing the game in the headset (it shows `<process>`).
>
> This is usually SteamVR's Desktop Game Theatre. In Steam, open the Properties of Alien: Isolation and uncheck 'Use Desktop Game Theatre while SteamVR is active', then restart SteamVR and the game.

`<process>` is the file name of the other process's executable, as
without its folder (for example `steam.exe`). When the process is
none (0) or its executable cannot be read, the parenthesis and the space
before it are left out, so the first line is "SteamVR is not showing the
game in the headset."

The warning must block neither the render thread nor the game thread
(13 §5.2): the frame loop keeps submitting while it is open, so the game
appears in the headset as soon as the player fixes the cause. The check is
made once per process; it is not repeated after a display reset.

### 6.8 The runtime moves its tracking space (OpenVR)

SteamVR can move the origin of the universe the poses are in while the
game runs: a room setup committed with a different universe
(`VREvent_ChaperoneUniverseHasChanged`), the seated or standing zero pose
reset (`VREvent_SeatedZeroPoseReset`, `VREvent_StandingZeroPoseReset`),
for example by a headset's own recenter or by a streaming client
establishing its play area after start-up. The reference pose (§2) and the
virtual screen (§7) were taken in the old space: in the new one, forward
points elsewhere and the screen stands at the old coordinates of a moved
origin, which a player sees as a screen behind, below or far above them.

Required:

| Item | Required result |
|---|---|
| reading events | the mod reads `IVRSystem::PollNextEvent` on the render thread once per submitted frame, before that frame's `WaitGetPoses` (§6.2), at most 64 events per frame (a project decision: far more than SteamVR sends in a frame, so the queue never falls behind, while a runtime that floods it cannot hold the frame). It is the process's only reader of that queue: every reader in a process shares one read position, so a second reader would take events from it. Not before the SteamVR Input action manifest is set (04 §5.8), which the API requires to come first |
| a change | any of the three events above, whichever universe the poses are in (reason: some runtimes move both universes together, and a retake the space did not need only faces forward again): logged once per event (11 §2.4); the virtual screen is not shown from then until the retake; a retake is queued (§2.2) |
| order | the change reaches the game thread only after the `WaitGetPoses` that follows the read, so the pose the retake is taken from was predicted after the runtime reported the change |
| the retake | as any queued retake (§2.2): on the first frame where the headset is in use and its pose valid. The virtual screen then shows again, placed from the new reference, and the game's own menu quad is placed again (05 §4.2) |
| the headset | the headset's `VREvent_TrackedDeviceUserInteractionStarted` and `...Ended` are logged (11 §2.4) but do not queue a retake by themselves (§2.2: putting the headset on again does not) |
| other events | left unhandled |

OpenXR reports the same situation as a pending reference space change and
re-anchors its own spaces (09 §6).

## 7. The virtual screen for screen frames

Screen frames are those where the game did not ask for a head-tracked pose:
menus, the title video, loading screens, cutscenes, and the start-up
sequence (the logo videos and the legal screen) [likely: slot 11's only
per-frame caller is the VR camera behaviour of a head-tracked camera
(02 §6.3), and no level, so no such camera, exists before the title
screen; the start-up sequence is started by a front-end method of its
own (RVA 0x441AA0, which pushes `ShowBootup`, 05 §11), not by the
front-end menu's Open (0x441950), so whether the game is in stereo mode 1
during it is not settled by 05 §5. The always-on frame-kind lines of §6.2
settle it from a tester's log]. Their camera ignores the
head, so they are shown on a flat, world-locked screen instead of as stereo
3D. What the player sees: the game's flat picture (the left eye's image,
middle half vertically) on a level panel about 3.56 m wide, 1.5 m ahead
at the head height of the last recenter, with no frame or border; the
rest of the view is black.

Required in every screen frame:

| Property | Value |
|---|---|
| mechanism | an `IVROverlay` overlay (public OpenVR API), created with the runtime render resources (§4.4) and destroyed in slot 4; its key and name are the implementation's choice. If creation fails, log the error once and continue: screen frames then show black in the headset, the mirror still works |
| content | the stereo target texture (`TextureType_DirectX`, `ColorSpace_Gamma`), region `u 0.0..0.5`, `v 0.25..0.75`: eye 0's half, middle half vertically |
| width | 32/9 m (3.5556 m); height follows the region's pixel aspect (`H / W` of the target times the width, 1.778 m for square eyes) |
| placement | absolute, in the universe the poses are in (§1): the unconverted yaw-only matrix `Y` of the latest recenter (§2.1), with its translation moved 1.5 m along `Y`'s forward axis (minus its third column) in X and Z only. The screen centre is 1.5 m in front of where the head was at the recenter, at that head height, facing it, level |
| visibility | shown in screen frames once a reference was taken from a valid pose in the current tracking space; hidden on the first head-tracked frame. Not shown (the view is all black) before the first such reference, nor from a tracking space change (§6.8) until the retake. Reason (project decision, r9): without a reference the screen would stand at the universe's origin, on the floor or behind the player, and a reference from before a space change places it wherever the old coordinates now fall; a few frames of black until the headset is worn and tracked are better than a screen the player has to look for |
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

Settings (06 §1), on the options page's `Rendering` page (06 §5) with the
labels players of the earlier mod knew: **mirror view**
(`Rendering.MirrorViewType`, the list `Mirror View` with the values
`Both Eyes`, `Right Eye`, `Left Eye`) = both eyes (0), right eye (1,
default) or left eye (2); **symmetric** (`Rendering.SymmetricMirrorView`,
the toggle `Mirror View Symmetry`) = on (default) or off.

Head-tracked frames show the eye image(s):

| Mirror view | Symmetric on: source region (texture UV) | Symmetric off: source region |
|---|---|---|
| left eye | `u 0..0.5, v 0..1` | eye 0 crop bounds (§3.3) |
| right eye | `u 0.5..1, v 0..1` | eye 1 crop bounds |
| both eyes | `u 0..1, v 0..1` | eye 0 crop on the left half of the draw area, eye 1 crop on the right half |

Screen frames show what the virtual screen shows: the region
`u 0..0.5, v 0.25..0.75`, whole (letterboxed, never cropped). A sharper
source for flat menus is open question 4.

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

The mirror is drawn with the implementation's own shaders, written in HLSL
from this section, compiled when the DLL is built and embedded in it; the
DLL does not load a shader compiler at run time (project decision: a
run-time compiler adds a dependency and start-up cost for shaders that
never change; 13 §7, Shaders row). The build fails if they do not
compile. The compiled shaders must not contain the dither pattern of
08 §1.1: the shader fix of 08 §1 recognises the shaders it changes by that
pattern, and the mirror's own shaders must reach the driver unchanged
(08 §1.4, Where). How the geometry, constants and sampler are set up is the
implementation's choice; the result must be:

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
- **Target**: the game swapchain's back buffer, no depth buffer. The game
  normally has it bound for output at that point; when nothing is bound
  for output at the end of a frame, the mirror binds the back buffer
  itself, so the window never shows a stale picture. The view it binds
  may be kept for the life of the back buffer and released before every
  `ResizeBuffers` (DXGI requires every reference to the buffers released
  then); it is not created per frame, because that would allocate on the
  render thread (§6.3). Clearing it first is
  allowed. Restoring the game's bindings afterwards is not needed, because
  `Present` follows (§6.2). Every reference to a view or resource obtained
  from the context or the swapchain in a frame must be released in that
  frame, because a reference kept per frame leaks one object per frame.
- **Resources missing**: if the mirror's shaders, constant buffer or
  sampler cannot be created, this is logged once with the error (11 §2.4)
  and VR is unaffected; the back buffer is then cleared to opaque black
  every frame instead of drawn, and `Present` still runs. Nothing is drawn
  with a missing object.

The mapping must be current for the layout and the recorded window size
after every configuration (slot 3, so after every rebuild) and for the
current mirror settings. A mirror setting changed in the menu reaches it
through the rebuild the mod requests (§6.5, 06 §4.6); recomputing it per frame
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

Each of the three replacements is installed on its own: one that cannot be
installed is logged (11 §2.2) and the others are still installed. The
stereo frame itself does not depend on any of them. Without the `Present`
replacement the OpenXR path without a stereo device (09 §8) is off;
without the `ResizeBuffers` replacement the back buffer follows the size
the game asks for after a resize; without the `SetFullscreenState`
replacement the game's fullscreen option may break the mirror (§9.4).

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
resolution, or the stereo size) and gives it a title bar and borders. Forced
windowed, a client area of the monitor's size plus a title bar and borders
runs past the screen.

Game fact (AI.exe 2015, traced by the spec side [sure]): the game's window
procedure (RVA 0x612850) passes `WM_SIZE` to the size handler of its
display object (handler RVA 0x611B90, object at RVA 0x1251288), which
records the new client size and resizes only while the display mode at
object `+0x04` is 0; in any other mode it returns without using the new
size. Mode 2 is the fullscreen mode (it is the mode in which focus loss
pauses the game, §9.6) and 0 the windowed one [likely, from these two
uses]. The game keeps its fullscreen mode under VR because only the
swapchain is forced windowed, so it ignores every change of its window's
size and keeps the size it chose for its UI and mouse. Observed in game by
the spec side: a window shrunk to fit the monitor, as the spec r4 decision
required, had a client area smaller than that size, and the menus and the
cursor were off centre. So a window that does not fit is not shrunk; it
becomes borderless and covers its monitor, which gives a client area of
the monitor's size, as fullscreen would have (project decision of spec r6,
replacing the shrink-and-centre decision of spec r4).

Required, after the game's swapchain is first created and after every
successful `ResizeBuffers` on it, while a headset is attached and the
swapchain is windowed, for its output window:

- **Area it must fit**: its monitor's work area when the window has a
  title bar (so the title bar and the taskbar stay visible), else the
  whole monitor.
- **Result for a window larger than that area**: no title bar and no
  border, and a client area equal to its monitor's full rectangle (not
  the work area).
- **Left alone**: a window that fits (including one the player sized), and
  one the player has maximised or minimised. If its size or its monitor
  cannot be determined, nothing changes.
- **Constraints**: the change must not wait for the game thread, which
  owns the window and may itself be waiting on the thread that changes the
  swapchain; it must not activate the window or change its Z order; and
  the window keeps its ANSI character set (the game registers its window
  class with `RegisterClassExA`). The Win32 calls and window styles used
  are the implementation's choice.
- **Logged** once per change, with the old window size and the new
  borderless size (11 §2.2).

Re-checking after every `ResizeBuffers` is harmless: a window made
borderless over its monitor fits the whole-monitor area and is left alone,
and so is one the player sized to fit. The back buffer stays at the
recorded size and DXGI scales it to the client area.

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

### 9.6 Desktop focus does not pause the game

Game facts (AI.exe 2015, the game's window procedure at RVA 0x612850,
registered with `RegisterClassExA` at RVA 0x612F90, so an ANSI window
class; verified by the spec side):

- the game keeps a window-focus byte at RVA 0x1251298: `WM_SETFOCUS`
  (branch at RVA 0x6128FE) sets it to 1, and `WM_KILLFOCUS` clears it
  first, with `mov byte [0x1251298], 0` at RVA 0x612917, before any other
  handling. While it is 0 the game's input update drops its keyboard and
  mouse state;
- on `WM_KILLFOCUS` (branch at RVA 0x612910) while its display mode is
  fullscreen (display object `+0x04` equal to 2, §9.3), the game then posts
  the system event that opens its pause menu and releases its mouse
  buttons;
- the game registers raw input for the keyboard and the mouse with
  `RIDEV_INPUTSINK` (at RVA 0x61095B), so Windows keeps delivering both to
  it while another window has the focus; the focus byte is what stops the
  game from acting on them;
- on `WM_ACTIVATE` (branch at RVA 0x61288F) with the window becoming
  inactive, it releases its cursor clip and clears its keyboard state.

Under VR the game keeps its fullscreen display mode while the swapchain is
forced windowed (§9.2, §9.4), so the first rule applies whenever any other
window takes the desktop focus. The desktop window is only a mirror in VR,
and SteamVR, its dashboard or a streaming client for a wireless headset
take the focus on their own: a tester's game paused, or stayed at a
loading screen, until the window was clicked (reported on SteamVR with a
Steam Frame headset).

Required, while a headset is attached (as defined at the top of §9):

- **Focus loss is kept from the game.** A loss of keyboard focus must not
  reach the game's own focus-loss handling (the first game fact above), so
  the game neither opens its pause menu nor drops its mouse buttons when
  another window takes the focus.
- **The focus byte still follows the focus.** On each focus loss kept from
  the game, the game's focus byte is cleared as the game's own handling
  would have cleared it (the first game fact above), so keyboard and mouse
  input does not reach the game in the background. A build without a
  known, verified address for the byte keeps the focus loss from the game
  without clearing it, and logs that background input is not dropped.
- **Everything else reaches the game unchanged.** `WM_ACTIVATE` and every
  other message reach the game's window procedure as before, in the ANSI
  form its class expects (a Unicode path would change how text messages
  reach the game). So the game still releases its cursor clip when
  another window becomes active (the cursor is free on the desktop) and
  clears its keyboard state (no key stays held).
- **Without a headset attached** (runtime None, a runtime that failed, or
  a headset that detached later, 09 §7) focus loss reaches the game as in
  the unmodded game.
- **When it holds**: at the latest from the creation of the game's
  swapchain with a headset attached (the loading screens that follow are
  where a lost focus held the game), for as long as one is attached. How
  this is done (subclassing the game's window is one way) and when it is
  put in place are the implementation's choice.
- **Thread**: the game's window messages are delivered on the game thread,
  which created the window; whether a headset is attached is a value
  another thread may change, read there without tearing (12 §3).
- **Logged** (11 §2.2): that the fix is active; each focus loss kept from
  the game, with the application that took the focus (the executable of
  the process owning the foreground window, and its process id), so a log
  shows which application took it.

What the player still sees: keyboard and mouse input stops reaching the
game while another window has the focus, because the focus byte is cleared
(the raw input itself keeps arriving, see the game facts); the gamepad and
the VR controllers keep working, because the game reads them by polling,
not through window messages.

## 10. Pixel scale and the runtime's supersampling

`p` multiplies the runtime's recommended per-eye size (§4.1). That size
already includes the runtime's own resolution setting (SteamVR's per-app or
global render resolution, both for OpenVR and for SteamVR's OpenXR runtime).
So `p` is a second, per-axis multiplier on top of it: 1.5 means 2.25 times
the runtime's pixels, 2.0 four times. Observed on Index: GPU time in the
same scene 3.1 to 3.6 ms at 1.0 and 7.3 to 7.7 ms at 1.5; a heavy scene at
1.5 missed 90 Hz. [sure]

Required: default 1.0; the options page (the slider `Pixel Scale` on the
`Rendering` page, whose description text 06 §5 gives) and the player guide
describe it as a multiplier of the runtime's (SteamVR's) resolution and
warn that raising both multiplies; the cap of §4.1 applies to any value; a change
applies at the next render-target rebuild (§4.2), which the mod requests
when the value changes.

## 11. Data crossing threads

Requirements, whatever the mechanism:

| Data | Written by | Read by | Requirement |
|---|---|---|---|
| "head pose requested" flag | game thread (slot 11) | render thread (slots 5 and 6, which clears it) | a set made before the frame's slot 5 is seen there |
| the render pose (the headset pose slot 11 returned) | game thread | render thread (submit pose) | the submit uses exactly the pose the frame was rendered with; never a torn matrix |
| unconverted yaw-only reference `Y` | game thread (recenter) | render thread (virtual screen placement) | never torn; a recenter shows on the next screen frame |
| whether `Y` belongs to the current tracking space (§7) | game thread (a valid recenter sets it), render thread (a space change clears it) | render thread | a set made with `Y` is seen with that `Y` or a newer one |
| a tracking space change (§6.8) | render thread, after the `WaitGetPoses` that follows the event | game thread (stereo update, queues the retake) | seen once; several changes before the game thread looks count as one |
| controller poses and states | render thread (after `WaitGetPoses`) | game thread | handed over whole (no mix of two frames' data in one read), 04 |
| device pointer at manager +0x04 and device state (+0x04 of the device) | game thread; the render thread when slot 3 detaches the device after a failed OpenXR session (§4.4, 09 §7) | render thread, game thread and job threads | aligned 32-bit atomic stores and loads that are never torn; a value one frame stale is harmless. A call into the runtime that began before a detach is never left using a released runtime (02 §7.4, 12 §3.2) |
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
| `CreateDevice` (the game's method the site calls) | 0x5E0D40 | ? | §6.4, §6.5: the site's only target; no other call or pointer to it (02 §6.1) |
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
| game window procedure | 0x612850 | ? | §9.3, §9.6; registered at 0x612F90 (`RegisterClassExA`); `WM_KILLFOCUS` branch 0x612910 (focus byte 0x1251298 cleared at 0x612917), `WM_ACTIVATE` branch 0x61288F |
| display object and its size handler | object 0x1251288 (display mode at `+0x04`), handler 0x611B90 | ? | §9.3, §9.6 (read only, never patched) |

## 13. Acceptance

Checks with OpenVR (SteamVR) unless noted; the log's events, format and
wording are those of 11; message boxes are those of §6.4 and §6.7.

1. Start with a headset worn: the log shows the runtime chosen, the OpenVR
   start-up steps succeeding, the manufacturer, model and render tangents,
   the first render-target size equal to `2 * trunc(w * p')` by
   `trunc(h * p')` for the logged recommended size, the configuration with
   the same size, and stereo starting. In game the image is sharp (each eye at the
   recommended resolution times the pixel scale).
2. Turn the head
   quickly in gameplay: no lag or swimming beyond the compositor's normal
   reprojection.
3. Set `Pixel Scale` to 2.0 on the options page during play: within a second the log shows
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
   back-buffer size used equal to the window size, and the window made
   borderless with its old size and the monitor's size; the window has no
   title bar or borders and covers the whole monitor; the menus and the
   mouse cursor line up with the picture (a click lands on the item under
   the cursor). A change of pixel scale (a `ResizeBuffers`) does not move or
   restyle it again, and a window resized by the player to fit is not
   moved again.
10. Turning on the game's fullscreen option while stereo runs: the log shows
    the refusal once; the window stays a working mirror. Alt+Enter
    does nothing.
11. Mirror views: right eye (default) fills the window width, cropped top
    and bottom in a 16:9 window; left eye likewise; both eyes side by side
    with bars above and below; symmetric off shows each eye cropped to its
    real FOV, never stretched; menus show the virtual-screen region whole.
    With symmetric off and mirror view right eye, a horizontal edge visible
    only to the right eye (close one eye in the headset to find one)
    appears in the mirror; with both eyes, the left half shows the left
    eye's crop and the right half the right eye's, neither mirrored; a
    circle in the scene stays round in every mode. On a headset whose eyes
    are vertically asymmetric, the cropped mirror shows the same rows the
    headset shows.
12. With runtime None, or SteamVR not installed with OpenVR forced: no
    stereo, the game's fullscreen option works, no swapchain changes are
    logged.
13. SteamVR running with Desktop Game Theatre active: the box of §6.4
    for error 117 appears, its second line reading `[<description> | 117]`
    with SteamVR's description of that error, with the two Desktop Game
    Theatre paragraphs; after OK the game continues without VR.
14. Start the game with SteamVR not running and the headset connected:
    SteamVR starts, the log shows the interface versions obtained, and VR
    works without a restart; the start-up wait ends on the first valid
    headset pose, and the whole wait never exceeds about 10 s, however late
    the compositor appears. With the headset unplugged: the box
    "Failed to initialize SteamVR!" of §6.4 appears within about 10 s, its
    second line showing a non-empty description and a non-zero number,
    and the game runs flat.
15. In gameplay, cover the headset's tracking (or step out of tracking):
    the image holds the last pose, never flips or blanks with invalid
    values. Press the recenter chord while tracking is lost: the reference
    does not change until tracking returns, then the recenter happens once.
    Recentering while looking straight up or down keeps the previous
    forward direction. With the headset's position tracking lost but its
    orientation tracked, the view keeps turning with the head while the
    position holds.
16. Steam's "Use Desktop Game Theatre while SteamVR is active" on for the
    game, SteamVR already running so start-up succeeds: about 3 s after
    the first frames the log names the process SteamVR shows instead of
    the game, and the warning box of §6.7 appears once, its first line
    naming that process; the game keeps running and frames keep reaching
    SteamVR while it is open. With the setting off: the log says the
    headset shows the game, and no warning.
17. Focus: in gameplay with the headset on, click on another window on the
    desktop (or open SteamVR's desktop window, or let a streaming client
    take the focus): the game does not pause, and a loading screen
    started before it completes on its own; the log shows the focus loss
    with the other application's executable. Keyboard and mouse do not
    reach the game until its window is clicked again; the VR controllers
    and a gamepad keep working throughout. With runtime None, the same
    click pauses the game as unmodded.
18. Pose handover: over a 900-frame window of gameplay with fast head
    motion, the pose submitted with each frame is exactly the pose slot 11
    returned for that frame (checked by any means that compares the two
    per frame, for example a comparison in a test build; 11 §4.1 leaves
    this check to this item and defines no event for it); no frame is
    submitted with a torn or newer pose.
19. Start-up placement: start the game with the headset worn and SteamVR
    already running, `SkipStartupIntro` off. From the first logo video on,
    every start-up video, the legal screen, the title video and the main
    menu appear on the same level screen 1.5 m ahead at the head height of
    the start-up recenter; the log shows the start-up recenter and
    `frames now on the screen` before the first video, without
    `DebugOptions`.
20. Tracking space change: on the title screen or the main menu, reset the
    standing (or seated) zero pose from SteamVR, or let the headset
    recenter the play area (on a Quest, hold the Oculus button), while the
    headset is worn: the log names the event, the screen goes black for a
    moment and comes back 1.5 m in front of where the head faces now, with
    one recenter logged after the event. With the headset lying on a desk
    during the event, the screen stays black until the headset is worn,
    then appears in front of it. In gameplay the same event re-centres
    forward to where the player faces.
21. Universe: with the application's tracking space reported as seated
    (any runtime or tool that starts it so), the virtual screen still
    appears in front at head height, not offset by the difference between
    the seated and standing origins.

## 14. Open questions

1. **2020 build.** The 2020 RVAs above were taken from the earlier mod's
   table and have not been verified against a 2020 `AI.exe`; renderer initialisation, the
   size query call, the D3D11 creation call, the rebuild test and the
   reset-orientation method have no 2020 address. To resolve: find the
   2015 byte patterns of 01 §4 in the 2020 binary.
2. **Resolved in r6.** The game's handling of window size changes in its
   fullscreen mode is now traced (§9.3). Still open: the meaning of the
   display mode values other than 0 and 2 (§9.3 marks 0 and 2 [likely]).
   To resolve: read the writers of display object `+0x04`.
3. **Game reaction to the fullscreen refusal** (§9.4): whether the video
   options keep showing "Fullscreen" and whether the game retries. To
   resolve: turn fullscreen on at start and while stereo runs and read the
   options page and the log.
4. **A sharper desktop picture for screen frames** (§8.1). The game lays
   out its menus in a 1280x720 UI space (05 §7.7). Whether it renders them
   into a target of its own that can be found through the renderer when
   slot 6 runs in a screen frame, which would let the desktop mirror show
   menus sharper than the virtual-screen region, is not known. Until it
   is, the region of §8.1 is required. To resolve: trace the render targets
   the game's UI drawing binds. If one can be found, the spec gives it as a game fact with its
   address and format and keeps the region as the fallback.
5. **Which game code consumes slot 13's vertical FOV and aspect** (§3.2):
   symmetric frusta are required on the assumption that such code expects
   a centred frustum. To resolve: trace the callers of the slot 13 wrapper
   (RVA 0x5B86C0).
