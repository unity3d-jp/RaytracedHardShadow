#pragma once

namespace rths {

constexpr float PI = 3.14159265358979323846264338327950288419716939937510f;
constexpr float DegToRad = PI / 180.0f;
constexpr float RadToDeg = 1.0f / (PI / 180.0f);

using nanosec = uint64_t;

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

using TextureData = void*;
using BufferData = void*;

struct MeshData
{
    void *vertex_buffer = nullptr; // host
    void *index_buffer = nullptr; // host
    int vertex_stride = 0; // if 0, treated as size_of_vertex_buffer / vertex_count
    int vertex_count = 0;
    int vertex_offset = 0; // in byte
    int index_stride = 0;
    int index_count = 0;
    int index_offset = 0; // in byte
    bool is_dynamic = false; // true if skinned, has blendshapes, etc

    bool operator==(const MeshData& v) const;
    bool operator<(const MeshData& v) const;
};


struct SkinWeight1
{
    float weight;
    int index;
};

struct SkinWeight4
{
    float weight[4];
    int index[4];
};

struct SkinData
{
    const uint8_t *bone_counts;
    const SkinWeight1 *weights1;
    const SkinWeight4 *weights4;
    const float4x4 *matrices;
    int num_bone_counts;
    int num_weights1;
    int num_weights4;
    int num_matrices;

    bool operator==(const SkinData& v) const;
    bool operator<(const SkinData& v) const;
};

struct SkinDataHolder
{
    std::vector<uint8_t> bone_counts;
    std::vector<SkinWeight1> weights1;
    std::vector<SkinWeight4> weights4;
    std::vector<float4x4> matrices;

    void assign(const SkinData& v);
};


struct MeshInstanceData
{
    MeshData mesh;
    SkinData skin;
    float4x4 transform;
};

} // namespace rths
