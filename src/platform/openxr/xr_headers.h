#pragma once

// OpenXR with the Win32 + D3D11 platform bindings. Include this instead of
// <openxr/openxr.h> so every translation unit sees the same configuration.
//
// Only D3D11 *interfaces* are used (methods via vtables). Never link
// d3d11.lib: this DLL is itself named d3d11.dll and would import from itself.

#include <windows.h>
#include <unknwn.h>
#include <d3d11.h>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
