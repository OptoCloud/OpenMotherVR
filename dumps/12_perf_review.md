# 12. Performance review of `recon/` (d3d11.dll proxy reconstruction)

Scope: every per-frame, per-call and startup path in `recon/src` at `617eaf3`. Where a number says **measured**, it
comes from an instrumented copy of the DLL run in the game. Where it says **estimated**, it is reasoned from code and
hardware figures and was not measured.

No file under `recon/` was changed. The instrumented build lived only in the scratchpad
(`.../scratchpad/perf/`, `.../scratchpad/build-perf/`). After every run the normal `recon/build/Release/d3d11.dll`
was copied back (MD5 `8a9625847e395c715b398f46c8060561`) and `mothervr.json` was restored byte for byte.

---

## 0. Setup and method

**Machine.** RTX 4090, Ryzen 7 7800X3D, Windows 11. Bigscreen Beyond at 90 Hz, SteamVR 2.17.10 (OpenXR and
OpenVR). Both runtimes recommend **2688x2688 per eye**. The player's `mothervr.json` has `PixelScale: 1.5`, so the
stereo target is 8064x4032.

**Caveats.**
- VRChat, the Unity Editor and a set of overlays (XSOverlay, fpsVR, OVR Advanced Settings) were running during
  the measurements. The GPU sat at 37 % before the game started. Absolute GPU times are therefore pessimistic. The
  A/B comparisons below alternate phases within one run, so the background load affects both sides equally.
- The headset was not worn (proximity sensor off), but SteamVR still paced the frames.
- Gameplay was not reached: synthetic Enter presses did not load a save. Measurements cover:
  - the main menu's tracked 3D scene, with 2200 to 4200 cull tests per frame;
  - loading and cinema phases;
  - a heavier segment of about 45 s reached after the key presses (GPU 13 to 15 ms; the scene could not be
    identified because the screen was not visible).

  Per-eye planar reflections never ran (`refl_stereo n=0`), so they are unmeasured.

**Instrumentation** (scratch copy only):
- QPC scopes around `dev_begin_frame`, `dev_end_frame`, `submit_frame`, `xrWaitFrame`, `xrBeginFrame`, the
  action sync, `xrLocateViews`, the eye copies, `xrEndFrame`, `Submit`, `WaitGetPoses`, the mirror, `Present`,
  `head_pose`, `stereo_update`, `log_line`, the shader patch and `CreatePixelShader`.
- Call counters on every hook, and the thread id of each hook.
- D3D11 timestamp queries, read back 4 or more frames late with `DONOTFLUSH`: game render (BeginFrame to
  EndFrame), eye copies, mirror, the SMAA pass, and the per-eye reflection pass.
- A pose-age probe: the display time the declared views were predicted for, compared with the frame's own display
  time.
- Rotating A/B phases of 300 or 450 frames: baseline, mirror off, culling restored (the hooks call the game's
  original tests), reflections mono.
- One run with the back buffer at 1/4 size and PixelScale 1.0, and one run on the OpenVR backend.

### Frame budget as measured (main menu 3D scene, OpenXR, PixelScale 1.5, steady phases)

| Item | Render thread CPU | GPU |
|---|---|---|
| Frame interval | 11.11 ms (paced at 90 Hz) | |
| Game draw submission (BeginFrame to EndFrame) | 3.5 to 4.4 ms (2.6 to 3.2 ms with culling restored) | 7.3 to 7.7 ms, of which SMAA 0.58 ms |
| Blocked in `xrWaitFrame` | 5 to 7 ms | |
| `xrEndFrame` (runtime) | 0.2 to 1.8 ms | |
| Eye copies (2 x 4032x4032) | 0.09 to 0.14 ms | 0.12 to 0.15 ms |
| Mirror (clear and draw into 8064x4032) | 0.004 to 0.01 ms | 0.31 to 0.34 ms |
| `Present(0,0)` | 0.08 to 0.15 ms | DWM scaling not measured |
| Input (sync, 14 gets, 2 locates), events, locate views | 0.07 to 0.2 ms | |
| BeginFrame, head pose, stereo update, pad hooks | under 0.01 ms total | |

OpenVR backend, same scene: two `Submit` calls take 0.26 to 1.3 ms, `WaitGetPoses` blocks 4.7 to 6.9 ms, and SteamVR
Input polling takes 43 µs. GPU game time is 7.2 to 8.2 ms. The frame is paced at 11.11 ms.

Heavier segment (OpenXR, 1.5): GPU game 13.3 to 15.0 ms, frame interval 16 to 17 ms. `xrWaitFrame` no longer
blocks (20 µs). Instead `xrEndFrame` blocks for **8 to 10.6 ms** and `xrWaitSwapchainImage` for 1.6 to 2.9 ms.

PixelScale 1.0 with the back buffer at 1344x672 (same menu scene): GPU game time **3.1 to 3.6 ms**, mirror
0.027 ms, copies 0.06 ms.

**Verdict.** In the measured scenes the mod is **sync-bound**: it is paced by the runtime, with about 25 % GPU
headroom and about 50 % render-thread headroom. When it misses frames it is **GPU-bound**, and the cause is the
render resolution. The mod's own per-frame cost is small: about 0.5 ms GPU and under 0.3 ms CPU, not counting the
runtime calls. The exception is the culling change, which costs about 1 ms of render-thread CPU.

---

## 1. Findings, ranked by expected frame-time gain

### P1. Render resolution: PixelScale multiplies a size that already includes SteamVR's supersampling
**Where:** `stereo/stereo_device.cpp: dev_render_target_size`, `runtime/openxr_hmd.cpp: eye_texture_size`,
`runtime/openvr_hmd.cpp: eye_texture_size`, `config/config.cpp` (clamp to [0.5, 4]).

**Measured:**
- Stereo target sizes: PixelScale 1.5 gives 8064x4032 and 2.0 gives 10752x5376. The 10752x5376 in the logs came
  from an **OpenXR** run at 2.0 (19:33 session), not from OpenVR. OpenVR recommends the same 2688x2688 for this
  headset.
- GPU game time in the same scene: 7.3 to 7.7 ms at 1.5, and 3.1 to 3.6 ms at 1.0. That is 2.2 times, close to
  the 2.25 times pixel ratio.
- In the heavy segment, 13.3 to 15.0 ms at 1.5 missed 90 Hz (16 to 17 ms frames). At 1.0 it would be about 6 to
  7 ms (**estimated**).

**Why:** both runtimes' recommended size already includes SteamVR's render-resolution setting. PixelScale is a
second, per-axis multiplier on top of it: 1.5 means 2.25 times the pixels and 2.0 means 4 times. This is faithful
to the original, but it is supersampling twice whenever the player also raises SteamVR's slider. At 3.05 or more,
the width passes D3D11's 16384 texture limit and the game's target creation fails. The 19:33 log shows a ramp up to
13440x6720.

**Fix:**
- Keep the code default at 1.0.
- Label the option in the menu as "x SteamVR resolution".
- Clamp so that `2 * w * scale <= 16384` and `h * scale <= 16384`.
- Log the resulting megapixels.

**Expected gain:** 4 ms GPU per frame in the menu scene and about 7 ms in the heavy scene, going from 1.5 to 1.0.
This is the single largest lever.

### P2. Culling disabled in every phase costs about 1 ms of render-thread CPU
**Where:** `stereo/game_hooks.cpp: cull_distance / cull_occlusion / cull_view_volume`;
`hook/hook_installer.cpp: fix_culling`.

**Measured:**
- Call counts per frame, on job threads: 2200 to 4200 distance tests, 100 to 1500 occlusion tests and 0 to 33
  view-volume tests. The wrapper costs nanoseconds per call.
- A/B with the game's original tests restored, in the same scene, render-thread draw submission (`game_b`):

  | Run | Culling restored | Culling disabled |
  |---|---|---|
  | run3 | 2.65 to 2.80 ms | 3.5 to 4.2 ms |
  | run4 | 2.98 ms | 3.9 to 5.0 ms |
  | run5 (OpenVR) | 2.90 to 3.23 ms | 3.96 to 4.43 ms |

- The GPU difference is 0 to 0.4 ms in the menu scene. In a real level, with more occluded rooms, both numbers are
  likely larger (**estimated**).

**Why:** the hooks return "visible" for the distance, occlusion and portal tests in every POV phase: main view,
shadow and reflection. GRAND (`08d` §2.2) removes the distance test everywhere, but drops the occlusion and portal
tests only in the main phase (phase 2), and keeps the frustum test there. Ours draws everything the frustum lets
through, in all phases.

**Fix:**
- Keep the distance-test bypass (the pop-in fix).
- Apply the occlusion and view-volume bypass only while the main stereo POV is culling. Track the phase from
  `POVManager::CullPhase` as GRAND does, or check the cull context against the main POV.
- Better still, keep both tests and give them a cull volume that is the union of the two eye frusta.

**Expected gain:** about 1 ms render-thread CPU per frame (measured, menu), and up to 0.4 ms GPU. More in
gameplay.

### P3. The desktop mirror and back buffer are the size of both eyes (8064x4032)
**Where:** `proxy/d3d11_proxy.cpp: D3D11CreateDeviceAndSwapChain, hook_ResizeBuffers`;
`stereo/stereo_device.cpp: dev_end_frame` (clear and full-window draw).

**Measured:**
- Mirror GPU time: 0.31 to 0.34 ms at 8064x4032 and 0.027 ms at 1344x672 (A/B with the mirror off: 3 to 7 µs).
- With the back buffer forced to 1/4 size, the game ran 95 s at a steady 11.11 ms with no errors.
- `Present(0,0)` takes 0.08 to 0.15 ms of CPU.

**Estimated:**
- DWM scales a 130 MB surface to the window every frame: about 0.1 to 0.3 ms GPU.
- VRAM: 2 x 8064 x 4032 x 4 B = **260 MB** for the back buffers, and 460 MB at PixelScale 2.0.

**Why:** the game takes its display mode from `GetRenderTargetSize`, so the swapchain gets the stereo size. In
stereo, only our mirror draws into it.

**Fix:** with a stereo device attached, create and resize the swapchain at the window size the game asked for
(`StereoInterface::mirror_width/height`, recorded in `get_render_target_size`) or at the client rect, not at
`rt_width x rt_height`. `dev_end_frame` already takes the viewport from the back buffer's own description. This
needs one in-game look at the mirror and the menus; the screen could not be watched here.

**Expected gain:** 0.3 to 0.5 ms GPU per frame and about 250 MB VRAM.

### P4. SMAA in stereo: 0.58 ms GPU at 8064x4032
**Where:** `stereo/game_hooks.cpp: stereo_final_draw, run_smaa`.

**Measured:** the AA pass takes 0.52 to 0.76 ms GPU (average about 0.59 ms) at 8064x4032.

**Why:** this is a feature: before `e3a4412` there was no AA in stereo. At PixelScale 1.5 or more, the target is
already supersampled 2.25 times and SMAA adds little.

**Fix:** keep SMAA at PixelScale 1.0 or below. Above about 1.3, either run FXAA (cheaper) or skip AA in stereo by
default, and expose the choice.

**Expected gain:** about 0.5 ms GPU when AA is skipped at high pixel scales.

### P5. Every menu, pause or loading open rebuilds all render targets: a 180 to 250 ms freeze each time
**Where:** `integration/script_bridge.cpp: push_settings_to_game -> request_display_reset()` (unconditional),
called from `on_menu_open`, `pause_open` and `loading_open` (`stereo/game_hooks.cpp`).

**Measured:**
- Every reset logs `ResizeBuffers 8064x4032`. The call itself takes 3.4 to 5.0 ms.
- The frame after it takes 177, 185, 189, 195, 213 or 232 ms. 2 to 4 such resets happened per short run.
- The game re-allocates its whole stereo target set each time (the texture pointer changes).

**Why:** the original does this too, and it is a hitch the player sees. In the headset, the compositor shows a
stale frame for 0.2 s every time the pause menu opens. It also churns hundreds of MB of VRAM.

**Fix:**
- Remember the pixel scale and mirror settings the targets were last built with.
- Raise the reset flag only when they differ: in the console commands and when the options page changes them, not
  in `push_settings_to_game`.
- The mirror constants can be refreshed without a reset (see 11 B3).

**Expected gain:** removes a 0.2 s stall per menu, pause and loading open. The average frame time is unchanged.

### P6. OpenXR: the game renders with views predicted one frame too early (+11.1 ms prediction error)
**Where:** `runtime/openxr_backend.cpp: frame()` (`li.displayTime = t`), `runtime/openxr_hmd.cpp: head_pose`.

**Measured:** in 593 of 600 frames, the views the game rendered with, and declared, were predicted for the display
time of the **previous** `xrWaitFrame`. The mean was 1.01 periods early, every steady phase, both scale settings.

**Why:**
- `frame()` runs at the end of render frame k. Its order is `xrWaitFrame`, then `xrLocateViews(t_k)`, then submit
  image k with `displayTime = t_k`.
- The game thread then fetches those views for frame k+1, which is submitted one period later.
- The declared pose matches the rendered pose, so the compositor reprojects correctly. But every frame carries
  11 ms more rotation and translation correction than necessary: more edge smear, and positional error that
  reprojection does not remove.
- OpenVR is correct here. The pose-age probe gives 0 in all 600 frames: `WaitGetPoses` at the end of
  `submit_frame` is a proper running start, and the game thread reads the poses after it returns.

**Fix:** in `frame()`, locate the views and the head at `t + fs.predictedDisplayPeriod`. Keep `displayTime = t` in
`xrEndFrame`, since the declared pose stays exact. The cleaner fix is P7's restructure, which hands each frame its
own `predictedDisplayTime`.

**Expected gain:** frame time unchanged; motion-to-photon prediction error 11.1 ms lower at 90 Hz.

### P7. OpenXR frame-loop placement: `xrWaitFrame` sits between rendering and submission, and `xrEndFrame` blocks while the GPU is busy
**Where:** `runtime/openxr_backend.cpp: frame()`, called from `OpenXrHmd::submit_frame` in `dev_end_frame`.

**Measured:**
- In the 90 Hz scenes, `xrWaitFrame` blocks 5 to 7 ms and `xrEndFrame` 0.2 to 1.8 ms.
- In the GPU-bound segment, the wait moves into `xrEndFrame` (8 to 10.6 ms) and `xrWaitSwapchainImage` (1.6 to
  2.9 ms). The render thread then idles while the GPU drains frame k, and only afterwards records frame k+1 (3 to
  4 ms of `game_b`).
- The frame interval was 16 to 17 ms against about 15.1 ms of GPU work. The 1 to 2 ms gap is consistent with
  CPU/GPU serialisation (**estimate**).

**Why:**
- Image k is held back until the *next* frame's wait returns, so it waits in our process for the length of the
  wait.
- SteamVR's D3D11 `xrEndFrame` waits for the GPU, which serialises the render thread with the GPU when GPU-bound.

**Fix:**
- Split `frame()`. Do `xrWaitFrame`, `xrBeginFrame` and `xrLocateViews` at the frame's start: in `dev_begin_frame`,
  or before the game thread's `head_pose`, with the 1-frame pipeline in mind. Do the copies and `xrEndFrame` at
  `dev_end_frame`.
- Call `ctx->Flush()` right after the copies so the GPU starts on them before `xrEndFrame`.
- Moving `xrEndFrame` to a submit thread would fully hide the block. It needs `ID3D11Multithread` protection,
  because the runtime may use the immediate context, so treat it as an experiment.

**Expected gain:** frame time unchanged when GPU headroom exists; about 1 to 2 ms in GPU-bound scenes
(**estimate**); also part of P6's latency fix.

### P8. Shader patch: an MD5 of every pixel shader, on the game thread
**Where:** `render/shader_patch.cpp: patch_position_dither` (`dxbc_checksum` on every blob before any pattern
test), `proxy/d3d11_proxy.cpp: hook_CreatePixelShader`.

**Measured:**
- About 19 to 22 µs per shader: 67 to 78 ms per 3500 shaders, or 2.7 to 3.2 ms per MB of bytecode.
- 676 sites were found in 3500 shaders, consistent with about 12.7k sites in 68k.
- With a warm driver shader cache, creating 3500 shaders took 140 ms, so the patch added **56 %**. With a cold
  cache it added about 18 % (373 ms).
- `CreatePixelShader` runs on the game/simulation thread (the same thread id as `head_pose`), so it feeds
  streaming hitches.
- **Estimated** over 68k shaders: about 1.3 to 1.5 s of game-thread time per session.

**Fix:**
- Before the checksum, scan the bytecode for the immediate `0x3A83126F` appearing twice in a row (the `l(0.001,
  0.001, …)` of the `mul`). Skip the blob when it is absent; most shaders are skipped.
- Checksum, parse and rewrite only the candidates. The output checksum must still be computed.

**Expected gain:** about 80 % of the patch time, roughly 1 s per session. The per-frame average is unchanged; the
gain is fewer streaming hitches.

### P9. Logging opens and closes the file for every line
**Where:** `diag/log.cpp: log_line` (`CreateFileW`, `WriteFile`, `CloseHandle` per line). It is called on the render
thread every 128 frames (`present #` in `hook_Present`) and on events: menus, resets, reflection-POV changes (up to
60 lines), controller changes.

**Measured:** 103 to 131 µs per line on average; **one call took 249 ms** during a loading spike.

**Fix:**
- Open the handle once with `FILE_APPEND_DATA` (appends stay atomic across the launcher and game processes), or
  push lines to a small background writer.
- Drop the periodic `present #` line in release builds; the perf counter in `dev_end_frame` is a better place for
  it.

**Expected gain:** under 1 µs per frame on average, but it removes rare multi-ms to 250 ms stalls on the render
thread.

### P10. OpenXR input is sampled at the end of the frame and read two frames later
**Where:** `runtime/openxr_backend.cpp: frame()` (`input::sync` after `xrBeginFrame`),
`runtime/openxr_hmd.cpp: poll_input` (in `dev_begin_frame`, render thread).

**Measured:** sync, 14 gets and 2 locates take 30 to 150 µs per frame. The pad hooks run mostly on the game thread.

**Why:** the actions are synced at the end of render frame k and copied into the tiers at BeginFrame k+1. The game
thread reads them during its next simulation step. That is one to two frames (11 to 22 ms, **estimated**) of
controller latency, and the cost is fine.

**Fix:** once P7 moves `xrWaitFrame` and `xrBeginFrame` to the frame's start, sync there and write the tiers at the
same point. OpenVR polls in `dev_begin_frame` with the same lag.

**Expected gain:** frame time unchanged; about 11 ms less input latency.

### P11. OpenXR head pose: data race between the game thread and the render thread (correctness)
**Where:** `runtime/openxr_hmd.cpp: head_pose` (game thread copies `xr::views()`); `runtime/openxr_backend.cpp:
frame()` (render thread writes `g.views`).

**Measured:** `head_pose` and `stereo_update` run on one thread, and BeginFrame, EndFrame, the OpenXR calls and the
mirror on another. The pose is never fetched between BeginFrame and EndFrame (0 of 600 frames, both backends), so
in practice the declared pose matches the rendered one. The copy can still interleave with `xrLocateViews`'
writes and tear.

**Fix:** double-buffer `FrameViews` with an atomic index, or use a seqlock.

**Gain:** none in frame time; it avoids an occasional torn pose.

### P12. OpenXR virtual-screen fallback path: two waits per frame (low)
**Where:** `stereo/present.cpp: on_present` (only when OpenXR has no stereo device).

`frame()` (`xrWaitFrame`) runs inside the game's own `Present`, which then presents with the game's vsync setting.
Both waits apply, so the game is capped at the slower of the two rates and judders. This path is rare.

**Fix:** pass `SyncInterval = 0` to the real `Present` on this path.

---

## 2. Looks expensive but is fine (do not chase)

| Item | Measured / estimated | Notes |
|---|---|---|
| Gamepad vtable hooks | **measured** about 120 calls per frame (95 IsDown, 8 GetValue, 15 GetAxis, 0 to 1 ControlToName, 1 to 3 HasActivity); `remap_all` 0 per frame in steady state (once at startup, 128 µs) | **estimated** under 10 µs per frame. Mostly called on the game thread. |
| Cull hook wrappers themselves | **measured** 2200 to 4200 plus 100 to 1500 calls per frame on job threads | One relaxed atomic load each, **estimated** under 20 µs per frame in total. The cost is the effect (P2), not the hook. |
| Present hook and vsync | **measured** 1.00 hook call per frame (our own `Present(0,0)` from EndFrame; the game never presents in stereo), 80 to 150 µs CPU | Not vsync-blocked despite the forced windowed mode. The interval is exactly 11.11 ms, set by the runtime. The game has no limiter in the way: 0.15 to 0.6 ms from EndFrame to BeginFrame. |
| BeginFrame, `head_pose`, `stereo_update`, OpenXR `poll_input` | **measured** 1 µs, 0.3 µs, 0.7 to 1.2 µs, 0.5 µs | |
| OpenVR `poll_input` (SteamVR Input, 14 actions) | **measured** 43 µs | |
| OpenVR `Submit` x2 plus `IsOverlayVisible` | **measured** 0.26 to 1.3 ms CPU | Runtime work. The same texture with two bounds is the intended use. |
| OpenXR eye copies (2 x 4032x4032 `CopySubresourceRegion`) | **measured** 0.12 to 0.15 ms GPU, about 0.1 ms CPU | One double-wide swapchain would save a call, not bandwidth. |
| Cinema quad copy | **measured** 0.04 to 0.06 ms GPU | Only in cinema mode. Not done while the eyes are shown. |
| `xrPollEvent`, `xrLocateViews`, head `xrLocateSpace` | **measured** 20 to 30 µs and 30 to 40 µs | |
| Shader patch ALU (one extra `mul` per site, `cb2[39]` operand) | **estimated** under 0.01 ms per frame | About 1 MUL per pixel per site on a 4090. |
| Clear gate | **estimated** ns per clear | One byte compare in the stub. |
| Per-eye reflections | not exercised (the POV flags were never 0x11 in our scenes) | Pixel work matches mono (two half viewports of the same target); draw submission doubles for that pass. Measure in a level with a reflective floor before changing anything. |
| Hot-path allocations | **estimated** about 0.2 µs per frame | `openxr_backend.cpp: pick_format` builds a `std::vector` on every `ensure_chain` call (2 to 3 per frame). Trivially replaceable by a static array, but not worth a separate change. No json or format work per frame. |
| GPU-CPU stalls from mod code | none | No `Map`, readback, query or `Flush` in the mod. `GetDesc`, `OMGetRenderTargets` and `GetResource` per frame are free. |
| ResizeBuffers call | **measured** 3.4 to 5 ms | The stall is the game's target rebuild (P5), not the call. |
| Startup: `DllMain` | **measured** about 4 ms when MOTHER.PAK is current; about 70 to 90 ms when it is rebuilt (17.5 MB read and written, after a DLL update or a UI.PAK change only) | |
| Startup: VR probe and json | **measured** about 1 ms | |
| Startup: runtime init | **measured** `xrCreateInstance` 2.4 to 3.0 s, `VR_InitInternal2` 2.4 s | Runtime side. |
| Memory: our resources | **measured** back buffer 260 MB (P3); OpenXR eye swapchains 2 x 3 x 4032^2 x 4 B = **390 MB**; screen swapchain 97.5 MB, created once at the first cinema frame | No per-menu or per-level leak in mod code: `dev_configure_rendering` releases before recreating, `ensure_chain` recreates only on a size or format change, and the OpenVR overlay is created once. The game re-allocates its own targets on each reset (P5). |

---

## 3. Suggested order of work

1. **P5** (gate the display reset) and **P9** (logger): small, local, and they remove the visible hitches.
2. **P1** (scale semantics and the 16384 clamp) and **P4** (AA policy at high scale): config and UI only.
3. **P3** (window-sized back buffer): about 20 lines in the proxy, plus one in-game check of the mirror and menus.
4. **P2** (phase-limited culling): needs the cull-phase global from `08d` §2.2.
5. **P6, P7, P10, P11** (OpenXR frame loop restructure): one change set, with the pose-age probe kept as a debug
   counter to verify it.
6. **P8** (shader pre-filter): independent, and pays off on level loads.
