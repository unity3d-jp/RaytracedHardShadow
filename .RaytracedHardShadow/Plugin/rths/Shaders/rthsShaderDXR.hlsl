#define kMaxLights 32

struct CameraData
{
    float4x4 view;
    float4x4 proj;
    float4 position;
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

RWTexture2D<float> gOutput : register(u0);
RaytracingAccelerationStructure gRtScene : register(t0);
ConstantBuffer<SceneData> gScene : register(b0);

float3 CameraPosition() { return gScene.camera.position.xyz; }
float3 CameraRight() { return gScene.camera.view[0].xyz; }
float3 CameraUp() { return gScene.camera.view[1].xyz; }
float3 CameraForward() { return -gScene.camera.view[2].xyz; }
float CameraFocalLength() { return abs(gScene.camera.proj[1][1]); }
float CameraNearPlane() { return gScene.camera.near_plane; }
float CameraFarPlane() { return gScene.camera.far_plane; }


[shader("raygeneration")]
void RayGen()
{
    uint3 screen_idx = DispatchRaysIndex();
    uint3 screen_dim = DispatchRaysDimensions();

    float aspect_ratio = (float)screen_dim.x / (float)screen_dim.y;
    float2 screen_pos = ((float2(screen_idx.xy) / float2(screen_dim.xy)) * 2.0f - 1.0f);
    screen_pos.x *= aspect_ratio;

    RayDesc ray;
    ray.Origin = CameraPosition();
    ray.Direction = normalize(
        CameraRight() * screen_pos.x +
        CameraUp() * screen_pos.y +
        CameraForward() * CameraFocalLength());
    ray.TMin = CameraNearPlane();
    ray.TMax = CameraFarPlane();

    RayPayload payload;
    payload.shadow = 0.0;

    TraceRay(gRtScene, 0, 0xFF, 0, 0, 0, ray, payload);
    gOutput[screen_idx.xy] = payload.shadow;
}

[shader("miss")]
void Miss(inout RayPayload payload : SV_RayPayload)
{
    // nothing to do for now
}

[shader("closesthit")]
void Hit(inout RayPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    payload.shadow = 1.0;
}
