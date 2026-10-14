# Embedded resources and the UI layer

The 18 MB `.rsrc` section (98% of `dxgi.dll`) was not covered by the first
analysis pass. It holds two payloads, both extracted to `dumps/rsrc/`.

| Resource | Size | What it is | File |
|---|---|---|---|
| `BINARY` 201 (0xC9) | 424,736 | Valve `openvr_api.dll` 1.0.10, stock retail build (PDB `c:\buildslave\steamvr_rel_win32\...`), x86 | `rsrc/res201_embedded.dll` |
| `BINARY` 202 (0xCA) | 17,613,608 | Full replacement for the game's `DATA/UI.PAK` (PAK2 archive, 522 entries) | `rsrc/res202_UI.PAK` |
| `RT_MANIFEST` 2 | 548 | Standard manifest | |

`openvr_api.dll` is open source (OpenVR SDK 1.0.10) and is not decompiled.

## How the original deploys them (`FUN_1001d6f0`, the hook installer)

1. `FUN_1001e720`: if `<cwd>\DATA\MOTHER.PAK` exists and its write time equals
   the mod's own timestamp (`this+0x38`), skip. Otherwise require
   `DATA\UI.PAK` to exist (fatal message box if not), then write resource 202
   to `DATA\MOTHER.PAK` via a file mapping and stamp all three file times with
   `this+0x38`. Returns false on failure, which aborts hook installation.
2. Patch the 4-byte pointer at `DAT_10066d34` (inside AI.exe) to point at the
   mod's `std::string` at `this+0x00`, which holds `data\mother.pak`
   (literal at `0x10059dd8`). The game then loads the modded UI archive instead
   of `data\ui.pak`; the stock file on disk is never touched.
3. If the configured runtime (`config+0x2ec`) is 2 (OpenVR), `FUN_1001e940`
   writes resource 201 to `<cwd>\openvr_api.dll` with the same timestamp
   check. The mod therefore always runs against its own pinned OpenVR 1.0.10
   loader, not the one shipped with SteamVR.

**Reconstruction status:** none of this is implemented in `recon/`. Without
step 2 the game loads the stock UI and the Options > MotherVR menu does not
exist, so `mvr_*` commands are never sent.

## PAK2 format

See `scripts/pak2.py` (list / extract / diff). Header `PAK2`, `names_size`,
`count`, `alignment`; then NUL-terminated names; then `count` absolute end
offsets; entries start at the previous end rounded up to `alignment`.

## What the modded UI.PAK changes (`rsrc/UI_diff.txt`)

Against the stock Steam `DATA/UI.PAK` (505 entries): 24 `.GFX` movies changed,
17 `GLYPH_VR_*.DDS` controller glyphs added, nothing removed. ActionScript 3
was decompiled with JPEXS FFDec 26.3.0 (`C:\Tools\ffdec_26.3.0`) into
`dumps/as/{stock,mod}/<MOVIE>/scripts/`; the 29 changed classes are listed in
`dumps/as_diff.txt`.

- `frontend/GameMenu.as` (NEWFRONTENDMENU, PAUSEMENU, GAMEOVERMENU; about 280
  changed lines each): the MotherVR options page.
- `ImageSubstitution.as` (19 movies): `vr_*` input IDs and
  `mother_load_image_files()` for the VR controller glyphs.
- Small changes in CraftingScreen, LoadingScreen, NewFrontendMenu,
  PickupOverlay, PopupOverlay, TRCSystemOverlay, WeaponStuff.

### UI to native contract

UI to DLL, via `ExternalInterface.call("<cmd> <value>")`, handled by the
command dispatcher the mod installs (`recon: handle_mvr_command`):

`mvr_clipblinding`, `mvr_debug`, `mvr_inputrelativewalking`,
`mvr_inputstrafing`, `mvr_mirrorview`, `mvr_mirrorviewsym`, `mvr_pixelscale`,
`mvr_skipintro`, `mvr_snaprotation`, `mvr_snaprotationsegments`,
`mvr_togglecrouch`, `mvr_togglegrips`, `mvr_vrruntime 0|1|2`

DLL to UI, AS3 functions the DLL invokes to push state:

`setMotherVROptionsEnabled`, `setMotherClipBlinding`, `setMotherDebug`,
`setMotherInputRelativeWalking`, `setMotherInputStrafing`,
`setMotherMirrorView`, `setMotherMirrorViewSym`, `setMotherPixelScale`,
`setMotherSetVRRuntime`, `setMotherSkipIntro`, `setMotherSnapRotation`,
`setMotherSnapRotationSegments`, `setMotherToggleCrouch`,
`setMotherToggleGrips`, plus `addMotherViewControl`.

All of these are already referenced in `recon/src/integration/script_bridge.cpp`.

# Decompile v2 (`dumps/v2/`)

`scripts/FillGaps.java` creates functions the auto-analysis missed, from
relocation targets, orphan instructions, and undefined non-padding bytes
(discarding multi-byte NOP filler and stray `ret` tails). Result, written to
the saved Ghidra project:

| | v1 (`dumps/decompiled`) | v2 (`dumps/v2/decompiled`) |
|---|---|---|
| `.text` bytes inside functions | 94.22% | 97.38% |
| Orphan instructions | 6,341 B | 0 |
| Functions decompiled | 1,930 | 2,264 |
| Decompile errors / `halt_baddata` | 0 | 0 |

The remaining 1,967 undefined bytes are switch tables and multi-byte NOP
padding that the coverage count does not class as padding. Newly recovered
functions include the render detours referenced by the hook installer:
`10020c60`, `10020c80`, `10020cf0`, `10020da0`, `10020dd0`, `1001f830`.
`dumps/v2/` also has refreshed `functions.tsv` etc. Use v2 from now on; v1 is
kept for reference. Ghidra project backup from before FillGaps:
`ghidra_project.bak-20261014/`.
