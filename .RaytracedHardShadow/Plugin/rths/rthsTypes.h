#pragma once
#include "rthsSettings.h"
#include "Foundation/rthsRefPtr.h"
#include "Foundation/rthsMath.h"
#include "Foundation/rthsHalf.h"

namespace rths {

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
    DbgTimestamp            = 0x01000000,
    DbgForceUpdateAS        = 0x02000000,
};

enum class LightType : uint32_t
{
    Directional = 1,
    Spot        = 2,
    Point       = 3,
    ReversePoint= 4,
};

enum class HitMask : uint8_t
{
    Receiver    = 0x01,
    Caster      = 0x02,
    Both        = Receiver | Caster,
    AllCaster   = 0xfe,
};

enum class UpdateFlag : uint32_t
{
    None = 0,
    Transform = 1,
    Blendshape = 2,
    Bones = 4,

    Any = Transform | Blendshape | Bones,
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

struct CameraData
{
    float4x4 view;
    float4x4 proj;
    union {
        float3 position;
        float4 position4;
    };
    float near_plane;
    float far_plane;
    float2 pad1;
};

struct LightData
{
    LightType light_type{};
    uint32_t pad[3];

    float3 position{};
    float range{};
    float3 direction{};
    float spot_angle{}; // radian
};

#define kMaxLights 32

struct SceneData
{
    uint32_t render_flags; // combination of RenderFlag
    uint32_t light_count;
    float shadow_ray_offset;
    float self_shadow_threshold;

    CameraData camera;
    LightData lights[kMaxLights];

    bool operator==(SceneData& v) const { return std::memcmp(this, &v, sizeof(*this)) == 0; }
    bool operator!=(SceneData& v) const { return !(*this == v); }
};

struct GlobalSettings
{
    bool deferred_initilization = false;
};

GlobalSettings& GetGlobals();
void AddDeferredCommand(const std::function<void()>& v);
void FlushDeferredCommands();

using GPUResourcePtr = const void*;
using CPUResourcePtr = const void*;

class DeviceMeshData
{
public:
    virtual ~DeviceMeshData() {};
    virtual bool valid() const = 0;
    virtual bool isRelocated() const = 0;
};

class DeviceMeshInstanceData
{
public:
    virtual ~DeviceMeshInstanceData() {};
    virtual bool valid() const = 0;
};

class DeviceRenderTargetData
{
public:
    virtual ~DeviceRenderTargetData() {};
    virtual bool valid() const = 0;
    virtual bool isRelocated() const = 0;
};


struct BoneWeight1
{
    float weight = 0.0f;
    int index = 0;
};
struct BoneWeight4
{
    float weight[4]{};
    int index[4]{};
};
struct SkinData
{
    std::vector<float4x4>    bindposes;
    std::vector<uint8_t>     bone_counts;
    std::vector<BoneWeight1> weights;

    bool valid() const;
};

struct BlendshapeFrameData
{
    std::vector<float3> delta;
    float weight = 0.0f;
};
struct BlendshapeData
{
    std::vector<BlendshapeFrameData> frames;
};


class MeshData : public SharedResource<MeshData>
{
public:
    std::string name;
    GPUResourcePtr gpu_vertex_buffer = nullptr;
    GPUResourcePtr gpu_index_buffer = nullptr;
    CPUResourcePtr cpu_vertex_buffer = nullptr;
    CPUResourcePtr cpu_index_buffer = nullptr;
    int vertex_stride = 0; // if 0, treated as size_of_vertex_buffer / vertex_count
    int vertex_count = 0;
    int vertex_offset = 0; // in byte
    int index_stride = 0;
    int index_count = 0;
    int index_offset = 0; // in byte
    SkinData skin;
    std::vector<BlendshapeData> blendshapes;
    bool is_dynamic = false;

    DeviceMeshData *device_data = nullptr;

    MeshData();
    ~MeshData();
    void release();
    bool valid() const;
    bool isRelocated() const;
};
using MeshDataPtr = ref_ptr<MeshData>;


class MeshInstanceData : public SharedResource<MeshInstanceData>
{
public:
    std::string name;
    MeshDataPtr mesh;
    float4x4 transform = float4x4::identity();
    std::vector<float4x4> bones;
    std::vector<float> blendshape_weights;
    uint32_t update_flags = 0; // combination of UpdateFlag

    DeviceMeshInstanceData *device_data = nullptr;

    MeshInstanceData();
    ~MeshInstanceData();
    void release();
    bool valid() const;
    bool isUpdated(UpdateFlag v) const;
    void markUpdated(UpdateFlag v);
    void markUpdated(); // for debug
    void setTransform(const float4x4& v);
    void setBones(const float4x4 *v, size_t n);
    void setBlendshapeWeights(const float *v, size_t n);
};
using MeshInstanceDataPtr = ref_ptr<MeshInstanceData>;


// one instance may be rendered multiple times with different hit mask in one frame.
// (e.g. one renderer render the object as a receiver, another one render it as a caster)
// so, hit mask must be separated from MeshInstanceData.
class GeometryData
{
public:
    MeshInstanceDataPtr instance;
    uint8_t receive_mask;
    uint8_t cast_mask;

    bool valid() const;
    void markUpdated(); // for debug
};


class RenderTargetData : public SharedResource<RenderTargetData>
{
public:
    std::string name;
    GPUResourcePtr gpu_texture = nullptr;
    int width = 0;
    int height = 0;
    RenderTargetFormat format = RenderTargetFormat::Unknown;

    DeviceRenderTargetData *device_data = nullptr;


    RenderTargetData();
    ~RenderTargetData();
    void release();
    bool isRelocated() const;
};
using RenderTargetDataPtr = ref_ptr<RenderTargetData>;



template<class T>
inline void ExternalRelease(T *self)
{
    if (GetGlobals().deferred_initilization)
        AddDeferredCommand([self]() { self->internalRelease(); });
    else
        self->internalRelease();
}

} // namespace rths
