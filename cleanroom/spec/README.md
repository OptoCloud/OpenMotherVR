<!-- Provenance: process document, written by the spec side. Revision: r3 -->

# OpenMotherVR functional specification

What the mod must do, and the facts about Alien: Isolation it needs. Written
by the spec side under the rules in `../CLEANROOM.md`; the implementation
side builds from these documents only.

Target: Alien: Isolation, Steam, `AI.exe` 2015 build (PE timestamp
0x54E3199D, image base 0x400000). Addresses are RVAs unless stated. The 2020
build is listed where known.

| Document | Covers |
|---|---|
| `00-overview.md` | Goals, scope of v1, user-visible behaviour, runtimes, glossary |
| `01-loading.md` | How the mod gets into the process on Windows 11, patch sets (runtime None is the unmodded game), build detection, per-build addresses |
| `02-game-stereo-path.md` | The game's dormant HMD rendering path and the objects the mod must provide to it |
| `03-hmd-and-frames.md` | Head tracking, projection, render-target sizing, frame submission, the virtual screen, the desktop mirror, keeping the swapchain windowed (OpenVR) |
| `04-input.md` | The game's input system and how VR controllers drive it; SteamVR Input and OpenXR actions with default bindings; glyphs; snap turn; recentering |
| `05-game-patches.md` | Changes to game behaviour: camera, HUD and menu placement, stereo mode stack, settings, loading |
| `06-options-and-config.md` | Settings, config file, command line, the options menu and its protocol with the mod, patching the player's `UI.PAK`, the game's UI scripting interface |
| `07-reflections.md` | Planar reflections in stereo |
| `08-render-fixes.md` | Per-eye lighting (shader dither), object pop-in, anti-aliasing in stereo, head-in-wall blackout delay, desktop mirror scale |
| `09-openxr.md` | The stereo device on OpenXR: 32-bit runtime discovery, session creation order, FOV without a session, layers, frame pacing, spaces and recentering |
| `ANSWERS.md` | Answers to the implementation side's questions (`spec/QUESTIONS.md` in its clone), with the sections changed |

Every document starts with a provenance header:

```
<!-- Provenance: <what it was derived from>. Revision: rN -->
```

Run `python -I tools/spec_lint.py spec` before committing a spec change.
