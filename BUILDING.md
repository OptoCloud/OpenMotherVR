# MotherVR reconstruction

Reverse-engineered C++ source for MotherVR 0.8.1, the unofficial VR mod for
Alien: Isolation. The original ships as a `dxgi.dll` proxy; Windows 11 will
not load a local `dxgi.dll`, so this rebuild proxies `d3d11.dll` instead. The
mod logic is otherwise the same.

Reconstruction is in progress. The analysis behind it is in `dumps/`
(`03_*` third-party inventory, `04_*` entry and integration surface, `05_*`
embedded resources and UI). The raw Ghidra decompiles of the original are not
tracked any more (`archive/dumps/v2/decompiled/` where kept locally).

## Layout

```
./
  CMakeLists.txt               build config (32-bit MSVC): omvr_util, omvr_core, d3d11.dll, tests
  vcpkg.json                   nlohmann-json, openxr-loader, openvr (headers only), minhook, doctest
  .clang-format, .clang-tidy   formatting and static analysis (cleanroom/STYLEGUIDE.md section 1)
  res/                         payloads embedded in the DLL
    ui_patch.bin               delta stock UI.PAK -> MotherVR's MOTHER.PAK (neither archive is in the repo)
    openvr_api.dll             Valve OpenVR loader 1.0.10
  src/                         layers, each depending only on those above it; every
                               directory lists its files in its own sources.cmake,
                               named in CMakeLists.txt's MOTHERVR_SOURCE_DIRS
                               (configure fails on an unlisted file or directory)
    util/                      standard library only: math, sync, hash, binary, text, base
                               (Error, Result); the omvr_util static library
    platform/                  one external API each, wrapped in project types
      win32/                   OS base: files, handles, registry, environment, log file,
                               dialogs, crash record; patching/, window/, input/
      d3d11/                   the system d3d11.dll, swapchain, handle conversions;
                               render/ (mirror renderer, texture dump, shaders/)
      openvr/, openxr/         the VR runtimes, their input and probes
      audio/                   game audio to the headset (OpenXR on Meta runtimes)
    game/                      AI.exe: addresses.h and ai_address_tables.inc (every
                               address), layout.h, constants.h, x86.h, build detection,
                               verified patching (patcher, preflight), sites/ (the
                               patch sites); stereo/, input/, ui/, audio/: accessors
                               over the game's objects and functions
    core/                      the mod's logic, plain C++ over value structs, talking
                               to the outside through ports/ (abstract interfaces):
                               stereo, render (shader fix), vr, input, config
                               (mothervr.json), ui (options page, MOTHER.PAK), diag;
                               each area's tuning.h holds its constants; the
                               omvr_core static library
    loader/                    composition root: DLL entry, the Context that builds
                               and wires every object, the d3d11 exports (d3d11.def,
                               resources.rc, resource_ids.h) and hooks/, the thin
                               functions the game, COM and the OS call
  tests/                       unit tests of util and core (doctest), fakes/ for the ports
```

The layers and what each may include are in `cleanroom/STYLEGUIDE.md`
section 2.1: `util` < `platform` < `game` < `core` < `loader`; `platform` and
`game` see only the `core/**/ports/*.h`, `core/**/types.h` and
`core/**/tuning.h` headers of core, and core sees only `game/constants.h` of
the game. The one exception is `core/stereo/stereo_device`, which works on
the game's `StereoInterface` in place (`game/layout.h`).

`src/game/ai_address_tables.inc` is generated from the original binary:

```
python scripts/gen_ai_addresses.py archive/dxgi.dll archive/dumps/v2/decompiled/FUN_10001840_10001840.c src/game/ai_address_tables.inc
```

## Mapping back to the original

Each `.cpp` names the original Ghidra functions it was reconstructed from in a
comment (e.g. `From: FUN_1001d6f0 @ 0x1001d6f0`). These are scaffolding and
should be removed once the reconstruction is verified.

## Building

### Prerequisites

- Windows 10 or 11.
- Visual Studio 2022 or 2026 (the Build Tools are enough) with the
  "Desktop development with C++" workload. The MSVC x86/x64 tools are part of
  it; the mod is a 32-bit DLL because the game is 32-bit.
- CMake 3.20 or newer (Visual Studio ships one).
- vcpkg, cloned and bootstrapped anywhere, with `VCPKG_ROOT` pointing at it:

  ```powershell
  git clone https://github.com/microsoft/vcpkg C:\vcpkg
  C:\vcpkg\bootstrap-vcpkg.bat -disableMetrics
  [Environment]::SetEnvironmentVariable('VCPKG_ROOT', 'C:\vcpkg', 'User')
  ```

  Dependencies (nlohmann-json, openxr-loader, openvr headers) come from the
  manifest `vcpkg.json`, pinned by its `builtin-baseline`, and build on
  the first configure. The overlay triplet `x86-windows-mothervr` (`triplets/`,
  selected by `CMakeLists.txt`) makes them static libraries with the static
  CRT, so the deployed DLL needs no runtime DLLs.
- `res/openvr_api.dll`, Valve's OpenVR loader 1.0.10, which the DLL
  embeds. It is in Git LFS; without LFS, fetch it with
  `pwsh scripts/fetch_openvr_api.ps1`. `CMakeLists.txt` pins the SDK release
  and the file's SHA-256 (`OPENVR_API_RELEASE`, `OPENVR_API_SHA256`); configure
  stops with a message when the file is missing, an LFS pointer, not an x86
  DLL, or another file. The fetch script reads both values from there and
  discards a download that does not match. To move to another loader
  release, change the two values together.

### Build

From a PowerShell prompt in the repository root, with the Visual Studio
generator:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32 `
      -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

Use `-G "Visual Studio 17 2022"` with Visual Studio 2022. The result is
`build/Release/d3d11.dll`.

Or with Ninja, as CI does: open an "x86 Native Tools Command Prompt for VS"
(or run `vcvars32.bat`), then

```bat
cmake -S . -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build build-ninja
```

which gives `build-ninja/d3d11.dll`.

CI (`.github/workflows/ci.yml`) runs on every push to `master` and every pull
request: it checks that no game files are tracked and that commit subjects
follow Conventional Commits (`scripts/check_repo.sh`), then builds with
`-DMOTHERVR_WERROR=ON` (warnings are errors; off by default locally), runs
the unit tests (`ctest`), checks
the DLL (`scripts/check_dll.ps1 <dll> -Version <version>`: x86; exactly the
51 exports of the system `d3d11.dll`; imports only from allowed system DLLs,
never `d3d11.dll`, `dxgi.dll`, the OpenVR or OpenXR loader or a C/C++
runtime; the version resource carries the version built) and uploads the
packaged folder and the symbols as workflow artifacts. A failed check
stops the job before packaging. The build itself is
`.github/workflows/build.yml`, shared with the release workflow.

Release builds also write `d3d11.pdb` next to the DLL (`/Z7`, `/DEBUG` with
`/OPT:REF /OPT:ICF`, so the code is the same as without symbols). It never
goes into the player zip; `scripts/package.ps1` puts it in
`OpenMotherVR-<version>-symbols/` and `-symbols.zip`, and each release keeps
it as an asset, so a crash offset inside `d3d11.dll` from `mothervr.log` can
be mapped to source with that release's PDB.

`-DMOTHERVR_VERSION=1.2.3` stamps a version into the DLL's version resource
(Properties > Details in Explorer); local builds are `0.0.0-dev`.

`src/loader/d3d11.def` and `src/loader/d3d11_exports.inc` both list the pass-through
exports; configure fails if they differ.

### Unit tests

`MOTHERVR_BUILD_TESTS` (on by default) builds `omvr_tests` from `tests/`: a
doctest executable that links `omvr_core` and `omvr_util` only, never the
DLL, so it needs no game, headset or Windows feature beyond the CRT. Run it
with ctest after a build:

```powershell
ctest --test-dir build -C Release --output-on-failure   # Visual Studio generator
ctest --test-dir build-ninja --output-on-failure         # Ninja
```

CI runs the same after its build. Expected values come from the behaviour
of the code before the restructure, the published known answers (CRC-32,
MD5) or the spec. `tests/fakes/fakes.h` has test doubles for the ports
(clock, log, file store, script variables, game signals, archive files).

### Formatting and static analysis

`.clang-format` and `.clang-tidy` carry the rules of
`cleanroom/STYLEGUIDE.md` section 1. Format a file with
`clang-format -i <file>` (the naked forwarder thunks are excluded in
`.clang-format-ignore`). clang-tidy needs the `compile_commands.json` a
Ninja configure writes: `clang-tidy -p build-ninja <file.cpp>`.

### Install and test

Copy `d3d11.dll` next to `AI.exe` in the game folder. On first run it builds
`DATA\MOTHER.PAK` (rebuilt when the mod or `UI.PAK` changes; `MOTHER.PAK.src`
records which `UI.PAK` it came from) from the player's own `DATA\UI.PAK` and the embedded patch
(no game files ship with the mod), writes `openvr_api.dll` and the
`mothervr_input` bindings, and logs to `mothervr.log`. `packaging/README.txt`
is the player-facing guide.

`scripts/run_game.ps1` (`-Seconds N`, `-Keep`, `-NoDeploy`, `-Dll <path>`)
copies the build (and its `.pdb`) into the game folder, starts the game
through Steam and reports the new log lines and any crash, with the
faulting AI.exe code disassembled.

### Profiling (Tracy)

The traced flavour is `d3d11.dll` with the [Tracy](https://github.com/wolfpld/tracy)
0.14.1 profiler client, for local profiling only. It is off unless asked
for, and nothing of it reaches any other build:

- `-DMOTHERVR_TRACY=ON` turns on the vcpkg feature `tracy` (the overlay port
  in `ports/tracy`, pinned to the profiler's version) and defines
  `OMVR_TRACE`. Without it `core/diag/trace.h`'s macros expand to nothing,
  `TracerRef` and `GpuSpan` are empty types, `src/platform/tracy` is not
  compiled and Tracy is not fetched.
- Configure stops if a traced build carries a release version
  (`MOTHERVR_VERSION` other than `0.0.0-dev`), and `scripts/check_dll.ps1`
  fails a DLL that imports `ws2_32.dll` or contains the string `Tracy`, so a
  traced DLL cannot be packaged.

```powershell
cmake -S . -B build-tracy -G "Visual Studio 18 2026" -A Win32 `
      -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
      -DMOTHERVR_WERROR=ON -DMOTHERVR_TRACY=ON
cmake --build build-tracy --config Release   # -> build-tracy/Release/d3d11.dll
```

The client starts with the game's first D3D11 device (`trace: Tracy client
started ...` in the log), listens on localhost only and sends data only
while a profiler is connected; it installs no crash handler. Connect with
the `tracy-profiler` GUI of the same version (0.14.1) from the Tracy
release, or record without it:

```powershell
$env:TRACY_DIR = 'C:\Tools\tracy-0.14.1'      # tracy-capture and tracy-csvexport
pwsh scripts/run_game.ps1 -Tracy -Seconds 120 [-Baseline traces\before.tracy]
python -I scripts/tracy_summary.py traces\<time>.tracy [--baseline old.tracy]
```

`run_game.ps1 -Tracy` deploys `build-tracy`'s DLL and PDB (and refuses an
untraced one; a plain run refuses a traced one), records the session with
`tracy-capture` into `traces\` (ignored by git) and prints the per-zone
tables (count, mean, p50, p95, p99, max; CPU zones per thread, GPU zones
separately).

What is traced: the render thread's frame (`begin_frame`, `end_frame` with
`comfort`, `submit`, `mirror`, `present`), inside `submit` each backend's
work (OpenXR `eye_copy`, `flush`, `xr_end_frame`, `xr_wait_frame`,
`xr_begin_frame`, `input_sync`, `locate`; OpenVR `openvr_submit`,
`wait_get_poses`, `input_sync`), the game thread's `head_pose`, GPU spans
for the game's frame, the comfort pass and the mirror, and CPU and GPU
zones around the game passes the mod hooks: `reflection`, `final_draw`
(the stereo final pass with its AA), `smaa` and `lens_flares`. The rest of
`game_frame` (scene, shadows, lighting) is not broken down. Frame marks
are the render thread's frames and the game thread's head-pose reads
(`game`). Add a zone with `OMVR_ZONE(tracer, "name")` (or
`OMVR_GPU_ZONE`) where a `core::diag::TracerRef` is at hand; a zone
costs nothing in a release build. `xr_wait_frame` and `wait_get_poses`
are the runtime pacing the frame, not work: a frame over budget shows up
there as the GPU's `game_frame` running long.

### Regenerating the UI patch

After changing the menu, rebuild the embedded patch from the stock archive and
MotherVR's modified one (Python 3; `pip install pefile capstone numpy pillow`
for the scripts in `scripts/`). The v2 pages (Comfort, Controls, Hands, Body,
Interaction, HUD, Graphics) are added to MotherVR's archive first: `scripts/ui_pages.py` splices
`scripts/ui_pages/v2_pages.as` into `frontend.GameMenu` of the three menu
movies and recompiles that class with JPEXS FFDec 26.3.0 (Java on `PATH`;
`--ffdec <ffdec.jar>` if it is not at `C:\Tools\ffdec_26.3.0`). Then
`scripts/ui_glyphs.py` adds the VR button glyphs: one image per id in
`src/core/input/glyph_ids.h`, drawn by `scripts/glyph_art.py` from simple
shapes (no font or image is read; `python -I scripts/glyph_art.py <dir> --png`
writes previews), registered in the 19 movies with `ImageSubstitution` by the
same FFDec splice; every other VR glyph image of the input is replaced by the
project's `none` glyph. A new glyph id in `glyph_ids.h` therefore needs a
design in `glyph_art.py` and a new patch. Last, `scripts/ui_terminal.py` adds
the terminal text sizes of `Interaction.TerminalTextScale`: three edited copies
of `TERMINALINTERACTION.GFX`, named `DATA/UI/TERMINALINTERACTION.GFX|text125`,
`|text150` and `|text175`, which the DLL puts in place of the movie for that
setting (`ui_patch.py apply ... <tag>` does the same). The output is the same bytes on
every run; keep it and its work directory under `build-*/` (ignored), never in
the repository:

```powershell
python -I scripts/ui_pages.py <MotherVR 0.8.1 MOTHER.PAK> build\ui\MOTHER_v2.PAK build\ui\work
python -I scripts/ui_glyphs.py build\ui\MOTHER_v2.PAK build\ui\MOTHER_v2g.PAK build\ui\work-glyphs
python -I scripts/ui_terminal.py build\ui\MOTHER_v2g.PAK build\ui\MOTHER_v2gt.PAK
python -I scripts/ui_patch.py make  "<game>\DATA\UI.PAK" build\ui\MOTHER_v2gt.PAK res/ui_patch.bin
python -I scripts/ui_patch.py check "<game>\DATA\UI.PAK" build\ui\MOTHER_v2gt.PAK res/ui_patch.bin
```

A new `res/ui_patch.bin` reaches players with the next DLL: `MOTHER.PAK` is
rebuilt whenever the DLL's build time differs from the one stamped on it.

## Making a release

Releases are built and published by `.github/workflows/release.yml` when a
tag `vMAJOR.MINOR.PATCH` or `vMAJOR.MINOR.PATCH-pre` is pushed. A tag with a
hyphen (`v1.0.0-rc.1`) becomes a prerelease.

1. Write `release-notes/<tag>.md` (for example `release-notes/v1.0.0.md`).
   It becomes the release description on GitHub, so write it for players.
   Commit it to `master` and push.
2. Wait for the `ci` run on that commit to pass.
3. Tag that commit and push the tag:

   ```powershell
   git tag -s v1.0.0 -m "OpenMotherVR 1.0.0"
   git push origin v1.0.0
   ```

The release workflow then:

- refuses a tag that is not on `master`, does not match the version format,
  or has no release notes file;
- builds the DLL with the tag's version stamped in and runs the DLL checks;
- packages `OpenMotherVR-<version>.zip` (`d3d11.dll`, `README.txt`,
  `LICENSE.txt`, `THIRD_PARTY_LICENSES.txt`), the symbols zip
  `OpenMotherVR-<version>-symbols.zip` (`d3d11.pdb`) and `SHA256SUMS.txt`
  (both zips and the DLL);
- signs a build provenance attestation for the zips and the DLL;
- creates the GitHub release with the notes file as its description.

If the workflow fails after the tag is pushed, fix the problem on `master`,
then move the tag:

```powershell
git tag -d v1.0.0
git push origin :refs/tags/v1.0.0
git tag -s v1.0.0 -m "OpenMotherVR 1.0.0"
git push origin v1.0.0
```

Re-running the workflow for an existing release replaces its notes and
assets. `scripts/package.ps1` builds the same package locally.
