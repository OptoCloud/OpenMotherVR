# 09: How completely `recon/` covers MotherVR 0.8.1

Target: the original `dxgi.dll` (MotherVR 0.8.1, image base 0x10000000), compared with
`recon/src/**` at commit 4d242c7. This is analysis only; nothing in `recon/` was changed.

Confidence tags: **[sure]** read from the decompile or disassembly, **[likely]** strong inference,
**[guess]** plausible but unconfirmed.

## 0. Method and totals

* **Function universe.** `dumps/v2/functions.tsv` lists 2310 functions. It is a superset of the 1975 in
  `dumps/functions.tsv`: FillGaps adds 335 functions, mostly Catch funclets, Ghidra-split chunks and
  CRT pieces, and none of them is MotherVR logic. Every function of both lists has exactly one bucket.
  Classifying them used the call graph and string references parsed from `dumps/v2/all_functions.c`,
  the existing analyses (03..07; the 08* GRAND notes are about another DLL whose addresses collide),
  every `FUN_`/`0x` address cited in `recon/src`, the git log, spot disassembly and, for AI.exe-side
  questions, the installed 2015 `AI.exe` (timestamp 0x54E3199D).
* **Scripts** (scratchpad, not committed): `analyze.py` builds `graph.json` (callers, callees, strings,
  imports), and `cov_classify.py` assigns the buckets and makes the tables below.

| Bucket | v2 list (2310) | bytes | old list (1975) |
|---|---:|---:|---:|
| a. library / toolchain | 2125 | 250707 | 1812 |
| b. reconstructed | 145 | 39640 | 123 |
| c. deliberately replaced | 35 | 9443 | 35 |
| d. not reconstructed | 5 | 857 | 5 |

MotherVR's own code (b + c + d) comes to 185 functions and 49940 bytes. By bytes, 79.4% of it is
reconstructed, 18.9% is replaced and 1.7% is missing. Several reconstructed functions are **partial**:
bucket b marks a function as ported, but a few behaviours inside it are not, and section 4 lists them
(F1, F3, F4, F6, F7).

Address layout of the original **[sure]**:

* 0x10001000..0x10020e48: MotherVR code, interleaved with STL and nlohmann::json template instances
  (MSVC places them in the TU that uses them).
* 0x10020f30..0x100245b0 (plus 0x10024610..0x10024900): the LibOVR CAPI shim and OVR_CAPI_Util, compiled in.
* 0x100245e0..0x1004c5bd: UCRT/VCRuntime/STL, the delay-load helper, EH funclets, `__ehhandler$` thunks
  and atexit destructors.

## 1. Bucket a: library and toolchain code (2125 functions)

| Group | Address span | Functions | Bytes |
|---|---|---:|---:|
| dynamic initializers of globals (compiler-generated) | 0x10001000..0x10001709 | 10 | 251 |
| MSVC STL instantiations (std::string, std::wstring, std::vector) | 0x10001e00..0x10020f20 | 57 | 5444 |
| DirectX11::ReleaseDevice from the Oculus sample header Win32_DirectXAppUtil.h (dead code) | 0x10001f40..0x10001f40 | 1 | 324 |
| OpenVR header-inline code (COpenVRContext, VR_Init) | 0x100066c0..0x10006800 | 4 | 447 |
| MSVC STL (iostream, locale, system_error, filesystem) + nlohmann::json 3.4.0 templates | 0x10009340..0x1001d3c5 | 349 | 68453 |
| UCRT/VCRuntime inline helpers (sprintf_s, std::exception) | 0x1001d3d0..0x1001d4c0 | 6 | 220 |
| LibOVR 1.30 CAPI shim (OVR_CAPIShim.c, signature check) + OVR_CAPI_Util | 0x10020f30..0x10024900 | 31 | 14661 |
| UCRT / VCRuntime / STL library code (CRT startup, heap, stdio, locale, math, EH runtime) | 0x100245e0..0x10049fce | 1115 | 151134 |
| delay-load helper + VR_* delay-load thunks | 0x100249ac..0x10024ceb | 16 | 1476 |
| EH unwind funclets (compiler-generated) | 0x1004a180..0x1004c0f8 | 409 | 4130 |
| __ehhandler$ thunks (compiler-generated) | 0x1004a18c..0x1004c0b0 | 101 | 3085 |
| atexit destructors of globals (compiler-generated) | 0x1004c0dd..0x1004c5bd | 26 | 1082 |

The spans are min..max addresses. The groups interleave, so a span may contain functions of other groups.

Evidence per group:
* **CRT/VCRuntime/STL**: Ghidra FunctionID names (`__crt_strtox::*`, `std::locale::*`, `dllmain_*`,
  `__CxxThrowException`, ...), "Visual Studio 2015/2019 Release" banners, and the
  `_MSC_FULL_VER` 19.00.24210 that LibOVR's `ovr_ReportClientInfo` reports (VS2015 Update 3).
  The unnamed `FUN_` functions above 0x100245e0 are only called from named CRT code or from each other.
* **nlohmann::json 3.4.0**: the `[json.exception.` and lexer/parser error strings, the version
  fingerprint `961c151d2e87f2686a955a9be24d316f1362bf21 3.4.0`, and RTTI `nlohmann::detail::*`.
  The parser (0x10014dd0, 0x10015fc0), serializer (0x10011100), lexer (0x100115d0, 0x10012190..0x10013710)
  and `basic_json` members are all generic. `from_json(bool/number)` (0x1001bf40, 0x1001c000, 0x1001c150)
  is library code. The NLOHMANN_JSON_SERIALIZE_ENUM-generated enum (de)serialisers are MotherVR data and
  count as b.
* **STL instantiations in the mod range**: `std::string`/`std::wstring` growth, assign, find and substr
  ("string too long", "invalid string position"), `std::vector` growth ("vector<T> too long"),
  `vector::at`, and the iostream, locale and system_error categories. `MemMapBase::scalar_deleting_destructor`
  (0x1000a1f0) is a FunctionID false match for `std::_Ref_count_base`.
* **OpenVR header-inline code**: `COpenVRContext::VRSystem()/VROverlay()/Clear()` and the inlined `VR_Init`
  (0x100066c0, 0x10006730, 0x100067a0, 0x10006800). The VR_* delay-load thunks sit at 0x100249ac..0x100249ef.
* **LibOVR shim**: `OVR_CAPIShim.c`. 0x10020f30 builds the DLL name (`LIBOVR_DLL_DIR` environment variable,
  then `%lsLibOVRRT%hs_%d.dll` = `LibOVRRT32_1.dll`, `SearchPathW`). 0x10023ac0 and 0x10023bc0..0x10023e20
  load it only after an Authenticode check through `wintrust`/`crypt32` (signer CN "Oculus VR, LLC", DigiCert
  chain strings at 0x1004d280). 0x10021160 resolves 93 `ovr_*` symbols. 0x10023fc0..0x100245b0 are the
  per-symbol wrappers. 0x10024610..0x10024900 are `ovr_CalcEyePoses`/`ovr_GetEyePoses` from OVR_CAPI_Util.
* **Oculus sample header**: two never-constructed `DirectX11` helper objects (`static DirectX11 DIRECTX;`
  from OculusRoomTiny's `Win32_DirectXAppUtil.h`, included in two TUs). Their initializers are 0x10001080
  and 0x10001690, the atexit destructors 0x1004c200 and 0x1004c4b0, and `ReleaseDevice` is 0x10001f40, with
  `DestroyWindow` + `UnregisterClassW(L"App")`. Nothing creates the window. This is dead code, which also
  explains the `DestroyWindow`/`UnregisterClassW` imports without any `CreateWindow`.
* **Compiler-generated** code: the dynamic initializers 0x10001000..0x10001709, which construct mod globals
  such as the two `std::wstring`s at 0x100686a4/0x100686bc, the controls-help vector 0x100698e0 and the
  iostream init objects. Also the atexit destructors 0x1004c0dd..0x1004c5bd, including 0x1004c370, which
  runs the StereoManager destructor. The data they set up is reconstructed where it matters (b).
* RTTI names that grep finds in the file but that belong to no code here (`Json::*` from jsoncpp, ConcRT
  `Concurrency::*`) come from the embedded `openvr_api.dll` resource (0xC9). `MotherVR::vrRuntime` is the
  abstract base of `MotherVR::OpenVR`/`MotherVR::Oculus` (recon: `runtime::HmdBackend`).

## 2. Bucket b: reconstructed (145 functions)

| Original | Size | Reconstruction | What |
|---|---:|---|---|
| 10001090 | 1535 | `input/gamepad.cpp` | glyph-name table (g_glyphs) static initializer |
| 10001720 | 25 | `stereo/game_hooks.cpp` | follow_on_activate / follow_on_deactivate |
| 10001740 | 25 | `stereo/game_hooks.cpp` | follow_on_activate / follow_on_deactivate |
| 10001760 | 213 | `hook/ai_addresses.cpp` | init_ai_addresses (select + rebase table) |
| 10001840 | 1176 | `hook/ai_address_tables.inc` | kAiRvas2020 (2020 table initializer) |
| 10001ce0 | 6 | `stereo/stereo_device.cpp` | mgr_on_event (GraphicsSetOption thunk) |
| 10001cf0 | 6 | `input/gamepad.cpp + stereo/game_hooks.cpp` | thunks to the saved pad methods, used by stereo_update (pad_button / pad_axis_value / pad_activity) |
| 10001d00 | 6 | `input/gamepad.cpp + stereo/game_hooks.cpp` | thunks to the saved pad methods, used by stereo_update (pad_button / pad_axis_value / pad_activity) |
| 10001d10 | 6 | `input/gamepad.cpp + stereo/game_hooks.cpp` | thunks to the saved pad methods, used by stereo_update (pad_button / pad_axis_value / pad_activity) |
| 10001d20 | 6 | `input/gamepad.cpp + stereo/game_hooks.cpp` | thunks to the saved pad methods, used by stereo_update (pad_button / pad_axis_value / pad_activity) |
| 10001d30 | 6 | `stereo/game_hooks.cpp` | stereo_update: KeyboardKeyDown / KeyboardAnyKey / MouseActive thunks (game_fn) |
| 10001d40 | 6 | `stereo/game_hooks.cpp` | stereo_update: KeyboardKeyDown / KeyboardAnyKey / MouseActive thunks (game_fn) |
| 10001d50 | 6 | `stereo/game_hooks.cpp` | stereo_update: KeyboardKeyDown / KeyboardAnyKey / MouseActive thunks (game_fn) |
| 10001f20 | 3 | `stereo/stereo_device.cpp` | dev_eye_render_nop / mgr_release (shared no-op) |
| 10002090 | 164 | `hook/hook_installer.cpp` | kMotherPakPath / kSkipBootup (installer singleton strings) |
| 10002140 | 156 | `hook/hook_installer.cpp` | kMotherPakPath / kSkipBootup (installer singleton strings) |
| 100021e0 | 111 | `hook/hook_installer.cpp` | kMotherPakPath / kSkipBootup (installer singleton strings) |
| 10002250 | 552 | `dllmain.cpp + integration/exe_check.cpp` | DllMain / identify_ai_exe / this_module_build_filetime |
| 10002ef0 | 168 | `input/gamepad.cpp` | tiers() (three action-map tiers, magic statics) |
| 10002fa0 | 168 | `input/gamepad.cpp` | tiers() (three action-map tiers, magic statics) |
| 10003050 | 168 | `input/gamepad.cpp` | tiers() (three action-map tiers, magic statics) |
| 10003100 | 24 | `stereo/game_hooks.cpp` | is_move_on_left_stick |
| 10003130 | 117 | `input/gamepad.cpp` | pad-code lookup / refresh / remap |
| 100031b0 | 74 | `input/gamepad.cpp` | pad-code lookup / refresh / remap |
| 10003200 | 575 | `input/gamepad.cpp` | pad-code lookup / refresh / remap |
| 10003460 | 4 | `stereo/stereo_device.cpp` | dev_increment_frame / dev_is_ready / dev_position_tracked |
| 10003470 | 3 | `stereo/stereo_device.cpp` | dev_increment_frame / dev_is_ready / dev_position_tracked |
| 10003480 | 8 | `stereo/stereo_device.cpp` | dev_increment_frame / dev_is_ready / dev_position_tracked |
| 10003490 | 113 | `stereo/stereo_device.cpp` | manager() (StereoManager singleton) |
| 10003510 | 289 | `stereo/stereo_device.cpp` | initialize() (manager + device defaults) |
| 10003640 | 163 | `stereo/stereo_device.cpp` | initialize() (manager + device defaults) |
| 10004270 | 1775 | `input/vr_controls.cpp` | set_control_scheme(Thumbstick): the Oculus Touch glyphs + help text (Oculus slot 9) |
| 100055a0 | 137 | `input/vr_controls.cpp` | control_help() (help-line vector push_back) |
| 100056a0 | 519 | `runtime/openvr_hmd.cpp + stereo/game_hooks.cpp` | matrix -> quaternion (yaw extraction; recon uses atan2 directly) |
| 10005a40 | 277 | `runtime/hmd_backend.h` | to_game_pose (4x4 multiply) |
| 10006360 | 649 | `runtime/openvr_hmd.cpp + stereo/game_hooks.cpp` | quaternion -> Euler (yaw extraction; recon uses atan2 directly) |
| 100068b0 | 645 | `runtime/openvr_hmd.cpp + runtime/openvr_backend.cpp` | OpenVrHmd::init (OpenVR slot 0) |
| 10006b40 | 979 | `runtime/openvr_hmd.cpp` | get_hmd_desc (slot 1) |
| 10006f20 | 162 | `runtime/openvr_hmd.cpp` | eye_texture_size (slot 2) |
| 10006fe0 | 127 | `runtime/openvr_hmd.cpp` | create_render_resources (slot 3) |
| 10007060 | 120 | `runtime/openvr_hmd.cpp` | release_render_resources (slot 4) |
| 100070e0 | 319 | `runtime/openvr_hmd.cpp` | head_pose (slot 5) |
| 10007220 | 736 | `runtime/openvr_hmd.cpp` | head_yaw_pose (slot 6) |
| 10007500 | 1483 | `runtime/openvr_hmd.cpp + input/vr_controls.cpp` | register_control_hints (slot 9), Touchpad scheme glyphs + help |
| 100085c0 | 329 | `runtime/openvr_hmd.cpp` | begin_frame (slot 7) |
| 10008710 | 1467 | `runtime/openvr_hmd.cpp` | submit_frame (slot 8) |
| 10008cd0 | 114 | `runtime/hmd_backend.h` | identity-matrix init (inlined in recon) |
| 10008d50 | 199 | `input/gamepad.cpp` | pad hooks IsDown / GetValue / GetAxis / ControlToName / HasActivity |
| 10008e20 | 223 | `input/gamepad.cpp` | pad hooks IsDown / GetValue / GetAxis / ControlToName / HasActivity |
| 10008f00 | 149 | `input/gamepad.cpp` | pad hooks IsDown / GetValue / GetAxis / ControlToName / HasActivity |
| 10008fa0 | 132 | `input/gamepad.cpp` | pad hooks IsDown / GetValue / GetAxis / ControlToName / HasActivity |
| 10009030 | 118 | `input/gamepad.cpp` | pad hooks IsDown / GetValue / GetAxis / ControlToName / HasActivity |
| 1000b560 | 3163 | `config/config.cpp` | write_config (to_json) |
| FUN_1000c1d0 (+37 chunks/funclets 0x1000c2b7..0x1000ce6e) | 3243 | `config/config.cpp` | read_config (from_json, one try/catch per key keeps its default); Ghidra split it into chunks + Catch_All funclets |
| 1000ce80 | 521 | `config/config.cpp` | initialize (load + rewrite); first-run welcome box NOT ported |
| Catch_All@1000d089 1000d089 | 6 | `config/config.cpp` | initialize (load + rewrite); first-run welcome box NOT ported |
| 1000d08f | 6 | `config/config.cpp` | initialize (load + rewrite); first-run welcome box NOT ported |
| 1000d095 | 42 | `config/config.cpp` | initialize (load + rewrite); first-run welcome box NOT ported |
| 1000d150 | 353 | `config/config.cpp` | save / save_current |
| 10010750 | 184 | `config/config.cpp` | read_document (open mothervr.json) |
| 10014680 | 450 | `config/config.cpp` | config path ("mothervr.json", relative to the CWD in the original) |
| 1001bc60 | 362 | `config/config.cpp` | enum (de)serialisers: to_string / parse_vr_runtime / parse_mirror_view_type |
| 1001bdd0 | 362 | `config/config.cpp` | enum (de)serialisers: to_string / parse_vr_runtime / parse_mirror_view_type |
| 1001c6a0 | 396 | `config/config.cpp` | enum (de)serialisers: to_string / parse_vr_runtime / parse_mirror_view_type |
| 1001c830 | 396 | `config/config.cpp` | enum (de)serialisers: to_string / parse_vr_runtime / parse_mirror_view_type |
| 1001d4d0 | 103 | `hook/memory_patch.cpp` | write_bytes / write_dword |
| 1001d6f0 | 4142 | `hook/hook_installer.cpp + hook/ai_patches.cpp` | install() |
| 1001e720 | 533 | `integration/payloads.cpp + integration/ui_patch.cpp` | deploy_mother_pak (archive now built from the player UI.PAK + delta) |
| 1001e940 | 384 | `integration/payloads.cpp` | deploy_openvr_api |
| 1001eac0 | 309 | `stereo/stereo_device.cpp` | init_device |
| 1001ecf0 | 85 | `runtime/openvr_hmd.cpp` | make_openvr_backend |
| 1001ed50 | 38 | `stereo/stereo_device.cpp` | dev_release |
| 1001ed80 | 14 | `stereo/stereo_device.cpp` | dev_start_sensor |
| 1001ed90 | 14 | `stereo/stereo_device.cpp` | dev_stop_sensor |
| 1001eda0 | 1283 | `stereo/stereo_device.cpp` | dev_configure_rendering + mirror_constants |
| 1001f2b0 | 14 | `stereo/stereo_device.cpp` | dev_shutdown_rendering |
| 1001f2c0 | 52 | `stereo/stereo_device.cpp` | dev_head_pose |
| 1001f300 | 40 | `stereo/stereo_device.cpp` | dev_recenter_pose |
| 1001f330 | 179 | `stereo/stereo_device.cpp` | dev_begin_frame |
| 1001f3f0 | 316 | `stereo/stereo_device.cpp` | dev_end_frame (+ mirror draw) |
| 1001f530 | 264 | `stereo/stereo_device.cpp` | dev_render_target_size |
| 1001f640 | 10 | `stereo/stereo_device.cpp` | dev_hmd_resolution |
| 1001f650 | 10 | `stereo/stereo_device.cpp` | dev_display_name |
| 1001f660 | 6 | `stereo/stereo_device.cpp` | slot 12: tail jump to the game EyeTransform (kept) |
| 1001f670 | 6 | `stereo/stereo_device.cpp` | slot 13: tail jump to the game FovAspect (kept) |
| 1001f680 | 6 | `stereo/stereo_device.cpp` | slot 14: tail jump to the game EyeScaleOffset (kept) |
| 1001f690 | 6 | `stereo/stereo_device.cpp` | slot 15: was a tail jump to the game; now own dev_eye_viewport |
| 1001f6a0 | 83 | `stereo/stereo_device.cpp` | create_device |
| 1001f700 | 293 | `stereo/stereo_device.cpp + runtime/vr_detect.cpp` | world_scale_from_command_line; only -vrforceopenvr (partial) |
| 1001f830 | 109 | `stereo/game_hooks.cpp` | ui_on_message |
| 1001f8a0 | 887 | `stereo/game_hooks.cpp` | stereo_update |
| 1001fc20 | 930 | `stereo/game_hooks.cpp` | place_virtual_screen |
| 1001ffd0 | 513 | `stereo/game_hooks.cpp` | place_hud_panel |
| 100201e0 | 59 | `stereo/game_hooks.cpp` | push_mode |
| 10020220 | 15 | `stereo/game_hooks.cpp` | pop_mode |
| 10020230 | 91 | `stereo/stereo_device.cpp` | mgr_on_event |
| 10020290 | 5 | `stereo/stereo_device.cpp` | mgr_on_event2 |
| 100202a0 | 63 | `stereo/stereo_device.cpp` | get_render_target_size |
| 100202e0 | 1418 | `integration/script_bridge.cpp` | handle_mvr_command |
| 10020870 | 996 | `integration/script_bridge.cpp` | push_settings_to_game (incl. the addMotherViewControl loop) |
| 10020c60 | 19 | `integration/script_bridge.cpp` | on_menu_open |
| 10020c80 | 30 | `stereo/game_hooks.cpp` | menu_close |
| 10020ca0 | 65 | `stereo/game_hooks.cpp` | pause_open |
| 10020cf0 | 30 | `stereo/game_hooks.cpp` | pause_close |
| 10020d10 | 77 | `stereo/game_hooks.cpp` | loading_open |
| 10020d60 | 51 | `stereo/game_hooks.cpp` | loading_close |
| 10020da0 | 45 | `stereo/game_hooks.cpp` | attract_open |
| 10020dd0 | 22 | `stereo/game_hooks.cpp` | attract_close |

Partial ports (details in section 4): 0x10002250 lacks the `-loadcatch` prompt and the "Exe check failed"
dialog (F3, F8). 0x1000ce80 lacks the first-run welcome box and the rewrite on every start (F3, F7).
0x100068b0 lacks the `vrmonitor.exe` precondition and the error dialogs (F3, F6). 0x1001f700 lacks
`-vrforceoculus`/`-vrforcenone` (F4). 0x100202e0 and 0x10020870 do not raise the game's display-reset
flag (F1).

## 3. Bucket c: deliberately replaced (35 functions)

| Original | Size | Replaced by |
|---|---:|---|
| 100025b0 | 284 | proxy/d3d11_proxy.cpp: loads the system d3d11.dll and resolves its exports (was: GetProcAddress of the 20 system dxgi exports) |
| ApplyCompatResolutionQuirking | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| CompatString | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| CompatValue | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| CreateDXGIFactory | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| CreateDXGIFactory1 | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| CreateDXGIFactory2 | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| DXGID3D10CreateDevice | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| DXGID3D10CreateLayeredDevice | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| DXGID3D10ETWRundown | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| DXGID3D10GetLayeredDeviceSize | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| DXGID3D10RegisterLayers | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| DXGIDumpJournal | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| DXGIGetDebugInterface1 | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| DXGIReportAdapterConfiguration | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| DXGIRevertToSxS | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| PIXBeginCapture | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| PIXEndCapture | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| PIXGetCaptureState | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| SetAppCompatStringPointer | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| UpdateHMDEmulationStatus | 6 | proxy/d3d11_exports.inc + d3d11.def: d3d11 forwarders (was: a dxgi forwarder export) |
| 10003120 | 10 | omitted: GetControlPromptName pass-through hook, no behavioural effect |
| 100036f0 | 183 | none: the StereoManager is a static object never destroyed (original released the SI mirror COM objects at exit) |
| 100037b0 | 301 | OpenXR backend (runtime/openxr_hmd.cpp, openxr_backend.cpp): Oculus slot 0 Init |
| 100038e0 | 261 | openxr_hmd.cpp get_hmd_desc: Oculus slot 1 |
| 100039f0 | 83 | openxr_hmd.cpp eye_texture_size: Oculus slot 2 |
| 10003a50 | 624 | openxr_hmd.cpp / openxr_backend.cpp swapchains: Oculus slot 3 |
| 10003cc0 | 18 | openxr_hmd.cpp: Oculus slot 4 |
| 10003ce0 | 623 | openxr_hmd.cpp head_pose: Oculus slot 5 |
| 10003f50 | 798 | openxr_hmd.cpp head_yaw_pose: Oculus slot 6 (the original also calls ovr_RecenterTrackingOrigin) |
| 10004960 | 2438 | runtime/openxr_input.cpp + input/vr_controls.cpp: Oculus slot 10 PollInput |
| 100052f0 | 508 | openxr_hmd.cpp begin_frame: Oculus slot 7 |
| 100054f0 | 171 | openxr_hmd.cpp submit_frame: Oculus slot 8 |
| 10007ad0 | 2788 | runtime/openvr_actions.cpp (SteamVR Input); the legacy reading survives as OpenVrHmd::legacy_controls: OpenVR slot 10 PollInput |
| 1001ec00 | 233 | make_openxr_backend: Oculus constructor (0x5b8 bytes) |

Notes:
* The proxy pivot (dxgi to d3d11) is the Win11 workaround. The original's 20 exports were pure
  `jmp [IAT]` forwarders. Nothing in MotherVR hooked DXGI itself.
* The Oculus class is replaced as a whole by the OpenXR backend: the menu's "Oculus" entry was relabelled
  OpenXR in a6950f6. Its control hints (slot 9, 0x10004270) and its Touch button mapping survive as data in
  `input/vr_controls.cpp`, so 0x10004270 counts as b. The LibOVR-specific behaviours that OpenXR does not
  reproduce are listed under F5 with a port spec.

## 4. Bucket d: not reconstructed (5 functions) and missing behaviours

| Original | Size | Feature |
|---|---:|---|
| 10001d60 | 157 | audio endpoint redirect: IMMDevice::Activate replacement (F2) |
| 10002480 | 293 | -loadcatch: is the parent process Steam.exe (F8) |
| 1001d540 | 100 | MessageBoxW helper, caption "MotherVR 0.8.1" (F3) |
| 1001d5b0 | 124 | MessageBoxA helper, caption "MotherVR 0.8.1" (F3) |
| 1001d630 | 183 | audio endpoint redirect: installs the IMMDevice::Activate vtable hook (F2) |

The five bucket-d functions are small. Most of what is missing is **behaviour inside functions that
are otherwise ported**, or behaviour of the replaced Oculus backend. Features ranked by importance for
faithful behaviour:

### F1. Game display-reset flag on settings push (DAT_10066d44): HIGH, verify in game
* **What.** `FUN_10020870` (push settings to the menu) writes `*DAT_10066d44 = 1`, between
  `setMotherClipBlinding` and `setMotherSnapRotation`. `FUN_100202e0` writes it as well, after
  `mvr_mirrorview`, `mvr_mirrorviewsym` and `mvr_pixelscale`. DAT_10066d44 is AI.exe table slot **0x5F**:
  RVA 0x01358470 (2015), 0x01368620 (2020), a byte in AI.exe `.data`. **[sure]**
* **Meaning in AI.exe** (2015 VA 0x1758470) **[likely]**: the renderer's "display changed" flag. The
  window-resize handler (0xa11240) and the restore-from-minimised path (0xa11cb0, after `ShowWindow(SW_RESTORE)`)
  set it. The frame loop at 0x7903b2 tests it and then runs a render-system reset: it re-queries sizes,
  calls 0xa13f90 on the stereo/display object 0x17583e8, sizes the stereo UI targets through 0x9ccc50 with
  half of [0x17588c4]/[0x17588c8], rebuilds reflection targets through 0x99ab30, and so on.
* **Effect in the original.** Every front-end, pause or loading-screen open, and every change of pixel
  scale or mirror mode, makes the game rebuild its render targets. That is how a PixelScale change from the
  menu takes effect without a restart, and how the mirror re-derives its letterbox.
* **Recon.** `script_bridge.cpp` raises its own `g_stereo_settings_dirty`, which **nothing reads**. The game's
  flag is never set, so a PixelScale change made in the menu probably does nothing until the game resizes
  or restarts **[likely; verify]**. Port: add `AiAddr::DisplayResetFlag = 0x5F` and write 1 where the
  original does. Check in game that the reset does not undo the recon's "full resolution from the start"
  fix (799bafb). It should not: the reset asks `GetRenderTargetSize` again, and the recon answers with the
  configured scale.

### F2. Audio endpoint redirect to the headset: MEDIUM (Rift/Quest users)
* **Functions.** `FUN_1001d630` (installer step 4) and `FUN_10001d60` (hook).
* **Install** (called from `FUN_1001d6f0` right after config load, inside DllMain) **[sure]**:
  `CoInitialize(NULL)`, then `CoCreateInstance(CLSID_MMDeviceEnumerator {BCDE0395-E52F-467C-8E3D-C4579291692E},
  CLSCTX_ALL=0x17, IID_IMMDeviceEnumerator {A95664D2-9614-4F35-A746-DE8DB63617E6})`, then
  `GetDefaultAudioEndpoint(eRender=0, eConsole=0, &dev)` (vtable +0x10). It saves `dev->vtbl[3]`
  (`IMMDevice::Activate`) in DAT_10068644 and overwrites that slot of the **shared MMDevAPI vtable** with
  `FUN_10001d60`, through `FUN_1001d4d0`. It releases `dev` and the enumerator. On any failure:
  `MessageBoxW("Failed to hook Audio!\n[0x%X]", GetLastError(), MB_ICONERROR)`.
* **Hook** `HRESULT __stdcall Activate(IMMDevice* self, REFIID, DWORD clsctx, PROPVARIANT*, void** out)`:
  if the wide string at 0x100686a4 (an MSVC `std::wstring`: length at 0x100686b4, capacity at 0x100686b8)
  is not empty, it creates a new enumerator, calls `GetDevice(id, &other)` (vtable +0x14), and calls the
  saved original Activate with `self = other`. Otherwise it passes through. Neither the new enumerator
  nor `other` is released (a small leak per call).
* **Data source.** Only the Oculus backend fills the string. In `FUN_100037b0` (slot 0), after `ovr_Create`,
  it calls `ovr_GetAudioDeviceOutGuidStr(WCHAR[128])`, assigns the string, then calls AI.exe table slot
  **0x57** (RVA 0x831B00 2015 / 0x8473D0 2020) **[likely: audio engine re-initialisation]**. At 2015 VA
  0xC31B00 that is `mov byte [0x1bf9761],1; call 0xc2b330; ... jmp 0xc5dc40` on `[0x1bf9678]+0x88`.
  Under OpenVR the string stays empty and the hook is inert.
* **Effect.** The game's WASAPI output (Wwise) opens on the HMD's headphones even when Windows' default
  device is something else.
* **Port.** OpenXR can supply the ID through `XR_OCULUS_audio_device_guid`
  (`xrGetAudioOutputDeviceGuidOculus`, a WCHAR[128] in the same format) on Meta runtimes. SteamVR keeps the
  chosen device in its settings (`audio` section, playback device) **[guess]**. Install the hook outside
  DllMain if possible: `CoInitialize` in DllMain is fragile. Then call slot 0x57 after setting the ID.

### F3. User-facing message boxes: MEDIUM-LOW (UX)
Helpers: `FUN_1001d540` (`MessageBoxW`) and `FUN_1001d5b0` (`MessageBoxA`, which returns the button).
Both format with `vsnprintf_s` into a 1024-character buffer and use the caption **"MotherVR 0.8.1"**.
Recon shows only the UI.PAK one (in `payloads.cpp`, caption "MotherVR"); every other case is a log line.

| Where | Flags | Text (abridged) | Trigger |
|---|---|---|---|
| 0x10002250 | 0x41 OKCANCEL+INFO | "DLL load catch!" | `-loadcatch` (F8); Cancel aborts startup |
| 0x10002250 | 0x10 | "Failed to load System dxgi.dll! [%s] ... will now exit." | n/a in the d3d11 proxy |
| 0x10002250 | 0x30 WARNING | "Exe check failed! [0x%X] ... MotherVR will now be disabled ..." | AI.exe timestamp not 0x54E3199D or 0x5FD25676 |
| 0x1000ce80 | 0x41 | "Welcome to MotherVR for Alien: Isolation! To Enable VR ... Options->MotherVR ... only shown on the first launch" | `_stat("mothervr.json")` in the CWD fails; button ignored |
| 0x1001d630 | 0x10 (W) | "Failed to hook Audio! [0x%X]" | F2 failure |
| 0x1001e720 | 0x10 (W) | "Failed to find UI.PAK! [%s] ... will now exit." | ported (payloads.cpp) |
| 0x100068b0 | 0x10 (W) | "Failed to load openvr_api.dll! [%s] ... VR disabled ..." | `LoadLibraryW(<cwd>\openvr_api.dll)` fails |
| 0x100068b0 | 0x10 (W) | "SteamVR not running! [vrmonitor.exe] ... launch SteamVR beforehand ..." | F6 |
| 0x100068b0 | 0x10 | "Failed to initialize SteamVR! [%s / %d] ... SteamVR Desktop Game Theatre ..." | `VR_Init` error 0x75 |
| 0x100068b0 | 0x10 | "Failed to initialize SteamVR! [%s / %d] ..." | any other `VR_Init` error |
| 0x100037b0 | 0x10 | "Failed to initialize Oculus! [%s / %d] Make sure the Oculus runtime is installed ..." | `ovr_Initialize` < 0 (text from `ovr_GetLastErrorInfo`) |
| 0x100037b0 | 0x10 | "Failed to initialize Oculus! [%s / %d] Look into the above error ..." | `ovr_Create` < 0, then `ovr_Shutdown` |

(The original strings use `|` where this table shows `/`.)

The welcome box matters most for fidelity. Its first-run semantics: the original's default runtime is
**None** **[sure: FUN_1000ce80 default block, cfg+0x2ec = 0]**, so the box is what tells a new user how to
turn VR on. Recon defaults to OpenVR and shows nothing.

### F4. Runtime-forcing command-line switches: LOW
`FUN_1001f700` (called from `FUN_1001f6a0`, i.e. `Stereo::Init`) walks `CommandLineToArgvW` with exact,
case-insensitive `_wcsicmp`. `-vrforceoculus`, `-vrforceopenvr` and `-vrforcenone` set the force flag
(cfg+0x2e4 = 1) and the forced runtime (cfg+0x2e8 = 1, 2 or 0). `-vrscale <x>` takes the next argv,
parsed with `_wtof` (the call target differs from the `atof` used for `mvr_pixelscale`), and falls back to
1.0 outside [0.495, 1.505]. `FUN_1001eac0` then uses `force ? forced : configured`, with **no fallback** to
another runtime. Recon handles only `-vrforceopenvr` (substring match, and only if an OpenVR runtime is
detected) and `-vrscale` (substring + `wcstod`). `-vrforcenone` (start flat whatever the config says) and
`-vrforceoculus` (could map to OpenXR) are missing. Side effect in the original: the installer deploys
`openvr_api.dll` only when the *configured* runtime is 2, so `-vrforceopenvr` with another configured
runtime relies on an existing `openvr_api.dll`.

### F5. Native LibOVR backend (`MotherVR::Oculus`, vtable 0x1005dddc): LOW, replaced by OpenXR (c)
Port spec, in case a native LibOVR path or LibOVR-equivalent behaviour on OpenXR is wanted.

* **LibOVR version: 1.30** **[likely]**. The shim's default init params are
  `{Flags = ovrInit_RequestVersion (4), RequestedMinorVersion = 30, LogCallback = 0, UserData = 0,
  ConnectionTimeoutMS = 0}`, with minimum minor 17, and it reports `ovr_ReportClientInfo(19.00.24210, 1, 1,
  30, 0, 0)`. The runtime DLL is `LibOVRRT32_1.dll`. The shim knows 93 symbols, including
  `ovr_GetRenderDesc2`, `ovr_SubmitFrame2`, `ovr_GetDevicePoses`, `ovr_GetFovStencil` and the Vk set.
* **ovr_* functions MotherVR calls**, through shim wrappers: `ovr_Initialize` (0x100243b0),
  `ovr_Shutdown` (0x10024570), `ovr_GetLastErrorInfo` (0x10024150), `ovr_Create` (0x10023fe0),
  `ovr_GetAudioDeviceOutGuidStr` (0x10024040), `ovr_GetHmdDesc` (0x100240b0),
  `ovr_SetTrackingOriginType` (0x10024550), `ovr_RecenterTrackingOrigin` (0x10024530),
  `ovr_GetFovTextureSize` (0x10024060), `ovr_CreateTextureSwapChainDX` (0x10024000),
  `ovr_GetTextureSwapChainLength` (0x100242e0), `ovr_GetTextureSwapChainBufferDX` (0x10024280),
  `ovr_GetTextureSwapChainCurrentIndex` (0x100242c0), `ovr_CommitTextureSwapChain` (0x10023fc0),
  `ovr_DestroyTextureSwapChain` (0x10024020), `ovr_GetPredictedDisplayTime` (0x10024180),
  `ovr_GetTrackingState` (0x10024320), `ovr_GetRenderDesc` (= `ovr_GetRenderDesc2`, 0x100241b0),
  `ovr_GetEyePoses` (OVR_CAPI_Util 0x10024900, which uses `GetPredictedDisplayTime`, `GetTrackingState`,
  `ovr_CalcEyePoses` 0x100247f0 and `ovr_GetTimeInSeconds` 0x10024300), `ovr_SubmitFrame`
  (= `ovr_SubmitFrame2`, 0x100245b0), `ovr_GetSessionStatus` (0x10024250) and `ovr_GetInputState`
  (0x10024120). Inside the shim: `ovr_ReportClientInfo`.
* **Object.** 0x5b8 bytes, constructor `FUN_1001ec00` (memset; +0x49c = 1.0f). `+0x4/+0x8`: the game's
  ID3D11Device/Context (from slot 3). `+0xc`: ovrSession. `+0x10`: LUID (ignored). `+0x18`: ovrHmdDesc
  (0x100 bytes). `+0x118`: symmetric render FOV. `+0x128`: texture sizes. `+0x148`: ovrLayerEyeFov.
  `+0x258`: ovrLayerQuad. `+0x318`: layer pointer. `+0x31c`: swap chain. `+0x320`: swap-chain desc.
  `+0x348`: `std::vector<ID3D11RenderTargetView*>`. `+0x358`: last tracking state. `+0x4ac`: eye render
  descs. `+0x53c`: eye render poses. `+0x5b0`: sensor sample time.
* **Slot 0 Init** (`FUN_100037b0`): `ovr_Initialize(NULL)`, then `ovr_Create(&session, &luid)`, then
  `ovr_GetAudioDeviceOutGuidStr`, which feeds F2 plus AI.exe slot 0x57. Errors produce the two F3 dialogs
  and VR stays disabled (the object leaks).
* **Slot 1 GetHmdDesc** (`FUN_100038e0`): `ovr_GetHmdDesc` is copied to +0x18, then
  `ovr_SetTrackingOriginType(session, ovrTrackingOrigin_EyeLevel = 0)` and `ovr_RecenterTrackingOrigin`.
  Returns `eyeFov` = DefaultEyeFov and `renderFov` = the symmetric max of up/down and of left/right over
  both eyes, `productName` = desc.ProductName and `manufacturer` = desc.Manufacturer.
* **Slot 2** (`FUN_100039f0`): `ovr_GetFovTextureSize(session, eye, renderFov, pixelScale)`, cached per eye.
* **Slot 3** (`FUN_10003a50`): one side-by-side swap chain `{Type 2D, Format 7
  (OVR_FORMAT_B8G8R8A8_UNORM_SRGB [likely]), ArraySize 1, W = w0 + w1, H = max(h0, h1), Mips 1, Samples 1,
  Static false, MiscFlags 0, BindFlags 1 (DX_RenderTarget)}`, plus one RTV per buffer (they appear unused).
  The EyeFov layer: Header.Type = 1, viewports `{0,0,W/2,H}` and `{W/2,0,W/2,H}`, Fov = renderFov for both.
  The Quad layer (menus and cinema): Header.Type = 3, Flags 0 (world-locked), viewport `{0, H/4, W/2, H/2}`
  (the left eye's middle half, the same crop as the OpenVR overlay), pose orientation (0,0,0,1) at
  (0, 0, -1.5) m, size 3.5556 x 2.0 m.
* **Slot 4** (`FUN_10003cc0`): `ovr_DestroyTextureSwapChain`.
* **Slot 5 GetHeadPose** (`FUN_10003ce0`): `t = ovr_GetPredictedDisplayTime(session, 0)`, then
  `ovr_GetTrackingState(session, t, latencyMarker = 1)`. The head quaternion becomes a rotation matrix,
  with the position, and goes through the same `diag(-1, -1, 1, -worldScale)` transform as OpenVR slot 5
  (see 06b).
* **Slot 6 GetHeadYawPose** (`FUN_10003f50`): calls **`ovr_RecenterTrackingOrigin` first**, then reads
  the tracking state and builds a yaw-only pose (quaternion to Euler, half-angle). Every game recenter
  (LB+RB, Ctrl+Alt, the menu's reset view) therefore re-zeroes the Oculus origin too.
* **Slot 7 BeginFrame(eyeOffsets[6], headTracked)** (`FUN_100052f0`): `ovr_GetRenderDesc` for both eyes;
  writes the eye offsets from `HmdToEyePose.Position`. If tracked: `ovr_GetEyePoses(session, 0, true,
  HmdToEyePose[2], RenderPose, &SensorSampleTime)` into the EyeFov layer and select it; otherwise select
  the Quad layer. Both layers point at the one swap chain.
* **Slot 8 SubmitFrame(tex)** (`FUN_100054f0`): current index, then `GetTextureSwapChainBufferDX(IID_ID3D11Texture2D)`,
  `ctx->CopyResource(buffer, tex)`, `ovr_CommitTextureSwapChain` and
  `ovr_SubmitFrame(session, 0, NULL, &layer, 1)`.
* **Slot 9** (`FUN_10004270`): the Touch glyphs (A to VR_A, B to VR_RSC, X to VR_B, Y to VR_Y, BACK to
  VR_X, START to VR_MENU, LB to VR_LG, RB to VR_RG) and the help lines. **Ported** (`vr_controls.cpp`,
  Thumbstick).
* **Slot 10 PollInput** (`FUN_10004960`): `ovr_GetSessionStatus` first:
  * **ShouldRecenter**: set SM+0x31d (the stereo update then recenters and re-places the screen) and call
    `ovr_RecenterTrackingOrigin`.
  * else **ShouldQuit**: set AI.exe slot 0x64 (`*DAT_10066d58 = 1`, the game's quit-request byte, read by
    the getter at 2015 0xa14760 **[likely]**) and return.
  * else **!HasInputFocus**: return without touching the tiers, so the controls stay stale (a known
    original bug).
  
  Then `ovr_GetInputState(session, ovrControllerType_Touch = 3)`, mapped as in 06d section 4.3; the mapping
  is ported in `vr_controls.cpp`.
* **What the OpenXR replacement does not reproduce:** F2 (audio); ShouldQuit quitting the game (recon
  disables VR on `EXITING`/`LOSS_PENDING` and the game keeps running); a recenter request from the runtime
  setting SM+0x31d (recon re-anchors its own space on `REFERENCE_SPACE_CHANGE_PENDING` but does not
  re-place the virtual screen through the stereo update) **[likely]**; the EyeLevel origin (recon uses LOCAL);
  and the Oculus-specific error dialogs.

### F6. OpenVR init preconditions: LOW (some were deliberately dropped)
`FUN_100068b0`: it loads `<cwd>\openvr_api.dll` (the deployed 1.0.10). It **refuses to start VR unless
`vrmonitor.exe` is already running** (a Toolhelp32 process scan, with the F3 dialog). It then spins
**without limit**, with `Sleep(50)`, until `IVRCompositor_021` exists, and calls `WaitGetPoses` until the
HMD pose is non-zero. The recon bounds both waits (200 tries), skips the process check, uses the header's
interface versions and logs instead of showing dialogs.

### F7. Config persistence details: LOW
`FUN_1000ce80` always reloads and then **rewrites** `mothervr.json` at every start, through `FUN_1000d095`
and `FUN_1000d150`, which normalises the file. It resolves the path relative to the **current directory**
(`FUN_10014680`). Recon writes only when the file is missing, uses the DLL's directory, keeps unknown keys
and accepts the legacy and integer forms.

### F8. `-loadcatch`: VERY LOW (developer aid)
If `argv[1]` is `-loadcatch` (case-insensitive) and the **parent** process is `Steam.exe`
(`FUN_10002480`: Toolhelp32 walk, own PID, then parent PID, then compare `szExeFile` case-sensitively),
it shows `MessageBoxA(0x41, "DLL load catch!")`. That pauses DllMain so a debugger can attach. Cancel
returns from DllMain before the system dxgi.dll is loaded, which leaves the forwarders null **[likely:
crashes the game]**.

### F9. StereoManager destructor at exit: NEGLIGIBLE
`FUN_100036f0`, run by atexit `0x1004c370`, releases the D3D/COM pointers the manager holds at +0x21c..+0x22c.
Recon's manager is a never-destroyed static object.

### F10. GetControlPromptName pass-through (table slot 0x7D, site 2015 RVA 0x43AB40): NONE
`FUN_10003120` only forwards to the original 0x82cff0 (`DAT_10066c1c` slot 0x15). Recon leaves the call
alone, with identical behaviour.

**Not present in the original at all** **[sure]**: background threads (no `CreateThread`/`_beginthread`
anywhere in the mod code; the CRT's ConcRT strings belong to the embedded openvr_api.dll), registry
access (no ADVAPI32 import; the LibOVR shim uses an environment variable and `SearchPathW`), timers
(only `clock()` for the 301 ms snap-turn auto-repeat), and logging (no `OutputDebugString`, no log file).
File writes in the original are `DATA\MOTHER.PAK` (file mapping + `SetFileTime` stamp),
`<cwd>\openvr_api.dll` (runtime 2 only) and `mothervr.json`.

## 5. Where the reconstruction knowingly deviates (for review)

From the git log (up to 4d242c7) and the code comments:

1. **Proxy**: `d3d11.dll` instead of `dxgi.dll` (Windows 11 rejects local dxgi proxies). The CRT and the
   delay-load are gone: OpenVR is `LoadLibrary`'d from the DLL directory (the bundled 1.0.10) or else from
   SteamVR's `bin\win32`, with `SetDllDirectory`.
2. **Runtimes**: the menu's "Oculus" entry selects **OpenXR** (a6950f6, relabelled in the UI patch).
   LibOVR is not reconstructed (F5).
3. **Default runtime OpenVR** (3f1fd41; the original's is None), so there is no welcome box (F3).
4. **Runtime fallback**: `vr_detect` falls back to any available runtime when the configured one is
   missing. The original never falls back.
5. **Command line**: F4.
6. **Controls**: SteamVR Input with per-controller default bindings, written to `mothervr_input\` (a new
   file write) (1b293c8). The legacy reading is kept as a fallback. The glyphs and help follow the
   controller type, including an Index scheme with pause on the left trackpad press (not in the original).
7. **Recentering**: automatic when the headset is worn upright and whenever a loading screen closes
   (45c9f4b). The original recenters only at launch and on LB+RB / Ctrl+Alt / the menu.
8. **Resolution**: full pixel scale from the first `GetRenderTargetSize` call (799bafb). The eye layout
   follows the texture actually created (a79ab21). The swap chain is kept windowed and
   `SetFullscreenState` is refused while a headset is attached (8106c8b). Related: F1, the
   display-reset flag the recon no longer raises.
9. **Mirror**: `ScaleAdjust` is always 1.0 (the original used 1.0, then 0.5/pixelScale). The per-frame RTV
   leak in EndFrame is fixed.
10. **Device slot 15** (eye viewport) is ours, not the game's (9088b92), for per-eye reflections.
11. **Fixes the original lacks**, all 2015-build only and each guarded by a byte check: per-eye planar
    reflections, AA in stereo (SMAA T2x run as T1x), the culling fix, a 0.6 s head-in-wall delay and the
    pixel-shader dither patch (a `CreatePixelShader` hook).
12. **UI archive**: built at first run from the player's `UI.PAK` + a 177 KB delta (ce5c542), where the
    original embedded the full modded archive. If UI.PAK is missing or another version, the recon
    **continues without the options page**; the original showed the dialog and skipped *all* patching.
13. **Unknown AI.exe build**: a log line, no "Exe check failed" dialog.
14. **Config**: see F7. The added enum value `"OpenXR"` makes the original fall back to the default for
    that key if it ever reads a recon-written file. `mvr_vrruntime` accepts only 0..2 (the original stores
    any `atoi`). Like the original, it still sets AI.exe slot 0x64, which the recon calls
    `VrRuntimeChangedFlag` but which is the game's quit-request byte **[likely]**. A runtime change from the
    menu therefore quits the game in both.
15. **Settings dirty flag**: `g_stereo_settings_dirty` is written and never read (F1).
16. **Gamepad hooks**: bounds checks added where the original indexes unchecked (596e0ad).
17. **Logging** (`mothervr.log`) and the F10 texture dump (`diag/texture_dump`) are new.
18. **Stale comments** in `recon/src/hook/hook_installer.h`: it still lists the render detours and byte
    patches as "Not yet covered" (they are done), and claims install fails when the UI archive cannot be
    deployed (it carries on). `hook/ai_patches.cpp` says the FUN_1001d630 patches "will land with their
    owning subsystems" (the audio hook has not).
19. **Init order**: the original loads config inside the installer, after the address table and the SM
    singleton. Recon loads config first in `on_attach` and runs `vr_detect` before the hooks. There is no
    `CoInitialize` in DllMain (F2 dropped).
