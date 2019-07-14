#include "pch.h"
#include "Foundation/rthsMath.h"
#include "Foundation/rthsLog.h"
#include "rthsRenderer.h"
#include "rths.h"

using namespace rths;

rthsAPI const char* rthsGetVersion() { return rthsVersion; }
rthsAPI const char* rthsGetReleaseDate() { return rthsReleaseDate; }

rthsAPI const char* rthsGetErrorLog()
{
    static std::string s_log;
    s_log = GetErrorLog();
    return s_log.c_str();
}

rthsAPI void rthsClearErrorLog()
{
    ClearErrorLog();
}

rthsAPI bool rthsGlobalsGetDeferredInitialization()
{
    return rths::GetGlobals().deferred_initilization;
}
rthsAPI void rthsGlobalsSetDeferredInitialization(bool v)
{
    rths::GetGlobals().deferred_initilization = v;
}

rthsAPI bool rthsGlobalsGetPowerStableState()
{
    return rths::GetGlobals().power_stable_state;
}
rthsAPI void rthsGlobalsSetPowerStableState(bool v)
{
    rths::GetGlobals().power_stable_state = v;
}


rthsAPI MeshData* rthsMeshCreate()
{
    return new MeshData();
}
rthsAPI void rthsMeshRelease(MeshData *self)
{
    if (!self)
        return;
    self->release();
}

rthsAPI bool rthsMeshIsRelocated(MeshData *self)
{
    if (!self)
        return false;
    return self->isRelocated();
}

rthsAPI void rthsMeshSetName(rths::MeshData *self, const char *name)
{
    if (!self)
        return;
    self->name = name ? name : std::string();
}

rthsAPI void rthsMeshSetCPUBuffers(rths::MeshData * self, CPUResourcePtr vb, CPUResourcePtr ib,
    int vertex_stride, int vertex_count, int vertex_offset, int index_stride, int index_count, int index_offset)
{
    if (!self)
        return;
    self->cpu_vertex_buffer = vb;
    self->cpu_index_buffer = ib;
    self->vertex_stride = vertex_stride;
    self->vertex_count = vertex_count;
    self->vertex_offset = vertex_offset;
    self->index_stride = index_stride;
    self->index_count = index_count;
    self->index_offset = index_offset;
}

rthsAPI void rthsMeshSetGPUBuffers(MeshData *self, GPUResourcePtr vb, GPUResourcePtr ib,
    int vertex_stride, int vertex_count, int vertex_offset, int index_stride, int index_count, int index_offset)
{
    if (!self)
        return;
    self->gpu_vertex_buffer = vb;
    self->gpu_index_buffer = ib;
    self->vertex_stride = vertex_stride;
    self->vertex_count = vertex_count;
    self->vertex_offset = vertex_offset;
    self->index_stride = index_stride;
    self->index_count = index_count;
    self->index_offset = index_offset;
}

rthsAPI void rthsMeshSetSkinBindposes(MeshData *self, const float4x4 *bindposes, int num_bindposes)
{
    if (!self)
        return;
    self->skin.bindposes.assign(bindposes, bindposes + num_bindposes);
}
rthsAPI void rthsMeshSetSkinWeights(MeshData *self, const uint8_t *c, int nc, const BoneWeight1 *w, int nw)
{
    if (!self)
        return;
    self->skin.bone_counts.assign(c, c + nc);
    self->skin.weights.assign(w, w + nw);
}
rthsAPI void rthsMeshSetSkinWeights4(MeshData *self, const BoneWeight4 *w4, int nw4)
{
    if (!self)
        return;

    self->skin.bone_counts.resize(nw4);
    self->skin.weights.resize(nw4 * 4); // reserve

    int tw = 0;
    for (int vi = 0; vi < nw4; ++vi) {
        auto& tmp = w4[vi];
        int c = 0;
        for (int wi = 0; wi < 4; ++wi) {
            if (tmp.weight[wi] > 0.0f) {
                self->skin.weights[tw] = { tmp.weight[wi], tmp.index[wi] };
                ++tw;
                ++c;
            }
            else
                break;
        }
        self->skin.bone_counts[vi] = c;
    }
    self->skin.weights.resize(tw); // shrink to fit
}

rthsAPI void rthsMeshSetBlendshapeCount(MeshData *self, int num_bs)
{
    if (!self)
        return;

    self->blendshapes.resize(num_bs);
}
rthsAPI void rthsMeshAddBlendshapeFrame(MeshData *self, int bs_index, const float3 *delta, float weight)
{
    if (!self)
        return;

    if (bs_index <= self->blendshapes.size())
        self->blendshapes.resize(bs_index + 1);

    BlendshapeFrameData frame;
    frame.delta.assign(delta, delta + self->vertex_count);
    frame.weight = weight;
    self->blendshapes[bs_index].frames.push_back(std::move(frame));
}

rthsAPI void rthsMeshMarkDyncmic(MeshData *self, bool v)
{
    if (!self)
        return;
    self->is_dynamic = v;
}


rthsAPI MeshInstanceData* rthsMeshInstanceCreate(rths::MeshData *mesh)
{
    auto ret = new MeshInstanceData();
    ret->mesh = mesh;
    return ret;
}
rthsAPI void rthsMeshInstanceRelease(MeshInstanceData *self)
{
    if (!self)
        return;
    self->release();
}
rthsAPI void rthsMeshInstanceSetName(rths::MeshInstanceData * self, const char *name)
{
    if (!self)
        return;
    self->name = name ? name : std::string();
}
rthsAPI void rthsMeshInstanceSetTransform(MeshInstanceData *self, float4x4 transform)
{
    if (!self)
        return;
    self->setTransform(transform);
}
rthsAPI void rthsMeshInstanceSetBones(MeshInstanceData *self, const float4x4 *bones, int num_bones)
{
    if (!self || !self->mesh->skin.valid())
        return;
    self->setBones(bones, num_bones);
}
rthsAPI void rthsMeshInstanceSetBlendshapeWeights(MeshInstanceData *self, const float *bsw, int num_bsw)
{
    if (!self || self->mesh->blendshapes.empty())
        return;
    self->setBlendshapeWeights(bsw, num_bsw);
}


rthsAPI RenderTargetData* rthsRenderTargetCreate()
{
    return new RenderTargetData();
}
rthsAPI void rthsRenderTargetRelease(RenderTargetData *self)
{
    if (!self)
        return;
    self->release();
}
rthsAPI bool rthsRenderTargetIsRelocated(RenderTargetData *self)
{
    if (!self)
        return false;
    return self->isRelocated();
}
rthsAPI void rthsRenderTargetSetName(RenderTargetData * self, const char *name)
{
    if (!self)
        return;
    self->name = name ? name : std::string();
}
rthsAPI void rthsRenderTargetSetGPUTexture(RenderTargetData *self, GPUResourcePtr tex)
{
    if (!self)
        return;
    self->gpu_texture = tex;
}
rthsAPI void rthsRenderTargetSetup(RenderTargetData *self, int width, int height, RenderTargetFormat format)
{
    if (!self)
        return;
    self->width = width;
    self->height = height;
    self->format = format;
}


rthsAPI IRenderer* rthsRendererCreate()
{
    return CreateRendererDXR();
}

rthsAPI void rthsRendererRelease(IRenderer *self)
{
    if (!self)
        return;
    self->release();
}

rthsAPI bool rthsRendererIsInitialized(IRenderer *self)
{
    if (!self)
        return false;
    return self->initialized();
}

rthsAPI bool rthsRendererIsValid(IRenderer *self)
{
    if (!self)
        return false;
    return self->valid();
}

rthsAPI bool rthsRendererIsRendering(IRenderer *self)
{
    if (!self)
        return false;
    return self->isRendering();
}

rthsAPI void rthsRendererSetName(IRenderer *self, const char *name)
{
    if (!self)
        return;
    self->setName(name ? name : std::string());
}

rthsAPI void rthsRendererSetRenderTarget(IRenderer *self, RenderTargetData *render_target)
{
    if (!self || !render_target)
        return;
    self->setRenderTarget(render_target);
}

rthsAPI void rthsRendererBeginScene(IRenderer *self)
{
    if (!self)
        return;
    self->beginScene();
}

rthsAPI void rthsRendererEndScene(IRenderer *self)
{
    if (!self)
        return;
    self->endScene();
}

rthsAPI void rthsRendererSetRenderFlags(IRenderer *self, uint32_t v)
{
    if (!self)
        return;
    self->setRaytraceFlags(v);
}
rthsAPI void rthsRendererSetShadowRayOffset(IRenderer *self, float v)
{
    if (!self)
        return;
    self->setShadowRayOffset(v);
}
rthsAPI void rthsRendererSetSelfShadowThreshold(IRenderer *self, float v)
{
    if (!self)
        return;
    self->setSelfShadowThreshold(v);
}

rthsAPI void rthsRendererSetCamera(IRenderer *self, float3 pos, float4x4 view, float4x4 proj)
{
    if (!self)
        return;
    self->setCamera(pos, view, proj);
}

rthsAPI void rthsRendererAddDirectionalLight(IRenderer *self, float3 dir, uint32_t lmask)
{
    if (!self)
        return;
    self->addDirectionalLight(dir, lmask);
}

rthsAPI void rthsRendererAddSpotLight(IRenderer *self, float3 pos, float3 dir, float range, float spot_angle, uint32_t lmask)
{
    if (!self)
        return;
    self->addSpotLight(pos, dir, range, spot_angle, lmask);
}

rthsAPI void rthsRendererAddPointLight(IRenderer *self, float3 pos, float range, uint32_t lmask)
{
    if (!self)
        return;
    self->addPointLight(pos, range, lmask);
}

rthsAPI void rthsRendererAddReversePointLight(IRenderer *self, float3 pos, float range, uint32_t lmask)
{
    if (!self)
        return;
    self->addReversePointLight(pos, range, lmask);
}

rthsAPI void rthsRendererAddMesh(IRenderer *self, rths::MeshInstanceData *mesh)
{
    if (!self)
        return;
    self->addMesh(mesh);
}

rthsAPI void rthsRendererStartRender(IRenderer *self)
{
    if (!self)
        return;
    self->render();
}

rthsAPI void rthsRendererFinishRender(IRenderer *self)
{
    if (!self)
        return;
    self->finish();
}

rthsAPI bool rthsRendererReadbackRenderTarget(IRenderer *self, void *dst)
{
    if (!self)
        return false;
    return self->readbackRenderTarget(dst);
}

rthsAPI const char* rthsRendererGetTimestampLog(IRenderer *self)
{
    if (!self)
        return nullptr;
    static std::string s_log;
    s_log = self->getTimestampLog();
    return s_log.c_str();
}

rthsAPI GPUResourcePtr rthsRendererGetRenderTexturePtr(IRenderer *self)
{
    if (!self)
        return nullptr;
    return self->getRenderTexturePtr();
}

rthsAPI void rthsMarkFrameBegin()
{
    rths::MarkFrameBegin();
}
rthsAPI void rthsMarkFrameEnd()
{
    rths::MarkFrameEnd();
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

static void UNITY_INTERFACE_API _FlushDeferredCommands(int)
{
    rths::FlushDeferredCommands();
}
extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
rthsGetFlushDeferredCommands()
{
    return _FlushDeferredCommands;
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
