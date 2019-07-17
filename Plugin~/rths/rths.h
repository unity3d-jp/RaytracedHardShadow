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

enum class DebugFlag : uint32_t
{
    Timestamp           = 0x01,
    NoLayerCompaction   = 0x02,
    ForceUpdateAS       = 0x04,
    PowerStableState    = 0x08,
};

enum class GlobalFlag : uint32_t
{
    DeferredInitialization = 0x01,
};

enum class RenderFlag : uint32_t
{
    CullBackFaces           = 0x00000001,
    FlipCasterFaces         = 0x00000002,
    IgnoreSelfShadow        = 0x00000004,
    KeepSelfDropShadow      = 0x00000008,
    AlphaTest               = 0x00000010,
    Transparent             = 0x00000020,
    AdaptiveSampling        = 0x00000100,
    Antialiasing            = 0x00000200,
    GPUSkinning             = 0x00010000,
    ClampBlendShapeWights   = 0x00020000,
    ParallelCommandList     = 0x00040000,
};

enum class InstanceFlag : uint32_t
{
    ReceiveShadows  = 0x01,
    ShadowsOnly     = 0x02,
    CastShadows     = 0x04,
    CullFront       = 0x10,
    CullBack        = 0x20,
    CullFrontShadow = 0x40,
    CullBackShadow  = 0x80,

    Default = ReceiveShadows | CastShadows | CullBack,
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
#endif // rthsImpl

using GPUResourcePtr = const void*;
using CPUResourcePtr = const void*;
class MeshData;
class MeshInstanceData;
class RenderTargetData;
class IRenderer;

} // namespace rths


// global info & settings
rthsAPI const char* rthsGetVersion();
rthsAPI const char* rthsGetReleaseDate();
rthsAPI const char* rthsGetErrorLog();
rthsAPI void rthsClearErrorLog();
rthsAPI uint32_t rthsGlobalsGetDebugFlags();
rthsAPI void rthsGlobalsSetDebugFlags(uint32_t v);
rthsAPI uint32_t rthsGlobalsGetFlags();
rthsAPI void rthsGlobalsSetFlags(uint32_t v);

// mesh interface
rthsAPI rths::MeshData* rthsMeshCreate();
rthsAPI void rthsMeshRelease(rths::MeshData *self);
rthsAPI bool rthsMeshIsRelocated(rths::MeshData *self);
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
rthsAPI void rthsMeshMarkDyncmic(rths::MeshData *self, bool v);

// mesh instance interface
rthsAPI rths::MeshInstanceData* rthsMeshInstanceCreate(rths::MeshData *mesh);
rthsAPI void rthsMeshInstanceRelease(rths::MeshInstanceData *self);
rthsAPI void rthsMeshInstanceSetName(rths::MeshInstanceData *self, const char *name);
rthsAPI void rthsMeshInstanceSetFlags(rths::MeshInstanceData *self, uint32_t flags);
rthsAPI void rthsMeshInstanceSetLayer(rths::MeshInstanceData *self, uint32_t layer);
rthsAPI void rthsMeshInstanceSetTransform(rths::MeshInstanceData *self, rths::float4x4 transform);
rthsAPI void rthsMeshInstanceSetBones(rths::MeshInstanceData *self, const rths::float4x4 *bones, int num_bones);
rthsAPI void rthsMeshInstanceSetBlendshapeWeights(rths::MeshInstanceData *self, const float *bsw, int num_bsw);

// render target interface
rthsAPI rths::RenderTargetData* rthsRenderTargetCreate();
rthsAPI void rthsRenderTargetRelease(rths::RenderTargetData *self);
rthsAPI bool rthsRenderTargetIsRelocated(rths::RenderTargetData *self);
rthsAPI void rthsRenderTargetSetName(rths::RenderTargetData *self, const char *name);
rthsAPI void rthsRenderTargetSetGPUTexture(rths::RenderTargetData *self, rths::GPUResourcePtr tex);
rthsAPI void rthsRenderTargetSetup(rths::RenderTargetData *self, int width, int height, rths::RenderTargetFormat format);

// renderer interface
rthsAPI rths::IRenderer* rthsRendererCreate();
rthsAPI void rthsRendererRelease(rths::IRenderer *self);
rthsAPI bool rthsRendererIsInitialized(rths::IRenderer *self);
rthsAPI bool rthsRendererIsValid(rths::IRenderer *self);
rthsAPI bool rthsRendererIsRendering(rths::IRenderer *self);
rthsAPI void rthsRendererSetName(rths::IRenderer *self, const char *name);
rthsAPI void rthsRendererSetRenderTarget(rths::IRenderer *self, rths::RenderTargetData *render_target);
rthsAPI void rthsRendererBeginScene(rths::IRenderer *self);
rthsAPI void rthsRendererEndScene(rths::IRenderer *self);
rthsAPI void rthsRendererSetRenderFlags(rths::IRenderer *self, uint32_t flag); // flag: combination of RenderFlag
rthsAPI void rthsRendererSetShadowRayOffset(rths::IRenderer *self, float v);
rthsAPI void rthsRendererSetSelfShadowThreshold(rths::IRenderer *self, float v);
rthsAPI void rthsRendererSetCamera(rths::IRenderer *self, rths::float3 pos, rths::float4x4 view, rths::float4x4 proj, uint32_t lmask = -1);
rthsAPI void rthsRendererAddDirectionalLight(rths::IRenderer *self, rths::float3 dir, uint32_t lmask = -1);
rthsAPI void rthsRendererAddSpotLight(rths::IRenderer *self, rths::float3 pos, rths::float3 dir, float range, float spot_angle, uint32_t lmask = -1);
rthsAPI void rthsRendererAddPointLight(rths::IRenderer *self, rths::float3 pos, float range, uint32_t lmask = -1);
rthsAPI void rthsRendererAddReversePointLight(rths::IRenderer *self, rths::float3 pos, float range, uint32_t lmask = -1);
rthsAPI void rthsRendererAddMesh(rths::IRenderer *self, rths::MeshInstanceData *mesh);
rthsAPI void rthsRendererStartRender(rths::IRenderer *self);
rthsAPI void rthsRendererFinishRender(rths::IRenderer *self);
rthsAPI bool rthsRendererReadbackRenderTarget(rths::IRenderer *self, void *dst);
rthsAPI const char* rthsRendererGetTimestampLog(rths::IRenderer *self);
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
