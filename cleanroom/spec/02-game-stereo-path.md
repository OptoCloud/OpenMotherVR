<!-- Provenance: game analysis of AI.exe 2015 (disassembly of every site, wrapper and game method named here, re-checked against the binary for r2: frame loop, present step, display reset, configure-rendering and shutdown wrappers, CreateDevice, setting handlers); 2020 values were taken from the earlier mod's per-build address table (and, for inline sites, its installer) and have not been verified against a 2020 `AI.exe`; public LibOVR 0.4.0 headers (layouts of the headset description, eye descriptors and texture descriptors, and their constants); observation in game by the spec side (thread ids and call counts per hook; frame times after a display reset; an earlier mod showed each eye at half resolution); user requirements (full resolution from the first answer, eye layout from the created texture, display reset only on a changed setting); AI.exe 2015 analysis by the spec side for r4 (the game's window procedure on focus loss and deactivation); a tester's report on SteamVR with a Steam Frame headset (the game paused or held at a loading screen when another window took the focus); clean-room review round 2 and coverage review of spec r6, applied in r5 (2020 cells not taken from the earlier mod's table set to "?"; the device description fields left free without example values; the function at RVA 0x5E0D40 labelled `CreateDevice` in every document; the no-headset outcome told to the player only when a runtime was chosen; the thread rule of a late detach; the Stereo Mode handler changes no other setting, and a VR run leaves the player's saved values alone; AI.exe 2015 analysis by the spec side for r5: the only call site of `CreateDevice`, the window procedure's RVAs in 01 §4.6). Revision: r5 -->

# 02 - The game's dormant stereo path

Alien: Isolation ships a complete, disabled Oculus Rift DK2 integration built
on LibOVR 0.4.0 (statically linked). It is switched on by graphics setting
0x18 "Stereo Mode" having the value "Rift". When it runs, the game renders
both eyes side by side into one render target, asks a *stereo device* for the
head pose, the per-eye projection and the eye offsets, and lets the device
present the frame.

The mod drives VR through this path. It does not add a new renderer: it gives
the game a stereo manager and a stereo device of its own that the game's
existing code calls. This document lists what the game expects of those two
objects, when and on which thread the game calls them, and which game sites
must be changed so they are used.

All addresses are RVAs (image base 0x400000 in the 2015 file). The 2015
column is verified against the 2015 binary (PE timestamp 0x54E3199D). 2020
values were taken from the earlier mod's per-build address table (and, for
inline sites, its installer) and have not been verified against a 2020
`AI.exe`; "?" means unknown. `01-loading.md` holds the authoritative
per-build table; where this document and 01's table disagree, 01 wins.

Confidence: **[sure]** read directly from the game's code; **[likely]**
strong inference; **[guess]** to be confirmed in game. Statements without a
tag about game code are [sure].

## 1. Required versus free

Required by the game:

- the stereo manager's first 0xB0 bytes have the game's layout (section 3),
  because game code reads and writes those fields directly;
- the stereo device's first 0x1F4 bytes have the game's layout (section 4),
  because the game's own device methods for slots 12 to 14 run on the mod's
  device and read it through those offsets;
- the device's vtable has the game's 21 slots in the game's order, with the
  game's argument lists, `ret N` and return registers (section 5);
- the manager's vtable has the game's 3 slots (section 3.2).

Free for the implementation:

- where the objects live and how they relate in memory beyond the game's
  layouts. The game only follows the pointer at manager +0x04;
- anything the mod stores after +0xB0 in the manager or after +0x1F4 in the
  device;
- how the recenter call sites reach a yaw-only pose, within the constraints
  of section 6.4;
- the internal split between "device" code and "headset runtime" code.

## 2. Calling convention

Every method of both objects, and every game function listed here as
`thiscall`, uses MSVC x86 `__thiscall`: `this` in ECX, stack arguments pushed
right to left, the callee pops them (`ret N`, N = 4 x argument count),
integer, pointer and bool results in EAX (bools in AL), EBX, ESI, EDI and EBP
preserved. No device or manager method returns a float in ST0; float results
go through pointer arguments.

The game calls device methods through the vtable (`mov eax,[ecx]; call
[eax+off]`, or a tail `jmp`), so replacement methods must use exactly this
convention. A mismatch in `ret N` corrupts the caller's stack immediately
(observed: the game crashes at launch). In C++ either `__thiscall` member
functions or free functions declared `__fastcall` with an unused second
(EDX) parameter give the same stack behaviour.

The renderer accessors in section 8 are plain `__cdecl` (caller pops).

## 3. The stereo manager (0xB0 bytes)

### 3.1 Allocation site

The renderer system object's constructor (function at RVA 0x5AFA70, `this` in
ESI = the static object at RVA 0x13583A0) allocates the manager and
constructs it in place:

| RVA 2015 | RVA 2020 | Bytes (2015) | Meaning |
|---|---|---|---|
| 0x5AFBA1 | 0x501CC1 | `68 b0 00 00 00` | `push 0xB0` (size for `operator new`) |
| 0x5AFBA6 | ? | `89 be 98 f6 0c 00` | `mov [esi+0xCF698], edi` (edi = 0): zeroes RVA 0x1427A38 |
| 0x5AFBAC | ? | `89 be 9c f6 0c 00` | `mov [esi+0xCF69C], edi`: zeroes RVA 0x1427A3C |
| 0x5AFBB2 | ? | `e8 rel32` | `operator new` (RVA 0x044330) |
| 0x5AFBB7 | ? | `83 c4 04`, `3b c7`, `74 12` | pop the argument, test for null |
| 0x5AFBC0 | ? | `e8 rel32` | constructor (RVA 0x5D9A60), `this` = new block |
| 0x5AFBC5 | ? | `89 86 8c e6 00 00` | `mov [esi+0xE68C], eax`: store the manager pointer |

On 2020 only the first address is known; the rest of the sequence must be
located and checked against the 2015 bytes before use (section 11 item 1).

`[esi+0xE68C]` is the global that every other game site reads as the
manager pointer: VA 0x1766A2C in the 2015 file, RVA 0x1366A2C (2020: RVA
0x1376BDC). Game code loads it with `mov ecx,[StereoObjectPtr]` before
calling manager methods; every caller tests it for null first.

Required: this sequence stores a pointer to the mod's manager instead of
allocating the game's. The constraints on any change here:

- when execution reaches the store at 0x5AFBC5, EAX holds the address of the
  mod's manager, so the game's own store puts it into the global;
- the game's `operator new` (0x5AFBB2) and the manager constructor
  (0x5AFBC0) do not run, so no game manager is ever created;
- the change stays within 0x5AFBA1..0x5AFBC4 (0x24 bytes) and leaves every
  byte from 0x5AFBC5 on as it is;
- the two zero stores at 0x5AFBA6 and 0x5AFBAC may run or be skipped: both
  dwords are zero-initialised image data [likely] and this sequence runs
  once, at start-up, before anything writes them.

How the span is rewritten is the implementation's choice. Before writing,
check the bytes of the span against the table above (the `e8` opcodes and
their targets for the two calls) and the store at 0x5AFBC5; if anything
differs, patch nothing (01 §2.4).

### 3.2 Manager vtable (game: RVA 0xFE57BC, 3 slots; 2020: ?)

| Slot | Off | Game RVA 2015 | 2020 | Signature | Meaning | Required of the mod |
|---|---|---|---|---|---|---|
| 0 | +0x00 | 0x5E3F40 | ? | `(unsigned flags)` `ret 4`, returns `this` | scalar deleting destructor | Never called once the game's manager is replaced (no call was observed). Must not free the manager |
| 1 | +0x04 | 0x5CCDB0 | 0x51EF20 | `(const Setting* s)` `ret 4` | a graphics setting changed; `s+0` is the setting id | Section 6.5 |
| 2 | +0x08 | 0x5CCEB0 | ? | `(const Setting* s, const int* value)` `ret 8`, bool in AL | may this setting take this value? The game refuses "Stereo Mode = Rift" unless a device exists with state 1 | Return true (AL = 1) for everything |

The manager's vtable pointer must point at the mod's own three-entry table;
it must never point at the game's (slot 1 of the game's would run the game's
Rift preset and its stop-sensor path).

### 3.3 Manager fields

| Off | Type | Game ctor value | Meaning | Who uses it |
|---|---|---|---|---|
| +0x00 | vptr | game vtable | see 3.2 | game |
| +0x04 | device* | 0 | **the stereo device**, or null when there is no headset. Every game site tests it before use; `[dev+4] == 2` means "stereo running" | game everywhere; written only by the mod |
| +0x08 | target object* | 0 | the **stereo render target object** (0x84 bytes, created by the game in the configure-rendering wrapper, 6.7): word +0x00 width, word +0x02 height, word +0x80 the renderer's render-target index (section 8). Passed to device slot 3 | game creates and frees it; mod reads |
| +0x0C | target object* | 0 | a second render target object of the same size, created and freed with +0x08 (purpose not established) | game |
| +0x10 | | 0 | game private | game |
| +0x14 | word | 0 | index of a further render target the configure-rendering wrapper creates and the shutdown wrapper frees | game |
| +0x20..+0x5F | float 4x4 | identity | world matrix of the virtual screen quad (mode 1), row-vector convention; written when mode 1 is entered (05) | game, mod |
| +0x60 | int | 0 | current stereo mode: 0 = 3D world with a head-locked HUD quad, 1 = virtual screen (menus, videos drawn on a world-fixed quad) | game, mod |
| +0x64 | float | 1.0 | scene visibility multiplier: 1 = the 3D scene is drawn, 0 = black | game reads, mod writes (05) |
| +0x68..+0x73 | int array | zero | the stereo mode stack: data pointer +0x68, element count +0x6C, capacity +0x70. Push RVA 0x5D9C00 (2020 0x52BE00) `(int mode) ret 4`, pop RVA 0x5D9C80 (2020 0x52BE80) `() ret` | game; mod policy in 05 |
| +0x74 | byte | 0 | game private | game |
| +0x78..+0xA7 | float 3x4, row-major | identity | **reference (recenter) pose**, in the device's head pose convention (03 §1). Written by the recenter call sites (6.4) | game reads, mod writes |
| +0xA8 | byte | 1 | target for the positional weight at +0xAC: 1 = position tracking on. The game writes it at RVA 0x25B3F1 (sets 1) and RVA 0x275F58 (writes a computed value) | game |
| +0xAC | float | 1.0 | positional weight in [0, 1]. The game multiplies the head translation by it (6.3). The game's per-frame update (RVA 0x5BE2A0) moves it towards +0xA8 at 1.0 per second | game |

The mod's manager must be initialised to the game constructor's values above
(all other bytes zero), with one difference: the reference pose at +0x78
must start as the "no rotation, no translation" pose *in the device's
convention*, rows (-1,0,0,0), (0,-1,0,0), (0,0,1,0) (03 §1). The game's
identity would be a 180 degree roll in that convention until the first
recenter.

The game's per-frame update is replaced by the mod (04, 05). The replacement
does not animate +0xAC; it stays 1.0, so position tracking is always applied.

## 4. The stereo device (0x1F4 bytes)

The game constructs its own device with an aligned allocation of 0x1F4 bytes
(alignment 4) and the constructor at RVA 0x5BE9C0; the mod provides its own
object instead, through CreateDevice (6.1). Layout (offsets from the device):

| Off | Type | Meaning | Read by game code |
|---|---|---|---|
| +0x000 | vptr | 21-slot vtable, section 5 | yes |
| +0x004 | int | **state**: 0 = none / failed, 1 = initialised, 2 = sensor started (stereo running) | yes, everywhere, on several threads (section 9) |
| +0x008 | uint | frame index, incremented by slot 9 | game slot 5 only |
| +0x00C | ovrHmd | LibOVR handle. Must be 0 | game's own slots 0 and 1 to 9 only |
| +0x010..+0x08B | ovrHmdDesc (0.4.0, 0x7C bytes) | headset description, below | game slots 17, 18 |
| +0x08C..+0x0AB | ovrRecti[2] `{x, y, w, h}` | **eye viewports** in the stereo render target, eye 0 then eye 1 | game slot 15 (not used once slot 15 is replaced) |
| +0x0AC..+0x0CB | ovrFovPort[2] `{Up, Down, Left, Right}` (tangents) | **EyeFov**: the FOV each eye is rendered with | game slots 12, 13, 14 |
| +0x0CC..+0x13B | ovrEyeRenderDesc[2] (0x38 each: int Eye, FovPort, Recti DistortedViewport, float PixelsPerTanAngle[2], float ViewAdjust[3]) | only **ViewAdjust** (3 floats at +0x2C in each) is used: eye 0 at **+0x0F8**, eye 1 at **+0x130** | game slot 12 |
| +0x13C..+0x177 | ovrD3D11Texture (0x3C: int API, Sizei TextureSize, Recti RenderViewport, ID3D11Texture2D*, ID3D11ShaderResourceView*, 6 reserved dwords) | eye 0 texture descriptor: API +0x13C (5 = D3D11), TextureSize +0x140, RenderViewport +0x148, texture +0x158, view +0x15C | game's own slots 3 and 8 only |
| +0x178..+0x1B3 | ovrD3D11Texture | eye 1 texture descriptor, same layout (RenderViewport at +0x184) | game's own slots only |
| +0x1B4..+0x1EB | ovrPosef[2] (0x1C each: quaternion, position) | per-eye render poses written by the game's own slot 7 | game's own slots only |
| +0x1EC | int | **render-target width** (both eyes side by side) | game slot 16 |
| +0x1F0 | int | **render-target height** | game slot 16 |

ovrHmdDesc 0.4.0, at device +0x10 (device offsets):

| Off | Field | Required value |
|---|---|---|
| +0x10 | Handle | 0 |
| +0x14 | Type | free (below) |
| +0x18, +0x1C | ProductName, Manufacturer (`const char*`) | free (below); if non-null, each must point at a string that lives as long as the device |
| +0x20 | HmdCaps | free (below) |
| +0x24 | SensorCaps | free (below) |
| +0x28 | DistortionCaps | free (below) |
| +0x2C, +0x30 | Resolution w, h | 0 (read only by the game's slot 17, which is replaced) |
| +0x34 | WindowsPos x, y | 0 |
| +0x3C | DefaultEyeFov[2] | each eye's real tangents (03 §3) |
| +0x5C | MaxEyeFov[2] | the symmetric FOV actually rendered (03 §3) |
| +0x7C | EyeRenderOrder[2] | 0 |
| +0x84 | DisplayDeviceName (`const char*`) | null (read only by the game's slot 18, which is replaced) |
| +0x88 | DisplayId | 0 |

No game code outside the replaced device slots reads Type, the three caps
fields, ProductName or Manufacturer, so any values are acceptable (zero, or
any consistent description); if non-null, each name points at a string that
lives as long as the device.

Fields marked "game's own slots only" are free for the implementation once
those slots are replaced, including the two texture descriptors. The device
must keep the configured stereo texture and its view somewhere, because
submission and the desktop mirror use them (03 §6, §8); where is its choice.

## 5. Device vtable (game: RVA 0xFE5418, 21 slots)

"Wrapper" is the manager method through which the game reaches the slot.
Unless noted, the wrapper does nothing unless the device exists and its state
is 2. "Keep" means the slot must point at the game's own function. "Thread"
is where the game calls it (section 9).

| # | Off | Game RVA 2015 | 2020 | Meaning | Args | `ret` | Returns | Wrapper / callers | Thread | Required of the mod |
|---|---|---|---|---|---|---|---|---|---|---|
| 0 | +0x00 | 0x5CD460 | 0x51F5D0 | scalar deleting destructor | `unsigned flags` | 4 | - | only the game's manager shutdown (RVA 0x5D9BA0, passes 0), which the mod never lets run | - | Never called once the game's manager is replaced (3.1, 6.1). Must not free the device; any harmless behaviour is acceptable |
| 1 | +0x04 | 0x5B8E20 | 0x50AFC0 | start sensor | - | 0 | - | manager slot 1 on setting 0x18 (6.5) | game | If state is 1, set 2 and log the transition (11). Otherwise nothing |
| 2 | +0x08 | 0x5B8E50 | 0x50AFF0 | stop sensor | - | 0 | - | the game's manager slot 1 when Stereo Mode leaves "Rift" (never with the mod's slot 1) | - | If state is 2, set 1 |
| 3 | +0x0C | 0x5BEB20 | 0x510CE0 | configure rendering | stereo target object* | 4 | bool (AL), ignored by the wrapper | wrapper RVA 0x5D9B30 (6.7); requires device and state != 0. Called from RVA 0x613EF4 (start-up) and 0x614129 (display reset) | render | Section 7.3 and 03 §4.4; return true |
| 4 | +0x10 | 0x5B8DF0 | 0x50AF90 | shut down rendering | - | 0 | bool (AL) | wrapper RVA 0x5D9B60, requires device and state != 0; called from RVA 0x613FBA (display reset) | render | Release the runtime's render resources (03 §4.4); return true |
| 5 | +0x14 | 0x5B9100 | 0x50B2B0 | begin frame | - | 0 | - | wrapper RVA 0x5B8870: slot 5, slot 7(0), slot 7(1) | render | On return, EyeFov and ViewAdjust are valid for this frame and the runtime's frame has begun as head-tracked or screen (03 §6). Where input is polled is the implementation's choice (04) |
| 6 | +0x18 | 0x5B9120 | 0x50B2D0 | end frame | - | 0 | - | wrapper RVA 0x5B88B0: slot 8(0), slot 8(1), then tail-jumps to slot 6 | render | Submit to the headset, draw the desktop mirror and **Present** the game's swapchain (03 §6). The game does not present by itself while slot 10 answers true |
| 7 | +0x1C | 0x5B9130 | 0x50B2E0 | begin eye render | `int eye` | 4 | - | begin-frame wrapper | render | Nothing |
| 8 | +0x20 | 0x5B9190 | 0x50B340 | end eye render | `int eye` | 4 | - | end-frame wrapper | render | Nothing |
| 9 | +0x24 | 0x5B9200 | 0x50B3B0 | increment frame index | - | 0 | - | wrapper RVA 0x5B88F0, called once per frame from a task function (RVA 0x374FA0) | not established | Increment device +0x08. Nothing reads it once slot 5 is replaced |
| 10 | +0x28 | 0xAA70F0 | ? | **device presents** | - | 0 | bool (AL); the game's returns 1 | wrapper RVA 0x5B8DD0 (returns false without a running device), called in the present step (6.8) | render | Return true. False makes the game present the back buffer itself as well |
| 11 | +0x2C | 0x5E0DA0 | ? | **head pose** | `float* out` (3x4) | 4 | `out` in EAX | relative pose RVA 0x5BE360 (6.3); the two recenter sites (6.4) | game | Write the current head pose (03 §1, including its rule for a pose the runtime reports as not valid; never NaN); record that the game asked for it this frame |
| 12 | +0x30 | 0x5CD4C0 | 0x51F630 | apply eye transform | `int eye, float4x4* view, float4x4* proj` | 0xC | - | wrapper RVA 0x5B86A0 (callers RVA 0x5B3355, 0x5B3985 in `POV::SetEye`, 0x5F0946); eye shader constants RVA 0x5B8910 | render | **Keep** |
| 13 | +0x34 | 0x5B8E70 | 0x50B010 | FOV and aspect | `int eye, float* vfov, float* aspect` | 0xC | - | wrapper RVA 0x5B86C0 (callers RVA 0x5B336C, 0x5F095D) | render | **Keep** |
| 14 | +0x38 | 0x5B8F10 | 0x50B0C0 | NDC scale and offset of an eye's FOV | `int eye, float out[4]` | 8 | - | wrapper RVA 0x5B86E0 (default {0,0,1,1}; caller RVA 0x57EC15); slot 12 | render | **Keep** |
| 15 | +0x3C | 0x5B9000 | 0x50B1B0 | eye viewport rectangle | `int eye, int halfWidth, int height, int* left, int* top, int* right, int* bottom` | 0x1C | - | wrapper RVA 0x5B8730, called from the POV viewport (RVA 0x5B085C) | render | **Replace**: eye 1 gives `{halfWidth, 0, 2·halfWidth, height}`, any other eye `{0, 0, halfWidth, height}` (07 §4.4) |
| 16 | +0x40 | 0x5B9050 | 0x50B200 | render-target size | `int* w, int* h` | 8 | not used by callers | slots 12 and 13 (every eye, every frame); manager wrapper RVA 0x5B8750 (6.2); the configure-rendering wrapper (6.7) | render, and start-up | Section 7.2 |
| 17 | +0x44 | 0x5B9070 | 0x50B220 | headset panel resolution | `int* w, int* h` | 8 | not used by callers | manager wrapper RVA 0x5B8780 (6.2), whose call site the mod redirects | - | Leave `*w`, `*h` unchanged |
| 18 | +0x48 | 0x5B9090 | 0x50B240 | display device name | `char* buf, int size` | 8 | `buf` in EAX | wrapper RVA 0x5B87B0 (only checks the device), called at RVA 0x6137BD in the output selection (6.9) | start-up, render | Leave the buffer unchanged (the game zeroed it), return `buf` |
| 19 | +0x4C | 0x5B90B0 | 0x50B260 | position tracked? | - | 0 | bool (AL) | no caller | - | Return true. Use the game's argument count (none) |
| 20 | +0x50 | 0x669C90 | ? | unused (a shared bare `ret`) | - | 0 | - | none | - | Nothing |

The table entry after slot 20 in the game's vtable is 0; the game never reads
past slot 20. Entries the mod's vtable has after slot 20 are therefore reached
only through sites the mod itself patches (6.4).

The 2020 RVAs of the slots listed above differ from the 2015 ones by
nearly constant amounts (see the table), which helps when locating the
2020 wrappers (section 11 item 1).

### 5.1 What the kept game methods rely on

The game's slots 12 to 14 run with `this` = the mod's device and read it
through the game layout. Eye index -1 means "centre" (the flat camera).

- **Slot 12** (apply eye transform) does nothing unless state (+0x04) is 2.
  It calls slot 16 through the vtable for the target size, reads EyeFov
  (+0xAC) of the eye (eye 0's for -1), and builds a perspective projection
  with vertical FOV `atan(Up) + atan(Down)`, aspect `(W/2)/H`, and the near
  and far planes from the game globals at RVA 0x1358138 / 0x135813C. It then
  calls slot 14 through the vtable and overwrites the projection's X and Y
  scale with slot 14's scale and applies its offset, so the final projection
  covers exactly the EyeFov tangents whatever the target's pixel aspect.
  Last, unless eye is -1, it post-multiplies the view matrix by a
  translation by ViewAdjust (+0x0F8 for eye 0, +0x130 for eye 1): with the
  game's row-vector convention, view-space points move by +ViewAdjust, so
  the eye moves by -ViewAdjust. It adds no rotation.
- **Slot 13** (FOV and aspect) reads EyeFov and calls slot 16:
  `vfov = atan(Up) + atan(Down)`, `aspect = (w * 0.5) / h`.
- **Slot 14** reads EyeFov only: for symmetric tangents it yields scale
  `(1/Right, 1/Up)` and offset 0 (LibOVR's FovPort-to-NDC formula).
- **The game's slot 15** (replaced) reads the eye viewport at +0x8C +
  16 x eye and returns `left = x, top = y, right = x + w, bottom = y + h`,
  ignoring its second and third arguments; for any target other than the
  main stereo target (the planar reflection target, 07) that is the wrong
  rectangle. The replacement gives the same rectangles for the main target
  when its viewports are the two halves.

Consequences for the mod:

- slot 16 is called several times per frame on the render thread; it must
  not allocate, log or call the runtime in a way that blocks;
- EyeFov, ViewAdjust and the viewports must be valid whenever state is 2.

## 6. Game sites the mod changes or must serve

### 6.1 CreateDevice (the manager's device creation)

`CreateDevice` is the spec's label for the manager method at RVA 0x5E0D40
that creates the stereo device; 01 and 03 use the same label, and its call
site is `StereoInitCall` (01 §4.2).

| | 2015 | 2020 |
|---|---|---|
| call site | RVA 0x613941 | RVA 0x565E11 |
| game target | RVA 0x5E0D40 | ? |

Sequence at the site: `mov ecx,[StereoObjectPtr]` (`8b 0d` + VA), `push 1`
(`6a 01`), `call rel32` (`e8 ...`). The site is in the display
initialisation (RVA 0x6138F0), which has a single caller (RVA 0x59C307) and
runs once at start-up on the main thread; nothing else calls the target
(`StereoInitCall` is its only call site, and no pointer to it exists in the
image), so the game creates the device exactly once per run [sure]. In
particular the display reset (9.3) does not call it.

The target is `thiscall(manager, int type)`, `ret 4`. The game's version, for
type 1, allocates and constructs the LibOVR device into manager +0x04; then,
if the device's state is 0, it calls the manager shutdown (RVA 0x5D9BA0),
which frees the stereo targets, calls device slot 0 and frees the device
through the game allocator.

The mod must redirect this call (check the `e8` opcode and, where known, its
target first) to its own function with the same convention. When it returns for
`type == 1`, exactly one of these holds:

- **Headset.** Manager +0x04 points at the mod's device in state 1, with the
  headset description and EyeFov (the rendered FOV) filled as in section 4;
  manager +0x78 holds a reference pose (03 §2), which is retaken once the
  headset is in use if it was not in use at that moment (03 §2.2); a
  reference is only ever taken from a fully valid head pose, so without one
  the retake stays pending (03 §2.1); the
  world scale from `-vrscale` (06 §2) is in effect.
- **No headset.** Manager +0x04 is null, the reason is logged, and the
  game's manager shutdown has **not** run (it would free objects the mod
  owns). When a runtime was chosen and failed to start, the player has been
  told once (13); when no runtime was chosen (none available, or a forced
  runtime missing) there is no message (01 §5.3). The game then runs flat
  (7.5).

For any other `type` nothing changes.

The headset runtime chosen at load time (03 §6, 09) is started at most once
per process, so a second entry (not observed) leaves +0x04 as the first one
left it. The only order forced here is that the runtime must be up before
the eye FOV and the reference pose can be read from it. Each call and its
outcome is logged (11).

### 6.2 Render-target size and display size (manager methods)

| Call site 2015 | 2020 | Game target | Signature | Purpose in the game |
|---|---|---|---|---|
| RVA 0x60FD64 | RVA 0x562244 | RVA 0x5B8750 | `thiscall(manager, int* w, int* h)` `ret 8` | render-target size: writes 1280x800, then, if a device exists (any state), asks device slot 16 |
| RVA 0x60FE5D | RVA 0x56233D | RVA 0x5B8780 | `thiscall(manager, int* w, int* h)` `ret 8` | display size: writes 1280x800, then, if a device exists, asks device slot 17 |

Both are `e8 rel32` calls preceded by `mov ecx,[StereoObjectPtr]` and two
pushes, and both sit behind a "Stereo Mode == Rift" comparison (6.6).

The first site is in the back-buffer size query (RVA 0x60FC40,
`thiscall(display, int* w, int* h)`, `ret 8`), which the game runs at
start-up (from RVA 0x613AE8) and at every display reset (from RVA 0x613FE5).
It starts from 1280x720, takes the size from the resolution settings, then
(Rift) asks the stereo object, then lets a size forced in the display object
(+0x4B8 flag, +0x4BC/+0x4C0) override the answer. The second is at the end of
the display-resolution query (RVA 0x60FDD0).

- **Render-target size.** On entry `*w`, `*h` hold the size the game derived
  from its resolution setting: the desktop window's size. With a device the
  replacement must store that size as the desktop window size (the mirror
  fits itself to it, 03 §8, and the swapchain is sized from it, 03 §9.2),
  then answer through device slot 16 (section 7.2). With no device it must
  leave both values unchanged. The game sizes its swapchain back buffer from
  this answer; the stereo render target itself is sized by slot 16 inside
  the configure-rendering wrapper (6.7).
- **Display size.** The replacement must leave `*w`, `*h` unchanged and
  return, so the window keeps the resolution the player chose instead of
  1280x800 or the headset panel size.

### 6.3 Relative head pose (served, not changed)

RVA 0x5BE360 `thiscall(manager, float* out3x4)` `ret 4`, called once per
frame from the VR camera behaviour at RVA 0x34DEA0 (in its update, RVA
0x34DE50), on the game thread, only while a head-tracked camera is active.
It writes identity, and if the device exists with state 2:

1. calls device slot 11 into `out`;
2. subtracts the reference translation (manager +0x84, +0x94, +0xA4) from
   `out`'s translation;
3. expresses `out` relative to the reference rotation (manager +0x78 rows);
4. multiplies the resulting translation by the positional weight at manager
   +0xAC.

The result drives the first-person camera. This is why the reference pose
and slot 11 must use the same convention (03 §1). Because this is the only
per-frame caller of slot 11, "slot 11 was called since the last end frame"
is exactly "this frame is head tracked" (03 §6).

### 6.4 The two recenter sites

Both call **device slot 11** (`+0x2C`) to fill the reference pose at manager
+0x78:

| Site 2015 | 2020 | Bytes | Context |
|---|---|---|---|
| RVA 0x5BE2E8 | RVA 0x5104A8 | `8b 40 2c` (`mov eax,[eax+0x2C]`), then `8d 56 78`, `push edx`, `call eax` | in the game's per-frame update RVA 0x5BE2A0, when pad LB and RB are held |
| RVA 0x5B8691 | RVA 0x50A831 | `8b 42 2c` (`mov eax,[edx+0x2C]`), `call eax`, `ret` | in the manager's reset-orientation method RVA 0x5B8680 `thiscall() ret` (checks only that a device exists), called from RVA 0x091BD7 (the menu's reset-view action [likely]) |

In the game, slot 11 serves both the per-frame head pose and the reference
pose. The mod needs them to differ:

- the reference must be **yaw-only** (heading plus position, no pitch or
  roll), or a recenter while looking down would tilt the whole world;
- a recenter must not count as "the game asked for a head-tracked pose this
  frame" (03 §6).

So these sites must reach a function that returns the yaw-only reference
pose (03 §2.1), with slot 11's signature (`float* out` 3x4, `ret 4`, returns
`out`). How they reach it is the implementation's choice. The game facts
that bound the choice:

- each site loads the method from the device's vtable with a signed 8-bit
  displacement (`8b`, ModRM mod bits 01, disp8 0x2C at site + 2) and calls
  it through the register just loaded, with ECX = the device;
- a redirection that only changes that displacement can reach a vtable
  entry at most 0x7F bytes from the vtable's start, and must pick an entry
  the game never calls: the game reads slots 0 to 20 only (+0x00 to +0x50,
  section 5), so an entry after slot 20 is reached only through the
  patched sites;
- any other redirection (for example of the whole call) is equally valid.

Check before writing that each site holds the bytes listed above.

The game's per-frame update containing the first site is replaced as a whole
(04, 05), so only the reset-orientation site stays live; patching both keeps
the game consistent if the update is ever left alone.

### 6.5 Setting 0x18 "Stereo Mode" handler (manager slot 1)

The game's handler (RVA 0x5CCDB0), for a setting whose id (at +0) is 0x18
while a device exists:

- if the new value is "Rift" (the value id of the string at RVA 0xFE5010):
  calls device slot 1 (start sensor) and sets nine graphics settings (the
  game's Rift preset, below);
- otherwise, if the device state is 2: calls device slot 2 (stop sensor).

Required replacement behaviour, for id 0x18 with a device present, whatever
the new value (the "Rift" comparisons are forced elsewhere, 6.6, so the
stored value is irrelevant): afterwards the device has made slot 1's
transition (state 1 to 2, 7.1) and these settings hold:

| Setting id | Name | Value |
|---|---|---|
| 0x00 | Depth of Field | "Off" |
| 0x15 | SSAO | "Off" |
| 0x0D | Motion Blur | "Off" |

The order of the state change and the three setter calls is free: each
setter call reaches this handler with a different id, which does nothing.
For any other id, or with no device, do nothing. Depth of field and motion
blur are unpleasant in a headset; SSAO is a screen-space effect that is wrong
across a side-by-side target (it is also disabled by a patch, 05).

The handler changes no setting other than these three. VSync (id 0x07),
which the game's own Rift preset turns on, keeps the player's value: while
stereo runs it has no effect, because the game's own Present (which uses the
sync interval) is skipped when slot 10 answers true (6.8) and the device
presents with its own interval (03 §6).

A run with runtime None must show the player's own values of the three
settings, because runtime None is the unmodded game (01 §2.3). Whether a
value set through the setter below reaches the player's saved graphics
settings is not established (section 11 item 5); if it does, the three
changes must not remain in the saved settings after a VR run (for example
the player's values are restored before the game saves them), so that
turning VR off gives back the effects the player chose.

The setter is `thiscall(settings, int id, const char* value)` `ret 8` at RVA
0x5A8910 (2020: 0x4FAA50) on the settings object at RVA 0x1358A50 (2020:
0x1368C00). The value strings must be the game's own spellings shown above.

The game's own Rift preset, for reference, in its order: 0x00 Depth of Field
"Off", 0x0B Shadow Map Resolution "512", 0x0C Shadow Mapping "High", 0x10
Planar Reflections "Off", 0x15 SSAO "Standard", 0x16 Texture Filtering
"Anisotropic 2x", 0x0D Motion Blur "Off", 0x07 VSync "On", 0x12 Volumetric
Lighting "Off". Other known ids: 0x09 anti-aliasing, 0x18 Stereo Mode. The
mod deliberately does **not** turn Planar Reflections off (07).

The event fires during start-up once the stereo target is configured
(observed: the transition to state 2 follows the slot 3 call within
milliseconds), on the game thread.

### 6.6 "Stereo Mode == Rift" tests

The path is gated by comparisons of the Stereo Mode setting against the
"Rift" value (returned by RVA 0x5BE350). Each is a 2-byte `cmp r32, r32`
(`3b ..`); writing `39 c0` (`cmp eax, eax`) over it makes the test always
equal. Sites:

| 2015 | 2020 |
|---|---|
| 0x0336B6 | 0x042F96 |
| 0x60FD58 | 0x562238 |
| 0x60FE50 | 0x562330 |
| 0x60FEBD | 0x56239D |
| 0x60FF15 | 0x5623F5 |
| 0x611740 | 0x563C10 |
| 0x61378A | 0x565C5A |
| 0x613C80 | 0x566150 |

They are listed here because they open the path; `05-game-patches.md` and
01 §4.1 own them.

### 6.7 Configure-rendering and shut-down-rendering wrappers (served, not changed)

**Configure rendering**, RVA 0x5D9B30, `thiscall(manager)`, `ret 0`. Does
nothing unless the device exists and its state is not 0. Otherwise:

1. RVA 0x5CCF00: asks **device slot 16** for `(w, h)` (1280x800 if it does
   not answer), then creates two 0x84-byte render target objects of that
   size and stores them at manager +0x0C and **+0x08**;
2. RVA 0x5BE8B0: creates a further render target (index in manager +0x14);
3. calls **device slot 3** with manager +0x08.

So the stereo render target's size is slot 16's answer at this moment
(state 1 at start-up, state 2 at a display reset). This is why slot 16 must
answer the full configured size from the first call (section 7.2; 00 §4
item 1).

**Shut down rendering**, RVA 0x5D9B60, `thiscall(manager)`, `ret 0`. Does
nothing unless the device exists and its state is not 0. Otherwise frees the
target at +0x14, frees the target objects at +0x0C and +0x08 (RVA 0x5CD010,
which zeroes both fields), then tail-jumps to **device slot 4**.

After slot 4 returns, the texture and view the device recorded at slot 3 are
gone: nothing may use them until the next slot 3.

### 6.8 The present step (served, not changed)

RVA 0x610240, `thiscall(display)`, runs once per rendered frame between the
render and the end-frame wrapper. In order, each only if the manager pointer
is non-null:

1. RVA 0x5B8850: with a running device (state 2), a renderer call (RVA
   0x6275E0) for the current eye owner;
2. RVA 0x5CD170: the stereo overlay draw (the HUD quad in mode 0, using the
   quad builder call at RVA 0x5CD2EE, 05);
3. the slot 10 wrapper (RVA 0x5B8DD0): false without a running device,
   otherwise slot 10's answer.

If step 3 gives false (or there is no manager), the game presents itself
(RVA 0x623F30: `IDXGISwapChain::Present(renderer +0x0C sync interval, 0)`).
If true, it does not; the device's slot 6 must then present. A slot 10 that
answered false while slot 6 also presents would show every frame twice on the
desktop and add a vsync wait.

### 6.9 Output selection (served, not changed)

RVA 0x613760 (called at start-up and at every display reset, RVA 0x614051):
if Stereo Mode is "Rift" (forced, 6.6) and the manager exists, it zeroes a
0x104-byte buffer, asks the slot 18 wrapper for the display device name
(`size` 0x104) and looks for a monitor with that name (RVA 0x60FA20). With
the buffer left empty the lookup fails and the game falls back to its normal
output choice, which is the required result.

### 6.10 Per-frame update

The game's manager update, `thiscall(manager, float dt)` `ret 4` at RVA
0x5BE2A0, called from RVA 0x3B4100 (2020: 0x3C4FB0) on the game thread, does
the LB+RB recenter (6.4) and animates +0xAC. The mod replaces this call (check
the `e8` opcode); its behaviour is specified in 04 and 05.

## 7. Device behaviour by state

### 7.1 States and transitions

| From | To | Cause |
|---|---|---|
| (none) | 0 | the device object before `CreateDevice`, or a runtime that failed |
| 0 | 1 | CreateDevice succeeded (6.1) |
| 1 | 2 | slot 1, from setting event 0x18 at start-up (6.5) |
| 2 | 1 | slot 2; not reached in normal play (the mod's manager slot 1 never calls slot 2) |
| any | 0, detached | the runtime cannot drive the headset after all (7.4) |

State 2 lasts from start-up to exit. Display resets, focus changes, a
headset taken off, the pause menu and loading screens do not change it.

### 7.2 Slot 16: the render-target size

Slot 16 answers the side-by-side size at the **configured** pixel scale `p`
(03 §4.1, 03 §10) in every state, including state 1: the stereo target is
built from the answer given in state 1 at start-up (6.7), so an answer at a
reduced scale in state 1 leaves the eyes at half resolution (00 §4 item 1).

The answer is defined as follows. With `wL x hL` and `wR x hR` the runtime's
recommended per-eye sizes at scale 1.0:

- the effective scale is `p' = min(p, 16384 / (wL + wR), 16384 / max(hL, hR))`,
  so the target fits Direct3D 11's 16384-texel limit;
- each eye's size is its recommended size times `p'`, truncated to an
  integer;
- `W = width(eye 0) + width(eye 1)` and `H = max(height(eye 0),
  height(eye 1))`. (On all current headsets both eyes have the same size.)

Every call writes `W`, `H` through its pointers.

The **eye layout** (+0x1EC, +0x1F0 and the viewports at +0x8C) is an
invariant, not a copy of the latest answer: it always describes the texture
last configured by slot 3, split into two halves (+0x1EC and +0x1F0 its
width and height, eye 0 viewport `{0, 0, W/2, H}`, eye 1 `{W/2, 0, W/2, H}`
with `W`, `H` the texture's size, integer division). Before the first
slot 3 it describes slot 16's latest answer. So after a pixel-scale change
the next display reset builds the stereo target at the new size (6.7), and
the eye halves never disagree with the texture being rendered.

Log the computed size once per change of `(W, H)`, never per call; 11 lists
what the event conveys.

### 7.3 Slot 3: configure rendering

When slot 3 returns:

- the device uses the texture and shader resource view of the stereo render
  target named by the word at +0x80 of the argument, obtained through the
  renderer accessors (section 8); a null argument keeps the current ones;
- the eye layout (7.2) matches the texture's real size, taken from
  `ID3D11Texture2D::GetDesc` (the target's size can differ from slot 16's
  answer);
- the runtime's render resources exist for the game's device (03 §4.4, 09),
  or, if the runtime reports that it cannot drive the headset with this
  device, the device has detached (7.4);
- the desktop mirror's size-dependent resources match the new texture
  (03 §8), and none made for an earlier texture is left allocated;
- the return value is true (AL = 1).

The configuration is logged once per call (11); slot 3 is not called per
frame.

### 7.4 Detaching after a late runtime failure

When the runtime fails after CreateDevice succeeded (for example an OpenXR
session that cannot be created on the game's device, 09 §7), afterwards:
the device's state is 0, manager +0x04 is null, the runtime is released, the
VR options are marked unavailable in the options page (06), and the player
has been told once (13). Every game wrapper then sees no device and the game
runs flat from the next frame (7.5). The runtime is not started again in
this run.

The order of these is free, with one exception: the state and manager
+0x04 are cleared before the runtime is released, so that a thread that
reads them afterwards (9.1) does not start using a released runtime.

A late detach can happen on the render thread (slot 3 runs there, 6.7), so
the device state and manager +0x04 then have the render thread as a writer
and the game and job threads as readers (atomic stores and loads, 9.1). A
call into the runtime that another thread began before the detach (for
example a head pose read by slot 11 on the game thread) may still be
running when the detach happens. The runtime is not released while such a
call can still be in progress: either the release waits until none can be
(bounded, so the render thread never waits longer than one such call), or
the runtime is kept until process exit and only stops being used. Either
way, no call that starts after the detach uses the runtime.

### 7.5 No device

With manager +0x04 null (runtime None is a separate case: there the mod
installs no stereo patches at all, 01 §2.3), the game's own code behaves as
follows [sure, from the wrappers]: no begin or end frame calls, no slot 11,
slots 12 to 15 not reached (the POV code takes the centre view and the whole
target), the present step presents the back buffer itself (6.8), the
render-target and display size replacements leave the window size alone,
and the setting 0x18 handler does nothing. The game runs flat on the
monitor.

## 8. Renderer accessors

Plain `__cdecl` functions in AI.exe. The renderer pointer is read from the
global at RVA 0x17DF5CC (2020: 0x17EF77C).

| Returns | RVA 2015 | RVA 2020 | Arguments | Game code |
|---|---|---|---|---|
| `ID3D11Device*` | 0x621E20 | 0x574310 | `(Renderer*)` | `[r+0x4]` |
| `IDXGISwapChain*` | 0x621E30 | 0x574320 | `(Renderer*)` | `[r+0x8]` |
| `ID3D11DeviceContext*` (immediate) | 0x621E40 | 0x574330 | `(Renderer*)` | `[r+0x1EF3E8]` |
| `ID3D11Texture2D*` of render target `idx` | 0x6232D0 | 0x575800 | `(Renderer*, uint16 idx)` (pushed as a 32-bit value) | `[r + 0x1CD240 + idx*16]` |
| `ID3D11ShaderResourceView*` of render target `idx` | 0x6232F0 | 0x5757C0 | `(Renderer*, uint16 idx)` | `[r + 0x1CD248 + idx*16]` |

The returned interface pointers are borrowed (no reference added); the mod
must not release them. The stereo render target's index is the word at
+0x80 of the object the game passes to device slot 3 (manager +0x08). The
swap chain's sync interval is the dword at renderer +0x0C.

## 9. Threads and the frame

### 9.1 Who calls what, where

Observed in game by the spec side (thread id of each call) and from the call
sites:

| Thread | Calls |
|---|---|
| main thread, start-up | manager allocation (3.1), CreateDevice (6.1), the first render-target and display size queries (6.2), output selection (6.9), swapchain creation, the first configure rendering (6.7) |
| game (simulation) thread | per-frame update (6.10), relative head pose and so slot 11 (6.3), the recenter sites (6.4), manager slot 1 (6.5), the menu and loading screen methods (05), pixel shader creation (08) |
| render thread | begin frame, the renders that call slots 12 to 16 (and 15 through the POV viewport), the present step with slot 10, end frame, the display reset (9.3) with slots 4, 16, 18 and 3 |
| job threads | the per-object visibility tests (08 §2), which read the device state |

Slot 11 was never observed between a begin frame and its end frame (0 of 600
frames); the game thread prepares frame k+1 while the render thread draws
frame k, so the pose slot 11 returns is used by the next begin and end
frame.

Requirements:

- the device state and manager +0x04 are read on the game, render and job
  threads, and written on the game thread (6.1) and, for a late detach, on
  the render thread (7.4): write them as single aligned 32-bit atomic
  stores and read them as atomic loads (a value one frame stale is
  harmless);
- the head pose (written by slot 11 on the game thread) and anything the
  runtime produces on the render thread for the game thread (poses, view
  FOVs) must be published so that no reader sees a half-written matrix
  (for example a sequence lock or double buffering);
- the "head pose asked for" record set by slot 11 and cleared by slot 6
  crosses the same threads and must be an atomic flag;
- no allocation, file I/O or logging per frame on the render thread; log
  once per change.

### 9.2 One frame, in order

Main frame loop (RVA 0x390240 onwards, render thread):

1. begin-frame wrapper (RVA 0x5B8870, call at RVA 0x3902A9, only if the
   manager pointer is non-null): with state 2, slot 5, slot 7(0), slot 7(1);
2. the frame's renders: per-eye POV setup (`POV::SetEye`, RVA 0x5B38D0)
   calls the wrappers for slots 12 (which itself calls 16 and 14) and 13;
   POV viewports call slot 15; planar reflections (07);
3. the present step (RVA 0x610240, call at RVA 0x39038E, 6.8): overlay draw,
   slot 10, and the game's own Present only if slot 10 answered false;
4. end-frame wrapper (RVA 0x5B88B0, call at RVA 0x3903AD): with state 2,
   slot 8(0), slot 8(1), slot 6;
5. the display-reset check (RVA 0x3903B2, 9.3).

Slot 9 runs once per frame from a task (RVA 0x374FA0). Two blocking loops
used while the game loads render frames with the same bracket: begin frame
at RVA 0x409338 / 0x43AE88, present step at RVA 0x4093BB / 0x43AF1F, end
frame at RVA 0x4093D9 / 0x43AF3D. The device must therefore not assume that
every frame comes from the main loop; it treats every begin/end pair alike.

### 9.3 Display reset

The display object (RVA 0x13583E8) holds a "rebuild render targets" byte at
+0x88 (RVA 0x1358470; 2020: 0x1368620). It is set by:

- the resolution setter (RVA 0x611250) when the window size changes;
- the restore path after a minimised window is restored (RVA 0x611CB0,
  `ShowWindow(SW_RESTORE)`);
- the display object's setting handler (RVA 0x6104C0) when a display setting
  with id 2, 3, 4, 5 or 6 changes (setting 7, VSync, only updates the sync
  interval and buffer count);
- the mod, when a setting the stereo targets are built from changes (pixel
  scale, mirror view, symmetric mirror; 06 §4). The mod must raise it only
  on an actual change: a reset costs a visible stall (observed: the frame
  after a reset takes 177 to 232 ms, so raising it without a change stalls
  every menu open).

After the end frame, the main loop checks the byte; if it is set and the
dword at RVA 0x125127C is 0, the game runs the reset (RVA 0x613F90, render
thread), in this order:

1. shut-down-rendering wrapper (6.7): stereo targets freed, slot 4;
2. back-buffer size query (6.2): the redirected site, slot 16;
3. pixel format and output selection (6.9): slot 18;
4. swapchain rebuilt (the mod sees `IDXGISwapChain::ResizeBuffers` on the
   game's swapchain, 03 §9.2);
5. configure-rendering wrapper (6.7): new stereo targets from slot 16, then
   slot 3;
6. the byte is cleared; the main loop then rebuilds the other size-dependent
   targets (lighting targets at half size, planar reflection target, 07).

CreateDevice is not called again and the device stays in state 2 throughout.
Each reset changes the stereo texture (new pointer, possibly new size), which
is why slot 3 must re-read it every time.

### 9.4 Other events

| Event | What the game does | Required of the device |
|---|---|---|
| Window loses focus | nothing in the stereo path. Its window procedure, on `WM_KILLFOCUS` while its display mode is fullscreen (which it keeps under VR, 03 §9.2), posts the system event that opens the pause menu and releases its mouse buttons; on `WM_ACTIVATE` inactive it releases its cursor clip and clears its keyboard state (03 §9.6; the window procedure and both branches are in 01 §4.6) | keep running: the game keeps calling begin and end frame; the runtime keeps receiving frames. While a headset is attached, focus loss is not passed to the game, so it does not pause; activation still is (03 §9.6) |
| Window minimised and restored | display reset on restore (9.3) | as 9.3 |
| Headset taken off, put on | nothing | keep state 2 and keep submitting; a queued recenter waits until the headset is in use again (03 §2.2) |
| VR runtime changed in the options menu | the mod sets the game's quit request; the game exits (06 §4.5) | nothing special; the new runtime is used at the next launch |
| Game exit | no call to the stereo device or manager was observed | do not shut the runtime down from the DLL's process-detach handler (it runs under the loader lock, where runtime shutdown can deadlock); leaving the runtime connection to process exit is acceptable |

## 10. Acceptance

Checks an implementation must pass on the 2015 build, read from the mod's log
or seen in game:

1. Start-up with OpenVR or OpenXR and a headset connected: the log shows, in
   this order, the device-creation event reporting a headset, the
   render-target size event (W x H equal to twice the recommended eye width
   times the pixel scale, by the recommended height times the pixel scale),
   swapchain creation, the slot 3 event with the **same** W x H, then the
   state-2 event (11 names the events). No crash at the first frame (a
   `ret N` mismatch crashes there).
2. With pixel scale 1.0 the configured texture is exactly the runtime's
   recommended per-eye size side by side (for example 2 x 2688 by 2688 on a
   2688 x 2688 headset), never half of it.
3. Every frame is presented exactly once: the desktop mirror updates, and a
   frame-rate tool shows the game's swapchain presenting at the headset's
   rate, not twice it.
4. In the headset, depth is correct and the left eye sees the left half
   (eye 0 ViewAdjust +IPD/2 in X, 03 §5).
5. Change the pixel scale in the options menu and close the menu: the log
   shows one display reset (a `ResizeBuffers` on the game's swapchain), a new
   render-target size and a configure rendering with the new size; the image
   in the headset is sharper or softer at once, with no slice mismatch
   between the eyes. Opening and closing the pause menu without changing a
   setting logs no reset.
6. Resize or minimise and restore the game window: one display reset, the
   image stays correct in the headset and in the mirror.
7. With a pixel scale whose target would exceed 16384 texels in width, the
   logged size is capped at 16384 wide and the game still starts.
8. With the runtime set to OpenVR and SteamVR not running beforehand, and a
   headset connected: initialisation starts SteamVR and the game starts in
   VR as in item 1 (SteamVR need not be running first). With the runtime set
   to OpenVR or OpenXR but the runtime unable to start, or no headset
   appearing within the bounded wait (03 §6.4): the log says no headset was
   attached and why, the player is told once, and the game runs flat on the
   monitor with working menus and gameplay. With no runtime available (for
   example `-vrforceopenvr` with SteamVR not installed) the log says why
   and no message is shown (01 §5.3).
9. The game's "reset view" menu item and pad LB+RB recenter yaw only:
   looking down while recentering leaves the horizon level.
10. With the address checks of 3.1, 6.1, 6.2, 6.4 and 6.10 deliberately made
    to fail (a modified copy of the executable), nothing is patched and the
    log says which check failed.
11. After a VR run that reached state 2, start with runtime None: Depth of
    Field, SSAO and Motion Blur show the values the player chose in the
    game's options, and no VR run has changed VSync.
12. Make the OpenXR session fail at slot 3 (09 §7) while the game thread
    is reading head poses: the game continues flat with no crash, and the
    log shows the detach once.

What cannot be checked without a 2020 binary: every 2020 address above.

## 11. Open questions

1. **2020 addresses.** Slot 10 and slot 11 game functions, slot 20, the
   manager destructor and slot 2, both vtables, the `CreateDevice` target,
   every wrapper
   and helper of sections 6.7 to 9 and the allocation sequence bytes after
   its first instruction are unknown or unverified for 2020. Resolve by
   reading the 2020 `AI.exe`: find the stereo device vtable by the 21-entry
   table whose slots 1 to 9 match the 2020 values of section 5, then check each site's
   bytes against the 2015 bytes given here.
2. **Manager +0x0C target and +0x10.** What the second target object and
   the dword at +0x10 are for is not established; the mod does not need
   them. Resolve by following the users of manager +0x0C in the stereo
   overlay draw (RVA 0x5CD170).
3. **Thread of slot 9 and of the loading loops.** The task that calls slot 9
   and the two loading render loops (9.2) were not tied to a thread id.
   Resolve with a per-hook thread-id log during a level load. Nothing in the
   required behaviour depends on it as long as slot 9 stays a plain
   increment of a field nobody reads.
4. **Menu reset-view caller.** That RVA 0x091BD7 is the menu's "reset view"
   item is [likely]; confirm by selecting the item in game and seeing a
   recenter logged.
5. **Are settings set through the setter saved?** Whether
   `SettingsSetOption` (RVA 0x5A8910) only changes the running value or
   also reaches the graphics settings the game saves for the player is not
   established; 6.5 depends on it. Resolve by following the writers of the
   game's saved graphics settings and their callers in the 2015 `AI.exe`,
   or by checking the saved settings after a VR run that changed the three
   values.
