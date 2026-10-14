# 06c: FUN_1001d6f0 detours and byte patches (what each one does in AI.exe)

Scope: every AI.exe write made by the hook installer `FUN_1001d6f0` that was not reconstructed
yet, plus the DLL functions those writes route to. All AI.exe addresses are **2015 build VAs**
(image base 0x400000; RVA = VA - 0x400000) unless they are marked otherwise. Both RVA columns
come from `recon/src/hook/ai_address_tables.inc`.

Confidence tags: **[sure]** means the disassembly shows it directly. **[likely]** means a strong
inference. **[guess]** means a plausible reading that needs runtime confirmation.

---

## 0. The key insight: MotherVR reuses the game's dormant Oculus Rift path

AI.exe 2015 has a complete built-in LibOVR 0.4 (DK2) integration. The LibOVR code is statically
linked (`OVR::CAPI::HMDState`, `OVR::HMDDevice`, ...). It is gated by graphics setting
**#0x18 "Stereo Mode" == "Rift"** (string at 0x13e5010). The game-side objects are:

| Game object | Where | Notes |
|---|---|---|
| **Stereo manager** (0xB0 bytes, ctor 0x9d9a60, vtable 0x13e57bc = {dtor, OnSettingChanged 0x9ccdb0, IsSettingAllowed 0x9cceb0}) | `renderer+0xe68c`; global ptr `[0x1766a2c]` (table slot 0x63) | `+4` HMD device*, `+8/+0xc` UI render targets, `+0x20` 4x4 virtual-screen world matrix, `+0x60` current stereo mode, `+0x64` scene visibility multiplier (1 = visible, 0 = black), `+0x68/+0x6c` mode stack (data*, count), `+0x78..+0xa7` 3x4 recenter/base transform, `+0xa8` fade target bool, `+0xac` fade alpha |
| **HMD device** (0x1F4 bytes, ctor 0x9be9c0, vtable 0x13e5418, 21 slots) | `stereo+4` | `+4` state (0 none, 1 init, 2 tracking running), `+0xac` eye FovPorts[2], `+0xcc` ovrEyeRenderDesc[2] (ViewAdjust at +0xf8/+0x130), `+0x8c..+0xa8` eye viewports, `+0x1b4` eye pose cache, `+0x1ec/+0x1f0` RT size |
| Stereo mode stack | `Stereo::PushMode(int)` 0x9d9c00 (thiscall, ret 4), `Stereo::PopMode()` 0x9d9c80 (thiscall, ret) | mode 0 = 3D world + head-locked HUD quad; mode 1 = "virtual screen" (menus and cinematics drawn on a world-fixed quad) |
| `Stereo::SetMode(int)` 0x9cd0a0 | called by push and pop | for mode 1 it builds `+0x20` with 0x9be550 |
| `Stereo::Update(float dt)` 0x9be2a0 | called each frame from 0x7b4100 | LB+RB = recenter; animates the fade alpha `+0xac` |
| `Stereo::DrawOverlay()` 0x9cd170 | | draws the UI render target per eye on a quad (mode 0: HUD matrix built each frame; mode 1: `+0x20`) |
| VR camera behaviour "VRCameraBehavior" (vtable around 0x12f19d8, GetName 0x745110, update 0x74de50) | | head-in-wall fade and a camera correction offset (see bdc/be0) |

MotherVR does the following:
1. It replaces the game's stereo object with its own `StereoManager` (DAT_1006ac18, patch **c9c**). The
   first 0xB0 bytes are layout-compatible. Its embedded `StereoInterface` at `+0xb0` stands in for the HMD
   device, and its vtable mirrors the game's 21-slot device vtable plus a 22nd slot, `+0x54`.
2. It forces every "Stereo Mode == Rift" test true (the 8 `39 c0` patches, already reconstructed).
3. It reroutes direct (non-virtual) calls to the game's stereo methods to its own versions (d64..d90).
4. It disables or retunes Rift-era behaviours that are wrong for modern VR (byte patches).

**Correction to the brief:** none of the call-redirect detours injects the HMD pose into the
camera. They handle init, the per-frame update (recenter, snap turn, blackout), UI quad placement,
RT sizing and the mode stack. Head-pose ingestion goes through the StereoInterface slots, which are
a separate subsystem: the game calls `device->vfunc[...]`. Notably, MotherVR's slots +0x1c
(GetEyePose) and +0x20 (EndEyeRender) are the no-op `ret 4` 0x10001f20. Slots +0x30..+0x3c tail-jump
back into the **game's own** implementations (DAT_10066cd8..ce4 = 0x9cd4c0/0x9b8e70/0x9b8f10/0x9b9000),
which then run on MotherVR's data (`SI+0xac` FovPorts, `SI+0xf8/+0x130` eye offsets). The pose path is
**unresolved here** and needs its own pass (06x StereoInterface).

### Camera / view-matrix conventions (needed by the reconstruction)
- POV manager: `[0x1766a00]` (table slot 0x62, `DAT_10066d50`). `[mgr+8]` = POV[0] (main view). **POV[0]
  starts with a 4x4 float world→view matrix** (no vtable in front of it). `[mgr+0x88]` = POV count.
- The matrix uses D3D row-vector convention, left-handed, units are **metres**:
  `V = [[r.x u.x f.x 0] [r.y u.y f.y 0] [r.z u.z f.z 0] [-p·r -p·u -p·f 1]]`.
  Columns 0/1/2 are the camera right/up/forward axes in world space; row 3 is the view translation.
  So camera world position = `(-dot(row0,row3), -dot(row1,row3), -dot(row2,row3))`, and world
  forward = column 2 = `(M[2], M[6], M[10])`. This is proven by FUN_1001ffd0, whose output translation
  is `campos + forward*1.0`, and whose `up × forward = right`, which only holds for left-handed axes.
- A "world matrix" built for a UI quad uses the same row-vector convention: rows 0..2 = the quad's
  local axes in world space (already scaled), row 3 = position.
- First-person look is the **FollowBehavior** camera (vtable 0x11c6600; `GetName` slot 14 returns
  "FollowBehavior"). Yaw is at `+0x204` and its mirror at `+0x1fc`. Both are normalised so that ±1.0 =
  ±180° and the value wraps at ±1 (the game wraps it with 0x41d990). Pitch is at `+0x208`, clamped.
  Slot 11 (0x42d200) applies stick or mouse look deltas only while byte `+0x264` ("look input
  enabled") is set. Slot 0 (0x42c930) is OnActivate(arg) and slot 1 (0x42bbf0) is OnDeactivate(arg),
  both `ret 4`.

---

## 1. Summary table

`slot` = index into the 127-entry table (`DAT_10066bc8 + 4*slot`). "RVA" columns give the address
the installer writes at (the site), not counting the `+N` offsets shown in the Site column.

| Site (global+off) | slot | RVA 2015 | RVA 2020 | Payload | Effect in game | Purpose |
|---|---|---|---|---|---|---|
| c5c | 0x25 | 0x575421 | 0x4C71D1 | `eb 29` | SSAO-setting handler (setting #0x15) always stores mode = raw value and `enabled(+0xd84)` = 0 | disables the SSAO pass [likely] |
| bdc+3 | 0x05 | 0x346E94 | 0x35794E | `&SM+0x30c` (disp32) | `comiss dist,[1e-5]` threshold in head-in-wall fade now reads SM+0x30c | "Clipping blinders" option: 1e-5 (on) or 1.0 (off) |
| bd8 | 0x04 | 0x2603B1 | 0x270C61 | `eb 05` | skips `call 0x64bf50` (procedural camera noise) | removes camera sway/breathing [likely] |
| bd4 | 0x03 | 0x02D83C | 0x03D0BC | `b1 01 eb 00` | FollowBehavior `+0x261` forced to 1 | keeps one look-input mode branch always on [guess] |
| be0 | 0x06 | 0x34A0CE | 0x35ACCE | `e9 2c 03 00 00 90` | `jbe` becomes `jmp` to the same target, so the VR camera correction vector is always (0,0,0) | stops game-side camera push-back/neck correction [likely] |
| bd0+8 | 0x02 | 0x02C930 | 0x03C1A0 | 6× `90` | FollowBehavior::OnActivate no longer sets `+0x264 = 1` | game stick or mouse look is off unless MotherVR enables it |
| d2c / d2c+4 | 0x59 | 0xDC6600 | 0xDDD6E8 | FUN_10001720 / FUN_10001740 | FollowBehavior vtable slots 0 and 1 | track the active FollowBehavior instance and its active state |
| d30 | 0x5A | 0xDFCE18 | 0xE13F38 | FUN_1001f830 | UI entity message handler (thiscall, ret 0x10) | watches `cutscene_visibility_*` and `level_fade_*` |
| c28+4 | 0x18 | 0x443564 | 0x454704 | `ec` (2015) / `9c` (2020) | mouse-cursor centring `movss xmm2,[W]` now reads the UI reference width 1280.0 | mouse-to-UI mapping in a 1280x720 space |
| c2c+4 | 0x19 | 0x4435C2 | 0x454762 | `f0` / `a0` | same for height, now 720.0 | " |
| c30 | 0x1A | 0x43EB3A | 0x44FCFA | `eb` | `je` becomes `jmp`: on UI input event 0x12, always switches the menu to state 2 | menus always use pad/focus navigation [guess] |
| c9c / +1 / +5 | 0x35 | 0x5AFBA1 | 0x501CC1 | `b8` + `&SM` + `eb 1d` | replaces `new(0xB0)` + `Stereo::Stereo()` with `mov eax,&StereoManager` | MotherVR's StereoManager becomes the game's stereo object |
| ca0+2 | 0x36 | 0x5BE2E8 | 0x5104A8 | `54` | `call [dev+0x2c]` becomes `call [dev+0x54]` in Stereo::Update (LB+RB recenter) | recenter goes to SI slot 0x54 = FUN_1001f300 |
| ca4+2 | 0x37 | 0x5B8691 | 0x50A831 | `54` | same in `Stereo::ResetOrientation()` 0x9b8680 (menu "reset view") | " |
| d94 | 0x73 | 0x453682 | 0x4649B2 | `eb 09`+9 nop | removes `Stereo::PopMode()` in front-end "Resume_game" (0x8535d0) | mode stack handled by the menu wrappers |
| d98 | 0x74 | 0x43E545 | 0x44F705 | `eb 0b`+11 nop | removes `Stereo::PushMode(1)` in the in-game "Pause" handler (0x83e520) | " |
| dac | 0x79 | 0x43B01C | 0x44C1CC | `53` | `push ecx` becomes `push ebx` (=0): `SleepEx(33-elapsed)` becomes `SleepEx(0)` | removes a ~30 Hz throttle in a loader/state loop [likely] |
| db4 | 0x7B | 0x60EE6F | 0x56133F | `b1 01 90` | `setne cl` becomes `mov cl,1`: XInput pad `+0x289 connected` is always 1 | the game keeps polling the "gamepad", which MotherVR feeds with VR controllers |
| d64 | 0x67 | 0x613941 | 0x565E11 | call→FUN_1001f6a0 | replaces `Stereo::Init(1)` 0x9e0d40 | creates the VR backend instead of the DK2 device |
| d68 | 0x68 | 0x3B4100 | 0x3C4FB0 | call→FUN_1001f8a0 | replaces `Stereo::Update(dt)` 0x9be2a0 | recenter, snap turn, look gating, blackout |
| d6c | 0x69 | 0x5CD132 | 0x51F2A2 | call→FUN_1001fc20 | replaces 0x9be550 in `SetMode(1)` | virtual-screen placement |
| d70 | 0x6A | 0x5CD2EE | 0x51F45E | call→FUN_1001ffd0 | replaces 0x9be550 in `DrawOverlay` mode 0 | head-locked HUD quad placement |
| d74 | 0x6B | 0x60FE5D | 0x56233D | call→FUN_1001f640 | replaces `Stereo::GetDisplaySize` 0x9b8780 | keeps the window size (no 1280x800 forcing) |
| d78 | 0x6C | 0x60FD64 | 0x562244 | call→FUN_100202a0 | replaces `Stereo::GetRenderTargetSize` 0x9b8750 | side-by-side RT sized for the HMD |
| d7c/d80/d84 | 0x6D-0x6F | 0x40EB8E / 0x43EECA / 0x44197A | 0x41FD4E / 0x45008A / 0x452B2A | call→FUN_100201e0 | replaces `PushMode(m)` | no stack growth; re-places the screen |
| d88/d8c/d90 | 0x70-0x72 | 0x413DDA / 0x416382 / 0x424C2A | 0x42508A / 0x4275D2 / 0x435DCA | call→FUN_10020220 | replaces `PopMode()` | pops only from mode 1 |
| db8+1, db8+0xed | 0x7C | 0x42D0A0 (+0xEC) | 0x43E1F0 | call→**FUN_10003100 (both)** | replaces `IsMoveOnLeftStick()` 0x97dce0 | correct lstick/rstick UI prompts |
| dbc | 0x7D | 0x43AB40 | 0x44BCF0 | call→FUN_10003120 | redirects `GetControlPromptName` 0x82cff0 to a pure pass-through | no-op hook (placeholder) |
| d9c+8, da0, da0+8, da4, da4+8, da8, da8+8 | 0x75-0x78 | .rdata vtables | | FUN_10020c80..dd0 | menu enter/exit wrappers | stereo mode push/pop and scene blackout |

**Version dependence:** every row uses the per-build address table, so all of them depend on the
version. In addition, two things are coded inline in FUN_1001d6f0: (a) the 8-site `39 c0` RVA arrays,
where `param_2 == 0` selects the first array, which is the 2015 build; (b) the c28/c2c byte. 2015 writes
`ec`/`f0`; 2020 (param_2 != 0) writes `9c`/`a0`. In 2015 this is the low byte of disp32
0x17588dc→0x17588ec and 0x17588e0→0x17588f0, which moves from struct 0x17588b0 fields `+0x2c/+0x30`
(backbuffer W/H) to `+0x3c/+0x40` (constant 1280.0/720.0, set by 0xa10360). For 2020 the same
+0x10 shift is assumed [likely; 2020 exe not checked].

**Brief correction (db8):** both rel32 writes at db8 resolve to **0x10003100**. Arithmetic: site+1
gets `0x100030fb - site`, the next IP is site+5, so the target is 0x10003100. site+0xed gets
`0x1000300f - site`; that is the rel32 of a second `E8` at site+0xec (0x82d18c `call 0x97dce0`),
so the next IP is site+0xf1 and the target is 0x1000300f + 0xf1 = **0x10003100**. 0x10003014 lies in the
middle of an instruction and is not a target.

Other +5 checks hold: UNK_x + 5 gives 0x1001f6a0, 0x1001f8a0, 0x1001fc20, 0x1001ffd0, 0x1001f640, 0x100202a0,
0x100201e0, 0x10020220 and 0x10003120.

---

## 2. Byte patches in detail

### c5c: SSAO setting handler (0x975400) [likely]
```
00975400 OnSettingChanged(SettingEvent* e)  ; thiscall, ret 4, renderer sub-object
  if (e->id != 0x15 /*"Screen Space Ambient Occlusion"*/) return;
  v = GetSetting(e)->value_index (+0x50)
  0x975421: jne +0x10  -> patched: jmp 0x97544c
  v==1: d80=1, d84=1 | v==0: d80=0, d84=1 | v==2: d80=2, d84=0
patched: always   [esi+0xd80] = v; [esi+0xd84] = 0
```
`+0xd84` gates the AO compute pass 0x993af0 (called from 0x9b04a1 only if `d84`). `+0xd80` picks
which AO texture the resolve uses (0x9752a9: id 0x402 → `+0xc70` / `[d7c]+0xac` / `[0x1bdf610]`).
Result: the screen-space AO pass never runs, whatever the setting. Screen-space AO on a double-wide
stereo RT bleeds across eyes, and it is also expensive. Setting indices come from the name table at
0x164fc20: 0 DoF, 7 VSync, 9 AA, 0xb ShadowMapRes, 0xc ShadowMapping, 0xd MotionBlur, 0x10 Planar
Reflections, 0x12 Volumetric Lighting, 0x15 SSAO, 0x16 Texture Filtering, 0x18 Stereo Mode.

### bdc+3: head-in-wall blackout threshold (in 0x746de0, VRCameraBehavior) [sure for mechanics]
```
00746de0 float __stdcall HeadClipFade(const vec3* eye, const vec3* pivot)  ; ret 8, result in ST0
  d     = |eye - pivot|
  near  = pow(1 - clamp((d - 0.5)*10, 0, 1), 5)          ; 1 below 0.5 m, 0 above 0.6 m
  0x746e94: comiss d, [0x11c68a8] (=1e-5)  -> operand now [&StereoManager+0x30c]
  if (d > threshold) {                                    ; raycast-ish occlusion along eye-pivot
      t = 0x7fa450(pivot, pivot + normalize(eye-pivot)*0.1, ...)
      occl = pow(clamp((t-0.8)*10,0,1)..., 5); near *= occl }
  return (flags & [0x12f3018] ? 0 : 1) * near
caller 0x74e2c1: [[0x1766a2c]+0x64] = HeadClipFade(...)   ; stereo scene visibility
```
`SM+0x30c` = 0x3727c5ac (1e-5, identical to the game constant) when `clipping_blinders` (SM+0x2fc) is
on, and 1.0f when it is off (FUN_1001d6f0 line 135 at install; FUN_100202e0 on `mvr_clippingblinders`).
With blinders off, the occlusion test only runs when the head is more than 1 m from the pivot, so
leaning into walls no longer blacks out the view. (The `near` factor still applies above 0.5 m.) `+0x64`
multiplies the scene output (read at 0xa10187, used as a colour/viewport scale in the stereo resolve).

### bd8: skip 0x64bf50 in camera update 0x660320 [likely]
`mov ecx,esi; call 0x64bf50` (7 bytes) is jumped over. 0x64bf50 ramps a blend `+0x12c` and samples two
procedural channels through the character's anim/input component (`[char+0x36c]->vf44(n,&a,&b)`). It
writes camera offsets `+0xe0/+0xe4/+0xe8/+0xec` = amplitude (`+0x80..+0x8c`) × channel × blend. Skipping
it freezes those offsets (at 0), which removes idle sway, breathing and handheld noise from the camera.
That motion is nauseating in VR.

### bd4: FollowBehavior state byte +0x261 forced to 1 (in 0x42d600) [guess]
0x42d600 recomputes the FollowBehavior state bytes (`+0x258..+0x263`). It snapshots the previous
state and, at 0x42d83c, `mov cl,[esp+0xf]` (the old `+0x25e`) sets `+0x261`. The patch makes it
`mov cl,1; jmp $+2`. `+0x261` is read at 0x42bfad (in 0x42bc50, the look/aim integrator called
from activate and update): `if (+0x260 || +0x261) allow = 1`. So the second look-integration branch
is always enabled. The exact meaning (probably "free look / not locked by animation") is unconfirmed.

### be0: VR camera correction always zero (0x74a050) [likely]
`0x74a050` (thiscall, ret 0x10: `vec3* out = f(a, vec3* A, vec3* B, vec3* out)`) is called from
VRCameraBehavior update 0x74de50 at 0x74e1b9. It zeroes `out` and then
`if (|A.y-B.y| <= 0.999) { ...compute correction... }`. The 6-byte `jbe rel32` at 0x74a0ce becomes `jmp rel32`
plus `nop`, with the same target 0x74a3ff (epilogue). So `out` stays (0,0,0). The game's DK2-era body/neck
correction against head displacement is disabled.

### bd0+8: FollowBehavior::OnActivate no longer enables look input [sure]
In 0x42c930, `mov [esi+0x264], al` (al=1, 6 bytes at +8) is NOP'ed. The ctor and OnDeactivate both
clear `+0x264`, so the game's own stick/mouse look (slot 11 0x42d200, gated on `+0x264`) stays off
unless MotherVR's Stereo::Update replacement (FUN_1001f8a0) sets it. It sets it when snap rotation is off
or keyboard/mouse is in use.

### d2c / d2c+4: FollowBehavior vtable slots 0 and 1 [sure]
These are FUN_10001720 and FUN_10001740. See section 3.

### d30: UI entity message handler (vtable 0x11fce10, slot 2 = 0x91bd70) [sure]
FUN_1001f830. See section 3.

### c28+4 / c2c+4: mouse cursor centring in a 1280x720 UI space [sure for mechanics]
0x843520 (UI mouse update): `cursor = (mouse.x - W*0.5, -(mouse.y - H*0.5))` → UI coordinates via
0x8045e0. W and H were the backbuffer size (struct 0x17588b0 `+0x2c/+0x30`). After the patch they
are the constant reference resolution 1280.0/720.0 (`+0x3c/+0x40`). With MotherVR the backbuffer is the
double-wide stereo RT, so using its size would put the cursor off-centre and wrongly scaled on the UI
quad.

### c30: UI input event 0x12 always selects pad mode (0x83eb00) [guess]
0x83eb00 is a menu input-mode event handler (thiscall, ret 8). For event 0x12 the original skipped
`0x834e90` if the keyboard/mouse device (`[0x1759b44]+0x4a74`) was present and its flags
`+0x10 && +0x11` were set. `0x834e90` sets `+0xd0 = 2` and `setState(2)` on the Scaleform movie.
Events 0x15 and 9 do the reverse: `+0xd0 = 0`, `setState(0)`, and show the cursor via 0x97d2a0(1).
Patching `je` to `jmp` makes event 0x12 always switch to state 2 (gamepad/focus navigation). That
matches VR controllers being presented as a gamepad.

### c9c: hijack the stereo object allocation (in renderer init 0x9afb??..0x9afbc5) [sure]
Original:
```
9afba1 68 b0000000        push 0xB0
9afba6 89be98f60c00       mov [esi+0xcf698], edi   ; edi = 0
9afbac 89be9cf60c00       mov [esi+0xcf69c], edi
9afbb2 e8 ...             call operator new
9afbb7 ..9afbc0           if (p) p = Stereo::Stereo(p) (0x9d9a60)
9afbc5 89868ce60000       mov [esi+0xe68c], eax
```
Patched: `b8 <&DAT_1006ac18>` (mov eax, &StereoManager) then `eb 1d` (jmp 0x9afbc5). Side effect: the two
zero stores to `+0xcf698/+0xcf69c` are skipped. A reimplementation should either keep them or ensure
the renderer memory is already zero. MotherVR's FUN_10003510 must therefore initialise the
game-visible first 0xB0 bytes as the game ctor would: `+0x20` identity, `+0x60` 0, `+0x64` 1.0,
`+0xa8` 1, `+0xac` 1.0. One difference: MotherVR's `+0x78` base is diag(-1,-1,1) (180°) instead of the
identity, and its vtable has only 3 slots (`10001f20` ret 4 / `10020230` / `10020290`).
- `FUN_10020230(SettingEvent* e)` replaces 0x9ccdb0. If `e->id == 0x18` and a device exists, it calls
  `device->vf4()` (start tracking) and forces DoF=Off, SSAO=Off, MotionBlur=Off and VSync=On via
  `FUN_10001ce0`→0x9a8910. The game's own Rift preset forced 9 settings.
- `FUN_10020290` replaces 0x9cceb0 (IsSettingAllowed) and returns true.

### ca0+2 / ca4+2: recenter goes to StereoInterface slot +0x54 [sure]
Game device slot `+0x2c` (0x9e0da0) = Rift recenter: it reads `ovrHmd_GetTrackingState`, takes the yaw,
and writes the 3x4 base transform to `stereo+0x78`. Both call sites (0x9be2e8 in Update when LB+RB are
pressed; 0x9b8691 in `Stereo::ResetOrientation()` 0x9b8680, called from 0x491bd7 [menu reset view]) now call
**slot +0x54 = FUN_1001f300**:
`backend(SI+0x1f4)->vf18(out = SM+0x78, worldScale = SI+0x204)`, returning `out` (thiscall, ret 4).
MotherVR's own slot +0x2c (FUN_1001f2c0) has a different meaning (`backend->vf14(arg, scale);
SI+0x208 = 1`), which is why the call offset had to change.

### d94 / d98: remove two stray stack operations [sure]
- d94 (0x853682, front-end class slot 4 0x8535d0 "Resume_game"):
  `mov ecx,[0x1766a2c]; call PopMode` (11 bytes) becomes `jmp +9`.
- d98 (0x83e545, pause-menu class slot 3 0x83e520 "Pause"):
  `mov ecx,[0x1766a2c]; push 1; call PushMode` (13 bytes) becomes `jmp +0xb`.

The menu vtable wrappers (section 4) now own push and pop for these screens.

### dac: un-throttle a state loop (0x83af50) [likely]
In 0x83af50 (`while ([0x174a795]) switch ([0x174a4c8])` loop), the state-3 branch measures
`elapsed = GetTickCount() - last` and calls `SleepEx(33 - min(elapsed,33), TRUE)`. Patching `push ecx`
to `push ebx` (ebx = 0 throughout) makes it `SleepEx(0, TRUE)`. This removes a 30 Hz cap,
probably in the loading/boot render loop, which must keep submitting frames at the HMD rate.

### db4: XInput "connected" always true (XInput pad Poll 0xa0ee30) [sure]
`r = XInputGetState(+0x290, &st)` (XINPUT1_3 ord 2) is followed by `+0x289 = (r != ERROR_DEVICE_NOT_CONNECTED)`.
The patch gives `+0x289 = 1`. The gamepad device object (vtable **0x13e7670**) is then always treated as
present (readers 0x977000, 0x97cd44), so the game keeps calling its virtuals. MotherVR has replaced
those (db0 = 0x13e7674 = vtable+4, i.e. slots 1, 2, 3, 5, 11) to feed VR-controller input.
**Note for recon:** `stereo_interface.h` names the db0 callbacks get_control_key, get_float_value,
get_view_value, get_view_name and update_eye. They are actually gamepad-device methods: slot 1 =
IsButtonDown(btn) (orig 0x976730), slot 3 = axis value (FUN_10008e20(300,0) is read as the turn axis),
slot 11 = "had input this frame" (FUN_10008fa0). The ECX they receive is the gamepad object.

Game pad bit numbering (built in 0xa0ee30 from XInput wButtons): 0 A, 1 B, 2 X, 3 Y, 4-7 D-pad
U/D/L/R, 8 Back, 9 Start, 10 LB, 11 RB, 12 LT, 13 RT, 14 L3, 15 R3. Bits ≥ 0x10 are triggers vs threshold.

---

## 3. Call-redirect and vtable targets

ABI rule followed below: each target must match the original callee's `ret N` and register use.
Free-function sketches use `__fastcall(self, edx, ...)` for thiscall.

### FUN_1001f6a0: replaces `Stereo::Init(int type)` 0x9e0d40 (site 0xa13941) [sure]
Original (thiscall, ret 4): if type==1, `this+4 = new HmdDevice(0x1F4)` (0x9be9c0, LibOVR create).
If the device state == 0 it calls Shutdown 0x9d9ba0.
```cpp
// this = MotherVR StereoManager (the game passes [0x1766a2c])
void __fastcall Stereo_Init(StereoManager* sm, void*, int type) {   // ret 4
    if (type != 1) return;
    sm->ParseCommandLine();            // FUN_1001f700: -vrforceoculus/-vrforceopenvr/-vrforcenone
                                       //   -> sm+0x2e4=1, sm+0x2e8=1/2/0; -vrscale f (0.495..1.505) -> sm+0x2b4
    StereoInterface* si = &sm->si;     // sm+0xb0
    si->Init();                        // FUN_1001eac0: runtime = sm+0x2e4 ? sm+0x2e8 : sm+0x2ec;
                                       //  1 -> Oculus (0x5b8), 2 -> OpenVR (0x620) into si+0x1f4;
                                       //  on success: backend->vf24(); fill si+0x10.. via backend->vf4();
                                       //  copy eye FovPorts to si+0xac..+0xc8; si->state(+4) = 1
    sm->si_ptr_2e0 = si;               // sm+0x2e0
    sm->device = si->state ? si : nullptr;   // sm+4 (game-visible device ptr)
    if (sm->device && si->state == 1)
        si->vtbl->Recenter_54(si, &sm->base_78);  // tail call, reuses the caller's arg slot
}
```

### FUN_1001f8a0: replaces `Stereo::Update(float dt)` 0x9be2a0 (site 0x7b4100) [sure]
Original (thiscall, ret 4): if device state==2 and pad LB+RB are held, it calls `device->vf2c(&this+0x78)`
(recenter). It then animates the fade: `+0xac += (+0xa8 ? 1 : k)*dt`, clamped to [0,1].
MotherVR ignores `dt` and never touches `+0xac`.
```cpp
void __fastcall Stereo_Update(StereoManager* sm, void*, float /*dt*/) {  // ret 4
    if (!sm->device || sm->device->state != 2) { g_follow->look_enabled_264 = 1; return; }
    auto* im  = *g_InputMgr;                     // [0x1759b44] (DAT_10066d4c)
    auto* pad = im->gamepad_4a7c;  auto* kb = im->keyboard_4a74;  auto* mouse = im->mouse_4a70;
    bool lb=0, rb=0, r3=0, ctrl=0, alt=0;
    if (pad) {
        lb = pad->IsButtonDown(10); rb = pad->IsButtonDown(11); r3 = pad->IsButtonDown(15); // orig 0x976730
        if (pad->vtbl->HadInput_slot11(pad)) sm->kbm_mode_311 = 0;   // FUN_10008fa0
        float turn = pad->vtbl->Axis_slot3(pad, 300, 0);             // FUN_10008e20, VR turn axis
        bool out = turn < -0.5f || turn > 0.5f;
        // snap turn: only while FollowBehavior is active, snap enabled, not in virtual-screen mode
        if (g_follow_active && sm->snap_rotation_2fd && out && sm->mode_60 != 1 && !sm->snap_latch_314) {
            sm->snap_time_318 = clock();
            float step = 1.0f / float(sm->snap_segments_300 / 2);   // 2/segments; full turn = 2.0
            float yaw = g_follow->yaw_204 + (turn < -0.5f ? -step : step);
            if (yaw > 1) yaw -= 2; if (yaw < -1) yaw += 2;
            g_follow->yaw_204 = g_follow->yaw_1fc = yaw;
            sm->snap_latch_314 = 1;
        } else if ((!out && sm->snap_latch_314) ||
                   (g_follow_active && out && sm->snap_latch_314 && clock() - sm->snap_time_318 > 300)) {
            sm->snap_latch_314 = 0; sm->snap_time_318 = 0;          // release or 300 ms auto-repeat
        }
    }
    if (kb && mouse) {
        ctrl = kb->KeyDown(0x1d); alt = kb->KeyDown(0x38);          // orig 0xa0eb20 (DIK codes)
        if (kb->AnyKey_224() || mouse->Active_54()) sm->kbm_mode_311 = 1; // orig 0xa0ee00 / 0xdbbe70
    }
    g_follow->look_enabled_264 = !sm->snap_rotation_2fd || sm->kbm_mode_311;
    bool combo = (lb && rb) || (ctrl && alt) || sm->runtime_recenter_req_31d;  // 31d set by Oculus ShouldRecenter
    if (combo && !sm->recentered_31c) {
        sm->device->vtbl->Recenter_54(sm->device, &sm->base_78);
        sm->recentered_31c = 1; sm->runtime_recenter_req_31d = 0;
    } else if (combo && r3 && !sm->r3_latch_313) {
        sm->r3_latch_313 = 1;
    } else if (sm->recentered_31c && sm->replace_count_320 < 2) {   // 2 frames after a recenter
        PlaceVirtualScreen(&sm->screen_20, -1.0f, 0.5625f, -1.0f);   // FUN_1001fc20
        ++sm->replace_count_320;
    } else if (!combo) { sm->recentered_31c = sm->r3_latch_313 = 0; sm->replace_count_320 = 0; }
    if (sm->mode_6c != sm->mode_60) sm->mode_6c = sm->mode_60;
    if (g_cutscene_or_fade_863a) {                    // set by FUN_1001f830
        if (sm->mode_60 != 1) sm->visibility_64 = 1.0f;   // never black out during cutscenes and fades
        if (!g_follow_active) g_cut_took_cam_863b = 1;
        else if (g_cut_took_cam_863b) g_cutscene_or_fade_863a = g_cut_took_cam_863b = 0;
    }
    if (sm->loading_blackout_310) sm->visibility_64 = 0.0f;  // set by the loading-screen wrapper
}
```
This writes FollowBehavior yaw (`+0x204`, `+0x1fc`) and `+0x264`, plus the StereoManager fields
0x64, 0x6c, 0x311, 0x313, 0x314, 0x318, 0x31c, 0x31d and 0x320. It reads only the pad, keyboard and
mouse state and the VR turn axis, not the HMD pose.

### FUN_1001fc20: replaces 0x9be550 inside `Stereo::SetMode(1)` (site 0x9cd132) [sure]
Original `Matrix44* __cdecl BuildUiQuad(Matrix44* out, float size, float invAspect, float dist)`
(plain `ret`, the caller pops 0x10 and **uses the returned pointer** with `rep movsd`):
`out.row0 = right*size`, `row1 = up*size*invAspect`, `row2 = fwd`, `row3 = campos + fwd*dist`,
using the full camera orientation. `SetMode(1)` passes (3.5, h/w of UI RT, 3.0).
MotherVR ignores `size` and `dist`. It builds a **yaw-only, level** screen 1.0 m wide at 1.5 m:
```cpp
Matrix44* __cdecl PlaceVirtualScreen(Matrix44* out, float, float invAspect, float) {
    const Matrix44& V = *g_PovMgr->pov[0];               // [[0x1766a00]+8], world->view
    Quat q = QuatFromMatrix(V);                          // FUN_100056a0
    float yaw; q.GetEuler(&yaw, nullptr, nullptr);       // FUN_10006360
    float h = -yaw * 0.5f;  Quat qy(0, sinf(h), 0, cosf(h));
    Matrix44 R = ToMatrix(qy);
    *out = Scale(1, invAspect, 1) * R;                   // FUN_10005a40
    vec3 cam = CamPosFromView(V);                        // -dot(row_i, row3)
    out->m[3][0] = cam.x + out->m[2][0]*1.5f;
    out->m[3][1] = cam.y;
    out->m[3][2] = cam.z + out->m[2][2]*1.5f;
    return out;                                          // MUST return out in EAX
}
```

### FUN_1001ffd0: replaces 0x9be550 in `Stereo::DrawOverlay` mode 0 (site 0x9cd2ee) [sure]
Same ABI (`__cdecl`, returns out). The game passes (0.8, invAspect, 1.0). MotherVR ignores size
and dist and uses a head-locked quad 0.55 m wide at 1.0 m:
```cpp
Matrix44* __cdecl PlaceHudQuad(Matrix44* out, float, float invAspect, float) {
    const float* M = g_PovMgr->pov[0]->m;               // world->view, row-vector
    vec3 up{M[1],M[5],M[9]}, fwd{M[2],M[6],M[10]};
    *out = Identity();
    out->row1 = up * (0.55f * invAspect);
    out->row2 = fwd;
    out->row0 = cross(up*0.55f, fwd);                    // = right * 0.55 (LH)
    out->row3 = fwd + CamPosFromView(M);                 // 1 m in front of the eyes' centre
    return out;
}
```

### FUN_1001f640: replaces `Stereo::GetDisplaySize(int* w, int* h)` 0x9b8780 (site 0xa0fe5d) [sure]
Original (thiscall, ret 8): `*w=1280; *h=800;` then `device->vf44(w,h)` (HMD panel resolution).
It is called at the end of the display-resolution query 0xa0fdd0 when Stereo Mode == Rift. MotherVR:
`int* __fastcall(void*, void*, int* w, int* h) { return h; }`. The window size the game already
computed is kept. The same function sits in SI slot +0x44.

### FUN_100202a0: replaces `Stereo::GetRenderTargetSize(int* w, int* h)` 0x9b8750 (site 0xa0fd64) [sure]
Original (thiscall, ret 8): `1280x800` defaults, then `device->vf40(w,h)`.
```cpp
int* __fastcall Stereo_GetRTSize(StereoManager* sm, void*, int* w, int* h) {   // ret 8
    if (!sm->device) return h;
    sm->device->window_w_20c = *w; sm->device->window_h_210 = *h;   // remember the window size (mirror)
    return sm->device->vtbl->GetRenderTargetSize_40(sm->device, w, h);  // FUN_1001f530
}
// FUN_1001f530: scale = (si->state==1) ? 0.5f : sm->pixel_scale_2f8;
//   backend->vf8(&eyeL, 0, scale); backend->vf8(&eyeR, 1, scale);
//   W = eyeL.w + eyeR.w; H = max(h); si+0x1ec=W, +0x1f0=H;
//   viewports: L(+0x8c..+0x98) = {0,0,W/2,H}, R(+0x9c..+0xa8) = {W/2,0,W/2,H}; *w=W; *h=H
```
Result: the game allocates a side-by-side stereo backbuffer at the runtime's recommended per-eye size
× pixel scale.

### FUN_100201e0: replaces `Stereo::PushMode(int)` 0x9d9c00 (sites 0x80eb8e, 0x83eeca "Hub"-type screen, 0x84197a front-end Open) [sure]
Original (thiscall, ret 4): pushes the mode on `+0x68/+0x6c` and calls `SetMode(mode)` 0x9cd0a0.
```cpp
void __fastcall Stereo_Push(StereoManager* sm, void*, int mode) {   // ret 4
    if (mode == 1 && sm->mode_60 == 0) { g_PushMode(sm, 1); return; }   // DAT_10066d04 = 0x9d9c00
    g_PopMode(sm);                                                      // DAT_10066d08 = 0x9d9c80
    g_PushMode(sm, mode);   // re-push: keeps depth 1 and re-places the screen in front of the current view
}
```

### FUN_10020220: replaces `Stereo::PopMode()` 0x9d9c80 (sites 0x813dda, 0x816382, 0x824c2a) [sure]
`void __fastcall(StereoManager* sm, void*) { if (sm->mode_60 == 1) g_PopMode(sm); }` (plain ret, a tail jmp).
The original pops `+0x6c` and re-applies the top (or 0) through SetMode.

### FUN_10003100: replaces 0x97dce0 at both call sites in 0x82cff0 (db8+1 and db8+0xed) [sure]
Original (thiscall, no args, plain ret, ECX = game action map): it scans the first analog action's
bindings (`[this+0xa74]`, count `[this+0xa78]`, 8-byte entries {.., short code@4, byte disabled@6}),
finds the first enabled one and returns `code == 0x12` (no binding also gives true). That means
"movement is on the left stick", and it chooses `"lstick"`/`"rstick"` for the UI prompt keys
`look_controls`, `hack_left_stick_controls` and `hack_right_stick_controls`.
MotherVR has rewritten the game's action map (FUN_100031b0 → FUN_10003200: each action's first raw
binding is replaced by a virtual code `+0x5c4+i` and the original is saved in `map+0x5d0[i]`;
`+0x5c8` = number of digital actions). So it answers from the saved originals:
```cpp
bool __fastcall IsMoveOnLeftStick(void* /*ignored*/, void*) {
    auto* m = ActionMap();                                 // FUN_10002ef0, DAT_1006a5c8
    return m->orig_code_5d0[m->num_digital_5c8] == 0x12;   // first analog action
}
```

### FUN_10003120: replaces `GetControlPromptName(key, out)` 0x82cff0 call at 0x83ab40 [sure]
`push ebp; mov ebp,esp; pop ebp; jmp [DAT_10066c1c]` is a pure pass-through to the original (cdecl, 2 args,
caller pops). It has no effect and is presumably a leftover hook point. Reconstruct it as a
no-op or skip it.

### FUN_10001720 / FUN_10001740: FollowBehavior vtable slots 0 and 1 (d2c, d2c+4) [sure]
```cpp
// both: thiscall, 1 stack arg, the original does `ret 4`; implemented as tail-jumps (stack untouched, EDX zeroed)
void __fastcall Follow_OnActivate  (Follow* f, void*, void* a){ g_follow=f; g_follow_active=1; orig_slot0(f,a); } // DAT_10066bd0 = 0x42c930
void __fastcall Follow_OnDeactivate(Follow* f, void*, void* a){ g_follow=f; g_follow_active=0; orig_slot1(f,a); } // DAT_10066bcc = 0x42bbf0
```
`g_follow` = DAT_1006863c, `g_follow_active` = DAT_10068638. Both are used by FUN_1001f8a0.

### FUN_1001f830: UI entity message handler (d30, slot 2 of vtable 0x11fce10) [sure, with an ABI caveat]
The original 0x91bd70 is `thiscall(this, const ShortGuid* msg, a2, a3, a4)` with `ret 0x10`. The globals
compared are runtime-hashed names (static initialisers at 0x1152xxx → 0x8bf7f0):
c44 = `cutscene_visibility_start`, c48 = `cutscene_visibility_finish`, c4c = `level_fade_start`,
c50 = `level_fade_finish`.
```cpp
// naked in the original: pushfd/pushad ... popad/popfd; jmp [orig]
void __fastcall UiEntity_OnMessage(void* self, void*, const uint32_t* msg, int a2, int a3, int a4) {
    uint32_t id = *msg;
    if (id == *g_cutscene_vis_start || id == *g_level_fade_start)  g_cutscene_or_fade_863a = 1;
    else if (id == *g_cutscene_vis_finish || id == *g_level_fade_finish) g_cutscene_or_fade_863a = 0;
    return orig_91bd70(self, msg, a2, a3, a4);   // DAT_10066c40
}
```
**Caveat:** the original reads `[ebp+8]` with no prologue of its own, which is the *caller's* first
argument. It works only if the dispatcher has a frame pointer and the same message pointer as its own
arg 1. The reconstruction should read its own arg 1, as the original 0x91bd70 does.

---

## 4. Menu-object hooks (d9c+8, da0, da0+8, da4, da4+8, da8, da8+8)

All of them are thiscall, take no stack args and return with a plain `ret`. They call the original with
ECX = the game object untouched and EDX zeroed. Afterwards they touch the StereoManager through
`FUN_10003490()` and `DAT_10066d04` = `Stereo::PushMode(int)` 0x9d9c00, `DAT_10066d08` =
`Stereo::PopMode()` 0x9d9c80, called with ECX = StereoManager. (So d04 and d08 are **push/pop stereo
mode**. They are not pause/unpause or HMD reset.)

| Install | Game class (vtable) | Slot | Original (global) | Wrapper | Added behaviour |
|---|---|---|---|---|---|
| d9c | Front-end menu (0x13000e0: playBackgroundMovies, setLanguage, stop_front_end_music) | 0 Open | 0x841950 (c10) | FUN_10020c60 | push settings to UI (FUN_10020870), already reconstructed |
| d9c+8 | " | 2 Close | 0x813dd0 (c14) | FUN_10020c80 | `if (mode==1) Pop()` |
| da0 | Pause menu (0x1300180: setItemVisible, setChallengeOrPreorderEnabled; slot 3 handles "Pause") | 0 Open | 0x841d90 (c34) | FUN_10020ca0 | `if (mode!=0) Pop(); Push(1);` push settings |
| da0+8 | " | 2 Close | 0x8155e0 (c08) | FUN_10020cf0 | `if (mode==1) Pop()` |
| da4 | Loading screen (0x1300218: setGameplayTip, setState) | 0 Open | 0x841ea0 (c38) | FUN_10020d10 | `if (mode!=0) Pop(); Push(1); sm+0x310 = 1` (scene black) ; push settings |
| da4+8 | " | 2 Close | 0x815670 (c0c) | FUN_10020d60 | `sm+0x310 = 0; if (mode==1) Pop()` |
| da8 | Attract/title video screen (0x12fcc80: playAttractVideo) | 0 Open | 0x80b3a0 (c00) | FUN_10020da0 | `if (mode!=0) Pop(); Push(1);` |
| da8+8 | " | 2 Close | 0xa69c90 = empty `ret` (not called) | FUN_10020dd0 | `if (mode==1) Pop()` |

```cpp
void __fastcall Pause_Open(void* menu, void*) {           // FUN_10020ca0 (da4/da8 analogous)
    orig_c34(menu);
    auto* sm = GetStereoManager();
    if (sm->mode_60 != 0) g_PopMode(sm);
    g_PushMode(sm, 1);
    PushSettingsToUi(menu);                                // FUN_10020870 (not in da8)
}
void __fastcall Menu_Close(void* menu, void*) {           // FUN_10020c80 / cf0 / d60 / dd0
    orig(menu);                                            // none for dd0
    auto* sm = GetStereoManager();
    // d60 also clears sm->loading_blackout_310 = 0 first
    if (sm->mode_60 == 1) g_PopMode(sm);
}
```
Net effect: whenever a full-screen menu, the loading screen or the attract screen is up, the game is in
virtual-screen mode 1. The 2D UI is shown on a level 1 m × 0.5625 m quad 1.5 m ahead, placed once at
open time by FUN_1001fc20. During loading screens the 3D scene is also blacked out (`+0x64 = 0`).

---

## 5. Init calls at the top of FUN_1001d6f0

| Call | What it does |
|---|---|
| FUN_10001760 | Selects and rebases the per-build AI.exe address table (reconstructed). |
| FUN_10003490 | StereoManager singleton getter (TLS once, memset 0x324, FUN_10003510 init, atexit). |
| **FUN_1000ce80** | Config loader (this = the installer object's settings block). It `_stat`s the config file and, if missing, shows the one-time message box "Welcome to MotherVR for Alien: Isolation! ... Options->MotherVR ...". It parses the JSON (nlohmann) with defaults `{1, 1.0f, 0x0101, 12, 0x0101, 0}` (vr runtime, pixel scale, blinders/snap, snap segments, walking/strafing flags...) and copies them into the object. This is the job of `recon/src/config/config.cpp`. |
| **FUN_1001d630** | **Audio redirect hook.** `CoInitialize`; `CoCreateInstance(CLSID_MMDeviceEnumerator, IID_IMMDeviceEnumerator)`; `GetDefaultAudioEndpoint(eRender, eConsole, &dev)`; it saves `dev->vtbl[3]` (IMMDevice::Activate) in DAT_10068644 and overwrites that **shared vtable slot** with FUN_10001d60 (FUN_1001d4d0 = VirtualProtect + WriteProcessMemory of 4 bytes). On failure it shows "Failed to hook Audio! [0x%X]". FUN_10001d60(this, iid, ctx, params, ppv) (stdcall COM, 5 args): if the wide string DAT_100686a4 (HMD audio endpoint ID, filled by the Oculus backend around 0x100038bd before calling game function DAT_10066d24 = 0xc31b00 to reinit audio [likely]) is non-empty, it replaces `this` with `enumerator->GetDevice(id)` and then calls the original Activate. So the game's audio (Wwise) opens on the headset's audio device. |
| **FUN_1001e720** | Deploys `DATA\MOTHER.PAK` (resource 0xCA "BINARY"). If the file exists with a creation time equal to the DLL build FILETIME (this+0x38) it skips. Otherwise it requires `DATA\UI.PAK` (if that is missing: message box "Failed to find UI.PAK! ... will now exit." and **returns false, so the installer aborts**). It then writes the resource through a file mapping and stamps the file times. Reconstructed as `integration::deploy_mother_pak`. |
| (later) FUN_1001e940 | Only if vr_runtime == 2: deploy `openvr_api.dll` (reconstructed). |

---

## 6. Open items and flags
- **Head pose path:** it is not part of these patches. StereoInterface slots +0x1c and +0x20 are no-ops, and
  slots +0x30..+0x3c reuse game code on MotherVR data. Find where the HMD orientation reaches
  the view: candidates are the backend calls inside SI BeginFrame (FUN_1001f330, which calls backend vf28/vf1c and writes eye
  offsets ×worldScale to SI+0xf8/+0x130) and the db0 gamepad methods.
- `recon/src/stereo/stereo_interface.h` naming for the db0 slots looks wrong (they are gamepad methods). See db4.
- The 2020 table slot 0x4E holds 0x00D1F020 in the original FUN_10001840, probably a typo for 0x51F020.
  It is unused by the installer.
- The c30, bd4 and dac interpretations are [guess]/[likely]. Confirm them at runtime by toggling each patch.
- c9c skips two zero stores (`renderer+0xcf698/+0xcf69c`). Keep that in mind if the renderer object
  is not zero-initialised.
