<!-- Provenance: spec side's answers to the implementation side's QUESTIONS.md r1 (questions on spec r2); user decisions for spec r3; AI.exe 2015 analysis (mode-stack container); DXGI public API semantics. Revision: r1 -->

# Answers to the implementation side's questions (spec r3)

Each answer names the documents and sections changed in spec r3. Where the
answer is a decision rather than a game fact, the reason is given.

## Q1. `-vrscale`: world scale or pixel scale?

**World scale.** `-vrscale <f>` sets the world (metres) scale `s` for one
run, accepted in [0.495, 1.505], otherwise 1.0. The pixel scale is only the
config / menu setting (0.5 to 4.0) and has no command-line override. Your
implementation is correct.

Changed: `06-options-and-config.md` §1 (runtime values paragraph: world
scale is not a setting) and §2 (command-line table); `03-hmd-and-frames.md`
parameter table (sources of `s` and `p`).

## Q2. Runtime values without a backend ("Oculus", "OpenXR")

**The runtime values are None, OpenVR and OpenXR, and v1 has both
backends.** OpenXR is now specified (`09-openxr.md`). Value 1, the
original's "Oculus" slot, now means OpenXR: the menu entry is relabelled
"OpenXR", and a config file saying `"Oculus"`, `VRRuntime` 1 or
`-vrforceoculus` all select OpenXR (Meta's PC runtime is an OpenXR runtime,
and v1 has no LibOVR backend). The mod writes `"OpenXR"`. There is no
value 3. The one-time "choose SteamVR" message is not needed.

Changed: `06-options-and-config.md` §1, §2 (`-vrforceopenxr` added,
`-vrforceoculus` = OpenXR), §3.2, §3.3, §4.2 (`mvr_vrruntime 1`), §4.3
(`setMotherSetVRRuntime`), §5.1 (labels), §7.7; `00-overview.md` §2.5, §3;
new `09-openxr.md`.

## Q3. Mirror "both eyes" with symmetric off: crops of different height

**Both crops use the union of the two vertical ranges; each keeps its own
horizontal range.** `v0 = min(vMin_0, vMin_1)`, `v1 = max(vMax_0, vMax_1)`;
`a` uses the summed widths over `v1 - v0`; eye 0 left, eye 1 right, each
half as wide as its crop's share. Why: a common vertical range keeps the
same rows of the target at the same height in both halves and stretches
neither crop; a mean height would stretch one of them. On current headsets
(mirror-image eyes) the ranges are equal, so this only matters for unusual
hardware. Your implementation differs only in that case.

Changed: `03-hmd-and-frames.md` §8.2.

## Q4. Fullscreen after swap-chain creation

**Yes, block it while the stereo device exists.** The swapchain must stay
windowed while a headset is attached, and a later switch to fullscreen is
refused: `IDXGISwapChain::SetFullscreenState(TRUE, ...)` returns
`DXGI_ERROR_NOT_CURRENTLY_AVAILABLE` without changing state, `FALSE` passes
through, DXGI's Alt+Enter is disabled with `MakeWindowAssociation(...,
DXGI_MWA_NO_ALT_ENTER)`, and `ResizeTarget` passes through (windowed, it
only resizes the window, which the mirror follows). Without a stereo device
none of this applies. Why: the side-by-side back buffer is larger than any
display mode, so a fullscreen transition cannot succeed and would leave the
swapchain in a mode the mirror cannot use; the refusal code is the one DXGI
documents for an impossible transition. How the game's video menu reacts to
the refusal is still to be checked in game.

Changed: `03-hmd-and-frames.md` §4.5.

## Q5. Raising the mode stack count with no stack storage

**Raise it only through the game's own append routine.** Game fact: the
stack is a growable int array `{+0x68 data, +0x6C count, +0x70 capacity}`
that starts empty; the game grows it only in its append routine at RVA
0x294540 (thiscall, ECX = stereo +0x68, one argument `const int*`, `ret 4`,
returns the new element's address; 8 entries on first use, then doubling),
called from PushMode at 0x5D9C53. So: lower the count by writing it;
raise it by appending the value 1 through that routine. Your fallback
(raise only when data is non-null and capacity allows, else log) is the
required behaviour where the routine's address is unknown (2020 build).

Changed: `05-game-patches.md` §3.2f; `01-loading.md` §4.5
(`StereoModeStackAppend`).

## Q6. Patches when the VR runtime is None

**With runtime None the game is the unmodded game.** No stereo device is
created and no game patch is applied, except a small UI set that lets the
player turn VR on: the UI archive redirect, the UI callback slot, the
settings push in the front-end and pause menu Open wrappers (without the
stereo-mode handling), and skip intro when it is on. No Rift tests, pad,
camera, SSAO, mode-stack, UI-quad, render-path or shader patches; no D3D11
wrapping beyond the pure forwarders. The patch set is decided at attach from
the config file and the command line, which are both known then. When the
runtime is OpenVR or OpenXR but fails to start, all patches are already in
place and the game runs flat on the stereo path's no-device behaviour (02);
the player can choose None for the unmodded game.

Changed: `01-loading.md` §2.2 and new §2.3 (patch sets; the old §2.3 is now
§2.4); `00-overview.md` §2.7; `06-options-and-config.md` §1, §2, §4.3.

## Other changes in spec r3

| Document | Change |
|---|---|
| `04-input.md` r2 | §5.8 new: the runtime input layer (semantic inputs; SteamVR Input action manifest and default bindings for `knuckles`, `oculus_touch`, `holographic_controller`, `vive_controller`; legacy fallback without `IVRInput`; OpenXR actions and suggested bindings per interaction profile; semantic inputs to game actions). §4.1 glyph table gains a WMR column; §5.3 pause on Index; §5.4 WMR mapping settled; §5.5 scheme choice per runtime; §8 runtime recenter request |
| `06-options-and-config.md` r2 | §7 rewritten: patching the player's `UI.PAK` (requirement, PAK2 format including sorted names, which archive loads, per-entry size and CRC-32 checks of the Steam 2015 movies, behaviour when the archive differs, the game's UI scripting interface, required movie changes, glyph images); the implementation authors its own movie changes; §4.1 the movie's call prefixes |
| `09-openxr.md` r1 | new: the stereo device on OpenXR, verified on SteamVR with Index |
| `01-loading.md` r2 | §4.7 lists every game address 07 and 08 use, with checked bytes for the sites |
| `00-overview.md` r3 | runtimes, controllers, VR-not-available behaviour |
| `README.md` r3 | index: 09 and this file |
