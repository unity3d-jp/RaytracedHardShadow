#define kMaxLights 32

enum LightType
{
    LIGHT_TYPE_DIRECTIONAL   = 1,
    LIGHT_TYPE_SPOT          = 2,
    LIGHT_TYPE_POINT         = 3,
    LIGHT_TYPE_REVERSE_POINT = 4,
};

enum RaytraceFlags
{
    RTFLAG_IGNORE_SELF_SHADOW = 1,
    RTFLAG_KEEP_SELF_DROP_SHADOW = 2,
};

struct CameraData
{
    float4x4 view;
    float4x4 proj;
    float4 position;
    float near_plane;
    float far_plane;
    float fov;
    float pad1;
};

struct LightData
{
    int light_type;
    int3 pad1;

    float3 position;
    float range;
    float3 direction;
    float spot_angle; // radian
};

struct SceneData
{
    CameraData camera;

    int raytrace_flags;
    int light_count;
    int2 pad1;

    float shadow_ray_offset;
    float self_shadow_threshold;
    float2 pad2;

    LightData lights[kMaxLights];
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

int RaytraceFlags() { return gScene.raytrace_flags; }
float ShadowRayOffset() { return gScene.shadow_ray_offset; }
float SelfShadowThreshold() { return gScene.self_shadow_threshold; }

int LightCount() { return gScene.light_count; }
LightData GetLight(int i) { return gScene.lights[i]; }

float3 HitPosition() { return WorldRayOrigin() + WorldRayDirection() * (RayTCurrent() - ShadowRayOffset()); }

// a & b must be normalized
float angle_between(float3 a, float3 b) { return acos(clamp(dot(a, b), 0, 1)); }
