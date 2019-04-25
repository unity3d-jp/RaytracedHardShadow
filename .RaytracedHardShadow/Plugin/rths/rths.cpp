#include "pch.h"
#include "rths.h"
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

rthsAPI Renderer* rthsCreateRenderer()
{
    if (!GfxContext::initializeInstance())
        return nullptr;
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
    self->setRenderTarget(render_target);
}

rthsAPI void rthsBeginScene(Renderer *self)
{
    if (!self)
        return;
    self->beginScene();
}

rthsAPI void rthsEndScene(Renderer *self)
{
    if (!self)
        return;
    self->endScene();
}

rthsAPI void rthsRender(Renderer *self)
{
    if (!self)
        return;
    self->render();
}

rthsAPI void rthsSetCamera(Renderer *self, float4x4 transform, float near_, float far_, float fov)
{
    if (!self)
        return;
    self->setCamera(transform, near_, far_, fov);
}

rthsAPI void rthsAddDirectionalLight(Renderer *self, float4x4 transform)
{
    if (!self)
        return;
    self->addDirectionalLight(transform);
}

rthsAPI void rthsAddMesh(Renderer *self, float4x4 transform, void *vb, void *ib)
{
    if (!self)
        return;
    self->addMesh(transform, vb, ib);
}
