# 06d: VR input layer (gamepad virtualisation)

Target: MotherVR 0.8.1 `dxgi.dll` (base 0x10000000) on the 2015 `AI.exe` (base 0x400000).
Analysis only. Everything below was read from disassembly (`scripts/disasm.py`); Ghidra's
decompiles of these functions have wrong calling conventions and drop the `this`/index
registers. Confidence tags: **[sure]** read directly from code, **[likely]** strong inference,
**[guess]** plausible but unverified.

## TL;DR

* The five "StereoInterface" slots in `recon/src/stereo/stereo_interface.*` are not stereo
  at all. They are overrides on **AI.exe's XInput gamepad device class** (vtable VA 0x013E7670;
  MotherVR's `DAT_10066db0` points at the second slot, VA 0x013E7674 = RVA 0xFE7674).
  Their names and descriptions in that header are wrong.
* MotherVR **virtualises the gamepad**. It rewrites every gamepad binding in the game's three
  action maps so that action *k* is bound to a virtual control code `base + k` (base
  0x100 / 0x200 / 0x400 per map). When the game asks the pad "is control 0x10B down?", the hook
  looks up the original pad control (e.g. `RB`), asks the real pad, and ORs in a per-action VR
  state byte that the HMD backend writes each frame. Analog actions work the same way with
  float tables (max or max-abs merge).
* VR state is written **per game action, not per physical button**. The backends
  (`OpenVR::slot10` FUN_10007ad0, `Oculus::slot10` FUN_10004960) read the controllers and
  write the action tables directly. Physical-to-action mapping is hard-coded per backend.
* UI glyphs: when "VR glyph mode" (`DAT_10066bc0`, initially 1) is on, the pad's
  `ControlToName` returns VR glyph ids ("VR_TP_D", "VR_LG", "VR_A", ...) that the modded
  ActionScript `ImageSubstitution.as` maps to `glyph_vr_*` images.
* `FUN_10008fa0` is the pad's **HasActivity()** override (game uses it to pick the active
  input device). It ORs in VR activity and, when the VR/non-VR state flips, briefly toggles
  the pad/keyboard "active device" flags so the game re-fires its device-changed path and
  refreshes the glyphs. It has nothing to do with eyes.
* Only SnapRotation (+0x2fd), SnapRotationSegments (+0x300) and ToggleGrips (+0x306) are
  consumed natively. VRInputRelativeWalking (+0x304), VRInputStrafing (+0x305) and
  ToggleCrouch (+0x307) are only forwarded to the game's script layer
  (`setMotherInputRelativeWalking` / `setMotherInputStrafing` / `setMotherToggleCrouch` in
  FUN_10020870). No native code reads them. **[sure]**
* No haptics. The OpenVR path never calls `TriggerHapticPulse`. Oculus vibration entry
  points are resolved by the loader (FUN_10021160) but never called. **[likely]**

---

## 1. The patched AI.exe class: XInput gamepad device

### 1.1 Identification

`DAT_10066db0` = address slot 0x7A = RVA 0xFE7674 (VA 0x013E7674). The vtable really starts
one slot earlier at VA 0x013E7670. Slot 0 is the scalar deleting destructor 0x00A10FE0, which
writes `0x13E7670` into `[this]`. The ctor at 0x00A0EE10 calls base ctor 0x009864F0 and then
sets vptr 0x13E7670. AI.exe has no RTTI for it. **[sure]**

The object (0x2B8 bytes) is created by the input manager in 0x00986B73 and registered as
`mgr+0x4A78` / `mgr+0x4A7C` (current pad). Slot +0x24 (0x00A0F280) polls `XInputGetState`
(import thunk 0x1008A82) for users 0..3. Its control-name table is the XInput layout below.
So this is the game's **XInput pad device** ("CPadXInput"-like; real name unknown). **[sure]**

Sibling devices from the same manager (0x00986A50) are typed by `dev+0x0C`: **0 = pad,
1 = keyboard** (ctor 0x00A0E9C0, vtable 0x13E7628, 0x228 bytes), **2 = mouse** (ctor 0x00A0E520,
vtable 0x13E75E0, 0xB0 bytes). The manager keeps `mgr+0x4A70` = mouse, `+0x4A74` = keyboard,
`+0x4A78` = pad, `+0x4A7C` = active pad. **[sure]** (The type 1 / 2 to keyboard / mouse
naming is **[likely]**: the type-1 device memsets a 0x100-byte key array.)

Game control codes (pad), from `ControlToName` 0x009764E0 and the MotherVR copy at
FUN_10001090: **[sure]**

| code | name | code | name |
|---|---|---|---|
| 0x00 | A | 0x0C | LT_BUTTON |
| 0x01 | B | 0x0D | RT_BUTTON |
| 0x02 | X | 0x0E | LSTICK_BUTTON |
| 0x03 | Y | 0x0F | RSTICK_BUTTON |
| 0x04-0x07 | DPAD_UP/DOWN/LEFT/RIGHT | 0x10 / 0x11 | LT / RT (analog) |
| 0x08 | BACK | 0x12 / 0x13 | LSTICK_X / LSTICK_Y |
| 0x09 | START | 0x14 / 0x15 | RSTICK_X / RSTICK_Y |
| 0x0A / 0x0B | LB / RB | 0x16 / 0x17 | TOUCH_ZOOM_IN / OUT |

Pad object fields used: `+0x18` sub-state (dword at +0x18 = "something pressed" or packet
counter **[guess]**; `+0x1C` = 16-bit button mask indexed by code 0..15), `+0x268/26C/270/274` =
LX, LY, RX, RY, `+0x278/27C` = LT, RT, `+0x280/284` = touch zoom in/out, `+0x04` = trigger
press threshold (0.5), `+0x10` = enabled byte, `+0x11` = **is-active-device** byte.

### 1.2 Vtable (VA 0x013E7670); offsets relative to MotherVR's `DAT_10066db0`

| MVR off | game off | AI.exe impl | meaning | ABI | patched by |
|---|---|---|---|---|---|
| (-4) | +0x00 | 0x00A10FE0 | scalar deleting dtor | thiscall, ret 4 | |
| **+0x00** | +0x04 | 0x00976730 | `bool IsDown(int code)`: codes <0x10 test bit in `+0x1C`, 0x10/0x11 = trigger > `+0x04`, else false | thiscall, ret 4 | FUN_10008d50 (orig in `DAT_10066c6c`) |
| **+0x04** | +0x08 | 0x00976780 | `float GetValue(int code)`: buttons give 1/0. 0x10, 0x11, 0x16, 0x17 give raw LT/RT/zoom. **Sticks (0x12-0x15) give 0.** Non-negative "pressure" value. | thiscall, ret 4, ST0 | FUN_10008f00 (`c70`) |
| **+0x08** | +0x0C | 0x00976820 → 0x00976630 on `this+0x18` | `float GetAxis(int code, bool negate)`: value times (negate ? -1 : +1). Buttons give ±1/0. 0x10→LT, 0x11→RT, 0x12-0x15 sticks, 0x16/0x17 zoom. Signed. | thiscall, ret 8, ST0 | FUN_10008e20 (`c74`) |
| +0x0C | +0x10 | 0x00976830 → 0x00976280 | `int NameToControl(const char*)` | thiscall, ret 4 | no |
| **+0x10** | +0x14 | 0x00976840 → 0x009764E0 | `const char* ControlToName(uint16 code)` (glyph id for UI) | thiscall, ret 4 | FUN_10009030 (`c78`) |
| +0x14 | +0x18 | 0x00976260 | returns "" (device name?) | | no |
| +0x18 | +0x1C | 0x00976850 | `int ControlType(uint16)`: 0 button, 2 trigger (0x10/0x11), 1 axis (0x12-0x17) | ret 4 | no |
| +0x1C | +0x20 | 0x00A0EE30 | rumble apply? (uses `+0x290`) **[guess]** | | no |
| +0x20 | +0x24 | 0x0097C350 | latch vibration request `+0x2A4..` into `+0x294..` **[guess]** | | no |
| +0x24 | +0x28 | 0x00A0F280 | Poll: `XInputGetState(0..3)` | | no |
| **+0x28** | +0x2C | 0x00976880 | `bool HasActivity()`: `[+0x18]!=0` or any of \|LX\|,\|LY\|,\|RX\|,\|RY\|,LT,RT > 0.85 | thiscall, ret 0 | FUN_10008fa0 (`c7c`) |
| | +0x40 | ? | OnActiveChanged (called by device selection) | | |

All five originals are saved in `DAT_10066c6c..c7c` (patch code in FUN_1001d6f0 lines
368-397). `FUN_10001d20` is just `jmp [DAT_10066c6c]`, a thunk to the **original** IsDown.
**[sure]**

### 1.3 Who calls them

* The game's action maps evaluate each binding by calling `device[binding.device]->IsDown /
  GetValue / GetAxis(binding.code)`. Pad bindings carry pad codes. After MotherVR's
  remap they carry virtual codes ≥ 0x100. **[likely]** (the per-map device array is at
  `map+0x103C`, see §2.4)
* Device selection, AI.exe 0x0097CBC0 (`this` = input manager): for each registered
  device with `type==0 && !active(+0x11)` it calls vt+0x2C (HasActivity). On true it clears
  `+0x11` on every device (calling vt+0x40, then vt+0x28 if still inactive), sets the new
  device's `+0x11=1`, calls vt+0x40, and stores it in `mgr+0x4A7C` and in
  `mgr + 0x1084/0x39D4/0x4A48 + 4*type`. **[sure]**
* MotherVR itself calls the hooks every frame from `StereoInterface` FUN_1001f8a0 with
  `ecx = mgr->GetPad()`. It calls `FUN_10001d20(LB/RB/RSTICK_BUTTON)` (original IsDown, for
  the LB+RB recenter chord), `FUN_10008fa0()` (the HasActivity override, so it runs every
  frame) and `FUN_10008e20(300, 0)` (virtual analog 0x12C = map0 analog #3 = **turn**, for
  snap rotation, see §4.6). **[sure]**

### 1.4 Index space

* `code < 0x100`: a real pad code (0..0x17), or 0xFFFF/-1 = unbound. After MotherVR's
  remap the game should never pass a real pad code. Seeing one means the maps were
  (re)loaded and need remapping, so IsDown triggers the remap.
* `0x100 ≤ code < 0x200`: map 0 (tier A), `0x200..0x3FF`: map 1 (tier B), `≥ 0x400`: map 2
  (tier C).
  Within a tier: `code - base = k` for digital action *k*; `code - base = numDigital + j` for
  analog action *j*. **[sure]**

---

## 2. Action-map tiers

### 2.1 What they are

Three function-local statics (magic-static init with TLS epoch guard), one per AI.exe
action map in the input manager (`*DAT_10066d4c`, AI.exe VA 0x01759B44 = slot 0x61). **[sure]**

| getter | static | base (+0x5C4) | game map (FUN_100031b0) | AI.exe map ctor | contents [likely] |
|---|---|---|---|---|---|
| FUN_10002ef0 | 0x1006A5C8 | 0x100 | `mgr+0x48` | 0x00997750 | **in-game player map** (move, turn, sprint, crouch, aim, fire, interact, radial, reload, tracker, flashlight, map, weapon next/prev). 0x29 = 41 digital actions (see §4.6). |
| FUN_10002fa0 | 0x10069F80 | 0x200 | `mgr+0x2998` | 0x009978B0 | **UI / menu map** (confirm, back, pause, both sticks, both triggers) |
| FUN_10003050 | 0x10069938 | 0x400 | `mgr+0x3A0C` | 0x009974E0 (vtable 0x13E3B60) | third map. **Never written by VR**, so its pad bindings are virtualised and pass through the real pad only (debug/other map **[guess]**). |

Map names are inferred from which VR controls are written into them and from the help
strings. AI.exe's internal names are unknown.

### 2.2 Tier object layout (0x644 bytes; magic-static guard dword at +0x644) **[sure]**

```
+0x000  uint8_t  digital[0x2C];      // VR state of digital action k (written by backend)
+0x02C  float    analog[0x10];       // VR value of analog action j
+0x06C  std::string names[0x39];     // MSVC std::string (0x18 each); ctor FUN_10003440, dtor FUN_10002810.
                                     //   Never written or read anywhere else: unused
+0x5C4  int32_t  base;               // 0x100 / 0x200 / 0x400 (set at static init)
+0x5C8  int32_t  num_digital;        // = map+0x1054 (set by refresh)
+0x5CC  int32_t  num_analog;         // = map+0x1058
+0x5D0  int16_t  original_code[0x39];// [k] digital, [num_digital+j] analog; -1 = had no pad binding
```

`FUN_10003100` (unreferenced?) tests `original_code[num_digital] == 0x12` on tier A, i.e.
"is analog action 0 bound to LSTICK_X". **[sure]** for the code. Purpose unknown.

### 2.3 Refresh / remap: FUN_100031b0 → FUN_10003200 (thiscall tier, arg = map*, ret 4) **[sure]**

`FUN_100031b0` (no args) calls `tier.remap(map)` for (A, mgr+0x48), (B, mgr+0x2998),
(C, mgr+0x3A0C). Its only caller is the IsDown hook, when it sees a code < 0x100 other than -1.

```
remap(tier, map):
  tier.num_digital = map.num_digital            // map+0x1054
  for k in [0, num_digital):
      act = map.digital[k]                      // map+0x04 + 0x40*k
      found = false
      for b in act.bindings[0..act.count):      // bindings at act+0x04, 8 bytes; count at act+0x34
          if b.device != 0: continue            // byte +2: only gamepad bindings
          if found: b.code = -1                 // extra pad bindings are disabled
          else:
              found = true
              if b.code < 0x100:                // not yet virtualised
                  tier.original_code[k] = b.code
                  b.code = tier.base + k
          b.flags &= ~0x40                      // byte +3
      if !found:                                // append a pad binding
          b = act.bindings[act.count++]; b = {code = base+k, device = 0, flags = 0}
          tier.original_code[k] = -1
  tier.num_analog = map.num_analog              // map+0x1058
  for j in [0, num_analog):                     // map+0xA44 + 0x44*j, same binding layout
      same as above but index n = num_digital + j, no flags&=~0x40,
      appended binding flags = 1
  fill original_code[num_digital+num_analog .. 0x39) = -1
  for c in map.composite[0 .. map+0x1038):      // entries at map+0xE84, stride 0x24
      if c.byte_0x21 == 0: c.codes[0..c.count(+0x1C)) = -1   // 7 dword slots at +0x00
```

The composite part disables all codes in some 2D/combo binding list **[guess: stick
vectors built from pad axes or button chords]**. With it gone, the pad only reaches the game
through the virtualised per-action bindings.

**Note:** the remap is idempotent. Bindings already ≥ 0x100 are skipped but still count as
`found`.

### 2.4 AI.exe action-map layout (as used by the remap) **[sure for offsets, likely for meaning]**

```
struct GameBinding  { int16 code; uint8 device /*0 pad,1 kb,2 mouse*/; uint8 flags; uint32 unk; };   // 8
struct GameDigital  { uint32 unk; GameBinding b[6]; int32 count /*+0x34*/; uint8 pad[8]; };        // 0x40
struct GameAnalog   { uint32 unk; GameBinding b[6]; int32 count /*+0x34*/; uint8 pad[0xC]; };      // 0x44
struct GameComposite{ uint32 codes[7]; int32 count /*+0x1C*/; uint8 unk20; uint8 flag21; uint8 pad[2]; }; // 0x24
struct GameActionMap {
  void*         vptr;              // +0x000
  GameDigital   digital[0x29];     // +0x004 .. +0xA44
  GameAnalog    analog[0x10];      // +0xA44 .. +0xE84
  GameComposite composite[12];     // +0xE84 .. ~+0x1038 (count of 12 is inferred)
  int32         composite_count;   // +0x1038
  InputDevice*  device_by_type[3]; // +0x103C  (pad, keyboard, mouse)
  ...
  int32         num_digital;       // +0x1054
  int32         num_analog;        // +0x1058
};
```

### 2.5 `*DAT_10066d4c` (AI.exe input manager) fields seen **[sure]**

`+0x40` device list, `+0x44` device count (≤16), `+0x48` map A, `+0x2998` map B,
`+0x3A0C` map C, `+0x4A70` mouse, `+0x4A74` keyboard, `+0x4A78` pad, `+0x4A7C` active pad.
`+0x1084` = mapA.device_by_type[0] = **pad**, `+0x1088` = mapA.device_by_type[1] = **keyboard**.
(These are the "two objects" FUN_10008fa0 touches. Their `+0x11` is the is-active-device byte.)
Getters in AI.exe: `DAT_10066c60` → 0x976CD0 (`+0x4A70`), `c64` → 0x976CE0 (`+0x4A74`), `c68` →
0x976CF0 (`+0x4A7C`). MotherVR thunks: FUN_10001cf0 (mouse), FUN_10001d10 (keyboard),
FUN_10001d00 (pad).

---

## 3. The five replacements: exact semantics **[sure]**

Common helper `tier_for(code)`: code < 0x100 → **nullptr** (the original then dereferences
null, but no caller can reach it). Else `<0x200` → A, `<0x400` → B, else → C.
`FUN_10003130` (ecx = code, cdecl-ish, no stack args) = `tier_for(code)->original_code[code & 0xFF]`
(sign-extended int16).

### 3.1 FUN_10008d50: `bool IsDown(int code)`, thiscall, ret 4
```
if code < 0x100:
    if code != -1: remap_all()          // FUN_100031b0
    return false                        // no pass-through of raw pad codes
vr_or_game = false
if original_code(code) != -1: vr_or_game = orig_IsDown(this, original_code(code))
return tier_for(code)->raw_bytes[code & 0xFF] | vr_or_game
```
Quirk: the byte read uses `code & 0xFF`, which equals `k` for digital actions. For an analog
virtual code it reads `raw_bytes[num_digital+j]`, which can run past `digital[0x2C]` into the
float table. The recon should bounds-check and return the game value only.

### 3.2 FUN_10008f00: `float GetValue(int code)`, thiscall, ret 4, result in ST0
```
if code < 0x100: return 0.0f
g  = orig_GetValue(this, original_code(code))            // called even when -1 (orig returns 0)
t  = tier_for(code)
vr = t->analog[(code - t->num_digital) & 0xFF]           // maxss: (vr > g) ? vr : g
return vr > g ? vr : g
```
Same quirk: for a digital code the index wraps (`k - num_digital + 256`) and reads out of
the table. Guard it in the recon.

### 3.3 FUN_10008e20: `float GetAxis(int code, int negate)`, thiscall, **ret 8**, ST0
```
if code < 0x100: return 0.0f
g  = orig_GetAxis(this, original_code(code), negate)     // negate passed through untouched
vr = tier->analog[(code - num_digital) & 0xFF]           // NOT negated
return |vr| > |g| ? vr : g
```
Quirk: `negate` is not applied to the VR value. The original relies on the backends writing
already-signed values. **[sure]**

### 3.4 FUN_10009030: `const char* ControlToName(uint16 code)`, thiscall, ret 4
Compare is on **int16** (`cmp ax, cx`).
```
if (int16)code < 0x100: return nullptr
if !g_vr_glyphs (DAT_10066bc0 == 0): return orig_ControlToName(this, original_code(code))
o = original_code(code)
if o == -1: return "VR_MENU"                             // 0x1005A3E8
return g_glyph_names.at(o).c_str()                       // FUN_100090b0: vector<string>[o],
                                                         //   throws out_of_range if o >= size (0x18)
```
`g_glyph_names` (`std::vector<std::string>` at `DAT_100686d4/d8/dc`) is built in static init
FUN_10001090 as the game's 0x18 pad names ("A", "B", ... "TOUCH_ZOOM_OUT"). The active backend's
slot 9 then overwrites some of them (§4.5).

### 3.5 FUN_10008fa0: `bool HasActivity()`, thiscall, ret 0
```
game = orig_HasActivity(this)                            // (al)
vr   = std::exchange(g_vr_activity /*DAT_10068639*/, false)
any  = game | vr
if any && g_vr_glyphs != vr:                             // input source flipped VR <-> pad
    g_vr_glyphs = vr                                     // DAT_10066bc0
    g_pulse     = true                                   // DAT_1006af7c
    mgr->device_by_type[0] /*pad, +0x1084*/->active(+0x11) = 0
    mgr->device_by_type[1] /*kbd, +0x1088*/->active(+0x11) = 1
    return any
if g_pulse:                                              // next call: restore
    g_pulse = false
    pad->active = 1; kbd->active = 0
return any                                               // only AL is meaningful
```
Interpretation **[likely]**: briefly marking the pad inactive makes AI.exe's device-selection
routine (0x0097CBC0) run its "pad became active" path on the next pass (vt+0x40), which
refreshes button prompts. The prompts then re-query `ControlToName`, which now returns VR or
pad glyphs. The return value feeds that routine's activity test, so the pad also becomes the
active device whenever VR controllers are used. FUN_1001f8a0 also uses the result to clear
`StereoInterface+0x311` (the mouse-look override).

---

## 4. Hardware → game path

Per frame: `StereoInterface` slot 5 FUN_1001f330 calls `backend->vt[10]()`
(OpenVR FUN_10007ad0 / Oculus FUN_10004960). The backend reads the controllers and writes
`tierA/tierB.digital[]/analog[]` directly, and sets `g_vr_activity = 1` whenever a written
digital is true or a written analog has |v| > 0.85. **[sure]**

### 4.1 OpenVR (FUN_10007ad0), legacy `IVRSystem_017`, no IVRInput **[sure]**

```
for role in (LeftHand=1, RightHand=2):
    idx = sys->GetTrackedDeviceIndexForControllerRole(role)              // vt +0x48
    sys->GetControllerStateWithPose(TrackingUniverseStanding=1, idx,
                                    &state[role], 0x40, &pose[role])     // vt +0x88
```
`VRControllerState_t`: pressed @+0x08 (u64), touched @+0x10, rAxis[0] (pad/stick) @+0x18, rAxis[1]
(trigger) @+0x20. Bits: 1 ApplicationMenu, 2 Grip, 32 Axis0 (pad click / touch).
HMD position = `this+0x1C/+0x2C/+0x3C` (translation of the cached HMD 3x4 at backend `+0x10`).

Derived right-touchpad gestures (rx, ry = R.axis0):
```
rclick  = R.pressed bit32
horiz   = |rx| > |ry|
centre  = |rx| < 0.33 && |ry| < 0.33
down    = rclick && !horiz && !centre && ry < 0
up      = rclick && !horiz && !centre && ry > 0
mid     = rclick && centre
nearHead= |R.pose.pos - HMD.pos| < 0.2 m
turn    = (horiz && !centre && (rclick || !SnapRotation)) ? rx * 0.85 : 0
```

### 4.2 Oculus (FUN_10004960), `ovr_GetInputState(session, ovrControllerType_Touch=3)` **[sure]**

The session status is checked first: ShouldRecenter sets `cfg+0x31D` and clears the flag;
ShouldQuit sets `*DAT_10066d58 = 1`; when !HasInputFocus it returns **without clearing the
tiers** (VR state stays stale, which is a latent bug). Fields: Buttons @+0x08 (A=0x1, B=0x2,
RThumb=0x4, X=0x100, Y=0x200, LThumb=0x400, Enter=0x100000), IndexTrigger[L,R] @+0x10/+0x14,
HandTrigger @+0x18/+0x1C, Thumbstick L @+0x20, R @+0x28. Right-hand position = backend
`+0x420..0x428`, HMD position = `+0x368..0x370`. Previous hand-trigger values (edge detect):
`DAT_1006ac10` (L), `DAT_1006ac14` (R).

### 4.3 Resulting action table

`grip` = OpenVR Grip button / Oculus HandTrigger > 0.5. `trig` = trigger axis, digital at > 0.5.
`[T]` = toggled on the rising edge when ToggleGrips (`cfg+0x306`).
Edge state: OpenVR `DAT_1006af41` (R), `DAT_1006af40` (L).

**Tier A (player map, base 0x100)**

| slot | help text (FUN_10007500 / FUN_10004270) | OpenVR (Vive wand) | Oculus Touch |
|---|---|---|---|
| d8 | Sprint | L pad click | LThumb click |
| d9 | Crouch | L trig > 0.5 | L trig > 0.5 |
| d0xA | Aim [T] | R grip | R grip |
| d0xB, d0xE | Fire / Throw / Melee | R trig > 0.5 | R trig > 0.5 |
| d0xC, d0xF | Interact | R pad `down` | A |
| d0xD | Radial Menu | R pad `mid` | RThumb click |
| d0x10, d0x11 | Reload / Activate | R pad `up` | B |
| d0x1A | Previous Weapon | - | !horiz && Ry < -0.5 |
| d0x1B | Next Weapon | - | !horiz && Ry > 0.5 |
| d0x1C | Motion Tracker [T] | L grip && !R pad `mid` | L grip && !RThumb |
| d0x1D | Flashlight | `down && nearHead` | Y \|\| (A && nearHead) |
| d0x26 | Map Menu | L ApplicationMenu | X |
| d0x27 | (raw L grip, hold variant? **[guess]**) | L grip | L grip |
| d0x28 | (raw R grip, hold variant? **[guess]**) | R grip | R grip |
| a0, a1 | Move X/Y | L pad xy × 1.1 if L pad touched, else 0 | L stick xy |
| a3 | Turn | `turn` (above) | Rx if horiz else 0 |

**Tier B (UI map, base 0x200)**

| slot | OpenVR | Oculus |
|---|---|---|
| d0, d0x10 (confirm **[likely]**) | R pad `down` \|\| L pad click | A |
| d1 | R pad `mid` | RThumb click |
| d8 (Pause) | R ApplicationMenu | Enter (Menu) |
| d9 | L grip (raw) | L grip |
| d0xA, d0xF (back **[likely]**) | R pad `up` | B |
| d0xB, d0xE | flashlight gesture (same value as A.d0x1D) | same |
| a0, a1 | L / R trigger value | L / R index trigger |
| a2, a3 | L pad xy if touched | L stick |
| a4, a5 | R pad xy if touched | R stick |

Tier C: never written.

### 4.4 Controller families

The original has exactly two hard-coded schemes: **OpenVR = Vive wand** (touchpad regions)
and **Oculus = Touch** (thumbsticks + ABXY). Under OpenVR, Index, WMR and Touch all go through
the wand scheme via legacy `rAxis[0]`/buttons (stick acts as touchpad, click = Axis0 press).
Index A/B and WMR thumbsticks are **not** used. There is no per-controller-type detection.
**[sure]**

### 4.5 Glyph table per backend (backend slot 9) **[sure]**

Index = original pad code. Ids map 1:1 to the ActionScript `INPUT_ID_VR_*` constants
(`vr_a, vr_b, vr_x, vr_y, vr_lsc, vr_rsc, vr_lg, vr_rg, vr_menu, vr_tp, vr_tp_c/u/d/l/r/lr`).

| pad code | OpenVR (FUN_10007500) | Oculus (FUN_10004270) |
|---|---|---|
| 0 A | VR_TP_D | VR_A |
| 1 B | VR_TP_C | VR_RSC |
| 2 X | VR_TP_U | VR_B |
| 3 Y | (unchanged "Y") | VR_Y |
| 8 BACK | VR_MENU | VR_X |
| 9 START | VR_MENU | VR_MENU |
| 0xA LB | VR_LG | VR_LG |
| 0xB RB | VR_RG | VR_RG |

So the game's default pad layout is A = Interact, B = Radial, X = Reload, Y = Flashlight,
BACK = Map, START = Pause, LB = Motion tracker, RB = Aim **[likely]**. The same functions also
push the controls-help lines ("Move\tL Touchpad", ...) through FUN_100055a0.

### 4.6 Snap rotation (FUN_1001f8a0, StereoInterface) **[sure]**

`turn = GetAxis_hook(pad, 0x12C, 0)`. 0x12C = tier A analog #3, which implies
`num_digital(map A) = 0x29`. That is the max-abs of the real right stick (its original code)
and the VR turn. If `DAT_10068638` (player camera active; set by the camera hook FUN_10001720,
which also stores the camera object in `DAT_1006863c`) and SnapRotation are on, and
`|turn| > 0.5`, `StereoInterface+0x60 != 1` and the latch `+0x314` is clear, then:
```
yaw = cam+0x204 ± 1/(segments/2)           // yaw stored in half-turn units, wrapped to [-1,1]
cam+0x204 = cam+0x1FC = yaw; latch = 1; t0 = clock()
```
The latch is released when the stick returns, or after ≥ 301 ms of hold (auto-repeat).
`cam+0x264 = !SnapRotation | mouseOverride(+0x311)` lets the game's own smooth stick yaw run
only when snap is off. LB+RB on the real pad, or Ctrl+Alt (scancodes 0x1D/0x38) on the
keyboard, recenters via backend vt+0x54.

### 4.7 Haptics

None found (see TL;DR).

---

## 5. Reconstruction design

### 5.1 Data

```cpp
namespace mothervr::input {

enum class Tier : int { Player = 0, Ui = 1, Other = 2 };
inline constexpr int kTierBase[3] = {0x100, 0x200, 0x400};

struct ActionTier {                         // our own; layout need not match the original
    std::array<uint8_t, 0x2C> digital{};
    std::array<float,   0x10> analog{};
    int32_t num_digital = 0, num_analog = 0;
    std::array<int16_t, 0x39> original_code;   // fill(-1)
};

struct GameBinding   { int16_t code; uint8_t device; uint8_t flags; uint32_t unk; };
static_assert(sizeof(GameBinding) == 8);
struct GameAction    { uint32_t unk; GameBinding b[6]; int32_t count; uint8_t rest[0x08]; };
static_assert(sizeof(GameAction) == 0x40);
struct GameAnalog    { uint32_t unk; GameBinding b[6]; int32_t count; uint8_t rest[0x0C]; };
static_assert(sizeof(GameAnalog) == 0x44);
struct GameComposite { uint32_t codes[7]; int32_t count; uint8_t unk20, keep; uint8_t pad[2]; };
static_assert(sizeof(GameComposite) == 0x24);
// map accessors by offset: digital @+0x4, analog @+0xA44, composite @+0xE84,
// composite_count @+0x1038, device_by_type @+0x103C, num_digital @+0x1054, num_analog @+0x1058.
// manager: maps @+0x48/+0x2998/+0x3A0C, pad @+0x1084, keyboard @+0x1088 (+0x11 = active byte).

struct InputState {                         // replaces DAT_10066bc0 / 10068639 / 1006af7c
    std::array<ActionTier, 3> tiers;
    bool vr_glyphs = true;                  // initial value 1 in the original
    std::atomic<bool> vr_activity{false};
    bool pulse = false;
    std::vector<std::string> glyphs;        // 0x18 pad names, backend overrides
};
}
```

### 5.2 The hooks (exact ABI: thiscall via `__fastcall(self, edx, ...)`)

```cpp
using IsDownFn   = bool        (__fastcall*)(void*, void*, int);
using ValueFn    = float       (__fastcall*)(void*, void*, int);
using AxisFn     = float       (__fastcall*)(void*, void*, int, int);
using NameFn     = const char* (__fastcall*)(void*, void*, uint32_t);   // uint16 in low word
using ActivityFn = bool        (__fastcall*)(void*, void*);
struct PadOriginals { IsDownFn is_down; ValueFn value; AxisFn axis; NameFn name; ActivityFn activity; };
// = DAT_10066c6c, c70, c74, c78, c7c (slots +0x00,+0x04,+0x08,+0x10,+0x28 of RVA 0xFE7674)

ActionTier* tier_for(int code) noexcept {
    if (code < 0x100) return nullptr;
    return &g.tiers[code < 0x200 ? 0 : code < 0x400 ? 1 : 2];
}
int original_code(int code) noexcept { return tier_for(code)->original_code[code & 0xFF]; }

bool __fastcall pad_is_down(void* self, void*, int code) noexcept {            // ret 4
    if (code < 0x100) { if (code != -1) remap_all_maps(); return false; }
    const ActionTier& t = *tier_for(code);
    const int k = code & 0xFF, o = original_code(code);
    bool v = o != -1 && g_orig.is_down(self, nullptr, o);
    if (k < t.num_digital) v |= t.digital[k] != 0;     // original: unchecked raw byte read
    return v;
}

float __fastcall pad_value(void* self, void*, int code) noexcept {             // ret 4
    if (code < 0x100) return 0.0f;
    const ActionTier& t = *tier_for(code);
    const float g  = g_orig.value(self, nullptr, original_code(code));
    const int   j  = (code - t.num_digital) & 0xFF;
    const float vr = j < t.num_analog ? t.analog[j] : 0.0f;  // original: unchecked
    return vr > g ? vr : g;
}

float __fastcall pad_axis(void* self, void*, int code, int negate) noexcept {  // ret 8
    if (code < 0x100) return 0.0f;
    const ActionTier& t = *tier_for(code);
    const float g  = g_orig.axis(self, nullptr, original_code(code), negate);
    const int   j  = (code - t.num_digital) & 0xFF;
    const float vr = j < t.num_analog ? t.analog[j] : 0.0f;
    return std::fabs(vr) > std::fabs(g) ? vr : g;     // negate deliberately not applied
}

const char* __fastcall pad_name(void* self, void*, uint32_t raw) noexcept {   // ret 4
    const int16_t code = static_cast<int16_t>(raw);
    if (code < 0x100) return nullptr;
    const int o = original_code(code);
    if (!g.vr_glyphs) return g_orig.name(self, nullptr, static_cast<uint16_t>(o));
    if (o == -1) return "VR_MENU";
    return static_cast<size_t>(o) < g.glyphs.size() ? g.glyphs[o].c_str() : "VR_MENU"; // orig throws
}

bool __fastcall pad_has_activity(void* self, void*) noexcept {                // ret 0
    const bool game = g_orig.activity(self, nullptr);
    const bool vr   = g.vr_activity.exchange(false);
    const bool any  = game || vr;
    auto* mgr = *ai::input_manager;             // *DAT_10066d4c
    auto  active = [](void* dev) -> uint8_t& { return *(static_cast<uint8_t*>(dev) + 0x11); };
    void* pad = *reinterpret_cast<void**>(mgr + 0x1084);
    void* kbd = *reinterpret_cast<void**>(mgr + 0x1088);
    if (any && g.vr_glyphs != vr) {
        g.vr_glyphs = vr; g.pulse = true;
        active(pad) = 0; active(kbd) = 1;
    } else if (g.pulse) {
        g.pulse = false;
        active(pad) = 1; active(kbd) = 0;
    }
    return any;
}
```

MSVC returns `float` from `__fastcall` in ST0 and `bool` in AL, which matches the original.
Return a full 0/1 for bool, which is safe for the `test al, al` callers. Rename the recon
functions and namespace (e.g. `mothervr::input::pad_*`) and fix the header's comments.

### 5.3 Remap (port of FUN_10003200)

Implement exactly as §2.3 over `GameActionMap*` (by offsets). Call it from `pad_is_down` on a
raw code, exactly like the original. A recon improvement would be to also call it once after
the maps exist. Keep the `device == 0` filter, the "-1 the duplicates" behaviour and the
append-binding path (appended digital flags = 0, analog flags = 1). Note that `count` can
reach 6 and the append does not bounds-check (original bug). Guard `count < 6`.

### 5.4 Backend interface

Replace the two hard-coded backend writers with one mapper fed by a normalized snapshot.
That also gives OpenXR/Index/WMR a proper thumbstick scheme.

```cpp
struct VrHand {
    bool  active = false;
    float trigger = 0, grip = 0;     // 0..1
    Vec2  stick{};                   // thumbstick or touchpad
    bool  stick_click = false;
    bool  stick_touch = false;       // NEW vs runtime/openxr_input.h: needed for wand move/pad gating
    bool  primary = false;           // A / X
    bool  secondary = false;         // B / Y
    bool  menu = false;
    Vec3  position{};                // seated/standing space, same space as head
};
struct VrInputSnapshot {
    std::array<VrHand,2> hands;      // [0]=left [1]=right
    Vec3 head_position{};
    enum class Scheme { Touchpad, Thumbstick } scheme;
};
void apply_vr_input(const VrInputSnapshot&, const config::InputSettings&) noexcept;
// writes tiers[0]/[1] per §4.3 (Touchpad = OpenVR column, Thumbstick = Oculus column),
// keeps grip toggle edge state, sets g.vr_activity on any true digital / |analog|>0.85.
// On focus loss: zero the tiers (fixes the Oculus stale-state bug).
// Also: void set_glyph_scheme(Scheme) -> overwrite g.glyphs per §4.5.
```

`runtime/openxr_input.h` already supplies everything except `stick_touch` and the head
position (take that from the view/HMD pose). `ControllerFamily::Touchpad/Thumbstick` maps to
`Scheme`. OpenXR haptics exist in that layer, but the original never vibrates, so feeding
game rumble (pad slots +0x1C/+0x20, `+0x2A4..+0x2B0`) would be a new feature.

### 5.5 Open questions

* Exact AI.exe names of maps A/B/C and of actions d0x27/d0x28, B.d0xB/0xE. They could be
  confirmed by dumping `map+0x04 + 0x40*k` at runtime, or from the game's input config data.
* Whether the game calls IsDown on analog virtual codes (the raw-byte quirk) or GetValue on
  digital ones (the wrap quirk). The guarded versions above are safe either way.
* Meaning of the composite list (`map+0xE84`, flag `+0x21`) that the remap blanks.
* The original passes -1 to `orig_GetValue/GetAxis` for unbound actions. The game returns 0
  for it (`code >= 0x10` path), so this is harmless.
