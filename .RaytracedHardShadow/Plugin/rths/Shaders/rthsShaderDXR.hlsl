#define kMaxLights 16
#define kRayOffset 0.0001f

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

struct DirectionalLightData
{
    float4 direction;
};

struct SpotLightData
{
    float3 position;
    float range;
    float3 direction;
    float spot_angle; // radian
};

struct PointLightData
{
    float3 position;
    float range;
};

struct SceneData
{
    CameraData camera;

    int directional_light_count;
    int spot_light_count;
    int point_light_count;
    int reverse_point_light_count;

    DirectionalLightData directional_lights[kMaxLights];
    SpotLightData        spot_lights[kMaxLights];
    PointLightData       point_lights[kMaxLights];
    PointLightData       reverse_point_lights[kMaxLights];
};

struct RayPayload
{
    float shadow;
    int recursion;
    int light_index;
};

RWTexture2D<float> gOutput : register(u0);
RaytracingAccelerationStructure gRtScene : register(t0);
ConstantBuffer<SceneData> gScene : register(b0);

float3 CameraPosition()   { return gScene.camera.position.xyz; }
float3 CameraRight()      { return gScene.camera.view[0].xyz; }
float3 CameraUp()         { return gScene.camera.view[1].xyz; }
float3 CameraForward()    { return -gScene.camera.view[2].xyz; }
float CameraFocalLength() { return abs(gScene.camera.proj[1][1]); }
float CameraNearPlane()   { return gScene.camera.near_plane; }
float CameraFarPlane()    { return gScene.camera.far_plane; }

int DirectionalLightCount()  { return gScene.directional_light_count; }
int SpotLightCount()         { return gScene.spot_light_count; }
int PointLightCount()        { return gScene.point_light_count; }
int ReversePointLightCount() { return gScene.reverse_point_light_count; }
int LightCount()             { return DirectionalLightCount() + SpotLightCount() + PointLightCount() + ReversePointLightCount(); }

DirectionalLightData DirectionalLight(int i)  { return gScene.directional_lights[i]; }
SpotLightData        SpotLight(int i)         { return gScene.spot_lights[i]; }
PointLightData       PointLight(int i)        { return gScene.point_lights[i]; }
PointLightData       ReversePointLight(int i) { return gScene.reverse_point_lights[i]; }

float3 HitPosition() { return WorldRayOrigin() + WorldRayDirection() * (RayTCurrent() - kRayOffset); }

// a & b must be normalized
float angle_between(float3 a, float3 b) { return acos(clamp(dot(a, b), 0, 1)); }



[shader("raygeneration")]
void RayGen()
{
    uint2 screen_idx = DispatchRaysIndex().xy;
    uint2 screen_dim = DispatchRaysDimensions().xy;

    float aspect_ratio = (float)screen_dim.x / (float)screen_dim.y;
    float2 screen_pos = ((float2(screen_idx) / float2(screen_dim)) * 2.0f - 1.0f);
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
    payload.recursion = 0;
    payload.light_index = 0;

    TraceRay(gRtScene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 0, 0, ray, payload);
    gOutput[screen_idx.xy] = payload.shadow;
}

[shader("miss")]
void Miss(inout RayPayload payload : SV_RayPayload)
{
    if (payload.recursion == 1) {
        payload.shadow += (1.0f / LightCount());
    }
}

[shader("closesthit")]
void Hit(inout RayPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    if (payload.recursion == 0) {
        payload.recursion += 1;

        int li;
        RayDesc ray;

        // directional lights
        for (li = 0; li < DirectionalLightCount(); ++li) {
            DirectionalLightData light = DirectionalLight(li);
            ray.Origin = HitPosition();
            ray.Direction = -light.direction.xyz;
            ray.TMin = 0.0f;
            ray.TMax = CameraFarPlane();
            TraceRay(gRtScene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 0, 0, 0, ray, payload);
            ++payload.light_index;
        }

        // spot lights
        for (li = 0; li < SpotLightCount(); ++li) {
            SpotLightData light = SpotLight(li);
            float3 pos = HitPosition();
            float3 dir = normalize(light.position - pos);
            float distance = length(light.position - pos);
            if (angle_between(-dir, light.direction) * 2.0f <= light.spot_angle && distance < light.range) {
                ray.Origin = pos;
                ray.Direction = dir;
                ray.TMin = 0.0f;
                ray.TMax = distance;
                TraceRay(gRtScene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 0, 0, 0, ray, payload);
            }
            ++payload.light_index;
        }

        // point lights
        for (li = 0; li < PointLightCount(); ++li) {
            PointLightData light = PointLight(li);
            float3 pos = HitPosition();
            float3 dir = normalize(light.position - pos);
            float distance = length(light.position - pos);

            if (distance < light.range) {
                ray.Origin = pos;
                ray.Direction = dir;
                ray.TMin = 0.0f;
                ray.TMax = distance;
                TraceRay(gRtScene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 0, 0, 0, ray, payload);
            }
            ++payload.light_index;
        }

        // reverse point lights
        for (li = 0; li < ReversePointLightCount(); ++li) {
            PointLightData light = ReversePointLight(li);
            float3 pos = HitPosition();
            float3 dir = normalize(light.position - pos);
            float distance = length(light.position - pos);

            if (distance < light.range) {
                ray.Origin = light.position + (dir * light.range);
                ray.Direction = -dir;
                ray.TMin = 0.0f;
                ray.TMax = light.range - distance;
                TraceRay(gRtScene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 0, 0, 0, ray, payload);
            }
            ++payload.light_index;
        }
    }
    else {
         // nothing to do for now
    }
}
