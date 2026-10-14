# MotherVR revived

Reverse engineering of MotherVR 0.8.1, the unofficial VR mod for
Alien: Isolation, and a C++ reconstruction of it that runs on Windows 11.

| Path | Contents |
|---|---|
| `src/`, `res/`, `CMakeLists.txt` | The reconstruction: a `d3d11.dll` proxy. See `BUILDING.md` to build |
| `dumps/` | Analysis notes. Start with `dumps/03_*`, `04_*`, `05_*.md`; `09_*` maps every original function to the reconstruction |
| `scripts/` | Ghidra headless scripts and Python tools (PAK2 archives, address tables) |
| `archive/` | Not tracked: the original `dxgi.dll`, the raw Ghidra decompiles and metadata (`archive/dumps/`), the decompiled UI ActionScript and the Ghidra projects. Everything MotherVR does is ported, so these are only needed to re-check a detail |

## Tooling

- Ghidra 12.0.4 (headless) with the scripts in `scripts/`. The Ghidra project
  itself is not tracked; recreate it by importing MotherVR 0.8.1's `dxgi.dll`, running
  auto-analysis, then `FillGaps.java` and `BulkDecompile.java`.
- JPEXS FFDec for the Scaleform `.GFX` movies.
- Python 3 with `pefile` for `scripts/*.py`.
- Visual Studio 2022/2026 (C++ workload), CMake and vcpkg for the DLL; see
  [BUILDING.md](BUILDING.md#building) for the build.

Large binaries (`*.dll`, `*.pak`) are stored with Git LFS.

## Roadmap

### v1: MotherVR 0.8.1, revived

Everything the original does, working on Windows 11 and current runtimes,
plus fixes for the original's own problems. No new features.

Reconstruction
- [x] d3d11 proxy that loads on Windows 11, AI.exe build detection, address tables
- [x] MotherVR options menu (`MOTHER.PAK`), console commands, settings push
- [x] Correct `__thiscall` ABI for every hooked AI.exe method
- [x] Stereo device handed to the game's hidden LibOVR path; OpenVR backend
      (head tracking, side-by-side submit, cinema screen for menus)
- [x] VR input through the game's gamepad (action-map virtualisation)
- [x] Remaining installer patches: update hook (snap turn, recenter,
      blackout), menu screen and HUD placement, mode stack, menu wrappers,
      camera and UI byte patches
- [x] Verify in game: head tracking, stereo, menus and HUD, loading screens
- [x] OpenXR backend behind the same stereo device (Quest Link, Pico, WMR,
      SteamVR's OpenXR); selectable in the menu (the original's "Oculus"
      entry, relabelled) (verified on SteamVR OpenXR)

Fixes to original issues
- [x] Gamepad dead under the mod (stubbed hooks); controller prompts
- [x] Eyes at half resolution: the game never rebuilt its targets after
      stereo started
- [x] Floors and walls lit differently in the right eye, worst at high
      resolution (looked like reflections moving against the head): the
      game's position-based shader dither, patched in the bytecode
- [ ] Planar reflections per eye: implemented, not yet seen in game (the
      tested scene had no planar reflection surface)
- [x] Desktop mirror only filled a corner of the window
- [x] Render-target-view leak every frame; out-of-range reads in the input
      hooks
- [ ] Stale Oculus input on focus loss (with the Oculus backend)
- [ ] Object pop-in: the game's per-object distance/LOD cull is wrong for
      the wide per-eye field of view
- [ ] Anti-aliasing modes are switched off in the stereo path: FXAA and
      SMAA T1x re-enabled, SMAA T2x runs as T1x (no per-eye history);
      implemented, not yet seen in game
- [ ] Short delay before the head-in-wall blackout, so walking through
      doors does not flash black
- [x] Controls only fit the Vive wand under OpenVR: SteamVR Input with
      default bindings for Index, Touch, WMR and Vive wands, rebindable in
      SteamVR (verified on Index)

### v2: enhanced mode

Hands
- Free 6DOF hand controllers: tracked hands in game, aiming with the hands
- Motion tracker and flashlight held and aimed with the hands
- Gesture shortcuts: over the shoulder for weapons, hip for the tracker,
  chest for the flashlight
- Haptics: Alien proximity, tracker pings, hacking feedback
- Physical interaction: levers, valves, the rewiring tool

Comfort
- Smooth turning with adjustable speed, alongside snap rotation
- Comfort vignette during stick locomotion
- Seated and standing modes with height calibration; real crouch drives the
  in-game crouch
- World scale slider

Rendering
- OpenXR as the default runtime (works since v1; default still OpenVR)
- Fixed foveated / variable rate shading and a resolution scale
- Fix or toggle 2D-only effects: HUD placement, lens flare, chromatic
  aberration, film grain, cutscene letterboxing

Quality of life
- In-headset settings for the new options in the MotherVR menu
- Test and support the 2020 build (current Steam, GOG, Epic)
- Crash-safe logging and a hotkey to disable VR
- Per-controller button glyphs (Index, Quest, WMR) in the UI
