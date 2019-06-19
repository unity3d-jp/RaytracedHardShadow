#pragma once
#include <cstdint>

#ifdef _WIN32
    #define rthsAPI extern "C" __declspec(dllexport)
#else
    #define rthsAPI extern "C" 
#endif

namespace rths {
#ifndef rthsImpl

struct float3 { float x, y, z; };
struct float4 { float x, y, z, w; };
struct float4x4 { float4 v[4]; };

enum class RenderFlag : uint32_t
{
    CullBackFace            = 0x0001,
    IgnoreSelfShadow        = 0x0002,
    KeepSelfDropShadow      = 0x0004,
    GPUSkinning             = 0x0100,
    ClampBlendShapeWights   = 0x0200,
};

struct BoneWeight1
{
    float weight;
    int index;
};
struct BoneWeight4
{
    float weight[4];
    int index[4];
};
#else // rthsImpl
struct float3;
struct float4;
struct float4x4;
struct BoneWeight1;
struct BoneWeight4;
#endif // rthsImpl

using GPUResourcePtr = void*;
struct MeshData;
struct MeshInstanceData;
class IRenderer;

} // namespace rths

rthsAPI rths::MeshData* rthsMeshCreate();
rthsAPI void rthsMeshRelease(rths::MeshData *self);
rthsAPI void rthsMeshSetGPUBuffers(rths::MeshData *self, rths::GPUResourcePtr vb, rths::GPUResourcePtr ib,
    int vertex_stride, int vertex_count, int vertex_offset, int index_stride, int index_count, int index_offset);
rthsAPI void rthsMeshSetSkinBindposes(rths::MeshData *self, const rths::float4x4 *bindposes, int num_bindposes);
rthsAPI void rthsMeshSetSkinWeights(rths::MeshData *self, const uint8_t *c, int nc, const rths::BoneWeight1 *w, int nw);
rthsAPI void rthsMeshSetSkinWeights4(rths::MeshData *self, const rths::BoneWeight4 *w4, int nw4);
rthsAPI void rthsMeshSetBlendshapeCount(rths::MeshData *self, int num_bs);
rthsAPI void rthsMeshAddBlendshapeFrame(rths::MeshData *self, int bs_index, const rths::float3 *delta, float weight);

rthsAPI rths::MeshInstanceData* rthsMeshInstanceCreate(rths::MeshData *mesh);
rthsAPI void rthsMeshInstanceRelease(rths::MeshInstanceData *self);
rthsAPI void rthsMeshInstanceSetTransform(rths::MeshInstanceData *self, rths::float4x4 transform);
rthsAPI void rthsMeshInstanceSetBones(rths::MeshInstanceData *self, const rths::float4x4 *bones, int num_bones);
rthsAPI void rthsMeshInstanceSetBlendshapeWeights(rths::MeshInstanceData *self, const float *bsw, int num_bsw);

rthsAPI const char* rthsGetErrorLog();
rthsAPI rths::IRenderer* rthsCreateRenderer();
rthsAPI void rthsReleaseRenderer(rths::IRenderer *self);
rthsAPI void rthsSetRenderTarget(rths::IRenderer *self, void *render_target);
rthsAPI void rthsBeginScene(rths::IRenderer *self);
rthsAPI void rthsEndScene(rths::IRenderer *self);
rthsAPI void rthsSetRenderFlags(rths::IRenderer *self, int v);
rthsAPI void rthsSetShadowRayOffset(rths::IRenderer *self, float v);
rthsAPI void rthsSetSelfShadowThreshold(rths::IRenderer *self, float v);
rthsAPI void rthsSetCamera(rths::IRenderer *self, rths::float4x4 transform, rths::float4x4 view, rths::float4x4 proj, float near_plane, float far_plane, float fov);
rthsAPI void rthsAddDirectionalLight(rths::IRenderer *self, rths::float4x4 transform);
rthsAPI void rthsAddSpotLight(rths::IRenderer *self, rths::float4x4 transform, float range, float spot_angle);
rthsAPI void rthsAddPointLight(rths::IRenderer *self, rths::float4x4 transform, float range);
rthsAPI void rthsAddReversePointLight(rths::IRenderer *self, rths::float4x4 transform, float range);
rthsAPI void rthsAddMesh(rths::IRenderer *self, rths::MeshInstanceData *mesh);
rthsAPI void rthsRender(rths::IRenderer *self);
rthsAPI void rthsFinish(rths::IRenderer *self);
rthsAPI void rthsRenderAll();

#ifdef _WIN32
struct ID3D11Device;
struct ID3D12Device;
rthsAPI void rthsSetHostD3D11Device(ID3D11Device *device);
rthsAPI void rthsSetHostD3D12Device(ID3D12Device *device);
#endif // _WIN32
