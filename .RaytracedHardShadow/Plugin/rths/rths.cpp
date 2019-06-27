#include "pch.h"
#include "rths.h"
#include "rthsLog.h"
#include "rthsRenderer.h"

using namespace rths;


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

rthsAPI void rthsMeshSetCPUBuffers(rths::MeshData * self, void *vb, void *ib, int vertex_stride, int vertex_count, int vertex_offset, int index_stride, int index_count, int index_offset)
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
rthsAPI void rthsMeshInstanceSetTransform(MeshInstanceData *self, float4x4 transform)
{
    if (!self)
        return;

    if (self->transform != transform) {
        self->update_flags |= (int)UpdateFlag::Transform;
        self->transform = transform;
    }
}
rthsAPI void rthsMeshInstanceSetBones(MeshInstanceData *self, const float4x4 *bones, int num_bones)
{
    if (!self || !self->mesh->skin.valid())
        return;

    if (self->bones.size() != num_bones)
        self->update_flags |= (int)UpdateFlag::Bone;

    if (num_bones == 0) {
        self->bones.clear();
    }
    else {
        if (self->bones.size() == num_bones && !std::equal(bones, bones + num_bones, self->bones.data()))
            self->update_flags |= (int)UpdateFlag::Bone;
        self->bones.assign(bones, bones + num_bones);
    }
}
rthsAPI void rthsMeshInstanceSetBlendshapeWeights(MeshInstanceData *self, const float *bsw, int num_bsw)
{
    if (!self || self->mesh->blendshapes.empty())
        return;

    if (self->blendshape_weights.size() != num_bsw)
        self->update_flags |= (int)UpdateFlag::Blendshape;

    if (num_bsw == 0) {
        self->blendshape_weights.clear();
    }
    else {
        if (self->blendshape_weights.size() == num_bsw && !std::equal(bsw, bsw + num_bsw, self->blendshape_weights.data()))
            self->update_flags |= (int)UpdateFlag::Blendshape;
        self->blendshape_weights.assign(bsw, bsw + num_bsw);
    }
}


rthsAPI const char* rthsGetErrorLog()
{
    return GetErrorLog().c_str();
}

rthsAPI IRenderer* rthsCreateRenderer()
{
    return CreateRendererDXR();
}

rthsAPI void rthsReleaseRenderer(IRenderer *self)
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

rthsAPI void rthsSetRenderFlags(IRenderer *self, uint32_t v)
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

rthsAPI void rthsAddGeometry(IRenderer *self, rths::MeshInstanceData *mesh, uint8_t mask)
{
    if (!self)
        return;
    self->addGeometry({ mesh, mask });
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

static void UNITY_INTERFACE_API _RenderAll(int)
{
    rths::RenderAll();
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
rthsGetRenderAll()
{
    return _RenderAll;
}
