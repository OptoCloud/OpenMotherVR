<!-- Provenance: process document, written by the spec side. Revision: r6 -->

# OpenMotherVR functional specification

What the mod must do, and the facts about Alien: Isolation it needs. Written
by the spec side under the rules in `../CLEANROOM.md`; the implementation
side builds from these documents only.

Target: Alien: Isolation, Steam, `AI.exe` 2015 build (PE timestamp
0x54E3199D, image base 0x400000). Addresses are RVAs unless stated. The 2020
build (PE timestamp 0x5FD25676) is listed where known. 2020 values were
taken from the earlier mod's per-build address table (and, for inline
sites, its installer) and have not been verified against a 2020 `AI.exe`;
an implementation verifies each one against the 2020 binary (the preflight
of 01) before enabling it, and `?` marks an unknown one.

Reading order: 00, 01, 02, 12, then the others as needed.

| Document | Covers |
|---|---|
| `00-overview.md` | Goals, scope of v1, what the player sees, which runtime runs, platform, the problems v1 fixes (including robustness and performance requirements), glossary |
| `01-loading.md` | The `d3d11.dll` proxy on Windows 11, what may happen at attach and in which order, patch sets (runtime None is the unmodded game), the preflight, build identification, runtime selection, the full address table with 2015 and 2020 RVAs |
| `02-game-stereo-path.md` | The game's dormant HMD rendering path: the stereo manager and device layouts, calling conventions, the game sites the mod changes or serves, device states, threads and display reset |
| `03-hmd-and-frames.md` | Head tracking, the recenter pose, field of view, render-target size and eye layout, frame timing on OpenVR, the virtual screen, the desktop mirror, the game's swapchain and window while a headset is attached, pixel scale |
| `04-input.md` | The game's input system and how VR controllers drive it; SteamVR Input and OpenXR actions with default bindings; button prompts; snap turn; look gating; recentering inputs; hand poses kept for v2 |
| `05-game-patches.md` | Changes to game behaviour: the Rift tests, the per-frame stereo update, UI quad placement and the stereo mode stack, menu wrappers, camera and cutscene tracking, byte patches, render-target rebuild on a settings change, skip intro |
| `06-options-and-config.md` | Settings, `mothervr.json`, the command line, the protocol between the game's UI and the mod, the options page and its behaviour, skip intro, the game's UI scripting interface |
| `07-reflections.md` | Planar reflections in stereo: how the game renders them, why they break, the per-eye render, diagnostics |
| `08-render-fixes.md` | Per-eye lighting (shader dither), object pop-in (culling), anti-aliasing in stereo, head-in-wall blackout delay, desktop mirror scale |
| `09-openxr.md` | The stereo device on OpenXR: 32-bit runtime discovery, session creation order, FOV without a session, frames and pacing, layers, spaces and recentering, failures |
| `10-audio.md` | Game audio to the headset's own audio device when the runtime names it (OpenXR), through the game's audio output |
| `11-diagnostics.md` | The log file `mothervr.log` and its format, the events that must be logged, rate rules, the debug diagnostics (`DebugOptions` in `mothervr.json`: pose age, hand poses, game events, reflection capture), logging of game script events, crash reporting |
| `12-threads-and-robustness.md` | The game's threads and which mod work runs on each, data crossing threads, verify-before-write for every patch, ABI rules for replacing game methods, per-frame budget, behaviour on every failure, display reset and process exit |
| `13-distribution.md` | What the player installs, the files the mod writes and when, the log as a file, message boxes, uninstall, the DLL's properties, the release package |
| `14-ui-resources.md` | The options menu's UI resources: authoring them from scratch, the developer build (AS3 compiled against declarations of the game's classes, glyph images to DDS), and linking them into the player's stock `UI.PAK` at install time to produce `MOTHER.PAK`, with validation, reuse and atomic replacement |
| `20-v2-overview.md` | v2 (enhanced mode): goals, the rule that every v2 option off is exactly v1, what the player sees, modes, phasing proposal, open questions across 21 to 24 |
| `21-hands-and-body.md` | v2: tracked hands and arms, aiming per weapon class, tracker and flashlight by hand, melee, throwing, body visibility, camera offset, room-scale, height and real crouch, body resync, wrist health, haptics |
| `22-controls-and-comfort.md` | v2: smooth and snap turning, comfort vignette, seated and standing, world scale, d-pad emulation, gestures and holsters, hiding, per-controller glyphs, OpenXR default, VR-off hotkey, crash-safe logging, the `EnhancedMode` switch |
| `23-interactions-and-hud.md` | v2: levers and valves by hand, camera takeovers, activation distances, terminal text size, crosshair, HUD and item highlight options, HUD placement |
| `24-rendering-v2.md` | v2: per-eye temporal AA, steady particle lighting, world behind the pause menu, engine settings overrides, foveated rendering, 2D-only effects, D3D11 state cost |
| `ANSWERS.md` | Answers to the implementation side's questions (`spec/QUESTIONS.md` in its clone), with the sections changed |

Every document starts with a provenance header:

```
<!-- Provenance: <what it was derived from>. Revision: rN -->
```

Run `python -I tools/spec_lint.py spec` before committing a spec change.
