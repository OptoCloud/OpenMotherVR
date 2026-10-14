<!-- Provenance: project decision (user request for a modern C++ style guide); public C++ and Windows practice. r2: decoupling, layers, constants, utilities and helpers (user request). Revision: r2 -->

# C++ style guide

How the implementation side writes OpenMotherVR. The spec (`spec/`) says
what the mod must do; this document says how the code that does it is
written. Where a rule here and a requirement in the spec meet (threads,
ABI, logging), the spec wins and this guide says how to meet it.

The short version: C++23, RAII everywhere, no undefined behaviour, no work
on the render thread that can stall it, every game address in one table,
every write to the game verified first, logic decoupled from the game and
the platform, no magic numbers, shared code in utilities and helpers
instead of copies.

## 1. Language and toolchain

- **Standard:** C++23 (`CMAKE_CXX_STANDARD 23`, `CMAKE_CXX_STANDARD_REQUIRED
  ON`, `CMAKE_CXX_EXTENSIONS OFF`). Use library features MSVC ships for that
  mode: `std::expected`, `std::span`, `std::bit_cast`, `std::byteswap`,
  `std::to_underlying`, `std::format`, `std::ranges`. No compiler
  extensions beyond the calling-convention keywords the game requires (§6).
- **Compiler:** MSVC from Visual Studio 2022 or newer, 32-bit (`-A Win32`, or
  Ninja in a `vcvars32` environment). The game is 32-bit; nothing else runs.
- **Flags:** `/W4 /permissive- /Zc:__cplusplus /utf-8 /EHsc`, plus `/WX` in
  CI. Static CRT (`/MT`), so the DLL needs no runtime installed.
- **Dependencies** come from vcpkg in manifest mode, pinned by
  `builtin-baseline`, built static with the static CRT through a project
  overlay triplet. Adding one needs a reason in the commit. Prefer the
  Windows SDK and the standard library to a new dependency.
- **Never link `d3d11.lib` or `dxgi.lib`**: the DLL is `d3d11.dll` and would
  import itself. Reach D3D11 and DXGI through interfaces and through the
  real system DLL loaded by full path.
- **Formatting:** `clang-format` with the checked-in `.clang-format`
  (4-space indent, 110 columns, braces on the same line, pointer `T* p`).
  CI fails on unformatted code. Do not hand-format around it.
- **Static analysis:** `clang-tidy` with the checked-in config
  (`bugprone-*`, `cppcoreguidelines-*` minus the noisy ones,
  `modernize-*`, `performance-*`, `readability-*` subset) runs in CI on
  changed files. MSVC `/analyze` in a nightly job.

## 2. Architecture and structure

### 2.1 Layers

The code is split into layers. A layer may depend only on the layers below
it; nothing reaches upward or sideways around this order.

| Layer | Directory | Knows about | Never knows about |
|---|---|---|---|
| Utilities | `util/` | the C++ standard library | the game, Windows, D3D, VR runtimes, the mod's features |
| Platform adapters | `platform/win32/`, `platform/d3d11/`, `platform/openvr/`, `platform/openxr/`, `platform/audio/` | one external API each, wrapped in project types | the game, other adapters, feature logic |
| Game interface | `game/` | `AI.exe`: the address table, layouts, ABIs, patching | VR runtimes, D3D, feature policy |
| Core logic | `core/` (`stereo/`, `input/`, `config/`, `ui/`, `diag/`) | project types and the interfaces of §2.3 | Windows, D3D, OpenVR, OpenXR, `AI.exe` addresses |
| Composition | `loader/` | everything: builds the objects and wires them together | (top layer) |

The point of the split: core logic (pose maths, snap turn, recenter rules,
config sanitising, the UI linker, the DXBC rewrite, log formatting) is
plain C++ that runs and is unit-tested without the game, a headset or
Windows. Platform and game code is thin and makes no decisions of its own.

### 2.2 Files and namespaces

- One class or one closely related set of functions per file pair
  (`name.h` / `name.cpp`). Headers include what they use and nothing more;
  `#pragma once`.
- Everything lives in `namespace omvr` and a sub-namespace per layer and
  area (`omvr::core::input`, `omvr::platform::openxr`, `omvr::util`).
  Implementation details go in an unnamed namespace in the `.cpp`, never in
  a header.
- Platform headers (`<windows.h>`, D3D, OpenVR, OpenXR) are included only in
  the `.cpp` files of their adapter, with `WIN32_LEAN_AND_MEAN` and
  `NOMINMAX` set project-wide. No header outside `platform/` includes them;
  adapter headers expose project types only (use the pimpl idiom or opaque
  handles where an adapter must hold platform objects).
- A file does one job. If its name needs "and" or "utils" to describe it,
  split it.

### 2.3 Decoupling

- **Interfaces at the seams.** Core logic talks to the outside through small
  abstract interfaces it owns, defined in `core/` (for example
  `HeadTracker`, `FrameSubmitter`, `ControllerSource`, `SettingsStore`,
  `Clock`, `LogSink`). Adapters in `platform/` and `game/` implement them.
  An interface has the few methods its caller needs, not a mirror of the
  API behind it.
- **Dependencies are passed in, not reached for.** A class receives what it
  uses through its constructor (references, or `std::unique_ptr` for owned
  parts). No singletons, no service locators, no `extern` globals in core,
  game or platform code.
- **One composition root.** `loader/` is the only place that creates the
  long-lived objects and connects them. The few globals the game's hooks
  force on us (a hook is a free function the game calls) live there, in one
  `Context` object reached through one accessor, and every hook forwards to
  a member of an object at once.
- **Hooks are thin.** A function the game calls checks its arguments,
  forwards to core logic and translates the result back into what the game
  expects. It contains no feature decisions.
- **Data flows one way per frame.** Game and runtime state are read into
  plain value structs (`HeadPose`, `ControllerState`, `FrameTiming`), core
  logic turns them into results, adapters write the results out. Core
  logic never holds pointers into game memory.
- **Time, randomness and I/O are injected** through `Clock`, file and log
  interfaces, so tests can drive them.
- **No cyclic dependencies** between files or directories; CI checks the
  include graph against §2.1.

### 2.4 Constants and magic numbers

- **No magic numbers.** Every literal that carries meaning is a named
  `constexpr` with its unit in the name or type and, where it comes from
  the spec, the spec section in a comment. Only `0`, `1`, `-1` and `2` in
  plainly arithmetic use (halving, sign, increments), `nullptr` and empty
  strings are written inline. clang-tidy's `readability-magic-numbers` is
  on.

  ```cpp
  // 05 §3.2c: a held stick repeats a step this often.
  inline constexpr std::chrono::milliseconds kSnapRepeat{300};
  // 03 §7: distance of the virtual screen from the player.
  inline constexpr Metres kVirtualScreenDistance{1.5f};
  ```
- **Constants live in their own headers, by kind:**
  - `game/addresses.h`: the per-build address table (§6);
  - `game/layout.h`: structure layouts with their `static_assert`s;
  - `game/constants.h`: other game facts (setting ids, mode numbers, stock
    strings, control code ranges);
  - `core/<area>/tuning.h`: behaviour parameters and project decisions of
    that area (distances, thresholds, timings, ranges);
  - `platform/<api>/constants.h`: values an external API defines that its
    headers do not (interface version strings, error codes we name).

  Feature code includes the header it needs; it never redefines a constant
  locally, and a value is defined in exactly one place.
- **Settings are not constants.** Defaults and ranges of `mothervr.json`
  settings live in one settings table in `core/config/` (key, type,
  default, range, sanitise rule), and every reader and writer uses that
  table.
- **Enumerations over integers.** Mode numbers, runtime kinds and game ids
  get an `enum class` with explicit values where the game fixes them. Bit
  masks the game defines get a named `constexpr` per bit and a small typed
  wrapper, never bare hex at the use site.
- **Byte sequences** written to or checked in the game are named
  `constexpr std::array<std::uint8_t, N>` in `game/`, next to the site they
  belong to, with the spec section.

### 2.5 Utilities and helpers

- **`util/` is generic.** It holds code with no knowledge of the game, the
  platform or the mod's features: strong unit types, maths (vectors,
  quaternions, matrices, angle wrap), string and path helpers, hashing
  (CRC-32, SHA-256), a binary reader and writer, `std::expected` helpers,
  scope guards, the torn-free handover, the fixed-capacity formatter for
  the render thread. Each is unit-tested.
- **Helpers instead of copies.** When the same logic appears a second time,
  it becomes a named helper. Helpers that know about one area live in that
  area (`core/stereo/projection.h`); helpers that know about nothing live
  in `util/`.
- **Small functions.** A function does one thing, fits on a screen (about
  40 lines as a guide) and nests at most three levels. Extract a named
  helper rather than writing a comment that explains a block.
- **Pure where possible.** Helpers take inputs and return outputs; they do
  not read globals, log or touch the game. Side effects stay in the thin
  layers of §2.3.
- **No dumping grounds.** `util/` is organised by topic (`util/math/`,
  `util/text/`, `util/binary/`, `util/sync/`). A helper that fits no topic
  belongs in an area instead.

### 2.6 Classes

- One responsibility per class. Prefer composition to inheritance; inherit
  only to implement an interface of §2.3.
- Rule of zero by default: members own their resources (§5), so classes
  declare no special members. If one is needed, declare all five, or delete
  copy and move explicitly.
- Data that is just data is a `struct` with public members and no
  invariants; anything with invariants is a `class` whose constructor
  establishes them.
- Single-argument constructors are `explicit`, overrides `override`, and
  classes not meant for derivation `final`.

## 3. Naming

| Kind | Style | Example |
|---|---|---|
| Types, enums, concepts | `PascalCase` | `StereoDevice`, `RuntimeKind` |
| Functions, variables, members | `snake_case` | `begin_frame`, `eye_size` |
| Private data members | `snake_case_` | `pose_` |
| Constants and `constexpr` values | `kPascalCase` | `kSnapRepeat` |
| Enumerators | `PascalCase` | `RuntimeKind::OpenXr` |
| Macros (avoid) | `OMVR_UPPER_CASE` | `OMVR_ASSERT` |
| Files | `snake_case.h/.cpp` | `stereo_device.cpp` |

Game structures keep the spec's names for their fields and give the offset
in a comment where it is not obvious. Units go in names when a value has
one: `distance_m`, `timeout_ms`, `yaw_rad`, `fov_tan`.

## 4. Types and values

- `enum class` for every enumeration; `std::to_underlying` to convert.
- Fixed-width integers (`std::uint32_t`) for anything that crosses into the
  game, a file format or a runtime API. `std::size_t` for sizes and indices
  in the mod's own containers.
- Units are types where mixing them would be a bug: `std::chrono` durations
  for time; small strong types (`Metres`, `Radians`) where the spec mixes
  units. No bare `int ms`.
- `std::bit_cast` to reinterpret bytes; never type-pun through a union or a
  `reinterpret_cast` to a different object type and read it.
- Conversions between floating point and integers are explicit
  (`static_cast`), and narrowing conversions that can lose data are checked
  or commented. The snap-turn step is computed in floating point (04 §6).
- No C-style casts. `reinterpret_cast` only for addresses of game code and
  data and for `GetProcAddress` results, each in one helper.
- `const` by default; `constexpr` for every table and constant the compiler
  can see (§2.4); `consteval` for compile-time checks of those tables.

## 5. Ownership and resources

- RAII for everything that must be released: COM interfaces through a
  `com_ptr<T>` (a small project template or `Microsoft::WRL::ComPtr`),
  handles through `unique_handle` wrappers, runtime objects (OpenXR
  handles, OpenVR overlays) through owners that destroy them. No manual
  `Release()`, `CloseHandle()` or `xrDestroy*` outside those owners.
- No `new`/`delete` in project code. `std::unique_ptr` for single ownership,
  values for everything that fits. `std::shared_ptr` only with a written
  reason.
- Objects the game owns are never freed by the mod. A pointer into game
  memory is a non-owning raw pointer or a reference, and its lifetime rule
  (for example "valid only while the camera behaviour is active", 05 §6) is
  written next to it.
- No objects with non-trivial destructors at namespace scope in code that can
  run at process exit: the mod does no work at exit (12). Use function-local
  statics with deliberate leaking (`static auto& x = *new T`) only where a
  global must outlive everything, and say why.

## 6. Interfacing with the game

- **Addresses:** every `AI.exe` RVA is in one `constexpr` table with one row
  per name and a column per build (2015, 2020; 0 means unknown, and the
  feature that needs it stays off on that build). Feature code asks the
  table by name; it never contains an address literal.
- **Layouts:** a game structure the mod reads or writes is a `struct` with
  explicit padding, and `static_assert`s on its size and on every field's
  `offsetof`. If the game's layout and the struct disagree, the build
  fails, not the game.
- **Calling conventions:** replacements for the game's `__thiscall` methods
  are `__fastcall(Self* self, void* /*edx*/, args...)` with the same stack
  arguments, so the callee pops what the game expects. `__cdecl` and
  `__stdcall` replacements match the game's function exactly. Every
  replacement has a `static_assert` or a test that checks its type against
  the declared original type. Return types that come back in `eax:edx` or
  `st(0)` (`bool`, 64-bit, `float`) are spelled exactly as the game's.
- **Writes to the game:** go through one patching module that verifies the
  site first (expected bytes, call target, prologue, vtable slot), changes
  protection, writes, restores protection and flushes the instruction
  cache. Nothing else calls `VirtualProtect` or writes game memory. All
  sites of a feature are verified before the first write; a mismatch
  patches nothing and is logged (12).
- **Replacements are `noexcept`.** An exception never crosses into game
  code: a hook catches what it can throw and logs it.
- Game strings are `std::string_view` over the game's memory, never
  modified.

## 7. Errors

- Expected failures (a runtime missing, a file unreadable, a site that does
  not match) return `std::expected<T, Error>` where `Error` carries a code
  and a short context. Callers handle or propagate them; ignoring one
  needs `[[maybe_unused]]` and a comment.
- Exceptions only for programming errors and for library code that throws
  (`std::format`, `nlohmann::json`). Every entry point the outside world
  calls (`DllMain`, exports, hooks, threads, COM callbacks) is `noexcept`
  and catches everything.
- Windows errors are captured right after the failing call
  (`GetLastError`, `HRESULT`) and carried in the `Error`, never re-read
  later.
- `OMVR_ASSERT(cond)` for invariants: logs and breaks in debug builds, logs
  once in release builds. It never aborts the game.

## 8. Threads

The spec (12) names the threads and what crosses between them. In code:

- Each function documents the thread it runs on (`// Render thread.`) when
  that is not obvious from where it lives.
- Data shared between threads is `std::atomic<T>` for single values, or a
  torn-free handover (a sequence lock with atomic payload words, or a
  double buffer with an atomic index) for multi-word values. No plain
  shared variables, no `volatile` for synchronisation.
- Memory orders are explicit and justified in a comment when weaker than
  `seq_cst`.
- Mutexes (`std::mutex`, `std::scoped_lock`) only off the render thread,
  never held across a call into the game or a runtime.
- No thread is created in `DllMain`; no work under the loader lock beyond
  what 01 allows.

## 9. The render thread and performance

- No allocation, file I/O, locking or logging per frame on the render
  thread. Buffers are sized once (at configure) and reused; `std::format`
  into a fixed buffer when a rate-limited log line is needed (11).
- No `std::function` or virtual dispatch added to per-frame paths where a
  direct call works; measure before adding indirection there.
- Containers on hot paths are `std::array` or pre-reserved `std::vector`.
- A change that touches a per-frame path states its cost (measured with
  the in-game timers or a frame capture) in the commit body.

## 10. Logging

- Through the project logger only, in the format of 11. Each event has a
  message template with named properties; the template text is a constant
  so it can be searched for.
- Log once per state change. Repeating conditions are rate limited as 11
  says. Debug-only output checks the debug setting at the call site.
- Never log secrets or full user paths beyond the game and mod directories.

## 11. Comments and documentation

- Comments say why, not what: the game behaviour or requirement that forces
  the code, with the spec section (`// 05 §4.2: the game reads the quad
  matrix before Present.`).
- Every game fact used in code cites the spec section it comes from.
- No commented-out code. No `TODO` without an issue number.
- Public functions in headers get one or two lines saying what they do, on
  which thread, and what they need; not a restatement of the signature.

## 12. Testing

- Pure logic has unit tests (doctest or Catch2 via vcpkg), built as a
  separate 32-bit test executable and run in CI: maths (pose conversion,
  projection, snap step), config parsing and sanitising, the command line,
  the log formatter, the address table's integrity, the PAK2 reader and
  writer, the DXBC parser and checksum, the UI linker's checks.
- Core logic is tested through fake implementations of the §2.3
  interfaces (a fake clock, a scripted head tracker, an in-memory settings
  store), so tests need no game, headset or Windows API.
- Code that needs the game is checked in game against the spec's
  Acceptance sections, and the commit says which checks were run.
- Tests use synthetic data or data generated from the player's own game
  files at test time. Game files are never committed.

## 13. Commits

- Conventional Commits 1.0.0: `type(scope): imperative summary`, at most 72
  characters, no trailing period; types `feat`, `fix`, `perf`, `refactor`,
  `docs`, `build`, `ci`, `test`, `chore`, `revert`.
- One logical change per commit. The body says what and why.
- A commit that implements or changes behaviour names the spec documents
  and revisions it follows: `Spec: spec/03-hmd-and-frames.md r3`.
- No AI-attribution trailers.

## 14. Clean-room hygiene in code

- Code comes from the spec, public SDK documentation and the game itself,
  nothing else (`CLEANROOM.md`). If something cannot be built from those,
  ask the spec side; do not guess from memory of other mods.
- Names in code are the project's own or the game's (as the spec gives
  them). Do not adopt names that appear only as examples of what the spec
  forbids.
- Shaders are written in HLSL from the spec's description and compiled at
  build time; menu resources are built as `spec/14-ui-resources.md` says.
