# Deploy the rebuilt d3d11.dll, launch Alien: Isolation through Steam, wait,
# then report whether it is still running, the new mothervr.log lines and any
# crash recorded in the Application event log (with the faulting AI.exe RVA
# disassembled when the fault is inside AI.exe).
#
#   pwsh scripts/run_game.ps1 [-Seconds 60] [-Keep] [-NoDeploy]
#
# A running AI.exe is closed first. -Keep leaves the game running afterwards.

param(
    [int]$Seconds = 60,
    [switch]$Keep,
    [switch]$NoDeploy,
    [string]$GameDir = 'D:\SteamLibrary\steamapps\common\Alien Isolation'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$dll  = Join-Path $repo 'build\Release\d3d11.dll'
$log  = Join-Path $GameDir 'mothervr.log'

function Stop-Game {
    $p = Get-Process AI -ErrorAction SilentlyContinue
    if (-not $p) { return }
    $p.CloseMainWindow() | Out-Null
    if (-not $p.WaitForExit(10000)) { $p.Kill(); $p.WaitForExit(5000) | Out-Null }
}

Stop-Game
if (-not $NoDeploy) { Copy-Item $dll (Join-Path $GameDir 'd3d11.dll') -Force }

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
