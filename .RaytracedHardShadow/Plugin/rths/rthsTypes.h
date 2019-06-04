#pragma once

namespace rths {

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
    float pad1[1];
};

struct DirectionalLightData
{
    union {
        float3 direction;
        float4 direction4;
    };
};

struct PointLightData
{
    union {
        float3 position;
        float4 position4;
    };
};

#define kMaxLights 32
struct SceneData
{
    CameraData camera;

    int directional_light_count;
    int point_light_count;
    int reverse_point_light_count;
    int pad1[1];

    DirectionalLightData directional_lights[kMaxLights];
    PointLightData point_lights[kMaxLights];
    PointLightData reverse_point_lights[kMaxLights];
};

} // namespace rths
