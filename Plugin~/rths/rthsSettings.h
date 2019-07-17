#pragma once

#define rthsVersion     "0.5.2"
#define rthsReleaseDate "20190711"


#ifdef rthsDebug
    // debug layer
    #define rthsEnableD3D12DebugLayer

    // GPU based validation
    // https://docs.microsoft.com/en-us/windows/desktop/direct3d12/using-d3d12-debug-layer-gpu-based-validation
    // note: enabling this can cause problems. in our case, shader resources bound by global root sig become invisible.
    //#define rthsEnableD3D12GBV

    //// DREAD (this requires Windows SDK 10.0.18362.0 or newer)
    //// https://docs.microsoft.com/en-us/windows/desktop/direct3d12/use-dred
    //#define rthsEnableD3D12DREAD

    //#define rthsEnableBufferValidation
    //#define rthsEnableRenderTargetValidation
    //#define rthsForceSoftwareDevice
#endif // rthsDebug

#define rthsEnableResourceName
#define rthsEnableTimestamp
#define rthsEnableD3D12StablePowerState
