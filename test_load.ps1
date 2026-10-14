# Test-load the built d3d11.dll and report what happens.
# This is a 32-bit DLL so we must use a 32-bit host. We invoke rundll32 from
# SysWOW64 (the 32-bit version), which will LoadLibrary our DLL and then call
# DllMain. If the DLL crashes during load we'll see it here without involving
# Alien: Isolation at all. (The host is not AI.exe, so the exe check fails and
# no hooks are installed; the "Exe check failed" box may appear.)

param([string]$Dll = (Join-Path $PSScriptRoot "build\Release\d3d11.dll"))

$rundll32 = "C:\Windows\SysWOW64\rundll32.exe"

Write-Host "Test-loading: $Dll"
Write-Host "Using 32-bit rundll32: $rundll32"
Write-Host ""

# rundll32 expects: rundll32 <path-to-dll>,<entry-point>
# Our DLL has no exposed entry point that rundll32 can call meaningfully,
# but rundll32 will still LoadLibrary the DLL first, which is the part we
# care about. If LoadLibrary fails, rundll32 prints an error.
$out = & $rundll32 "$Dll,nonexistent" 2>&1
Write-Host "Exit code: $LASTEXITCODE"
Write-Host "Output: $out"
