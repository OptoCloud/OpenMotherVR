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
- Unit tests of `src/util` and `src/core` are in `tests/` (doctest; target
  `omvr_tests`, built by default, never linked into the DLL):
  `ctest --test-dir build -C Release --output-on-failure` after a build. CI
  runs them after its build. Core logic changes come with tests; expected
  values come from the behaviour before the change or the spec, not from
  the new code.
- `.clang-format` and `.clang-tidy` hold the style guide's formatting and
  analysis rules: `clang-format -i <file>` on what you touch;
  `clang-tidy -p <ninja build dir> <file.cpp>` for analysis.

## Source layout

The code follows `cleanroom/STYLEGUIDE.md` (namespace `omvr`), in layers
that each depend only on the ones before them:

- `src/util/`: standard library only (maths, sync, hashing, binary, text,
  `Error`/`Result`).
- `src/platform/{win32,d3d11,openvr,openxr,audio}/`: one external API each,
  wrapped in project types. Platform headers (`<windows.h>`, D3D, OpenVR,
  OpenXR) only in their `.cpp` files.
- `src/game/`: everything about `AI.exe`: addresses, layouts, facts
  (`constants.h`), x86 encodings (`x86.h`), verified patching, `sites/`,
  and thin accessors over game objects (`stereo/`, `input/`, `ui/`,
  `audio/`).
- `src/core/{stereo,render,vr,input,config,ui,diag}/`: the mod's logic over
  value structs. It reaches the outside only through `ports/` (abstract
  interfaces) handed in at construction; each area's constants are in its
  `tuning.h`. Core may include `game/constants.h` and nothing else of the
  game (exception: `core/stereo/stereo_device`, which works on the game's
  `StereoInterface` in place); `platform/` and `game/` may include only
  `core/**/ports/*.h`, `core/**/types.h` and `core/**/tuning.h`. The two
  VR backends (`platform/openvr`, `platform/openxr`) may also include the
  backend-neutral rules both apply, `core/vr/*.h` and
  `core/input/controller_*.h`: pure functions and self-contained classes,
  so each rule is written and tested once. A rule a backend would
  otherwise write for itself goes there, not in `platform/`. One exception
  is left: the backends take the player's runtime warnings from
  `core/ui/notices.h`, until those go through the `Dialogs` port.
- `src/loader/`: the composition root. `Context` (`loader/context.h`)
  builds and wires every long-lived object once at attach; `hooks/` holds
  every function the game, COM or the OS calls, each thin: read game
  memory into values, call one `context()` member, write the result back.
- No globals or singletons outside `loader/` (the few the patched game
  code reads at a fixed address are commented where they are).

Each source directory lists its files in its own `sources.cmake`. util and
core build as the static libraries `omvr_util` and `omvr_core`; the DLL
links both.

## Definition of done

- The build is clean with `MOTHERVR_WERROR=ON`.
- If the change touches anything that runs in the game, it was run with
  `scripts/run_game.ps1` and the log checked for the expected lines; report
  what could not be checked (a headset that was not worn, a level that was
  not loaded, the 2020 game build).
- Do not claim a fix works in VR from a build alone.
- A change that cannot be checked in game now adds its check to
  `IN_GAME_CHECKS.md` (what to do, what should happen, the log line).

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

- Every `AI.exe` address goes in a table: `src/game/ai_address_tables.inc`
  (generated, from the original's table) or `src/game/addresses.h`,
  with the 2015 RVA and the 2020 RVA, or 0 when the 2020 one is unknown so
  the feature stays off there. No address literals in feature code. The
  x86 encodings the patches check and write are named in `src/game/x86.h`,
  and every write goes through `game::Patcher` and
  `platform::win32::write_memory`.
- Verify before writing: a call site with `calls(site, target)`, code bytes
  in the installer's preflight, a function entry through `game::detour` with
  its expected prologue bytes. If anything differs, patch nothing and log it.
- A replacement for a `__thiscall` method is
  `__fastcall(void* self, void* edx, ...)` with the same stack arguments and
  cleanup. Get this wrong and the game crashes at the first call. Every
  replacement lives in `src/loader/hooks/`, is `noexcept` and has a
  `static_assert` that its type converts to the game's.
- `StereoManager`, `StereoInterface` and `HmdDesc` have the game's memory
  layout; their `static_assert`s on size and offsets must keep holding.
- Threads: the game thread reads head poses, the render thread runs
  begin/end frame and the runtime, and job threads run the cull hooks.
  Data crossing threads goes through atomics or `util::SeqLock`.
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
- A file whose first lines carry `OpenMotherVR-Provenance: clean` holds
  nothing derived from MotherVR or any other mod (game facts and public APIs
  are allowed); remove the line when such a file gains mod-derived content.
  List them with `rg -l "OpenMotherVR-Provenance: clean"`.
- Commit messages follow Conventional Commits 1.0.0:
  `type(scope): imperative summary`, at most 72 characters, no trailing
  period. Types: `feat`, `fix`, `perf`, `refactor`, `docs`, `build`, `ci`,
  `test`, `chore`, `revert`. Scope is the module or area: `hook`, `stereo`,
  `runtime`, `input`, `proxy`, `config`, `render`, `integration`, `game`,
  `audio`, `ui`, `scripts`, `util`, `core`, `platform`, `loader`, `tests`, `diag`, `cleanroom`
  (CI checks this list: `scripts/check_repo.sh`). The body says what changed and why;
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
