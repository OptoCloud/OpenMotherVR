<!-- Provenance: AI.exe 2015 analysis (PE headers, imports, code at each listed RVA; every patch-site and preflight byte in this revision re-read from the installed Steam 2015 AI.exe, timestamp 0x54E3199D); Windows 11 load tests in the game directory; SysWOW64\d3d11.dll export table; the built proxy's import and export tables; Windows' built-in application compatibility database (its entry for AI.EXE); 2020 values were taken from the earlier mod's per-build address table (and, for inline sites, its installer) and have not been verified against a 2020 `AI.exe`; this project's own design decisions for v1 (preflight, runtime probing and fallback, deferred message boxes, display-reset re-entry); orderings are stated only where §2.6 gives a game, Windows or runtime reason; in-game runs on the 2015 build with SteamVR and Index; user decision on flat play with runtime None (answer to Q6, ANSWERS.md); user decision that the UI archive is built as defined in 14-ui-resources.md; AI.exe 2015 analysis by the spec side for r5 (the five pointer-table slots that also hold ScriptEventTap's address); a tester's report on SteamVR with a Steam Frame headset (OpenXR not starting from a leftover 32-bit registration, Desktop Game Theatre, a crash without a record); project decisions of r5, each with its reason inline (runtime discovery order, the options page shows the runtime this run drives, the crash record's filter installed at attach); clean-room review round 2 and coverage review of spec r6, applied in r6 (the 2020 mouse displacement bytes credited to the earlier mod's installer; the OpenXR discovery rule given with its reason; probing leaves the process environment alone; the function at RVA 0x5E0D40 labelled `CreateDevice` as in 02 and 03; the reflection fix all-or-nothing as 07 requires; exact vtable slot checks; the skip-intro operand only with the mod's archive; the device creation export's window results; the proxy pinned in the game process; a host that is not the game; no welcome on a run where the mod is not active; 2020 structure offsets unverified); AI.exe 2015 analysis by the spec side for r6 (`CreateDevice`'s only call site; the game's window procedure and its `WM_ACTIVATE` and `WM_KILLFOCUS` branches; the C runtime's unhandled-exception filter, installed by a C runtime initialiser after the DLL attach, and its fail-fast paths); project decision of r6 with its reason inline (the crash record's filter installed again at the first device creation call); cross-document pass after the second clean-room review, applied in r7 (the UI archive redirect string stated by its form as in 14; the two processes of a Steam restart may overlap in time; the window procedure no longer described by one means of keeping focus loss from it; the v2 address tables pointed to); AI.exe 2015 analysis by the spec side for r7 (the game window's `WM_SIZE` path to the size handler of the object holding its display mode; the low 16-bit read and the 0x17 limit behind `PadControlToName`; the camera behaviour's yaw setter, its per-update method and the slots that fill its character and head positions; the padding after `ScriptEventTap` and the RVAs of its five pointer-table slots); r8 (CLEANROOM r3, what a player sees may be stated exactly): the unsupported-build message given as its exact text, caption, icon and button (the earlier mod's text, kept because the behaviour is unchanged; the caption without a version as a project decision), the other messages named by their exact first lines, the options page and its controls named by the labels a player sees. Revision: r8 -->

# 01. Loading, build identification, runtime selection and AI.exe addresses

This document covers how the mod gets into `AI.exe` on Windows 11, what it
may do at load time and in which order, how it tells game builds apart, the
preflight that checks every patch site before anything is written, how the
VR runtime for a run is chosen, and the full list of game addresses that
the other documents use. Files the mod writes and the texts of the
messages a player sees are in `13-distribution.md` (the unsupported-build
message, which belongs to build identification, is also given exactly in
§3.1); how the UI archive is built is in `14-ui-resources.md`.

Conventions: AI.exe is a 32-bit (x86) executable with preferred image base
0x400000. Addresses here are **RVAs** unless they say VA (VA = image base +
RVA, so VAs quoted from the 2015 build assume base 0x400000). `thiscall`
means `this` in ECX, arguments on the stack, callee pops them; `ret N` is
the number of argument bytes the callee pops. `cdecl` means the caller pops.
In the 2020 columns, `?` means unknown: the feature that needs the address
stays off on that build. 2020 values were taken from the earlier mod's
per-build address table (and, for inline sites, its installer) and have not
been verified against a 2020 `AI.exe`.

## 1. Entry: a `d3d11.dll` proxy

### 1.1 Requirement

The mod ships as a single 32-bit `d3d11.dll` placed in the game directory,
next to `AI.exe`. The Windows loader finds it there before the system copy,
because `AI.exe` imports `d3d11.dll` statically [sure]:

| DLL | Functions AI.exe imports (by name) |
|---|---|
| `d3d11.dll` | `D3D11CreateDeviceAndSwapChain` |
| `dxgi.dll` | `CreateDXGIFactory`, `CreateDXGIFactory1` |
| `XINPUT1_3.dll` | ordinals 2, 3, 4 |

No installer, launcher or injector is used. Removing the file restores the
unmodified game (`13-distribution.md` §6 lists the other files to delete).

The proxy itself must not import `d3d11.dll` (it would import itself) and
must not import any C runtime DLL (`13-distribution.md` §7). It imports
only system DLLs that every supported Windows ships (such as `KERNEL32`,
`USER32`, `ADVAPI32`, `SHELL32` and `ole32`); DXGI and D3D11 are used only
through interfaces the game's device hands out.

### 1.2 Why not `dxgi.dll`

A `dxgi.dll` proxy in the game directory does not work on Windows 11.
Tests on Windows 11 build 26200 with the 2015 Steam build: any local
`dxgi.dll`, even a minimal one with no C runtime and only five kernel32
imports, made `AI.exe` crash with an access violation at address 0 during
process start, before the DLL's entry point ran. KnownDLLs (dxgi is not
listed), Smart App Control (off), C runtime start-up, missing exports and
PE structure were ruled out. A `d3d11.dll` built the same way loads and its
entry point runs. The mod must not depend on a local `dxgi.dll` at all
[sure].

The probable cause [likely]: Windows' built-in application compatibility
database has an entry for `AI.EXE` (application "Alien: Isolation", vendor
SEGA) that matches only when the folder holding the executable is named
`Alien Isolation`, as a Steam install is. The entry applies a DXGI
compatibility shim (a 10-bit back-buffer upgrade). A shim on DXGI is
installed while the process starts, which fits a fault in a local
`dxgi.dll` before its entry point runs; the reasoning is not confirmed by a
test with the shim disabled. The `d3d11.dll` proxy is not affected, since
it replaces no DLL the shim targets. The mod never needs a registry change
or a renamed game folder to load.

### 1.3 Exports

Once the proxy is loaded as `d3d11.dll`, every later import of `d3d11.dll`
in the process binds to it by name, not only AI.exe's. The VR runtime's
32-bit client library imports `D3D11CreateDevice`; with that export missing
its load fails and OpenVR initialisation reports
`VRInitError_Init_VRClientDLLNotFound` (102) [sure, observed]. The proxy
must therefore export every name the system `d3d11.dll` exports and forward
each one unchanged to the system copy.

The 32-bit `C:\Windows\SysWOW64\d3d11.dll` on current Windows 11 exports
these 51 names (the proxy exports exactly these 51):

```
CreateDirect3D11DeviceFromDXGIDevice   D3DKMTGetDeviceState
CreateDirect3D11SurfaceFromDXGISurface D3DKMTGetDisplayModeList
D3D11CoreCreateDevice                  D3DKMTGetMultisampleMethodList
D3D11CoreCreateLayeredDevice           D3DKMTGetRuntimeData
D3D11CoreGetLayeredDeviceSize          D3DKMTGetSharedPrimaryHandle
D3D11CoreRegisterLayers                D3DKMTLock
D3D11CreateDevice                      D3DKMTOpenAdapterFromHdc
D3D11CreateDeviceAndSwapChain          D3DKMTOpenResource
D3D11CreateDeviceForD3D12              D3DKMTPresent
D3D11On12CreateDevice                  D3DKMTQueryAdapterInfo
D3DKMTCloseAdapter                     D3DKMTQueryAllocationResidency
D3DKMTCreateAllocation                 D3DKMTQueryResourceInfo
D3DKMTCreateContext                    D3DKMTRender
D3DKMTCreateDevice                     D3DKMTSetAllocationPriority
D3DKMTCreateSynchronizationObject      D3DKMTSetContextSchedulingPriority
D3DKMTDestroyAllocation                D3DKMTSetDisplayMode
D3DKMTDestroyContext                   D3DKMTSetDisplayPrivateDriverFormat
D3DKMTDestroyDevice                    D3DKMTSetGammaRamp
D3DKMTDestroySynchronizationObject     D3DKMTSetVidPnSourceOwner
D3DKMTEscape                           D3DKMTSignalSynchronizationObject
D3DKMTGetContextSchedulingPriority     D3DKMTUnlock
D3DKMTWaitForSynchronizationObject     D3DPerformance_BeginEvent
D3DKMTWaitForVerticalBlankEvent        D3DPerformance_EndEvent
D3DPerformance_GetStatus               D3DPerformance_SetMarker
EnableFeatureLevelUpgrade              OpenAdapter10
OpenAdapter10_2
```

Requirements:

- **Lazy load.** The system DLL is loaded on the first call of any export,
  once, thread-safely, not in `DllMain`. Later calls find it loaded at the
  cost of one compare.
- **Full path.** Load it as `<GetSystemDirectory()>\d3d11.dll`
  (`SysWOW64` for a 32-bit process on 64-bit Windows) with
  `LoadLibraryExW(..., LOAD_WITH_ALTERED_SEARCH_PATH)`. Loading `d3d11.dll`
  by bare name would return the proxy itself. If the returned module is the
  proxy, or the resolved `D3D11CreateDeviceAndSwapChain` is the proxy's own
  export, treat the load as failed.
- **Transparent forwarding.** A forwarder must not change arguments,
  registers, return values or the stack. The one-time load that the first
  call triggers must preserve every general register, so that an export
  with a register calling convention still reaches the system function
  intact. The callee's own convention (`__stdcall` for all of these)
  applies.
- **Missing names.** When the system DLL is loaded, resolve all 50
  pass-through names; log each name the system DLL lacks, and a summary
  line (resolved count, missing count). A call to a name that could not be
  resolved must not return a made-up success; it may fault or return
  `E_FAIL` with the correct stack cleanup. On every Windows 10 and 11 build
  tested all 51 names exist [likely], so this path is not reached in
  practice.
- **`D3D11CreateDeviceAndSwapChain`** is the only export the game calls. If
  the system DLL or this name cannot be resolved it logs the problem and
  returns `E_FAIL`. Otherwise, in this order:
  1. wait until every attach-time message box of this run has been closed
     (`13-distribution.md` §5.2), so the game does not start rendering
     under a box;
  2. when a stereo device is attached (02), adjust the swap-chain
     description as `03-hmd-and-frames.md` §9.2 requires (windowed, mirror
     size);
  3. call the system function;
  4. on success, install the device and swapchain replacements that 03 §9
     and 08 §1 require (pixel shader creation for 08 §1; `Present`,
     `SetFullscreenState`, `ResizeBuffers` for 03 §9), and, when a stereo
     device is attached, apply the window results of 03 §9 to the
     swapchain's output window: fitted to its monitor (03 §9.3), DXGI's
     Alt+Enter off (03 §9.4), and focus loss not passed to the game
     (03 §9.6). The replacements act only on the game's own swapchain, the
     last one created through this export.

  With runtime None (§2.3) only steps 1 and 3 apply: no replacement is
  installed on the game's device or swapchain, including the pixel shader
  replacement of 08 §1, and the log has no swapchain, shader or window
  lines.

  On its first call the export also makes the crash record's filter the
  process's top-level unhandled-exception filter again, on every run in
  the game process (§2.2).

### 1.4 Detach

On `DLL_PROCESS_DETACH` at process exit (`lpReserved` non-null) the mod
only appends the detach line to the log, which is already open (11 §2.1):
other threads are gone, the game's D3D objects are still alive and the
loader lock is held, so no runtime shutdown, D3D release or library unload
happens then (00 §4.2 item 24, 12 §7).

In the game process the proxy keeps itself loaded until the process ends
(for example `GetModuleHandleExW` with `GET_MODULE_HANDLE_EX_FLAG_PIN` at
attach), because from attach on the game holds pointers into it (the crash
record's filter, then the redirected calls, vtable slots, detours and the
window message handling of 03 §9.6) and freeing it would leave game code calling unmapped
memory. A dynamic unload (`lpReserved` null) can therefore only happen in a
host that is not the game (§3.1); there the proxy frees the system
`d3d11.dll` and logs the detach line.

`DLL_THREAD_ATTACH` and `DLL_THREAD_DETACH` notifications are disabled
(`DisableThreadLibraryCalls`).

## 2. Initialisation order

### 2.1 What happens when

Facts about the 2015 build, in process order [sure]:

1. The loader maps `AI.exe` and its static imports and runs their entry
   points, including the proxy's `DLL_PROCESS_ATTACH`, under the loader
   lock, on the process's main thread. `AI.exe`'s own C runtime has **not**
   started: none of its static constructors has run.
2. `AI.exe`'s C runtime initialises itself. Its C initialisers run first
   (the table at RVA 0xDBB124 to 0xDBB144); one of them (`CrtFilterInstall`,
   §4.6) makes the C runtime's own filter the process's top-level
   unhandled-exception filter, replacing any filter installed during the
   DLL attach. Then it runs its static constructors. One of them (at RVA
   0xD74F30) constructs the renderer (RVA 0x5AFA70), and that constructor
   allocates the game's stereo object at site `StereoObjectAlloc`
   (RVA 0x5AFBA1, see §4.2). Other static constructors compute the
   message-name hashes in §4.5 (`MsgCutsceneVisibilityStart` and the
   following three); before that the globals are zero.
3. `WinMain` starts. Renderer initialisation (function at RVA 0x6138F0)
   calls `CreateDevice(1)` at site `StereoInitCall` (RVA 0x613941), later
   queries the back-buffer size (call at RVA 0x613AE8 into RVA 0x60FC40,
   which contains `StereoRenderTargetSizeCall`), decides the swap-chain
   parameters (`RiftTest7`, RVA 0x613C80) and only then creates the D3D11
   device and swap chain (call at RVA 0x613CB8 into RVA 0x626FA0, which
   calls `D3D11CreateDeviceAndSwapChain`).
4. The front end loads its Scaleform UI from the UI archive named by
   `UiPakPathPtr` and starts the intro sequence (`ShowBootup`, §4.6). The
   front-end menu object first exists when its open method
   (`FrontEndMenuVtable` slot 0) runs.

When the game is started outside Steam, Steam can restart it, so the proxy
attaches in two processes one after the other [sure, observed in the log:
two attach headers with different process ids]. The second process can
start while the first is still running or exiting, so the two can overlap
in time. Everything below must be safe when the first process exits at any
point (for example before a pending attach-time message box was shown), and
every file both processes write (the UI archive, the log, the config file)
must be safe when they overlap (`14-ui-resources.md` §5.8, 11 §1.2).

### 2.2 Requirements at `DLL_PROCESS_ATTACH`

Allowed, and required to happen here:

- Open the log (`13-distribution.md` §4) and write a session header.
- Read the config file (06 §3) and the command line (§5.1).
- Identify the AI.exe build (§3) from the main module's PE header, which is
  already mapped.
- Decide the patch set (§2.3), run the preflight (§2.5) and apply the
  patches. The patches must be in place before step 2 above: the stereo
  object allocation runs from a static constructor, before `WinMain`.
  Patching here also means no game thread is running yet, so no patch can
  race with game code. `.text` and `.rdata` are read-only; for each write
  the mod makes the pages writable (`PAGE_EXECUTE_READWRITE`), writes,
  restores the previous protection and flushes the instruction cache for
  code (`WriteProcessMemory` on the current process does both the write
  and the flush).
- Produce `DATA\MOTHER.PAK` and redirect `UiPakPathPtr` to it (§2.6). This is the only file work at attach; it stays here because the game
  opens its UI archive during step 4 of §2.1 through the pointer patched
  here. How the archive is built, validated and when it is rebuilt is in
  `14-ui-resources.md`.
- Compute absolute addresses from the actual image base (§3.3).
- Install the crash record's unhandled-exception filter
  (`11-diagnostics.md` §6), whatever the build and the patch set, so a
  crash during the rest of the attach and the start of the game's C runtime
  is recorded in the log. Installing it takes no lock and loads nothing, so
  it is safe under the loader lock. The game's C runtime replaces it while
  it initialises (§2.1 step 2) with a filter that never calls the one it
  replaced (§4.6), so the mod installs it again at the first
  `D3D11CreateDeviceAndSwapChain` call (§1.3), chaining to the C runtime's
  filter (project decision: that export is the first point after the C
  runtime's initialisation at which the proxy runs on every run, runtime
  None included, and using it needs no change to the game's code). A crash
  between the C runtime's initialisation and device creation is left to
  Windows' own reporting.
- Decide which attach-time message boxes this run shows
  (`13-distribution.md` §5.2) and arrange for them to be shown: never under
  the loader lock, in front of the game window, and before the game's
  device creation completes (§1.3 step 1). How (threads, queues) is the
  implementation's choice.

Not allowed at attach:

- Calling any AI.exe function or reading game globals expecting values.
  The renderer, input manager, POV manager and stereo pointers are null, the
  message-name hashes are zero, and no UI or script object exists. Anything
  that needs them is done from the game hooks the other documents define
  (for example, settings are pushed to the UI from the front-end menu's open
  method, 06).
- Probing for or starting the VR runtime. Its client library imports
  `d3d11.dll` (which is now the proxy) and creates threads; doing that under
  the loader lock can deadlock, and the game has not decided yet whether
  stereo is wanted. The runtime is probed and started from the game's
  `CreateDevice(1)` call (§5), which is early enough: the back-buffer size
  query and device creation follow it.
- Loading the system `d3d11.dll` (§1.3), writing `openvr_api.dll` or the
  controller bindings (`13-distribution.md` §3), or writing the config file.
- Showing a message box while the loader lock is held (above).
- Letting an exception or a fault leave `DllMain`. Any error is logged and
  attach returns `TRUE`: the proxy must never stop the game from starting.

### 2.3 Patch sets: runtime None means the unmodded game

Flat play must be the unmodded game (user decision, ANSWERS Q6). The patch
set therefore depends on the runtime chosen for this run, which is known at
attach: the config file's `VRRuntime` (06 §1), overridden by a
`-vrforce...` switch (§5.1). The patch set does not depend on which runtimes
are installed; that is only probed later (§5.2).

| Runtime for this run | Applied at attach |
|---|---|
| OpenVR or OpenXR (also `"Oculus"`, which means OpenXR) | **every** code and data patch that 02 to 08 require |
| None (`-vrforcenone`, or `VRRuntime` `"None"`) | only the **UI set** below; no other patch |

The UI set is what the player needs to turn VR on from the menu and nothing
that changes how the game plays or renders:

- the UI archive redirect (`UiPakPathPtr`, `14-ui-resources.md`),
  when the archive is available;
- the UI callback slot (`UiExternalCallSlot`, 06 §4.1), so the options page's
  commands reach the mod;
- the front-end and pause menu Open wrappers (`FrontEndMenuVtable`,
  `PauseMenuVtable` slot 0), doing **only** the settings push of 06 §4.4
  (no stereo-mode handling of 05);
- the skip-intro operand (`ShowBootupPush`, 06 §6), only when
  `SkipStartupIntro` is on and `UiPakPathPtr` was redirected to the mod's
  archive in this run; the stock movie has no skip-intro function, so
  without the mod's archive the operand is left alone and the intro plays.

With runtime None the mod creates no stereo device (`StereoInitCall` is not
redirected, so the game's own `CreateDevice(1)` runs and finds no LibOVR
runtime, as in the unmodded game), does not probe for VR runtimes, does not
patch the Rift tests, the pad, the camera, SSAO, the mode stack, the UI
quads or the render paths of 07 and 08, does not wrap the D3D11 device or
swapchain beyond §1.3, and does not touch the game's pixel shaders (08 §1).
The game plays exactly as without the mod; only the Options menu has the
`MotherVR` page. The preflight (§2.5) for runtime None covers only the UI set's
sites.

When the runtime is OpenVR or OpenXR but is not installed or fails to start
(no headset, runtime error), every patch is already in place; the game then
runs flat on the stereo path's no-device behaviour defined in 02 (§5.3).
The player can set the runtime to None to get the unmodded game.

### 2.4 Failure behaviour

| Condition | What the mod does |
|---|---|
| Build not recognised (§3) | No patch at all, no `MOTHER.PAK`. Logs the PE timestamp. Shows the unsupported-build message `Exe check failed!` (§3.1, also listed in `13-distribution.md` §5.3), which shows the timestamp. No welcome message and no first-launch config file, so the first run on which the mod is active still shows the welcome (13 §5.3). The game runs unmodified; the forwarders keep working |
| Recognised build, but an address for it lies outside the image, or any preflight check (§2.5) fails | No patch at all, no `MOTHER.PAK`. Logs each failing check naming the site, and that nothing was patched. No message box (project decision: a recognised build that fails the preflight has been changed by something other than the mod; the game still plays unmodified, and the log names the site for anyone diagnosing it). No welcome message and no first-launch config file, as in the row above. The game runs unmodified |
| A page cannot be made writable or a write fails after the preflight passed | Logs the address, the Windows error, and that the game is left partly patched, and applies no further patch. Earlier patches are not undone. This is not expected to happen on any supported system; it is the only path that leaves a partly patched game |
| The game folder cannot be written | The game still starts. Each missing artefact degrades on its own: no `MOTHER.PAK` means no options page (row below), no local `openvr_api.dll` means the runtime's own loader (§5.3), and a log that cannot be opened goes to the debugger output (`11-diagnostics.md`). The mod never needs elevation, a registry change or a folder rename (12, 13) |
| An optional fix's own check fails (§2.5.2) | That fix stays off, with one log line; everything else is applied |
| The UI archive cannot be produced (`14-ui-resources.md`) | `UiPakPathPtr` is left alone; the options page is missing; VR works. One log line, and the message box `Failed to find UI.PAK!` of `13-distribution.md` §5.3 when `DATA\UI.PAK` itself is missing |
| The config file cannot be read or parsed | Defaults (06 §3.1); not a failure of loading |

The rule: the **core set** (the stereo path and stereo object hand-off,
the stereo device redirects, input, menus, camera and UI byte patches; every
site of §2.5.1) is all-or-nothing. Each fix of 07 and 08 (and the
diagnostic tap) is checked, enabled or skipped on its own, and logged
(§2.5.2); a mismatch there never blocks the core set or another fix.

A partly patched core set from a preflight failure is never acceptable: the
preflight exists so that the only partly patched case is a failing write
API, which is outside the mod's control.

### 2.5 Preflight

Before the first write, the mod checks every core site it will write, at
the addresses of the detected build (§3). Every failing check is logged
(so one run names all mismatches), and any failure means nothing is
written. The expected game bytes for each site are in §4; which checks to
run and in what order is the implementation's choice, as long as the rule
below holds.

#### 2.5.1 Core checks (all must pass, else nothing is patched)

The core set is every site of §4 that 02 to 06 write: the UI set of §2.3
(`UiPakPathPtr`, `ShowBootupPush`, `UiExternalCallSlot`,
`FrontEndMenuVtable` slot 0, `PauseMenuVtable` slot 0), the eight Rift
tests, the stereo object and stereo device sites, the mode stack calls and inline sites, the UI quad
calls, the camera, HUD and menu sites, the input sites and the pad vtable
slots. Each must satisfy:

- **Bounds.** Every address the mod reads or writes for the build lies
  inside `SizeOfImage` of the loaded main module; an address outside it is
  itself a failed check, and the other checks never read outside the image.
- **Instructions.** Each written code site holds its §4 instruction: the
  opcode and the operand bytes the patch depends on. Where a patch reuses
  an operand of the original instruction (for example the branch
  displacement at `CameraCorrectionBranch`), that operand must match
  exactly. Where a patch writes a value derived from an original operand
  byte (the displacement low bytes at `UiMouseWidthLoad` and
  `UiMouseHeightLoad`, 05 §7), that byte must be the original (§4.4 for
  2015; on 2020 the written value minus 0x10, §3.2 and 05 §7.7); a
  different byte is a failed check, because the reference size may then
  not lie 0x10 bytes past the back-buffer size. Displacements and register
  choices that differ between builds and that the patch does not depend on
  may be masked.
- **Data.** `UiPakPathPtr` holds a VA inside the image that points at the
  NUL-terminated string `data\ui.pak` (compared case-insensitively);
  `ShowBootupPush`'s operand points at the NUL-terminated `ShowBootup`.
- **Calls.** Each call site the mod redirects or relies on resolves to its
  listed callee. Where the callee has no entry of its own in §4 (the
  left-stick test behind `LeftStickPromptCall` and `LeftStickPromptCall2`),
  the sibling sites must resolve to the same callee.
- **Vtable slots.** Each vtable slot the mod replaces holds exactly the
  function §4 lists for it on this build (the menu and screen slots of
  §4.5, `FollowVtable` slot 0, `UiEntityOnMessage`, `UiExternalCallSlot`
  and the five pad slots of §4.4). Where §4 gives no value for the build,
  the slot holds a VA inside the image's `.text`. An exact check also
  catches a slot that points at another function of `AI.exe`, which an
  in-image check would let through. 04 §2.3, 05 §5 and 05 §6.2 follow
  this rule.

"Resolving to X" means the byte is 0xE8 and VA(site) + 5 + rel32 equals the
VA of X in the running image.

For runtime None only the UI set's sites are checked.

#### 2.5.2 Optional fixes (each checked on its own, right before it is written)

These are the fixes of 07 and 08, plus a diagnostic tap. Each is enabled only when its addresses are known for the
build (all are `?` for 2020, so all stay off there) and its own check
passes; a failure logs one line and leaves that fix off.

| Fix | Check |
|---|---|
| Planar reflections per eye (07) | `ReflectionRenderCall` resolves to `ReflectionRender` and `RendererClear` starts with `55 8b ec 83 ec 10`. Both must hold, else nothing is written for reflections and the game's mono render runs (logged), because a per-eye render without the clear skip would erase half of the target (07 §5) |
| Anti-aliasing in stereo (08 §3) | `StereoOutputDrawCall` resolves to `FullscreenDraw`, `SmaaCall` resolves to `SmaaRun`, `PostFxInstance` is `b8 <imm32> c3` (`mov eax, imm32; ret`), and `AaPass` starts with `55 8b ec 83 e4 f0`. Both call sites or neither |
| Object culling (08 §2) | `CullDistanceCall` resolves to `CullDistance`, `CullPlaneTestACall` resolves to `CullPlaneTestA`, and the 10 bytes before `CullPlaneTestACall` are `8b 46 5c 85 c0 74 0f 50 56 53`. `CullPlaneTestBCall` is not patched |
| Head-in-wall delay (08 §4) | `HeadClipFadeCall` resolves to `HeadClipFade` |
| Script event tap (diagnostic, §2.7) | the first 5 bytes of `ScriptEventTap` are `8b c1 c2 04 00` (`mov eax, ecx; ret 4`); the entry is detoured |

### 2.6 Ordering constraints at attach

Only these orderings are required; everything else at attach may run in
any order the implementation likes.

| Constraint | Why |
|---|---|
| The log is open before anything that can fail | so every later step can report |
| The config file and the command line are read before the patch set is decided | the patch set depends on the runtime for this run (§2.3) |
| The build is identified and the preflight (§2.5) has passed before the first write to AI.exe | a partly patched game is not acceptable (§2.4) |
| The original value of every vtable slot the mod replaces is read before the slot is written | the replacements call or restore the game's own method |
| `DATA\MOTHER.PAK` exists and `UiPakPathPtr` points at a NUL-terminated relative path to it, in the same form as the stock `data\ui.pak` (`14-ui-resources.md` §5.1), before attach returns | the game opens its UI archive through that pointer during start-up (§2.1 step 4). The string must live in the mod's static storage for the whole process |
| The skip-intro operand (`ShowBootupPush`, pointed at a NUL-terminated name, in static storage, of the skip-intro function that the mod's UI movie implements; the name is the implementation's choice, 06 §6, 14), when it is written at all (§2.3: only with the mod's archive), is written before attach returns | the front end runs the intro during start-up |
| The stereo object hand-off (`StereoObjectAlloc`) is written before attach returns | the game allocates its stereo object from a static constructor, before `WinMain` (§2.1 step 2) |
| All other patches are written before attach returns | no game thread exists yet, so no patch can race with game code |
| No message box is shown while the loader lock is held | showing UI under the loader lock can deadlock (§2.2) |
| The first-launch config file is written only after the welcome message has been dismissed | a process that exits first (a Steam restart, §2.1) leaves the first-launch state for the next process, so the player still sees the message once |

The optional fixes of §2.5.2 are each checked and written independently,
after the core preflight has passed. A failure at any point follows §2.4.

The log records, at attach: the game directory, the effective settings
that decide this run's behaviour (at least the runtime), the build found,
the image base and which build's addresses are used, whether the UI archive
was redirected, whether the intro is skipped, and which patches were
applied, enough to tell for each feature area of 02 to 08 whether it is
active, or the reason it is not. Wording and format are defined in
`11-diagnostics.md`.

### 2.7 Threads and later events

Threads, as requirements [sure for 2015]:

| Thread | What runs there |
|---|---|
| Loader thread (the main thread, before `WinMain`) | attach (§2.6) |
| Wherever the implementation shows the attach-time boxes (never the loader thread while it holds the lock, §2.2) | the attach-time boxes, one after another, then the first-launch config write |
| Game thread (the main thread after `WinMain`) | `CreateDevice(1)`, so runtime probing and start (§5); `D3D11CreateDeviceAndSwapChain`; UI callbacks and menu wrappers (config writes after a menu command); head pose reads |
| Render thread | the device's begin and end frame and the runtime's frame calls (03, 09) |
| Job threads | the culling hooks of 08 §2 |

Data crossing threads must use atomics or a sequence lock; the details are
in the documents that own each piece of data.

Later events:

- **Display reset.** The game does not call `CreateDevice` again: its
  only call site is `StereoInitCall`, in the display initialisation that
  runs once at start-up (02 §6.1) [sure]. The runtime is probed and started
  once per process; should a second call happen anyway, it only re-attaches
  the device that the first call created (or none, if the first failed),
  shows no message box, and logs that it was a repeated call (03 §6.5).
- **Runtime changed in the menu** (06 §4.2 `mvr_vrruntime`): the new value
  is saved and `QuitRequested` is set to 1 so the game exits; the new
  runtime and its patch set apply from the next launch. Nothing is
  unpatched or repatched in the running process.
- **Headset taken off, runtime exit:** 03 and 09. **Desktop focus loss**
  of the game window while a headset is attached: 03 §9.6.
- **Script event tap.** The game's scripts pass debug strings to a retail
  no-op method (`ScriptEventTap`, §4.6). The mod detours its entry
  (§2.5.2) for diagnostics only; what it derives and logs is defined in
  `11-diagnostics.md`. v1 behaviour does not depend on it. The tap's entry
  is also reached, through five `.rdata` pointer-table slots, with
  arguments that are not strings (§4.6, 11 §5.1). The tap must never fault
  or misread on such a call: it treats its argument as an event name only
  when it can be one. Every game event name is a string literal in the
  game's image (11 §5.2).

## 3. Build identification

### 3.1 Known builds

The proxy acts only in the game's process: the process whose main module
(the executable) has the file name `AI.exe`, compared case-insensitively.
In any other host (a test harness that loads the DLL, a tool started from
the game folder) the proxy only forwards the `d3d11.dll` exports (§1.3) and
logs one line naming the host; it reads no config, shows no message,
installs no filter, patches nothing and writes no other file (project
decision: such a host is not the game, and a message box there would block
an automated load test). A renamed game executable is not supported.

In the game's process the build is identified by
`IMAGE_FILE_HEADER.TimeDateStamp` of the main module, read from the
already-mapped headers (no file I/O).

| Build | PE timestamp | Date (UTC) | Status |
|---|---|---|---|
| 2015 launch build (Steam) | 0x54E3199D | 2015-02-17 10:36:13 | Primary target; every 2015 address below verified against the binary |
| 2020 update (Steam, GOG, Epic) | 0x5FD25676 | 2020-12-10 17:10:14 | Recognised; core addresses known (2020 columns), none verified against a 2020 binary; see §3.2 |

Any other timestamp (or a main module without valid DOS and NT headers,
read as timestamp 0) is unknown (§2.4). The player is then shown, on every
launch with that build, a standard Windows message box with caption
`MotherVR`, the warning icon and one OK button, in front of the game window
and never under the loader lock (§2.2). Its text is the one players of the
earlier mod saw for an unknown build, kept because the behaviour is the
same (the mod stays off and the game starts normally). It is quoted as in
03 §6.4: one box line per quoted line, a line ending in `\` continues on
the next box line without a blank line between, an empty quoted line is a
blank line in the box, and the code span marks the name `MotherVR`, which
the box shows as plain text:

> Exe check failed!\
> [0x`<timestamp>`]
>
> This check is to ensure that `MotherVR` is correctly modifying the game, and is looking in the correct locations.
>
> If Alien: Isolation is on an out-dated version, make sure to update through Steam or Epic. If a new update to the game has just been released, or this check failed for some other reason, `MotherVR` may need to be updated.
>
> `MotherVR` will now be disabled, and Alien: Isolation will startup normally.

`<timestamp>` is the PE timestamp found, in upper-case hexadecimal without
leading zeros (`0x0` for a main module without valid headers). The caption
has no version number, so it stays the same across releases (project
decision); the box has only OK because closing it is the only action.

The 2015 build: image base 0x400000, `SizeOfImage` 0x1F81000,
`DllCharacteristics` 0x8140, sections `.text` 0x1000, `.rdata` 0xD9F000,
`.data` 0x1236000, `.rsrc` 0x1D54000, `.reloc` 0x1D66000.

### 3.2 2020 build

The 2020 columns (including the eight Rift tests, `LeftStickPromptCall2`
and the displacement bytes below) were taken from the earlier mod's
per-build address table (and, for inline sites, its installer) and have not
been verified against a 2020 `AI.exe`; treat them as unverified until §7 Q1
is resolved. On that build the mod uses the 2020 addresses and the same
preflight rule (§2.5). If the preflight passes, the core patches are
applied; every optional fix of §2.5.2 stays off (its addresses are `?`).

Values that differ in content, not only in address:

- `UiMouseWidthLoad` / `UiMouseHeightLoad`: the displacement low byte that
  selects the 1280x720 reference fields is 0x9C / 0xA0 in 2020 (0xEC / 0xF0
  in 2015), taken from the earlier mod's installer and not verified against
  a 2020 `AI.exe`.

Structure offsets that the other documents give for game objects (for
example the input manager, its action maps, binding records and the pad
device in 04 §2) were read from the 2015 build. On 2020 they are assumed
equal and are unverified; a feature that depends on them is enabled on 2020
only when the plausibility check its document gives passes (04 owns the
check for the input structures), and stays off otherwise.

### 3.3 Image base

`AI.exe` has `IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE` set (DllCharacteristics
0x8140), so the mod takes the base from the loaded main module and adds the
RVA. Absolute addresses embedded in game instructions (for example the
`disp32` operands in `UiMouseWidthLoad`) are relocated by the loader; since
the relocation delta is a multiple of 64 KiB, their low byte is the same as
in the file.

## 4. Address table

Columns: **Name** is the stable name the spec uses. **2015** and **2020**
are RVAs (`?` = unknown). **What is there** describes the game code or data
at that address, with the original 2015 bytes for patch sites (re-read from
the 2015 binary for this revision). **Spec** names the documents that use
it.

For call sites, "call → X" means a 5-byte `call rel32` (opcode E8) whose
target is RVA X in the 2015 build.

The names are spec-side labels for game functions, data and sites; the
implementation may rename them. The table lists every game address v1
uses; no other address is needed.

### 4.1 Rift-mode tests

The game's DK2 path is gated by graphics setting 0x18 "Stereo Mode" being
"Rift". At each site below the game reads setting 0x18 (`GetSetting` RVA
0x59DF70 on the settings object, then `CurrentValue` RVA 0x5A4D00), calls
RVA 0x5BE350 (returns the value id of the string "Rift", RVA 0xFE5010) and
compares the two with a 2-byte `cmp reg, eax`. The listed address is that
`cmp`; the following flag use is the "is Rift" decision.

| Name | 2015 | 2020 | Original bytes | What the game decides | Spec |
|---|---|---|---|---|---|
| `RiftTest0` | 0x0336B6 | 0x042F96 | `3b f8` | On a Stereo Mode change: enables each camera's "VRCameraBehavior" (byte +0x80 of the behaviour, found by name through RVA 0x0293B0) | 02 |
| `RiftTest1` | 0x60FD58 | 0x562238 | `3b d8` | Back-buffer size query (RVA 0x60FC40): if Rift, ask the stereo object for the render-target size (`StereoRenderTargetSizeCall`) | 02, 03 |
| `RiftTest2` | 0x60FE50 | 0x562330 | `3b f0` | Display size query (RVA 0x60FD90): if Rift, ask the stereo object for the display size (`StereoDisplaySizeCall`) | 02, 03 |
| `RiftTest3` | 0x60FEBD | 0x56239D | `3b f0` | Present sync interval (RVA 0x60FE70, likely): 1 with "Vertical Sync", 2 under a start-up flag; Rift forces 1 | 02 |
| `RiftTest4` | 0x60FF15 | 0x5623F5 | `3b f0` | Swap-chain buffer count (RVA 0x60FEE0, likely): 3 when the sync interval is above 0, else 2; Rift forces 2 | 02 |
| `RiftTest5` | 0x611740 | 0x563C10 | `3b f0` | Back-buffer pixel format code (RVA 0x6116C0, likely): 9, or 15 with "Deep Colour (30bpp)"; Rift forces 9 | 02 |
| `RiftTest6` | 0x61378A | 0x565C5A | `3b f0` | Output selection (RVA 0x613760, likely): if Rift and the stereo object exists, get the HMD's display device name (RVA 0x5B87B0, `MAX_PATH` buffer) and look for that output (RVA 0x60FA20), else fall back | 02 |
| `RiftTest7` | 0x613C80 | 0x566150 | `3b d8` | Swap-chain description in renderer initialisation: Rift forces the value 2 (same choice as `RiftTest4`) | 02 |

### 4.2 Stereo object and stereo device

| Name | 2015 | 2020 | Kind | What is there | Spec |
|---|---|---|---|---|---|
| `StereoObjectPtr` | 0x1366A2C | 0x1376BDC | data | Global pointer to the game's stereo object (0xB0 bytes). Also stored at renderer +0xE68C. Read directly by game code throughout | 02, 05 |
| `StereoObjectAlloc` | 0x5AFBA1 | 0x501CC1 | site | In the renderer constructor: `68 b0 00 00 00` (`push 0xB0`), then `89 be 98 f6 0c 00` and `89 be 9c f6 0c 00` (zero renderer +0xCF698 and +0xCF69C), `call` → 0x044330 (operator new), `add esp,4`, a null test, a call to the stereo constructor (RVA 0x5D9A60) and at 0x5AFBC5 `89 86 8c e6 00 00` (store into renderer +0xE68C). From 0x5AFBA1 to the store is 0x24 bytes; the `push` + two stores span 0x11. A 5-byte `mov eax, imm32` followed by a 2-byte `jmp short` with displacement 0x1D lands exactly on the store | 02 |
| `StereoObjectVtable` | 0xFE57BC | ? | rdata | Game stereo object vtable, 3 slots: destructor (RVA 0x5E3F40), OnSettingChanged(SettingEvent*) (RVA 0x5CCDB0, thiscall, ret 4), IsSettingAllowed (RVA 0x5CCEB0) | 02 |
| `StereoDeviceVtable` | 0xFE5418 | ? | rdata | Game stereo device (HMD) vtable, 21 slots; slot 11 (+0x2C) = RVA 0x5E0DA0 head pose into a 3x4 matrix; slots 12 to 15 below; slot 16 (+0x40) render-target size; slot 17 (+0x44) display size; slot 18 (+0x48) display device name | 02 |
| `StereoInitCall` | 0x613941 | 0x565E11 | site | In renderer initialisation: `mov ecx,[StereoObjectPtr]; push 1; call` → 0x5E0D40 (`e8 fa d3 fc ff`), `CreateDevice(int type)` (02 §6.1): thiscall, ret 4; type 1 creates the LibOVR device. The only call of 0x5E0D40 in the image, and no pointer to it exists [sure] | 02, 03, 09 |
| `DeviceEyeTransform` | 0x5CD4C0 | 0x51F630 | function | Device slot 12: (int eye, float4x4* view, float4x4* proj), thiscall, ret 0xC. Builds the eye projection and offsets the view; eye -1 = centre | 02 |
| `DeviceFovAspect` | 0x5B8E70 | 0x50B010 | function | Device slot 13: (int eye, float* vfov, float* aspect), thiscall, ret 0xC | 02 |
| `DeviceEyeScaleOffset` | 0x5B8F10 | 0x50B0C0 | function | Device slot 14: (int eye, float out[4]), thiscall, ret 8 | 02 |
| `DeviceEyeViewport` | 0x5B9000 | 0x50B1B0 | function | Device slot 15: (int eye, int, int, int* left, int* top, int* right, int* bottom), thiscall, ret 0x1C; reads the device's eye rectangles, ignoring the target size it is given (07) | 02, 07 |
| `RecenterCallUpdate` | 0x5BE2E8 | 0x5104A8 | site | In `StereoUpdate`, when pad LB and RB are both held: `8b 40 2c` (`mov eax,[eax+0x2C]`, device slot 11), `8d 56 78` (`lea edx,[esi+0x78]`), `52` (`push edx`), `ff d0` (`call eax`); the slot offset byte is at +2 | 04 |
| `RecenterCallMenu` | 0x5B8691 | 0x50A831 | site | In the stereo object's reset-orientation method (RVA 0x5B8680, called at RVA 0x091BD7, from the menu's reset-view item (likely)): `8b 42 2c` (`mov eax,[edx+0x2C]`), `ff d0` (`call eax`), `c3`; offset byte at +2 | 04 |

### 4.3 Frames and render targets

| Name | 2015 | 2020 | Kind | What is there | Spec |
|---|---|---|---|---|---|
| `RendererPtr` | 0x17DF5CC | 0x17EF77C | data | Global pointer to the renderer | 03, 07 |
| `RendererDevice` | 0x621E20 | 0x574310 | function | cdecl (renderer) → `ID3D11Device*` (renderer +0x4) | 03 |
| `RendererSwapChain` | 0x621E30 | 0x574320 | function | cdecl (renderer) → `IDXGISwapChain*` (renderer +0x8) | 03 |
| `RendererContext` | 0x621E40 | 0x574330 | function | cdecl (renderer) → immediate `ID3D11DeviceContext*` (renderer +0x1EF3E8) | 03 |
| `RendererRtTexture` | 0x6232D0 | 0x575800 | function | cdecl (renderer, uint16 index) → `ID3D11Texture2D*` of render target `index` (renderer +0x1CD240 + 16·index) | 03 |
| `RendererRtSrv` | 0x6232F0 | 0x5757C0 | function | cdecl (renderer, uint16 index) → `ID3D11ShaderResourceView*` of render target `index` (renderer +0x1CD248 + 16·index) | 03 |
| `StereoRenderTargetSizeCall` | 0x60FD64 | 0x562244 | site | In the back-buffer size query, after `RiftTest1`: `mov ecx,[StereoObjectPtr]; push edi; push esi; call` → 0x5B8750 (`e8 e7 89 fa ff`), the stereo object's render-target size (int* w, int* h), thiscall, ret 8: sets 1280x800 then asks device slot 16 | 03 |
| `StereoDisplaySizeCall` | 0x60FE5D | 0x56233D | site | In the display size query, after `RiftTest2`: `call` → 0x5B8780 (`e8 1e 89 fa ff`), the stereo object's display size (int* w, int* h), thiscall, ret 8: sets 1280x800 then asks device slot 17 | 03 |
| `DisplayObject` | 0x13583E8 | ? | data | The game's display object: back-buffer and display state; its byte +0x88 is `RenderTargetResizePending` (02 §9.3) | 02, 03 |
| `RenderTargetResizePending` | 0x1358470 | 0x1368620 | data | Byte at `DisplayObject` +0x88: "rebuild render targets". Set by the resolution setter (RVA 0x611250) when the window size changes, by the restore path after a minimised window is restored (RVA 0x611CB0), by the display object's setting handler (RVA 0x6104C0) for display settings 2 to 6, and by the mod when a setting the stereo targets depend on changes (06 §4). After the end frame the main loop tests it (at RVA 0x3903B2) and, when the dword at RVA 0x125127C is 0, runs `DisplayReset` | 02, 03, 06 |
| `DisplayReset` | 0x613F90 | ? | function | The game's display reset (render thread): shut-down-rendering wrapper, back-buffer size query, pixel format and output selection, swapchain rebuild, configure-rendering wrapper, then clears `RenderTargetResizePending` (order in 02 §9.3) | 02, 03 |
| `StereoConfigureRendering` | 0x5D9B30 | ? | function | Stereo object wrapper of device slot 3 (configure rendering): thiscall (stereo object), ret 0; requires a device and state != 0. Called from RVA 0x613EF4 (start-up) and 0x614129 (display reset) | 02 |
| `StereoShutdownRendering` | 0x5D9B60 | ? | function | Wrapper of device slot 4 (shut down rendering): thiscall (stereo object), ret 0; requires a device and state != 0; called from RVA 0x613FBA (display reset) | 02 |
| `StereoBeginFrame` | 0x5B8870 | ? | function | Wrapper of device slot 5 then slot 7 for eyes 0 and 1; called at RVA 0x3902A9 | 02 |
| `StereoEndFrame` | 0x5B88B0 | ? | function | Wrapper of device slot 8 for eyes 0 and 1, then tail-jumps to slot 6 (end frame); called at RVA 0x3903AD | 02 |
| `StereoDevicePresents` | 0x5B8DD0 | ? | function | Wrapper of device slot 10 ("device presents"): false without a running device; called in `PresentStep` | 02 |
| `PresentStep` | 0x610240 | ? | function | The per-frame present step: thiscall (display object), between begin and end frame (called at RVA 0x39038E): overlay draw, then the game's own present unless the device presents (02 §6.8) | 02 |
| `GamePresent` | 0x623F30 | ? | function | The game's own present: `IDXGISwapChain::Present(renderer +0x0C sync interval, 0)` | 02 |

### 4.4 Input

| Name | 2015 | 2020 | Kind | What is there | Spec |
|---|---|---|---|---|---|
| `InputManagerPtr` | 0x1359B44 | 0x1369CF4 | data | Global pointer to the input manager: +0x48 / +0x2998 / +0x3A0C action maps, +0x4A70 mouse, +0x4A74 keyboard, +0x4A78 pad, +0x4A7C active pad | 04 |
| `InputGetMouse` | 0x576CD0 | 0x4C8A80 | function | thiscall (manager) → mouse device (+0x4A70), ret 0 | 04 |
| `InputGetKeyboard` | 0x576CE0 | 0x4C8A90 | function | thiscall (manager) → keyboard device (+0x4A74), ret 0 | 04 |
| `InputGetPad` | 0x576CF0 | 0x4C8AA0 | function | thiscall (manager) → active pad device (+0x4A7C), ret 0 | 04 |
| `PadVtableSlot1` | 0xFE7674 | 0xF22B34 | rdata | Address of slot 1 of the XInput pad device vtable (vtable starts 4 bytes earlier, RVA 0xFE7670). Slots 1, 2, 3, 5 and 11 are listed next | 04 |
| `PadIsDown` | 0x576730 | 0x4C84D0 | function | Pad slot 1: bool IsDown(int code), thiscall, ret 4 | 04 |
| `PadGetValue` | 0x576780 | 0x4C8520 | function | Pad slot 2: float GetValue(int code), thiscall, ret 4, result in ST0 | 04 |
| `PadGetAxis` | 0x576820 | 0x4C85C0 | function | Pad slot 3: float GetAxis(int code, bool negate), thiscall, ret 8, ST0 (forwards to RVA 0x576630 with `this`+0x18) | 04 |
| `PadControlToName` | 0x576840 | 0x4C85E0 | function | Pad slot 5: const char* ControlToName(int code), thiscall, ret 4 (glyph id for prompts). It passes the code to RVA 0x5764E0, which reads only its low 16 bits (`movzx`) and returns null for any value above 0x17 (04 §2.3) | 04 |
| `PadHasActivity` | 0x576880 | 0x4C8620 | function | Pad slot 11: bool HasActivity(), thiscall, ret 0 | 04 |
| `PadConnectedSet` | 0x60EE6F | 0x56133F | site | In the pad poll (RVA 0x60EE30) after `XInputGetState`: `0f 95 c1` (`setne cl`, result != `ERROR_DEVICE_NOT_CONNECTED`), then `88 8e 89 02 00 00` (store to pad +0x289 "connected") | 04 |
| `KeyboardKeyDown` | 0x60EB20 | 0x560FE0 | function | Keyboard: bool KeyDown(int scancode), thiscall, ret 4 (DirectInput scan codes: 0x1D Ctrl, 0x38 Alt) | 04 |
| `KeyboardAnyKey` | 0x60EE00 | 0x5612D0 | function | Keyboard: bool AnyKey() (byte +0x224), thiscall, ret 0 | 04 |
| `MouseActive` | 0x9BBE70 | 0x4C87E0 | function | Mouse: bool Active() (+0x54 != 0), thiscall, ret 0 | 04 |
| `LeftStickPromptCall` | 0x42D0A0 | 0x43E1F0 | site | In the control-prompt name lookup (RVA 0x42CFF0): `e8 3b 0c 15 00`, `call` → 0x57DCE0, "is movement on the left stick" (thiscall on an action map, ret 0, returns AL), then `test al,al` choosing `"lstick"` / `"rstick"` | 04 |
| `LeftStickPromptCall2` | 0x42D18C | 0x43E2DC | site | The second identical call, at `LeftStickPromptCall` + 0xEC: `e8 4f 0b 15 00`, `call` → 0x57DCE0 | 04 |
| `UiMouseWidthLoad` | 0x443564 | 0x454704 | site | In the UI mouse update (RVA 0x443520): `f3 0f 10 15 dc 88 75 01` (`movss xmm2,[VA 0x17588DC]`), back-buffer width (display info +0x2C, struct at VA 0x17588B0). Display info +0x3C holds the UI reference width 1280.0. disp32 starts at +4 | 04 |
| `UiMouseHeightLoad` | 0x4435C2 | 0x454762 | site | Same for height: `f3 0f 10 15 e0 88 75 01` (display info +0x30); +0x40 holds 720.0 | 04 |
| `UiInputModeBranch` | 0x43EB3A | 0x44FCFA | site | Menu input-mode event handler (RVA 0x43EB00, thiscall, ret 8), event 0x12: `74 0c` (`je`) skipping the switch to focus/pad navigation (RVA 0x434E90) when keyboard and mouse are present | 04 |

### 4.5 Camera, HUD, menus, mode stack and loading

| Name | 2015 | 2020 | Kind | What is there | Spec |
|---|---|---|---|---|---|
| `StereoPushMode` | 0x5D9C00 | 0x52BE00 | function | Stereo object: push(int stereo mode), thiscall, ret 4. Pushes on the stack at +0x68/+0x6C and applies via RVA 0x5CD0A0 | 05 |
| `StereoPopMode` | 0x5D9C80 | 0x52BE80 | function | Stereo object: pop(), thiscall, ret 0 | 05 |
| `StereoModeStackAppend` | 0x294540 | ? | function | The game's int-array append used by `StereoPushMode` (its call at 0x5D9C53): thiscall (ECX = stereo +0x68, the array `{data, count, capacity}`), one argument `const int*`, ret 4, returns the new element's address or null; grows the array (8, then doubling) through the game allocator | 05 |
| `PushCallA` | 0x40EB8E | 0x41FD4E | site | `e8 6d b0 1c 00`, `call` → 0x5D9C00 (stereo push) | 05 |
| `PushCallB` | 0x43EECA | 0x45008A | site | `e8 31 ad 19 00`, `call` → 0x5D9C00, in a hub-type screen | 05 |
| `PushCallC` | 0x44197A | 0x452B2A | site | `e8 81 82 19 00`, `call` → 0x5D9C00, in the front-end menu's open method | 05 |
| `PopCallA` | 0x413DDA | 0x42508A | site | `e8 a1 5e 1c 00`, `call` → 0x5D9C80 (stereo pop), in the front-end menu's close method | 05 |
| `PopCallB` | 0x416382 | 0x4275D2 | site | `e8 f9 38 1c 00`, `call` → 0x5D9C80 | 05 |
| `PopCallC` | 0x424C2A | 0x435DCA | site | `e8 51 50 1b 00`, `call` → 0x5D9C80 | 05 |
| `ResumeGamePop` | 0x453682 | 0x4649B2 | site | Front-end "Resume_game" handler (RVA 0x4535D0): 11 bytes, `8b 0d 2c 6a 76 01` (`mov ecx,[StereoObjectPtr]`) and `e8 f3 65 18 00` (`call` → 0x5D9C80) | 05 |
| `PauseMenuPush` | 0x43E545 | 0x44F705 | site | Pause menu "Pause" handler (RVA 0x43E520): 13 bytes, `8b 0d 2c 6a 76 01`, `6a 01` (`push 1`) and `e8 ae b6 19 00` (`call` → 0x5D9C00) | 05 |
| `StereoUpdateCall` | 0x3B4100 | 0x3C4FB0 | site | Once per frame: `e8 9b a1 20 00`, `call` → 0x5BE2A0, `StereoUpdate(float dt)`, thiscall, ret 4 (LB+RB recenter, fade animation) | 04, 05 |
| `BuildUiQuad` | 0x5BE550 | 0x510710 | function | cdecl `float4x4* (float4x4* out, float size, float inv_aspect, float dist)`, returns `out` in EAX; the caller copies from the returned pointer. Builds a quad facing the camera at `dist` | 05 |
| `VirtualScreenCall` | 0x5CD132 | 0x51F2A2 | site | In the stereo object's set-mode method (RVA 0x5CD0A0), mode 1: `e8 19 14 ff ff`, `call` → 0x5BE550 with (3.5, UI height/width, 3.0); result copied to stereo +0x20 | 03, 05 |
| `HudPanelCall` | 0x5CD2EE | 0x51F45E | site | In the stereo overlay draw (RVA 0x5CD170), mode 0: `e8 5d 12 ff ff`, `call` → 0x5BE550 with (0.8, inv aspect, 1.0) (caller pops 0x10); used for this frame's HUD quad | 05 |
| `PovManagerPtr` | 0x1366A00 | 0x1376BB0 | data | Global pointer to the POV manager: +0x8 = POV 0 (main view), whose first 64 bytes are the world-to-view matrix; +0x88 POV count | 03, 05, 07 |
| `FollowVtable` | 0xDC6600 | 0xDDD6E8 | rdata | First-person camera behaviour vtable ("FollowBehavior"): slot 0 OnActivate(arg) (RVA 0x02C930), slot 1 OnDeactivate(arg) (RVA 0x02BBF0), both thiscall, ret 4; slot 6 (RVA 0x02C9F0) and slot 8 (`FollowUpdate`) fill the followed character's position at +0x1C0 and its `Head1` bone position at +0x1CC, with the bone index at +0x248 (04 §2.7). Yaw at +0x204 (mirror +0x1FC), ±1.0 = ±180°; pitch +0x208; look input enabled byte +0x264 | 04, 05, 11 |
| `FollowOnActivate` | 0x02C930 | 0x03C1A0 | site | Function start (`53 56 8b f1 33 db b0 01`); at +8 (RVA 0x02C938) `88 86 64 02 00 00` (`mov [esi+0x264],al`, al = 1) enables game look input | 05 |
| `FollowOnDeactivate` | 0x02BBF0 | 0x03B340 | function | Clears +0x264 | 04 |
| `FollowSetYaw` | 0x02DF30 | ? | function | The behaviour's yaw setter: thiscall (float yaw, bool), ret 8. Wraps the yaw into ±1.0, limits it to the range held at +0xB4 and +0xB8, writes +0x204 and, when the bool is non-zero, the same value to +0x1FC. Not patched; listed because the snap step writes both fields as the game does (04 §2.7, §6) | 04, 05 |
| `FollowUpdate` | 0x02E970 | ? | function | `FollowVtable` slot 8, the per-update method: thiscall, the frame time as its first argument, ret 8. Copies the character position to +0x1C0 and the `Head1` bone position to +0x1CC on every update, and also rewrites +0x1FC from the character's heading. Not patched; its fields are read only by the snap-turn trace (11 §4.5) | 04, 11 |
| `FollowLookState` | 0x02D83C | 0x03D0BC | site | In the behaviour's state update (RVA 0x02D600): `8a 4c 24 0f` (`mov cl,[esp+0xF]`, previous +0x25E) then `88 8e 61 02 00 00` (store to +0x261), a byte that enables the second look-integration branch | 05 |
| `CameraNoiseCall` | 0x2603B1 | 0x270C61 | site | In the camera update (RVA 0x260320): `8b ce` (`mov ecx,esi`) and `e8 98 bb fe ff` (`call` → 0x24BF50), procedural sway and breathing offsets; 7 bytes in total | 05 |
| `HeadClipCompare` | 0x346E94 | 0x35794E | site | In the head-in-wall fade (RVA 0x346DE0, stdcall, ret 8): `0f 2f 05 a8 68 1c 01` (`comiss xmm0,[VA 0x11C68A8]`, a float 1e-5 at RVA 0xDC68A8); disp32 at +3. The result goes to stereo object +0x64 | 05 |
| `CameraCorrectionBranch` | 0x34A0CE | 0x35ACCE | site | In the VR camera correction (RVA 0x34A050, thiscall, ret 0x10): `0f 86 2b 03 00 00` (`jbe` to the epilogue at RVA 0x34A3FF). The output vector is zeroed first; the correction is computed only when this branch is not taken | 05 |
| `SsaoSettingBranch` | 0x575421 | 0x4C71D1 | site | In the SSAO setting handler (RVA 0x575400, setting 0x15): `75 10` (`jne`), choosing what to store in +0xD80 and the enable byte +0xD84 | 05 |
| `UiEntityOnMessage` | 0xDFCE18 | 0xE13F38 | rdata | Vtable entry (vtable at RVA 0xDFCE10, slot 2) holding RVA 0x51BD70 (2020 0x6241A0), the UI entity's message handler: thiscall (const ShortGuid* msg, int, int, int), ret 0x10 | 05 |
| `MsgCutsceneVisibilityStart` | 0x134DC0C | 0x17F6B84 | data | Runtime hash of `cutscene_visibility_start` (set by a static constructor) | 05 |
| `MsgCutsceneVisibilityFinish` | 0x134DC18 | 0x17F6B90 | data | Hash of `cutscene_visibility_finish` | 05 |
| `MsgLevelFadeStart` | 0x134DD1C | 0x17F6C94 | data | Hash of `level_fade_start` | 05 |
| `MsgLevelFadeFinish` | 0x134DD28 | 0x17F6CA0 | data | Hash of `level_fade_finish` | 05 |
| `FrontEndMenuVtable` | 0xF000E0 | 0xF17840 | rdata | Front-end menu vtable: slot 0 open (RVA 0x441950, 2020 0x452B00), slot 2 close (RVA 0x413DD0, 2020 0x425080). Both thiscall, no arguments, ret 0 | 05, 06 |
| `PauseMenuVtable` | 0xF00180 | 0xF178E0 | rdata | Pause menu: slot 0 open (RVA 0x441D90, 2020 0x452F40), slot 2 close (RVA 0x4155E0, 2020 0x426890), slot 3 handles "Pause" (RVA 0x43E520) | 05 |
| `LoadingScreenVtable` | 0xF00218 | 0xF17978 | rdata | Loading screen: slot 0 open (RVA 0x441EA0, 2020 0x453050), slot 2 close (RVA 0x415670, 2020 0x4268D0) | 05 |
| `AttractScreenVtable` | 0xEFCC80 | 0xF143E0 | rdata | Title/attract video screen: slot 0 open (RVA 0x40B3A0, 2020 0x41C610), plays `playAttractVideo`; slot 2 is a shared empty `ret` (RVA 0x669C90) | 05 |
| `LoaderSleepPush` | 0x43B01C | 0x44C1CC | site | In a front-end state loop (RVA 0x43AF50), state 3: `51` (`push ecx`, ecx = 33 minus elapsed ms) as the first argument of `SleepEx`; EBX is 0 here | 05 |
| `GraphicsSettings` | 0x1358A50 | 0x1368C00 | data | The graphics settings object (`this` for the two functions below; settings indexed 0x00 to 0x19, names at RVA 0x124FC20) | 02, 05 |
| `SettingsSetOption` | 0x5A8910 | 0x4FAA50 | function | thiscall (int setting index, const char* value name), ret 8: set a graphics setting by value name | 02, 05 |

### 4.6 Options, UI, configuration and process

| Name | 2015 | 2020 | Kind | What is there | Spec |
|---|---|---|---|---|---|
| `UiInvokeStr2` | 0x42B450 | 0x43C840 | function | On a menu object (the `this` of the menu vtable methods): call a named ActionScript function of its movie with two strings; thiscall (const char* name, const char*, const char*), ret 0xC | 06 |
| `UiInvokeBool` | 0x42A7D0 | 0x43B920 | function | Same with one bool; thiscall (name, bool), ret 8 | 06 |
| `UiInvokeInt` | 0x1341A0 | 0x144470 | function | Same with one int; thiscall (name, int), ret 8 | 06 |
| `UiInvokeFloat` | 0x134240 | 0x144510 | function | Same with one float; thiscall (name, float), ret 8 | 06 |
| `UiExternalCallSlot` | 0xF00078 | 0xF177D8 | rdata | Vtable entry (slot 1 of the vtable at RVA 0xF00074) holding RVA 0x43A0A0 (2020 0x44B250): the front end's Scaleform external-interface dispatch. thiscall (context, const char* name, args, arg count), ret 0x10; hashes `name` and calls the callback registered for it | 06 |
| `UiPakPathPtr` | 0x124F748 | 0x125F978 | data | Pointer (VA 0x13E3C34 in 2015) to the string `data\ui.pak`, the UI archive the game opens | 06, 14 |
| `ShowBootupPush` | 0x441AED | 0x452C9D | site | In a front-end method: `68 78 e2 2f 01` (`push` VA of the string `ShowBootup`), then `8b ce` (`mov ecx,esi`) and a `call` → 0x42AAD0, which invokes that ActionScript function (with 1 and one more argument; thiscall, ret 0xC) and starts the intro sequence | 06 |
| `QuitRequested` | 0x17DA920 | 0x17EAAD0 | data | Byte: the game's exit request flag (getter RVA 0x614760, setter RVA 0x614770; set for example right after a fatal start-up `MessageBoxA` at RVA 0x613F38). Setting it to 1 makes the game exit; the mod relies on this after a runtime change (§2.7) [likely] | 06 |
| `ScriptEventTap` | 0xA1BCC0 | ? | function | The game's script event dispatch: a retail no-op that the game's scripts pass debug strings to: `8b c1 c2 04 00` (`mov eax, ecx; ret 4`), thiscall (const char*), ret 4; 683 call sites [sure]. The five bytes are followed by eleven `cc` padding bytes; the next function starts at RVA 0xA1BCD0. The same address also fills five slots of pointer tables in `.rdata`, at RVAs 0x1076A18, 0x1076EA0, 0x1076F80, 0x1077060 and 0x1077140 [sure, counted in the 2015 binary]; calls through them pass arguments that are not strings [likely]. Diagnostic site, detoured at its entry (§2.5.2); the detour writes at most the five bytes of the body, never the padding (11 §5.2) | 01, 11 |
| `GameWindowProc` | 0x612850 | ? | function | The game window's procedure: registered with `RegisterClassExA` at RVA 0x612F90 (class style 3), ANSI, stdcall (hwnd, msg, wParam, lParam), ret 0x10. Messages 2 to 8 are dispatched through a jump table at RVA 0x612E64; `WM_SETFOCUS` sets the byte at RVA 0x1251298 to 1; `WM_SIZE` goes to `WindowSizeHandler` (call at RVA 0x612E51). Not patched: focus loss is kept from it as 03 §9.6 requires, by a means of the implementation's choosing; listed so the facts of 02 §9.4 and 03 §9.3 can be confirmed | 02, 03 |
| `WndProcActivate` | 0x61288F | ? | site | `WM_ACTIVATE` branch of `GameWindowProc`: with an input manager, passes "active" (wParam != 0) to the mouse device's method at RVA 0x611A40 and to the keyboard device's method at RVA 0x60ED90 (cursor clip and keyboard state, 02 §9.4). Not patched | 02, 03 |
| `WndProcKillFocus` | 0x612910 | ? | site | `WM_KILLFOCUS` branch: `83 3d 8c 12 65 01 02` (`cmp dword [VA 0x165128C], 2`, the display mode at `WindowDisplayState` +0x04, 2 when fullscreen), clears the byte at RVA 0x1251298 and, when the compare is equal, posts system event 4 through RVA 0x58FB90 on the object at RVA 0x1358298 (the event that opens the pause menu, 02 §9.4). Not patched | 02, 03 |
| `WindowDisplayState` | 0x1251288 | ? | data | The object holding the game window's display mode at +0x04 (2 fullscreen, 0 windowed [likely, from its two uses below]); `this` of `WindowSizeHandler`. Not the `DisplayObject` of §4.3. Read only (03 §9.3, §9.6) | 03 |
| `WindowSizeHandler` | 0x611B90 | ? | function | thiscall on `WindowDisplayState` (msg, wParam, lParam), reached from `GameWindowProc` for `WM_SIZE`: records the new client size and resizes only while +0x04 is 0; in any other mode it returns without using the new size (03 §9.3). Not patched | 03 |
| `CrtFilterInstall` | 0x6826A6 | ? | function | A C runtime initialiser (the entry at RVA 0xDBB140 of the initialiser table RVA 0xDBB124 to 0xDBB144, run before the static constructors, §2.1 step 2): `68 64 26 a8 00` (`push` VA of `CrtUnhandledFilter`), `ff 15 88 f2 19 01` (`call` `SetUnhandledExceptionFilter` through its import slot at RVA 0xD9F288). Replaces any filter installed at attach. Not patched | 01, 11 |
| `CrtUnhandledFilter` | 0x682664 | ? | function | The C runtime's top-level filter, stdcall, ret 4: for an MSVC C++ exception (code 0xE06D7363 with one of its four magic numbers) it calls the C runtime's terminate (RVA 0x67837E); for any other exception it returns 0 (`EXCEPTION_CONTINUE_SEARCH`). It never calls the filter it replaced | 01, 11 |
| `CrtFailFast` | 0x6770BD, 0x684C81 | ? | site | The other two calls of `SetUnhandledExceptionFilter`, in the C runtime's failure reports [likely: invalid parameter and buffer security check]: each calls it with null and then calls `UnhandledExceptionFilter` directly, so a termination through them reaches no installed filter and leaves no crash record. Not patched | 11 |
| `AudioReopenOutput` | 0x831B00 | 0x8473D0 | function | Re-initialises the game's audio output device: cdecl, no arguments; starts `c6 05 61 97 bf 01 01` (sets a byte flag) then calls the device open. Called once, after the mod redirects audio to the headset's endpoint (10) | 10 |

### 4.7 Planar reflections and render fixes

2015 only; the 2020 addresses are not known yet, so the fixes of 07 and 08
§2 to §4 stay off on that build (08 §1 needs no game address). Bytes are
the 2015 originals, checked against the binary. "call → X" sites are 5-byte
`call rel32` (E8) whose target is RVA X; a redirect must check the opcode
and that the displacement resolves to X (§2.5.2).

Reflections (07):

| Name | 2015 | 2020 | Kind | What is there | Spec |
|---|---|---|---|---|---|
| `ReflectionManagerPtr` | 0x135835C | ? | data | Global pointer to the reflection manager: +0x04 target, +0x56 usable, +0x58/+0x5C target size, +0x60 quality, +0x6C/+0x70 candidate planes and count, +0x78 flag | 07 |
| `ReflectionPovUpdate` | 0x5B41E0 | ? | function | Reflection POV per-frame update (thiscall, POV in ECX); called at 0x5B4640 (`e8 9b fb ff ff`) from the POV manager update 0x5B45A0 for every type-1 POV | 07 |
| `ReflectionRenderCall` | 0x5B5D91 | ? | site | `e8 aa fe ff ff`: call → 0x5B5C40, in the POV manager's render loop (RVA 0x5B5D40), POV type 1; the only call of it | 07 |
| `ReflectionRender` | 0x5B5C40 | ? | function | Renders the reflection POV, mono; thiscall, no arguments, ret 0; prologue `55 8b ec 83 e4 f0 83 ec 2c` | 07 |
| `PovSetEye` | 0x5B38D0 | ? | function | POV set eye (int eye), thiscall, ret 4. Eye -1 saves the centre view and projection to `PovEyeSave`. Must not be called on the reflection POV | 07 |
| `PovEyeSave` | 0x142F5E0 | ? | data | Shared save area of `PovSetEye`: view float[16] at 0x142F5E0, projection float[16] at 0x142F620 | 07 |
| `PovBeginRender` | 0x5B11D0 | ? | function | POV begin render: uploads view, projection and camera constants, sets target and viewport | 07 |
| `PovUpdateFrustum` | 0x5B2FB0 | ? | function | Recomputes a POV's culling frustum from its view and projection; thiscall, no stack arguments | 07 |
| `PovViewport` | 0x5B0820 | ? | function | POV viewport: thiscall, one pointer to `{left, top, right, bottom}`, ret 4; whole target for eye -1, else via `StereoEyeViewport` | 07 |
| `StereoEyeViewport` | 0x5B8730 | ? | function | Stereo object wrapper that calls device slot 15 with `(eye, width / 2, height, &l, &t, &r, &b)` when the device state is 2 | 07 |
| `StereoApplyEyeTransform` | 0x5B86A0 | ? | function | Stereo object: (int eye, float view[16], float proj[16]), thiscall, ret 0xC; forwards to device slot 12 when the device state is 2 | 07 |
| `RendererEyeOwner` | 0x13583E4 | ? | data | Global pointer; the int at +0x2C is the renderer's current eye | 07 |
| `RendererClear` | 0x621770 | ? | function | Renderer clear, cdecl (renderer, flags, ARGB colour, float depth, stencil); flags 1 colour, 2 depth, 4 stencil. First six bytes `55 8b ec 83 ec 10`, position independent | 07 |
| `PassSetup` | 0x595170 | ? | function | Render pass setup; for some pass types rebuilds the viewport through 0x574EE0 as `{0, 0, right - left, bottom - top}` (07 §8 item 1) | 07 |
| `RendererSetViewport` | 0x621350 | ? | function | cdecl (context, int vp[6] = {x, y, w, h, minZ, maxZ}) | 07 |
| `ScreenConstantsUpload` | 0x574F50 | ? | function | Uploads pixel constant buffer 2 register 60 (`ScreenResolution`) and vertex constant buffer 1 register 21 (viewport fraction of the target) | 07 |

Culling, anti-aliasing and head-in-wall fade (08):

| Name | 2015 | 2020 | Kind | What is there | Spec |
|---|---|---|---|---|---|
| `CullObjectsCall` | 0x5E984B | ? | site | `e8 d0 64 03 00`: call → 0x61FD20, the only call of the per-object visibility loop; cdecl, the caller pops 0x18 | 08 |
| `CullObjects` | 0x61FD20 | ? | function | Per-object visibility loop: cdecl (cull context*, flags*, bounds[] stride 0x40, items[], count, out[]); serves the main view and shadow casters | 08 |
| `CullDistanceCall` | 0x61FD95 | ? | site | `e8 b6 9c ff ff`: call → 0x619A50, followed by `83 c4 0c` | 08 |
| `CullDistance` | 0x619A50 | ? | function | Distance/LOD test: cdecl (bounds*, const vec3* camera position, float scale), result in AL (true = keep) | 08 |
| `CullFrustum` | 0x619030 | ? | function | Frustum test: (bounds*, context*) | 08 |
| `CullPlaneTestACall` | 0x61FDB9 | ? | site | `e8 a2 9a ff ff`: call → 0x619860, the occlusion test (taken when context +0x5C is set), followed by `83 c4 0c`. Preceded by the 10 bytes `8b 46 5c 85 c0 74 0f 50 56 53` (`mov eax,[esi+0x5C]; test eax,eax; je +0xF; push eax; push esi; push ebx`) | 08 |
| `CullPlaneTestA` | 0x619860 | ? | function | The occlusion test: tests the object against the context's occlusion buffer, which only the main view has (08 §2); cdecl, 3 arguments, result in AL | 08 |
| `CullPlaneTestBCall` | 0x61FDD2 | ? | site | `e8 49 9b ff ff`: call → 0x619920, the shadow-light view-volume test (taken when context +0x60 is set, with context +0x70), followed by `83 c4 0c`. Not patched | 08 |
| `CullPlaneTestB` | 0x619920 | ? | function | The shadow lights' view-volume test (shadow casters against the main view's volume, 08 §2); cdecl, 3 arguments, result in AL | 08 |
| `AaSettingHandler` | 0x5F6AC0 | ? | function | Graphics setting 0x09 handler: thiscall, one argument, ret 4; stores the AA mode at post-processing object +0x1B3C (1 FXAA, 2 SMAA T1x, 3 SMAA T2x, else 0) | 08 |
| `PostFxInstance` | 0x5F19C0 | ? | function | Returns the post-processing object; cdecl, no arguments; `b8 70 09 9d 01 c3` (`mov eax, VA 0x19D0970; ret`) | 08 |
| `FinalPostChain` | 0x610040 | ? | function | Final post chain (to 0x6101FA): post pass, then flat AA + post pass, or the stereo output | 08 |
| `StereoRunningTest` | 0x610144 | ? | site | `b9` + VA of 0x13583A0 (`mov ecx, ...`) then a call → 0x01E150: true when the stereo device state is 2; selects the stereo branch | 08 |
| `AaPass` | 0x600210 | ? | function | Post-processing AA pass, thiscall on the post-processing object, no arguments; starts `55 8b ec 83 e4 f0 83 ec 44` | 08 |
| `SmaaCall` | 0x600479 | ? | site | In the AA pass: `e8 42 a0 01 00`: call → 0x61A4C0 (`SmaaRun`), followed by `83 c4 0c` | 08 |
| `SmaaRun` | 0x61A4C0 | ? | function | The SMAA runner: cdecl, 3 arguments | 08 |
| `PostFxPass` | 0x600490 | ? | function | Post-processing pass | 08 |
| `StereoOutput` | 0x5FC560 | ? | function | Stereo output: sets pixel constant buffer 2 register 36 from post values times stereo +0x64, then draws through 0x5A0980; first bytes `83 ec 1c 53` | 08 |
| `StereoOutputDrawCall` | 0x6101DC | ? | site | `e8 9f 07 f9 ff`: call → 0x5A0980 (`FullscreenDraw`) in the stereo branch of the final post chain; the place where the AA pass is missing | 08 |
| `FullscreenDraw` | 0x5A0980 | ? | function | The full-screen triangle draw: cdecl, no arguments | 08 |
| `VelocityPass` | 0x5B1720 | ? | function | SMAA T2x velocity pass; runs on the eye-0/centre path only | 08 |
| `MainViewEye1` | 0x5B5B70 | ? | function | Prepares and renders the main view's eye 1 (`PovSetEye(1)`, render, restore the centre); skips the velocity pass | 08 |
| `PostSourceResolution` | 0x6007B0 | ? | site | Post-processing setup (to 0x600811): size query 0x627B90, then the constant upload 0x621C90 of pixel constant buffer 2 register 39 = (W, H, 1/W, 1/H) | 08 |
| `HeadClipFadeCall` | 0x34E2C1 | ? | site | `e8 1a 8b ff ff`: call → 0x346DE0 in the VR camera behaviour update; its result is stored to stereo +0x64 | 08 |
| `HeadClipFade` | 0x346DE0 | ? | function | Head-in-wall fade: stdcall (const vec3* eye, const vec3* pivot), ret 8, visibility in ST0 (1 visible to 0 black) | 05, 08 |

### 4.8 v2 addresses

The game addresses that only the v2 features use (`20-v2-overview.md`) are
listed, 2015 only with 2020 `?`, in the address tables of the v2 documents
that use them: `21-hands-and-body.md` §20, `22-controls-and-comfort.md`
§2, `23-interactions-and-hud.md` §2 and `24-rendering-v2.md` §1.1 and
§2.3. v1 uses none of them. They are collected into this section in the
revision that applies v2's changes to the v1 documents (20 §11); each one
is then an optional fix in the sense of §2.5.2, enabled only when its own
check passes.

### 4.9 Known but not needed

These are in the game, but v1 does not change them:

- RVA 0x43AB40 (2020 0x44BCF0): a call to the control-prompt name lookup
  (RVA 0x42CFF0, cdecl, 2 arguments).
- RVA 0x5CCDB0 / 0x5CCEB0 (2020 0x51EF20 / unknown): the game's own stereo
  OnSettingChanged and IsSettingAllowed, listed in `StereoObjectVtable`.

## 5. Runtime selection

### 5.1 The runtime for this run

At attach the mod takes the configured runtime (06 §1, `Settings.VRRuntime`;
`"Oculus"` and value 1 mean OpenXR) and applies the command-line switches:

| Argument | Runtime for this run |
|---|---|
| `-vrforcenone` | None |
| `-vrforceopenvr` | OpenVR |
| `-vrforceopenxr` | OpenXR |
| `-vrforceoculus` | OpenXR |

A switch matches only as a whole argument (the command line split as
`CommandLineToArgvW` does), compared case-insensitively. If several are
given, the last one wins. A forced runtime is not saved. The switches
decide the patch set (§2.3) at attach and the runtime choice (§5.2) later;
both use the same value.

### 5.2 Probing and choice

With runtime OpenVR or OpenXR, the first `CreateDevice(1)` call (game
thread) probes for installed runtimes and logs each result. Probing has no
effect on the process beyond the log: it loads none of the runtimes' DLLs
and changes no environment variable.

- **OpenVR is available** when `%LOCALAPPDATA%\openvr\openvrpaths.vrpath`
  exists and parses as JSON, its `"runtime"` member is a non-empty array
  whose first element is a string (a UTF-8 path, the SteamVR install
  directory), and `<that path>\bin\win32\openvr_api.dll` exists. Any other
  outcome means not available (logged: file missing or unparseable, or the
  path without the DLL).
- **OpenXR is available** when `09-openxr.md` §1 finds a runtime usable
  from a 32-bit process. A non-empty `XR_RUNTIME_JSON` set by the player
  wins. Otherwise the rule prefers an x86 build of the runtime the player
  made active over a different entry in the 32-bit registry view, because
  runtimes write only the 64-bit view when made active, so a 32-bit entry
  that differs is usually left over from a runtime used earlier (project
  decision, ANSWERS r6). The chosen manifest reaches the OpenXR loader (in
  the environment and as a loader initialisation property, 09 §1, because
  an elevated process's loader ignores the environment) only when OpenXR
  is the runtime that will start, and only then is an elevated process
  logged (11 §2.3): a variable left in the environment would reach every
  process the game starts and any later OpenXR loader in the process, for
  a runtime the player did not choose.
- LibOVR (the Oculus PC SDK) is never probed or used.

The choice, in order:

1. Preferred runtime available: use it.
2. Preferred runtime not available and it was **forced** by a switch: no
   runtime (no fallback).
3. Otherwise fall back: OpenVR if available, else OpenXR if available.
4. Otherwise no runtime.

The log records the availability of each, the preferred runtime, whether
it was forced, and the choice. The probe runs once per process; the result
is reused (display resets, the OpenXR frame pacing of 09).

### 5.3 Start and failure

The chosen runtime is started from the same `CreateDevice(1)` call (03 for
OpenVR, 09 for OpenXR). OpenVR's start first writes `openvr_api.dll` next to
the proxy when it is not current (`13-distribution.md` §3.2), then loads
that copy (or, if it does not exist, the runtime's own
`bin\win32\openvr_api.dll`) with the runtime's `bin\win32` directory on the
DLL search path while `openvr_api.dll` loads and `VR_InitInternal2` runs.

| Outcome | Behaviour |
|---|---|
| No runtime chosen (§5.2 step 2 or 4) | No stereo device; logged; **no message box**. The game runs flat on the patched stereo path (02's no-device behaviour) |
| The chosen runtime fails to start (DLL cannot load, init error, missing interface, no OpenXR session possible with the game's device) | No stereo device; the runtime is shut down and unloaded; the runtime-failure message, `Failed to initialize SteamVR!` (03 §6.4) or `Failed to initialize OpenXR!` (09 §7), both also listed in `13-distribution.md` §5.3, is shown once, modal, on the thread that hit the failure (the game thread), and the game waits until it is closed; then the game runs flat |
| Started | Device attached to the stereo object; the reference pose taken, and retaken once the headset is in use if it was not in use at that moment (03 §2.2) |

A failure detected later, when the game configures rendering (OpenXR
session creation needs the game's device, 09 §2), follows the same row and
detaches the device there.

Neither a fallback nor a failure rewrites the stored runtime: `VRRuntime`
keeps the player's preference, so the next launch tries it again. The
options page's `VR Runtime Set` shows the runtime actually running: `None`
after a failed start, the substitute after a fallback. So the player sees that the
preferred runtime is not in use and can choose again (06 §5.2).

A runtime that started can still fail to reach the headset: SteamVR's
Desktop Game Theatre shows the game's desktop window instead of its
frames. That is detected after start-up and the player is warned without
blocking (03 §6.7 for OpenVR, 09 §3.1 for OpenXR); the run continues.

## 6. Acceptance

Each check names what to look at. "Log" is `mothervr.log` in the game
directory (`13-distribution.md` §4).

1. **Loads on Windows 11.** With only the proxy added to a clean 2015 Steam
   install, the game starts; the log has an attach header, the game
   directory, the config summary and the recognised build with its timestamp 0x54E3199D.
2. **Forwarders.** With OpenVR chosen and SteamVR installed, OpenVR init
   succeeds (no error 102). The log shows the system `d3d11.dll` loaded from
   the system directory, `D3D11CreateDeviceAndSwapChain` resolved, and a
   summary of 50 pass-through exports resolved and none missing.
   `dumpbin /exports` on the proxy lists exactly the 51 names of §1.3.
3. **No loader-lock work.** The log shows the system `d3d11.dll` load and
   the runtime probe only after the attach lines, and the attach-time boxes
   appear after the attach lines. With a box open, the game window does not
   start rendering until it is closed.
4. **Unknown build.** Changing one byte of the PE timestamp in a copy of
   `AI.exe` (test only): the `Exe check failed!` box of §3.1 appears, its
   second line showing the new value as `[0x...]` in upper-case hex, the log says the build is unknown and that nothing is patched, the
   game plays unmodified, and `DATA\MOTHER.PAK` is not created.
5. **Preflight.** Changing the byte at `SsaoSettingBranch` in a test copy
   from 0x75 to 0x74: the log names the SSAO branch, says nothing was
   patched, and the game plays unmodified, with no box. No other site is
   modified (compare process memory of a few sites with the file).
6. **Runtime None.** With `-vrforcenone`: the log shows only the UI set
   (§2.3) applied and no runtime probe; the Options menu has the
   `MotherVR` entry;
   graphics options behave as unmodded (SSAO can be enabled and shows); no
   replacement is installed on the game's device or swapchain, including
   the pixel shader replacement of 08 §1, and the log has no swapchain,
   shader or window lines.
7. **Forced missing runtime.** With SteamVR not installed (or
   `openvrpaths.vrpath` renamed) and `-vrforceopenvr`: the log shows OpenVR
   unavailable, chosen None, no box, and the game runs flat.
8. **Fallback.** With `VRRuntime` `"OpenXR"`, no OpenXR runtime usable from
   32-bit, and SteamVR installed, no switch: the log shows the fallback to
   OpenVR and VR starts; the options page's `VR Runtime Set` shows
   `SteamVR`, and `mothervr.json`
   still says `"OpenXR"`.
9. **Runtime failure box.** With SteamVR installed but its server unable to
   start (for example the headset unplugged, or another VR app launching):
   one `Failed to initialize SteamVR!` box (03 §6.4), its second line
   showing the runtime's error text and its code; the game continues flat
   after it is closed. The options page's `VR Runtime Set` then shows
   `None`, and `mothervr.json` still says `"OpenVR"`.
10. **Display reset.** Changing the resolution in the game's video options
    in VR: the log shows a display reset and no second `CreateDevice` call,
    no second runtime start and no box; VR continues.
11. **Runtime change.** Pressing `Use OpenXR` (or `Disable VR`) on the
    `VR Runtime` page: the game exits at once, without a prompt; `mothervr.json` holds the new value; the next launch uses it.
12. **Two processes.** Starting `AI.exe` directly (outside Steam) produces
    two attach headers with different process ids in one log, and neither
    process crashes.
13. **Optional fixes.** On the 2015 build the log shows reflections,
    anti-aliasing, culling, the head-in-wall delay and the event tap each
    reported active; on the 2020 build none of them, and the core
    patches still applied if the preflight passed.
14. **Not the game.** Loading the proxy into another 32-bit program (for
    example `rundll32` with a copy of the DLL): no message box appears, no
    `mothervr.json` or `MOTHER.PAK` is written, and the log has one line
    naming the host.
15. **Skip intro without the options archive.** With `SkipStartupIntro`
    on and `DATA\UI.PAK` renamed: the log shows the UI archive not
    redirected and the skip-intro operand not written; the intro plays and
    the game reaches the title screen.
16. **No welcome without the mod.** On the build of check 4 with no
    `mothervr.json`: no welcome appears and no `mothervr.json` is written;
    a later run on the unchanged game shows the welcome (00 §2.7) once.
17. **Crash record after start-up.** In a test build of the mod that
    raises an access violation on the game thread once the title screen is
    up: the log has the crash record (11 §6), and Windows still reports the
    crash as it does without the mod.
18. **Exact vtable checks.** Changing the stored value of `PauseMenuVtable`
    slot 0 in a test copy to another function of `AI.exe` (for example the
    loading screen's open method): the log names that slot and says nothing
    was patched.
19. **Mouse displacement byte.** Changing the displacement low byte at
    `UiMouseWidthLoad` + 4 in a test copy from 0xDC to 0xD8: the log names
    the site and says nothing was patched.

## 7. Open questions

1. **2020 build addresses.** No 2020 `AI.exe` has been checked. To resolve:
   load a 2020 binary (timestamp 0x5FD25676), run the §2.5 checks over its
   column, and confirm the bytes match the 2015 encodings; then locate the
   §4.7 and `ScriptEventTap` addresses by the same byte patterns (call
   targets and prologues listed there) and confirm the
   `UiMouseWidthLoad` displacement bytes.
2. **Resolved in r6.** The game does not call `CreateDevice` again on a
   display reset: `StereoInitCall` is its only call site, in the display
   initialisation that runs once (02 §6.1). A repeated call stays handled
   (§2.7, 03 §6.5).
3. **Is setting `QuitRequested` the game's normal exit path?** Setting it
   after a runtime change was observed in game to exit the game. To resolve: confirm that the
   game's own quit menu item writes the same byte through the setter at RVA
   0x614770 and that save data is flushed on that path.
