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
  CMakeLists.txt               build config (32-bit MSVC target)
  vcpkg.json                   nlohmann-json, openxr-loader, openvr (headers only)
  res/                         payloads embedded in the DLL
    ui_patch.bin               delta stock UI.PAK -> MotherVR's MOTHER.PAK (neither archive is in the repo)
    openvr_api.dll             Valve OpenVR loader 1.0.10
  src/                         layers, each depending only on those above it
    util/                      generic helpers: maths, sync, hashing, binary, text
    platform/                  one external API each, wrapped in project types
      win32/                   log file, dialogs, payload files, patching, window, crash record
      d3d11/                   the system d3d11.dll, swapchain, mirror renderer, texture dump
      openvr/, openxr/         the VR runtimes, their input and probes
      audio/                   game audio to the headset (OpenXR on Meta runtimes)
    game/                      AI.exe: address tables (generated .inc), layouts, build
                               detection, verified patching, the patch sites
    core/                      the mod's logic: stereo, render (shader fix), vr, input,
                               config (mothervr.json), ui (options page, MOTHER.PAK), diag
    loader/                    DLL entry, the context that wires everything, the d3d11
                               exports (d3d11.def, resources.rc, resource_ids.h) and the
                               hooks the game, COM and the OS call
```

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
`-DMOTHERVR_WERROR=ON` (warnings are errors; off by default locally), checks
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

### Install and test

Copy `d3d11.dll` next to `AI.exe` in the game folder. On first run it builds
`DATA\MOTHER.PAK` (rebuilt when the mod or `UI.PAK` changes; `MOTHER.PAK.src`
records which `UI.PAK` it came from) from the player's own `DATA\UI.PAK` and the embedded patch
(no game files ship with the mod), writes `openvr_api.dll` and the
`mothervr_input` bindings, and logs to `mothervr.log`. `packaging/README.txt`
is the player-facing guide.

`scripts/run_game.ps1` (`-Seconds N`, `-Keep`, `-NoDeploy`) copies the build
into the game folder, starts the game through Steam and reports the new log
lines and any crash, with the faulting AI.exe code disassembled.

### Regenerating the UI patch

After changing the menu, rebuild the embedded patch from the stock archive and
MotherVR's modified one (Python 3; `pip install pefile capstone numpy pillow`
for the scripts in `scripts/`):

```powershell
python -I scripts/ui_patch.py make  "<game>\DATA\UI.PAK" <MotherVR 0.8.1 MOTHER.PAK> res/ui_patch.bin
python -I scripts/ui_patch.py check "<game>\DATA\UI.PAK" <MotherVR 0.8.1 MOTHER.PAK> res/ui_patch.bin
```

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
