# Clean-room process

Revision r2. r1: roles, spec rules, provenance. r2 (with spec r5): no other
mod named in the spec and the compatibility exceptions; when "the earlier
mod" may be mentioned; provenance classes, including facts taken from the
earlier mod's data; the reconstruction is never a source; how the
implementation side confirms facts in `AI.exe`; the questions and answers
channel; spec-side labels for game code; the lint before every spec commit.

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
- `AI.exe` itself and the game's own data files, to confirm interface facts
  the spec gives (an address, a field, a calling convention, the bytes at a
  site). It may examine them with static analysis tools (a disassembler, a
  hex viewer, a PE or archive reader) and may run the game with no mod, or
  with its own build. It never runs the game with another mod installed and
  never inspects a process in which another mod is loaded. When the game
  differs from the spec, it does not work around the difference from the
  binary on its own: it reports it as a question (below) and leaves the
  affected feature off until the answer arrives.

It must not read MotherVR's binary, decompiles or reconstruction, GRAND or
any other mod, or the spec side's workspace and notes.

### Questions and answers

The implementation side writes its questions in `spec/QUESTIONS.md` in its
own clone, each naming the spec document, revision and section it is
about. The user carries that file to the spec side. The spec side answers
in `spec/ANSWERS.md` and in changes to the spec documents, which reach the
implementation side as a new spec revision. Answers follow the spec rules:
they state required results, game facts and decisions with their reasons,
and never quote or describe another mod or the spec side's notes. Where the
spec side has looked at the implementation's behaviour to answer, it
comments only on whether that behaviour meets the required result.

## Spec rules

The spec describes **what the mod must do** and **how the game works**, not
how MotherVR does it.

Allowed:
- Facts about `AI.exe` and the game: addresses (RVAs per build), data
  layouts, calling conventions, what game functions and fields mean, what
  game data files contain. Game code is described in prose and as the byte
  strings an implementation must check or write; disassembly listings are
  not used, of the game or of any other binary, so that no mod listing can
  pass as a game one (the lint rejects listings).
- Required behaviour, observable results and the reasons for them
  ("menus appear on a level screen 1.5 m in front of the player, because...").
- Numeric parameters where they define behaviour (distances, thresholds),
  each with a reason or marked as a project decision.
- Interfaces between parts of the mod, as prose or tables.
- References to public APIs (OpenVR, OpenXR, D3D11) by name.
- Labels for game functions, fields and sites that the game leaves unnamed
  (for example `StereoPushMode`). The spec side chooses them to describe
  what the code does; they are not taken from the reconstruction or from a
  decompiler project, and the implementation may rename them.

Not allowed:
- Code from MotherVR or any other mod, in any language, including
  pseudo-code transliterated from a decompile.
- Decompiler output, disassembly of mod binaries, or mod binary addresses
  and symbol names (`FUN_...`, `DAT_...`, `0x10......`).
- Mod-specific assets: shader bytecode, UI archives, images.
- Structure that only mirrors MotherVR's internal design where the game does
  not require it (class names, file layout, internal vtables of the mod).
  This includes the order of the mod's steps: the spec states an order only
  where the game, Windows or a runtime API requires it, and says why.
- The mod's own wording: message box and log text, menu labels and help
  texts, and its visual layout. The spec says what a message or label must
  convey; the implementation writes its own text. Strings the game itself
  contains may be named.
- Names MotherVR invented for its own internal use, such as the functions
  its menu calls in its DLL. The spec describes what such messages carry;
  the names are the implementation's choice. Names players or their files
  depend on stay exact for compatibility (`mothervr.json` keys and values,
  command-line switches, `mvr_*` console commands), and the spec says that
  compatibility is the reason.
- The name of any other mod or of its author. The only exceptions are the
  compatibility names players or their files depend on, written exactly and
  always in a code span: `mothervr.json` (and its keys and values),
  `mothervr.log`, `MOTHER.PAK`, `mothervr_input` (the folder name),
  `openvr_api.dll`, the `mvr_*` console commands, the `-vrforce*` switches,
  `-vrscale`, and the options page title `MotherVR`.
- Describing the earlier mod. The spec may mention "the earlier mod" only in
  a document's provenance header, or where a section states a known problem
  the spec fixes, and then only as an effect a player could observe ("an
  earlier mod showed each eye at half resolution"). It never describes that
  mod's internals: its code, data structures, call order, bugs in its code,
  which functions it hooked or what it logged.
- Citing the reconstruction. The project's own earlier code (the current
  rebuild, `src/`) is never named as a source or a baseline. What was
  learnt from it is restated as required behaviour, a project decision
  with its reason, or an observation of the game by the spec side.

How detailed the spec is does not matter; what kind of thing it states
does. Facts about the game and its file formats, required behaviour, its
numeric parameters and the interfaces compatibility needs can be as exact
as necessary. A useful test for any section: if an implementer following
it could only write code that mirrors one of the mod's functions step by
step, it is a translation of that code and must be rewritten as the
required result.

Short pseudo-code is acceptable only to state an algorithm the spec side
derived from the game or from the requirement itself (for example a matrix
convention of `AI.exe`), and must say so.

`tools/spec_lint.py` checks `spec/` for the mechanical part of these rules.
The spec side runs it before every spec commit
(`python -I cleanroom/tools/spec_lint.py cleanroom/spec` from the
repository root) and commits only when it reports no problems; each warning
it prints is checked by hand.

## Provenance

- Every spec document starts with a header listing what it was derived from
  and its revision. The sources are:
  - game analysis (of `AI.exe` and the game's own data files);
  - observation in game by the spec side;
  - user requirements and project decisions, each with its reason;
  - public API documentation;
  - facts taken from the earlier mod's data. These are interface facts about
    `AI.exe` that were first found there, for example the 2020 RVAs, which
    come from the earlier mod's per-build address table. Every document
    that uses them says so in its header and its conventions paragraph:
    "2020 values were taken from the earlier mod's per-build address table
    (and, for inline sites, its installer) and have not been verified
    against a 2020 `AI.exe`." They enter the spec only as such facts, never
    as behaviour.
- Behaviour seen in the earlier mod enters the spec only as an observable
  result with its reason, never as a sequence of steps.
- The reconstruction is not a source class. It is never cited.
- Implementation commits that implement or change behaviour name the spec
  documents and revision they follow (`Spec: spec/03-hmd-and-frames.md r3`).
- Assets are made here: shaders are written in HLSL from the spec's
  description; the options menu is produced by patching the player's own
  `UI.PAK` at install time, never by shipping a modified game archive.
