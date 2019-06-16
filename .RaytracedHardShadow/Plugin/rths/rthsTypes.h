#pragma once

namespace rths {

constexpr float PI = 3.14159265358979323846264338327950288419716939937510f;
constexpr float DegToRad = PI / 180.0f;
constexpr float RadToDeg = 1.0f / (PI / 180.0f);

using nanosec = uint64_t;

struct int2
{
    int x, y;

    int& operator[](size_t i) { return ((int*)this)[i]; }
    const int& operator[](size_t i) const { return ((int*)this)[i]; }
};

struct int3
{
    int x, y, z;

    int& operator[](size_t i) { return ((int*)this)[i]; }
    const int& operator[](size_t i) const { return ((int*)this)[i]; }
};

struct int4
{
    int x, y, z, w;

    int& operator[](size_t i) { return ((int*)this)[i]; }
    const int& operator[](size_t i) const { return ((int*)this)[i]; }
};

struct float2
{
    float x,y;

    float& operator[](size_t i) { return ((float*)this)[i]; }
    const float& operator[](size_t i) const { return ((float*)this)[i]; }
};

struct float3
{
    float x,y,z;

    float& operator[](size_t i) { return ((float*)this)[i]; }
    const float& operator[](size_t i) const { return ((float*)this)[i]; }
};

struct float4
{
    float x,y,z,w;

    float& operator[](size_t i) { return ((float*)this)[i]; }
    const float& operator[](size_t i) const { return ((float*)this)[i]; }
};

struct float3x4
{
    float4 v[3];

    float4& operator[](size_t i) { return v[i]; }
    const float4& operator[](size_t i) const { return v[i]; }
};

struct float4x4
{
    float4 v[4];

    float4& operator[](size_t i) { return v[i]; }
    const float4& operator[](size_t i) const { return v[i]; }
};


inline float3 operator-(const float3& l) { return{ -l.x, -l.y, -l.z }; }
inline float3 operator*(const float3& l, float r) { return{ l.x * r, l.y * r, l.z * r }; }
inline float3 operator/(const float3& l, float r) { return{ l.x / r, l.y / r, l.z / r }; }

inline float dot(const float3& l, const float3& r) { return l.x*r.x + l.y*r.y + l.z*r.z; }
inline float length_sq(const float3& v) { return dot(v, v); }
inline float length(const float3& v) { return sqrt(length_sq(v)); }
inline float3 normalize(const float3& v) { return v / length(v); }

inline float4 to_float4(const float3& xyz, float w)
{
    return{ xyz.x, xyz.y, xyz.z, w };
}

inline float3x4 to_float3x4(const float4x4& v)
{
    // copy with transpose
    return float3x4{ {
        {v[0][0], v[1][0], v[2][0], v[3][0]},
        {v[0][1], v[1][1], v[2][1], v[3][1]},
        {v[0][2], v[1][2], v[2][2], v[3][2]},
    } };
}
inline float3 extract_position(const float4x4& m)
{
    return (const float3&)m[3];
}
inline float3 extract_direction(const float4x4& m)
{
    return normalize((const float3&)m[2]);
}

float4x4 operator*(const float4x4 &a, const float4x4 &b);
float4x4 invert(const float4x4& x);



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

enum class RaytraceFlags : int
{
    IgnoreSelfShadow = 1,
    KeepSelfDropShadow = 2,
    GPUSkinning = 4,
};

enum class LightType : int
{
    Directional = 1,
    Spot        = 2,
    Point       = 3,
    ReversePoint= 4,
};

struct LightData
{
    LightType light_type{};
    int pad[3];

    float3 position{};
    float range{};
    float3 direction{};
    float spot_angle{}; // radian
};


#define kMaxLights 32

struct SceneData
{
    CameraData camera;

    int raytrace_flags;
    int light_count;
    int pad1[2];

    float shadow_ray_offset;
    float self_shadow_threshold;
    float pad2[2];

    LightData lights[kMaxLights];
};

using GPUResourcePtr = void*;

using TextureData = GPUResourcePtr;
using BufferData = GPUResourcePtr;

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
    const float4x4 *bindposes;
    const uint8_t *bone_counts;
    const BoneWeight1 *weights1;
    const BoneWeight4 *weights4;
    int num_bones;
    int num_bone_counts;
    int num_weights1;
    int num_weights4;

    bool operator==(const SkinData& v) const;
    bool operator<(const SkinData& v) const;
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

    bool operator==(const MeshData& v) const;
    bool operator<(const MeshData& v) const;
};

struct MeshInstanceData
{
    MeshData mesh{};
    float4x4 transform{};
    BonesData bones{};
    BlendshapeWeightData blendshape_weights{};
};

} // namespace rths
