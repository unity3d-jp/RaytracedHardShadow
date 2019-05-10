#include "pch.h"
#include "rths.h"
#include "rthsLog.h"
#include "rthsRenderer.h"

#ifdef _WIN32
    #define rthsAPI extern "C" __declspec(dllexport)
#else
    #define rthsAPI extern "C" 
#endif

using namespace rths;

rthsAPI const char* rthsGetErrorLog()
{
    return GetErrorLog().c_str();
}

rthsAPI IRenderer* rthsCreateRenderer()
{
    return CreateRendererDXR();
}

rthsAPI void rthsDestroyRenderer(IRenderer *self)
{
    return delete self;
}

rthsAPI void rthsSetRenderTarget(IRenderer *self, void *render_target)
{
    if (!self || !render_target)
        return;
    self->setRenderTarget(render_target);
}

rthsAPI void rthsBeginScene(IRenderer *self)
{
    if (!self)
        return;
    self->beginScene();
}

rthsAPI void rthsEndScene(IRenderer *self)
{
    if (!self)
        return;
    self->endScene();
}

rthsAPI void rthsRender(IRenderer *self)
{
    if (!self)
        return;
    self->render();
}

rthsAPI void rthsSetCamera(IRenderer *self, float4x4 transform, float near_, float far_, float fov)
{
    if (!self)
        return;
    self->setCamera(transform, near_, far_, fov);
}

rthsAPI void rthsAddDirectionalLight(IRenderer *self, float4x4 transform)
{
    if (!self)
        return;
    self->addDirectionalLight(transform);
}

rthsAPI void rthsAddMesh(IRenderer *self, float4x4 transform, void *vb, void *ib, int vertex_count, int index_count, int index_offset)
{
    if (!self || !vb || !ib)
        return;
    self->addMesh(transform, vb, ib, vertex_count, index_count, index_offset);
}
