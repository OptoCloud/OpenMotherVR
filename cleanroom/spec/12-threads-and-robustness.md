<!-- Provenance: AI.exe 2015 analysis (initialisation order, the per-object visibility loop, the calling conventions of the replaced methods); measurements in game of test builds (thread ids of every hook, call counts per frame, render-thread timings, 600-frame pose-fetch timing on OpenVR and OpenXR, a torn head-pose handover on OpenXR); user requirements (verify before writing, no per-frame logging on the render thread, no work at process exit, unknown build patches nothing, the mod never needs elevation); project decisions with their reasons inline (message boxes at attach, unwritable game folder); the earlier mod's player-facing behaviour (its message for OpenVR error 117 pointing at Steam's Desktop Game Theatre); public OpenVR headers (`EVRInitError` values); Microsoft documentation of DllMain, the loader lock, structured exceptions and the DLL search order, DXGI, OpenVR and OpenXR threading rules; the C++ memory model; a tester's report on SteamVR with a Steam Frame headset (focus taken by other applications, the game not shown in the headset, a crash with nothing in the log); project decisions of r3 with their reasons in 03, 09, 11 and 13 (the borderless mirror window, focus loss kept from the game, the non-blocking headset warning, the crash record, the event tap's image-range guard, the page showing the runtime this run drives); AI.exe 2015 analysis by the spec side for r4 (the C runtime's own unhandled-exception filter, installed after DLL attach, 11 §6.1); project decisions of r4, each with its reason inline (an OpenXR loss never quits the game; the proxy stays loaded once anything points into it; a caught exception's return value; a protection that cannot be restored; groups of sites applied whole; a late detach on the render thread and the runtime's release; the controls help set crossing threads; an attach-time message that cannot be shown); r5 (spec r7 cross-check): AI.exe 2015 analysis by the spec side (the stereo device creation has one call site, so a display reset does not call it again, 02 §6.1), the attach cost stated as 14 §5.10's limits instead of measurements, a process that is not the game, the welcome only on a run where the mod is active, a checked original byte behind a derived written value, the logging of game script events named as in 11 §5, the focus-loss fix described by its result; r6 (CLEANROOM r3, what a player sees may be stated exactly): messages to the player named by the first line a player reads (the texts are in 03 §6.4 for SteamVR, 09 §3.1 and §7 for OpenXR, and 13 §5.3 for all), the earlier mod's SteamVR failure message for error 117 stated as the player-visible source of the Desktop Game Theatre cause, options page items named by their labels in the acceptance checks. Section numbers are unchanged from r5. Revision: r6 -->

# 12. Threads and robustness

Which game threads the mod runs on, what may cross between them, how every
patch is checked before it is written, the ABI rules for replacing game
methods, the per-frame budget rules, and how the mod behaves when something
is missing or fails. These are requirements with their reasons; how the
implementation meets them is its own choice.

Addresses are RVAs (01 §3.3); the sites named here are listed with both
builds in 01 §4.

## 1. Why this document exists

The game is multi-threaded and was never written to host a mod. The
failures that are hardest to find in a mod like this one are not in the VR
logic but here: a replacement that pops the wrong number of bytes, a pose
read while another thread writes it, a log write that stalls a frame, a
runtime shut down under the loader lock at exit, a patch written into code
that is not what the patch expects. Each rule below guards against one of
those.

## 2. Threads

### 2.1 The game's threads relevant to the mod

| Thread | What it is | Facts |
|---|---|---|
| **Loader thread** | the process's first thread while the loader runs `DLL_PROCESS_ATTACH` under the loader lock | no game thread exists yet; `AI.exe`'s C runtime has not started (01 §2.1) [sure] |
| **Game thread** | the same thread once `WinMain` runs: the simulation, scripts, UI, camera, input polling and the game window | runs renderer initialisation (`StereoInitCall`, device creation, `D3D11CreateDeviceAndSwapChain`), the per-frame stereo update, the head-pose fetch (device slot 11), the recenter pose (slot 21), the UI menu open/close methods, the console command dispatch, the stereo mode push and pop, the first-person camera behaviour (including `HeadClipFade`), and `ID3D11Device::CreatePixelShader` while levels stream [sure, measured: same thread id for the head-pose fetch, the stereo update and shader creation] |
| **Render thread** | the game's thread that records and submits frames | runs stereo device slots BeginFrame (5) and EndFrame (6), the game's POV render loop (planar reflections, 07), the final post chain (anti-aliasing, 08 §3) and the `Present` of the game's swapchain [sure, measured: BeginFrame, EndFrame, the runtime calls and the mirror on one thread id, different from the game thread's] |
| **Job threads** | the game's worker pool | run the per-object visibility loop (`CullObjects`, 0x61FD20) and so the distance and occlusion tests the mod replaces (08 §2): 2200 to 4200 distance tests and 100 to 1500 occlusion tests per frame [sure, measured] |
| **Runtime threads** | created by OpenVR, OpenXR runtimes, audio, overlays | not the mod's; the mod must not assume which thread calls a runtime callback |

The game pipelines one frame: while the render thread draws frame k, the
game thread simulates frame k+1. The head pose for a frame is fetched on
the game thread between the render thread's EndFrame of the previous frame
and its BeginFrame of this one; in 600 frames on each runtime it was never
fetched between a BeginFrame and its EndFrame [sure, measured].

### 2.2 Which mod work runs where

| Work | Thread |
|---|---|
| the attach set (§2.3): the log, the crash record's exception filter (11 §6.1), the config file and command line, build identification, the preflight, `MOTHER.PAK` when stale (14), the `AI.exe` patches | loader thread (attach) |
| load the system `d3d11.dll` (once, on the first forwarded call) | whichever thread first calls an export: the game thread [likely] |
| decide the runtime (probe), start it, write the embedded `openvr_api.dll` just before loading it, show a runtime failure message | game thread, from the game's stereo device creation (01 §2.2, 02) |
| swapchain creation changes (windowed, mirror size), hooking `Present`, `ResizeBuffers`, `SetFullscreenState`, `CreatePixelShader`; installing the crash record's filter again after the game's C runtime has replaced it (11 §6.1) | game thread, in `D3D11CreateDeviceAndSwapChain` |
| pixel shader dither patch (08 §1) | game thread (shader creation), during level streaming |
| stereo update: snap turn, recenter, look gate, scene visibility (05) | game thread |
| head pose and recenter pose answers; hand poses | game thread (reads what the render thread published, §3) |
| menu, pause, loading and attract screen wrappers; settings push; console commands; mode stack (05, 06) | game thread |
| gamepad vtable replacements (04) | mostly the game thread [sure, measured about 120 calls per frame] |
| head-in-wall fade delay (08 §4) | game thread (camera update) |
| runtime frame calls (OpenVR `WaitGetPoses`, `Submit`; OpenXR wait, begin, locate, end), eye copies, controller polling and handing the controls to the game, the desktop mirror draw and `Present` | render thread, in BeginFrame and EndFrame |
| OpenXR virtual-screen path when no stereo device exists (09) | render thread, in the `Present` hook |
| per-eye reflections, anti-aliasing in stereo | render thread |
| distance and occlusion test replacements | job threads |
| showing the messages decided at attach (§2.4) | any thread except the loader thread under the lock; the implementation's choice |
| logging of game script events (11 §5) | the thread that calls `ScriptEventTap`; not established (11 §8) |
| keeping focus loss from the game (03 §9.6) | game thread: the game window's messages are delivered on the thread that created the window |
| the check that the headset shows the game (03 §6.7, 09 §3.1) | render thread; the warning it may raise must not block the render thread or the game thread (13 §5.2) |
| the crash record (11 §6.1) | the thread that raised the unhandled exception |

### 2.3 Attach (loader lock)

The work done at attach is a set, not a sequence: the log is opened; the
crash record's exception filter is installed (11 §6.1); the
config file and the command line are read; the build is identified; the
core sites are checked; `MOTHER.PAK` is built when missing or stale; the
`AI.exe` patches of the chosen patch set are written; the messages to show
the player are decided. Only these orderings are required, each for the
reason given:

- the log is open (or found unavailable, 11 §1.1) before anything that can
  fail, so attach itself is recorded;
- the config file and the command line are read before any patch, because
  they decide the patch set (01 §2.3);
- the build is identified and every core check has passed before the first
  write into `AI.exe`, because a failed check means nothing is written
  (§4.2);
- `MOTHER.PAK` is complete before the game opens its UI archive (below).

Everything else may run in any order. Allowed at attach: what 01 §2.2
lists. Heavy work there is limited to one item: **building `MOTHER.PAK`**
from the player's `UI.PAK` when it is missing or stale (14), within the
rebuild limit of 14 §5.10. It must be complete before the game opens its
UI archive, which it does through the redirected path during start-up, on
the game thread, before the front end shows; there is no later hook early
enough. When the archive is current, the UI archive work at attach stays
within the fast-path limit of 14 §5.10.

Not at attach, because each can deadlock or misbehave under the loader
lock: loading the system `d3d11.dll`, writing `openvr_api.dll`, starting a
VR runtime, the OpenXR registry probe, showing a message box, calling any
game function.

No exception may propagate out of `DllMain`: Windows does not catch an
exception that unwinds out of a DLL entry point into the loader during
process start-up, so the process terminates before the game starts.
Failures during attach are logged, and attach continues with what
succeeded.

### 2.4 Messages decided at attach

Some messages to the player are decided during attach: unsupported build
(the box starting `Exe check failed!`), first-launch welcome (starting
`Welcome to`, only on a run where the mod is active, 01 §2.4), the
`UI.PAK` message (starting `Failed to find UI.PAK!`) and the UI-archive
message (13 §5.3 gives every text exactly). None is shown in a process that
is not the game (01 §3.1). The requirements (project decision; the reasons
are given with each):

- never shown under the loader lock: a message box runs a message loop,
  and waiting on the player there blocks every other thread that loads a
  DLL;
- shown in front of the game window, not behind it: the game window may be
  fullscreen or topmost, and a box behind it leaves the player looking at a
  frozen game;
- the game's device creation (`D3D11CreateDeviceAndSwapChain` through the
  proxy) does not complete until every one of them is closed, so the game
  does not start rendering under a box; game start-up before that point may
  continue;
- the config template of a first launch is written only after the welcome
  message is closed, because a process that exits first (a Steam relaunch)
  must leave the welcome for the next launch (13 §3.4);
- if a message cannot be shown at all (Windows refuses to create it), its
  content is logged with the reason (11 §2.6), device creation does not
  wait for it, and when it is the welcome the config template is not
  written, so the welcome is tried again at the next launch (13 §5.2): a
  message the player never saw must neither hold up the game nor count as
  seen.

How and on which thread they are shown (and in which order among
themselves) is the implementation's choice.

## 3. Data that crosses threads

### 3.1 Rule

Every value written on one thread and read on another is read whole: never
a mix of an old and a new write.

- A single naturally aligned 32-bit value (a pointer, an int, a float, a
  bool) is accessed with atomic loads and stores (relaxed order is enough
  where no other data depends on it; release/acquire where it publishes
  other data).
- A multi-word value (a pose, a set of views, a matrix) is handed over so
  that a reader always gets one complete write, the writer never waits,
  and a reader never waits longer than one copy takes. The handover must be
  free of data races under the C++ memory model: a plain copy that races
  with a write is undefined behaviour even if a later check discards it. A
  sequence lock whose payload is read and written with atomic accesses, or
  a double buffer with an atomic index, both meet this; the choice is the
  implementation's.
- No lock is held across a runtime call, a game call or a log write on the
  render thread or a job thread.

### 3.2 The crossings

| Data | Writer | Readers | Requirement |
|---|---|---|---|
| the stereo object's device pointer and the device state (02) | game thread (device creation, StartSensor/StopSensor); render thread (a late detach in configure rendering, 02 §7.4, 09 §2.3) | job threads (the cull replacements), render thread (AA, reflections, mirror), game thread | atomic 32-bit loads and stores **on the fields at their game offsets**: the game's layout of the stereo object and device must not change (02). After a detach, no call starts using the runtime, and the runtime is not released while a call into it that began before the detach may still run on another thread: the release waits until no such call can be in progress (bounded, so the render thread never waits long), or the runtime is kept until process exit and only stops being used |
| the views (eye poses, FOVs, head pose, display time) of the frame the render thread began | render thread (after the runtime's frame wait) | game thread (head-pose fetch, hand poses, recenter) | read whole (§3.1) |
| the pose the game rendered with (the views handed out by the head-pose fetch) | game thread | render thread (declares exactly that pose at submission) | read whole (§3.1); the render thread must also know whether the game fetched a pose for the frame being submitted, and declare that pose when it did |
| the recenter reference (yaw pose) | game thread | render thread (places the virtual screen, 03, 09) | read whole (§3.1) |
| hand grip poses and velocities | render thread (right after `WaitGetPoses`, or with the frame's views on OpenXR) | game thread | read whole (§3.1), and consistent with the views of the same prediction |
| the controls handed to the game's input actions (04) | render thread (end of submission) | game thread (gamepad replacements) | each value a single aligned scalar, written and read whole; no cross-value consistency needed (the game samples them within one step) |
| "VR controller activity" and other flags | render thread | game thread | atomic |
| device poses for the near-head check (04) | render thread takes **its own** copy from the runtime | | the game thread's pose buffer is never read on the render thread |
| the game's swapchain pointer (proxy) | game thread | render thread (`Present` hook) | atomic, release/acquire |
| interaction and hiding state (11 §5) | the tap's thread | any | atomic |
| the controls help entries (04 §5.7) | render thread (chosen when the controller is identified) | game thread (pushed when a menu opens, 06 §4.3) | a reader gets one whole set, the old one or the new one, never the entries of one set with the count of another, and never reads outside the set it got |

A torn OpenXR head-pose handover was observed in game in a test build (a
copy on the game thread overlapping the render thread's write of newly
located views). Reading whole is a correctness requirement, not an
optimisation.

### 3.3 Job threads

The distance and occlusion replacements run thousands of times per frame on
several job threads at once. Each must do no more than: read the device
pointer and state atomically, and either answer "visible" or call the
game's original test with the same arguments. No locks, allocation,
logging or other shared writes.

## 4. Patching: verify before writing

### 4.1 Rule

Before writing anything into `AI.exe`, the mod checks that each site holds
exactly what the patch was written for. If a check fails, it writes
nothing for that patch and logs which site differed (11 §2.1). A patch
written over unexpected code corrupts the game in ways that surface far from
the cause (a crash minutes later, or the game running its own LibOVR
initialisation over the mod's stereo object).

The table gives, per class of site, the minimum evidence the check must
produce before that site is written. The expected values per site are game
facts in 01 (and 02, 05, 07, 08, 10 for their sites); how the checks are
organised is the implementation's choice.

| Class of site | Minimum evidence before writing |
|---|---|
| any site | its address lies inside the loaded image (below `SizeOfImage`) |
| a `call rel32` to redirect | it is a `call rel32` (opcode `E8`); where 01 gives the callee for this build, the call resolves to it (so a callee 01 gives only for 2015 is checked on 2015, and on 2020 the rules after it apply); where 01 gives none, it resolves into `AI.exe`'s code, and sites that 01 marks as calling the same function resolve to the same target |
| a byte patch | the original bytes 01 lists for the parts of the instruction that do not depend on the build (opcode, ModRM pattern, immediate); where the written value is derived from an original byte (the mouse displacement low bytes, 05 §7.7), that byte too, because the derived value is right only for that original (01 §2.5.1) |
| a data pointer to replace | it points inside the image at a NUL-terminated string with the text 01 gives (`data\ui.pak` for the UI archive path, `ShowBootup` for the intro push). A case-insensitive compare is acceptable for the path, because Windows file names are case-insensitive and either spelling names the same file |
| an immediate operand | it holds the value 01 gives (for example the stereo object size pushed before its allocation, the device field store at `+0x264`) |
| a vtable slot | where 01 gives the function the slot holds, it holds that function; where 01 gives none, it points into `AI.exe`'s code |
| a function entry to detour | its first bytes equal the prologue 01 gives (for example `8b c1 c2 04 00` for `ScriptEventTap`, `55 8b ec 83 e4 f0` for the AA pass, `55 8b ec 83 ec 10` for the renderer clear) |

### 4.2 Core and optional patches

- **Core**: every patch the stereo hand-off, the UI set and the gamepad
  need (01 §4, 02, 05, 04). All core sites are checked in one preflight
  before the first write. One failure means **no patch at all**: the game
  runs unmodified (the D3D11 forwarders keep working), the log names each
  differing site, and no message is shown (01 §2.4).
- **Optional**: each fix beyond the original (per-eye reflections 07,
  stereo AA 08 §3, culling 08 §2, head-in-wall delay 08 §4, the logging
  of game script events 11 §5) checks its own sites just before it is installed and stays off on
  its own when they differ or when its address for this build is unknown
  (0). The rest of the mod is unaffected.
- A write that fails after its checks passed (Windows refusing the
  protection change or the write, not a content mismatch) is logged with
  the site and the Windows error, and no further core patch is attempted.
  The result is a game that is partly modified: the patches already
  written stay, and the log must make that state clear (11 §2.1). The
  checks are what keep this from happening in practice.
- Some sites work only as a group, because a part in place without the rest
  breaks the game's own behaviour (the gamepad's replaced slots with the
  connected-flag patch and the left-stick redirect; 04 §3.3 says why). A
  write failure inside such a group leaves none of the group in place: the
  parts already written are restored to the values they held before (each
  was checked against a known value, §4.1), and the area's outcome event
  says the area is off and which site failed (11 §2.1).

### 4.3 Writing

- Change the page protection to writable for the write, restore the
  previous protection afterwards, and flush the instruction cache for code
  (`WriteProcessMemory` on the own process does both the write and the
  flush; `VirtualProtect` plus `FlushInstructionCache` is equivalent).
  If the previous protection cannot be restored after a successful write,
  the write counts as done and the failure is logged with the address and
  the Windows error; the page stays writable, which does not change the
  game's behaviour.
- Every write's result is checked and a failure logged with the Windows
  error code.
- Code patches are written at attach, before any game thread exists, so no
  game thread can execute a half-written instruction. Patches applied later
  (DXGI and D3D11 vtable slots at device creation, the audio endpoint
  vtable, 10) are single aligned 4-byte slot writes.
- A redirected call keeps the call instruction's length; byte patches keep
  instruction boundaries (pad with `nop`) so no jump lands inside a changed
  instruction.

### 4.4 Shared COM vtables

A vtable slot of a DXGI swapchain or D3D11 device class is shared by every
object of that class in the process, including those an OpenXR runtime, an
overlay or a capture tool creates in the game process. A replacement must
act only on the game's own object (the swapchain last created through the
proxy's `D3D11CreateDeviceAndSwapChain`) and pass every other call
straight through. The pixel-shader replacement may see every device's
shaders: it changes only bytecode that matches the game's dither pattern
exactly (08 §1), so others pass unchanged.

## 5. ABI rules for replacing game methods

### 5.1 thiscall

The game's C++ methods use MSVC x86 `thiscall`: `this` in ECX, arguments on
the stack right to left, and the **callee** pops its stack arguments (`ret
N`). A replacement written in C or C++ must have exactly the same
contract. The usual way is a `fastcall` function whose first parameter is
`this` (ECX), whose second parameter is an unused placeholder for EDX, and
whose remaining parameters are the method's stack arguments: `fastcall`
then pops the same N bytes. The placeholder's value is garbage and must not
be used. When the replacement calls the game's original, it passes `this`
in ECX the same way.

Other conventions the mod meets:

| Convention | Rule |
|---|---|
| `cdecl` (the cull tests, the full-screen draw, the SMAA runner, the renderer clear, renderer accessors) | caller pops; the replacement pops nothing |
| `stdcall` (`HeadClipFade`, COM methods) | callee pops all arguments; for COM, `this` is the first stack argument |
| `bool` results | in AL; the callers of the cull tests test AL only |
| `float` results | in x87 ST0 (`HeadClipFade`); the x87 stack must be balanced on return |
| pointer results | in EAX; several device methods return one of their pointer arguments (02 lists which) |

### 5.2 Getting it wrong

A mismatch in the number of bytes popped corrupts the caller's stack and
crashes at the caller's next return or local access, usually on the first
call. Each replaced method's
convention and byte count is in 01 and 02; check each against the game's
own `ret N` before writing the replacement.

### 5.3 Other register and stack rules

- Preserve EBX, ESI, EDI and EBP (any C/C++ compiler does).
- Do not assume a 16-byte aligned stack on entry: the game's own aligned
  functions realign themselves (`and esp, -16` in their prologue).
- Leave the x87 control word and the SSE control register (`MXCSR`) as
  found.
- No C++ exception may leave a replacement into game code: each entry
  point the game can call catches everything (or cannot throw).

## 6. Per-frame budget

Measured headroom: about 50 % of the render thread and 25 % of the GPU at
90 Hz in a menu scene with the eyes at 2688x2688. The mod's own per-frame
cost must stay small and, above all, never spike.

### 6.1 Render thread

Per frame, the render thread must not:

- allocate or free heap memory (resources are created only when a size or
  format changes, and released before being recreated);
- open, read or write a file, or log (11 §3: once per change, or every 900
  frames behind `DebugOptions`);
- read back from the GPU (`Map` of a GPU resource, queries) or flush the
  immediate context, except the one flush right after the OpenXR eye copies
  so the GPU starts them before `xrEndFrame` waits for them (09);
- wait on anything except the runtime's own frame pacing
  (`WaitGetPoses`, `xrWaitFrame`, `xrWaitSwapchainImage`, `xrEndFrame`);
- take a lock the game thread may hold.

Exceptions, both only while `DebugOptions` is on: the reflection
capture (11 §4.4, on the key press only) and the periodic diagnostics
(every 900 frames).

### 6.2 Game thread

- The pixel-shader patch runs during level streaming on the game thread.
  It must reject a shader cheaply before any checksum or parse: only
  bytecode that contains the dither's immediate `0x3A83126F` twice in a row
  is a candidate (08 §1). Hashing every shader added about 20 µs each, up to
  1.5 s per session.
- The per-frame stereo update, head-pose fetch and gamepad replacements
  together measured under 0.01 ms per frame; keep them free of allocation
  and file I/O as well.

### 6.3 Display reset

Raising the game's display-reset flag makes the game rebuild every render
target, a 180 to 250 ms freeze and hundreds of MB of reallocation. The mod
raises it only when a setting that sizes the targets or the mirror changed
since they were built: pixel scale, mirror view type, symmetric mirror (06,
03). Opening a menu, the pause screen or a loading screen must not raise it.

## 7. Behaviour when something is missing or fails

| Situation | Required behaviour |
|---|---|
| the process is not the game (its executable is not `AI.exe`, 01 §3.1) | forward the Direct3D 11 exports only; log one event naming the host (11 §2.1); no settings, message, filter, patch or other file |
| `AI.exe` is not a recognised build | patch nothing; the forwarders work; log the timestamp; the unsupported-build message of 13 §5.3 (01 §2.4); no welcome and no first-launch `mothervr.json`, because the welcome would describe a mod that is not running (01 §2.4) |
| a core site differs (preflight) | patch nothing; log every differing site; the game runs unmodified |
| an optional fix's site differs or its address is unknown on this build | that fix stays off; log it; everything else works |
| `UI.PAK` missing or not the expected version | no options page this session; VR works; the message 13 §5.3 gives for the cause (`Failed to find UI.PAK!` when the file is missing; 06, 14) |
| config file missing, unreadable or invalid | defaults; out-of-range values sanitised (06) |
| the game folder cannot be written (the player's account lacks write access; project decision: the mod never requires elevation, a registry change or a folder rename, because players cannot be expected to change their installation for it) | the game still starts and each missing file degrades on its own: events go to the debugger output when the log cannot be opened (11 §1.1); without a usable `MOTHER.PAK` there is no options page and the UI-archive message of 13 §5.3 is shown; without a local `openvr_api.dll` the runtime's own loader is used (13 §3.2); a `mothervr.json` that cannot be written leaves the settings in effect for this session only (13 §3.6) |
| runtime None (configured or `-vrforcenone`) | the unmodded game: only the UI set of 01 §2.3 is patched, no stereo device, no D3D11 wrapping beyond the forwarders, no shader patch |
| the configured runtime is not installed (not forced) | fall back to another installed runtime, OpenVR first, then OpenXR; log why (11 §2.3) |
| a runtime forced on the command line is not installed, or none is installed | no stereo device; the game runs flat on the stereo path's no-device behaviour (02); logged, no message box |
| the runtime fails to start (OpenVR `VR_Init` error, OpenXR instance, system or session failure) | shut the runtime down again, destroy the backend, leave the stereo object without a device (device pointer null, state none) and the options page's VR-enabled flag off, with the page showing runtime None while the stored `VRRuntime` is kept (06 §5.2), show the runtime-failure message once: `Failed to initialize SteamVR!` (03 §6.4, 13 §5.3) or `Failed to initialize OpenXR!` (09 §7), whose bracket line gives the error's description, never empty, and its code, and whose last paragraph says that VR stays off until a restart; the OpenXR one also says how to go back to SteamVR. For OpenVR's `EVRInitError` value 117 (`VRInitError_Init_AnotherAppLaunching` in the public OpenVR headers) the SteamVR message is the variant of 03 §6.4 that adds the paragraphs naming SteamVR Desktop Game Theatre and Steam's setting for it [likely cause: the earlier mod's message for this error, which players saw, named the Theatre as a possible cause; not reproduced by the spec side]. The game continues flat; it must not be left in Rift mode with no device, rendering both eyes into a window that nothing submits |
| the OpenXR session cannot be created on the game's D3D11 device (found when the game configures rendering, for example the game on a GPU other than the headset's) | the same detach and message, at that point. Calls into the stereo device that the game makes after the detach (it may already be committed to rendering this frame) must still be safe: none may use a resource the failed path did not create. The `Present` path must not try to start the runtime again |
| the runtime asks the application to quit (OpenXR session `EXITING`, 09 §3) | set the game's quit request flag, as the game did for its old runtime's quit signal; log; VR stays off until restart |
| the OpenXR session or instance is lost (`LOSS_PENDING`, instance loss pending, 09 §3) | VR stops for this run; the game is not asked to quit and keeps running in stereo on the mirror (09 §3); logged |
| headset not worn, or not upright | keep rendering and submitting; recenter requests wait until it is worn and upright (03, 05) |
| the runtime's dashboard has input focus | controller state reads as zero, not as the last value (04) |
| another swapchain or device in the process | its calls pass through untouched (§4.4) |
| the game asks for fullscreen while a headset is attached | refused with `DXGI_ERROR_NOT_CURRENTLY_AVAILABLE`, the swapchain stays windowed (03) |
| the game window does not fit its monitor (its work area for a window with a title bar) | made borderless and placed over its whole monitor; a window that fits is left alone; asynchronously, because the call may come from a thread other than the window's owner while that thread waits (03 §9.3) |
| another application takes the desktop focus while a headset is attached | the game is not paused: its window does not receive the focus loss (03 §9.6); activation still reaches it; logged with the application's executable. Keyboard and mouse stop until the game window has the focus again; VR controllers and the gamepad keep working |
| the runtime started but does not show the game in the headset (SteamVR's Desktop Game Theatre: OpenVR's scene application is another process about 3 s of frames after the first submission, 03 §6.7; or an OpenXR session not `VISIBLE` or `FOCUSED` within about 10 s of running, 09 §3.1) | logged; the player is warned once without blocking (13 §5.3); VR stays on and frames keep being submitted |
| `ScriptEventTap` is reached with an argument that is not a string (its address also sits in pointer tables, 11 §5.1) | the argument is read only when it points into the game's image; otherwise the call passes through untouched (11 §5.2) |
| display reset (the game rebuilds its targets; it does not call its stereo device creation again, which has one call site, 02 §6.1) | the eye layout follows the texture the game actually creates (03); the swapchain is resized to the mirror size. A repeated creation call is not expected but still handled: the runtime is started once per process, so a second call re-attaches the device that exists (or none after a failure) and never shows a failure message again (03 §6.5) |
| writing `openvr_api.dll` fails part way | delete the partial file; the file counts as current only after a complete write (13 §3.2) |
| loading `openvr_api.dll` | the DLLs that `openvr_api.dll` loads resolve from its own directory, and the process's DLL search path is the same after the load as before, because the game and other modules load DLLs later and must not pick up files from the mod's choice of directory. `LoadLibraryExW` with `LOAD_WITH_ALTERED_SEARCH_PATH` or with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR`, or a `SetDllDirectoryW` change undone right after the load, all meet this |
| process exit (`DLL_PROCESS_DETACH` with a non-null reserved argument) | log the detach marker and return: no runtime shutdown (`VR_Shutdown` and `xrDestroyInstance` wait on runtime threads and can deadlock under the loader lock), no release of D3D objects (the game's still exist), no `FreeLibrary` of the system `d3d11.dll`, and no static destructor that calls into a runtime. The OS and the runtime clean up after an ended process |
| dynamic unload (reserved argument null) | free the system `d3d11.dll`. In the game process this cannot happen: from attach on the game holds pointers into the proxy (the crash record's filter, then the patches, vtable slots, detours and the replacement of the game window's focus-loss handling, 03 §9.6), and game code would call into unmapped memory after an unload, so the proxy keeps itself loaded until the process ends (01 §1.4; pinning its own module, for example with `GetModuleHandleExW` and `GET_MODULE_HANDLE_EX_FLAG_PIN`, is one way). A dynamic unload happens only in a host that is not the game (01 §3.1) |
| an exception in any mod code the game calls | caught at the entry point and logged once per entry point; never propagated into game code. The call then returns what that slot returns without a device (02 §7.5), or passes the call to the game's original where the mod replaced one |
| an unhandled exception (a crash, in the game or the mod; including a stack overflow) | the crash record of 11 §6.1 is logged once, without allocation and within the stack a stack overflow leaves, then the exception goes to the filter the mod's replaced, so Windows records the crash as it would without the mod. The mod's filter is installed again after the game's C runtime has replaced it with its own, which does not chain (11 §6.1) |

## 8. Acceptance

1. Thread check (debug build that records the thread id of each entry
   point once): the head-pose fetch, the stereo update and the shader
   replacement share one id; BeginFrame, EndFrame and `Present` share
   another; the cull replacements report several other ids.
2. Play 10 minutes with OpenXR and with OpenVR: no visible pose jump in the
   headset. With OpenXR and `DebugOptions` on, the pose-age events (11
   §4.1) report 900 of 900 frames on time (OpenVR has no pose-age probe).
3. Edit a copy of `AI.exe` so one core site differs (for example the
   `jne` at `SsaoSettingBranch`): the log lists that site, no patch line
   follows, the game starts flat and unmodified.
4. Edit a copy so only `ReflectionRenderCall` differs: the log says
   reflections are not patched, VR and every other fix work.
5. Start with OpenVR selected and SteamVR not installed: if an OpenXR
   runtime exists the log shows the fallback to OpenXR; with
   `-vrforceopenvr` instead, no stereo device and the game runs flat.
6. Start with the headset unplugged (SteamVR installed): one
   `Failed to initialize SteamVR!` message box (03 §6.4) with the
   runtime's error text and code; the game reaches the main menu on the
   monitor at its normal resolution; under Options > `MotherVR`, the
   `Rendering` and `Input` entries are greyed and the `VR Runtime` page's
   `VR Runtime Set` shows `None`, while `mothervr.json` still holds the
   stored runtime (06 §5.2).
7. Change the pixel scale in the menu: one `ResizeBuffers` and one freeze;
   open and close the pause menu ten times: no `ResizeBuffers` line and no
   freeze.
8. Quit from the main menu with VR running: the process ends within a few
   seconds, the log's last line for that process id is the detach marker,
   and Event Viewer records no `AI.exe` error.
9. First launch without `mothervr.json`: the welcome box (13 §5.3,
   starting `Welcome to`) appears in front of the game, the game window
   does not render until it is closed, and the config file exists
   afterwards.
10. Run with a performance overlay (SteamVR frame timing): no periodic
    render-thread spikes at a fixed interval during steady play.
11. Remove write access to the game folder for the player's account
    (`MOTHER.PAK`, `mothervr.log` and `openvr_api.dll` absent) and start
    with OpenVR: the game reaches the main menu, VR runs through the
    runtime's own `openvr_api.dll`, Options has no `MotherVR` entry and
    the UI-archive message of 13 §5.3 is shown, no elevation prompt
    appears, and a debug-output viewer shows the events.
12. Edit a copy of `AI.exe` so a redirected call site (for example
    `StereoUpdateCall`) calls another function of the game: the log names
    that site as failing its callee check and nothing is patched.
13. Write failure in a group (a developer build that makes the write of
    the third gamepad slot fail): the slots already written hold their
    original values again, the controls area is logged as off with the
    failing slot, and the real gamepad works as in the unmodded game.
14. Controls help across threads: with OpenVR, switch controllers (a wand
    and then a Touch-style controller) while opening and closing the pause
    menu repeatedly: the `View Controls` page always shows one complete
    set of rows (04 §5.7), never rows of both.
15. Unload safety: a developer build that calls `FreeLibrary` on the
    proxy, after the game has started, more times than it was loaded: the
    proxy stays loaded and the game keeps running.
16. Exception in a replacement (a developer build that throws in a
    replaced device method): one log event for that entry point, the game
    keeps running, and later throws from the same entry point add no
    events.
17. OpenXR loss: stop SteamVR's compositor while the game runs on OpenXR
    (or end the runtime process): the log shows the loss, the game does
    not quit and keeps running on the monitor.

## 9. Open questions

- Which thread calls `IDXGISwapChain::ResizeBuffers` on a display reset
  (game or render thread). To resolve: record the thread id in a debug
  build while changing the pixel scale. The window fit is asynchronous
  either way.
- Which thread calls `ScriptEventTap` (11 §8).
- Whether the game creates pixel shaders on threads other than the game
  thread in some levels (only the game thread was seen, in menus and one
  short segment). To resolve: record thread ids of the shader replacement
  during a full level load. The replacement is thread-safe either way, but
  the timing rule of §6.2 assumes the game thread.
