# OpenMotherVR-Provenance: clean
# Deploy the rebuilt d3d11.dll, launch Alien: Isolation through Steam, wait,
# then report whether it is still running, the new mothervr.log lines and any
# crash recorded in the Application event log (with the faulting AI.exe RVA
# disassembled when the fault is inside AI.exe).
#
#   pwsh scripts/run_game.ps1 [-Seconds 60] [-Keep] [-NoDeploy] [-Dll <path>]
#   pwsh scripts/run_game.ps1 -Tracy [-Baseline <old.tracy>] [-TraceOut <file>]
#
# A running AI.exe is closed first. -Keep leaves the game running afterwards.
# The DLL's .pdb is deployed beside it when there is one.
#
# -Tracy deploys the traced flavour (build-tracy, MOTHERVR_TRACY=ON; see
# "Profiling" in BUILDING.md), records the session with tracy-capture into
# traces/<time>.tracy and prints scripts/tracy_summary.py's tables, against
# -Baseline when given. The Tracy tools come from -TracyDir or $env:TRACY_DIR.
# Without -Tracy a traced DLL is refused: it is never what a plain run means.

param(
    [int]$Seconds = 60,
    [switch]$Keep,
    [switch]$NoDeploy,
    [string]$Dll = '',
    [switch]$Tracy,
    [string]$Baseline = '',
    [string]$TraceOut = '',
    [string]$TracyDir = $env:TRACY_DIR,
    [string]$GameDir = 'D:\SteamLibrary\steamapps\common\Alien Isolation'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$log  = Join-Path $GameDir 'mothervr.log'

# The build's DLL: the Visual Studio layout (<dir>\Release) or Ninja's (<dir>).
if (-not $Dll) {
    $dir = Join-Path $repo $(if ($Tracy) { 'build-tracy' } else { 'build' })
    $Dll = @((Join-Path $dir 'Release\d3d11.dll'), (Join-Path $dir 'd3d11.dll')) |
        Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $Dll -and -not $NoDeploy) { throw "no d3d11.dll in ${dir}: build it first" }
}

# The traced flavour carries the Tracy client; scripts/check_dll.ps1 keeps
# the name out of every other build, so it tells the two apart.
function Test-Traced([string]$path) {
    [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($path)).Contains('Tracy')
}
if (-not $NoDeploy) {
    $traced = Test-Traced $Dll
    if ($Tracy -and -not $traced) { throw "$Dll is not a traced build (configure with -DMOTHERVR_TRACY=ON)" }
    if (-not $Tracy -and $traced) { throw "$Dll is a traced build; pass -Tracy to run it" }
}

$capture = $null
if ($Tracy) {
    $exe = if ($TracyDir) { Join-Path $TracyDir 'tracy-capture.exe' } else { (Get-Command tracy-capture.exe -ErrorAction SilentlyContinue).Source }
    if (-not $exe -or -not (Test-Path $exe)) { throw 'tracy-capture.exe not found: pass -TracyDir or set TRACY_DIR' }
    if (-not $TraceOut) {
        $TraceOut = Join-Path $repo ('traces' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.tracy')
    }
    New-Item -ItemType Directory -Force (Split-Path $TraceOut -Parent) | Out-Null
}

function Stop-Game {
    $p = Get-Process AI -ErrorAction SilentlyContinue
    if (-not $p) { return }
    $p.CloseMainWindow() | Out-Null
    if (-not $p.WaitForExit(10000)) { $p.Kill(); $p.WaitForExit(5000) | Out-Null }
}

Stop-Game
if (-not $NoDeploy) {
    Copy-Item $Dll (Join-Path $GameDir 'd3d11.dll') -Force
    $pdb = [IO.Path]::ChangeExtension($Dll, '.pdb')
    if (Test-Path $pdb) { Copy-Item $pdb (Join-Path $GameDir 'd3d11.pdb') -Force }
}
if ($Tracy) {
    # It waits for the client, which starts with the game's D3D11 device,
    # and records for -Seconds from there (localhost only, like the client).
    $capture = Start-Process -FilePath $exe -PassThru -NoNewWindow `
        -ArgumentList @('-o', "`"$TraceOut`"", '-a', '127.0.0.1', '-f', '-s', $Seconds)
}

$logLines = if (Test-Path $log) { (Get-Content $log).Count } else { 0 }
$started  = Get-Date
Start-Process 'steam://rungameid/214490'

# Wait for the process to appear, then for it to exit or the time to run out.
$deadline = $started.AddSeconds($Seconds)
$proc = $null
while ((Get-Date) -lt $deadline) {
    $proc = Get-Process AI -ErrorAction SilentlyContinue
    if ($proc) { break }
    Start-Sleep -Milliseconds 500
}
while ($proc -and -not $proc.HasExited -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
}

$running = $proc -and -not $proc.HasExited
Write-Host ("== AI.exe: " + $(if (-not $proc) { 'never started' } elseif ($running) { 'running' } else { "exited at $($proc.ExitTime)" }))

Write-Host '== mothervr.log (new lines, frame counters omitted)'
if (Test-Path $log) {
    Get-Content $log | Select-Object -Skip $logLines | Where-Object { $_ -notmatch 'present #' }
}

$crashes = Get-WinEvent -FilterHashtable @{ LogName = 'Application'; StartTime = $started } -ErrorAction SilentlyContinue |
    Where-Object { $_.Id -in 1000, 1001, 1026 -and $_.Message -match 'AI\.exe' }
foreach ($c in $crashes) {
    Write-Host "== event $($c.Id) at $($c.TimeCreated)"
    $c.Message -split "`n" | Where-Object { $_ -match 'module name|Exception code|Fault offset' } | ForEach-Object { '  ' + $_.Trim() }
    if ($c.Message -match 'Faulting module name: AI\.exe' -and $c.Message -match 'Fault offset: 0x([0-9a-fA-F]+)') {
        $rva = [Convert]::ToInt32($Matches[1], 16)
        Write-Host '  -- AI.exe at fault (RVA):'
        python -I (Join-Path $PSScriptRoot 'disasm.py') (Join-Path $GameDir 'AI.exe') ('{0:x}' -f ($rva - 0x20)) 0x30 --rva |
            ForEach-Object { '  ' + $_ }
    }
}

if (-not $Keep) { Stop-Game }

if ($capture) {
    # The capture ends on its time limit or when the game closes.
    if (-not $capture.WaitForExit(($Seconds + 30) * 1000)) {
        $capture.Kill(); Write-Host '== tracy-capture did not finish; stopped'
    }
    if (Test-Path $TraceOut) {
        Write-Host "== trace: $TraceOut"
        $sumArgs = @((Join-Path $PSScriptRoot 'tracy_summary.py'), $TraceOut)
        if ($TracyDir) { $sumArgs += @('--tools', $TracyDir) }
        if ($Baseline) { $sumArgs += @('--baseline', $Baseline) }
        python -I @sumArgs
    } else {
        Write-Host '== no trace recorded (the client never connected?)'
    }
}
