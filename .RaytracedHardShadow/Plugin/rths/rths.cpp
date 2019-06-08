#include "pch.h"
#include "rths.h"
#include "rthsLog.h"
#include "rthsRenderer.h"

#ifdef _WIN32
    #define rthsAPI extern "C" __declspec(dllexport)
    #include "rthsGfxContextDXR.h"
    #include "rthsResourceTranslatorDXR.h"
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
    delete self;
}

rthsAPI void rthsUpdate(IRenderer *self)
{
    if (!self)
        return;
    self->update();
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

rthsAPI void rthsFinish(IRenderer *self)
{
    if (!self)
        return;
    self->finish();
}

rthsAPI void rthsSetCamera(IRenderer *self, float4x4 transform, float4x4 view, float4x4 proj, float near_plane, float far_plane, float fov)
{
    if (!self)
        return;
    self->setCamera(transform, view, proj, near_plane, far_plane, fov);
}

rthsAPI void rthsAddDirectionalLight(IRenderer *self, float4x4 transform)
{
    if (!self)
        return;
    self->addDirectionalLight(transform);
}

rthsAPI void rthsAddSpotLight(IRenderer *self, float4x4 transform, float range, float spot_angle)
{
    if (!self)
        return;
    self->addSpotLight(transform, range, spot_angle);
}

rthsAPI void rthsAddPointLight(IRenderer *self, float4x4 transform, float range)
{
    if (!self)
        return;
    self->addPointLight(transform, range);
}

rthsAPI void rthsAddReversePointLight(IRenderer *self, float4x4 transform, float range)
{
    if (!self)
        return;
    self->addReversePointLight(transform, range);
}

rthsAPI void rthsAddMesh(IRenderer *self, float4x4 transform,
    void *vb, void *ib, int vertex_count, int index_bits, int index_count, int index_offset, bool is_dynamic)
{
    if (!self || !vb || !ib)
        return;
    self->addMesh(transform, vb, ib, vertex_count, index_bits, index_count, index_offset, is_dynamic);
}


#ifdef _WIN32
namespace rths {
    extern ID3D11Device *g_unity_d3d11_device;
    extern ID3D12Device *g_unity_d3d12_device;
} // namespace rths
#endif // _WIN32

// Unity plugin load event
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
    using namespace rths;
#ifdef _WIN32
    auto* graphics = unityInterfaces->Get<IUnityGraphics>();
    switch (graphics->GetRenderer()) {
    case kUnityGfxRendererD3D11:
        g_unity_d3d11_device = unityInterfaces->Get<IUnityGraphicsD3D11>()->GetDevice();
        break;
    case kUnityGfxRendererD3D12:
        g_unity_d3d12_device = unityInterfaces->Get<IUnityGraphicsD3D12>()->GetDevice();
        break;
    default:
        // graphics API not supported
        SetErrorLog("Graphics API must be D3D11 or D3D12");
        return;
    }

    GfxContextDXR::initializeInstance();
#endif // _WIN32
}
