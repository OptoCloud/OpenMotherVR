# OpenMotherVR-Provenance: clean
# Checks on a built d3d11.dll before it ships (spec 13 section 7). Needs
# dumpbin on PATH (a "Native Tools Command Prompt for VS", or vcvars32.bat).
# CI runs it before packaging; a failure means no package.
#
#   pwsh scripts/check_dll.ps1 build/d3d11.dll [-Version 1.0.0]
#
# - 32-bit: the game is 32-bit and loads nothing else.
# - Exports exactly the 51 names of the system d3d11.dll (spec 01 section
#   1.3): the VR runtime's client library imports D3D11CreateDevice from
#   whatever d3d11.dll is loaded, and a missing name fails its load.
# - Imports only system DLLs every supported Windows ships. Never d3d11.dll
#   (the proxy is d3d11.dll and would import itself, AGENTS.md "Never link
#   d3d11.lib"), dxgi.dll (reached through the game's device), the OpenVR
#   or OpenXR loader (loaded at run time / linked statically), or a C or C++
#   runtime DLL (the static CRT is what lets players skip the VC++
#   redistributable).
# - With -Version: the version resource carries that version, so the zip
#   name, the file's Details tab and the log agree.

param(
    [Parameter(Mandatory)] [string]$Dll,
    [string]$Version
)

$ErrorActionPreference = 'Stop'
$failed = $false
function Fail([string]$msg) { Write-Host "::error::$msg"; $script:failed = $true }

$headers = dumpbin /nologo /headers $Dll | Out-String
if ($headers -notmatch '14C machine \(x86\)') { Fail "$Dll is not a 32-bit (x86) image" }

# ---- imports ----------------------------------------------------------------

$deps = dumpbin /nologo /dependents $Dll | Out-String
$dlls = @([regex]::Matches($deps, '(?im)^\s+(\S+\.dll)\s*$') | ForEach-Object { $_.Groups[1].Value.ToLowerInvariant() })
foreach ($banned in 'd3d11.dll', 'dxgi.dll', 'openvr_api.dll', 'openxr_loader.dll') {
    if ($dlls -contains $banned) { Fail "$Dll imports $banned" }
}
$crt = $dlls | Where-Object { $_ -match '^(msvcp|msvcr|vcruntime|ucrtbase|concrt|api-ms-win-crt-)' }
if ($crt) { Fail "$Dll depends on the C/C++ runtime: $($crt -join ', ')" }
# System DLLs present on every Windows 10 and 11 install. Extend the list
# only with another such DLL, never with a redistributable.
$system = 'kernel32.dll', 'user32.dll', 'gdi32.dll', 'advapi32.dll', 'shell32.dll', 'shlwapi.dll',
          'ole32.dll', 'oleaut32.dll', 'version.dll', 'bcrypt.dll', 'dbghelp.dll', 'winmm.dll', 'ntdll.dll'
$other = $dlls | Where-Object { $system -notcontains $_ -and $_ -notin $crt }
if ($other) { Fail "$Dll imports DLLs outside the system allow list: $($other -join ', ')" }

# ---- exports ----------------------------------------------------------------

$expected = @(
    'CreateDirect3D11DeviceFromDXGIDevice', 'CreateDirect3D11SurfaceFromDXGISurface',
    'D3D11CoreCreateDevice', 'D3D11CoreCreateLayeredDevice', 'D3D11CoreGetLayeredDeviceSize',
    'D3D11CoreRegisterLayers', 'D3D11CreateDevice', 'D3D11CreateDeviceAndSwapChain',
    'D3D11CreateDeviceForD3D12', 'D3D11On12CreateDevice', 'D3DKMTCloseAdapter',
    'D3DKMTCreateAllocation', 'D3DKMTCreateContext', 'D3DKMTCreateDevice',
    'D3DKMTCreateSynchronizationObject', 'D3DKMTDestroyAllocation', 'D3DKMTDestroyContext',
    'D3DKMTDestroyDevice', 'D3DKMTDestroySynchronizationObject', 'D3DKMTEscape',
    'D3DKMTGetContextSchedulingPriority', 'D3DKMTGetDeviceState', 'D3DKMTGetDisplayModeList',
    'D3DKMTGetMultisampleMethodList', 'D3DKMTGetRuntimeData', 'D3DKMTGetSharedPrimaryHandle',
    'D3DKMTLock', 'D3DKMTOpenAdapterFromHdc', 'D3DKMTOpenResource', 'D3DKMTPresent',
    'D3DKMTQueryAdapterInfo', 'D3DKMTQueryAllocationResidency', 'D3DKMTQueryResourceInfo',
    'D3DKMTRender', 'D3DKMTSetAllocationPriority', 'D3DKMTSetContextSchedulingPriority',
    'D3DKMTSetDisplayMode', 'D3DKMTSetDisplayPrivateDriverFormat', 'D3DKMTSetGammaRamp',
    'D3DKMTSetVidPnSourceOwner', 'D3DKMTSignalSynchronizationObject', 'D3DKMTUnlock',
    'D3DKMTWaitForSynchronizationObject', 'D3DKMTWaitForVerticalBlankEvent',
    'D3DPerformance_BeginEvent', 'D3DPerformance_EndEvent', 'D3DPerformance_GetStatus',
    'D3DPerformance_SetMarker', 'EnableFeatureLevelUpgrade', 'OpenAdapter10', 'OpenAdapter10_2'
)
if ($expected.Count -ne 51) { throw "check_dll.ps1: the expected export list has $($expected.Count) names, not 51" }

# The table between the "ordinal hint RVA name" header and "Summary":
# "ordinal hint RVA name [= symbol]"; a forwarder has no RVA.
$exportText = dumpbin /nologo /exports $Dll | Out-String
$table = if ($exportText -match '(?s)ordinal\s+hint\s+RVA\s+name(.*?)\r?\n\s*Summary') { $Matches[1] } else { '' }
$exports = @([regex]::Matches($table, '(?m)^\s+\d+\s+[0-9A-Fa-f]+\s+(?:[0-9A-Fa-f]{8}\s+)?([A-Za-z_][A-Za-z0-9_@?$]*)') |
             ForEach-Object { $_.Groups[1].Value })
$missing = $expected | Where-Object { $exports -cnotcontains $_ }
$extra   = $exports  | Where-Object { $expected -cnotcontains $_ }
if ($missing) { Fail "$Dll does not export: $($missing -join ', ')" }
if ($extra)   { Fail "$Dll exports names the system d3d11.dll does not: $($extra -join ', ')" }
if ($exports.Count -ne 51) { Fail "$Dll exports $($exports.Count) names, not 51" }

# ---- version resource --------------------------------------------------------

if ($Version) {
    $info = [Diagnostics.FileVersionInfo]::GetVersionInfo((Resolve-Path $Dll).Path)
    if ($Version -notmatch '^(\d+)\.(\d+)\.(\d+)') { Fail "-Version '$Version' is not MAJOR.MINOR.PATCH[-pre]" }
    else {
        $numeric = "$($Matches[1]).$($Matches[2]).$($Matches[3]).0"
        $fileNumeric = "$($info.FileMajorPart).$($info.FileMinorPart).$($info.FileBuildPart).$($info.FilePrivatePart)"
        $productNumeric = "$($info.ProductMajorPart).$($info.ProductMinorPart).$($info.ProductBuildPart).$($info.ProductPrivatePart)"
        if ($fileNumeric -ne $numeric) { Fail "FILEVERSION is $fileNumeric, expected $numeric" }
        if ($productNumeric -ne $numeric) { Fail "PRODUCTVERSION is $productNumeric, expected $numeric" }
    }
    if ($info.FileVersion -ne $Version) { Fail "FileVersion string is '$($info.FileVersion)', expected '$Version'" }
    if ($info.ProductVersion -ne $Version) { Fail "ProductVersion string is '$($info.ProductVersion)', expected '$Version'" }
    if ($info.ProductName -ne 'OpenMotherVR') { Fail "ProductName is '$($info.ProductName)', expected 'OpenMotherVR'" }
    if ($info.OriginalFilename -ne 'd3d11.dll') { Fail "OriginalFilename is '$($info.OriginalFilename)', expected 'd3d11.dll'" }
}

if ($failed) { exit 1 }
Write-Host "$Dll ok: x86, $($exports.Count) exports, imports $($dlls -join ', ')$(if ($Version) { ", version $Version" })"
