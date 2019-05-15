#pragma once

namespace rths {

struct float2 { float v[2]; };
struct float3 { float v[3]; };
struct float4 { float v[4]; };
struct float3x4 { float v[12]; };
struct float4x4 { float v[16]; };

inline float3x4 to_float3x4(const float4x4& src)
{
    auto v = src.v;
    return {
        v[0], v[4],  v[8], v[12],
        v[1], v[5],  v[9], v[13],
        v[2], v[6], v[10], v[14],
    };
}


struct CameraData
{
    union {
        float3 position;
        float4 position4;
    };
    union {
        float3 direction;
        float4 direction4;
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

struct SceneData
{
    CameraData camera;

    int directional_light_count;
    int point_light_count;
    int reverse_point_light_count;
    int pad1[1];

    DirectionalLightData directional_lights[32];
    PointLightData point_lights[32];
    PointLightData reverse_point_lights[32];
};

} // namespace rths
