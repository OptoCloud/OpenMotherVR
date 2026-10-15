# Download Valve's OpenVR loader (openvr_api.dll, win32), which the mod
# embeds and deploys next to the game, into res/.
#
#   pwsh scripts/fetch_openvr_api.ps1
#
# The repository also tracks the file in Git LFS; use this when the LFS copy
# is unavailable (a checkout without LFS gets a small pointer file instead).
#
# The SDK release and the file's SHA-256 are pinned in CMakeLists.txt
# (OPENVR_API_RELEASE, OPENVR_API_SHA256), which also refuses to configure
# with any other file. A download that does not match is discarded, so a
# bad copy never replaces res/openvr_api.dll.

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$cmake = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw
if ($cmake -notmatch 'set\(OPENVR_API_RELEASE "([^"]+)"\)') { throw 'OPENVR_API_RELEASE not found in CMakeLists.txt' }
$release = $Matches[1]
if ($cmake -notmatch 'set\(OPENVR_API_SHA256 "([0-9a-fA-F]{64})"\)') { throw 'OPENVR_API_SHA256 not found in CMakeLists.txt' }
$expected = $Matches[1].ToUpperInvariant()

$target = Join-Path $root 'res/openvr_api.dll'
$url = "https://github.com/ValveSoftware/openvr/raw/v$release/bin/win32/openvr_api.dll"
$tmp = "$target.download"

try {
    Invoke-WebRequest -Uri $url -OutFile $tmp
    $hash = (Get-FileHash $tmp -Algorithm SHA256).Hash
    if ($hash -ne $expected) {
        throw "openvr_api.dll from $url has SHA-256 $hash, expected $expected (CMakeLists.txt); not used"
    }
    Move-Item -Force $tmp $target
} finally {
    if (Test-Path $tmp) { Remove-Item -Force $tmp }
}
# The same file MotherVR 0.8.1 embedded.
Write-Host "openvr_api.dll $release -> $target"
Write-Host "SHA-256 $hash (matches CMakeLists.txt)"
