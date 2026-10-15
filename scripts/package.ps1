# OpenMotherVR-Provenance: clean
# Build the player-facing release folder and zip from a built d3d11.dll.
#
#   pwsh scripts/package.ps1 -Dll build/d3d11.dll -Version 1.0.0 [-OutDir out]
#
# Writes <OutDir>/OpenMotherVR-<Version>/ (d3d11.dll, README.txt, LICENSE.txt,
# THIRD_PARTY_LICENSES.txt), <OutDir>/OpenMotherVR-<Version>.zip and
# <OutDir>/SHA256SUMS.txt. The third-party notices come from the vcpkg ports
# linked into the DLL; the OpenVR notice also covers the embedded
# openvr_api.dll.
#
# The DLL's symbol file (d3d11.pdb, written next to it by a Release build)
# goes to <OutDir>/OpenMotherVR-<Version>-symbols/ and
# <OutDir>/OpenMotherVR-<Version>-symbols.zip, never into the player zip: it
# is kept with the release so a crash offset inside the proxy (mothervr.log)
# can be mapped to source. -RequireSymbols makes a missing PDB an error.

param(
    [Parameter(Mandatory)] [string]$Dll,
    [Parameter(Mandatory)] [string]$Version,
    [string]$OutDir = 'out',
    # vcpkg manifest-mode install tree. Default: next to the DLL (Ninja) or
    # one level up (Visual Studio generator, build/Release/d3d11.dll).
    [string]$VcpkgInstalled,
    # Default: d3d11.pdb next to the DLL.
    [string]$Pdb,
    [switch]$RequireSymbols
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$dllDir = Split-Path (Resolve-Path $Dll) -Parent
if (-not $VcpkgInstalled) {
    $VcpkgInstalled = Join-Path $dllDir 'vcpkg_installed/x86-windows-mothervr'
    if (-not (Test-Path $VcpkgInstalled)) {
        $VcpkgInstalled = Join-Path (Split-Path $dllDir -Parent) 'vcpkg_installed/x86-windows-mothervr'
    }
}
if (-not (Test-Path $VcpkgInstalled)) { throw "vcpkg install tree not found: $VcpkgInstalled" }

$name = "OpenMotherVR-$Version"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$dir = Join-Path $OutDir $name
if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
New-Item -ItemType Directory -Force $dir | Out-Null

Copy-Item $Dll (Join-Path $dir 'd3d11.dll')
# CRLF so Notepad shows the lines on any Windows version.
foreach ($pair in @(@('packaging/README.txt', 'README.txt'), @('LICENSE', 'LICENSE.txt'))) {
    $text = (Get-Content (Join-Path $root $pair[0]) -Raw) -replace "`r?`n", "`r`n"
    [IO.File]::WriteAllText((Join-Path $dir $pair[1]), $text)
}

# Only the ports whose code ends up in the DLL; the rest of the tree is build
# tooling and header registries.
$ports = 'minhook', 'nlohmann-json', 'openvr', 'openxr-loader', 'jsoncpp'
$notices = foreach ($port in $ports) {
    $file = Join-Path $VcpkgInstalled "share/$port/copyright"
    if (-not (Test-Path $file)) { throw "missing license file for $port ($file)" }
    "==== $port " + ('=' * (70 - $port.Length))
    ''
    (Get-Content $file -Raw).TrimEnd()
    ''
}
[IO.File]::WriteAllText((Join-Path $dir 'THIRD_PARTY_LICENSES.txt'), ($notices -join "`r`n") + "`r`n")

$zip = Join-Path $OutDir "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $dir '*') -DestinationPath $zip

$sums = @(
    "$((Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant())  $name.zip"
    "$((Get-FileHash (Join-Path $dir 'd3d11.dll') -Algorithm SHA256).Hash.ToLowerInvariant())  d3d11.dll"
)

if (-not $Pdb) { $Pdb = Join-Path $dllDir 'd3d11.pdb' }
$symbols = "$name-symbols"
if (Test-Path $Pdb) {
    $symDir = Join-Path $OutDir $symbols
    if (Test-Path $symDir) { Remove-Item -Recurse -Force $symDir }
    New-Item -ItemType Directory -Force $symDir | Out-Null
    Copy-Item $Pdb (Join-Path $symDir 'd3d11.pdb')
    $symZip = Join-Path $OutDir "$symbols.zip"
    if (Test-Path $symZip) { Remove-Item -Force $symZip }
    Compress-Archive -Path (Join-Path $symDir '*') -DestinationPath $symZip
    $sums += "$((Get-FileHash $symZip -Algorithm SHA256).Hash.ToLowerInvariant())  $symbols.zip"
} elseif ($RequireSymbols) {
    throw "symbol file not found: $Pdb (a Release build writes d3d11.pdb next to d3d11.dll)"
} else {
    Write-Warning "no symbol file at $Pdb; packaged without symbols"
    $symbols = ''
}
[IO.File]::WriteAllText((Join-Path $OutDir 'SHA256SUMS.txt'), ($sums -join "`n") + "`n")

Write-Host "packaged $zip"
$sums | ForEach-Object { Write-Host $_ }
if ($env:GITHUB_OUTPUT) {
    "name=$name" >> $env:GITHUB_OUTPUT
    "symbols=$symbols" >> $env:GITHUB_OUTPUT
}
