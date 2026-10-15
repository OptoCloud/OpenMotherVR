<!-- Provenance: in-game observation on the 2015 Steam build with SteamVR and a Valve Index (files that appear next to the game and when, the log shared by the two processes of a Steam relaunch, message boxes and what the game does while they are open); Windows documentation (DllMain and the loader lock, file sharing modes, version resources, PE headers and imports); OpenVR public documentation (`VR_InitInternal2` error codes including 117, the x86 `openvr_api.dll` redistributable of the public SDK, SteamVR Input action manifests); the requirements of 03 §6.4, 06 §3, 10, 11, 12 and 14 that this document gathers from the player's side; the project's own release packaging and player guide; the GPL-3.0 licence's terms for conveying object code; OpenXR loader documentation (`XR_KHR_loader_init`, `XR_EXT_loader_init_properties`); Semantic Versioning (pre-release and build-metadata suffixes); user decisions (default runtime OpenVR in a fresh `mothervr.json`, no game files shipped, UI archive built as in 14-ui-resources.md, the mod never needs elevation); a tester's report on SteamVR with a Steam Frame headset (an OpenXR failure message with an empty result text and no way back to SteamVR; the game not shown in the headset with Desktop Game Theatre on); project decisions of spec r6, each with its reason inline (the OpenXR failure message's added facts; the non-blocking headset-not-showing-the-game warning); project decisions of spec r7, each with its reason inline (required and optional OpenVR interfaces; the settings file never left partly written; the welcome only on a run where the mod is active; an attach-time message that cannot be shown; no message in a process that is not the game; the message caption; failure messages when the runtime returned no error; the build's checks of the DLL, the embedded loader, the version string and the shaders; symbol files; the statically linked OpenXR loader's capabilities; the release process, its integrity and the corresponding source; the player guide's report instructions and file list); r5 aligns this document with 01 r6, 03 r6, 07 r4, 09 r5 and 14 r3 (the test for a process that is not the game, the bounded wait being for the compositor, the two reflection passes, the content of the -51 message, the UI archive's key file and temporary files, the mod's shaders left alone by the dither fix, the OpenXR environment set only for a start); r6, under the clean-room process r3 (what a player sees may be stated exactly): every message box given exactly in §5.3 (caption, icon, one OK button, text and the filling of its placeholders), with the earlier mod's text where this project's behaviour is unchanged (unsupported build, SteamVR failure, audio failure) and this project's text, with its reason, where it changed or the earlier mod had none (welcome, UI.PAK missing, UI archive unusable, OpenXR failure, the two headset-not-showing-the-game warnings; the texts of 03 §6.4 and §6.7, 09 §3.1 and §7 and 10 §3.2 repeated word for word); project decision: the caption is `MotherVR` without a version, replacing the spec r7 caption (the product name), with its reason in §5.1; the earlier mod's messages that v1 does not show; the r4 decision that message wording is the implementation's own is withdrawn; the log records each message's exact text; the player guide names settings by their labels; acceptance checks 1, 8, 9, 11, 16, 22 and 23 check the exact texts. Section numbers are unchanged from r5. Revision: r6 -->

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
        <key file of MOTHER.PAK, 14 §5.1; name of the implementation's choosing>
```

The names `mothervr.json`, `mothervr.log`, `openvr_api.dll`,
`mothervr_input` and `DATA\MOTHER.PAK` are fixed for compatibility: players
already have files and folders with these names in their game directory,
and player guides and support requests refer to them [sure]. The names of
the files inside `mothervr_input` are the implementation's choice (SteamVR
keeps a player's own bindings per application, not per file name), as are
the names of the UI archive's key file and of any temporary file (14 §5.1,
§5.8: nothing outside the mod depends on them). The mod
never writes outside the game directory, never writes to `AI.exe` or any
stock game file, and never writes to the registry. The only process-wide
change outside files is the `XR_RUNTIME_JSON` environment variable of the
game's own process, which 09 §1 sets only when OpenXR is the runtime that
will start (probing for a runtime changes nothing, 01 §5.2).

## 3. Files the mod writes

### 3.1 Summary

| File | Written when | Thread | Condition | If writing fails |
|---|---|---|---|---|
| `mothervr.log` | from attach to exit, appended (§4) | any | always (if the file can be opened) | log events go to the debugger output instead (§3.6); the game is unaffected |
| `mothervr.json` | first launch: after the welcome message is closed. Afterwards: after every settings command from the VR page (06 §4) | any thread, after the welcome message is closed (first launch); the thread that ran the command (afterwards) | §3.4 | the game is unaffected; the setting holds for this session only; the file on disk stays the complete previous one (06 §3.1) |
| `DATA\MOTHER.PAK` and its key file | at attach, before the game opens its UI archive, when absent or stale (14 §5) | the thread running attach (the game opens its UI archive during start-up, so no later point is early enough, 14 §5.1) | build recognised and preflight passed (01 §2.4, §2.5); also for runtime None | the options page is missing this session; VR works; the message of §5.3 as 14 §5.9 defines |
| `openvr_api.dll` | when OpenVR starts, before it is loaded (01 §5.3, 03 §6.4) | game thread | OpenVR chosen for this run (configured, forced, or reached by fallback), and the file is absent or not the copy this DLL ships | logged; an existing copy in the game directory is used, else the runtime's own `bin\win32\openvr_api.dll` |
| files in `mothervr_input\` | every time OpenVR starts and `IVRInput` is available | game thread | OpenVR started | logged; SteamVR Input is not used and the legacy controller path of 04 §5.8 applies |
| diagnostic texture dumps | only on the developer key while `DebugOptions` is on and reflections render per eye (§3.5) | implementation's choice | diagnostic, optional (§3.5) | logged; nothing else changes |

Nothing is written at attach except the log and the UI archive: work under
the loader lock must stay short and must not load DLLs or wait on other
threads (01 §2.2), and these two cannot wait (the log must capture attach;
the game opens its UI archive during start-up).

### 3.2 `openvr_api.dll`

- The DLL embeds the x86 `openvr_api.dll` of one public OpenVR SDK
  release (§7 says how the build pins and checks it). It is a thin loader:
  it passes each interface request on to the installed SteamVR, which
  serves the interfaces, so a loader release only has to export the entry
  points the implementation calls; it need not match the interface
  versions requested.
- The **required** interfaces are `IVRSystem`, `IVRCompositor` and
  `IVROverlay`, at `IVRSystem_017`, `IVRCompositor_021`, `IVROverlay_016`
  or later versions (03 §6.4). One of them not served is a start-up
  failure (§5.3). `IVRInput` and `IVRApplications` are **optional**:
  without `IVRInput` the legacy controller path of 04 §5.8.3 applies,
  without `IVRApplications` the check of 03 §6.7 is skipped, and both
  absences are logged (project decision of spec r7: neither is needed to
  show the game in the headset, and refusing VR for their absence would
  stop setups that otherwise work).
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
  that exists but does not parse is not a first launch). On a first launch
  on which the mod is active (the build is recognised and the core checks
  of 01 §2.5.1 passed) the welcome message is shown (§5.3), and the file is
  written with the defaults only after the player closes it. On a run where
  the mod is not active, neither happens (06 §3.1), so the first run on
  which it is active still shows the welcome. If the process ends before
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
- **Never partly written.** After any write the file is the complete
  previous one or the complete new one, even when the process ends during
  the write (06 §3.1 gives the reason). A temporary file left next to it by
  an interrupted write is never read as settings, is replaced or removed by
  the next write, and is listed for uninstall (§6).

### 3.5 Diagnostic texture dumps (optional)

A developer aid of 07 and 11: while `DebugOptions` is on and reflections
render per eye in stereo, a developer key writes the render target of each
of the two reflection passes (07 §4.1: the pass for one eye's half is
sampled by the other eye) to a DDS file in the game directory (not the
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
| `DATA\MOTHER.PAK` or its key file | the game opens its stock `UI.PAK`: no VR options page, settings only through `mothervr.json` and the command line; VR works; the player is told as 14 §5.9 defines (§5.3) |
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
  patches applied or skipped, with the reason (01 §2.3, §2.5); the UI archive
  produced, reused or unavailable (14); each file of §3 written or found
  current, and each write failure with the Windows error; the runtime probe
  results and choice (01 §5.2); the runtime start or its failure with the
  runtime's error; every message shown to the player, with its exact text
  (11 §2.6).
- **Rate:** no logging per frame on the render thread; a repeating
  condition is logged as 11 allows.
- The file is rotated as 11 §1 specifies, so it does not grow without
  bound.

## 5. Messages to the player

### 5.1 Common rules

- Standard Windows message boxes that tell the player something. None asks
  the player to decide anything: each has one OK button, and closing the
  box is the only action.
- The caption of every box is `MotherVR`, without a version (project
  decision: it is the name players know from the earlier mod's boxes and
  from the options page, 06 §5.1, so they can tell which program speaks;
  the version is left out because the installed release is shown in the
  DLL's Details tab, §7, and in the log, 11 §1.7, and a caption naming it
  would have to change with every release). This replaces the caption
  `OpenMotherVR` of spec r7.
- The icon of each box is given in §5.3: information, warning
  (exclamation mark) or error (stop sign), the standard Windows icons.
- The text of each box is exactly the text of §5.3. Where the earlier mod
  showed a message for the same situation and this project's behaviour is
  unchanged, the text is the one its players saw; where the behaviour
  changed, or the earlier mod had no such message, the text is this
  project's, with its reason. Windows wraps a long line to the box's
  width.
- How texts are quoted here (the same in 01 §3.1, 03 §6.4 and §6.7, 09
  §3.1 and §7, 10 §3.2): one box line per quoted line; a line ending in
  `\` continues on the next box line with no blank line between; an empty
  quoted line is a blank line in the box. A code span inside a text, such
  as `MotherVR` or `mothervr.log`, marks a compatibility name and is shown
  as plain text. `<...>` is filled in at run time as the text's notes
  say.
- A message appears at most once per process. How often it appears across
  launches is given in the "Shown" column of §5.3.
- Every message shown is logged with its kind and exact text (11 §2.6).
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

If an attach-time message cannot be shown at all (Windows refuses to
create it), its text is logged with the reason, device creation does
not wait for it, and when it is the welcome, `mothervr.json` is not
written, so the welcome is tried again at the next launch (project
decision of spec r7: a message the player never saw must neither hold up
the game nor count as seen).

How this is done (threads, queues, order among several messages) is the
implementation's choice.

Messages that arise later (runtime failure, audio redirect failure) appear
in front of the game window when the failure is detected; the game then
continues (flat after a runtime failure). An attach-time message and a
later one may be on screen at the same time; that is acceptable.

The headset-not-showing-the-game warning arises while VR is running (03
§6.7, 09 §3.1). It must not hold up the game: while it is on screen,
frames keep reaching the runtime, the mirror keeps updating and the game
keeps running. How it is shown is the implementation's choice.

### 5.3 The messages

| Message | Trigger | Icon | Shown |
|---|---|---|---|
| Unsupported game build | the process is the game (the main module's file name is `AI.exe`, compared case-insensitively, 01 §3.1; see below the texts) and the PE timestamp of `AI.exe` is not a recognised build (01 §3) | warning | every launch with that build |
| Welcome | first launch (§3.4) on a run where the mod is active: the build is recognised and the core checks of 01 §2.5.1 passed (01 §2.4, 06 §3.1) | information | until a launch on which the mod is active writes `mothervr.json` (§3.4) |
| `UI.PAK` missing | the build is supported and `DATA\UI.PAK` does not exist when the UI archive is to be produced (14 §5.9) | error | every launch while the file is missing (14 §5.9) |
| UI archive unusable | `DATA\UI.PAK` exists but the UI archive cannot be produced: it is not a valid archive or is damaged, it is a version the mod does not know, the mod's changes do not fit it, or the archive cannot be written (14 §5.9) | warning | once per distinct `UI.PAK` (14 §5.9) |
| Runtime failure, SteamVR | OpenVR was chosen and could not start (01 §5.3, 03 §6.4): SteamVR could not be started, the compositor did not appear within the bounded wait (about 10 s), the loader could not be loaded or lacks an export, a required interface is not served (§3.2), or the runtime returned an error (no headset, for example, is an error the runtime returns). A headset whose pose is not yet valid when the wait ends is not a failure (03 §6.4). SteamVR not running when the game starts is not a failure: initialisation may start it, and no process check is made | error | each launch on which it occurs |
| Runtime failure, OpenXR | OpenXR was chosen and failed to start, including when no session can be created with the game's device (09 §7) | error | each launch on which it occurs |
| Headset not showing the game | VR started, but the runtime does not show the game in the headset: with OpenVR, once the runtime has had the settling time of 03 §6.7 after the first submitted frame, it reports another process as the one it shows; with OpenXR, a running session has not become visible within the bound of 09 §3.1 | warning | each launch on which it occurs; without blocking (§5.2) |
| Audio redirect failed | the OpenXR runtime reports the headset's own audio output device and the mod cannot move the game's audio to it (10 §3.2) | error | each launch on which it occurs |

The texts follow.

**Unsupported game build.** The earlier mod's text; the behaviour is
unchanged (the game starts unmodified):

> Exe check failed!\
> [0x`<timestamp>`]
>
> This check is to ensure that `MotherVR` is correctly modifying the game, and is looking in the correct locations.
>
> If Alien: Isolation is on an out-dated version, make sure to update through Steam or Epic. If a new update to the game has just been released, or this check failed for some other reason, `MotherVR` may need to be updated.
>
> `MotherVR` will now be disabled, and Alien: Isolation will startup normally.

`<timestamp>`: the PE timestamp found in `AI.exe`, in upper-case
hexadecimal digits without leading zeros, after the literal `0x` (for
example `0x54E3199E`).

**Welcome.** The project's text, based on the earlier mod's welcome:

> Welcome to `MotherVR` for Alien: Isolation!
>
> VR starts with SteamVR. To use OpenXR or turn VR off, go to Options->`MotherVR` after the game starts up. This message is only shown on the first launch of `MotherVR`.

Why this text (project decision): the first line, the way to the page
("Options->" and the page title) and the last sentence are the earlier
mod's, which players know. The earlier welcome told players to turn VR
on in the options; in v1 VR is on with SteamVR from the first launch
(06 §1, user decision r4), so the middle sentence says that and names
the two other choices of the `VR Runtime` page (06 §5.2), which take
effect at the next start, as the page's description lines say. The
`-vrforce*` launch options are in the player guide (§8), not here,
because a player reading the welcome is already in the game. The last
sentence holds as players read it: in a game folder that cannot be
written the welcome returns at every launch (§3.6), which is accepted.
The earlier welcome had OK and Cancel buttons that both just closed it;
this one has OK only, because there is nothing to decide (§5.1).

**`UI.PAK` missing.** The project's text, based on the earlier mod's
message for the same check:

> Failed to find UI.PAK!\
> [`<path>`]
>
> The `MotherVR` options page will be unavailable for this session.
>
> UI.PAK is a file of the game itself. Repair the game's installation (in Steam: Verify integrity of game files) to restore it.

`<path>`: the full path that was expected, the game directory followed by
`\DATA\UI.PAK`.

Why this text (project decision): the first two lines are the earlier
mod's. Its last line said that the game would now exit; v1 does not end
the game (14 §5.9), so the line says what the player loses instead. The
last paragraph is added because the file is the game's own, so only a
repair of the installation restores it; "Verify integrity of game files"
is Steam's own label for that action.

**UI archive unusable.** The project's text; the earlier mod had no such
message:

> Failed to create `MOTHER.PAK`!\
> [`<cause>`]
>
> The `MotherVR` options page will be unavailable. VR still works, and settings can be changed in `mothervr.json` in the game folder.

`<cause>` is one of:

| 14 §5.9 cause | `<cause>` |
|---|---|
| `UI.PAK` not an archive, damaged, of an unknown version, or the mod's changes do not fit it (`format`, `unknown_version`, `link`, `layout`) | `UI.PAK not recognised` |
| the archive or its key file cannot be written (`write`) | `cannot write <path> (error <code>)`, with the full path of the file that failed and the Windows error code in decimal |

Why this text (project decision): the first line follows the "Failed to
...!" form of the other messages and names the file players find in
`DATA`; the bracket line gives the cause for a bug report; the last line
says what still works and where the settings are, because the message is
shown only once per `UI.PAK` (14 §5.9), so it does not say "for this
session".

**Runtime failure, SteamVR.** The earlier mod's text; the behaviour is
unchanged (VR stays off for this run, the game continues flat). The texts
and the table that fills `<description>` and `<code>` are 03 §6.4's, the
same word for word. For any error other than 117:

> Failed to initialize SteamVR!\
> [`<description>` | `<code>`]
>
> Look into the above error, and try again once it has been resolved.
>
> `MotherVR` will now start with VR disabled. Restart the game to try again.

For error 117 (`VRInitError_Init_AnotherAppLaunching`):

> Failed to initialize SteamVR!\
> [`<description>` | `<code>`]
>
> Look into the above error, and try again once it has been resolved.
>
> It's possible that this error is being caused by SteamVR Desktop Game Theatre.
>
> To use `MotherVR` with SteamVR, you need to uncheck 'Use Desktop Game Theatre while SteamVR is active' in the Steam Properties for Alien: Isolation, and make sure that VR Game Theatre isn't running when you start the game.
>
> `MotherVR` will now start with VR disabled. Restart the game to try again.

`<description>` is never empty: the runtime's English description of the
error, or `unknown error` when that text is empty (project decision: an
empty bracket helps no one), or, for a failure that is not an error the
runtime returned (the loader not loaded, a loader export or a required
interface missing, no compositor within the wait), the project's
description of 03 §6.4 that names what was missing. `<code>` is a decimal
number, the OpenVR error or the Windows error code as 03 §6.4 says. A
loader that cannot be loaded is reported in this box; the earlier mod
showed a separate box for it, which v1 drops so that every SteamVR start-up
failure looks the same (03 §6.4).

**Runtime failure, OpenXR.** The project's text (09 §7, the same word for
word, with the reasons there); the earlier mod had no OpenXR message:

> Failed to initialize OpenXR!\
> [`<error>` | `<code>`]
>
> No usable 32-bit OpenXR runtime could be loaded. `mothervr.log` lists the runtimes that were tried.
>
> Look into the above error, and try again once it has been resolved.
>
> To go back to SteamVR, choose "Use SteamVR" on the `MotherVR` options page, or start the game with -vrforceopenvr.
>
> `MotherVR` will now start with VR disabled. Restart the game to try again.

The paragraph starting "No usable 32-bit OpenXR runtime" and the blank
line after it appear only for `XR_ERROR_RUNTIME_UNAVAILABLE` (-51).
`<error>` is never empty: the runtime's text for the result, or its
symbolic name when no instance exists, or `unknown error` for a value the
OpenXR registry does not define; `<code>` is the `XrResult` as a signed
decimal number (09 §7).

**Headset not showing the game, OpenVR.** The project's text (03 §6.7,
the same word for word); the earlier mod had no such check:

> SteamVR is not showing the game in the headset (it shows `<process>`).
>
> This is usually SteamVR's Desktop Game Theatre. In Steam, open the Properties of Alien: Isolation and uncheck 'Use Desktop Game Theatre while SteamVR is active', then restart SteamVR and the game.

`<process>` is the file name of the executable SteamVR shows instead,
without its folder; when it is unknown, the parenthesis and the space
before it are left out (03 §6.7).

**Headset not showing the game, OpenXR.** The project's text (09 §3.1,
the same word for word):

> The OpenXR runtime is not showing the game in the headset.
>
> With SteamVR this is usually its Desktop Game Theatre. In Steam, open the Properties of Alien: Isolation and uncheck 'Use Desktop Game Theatre while SteamVR is active', then restart SteamVR and the game.

**Audio redirect failed.** The earlier mod's text; the behaviour is
unchanged (10 §3.2, the same word for word):

> Failed to hook Audio!\
> [0x`<result>`]

`<result>`: the failing `HRESULT` in upper-case hexadecimal digits
without leading zeros, after the literal `0x` (for example `0x80070490`).
VR keeps running and the sound stays on the Windows default device.

**Messages players of the earlier mod saw that v1 does not show:** the one
for SteamVR not running (v1 makes no process check: initialisation starts
SteamVR, 01 §5.3), the separate one for a loader that could not be loaded
(part of the SteamVR failure above), the two for the Oculus runtime (v1
has no LibOVR backend; OpenXR has its own message above), and one for a
system graphics library that could not be loaded (in v1 a system
`d3d11.dll` that cannot be loaded is logged and the game's device creation
fails, 01 §1.3).

No message is shown when the preflight fails on a supported build, when no
runtime is installed or the forced one is missing (01 §5.3), or when the
config file cannot be read or written; those are logged.

No message is shown either when the DLL is loaded into a process that is
not the game: the main module's file name is not `AI.exe` (a test harness,
or another program started from the game directory; 01 §3.1). The proxy
then only forwards the `d3d11.dll` exports (01 §1.3), logs one event naming
the host, reads no settings, installs no crash filter, applies no patch and
writes no file but the log. A renamed game executable is therefore not
supported (project decision of spec r7: a modal box in a program that is
not the game blocks it for nothing, and automated load tests must run
unattended).

## 6. Uninstall

Delete from the game directory:

- `d3d11.dll` (this alone restores the unmodified game: nothing else is
  loaded or read without it);
- `openvr_api.dll`, `mothervr.json` and any temporary file an interrupted
  write of it left (§3.4), `mothervr.log` and its rotated copy (11 §1), the
  folder `mothervr_input`, and any diagnostic dumps (§3.5);
- `DATA\MOTHER.PAK`, its key file, and any temporary file an interrupted
  write of either left (14 §5.1, §5.8; their names are the
  implementation's choice, so the player guide gives them).

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
| Runtime dependencies | `openvr_api.dll` is never a static import; it is loaded at run time from a path decided then (§3.2). The OpenXR loader is linked statically, so no `openxr_loader.dll` is needed; it is a release that implements `XR_KHR_loader_init` and `XR_EXT_loader_init_properties` on Windows, so the elevated-process path of 09 §1 is always available in a release build and the fallback of 09 §1 exists for safety only. JSON parsing and any hooking library are linked statically |
| Embedded resources | the `openvr_api.dll` redistributable (§3.2) and the project's own UI resources that 14 defines. Never a stock or modified game movie, archive or other game file, and nothing taken from another mod |
| Embedded loader | the build names the public OpenVR SDK release whose x86 loader it embeds, with that file's SHA-256, and fails if the file is missing, is not an x86 PE DLL (machine 0x14C), or has another hash; it never embeds a placeholder (for example a large-file pointer left by a checkout). Updating the loader changes the release and the hash together. The licence notice of that release is the one in `THIRD_PARTY_LICENSES.txt` (§8). The interface versions requested (§3.2) are chosen independently of the loader release |
| Shaders | the mod's own shaders (the mirror of 03 §8.3 and any other the mod draws with) are compiled from the project's HLSL at build time and embedded; the DLL never loads a shader compiler at run time (project decision of spec r7: a run-time compiler adds a DLL dependency and start-up cost for shaders that never change). The build fails if one does not compile. They must not contain the dither pattern of 08 §1.1, so the shader fix of 08 §1 leaves them unchanged (03 §8.3) |
| Symbols | every release build also produces the program database (PDB) of exactly the shipped `d3d11.dll`, without changing the shipped code; the DLL's debug directory names it so a debugger finds it. It is not in the player package (§8) and is kept as §8.3 says |
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
suffix of `-` followed by letters, digits and dots, for example `1.0.0` or
`1.0.0-rc.2`. A `+` suffix is not used: in Semantic Versioning it marks
build metadata, so it would make neither a prerelease nor a distinct
release. A version with a suffix is a prerelease (§8.1). Builds that are
not releases use `0.0.0-dev`, optionally followed by `.` and a short commit
id (`0.0.0-dev.1a2b3c4`). A build given a version string of any other form
fails, rather than producing a DLL with a wrong or empty version resource.
`FILEVERSION` and `PRODUCTVERSION` carry only MAJOR, MINOR and PATCH, so a
prerelease and the final release of one version share them; the string
versions tell them apart.

**Checked by the build.** Every build that produces a release package
checks the built DLL before packaging and produces no package if any
property of this section does not hold: machine x86; exactly the 51 export
names of 01 §1.3, no more and no fewer; no import of `d3d11.dll`,
`dxgi.dll`, `openvr_api.dll`, `openxr_loader.dll` or any C or C++ runtime
DLL; imports only from the system DLLs 01 §1.1 allows; a version resource
whose numeric and string versions equal the version the build was given
(project decision of spec r7: each of these, when wrong, breaks every
player's install, and a check by hand before a release is easy to skip).

## 8. Release package

A release is a zip named `OpenMotherVR-<version>.zip` whose root holds:

| File | Content |
|---|---|
| `d3d11.dll` | the mod |
| `README.txt` | the player guide (requirements, install, uninstall, controls, settings, troubleshooting) |
| `LICENSE.txt` | the project's licence |
| `THIRD_PARTY_LICENSES.txt` | the licence notice of every third-party library whose code ends up in the DLL, including libraries linked only because another dependency needs them, and the notice of the exact OpenVR SDK release the embedded loader comes from (§7) |

All three text files use CR LF line ends, so Notepad shows them on any
Windows. Building the package fails when a notice is missing.

Next to the zip, `SHA256SUMS.txt` lists the SHA-256 (lowercase hex) of the
zip and of `d3d11.dll`, one per line as `<hash>  <name>`.

The package never contains game files (`UI.PAK`, `MOTHER.PAK`, movies,
`AI.exe`, shader caches), files from other mods, a separate
`openvr_api.dll`, or symbol files (§8.3).

The player guide:

- lists every file and folder of §2 and §6, including the rotated log,
  the key file of `MOTHER.PAK` and the temporary files an interrupted
  write can leave (settings file, UI archive), and is updated whenever a setting, a control or a written file
  changes;
- names both runtime options of §1 among its requirements, and the
  `-vrforce*` launch options of 06 §2;
- tells the player, in its troubleshooting part, to turn off Steam's "Use
  Desktop Game Theatre while SteamVR is active" for the game, because with
  it on SteamVR does not show the game in the headset (03 §6.7) or does
  not start (error 117, 03 §6.4), and how to go back to SteamVR after
  OpenXR fails (`Use SteamVR` on the `VR Runtime` page of the `MotherVR`
  options page, or `-vrforceopenvr`, 09 §7);
- names each setting by its label on the options page and the page it is
  on (06 §5.2), as the player sees them;
- says where problems are reported (the project's public issue tracker)
  and what to attach: `mothervr.log`, and `mothervr.log.1` when present
  (11 §1.3), and for a crash the Windows Event Viewer entry for `AI.exe`
  (11 §6.2);
- says where the source of the release is (§8.3).

### 8.1 Making a release

- A release is made by a tag `v<version>`, where `<version>` is a release
  version string of §7, on a commit of the main branch that passed CI.
  The release build stamps exactly that version into the DLL (§7) and the
  zip name, so the tag, the zip, Explorer's Details tab and the session
  start event of 11 §1.7 agree.
- A version with a pre-release suffix is published as a prerelease.
- Each release has player-facing release notes kept in the repository
  under a name derived from the tag and committed before the tag; a
  release whose notes are missing or empty is not published. The notes
  cover: how to install; the requirements (§1); what changed since the
  previous release, including the problems of 00 §4 it fixes; known
  issues, including features not yet confirmed in game; how to report a
  problem and what to attach (§8); how to verify the download (§8.2).
- Publishing the same tag again replaces that release's notes and assets;
  it never creates a second release.

Reason (project decision of spec r7): a player must be able to match the
file they run to a release and its notes, and a re-run after a failed
publish must not leave two releases of one version.

### 8.2 Integrity

- Release builds run on hosted CI, from the tagged commit only.
- Every third-party build step and library is pinned to an exact revision;
  updating one is a reviewed change of its own.
- Build jobs have read-only access to the repository; only the job that
  publishes the release may write to it.
- Each release publishes, besides `SHA256SUMS.txt`, a signed build
  provenance statement for the zip and for `d3d11.dll` that a player can
  check with public tools, where the hosting service offers one; the
  release notes say how.

Reason (project decision of spec r7): the DLL runs inside the game with
the player's rights, so a player must be able to check that it was built
from the published source by the published process.

### 8.3 Source and symbol files

- Each release states where the complete corresponding source of that
  exact version is: the tagged commit in the public repository, including
  the build scripts, the pinned dependency revisions and the UI resource
  sources of 14 §4. The player guide and the release notes name that
  place. Reason: the project's licence (GPL-3.0) requires it when object
  code is conveyed.
- The symbol file of each release (§7) is kept with that release for as
  long as the release is published, as a separate release asset or in an
  archive the project keeps, never inside the player zip. When published
  as an asset it is listed in `SHA256SUMS.txt`. Reason: a crash record
  gives offsets inside the proxy (11 §6.1), which can be mapped to source
  only with the matching symbol file (11 §6.2).

## 9. Acceptance

1. **Clean install.** In a clean 2015 Steam install with only `d3d11.dll`
   added and no `mothervr.json`: the welcome message (§5.3, first line
   "Welcome to `MotherVR` for Alien: Isolation!", information icon, OK
   only, caption `MotherVR`) appears once,
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
8. **UI.PAK missing.** With `DATA\UI.PAK` renamed (test only): the
   message "Failed to find UI.PAK!" of §5.3, naming the expected path, at
   every launch; the game runs without the VR page; VR
   still starts.
9. **UI archive unusable.** With a `UI.PAK` the mod does not know (14 §5.9):
   the message "Failed to create `MOTHER.PAK`!" with
   `[UI.PAK not recognised]` on the first launch with that file, none on the next; VR
   works without the VR page.
10. **SteamVR not running.** With SteamVR closed and runtime OpenVR: the
    game starts SteamVR and VR works, with no message.
11. **Runtime failure message.** With SteamVR unable to start (for example
    no headset connected): one message "Failed to initialize SteamVR!"
    (error icon) whose bracket line holds a non-empty description and a
    number; after it closes the game runs flat; no further message on a
    display reset. With OpenXR forced and no usable 32-bit OpenXR runtime
    reachable: one message "Failed to initialize OpenXR!" whose bracket
    line reads `[XR_ERROR_RUNTIME_UNAVAILABLE | -51]` or another non-empty
    result text, with the text of §5.3 word for word, including the
    paragraph on the 32-bit runtime for -51 only (09 §7).
12. **Unwritable directory.** With the game directory and `DATA` made
    read-only for the user, and no files of §2 present: the game starts and
    runs; no elevation is requested; log events appear in a debugger output
    viewer; VR works through the runtime's own `openvr_api.dll`; the options
    page is missing and the player is told (§5.3).
13. **Uninstall.** Deleting the files of §6 leaves the directory as Steam
    installed it, and the game runs unmodified.
14. **DLL properties.** The release build's own check of §7 passes, and
    `dumpbin /headers /dependents /exports d3d11.dll` agrees: machine x86,
    no `d3d11.dll`, `dxgi.dll`, `openvr_api.dll`, `openxr_loader.dll` or C
    runtime import, and the 51 exports; Explorer's Details tab shows the
    version of the release. A build of a DLL with one export removed, or
    given the version `1.0`, produces no package.
15. **Package.** The release zip has exactly the four files of §8, its
    three text files have CR LF line ends, and the hashes in
    `SHA256SUMS.txt` match.
16. **Headset not showing the game.** With Steam's "Use Desktop Game
    Theatre while SteamVR is active" on for the game and runtime OpenVR:
    within a few seconds of the first frames one warning appears (warning
    icon), with the OpenVR text of §5.3, naming the program SteamVR shows
    when it is known; while it is open the
    mirror keeps updating and the game keeps running. With the setting
    off, no warning.
17. **Embedded loader.** Replace the loader file the build embeds with a
    small text file (as a checkout without large-file support leaves it):
    the build fails and says how to obtain the file.
18. **Symbols.** For a release, the symbol file kept with it (§8.3)
    resolves a proxy offset from a crash record of 11 §6.1 to a function
    of the source at the release tag.
19. **Player guide.** The guide in the package lists every file and folder
    of §2 and §6 (including the rotated log, the key file of `MOTHER.PAK`
    and the temporary files an interrupted write can leave), names both runtime
    options of §1 and the `-vrforce*` launch options, says where to report
    a problem and what to attach, and where the source is.
20. **Release.** Pushing a tag `v1.2.3-rc.1` on the main branch with its
    notes publishes a prerelease whose zip is `OpenMotherVR-1.2.3-rc.1.zip`
    and whose DLL's Details tab shows `1.2.3-rc.1`; the same tag without
    notes publishes nothing; publishing the tag again leaves one release.
21. **Not the game.** Loading `d3d11.dll` into another 32-bit process (for
    example `rundll32`) from the game directory shows no message, writes
    no file but the log, and the log names the host.
22. **Unknown build, first launch.** With no `mothervr.json` and an
    `AI.exe` whose timestamp is not recognised (test only): the
    message "Exe check failed!" appears with that timestamp in upper-case
    hexadecimal, and no welcome; no `mothervr.json`
    is written. Restoring the real `AI.exe` then shows the welcome.
23. **SteamVR without a required interface.** With a runtime that does not
    serve one of the interfaces of §3.2 (test only): the bracket line of
    "Failed to initialize SteamVR!" names the interface version
    requested, as in `[IVRCompositor_021 not available | <code>]`
    (03 §6.4).
24. **Diagnostic dumps.** If the implementation has the dumps of §3.5:
    with the game started from a shortcut whose working directory is
    another folder, the dumps appear in the game directory.

## 10. Open questions

None. Log growth is settled by 11 §1.
