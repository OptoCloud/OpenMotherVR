# 11. Code review of `recon/` (d3d11.dll proxy reconstruction)

Scope: every file under `recon/src`, `recon/CMakeLists.txt`, `recon/triplets`, `recon/vcpkg.json`,
`recon/test_load.ps1`, `recon/README.md`, at `4d242c7`. Game facts (addresses, layouts) were taken from the
`dumps/` analyses and not re-verified. Each claim below was checked against the code; line numbers are for
that commit.

Build check: a scratch configure with the VS 2026 generator, Win32, the vcpkg toolchain, and `/W4` plus the
off-by-default warnings C4242 C4254 C4263 C4265 C4287 C4296 C4388 C5038 C4062 C4826 C4905 C4906 C4928 C4946
C5204 C4191 rebuilt cleanly. The only warnings were **C4191** on `FARPROC` casts
(`proxy/d3d11_proxy.cpp:235,239`, `runtime/openvr_backend.cpp:56`), which are expected for `GetProcAddress`.
`CMakeLists.txt` already passes `/W4`.

Severity key: **bug** = wrong behaviour today; **risk** = wrong behaviour under plausible conditions (other
build, missing runtime, malformed input, threads, failure path); **cleanup** = maintainability or hygiene.

Totals: **4 bugs, 17 risks, 22 cleanups** (plus the build-hygiene notes at the end).

**Note: HEAD moved during the review.** Three commits landed after `4d242c7`: `6051363` (display-reset flag),
`774d692` (dialogs and -vrforce switches) and `dca7ab0` (audio redirect).
- **B3 is fixed by `6051363`:** `g_display_reset_flag` replaces the write-only dirty flag.
- **R3 got worse:** `774d692` adds two more `MessageBoxW` calls in `on_attach`, for an unknown build and on
  first launch, both under the loader lock.
- **R1 still holds:** `dllmain.cpp` still logs `dir.string()`.
- **Line numbers may be off:** they can be a few lines out in `dllmain.cpp`, `vr_detect.cpp`,
  `openxr_backend.cpp`, `openvr_backend.cpp`, `script_bridge.*` and `hook_installer.cpp`.

The new `audio/` module was not reviewed.

---

## Bugs

### B1. Index controller default bindings: pause and flashlight are bound to the wrong buttons (bug)
`runtime/openvr_actions.cpp:99-105`
`stick_sources(...)` already pushes the left-hand buttons as `hand("left", button_x)` / `hand("left", button_y)`.
The knuckles call passes `"a"` / `"b"` for x/y, so sources [8] = left/a → map and [9] = left/b → flashlight
are correct before the override. Lines 104-105 then overwrite **[9]** with left/a and **[10]** (the pause
source, `left/trackpad`) with left/b. As a result, flashlight and map are both on left A, pause is on left B,
and the left trackpad press does nothing. That contradicts `kIndexHelp` ("Flashlight Y", "Pause Menu Left
Trackpad Press") and the comment on lines 102-103. The two lines look left over from a version of
`stick_sources` that put x/y on the right hand.
**Fix:** delete lines 104-105. Replace the index-based JSON surgery with named parameters (e.g. pass
`left_a`/`left_b` explicitly), so the order of `push_back` calls cannot silently change the meaning.

### B2. Partly written `openvr_api.dll` is stamped as up to date (bug)
`integration/payloads.cpp:57-64`
`SetFileTime(h, &stamp, …)` (line 59) runs before `ok` is checked. If `WriteFile` fails partway (disk full,
AV interference), the truncated file still gets the build stamp. From then on `up_to_date()` returns true on
every launch, so the broken loader is never rewritten, and `openvr::init` loads it in preference to SteamVR's
own copy (`openvr_backend.cpp:91-92`).
**Fix:** stamp only when `ok`. On failure, `DeleteFileW(target)` after `CloseHandle`. Better still, write to
`target + L".tmp"`, stamp it, then `MoveFileExW(MOVEFILE_REPLACE_EXISTING)`. `deploy_mother_pak` gets this
right because it stamps only after `build_ui_archive` succeeds.

### B3. Menu changes to pixel scale and mirror view never reach the renderer; the dirty flag is write-only (bug)
`integration/script_bridge.h:64-70`, `integration/script_bridge.cpp:115,199,205,214`,
`stereo/stereo_device.cpp:246-250`
- `g_stereo_settings_dirty` is set in four places and read nowhere (checked with grep).
- The header says "The stereo manager reads and clears it on the next frame" and "From: DAT_10066d44". Both
  are false. DAT_10066d44 is a pointer to AI.exe's display-reset byte (address table slot 0x5F, see
  `09_mothervr_coverage.md` F1), not a MotherVR flag.
- The mirror constant buffer is only filled in `dev_configure_rendering`, and `get_render_target_size` is only
  asked again when the game rebuilds its targets.

As a result, `mvr_pixelscale`, `mvr_mirrorview` and `mvr_mirrorviewsym` take effect only after a restart or
resize. The original applies them at once by writing the game's flag. This is already listed in 09 F1; it is
repeated here because the code comment states the opposite.
**Fix:** add `AiAddr::DisplayResetFlag = 0x5F` and write `*ai_address(...) = 1` where the original does (push
settings and the three commands). Delete `g_stereo_settings_dirty` and correct the header comment.
Separately, recompute the mirror constants in `dev_end_frame` when the config changes, which is cheap.

### B4. The `mvr_vrruntime` command sets the change flag even when the value is rejected (bug, minor)
`integration/script_bridge.cpp:179-185`
For `n` outside 0..2, `cfg.vr_runtime` is unchanged, but `*g_vr_runtime_changed = 1` is still written and
`config::save_current()` still runs. The game-side meaning of the flag is unknown (header line 53), so raising
it without a change is at best noise.
**Fix:** move the flag write inside the `if (n >= 0 && n <= 2)` branch.

---

## Risks (ranked)

### R1. `std::filesystem::path::string()` can throw inside `noexcept` code and inside `DllMain` (risk, high impact)
`dllmain.cpp:37`, `runtime/vr_detect.cpp:102,104` (in `noexcept detect()`), `integration/payloads.cpp:53,63,66,76,84`
(in `noexcept` functions), `runtime/openxr_backend.cpp:61,64,349,353`, `runtime/openvr_backend.cpp:103,108,129`,
`runtime/openvr_actions.cpp:218`
MSVC's `path::string()` converts with the ANSI code page and throws `std::system_error` ("No mapping for the
Unicode character…") when a character has no mapping. Triggers include the install path, `%LOCALAPPDATA%`
(the Windows user name) and the SteamVR library path. In a `noexcept` function this calls `std::terminate`.
In `on_attach` the exception leaves `DllMain`. Either way the game dies at launch.
`SetActionManifestPath(manifest_path.string())` also passes ACP text where OpenVR expects UTF-8.
Separately, `vr_detect.cpp:99` uses the throwing `std::filesystem::exists(api_dll)` overload inside
`noexcept detect()`.
**Fix:** add `std::string diag::utf8(const std::filesystem::path&)` that uses `WideCharToMultiByte(CP_UTF8)`
(or `u8string()` copied to `std::string`) and use it for every log argument and for OpenVR. Use the
`error_code` overload of `exists`. Wrap `on_attach` in `try { … } catch (...) { log }` so that nothing can
escape `DllMain`.

### R2. Ignored `redirect_call` failures can leave the stereo hand-off half installed (risk)
`hook/hook_installer.cpp:148-164`, `199-214`
- Several writes are blind: the `StereoManagerAlloc` code (`B8 imm32; EB 1D`, lines 151-153, with no check
  that the site is `push 0xb0`), the slot-21 byte patches (158-159), and every byte patch in step 9.
- The `redirect_call` results that guard the critical call sites (`CreateDevice`, `GetRenderTargetSize`,
  `Stereo::Update`, `PushMode`/`PopMode`) are discarded.

If any one of these fails, for example because a site does not hold `E8`, the game receives the static
`StereoManager` but runs its own `Stereo::Init` on it, which allocates a LibOVR device into fields our code
also owns. The `calls()` pre-checks used for the reconstruction's own fixes (lines 236-285) show the safer
pattern.
**Fix:** run a preflight pass that verifies every site (opcode byte plus, where known, the call target or a few
prologue bytes) before writing anything. If any core site fails, return false with nothing patched. Keep the
per-fix opt-out only for optional fixes.

### R3. Heavy work in `DllMain`, under the loader lock (risk)
`dllmain.cpp:31-63`, `proxy/d3d11_proxy.cpp:220`, `integration/payloads.cpp:86`, `integration/ui_patch.cpp`
On attach the DLL does all of the following under the loader lock:
- `LoadLibraryExW(System32\d3d11.dll)`
- reads the whole of `UI.PAK` and writes `MOTHER.PAK`
- writes `mothervr.json`
- reads the registry and JSON for the OpenXR probe (plus `_wputenv_s`)
- may show a `MessageBoxW`

Microsoft documents LoadLibrary and UI calls in DllMain as deadlock hazards. It works today because the game is
single-threaded at static-import time, but any future injection path, such as an overlay that loads d3d11
early on a worker thread, would hit it.
**Fix:** in `DllMain`, keep only the address table, the code and vtable patches (which must precede game code)
and the log. Move `proxy::d3d11::init` to the first call of any export: the naked thunks can `call` a one-time
init stub before jumping. Move PAK building to the first moment the game opens UI.PAK, or at least make it
the only file work left in attach. Replace the message box with a log line, or post it later.

### R4. Stereo backend lifecycle: no shutdown, re-entry leaks, and an OpenVR session left open on a failed init (risk)
`stereo/stereo_device.cpp:93-113`, `507-521`; `runtime/openvr_hmd.cpp:101-117`; `runtime/hmd_backend.h:59-61`
- `create_device(1)` unconditionally creates a new backend. A second call (the game's `CreateDevice(int)` can
  be re-entered on a display reset) destroys the old `OpenVrHmd` without `VR_Shutdown`. It then runs
  `VR_InitInternal2` again, re-`LoadLibrary`s `openvr_api.dll`, and tries to create a second
  `"MotherVROverlay"`, which fails with KeyInUse because the old overlay was never destroyed.
- `OpenVrHmd::init` returns false at line 116 (a missing interface) after `openvr::init` succeeded, without
  calling `openvr::shutdown()`.
- Neither backend has a destructor that releases its runtime. `openxr::shutdown()` is never called (its header
  claims "shutdown() at detach").

**Fix:** make `create_device` idempotent: return early if `g_backend` is already initialised. Give
`OpenVrHmd`/`OpenXrHmd` destructors that call `openvr::shutdown()`/`openxr::shutdown()`, and call
`openvr::shutdown()` on every failure after a successful `openvr::init`.

### R5. A failed OpenXR session leaves the game rendering in stereo with no output (risk)
`runtime/openxr_hmd.cpp:68-74`, `101-105`, `137-138`
`OpenXrHmd::init` succeeds on `init_system()` alone, so the device becomes `Ready` and later `Running`. If
`xr::init(device)` fails in `create_render_resources`, for example because of an adapter mismatch,
`xrCreateSession` failing or the headset sleeping, `session_` stays false. `submit_frame` then returns at
once, so the headset gets nothing and xrWaitFrame no longer paces the game. Meanwhile the game keeps rendering
both eyes at headset resolution into a desktop mirror. The only trace is a log line.
**Fix:** when session creation fails, drop the device back to `DeviceState::None` / `manager().device =
nullptr`, as `create_device` does for a failed `init()`. Otherwise retry session creation on later
`ConfigureRendering` calls.

### R6. `g_follow` may point at a freed FollowBehavior (risk, inherited from the original)
`stereo/game_hooks.cpp:150`, `198`, `639-648`
`g_follow` is set on activate and on deactivate, and is never cleared. `stereo_update` writes a byte at
`+0x264` through it every frame: when stereo is off (line 150) and when it is on (line 198), whether or not the
camera is still active. If the behaviour object is destroyed on a level unload, this is a one-byte heap write
into freed memory. The original (DAT_1006863c) has the same pattern, but the reconstruction does not need to
keep it.
**Fix:** in `follow_on_deactivate`, call the original first and then set `g_follow = nullptr`. Gate both writes
on `g_follow_active`. Better, hook the behaviour's destructor slot and clear the pointer there.

### R7. Dither shader patch drops the 0.001 factor the analysis says must stay; the header comment overstates it (risk)
`render/shader_patch.h:13-28`, `render/shader_patch.cpp:200-213`
The patch turns `mul rA.xy, vPos, l(0.001,0.001)` into `mul rA.xy, vPos, cb2[39].zw`, so `rA = n = pos/size`,
with no ×0.001. The `dp2` that follows then computes `d = (0.001·n − t)·n`. The documented GRAND fix
(`08d` §1.2) is `d = (0.001·n − t)·(0.001·n)`, and `08d` §1.4 says "The 0.001 factor must stay". Our `d` is
about 1000× larger, roughly `−t·(n.x+n.y)`. With tiled or negative texcoords beyond about |t|≈5, `2^d·10000`
passes 2^24 again and the dither vanishes or goes NaN, the original symptom, though now in both eyes alike.
The header line "which reproduces what the game computed on a screen about 1000 pixels wide" holds for `rA`
but not for the `mad` result.
**Fix:** confirm in game on a surface with large texcoords. If the GRAND math is wanted, grow the SHEX chunk by
one `mul rA.xy, rA.xyxx, l(0.001,0.001,0,0)`; this means rewriting the chunk size, the program length `t[1]`,
the following chunk offsets and the container size. Otherwise, at least correct the comment to the actual
formula.

### R8. The Present and CreatePixelShader hooks patch shared vtables and act on every swapchain and device in the process (risk)
`proxy/d3d11_proxy.cpp:99-112`, `174-183`, `271-305`; `stereo/present.cpp:31-59`
Vtable slots are process-wide per class. `hook_Present` calls `stereo::on_present(This)` for every DXGI
swapchain that presents, including one an in-process OpenXR runtime or overlay may own. On the OpenXR
virtual-screen path, that runs `xrWaitFrame` and `xrEndFrame` once per foreign Present and copies the wrong back
buffer.
**Fix:** remember the game's swapchain pointer in `install_swapchain_hooks` (the first one, created through our
export) and run `on_present` and the frame counter only when `This` matches.

### R9. Config values from `mothervr.json` are not validated (risk)
`config/config.cpp:120-136`; used by `stereo/stereo_device.cpp:384-387`, `runtime/openvr_hmd.cpp:167-172`,
`runtime/openxr_hmd.cpp:95-99`
`PixelScale` is taken as-is. A hand edit to 0, a negative number or 50 gives a zero, negative or enormous eye
target, and `CreateTexture2D` fails or the game runs out of memory. Only `mvr_pixelscale` clamps it, to
[0.5, 4] (`script_bridge.cpp:214`). `SnapRotationSegments` is stored as-is and only quantised when it is shown
in the menu.
**Fix:** apply one `config::sanitize(Config&)` after `read_config` and after every console command: clamp the
pixel scale with the same [0.5, 4] → 1.0 rule and quantise the segments. Have `script_bridge` call it instead
of repeating the rules.

### R10. `handle_mvr_command` truncates non-`mvr_` commands before forwarding them (risk, inherited)
`integration/script_bridge.cpp:165-171`
`strtok(cmd, " ")` writes a NUL after the first token, and then the same `cmd` goes to the game's handler. Any
game console or UI command with arguments loses them. The original does the same (`FUN_100202e0`), so this is
faithful, but the fix costs nothing.
**Fix:** test `std::strncmp(cmd, "mvr_", 4)` before tokenising, and forward untouched commands unmodified.

### R11. Patch primitives ignore failures (risk)
`hook/memory_patch.cpp:19-24`, `44-47`
The results of `VirtualProtectEx` and `WriteProcessMemory` are discarded. When the protect call fails, the
destructor "restores" protection 0. Every patch failure is therefore silent, and `install()` reports success.
**Fix:** return `bool` from `write_bytes`/`write_dword`, log `GetLastError()`, and propagate the result to
the R2 preflight/commit logic. Restore protection only if the change succeeded.

### R12. Gamepad virtual code lookup has no bounds check (risk, low)
`input/gamepad.cpp:136-143`
`original_code[code & 0xFF]` indexes a 0x39-entry array with values up to 0xFF. `tier_for` maps 0x300-0x3FF
to tier 1, where `& 0xFF` aliases onto 0x200-0x2FF. The game only queries codes we created (index < 0x39), so
this is safe today. A stale map, or the third map's unknown codes, would read past the tier, or past
`g_tiers` for tier 2, and pass garbage to the game's `IsDown`. `06d` (line 252) already asks the
reconstruction to bounds-check.
**Fix:** `const int k = code - t.base; return (k >= 0 && k < 0x39) ? t.original_code[k] : -1;`.

### R13. The "UI.PAK missing" message box is wrong, and is shown under the loader lock (risk)
`integration/payloads.cpp:82-87`
The text says "MotherVR will be disabled for this session", but `hook_installer.cpp:96-99` continues with VR
on. The `MessageBoxW` also runs inside `DllMain` (see R3).
**Fix:** change the text to "the MotherVR options page will be unavailable" and show it later, or only log it.

### R14. `MOTHER.PAK` staleness depends only on the DLL's build stamp (risk)
`integration/payloads.cpp:73-77`
If a game update or a language switch replaces `UI.PAK`, the old `MOTHER.PAK` keeps the current DLL stamp and
is reused. The game then loads UI built from the previous `UI.PAK`. The patch's CRC check (`ui_patch.cpp`)
only runs when the archive is rebuilt.
**Fix:** include `UI.PAK`'s size and last-write time in the stamp, for example in a sidecar
`MOTHER.PAK.src`, or always re-verify the CRCs, which is cheap compared with the rebuild.

### R15. OpenXR init ignores reference space failures (risk, low)
`runtime/openxr_backend.cpp:473-475`, `268-272`
`make_reference_space` returns `XR_NULL_HANDLE` on failure, but `init` still reports `ok`. Every later
`xrLocateSpace`/`xrLocateViews` then fails, and the frame submits no layer. `handle_state(READY)` logs
"session running" even when `xrBeginSession` failed.
**Fix:** treat a null `local`/`seated`/`view` as an init failure (teardown, return `ok=false`), and log
"running" only when `g.running` is true.

### R16. The OpenVR loader changes process-wide DLL search state (risk, low)
`runtime/openvr_backend.cpp:99`, `105`, `116`, `140`, `126`
`SetDllDirectoryW(bin_dir)` and then `SetDllDirectoryW(nullptr)` change the search path for the whole process
while other threads may be loading DLLs. Resetting to `nullptr` also discards any value set earlier by the
game or an overlay. The `vrclient.dll` handle preloaded at line 126 is never freed, including on the failure
paths.
**Fix:** use `AddDllDirectory` with `LoadLibraryExW(..., LOAD_LIBRARY_SEARCH_DEFAULT_DIRS |
LOAD_LIBRARY_SEARCH_USER_DIRS)` and remove the cookie afterwards. Alternatively, save and restore the previous
`GetDllDirectoryW` value. Free `hVrclient` on failure.

### R17. Job-thread reads of the device state are formally data races (risk, low)
`stereo/game_hooks.cpp:510-513`, `569-582`
The cull hooks run on job threads (the comment at 569-570 acknowledges this) and read `sm().device` and
`device->state` as plain loads while the main thread writes them. On x86 this is benign, but it is UB, and the
compiler may hoist the load out of a loop in a future refactor.
**Fix:** read through `std::atomic_ref<DeviceState>(dev->state).load(std::memory_order_relaxed)` and
`std::atomic_ref<StereoInterface*>`. Layout and ABI are unchanged.

---

## Cleanups

### Stale or wrong comments
- **C1** `hook/hook_installer.h:13-31`: the step list stops at 7. The "Not yet covered (TODO …)" block lists
  detours and byte patches that `hook_installer.cpp` now applies (steps 8, 8b, 9). "Returns false … or the UI
  archive could not be deployed" is wrong: `.cpp:96-99` carries on.
- **C2** `hook/ai_patches.h:30-35`, `hook/ai_patches.cpp:8-15`: "remaining ~20 patches … pending" and "~50
  patch writes … will land with their owning subsystems". They have landed, in `hook_installer.cpp` step 9.
- **C3** `dllmain.cpp:9-10` ("subsystems plug in later") and `:56` ("backend init wires in later") describe an
  earlier stage. `#include "runtime/openvr_backend.h"` (line 17) is unused.
- **C4** `runtime/vr_detect.cpp:1-13`: it lists OpenVR and an Oculus TODO but not OpenXR, and says backends
  start "lazily on first frame". They now start in `Stereo::CreateDevice`.
- **C5** `runtime/openxr_backend.h:6-10`: "all on the game's render thread … init() on the first Present …
  shutdown() at detach". In stereo-device mode, `init_system` runs in CreateDevice and `init` runs in
  ConfigureRendering. Nothing calls `shutdown()`.
- **C6** `integration/exe_check.h:19` ("the dxgi.dll proxy itself"), `proxy/d3d11_proxy.h:7-11` ("the one
  function AI.exe imports"; 50 forwards are resolved too), `proxy/d3d11_proxy.cpp:4` (points readers at an
  assistant memory file, not a repo document).
- **C7** `integration/script_bridge.cpp:44`: "Null-guards all four slots", but it checks three.
  `integration/script_bridge.h:64-70` (see B3).
- **C8** `recon/README.md:18,37,39`: the layout omits `render/`, `diag/texture_dump`, the OpenXR backend,
  `openxr-loader` in vcpkg.json and the stereo game hooks. `recon/test_load.ps1:1,7` still loads
  `build\Release\dxgi.dll`.
- **C9** `render/shader_patch.h:26`: see R7.

### Duplication
- **C10** There are four copies of "directory of this DLL": `dllmain.cpp:26`, `diag/log.cpp:25`,
  `runtime/openvr_backend.cpp:44` and `runtime/openvr_hmd.cpp:48`. The last one ignores `n == 0` and
  truncation. Expose one from `diag` or a `util` header and pass `game_dir` in.
- **C11** Knowledge of the input-manager layout is split between modules. `stereo/game_hooks.cpp:88-90,155-157`
  has its own `input_manager()` and the magic numbers `0x4A7C/0x4A74/0x4A70`, while `input/gamepad.cpp:85-88`
  names `kPadDevice/kKeyboardDevice` for the same object. Put the offsets and accessors in `input/` (e.g.
  `input::pad_device()`, `input::keyboard()`, `input::mouse()`).
- **C12** Touchpad decoding (`horiz`/`centre`/interact/reload/radial) is written three times:
  `runtime/openvr_hmd.cpp:315-344`, `runtime/openxr_hmd.cpp:192-206` and partly `input/vr_controls.cpp:33-41`.
  The constants `kPadDeadZone` (0.33, three copies), `kCinemaDistance`, `kCinemaWidth` and `kNearHead` (two
  copies each) also repeat. Move the decode into `input::decode_touchpad(stick, click)` and keep the constants
  in `vr_controls.h`.
- **C13** There are two patch-table mechanisms. `hook/ai_patches.cpp:47-56` hard-codes `{rva_2015, rva_2020}`
  pairs, while everything else uses the generated table plus `AiAddr`. Version-specific bytes appear inline at
  `hook_installer.cpp:310-312`, and a sibling call site is an offset (`LeftStickPromptCall + 0xEC`,
  line 212). Move the eight branch sites into `AiAddr`/`ReconRvas`, and give the second prompt call its own
  entry.
- **C14** `d3d11_exports.inc` and `d3d11.def` list the same 50 exports; they are in sync today (checked). Have
  CMake generate the `.def` from the `.inc`, or add a configure-time check.

### Dead code and over-exposed headers
- **C15** Unused: `hook::write_word` and the `write_bytes(span)` overload (`memory_patch.h:22,28`),
  `AiAddr::GameDeviceEyeViewport` (`ai_addresses.h:43`), `stereo::initialize`'s `ai_base` parameter,
  `openxr::haptic`, `set_screen_layout`, `instance()`, `session()`, `seated_space()`, the
  `HandState::aim/linear_velocity/angular_velocity` fields, `openvr::session_info::recommended_*/ipd_meters`
  (only logged) and `openvr::g_system` (queried again by `OpenVrHmd`). `render::dxbc_checksum` is exported
  from `shader_patch.h:37` but used only inside the `.cpp`.
- **C16** `hook/hook_installer.cpp:71-333` is one 260-line function. Split it into one function per step with a
  status each; this also gives R2's preflight a natural shape.

### Style and consistency
- **C17** `stereo/game_hooks.cpp:240-241`: `field<int32_t>(self, 0x6C)` reaches into `StereoManager::game_68`.
  Name the field in the struct (`std::int32_t stack_top; // +0x6C`). `if (a != b) a = b;` is just `a = b;`.
- **C18** Snap repeat (`game_hooks.cpp:170,181`) uses `std::clock()` and assumes `CLOCKS_PER_SEC == 1000`,
  while the head-clip delay uses `steady_clock`. Use `steady_clock` for both.
- **C19** `runtime/openvr_actions.cpp:178-183`: `handle(name)` does a linear `string_view` search on every one
  of the 14 per-frame lookups. Its state is split between globals (`g_input`, `g_handles`) and the `ready_`
  member. Index by an `enum class Action`, and keep the state in `Actions`.
- **C20** `stereo/game_hooks.cpp:365`: the clear-gate stub stays `PAGE_EXECUTE_READWRITE`. After writing it,
  `VirtualProtect` it to `PAGE_EXECUTE_READ`.
- **C21** `input/gamepad.cpp:125`: `g_glyphs` is a `std::vector<std::string>` whose `c_str()` is handed to
  the game, and `set_glyph` can reassign entries from the render thread. All names fit in SSO, so the pointers
  are stable today, but that is accidental. Use a fixed `std::array<const char*, 24>` of string literals.
- **C22** `hook/hook_installer.cpp:224-225`: the reflection fix checks only `*site == 0xE8`, while the AA,
  cull and head-clip fixes check the call target with `calls()`. It also installs the clear gate before it
  knows that the redirect succeeded. Use `calls(site, render)` first, as the other fixes do.

---

## Build hygiene
- Clean at `/W4` and with the extra warnings listed at the top; only C4191 remains, from `GetProcAddress`
  casts. Turn on `/WX` in CI, and silence C4191 locally with a `proc<T>(HMODULE, const char*)` helper that
  casts through `void*`.
- `CMakeLists.txt` (`LINK_FLAGS "/DEF:…"`): the `.def` is not a build dependency, so editing it does not
  relink, and `LINK_FLAGS` is deprecated. Add `src/d3d11.def` to `add_library` sources (CMake passes it to
  the MSVC linker) and drop the property.
- `CMakeLists.txt` (`find_path … HINTS "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/…"`): the vcpkg toolchain
  already puts the include directory on `CMAKE_PREFIX_PATH`. The hard-coded hint breaks with
  `VCPKG_INSTALLED_DIR` overrides. Drop `HINTS`.
- `_CRT_SECURE_NO_WARNINGS` is defined globally, which hides `strtok`, `strncpy` and `_wgetenv` warnings
  project-wide. Scope it, or use the `_s` forms.
- Includes: `dllmain.cpp` includes the unused `runtime/openvr_backend.h`. `diag/log.cpp` uses `std::memcpy`
  without `<cstring>`. `integration/script_bridge.cpp` uses `std::string`/`std::string_view` without
  including them; it compiles only through transitive includes.
- `dllmain.cpp:79-81`: `on_detach` ignores `lpReserved` and calls `FreeLibrary` on the real d3d11 during
  process termination, while game COM objects still live. When `lpReserved != nullptr`, return without
  cleanup.
- Hardening: the `uint32` sums `off + 8` and `off + 8 + bytes` in `render/shader_patch.cpp:150,154` can wrap
  on 32-bit. The checksum gate makes a malformed blob unlikely, but `std::size_t(off) + 8` costs nothing.

---

## Top 10 (by severity, then impact)
1. **B1** Index bindings: pause on left B, flashlight duplicated on left A (`openvr_actions.cpp:104-105`).
2. **B3** Pixel-scale and mirror changes never applied; the dirty flag is write-only and its comment is false
   (`script_bridge.*`). *Fixed at HEAD by `6051363`.*
3. **B2** Partly written `openvr_api.dll` stamped as current, so it is never repaired (`payloads.cpp:59`).
4. **R1** `path::string()` throws on non-ANSI paths inside `noexcept` code and `DllMain`, killing the game at
   launch.
5. **R2** Ignored `redirect_call` results and blind writes can leave the stereo hand-off half patched
   (`hook_installer.cpp:148-214`).
6. **R4** Backend lifecycle: re-entered `CreateDevice` re-inits OpenVR, an OpenVR session is leaked on a
   failed init, and nothing shuts down.
7. **R6** `g_follow` written every frame through a possibly freed pointer (`game_hooks.cpp:150,198`).
8. **R7** Dither patch omits the 0.001 factor `08d` requires; the comment claims otherwise.
9. **R5** A failed OpenXR session leaves the game in stereo with no headset output and no pacing.
10. **R3** `LoadLibraryEx`, PAK building, a registry probe and a `MessageBox` run under the loader lock.
