#include "pch.h"
#include "rths.h"
#include "rthsRenderer.h"

#ifdef _WIN32
    #define rthsAPI extern "C" __declspec(dllexport)
#else
    #define rthsAPI extern "C" 
#endif

using namespace rths;

rthsAPI bool rthsInitializeGfxDevice()
{
    return false;
}

rthsAPI void rthsFinalizeGfxDevice()
{
}

rthsAPI Renderer* rthsCreateRenderer()
{
    // todo: return null if DXR is not supported
    return new Renderer();
}
rthsAPI void rthsDestroyRenderer(Renderer *self)
{
    return delete self;
}
rthsAPI void rthsSetRenderTarget(Renderer *self, void *render_target)
{
    if (!self)
        return;
}

rthsAPI void rthsBeginScene(Renderer *self)
{
    if (!self)
        return;
}

rthsAPI void rthsEndScene(Renderer *self)
{
    if (!self)
        return;
}

rthsAPI void rthsRender(Renderer *self)
{
    if (!self)
        return;
}

rthsAPI void rthsSetCamera(Renderer *self, float4x4 transform, float near_, float far_, float fov)
{
    if (!self)
        return;
}

rthsAPI void rthsAddDirectionalLight(Renderer *self, float4x4 transform)
{
    if (!self)
        return;
}

rthsAPI void rthsAddMesh(Renderer *self, float4x4 transform, void *vb, void *ib)
{
    if (!self)
        return;
}
