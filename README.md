# OpenMotherVR

A rebuild of MotherVR 0.8.1, the unofficial VR mod for Alien: Isolation,
that runs on Windows 10 and 11 with SteamVR (OpenVR) or any OpenXR runtime.
The project has two parts:

- **The reconstruction** (`src/`): a `d3d11.dll` proxy reverse-engineered
  from the original, with fixes for its problems. This is what releases
  ship today.
- **The clean-room specification** (`cleanroom/`): a complete description
  of what the mod must do and how the game works, from which the mod is to
  be reimplemented from scratch in a separate repository, without the
  original's code.

| Path | Contents |
|---|---|
| `src/`, `res/`, `CMakeLists.txt`, `vcpkg.json`, `triplets/` | The reconstruction, in layers: `util/`, `platform/` (Windows, D3D11, OpenVR, OpenXR, audio), `game/` (AI.exe addresses, layouts, patching), `core/` (the mod's logic behind ports), `loader/` (DLL entry, wiring, hooks). See `BUILDING.md` to build |
| `tests/` | Unit tests of `util/` and `core/` (doctest, run with ctest) |
| `.clang-format`, `.clang-tidy` | Formatting and static-analysis rules of the style guide |
| `cleanroom/` | The clean-room process (`CLEANROOM.md`), the specification (`spec/`, v1 in 00 to 14, v2 in 20 to 24), the C++ style guide for the reimplementation and the spec lint |
| `dumps/` | Analysis notes on the original and the game. Start with `dumps/03_*`, `04_*`, `05_*.md`; `09_*` maps every original function to the reconstruction |
| `scripts/` | Ghidra headless scripts, Python tools (PAK2 archives, address tables, the UI patch, disassembly) and the build, packaging and in-game test scripts |
| `packaging/` | The player guide shipped with each release |
| `release-notes/` | One file per release tag |
| `.github/` | CI (build, DLL checks, repository and commit checks) and the release workflow |
| `archive/` | Not tracked: the original `dxgi.dll` and its Ghidra project, the original `MOTHER.PAK` (input for `scripts/ui_patch.py`) and a few raw analysis files. Only needed to re-check a detail or regenerate the UI patch |

## Tooling

- Ghidra 12.0.4 (headless) with the scripts in `scripts/`. The Ghidra project
  is not tracked; recreate it by importing MotherVR 0.8.1's `dxgi.dll`, running
  auto-analysis, then `FillGaps.java` and `BulkDecompile.java`.
- JPEXS FFDec for the Scaleform `.GFX` movies.
- Python 3 with `pefile` (and `capstone` for `scripts/disasm.py`).
- Visual Studio 2022/2026 (C++ workload), CMake and vcpkg for the DLL; see
  [BUILDING.md](BUILDING.md#building) for the build and
  [Making a release](BUILDING.md#making-a-release).

Large binaries (`*.dll`, `*.exe`, `*.pak`) are stored with Git LFS. The
embedded `res/openvr_api.dll` is pinned by SHA-256; configure stops on a
wrong file (run `pwsh scripts/fetch_openvr_api.ps1`).

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
- [x] Planar reflections per eye (verified in game)
- [x] Desktop mirror only filled a corner of the window
- [x] Render-target-view leak every frame; out-of-range reads in the input
      hooks
- [x] Stale Oculus input on focus loss: the Oculus backend is replaced by
      OpenXR, and both backends zero the controller state while the game
      lacks input focus (dashboard open)
- [x] Object pop-in: the game's per-object distance/LOD cull is wrong for
      the wide per-eye field of view; bypassed in stereo (verified in game)
- [x] Anti-aliasing modes are switched off in the stereo path: FXAA and
      SMAA T1x re-enabled, SMAA T2x runs as T1x (no per-eye history)
      (verified in game)
- [x] Short delay before the head-in-wall blackout, so walking through
      doors does not flash black (600 ms, verified in game)
- [x] Snap turn at 3 segments turned 180 degrees (integer division)
- [x] Controls only fit the Vive wand under OpenVR: SteamVR Input with
      default bindings for Index, Touch, WMR and Vive wands, rebindable in
      SteamVR (verified on Index)

From the first tester reports
- [x] OpenXR runtime discovery prefers the runtime made active in SteamVR
      or the vendor's app over a leftover 32-bit registration; failure
      messages name the error and say how to get back to SteamVR, and the
      options page shows the runtime actually running
- [ ] Warning when SteamVR's Desktop Game Theatre shows the game flat
      instead of in VR (implemented, not yet triggered in game)
- [x] An oversized desktop mirror window goes borderless over its monitor
      instead of running past the screen
- [x] Losing the desktop focus (SteamVR, a streaming client) no longer
      pauses the game or stalls loading screens
- [x] Desktop mirror modes crop each eye from its own half
- [x] Head pose held when tracking is lost; recenter waits for a valid pose
- [ ] Runtime None leaves the game unmodded apart from the options page
      (implemented, not yet run in game)
- [ ] Crash record in `mothervr.log` (module, offset, registers, callers)
      (implemented; the filter is in place, no crash recorded yet)
- [ ] Not yet understood: a crash on B in the pause menu, the old menu
      lingering over the Continue video, the view sliding on snap turns
      while walking (diagnostics added behind Dev. Debug Options)

### v2: enhanced mode

Specified in `cleanroom/spec/20` to `24`; every option defaults to v1
behaviour.

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
- A hotkey to disable VR (crash records are in v1)
- Per-controller button glyphs (Index, Quest, WMR) in the UI
- Discord Rich Presence: the area and what the player is doing (done;
  `DiscordRichPresence` in `mothervr.json`)

### Linux (Proton)

Not started. The mod stays a Windows DLL; the work is making it run inside
Proton/Wine with SteamVR for Linux or a Linux OpenXR runtime (Monado,
WiVRn, ALVR).

- [ ] Load under Proton next to DXVK: `WINEDLLOVERRIDES="d3d11=n,b"`,
      forward to Proton's own `d3d11.dll` (DXVK) instead of `system32`
- [ ] OpenVR through Proton's bridge to SteamVR for Linux (the embedded
      `openvr_api.dll` against Proton's `vrclient`)
- [ ] OpenXR through `wineopenxr`: runtime discovery without the Windows
      registry entries, D3D11 swapchain images over DXVK's Vulkan interop
- [ ] Headset audio redirect, crash record and desktop-focus fixes checked
      under Wine (`IMMDevice`, SEH filter, window messages)
- [ ] `scripts/run_game.ps1` counterpart for Linux and a Proton test pass
      on the 2015 and 2020 builds
- [ ] Player guide: Proton version, launch options, known issues
