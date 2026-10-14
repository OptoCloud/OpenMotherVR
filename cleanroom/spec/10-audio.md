<!-- Provenance: game analysis of AI.exe 2015 (the audio output reopen request function, the audio thread's device-recreate check, the game's default-device-change listener; imports SetEvent, CoInitializeEx, WaitForSingleObject); the reopen function was located from the earlier mod's use of the game's audio reopen, and its behaviour (2.2) and the listener (2.3) were then verified in AI.exe; public Windows Core Audio (MMDevice) and OpenXR XR_OCULUS_audio_device_guid documentation; user requirement (game audio on the headset when the runtime names its device); spec-side decisions with their reasons (installed once per process, pass-through when the headset device is unavailable, the player told once on failure). 2020 values were taken from the earlier mod's per-build address table (and, for inline sites, its installer) and have not been verified against a 2020 `AI.exe`. Revision: r2 -->

# 10 Audio on the headset

The game's sound must come out of the headset's own headphones when the VR
runtime says which audio device belongs to the headset, even when Windows'
default playback device is something else (desk speakers, a monitor). The
game itself is not told about the headset: it keeps opening "its" output
device, and the mod makes that open land on the headset's device.

Addresses are RVAs (image base 0x400000). 2015 values are verified against
the Steam 2015 `AI.exe` (PE timestamp 0x54E3199D). 2020 values were taken
from the earlier mod's per-build address table (and, for inline sites, its
installer) and have not been verified against a 2020 `AI.exe`.

## 1. When it applies

| Runtime | Headset audio device known | Behaviour |
|---|---|---|
| OpenXR, runtime offers `XR_OCULUS_audio_device_guid` and `xrGetAudioOutputDeviceGuidOculus` succeeds with a non-empty ID | yes | redirect (section 3) |
| OpenXR, extension not offered (for example SteamVR's OpenXR runtime [likely]), the function cannot be fetched, the call fails, or the ID is empty | no | nothing; the game uses the Windows default device |
| OpenVR (SteamVR) | no | nothing. SteamVR can itself switch Windows' default playback device to the headset when it starts (a SteamVR setting); the game follows default-device changes on its own (2.3) |
| None, or the runtime failed to start | no | nothing |

The ID comes from the runtime, never from the player or the config file.
There is no setting for this feature.

### 1.1 Getting the ID (OpenXR)

When creating the OpenXR instance (09), `XR_OCULUS_audio_device_guid` is
enabled **if and only if** the runtime lists it in
`xrEnumerateInstanceExtensionProperties`; instance creation never fails
because it is missing. The ID is read through
`xrGetAudioOutputDeviceGuidOculus` (fetched with `xrGetInstanceProcAddr`)
into a zero-filled `wchar_t[XR_MAX_AUDIO_DEVICE_STR_SIZE_OCULUS]` (128)
buffer. On success the buffer holds an MMDevice endpoint ID string (the
form `IMMDeviceEnumerator::GetDevice` takes, for example
`{0.0.0.00000000}.{guid}`). The function needs only the instance, so it may
be called at any time after `xrCreateInstance` succeeds; the ID is used
only once the OpenXR stereo device has started (3.1).

## 2. Game facts

### 2.1 The audio engine and its thread

The game's audio engine runs its own thread. That thread calls
`CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)` and then waits on an event
(`WaitForSingleObject`), periodically and when signalled. Each time it runs
it checks its output device; when a reopen has been requested (2.2), or
the device reports it is no longer usable, it clears the request and
creates its output again. Creating the output opens the default render
endpoint through MMDevice and calls `IMMDevice::Activate` on it [sure for
the request flag and the recreate path; likely for the exact endpoint role
used, see Open questions].

### 2.2 The reopen request function

| | 2015 | 2020 |
|---|---|---|
| RVA | **0x831B00** | 0x8473D0 |
| ABI | `void __cdecl ReopenAudioOutput(void)`: no arguments, plain `ret`, no return value | same (from the earlier mod's table) |

What it does (2015, verified):

- the byte at RVA 0x17F9761 (the "reopen requested" flag) is set to 1;
- if the audio engine is initialised (getter at RVA 0x82B330, which returns
  the byte at RVA 0x17F96EC in `al`), the audio thread's event is signalled:
  `SetEvent` on the handle at `engine + 0x88`, where the engine object
  pointer is at RVA 0x17F9678 (through a small helper at RVA 0x85DC40 that
  skips a null handle);
- otherwise nothing else happens.

Entry bytes (2015): `C6 05 <abs32> 01` (`mov byte [flag], 1`, where
`abs32` is image base + 0x17F9761) then `E8 <rel32>` (the getter call).

So it is safe to call from any thread at any time after `AI.exe`'s static
data is initialised: before the audio engine exists it only leaves the
request set (the engine's first open goes through the redirect anyway);
afterwards it wakes the audio thread, which reopens its output on its own
thread. It does not wait for the reopen.

### 2.3 Default-device changes

The game registers a device-change listener (an
`IMMNotificationClient`). Its `OnDefaultDeviceChanged(flow, role, id)`
(2015 RVA 0x831E10; `__stdcall` with `this`, `ret 0x10`) does the same as
2.2 when `flow` is `eRender` (0) and `role` is `eMultimedia` (1): sets the
reopen flag and signals the audio thread. So the game already follows the
Windows default device; the redirect only matters when the headset is not
the default.

### 2.4 Windows facts used

- `IMMDevice::Activate` is slot 3 of the `IMMDevice` vtable:
  `HRESULT __stdcall Activate(IMMDevice* self, REFIID iid, DWORD clsctx,
  PROPVARIANT* params, void** out)`.
- That vtable is shared by every `IMMDevice` object in the process (it
  lives in the system's MMDevice implementation, so any `IMMDevice` reaches
  it), and it is in read-only memory. Replacing slot 3 there affects every
  device opened later, from any thread.
- COM must be initialised on a thread before that thread creates any
  MMDevice object. Either apartment model works for MMDevice.

## 3. Required behaviour

### 3.1 When

When the OpenXR stereo device has started (its instance was created and
the headset system was found, 09) and the ID of 1.1 is not empty. The
redirect is installed at most once per process: the stereo device starts
once per process, and a second installation would gain nothing. It is
never installed in `DllMain` (COM initialisation under the loader lock is
unsafe; 01 §2.2).

### 3.2 Installing the redirect

After installation:

- slot 3 of the shared `IMMDevice` vtable (2.4) holds the replacement of
  3.3, and the original slot value has been kept for the replacement to
  call; the vtable's memory protection is as it was;
- so every `IMMDevice` the game creates later goes through the
  replacement;
- the installing code holds no COM reference: every interface it obtained
  to reach the vtable has been released;
- COM, if the installing thread had to initialise it, stays initialised
  for the life of the process, because the replacement outlives the
  installation.

If installation fails (no `IMMDevice` could be obtained, or the slot could
not be written), nothing is changed, VR and the game's audio continue
normally on the default device, the failure is logged, and the player is
told once (13 §5.3: game audio could not be moved to the headset and stays
on the Windows default device, with the error code).

On success the event is logged, then the game's reopen function (2.2) is
called, so that a device the game already opened is reopened through the
redirect. The function is called only if its address is known for the
running build and its entry bytes match (`C6 05` at +0, `01` at +6, `E8` at
+7); otherwise it is not called, the redirect applies from the game's next
reopen (for example the next default-device change), and that is logged.

### 3.3 The replacement `Activate`

Required result, for every call, on any thread:

- If the headset's endpoint (the ID of 1.1) can be obtained, the call
  returns what the original `Activate` returns for the headset's endpoint
  with the caller's `iid`, `clsctx`, `params` and `out`: the caller gets
  the interface it asked for, on the headset's device.
- If the headset's endpoint cannot be obtained (unplugged, disabled, or
  COM not usable on the calling thread), the call behaves exactly as the
  unmodified `Activate` on the caller's own device.
- No COM reference obtained during a call outlives the call, other than the
  interface returned to the caller (acceptance 6).
- Nothing is logged inside the replacement: it can run on the audio thread
  at any time (11).

Whether the headset's endpoint is looked up on each call or cached is the
implementation's choice, as long as an endpoint that disappears is
handled as above.

The redirect applies to every `Activate` in the process (any interface:
`IAudioClient`, endpoint volume, ...), whatever device and role the game
chose. That is intended: the game plays all its sound through one output.

### 3.4 What is not done

- No change to Windows' default device or to any system setting.
- No redirect of capture (microphone) devices beyond what 3.3 implies: the
  game does not use capture [likely].
- The redirect is not removed when the stereo device shuts down; it lasts
  for the process.

## 4. Logging

| Event | What the entry conveys |
|---|---|
| install succeeded | that game audio now goes to the headset's endpoint, with its ID |
| install failed | that the redirect could not be installed, with the `HRESULT` |
| reopen function not called (address unknown or bytes differ) | that the game's output will switch at its next reopen |

The levels and message templates of these events are in 11 §2.5.
No per-frame or per-`Activate` entries (11: nothing is logged per frame or
from the audio thread).

## 5. Acceptance

1. Meta Quest over Link with Meta's OpenXR runtime, runtime OpenXR,
   Windows default playback device set to desk speakers: the game's sound
   plays in the headset; the log has the redirect event once.
2. Same, with the headset's audio already the Windows default: no change
   audible, the redirect event is still logged, no message.
3. SteamVR (OpenVR, or SteamVR as the OpenXR runtime): no redirect event;
   the game plays on the Windows default device (and follows SteamVR if it
   switches the default).
4. Runtime None: no redirect event.
5. Start in VR on Meta's runtime, then disable the headset's audio
   endpoint in Windows sound settings while playing: the game's sound
   continues on the device it asked for after its next reopen (3.3), no
   crash.
6. Play a long session: the process's handle and memory use do not grow
   with audio reopens.

## 6. Open questions

- Which role (`eConsole` or `eMultimedia`) the audio thread asks
  `GetDefaultAudioEndpoint` for when it recreates its output. It does not
  change the requirement (the redirect replaces any device); trace the
  recreate path from RVA 0x831BE8 (2015) into the call at RVA 0x85E390.
- The 2020 address of the reopen function (0x8473D0) is from the earlier
  mod's table only. Confirm on a 2020 binary by the 2015 entry pattern
  (`C6 05 <abs32> 01 E8`).
- Whether any current OpenXR runtime other than Meta's offers
  `XR_OCULUS_audio_device_guid` (Pico, Virtual Desktop's VDXR). Check each
  runtime's extension list; the requirement is unchanged either way.
- Whether SteamVR exposes the headset's playback device to OpenVR
  applications. If it does, OpenVR could feed the same redirect; v1 does
  not.
