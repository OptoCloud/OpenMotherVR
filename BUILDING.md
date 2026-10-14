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
  src/
    dllmain.cpp                DLL entry, init order
    d3d11.def, mothervr.rc     exports, embedded payloads (resource.h has the IDs)
    proxy/                     d3d11 forwarders, swapchain Present/ResizeBuffers and
                               CreatePixelShader hooks
    render/                    pixel shader fix (per-eye lighting dither)
    diag/                      log file; texture_dump writes a texture to .dds (debug)
    config/                    mothervr.json
    util/                      small helpers (typed GetProcAddress)
    audio/                     game audio to the headset (OpenXR on Meta runtimes)
    integration/
      exe_check                AI.exe build detection (2015 / 2020)
      payloads                 deploys MOTHER.PAK and openvr_api.dll
      ui_patch                 builds MOTHER.PAK from UI.PAK and the embedded patch
      script_bridge            mvr_* console commands, setMother* UI pushes
      dialogs                  message boxes queued in DllMain, shown after it
    hook/
      ai_addresses             per-build AI.exe address tables (generated .inc)
      hook_installer           the patch sequence from FUN_1001d6f0
      ai_patches, memory_patch patch tables and write helpers
    runtime/                   VR runtime detection, OpenVR and OpenXR backends and input
    input/                     VR input through the game's gamepad (vtable hooks, action-map remap)
    stereo/                    StereoManager and stereo device, game-side stereo hooks
                               (game_hooks), the OpenXR virtual-screen Present path
```

`src/hook/ai_address_tables.inc` is generated from the original binary:

```
python scripts/gen_ai_addresses.py archive/dxgi.dll archive/dumps/v2/decompiled/FUN_10001840_10001840.c src/hook/ai_address_tables.inc
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
  `pwsh scripts/fetch_openvr_api.ps1` (configure stops with a message if the
  file is only an LFS pointer).

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
the DLL (`scripts/check_dll.ps1`: x86, no import of `d3d11.dll` or the MSVC
runtime, the D3D11 exports present) and uploads the packaged folder as a
workflow artifact. The build itself is `.github/workflows/build.yml`, shared
with the release workflow.

`-DMOTHERVR_VERSION=1.2.3` stamps a version into the DLL's version resource
(Properties > Details in Explorer); local builds are `0.0.0-dev`.

`src/d3d11.def` and `src/proxy/d3d11_exports.inc` both list the pass-through
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
  `LICENSE.txt`, `THIRD_PARTY_LICENSES.txt`) and `SHA256SUMS.txt`;
- signs a build provenance attestation for the zip and the DLL;
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
