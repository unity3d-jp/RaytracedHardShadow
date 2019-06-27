#pragma once
#include "Foundation/rthsRefPtr.h"
#include "Foundation/rthsMath.h"
#include "Foundation/rthsHalf.h"

namespace rths {

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
    float fov;
    float pad1;
};

enum class RenderFlag : uint32_t
{
    CullBackFace            = 0x0001,
    IgnoreSelfShadow        = 0x0002,
    KeepSelfDropShadow      = 0x0004,
    GPUSkinning             = 0x0100,
    ClampBlendShapeWights   = 0x0200,
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
    Receiver    = 0x0001,
    Caster      = 0x0002,
    All = Receiver | Caster,
};

enum class UpdateFlag : uint32_t
{
    None = 0,
    Transform = 1,
    Blendshape = 2,
    Bone = 4,
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
    CameraData camera;

    uint32_t render_flags; // combination of RenderFlag
    uint32_t light_count;
    uint32_t pad1[2];

    float shadow_ray_offset;
    float self_shadow_threshold;
    float pad2[2];

    LightData lights[kMaxLights];

    bool operator==(SceneData& v) const { return std::memcmp(this, &v, sizeof(*this)) == 0; }
    bool operator!=(SceneData& v) const { return !(*this == v); }
};

using GPUResourcePtr = void*;

using TextureData = GPUResourcePtr;
using BufferData = GPUResourcePtr;

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


class MeshData;
using MeshDataCallback = std::function<void(MeshData*)>;

class MeshData
{
public:
    GPUResourcePtr gpu_vertex_buffer = nullptr; // host
    GPUResourcePtr gpu_index_buffer = nullptr; // host
    void *cpu_vertex_buffer = nullptr;
    void *cpu_index_buffer = nullptr;
    int vertex_stride = 0; // if 0, treated as size_of_vertex_buffer / vertex_count
    int vertex_count = 0;
    int vertex_offset = 0; // in byte
    int index_stride = 0;
    int index_count = 0;
    int index_offset = 0; // in byte
    SkinData skin;
    std::vector<BlendshapeData> blendshapes;

    MeshData();
    ~MeshData();
    void addref();
    void release();
    bool valid() const;
    bool operator==(const MeshData& v) const;
    bool operator!=(const MeshData& v) const;
    bool operator<(const MeshData& v) const;

private:
    uint64_t id = 0;
    std::atomic_int ref_count = 1;
};
using MeshDataPtr = ref_ptr<MeshData>;


class MeshInstanceData;
using MeshInstanceDataCallback = std::function<void(MeshInstanceData*)>;

class MeshInstanceData
{
public:
    MeshDataPtr mesh;
    float4x4 transform = float4x4::identity();
    std::vector<float4x4> bones;
    std::vector<float> blendshape_weights;
    uint32_t update_flags = 0; // combination of UpdateFlag

    MeshInstanceData();
    ~MeshInstanceData();
    void addref();
    void release();
    bool valid() const;
    bool operator==(const MeshInstanceData& v) const;
    bool operator!=(const MeshInstanceData& v) const;
    bool operator<(const MeshInstanceData& v) const;

private:
    uint64_t id = 0;
    std::atomic_int ref_count = 1;
};
using MeshInstanceDataPtr = ref_ptr<MeshInstanceData>;


// one instance may be rendered multiple times with different hit mask in one frame.
// (e.g. one renderer render the object as a receiver, another one render it as a caster)
// so, hit mask must be separated from MeshInstanceData.
class GeometryData
{
public:
    MeshInstanceDataPtr instance;
    uint8_t hit_mask; // combination of HitMask

    bool valid() const;
};

} // namespace rths
