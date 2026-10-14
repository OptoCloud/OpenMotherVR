# Entry points, DXGI proxy surface, and OpenVR call sites

## PE entry chain

The CRT startup chain is standard MSVC 2015:

```
entry @ 0x10026e8f               (_DllMainCRTStartup, 35 bytes)
  → dllmain_dispatch @ 0x10026d69   (216 bytes — CRT-managed dispatch)
       → dllmain_raw @ 0x10026e65   (42 bytes — empty user stub, returns 1)
       → dllmain_crt_dispatch @ 0x10026b87   (CRT init/teardown)
       → FUN_10002250 @ 0x10002250   ← REAL USER DllMain  (552 bytes)
```

**The reverse target for "DllMain" is `FUN_10002250`.** The other six are CRT scaffolding (matched as "Visual Studio 2015 Release" by Ghidra FunctionID) and can be ignored — we just write a normal `BOOL WINAPI DllMain(HMODULE, DWORD, LPVOID)`.

### What `FUN_10002250` does (decoded)

On `DLL_PROCESS_ATTACH`:

1. **Parse command line** with `GetCommandLineW` + `CommandLineToArgvW`. Checks for `-loadcatch` flag (debugging aid — calls `FUN_10002480` then `FUN_1001d5b0(0x41)`).
2. **Load the real system DXGI:** `GetSystemDirectoryA` + `LoadLibraryA("...\\System32\\dxgi.dll")`, handle stored in `DAT_10068698`. Failure → message box `"Failed to load System dxgi.dll!"` → return without enabling.
3. **Resolve real DXGI exports** by calling `FUN_100025b0()` — see below.
4. **Validate Alien: Isolation .exe build:**
   - `GetModuleHandleA("AI.exe")` + `GetModuleFileNameA` to get the game executable path.
   - `MapAndLoad("AI.exe", systemDir, &loadedImg, dotDll=1, readOnly=1)` (IMAGEHLP) opens it as a PE.
   - Reads `IMAGE_FILE_HEADER.TimeDateStamp` (offset +8 in the header pointer at `local_140`).
   - Compares to magic value **`0x5FD25676`** — the targeted AI.exe build timestamp (≈ Dec 10 2020 UTC, the Steam build mod was authored against).
   - On mismatch → `"Exe check failed! [0x%X]"` message and degrade gracefully (VR stays disabled).
5. (More in the rest of the function — UI.PAK check, first-launch welcome dialog, runtime detection between Oculus/OpenVR — to be unpacked when reconstructing this subsystem.)

## DXGI proxy surface

`dxgi.dll` exports 21 symbols. All 21 are **6-byte forwarder thunks** — pure `jmp dword ptr [IAT_slot]`:

| Export | Stub addr | Forwards via |
|---|---|---|
| ApplyCompatResolutionQuirking | 0x100026d0 | `*_DAT_10068648` |
| CompatString                  | 0x100026e0 | `*_DAT_1006864c` |
| CompatValue                   | 0x100026f0 | `*_DAT_10068650` |
| CreateDXGIFactory             | 0x10002700 | `*_DAT_10068654` |
| CreateDXGIFactory1            | 0x10002710 | `*_DAT_10068658` |
| CreateDXGIFactory2            | 0x10002720 | `*_DAT_1006865c` |
| DXGID3D10CreateDevice         | 0x10002730 | `*_DAT_10068660` |
| DXGID3D10CreateLayeredDevice  | 0x10002740 | `*_DAT_10068664` |
| DXGID3D10ETWRundown           | 0x10002750 | `*_DAT_10068668` |
| DXGID3D10GetLayeredDeviceSize | 0x10002760 | `*_DAT_1006866c` |
| DXGID3D10RegisterLayers       | 0x10002770 | `*_DAT_10068670` |
| DXGIDumpJournal               | 0x10002780 | `*_DAT_10068674` |
| DXGIGetDebugInterface1        | 0x10002790 | `*_DAT_10068678` |
| DXGIReportAdapterConfiguration| 0x100027a0 | `*_DAT_1006867c` |
| DXGIRevertToSxS               | 0x100027b0 | `*_DAT_10068680` |
| PIXBeginCapture               | 0x100027c0 | `*_DAT_10068684` |
| PIXEndCapture                 | 0x100027d0 | `*_DAT_10068688` |
| PIXGetCaptureState            | 0x100027e0 | `*_DAT_1006868c` |
| SetAppCompatStringPointer     | 0x100027f0 | `*_DAT_10068690` |
| UpdateHMDEmulationStatus      | 0x10002800 | `*_DAT_10068694` |
| (one ordinal aliases another) |  | |

The `_DAT_*` slots from `0x10068648`–`0x10068694` are populated at startup by **`FUN_100025b0` @ 0x100025b0** (~284 bytes) — a flat sequence of `GetProcAddress(systemDxgi, "...")` calls, one per export name.

**Crucial insight:** none of the exports themselves are hooked. They forward the call cleanly to the real system DXGI. The interception happens on the **COM objects** the real DXGI returns. Specifically, after a game-side `CreateDXGIFactory*` call returns to the game, the game holds a real `IDXGIFactory*`. Either the mod patches that factory's vtable in-place (vtable hooking) or the game calls `CreateSwapChain`, gets a real `IDXGISwapChain*`, and *that* swapchain's vtable gets patched. The actual hook installation lives in:

- **`FUN_1001d6f0` @ 0x1001d6f0** — 4142 bytes, one of the binary's largest non-CRT functions. Uses `VirtualProtect`. Almost certainly the central code/vtable patcher (probably MinHook-style trampoline writer, or a custom IAT/vtable rewrite routine — to be confirmed on inspection).
- **`FUN_1001d4d0` @ 0x1001d4d0** — companion function, also touches `VirtualProtect`. Likely a helper (single-pointer patch / page-protection RAII).
- `DloadProtectSection @ 0x10024bf9` — CRT-generated delay-load helper, ignore.

This is a small surface — three functions handle all in-process code/vtable patching for the entire mod.

## OpenVR integration surface

`openvr_api.dll` is **delay-loaded** (linker flag `/DELAYLOAD:openvr_api.dll`). The 6 imports have linker-generated thunks at `0x100249xx`:

```
DelayLoad_VR_GetInitToken                     @ 0x100249ac
DelayLoad_VR_GetGenericInterface              @ 0x100249c7
DelayLoad_VR_InitInternal2                    @ 0x100249d1
DelayLoad_VR_IsInterfaceVersionValid          @ 0x100249db
DelayLoad_VR_ShutdownInternal                 @ 0x100249e5
DelayLoad_VR_GetVRInitErrorAsEnglishDescription @ 0x100249ef
```

These thunks call `__delayLoadHelper2` on first use, which `LoadLibrary`s `openvr_api.dll` lazily. The "Failed to load openvr_api.dll!" error path is the user-friendly recovery for a delay-load failure.

**13 distinct functions** in the mod call into these thunks (i.e., touch OpenVR directly). Listed from `grep`-trace through `decompiled/`:

- `FUN_100066c0`
- `FUN_10006730`
- `FUN_10006800`
- `FUN_100068b0`
- `FUN_10006b40`
- `FUN_10006f20`
- `FUN_10006fe0`
- `FUN_10007060`
- `FUN_100070e0`
- `FUN_10007220`
- `FUN_10007ad0`
- `FUN_100085c0`
- `FUN_10008710`

All cluster in **`.text` range `0x10006xxx – 0x10008xxx`** — this is the `MotherVR::OpenVR` class's methods. The other ~1200+ mod-code functions are organized similarly into:

- `0x10004xxx – 0x10005xxx` — likely `MotherVR::Oculus` (LibOVR backend, since it has the largest functions and uses `ovr_*` symbol-name strings)
- `0x10009xxx – 0x1000Bxxx` — likely `MotherVR::StereoManager` / `MotherVR::StereoInterface` (orchestration)
- `0x1000Cxxx – 0x10013xxx` — game integration, hooks, config, menus
- `0x10014xxx – 0x1001Cxxx` — render-side and input handling
- `0x1001Dxxx – 0x10024xxx` — hook plumbing + CRT mix

(Boundaries are educated guesses — refine when actually reconstructing each.)

Everything past `VR_GetGenericInterface` is **virtual-dispatch through OpenVR interface vtables** (`IVRSystem`, `IVRCompositor`, `IVRInput`, `IVROverlay`, `IVRChaperone`, etc.). Those won't show up as `VR_*` string references — they're `(**(code **)(*ivrSystem + offset))(...)` patterns in the decompile. To find them: grep for indirect-call patterns from pointers obtained via `VR_GetGenericInterface` (which always returns a vtable).

## Reconstruction implications

- The DllMain rewrite is mechanical: load real DXGI, populate function-pointer table, run sanity checks, install hooks, optionally pop a welcome dialog, return TRUE.
- The 21 DXGI proxy stubs can be a single small `.def` file plus 21 trivial `__declspec(naked) jmp` thunks (or a generator macro).
- The OpenVR backend (`MotherVR::OpenVR`) is a contained ~13-function class; recover it as one unit (`open_vr.h` + `open_vr.cpp`).
- All vtable/code patching goes through 2 helpers (`FUN_1001d6f0`, `FUN_1001d4d0`). Recovering these gives the hook primitive used everywhere else.
