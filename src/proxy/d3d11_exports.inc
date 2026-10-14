// X-macro list of every System32 (SysWOW64) d3d11.dll export we forward
// verbatim. D3D11CreateDeviceAndSwapChain is deliberately absent: it has a
// real interceptor in d3d11_proxy.cpp.
//
// Why forward everything: once our proxy is loaded as "d3d11.dll", the loader
// binds EVERY later d3d11 import in the process to us. SteamVR's 32-bit
// vrclient.dll imports D3D11CreateDevice; with only one export, LoadLibrary
// of vrclient fails with ERROR_PROC_NOT_FOUND and VR_InitInternal2 reports
// VRInitError_Init_VRClientDLLNotFound (102).
//
// Generated from the export table of Windows\SysWOW64\d3d11.dll (51 names).
// CMake checks that d3d11.def lists the same names.
MVR_D3D11_FORWARD(0, CreateDirect3D11DeviceFromDXGIDevice)
MVR_D3D11_FORWARD(1, CreateDirect3D11SurfaceFromDXGISurface)
MVR_D3D11_FORWARD(2, D3D11CoreCreateDevice)
MVR_D3D11_FORWARD(3, D3D11CoreCreateLayeredDevice)
MVR_D3D11_FORWARD(4, D3D11CoreGetLayeredDeviceSize)
MVR_D3D11_FORWARD(5, D3D11CoreRegisterLayers)
MVR_D3D11_FORWARD(6, D3D11CreateDevice)
MVR_D3D11_FORWARD(7, D3D11CreateDeviceForD3D12)
MVR_D3D11_FORWARD(8, D3D11On12CreateDevice)
MVR_D3D11_FORWARD(9, D3DKMTCloseAdapter)
MVR_D3D11_FORWARD(10, D3DKMTCreateAllocation)
MVR_D3D11_FORWARD(11, D3DKMTCreateContext)
MVR_D3D11_FORWARD(12, D3DKMTCreateDevice)
MVR_D3D11_FORWARD(13, D3DKMTCreateSynchronizationObject)
MVR_D3D11_FORWARD(14, D3DKMTDestroyAllocation)
MVR_D3D11_FORWARD(15, D3DKMTDestroyContext)
MVR_D3D11_FORWARD(16, D3DKMTDestroyDevice)
MVR_D3D11_FORWARD(17, D3DKMTDestroySynchronizationObject)
MVR_D3D11_FORWARD(18, D3DKMTEscape)
MVR_D3D11_FORWARD(19, D3DKMTGetContextSchedulingPriority)
MVR_D3D11_FORWARD(20, D3DKMTGetDeviceState)
MVR_D3D11_FORWARD(21, D3DKMTGetDisplayModeList)
MVR_D3D11_FORWARD(22, D3DKMTGetMultisampleMethodList)
MVR_D3D11_FORWARD(23, D3DKMTGetRuntimeData)
MVR_D3D11_FORWARD(24, D3DKMTGetSharedPrimaryHandle)
MVR_D3D11_FORWARD(25, D3DKMTLock)
MVR_D3D11_FORWARD(26, D3DKMTOpenAdapterFromHdc)
MVR_D3D11_FORWARD(27, D3DKMTOpenResource)
MVR_D3D11_FORWARD(28, D3DKMTPresent)
MVR_D3D11_FORWARD(29, D3DKMTQueryAdapterInfo)
MVR_D3D11_FORWARD(30, D3DKMTQueryAllocationResidency)
MVR_D3D11_FORWARD(31, D3DKMTQueryResourceInfo)
MVR_D3D11_FORWARD(32, D3DKMTRender)
MVR_D3D11_FORWARD(33, D3DKMTSetAllocationPriority)
MVR_D3D11_FORWARD(34, D3DKMTSetContextSchedulingPriority)
MVR_D3D11_FORWARD(35, D3DKMTSetDisplayMode)
MVR_D3D11_FORWARD(36, D3DKMTSetDisplayPrivateDriverFormat)
MVR_D3D11_FORWARD(37, D3DKMTSetGammaRamp)
MVR_D3D11_FORWARD(38, D3DKMTSetVidPnSourceOwner)
MVR_D3D11_FORWARD(39, D3DKMTSignalSynchronizationObject)
MVR_D3D11_FORWARD(40, D3DKMTUnlock)
MVR_D3D11_FORWARD(41, D3DKMTWaitForSynchronizationObject)
MVR_D3D11_FORWARD(42, D3DKMTWaitForVerticalBlankEvent)
MVR_D3D11_FORWARD(43, D3DPerformance_BeginEvent)
MVR_D3D11_FORWARD(44, D3DPerformance_EndEvent)
MVR_D3D11_FORWARD(45, D3DPerformance_GetStatus)
MVR_D3D11_FORWARD(46, D3DPerformance_SetMarker)
MVR_D3D11_FORWARD(47, EnableFeatureLevelUpgrade)
MVR_D3D11_FORWARD(48, OpenAdapter10)
MVR_D3D11_FORWARD(49, OpenAdapter10_2)
