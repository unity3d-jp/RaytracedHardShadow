struct SceneData
{
    float4x4 camera_transform;
    float camera_near;
    float camera_far;
    float camera_fov;
    float pad1[1];

    int directional_light_count;
    int point_light_count;
    int reverse_point_light_count;
};

struct RayPayload
{
    float shadow;
};

RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float> gOutput : register(u0);
ConstantBuffer<SceneData> gScene : register(b0);

[shader("raygeneration")]
void rayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();

    float2 crd = float2(launchIndex.xy);
    float2 dims = float2(launchDim.xy);

    float2 d = ((crd/dims) * 2.f - 1.f);
    float aspectRatio = dims.x / dims.y;

    RayDesc ray;
    ray.Origin = float3(0, 0, -2);
    ray.Direction = normalize(float3(d.x * aspectRatio, -d.y, 1));

    ray.TMin = 0;
    ray.TMax = 100000;

    RayPayload payload;
    TraceRay( gRtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 0, 0, ray, payload );
    gOutput[launchIndex.xy] = payload.shadow;
}

[shader("miss")]
void miss(inout RayPayload payload)
{
    payload.shadow = 1.0;
}

[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    payload.shadow = 1.0;
}
