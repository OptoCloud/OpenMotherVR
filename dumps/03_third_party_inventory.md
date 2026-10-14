# Statically-linked third-party code inventory

Goal: tag everything that is **not** mod-original so we skip it during reconstruction.

## Confirmed statically linked

### Microsoft Universal CRT (UCRT) / MSVC runtime — Visual Studio 2015 toolchain
- **Evidence:** Ghidra FunctionID matched ~70 functions into MSVC namespaces; string `minkernel\crts\ucrt\inc\corecrt_internal_strtox.h` at `0x100523a0`; library banners "Library: Visual Studio 2015 Release" on `dllmain_*`.
- **Namespaces matched (counts from `functions.tsv`):**
  - `__crt_strtox::*` — 34 functions (~20 KB) — number parsing
  - `__crt_stdio_output::*` — ~22 functions (~3 KB) — printf machinery
  - `__crt_seh_guarded_call*` — ~9 functions — SEH wrappers
  - `__crt_state_management`, `__crt_unique_heap_ptr`, `_LocaleUpdate`, `_Init_atexit`, `_Fac_node`, `_Lockit`, `_Locinfo`, `_Yarn`, `_Facet_base`
  - `std::*` — ~30 functions (locale, ios_base, ctype, basic_filebuf, error_category, bad_exception, basic_streambuf, experimental::filesystem::v1)
  - 9 anonymous lambdas (CRT internals)
  - CRT startup: `entry`, `dllmain_dispatch`, `dllmain_raw`, `dllmain_crt_*`, `___scrt_dllmain_*` at `0x10026...`
- **Unmatched-but-likely-CRT in the Global namespace:** at least `_memcmp`, `_memset`, `_strcat_s`, `__wcsicmp`, `__security_check_cookie`, `__SEH_prolog4/epilog4`, and the heap/atexit/exception machinery. Probably another ~150–300 functions.
- **Action:** skip when reconstructing — link against the real UCRT instead.

### nlohmann::json (header-only library)
- **Evidence:** string `[json.exception.` at `0x1005c928`. Header-only library compiled into the binary.
- **Footprint:** probably 30–80 functions reachable from the config load/save paths (`FUN_10010750`, `FUN_10014680`). Hard to count without tracing — they don't have a distinct namespace because the header-only inlining flattens them into the calling TU.
- **Action:** skip — link the real `nlohmann/json.hpp` (single-header).

### C++ STL (`std::*`) — bundled with MSVC stdlib
- Already counted above under the MSVC namespaces.
- **Action:** skip — `std::*` is part of the toolchain.

## Dynamically loaded — NOT statically linked

### OpenVR (`openvr_api.dll`) — delay-loaded
- **Evidence:**
  - Static imports table lists 6 OPENVR_API.DLL symbols, but each goes through a **delay-load thunk** (`DelayLoad_VR_*_100249xx` in the decompile dir). That means the binary was linked with `/DELAYLOAD:openvr_api.dll` — the DLL is loaded on **first use**, not at process start.
  - Confirms the error path `"Failed to load openvr_api.dll!"` (the load can legitimately fail at first call).
  - String `\openvr_api.dll` at `0x1005a65c` is the path used when locating the runtime.
- **Surface:** only 6 import names, all `VR_*` entry points. All IVRSystem/IVRCompositor/IVRInput/IVROverlay calls happen through COM-style vtables returned by `VR_GetGenericInterface`.
- **Action:** in the rebuild, declare a normal `#pragma comment(lib, "openvr_api.lib")` with delay-load. Use real `<openvr.h>` types.

### Oculus PC SDK (LibOVR) — dynamically loaded via LoadLibrary/GetProcAddress
- **Evidence:** 32+ `ovr_*` symbol strings exist (`ovr_GetConnectedControllerTypes`, `ovr_CreateMirrorTextureWithOptionsDX`, `ovr_SetControllerVibration`, etc.) but **none appear in the PE imports table**. The mod must `LoadLibrary("LibOVRRT32_1.dll")` (or similar) on demand and `GetProcAddress` each function by name.
- **Footprint:** all the `ovr_*` strings live in `.rdata` and are used as argument literals to GetProcAddress. Counting them gives the size of the LibOVR client wrapper interface the mod uses (~32 functions).
- **Action:** in the rebuild, link normally against the real LibOVR — but preserve the dynamic-load wrapper if we want to keep the behavior of degrading gracefully when LibOVR isn't installed.

### D3D11 / D3D12 — resolved dynamically via DXGI COM objects
- **Evidence:** no static imports for `D3D11.dll`/`D3D12.dll`, no `D3D*CreateDevice*` import strings. The mod gets D3D devices via the swapchains and factories returned through the DXGI proxy (`IDXGISwapChain::GetDevice` etc.).
- **Action:** N/A — we don't link D3D either; we work with the device pointers the game gives us.

### Windows system DLLs (statically imported, used directly)
- KERNEL32, USER32, OLE32, SHELL32, SHLWAPI, IMAGEHLP — all standard, used for: file I/O, command line parsing, message boxes, PE inspection (`MapAndLoad` is from IMAGEHLP), COM init.
- **Action:** standard Win32, no work needed.

## Estimated reconstruction surface

| Category | Functions (rough) | KB | Reconstruct? |
|---|---|---|---|
| Total `.text` | 1976 | 308 | — |
| FunctionID-matched MSVC/STL | ~70 | ~25 | No |
| Unmatched UCRT in Global | ~200 | ~50 | No |
| nlohmann::json inlined | ~50 | ~25 | No |
| **Mod-original** | **~1600** | **~200** | **Yes** |

The 1600 / 200 KB target is what we actually need to rewrite as idiomatic C++. About 800 of these will be small helpers (1–3 line getters, vtable trampolines, RAII destructors) and ~100 will be the meaty subsystem cores.
