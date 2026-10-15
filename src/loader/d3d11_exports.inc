// OpenMotherVR-Provenance: clean
// X-macro list of every System32 (SysWOW64) d3d11.dll export we forward
// verbatim. D3D11CreateDeviceAndSwapChain is deliberately absent: it has a
// real interceptor in d3d11_entry.cpp.
//
// Why forward everything: once our proxy is loaded as "d3d11.dll", the loader
// binds EVERY later d3d11 import in the process to us. SteamVR's 32-bit
// vrclient.dll imports D3D11CreateDevice; with only one export, LoadLibrary
// of vrclient fails with ERROR_PROC_NOT_FOUND and VR_InitInternal2 reports
// VRInitError_Init_VRClientDLLNotFound (102).
//
// Generated from the export table of Windows\SysWOW64\d3d11.dll (51 names).
// CMake checks that d3d11.def lists the same names.
OMVR_D3D11_FORWARD(0, CreateDirect3D11DeviceFromDXGIDevice)
OMVR_D3D11_FORWARD(1, CreateDirect3D11SurfaceFromDXGISurface)
OMVR_D3D11_FORWARD(2, D3D11CoreCreateDevice)
OMVR_D3D11_FORWARD(3, D3D11CoreCreateLayeredDevice)
OMVR_D3D11_FORWARD(4, D3D11CoreGetLayeredDeviceSize)
OMVR_D3D11_FORWARD(5, D3D11CoreRegisterLayers)
OMVR_D3D11_FORWARD(6, D3D11CreateDevice)
OMVR_D3D11_FORWARD(7, D3D11CreateDeviceForD3D12)
OMVR_D3D11_FORWARD(8, D3D11On12CreateDevice)
OMVR_D3D11_FORWARD(9, D3DKMTCloseAdapter)
OMVR_D3D11_FORWARD(10, D3DKMTCreateAllocation)
OMVR_D3D11_FORWARD(11, D3DKMTCreateContext)
OMVR_D3D11_FORWARD(12, D3DKMTCreateDevice)
OMVR_D3D11_FORWARD(13, D3DKMTCreateSynchronizationObject)
OMVR_D3D11_FORWARD(14, D3DKMTDestroyAllocation)
OMVR_D3D11_FORWARD(15, D3DKMTDestroyContext)
OMVR_D3D11_FORWARD(16, D3DKMTDestroyDevice)
OMVR_D3D11_FORWARD(17, D3DKMTDestroySynchronizationObject)
OMVR_D3D11_FORWARD(18, D3DKMTEscape)
OMVR_D3D11_FORWARD(19, D3DKMTGetContextSchedulingPriority)
OMVR_D3D11_FORWARD(20, D3DKMTGetDeviceState)
OMVR_D3D11_FORWARD(21, D3DKMTGetDisplayModeList)
OMVR_D3D11_FORWARD(22, D3DKMTGetMultisampleMethodList)
OMVR_D3D11_FORWARD(23, D3DKMTGetRuntimeData)
OMVR_D3D11_FORWARD(24, D3DKMTGetSharedPrimaryHandle)
OMVR_D3D11_FORWARD(25, D3DKMTLock)
OMVR_D3D11_FORWARD(26, D3DKMTOpenAdapterFromHdc)
OMVR_D3D11_FORWARD(27, D3DKMTOpenResource)
OMVR_D3D11_FORWARD(28, D3DKMTPresent)
OMVR_D3D11_FORWARD(29, D3DKMTQueryAdapterInfo)
OMVR_D3D11_FORWARD(30, D3DKMTQueryAllocationResidency)
OMVR_D3D11_FORWARD(31, D3DKMTQueryResourceInfo)
OMVR_D3D11_FORWARD(32, D3DKMTRender)
OMVR_D3D11_FORWARD(33, D3DKMTSetAllocationPriority)
OMVR_D3D11_FORWARD(34, D3DKMTSetContextSchedulingPriority)
OMVR_D3D11_FORWARD(35, D3DKMTSetDisplayMode)
OMVR_D3D11_FORWARD(36, D3DKMTSetDisplayPrivateDriverFormat)
OMVR_D3D11_FORWARD(37, D3DKMTSetGammaRamp)
OMVR_D3D11_FORWARD(38, D3DKMTSetVidPnSourceOwner)
OMVR_D3D11_FORWARD(39, D3DKMTSignalSynchronizationObject)
OMVR_D3D11_FORWARD(40, D3DKMTUnlock)
OMVR_D3D11_FORWARD(41, D3DKMTWaitForSynchronizationObject)
OMVR_D3D11_FORWARD(42, D3DKMTWaitForVerticalBlankEvent)
OMVR_D3D11_FORWARD(43, D3DPerformance_BeginEvent)
OMVR_D3D11_FORWARD(44, D3DPerformance_EndEvent)
OMVR_D3D11_FORWARD(45, D3DPerformance_GetStatus)
OMVR_D3D11_FORWARD(46, D3DPerformance_SetMarker)
OMVR_D3D11_FORWARD(47, EnableFeatureLevelUpgrade)
OMVR_D3D11_FORWARD(48, OpenAdapter10)
OMVR_D3D11_FORWARD(49, OpenAdapter10_2)
