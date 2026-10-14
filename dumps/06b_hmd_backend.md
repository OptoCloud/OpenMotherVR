# 06b: HMD backend interface (`MotherVR::OpenVR` / `MotherVR::Oculus`)

Target: MotherVR 0.8.1 `dxgi.dll` (image base 0x10000000).
Sources: `dumps/v2/decompiled/*`, disassembly via `scripts/disasm.py`, RTTI vtables in
`dumps/vtables.txt`. Every ABI claim below (arg count, `ret N`, return register) comes from
the disassembly, not the Ghidra signatures, which are mostly wrong.

Confidence markers: **[sure]** read from disasm, **[likely]** strong inference,
**[guess]** plausible but not confirmed.

---

## 0. Summary

* Both backends implement one abstract interface with **11 virtual methods and no
  virtual destructor**. Slot 0 is `Init()`. No code anywhere deletes the backend.
* Every method is `__thiscall`: `this` in ECX, the callee pops its args.
* The OpenVR backend targets **openvr 1.0.10**: `IVRSystem_017`, `IVRCompositor_021`,
  `IVROverlay_016`. It uses **no** IVRInput. Input goes through legacy
  `GetControllerStateWithPose`. It never calls `SetTrackingSpace`, so it runs in the
  compositor's default **Standing** universe. It never calls `ResetSeatedZeroPose`, so
  there is no recenter. It never calls `PollNextEvent`.
* Frame pacing uses the "running start" pattern. `SubmitFrame` calls
  `Submit(L)`, then `Submit(R)`, then `WaitGetPoses(NULL,0,NULL,0)`. That blocks until
  the next frame. Pose queries during the frame use `GetLastPoses`.
* The game renders **one side-by-side texture**. Each eye uses a **symmetric** frustum,
  sized to the max tangent over both eyes and both sides. The compositor crops each eye
  with `VRTextureBounds_t`, using the SteamVR-Unity-plugin formula.
* When the game did not request a head-tracked pose this frame (menus, cutscenes,
  loading), the backend switches to **cinema mode**. The frame goes onto a world-locked
  `IVROverlay` quad: 3.5556 m wide, 1.5 m in front of the head, yaw-only orientation.
  The eye submits get near-zero bounds, so the 3D scene goes black.

---

## 1. The interface: slots, ABI, meaning

Vtables: OpenVR `0x1005ddac`, Oculus `0x1005dddc`. Callers are all in
`MotherVR::StereoInterface` (vtable `0x1005a5c0`). The backend pointer lives at `SI+0x1f4`.

| # | off | OpenVR | Oculus | `ret` | Signature (reconstructed) | Called from (SI) |
|---|-----|--------|--------|-------|---------------------------|------------------|
| 0 | +0x00 | 100068b0 | 100037b0 | `ret` | `bool Init()` (AL = 0/1) | FUN_1001eac0 (SI::InitBackend) |
| 1 | +0x04 | 10006b40 | 100038e0 | `ret 4` | `void GetHmdDesc(HmdDesc* desc)` | FUN_1001eac0 |
| 2 | +0x08 | 10006f20 | 100039f0 | `ret 0xc` | `Sizei GetEyeTextureSize(int eye, float pixelScale)`, MSVC member UDT return (hidden ptr is first stack arg, EAX = that ptr) | FUN_1001f530 (SI vt#16) |
| 3 | +0x0c | 10006fe0 | 10003a50 | `ret 8` | `void CreateRenderResources(ID3D11Device*, ID3D11DeviceContext*)` | FUN_1001eda0 (SI vt#3) |
| 4 | +0x10 | 10007060 | 10003cc0 | `ret` | `void ReleaseRenderResources()` | FUN_1001f2b0 (SI vt#4) |
| 5 | +0x14 | 100070e0 | 10003ce0 | `ret 8` | `void GetHeadPose(Matrix34* out, float worldScale)` | FUN_1001f2c0 (SI vt#11). SI sets `SI+0x208=1` ("head tracked this frame") |
| 6 | +0x18 | 10007220 | 10003f50 | `ret 8` | `void GetHeadYawPose(Matrix34* out, float worldScale)` | FUN_1001f300 (SI vt#21) |
| 7 | +0x1c | 100085c0 | 100052f0 | `ret 0xc` | `void BeginFrame(float eyeOffsets[6], bool headTracked, int unused)` | FUN_1001f330 (SI vt#5) |
| 8 | +0x20 | 10008710 | 100054f0 | `ret 4` | `void SubmitFrame(ID3D11Texture2D* sbsTexture)` | FUN_1001f3f0 (SI vt#6) |
| 9 | +0x24 | 10007500 | 10004270 | `ret` | `void RegisterControlHints()` | FUN_1001eac0, right after a successful Init |
| 10 | +0x28 | 10007ad0 | 10004960 | `ret` | `void PollInput()` | FUN_1001f330, before slot 7 |

ABI notes:
* Slot 2: OpenVR ends with `mov eax, edi` (the out ptr). Oculus `return param_1`. A C++
  method returning `Sizei` by value produces exactly this ABI under MSVC x86, because
  instance methods always return UDTs through a hidden pointer. **[sure]**
* Slots 5 and 6 do **not** set EAX. They are `void` with an out-param. SI returns the out
  ptr itself. **[sure]**
* Slot 7 arg 2 is pushed as `movzx eax, al; push eax`, so it is a 4-byte stack `bool`.
  Arg 3 is always `1` from SI and neither backend reads it. **[sure]**
* Slot 3: SI pushes the context, then the device, so arg1 = device. Oculus stores arg1 at
  `+4` and calls `ID3D11Device::CreateRenderTargetView` (vt+0x24) on it. It stores arg2
  at `+8` and calls `ID3D11DeviceContext::CopyResource` (vt+0xbc) on it. OpenVR ignores
  both. **[sure]**

Names were cross-checked against Oculus:
* slot 0 Oculus: `ovr_Initialize` + `ovr_Create`
* slot 1: `ovr_GetHmdDesc`, `SetTrackingOriginType(EyeLevel)`, `RecenterTrackingOrigin`
* slot 2: `ovr_GetFovTextureSize`
* slot 3: `ovr_CreateTextureSwapChainDX` + RTVs
* slot 4: `ovr_DestroyTextureSwapChain`
* slot 5: `GetPredictedDisplayTime` + `GetTrackingState`, then HeadPose to matrix
* slot 7: `ovr_GetRenderDesc` x2, keeping `HmdToEyePose.Position`; when tracked,
  `ovr_GetEyePoses` and the EyeFov layer, else a quad layer
* slot 8: `CopyResource` into the swapchain + `Commit` + `ovr_SubmitFrame`
* slot 9: the same help strings, Touch flavour
* slot 10: `GetSessionStatus` (ShouldRecenter, then Recenter) + `ovr_GetInputState(Touch)`

### Shared data types

```cpp
struct Matrix34 { float m[3][4]; };          // row-major, m[r][3] = translation
struct Sizei    { int32_t w, h; };
struct FovPort  { float up, down, left, right; };   // tangents, ovrFovPort layout

// Lives at StereoInterface+0x10. SI fills the "in" fields before calling slot 1.
struct HmdDesc {
    uint32_t    in_00;          // +0x00  = SI+0x1fc (unknown, [guess] window/adapter)
    uint32_t    in_04;          // +0x04  = 6
    const char* productName;    // +0x08  OUT  (OpenVR: Prop_ModelNumber_String)
    const char* manufacturer;   // +0x0c  OUT  (OpenVR: Prop_ManufacturerName_String)
    uint32_t    in_10;          // +0x10  = 0x83
    uint32_t    in_14;          // +0x14  = 0x70
    uint32_t    in_18;          // +0x18  = 2
    uint8_t     _1c[0x10];      // +0x1c  untouched by backends
    FovPort     eyeFov[2];      // +0x2c  OUT  per-eye real FOV (ovr DefaultEyeFov)
    FovPort     renderFov[2];   // +0x4c  OUT  symmetric render FOV, both eyes identical:
                                //        {maxV, maxV, maxH, maxH}
};                              // size >= 0x6c
```

After slot 1 returns, SI copies `desc+0x4c..0x6b` (`renderFov[2]`) to `SI+0xac..0xcb`.
SI's mirror code (FUN_1001eda0) reads `eyeFov`/`renderFov` to crop the desktop mirror.

---

## 2. OpenVR class: layout, construction, creation

### 2.1 Field layout (`sizeof == 0x620`, `operator new(0x620)` + memset 0)

| off | type | meaning |
|-----|------|---------|
| +0x000 | vptr | `0x1005ddac` |
| +0x004 | (none) | unused by OpenVR (Oculus: `ID3D11Device*`) |
| +0x008 | (none) | unused by OpenVR (Oculus: `ID3D11DeviceContext*`) |
| +0x00c | `HMODULE` | `LoadLibraryW(<cwd>\openvr_api.dll)` (Oculus: `ovrSession`) |
| +0x010 | `vr::TrackedDevicePose_t[16]` | stride 0x50. Filled by WaitGetPoses/GetLastPoses. `[0]` = HMD. HMD `mDeviceToAbsoluteTracking` is at +0x10..+0x3f. Translation is at +0x1c, +0x2c, +0x3c |
| +0x510 | `HmdMatrix34_t` | yaw-only head transform (OpenVR space, unscaled). Written by slot 6, read by slot 8 for overlay placement |
| +0x540 | `bool` | `cinemaMode` = `!headTracked` (set by slot 7) |
| +0x548 | `vr::VROverlayHandle_t` (u64) | overlay "MotherVROverlay" |
| +0x550 | `float[4]` | `{maxV, maxV, maxH, maxH}` symmetric render tangents |
| +0x560 | `float[4]` | eye 0 raw FOV: `{-top, bottom, -left, right}` (see 3/slot 1) |
| +0x570 | `float[4]` | eye 1 raw FOV, same order |
| +0x580 | `VRTextureBounds_t` | left-eye crop `{uMin, vMin, uMax, vMax}` |
| +0x590 | `VRTextureBounds_t` | right-eye crop |
| +0x5a0 | `char[64]` | `Prop_ManufacturerName_String` (1005) |
| +0x5e0 | `char[64]` | `Prop_ModelNumber_String` (1001) |

### 2.2 Constructor FUN_1001ecf0 **[sure]**

Sets the vptr and `+0xc = 0`, then `memset(+0x10, 0, 0x530)`, `+0x540 = 0`,
`+0x548 = +0x54c = 0`, `memset(+0x550, 0, 0xd0)`. Returns `this`.

The Oculus constructor is FUN_1001ec00 (size 0x5b8). It also sets `+0x49c = 1.0f`
(a quaternion w).

### 2.3 Creation and runtime selection

* **FUN_1001d6f0** is the hook installer / startup. It calls **FUN_1001e720** to extract
  resource 0xCA (`BINARY`) to `DATA\MOTHER.PAK` when the file is missing or its
  timestamp differs. If `UI.PAK` is missing it shows an error and aborts. If
  `StereoManager.vr_runtime (+0x2ec) == 2` it calls **FUN_1001e940**, which extracts
  resource 0xC9 to `<cwd>\openvr_api.dll` (the bundled 1.0.10 loader) under the same
  timestamp check. That DLL is what the delay-load imports bind to.
* **FUN_1001f6a0(1)** is StereoManager init. It calls **FUN_1001f700**, which parses
  the command line:
  * `-vrforceoculus` sets `cfg+0x2e4 = 1` and `cfg+0x2e8 = 1`
  * `-vrforceopenvr` sets `cfg+0x2e4 = 1` and `cfg+0x2e8 = 2`
  * `-vrforcenone` sets `cfg+0x2e4 = 1` and `cfg+0x2e8 = 0`
  * `-vrscale x` sets `cfg+0x2b4 = x`, falling back to 1.0 when outside [0.495, 1.505]

  It then calls **FUN_1001eac0** on the StereoInterface at `StereoManager+0xb0`.
* **FUN_1001eac0** (SI::InitBackend):
  ```
  SI+4 = SI+8 = SI+0xc = 0; SI+0x1ec = SI+0x1f0 = 0;
  rt = cfg+0x2e4 ? cfg+0x2e8 : cfg+0x2ec;  SI+0x1f8 = rt;
  rt==1 -> new Oculus (0x5b8) ; rt==2 -> new OpenVR (0x620) ; else none
  if (backend && backend->Init()) {           // slot 0
      backend->RegisterControlHints();         // slot 9
      desc = SI+0x10: in_00=SI+0x1fc, in_10=0x83, in_18=2, in_14=0x70, in_04=6
      backend->GetHmdDesc(desc);               // slot 1
      copy desc.renderFov -> SI+0xac..0xcb
      SI+4 = 1;                                 // state: VR available
  }
  ```
  When Init fails, the backend object leaks and `SI+4` stays 0 (VR disabled).
* Error dialogs use FUN_1001d540 (wide) and FUN_1001d5b0 (narrow). Both are
  `MessageBox(NULL, fmt, "MotherVR 0.8.1", MB_ICONERROR(0x10))` with printf formatting.

### 2.4 OpenVR context globals (inlined `vr::VRSystem()` etc.)

This is the `COpenVRContext` from openvr.h, at `0x1006af44`:
* `+0x44` IVRSystem
* `+0x48` Chaperone
* `+0x4c` ChaperoneSetup
* `+0x50` **IVRCompositor**
* `+0x54` **IVROverlay**
* `+0x58` Resources
* `+0x5c` RenderModels
* `+0x60` ExtendedDisplay
* `+0x64` Settings
* `+0x68` Applications
* `+0x6c` TrackedCamera
* `+0x70` Screenshots
* `+0x74` DriverManager
* `0x1006af78` init token

Every interface use is preceded by the header's accessor pattern:
`if (VR_GetInitToken()!=token) { Clear() /*FUN_100067a0*/; token=VR_GetInitToken(); }`,
followed by a lazy `VR_GetGenericInterface("IVRSystem_017" | "IVRCompositor_021" | "IVROverlay_016", &err)`.

FUN_10006800 is the inlined `vr::VR_Init(&err, VRApplication_Scene=1)`. It calls
`VR_InitInternal2(&err, 1, NULL)`, clears the context, and then checks
`VR_IsInterfaceVersionValid("IVRSystem_017")`. If that fails it calls
`VR_ShutdownInternal` and sets `err = 105` (InterfaceNotFound).

### 2.5 openvr 1.0.10 vtable offsets used (vcpkg's 2.x header ordering differs; do not use it for offsets)

**IVRSystem_017**

| off | method |
|-----|--------|
| +0x00 | GetRecommendedRenderTargetSize |
| +0x08 | GetProjectionRaw |
| +0x10 | GetEyeToHeadTransform |
| +0x48 | GetTrackedDeviceIndexForControllerRole |
| +0x6c | GetStringTrackedDeviceProperty |
| +0x88 | GetControllerStateWithPose |

**IVRCompositor_021**

| off | method |
|-----|--------|
| +0x08 | WaitGetPoses |
| +0x0c | GetLastPoses |
| +0x14 | Submit |

**IVROverlay_016**

| off | method |
|-----|--------|
| +0x04 | CreateOverlay |
| +0x08 | DestroyOverlay |
| +0x58 | SetOverlayWidthInMeters |
| +0x70 | SetOverlayTextureBounds |
| +0x84 | SetOverlayTransformAbsolute |
| +0x88 | GetOverlayTransformAbsolute |
| +0xa4 | ShowOverlay |
| +0xa8 | HideOverlay |
| +0xac | IsOverlayVisible |
| +0xe4 | SetOverlayTexture |

These offsets come from the 1.0.10 ordering (SetHighQualityOverlay, overlay-relative
transforms, CaptureInputFocus and others still present). Every arg count matches the
disassembly. **[sure]** for the ones above.

Structs (Windows packing, natural alignment):
* `TrackedDevicePose_t` = 0x50
* `VRControllerState_t` = 0x40: packet@0, pressed@8, touched@0x10, rAxis[5]@0x18
* `Texture_t` = 0xc
* `VRTextureWithPose_t` = 0x3c

---

## 3. Per-slot behaviour of the OpenVR implementation

### Slot 0: `bool Init()` (100068b0) **[sure]**
1. `this+0xc = LoadLibraryW(GetCurrentDirectory()+"\\openvr_api.dll")`. On failure it
   shows the message *"Failed to load openvr_api.dll!\n[%s]..."* and returns false.
2. `CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS)` and walk the processes looking for
   `vrmonitor.exe`, compared case-sensitively. The snapshot handle is never closed.
   When it is not found the message is *"SteamVR not running!..."* and the result is false.
3. `VR_Init(&err, Scene)`:
   * err == 117 (`Init_AnotherAppLaunching`): message about "Desktop Game Theatre"
     (format `"%s | %d"` with `VR_GetVRInitErrorAsEnglishDescription`), return false.
   * any other non-zero err: the generic "Failed to initialize SteamVR!" message,
     return false.
4. Spin `Sleep(50)` until `VR_GetGenericInterface("IVRCompositor_021")` is non-null.
5. Repeat `Compositor->WaitGetPoses(this+0x10, 16, NULL, 0)` until
   `pose[0].m[0][0]+m[0][1]+m[0][2] != 0`, i.e. the HMD pose is populated.
6. Return true.

### Slot 1: `void GetHmdDesc(HmdDesc* d)` (10006b40) **[sure]**
```
Sys->GetProjectionRaw(Eye_Left,  &f[0x568], &f[0x56c], &f[0x560], &f[0x564]); // L,R,T,B
Sys->GetProjectionRaw(Eye_Right, &f[0x578], &f[0x57c], &f[0x570], &f[0x574]);
negate +0x560,+0x568,+0x570,+0x578          // -> {-T, B, -L, R} per eye (all >0 normally)
d->eyeFov[0] = {+0x560..+0x56c}; d->eyeFov[1] = {+0x570..+0x57c};
maxV = max(-T0, B0, -T1, B1);  maxH = max(-L0, R0, -L1, R1);
this+0x550 = {maxV, maxV, maxH, maxH};  d->renderFov[0] = d->renderFov[1] = same
// SteamVR-Unity-plugin crop for a side-by-side texture:
boundsL = { (0.5 + 0.5*L0/maxH)*0.5,      0.5 - 0.5*B0/maxV,
            (0.5 + 0.5*R0/maxH)*0.5,      0.5 - 0.5*T0/maxV };     // this+0x580
boundsR = { (0.5 + 0.5*L1/maxH)*0.5+0.5,  0.5 - 0.5*B1/maxV,
            (0.5 + 0.5*R1/maxH)*0.5+0.5,  0.5 - 0.5*T1/maxV };     // this+0x590
Sys->GetStringTrackedDeviceProperty(0, Prop_ManufacturerName_String(1005), this+0x5a0, 64, NULL);
Sys->GetStringTrackedDeviceProperty(0, Prop_ModelNumber_String(1001),      this+0x5e0, 64, NULL);
d->manufacturer = this+0x5a0;  d->productName = this+0x5e0;
```
T, B, L and R are the raw values. In the code the stored values are negated, so it
writes `0.5 - (stored_L*0.5)/maxH` and so on. The formulas above are the same thing.

Vertical semantics: OpenVR's raw `top` is negative and `bottom` positive, and the crop
treats `B` as the top-of-image edge. This is the well-known OpenVR top/bottom quirk.
MotherVR therefore labels `-T` as "Up" in the ovrFovPort, which for real HMDs is really
the downward tangent. The mismatch is harmless because maxV symmetrises it, and it only
matters for the SI mirror crop. **[likely]**

### Slot 2: `Sizei GetEyeTextureSize(int eye, float scale)` (10006f20) **[sure]**
`Sys->GetRecommendedRenderTargetSize(&w, &h)`. Returns
`{ (int)((float)w*scale), (int)((float)h*scale) }`, truncated with `cvttss2si`.
`eye` is ignored.

The caller is SI FUN_1001f530. It passes `scale = 0.5` when `SI+4 == 1`, otherwise
`cfg.pixel_scale (+0x2f8)`. It calls the slot for eye 0 and eye 1, then sets
`SI+0x1ec = w0+w1` and `SI+0x1f0 = (w0<=w1 ? h1 : h0)`. It also writes the per-eye
viewports `{0,0,W/2,H}` and `{W/2,0,W/2,H}` at SI+0x8c..0xa8, and returns W and H to
the game.

### Slot 3: `void CreateRenderResources(dev, ctx)` (10006fe0) **[sure]**
`Overlay->CreateOverlay("MotherVROverlay", "MotherVROverlay", &this->overlayHandle /*+0x548*/)`.
The device and context args are ignored.

### Slot 4: `void ReleaseRenderResources()` (10007060) **[sure]**
`Overlay->DestroyOverlay(this->overlayHandle)`.

### Slot 5: `void GetHeadPose(Matrix34* out, float s)` (100070e0) **[sure]**
```
Comp->GetLastPoses(this->poses /*+0x10*/, 16, NULL, 0);
M = 4x4 { rows 0..2 = poses[0].mDeviceToAbsoluteTracking, row 3 = 0,0,0,1 }
B = | -1  0  0  0 |
    |  0 -1  0  0 |
    |  0  0  1  0 |
    |  0  0  0 -s |
out = rows 0..2 of (M * B)       // FUN_10005a40: R[i][j] = sum_k M[i][k]*B[k][j]
```
Each output row `r` is `( -R[r][0], -R[r][1], +R[r][2], -s*T[r] )`.

In other words, the rotation's X and Y basis columns are negated (a 180 degree roll about
the local Z/view axis) and the translation is negated and multiplied by the world scale.
Units are metres times `s`, where `s = SI+0x204`. The output stays row-major 3x4 in
OpenVR axis order (rows are not permuted). Oculus slot 5 applies the identical `B` to
its head pose, so this is the format the SI consumer expects. **[sure]**

### Slot 6: `void GetHeadYawPose(Matrix34* out, float s)` (10007220) **[sure]**
1. `GetLastPoses` as in slot 5. Convert the HMD 3x3 to a quaternion (FUN_100056a0).
2. Compute `yaw = atan2(2(w*y + x*z), w^2-x^2-y^2+z^2)` (FUN_10006360 output 1,
   sign-corrected).
3. Build `Y = [[cos,0,sin,Tx],[0,1,0,Ty],[-sin,0,cos,Tz]]` (rotation about +Y, full HMD
   translation). It is built through a half-angle quaternion, which gives the same result.
4. Store Y unscaled at `this+0x510`.
5. `out = rows 0..2 of (Y * B)` with the same `B` as slot 5.

### Slot 7: `void BeginFrame(float eo[6], bool headTracked, int)` (100085c0) **[sure]**
```
L = Sys->GetEyeToHeadTransform(Eye_Left);  R = Sys->GetEyeToHeadTransform(Eye_Right);
eo[0..2] = L.m[0..2][3];  eo[3..5] = R.m[0..2][3];   // raw metres, NO sign change
this->cinemaMode (+0x540) = !headTracked;
```
SI FUN_1001f330 first calls slot 10. It then calls slot 7 with
`headTracked = (SI+0x208 != 0)`; `SI+0x208` is set by SI::GetHeadPose since the last
submit. SI scales the result by `s`: `SI+0x130..0x138 = s*eo[0..2]` (left) and
`SI+0xf8..0x100 = s*eo[3..5]` (right).

The eye-to-head transform is assumed to be translation-only (the rotation part is
dropped). Oculus fills the same 6 floats from `ovrEyeRenderDesc.HmdToEyePose.Position`.

### Slot 8: `void SubmitFrame(ID3D11Texture2D* tex)` (10008710) **[sure]**
Both eyes share **one** side-by-side texture (`SI+0x158`, the game's stereo render target):
```
vr::Texture_t        t   = { tex, TextureType_DirectX(0), ColorSpace_Gamma(1) };
vr::VRTextureWithPose_t tl = { t, poses[0].mDeviceToAbsoluteTracking }, tr = same;
if (!cinemaMode) {
    if (Overlay->IsOverlayVisible(h)) Overlay->HideOverlay(h);
    Comp->Submit(Eye_Left,  &tl, &boundsL /*+0x580*/, Submit_TextureWithPose(8));
    Comp->Submit(Eye_Right, &tr, &boundsR /*+0x590*/, Submit_TextureWithPose(8));
} else {
    Overlay->SetOverlayTexture(h, &t);
    Overlay->GetOverlayTransformAbsolute(h, &origin, &m);   // only to obtain origin
    m = this->yawMat (+0x510);
    m[0][3] -= 1.5f*m[0][2];   m[2][3] -= 1.5f*m[2][2];      // 1.5 m along -Z(yaw); y = head height
    Overlay->SetOverlayTransformAbsolute(h, origin, &m);
    Overlay->SetOverlayWidthInMeters(h, 3.5555556f);         // 0x40638e39 = 32/9
    Overlay->SetOverlayTextureBounds(h, {0.0, 0.25, 0.5, 0.75});  // left-eye half, middle half vertically
    Overlay->ShowOverlay(h);
    VRTextureBounds_t tiny = {0, 0, 1e-5f, 1e-5f};
    Comp->Submit(Eye_Left,  &tl, &tiny, 8);
    Comp->Submit(Eye_Right, &tr, &tiny, 8);                  // scene = 1 texel, effectively black
}
Comp->WaitGetPoses(NULL, 0, NULL, 0);                         // running start for the next frame
```
Return codes are ignored everywhere. The pose submitted with the texture is the
**latest** `GetLastPoses` result, the one used by the most recent slot 5/6 call.

After the submit, SI FUN_1001f3f0 draws the mirror to the game back buffer itself and
clears `SI+0x208`. The backend does **no** mirror handling: there is no `ShowMirrorWindow`
and no `GetMirrorTextureD3D11`.

### Slot 9: `void RegisterControlHints()` (10007500) **[sure]**
1. Assigns button-glyph names into `std::vector<std::string>` at
   `0x100686d4..d8` (stride 0x18):
   * `[0]="VR_TP_D"`, `[1]="VR_TP_C"`, `[2]="VR_TP_U"`
   * `[10]="VR_LG"`, `[11]="VR_RG"`
   * `[9]="VR_MENU"`, `[8]="VR_MENU"`

   Each index is bounds-checked and throws "invalid vector<T> subscript".
2. `push_back`s 13 help lines (`"Action\tButton"`) into the vector at `0x100698e0`:
   * Move / L Touchpad
   * Sprint / L Touchpad Click
   * Crouch / L Trigger
   * Turn / R Touchpad L/R
   * Aim / R Grip
   * Fire / Throw / Melee / R Trigger
   * Motion Tracker / L Grip
   * Interact / R Touchpad Down
   * Radial Menu / R Touchpad Center
   * Reload / Activate / R Touchpad Up
   * Flashlight [Press Interact near Head] / R Touchpad Down
   * Map Menu / L Menu
   * Pause Menu / R Menu

The Oculus variant uses "VR_RSC" and thumbstick/A/B/X/Y wording and adds Next/Previous
Weapon.

### Slot 10: `void PollInput()` (10007ad0) **[sure]** (legacy input, no IVRInput)
```
idxL = Sys->GetTrackedDeviceIndexForControllerRole(TrackedControllerRole_LeftHand=1);
Sys->GetControllerStateWithPose(TrackingUniverseStanding=1, idxL, &LS, 0x40, &LP);
idxR = ...(RightHand=2);  Sys->GetControllerStateWithPose(1, idxR, &RS, 0x40, &RP);
```
No connected or valid checks: a missing controller just yields zeros. Bits read from
`ulButtonPressed` / `ulButtonTouched`:
* bit1 = ApplicationMenu
* bit2 = Grip
* bit32 = SteamVR_Touchpad (Axis0)

Axes: `rAxis[0]` is the touchpad (x, y). `rAxis[1].x` is the trigger.

Derived values (R = right controller):
* `clickR = RS.pressed bit32`, `clickL = LS.pressed bit32`
* `touchR = RS.touched bit32`, `touchL = LS.touched bit32`
* `horiz = |R.pad.x| > |R.pad.y|`
* `center = |R.pad.x| < 0.33 && |R.pad.y| < 0.33`
* `padDown = clickR && !horiz && !center && R.pad.y < 0`
* `padUp = clickR && !horiz && !center && R.pad.y > 0`
* `padCenter = clickR && center`
* `turn`:
  * 0 when `!clickR && cfg.snap_rotation(+0x2fd)` (snap mode needs a click)
  * otherwise `(horiz && !center) ? R.pad.x*0.85 : 0`
* `near = |RP.translation - poses[0].translation| < 0.2 m`. The head comes from the last
  GetLastPoses; both are in Standing space.

Outputs go to two action-state singletons:
* **A** = FUN_10002ef0 → `0x1006a5c8`, the gameplay map, id 0x100
* **B** = FUN_10002fa0 → `0x10069f80`, the menu/frontend map, id 0x200

Each write sets `DAT_10068639 = 1` when the bool is true or `|value| > 0.85`. That flag
is a sticky "VR controller input this frame" flag, consumed elsewhere.

| target | value | game action |
|--------|-------|-------------|
| A+0x08 | clickL | Sprint |
| A+0x09 | L.trigger > 0.5 | Crouch |
| A+0x0a | R grip; when `cfg.toggle_grips(+0x306)` it flips on the rising edge (prev in `DAT_1006af41`) | Aim |
| A+0x0b, A+0x0e | R.trigger > 0.5 | Fire / Throw / Melee |
| A+0x0c, A+0x0f | padDown | Interact |
| A+0x0d | padCenter | Radial menu |
| A+0x10, A+0x11 | padUp | Reload / Activate |
| A+0x1c | toggle mode: L grip rising-edge flip (prev in `DAT_1006af40`); else `Lgrip && !padCenter` | Motion tracker |
| A+0x1d | near && padDown | Flashlight |
| A+0x26 | L AppMenu (bit1) | Map |
| A+0x27 | L grip raw | (none) |
| A+0x28 | R grip raw | (none) |
| A+0x2c / A+0x30 | touchL ? L.pad.x*1.1 / L.pad.y*1.1 : 0 | Move |
| A+0x38 | turn | Turn |
| B+0x00, B+0x10 | padDown \|\| clickL | menu confirm [likely] |
| B+0x01 | padCenter | |
| B+0x08 | R AppMenu (bit1) | Pause |
| B+0x09 | L grip raw | |
| B+0x0a, B+0x0f | padUp | menu back [guess] |
| B+0x0b, B+0x0e | near ? padDown : 0 | |
| B+0x2c / B+0x30 | L.trigger / R.trigger (raw analog) | |
| B+0x34 / B+0x38 | touchL ? L.pad.xy : 0 | |
| B+0x3c / B+0x40 | touchR ? R.pad.xy : 0 | |

The controller poses LP and RP are used **only** for the flashlight proximity test.
There is no 6DOF hand tracking in 0.8.1. Snap rotation itself is not done in the
backend: the backend only gates `turn`, and the consumer of A+0x38 does the snapping.
There is no haptics.

### Recentering
None in the OpenVR backend:
* no `ResetSeatedZeroPose`
* no `SetTrackingSpace`
* no event handling

Poses are Standing-universe, so the floor is at y=0. Oculus, by contrast, sets
`ovrTrackingOrigin_EyeLevel`, recenters in slot 1, and recenters again in slot 10 on
`ShouldRecenter`.

---

## 4. Per-frame call order (as driven by SI and AI.exe)

```
SI vt#5  FUN_1001f330  -> backend.PollInput()            (slot 10)
                       -> backend.BeginFrame(eo, SI+0x208!=0, 1)   (slot 7)
SI vt#11 FUN_1001f2c0  -> backend.GetHeadPose(out, SI+0x204)     (slot 5; SI+0x208 = 1)
SI vt#21 FUN_1001f300  -> backend.GetHeadYawPose(out, SI+0x204)  (slot 6)
   ... game renders both eyes into the SBS target ...
SI vt#6  FUN_1001f3f0  -> backend.SubmitFrame(SI+0x158)          (slot 8) ; mirror blit ; SI+0x208 = 0
```
Note the one-frame lag: `headTracked`, and therefore cinema mode, is decided in
BeginFrame from whether GetHeadPose was called **since the previous submit**.

`SI vt#3` (FUN_1001eda0) runs on render-target (re)creation and calls slot 3.
`SI vt#4` (FUN_1001f2b0) calls slot 4.
`SI vt#16` (FUN_1001f530) calls slot 2 twice to size the SBS target.

---

## 5. Reconstruction-ready interface

```cpp
// recon/src/runtime/hmd_backend.h  (proposed)
#pragma once
#include <cstddef>
#include <cstdint>
struct ID3D11Device; struct ID3D11DeviceContext; struct ID3D11Texture2D;

namespace mothervr::runtime {

struct Matrix34 { float m[3][4]; };
struct Sizei    { std::int32_t w, h; };
struct FovPort  { float up, down, left, right; };
struct HmdDesc  {                          // must keep this exact layout (lives in SI+0x10)
    std::uint32_t in_00, in_04;
    const char*   productName;             // +0x08
    const char*   manufacturer;            // +0x0c
    std::uint32_t in_10, in_14, in_18;     // +0x10..0x18
    std::uint8_t  _1c[0x10];
    FovPort       eyeFov[2];               // +0x2c
    FovPort       renderFov[2];            // +0x4c
};
static_assert(offsetof(HmdDesc, eyeFov) == 0x2c && offsetof(HmdDesc, renderFov) == 0x4c);

// Vtable order == original. No virtual destructor in the original. Adding one
// is fine as long as it is declared LAST, because no foreign code calls this vtable.
class HmdBackend {
public:
    virtual bool  Init() = 0;                                                    // 0  ret
    virtual void  GetHmdDesc(HmdDesc* desc) = 0;                                 // 1  ret 4
    virtual Sizei GetEyeTextureSize(int eye, float pixelScale) = 0;              // 2  ret 0xc
    virtual void  CreateRenderResources(ID3D11Device*, ID3D11DeviceContext*) = 0;// 3  ret 8
    virtual void  ReleaseRenderResources() = 0;                                  // 4  ret
    virtual void  GetHeadPose(Matrix34* out, float worldScale) = 0;              // 5  ret 8
    virtual void  GetHeadYawPose(Matrix34* out, float worldScale) = 0;           // 6  ret 8
    virtual void  BeginFrame(float eyeOffsets[6], bool headTracked, int unused) = 0; // 7 ret 0xc
    virtual void  SubmitFrame(ID3D11Texture2D* sbsTexture) = 0;                  // 8  ret 4
    virtual void  RegisterControlHints() = 0;                                    // 9  ret
    virtual void  PollInput() = 0;                                               // 10 ret
    virtual ~HmdBackend() = default;                                             // recon-only, slot 11
};

// Shared helper: both runtimes run the HMD pose through this.
inline Matrix34 to_game_pose(const float r[3][4], float s) {
    Matrix34 o;
    for (int i = 0; i < 3; ++i) {
        o.m[i][0] = -r[i][0]; o.m[i][1] = -r[i][1]; o.m[i][2] = r[i][2]; o.m[i][3] = -s * r[i][3];
    }
    return o;
}
} // namespace
```
Because the backend is only ever called from MotherVR's own code (SI), the vtable does
not need to be binary-compatible. Exact ordering only matters if the recon keeps a
literal SI port that indexes it. All struct layouts that SI reads (`HmdDesc`, the 6-float
eye offsets, `Matrix34`) must be kept.

### 5.1 OpenVR implementation notes (`OpenVrBackend`)
* Request **`IVRSystem_017`, `IVRCompositor_021`, `IVROverlay_016`** from the bundled
  1.0.10 `openvr_api.dll`. Use the version strings literally; the recon's
  `vr::IVRSystem_Version` is `_026`.
  * Calling these through the vcpkg 2.x `vr::IVRSystem` class would hit the
    **wrong vtable slots**. For example, in 2.x `+0x10` is `ComputeDistortionSet`, not
    `GetEyeToHeadTransform`, and `+0x88` is `GetHiddenAreaMesh`, not
    `GetControllerStateWithPose`.
  * Either vendor a 1.0.10 `openvr.h`, or call through hand-written vtable-offset thunks.
  * Alternative: request the current versions (`IVRSystem_022` etc.) from SteamVR's own
    loader with the 2.x header. SteamVR still serves 017/021/016, so both approaches work.
* `Init`: as in 3/slot 0. The recon's `openvr_backend.cpp` already does the loading
  (prefers the bundled DLL, adds `SetDllDirectory`). Add:
  * the vrmonitor check, or replace it with `VR_IsHmdPresent`
  * the compositor wait loop
  * the first-valid-pose loop
* `GetHmdDesc`: copy the formulas in 3/slot 1 verbatim, including the crop bounds.
* `SubmitFrame`: identical `Texture_t` / `VRTextureWithPose_t`, flag 8, Gamma color space.
  * The game texture is presumably `R8G8B8A8_UNORM` holding sRGB-encoded data. **[likely]**
  * The cinema overlay geometry is in 3/slot 8.
  * End with `WaitGetPoses(nullptr,0,nullptr,0)`.
* `PollInput`: keep legacy `GetControllerStateWithPose` for exactness. An IVRInput
  action-manifest port is possible, but it is a behavioural change.

### 5.2 OpenXR implementation (`OpenXrBackend`) mapped onto the recon's `openxr_backend.h`

| slot | OpenXR equivalent |
|------|-------------------|
| 0 Init | `openxr::probe()`, then create the instance and get the system. The current `openxr::init(device)` needs the game's device. The original calls slot 0/1 before slot 3 has a device. Two options: (a) obtain the device earlier from the d3d11 proxy's `D3D11CreateDevice` hook, and create the session in Init; or (b) defer session creation to slot 3 and have slot 1 return provisional FOV. For (b), use `XR_EPIC_view_configuration_fov` `recommendedFov` when the runtime exposes it (SteamVR does **[likely]**), or `xrLocateViews` once the session exists. |
| 1 GetHmdDesc | Per eye from `XrFovf`: `up = tan(angleUp)`, `down = -tan(angleDown)`, `left = -tan(angleLeft)`, `right = tan(angleRight)`. Store these straight into `eyeFov`; OpenXR has no top/bottom quirk. `maxV`/`maxH` are computed as in OpenVR. Strings come from `XrSystemProperties::systemName` and `XrInstanceProperties::runtimeName`. Keep the char buffers in the object. |
| 2 GetEyeTextureSize | `XrViewConfigurationView[eye].recommendedImageRectWidth/Height * scale`, truncated. |
| 3 CreateRenderResources | Create the session if deferred, then create the swapchains. Either one SBS swapchain of the game's WxH or two per-eye ones. |
| 4 ReleaseRenderResources | Destroy the swapchains and the quad swapchain. |
| 5 GetHeadPose | `xrLocateSpace(VIEW, base, predictedDisplayTime)`, convert the pose to a 3x4 (OpenXR is also right-handed, +Y up, -Z forward, metres, so the axes match OpenVR), then `to_game_pose()`. Base = STAGE to match OpenVR Standing, or LOCAL plus a floor offset. Note the recon's `openxr_backend` currently uses LOCAL/"seated". |
| 6 GetHeadYawPose | Same yaw extraction and `Y` construction. Cache `Y` for the cinema quad. |
| 7 BeginFrame | Fill `eo` from `xrLocateViews(..., space = VIEW)`: `views[i].pose.position` is the eye-in-head offset. Equivalent to `GetEyeToHeadTransform`. Set `cinema = !headTracked`. |
| 8 SubmitFrame | Copy `tex` into the swapchain image(s) with `CopySubresourceRegion`. Stereo: an `XrCompositionLayerProjection` with one view per eye. `subImage.imageRect` is the **full eye half** (`{0,0,W/2,H}` and `{W/2,0,W/2,H}`). `fov` is the **symmetric** render frustum: `angleLeft = -atan(maxH)`, `angleRight = atan(maxH)`, `angleDown = -atan(maxV)`, `angleUp = atan(maxV)`. `pose` = the eye pose the game rendered with: head pose from slot 5 composed with the eye offset. That replaces the OpenVR bounds crop and is exact. Cinema: an `XrCompositionLayerQuad` with `size = {3.5556, 3.5556*H/W}`. The {0.5x0.5} region of the SBS texture is W/2 x H/2 pixels, so its aspect is W:H; OpenVR keeps the texel aspect implicitly. Its `pose` = yaw matrix translated 1.5 m along -Z(yaw), and `subImage.imageRect = {0, H/4, W/2, H/2}`. Then `xrEndFrame`, followed immediately by `xrWaitFrame` + `xrBeginFrame` for the next frame. This reproduces the running start; slot 5 then uses that frame's `predictedDisplayTime`. The existing `openxr::frame()` / `set_eye_images()` API can host this: `set_eye_images` = stereo path, `clear_eye_images` = screen path. The screen path then needs the 1.5 m / 3.5556 m layout and the {0,0.25,0.5,0.75} region instead of the full back buffer. |
| 9 RegisterControlHints | Choose the text set from `openxr::input::ControllerFamily`: Touchpad means the OpenVR strings, Thumbstick means the Oculus strings. |
| 10 PollInput | `openxr::input::sync()`, then map `HandState` to the A/B fields in 3/slot 10. Touchpad family: `stick` = pad, `stick_click` = pad click. Thumbstick family: follow the Oculus slot-10 mapping. `touched` has no direct OpenXR equivalent, so use the `.../trackpad/touch` action or treat `stick != 0` as touched. Proximity uses the grip pose vs the VIEW pose (< 0.2 m). Recenter: `openxr::recenter()` on `XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING`, mirroring Oculus `ShouldRecenter`. |

Uncertainties:
1. The meaning of the `HmdDesc` "in" fields (0x83, 0x70, 2, 6, SI+0x1fc). Neither
   backend reads them.
2. The exact game semantics of the A/B byte offsets. They are inferred from the help
   strings and the order of the writes.
3. Whether cinema mode is meant purely for "not head-tracked" frames or also for the
   SI state==1 path that uses scale 0.5. The 0.5 scale suggests both coincide.
