# Sanity checks on a built d3d11.dll before it ships. Needs dumpbin on PATH
# (a "Native Tools Command Prompt for VS", or vcvars32.bat).
#
#   pwsh scripts/check_dll.ps1 build/d3d11.dll
#
# - 32-bit: the game is 32-bit and loads nothing else.
# - Does not import d3d11.dll: the proxy is d3d11.dll and would import itself
#   (AGENTS.md, "Never link d3d11.lib").
# - Imports no MSVC runtime DLL: the static CRT is what lets players skip the
#   VC++ redistributable.
# - Exports the D3D11 entry points the game calls.

param([Parameter(Mandatory)] [string]$Dll)

$ErrorActionPreference = 'Stop'
$failed = $false
function Fail([string]$msg) { Write-Host "::error::$msg"; $script:failed = $true }

$headers = dumpbin /nologo /headers $Dll | Out-String
if ($headers -notmatch '14C machine \(x86\)') { Fail "$Dll is not a 32-bit (x86) image" }

$deps = dumpbin /nologo /dependents $Dll | Out-String
$dlls = [regex]::Matches($deps, '(?im)^\s+(\S+\.dll)\s*$') | ForEach-Object { $_.Groups[1].Value.ToLowerInvariant() }
if ($dlls -contains 'd3d11.dll') { Fail "$Dll imports d3d11.dll (itself)" }
$crt = $dlls | Where-Object { $_ -match '^(msvcp|vcruntime|ucrtbase|api-ms-win-crt-)' }
if ($crt) { Fail "$Dll depends on the MSVC runtime: $($crt -join ', ')" }

$exports = dumpbin /nologo /exports $Dll | Out-String
foreach ($fn in 'D3D11CreateDevice', 'D3D11CreateDeviceAndSwapChain') {
    if ($exports -notmatch "\b$fn\b") { Fail "$Dll does not export $fn" }
}

if ($failed) { exit 1 }
Write-Host "$Dll ok: x86, imports $($dlls -join ', ')"
