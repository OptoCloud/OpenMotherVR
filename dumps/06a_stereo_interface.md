# 06a: `MotherVR::StereoInterface` and how it replaces the game's stereo device

Target: MotherVR 0.8.1 `dxgi.dll` (image base 0x10000000) and AI.exe 2015 build (image base 0x400000).
AI.exe addresses below are VAs (RVA + 0x400000) unless marked RVA.

## 0. Summary

* Alien: Isolation ships a hidden **LibOVR 0.4.0-era** stereo path. It has two classes:
  * **game stereo manager** (`new(0xb0)`, ctor `0x9d9a60`, vtable `0x13e57bc`, 3 slots). The global pointer is
    `[0x1766a2c]`; the owner also keeps it at `[esi+0xe68c]`.
  * **game stereo device** (`new(0x1f4)`, ctor `0x9be9c0`, vtable `0x13e5418`, **21 slots**). It wraps
    `ovrHmd` and lives at `manager+4`.
* MotherVR replaces **both**, using one static object:
  * `DAT_1006ac18` is `MotherVR::StereoManager` (0x324 bytes). Bytes `+0x00..+0xaf` mirror the game
    manager's layout. Its vtable is `0x1005a61c`, 3 slots.
  * `DAT_1006acc8` (= manager `+0xb0`) is the **embedded** `MotherVR::StereoInterface`
    (0x230 bytes). Bytes `+0x000..+0x1f3` mirror the game device's layout exactly. MotherVR extensions start at `+0x1f4`.
    Its vtable is `0x1005a5c0`: 22 slots, the game's 21 plus one extra (slot 21).
* Hook points that wire it in:
  * slot 0x35 (`RVA 0x5AFBA1`): `push 0xb0; ...` becomes `mov eax,0x1006ac18; jmp +0x1d`. The game stores
    `&StereoManager` into `[esi+0xe68c]` instead of allocating its own manager.
  * slot 0x67 (`RVA 0x613941`): `call 0x9e0d40` (game `Manager::CreateDevice(int)`) becomes `call FUN_1001f6a0`.
    That function runs `StereoInterface::Init` and sets `manager+4 = &StereoInterface` (or NULL on failure).
  * slots 0x36 / 0x37: `mov eax,[eax+0x2c]` becomes `[eax+0x54]`, so two head-pose calls go to slot 21 instead of slot 11.
  * slot 0x6C (`RVA 0x60FD64`): `call 0x9b8750` (Manager::GetRenderTargetSize) becomes `call FUN_100202a0`.
    This records the mirror window size, then calls slot 16.
  * slot 0x6B (`RVA 0x60FE5D`): `call 0x9b8780` (Manager::GetHmdResolution) becomes `call FUN_1001f640`, a no-op.
  * slot 0x68 (`RVA 0x3B4100`): `call 0x9be2a0` (Manager::Update(dt)) becomes `call FUN_1001f8a0`. That function belongs to the
    input/snap-turn subsystem and also calls slot 21 for recenter.
* Slots 12-15 of the mod vtable are 6-byte `jmp [DAT_10066cd8..ce4]` thunks that tail-jump to the **game's own** device
  methods (`0x9cd4c0, 0x9b8e70, 0x9b8f10, 0x9b9000`). Those methods read the mod object through the game layout:
  `+0x8c` viewports, `+0xac` EyeFov, `+0xcc` EyeRenderDesc, and they call back into the vtable (slot 14 and slot 16).
  **This is why the first 0x1f4 bytes must match the game's layout exactly.**

## 1. Objects, constructors, layout

### 1.1 StereoManager (`DAT_1006ac18`, size 0x324)

* Getter `FUN_10003490` is a thread-safe static local (guard `DAT_1006af3c`): `memset(0x324)`, then `FUN_10003510`,
  then `atexit(FUN_1004c370)`.
* `FUN_10003510` (init, no `this`; uses absolute addresses):
  * vtable `0x1005a61c` at `+0`.
  * Fills the game-mirror part with these values:
    * `+0x20` 4x4 identity (from xmm constants `0x1005e150/170/1a0/230`)
    * `+0x64 = 1.0f`
    * `+0xa8 = (byte)1`
    * `+0xac = 1.0f`
    * `+0x78/+0x88/+0x98` = 3x4 rows `(-1,0,0,0)`, `(0,-1,0,0)`, `(0,0,1,0)`. Note that the game's own ctor uses identity here.
  * Then `mov ecx,0x1006acc8; call FUN_10003640` runs the **StereoInterface ctor on the embedded sub-object at +0xb0**.
  * Then the mod settings at `+0x2e4..+0x320` (see table).
* `FUN_1004c370` (atexit) is `mov ecx,0x1006acc8; jmp FUN_100036f0`, the StereoInterface dtor. It releases the
  ComPtrs at `+0x22c,+0x228,+0x224,+0x220,+0x21c` in that order. The backend at `+0x1f4` is **never deleted**.

StereoManager vtable `0x1005a61c` mirrors game vtable `0x13e57bc`:

| slot | mod fn | ABI | game fn | meaning |
|---|---|---|---|---|
| 0 | 10001f20 (`ret 4`, no-op) | thiscall(uint flags) | 9e3f40 | scalar deleting dtor (mod: static object, does nothing) |
| 1 | 10020230 | thiscall(Event* e) `ret 4` | 9ccdb0 | event handler. If `e->id==0x18 && device`: `device->slot1()` (StartSensor) plus 4 input bindings `FUN_10001ce0(0/0x15/0xd/7, ...)` |
| 2 | 10020290 | thiscall(Event*, int) `ret 8`, returns `al=1` | 9cceb0 | second event handler, always true |

StereoManager field map. Offsets are relative to `0x1006ac18`; the game mirror covers `+0..+0xaf`.

| off | type | meaning |
|---|---|---|
| +0x00 | vptr | 0x1005a61c |
| +0x04 | StereoInterface* | **active device** (game: `ovr device*`). NULL if the HMD failed. Read by the game everywhere (`[[0x1766a2c]+4]`, then `[dev+4]==2` means "stereo running") |
| +0x08 | void* | game's stereo render-target object. Passed to device slot 3 by game `0x9d9b30`; `word [rt+0x80]` is the RT index |
| +0x0c..+0x14 | | game private (`+0x14` word) |
| +0x20..+0x5f | float4x4 | game private matrix (identity) |
| +0x60 | int | game mode; mod hooks check `==1` / `!=0` (FUN_10020c80...) |
| +0x64 | float | 1.0 |
| +0x78..+0xa7 | float[12] | **reference (recenter) head pose, 3x4**. Filled by slot 21 |
| +0xa8 | byte | game flag (set by game at 0x65b3f1/0x675f58) |
| +0xac | float | fade value, lerped in game Update |
| **+0xb0..+0x2df** | **StereoInterface** | embedded, see 1.2 |
| +0x2e0 | StereoInterface* | always `this+0xb0` (set by FUN_1001f6a0) |
| +0x2e4 | bool | runtime forced by command line |
| +0x2e8 | int | forced runtime (`-vrforceoculus`=1, `-vrforceopenvr`=2, `-vrforcenone`=0) |
| +0x2ec | int | `mvr_vrruntime` (0 none, 1 Oculus, 2 OpenVR) |
| +0x2f0 | int | `mvr_mirrorview` (default 1) |
| +0x2f4 | bool | `mvr_mirrorviewsym` (default 1) |
| +0x2f8 | float | `mvr_pixelscale` (default 1.0, clamped 0.5..4) |
| +0x2fc | bool | clipblinding (default 1) |
| +0x2fd | bool | snaprotation (default 1) |
| +0x300 | int | snaprotation segments (default 12) |
| +0x304..+0x307 | bool x4 | relative walking (1), strafing (1), toggle grips (0), toggle crouch (0) |
| +0x308, +0x309 | bool | debug, skipintro |
| +0x30c | float | near-clip value, 1e-5 or 1.0. Its *address* `0x1006af24` is patched into AI.exe slot 0x05 |
| +0x310..+0x320 | | input/snap-turn state (FUN_1001f8a0) |

### 1.2 StereoInterface (`DAT_1006acc8` = manager+0xb0, size 0x230)

* Ctor `FUN_10003640` (thiscall, no args): vptr = `0x1005a5c0`; zeroes `+4,+8,+0xc`; `memset(+0x10, 0, 0x1e4)`;
  zeroes `+0x1f4,+0x1f8,+0x1fc`; sets `+0x204 = 1.0f`; zeroes `+0x208..+0x22c`. `+0x200` is never initialised (stays 0 from the static memset).
  The only caller is `FUN_10003510`.
* Non-virtual init `FUN_1001eac0` (thiscall, no args) is called from `FUN_1001f6a0`:
  1. Sets `state(+4)=0`, `+8=+0xc=0`, `+0x1ec=+0x1f0=0`.
  2. `runtime = mgr.forced ? mgr[+0x2e8] : mgr[+0x2ec]` and stores it to `+0x1f8`. 0 means return (no HMD).
     1 means `new(0x5b8)` + memset + `Oculus` ctor `FUN_1001ec00`. 2 means `new(0x620)` + memset + `OpenVR` ctor `FUN_1001ecf0`.
     The result goes to `+0x1f4`.
  3. If `backend->Init()` (backend slot 0) returns true:
     * `backend->slot9()` (+0x24, input/action setup).
     * Writes a fake `ovrHmdDesc` header: `Handle(+0x10)=[+0x1fc]` (always 0), `HmdCaps(+0x20)=0x83`, `SensorCaps(+0x24)=0x70`,
       `DistortionCaps(+0x28)=2`, `Type(+0x14)=6 (ovrHmd_DK2)`.
     * `backend->GetDesc(this+0x10)` (slot 1).
     * `EyeFov[0..1] (+0xac..+0xcb) = desc.MaxEyeFov (+0x5c..+0x7b)`.
     * `state = 1`.
* `FUN_1001f6a0` is `thiscall(StereoManager*, int enable) ret 4` and replaces game `Manager::CreateDevice` `0x9e0d40`.
  If `enable==1`:
  1. Parses the command line (`FUN_1001f700`: `-vrforce*`, and `-vrscale` writes `+0x2b4` = SI `+0x204`).
  2. `SI.Init()`, then `mgr+0x2e0 = &SI`, then `mgr+4 = SI.state ? &SI : NULL`.
  3. If `state==1`, tail-calls `SI.vtbl[21](mgr+0x78)` to capture the initial reference pose.
  4. Unlike the game, it does not call `0x9d9ba0` on failure.

Field layout. `+0x000..+0x1f3` is the game's `0x1f4`-byte LibOVR-0.4.0 device; the mod keeps the meaning.

| off | type | meaning (game / mod) | used by |
|---|---|---|---|
| +0x000 | vptr | 0x1005a5c0 | |
| +0x004 | int | **state**: 0 none/failed, 1 initialised, 2 sensor started ("stereo active") | slots 0,1,2,16; game everywhere |
| +0x008 | uint | frame index (slot 9 `++`) | game slot 5 only |
| +0x00c | ovrHmd | game: `ovrHmd` handle. Mod: always 0 | slot 0 (`!=0` sets state=0) |
| +0x010..+0x08b | ovrHmdDesc (0.4.0 layout, 0x7c) | +0x10 Handle, +0x14 Type, +0x18 ProductName, +0x1c Manufacturer, +0x20 HmdCaps, +0x24 SensorCaps, +0x28 DistortionCaps, **+0x2c/+0x30 Resolution w/h**, +0x34 WindowsPos, **+0x3c DefaultEyeFov[2]**, **+0x5c MaxEyeFov[2]**, +0x7c EyeRenderOrder[2], **+0x84 DisplayDeviceName**, +0x88 DisplayId | Init; slot 3 mirror math (+0x3c/+0x5c); game slot 17 (+0x2c), game slot 18 (+0x84) |
| +0x08c..+0x0ab | ovrRecti[2] | **eye viewports** {x,y,w,h}. Written by slot 16; read by slot 3 and game slot 15 | |
| +0x0ac..+0x0cb | ovrFovPort[2] | **EyeFov** {Up,Down,Left,Right tan}. Init copies MaxEyeFov here | game slots 12,13,14 |
| +0x0cc..+0x13b | ovrEyeRenderDesc[2] (0x38 each) | only **ViewAdjust** is used: eye0 at **+0xf8**, eye1 at **+0x130** (vec3). Written by slot 5 | game slot 12 |
| +0x13c..+0x177 | ovrD3D11Texture[0] (0x3c) | +0x13c API=5 (D3D11), +0x140/+0x144 TextureSize, +0x148 RenderViewport (= vp0), **+0x158 ID3D11Texture2D\***, **+0x15c ID3D11ShaderResourceView\*** | slot 3 writes; slot 6 reads +0x158/+0x15c |
| +0x178..+0x1b3 | ovrD3D11Texture[1] | copy of [0] with RenderViewport = vp1 (+0x184) | slot 3 writes, nobody reads in mod |
| +0x1b4..+0x1eb | ovrPosef[2] (0x1c) | game: BeginEyeRender poses. Unused by mod | |
| +0x1ec / +0x1f0 | int | **render-target width / height** (both eyes side by side) | slots 3, 16 |
| **+0x1f4** | `HmdBackend*` | MotherVR::Oculus / MotherVR::OpenVR (raw, never freed) | most slots |
| +0x1f8 | int | runtime id (0/1/2) | |
| +0x1fc | int | copied into desc.Handle. Never written anywhere found (0) | Init |
| +0x200 | ? | not initialised, unused (padding?) | |
| +0x204 | float | **world scale** (default 1.0; `-vrscale` 0.495..1.505) | slots 5, 11, 21 |
| +0x208 | int/bool | **head pose fetched this frame**. Slot 11 sets it, slot 6 clears it after Present | slots 5, 6, 11 |
| +0x20c / +0x210 | int | **mirror window size** w/h, from hooked game `GetRenderTargetSize(int*,int*)` (`FUN_100202a0`) | slot 3 |
| +0x214, +0x218 | ? | zeroed by ctor, unused in StereoInterface code | |
| +0x21c | ComPtr<ID3D11VertexShader> | mirror VS (fullscreen tri-strip) | slots 3, 6 |
| +0x220 | ComPtr<ID3D11PixelShader> | mirror PS "Eyes" (samples t1) | slots 3, 6 |
| +0x224 | ComPtr<ID3D11PixelShader> | mirror PS "UI" (samples t0) | slots 3, 6 |
| +0x228 | ComPtr<ID3D11Buffer> | 48-byte constant buffer | slots 3, 6 |
| +0x22c | ComPtr<ID3D11SamplerState> | linear clamp sampler | slots 3, 6 |

## 2. Vtable `0x1005a5c0`: all 22 slots

The game calls each slot as `__thiscall` (`this` in ECX, callee pops). The "game wrapper" column lists the `Manager::` thiscall
(`ecx = [0x1766a2c]`) through which the game normally reaches the slot. Every wrapper first checks `dev && dev->state==2`
(except 16/17/18, which only check `dev`).

| # | off | mod fn | ABI (mod) | game impl / wrapper | name | what the mod does |
|---|---|---|---|---|---|---|
| 0 | 0x00 | 1001ed50 | (uint flags) `ret 4`, void | 9cd460 scalar deleting dtor | `Destroy/Release` | if `state==2`: `this->vtbl[2]()` and `state=1`; if `+0xc!=0`: `state=0`. Never frees (static object) |
| 1 | 0x04 | 1001ed80 | () `ret`, void | 9b8e20 (`ovrHmd_StartSensor(hmd,0x70,0)`); called from mgr event handler (event 0x18) | `StartSensor` | `if(state==1) state=2` |
| 2 | 0x08 | 1001ed90 | () `ret`, void | 9b8e50 (`ovrHmd_StopSensor`) | `StopSensor` | `if(state==2) state=1` |
| 3 | 0x0c | 1001eda0 | (void* stereoRT) `ret 4`, returns `al=1` | 9beb20 (ConfigureRendering + build ovrTexture); wrapper 9d9b30 passes `mgr+8`; called at 0xa13ef4/0xa14129 | `ConfigureRendering` | see §4.1 |
| 4 | 0x10 | 1001f2b0 | () `ret`, returns `al=1` | 9b8df0 (`ovrHmd_ConfigureRendering(hmd,NULL,...)`, i.e. shutdown); wrapper 9d9b60 | `ShutdownRendering` | `backend->slot4()` (+0x10), return true |
| 5 | 0x14 | 1001f330 | () `ret`, void | 9b9100 (`ovrHmd_BeginFrame(hmd,frameIndex)`); wrapper `Manager::BeginFrame` 9b8870 = slot5, slot7(0), slot7(1) | `BeginFrame` | see §4.2 |
| 6 | 0x18 | 1001f3f0 | () `ret`, void | 9b9120 (`ovrHmd_EndFrame(hmd)`, which **presents** in SDK distortion mode); wrapper `Manager::EndFrame` 9b88b0 = slot8(0), slot8(1), jmp slot6 | `EndFrame` | see §4.3: submit to HMD, draw mirror, `IDXGISwapChain::Present(0,0)` |
| 7 | 0x1c | 10001f20 | (int eye) `ret 4` no-op | 9b9130 (`ovrHmd_BeginEyeRender`, pose to +0x1b4+eye*0x1c) | `BeginEyeRender` | nothing |
| 8 | 0x20 | 10001f20 | (int eye) `ret 4` no-op | 9b9190 (`ovrHmd_EndEyeRender(hmd,eye,pose,&tex[eye])`) | `EndEyeRender` | nothing |
| 9 | 0x24 | 10003460 | () `ret` | 9b9200 (same code); wrapper 9b88f0 | `IncrementFrameIndex` | `++[this+8]` |
| 10 | 0x28 | 10003470 | () `ret`, returns `al=1` | ea70f0 (returns 1); wrapper 9b8dd0 | `IsReady` (name uncertain) | return true |
| 11 | 0x2c | 1001f2c0 | (float* out3x4) `ret 4`, returns `out` | 9e0da0 (predicted sensor state to euler to 3x4; z negated); called by `Manager::GetRelativeHeadPose` 9be360 (writes 3x4 minus mgr+0x78), from 0x74dea0 | `GetHeadPose` | `backend->slot5(out, worldScale(+0x204))`; `+0x208 = 1` |
| 12 | 0x30 | 1001f660 to `[DAT_10066cd8]`=**0x9cd4c0** | (int eye, float4x4* view, float4x4* proj) `ret 0xc` | wrapper 9b86a0; used by `Manager::SetEyeShaderConstants` 9b8910 and 0x9b3355/0x9b3985/0x9f0946 | `ApplyEyeTransform` | **game code**: builds the per-eye projection from EyeFov/slot14/slot16, offsets the view by ViewAdjust(+0xf8+eye*0x38). eye -1 means centre |
| 13 | 0x34 | 1001f670 to **0x9b8e70** | (int eye, float* vfov, float* aspect) `ret 0xc` | wrapper 9b86c0 | `GetFovAndAspect` | game code: `vfov = atan(Up)+atan(Down)` of EyeFov[eye], `aspect = (w/2)/h` via slot 16 |
| 14 | 0x38 | 1001f680 to **0x9b8f10** | (int eye, float out[4]) `ret 8` | wrapper 9b86e0 (default {0,0,1,1}) | `GetEyeScaleOffset` (uncertain: NDC/UV scale+offset from FovPort) | game code reading EyeFov |
| 15 | 0x3c | 1001f690 to **0x9b9000** | (int eye, ?, ?, int* left, int* top, int* right, int* bottom) `ret 0x1c` (args 2,3 unused) | wrapper 9b8730, used at 0x9b085c | `GetEyeViewportRect` | game code reading `+0x8c+eye*16` |
| 16 | 0x40 | 1001f530 | (int* w, int* h) `ret 8` | 9b9050 (returns +0x1ec/+0x1f0); wrapper 9b8750 (defaults 1280x800), **hooked by FUN_100202a0** | `GetRenderTargetSize` | see §4.4. Recomputes size and viewports on every call |
| 17 | 0x44 | 1001f640 | (int* w, int* h) `ret 8`, returns `h` | 9b9070 (desc.Resolution +0x2c/+0x30); wrapper 9b8780 (defaults 1280x800). The call site at 0xa0fe5d is hooked to this same function | `GetHmdResolution` | **no-op** (leaves the caller's window resolution alone) |
| 18 | 0x48 | 1001f650 | (char* buf, int size) `ret 8`, returns `buf` | 9b9090 (copies DisplayDeviceName +0x84); wrapper 9b87b0 (called 0xa137bd) | `GetDisplayDeviceName` | no-op |
| 19 | 0x4c | 10003480 | `mov eax,1; ret 8` | 9b90b0: `() ret`, returns `(SensorState.StatusFlags>>1)&1` (PositionTracked) | `IsPositionTracked` | returns 1. **ABI mismatch**: game impl takes 0 args, mod pops 8 bytes (ICF-shared stub). No game call site was found by pattern search, so it is probably never called. Reconstruct with the game ABI `() ret` |
| 20 | 0x50 | 10001e40 (`guard_check_icall`, plain `ret`) | () `ret` | a69c90 (`ret`) | unused / no-op | nothing |
| 21 | 0x54 | 1001f300 | (float* out3x4) `ret 4`, returns `out` | none: MotherVR-only slot | `RecenterAndGetHeadPose` | `backend->slot6(out, worldScale)`; does **not** set +0x208. Callers: FUN_1001f6a0 (initial, `mgr+0x78`), patched game sites 0x9be2e8 (Manager::Update recenter on buttons 0xa+0xb) and 0x9b8691 (`Manager::CaptureReferencePose` 9b8680, called from 0x491bd7), and FUN_1001f8a0 (mod's Update replacement) |

Return types: everything is `void` or returns int/bool in EAX/AL. No method returns in ST0.

## 3. Game-side function pointers used

All are plain `__cdecl` (caller cleans: `add esp,8/0x10`).

| global | slot | AI.exe VA (RVA) | code | returns |
|---|---|---|---|---|
| DAT_10066d5c | 0x65 | 0x1BDF5CC (.data, RVA 0x17DF5CC) | **data** | address of the global `Renderer*`. The mod does `renderer = *DAT_10066d5c` |
| DAT_10066d0c | 0x51 | 0xA21E20 (0x621E20) | `return r->+4` | **ID3D11Device\*** (used for Create*Shader/Buffer/SamplerState) |
| DAT_10066d10 | 0x52 | 0xA21E30 (0x621E30) | `return r->+8` | **IDXGISwapChain\*** (vtbl +0x20 = Present(0,0)) |
| DAT_10066d14 | 0x53 | 0xA21E40 (0x621E40) | `return r->+0x1ef3e8` | **ID3D11DeviceContext\*** (immediate) |
| DAT_10066d18 | 0x54 | 0xA232D0 (0x6232D0) | `(r, uint16 idx) -> *(r + 0x1cd240 + idx*16)` | **ID3D11Texture2D\*** of render target `idx` |
| DAT_10066d1c | 0x55 | 0xA232F0 (0x6232F0) | `(r, uint16 idx) -> *(r + 0x1cd248 + idx*16)` | **ID3D11ShaderResourceView\*** of RT `idx` |
| DAT_10066cd8 | 0x44 | 0x9CD4C0 (0x5CD4C0) | game device slot 12 | (vtable target) |
| DAT_10066cdc | 0x45 | 0x9B8E70 (0x5B8E70) | game device slot 13 | (vtable target) |
| DAT_10066ce0 | 0x46 | 0x9B8F10 (0x5B8F10) | game device slot 14 | (vtable target) |
| DAT_10066ce4 | 0x47 | 0x9B9000 (0x5B9000) | game device slot 15 | (vtable target) |

`idx` comes from `word [stereoRT + 0x80]`, where `stereoRT` = slot 3's argument = `mgr+8`. Each renderer RT table
entry is 16 bytes `{ID3D11Texture2D*, ?, ID3D11ShaderResourceView*, ?}` (inferred from how they are used).

## 4. The big methods

COM vtable offsets: ID3D11Device +0x0c CreateBuffer, +0x30 CreateVertexShader, +0x3c CreatePixelShader, +0x5c CreateSamplerState.
ID3D11DeviceContext +0x20 PSSetShaderResources, +0x24 PSSetShader, +0x28 PSSetSamplers, +0x2c VSSetShader, +0x34 Draw,
+0x40 PSSetConstantBuffers, +0x60 IASetPrimitiveTopology, +0x84 OMSetRenderTargets, +0x8c OMSetBlendState, +0xac RSSetState,
+0xc0 UpdateSubresource, +0xc8 ClearRenderTargetView, +0x164 OMGetRenderTargets. IDXGISwapChain +0x20 Present.
IUnknown +0x08 Release.

### 4.1 Slot 3 `FUN_1001eda0`: ConfigureRendering(void* stereoRT), `ret 4`, returns true

1. Build `tex[0]` (+0x13c): API=5, size=(+0x1ec,+0x1f0), viewport=vp0 (+0x8c, 16 bytes).
   `renderer=*DAT_10066d5c`. If `stereoRT` is non-null: `tex[0].pTexture(+0x158) = d18(renderer, rt->idx)` and
   `tex[0].pSRView(+0x15c) = d1c(renderer, rt->idx)`. If it is null, the old values are kept.
2. `tex[1] (+0x178) = tex[0]`, then `tex[1].viewport (+0x184) = vp1 (+0x9c)`.
3. `device=d0c(renderer)`, `ctx=d14(renderer)`, then `backend->slot3(device, ctx)` (+0xc, `ret 8`).
4. For each ComPtr (release the old value first, then create):
   * `CreateVertexShader(0x1005bbd8, 0x300, NULL, &+0x21c)`
   * `CreatePixelShader(0x1005b338, 0x89c, NULL, &+0x220)`
   * `CreatePixelShader(0x1005ad90, 0x5a4, NULL, &+0x224)`
   * `CreateBuffer({ByteWidth=0x30, DEFAULT, CONSTANT_BUFFER, 0,0,0}, NULL, &+0x228)`
5. Compute the mirror constant buffer and `ctx->UpdateSubresource(cb, 0, NULL, &data, 4, 4)`. Inputs:
   * `m = mgr.mirrorView (+0x2f0)`, `sym = mgr.mirrorViewSym (+0x2f4)`
   * `rtAspect = w/h (+0x1ec/+0x1f0)`, `winAspect = +0x20c/+0x210`
   * Start values: `u0=0, u1=1, v0=0, v1=1, xr=1`.
   * If `sym`:
     * `m==0` gives the whole RT (both eyes).
     * `m==2` gives `u0=0, u1=0.5` (left eye).
     * any other `m` gives `u0=0.5, u1=1` (right eye).
   * Else (crop to the **DefaultEyeFov** inside the rendered **MaxEyeFov**):
     * `e = (m!=0 && m!=2) ? 1 : 0`, with `r_X = Default[e].X / Max[e].X`
     * `u0=(0.5-r_L*0.5)*0.5`, `u1=(r_R*0.5+0.5)*0.5`, `v0=0.5-r_U*0.5`, `v1=r_D*0.5+0.5`
     * if `m==0`, `xr=0.5`
     * Note: `u` stays in the left half even for `e==1`. As decompiled, the PS handles `m==0` by mirroring.
   * Then `k = winAspect*xr / ((u1-u0)*rtAspect)` and the cbuffer is
     `{DrawTopLeft=(0,(1-k)/2), DrawBottomRight=(xr,(1+k)/2), SampleTopLeft=(u0,v0), SampleBottomRight=(u1,v1), ScaleAdjust, WindowAspect=winAspect}`.
   * `ScaleAdjust = 1.0` on the very first call (static flag `DAT_1006afbc`), then `0.5/mgr.pixelScale`. Meaning uncertain.
   * Floats 10-11 of the 48-byte upload are stack garbage (they include the /GS cookie). The shaders do not use them.
6. `CreateSamplerState({Filter=0x15 MIN_MAG_MIP_LINEAR, AddressU/V/W=4 CLAMP, MipLODBias=0, MaxAniso=1, Cmp=1 NEVER, Border=0, MinLOD=-FLT_MAX, MaxLOD=FLT_MAX}, &+0x22c)`.

**Shaders** (all genuine DXBC, compiled with fxc 6.3.9600.16384, SM 5.0). Dumped to `dumps/shaders/*.bin`, with disassembly in `*.asm`:

| blob | size | file | content |
|---|---|---|---|
| 0x1005bbd8 | 0x300 | `vs_mirror_1005bbd8.bin` | vs_5_0: SV_VertexID quad: `uv=(id&1, id>>1)`, `pos=(2u-1, 1-2v)` (4-vertex strip) |
| 0x1005b338 | 0x89c | `ps_mirror_a_1005b338.bin` | ps_5_0 "Eyes" (t1, s0, cb0): draws the sample rect into the draw rect (both scaled by ScaleAdjust). For the `xr==0.5` case it draws the right half with a horizontally flipped `u`. Outside, black |
| 0x1005ad90 | 0x5a4 | `ps_mirror_b_1005ad90.bin` | ps_5_0 "UI" (t0, s0, cb0): samples `t0` with `uv = (u/ScaleAdjust, v*(1.7778/WindowAspect)/ScaleAdjust + (1-1.7778/WindowAspect)/2)`, i.e. 16:9 letterboxed |

cbuffer `ConstantBuffer` (48 bytes): `float2 DrawTopLeft; float2 DrawBottomRight; float2 SampleTopLeft; float2 SampleBottomRight; float ScaleAdjust; float WindowAspect;` (+ 8 bytes padding).

### 4.2 Slot 5 `FUN_1001f330`: BeginFrame(), `ret`

1. `backend->slot10()` (+0x28, per-frame update: OpenVR poses/input).
2. `backend->slot7(float tmp[6], bool(+0x208!=0), 1)` (+0x1c, `ret 0xc`). This gets the eye offsets. The 6 floats are local; the /GS cookie sits right after.
3. Scale by `s=+0x204` and write ViewAdjust:
   * `EyeRenderDesc[1].ViewAdjust (+0x130..+0x138) = s*tmp[0..2]`
   * `EyeRenderDesc[0].ViewAdjust (+0xf8..+0x100) = s*tmp[3..5]`

   (The first vec3 goes to eye 1. Whether that is a deliberate swap or the backend returns them in R,L order: uncertain.)

### 4.3 Slot 6 `FUN_1001f3f0`: EndFrame(), `ret`

1. `backend->slot8(tex[0].pTexture (+0x158))` (+0x20, `ret 4`) submits the side-by-side eye texture to the HMD.
2. `swap=d10(renderer)`, `ctx=d14(renderer)`.
3. `ctx->OMGetRenderTargets(1,&rtv,NULL)`, then `ClearRenderTargetView(rtv,{0.6,0,0,1})` (dark red), then `OMSetRenderTargets(1,&rtv,NULL)`.
   **The rtv reference from OMGetRenderTargets is never Released** (leaks one ref per frame).
4. `OMSetBlendState(NULL,NULL,0xffffffff)`, `RSSetState(NULL)`, `VSSetShader(+0x21c)`.
5. `PSSetShader(+0x208 ? Eyes(+0x220) : UI(+0x224))`. In 3D gameplay the camera polled the head pose this frame, so the mirror shows the eyes. Otherwise (menus, videos) it shows `t0`. The mod does not bind t0; it is whatever the game left bound (uncertain).
6. `PSSetShaderResources(1,1,&+0x15c)`, `PSSetConstantBuffers(0,1,&+0x228)`, `PSSetSamplers(0,1,&+0x22c)`,
   `IASetPrimitiveTopology(5 TRIANGLESTRIP)`, `Draw(4,0)`. No viewport is set (it uses the current one). No input layout or VB is needed (SV_VertexID).
7. `swap->Present(0,0)`, then `+0x208 = 0`. This replaces LibOVR's SDK-distortion Present inside `ovrHmd_EndFrame`.

### 4.4 Slot 16 `FUN_1001f530`: GetRenderTargetSize(int* w, int* h), `ret 8`

1. `scale = (state==1) ? 0.5f : mgr.pixelScale(+0x2f8)`.
2. `L = backend->slot2(eye 0, scale)` and `R = backend->slot2(eye 1, scale)` (+0x08, `ret 0xc`, `Sizei` returned through a hidden pointer pushed first).
3. `W = L.w + R.w`. `H = (L.w <= R.w) ? R.h : L.h` (quirk: it compares widths, not heights).
4. `+0x1ec=W, +0x1f0=H`. `vp0={0,0,W/2,H}`, `vp1={W/2,0,W/2,H}` (signed /2; the game used `(W+1)/2` for vp1.x).
5. `*w=W, *h=H`.

Called by the game's back-buffer sizing (through the hook FUN_100202a0, which first stores the incoming `*w,*h` into
`+0x20c/+0x210` as the mirror window size) and internally by game slots 12/13.

## 5. Reconstruction outline

### 5.1 Which binding style

**Recommended: a POD struct with exact offsets plus a hand-built `void* vtable[22]`.** Mod-implemented slots are free functions
`__fastcall(StereoInterface* self, void* /*edx*/, args...)` (callee-pops, so stack args match `__thiscall`).

Reasons:
* Slots 12-15 must point **directly at AI.exe code** (`ai(0x44..0x47)`). With an array you just store the address. A real C++ class
  would need naked `jmp` thunks.
* Slot 0 must not be a real virtual destructor. Slot 19 should use the game's 0-arg ABI. Slot 20 is a bare `ret`. A hand table makes all of this explicit.
* The object is a static sub-object of a game-layout struct. No compiler-generated vptr placement, RTTI or EH is wanted.

A real C++ class with 22 virtuals in the right order also works with MSVC/clang-cl x86, because virtuals are `__thiscall`.
That needs: no virtual dtor (declare `virtual void Release(unsigned)` in slot 0), the slot 12-15 thunks, and you must
keep the compiler from reordering overloads. That is riskier, so prefer the table.

```cpp
#include <cstdint>
#include <cstddef>
struct ID3D11VertexShader; struct ID3D11PixelShader; struct ID3D11Buffer; struct ID3D11SamplerState;
struct ID3D11Texture2D; struct ID3D11ShaderResourceView; struct ID3D11Device; struct ID3D11DeviceContext;

namespace mvr {
struct Sizei { int w, h; };
struct Recti { int x, y, w, h; };
struct FovPort { float up, down, left, right; };           // tangents
struct HmdDesc040 {                                        // LibOVR 0.4.0 ovrHmdDesc, 0x7c
  void* handle; int type; const char* productName; const char* manufacturer;
  uint32_t hmdCaps, sensorCaps, distortionCaps; Sizei resolution; int windowsPos[2];
  FovPort defaultEyeFov[2], maxEyeFov[2]; int eyeRenderOrder[2];
  const char* displayDeviceName; long displayId;
};
static_assert(sizeof(HmdDesc040) == 0x7c);
struct EyeRenderDesc040 { int eye; FovPort fov; Recti distortedViewport; float pixelsPerTan[2]; float viewAdjust[3]; };
static_assert(sizeof(EyeRenderDesc040) == 0x38);
struct D3D11Texture040 { int api; Sizei size; Recti viewport; ID3D11Texture2D* tex; ID3D11ShaderResourceView* srv; uintptr_t pad[6]; };
static_assert(sizeof(D3D11Texture040) == 0x3c);
struct Posef040 { float q[4]; float p[3]; };

class HmdBackend;   // see 5.2

struct StereoInterface {                    // embedded at StereoManager+0xb0
  void* const* vtbl;                        // +0x000 -> kStereoVtbl[22]
  int      state;                           // +0x004 0/1/2
  uint32_t frameIndex;                      // +0x008
  void*    hmd;                             // +0x00c always null
  HmdDesc040 desc;                          // +0x010
  Recti    viewport[2];                     // +0x08c
  FovPort  eyeFov[2];                       // +0x0ac
  EyeRenderDesc040 eyeDesc[2];              // +0x0cc (viewAdjust at +0xf8 / +0x130)
  D3D11Texture040 eyeTex[2];                // +0x13c
  Posef040 eyePose[2];                      // +0x1b4
  int      rtWidth, rtHeight;               // +0x1ec, +0x1f0   <- end of game layout (0x1f4)
  HmdBackend* backend;                      // +0x1f4
  int      runtime;                         // +0x1f8
  int      handleSeed;                      // +0x1fc
  uint32_t unk200;                          // +0x200
  float    worldScale;                      // +0x204
  int      poseFetchedThisFrame;            // +0x208
  int      mirrorWidth, mirrorHeight;       // +0x20c, +0x210
  uint32_t unk214, unk218;                  // +0x214, +0x218
  ID3D11VertexShader*  mirrorVS;            // +0x21c (ComPtr semantics)
  ID3D11PixelShader*   mirrorPSEyes;        // +0x220
  ID3D11PixelShader*   mirrorPSUI;          // +0x224
  ID3D11Buffer*        mirrorCB;            // +0x228
  ID3D11SamplerState*  mirrorSampler;       // +0x22c
};
static_assert(offsetof(StereoInterface, desc) == 0x10);
static_assert(offsetof(StereoInterface, viewport) == 0x8c);
static_assert(offsetof(StereoInterface, eyeFov) == 0xac);
static_assert(offsetof(StereoInterface, eyeDesc) == 0xcc);
static_assert(offsetof(StereoInterface, eyeDesc[0].viewAdjust) == 0xf8);
static_assert(offsetof(StereoInterface, eyeDesc[1].viewAdjust) == 0x130);
static_assert(offsetof(StereoInterface, eyeTex) == 0x13c);
static_assert(offsetof(StereoInterface, eyeTex[0].tex) == 0x158);
static_assert(offsetof(StereoInterface, eyeTex[0].srv) == 0x15c);
static_assert(offsetof(StereoInterface, eyePose) == 0x1b4);
static_assert(offsetof(StereoInterface, rtWidth) == 0x1ec);
static_assert(offsetof(StereoInterface, backend) == 0x1f4);
static_assert(offsetof(StereoInterface, worldScale) == 0x204);
static_assert(offsetof(StereoInterface, mirrorWidth) == 0x20c);
static_assert(offsetof(StereoInterface, mirrorVS) == 0x21c);
static_assert(sizeof(StereoInterface) == 0x230);

struct StereoManager {                      // replaces the game's 0xb0-byte manager; static, 0x324
  void* const* vtbl;                        // +0x000 -> {nop ret4, OnEvent ret4, OnEvent2 ret8}
  StereoInterface* device;                  // +0x004
  void*    stereoRT;                        // +0x008 (game-owned)
  uint8_t  gameState[0x78 - 0x0c];          // +0x00c..+0x077 (0x20 mat4, 0x60 int mode, 0x64 float)
  float    referencePose[12];               // +0x078 3x4
  uint8_t  gameFlagA8; uint8_t padA9[3];    // +0x0a8
  float    fade;                            // +0x0ac
  StereoInterface si;                       // +0x0b0
  StereoInterface* siAlways;                // +0x2e0
  bool forceRuntime; uint8_t pad2e5[3];     // +0x2e4
  int forcedRuntime, runtime, mirrorView;   // +0x2e8, +0x2ec, +0x2f0
  bool mirrorViewSym; uint8_t pad2f5[3];    // +0x2f4
  float pixelScale;                         // +0x2f8
  uint8_t settingsAndInputState[0x324 - 0x2fc]; // +0x2fc..+0x323 (see table in 1.1)
};
static_assert(offsetof(StereoManager, referencePose) == 0x78);
static_assert(offsetof(StereoManager, si) == 0xb0);
static_assert(offsetof(StereoManager, siAlways) == 0x2e0);
static_assert(offsetof(StereoManager, pixelScale) == 0x2f8);
static_assert(sizeof(StereoManager) == 0x324);
} // namespace mvr
```

Method signatures for the hand-built table. `S` = `StereoInterface*`, all `__fastcall(S, void* edx, ...)` unless noted:

```
[0]  void  Release(S, edx, unsigned flags)                      // ret 4
[1]  void  StartSensor(S, edx)                                  // ret
[2]  void  StopSensor(S, edx)
[3]  bool  ConfigureRendering(S, edx, void* stereoRT)           // ret 4, returns true
[4]  bool  ShutdownRendering(S, edx)
[5]  void  BeginFrame(S, edx)
[6]  void  EndFrame(S, edx)
[7]  void  BeginEyeRender(S, edx, int eye)                      // ret 4, no-op
[8]  void  EndEyeRender(S, edx, int eye)                        // ret 4, no-op
[9]  void  IncrementFrameIndex(S, edx)
[10] bool  IsReady(S, edx)                                      // true
[11] float* GetHeadPose(S, edx, float* out3x4)                  // ret 4, sets poseFetched
[12] = ai(0x44) 0x9cd4c0   (eye, float4x4* view, float4x4* proj) ret 0xc
[13] = ai(0x45) 0x9b8e70   (eye, float* vfov, float* aspect)     ret 0xc
[14] = ai(0x46) 0x9b8f10   (eye, float out[4])                   ret 8
[15] = ai(0x47) 0x9b9000   (eye, ?, ?, int*,int*,int*,int*)      ret 0x1c
[16] int*  GetRenderTargetSize(S, edx, int* w, int* h)          // ret 8
[17] int*  GetHmdResolution(S, edx, int* w, int* h)             // ret 8, no-op, returns h
[18] char* GetDisplayDeviceName(S, edx, char* buf, int size)    // ret 8, no-op, returns buf
[19] bool  IsPositionTracked(S, edx)                            // use game ABI (ret 0); original stub was "ret 8"
[20] void  Unused(S, edx)                                       // ret
[21] float* RecenterAndGetHeadPose(S, edx, float* out3x4)       // ret 4
```

### 5.2 Required HMD backend interface (MotherVR::Oculus / MotherVR::OpenVR, 11 slots)

These are real C++ virtuals (`__thiscall`) in the original. Ret sizes were verified on both backends.

| slot | off | signature | callers |
|---|---|---|---|
| 0 | +0x00 | `bool Init()` | SI::Init |
| 1 | +0x04 | `void GetDesc(HmdDesc040* out)` `ret 4`. Must fill MaxEyeFov (+0x4c in desc), DefaultEyeFov, Resolution | SI::Init |
| 2 | +0x08 | `Sizei GetRecommendedTextureSize(int eye, float pixelScale)`, hidden return pointer, `ret 0xc` | slot 16 |
| 3 | +0x0c | `void InitGraphics(ID3D11Device*, ID3D11DeviceContext*)` `ret 8` | slot 3 |
| 4 | +0x10 | `void ShutdownGraphics()` | slot 4 |
| 5 | +0x14 | `void GetHeadPose(float out[12], float worldScale)` `ret 8` (3x4, game convention) | slot 11 |
| 6 | +0x18 | `void RecenterAndGetHeadPose(float out[12], float worldScale)` `ret 8`. OpenVR stores a yaw-only reference at backend+0x510 | slot 21 |
| 7 | +0x1c | `void GetEyeOffsets(float out[6], bool poseFetched, int one)` `ret 0xc`. out[0..2] goes to eye 1, out[3..5] to eye 0 (unscaled) | slot 5 |
| 8 | +0x20 | `void Submit(ID3D11Texture2D* sideBySide)` `ret 4` | slot 6 |
| 9 | +0x24 | `void InitInput()` (OpenVR: action bindings "VR_TP_D", ...) | SI::Init |
| 10 | +0x28 | `void UpdateFrame()` (per frame poses/input; OpenVR FUN_10007ad0) | slot 5 |

### 5.3 Install-side checklist for the reconstruction

1. Static `StereoManager g_mgr` (0x324). Construct the game-mirror defaults as in 1.1 and `si` as in 1.2.
2. Patch slot 0x35: `b8 <&g_mgr> eb 1d`.
3. Hook slot 0x67 call to `CreateDevice(mgr, int)` (thiscall `ret 4`). The game calls it as `mov ecx,[0x1766a2c]; push 1; call`.
4. Patch slots 0x36/0x37 displacement byte `0x2c` to `0x54`.
5. Hook slot 0x6C (`GetRenderTargetSize`, thiscall `(int*,int*) ret 8`) to record the mirror size, then `si.vtbl[16]`.
   Hook slot 0x6B to a no-op `ret 8`.
6. Embed the three DXBC blobs from `dumps/shaders/` (byte-identical) or recompile equivalent HLSL from the `.asm` listings.

## 6. Open points / uncertainties

* Exact semantics of game slot 14 (`0x9b8f10`) and the 2 unused args of slot 15. The mod forwards them unchanged, so this does not affect the reconstruction.
* `ScaleAdjust` meaning (1.0 first time, then 0.5/pixelScale) and what is bound at PS t0 when the "UI" mirror shader runs.
* Whether the game ever calls device slots 0, 19 or 20. None found by pattern search; slot 0 likely runs only at teardown.
* Eye order of backend slot 7's output (first vec3 goes to eye 1).
* The rtv leak in EndFrame and the width-compare quirk in slot 16 are original behaviour. Decide whether to keep them.
