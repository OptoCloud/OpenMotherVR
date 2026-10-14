<!-- Provenance: AI.exe 2015 analysis (order of StereoInit, the render-target size query, D3D11 device creation and device slot 3, 01 §2.1, 02 §6); OpenXR 1.0 specification and the XR_KHR_D3D11_enable and XR_EPIC_view_configuration_fov extensions; the OpenXR loader's Windows registry convention; observation on the test machine (SteamVR registers only a 64-bit runtime manifest and installs a 32-bit sibling); in-game testing of an earlier rebuild on SteamVR's OpenXR runtime (SteamVR/OpenXR 2.17.10) with a Valve Index headset and controllers; user requirements. Revision: r1 -->

# 09: Running the stereo device on OpenXR

The stereo device of 02 and 03 driven by an OpenXR runtime instead of
OpenVR. Everything the game sees is unchanged: the same device layout, the
same head-pose convention (03 §1), the same symmetric render FOV (03 §3.2),
the same side-by-side target (03 §4) and the same frame sequence (03 §6).
This document gives what differs: when the OpenXR objects can be created,
how the FOV is obtained, how frames are submitted and paced, which spaces
are used, and how a 32-bit process finds the runtime.

Runtime value 1 selects OpenXR (06 §1); the menu's former "Oculus" slot,
a config file saying `"Oculus"` and `-vrforceoculus` all mean OpenXR.

**Status: verified in game** on SteamVR's OpenXR runtime with a Valve Index
headset and Index controllers: stereo, head tracking, the virtual screen,
input, recentering and the FOV fallback of §4.2. Other OpenXR runtimes
(Meta Quest Link, WMR, Pico, Varjo) follow the same API and are expected to
work; they are not yet verified.

## 1. Finding a runtime from a 32-bit process

`AI.exe` is a 32-bit process, so the OpenXR loader in it needs a runtime
library built for x86. The loader looks for the active runtime under
`HKLM\SOFTWARE\Khronos\OpenXR\1`, value `ActiveRuntime` (a path to the
runtime's JSON manifest), in the registry view of the process, which for a
32-bit process is the 32-bit view (`WOW6432Node`). The environment
variable `XR_RUNTIME_JSON`, when set, overrides the registry.

Observed: SteamVR registers its runtime only in the 64-bit view
(`...\SteamVR\steamxr_win64.json`). The loader in a 32-bit process then finds
no runtime. SteamVR does install a 32-bit manifest next to it,
`steamxr_win32.json`, whose `runtime.library_path` (`bin\vrclient.dll`,
relative to the manifest) is an x86 DLL. Meta's runtime follows the same
pattern with `oculus_openxr_64.json` / `oculus_openxr_32.json`.

Required, before the first OpenXR call (at CreateDevice, never at DLL
attach, 01 §2.2):

1. If `XR_RUNTIME_JSON` is already set, leave it: the player chose.
2. Else, if the 32-bit view has an `ActiveRuntime` whose manifest's library
   is an x86 PE file (`IMAGE_FILE_HEADER.Machine` = 0x014C), do nothing: the
   loader finds it.
3. Else read `ActiveRuntime` from the 64-bit view and try sibling manifests
   in the same folder, made by replacing in the file name the last `win64`
   by `win32`, `x64` by `x86`, or `64` by `32`. Take the first that exists
   and whose library (resolved relative to the manifest when relative) is
   x86, and set `XR_RUNTIME_JSON` to its full path for this process, in a
   way the loader reads (the process environment, and the C runtime's copy
   if the loader is linked against a C runtime that caches it).
4. Otherwise there is no usable runtime: CreateDevice fails, the game runs
   flat, and the player is told that the active OpenXR runtime has no 32-bit
   support (00 §2.7).

Log the manifest and library chosen and the machine type found; this is the
first thing to check when "OpenXR does not start" is reported.

## 2. Creation order: the session must wait for the game's device

### 2.1 What the game does, in order [sure]

From renderer initialisation (01 §2.1, 02 §6):

1. `StereoInit(1)` at `StereoInitCall` (RVA 0x613941): the mod's CreateDevice
   runs and must fill the device descriptor, including the FOV (02 §6.1).
2. The back-buffer size query (call at RVA 0x613AE8): the stereo
   render-target size is asked for and the game sizes its stereo target and
   swapchain from the answer (02 §6.2, 03 §4.2).
3. `D3D11CreateDeviceAndSwapChain` (call at RVA 0x613CB8 into RVA
   0x626FA0): the game's `ID3D11Device` first exists here.
4. Later, device slot 3 (configure rendering) with the stereo target; the
   device is available through `RendererDevice` (01 §4.3).

### 2.2 Consequence

An OpenXR session for Direct3D 11 is created with the application's
`ID3D11Device` (`XrGraphicsBindingD3D11KHR`). In steps 1 and 2 that device
does not exist, yet the game needs the FOV (step 1) and the recommended size
(step 2) then. So the work is split:

| When | OpenXR work |
|---|---|
| CreateDevice (step 1) | §1 discovery; `xrCreateInstance` with `XR_KHR_D3D11_enable`, plus `XR_EPIC_view_configuration_fov` when `xrEnumerateInstanceExtensionProperties` lists it; `xrGetSystem` (head-mounted display form factor); `xrEnumerateViewConfigurationViews` for `XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO`: the recommended per-eye size and, with the extension, the FOV (§4) |
| render-target size (step 2) | answer from the recommended size: eye size `(trunc(w * p), trunc(h * p))` with `w`, `h` = `recommendedImageRectWidth/Height` of view 0, target `2 * trunc(w * p)` by `trunc(h * p)` (03 §4.1) |
| configure rendering (step 4, first call only) | `xrGetD3D11GraphicsRequirementsKHR` (required before a session); compare its adapter LUID with the game device's adapter (`IDXGIDevice::GetAdapter`, `GetDesc`) and log a warning if they differ (hybrid-GPU laptops); `xrCreateSession` with the game's device; the reference spaces (§6); `xrEnumerateSwapchainFormats`; the input action set (04 §5.8.4). A repeated configure must not create a second session |

If `xrCreateInstance` or `xrGetSystem` fails (runtime not running, headset
off), CreateDevice fails: no device, flat play (02 §6.1). If session
creation fails at step 4, the device stays (the game has already sized for
stereo) but nothing is submitted; the desktop mirror keeps working, and the
failure is logged and shown once.

Observed: on SteamVR the session was created on the game's device at
configure rendering and began running within half a second.

## 3. Session life cycle

Poll events (`xrPollEvent`) once per frame, in device slot 6 before the
frame calls of §5:

| Event | Required |
|---|---|
| session state `READY` | `xrBeginSession` with `XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO`; the session is now running |
| session state `STOPPING` | `xrEndSession`; not running |
| session state `EXITING` or `LOSS_PENDING`, or instance loss pending | not running for the rest of this run; the game continues, flat on the desktop mirror; log it |
| interaction profile changed | re-read the controller profile (04 §5.8.4) |
| reference space change pending | §6.3 |

While the session is not running, slot 6 does not call the frame functions
and VR input is cleared (04 §5.1). The desktop Present then uses sync
interval 1 instead of 0, so that the game does not spin without the
runtime's pacing. At game exit, end the session if running and destroy the
session and the instance.

## 4. FOV and recommended size

### 4.1 From the extension

With `XR_EPIC_view_configuration_fov`, chain an
`XrViewConfigurationViewFovEPIC` to each `XrViewConfigurationView` of the
enumeration; its `recommendedFov` gives each eye's angles. In the tangent
terms of 03 §3.1, for eye e:

| Extent | Value |
|---|---|
| up | `tan(angleUp)` |
| down | `tan(-angleDown)` |
| left | `tan(-angleLeft)` |
| right | `tan(angleRight)` |

Then 03 §3.2 applies unchanged: `maxV`, `maxH`, EyeFov, MaxEyeFov and
DefaultEyeFov.

### 4.2 Without it: provisional FOV, replaced after the first frame

Observed: SteamVR's OpenXR runtime does not offer the extension. Core
OpenXR reports the FOV only from `xrLocateViews`, which needs a running
session, so it is not known at CreateDevice. Required:

1. At CreateDevice, use a **provisional FOV** of tangent 1.2 in all four
   directions for both eyes (about 100 degrees per axis), as DefaultEyeFov,
   MaxEyeFov and EyeFov.
2. On the first frame whose `xrLocateViews` reports a valid orientation,
   take each view's `fov` (converted as in 4.1) and replace the provisional
   values, once:
   - DefaultEyeFov (descriptor +0x3C) with each eye's real tangents;
   - MaxEyeFov (descriptor +0x5C) and the device's **EyeFov** (+0xAC, both
     eyes) with the new symmetric `{maxV, maxV, maxH, maxH}`. The game's
     device slots 12, 13 and 14 read EyeFov on every call (02 §5.1), so the
     next frame renders with the real FOV without any other game change;
   - the desktop mirror's crop bounds and constants (03 §3.3, §8), which
     depend on the tangents;
   - the FOV declared in the projection layer (§5.2).
3. What stays: the render-target size and the eye viewports (they come from
   the recommended size, not the FOV, and the game has already created its
   target); the pixel aspect of an eye half then differs from
   `maxH / maxV`, which 03 §3.2 allows.

Frames rendered before the change are declared with the provisional FOV
they were rendered with, so they show correctly, only at another sampling
density. Observed on Index: provisional `v = h = 1.2`, replaced in the
first frames by `v = 1.214`, `h = 1.154`.

The recommended size needs no session: it comes from
`xrEnumerateViewConfigurationViews` in either case (observed: 2688 x 2688
per eye on the Index test setup).

## 5. Frames

### 5.1 The frame sequence, mapped to 03 §6

| Step | OpenXR |
|---|---|
| slot 11 (head pose) | the head pose is the VIEW space located in the base space (§6) at the current OpenXR frame's predicted display time, converted with 03 §1 (OpenXR spaces are right-handed, +Y up, -Z forward, like OpenVR's). Remember this frame's **render views**: both eyes' poses and FOVs as last located (§5.3) |
| slot 5 (begin frame) | controller input from the last action sync (04 §5.8.4); eye offsets (5.4) |
| slot 6 (end frame) | §5.3: wait, begin, locate, sync, submit the frame the game just rendered, end; then the desktop mirror and `Present(0, 0)` as in 03 §6 |

### 5.2 What is submitted

**Head-tracked frame** (the game asked for a head pose, 03 §6): one
projection layer (`XrCompositionLayerProjection`) in the base space with two
views. View e shows eye e's half of the side-by-side target, and declares:

- **pose**: eye e's pose from the render views, the pose the game rendered
  that half with, not a newer one; the compositor reprojects from it;
- **fov**: the **symmetric FOV the game rendered with**, for both eyes:
  `angleLeft = -atan(maxH)`, `angleRight = atan(maxH)`,
  `angleUp = atan(maxV)`, `angleDown = -atan(maxV)`.

Unlike OpenVR (03 §3.3) no crop bounds are needed: the whole half is
declared with the FOV it covers and the compositor maps it to the eye's
real FOV. Poses are in the runtime's metres; the world scale `s` applies
only to what the game receives (03 §1, §5), never to layer poses.

**Screen frame** (menus, videos, loading, 03 §7): no projection layer; one
quad layer (`XrCompositionLayerQuad`), `XR_EYE_VISIBILITY_BOTH`, in the base
space, showing the same region as the OpenVR overlay: eye 0's half, middle
half vertically (`u 0..0.5, v 0.25..0.75`). Its pose is the yaw-only
reference pose of the latest recenter (03 §2.1, here in the base space)
moved 1.5 m along its horizontal forward axis at the reference's head
height, facing back towards it; its size is 32/9 m wide (3.5556 m) and
`width * regionHeight / regionWidth` high. Nothing else is shown, so the
view is black around the screen.

**Images.** OpenXR layers take images from OpenXR swapchains. Copy the
needed region of the game's stereo target (device +0x158) into an acquired
swapchain image (`xrAcquireSwapchainImage`, `xrWaitSwapchainImage`,
`ID3D11DeviceContext::CopySubresourceRegion` on the game's immediate
context, `xrReleaseSwapchainImage`) and point the layer's sub-image at it.
One swapchain per eye, or one twice as wide with an image rectangle per
eye, are both acceptable; the screen quad has its own. Swapchains are
created with usage colour attachment and transfer destination, one sample,
one mip, and recreated when the region size changes.

**Format.** The swapchain format must be in the same typeless family as the
game's target (a copy requires it; the game's target is an 8-bit RGBA or
BGRA UNORM format). Prefer the `_SRGB` variant: the game writes
display-ready, gamma-encoded values (03 §6 submits them to OpenVR as
`ColorSpace_Gamma`), and storing the same bytes in an sRGB swapchain tells
the compositor exactly that; a plain UNORM swapchain would be shown with the
wrong gamma. Fall back to the UNORM variant only if the runtime offers no
sRGB one. Observed: SteamVR offers `DXGI_FORMAT_B8G8R8A8_UNORM_SRGB`.

### 5.3 Frame pacing and the order inside slot 6

Each slot 6, while the session runs:

1. `xrWaitFrame`: blocks until the runtime wants the next frame. This is
   the game's frame pacing, the equivalent of OpenVR's `WaitGetPoses` (03
   §6); nothing else in the mod waits.
2. `xrBeginFrame`.
3. `xrLocateViews` for the primary stereo configuration in the base space at
   the new frame's `predictedDisplayTime`, and the VIEW space likewise:
   these are the poses the game's **next** frame reads (§5.1). A view state
   without a valid orientation leaves the previous views in use and is not
   used for the FOV of §4.2.
4. `xrSyncActions` and the controller states (04 §5.8.4), for the next
   frame's input.
5. If `shouldRender` is set: copy the images and build the layer of §5.2
   for the frame the game **just rendered**, declared with that frame's
   render views. Otherwise no layer.
6. `xrEndFrame` with the new frame's `predictedDisplayTime`, the
   environment blend mode the system reports first (opaque on headsets) and
   zero or one layer.

The submitted image is one OpenXR frame older than the display time it is
shown at; declaring the pose it was rendered with lets the compositor
correct for that. Every `xrWaitFrame` is matched by `xrBeginFrame` and
`xrEndFrame` in the same slot 6.

### 5.4 Eye offsets

At slot 5, for each eye, the offset of the eye from the head in head
coordinates: the eye's position minus the head's position (both from the
render views, in the base space), rotated by the inverse of the head's
orientation. ViewAdjust is then set from these exactly as 03 §5 does with
`GetEyeToHeadTransform` (eye 0 takes the right eye's offset times `s`, eye
1 the left eye's). Until views are valid, use ±0.032 m in X. Observed: IPD
0.066 m on Index, depth correct.

## 6. Reference spaces and recentering

### 6.1 Spaces

- **Base space**: a `LOCAL` reference space (seated: its origin is the
  runtime's seated origin, its +Y is up). Head, eye views, controllers and
  every layer are expressed in it, so they agree with each other.
- **VIEW** reference space: the head, located in the base space.
- The base space may instead be a `LOCAL` space created with a pose that
  re-anchors it at the head's yaw and position when the session first
  tracks; everything then still uses the one base space. (The verified
  build did this once, at the first tracked frame.)

### 6.2 Game recentering

Recentering works as in 03 §2: the reference pose the game reads is the
yaw-only head pose in the base space, taken by the same triggers (03 §2.2,
04 §8). It does not re-create OpenXR spaces. The virtual screen's quad
(5.2) is placed from that same reference.

At CreateDevice no session exists and no head pose is available: the
reference starts neutral (02 §3.3) and a retake is queued (03 §2.2), which
fires on the first frame where the headset is in use.

"Headset in use" under OpenXR: the session is running, the located views
have a valid, tracked orientation, and the head is upright (the world-Y
component of the head's up axis exceeds 0.7, as in 03 §2.3). Where the
runtime offers `XR_EXT_user_presence`, its "user present" state is also
required. Observed: the first recenter happened as the session started
running, with the headset worn.

### 6.3 Runtime recentering

When the player recenters in the runtime's own UI, the runtime moves the
`LOCAL` origin and sends `XrEventDataReferenceSpaceChangePending` for
`LOCAL`. Required: treat it as a recenter request (04 §8): queue a retake of
the game's reference, executed on a frame at or after the event's
`changeTime`, so the game's forward follows the player's new forward and
the virtual screen is placed in front of them.

## 7. Failures

| Situation | Behaviour |
|---|---|
| no 32-bit runtime (§1) | no device, flat play, told once |
| `xrCreateInstance` / `xrGetSystem` fails | no device, flat play, told once with the OpenXR result |
| session creation fails (§2.2) | device kept, nothing submitted, mirror works, told once |
| the game renders on a different adapter than the runtime wants | warning logged; continue (the runtime may still work, or fail at session creation) |
| no swapchain format compatible with the game's target | no layer submitted, logged once |
| session lost or exiting (§3) | VR stops for this run; the game continues on the mirror |
| no interaction profile bound | no VR input; gamepad, keyboard and mouse keep working |
