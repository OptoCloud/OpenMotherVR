<!-- Provenance: game analysis of AI.exe 2015 (disassembly of the sites and functions named here), observation in game (mod log of test runs, git history of the earlier reconstruction), behaviour of the reference mod as observed. Revision: r1 -->

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
objects and which game sites must be changed so they are used.

All addresses are RVAs in the 2015 build (image base 0x400000) unless marked.
A second column gives the 2020 build where it is known. `01-loading.md` holds
the authoritative per-build address table; where this document and that table
disagree, the table wins.

## 1. Required versus free

Required by the game:

- the stereo manager's first 0xB0 bytes have the game's layout (section 3),
  because game code reads and writes those fields directly;
- the stereo device's first 0x1F4 bytes have the game's layout (section 4),
  because the game's own device methods for slots 12-15 run on the
  mod's device and read it through those offsets;
- the device's vtable has the game's 21 slots in the game's order, with the
  game's argument lists, `ret N` and return registers (section 5);
- the manager's vtable has the game's 3 slots (section 3.2).

Free for the implementation:

- where the objects live (static or heap), and whether the device is embedded
  in the manager. The game only follows the pointer at manager +0x04;
- anything the mod stores after +0xB0 in the manager or after +0x1F4 in the
  device;
- how the recenter call sites reach a yaw-only pose (section 6.4); an extra
  22nd vtable entry (offset +0x54) after the game's 21 is one way;
- the internal split between "device" code and "headset runtime" code.

## 2. Calling convention

Every method of both objects, and every game function listed here as
`thiscall`, uses MSVC x86 `__thiscall`: `this` in ECX, stack arguments pushed
right to left, the callee pops them (`ret N`, N = 4 x argument count),
integer/pointer/bool results in EAX (bools in AL). No method returns a float
in ST0; float results go through pointer arguments.

The game calls device methods through the vtable (`mov eax,[ecx]; call
[eax+off]`), so replacement methods must use exactly this convention. A
mismatch in `ret N` corrupts the caller's stack immediately (observed: the
game crashes at launch). An implementation in C++
can use `__thiscall` member functions, or free functions declared
`__fastcall` with an unused second (EDX) parameter, which has the same stack
behaviour.

The renderer accessors in section 8 are plain `__cdecl` (caller pops).

## 3. The stereo manager (0xB0 bytes)

### 3.1 Allocation site

The renderer initialisation allocates the manager and constructs it in place:

| RVA 2015 | RVA 2020 | Bytes | Meaning |
|---|---|---|---|
| 0x5AFBA1 | 0x501CC1 | `68 b0 00 00 00` | `push 0xB0` (size for `operator new`) |
| 0x5AFBA6 | | `89 be 98 f6 0c 00` | `mov [esi+0xCF698], edi` (edi = 0) |
| 0x5AFBAC | | `89 be 9c f6 0c 00` | `mov [esi+0xCF69C], edi` |
| 0x5AFBB2 | | `e8 rel32` | `operator new` |
| 0x5AFBC0 | | `e8 rel32` | constructor (RVA 0x5D9A60), `this` = new block |
| 0x5AFBC5 | | `89 86 8c e6 00 00` | `mov [esi+0xE68C], eax`: store the manager pointer |

`esi` is a static game object, so `[esi+0xE68C]` is the global that every
other game site reads as the manager pointer: VA 0x1766A2C, RVA 0x1366A2C
(2020: RVA 0x1376BDC). Game code loads it with `mov ecx,[0x1766A2C]` before
calling manager methods.

The mod must make this sequence store a pointer to its own manager instead
of allocating the game's. Whatever replaces it must still perform the two
zero stores at +0xCF698 / +0xCF69C, or the implementation must make sure those
fields are already zero (an earlier approach that jumped over them relied on
the memory being zero).

### 3.2 Manager vtable (game: VA 0x13E57BC, RVA 0xFE57BC; 3 slots)

| Slot | Off | Game RVA | Signature | Meaning |
|---|---|---|---|---|
| 0 | +0x00 | 0x5E3F40 | `(unsigned flags)` `ret 4` | scalar deleting destructor. A static replacement must not free itself |
| 1 | +0x04 | 0x5CCDB0 | `(const Event* e)` `ret 4` | setting changed. `e+0` is the setting id. See 7 |
| 2 | +0x08 | 0x5CCEB0 | `(const Event* e, const int* value)` `ret 8`, bool in AL | may this setting take this value? The game refuses "Stereo Mode = Rift" unless a device exists with state 1. The replacement must return true for everything |

2020 RVAs: slot 1 0x51EF20, slot 2 0xD1F020 (from the address table, not
re-verified).

### 3.3 Manager fields

| Off | Type | Game ctor value | Meaning | Who uses it |
|---|---|---|---|---|
| +0x00 | vptr | game vtable | see 3.2 | game |
| +0x04 | device* | 0 | **the stereo device**, or null when there is no headset. Every game site tests it before use; `[dev+4] == 2` means "stereo running" | game everywhere |
| +0x08 | object* | 0 | the game's stereo render-target object. The game sets it and passes it to device slot 3. Its word at +0x80 is the render-target index for the accessors in 8 | game sets, mod reads |
| +0x0C..+0x13 | | 0 | game private | game |
| +0x14 | word | 0 | game private | game |
| +0x20..+0x5F | float 4x4 | identity | world matrix of the virtual screen quad (mode 1), row-vector convention; written when mode 1 is entered (see 05) | game, mod |
| +0x60 | int | 0 | current stereo mode: 0 = 3D world with a head-locked HUD quad, 1 = virtual screen (menus, videos drawn on a world-fixed quad) | game, mod |
| +0x64 | float | 1.0 | scene visibility multiplier: 1 = the 3D scene is drawn, 0 = black | game reads, mod writes (05) |
| +0x68..+0x73 | container | zero | the stereo mode stack: data pointer at +0x68, element count at +0x6C (+0x70 probably the capacity). Push RVA 0x5D9C00 `(int mode) ret 4`, pop RVA 0x5D9C80 `() ret` | game; mod policy in 05 |
| +0x74 | byte | 0 | game private | game |
| +0x78..+0xA7 | float 3x4, row-major | identity | **reference (recenter) pose**, in the device's head pose convention (03 section 1). Written by the recenter call sites (6.4) | game reads, mod writes |
| +0xA8 | byte | 1 | target for the positional weight at +0xAC: 1 = position tracking on. The game writes it at RVA 0x25B3F1 (sets 1) and RVA 0x275F58 (writes a computed value) | game |
| +0xAC | float | 1.0 | positional weight in [0, 1]. The game multiplies the head translation by it (6.3). The game's per-frame update (RVA 0x5BE2A0) moves it towards +0xA8 at 1.0 per second | game |

The mod's manager must be initialised to the game constructor's values above,
with one difference: the reference pose at +0x78 must start as the "no
rotation, no translation" pose *in the device's convention*, which is
rows (-1,0,0,0), (0,-1,0,0), (0,0,1,0) (see 03 section 1). The game's identity
would be a 180 degree roll in that convention until the first recenter.

The game's per-frame update is replaced by the mod (04, 05). That replacement
does not have to animate +0xAC; the reference behaviour leaves it at 1.0, so
position tracking is always applied. Animating it like the game is allowed.

## 4. The stereo device (0x1F4 bytes)

The game constructs its own device with `new(0x1F4)` and the constructor at
RVA 0x5BE9C0; the mod provides its own object instead, through CreateDevice
(6.1). Layout (offsets from the device):

| Off | Type | Meaning | Read by game code |
|---|---|---|---|
| +0x000 | vptr | 21-slot vtable, section 5 | yes |
| +0x004 | int | **state**: 0 = none / failed, 1 = initialised, 2 = sensor started (stereo running) | yes, everywhere |
| +0x008 | uint | frame index, incremented by slot 9 | game slot 5 only |
| +0x00C | ovrHmd | LibOVR handle. Must be 0 | game's own slots only |
| +0x010..+0x08B | ovrHmdDesc (0.4.0, 0x7C bytes) | headset description, below | slots 17, 18 of the game |
| +0x08C..+0x0AB | ovrRecti[2] `{x, y, w, h}` | **eye viewports** in the stereo render target, eye 0 then eye 1 | game slot 15 |
| +0x0AC..+0x0CB | ovrFovPort[2] `{Up, Down, Left, Right}` (tangents) | **EyeFov**: the FOV each eye is rendered with | game slots 12, 13, 14 |
| +0x0CC..+0x13B | ovrEyeRenderDesc[2] (0x38 each) | only **ViewAdjust** (3 floats at +0x2C in each) is used: eye 0 at **+0x0F8**, eye 1 at **+0x130** | game slot 12 |
| +0x13C..+0x177 | ovrD3D11Texture (0x3C) | eye 0 texture descriptor: API id +0x13C (5 = D3D11), TextureSize +0x140, RenderViewport +0x148, ID3D11Texture2D* +0x158, ID3D11ShaderResourceView* +0x15C | game's own slots 3 and 8 only |
| +0x178..+0x1B3 | ovrD3D11Texture | eye 1 texture descriptor, same layout | game's own slots only |
| +0x1B4..+0x1EB | ovrPosef[2] (0x1C each) | per-eye render poses written by the game's own slot 7 | game's own slots only |
| +0x1EC | int | **render-target width** (both eyes side by side) | game slot 16 |
| +0x1F0 | int | **render-target height** | game slot 16 |

ovrHmdDesc 0.4.0, at device +0x10 (device offsets):

| Off | Field | Notes |
|---|---|---|
| +0x10 | Handle | 0 |
| +0x14 | Type | 6 = DK2 is a plausible value |
| +0x18, +0x1C | ProductName, Manufacturer (`const char*`) | the headset's strings (from the runtime), or null |
| +0x20 | HmdCaps | 0x83 is a plausible DK2 value |
| +0x24 | SensorCaps | 0x70 |
| +0x28 | DistortionCaps | 2 |
| +0x2C, +0x30 | Resolution w, h | read only by the game's slot 17, which is replaced |
| +0x34 | WindowsPos x, y | |
| +0x3C | DefaultEyeFov[2] | the headset's real per-eye FOV (03 section 3) |
| +0x5C | MaxEyeFov[2] | the FOV actually rendered (03 section 3) |
| +0x7C | EyeRenderOrder[2] | |
| +0x84 | DisplayDeviceName (`const char*`) | read only by the game's slot 18, which is replaced |
| +0x88 | DisplayId | |

No game read of the caps fields outside the game's own device methods was
established; filling them with the plausible DK2 values above is harmless and
recommended.

Fields marked "game's own slots only" are free for the implementation once
those slots are replaced, but keeping them meaningful (texture pointer and
viewports in the descriptors) costs nothing and helps debugging.

## 5. Device vtable (game: VA 0x13E5418, RVA 0xFE5418; 21 slots)

"Wrapper" is the manager method through which the game normally reaches the
slot. Unless noted, the wrapper does nothing unless the device exists and its
state is 2. "Keep" means the slot must point at the game's own function.

| # | Off | Game RVA | Meaning | Args | `ret` | Returns | Wrapper / callers | Required of the mod |
|---|---|---|---|---|---|---|---|---|
| 0 | +0x00 | 0x5CD460 | scalar deleting destructor | `unsigned flags` | 4 | - | none found | Stop the sensor if running. Do not free a static object |
| 1 | +0x04 | 0x5B8E20 | start sensor | - | 0 | - | manager slot 1 on setting 0x18 | state 1 -> 2 |
| 2 | +0x08 | 0x5B8E50 | stop sensor | - | 0 | - | manager slot 1 (game) when Stereo Mode leaves "Rift" | state 2 -> 1 |
| 3 | +0x0C | 0x5BEB20 | configure rendering | stereo target object* | 4 | bool (AL) | wrapper RVA 0x5D9B30 passes manager +0x08; requires device and state != 0. Called when the stereo target is created, from RVAs 0x613EF4 and 0x614129 | Describe the eye texture, set up runtime and mirror resources; return true. 03 section 4 |
| 4 | +0x10 | 0x5B8DF0 | shut down rendering | - | 0 | bool (AL) | wrapper RVA 0x5D9B60 | Release runtime render resources; return true |
| 5 | +0x14 | 0x5B9100 | begin frame | - | 0 | - | wrapper RVA 0x5B8870: slot 5, then slot 7(0), slot 7(1) | Per-frame start: input, eye offsets into ViewAdjust, choose head-tracked or screen presentation. 03 section 6 |
| 6 | +0x18 | 0x5B9120 | end frame | - | 0 | - | wrapper RVA 0x5B88B0: slot 8(0), slot 8(1), then slot 6 | Submit to the headset, draw the desktop mirror and **Present** the game's swapchain. The game's version presented inside LibOVR; the game does not Present by itself while stereo runs |
| 7 | +0x1C | 0x5B9130 | begin eye render | `int eye` | 4 | - | begin-frame wrapper | Nothing |
| 8 | +0x20 | 0x5B9190 | end eye render | `int eye` | 4 | - | end-frame wrapper | Nothing |
| 9 | +0x24 | 0x5B9200 | increment frame index | - | 0 | - | wrapper RVA 0x5B88F0 | `++` device +0x08 |
| 10 | +0x28 | 0xAA70F0 | ready query | - | 0 | bool (AL), game returns 1 | wrapper RVA 0x5B8DD0 | Return true |
| 11 | +0x2C | 0x5E0DA0 | **head pose** | `float* out` (3x4) | 4 | `out` in EAX | relative pose RVA 0x5BE360 (6.3); the two recenter sites (6.4) | Write the current head pose (03 section 1); note that the game asked for it this frame |
| 12 | +0x30 | 0x5CD4C0 | apply eye transform | `int eye, float4x4* view, float4x4* proj` | 0xC | - | wrapper RVA 0x5B86A0; eye shader constants RVA 0x5B8910; `POV::SetEye` RVA 0x5B38D0 | **Keep** |
| 13 | +0x34 | 0x5B8E70 | FOV and aspect | `int eye, float* vfov, float* aspect` | 0xC | - | wrapper RVA 0x5B86C0 | **Keep** |
| 14 | +0x38 | 0x5B8F10 | NDC scale and offset of an eye's FOV | `int eye, float out[4]` | 8 | - | wrapper RVA 0x5B86E0 (default {0,0,1,1}); slot 12 | **Keep** |
| 15 | +0x3C | 0x5B9000 | eye viewport rectangle | `int eye, int halfWidth, int height, int* left, int* top, int* right, int* bottom` | 0x1C | - | wrapper RVA 0x5B8730; POV viewport setup | Keep, or replace with a version that honours the size arguments (07) |
| 16 | +0x40 | 0x5B9050 | render-target size | `int* w, int* h` | 8 | not used by callers | slots 12 and 13 (every eye, every frame); manager wrapper RVA 0x5B8750 (6.2) | Return the side-by-side size. Must be cheap and must not change the eye layout of a texture in use. 03 section 4 |
| 17 | +0x44 | 0x5B9070 | headset panel resolution | `int* w, int* h` | 8 | not used by callers | manager wrapper RVA 0x5B8780 (6.2) | Leave `*w`, `*h` unchanged |
| 18 | +0x48 | 0x5B9090 | display device name | `char* buf, int size` | 8 | `buf` in EAX | wrapper RVA 0x5B87B0, called at RVA 0x6137BD | Leave the buffer unchanged, return `buf` |
| 19 | +0x4C | 0x5B90B0 | position tracked? | - | 0 | bool (AL) | no caller found | Return true. Use the game's argument count (none) |
| 20 | +0x50 | 0x669C90 | unused (bare `ret`) | - | 0 | - | none found | Nothing |

The table entry after slot 20 in the game's vtable is 0; the game never reads
past slot 20.

2020 RVAs of the kept game functions: slot 12 0x51F630, slot 13 0x50B010,
slot 14 0x50B0C0, slot 15 0x50B1B0.

### 5.1 What the kept game methods rely on

The game's slots 12-15 run with `this` = the mod's device and read it through
the game layout. Eye index -1 means "centre" (the flat camera).

- **Slot 12** (apply eye transform) does nothing unless state (+0x04) is 2.
  It calls slot 16 through the vtable for the target size, reads EyeFov
  (+0xAC) of the eye (eye 0's for -1), and builds a perspective projection
  with vertical FOV `atan(Up) + atan(Down)`, aspect `(W/2)/H`, and the near and
  far planes from the game globals at RVA 0x1358138 / 0x135813C. It then calls
  slot 14 through the vtable and overwrites the projection's X and Y scale
  with slot 14's scale and applies its offset, so the final projection covers
  exactly the EyeFov tangents whatever the target's pixel aspect. Last, unless
  eye is -1, it post-multiplies the view matrix by a translation by ViewAdjust
  (+0x0F8 for eye 0, +0x130 for eye 1): with the game's row-vector
  convention, view-space points move by +ViewAdjust, so the eye moves by
  -ViewAdjust.
- **Slot 13** (FOV and aspect) reads EyeFov and calls slot 16:
  `vfov = atan(Up) + atan(Down)`, `aspect = (w * 0.5) / h`.
- **Slot 14** reads EyeFov only: for symmetric tangents it yields scale
  `(1/Right, 1/Up)` and offset 0 (LibOVR's FovPort-to-NDC formula).
- **Slot 15** reads the eye viewport at +0x8C + 16 x eye and returns
  `left = x, top = y, right = x + w, bottom = y + h`. Its second and third
  arguments (half width and height of the target being set up) are ignored,
  so for any target other than the main stereo target it returns the wrong
  rectangle. 07 needs a version that honours them.

Consequences for the mod:

- slot 16 is called several times per frame; it must not do expensive work
  and must keep returning the size whose eye layout matches +0x8C;
- EyeFov, ViewAdjust and the viewports must be valid whenever state is 2.

## 6. Game sites the mod changes or must serve

### 6.1 CreateDevice (manager `Init(int type)`)

| | 2015 | 2020 |
|---|---|---|
| call site | RVA 0x613941 | RVA 0x565E11 |
| game target | RVA 0x5E0D40 | |

Sequence at the site: `mov ecx,[0x1766A2C]` (`8b 0d 2c 6a 76 01`), `push 1`
(`6a 01`), `call rel32` (`e8 ...`). The target is
`thiscall(manager, int type)`, `ret 4`. The game's version, for type 1,
creates the LibOVR device into manager +0x04 and shuts the manager down
(RVA 0x5D9BA0) when that fails.

The mod must redirect this call to its own function with the same
convention, which, when `type == 1`:

1. brings up the headset runtime (03);
2. on success: fills the device as in section 4 (descriptor, EyeFov = the
   rendered FOV, state 1) and stores it at manager +0x04; on failure stores
   null, so the game continues without VR, and must not call the game's
   shutdown;
3. on success takes the first reference pose into manager +0x78 (03
   section 2).

The call happens once at startup, before the game asks for the render-target
size and before it creates its swapchain (observed order in test logs:
CreateDevice, render-target size, swapchain creation, configure rendering,
sensor start).

### 6.2 Render-target size and display size (manager methods)

| Call site 2015 | 2020 | Game target | Signature | Purpose in the game |
|---|---|---|---|---|
| RVA 0x60FD64 | RVA 0x562244 | RVA 0x5B8750 | `thiscall(manager, int* w, int* h)` `ret 8` | render-target size: writes 1280x800, then asks device slot 16 |
| RVA 0x60FE5D | RVA 0x56233D | RVA 0x5B8780 | `thiscall(manager, int* w, int* h)` `ret 8` | display size, at the end of the display-resolution query (RVA 0x60FDD0): writes 1280x800, then asks device slot 17 |

Both are `e8 rel32` calls preceded by `mov ecx,[0x1766A2C]` and two pushes, and
both sit behind a "Stereo Mode == Rift" comparison (6.6).

- **Render-target size.** On entry `*w`, `*h` hold the size the game derived
  from its resolution setting: the desktop window's size. The replacement
  must remember that size (the desktop mirror letterboxes for it, 03
  section 8), then write the side-by-side stereo size computed with the
  current pixel scale (03 section 4) through the pointers. With no device it must
  leave the values unchanged. The game sizes **both** its stereo render
  target **and** its swapchain back buffer from this answer, and does not ask
  again before creating them.
- **Display size.** The replacement must leave `*w`, `*h` unchanged and
  return, so the window keeps the resolution the player chose instead of
  1280x800 or the headset panel size.

### 6.3 Relative head pose (served, not changed)

RVA 0x5BE360 `thiscall(manager, float* out3x4)` `ret 4`, called once per
frame from the VR camera behaviour at RVA 0x34DEA0 (in its update, RVA
0x34DE50). It writes identity, and if the device exists with state 2:

1. calls device slot 11 into `out`;
2. subtracts the reference translation (manager +0x84, +0x94, +0xA4) from
   `out`'s translation;
3. expresses `out` relative to the reference rotation (manager +0x78
   rows);
4. multiplies the resulting translation by the positional weight at
   manager +0xAC.

The result drives the first-person camera. This is why the reference pose
and slot 11 must use the same convention (03 section 1).

### 6.4 The two recenter sites

Both call **device slot 11** (`+0x2C`) to fill the reference pose at manager
+0x78:

| Site 2015 | 2020 | Bytes | Context |
|---|---|---|---|
| RVA 0x5BE2E8 | RVA 0x5104A8 | `8b 40 2c` (`mov eax,[eax+0x2C]`) | in the game's per-frame update RVA 0x5BE2A0, when pad LB and RB are held |
| RVA 0x5B8691 | RVA 0x50A831 | `8b 42 2c` (`mov eax,[edx+0x2C]`) | in the manager's reset-orientation method RVA 0x5B8680 `thiscall() ret`, called from RVA 0x91BD7 (likely the menu's reset-view action) |

Problem: in the game, slot 11 serves two purposes, the per-frame head pose
and the reference pose. The mod needs them to differ:

- the reference must be **yaw-only** (heading plus position, no pitch or
  roll), or a recenter while looking down would tilt the whole world;
- a recenter must not count as "the game asked for a head-tracked pose this
  frame" (03 section 6).

So these sites must reach a function that returns the yaw-only reference
pose, with slot 11's signature (`float* out` 3x4, `ret 4`, returns `out`).
Changing the displacement byte at site + 2 from 0x2C to the offset of an
extra vtable entry (for example 0x54, a 22nd slot after the game's 21) is one
way; any other redirection is equally valid. The game never calls the extra
entry itself.

The game's per-frame update containing the first site is replaced as a whole
by the mod (04, 05), so in practice only the reset-orientation site remains
live; patching both keeps the game consistent if the update is left alone.

### 6.5 Setting 0x18 "Stereo Mode" handler (manager slot 1)

The game's handler (RVA 0x5CCDB0), for an event whose id is 0x18 while a
device exists:

- if the new value is "Rift": calls device slot 1 (start sensor) and forces
  nine graphics settings (game's Rift preset, below);
- otherwise, if the device state is 2: calls device slot 2 (stop sensor).

Required replacement behaviour: for id 0x18 with a device present, call
device slot 1 regardless of the value (the "Rift" comparisons are forced
elsewhere, 6.6, so the stored value is irrelevant), and set these four
settings:

| Setting id | Name | Value |
|---|---|---|
| 0x00 | Depth of Field | "Off" |
| 0x15 | SSAO | "Off" |
| 0x0D | Motion Blur | "Off" |
| 0x07 | VSync | "On" |

Depth of field and motion blur are unpleasant in a headset; SSAO is a
screen-space effect that is wrong across a side-by-side target (it is also
disabled by a patch, 05). VSync "On" is kept from the reference behaviour;
its effect in stereo is not established (the device presents with sync
interval 0, 03 section 6).

The setter is `thiscall(settings, int id, const char* value)` `ret 8` at
RVA 0x5A8910 (2020: 0x4FAA50) on the settings object at RVA 0x1358A50 (2020:
0x1368C00). The value strings must be the game's own spellings shown above.

The game's own Rift preset, for reference: 0x00 Depth of Field "Off",
0x0B Shadow Map Resolution "512", 0x0C Shadow Mapping "High", 0x10 Planar
Reflections "Off", 0x15 SSAO "Standard", 0x16 Texture Filtering
"Anisotropic 2x", 0x0D Motion Blur "Off", 0x07 VSync "On", 0x12 Volumetric
Lighting "Off". Other known ids: 0x09 anti-aliasing, 0x18 Stereo Mode.

The event fires during startup once the stereo target is configured
(observed: "sensor started" follows "configure rendering" within
milliseconds).

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

They are listed here because they open the path; `05-game-patches.md` owns
them.

### 6.7 Per-frame update

The game's manager update, `thiscall(manager, float dt)` `ret 4` at RVA
0x5BE2A0, called from RVA 0x3B4100 (2020: 0x3C4FB0), does the LB+RB recenter
(6.4) and animates +0xAC. The mod replaces this call; its behaviour is
specified in 04 and 05.

## 7. Device state lifecycle

1. CreateDevice (6.1) succeeds: state 1. Manager +0x04 points at the device.
2. The game asks for the render-target size (6.2), creates the swapchain and
   the stereo target, and calls slot 3 (configure rendering).
3. Setting event 0x18 (6.5): slot 1, state 2. From now on the game renders in
   stereo every frame and calls slots 5, 7, 7, (11), 12..16, 8, 8, 6.
4. Slot 2 or slot 0 would return the state to 1; not observed in normal play.

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

The returned interface pointers are borrowed (no reference added). The stereo
render target's index is the word at +0x80 of the object the game passes to
device slot 3 (manager +0x08).
