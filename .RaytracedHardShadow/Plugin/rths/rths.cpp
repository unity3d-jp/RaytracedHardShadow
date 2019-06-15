#include "pch.h"
#include "rths.h"
#include "rthsLog.h"
#include "rthsRenderer.h"

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

rthsAPI void rthsSetRaytraceFlags(IRenderer *self, int v)
{
    if (!self)
        return;
    self->setRaytraceFlags(v);
}
rthsAPI void rthsSetShadowRayOffset(IRenderer *self, float v)
{
    if (!self)
        return;
    self->setShadowRayOffset(v);
}
rthsAPI void rthsSetSelfShadowThreshold(IRenderer *self, float v)
{
    if (!self)
        return;
    self->setSelfShadowThreshold(v);
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

rthsAPI void rthsAddMesh(IRenderer *self, MeshData mesh, float4x4 transform)
{
    if (!self)
        return;
    self->addMesh(mesh, transform);
}

rthsAPI void rthsAddSkinnedMesh(IRenderer *self, MeshData mesh, SkinData skin)
{
    if (!self)
        return;
    self->addSkinnedMesh(mesh, skin);
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

rthsAPI void rthsRenderAll()
{
    rths::RenderAll();
}


#ifdef _WIN32
namespace rths {
    extern ID3D11Device *g_host_d3d11_device;
    extern ID3D12Device *g_host_d3d12_device;
} // namespace rths

rthsAPI void rthsSetHostD3D11Device(ID3D11Device *device)
{
    rths::g_host_d3d11_device = device;
}

rthsAPI void rthsSetHostD3D12Device(ID3D12Device *device)
{
    rths::g_host_d3d12_device = device;
}
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
        g_host_d3d11_device = unityInterfaces->Get<IUnityGraphicsD3D11>()->GetDevice();
        break;
    case kUnityGfxRendererD3D12:
        if (auto ifs = unityInterfaces->Get<IUnityGraphicsD3D12v5>()) {
            g_host_d3d12_device = ifs->GetDevice();
        }
        else if (auto ifs = unityInterfaces->Get<IUnityGraphicsD3D12v4>()) {
            g_host_d3d12_device = ifs->GetDevice();
        }
        else if (auto ifs = unityInterfaces->Get<IUnityGraphicsD3D12v3>()) {
            g_host_d3d12_device = ifs->GetDevice();
        }
        else if (auto ifs = unityInterfaces->Get<IUnityGraphicsD3D12v2>()) {
            g_host_d3d12_device = ifs->GetDevice();
        }
        else if (auto ifs = unityInterfaces->Get<IUnityGraphicsD3D12>()) {
            g_host_d3d12_device = ifs->GetDevice();
        }
        else {
            // unknown IUnityGraphicsD3D12 version
            SetErrorLog("Unknown IUnityGraphicsD3D12 version\n");
            return;
        }
        break;
    default:
        // graphics API not supported
        SetErrorLog("Graphics API must be D3D11 or D3D12\n");
        return;
    }
#endif // _WIN32
}

static void UNITY_INTERFACE_API _RenderAll(int)
{
    rths::RenderAll();
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
rthsGetRenderAll()
{
    return _RenderAll;
}
