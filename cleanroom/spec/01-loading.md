<!-- Provenance: AI.exe 2015 analysis (PE headers, imports, code at each listed RVA); Windows 11 load tests in the game directory; SysWOW64\d3d11.dll export table; 2020 RVAs from earlier analysis, not verified against a 2020 AI.exe; AI.exe 2015 code at the 07 and 08 sites (bytes checked against the binary); user decision on flat play with runtime None (answer to Q6, ANSWERS.md). Revision: r2 -->

# 01. Loading, build identification and AI.exe addresses

This document covers how the mod gets into `AI.exe` on Windows 11, what it
may do at load time, how it tells game builds apart, and the full list of
game addresses that the other documents use.

Conventions: AI.exe is a 32-bit (x86) executable with preferred image base
0x400000. Addresses here are **RVAs** unless they say VA (VA = image base +
RVA, so VAs quoted from the 2015 build assume base 0x400000). `thiscall`
means `this` in ECX, arguments on the stack, callee pops them; `ret N` is
the number of argument bytes the callee pops.

## 1. Entry: a `d3d11.dll` proxy

### 1.1 Requirement

The mod ships as a single 32-bit `d3d11.dll` placed in the game directory,
next to `AI.exe`. The Windows loader finds it there before the system copy,
because `AI.exe` imports `d3d11.dll` statically:

| DLL | Functions AI.exe imports (by name) |
|---|---|
| `d3d11.dll` | `D3D11CreateDeviceAndSwapChain` |
| `dxgi.dll` | `CreateDXGIFactory`, `CreateDXGIFactory1` |
| `XINPUT1_3.dll` | ordinals 2, 3, 4 |

No installer, launcher or injector is used. Removing the file restores the
unmodified game.

### 1.2 Why not `dxgi.dll`

A `dxgi.dll` proxy in the game directory, which is how earlier mods for this
game were loaded, does not work on Windows 11. Tests on Windows 11 build
26200 with the 2015 Steam build: any local `dxgi.dll`, even a minimal one
with no C runtime and only five kernel32 imports, made `AI.exe` crash with
an access violation at address 0 during process start, before the DLL's
entry point ran. KnownDLLs (dxgi is not listed), Smart App Control (off), C
runtime start-up, missing exports and PE structure were ruled out. A
`d3d11.dll` built the same way loads and its entry point runs. The cause is
inside the Windows loader or graphics stack and is not documented; the mod
must not depend on a local `dxgi.dll` at all.

### 1.3 Exports

Once the proxy is loaded as `d3d11.dll`, every later import of `d3d11.dll`
in the process binds to it by name, not only AI.exe's. The VR runtime's
32-bit client library imports `D3D11CreateDevice`; with that export missing
its load fails and OpenVR initialisation reports
`VRInitError_Init_VRClientDLLNotFound` (102). The proxy must therefore
export every name the system `d3d11.dll` exports and forward each one
unchanged to the system copy.

The 32-bit `C:\Windows\SysWOW64\d3d11.dll` on current Windows 11 exports
these 51 names:

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

- Forward to the system DLL loaded by full path (the system directory as
  the 32-bit process sees it, i.e. `SysWOW64` on 64-bit Windows). Loading
  `d3d11.dll` by bare name would return the proxy itself.
- A forwarder must not change arguments, return values or the stack; the
  callee's own calling convention (`__stdcall` for all of these) applies.
- `D3D11CreateDeviceAndSwapChain` is the only one the game calls. The mod
  may wrap it (03 uses the device and swap chain it returns); all others
  are pure forwards.
- If the system DLL cannot be loaded or a name cannot be resolved, the
  forwarder returns a failure (`E_FAIL` for HRESULT functions) instead of
  crashing, and the problem is logged.
- If a future Windows adds exports, the list must be extended; a startup
  check that compares the system DLL's export names with the proxy's and
  logs any missing one is required.

## 2. Initialisation order

### 2.1 What happens when

Facts about the 2015 build, in process order:

1. The loader maps `AI.exe` and its static imports and runs their entry
   points, including the proxy's `DLL_PROCESS_ATTACH`, under the loader
   lock. `AI.exe`'s own C runtime has **not** started: none of its static
   constructors has run.
2. `AI.exe`'s C runtime runs its static constructors. One of them (at RVA
   0xD74F30) constructs the renderer (RVA 0x5AFA70), and that constructor
   allocates the game's stereo object at site `StereoObjectAlloc`
   (RVA 0x5AFBA1, see §4.2). Other static constructors compute the
   message-name hashes in §4.5 (`MsgCutsceneVisibilityStart` and the
   following three); before that the globals are zero.
3. `WinMain` starts. Renderer initialisation (function at RVA 0x6138F0)
   calls `StereoInit(1)` at site `StereoInitCall` (RVA 0x613941), later
   queries the back-buffer size (call at RVA 0x613AE8 into RVA 0x60FC40,
   which contains `StereoRenderTargetSizeCall`), decides the swap-chain
   parameters (`RiftTest7`, RVA 0x613C80) and only then creates the D3D11
   device and swap chain (call at RVA 0x613CB8 into RVA 0x626FA0, which
   calls `D3D11CreateDeviceAndSwapChain`).
4. The front end loads its Scaleform UI from the UI archive and starts the
   intro sequence (`ShowBootup`, §4.6). The front-end menu object first
   exists when its open method (`FrontEndMenuVtable` slot 0) runs.

### 2.2 Requirements at `DLL_PROCESS_ATTACH`

Allowed, and required to happen here:

- Identify the AI.exe build (§3) from the main module's PE header, which is
  already mapped.
- Read the config file and open the log (both in the game directory, the
  directory of the proxy DLL).
- Decide the patch set from the configured runtime (the config file,
  then the command-line overrides of 06 §2), and apply it (§2.3). The
  patches must be in place before step 2 above: the stereo object
  allocation runs from a static constructor, before `WinMain`. Patching
  here also means no game thread is running yet, so no patch can race with
  game code. `.text` and `.rdata` are read-only; the mod changes page
  protection for the write, restores it, and flushes the instruction cache
  for code.
- Compute absolute addresses from the actual image base (§3.3).

Not allowed at attach:

- Calling any AI.exe function or reading game globals expecting values.
  The renderer, input manager, POV manager and stereo pointers are null, the
  message-name hashes are zero, and no UI or script object exists. Anything
  that needs them is done from the game hooks the other documents define
  (for example, settings are pushed to the UI from the front-end menu's open
  method, 06).
- Starting the VR runtime. Its client library imports `d3d11.dll` (which
  is now the proxy) and creates threads; doing that under the loader lock
  can deadlock, and the game has not decided yet whether stereo is wanted.
  The runtime starts from the game's `StereoInit(1)` call (step 3), which
  is early enough: the back-buffer size query and device creation follow it.
- Loading the system `d3d11.dll`. It is loaded on the first call to any
  forwarded export (thread-safe, once), not in `DllMain`.
- Message boxes, except a single fatal one when the mod refuses to run; a
  problem found at attach is otherwise logged and reported once the game
  window exists.

### 2.3 Patch sets: runtime None means the unmodded game

Flat play must be the unmodded game. The patch set therefore depends on the
runtime chosen for this run (06 §1, §2), which is known at attach:

| Runtime for this run | Applied at attach |
|---|---|
| OpenVR or OpenXR | **every** code and data patch that 02 to 08 require |
| None (the default, or `-vrforcenone`) | only the **UI set** below; no other patch |

The UI set is what the player needs to turn VR on from the menu and nothing
that changes how the game plays or renders:

- the UI archive redirect (`UiPakPathPtr`, 06 §7.3), when the patched
  archive is available;
- the UI callback slot (`UiExternalCallSlot`, 06 §4.1), so the options page's
  commands reach the mod;
- the front-end and pause menu Open wrappers (`FrontEndMenuVtable`,
  `PauseMenuVtable` slot 0), doing **only** the settings push of 06 §4.4
  (no stereo-mode handling of 05);
- the skip-intro operand (`ShowBootupPush`, 06 §6), when skip intro is on.

With runtime None the mod creates no stereo device (`StereoInitCall` is not
redirected, so the game's own `StereoInit(1)` runs and finds no LibOVR
runtime, as in the unmodded game), does not patch the Rift tests, the pad,
the camera, SSAO, the mode stack, the UI quads or the render paths of 07
and 08, does not wrap the D3D11 device or swapchain beyond the pure
forwarders of §1.3, and does not touch the game's pixel shaders (08 §1).
The game plays exactly as without the mod; only the Options menu has the
VR page.

When the runtime is OpenVR or OpenXR but fails to start (no headset, runtime
not installed), every patch is already in place; the game then runs flat on
the stereo path's no-device behaviour defined in 02, and the reason is
shown once (00 §2.7). The player can set the runtime to None to get the
unmodded game.

### 2.4 Failure behaviour

If the build is not recognised (§3), or a patch site does not contain the
bytes this document lists for it, the mod applies **no** patch at all, logs
the PE timestamp and the first mismatching site, and lets the game run
unmodified (forwarders keep working). It tells the user once, in a message
that names the found timestamp. A partly patched game is never acceptable.

## 3. Build identification

### 3.1 Known builds

The build is identified by `IMAGE_FILE_HEADER.TimeDateStamp` of the main
module (`AI.exe`).

| Build | PE timestamp | Date (UTC) | Status |
|---|---|---|---|
| 2015 launch build (Steam) | 0x54E3199D | 2015-02-17 10:36:13 | Primary target; every address below verified |
| 2020 update (Steam, GOG, Epic) | 0x5FD25676 | 2020-12-10 17:10:14 | Most addresses known (RVA 2020 column), none verified; see §3.2 |

The 2015 build: image base 0x400000, `SizeOfImage` 0x1F81000, sections
`.text` 0x1000, `.rdata` 0xD9F000, `.data` 0x1236000, `.rsrc` 0x1D54000,
`.reloc` 0x1D66000.

### 3.2 2020 build

The RVA 2020 column comes from earlier analysis and has not been checked
against a 2020 `AI.exe`. Before the mod enables itself on that build, each
2020 site must be checked against the bytes given for 2015 (instruction
encodings are expected to match even where displacements differ). An
address with a blank 2020 column is unknown; the feature that needs it
stays off on that build and the log says so.

Two 2020 values are known to differ in content, not only in address:

- `UiMouseWidthLoad` / `UiMouseHeightLoad`: the displacement low byte that
  selects the 1280x720 reference fields is 0x9C / 0xA0 in 2020 (0xEC / 0xF0
  in 2015), assuming the same +0x10 field distance.
- Address 0x00D1F020 was recorded for the 2020 `StereoIsSettingAllowed`;
  it is probably a typo for 0x0051F020. It is listed for reference only.

### 3.3 Image base

`AI.exe` has `IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE` set (DllCharacteristics
0x8140), so the mod takes the base from the loaded main module and adds the
RVA. Absolute addresses embedded in game instructions (for example the
`disp32` operands in `UiMouseWidthLoad`) are relocated by the loader; since
the relocation delta is a multiple of 64 KiB, their low byte is the same as
in the file.

## 4. Address table

Columns: **Name** is the stable name the spec uses. **2015** and **2020**
are RVAs. **What is there** describes the game code or data at that
address, with the original 2015 bytes for patch sites. **Spec** names the
documents that use it.

For call sites, "call → X" means a 5-byte `call rel32` (opcode E8) whose
target is RVA X in the 2015 build.

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
| `StereoObjectAlloc` | 0x5AFBA1 | 0x501CC1 | site | In the renderer constructor: `68 b0 00 00 00` (`push 0xB0`), then `89 be 98 f6 0c 00` and `89 be 9c f6 0c 00` (zero renderer +0xCF698 and +0xCF69C), `call` → 0x044330 (operator new), `add esp,4`, a null test, a call to the stereo constructor (RVA 0x5D9A60) and at 0x5AFBC5 `89 86 8c e6 00 00` (store into renderer +0xE68C). From 0x5AFBA1 to the store is 0x24 bytes; the `push` + two stores span 0x11 | 02 |
| `StereoObjectVtable` | 0xFE57BC | | rdata | Game stereo object vtable, 3 slots: destructor (RVA 0x5E3F40), OnSettingChanged(SettingEvent*) (RVA 0x5CCDB0, thiscall, ret 4), IsSettingAllowed (RVA 0x5CCEB0) | 02 |
| `StereoDeviceVtable` | 0xFE5418 | | rdata | Game stereo device (HMD) vtable, 21 slots; slot 11 (+0x2C) = RVA 0x5E0DA0 head pose into a 3x4 matrix; slots 12 to 15 below; slot 16 (+0x40) render-target size; slot 17 (+0x44) display size; slot 18 (+0x48) display device name | 02 |
| `StereoInitCall` | 0x613941 | 0x565E11 | site | In renderer initialisation: `mov ecx,[StereoObjectPtr]; push 1; call` → 0x5E0D40, `StereoInit(int type)`: thiscall, ret 4; type 1 creates the LibOVR device | 02 |
| `DeviceEyeTransform` | 0x5CD4C0 | 0x51F630 | function | Device slot 12: (int eye, float4x4* view, float4x4* proj), thiscall, ret 0xC. Builds the eye projection and offsets the view; eye -1 = centre | 02 |
| `DeviceFovAspect` | 0x5B8E70 | 0x50B010 | function | Device slot 13: (int eye, float* vfov, float* aspect), thiscall, ret 0xC | 02 |
| `DeviceEyeScaleOffset` | 0x5B8F10 | 0x50B0C0 | function | Device slot 14: (int eye, float out[4]), thiscall, ret 8 | 02 |
| `DeviceEyeViewport` | 0x5B9000 | 0x50B1B0 | function | Device slot 15: (int eye, int, int, int* left, int* top, int* right, int* bottom), thiscall, ret 0x1C; reads the device's eye rectangles, ignoring the target size it is given (07) | 02, 07 |
| `RecenterCallUpdate` | 0x5BE2E8 | 0x5104A8 | site | In `StereoUpdate`, when pad LB and RB are both held: `8b 40 2c` (`mov eax,[eax+0x2C]`, device slot 11), `8d 56 78` (`lea edx,[esi+0x78]`), `push edx`, `call eax`; the slot offset byte is at +2 | 04 |
| `RecenterCallMenu` | 0x5B8691 | 0x50A831 | site | In the stereo object's reset-orientation method (RVA 0x5B8680, called at RVA 0x091BD7, from the menu's reset-view item (likely)): `8b 42 2c` (`mov eax,[edx+0x2C]`), `call eax`, `ret`; offset byte at +2 | 04 |

### 4.3 Frames and render targets

| Name | 2015 | 2020 | Kind | What is there | Spec |
|---|---|---|---|---|---|
| `RendererPtr` | 0x17DF5CC | 0x17EF77C | data | Global pointer to the renderer | 03, 07 |
| `RendererDevice` | 0x621E20 | 0x574310 | function | cdecl (renderer) → `ID3D11Device*` (renderer +0x4) | 03 |
| `RendererSwapChain` | 0x621E30 | 0x574320 | function | cdecl (renderer) → `IDXGISwapChain*` (renderer +0x8) | 03 |
| `RendererContext` | 0x621E40 | 0x574330 | function | cdecl (renderer) → immediate `ID3D11DeviceContext*` (renderer +0x1EF3E8) | 03 |
| `RendererRtTexture` | 0x6232D0 | 0x575800 | function | cdecl (renderer, uint16 index) → `ID3D11Texture2D*` of render target `index` (renderer +0x1CD240 + 16·index) | 03 |
| `RendererRtSrv` | 0x6232F0 | 0x5757C0 | function | cdecl (renderer, uint16 index) → `ID3D11ShaderResourceView*` of render target `index` (renderer +0x1CD248 + 16·index) | 03 |
| `StereoRenderTargetSizeCall` | 0x60FD64 | 0x562244 | site | In the back-buffer size query, after `RiftTest1`: `mov ecx,[StereoObjectPtr]; push edi; push esi; call` → 0x5B8750, the stereo object's render-target size (int* w, int* h), thiscall, ret 8: sets 1280x800 then asks device slot 16 | 03 |
| `StereoDisplaySizeCall` | 0x60FE5D | 0x56233D | site | In the display size query, after `RiftTest2`: `call` → 0x5B8780, the stereo object's display size (int* w, int* h), thiscall, ret 8: sets 1280x800 then asks device slot 17 | 03 |
| `RenderTargetResizePending` | 0x1358470 | 0x1368620 | data | Byte. Set to 1 by the resolution setter (RVA 0x611250) when the size changes; the frame loop tests it at RVA 0x3903B2 and rebuilds the back buffer and render targets | 03, 06 |

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
| `PadControlToName` | 0x576840 | 0x4C85E0 | function | Pad slot 5: const char* ControlToName(uint16 code), thiscall, ret 4 (glyph id for prompts) | 04 |
| `PadHasActivity` | 0x576880 | 0x4C8620 | function | Pad slot 11: bool HasActivity(), thiscall, ret 0 | 04 |
| `PadConnectedSet` | 0x60EE6F | 0x56133F | site | In the pad poll (RVA 0x60EE30) after `XInputGetState`: `0f 95 c1` (`setne cl`, result != `ERROR_DEVICE_NOT_CONNECTED`), then `88 8e 89 02 00 00` (store to pad +0x289 "connected") | 04 |
| `KeyboardKeyDown` | 0x60EB20 | 0x560FE0 | function | Keyboard: bool KeyDown(int scancode), thiscall, ret 4 (DirectInput scan codes: 0x1D Ctrl, 0x38 Alt) | 04 |
| `KeyboardAnyKey` | 0x60EE00 | 0x5612D0 | function | Keyboard: bool AnyKey() (byte +0x224), thiscall, ret 0 | 04 |
| `MouseActive` | 0x9BBE70 | 0x4C87E0 | function | Mouse: bool Active() (+0x54 != 0), thiscall, ret 0 | 04 |
| `LeftStickPromptCall` | 0x42D0A0 | 0x43E1F0 | site | In the control-prompt name lookup (RVA 0x42CFF0): `call` → 0x57DCE0, "is movement on the left stick" (thiscall on an action map, ret 0, returns AL), then `test al,al` choosing `"lstick"` / `"rstick"`. A second identical call is at +0xEC (RVA 0x42D18C) | 04 |
| `UiMouseWidthLoad` | 0x443564 | 0x454704 | site | In the UI mouse update (RVA 0x443520): `f3 0f 10 15 dc 88 75 01` (`movss xmm2,[VA 0x17588DC]`), back-buffer width (display info +0x2C, struct at VA 0x17588B0). Display info +0x3C holds the UI reference width 1280.0. disp32 starts at +4 | 04 |
| `UiMouseHeightLoad` | 0x4435C2 | 0x454762 | site | Same for height: `f3 0f 10 15 e0 88 75 01` (display info +0x30); +0x40 holds 720.0 | 04 |
| `UiInputModeBranch` | 0x43EB3A | 0x44FCFA | site | Menu input-mode event handler (RVA 0x43EB00, thiscall, ret 8), event 0x12: `74 0c` (`je`) skipping the switch to focus/pad navigation (RVA 0x434E90) when keyboard and mouse are present | 04 |

### 4.5 Camera, HUD, menus, mode stack and loading

| Name | 2015 | 2020 | Kind | What is there | Spec |
|---|---|---|---|---|---|
| `StereoPushMode` | 0x5D9C00 | 0x52BE00 | function | Stereo object: push(int stereo mode), thiscall, ret 4. Pushes on the stack at +0x68/+0x6C and applies via RVA 0x5CD0A0 | 05 |
| `StereoPopMode` | 0x5D9C80 | 0x52BE80 | function | Stereo object: pop(), thiscall, ret 0 | 05 |
| `StereoModeStackAppend` | 0x294540 | | function | The game's int-array append used by `StereoPushMode` (its call at 0x5D9C53): thiscall (ECX = stereo +0x68, the array `{data, count, capacity}`), one argument `const int*`, ret 4, returns the new element's address or null; grows the array (8, then doubling) through the game allocator | 05 |
| `PushCallA` | 0x40EB8E | 0x41FD4E | site | `call` → 0x5D9C00 (stereo push) | 05 |
| `PushCallB` | 0x43EECA | 0x45008A | site | `call` → 0x5D9C00, in a hub-type screen | 05 |
| `PushCallC` | 0x44197A | 0x452B2A | site | `call` → 0x5D9C00, in the front-end menu's open method | 05 |
| `PopCallA` | 0x413DDA | 0x42508A | site | `call` → 0x5D9C80 (stereo pop), in the front-end menu's close method | 05 |
| `PopCallB` | 0x416382 | 0x4275D2 | site | `call` → 0x5D9C80 | 05 |
| `PopCallC` | 0x424C2A | 0x435DCA | site | `call` → 0x5D9C80 | 05 |
| `ResumeGamePop` | 0x453682 | 0x4649B2 | site | Front-end "Resume_game" handler (RVA 0x4535D0): 11 bytes, `8b 0d` + VA of `StereoObjectPtr` (`mov ecx,[...]`) and a `call` → 0x5D9C80 | 05 |
| `PauseMenuPush` | 0x43E545 | 0x44F705 | site | Pause menu "Pause" handler (RVA 0x43E520): 13 bytes, `8b 0d` + VA of `StereoObjectPtr`, `6a 01` (`push 1`) and a `call` → 0x5D9C00 | 05 |
| `StereoUpdateCall` | 0x3B4100 | 0x3C4FB0 | site | Once per frame: `call` → 0x5BE2A0, `StereoUpdate(float dt)`, thiscall, ret 4 (LB+RB recenter, fade animation) | 04, 05 |
| `BuildUiQuad` | 0x5BE550 | 0x510710 | function | cdecl `float4x4* (float4x4* out, float size, float inv_aspect, float dist)`, returns `out` in EAX; the caller copies from the returned pointer. Builds a quad facing the camera at `dist` | 05 |
| `VirtualScreenCall` | 0x5CD132 | 0x51F2A2 | site | In the stereo object's set-mode method (RVA 0x5CD0A0), mode 1: `call` → 0x5BE550 with (3.5, UI height/width, 3.0); result copied to stereo +0x20 | 03, 05 |
| `HudPanelCall` | 0x5CD2EE | 0x51F45E | site | In the stereo overlay draw (RVA 0x5CD170), mode 0: `call` → 0x5BE550 with (0.8, inv aspect, 1.0); used for this frame's HUD quad | 05 |
| `PovManagerPtr` | 0x1366A00 | 0x1376BB0 | data | Global pointer to the POV manager: +0x8 = POV 0 (main view), whose first 64 bytes are the world-to-view matrix; +0x88 POV count | 03, 05, 07 |
| `FollowVtable` | 0xDC6600 | 0xDDD6E8 | rdata | First-person camera behaviour vtable ("FollowBehavior"): slot 0 OnActivate(arg) (RVA 0x02C930), slot 1 OnDeactivate(arg) (RVA 0x02BBF0), both thiscall, ret 4. Yaw at +0x204 (mirror +0x1FC), ±1.0 = ±180°; pitch +0x208; look input enabled byte +0x264 | 04, 05 |
| `FollowOnActivate` | 0x02C930 | 0x03C1A0 | site | Function start; at +8 (RVA 0x02C938) `88 86 64 02 00 00` (`mov [esi+0x264],al`, al = 1) enables game look input | 05 |
| `FollowOnDeactivate` | 0x02BBF0 | 0x03B340 | function | Clears +0x264 | 04 |
| `FollowLookState` | 0x02D83C | 0x03D0BC | site | In the behaviour's state update (RVA 0x02D600): `8a 4c 24 0f` (`mov cl,[esp+0xF]`, previous +0x25E) then store to +0x261, a byte that enables the second look-integration branch | 05 |
| `CameraNoiseCall` | 0x2603B1 | 0x270C61 | site | In the camera update (RVA 0x260320): `8b ce` (`mov ecx,esi`) and a `call` → 0x24BF50, procedural sway and breathing offsets; 7 bytes in total | 05 |
| `HeadClipCompare` | 0x346E94 | 0x35794E | site | In the head-in-wall fade (RVA 0x346DE0, stdcall, ret 8): `0f 2f 05` + VA of a float 1e-5 (RVA 0xDC68A8) (`comiss xmm0,[...]`); disp32 at +3. The result goes to stereo object +0x64 | 05 |
| `CameraCorrectionBranch` | 0x34A0CE | 0x35ACCE | site | In the VR camera correction (RVA 0x34A050, thiscall, ret 0x10): `0f 86 2b 03 00 00` (`jbe` to the epilogue at RVA 0x34A3FF). The output vector is zeroed first; the correction is computed only when this branch is not taken | 05 |
| `SsaoSettingBranch` | 0x575421 | 0x4C71D1 | site | In the SSAO setting handler (RVA 0x575400, setting 0x15): `75 10` (`jne`), choosing what to store in +0xD80 and the enable byte +0xD84 | 05 |
| `UiEntityOnMessage` | 0xDFCE18 | 0xE13F38 | rdata | Vtable entry (vtable at RVA 0xDFCE10, slot 2) holding RVA 0x51BD70, the UI entity's message handler: thiscall (const ShortGuid* msg, int, int, int), ret 0x10 | 05 |
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

### 4.6 Options, UI and configuration

| Name | 2015 | 2020 | Kind | What is there | Spec |
|---|---|---|---|---|---|
| `UiInvokeStr2` | 0x42B450 | 0x43C840 | function | On a menu object (the `this` of the menu vtable methods): call a named ActionScript function of its movie with two strings; thiscall (const char* name, const char*, const char*), ret 0xC | 06 |
| `UiInvokeBool` | 0x42A7D0 | 0x43B920 | function | Same with one bool; thiscall (name, bool), ret 8 | 06 |
| `UiInvokeInt` | 0x1341A0 | 0x144470 | function | Same with one int; thiscall (name, int), ret 8 | 06 |
| `UiInvokeFloat` | 0x134240 | 0x144510 | function | Same with one float; thiscall (name, float), ret 8 | 06 |
| `UiExternalCallSlot` | 0xF00078 | 0xF177D8 | rdata | Vtable entry (slot 1 of the vtable at RVA 0xF00074) holding RVA 0x43A0A0 (2020 0x44B250): the front end's Scaleform external-interface dispatch. thiscall (context, const char* name, args, arg count), ret 0x10; hashes `name` and calls the callback registered for it | 06 |
| `UiPakPathPtr` | 0x124F748 | 0x125F978 | data | Pointer to the string `data\ui.pak`, the UI archive the game opens | 06 |
| `ShowBootupPush` | 0x441AED | 0x452C9D | site | In a front-end method: `68 78 e2 2f 01` (`push` VA of the string `ShowBootup`), then `mov ecx,esi` and a `call` → 0x42AAD0, which invokes that ActionScript function (with 1 and one more argument; thiscall, ret 0xC) and starts the intro sequence | 06 |
| `QuitRequested` | 0x17DA920 | 0x17EAAD0 | data | Byte: the game's exit request flag (getter RVA 0x614760, setter RVA 0x614770; set for example right after a fatal start-up `MessageBoxA` at RVA 0x613F38). Setting it to 1 asks the game to exit (likely) | 06 |

### 4.7 Planar reflections and render fixes

2015 only; the 2020 addresses are not known yet, so the fixes of 07 and 08
§2 to §4 stay off on that build (08 §1 needs no game address). Bytes are
the 2015 originals, checked against the binary. "call → X" sites are 5-byte
`call rel32` (E8) whose target is RVA X; a redirect must check the opcode
and that the displacement resolves to X.

Reflections (07):

| Name | 2015 | 2020 | Kind | What is there | Spec |
|---|---|---|---|---|---|
| `ReflectionManagerPtr` | 0x135835C | | data | Global pointer to the reflection manager: +0x04 target, +0x56 usable, +0x58/+0x5C target size, +0x60 quality, +0x6C/+0x70 candidate planes and count, +0x78 flag | 07 |
| `ReflectionPovUpdate` | 0x5B41E0 | | function | Reflection POV per-frame update (thiscall, POV in ECX); called at 0x5B4640 (`e8 9b fb ff ff`) from the POV manager update 0x5B45A0 for every type-1 POV | 07 |
| `ReflectionRenderCall` | 0x5B5D91 | | site | `e8 aa fe ff ff`: call → 0x5B5C40, in the POV manager's render loop (RVA 0x5B5D40), POV type 1; the only call of it | 07 |
| `ReflectionRender` | 0x5B5C40 | | function | Renders the reflection POV, mono; thiscall, no arguments, ret 0; prologue `55 8b ec 83 e4 f0 83 ec 2c` | 07 |
| `PovSetEye` | 0x5B38D0 | | function | POV set eye (int eye), thiscall, ret 4. Eye -1 saves the centre view and projection to `PovEyeSave`. Must not be called on the reflection POV | 07 |
| `PovEyeSave` | 0x142F5E0 | | data | Shared save area of `PovSetEye`: view float[16] at 0x142F5E0, projection float[16] at 0x142F620 | 07 |
| `PovBeginRender` | 0x5B11D0 | | function | POV begin render: uploads view, projection and camera constants, sets target and viewport | 07 |
| `PovUpdateFrustum` | 0x5B2FB0 | | function | Recomputes a POV's culling frustum from its view and projection; thiscall, no stack arguments | 07 |
| `PovViewport` | 0x5B0820 | | function | POV viewport: thiscall, one pointer to `{left, top, right, bottom}`, ret 4; whole target for eye -1, else via `StereoEyeViewport` | 07 |
| `StereoEyeViewport` | 0x5B8730 | | function | Stereo object wrapper that calls device slot 15 with `(eye, width / 2, height, &l, &t, &r, &b)` when the device state is 2 | 07 |
| `StereoApplyEyeTransform` | 0x5B86A0 | | function | Stereo object: (int eye, float view[16], float proj[16]), thiscall, ret 0xC; forwards to device slot 12 when the device state is 2 | 07 |
| `RendererEyeOwner` | 0x13583E4 | | data | Global pointer; the int at +0x2C is the renderer's current eye | 07 |
| `RendererClear` | 0x621770 | | function | Renderer clear, cdecl (renderer, flags, ARGB colour, float depth, stencil); flags 1 colour, 2 depth, 4 stencil. First six bytes `55 8b ec 83 ec 10`, position independent | 07 |
| `PassSetup` | 0x595170 | | function | Render pass setup; for some pass types rebuilds the viewport through 0x574EE0 as `{0, 0, right - left, bottom - top}` (07 §5 item 3) | 07 |
| `RendererSetViewport` | 0x621350 | | function | cdecl (context, int vp[6] = {x, y, w, h, minZ, maxZ}) | 07 |
| `ScreenConstantsUpload` | 0x574F50 | | function | Uploads pixel constant buffer 2 register 60 (`ScreenResolution`) and vertex constant buffer 1 register 21 (viewport fraction of the target) | 07 |

Culling, anti-aliasing and head-in-wall fade (08):

| Name | 2015 | 2020 | Kind | What is there | Spec |
|---|---|---|---|---|---|
| `CullObjectsCall` | 0x5E984B | | site | `e8 d0 64 03 00`: call → 0x61FD20, the only call of the per-object visibility loop; cdecl, the caller pops 0x18 | 08 |
| `CullObjects` | 0x61FD20 | | function | Per-object visibility loop: cdecl (cull context*, flags*, bounds[] stride 0x40, items[], count, out[]); serves the main view and shadow casters | 08 |
| `CullDistanceCall` | 0x61FD95 | | site | `e8 b6 9c ff ff`: call → 0x619A50, followed by `83 c4 0c` | 08 |
| `CullDistance` | 0x619A50 | | function | Distance/LOD test: cdecl (bounds*, const vec3* camera position, float scale), result in AL (true = keep) | 08 |
| `CullFrustum` | 0x619030 | | function | Frustum test: (bounds*, context*) | 08 |
| `CullPlaneTestACall` | 0x61FDB9 | | site | `e8 a2 9a ff ff`: call → 0x619860 (taken when context +0x5C is set), followed by `83 c4 0c` | 08 |
| `CullPlaneTestA` | 0x619860 | | function | Extra plane/occluder test for the main view; cdecl, 3 arguments, result in AL | 08 |
| `CullPlaneTestBCall` | 0x61FDD2 | | site | `e8 49 9b ff ff`: call → 0x619920 (taken when context +0x60 is set, with context +0x70), followed by `83 c4 0c` | 08 |
| `CullPlaneTestB` | 0x619920 | | function | Second extra plane test; cdecl, 3 arguments, result in AL | 08 |
| `AaSettingHandler` | 0x5F6AC0 | | function | Graphics setting 0x09 handler: thiscall, one argument, ret 4; stores the AA mode at post-processing object +0x1B3C (1 FXAA, 2 SMAA T1x, 3 SMAA T2x, else 0) | 08 |
| `PostFxInstance` | 0x5F19C0 | | function | Returns the post-processing object; cdecl, no arguments | 08 |
| `FinalPostChain` | 0x610040 | | function | Final post chain (to 0x6101FA): post pass, then flat AA + post pass, or the stereo output | 08 |
| `StereoRunningTest` | 0x610144 | | site | `b9` + VA of 0x13583A0 (`mov ecx, ...`) then a call → 0x01E150: true when the stereo device state is 2; selects the stereo branch | 08 |
| `AaPass` | 0x600210 | | function | Post-processing AA pass, thiscall on the post-processing object, no arguments | 08 |
| `SmaaCall` | 0x600479 | | site | In the AA pass: `e8 42 a0 01 00`: call → 0x61A4C0, the SMAA runner (cdecl, 3 arguments; `83 c4 0c` follows) | 08 |
| `PostFxPass` | 0x600490 | | function | Post-processing pass | 08 |
| `StereoOutput` | 0x5FC560 | | function | Stereo output: sets pixel constant buffer 2 register 36 from post values times stereo +0x64, then draws through 0x5A0980; first bytes `83 ec 1c 53` | 08 |
| `StereoOutputDrawCall` | 0x6101DC | | site | `e8 9f 07 f9 ff`: call → 0x5A0980, the full-screen draw (cdecl, no arguments) in the stereo branch of the final post chain; the place where the AA pass is missing | 08 |
| `VelocityPass` | 0x5B1720 | | function | SMAA T2x velocity pass; runs on the eye-0/centre path only | 08 |
| `MainViewEye1` | 0x5B5B70 | | function | Prepares and renders the main view's eye 1 (`PovSetEye(1)`, render, restore the centre); skips the velocity pass | 08 |
| `PostSourceResolution` | 0x6007B0 | | site | Post-processing setup (to 0x600811): size query 0x627B90, then the constant upload 0x621C90 of pixel constant buffer 2 register 39 = (W, H, 1/W, 1/H) | 08 |
| `HeadClipFadeCall` | 0x34E2C1 | | site | `e8 1a 8b ff ff`: call → 0x346DE0 in the VR camera behaviour update; its result is stored to stereo +0x64 | 08 |
| `HeadClipFade` | 0x346DE0 | | function | Head-in-wall fade: stdcall (const vec3* eye, const vec3* pivot), ret 8, visibility in ST0 (1 visible to 0 black) | 05, 08 |

### 4.8 Known but not needed

These are in the game and were touched by earlier mods, but v1 needs no
change to them:

- RVA 0x43AB40 (2020 0x44BCF0): a call to the control-prompt name lookup
  (RVA 0x42CFF0, cdecl, 2 arguments). Earlier mods hooked it with a pure
  pass-through.
- RVA 0x831B00 (2020 0x8473D0): re-initialises the audio output device.
  Only needed to move game audio to a headset's own audio endpoint, which
  OpenVR runtimes handle themselves.
- RVA 0x5CCDB0 / 0x5CCEB0 (2020 0x51EF20 / see §3.2): the game's own stereo
  OnSettingChanged and IsSettingAllowed, listed in `StereoObjectVtable`.
