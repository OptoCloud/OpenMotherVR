# AGENTS.md

A Windows-only VR mod for Alien: Isolation: `d3d11.dll`, a 32-bit proxy DLL
that the game loads from its install folder, which patches `AI.exe` in
memory to drive the game's hidden stereo renderer and submit to SteamVR
(OpenVR) or OpenXR. It reimplements MotherVR 0.8.1 and fixes its problems
(v1), then adds enhancements (v2); see the roadmap in `README.md`.

## Commands

Windows, Visual Studio 2022/2026 with the C++ workload, `VCPKG_ROOT` set.
Always a 32-bit (Win32) build: the game is 32-bit.

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32 `
      -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DMOTHERVR_WERROR=ON
cmake --build build --config Release        # -> build/Release/d3d11.dll
```

- CI builds with Ninja in a `vcvars32.bat` environment and `MOTHERVR_WERROR=ON`;
  a warning fails it.
- CI also fails on a commit subject that is not Conventional Commits, on a
  tracked game file (`scripts/check_repo.sh`), and on a built DLL with the
  wrong exports, imports or version (`scripts/check_dll.ps1`). Each build
  keeps its `d3d11.pdb` as a separate artifact.
- Releases: push a `v*` tag on `master` with `release-notes/<tag>.md`
  committed; see "Making a release" in `BUILDING.md`.
- Configure stops if `res/openvr_api.dll` is not the pinned file (its
  SHA-256 is in `CMakeLists.txt`; a Git LFS pointer fails too): run
  `pwsh scripts/fetch_openvr_api.ps1`.
- In-game check (needs the game installed through Steam):
  `pwsh scripts/run_game.ps1 -Seconds 60` deploys the build, starts the game,
  prints the new `mothervr.log` lines and disassembles any crash (the log
  also has a `crash:` record with module, offset and callers). `-Keep`
  leaves the game running.
- There are no unit tests.

## Definition of done

- The build is clean with `MOTHERVR_WERROR=ON`.
- If the change touches anything that runs in the game, it was run with
  `scripts/run_game.ps1` and the log checked for the expected lines; report
  what could not be checked (a headset that was not worn, a level that was
  not loaded, the 2020 game build).
- Do not claim a fix works in VR from a build alone.

## Testing with a person in the loop

- Launching the game takes over the user's headset and screen. Ask before
  launching, and when the test needs the headset worn or controllers held,
  run long enough for that (several minutes, or `-Keep`), then read the log.
- With "Dev. Debug Options" on (`DebugOptions` in `mothervr.json`) the mod
  logs extra diagnostics (pose age, hand poses, game events, stereo mode
  and frame-kind changes, the camera after each snap turn). Read the log
  instead of asking the user to describe what they saw.
- Reports from other testers come with their `mothervr.log`; ask for it
  with Dev. Debug Options on.

## Rules for patching the game

- Every `AI.exe` address goes in a table: `src/hook/ai_address_tables.inc`
  (generated, from the original's table) or `src/hook/recon_addresses.h`,
  with the 2015 RVA and the 2020 RVA, or 0 when the 2020 one is unknown so
  the feature stays off there. No address literals in feature code.
- Verify before writing: a call site with `calls(site, target)`, code bytes
  in the installer's preflight, a function entry through `hook::detour` with
  its expected prologue bytes. If anything differs, patch nothing and log it.
- A replacement for a `__thiscall` method is
  `__fastcall(void* self, void* edx, ...)` with the same stack arguments and
  cleanup. Get this wrong and the game crashes at the first call.
- `StereoManager`, `StereoInterface` and `HmdDesc` have the game's memory
  layout; their `static_assert`s on size and offsets must keep holding.
- Threads: the game thread reads head poses, the render thread runs
  begin/end frame and the runtime, and job threads run the cull hooks.
  Data crossing threads goes through atomics or `runtime::SeqLock`.
- No allocation, file I/O or logging per frame on the render thread; log
  once per change, or every 900 frames behind `DebugOptions`.
- Never link `d3d11.lib`: this DLL is `d3d11.dll` and would import itself.
  Use D3D11 through interfaces only.
- Runtime None applies only the options-page (UI) patch set: no stereo,
  camera, shader or window changes and no D3D11 wrapping.

## Content rules

- Never commit game files (`UI.PAK`, `MOTHER.PAK`, `AI.exe`, shader caches)
  or other mods' files. Changes to game assets ship as deltas applied to the
  player's own copy (`res/ui_patch.bin`, built by `scripts/ui_patch.py`).
- `archive/` is local reference material and is never committed, and so
  is `cleanroom-review/` (review reports that cite the original).
- `mothervr.json` keeps the original mod's keys and layout; new settings are
  added, unknown keys are preserved on save.
- Comments explain why. Keep `// From: FUN_xxxxxxxx` lines: they map code to
  the original's functions.
- Commit messages follow Conventional Commits 1.0.0:
  `type(scope): imperative summary`, at most 72 characters, no trailing
  period. Types: `feat`, `fix`, `perf`, `refactor`, `docs`, `build`, `ci`,
  `test`, `chore`, `revert`. Scope is the module or area: `hook`, `stereo`,
  `runtime`, `input`, `proxy`, `config`, `render`, `integration`, `game`,
  `audio`, `ui`, `scripts`. The body says what changed and why;
  a breaking change (for example to `mothervr.json` or the install layout)
  gets `!` after the scope and a `BREAKING CHANGE:` footer. One logical
  change per commit. No AI-attribution trailers or footers.

## Where things are explained

- `BUILDING.md`: build, install, CI, regenerating the UI patch.
- `dumps/NN_*.md`: analysis of the original and of the game (stereo
  interface 06a, HMD backends 06b, detours and patches 06c, VR input 06d,
  reflections 07, coverage map 09, code and performance reviews 11 and 12).
  Read the relevant one before changing that subsystem.
- `packaging/README.txt`: the player-facing guide; update it when a setting
  or control changes.
- `cleanroom/`: the clean-room specification and its process. Spec edits
  follow `cleanroom/CLEANROOM.md` and pass
  `python -I cleanroom/tools/spec_lint.py cleanroom/spec` before a commit.
- `release-notes/<tag>.md`: what each release changes, for players.
