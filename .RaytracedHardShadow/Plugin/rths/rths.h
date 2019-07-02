#pragma once
#include <cstdint>

#ifdef _WIN32
    #define rthsAPI extern "C" __declspec(dllexport)
#else
    #define rthsAPI extern "C" 
#endif

namespace rths {
#ifndef rthsImpl

struct float2
{
    float x, y;
    float& operator[](size_t i) { return ((float*)this)[i]; }
    const float& operator[](size_t i) const { return ((float*)this)[i]; }
};
struct float3
{
    float x, y, z;
    float& operator[](size_t i) { return ((float*)this)[i]; }
    const float& operator[](size_t i) const { return ((float*)this)[i]; }
};
struct float4
{
    float x, y, z, w;
    float& operator[](size_t i) { return ((float*)this)[i]; }
    const float& operator[](size_t i) const { return ((float*)this)[i]; }
};

struct float4x4
{
    float4 v[4];
    float4& operator[](size_t i) { return v[i]; }
    const float4& operator[](size_t i) const { return v[i]; }

    static float4x4 identity()
    {
        return{ {
             { 1, 0, 0, 0 },
             { 0, 1, 0, 0 },
             { 0, 0, 1, 0 },
             { 0, 0, 0, 1 },
         } };
    }
};

enum class RenderFlag : uint32_t
{
    CullBackFace            = 0x0001,
    IgnoreSelfShadow        = 0x0002,
    KeepSelfDropShadow      = 0x0004,
    GPUSkinning             = 0x0100,
    ClampBlendShapeWights   = 0x0200,
};

enum class HitMask : uint8_t
{
    Receiver    = 0x0001,
    Caster      = 0x0002,
    All = Receiver | Caster,
};

enum class RenderTargetFormat : uint32_t
{
    Unknown = 0,
    Ru8,
    RGu8,
    RGBAu8,
    Rf16,
    RGf16,
    RGBAf16,
    Rf32,
    RGf32,
    RGBAf32,
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
struct float2;
struct float3;
struct float4;
struct float4x4;
struct BoneWeight1;
struct BoneWeight4;
enum class RenderTargetFormat : uint32_t;
#endif // rthsImpl

using GPUResourcePtr = const void*;
using CPUResourcePtr = const void*;
class MeshData;
class MeshInstanceData;
class RenderTargetData;
class IRenderer;

} // namespace rths


rthsAPI const char* rthsGetErrorLog();

// mesh interface
rthsAPI rths::MeshData* rthsMeshCreate();
rthsAPI void rthsMeshRelease(rths::MeshData *self);
rthsAPI void rthsMeshSetName(rths::MeshData *self, const char *name);
rthsAPI void rthsMeshSetCPUBuffers(rths::MeshData *self, rths::CPUResourcePtr vb, rths::CPUResourcePtr ib,
    int vertex_stride, int vertex_count, int vertex_offset, int index_stride, int index_count, int index_offset);
rthsAPI void rthsMeshSetGPUBuffers(rths::MeshData *self, rths::GPUResourcePtr vb, rths::GPUResourcePtr ib,
    int vertex_stride, int vertex_count, int vertex_offset, int index_stride, int index_count, int index_offset);
rthsAPI void rthsMeshSetSkinBindposes(rths::MeshData *self, const rths::float4x4 *bindposes, int num_bindposes);
rthsAPI void rthsMeshSetSkinWeights(rths::MeshData *self, const uint8_t *c, int nc, const rths::BoneWeight1 *w, int nw);
rthsAPI void rthsMeshSetSkinWeights4(rths::MeshData *self, const rths::BoneWeight4 *w4, int nw4);
rthsAPI void rthsMeshSetBlendshapeCount(rths::MeshData *self, int num_bs);
rthsAPI void rthsMeshAddBlendshapeFrame(rths::MeshData *self, int bs_index, const rths::float3 *delta, float weight);

// mesh instance interface
rthsAPI rths::MeshInstanceData* rthsMeshInstanceCreate(rths::MeshData *mesh);
rthsAPI void rthsMeshInstanceRelease(rths::MeshInstanceData *self);
rthsAPI void rthsMeshInstanceSetName(rths::MeshInstanceData *self, const char *name);
rthsAPI void rthsMeshInstanceSetTransform(rths::MeshInstanceData *self, rths::float4x4 transform);
rthsAPI void rthsMeshInstanceSetBones(rths::MeshInstanceData *self, const rths::float4x4 *bones, int num_bones);
rthsAPI void rthsMeshInstanceSetBlendshapeWeights(rths::MeshInstanceData *self, const float *bsw, int num_bsw);

// render target interface
rthsAPI rths::RenderTargetData* rthsRenderTargetCreate();
rthsAPI void rthsRenderTargetRelease(rths::RenderTargetData *self);
rthsAPI void rthsRenderTargetSetName(rths::RenderTargetData *self, const char *name);
rthsAPI void rthsRenderTargetSetGPUTexture(rths::RenderTargetData *self, rths::GPUResourcePtr tex);
rthsAPI void rthsRenderTargetSetup(rths::RenderTargetData *self, int width, int height, rths::RenderTargetFormat format);

// renderer interface
rthsAPI rths::IRenderer* rthsRendererCreate();
rthsAPI void rthsRendererRelease(rths::IRenderer *self);
rthsAPI void rthsRendererSetName(rths::IRenderer *self, const char *name);
rthsAPI void rthsRendererSetRenderTarget(rths::IRenderer *self, rths::RenderTargetData *render_target);
rthsAPI void rthsRendererBeginScene(rths::IRenderer *self);
rthsAPI void rthsRendererEndScene(rths::IRenderer *self);
rthsAPI void rthsRendererSetRenderFlags(rths::IRenderer *self, uint32_t flag); // flag: combination of RenderFlag
rthsAPI void rthsRendererSetShadowRayOffset(rths::IRenderer *self, float v);
rthsAPI void rthsRendererSetSelfShadowThreshold(rths::IRenderer *self, float v);
rthsAPI void rthsRendererSetCamera(rths::IRenderer *self, rths::float3 pos, rths::float4x4 view, rths::float4x4 proj);
rthsAPI void rthsRendererAddDirectionalLight(rths::IRenderer *self, rths::float3 dir);
rthsAPI void rthsRendererAddSpotLight(rths::IRenderer *self, rths::float3 pos, rths::float3 dir, float range, float spot_angle);
rthsAPI void rthsRendererAddPointLight(rths::IRenderer *self, rths::float3 pos, float range);
rthsAPI void rthsRendererAddReversePointLight(rths::IRenderer *self, rths::float3 pos, float range);
rthsAPI void rthsRendererAddGeometry(rths::IRenderer *self, rths::MeshInstanceData *mesh, uint8_t mask = 0xff); // mask: combination of HitMask
rthsAPI void rthsRendererStartRender(rths::IRenderer *self);
rthsAPI void rthsRendererFinishRender(rths::IRenderer *self);
rthsAPI bool rthsRendererReadbackRenderTarget(rths::IRenderer *self, void *dst);
rthsAPI rths::GPUResourcePtr rthsRendererGetRenderTexturePtr(rths::IRenderer *self); // return raw texture ptr (ID3D12Resouce* etc)

rthsAPI void rthsMarkFrameBegin();
rthsAPI void rthsMarkFrameEnd();
// no need to call rthsMarkFrameBegin/End when use rthsRenderAll()
rthsAPI void rthsRenderAll();

#ifdef _WIN32
struct ID3D11Device;
struct ID3D12Device;
rthsAPI void rthsSetHostD3D11Device(ID3D11Device *device);
rthsAPI void rthsSetHostD3D12Device(ID3D12Device *device);
#endif // _WIN32
