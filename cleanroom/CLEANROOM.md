# Clean-room process

OpenMotherVR is a VR mod for Alien: Isolation (2015 Steam build of
`AI.exe`), written from scratch. Its behaviour is modelled on MotherVR 0.8.1
by Nibre, a closed-source mod. To keep this project free of MotherVR's
copyrighted expression, it is built in a clean room: the people (or agents)
who study existing mods never write code here, and the people (or agents)
who write code here never see existing mods.

## Roles

**Spec side.** Studies the game, MotherVR and other mods in a separate
workspace and writes the functional specification in `spec/`. It may read
anything. What it writes into `spec/` must follow the spec rules below.

**Implementation side.** Writes everything outside `spec/`. It works in its
own clone of this repository and may read only:

- this repository (`spec/`, its own code, these documents);
- public SDKs and their documentation (Windows SDK, Direct3D 11, OpenVR,
  OpenXR, nlohmann::json, ...);
- `AI.exe` itself and the game's own data files (to confirm interface facts
  the spec gives: an address, a field, a calling convention).

It must not read MotherVR's binary, decompiles or reconstruction, GRAND or
any other mod, or the spec side's workspace and notes. Questions about the
spec go back to the spec side as issues; answers come back as spec changes.

## Spec rules

The spec describes **what the mod must do** and **how the game works**, not
how MotherVR does it.

Allowed:
- Facts about `AI.exe` and the game: addresses (RVAs per build), data
  layouts, calling conventions, what game functions and fields mean, what
  game data files contain.
- Required behaviour, observable results and the reasons for them
  ("menus appear on a level screen 1.5 m in front of the player, because...").
- Numeric parameters where they define behaviour (distances, thresholds).
- Interfaces between parts of the mod, as prose or tables.
- References to public APIs (OpenVR, OpenXR, D3D11) by name.

Not allowed:
- Code from MotherVR or any other mod, in any language, including
  pseudo-code transliterated from a decompile.
- Decompiler output, disassembly of mod binaries, or mod binary addresses
  and symbol names (`FUN_...`, `DAT_...`, `0x10......`).
- Mod-specific assets: shader bytecode, UI archives, images.
- Structure that only mirrors MotherVR's internal design where the game does
  not require it (class names, file layout, internal vtables of the mod).

Short pseudo-code is acceptable only to state an algorithm the spec side
derived from the game or from the requirement itself (for example a matrix
convention of `AI.exe`), and must say so.

`tools/spec_lint.py` checks `spec/` for the mechanical part of these rules
and runs on every spec change.

## Provenance

- Every spec document starts with a header listing what it was derived from
  (game analysis, observation in game, user requirement) and its revision.
- Implementation commits that implement or change behaviour name the spec
  documents and revision they follow (`Spec: spec/03-stereo-device.md r2`).
- Assets are made here: shaders are written in HLSL from the spec's
  description; the options menu is produced by patching the player's own
  `UI.PAK` at install time, never by shipping a modified game archive.
