#pragma once

namespace rths {

struct float2 { float v[2]; };
struct float3 { float v[3]; };
struct float4 { float v[4]; };
struct float4x4 { float v[16]; };



struct LightData
{
    float4 position;
    float4 direction;
    float near_;
    float far_;
    float fov;
    float pad;
};

} // namespace rths
