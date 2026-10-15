<!-- Provenance: AI.exe 2015 analysis (order of StereoInit, the render-target size query, D3D11 device creation and device slot 3, 01 §2.1, 02 §6; the game's quit flag); 2020 values were taken from the earlier mod's per-build address table (and, for inline sites, its installer) and have not been verified against a 2020 `AI.exe`; OpenXR 1.0 specification and the XR_KHR_D3D11_enable, XR_EPIC_view_configuration_fov, XR_OCULUS_audio_device_guid, XR_KHR_loader_init and XR_EXT_loader_init_properties extensions, and the OpenXR registry (symbolic names of XrResult values); the OpenXR loader's Windows registry convention and its documented handling of elevated processes; observation by the spec side on the test machine (SteamVR registers only a 64-bit runtime manifest and installs a 32-bit sibling); in-game testing and test logs by the spec side on SteamVR's OpenXR runtime (SteamVR/OpenXR 2.17.10) with a Valve Index headset and controllers (session creation, FOV fallback, frame boundary with pose age 0, recentering); performance measurements by the spec side (frame boundary placement, controller latency, a frame loop that rendered with views one period old); user requirements; a tester's report on SteamVR with a Steam Frame headset (XR_ERROR_RUNTIME_UNAVAILABLE at instance creation, an empty failure text, no visible way back to SteamVR, the game not shown in the headset with Desktop Game Theatre on); project decisions, each with its reason inline (r4: runtime discovery prefers the 64-bit view's runtime, the not-shown check; r5: probing has no effect beyond the log and the loader is directed only when OpenXR starts, expandable registry values are expanded, a re-anchor looking straight up or down keeps the previous yaw, per-frame call failures are logged per change of result, a session or instance loss never quits the game, the failure message's content as an unordered list of facts). Revision: r5 -->

# 09: Running the stereo device on OpenXR

The stereo device of 02 and 03 driven by an OpenXR runtime instead of
OpenVR. Everything the game sees is unchanged: the same device layout, the
same head-pose convention (03 §1), the same symmetric render FOV (03 §3.2),
the same side-by-side target and size rules (03 §4), the same frame
sequence and threads (03 §6.1), the same desktop mirror and swapchain
handling (03 §8, §9). This document gives what differs: how a 32-bit
process finds the runtime, what the OpenXR API requires about when objects
can be created, how the FOV is obtained, how frames are paced and
submitted, which space poses are expressed in, how the session's states
are handled, how invalid poses are treated, and what happens on failure.

Runtime value 1 selects OpenXR (06 §1); the menu's former "Oculus" slot,
a config file saying `"Oculus"`, `-vrforceoculus` and `-vrforceopenxr` all
mean OpenXR.

**Status: verified in game** on SteamVR's OpenXR runtime with a Valve Index
headset and Index controllers: stereo, head tracking, the virtual screen,
input, recentering, the FOV fallback of §4.2 and the frame boundary of §5.3
(pose age 0). Other OpenXR runtimes (Meta Quest Link, WMR, Pico, Varjo)
follow the same API and are expected to work; they are not yet verified.

## 1. Finding a runtime from a 32-bit process

`AI.exe` is a 32-bit process, so the OpenXR loader in it needs a runtime
library built for x86. The loader looks for the active runtime under
`HKLM\SOFTWARE\Khronos\OpenXR\1`, value `ActiveRuntime` (a path to the
runtime's JSON manifest), in the registry view of the process, which for a
32-bit process is the 32-bit view (`WOW6432Node`). The environment
variable `XR_RUNTIME_JSON`, when set, overrides the registry, except in an
elevated process, where the loader ignores the environment. [sure]

Observed: SteamVR registers its runtime only in the 64-bit view
(`...\SteamVR\steamxr_win64.json`). The loader in a 32-bit process then finds
no runtime. SteamVR does install a 32-bit manifest next to it,
`steamxr_win32.json`, whose `runtime.library_path` (`bin\vrclient.dll`,
relative to the manifest) is an x86 DLL. Meta's runtime follows the same
pattern with `oculus_openxr_64.json` / `oculus_openxr_32.json`. [sure for
SteamVR, likely for Meta]

Observed by the spec side: SteamVR's "set as OpenXR runtime" writes only
the 64-bit view [sure for SteamVR]. Whether other runtimes do the same is
not observed (open question 1). A 32-bit entry that differs from the
64-bit one is therefore not necessarily the runtime the player last chose.
Reported by a tester on SteamVR with a Steam Frame headset:
with OpenXR selected, instance creation failed with
`XR_ERROR_RUNTIME_UNAVAILABLE` (-51) while SteamVR, the active runtime, has
a usable x86 build; a 32-bit entry that the loader took in preference to
it is the likely cause. [likely]

**Required result.** When the runtime is first chosen (06 §1: when the
stereo device is first created, never at DLL attach, 01 §2.2), the mod
decides whether OpenXR is available, and if it is, the loader in this
process loads an x86 build of the runtime the player made active. A
manifest is **usable** when the file exists, parses as JSON, has a string
`runtime.library_path`, and that library (resolved relative to the
manifest's folder when relative) is a PE file whose
`IMAGE_FILE_HEADER.Machine` is 0x014C (x86).

Both views' `ActiveRuntime` values are read (a `REG_SZ` or `REG_EXPAND_SZ`
path of whatever length the registry returns; either may be absent). A
`REG_EXPAND_SZ` value has its environment references expanded before use,
as the loader itself does, so a manifest registered as
`%ProgramFiles%\...` is found. The manifest chosen:

| Situation | Result |
|---|---|
| `XR_RUNTIME_JSON` is set and not empty | the player chose: it is left as it is, nothing else is passed to the loader, and OpenXR counts as available |
| the 64-bit view names a manifest that is itself usable (its library is x86) | that manifest |
| the 64-bit view names a manifest that is not usable, and a usable x86 sibling of it exists in the same folder | that sibling |
| neither of the above, and the 32-bit view names a usable manifest | the 32-bit view's manifest |
| none of the above (no registration in either view, or nothing usable) | OpenXR is not available. When OpenXR was preferred but not forced, another available runtime may be chosen instead (06 §1); when forced, there is no VR |

Why this order (project decision): the 64-bit view is the one the player's
last "make active" wrote, so its runtime is the one the player expects to
run; the 32-bit view is only a fallback for a runtime that registers
itself for 32-bit processes and has no 64-bit entry, or whose 64-bit entry
has no x86 build.

Finding the sibling: it must find at least SteamVR's `steamxr_win32.json`
next to `steamxr_win64.json` and Meta's `oculus_openxr_32.json` next to
`oculus_openxr_64.json`. How candidates are derived and ranked is the
implementation's choice, as long as only usable manifests are taken.

**Probing changes nothing.** Deciding whether OpenXR is available (above)
reads the registry and the manifests and logs what it found, and has no
other effect on the process. Why: the environment variable below reaches
every child process the game starts and every later OpenXR loader in the
process, so setting it for a runtime that will not start would point them
at a runtime the player did not choose for them.

**Handing the choice to the loader.** Only when OpenXR is the runtime that
will start (06 §1.1), the loader must load the chosen manifest (not the
32-bit view's entry) whether or not the process is elevated. So the chosen
manifest's full path is given to the loader in two ways:

- as `XR_RUNTIME_JSON` in the environment the loader reads: the process
  environment, and also the C runtime's own copy of the environment when
  the loader is linked statically against a C runtime that keeps one;
- as the loader property `XR_RUNTIME_JSON` through the loader's
  initialisation (`xrInitializeLoaderKHR` of `XR_KHR_loader_init`, with an
  `XrLoaderInitInfoPropertiesEXT` of `XR_EXT_loader_init_properties`),
  because the loader honours its properties in an elevated process, where
  it ignores the environment. The OpenXR specification requires
  `xrInitializeLoaderKHR` to be called before any other loader call except
  `xrGetInstanceProcAddr` (which is how it is obtained), so it precedes
  `xrEnumerateInstanceExtensionProperties` and `xrCreateInstance`. A loader
  that does not offer it, or refuses the property, is logged and start-up
  continues with the environment alone.

Logged (11 §2.3): both views' `ActiveRuntime` values (or that a view has
none); each manifest examined with the library it resolves to and that
library's PE machine; the manifest chosen and which rule chose it, or why
none was usable; when OpenXR starts, whether the loader property was
accepted and, once, that the process is elevated when it is (an elevated
game is the case where the environment alone would not reach the loader).
This is the first thing to
check when "OpenXR does not start" is reported.

## 2. Creation: the session must wait for the game's device

### 2.1 What the game does, in order [sure]

From renderer initialisation (01 §2.1, 02 §6), all on the game thread:

1. `StereoInit(1)` at `StereoInitCall` (RVA 0x613941, 2020: 0x565E11): the
   mod's CreateDevice runs and must fill the device descriptor, including the
   FOV (02 §6.1).
2. The back-buffer size query (call at RVA 0x613AE8): the stereo
   render-target size is asked for at `StereoRenderTargetSizeCall` (RVA
   0x60FD64, 2020: 0x562244) and the game sizes its stereo target from the
   answer (02 §6.2, 03 §4.2).
3. `D3D11CreateDeviceAndSwapChain` (call at RVA 0x613CB8 into RVA
   0x626FA0): the game's `ID3D11Device` first exists here.
4. Later, on the render thread, device slot 3 (configure rendering) with the
   stereo target; the device is available through `RendererDevice` (RVA
   0x621E20, 2020: 0x574310, 02 §8).

### 2.2 Consequence and the required split

An OpenXR session for Direct3D 11 is created with the application's
`ID3D11Device` (`XrGraphicsBindingD3D11KHR`). In steps 1 and 2 that device
does not exist, yet the game needs the FOV (step 1) and the recommended
size (step 2) then. So the work is split by what must be known when:

| When | What must exist or be known by then |
|---|---|
| CreateDevice (step 1), first call only | the runtime chosen (§1); an instance (`xrCreateInstance`, API version 1.0, application and engine name and version of the implementation's choosing) with `XR_KHR_D3D11_enable` (required), plus `XR_EPIC_view_configuration_fov` and `XR_OCULUS_audio_device_guid` each when `xrEnumerateInstanceExtensionProperties` lists it; the HMD system (`xrGetSystem`, `XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY`); the two views of `XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO` (`xrEnumerateViewConfigurationViews`, with an `XrViewConfigurationViewFovEPIC` chained to each when that extension is enabled): the recommended per-eye size and, with the extension, the FOV (§4). With the audio extension, the headset's audio endpoint ID (`xrGetAudioOutputDeviceGuidOculus`), handed to 10 §1.1; a failure leaves the ID empty and is not a start-up failure. Logged: the runtime's name and version (`xrGetInstanceProperties`) and the system's name (`xrGetSystemProperties`) |
| CreateDevice, before it returns | the descriptor: DefaultEyeFov, MaxEyeFov and EyeFov from §4; ProductName (+0x18) and Manufacturer (+0x1C) as below; the other header fields as 03 §3.2. State 1; attach; take the first reference pose (§6.2) |
| render-target size (step 2) | answered from the recommended size: `w`, `h` = `recommendedImageRectWidth/Height` of view 0, used for both eyes, then 03 §4.1 (cap and effective pixel scale included) |
| configure rendering (step 4), until a session exists | the session (`xrCreateSession` with the game's device), its spaces (§6.1), the environment blend mode, the swapchain format list, and the input action set (04 §5.8.4). A configure after a session exists creates nothing |

Dependencies the OpenXR API imposes, and nothing more: the loader
initialisation of §1 comes before every other loader call (the
specification requires it); everything else needs the instance; the
system and everything after it need `xrGetSystem`;
`xrGetD3D11GraphicsRequirementsKHR` must be called before `xrCreateSession`
(the extension requires it); reference spaces, `xrEnumerateSwapchainFormats`
and attaching the action set need the session. The audio endpoint ID needs
only the instance, and the environment blend modes need only the instance
and the system. Within these dependencies the order is free.

The environment blend mode is the first one `xrEnumerateEnvironmentBlendModes`
reports for the primary stereo configuration (the runtime lists its
preferred mode first), or opaque if it reports none. Before creating the
session, the adapter LUID from the graphics requirements is compared with
the game device's adapter (`IDXGIDevice::GetAdapter`,
`IDXGIAdapter::GetDesc`); a difference (hybrid-GPU laptops) is logged as a
warning and creation continues, because some runtimes still work across
adapters and the session call reports the cases that do not.

ProductName and Manufacturer: 02 allows null here and the game does not
depend on their content; the descriptor should still describe the
headset, so any descriptive strings that live as long as the device are
acceptable, for example the system name and the runtime name.

Observed: on SteamVR the session was created on the game's device at
configure rendering and began running within half a second.

### 2.3 Failures during creation

| Failure | Required |
|---|---|
| OpenXR not available (§1) | runtime choice of 06 §1; with OpenXR forced or nothing else available, no device: flat play |
| the instance, the system or the view configuration cannot be obtained at CreateDevice (runtime not running, headset off or asleep) | destroy what was created; no device (manager +0x04 null, state 0), flat play on the stereo path's no-device behaviour (02); tell the player once (§7) |
| the graphics requirements, the session or the spaces of §6.1 cannot be obtained at configure rendering | destroy the session and the instance; **detach** the device (state 0, manager +0x04 null) so the game stops rendering in stereo; tell the player once (§7). VR stays off for this run, also on later configures |
| the input action set cannot be created | log it; continue without VR controller input (gamepad, keyboard and mouse keep working) |

Why the detach: the game has already sized its target for stereo, but with
no session nothing reaches the headset and nothing paces the game. Leaving
the device attached would render both eyes at headset resolution into a
desktop mirror only. Detached, the game's stereo wrappers see no device and
the game renders and presents flat (02), and the swapchain rules of 03 §9
stop applying.

## 3. Session life cycle

Events (`xrPollEvent`, until it returns no event) are polled on the render
thread in device slot 6, before every `xrWaitFrame` (§5.3). Why: a state
change (stopping, exiting, loss) must be seen before `xrWaitFrame` is
called for a session that may no longer run.

| Event | Required |
|---|---|
| session state `READY` | `xrBeginSession` with `XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO`; the session is running only if that succeeds (logged either way) |
| session state `STOPPING` | not running; `xrEndSession` (logged). A later `READY` begins it again |
| session state `EXITING` | OpenXR's meaning: the application should exit (for example the player quit from the runtime's dashboard). Not running and **lost** for the rest of this run; the render thread sets the game's quit flag (`QuitRequested`, RVA 0x17DA920, 2020: 0x17EAAD0, byte = 1, 06 §4.5) no later than the next frame, so the game exits through its own path; controller state is no longer handed to the game (04). Logged |
| session state `LOSS_PENDING`, or `XrEventDataInstanceLossPending` | not running and lost for the rest of this run (logged). The game is not asked to quit: unlike `EXITING`, a loss is not a request to exit (the runtime failed or went away), and quitting would cost the player unsaved progress while the game can keep running on the mirror |
| `XrEventDataInteractionProfileChanged` | re-read the controller profile (04 §5.8.4) |
| `XrEventDataReferenceSpaceChangePending` | re-anchor the base space before the next frame's poses are located (§6.3) |
| other states (`IDLE`, `SYNCHRONIZED`, `VISIBLE`, `FOCUSED`) | nothing beyond the log entry below and the visibility check of §3.1; frames continue while running (input focus is 04's) |

Every session state change, whatever the state, is logged with the new
state (11 §2.4): the sequence of states is what shows whether, and how far,
the runtime brought the game into the headset.

While the session is not running (before `READY`, after `STOPPING`, or
lost), and from the moment exit is requested, every VR controller input the
game sees reads as released and centred (all buttons up, sticks and
triggers 0; 04 §5.1): the last synced state must not keep being handed to
the game. No OpenXR frame function is called, nothing is submitted and
nothing waits in the mod: the game is paced only by itself, and slot 6
still draws the desktop mirror and presents with sync interval 0 (03 §6.2).
The device stays attached in all these states, so the game keeps rendering
in stereo and the mirror keeps working (open question 2).

The session and instance are destroyed only when the device gives up its
runtime (§2.3). At process exit nothing is called: ending a session or
destroying an instance there would run under the loader lock and can
deadlock on runtime threads; the runtime handles a process that exits with
a session open.

### 3.1 A running session the runtime never shows

A session that is running but never reaches `VISIBLE` or `FOCUSED` is not
shown in the headset: the game is submitting frames that the runtime does
not display. With SteamVR the usual cause is its Desktop Game Theatre,
which shows the game's desktop window on a flat screen in the headset
instead of the game's own frames (reported by a tester on SteamVR with a
Steam Frame headset; the same cause as OpenVR's check in 03 §6.7).

Required: when a session has been running for a bounded time without the
runtime ever making it `VISIBLE` or `FOCUSED`, the mod logs it with the
current state (11 §2.4) and shows the headset-not-showing-the-game warning
of 13 §5.3, at most once per process. A session that has been visible is
never reported, even if it leaves those states later (taking the headset
off, the dashboard). The bound is a project decision: about 10 s, long
enough for a runtime's own loading and transition screens, short enough
that the player still connects the warning with the empty headset. It is
measured so that a change of the wall clock cannot trigger it.

The warning must block neither the render thread nor the game thread: it
is shown without waiting for the player (13 §5.2).

## 4. FOV and recommended size

### 4.1 From the extension

With `XR_EPIC_view_configuration_fov`, the `XrViewConfigurationViewFovEPIC`
chained to each `XrViewConfigurationView` gives `recommendedFov` for each
eye. In the tangent terms of 03 §3.1, for eye e:

| Extent | Value |
|---|---|
| up | `tan(angleUp)` |
| down | `tan(-angleDown)` |
| left | `tan(-angleLeft)` |
| right | `tan(angleRight)` |

Then 03 §3.2 applies unchanged: `maxV`, `maxH`, EyeFov, MaxEyeFov and
DefaultEyeFov.

### 4.2 Without it: provisional FOV, replaced once views are located

Observed: SteamVR's OpenXR runtime does not offer the extension. Core
OpenXR reports the FOV only from `xrLocateViews`, which needs a running
session, so it is not known at CreateDevice. Required:

1. At CreateDevice, use a **provisional FOV** for both eyes in all four
   directions, as DefaultEyeFov, MaxEyeFov and EyeFov: a typical headset
   FOV, tangent about 1.2 (about 100 degrees per axis). Any plausible value
   works, because it lasts only until the first located views and frames
   rendered with it are declared with it (below). Logged: that the FOV was
   not reported and the provisional tangents are in use.
2. At the first begin frame (slot 5, render thread) at which the latest
   located views (§5.3) are valid, take each view's `fov` (converted as in
   4.1), once:
   - DefaultEyeFov (descriptor +0x3C) with each eye's real tangents;
   - MaxEyeFov (descriptor +0x5C) and the device's **EyeFov** (+0xAC, both
     eyes) with the new symmetric `{maxV, maxV, maxH, maxH}`. The game's
     device slots 12, 13 and 14 read EyeFov on every call (02 §5.1), so the
     frame begun now renders with the real FOV without any other game
     change;
   - the FOV declared in the projection layer (§5.2), from the frame begun
     now.
   Logged: the new render tangents.
3. What stays: the render-target size and the eye viewports (they come from
   the recommended size, not the FOV, and the game has already created its
   target); the pixel aspect of an eye half then differs from
   `maxH / maxV`, which 03 §3.2 allows. The desktop mirror's crop constants
   (03 §8) are recomputed at the next configure rendering; until then the
   cropped mirror modes use the provisional tangents, under which the crop
   is the whole half. Recomputing them at once is allowed.

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
| slot 11 (head pose, game thread) | take the **latest published views** (§5.3): the head pose they hold (the head located in the base space, §6.1, with the invalid-pose rules of §5.5) converted with 03 §1 (OpenXR spaces are right-handed, +Y up, -Z forward, like OpenVR's), with the world scale. These views become this frame's **render views**, and the "head pose requested" flag is set (03 §6.2). Before any frame was located, the head is the neutral pose |
| slot 5 (begin frame, render thread) | the FOV of §4.2; eye offsets (§5.4) from the latest published views; this frame is head-tracked if the "head pose requested" flag is set |
| slot 6 (end frame, render thread) | the frame boundary of §5.3; then the controllers are handed to the game (04 §5.9); then the desktop mirror and `Present(0, 0)` as in 03 §6.2 |

### 5.2 What is submitted

**Head-tracked frame** (the game asked for a head pose, and its render views
are valid): one projection layer (`XrCompositionLayerProjection`, no layer
flags) in the base space with two views. View e shows eye e's half of the
side-by-side target (`x` from `e * W/2` to `e * W/2 + W/2`, all rows, with
`W` the texture's width from `GetDesc`, integer halves), and declares:

- **pose**: eye e's pose from the render views, the pose the game rendered
  that half with, not a newer one;
- **fov**: the **symmetric FOV the game rendered with**, for both eyes:
  `angleLeft = -atan(maxH)`, `angleRight = atan(maxH)`,
  `angleUp = atan(maxV)`, `angleDown = -atan(maxV)`.

Unlike OpenVR (03 §3.3) no crop bounds are needed: the whole half is
declared with the FOV it covers and the compositor maps it to the eye's
real FOV. Poses are in the runtime's metres; the world scale `s` applies
only to what the game receives (03 §1, §5), never to layer poses.

**Screen frame** (menus, videos, loading, 03 §7; also a head-tracked frame
whose render views are not valid): no projection layer; one quad layer
(`XrCompositionLayerQuad`, no layer flags), `XR_EYE_VISIBILITY_BOTH`, in the
base space, showing the same region as the OpenVR overlay: eye 0's half,
middle half vertically (`x 0..W/2`, `y H/4..3H/4`, integer division). Its
orientation is the yaw-only reference orientation of the latest recenter
(§6.2); its position is the reference position moved 1.5 m along the
reference's forward direction (the reference orientation applied to
(0, 0, -1)) in X and Z only, at the reference's height. Its size is 32/9 m
wide (3.5556 m) and `width * regionHeight / regionWidth` high, the same
screen as the OpenVR overlay (03 §7). The quad's front (+Z) faces back
towards the reference. Nothing else is shown, so the view is black around
the screen. Before any recenter the reference is the identity pose at the
base-space origin.

**Images.** OpenXR layers take images from OpenXR swapchains. The needed
region of the game's stereo target (device +0x158) is copied into an
acquired swapchain image (`xrAcquireSwapchainImage`,
`xrWaitSwapchainImage`, `ID3D11DeviceContext::CopySubresourceRegion` on the
game's immediate context, `xrReleaseSwapchainImage`; acquire, wait and
release are the API's required sequence for writing an image), and the
layer's sub-image is the whole image. The wait may be unbounded or bounded:
the compositor gives the image back within about one display period, so
the wait is only long when the runtime has stopped compositing; a wait
that fails or times out ends that frame with no layer. One swapchain per
eye and one for the screen quad, each the size of its region, so the copy
covers the whole image; one swapchain twice as wide with an image
rectangle per eye is equally acceptable. Swapchains are created with usage
colour attachment and transfer destination (the copy writes them), one
sample, one face, one array layer, one mip, and recreated when the region
size or the format changes (logged with size, format and image count). If
a swapchain cannot be created or a copy step fails, that frame is ended
with no layer.

**Format.** The swapchain format must be in the same typeless family as the
game's target (a copy requires it; the game's target is an 8-bit RGBA or
BGRA format). For a target in the `R8G8B8A8` family take
`DXGI_FORMAT_R8G8B8A8_UNORM_SRGB`, else `R8G8B8A8_UNORM`; for `B8G8R8A8`,
`B8G8R8A8_UNORM_SRGB`, else `B8G8R8A8_UNORM`; any other format only if the
runtime offers it exactly; whichever the runtime lists in
`xrEnumerateSwapchainFormats`. sRGB first: the game writes display-ready,
gamma-encoded values (03 §6 submits them to OpenVR as `ColorSpace_Gamma`),
and storing the same bytes in an sRGB swapchain tells the compositor exactly
that; a plain UNORM swapchain would be shown with the wrong gamma. No
compatible format: no layer, logged once. Observed: SteamVR offers
`DXGI_FORMAT_B8G8R8A8_UNORM_SRGB`.

### 5.3 Frame pacing: one boundary in slot 6

**The boundary.** OpenXR frame N is begun at the end of game frame N-1 and
ended at the end of game frame N; both happen in slot 6, the only place in
a game frame where OpenXR frame functions are called. Why there: the game
thread reads the head pose before the render thread begins its frame
(03 §6.1), so the views for game frame N must exist by the end of game
frame N-1; and the image of game frame N exists only at slot 6 of frame N.
The game thread's next slot 11 takes the views located at this boundary,
so the next game frame is rendered with views predicted for the display
time it is submitted at, and declares them (§5.2). [sure: observed pose age
0 in every frame; a measured alternative that waited between rendering and
submitting rendered every frame with views one display period (11 ms at
90 Hz) old, see 11 §4.1.]

Constraints on the work at the boundary, while a session exists:

1. **Ending.** The OpenXR frame in progress (if one was begun) is ended
   with **its own** `predictedDisplayTime`, the blend mode of §2.2, and
   zero or one layer: the layer of §5.2 only if that frame's
   `shouldRender` was set and the views it was rendered with were valid.
   The copies into the swapchain images are submitted to the GPU
   (`ID3D11DeviceContext::Flush`) before `xrEndFrame`, because `xrEndFrame`
   may wait for them and an unflushed copy would stall it.
2. **Events.** Events are polled (§3) before every `xrWaitFrame`. While the
   session is lost or not running, no frame is begun.
3. **Matching.** Each `xrWaitFrame` is followed by one `xrBeginFrame` and,
   at the next boundary, one `xrEndFrame`; a session lost in between needs
   no `xrEndFrame`. `predictedDisplayTime`, `predictedDisplayPeriod` and
   `shouldRender` are kept from `xrWaitFrame` for that frame.
4. **Pacing.** `xrWaitFrame` is the game's frame pacing, the equivalent of
   OpenVR's `WaitGetPoses`; nothing else in the mod waits on the headset.
5. **Re-anchor first.** A pending re-anchor (§6.3) is done before anything
   is located for the new frame, so all of the frame's poses are in the
   same base space.
6. **Locating.** The two views (`xrLocateViews`, primary stereo
   configuration), the head (the `VIEW` space) and the controllers' grip
   poses with their action states (`xrSyncActions`, 04 §5.8.4) are located
   in the base space at the new `predictedDisplayTime`, and published
   together with that time as **one snapshot** for the game thread (§9).
   Controllers are synced at the boundary so they reach the game before its
   next simulation step (04).
7. **Validity.** What is located is used according to its location flags
   (§5.5).
8. **IPD.** The first time the views are valid, the IPD (distance between
   the two eye positions) is logged in metres.

If the stereo texture is not known yet (no configure), slot 6 does none of
this.

### 5.4 Eye offsets

At slot 5, for each eye, the offset of the eye from the head in head
coordinates: the eye's position minus the head's position (both from the
latest published views, in the base space), rotated by the inverse of the
head's orientation. ViewAdjust is then set from these exactly as 03 §5 does
with `GetEyeToHeadTransform` (eye 0 takes the right eye's offset times `s`,
eye 1 the left eye's). Until views with valid positions have been located,
each eye is offset by half an average adult IPD (64 mm): -0.032 m in X for
the left eye and +0.032 m for the right (so ViewAdjust eye 0 is
`(+0.032 s, 0, 0)`). Observed: IPD 0.066 m on Index, depth correct.

### 5.5 Invalid poses

OpenXR reports per location whether its orientation and its position are
valid: `XR_VIEW_STATE_ORIENTATION_VALID_BIT` and
`XR_VIEW_STATE_POSITION_VALID_BIT` in the view state of `xrLocateViews`,
`XR_SPACE_LOCATION_ORIENTATION_VALID_BIT` and
`XR_SPACE_LOCATION_POSITION_VALID_BIT` in an `XrSpaceLocation`. Values
whose bit is clear are not to be used. Required:

| Location | Bit clear | Result |
|---|---|---|
| head (`VIEW` space) | orientation | the head the game receives is the last head whose orientation was valid, or the neutral pose (03 §1) if there has been none |
| head | position only | the new orientation with the last valid position (the neutral position if none). Project decision: a headset that loses positional tracking (occluded, outside the play area) still reports its orientation, and freezing the whole view while the head turns is worse than holding the position still |
| views (`xrLocateViews`) | orientation, or the call fails | the views are not valid: no projection layer for a frame rendered with them (a screen frame instead, §5.2), the FOV of §4.2 is not taken from them, the eye offsets of §5.4 stay as they were |
| views | position only | the views remain valid for the projection layer, with each eye's position replaced by the last valid one, so the declared pose matches the head the game received; the eye offsets stay as they were |
| grip poses | either | 04 decides what an untracked hand does; a value with its bit clear is never handed on as a pose |

Recentering is never taken from an invalid head: a game recenter (§6.2)
and a re-anchor (§6.3) need both the head's orientation and position
valid. A recenter requested while the head is invalid stays pending and
happens at the first frame where it is valid (and the headset in use, for
a game retake, 03 §2.2); until then the old reference stays.

Looking straight up or down: the yaw of §6.2 is undefined when the
horizontal length of the head's forward axis is below 1e-4, that is when
`sqrt((2 (w y + x z))^2 + (1 - 2 (x x + y y))^2) < 1e-4` for the head's
quaternion. Then the previous yaw is kept (0 before any). No pose, yaw or
offset handed to the game or to the runtime is ever NaN.

## 6. Reference spaces and recentering

### 6.1 The base space

Required: head, eye views, controllers and every layer are expressed in
**one base space**, so they agree with each other. Its +Y is up, its
origin is the head's position at the latest anchor (§6.3), and it is
rotated about +Y by the head's yaw at that anchor, so its forward (-Z) is
where the player faced, level. This makes the poses the game receives
independent of where the runtime's seated origin happens to be.

OpenXR forces part of how this is reached: the head is obtained as the
`VIEW` reference space, and a seated, gravity-aligned origin as the
`LOCAL` reference space. A `LOCAL` space created with the anchor pose
(the head's yaw about +Y and the head's position, both measured in plain
`LOCAL`) is one way to get the base space. The spaces are created with the
session; if the spaces needed for this cannot be created, session creation
fails (§2.3).

### 6.2 Game recentering

Recentering works as in 03 §2: the reference pose the game reads is the
yaw-only head pose in the base space, taken by the same triggers (03 §2.2,
04 §8). For a head orientation quaternion `q = (x, y, z, w)` the yaw is
`atan2(2 (w y + x z), 1 - 2 (x x + y y))` (this is spec-side derived
convention: the heading of the head's forward axis about +Y; §5.5 covers
looking straight up or down); the reference is the rotation by that yaw
about +Y with the head's full position, converted with 03 §1. The head is
the one of the latest published views and must be valid (§5.5). Game
recentering does not change the base space. The virtual screen's quad
(§5.2) is placed from that same reference, which the game thread writes
and the render thread reads (§9).

At CreateDevice no session exists and no head pose is available: the
reference is taken from the neutral pose (03 §1), and since the headset
is not in use a retake is queued (03 §2.2); it fires on the first frame
where the headset is in use.

"Headset in use" under OpenXR: the session is running, the latest published
views are valid, the head's orientation and position are valid (§5.5), and
the head is upright (the world-Y component of the head's up axis,
`1 - 2 (x x + z z)` of its quaternion, exceeds 0.7, as in 03 §2.3).
Observed: the first game recenter happened as the session started running,
with the headset worn.

### 6.3 Re-anchoring the base space

A re-anchor moves the base space so that it satisfies §6.1 for the head at
that moment: the head located against the runtime's seated origin at the
display time of the frame being begun, with both orientation and position
valid (§5.5; otherwise it stays pending and is tried again at the next
frame). It is done before that frame's poses are located (§5.3), so no
frame mixes two base spaces. The new anchor's yaw is logged. A re-anchor
taken while the horizontal length of the head's forward axis is below
1e-4 (looking straight up or down, §5.5) keeps the previous anchor's yaw
with the new position, for the same reason as a game recenter: the yaw is
undefined there.

It is pending:

- from session creation, so the base space is anchored at the head on the
  first tracked frame;
- after every `XrEventDataReferenceSpaceChangePending` (the player
  recentered in the runtime's own UI, which moves `LOCAL`); it is done at
  the next frame, not at the event's `changeTime`.

A re-anchor does not by itself retake the game's reference pose (open
question 3). Because the game's reference is normally taken shortly after
the first anchor (near yaw 0 and the origin of the base space), a runtime
recenter in practice moves the game's forward to where the player faces.

## 7. Failures and what the player is told

| Situation | Behaviour |
|---|---|
| no 32-bit runtime (§1) | runtime choice of 06 §1; logged |
| instance, system or view configuration fails at CreateDevice | no device, flat play, told once |
| graphics requirements, session or spaces fail at configure rendering | device detached, flat play, told once (§2.3) |
| the game renders on a different adapter than the runtime wants | warning logged; continue (the runtime may still work, or fail at session creation) |
| no swapchain format compatible with the game's target | no layer submitted, logged once |
| session stopping | nothing submitted until it is ready again; stereo and mirror continue |
| session exiting | the game quits (§3) |
| session or instance lost | VR stops for this run; the game continues in stereo on the mirror (§3) |
| a running session not shown in the headset within about 10 s (§3.1) | logged; the player is warned once, without blocking; nothing else changes |
| no interaction profile bound | no VR input; gamepad, keyboard and mouse keep working |

"Told once" is one message (13 owns messages to the player; wording,
icon and buttons are the implementation's). It must convey, each for its
reason:

- that OpenXR could not be started, so the player knows which part failed;
- the failing step with the result's text and number, for diagnosis by
  the player or in a bug report. The text is never empty: the runtime's
  own text (`xrResultToString`) when an instance exists, otherwise (a
  failure of `xrCreateInstance` or of a call before it) the result's
  symbolic name as the OpenXR registry defines it, for example
  `XR_ERROR_RUNTIME_UNAVAILABLE`, because `xrResultToString` needs an
  instance; a value the registry does not define is shown with its number
  and a text saying that it is not a defined result;
- that the game now runs with VR disabled, so the player knows why the
  game is flat;
- that a restart is needed to try again, because the runtime is not
  retried in this run; the stored runtime choice is kept, so a restart
  retries OpenXR (06 §5.2);
- how to go back to SteamVR (both routes: the options page's runtime
  choice and the `-vrforceopenvr` switch, 01 §5.1), because a player who
  switched to OpenXR and cannot start it otherwise has no obvious way
  back;
- for `XR_ERROR_RUNTIME_UNAVAILABLE` (-51) also: the likely cause (the
  loader found no runtime build it could load into a 32-bit process) and
  where the player can see which manifests were examined (the log, §1),
  because this result can only be diagnosed from that list.

The list is unordered; the wording and order are the implementation's.

Every failing OpenXR call whose failure ends or degrades VR is logged with
its result (11 §2.4), with the same never-empty text. A call made every
frame (`xrWaitFrame`, `xrBeginFrame`, `xrEndFrame`, the swapchain image
calls, `xrLocateViews`, `xrLocateSpace`, `xrSyncActions`) that keeps
failing is logged when its result changes, not on every frame (11 §3), and
a failure during frames never changes what a start-up failure message
reports: that message is about start-up only.

## 8. OpenXR without a stereo device: the flat game on a virtual screen

When OpenXR is the chosen runtime but the game's stereo device was never
attached and never failed (the game patches are not installed, for example
because the preflight failed, 01 §2.5, or CreateDevice was never called with
type 1), the mod still shows the game in the headset as a flat screen:

| Property | Value |
|---|---|
| start | at the first `Present` of the game's swapchain (render thread), once: `ID3D11Device` from `IDXGISwapChain::GetDevice`; then everything of §2.2 (instance, system, session, spaces, formats, input). On failure: logged, no message, flat game on the monitor only; not retried |
| frames | on every `Present` of the game's swapchain, before the real `Present`: the boundary of §5.3, with the image being the swapchain's current back buffer (`GetBuffer(0)`), whole, only if it is single-sampled |
| layer | always a quad (never a projection layer): 2.6 m wide, height from the back buffer's aspect, 2 m straight ahead of the base space's origin (at (0, 0, -2) m in the base space), identity orientation, so level and facing the anchor of §6.3 |
| pacing | when the boundary began a frame (it called `xrWaitFrame`), the real `Present` uses sync interval 0, so the game does not wait for both the headset and the monitor; otherwise the game's own sync interval |
| recenter | left or right Ctrl together with left or right Alt (`GetAsyncKeyState(VK_CONTROL)` and `VK_MENU`), on the press (rising edge): re-anchor the base space (§6.3) |
| input | the controllers drive the game as in 04 where the input hooks are installed |
| other swapchains | ignored (03 §9.1) |

The screen size differs from §5.2 on purpose. Project decision: §5.2's
screen (3.5556 m at 1.5 m, about 100 degrees wide) shows menus and videos
for short periods and is sized to fill most of the view so text is
readable; here the whole game is played on the screen for as long as it
runs, so it is about 66 degrees wide (2.6 m at 2 m), small enough that the
whole picture stays in view without turning the head, and 2 m away so the
eyes converge at a comfortable distance. There is also no game reference
pose to place it from, so it is placed from the base space's anchor
instead of from the game's recenter.

## 9. Data crossing threads

The names below ("published views", "render views") are descriptive; the
implementation chooses its own.

| Data | Written by | Read by | Requirement |
|---|---|---|---|
| published views (both eyes' poses and FOVs, head, hands, display time, validity) | render thread (§5.3, constraint 6) | game thread (slot 11, hand poses of 04), render thread (slot 5) | read as one consistent snapshot, never torn and never mixing two frames; the writer never waits for readers, because the render thread must not block on the game thread |
| render views (the snapshot slot 11 took) | game thread | render thread (slot 6, layer declaration) | the layer declares exactly the views the frame was rendered with |
| yaw-only reference (orientation and position) | game thread (recenter) | render thread (quad placement) | never torn |
| "head pose requested" flag | game thread | render thread | as 03 §11 |
| quit request | render thread (session events) | the game (`QuitRequested`) | set no later than the next frame (§3) |

Everything else (OpenXR handles, swapchains, the frame in progress) is used
on the render thread only, except creation at CreateDevice, which runs on
the game thread before the render thread uses OpenXR. No allocation, file
I/O or logging per frame on the render thread (11 §3); the pose-age
measurement of 11 §4.1 is counted every frame and logged only with
`DebugOptions` on, at the interval 11 sets.

## 10. AI.exe addresses used here

2015 RVAs were verified against the Steam 2015 binary. 2020 values were
taken from the earlier mod's per-build address table (and, for inline
sites, its installer) and have not been verified against a 2020 `AI.exe`.
"?" means unknown.

| Name | 2015 | 2020 | Use here |
|---|---|---|---|
| `StereoInitCall` | 0x613941 | 0x565E11 | §2.1 step 1 |
| back-buffer size query call | 0x613AE8 | ? | §2.1 step 2 |
| `StereoRenderTargetSizeCall` | 0x60FD64 | 0x562244 | §2.1 step 2 |
| D3D11 device creation call | 0x613CB8 (into 0x626FA0) | ? | §2.1 step 3 |
| `RendererDevice` | 0x621E20 | 0x574310 | §2.1 step 4, §2.2 |
| `RendererContext` | 0x621E40 | 0x574330 | §5.2 copies |
| `RendererRtTexture` | 0x6232D0 | 0x575800 | the stereo target (03 §4.4) |
| `QuitRequested` | 0x17DA920 | 0x17EAAD0 | §3, byte |

## 11. Acceptance

On SteamVR's OpenXR runtime with the headset worn, runtime set to OpenXR;
log content as in 11.

1. Discovery: with only SteamVR's 64-bit registration, the log shows both
   registry views' values (the 32-bit one absent), the 64-bit manifest,
   the `steamxr_win32.json` sibling resolving to an x86 library (machine
   0x014C), the loader property accepted, and OpenXR chosen.
2. Start-up: the log shows the runtime name and version, the system name,
   2688 x 2688 per eye (Index), that the FOV was not reported and the
   provisional tangent of about 1.2 is used; then the session created on
   the game's device, running within a second, swapchains created with an
   sRGB format, the IPD (about 0.066 m), and new render tangents (about
   `v = 1.214`, `h = 1.154`) in the first frames.
3. Pose age: with `DebugOptions` on, the pose-age report of 11 §4.1 shows
   every frame rendered with views of the frame it is submitted in.
4. Stereo and head tracking correct; menus on a level screen 3.56 m wide,
   1.5 m ahead, black around; gameplay hides it.
5. Recenter in the SteamVR dashboard: the log shows a re-anchor with its yaw;
   the game's forward and the menu screen move to where the player faces.
6. Start with the headset off the head, put it on: one game recenter when it
   is worn and upright.
7. Quit from the SteamVR dashboard: the session goes to exiting and the game
   exits normally. Holding a controller button while the session stops or is
   lost: the game sees it released.
8. Remove the 32-bit sibling manifest (or point `XR_RUNTIME_JSON` at a
   missing file) with OpenXR forced: no VR, the reason logged, the game runs
   flat.
9. Start with SteamVR not running and OpenXR forced (or the headset off when
   the runtime cannot start without it): one message naming the failing
   step and result; the game runs flat, the fullscreen option works.
10. Session failure at configure rendering (for example by forcing the game
    onto another adapter on a hybrid laptop): one message; the game renders
    flat and presents itself, not in stereo.
11. Invalid poses: cover the headset's tracking (or, on a camera-tracked
    headset, leave the tracked area) during gameplay: the view keeps
    turning with the head and does not jump to the origin; the log and the
    game show no NaN pose; a recenter requested meanwhile happens once
    tracking is back. Look straight down and recenter: the heading does
    not change.
12. Leftover 32-bit registration: with SteamVR active (64-bit view) and
    the 32-bit view's `ActiveRuntime` set by hand to another runtime's
    manifest (test only): the log shows both values and SteamVR's
    `steamxr_win32.json` chosen, and OpenXR starts on SteamVR.
13. Elevated: start the game as administrator with the setup of check 12:
    the log says once that the process is elevated, and OpenXR still
    starts on SteamVR (the loader property, not the environment, reached
    the loader).
14. Failure text: with no usable 32-bit runtime reachable and OpenXR
    forced in a way that reaches `xrCreateInstance` (for example
    `XR_RUNTIME_JSON` pointing at a manifest whose library cannot load):
    the message and the log give the result's symbolic name (never an
    empty text) and its number; the message says how to go back to
    SteamVR, and for -51 it names the likely cause and points at the log.
    The options page then shows runtime None (06 §5.2).
15. Not shown: with Steam's "Use Desktop Game Theatre while SteamVR is
    active" on for the game and SteamVR running, start with OpenXR: if the
    session runs but stays below `VISIBLE` for about 10 s, the log shows
    every state change and one not-visible event with the state, and one
    warning appears without stopping the game. With the setting off, no
    such event or warning.
16. Probing only: with OpenVR selected and an OpenXR runtime registered,
    start the game: the log shows the OpenXR registry values and manifests
    examined, but no loader direction and no elevation event, and
    `XR_RUNTIME_JSON` is not set in the game's environment (a process the
    game starts does not see it).
17. Expandable registration: with the 64-bit view's `ActiveRuntime`
    rewritten by hand (test only) as a `REG_EXPAND_SZ` value that names
    SteamVR's manifest through an environment reference, OpenXR starts on
    SteamVR as in check 1.
18. Re-anchor looking straight up: recenter in the SteamVR dashboard while
    looking straight up: the logged anchor yaw equals the previous one and
    no pose is NaN.
19. Repeated frame failures (a developer build that makes one per-frame
    call fail for some seconds, for example `xrWaitSwapchainImage`): one
    event when the failure starts and one when the result changes back,
    not one per frame; each failed image wait ends its frame with no
    layer; no failure message to the player.

## 12. Open questions

1. **Other runtimes.** Meta Quest Link, WMR, Pico and Varjo are untested:
   whether they report the FOV through the extension, which swapchain
   formats and blend modes they offer, how they register 32-bit support
   (whether making them active writes only the 64-bit view, as SteamVR's
   does), and whether their session states differ when the headset is removed. To
   resolve: run acceptance 1 to 7 on each and read the log.
2. **Lost or stopped session leaves stereo running.** After `STOPPING`
   without a later `READY`, or after a loss, the device stays attached: the
   game keeps rendering both eyes for the mirror and nothing paces it
   (present with sync interval 0). Whether that is acceptable, or the
   device should be detached on a loss and the present use sync interval 1
   while not running, is to be decided; in-game behaviour of SteamVR quitting
   mid-game has not been observed. Decided already: a loss never asks the
   game to quit (§3).
3. **Runtime recenter and the game's reference.** A
   `ReferenceSpaceChangePending` re-anchors the base space but does not queue
   a retake of the game's reference pose (§6.3), and that is the required
   behaviour. Whether a queued game retake should be added (it would make
   the result exact when the game's reference is far from the anchor) is
   to be decided; the observed result (acceptance 5) was correct with the
   re-anchor alone.
4. **The no-device virtual screen (§8)** runs even when `AI.exe` is not a
   recognised build, which 01 describes as running completely unmodified.
   Which of the two is required for an unrecognised build is to be decided
   (the shape of §8 stands either way for a recognised build whose patches
   failed).
5. **Invalid-pose behaviour (§5.5)** is specified but not yet observed in
   game; acceptance 11 is to be run.
6. **How SteamVR's Desktop Game Theatre affects an OpenXR session** is
   not observed by the spec side: whether the session is created and runs
   but stays below `VISIBLE` (what §3.1 detects), or whether creation
   fails instead. To resolve: run acceptance 15 and read the session
   states in the log.
