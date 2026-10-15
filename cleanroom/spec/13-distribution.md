<!-- Provenance: in-game observation on the 2015 Steam build with SteamVR and a Valve Index (files that appear next to the game and when, the log shared by the two processes of a Steam relaunch, message boxes and what the game does while they are open); Windows documentation (DllMain and the loader lock, file sharing modes, version resources, PE headers and imports); OpenVR public documentation (`VR_InitInternal2` error codes including 117, the x86 `openvr_api.dll` redistributable of the public SDK, SteamVR Input action manifests); the requirements of 03 §6.4, 06 §3, 10, 11, 12 and 14 that this document gathers from the player's side; the project's own release packaging and player guide; user decisions (default runtime OpenVR in a fresh `mothervr.json`, no game files shipped, UI archive built as in 14-ui-resources.md, message wording is the implementation's own, the mod never needs elevation); a tester's report on SteamVR with a Steam Frame headset (an OpenXR failure message with an empty result text and no way back to SteamVR; the game not shown in the headset with Desktop Game Theatre on); project decisions of r3, each with its reason inline (the OpenXR failure message's added facts; the non-blocking headset-not-showing-the-game warning). Revision: r3 -->

# 13. Distribution: what the player installs, files, messages, uninstall

What the player installs, every file the mod creates or rewrites next to the
game and when, the log file, the messages the player can see, how to
uninstall, the DLL's own properties and the release package. Loading and
patching are in `01-loading.md`; the config file's format is in
`06-options-and-config.md` §3; how the UI archive is built is in
`14-ui-resources.md`; the log's wording and format are in
`11-diagnostics.md`.

"Game directory" is the folder holding `AI.exe`, which is also the folder of
the proxy DLL. The mod finds it from its own module path, never from the
process's current directory, and every file of this document is in it or
below it (Steam may start the game with another current directory).

## 1. What the player installs

| Item | Requirement |
|---|---|
| Game | Alien: Isolation, `AI.exe` of a recognised build (01 §3). The 2015 Steam build is the tested one |
| Mod | One file, `d3d11.dll` (32-bit), copied into the game directory |
| VR runtime | SteamVR for runtime OpenVR (it need not be running when the game starts, §5.3); for OpenXR, an OpenXR runtime usable from a 32-bit process (09 §1). Neither is needed for runtime None |
| Anything else | Nothing: no installer, no Visual C++ redistributable, no registry change, no administrator rights, no files of any other mod, no extra DLL (§7) |

There is no installation step beyond copying the file. Everything else the
mod needs is created on first use (§3).

## 2. Directory layout after use

```
<game directory>\
    AI.exe                      the game (never modified on disk)
    d3d11.dll                   the mod (installed by the player)
    mothervr.json               settings (06 §3)
    mothervr.log                log (§4)
    openvr_api.dll              only once OpenVR has been started (§3.2)
    mothervr_input\             only once OpenVR has been started (§3.3)
        <action manifest and default-bindings files>
    DATA\
        UI.PAK                  the game's own archive (never modified)
        MOTHER.PAK              the UI archive with the VR page (14)
        <companion files of MOTHER.PAK that 14 §5 defines>
```

The names `mothervr.json`, `mothervr.log`, `openvr_api.dll`,
`mothervr_input` and `DATA\MOTHER.PAK` are fixed for compatibility: players
already have files and folders with these names in their game directory,
and player guides and support requests refer to them [sure]. The names of
the files inside `mothervr_input` are the implementation's choice (SteamVR
keeps a player's own bindings per application, not per file name). The mod
never writes outside the game directory, never writes to `AI.exe` or any
stock game file, and never writes to the registry. The only process-wide
change outside files is the `XR_RUNTIME_JSON` environment variable of the
game's own process, which 09 §1 may set.

## 3. Files the mod writes

### 3.1 Summary

| File | Written when | Thread | Condition | If writing fails |
|---|---|---|---|---|
| `mothervr.log` | from attach to exit, appended (§4) | any | always (if the file can be opened) | log events go to the debugger output instead (§3.6); the game is unaffected |
| `mothervr.json` | first launch: after the welcome message is closed. Afterwards: after every settings command from the VR page (06 §4) | any thread, after the welcome message is closed (first launch); the thread that ran the command (afterwards) | §3.4 | the game is unaffected; the setting holds for this session only |
| `DATA\MOTHER.PAK` and its companion files | at attach, before the game opens its UI archive, when absent or stale (14 §5) | the thread running attach (the game opens its UI archive during start-up, so no later point is early enough, 14 §5.1) | build recognised and preflight passed (01 §2.4); also for runtime None | the options page is missing this session; VR works; the message of §5.3 as 14 §5.9 defines |
| `openvr_api.dll` | when OpenVR starts, before it is loaded (01 §5.3, 03 §6.4) | game thread | OpenVR chosen for this run (configured, forced, or reached by fallback), and the file is absent or not the copy this DLL ships | logged; an existing copy in the game directory is used, else the runtime's own `bin\win32\openvr_api.dll` |
| files in `mothervr_input\` | every time OpenVR starts and `IVRInput` is available | game thread | OpenVR started | logged; SteamVR Input is not used and the legacy controller path of 04 §5.8 applies |
| diagnostic texture dumps | only on the developer key while `DebugOptions` is on and reflections render per eye (§3.5) | implementation's choice | diagnostic, optional (§3.5) | logged; nothing else changes |

Nothing is written at attach except the log and the UI archive: work under
the loader lock must stay short and must not load DLLs or wait on other
threads (01 §2.2), and these two cannot wait (the log must capture attach;
the game opens its UI archive during start-up).

### 3.2 `openvr_api.dll`

- The DLL embeds an x86 `openvr_api.dll` from a public OpenVR SDK release
  whose loader serves the interface versions the implementation requests:
  `IVRSystem_017`, `IVRCompositor_021`, `IVROverlay_016`, or later versions
  (03 §6.4). It is a thin loader; the interfaces are served by the
  installed SteamVR. A requested interface that is not served is a start-up
  failure (§5.3).
- It is written to the game directory only when OpenVR is about to start,
  on the game thread, never at attach (writing a DLL and then loading it
  under the loader lock is a deadlock hazard). Runtime None and OpenXR never
  write it.
- After an OpenVR start, the copy in the game directory is the one this
  build of the mod embeds: it is rewritten when it is absent or is a
  different file, so a mod update replaces it exactly once. Detecting a
  stale copy must not read the whole file on every launch; how it is
  detected is the implementation's choice.
- A partly written file is never taken as current by a later launch, and a
  failed write leaves no partial file behind.
- Load preference: the copy in the game directory if it exists, else
  `<SteamVR>\bin\win32\openvr_api.dll` (01 §5.3, 03 §6.4).
- If the file is in use (another game process has it loaded), the write
  fails, is logged, and the existing copy is used.

### 3.3 `mothervr_input\`

- Created (with parent directories) when missing. It holds the SteamVR
  Input action manifest and one default-bindings file per controller type
  of 04 §5.8 (`knuckles`, `oculus_touch`, `holographic_controller`,
  `vive_controller`); the file names are the implementation's choice. Their
  content is defined in 04 §5.8.
- The files are overwritten each time OpenVR starts, so a mod update always
  installs its current defaults; the player's own rebindings live in
  SteamVR, not here.
- The manifest path is passed to `IVRInput::SetActionManifestPath` as an
  absolute UTF-8 path. If any file cannot be written or the call fails, the
  failure is logged and the legacy input path applies (04 §5.8).
- Uninstalling removes the whole folder (§6), whatever files it holds.

### 3.4 `mothervr.json`

Format, defaults, compatibility and the settings commands are in 06 §3 and
§4. Distribution rules:

- **First launch** is "no `mothervr.json` in the game directory" (a file
  that exists but does not parse is not a first launch). On first launch
  the welcome message is shown (§5.3), and the file is written with the
  defaults only after the player closes it. If the process ends before
  that (Steam restarting the game, 01 §2.1), no file is written and the next
  process shows the welcome message again, so the player sees it once.
  Showing it once is a result of this rule, not something the message
  says.
- **Not rewritten at start-up.** Reading the file never writes it back: a
  file the player edited, or one that does not parse, is not replaced or
  normalised until the player changes a setting (06 §3.1).
- **Rewrites** happen after every settings command from the VR page, whole
  file, keeping keys the mod does not know (06 §3.2). A command-line
  override (01 §5.1) is never written.
- No other event writes the file (not exit, not a runtime failure).

### 3.5 Diagnostic texture dumps (optional)

A developer aid of 07 and 11: while `DebugOptions` is on and reflections
render per eye in stereo, a developer key writes the render target of each
eye's reflection pass to a DDS file in the game directory (not the
process's current directory). The file names are the implementation's
choice. An implementation may omit the feature; if it has it, the player
guide's uninstall section lists the files.

### 3.6 Unwritable game directory

A game directory the process cannot write to (a protected install
location, a read-only folder, a full disk, access denied) must not stop the
game. The mod never needs administrator rights, a registry change or a
renamed folder to work. Each artefact degrades on its own:

| Cannot be written | Result |
|---|---|
| `mothervr.log` | log events go to the debugger output (`OutputDebugString`); nothing else changes |
| `DATA\MOTHER.PAK` or its companion files | the game opens its stock `UI.PAK`: no VR options page, settings only through `mothervr.json` and the command line; VR works; the player is told as 14 §5.9 defines (§5.3) |
| `openvr_api.dll` | an existing copy in the game directory is used, else the runtime's own loader (§3.2) |
| `mothervr_input\` | the legacy controller path of 04 §5.8 |
| `mothervr.json` | settings hold for this session only (06 §3.1). On a first launch the file can then never be written, so the welcome message appears at every launch; that is accepted |

Every failed write is logged with its Windows error where a log exists.

## 4. The log file

`mothervr.log` in the game directory. Its line format, levels, wording and
rotation are defined in `11-diagnostics.md`; this section only fixes where
it is and how it behaves as a file.

- **Opened at attach** before anything that can fail, kept open for the
  process, in append mode, shared for read, write and delete, so the player
  can open or copy it while the game runs. If it cannot be opened, events
  go to the debugger output (§3.6).
- **Never truncated.** Starting the game outside Steam runs the proxy in two
  processes one after the other, and both write to the same file (01 §2.1).
  Truncating at attach would erase the first process's lines. Each line
  must reach the file in a single append so lines of the two processes
  never interleave within a line, and every line must identify its process.
- Paths are logged as UTF-8: an install path or a user name with
  characters outside the ANSI code page must not fail or throw.
- **Events logged** (the documents of each feature add their own, 11 §2):
  the start and end of each session; the game directory; the settings read
  and any command-line override; the build found or the unknown timestamp;
  each failing preflight check and the overall result; each group of
  patches applied or skipped, with the reason (01 §2.6); the UI archive
  produced, reused or unavailable (14); each file of §3 written or found
  current, and each write failure with the Windows error; the runtime probe
  results and choice (01 §5.2); the runtime start or its failure with the
  runtime's error; every message shown to the player and what it conveyed.
- **Rate:** no logging per frame on the render thread; a repeating
  condition is logged as 11 allows.
- The file is rotated as 11 §1 specifies, so it does not grow without
  bound.

## 5. Messages to the player

### 5.1 Common rules

- Standard Windows message boxes that tell the player something. None asks
  the player to decide anything; closing the box is the only action. The
  caption names the mod. Icon, buttons, wording and the format of numbers
  are the implementation's own.
- §5.3 gives, for each message, its trigger and the facts it must convey,
  in no particular order. The implementation writes its own text.
- A message appears at most once per process. How often it appears across
  launches is a behaviour given in the "Shown" column of §5.3, never part
  of the text.
- Every message shown is logged (11).
- No message box is shown under the loader lock (on the thread running
  `DllMain` during attach).

### 5.2 Timing

Messages that arise during attach (unsupported build, welcome, the
UI-archive messages):

- never shown under the loader lock;
- shown in front of the game window, not behind it;
- the game's device creation (`D3D11CreateDeviceAndSwapChain`, 01 §1.3)
  does not complete until every one of them is closed, so the game does not
  start rendering under a box; game start-up before that point may
  continue;
- the config template is written only after the welcome message is closed,
  because a process that exits first (a Steam relaunch) must leave the
  welcome for the next launch (§3.4).

How this is done (threads, queues, order among several messages) is the
implementation's choice.

Messages that arise later (runtime failure, audio redirect failure) appear
in front of the game window when the failure is detected; the game then
continues (flat after a runtime failure). An attach-time message and a
later one may be on screen at the same time; that is acceptable.

The headset-not-showing-the-game warning arises while VR is running, on
the render thread's side of the frame loop (03 §6.7, 09 §3.1). It must
not block: neither the render thread nor the game thread waits for the
player to close it, so frames keep reaching the runtime and the game keeps
running while it is open (a box shown from a thread of its own meets
this).

### 5.3 The messages

| Message | Trigger | Must convey (unordered) | Shown |
|---|---|---|---|
| Unsupported game build | the PE timestamp of `AI.exe` is not a recognised build (01 §3) | the timestamp found; that this game version is not supported, so VR is off for this run and the game runs unmodified; that updating the game or the mod may fix it | every launch with that build |
| Welcome | first launch (§3.4) | that the mod is active; that the default runtime is SteamVR (runtime OpenVR, the default of a fresh `mothervr.json`, 06 §1); how to change the runtime (OpenXR, or None for the unmodded game): on the `MotherVR` page of the game's Options menu, taking effect at the next start, or with a `-vrforce*` launch option (01 §5.1) | until a launch writes `mothervr.json` (§3.4) |
| `UI.PAK` missing | the build is supported and `DATA\UI.PAK` does not exist when the UI archive is to be produced (14 §5.9) | the full path that was expected; that this is the game's own file and the installation needs repair; that the mod's options page is not available | every launch while the file is missing (14 §5.9) |
| UI archive unusable | `DATA\UI.PAK` exists but the UI archive cannot be produced: it is not a valid archive or is damaged, it is a version the mod does not know, the mod's changes do not fit it, or the archive cannot be written (14 §5.9) | that VR works; that the mod's options page is not available; that settings can be changed in `mothervr.json`; the kind of cause (game archive not recognised, or the archive could not be written) | once per distinct `UI.PAK` (14 §5.9) |
| Runtime failure, SteamVR | OpenVR was chosen and could not start (01 §5.3, 03 §6.4): SteamVR could not be started, no headset appeared within the bounded wait (about 10 s), a requested interface is not served (§3.2), or the runtime returned an error. SteamVR not running when the game starts is not a failure: initialisation may start it, and no process check is made | that SteamVR could not be started; the runtime's own error text, or that it is unknown; the numeric error code; that VR is off and the game continues flat until it is restarted. When the code is 117 (`VRInitError_Init_AnotherAppLaunching`) also: that SteamVR's Desktop Game Theatre is the usual cause, and that Steam's game setting "Use Desktop Game Theatre while SteamVR is active" (Steam's own label) must be off | each launch on which it occurs |
| Runtime failure, OpenXR | OpenXR was chosen and failed to start, including when no session can be created with the game's device (09 §7) | that OpenXR could not be started; the failing step; the result's text, never empty (the runtime's own text when an instance exists, else the result's symbolic name from the OpenXR registry, for example `XR_ERROR_RUNTIME_UNAVAILABLE`); the numeric result; that VR is off and the game continues flat until it is restarted, and that a restart tries OpenXR again; how to go back to SteamVR: choose SteamVR (runtime OpenVR) on the `MotherVR` page of the Options menu, or start the game with `-vrforceopenvr`. When the result is `XR_ERROR_RUNTIME_UNAVAILABLE` (-51) also: that no usable 32-bit OpenXR runtime could be loaded, and that `mothervr.log` lists the runtimes that were tried | each launch on which it occurs |
| Headset not showing the game | VR started, but the runtime does not show the game in the headset: with OpenVR, about 3 s of frames after the first submission the compositor's scene application is another process (03 §6.7); with OpenXR, a running session has not reached `VISIBLE` or `FOCUSED` 10 s after it began (09 §3.1) | that the runtime (naming SteamVR, or OpenXR) is not showing the game in the headset; with OpenVR, the executable name of the process it shows instead when known; that SteamVR's Desktop Game Theatre is the usual cause; that Steam's game setting "Use Desktop Game Theatre while SteamVR is active" (Steam's own label) must be turned off in the game's Steam properties. A warning: VR stays on and the game continues | at most once per process; each launch on which it occurs. Shown without blocking (§5.2) |
| Audio redirect failed | the OpenXR runtime reports the headset's own audio output device and the mod cannot move the game's audio to it (10) | that game audio could not be moved to the headset; the error code; that audio stays on the Windows default device | each launch on which it occurs |

No message is shown when the preflight fails on a supported build, when no
runtime is installed or the forced one is missing (01 §5.3), or when the
config file cannot be read or written; those are logged.

## 6. Uninstall

Delete from the game directory:

- `d3d11.dll` (this alone restores the unmodified game: nothing else is
  loaded or read without it);
- `openvr_api.dll`, `mothervr.json`, `mothervr.log` and its rotated copy
  (11 §1), the folder `mothervr_input`, and any diagnostic dumps (§3.5);
- `DATA\MOTHER.PAK` and the companion files 14 §5 defines.

`openvr_api.dll` in the game directory is not used by the game itself, so
leaving it is harmless; the player guide lists it anyway. Steam's "Verify
integrity of game files" does not remove these files (they are not game
files) and does not need to be run.

## 7. The DLL itself

| Property | Requirement |
|---|---|
| Architecture | x86 (machine 0x14C), a DLL named `d3d11.dll` |
| Exports | exactly the 51 names of 01 §1.3 |
| Imports | system DLLs only. Never `d3d11.dll` (it would import itself), never `dxgi.dll` (not needed: DXGI is reached through the game's device), never a C or C++ runtime DLL (`msvcp*`, `vcruntime*`, `ucrtbase`, `api-ms-win-crt-*`): the C runtime is linked statically so players need no redistributable |
| Runtime dependencies | `openvr_api.dll` is never a static import; it is loaded at run time from a path decided then (§3.2). The OpenXR loader is linked statically, so no `openxr_loader.dll` is needed. JSON parsing and any hooking library are linked statically |
| Embedded resources | the `openvr_api.dll` redistributable (§3.2) and the project's own UI resources that 14 defines. Never a stock or modified game movie, archive or other game file, and nothing taken from another mod |
| Version resource | see below |

Version resource (`VS_VERSIONINFO`), shown by Explorer in the file's
Properties, Details tab, so a player can tell which release is installed:

| Field | Value |
|---|---|
| `FILEVERSION`, `PRODUCTVERSION` | MAJOR, MINOR, PATCH, 0 |
| `FILEOS` / `FILETYPE` | `VOS_NT_WINDOWS32` / `VFT_DLL` |
| String table | language 0x0409, code page 1200 (block `040904B0`) |
| `ProductName`, `CompanyName` | `OpenMotherVR` |
| `FileDescription` | the product name and that it is a VR mod for Alien: Isolation |
| `FileVersion`, `ProductVersion` | the full version string |
| `InternalName` / `OriginalFilename` | `d3d11` / `d3d11.dll` |
| `LegalCopyright` | the project's licence |

The version string is `MAJOR.MINOR.PATCH` with an optional pre-release
suffix (`-` or `+` then letters, digits and dots), for example `1.0.0` or
`1.0.0-rc.2`. Builds that are not releases use `0.0.0-dev`, optionally with
a commit suffix (`0.0.0-dev.1a2b3c4`).

## 8. Release package

A release is a zip named `OpenMotherVR-<version>.zip` whose root holds:

| File | Content |
|---|---|
| `d3d11.dll` | the mod |
| `README.txt` | the player guide (requirements, install, uninstall, controls, settings, troubleshooting: send `mothervr.log`), CR LF line ends so Notepad shows it on any Windows |
| `LICENSE.txt` | the project's licence |
| `THIRD_PARTY_LICENSES.txt` | the licence notice of every third-party library linked into the DLL and of the embedded `openvr_api.dll` |

Next to the zip, `SHA256SUMS.txt` lists the SHA-256 (lowercase hex) of the
zip and of `d3d11.dll`, one per line as `<hash>  <name>`.

The package never contains game files (`UI.PAK`, `MOTHER.PAK`, movies,
`AI.exe`, shader caches), files from other mods, or a separate
`openvr_api.dll`. The player guide must list every file of §2 and §6 and
be updated whenever a setting, a control or a written file changes. Its
troubleshooting part must tell the player to turn off Steam's "Use Desktop
Game Theatre while SteamVR is active" for the game, because with it on
SteamVR does not show the game in the headset (03 §6.7) or does not start
(error 117, 03 §6.4), and how to go back to SteamVR after OpenXR fails
(the options page or `-vrforceopenvr`, 09 §7).

## 9. Acceptance

1. **Clean install.** In a clean 2015 Steam install with only `d3d11.dll`
   added and no `mothervr.json`: the welcome message appears once (§5.3),
   the game does not render until it is closed, and after closing it
   `mothervr.json` exists with the defaults of 06 §1. The next launch shows
   no welcome message.
2. **Steam restart.** Starting `AI.exe` directly: one log contains two
   sessions with different process ids; the welcome message is shown once in
   total, and `mothervr.json` exists afterwards.
3. **No early file work.** With the runtime OpenXR or None, no
   `openvr_api.dll` or `mothervr_input` is created. With OpenVR, both appear
   only after the game window opens (log order: the attach lines, then the
   OpenVR start, then the file writes).
4. **openvr_api.dll update.** Replace `openvr_api.dll` with a different
   file: the next OpenVR start replaces it and logs the write; a following
   start does not rewrite it.
5. **Config not rewritten.** Add an unknown key and change the indentation
   of `mothervr.json` by hand, then start and quit without changing a
   setting: the file is byte for byte unchanged.
6. **Log sharing.** While the game runs, `mothervr.log` can be opened in
   Notepad and copied; after two launches it holds both sessions.
7. **Non-ASCII path.** With the game in a folder whose name contains
   characters outside the ANSI code page: the game starts, the log shows the
   path correctly as UTF-8, and SteamVR Input bindings load.
8. **UI.PAK missing.** With `DATA\UI.PAK` renamed (test only): one message
   naming the path at every launch; the game runs without the VR page; VR
   still starts.
9. **UI archive unusable.** With a `UI.PAK` the mod does not know (14 §6):
   one message on the first launch with that file, none on the next; VR
   works without the VR page.
10. **SteamVR not running.** With SteamVR closed and runtime OpenVR: the
    game starts SteamVR and VR works, with no message.
11. **Runtime failure message.** With SteamVR unable to start (for example
    no headset connected): one error message naming SteamVR with an error
    code; after it closes the game runs flat; no further message on a
    display reset. With OpenXR forced and no usable 32-bit OpenXR runtime
    reachable: one message whose result text is not empty, which names the
    way back to SteamVR (the options page or `-vrforceopenvr`), and, for
    -51, says that no usable 32-bit runtime could be loaded and that the
    log lists what was tried.
12. **Unwritable directory.** With the game directory and `DATA` made
    read-only for the user, and no files of §2 present: the game starts and
    runs; no elevation is requested; log events appear in a debugger output
    viewer; VR works through the runtime's own `openvr_api.dll`; the options
    page is missing and the player is told (§5.3).
13. **Uninstall.** Deleting the files of §6 leaves the directory as Steam
    installed it, and the game runs unmodified.
14. **DLL properties.** `dumpbin /headers /dependents /exports d3d11.dll`
    shows machine x86, no `d3d11.dll`, `dxgi.dll` or C runtime import, and
    the 51 exports; Explorer's Details tab shows the version of the release.
15. **Package.** The release zip has exactly the four files of §8, and the
    hashes in `SHA256SUMS.txt` match.
16. **Headset not showing the game.** With Steam's "Use Desktop Game
    Theatre while SteamVR is active" on for the game and runtime OpenVR:
    within a few seconds of the first frames one warning appears, naming
    Desktop Game Theatre and the Steam setting; while it is open the
    mirror keeps updating and the game keeps running. With the setting
    off, no warning.

## 10. Open questions

None. Log growth is settled by 11 §1.
