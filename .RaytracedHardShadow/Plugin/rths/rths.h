#pragma once

#ifdef _WIN32
    #define rthsAPI extern "C" __declspec(dllexport)
#else
    #define rthsAPI extern "C" 
#endif

namespace rths {
#ifndef rthsImpl
using uint8_t = unsigned char;
using GPUResourcePtr = void*;

struct float4 { float x, y, z, w; };
struct float4x4 { float4 v[4]; };

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
struct SkinData
{
    // bone_counts & weights1 and weights4 are mutually exclusive
    const uint8_t *bone_counts;
    const BoneWeight1 *weights1;
    const BoneWeight4 *weights4;
    int num_bone_counts;
    int num_weights1;
    int num_weights4;
};
struct BonesData
{
    const float4x4 *bones;
    int num_bones;
};

struct BlendshapeDeltaData
{
    const float3 *point_delta;
};
struct BlendshapeData
{
    const BlendshapeDeltaData *blendshapes;
    int num_blendshapes;
};
struct BlendshapeWeightData
{
    const float *weights;
    int num_blendshapes;
};

struct MeshData
{
    GPUResourcePtr vertex_buffer; // host
    GPUResourcePtr index_buffer; // host
    int vertex_stride; // if 0, treated as size_of_vertex_buffer / vertex_count
    int vertex_count;
    int vertex_offset; // in byte
    int index_stride;
    int index_count;
    int index_offset; // in byte

    SkinData skin;
    BlendshapeData blendshape;
};
#else // rthsImpl
struct float4;
struct float4x4;
struct BonesData;
struct BlendshapeWeightData;
struct MeshData;
#endif // rthsImpl

class IRenderer;

} // namespace rths

rthsAPI const char* rthsGetErrorLog();
rthsAPI rths::IRenderer* rthsCreateRenderer();
rthsAPI void rthsDestroyRenderer(rths::IRenderer *self);
rthsAPI void rthsSetRenderTarget(rths::IRenderer *self, void *render_target);
rthsAPI void rthsBeginScene(rths::IRenderer *self);
rthsAPI void rthsEndScene(rths::IRenderer *self);
rthsAPI void rthsSetRaytraceFlags(rths::IRenderer *self, int v);
rthsAPI void rthsSetShadowRayOffset(rths::IRenderer *self, float v);
rthsAPI void rthsSetSelfShadowThreshold(rths::IRenderer *self, float v);
rthsAPI void rthsSetCamera(rths::IRenderer *self, rths::float4x4 transform, rths::float4x4 view, rths::float4x4 proj, float near_plane, float far_plane, float fov);
rthsAPI void rthsAddDirectionalLight(rths::IRenderer *self, rths::float4x4 transform);
rthsAPI void rthsAddSpotLight(rths::IRenderer *self, rths::float4x4 transform, float range, float spot_angle);
rthsAPI void rthsAddPointLight(rths::IRenderer *self, rths::float4x4 transform, float range);
rthsAPI void rthsAddReversePointLight(rths::IRenderer *self, rths::float4x4 transform, float range);
rthsAPI void rthsAddMesh(rths::IRenderer *self, rths::MeshData mesh, rths::float4x4 transform);
rthsAPI void rthsAddSkinnedMesh(rths::IRenderer *self, rths::MeshData mesh, rths::float4x4 transform, rths::BonesData bones, rths::BlendshapeWeightData bs);
rthsAPI void rthsRender(rths::IRenderer *self);
rthsAPI void rthsFinish(rths::IRenderer *self);
rthsAPI void rthsRenderAll();

#ifdef _WIN32
struct ID3D11Device;
struct ID3D12Device;
rthsAPI void rthsSetHostD3D11Device(ID3D11Device *device);
rthsAPI void rthsSetHostD3D12Device(ID3D12Device *device);
#endif // _WIN32
