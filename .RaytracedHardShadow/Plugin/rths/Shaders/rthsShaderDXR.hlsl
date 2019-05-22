#define kMaxLights 32

struct CameraData
{
    float4 position;
    float4 direction;
    float near_plane;
    float far_plane;
    float fov;
    float pad1[1];
};

struct DirectionalLightData
{
    float4 direction;
};

struct PointLightData
{
    float4 position;
};

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

struct RayPayload
{
    float shadow;
};

RaytracingAccelerationStructure gRtScene : register(t0, space0);
RWTexture2D<float> gOutput : register(u0);
ConstantBuffer<SceneData> gScene : register(b0);

[shader("raygeneration")]
void RayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();

    float2 crd = float2(launchIndex.xy);
    float2 dims = float2(launchDim.xy);

    float2 d = ((crd / dims) * 2.f - 1.f);
    float aspectRatio = dims.x / dims.y;

    RayDesc ray;
    ray.Origin = float3(0, 0, -2);
    ray.Direction = normalize(float3(d.x * aspectRatio, -d.y, 1));

    ray.TMin = 0;
    ray.TMax = 100000;

    RayPayload payload;
    TraceRay(gRtScene, 0, 0xFF, 0, 0, 0, ray, payload);
    gOutput[launchIndex.xy] = payload.shadow;
}

[shader("miss")]
void Miss(inout RayPayload payload : SV_RayPayload)
{
    payload.shadow = 1.0;
}

[shader("closesthit")]
void Hit(inout RayPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    payload.shadow = 0.0;
}
