# Download Valve's OpenVR loader 1.0.10 (openvr_api.dll, win32), which the
# mod embeds and deploys next to the game, into res/.
#
#   pwsh scripts/fetch_openvr_api.ps1
#
# The repository also tracks the file in Git LFS; use this when the LFS copy
# is unavailable (a checkout without LFS gets a small pointer file instead).

$ErrorActionPreference = 'Stop'
$target = Join-Path (Split-Path $PSScriptRoot -Parent) 'res/openvr_api.dll'
$url = 'https://github.com/ValveSoftware/openvr/raw/v1.0.10/bin/win32/openvr_api.dll'

Invoke-WebRequest -Uri $url -OutFile $target
$hash = (Get-FileHash $target -Algorithm SHA256).Hash
Write-Host "openvr_api.dll 1.0.10 -> $target"
Write-Host "SHA-256 $hash"
# The copy MotherVR 0.8.1 embedded.
if ($hash -ne '9520D2B88A97F6390FB2D0447019FA0E85344FB1C4AE1AAA3F9D78FED0C8EDA3') {
    Write-Warning 'differs from the copy MotherVR 0.8.1 shipped'
}
