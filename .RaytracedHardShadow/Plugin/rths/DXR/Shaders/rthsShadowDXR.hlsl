#define kMaxLights 32

enum LIGHT_TYPE
{
    LT_DIRECTIONAL = 1,
    LT_SPOT = 2,
    LT_POINT = 3,
    LT_REVERSE_POINT = 4,
};

enum RENDER_FLAG
{
    RF_CULL_BACK_FACES = 0x0001,
    RF_IGNORE_SELF_SHADOW = 0x0002,
    RF_KEEP_SELF_DROP_SHADOW = 0x0004,
};

enum HIT_MASK
{
    HM_RECEIVER = 0x01,
    HM_CASTER = 0x02,
    HM_BOTH = HM_RECEIVER | HM_CASTER,
};

struct CameraData
{
    float4x4 view;
    float4x4 proj;
    float4 position;
    float near_plane;
    float far_plane;
    float2 pad1;
};

struct LightData
{
    uint light_type;
    uint3 pad1;

    float3 position;
    float range;
    float3 direction;
    float spot_angle; // radian
};

struct SceneData
{
    uint render_flags;
    uint light_count;
    float shadow_ray_offset;
    float self_shadow_threshold;

    CameraData camera;
    LightData lights[kMaxLights];
};

struct InstanceData
{
    uint related_caster_mask;
};


RWTexture2D<float> gOutput : register(u0);
RaytracingAccelerationStructure gRtScene : register(t0);
StructuredBuffer<InstanceData> gInstanceData : register(t1);
ConstantBuffer<SceneData> gScene : register(b0);

float3 CameraPosition() { return gScene.camera.position.xyz; }
float3 CameraRight() { return gScene.camera.view[0].xyz; }
float3 CameraUp() { return gScene.camera.view[1].xyz; }
float3 CameraForward() { return -gScene.camera.view[2].xyz; }
float CameraFocalLength() { return abs(gScene.camera.proj[1][1]); }
float CameraNearPlane() { return gScene.camera.near_plane; }
float CameraFarPlane() { return gScene.camera.far_plane; }

int RenderFlags() { return gScene.render_flags; }
float ShadowRayOffset() { return gScene.shadow_ray_offset; }
float SelfShadowThreshold() { return gScene.self_shadow_threshold; }

int LightCount() { return gScene.light_count; }
LightData GetLight(int i) { return gScene.lights[i]; }

float3 HitPosition() { return WorldRayOrigin() + WorldRayDirection() * (RayTCurrent() - ShadowRayOffset()); }
uint RelatedCasterMask() { return gInstanceData[InstanceID()].related_caster_mask; }

// a & b must be normalized
float angle_between(float3 a, float3 b) { return acos(clamp(dot(a, b), 0, 1)); }




struct RayPayload
{
    float shadow;
    uint instance_id;     // instance id for first ray
};

[shader("raygeneration")]
void RayGen()
{
    uint2 screen_idx = DispatchRaysIndex().xy;
    uint2 screen_dim = DispatchRaysDimensions().xy;

    float aspect_ratio = (float)screen_dim.x / (float)screen_dim.y;
    float2 screen_pos = ((float2(screen_idx) + 0.5f) / float2(screen_dim)) * 2.0f - 1.0f;
    screen_pos.x *= aspect_ratio;

    RayDesc ray;
    ray.Origin = CameraPosition();
    ray.Direction = normalize(
        CameraRight() * screen_pos.x +
        CameraUp() * screen_pos.y +
        CameraForward() * CameraFocalLength());
    ray.TMin = CameraNearPlane(); // 
    ray.TMax = CameraFarPlane();  // todo: correct this

    RayPayload payload;
    payload.shadow = 0.0;

    int render_flags = RenderFlags();
    int ray_flags = RAY_FLAG_FORCE_OPAQUE;
    if (render_flags & RF_CULL_BACK_FACES)
        ray_flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

    TraceRay(gRtScene, ray_flags, HM_RECEIVER, 0, 0, 0, ray, payload);
    gOutput[screen_idx.xy] = payload.shadow;
}

[shader("miss")]
void Miss1(inout RayPayload payload : SV_RayPayload)
{
    // nothing todo here
}

[shader("miss")]
void Miss2(inout RayPayload payload : SV_RayPayload)
{
    payload.shadow += (1.0f / LightCount());
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    payload.instance_id = InstanceID();

    // shoot shadow ray (hit position -> light)

    int render_flags = RenderFlags();
    int ray_flags = 0;
    if (render_flags & RF_CULL_BACK_FACES)
        ray_flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;
    if (render_flags & RF_IGNORE_SELF_SHADOW)
        ray_flags |= RAY_FLAG_FORCE_NON_OPAQUE; // calling any hit shader require non-opaque flag
    else
        ray_flags |= RAY_FLAG_FORCE_OPAQUE & RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

    int li;
    for (li = 0; li < LightCount(); ++li) {
        LightData light = GetLight(li);

        if (light.light_type == LT_DIRECTIONAL) {
            // directional light
            RayDesc ray;
            ray.Origin = HitPosition();
            ray.Direction = -light.direction.xyz;
            ray.TMin = 0.0f;
            ray.TMax = CameraFarPlane();
            TraceRay(gRtScene, ray_flags, RelatedCasterMask(), 1, 0, 1, ray, payload);
        }
        else if (light.light_type == LT_SPOT) {
            // spot light
            float3 pos = HitPosition();
            float3 dir = normalize(light.position - pos);
            float distance = length(light.position - pos);
            if (distance <= light.range && angle_between(-dir, light.direction) * 2.0f <= light.spot_angle) {
                RayDesc ray;
                ray.Origin = pos;
                ray.Direction = dir;
                ray.TMin = 0.0f;
                ray.TMax = distance;
                TraceRay(gRtScene, ray_flags, RelatedCasterMask(), 1, 0, 1, ray, payload);
            }
        }
        else if (light.light_type == LT_POINT) {
            // point light
            float3 pos = HitPosition();
            float3 dir = normalize(light.position - pos);
            float distance = length(light.position - pos);

            if (distance <= light.range) {
                RayDesc ray;
                ray.Origin = pos;
                ray.Direction = dir;
                ray.TMin = 0.0f;
                ray.TMax = distance;
                TraceRay(gRtScene, ray_flags, RelatedCasterMask(), 1, 0, 1, ray, payload);
            }
        }
        else if (light.light_type == LT_REVERSE_POINT) {
            // reverse point light
            float3 pos = HitPosition();
            float3 dir = normalize(light.position - pos);
            float distance = length(light.position - pos);

            if (distance <= light.range) {
                RayDesc ray;
                ray.Origin = pos;
                ray.Direction = -dir;
                ray.TMin = 0.0f;
                ray.TMax = light.range - distance;
                TraceRay(gRtScene, ray_flags, RelatedCasterMask(), 1, 0, 1, ray, payload);
            }
        }
    }
}

[shader("anyhit")]
void AnyHit(inout RayPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    // this shader is called only when 'ignore self shadow' is enabled.

    // this condition means:
    // - always ignore zero-distance shadow
    //   (comparing instance ID is not enough because in some cases meshes are separated but seamlessly continuous. e.g. head and body)
    // - always ignore shadow cast by self back faces (relevanet only when 'cull back faces' is disabled)
    // - ignore non-zero-distance self shadow if 'keep self drop shadow' is disabled
    if ( RayTCurrent() < SelfShadowThreshold() ||
        (payload.instance_id == InstanceID() && ((RenderFlags() & RF_KEEP_SELF_DROP_SHADOW) == 0 || HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)) )
    {
        IgnoreHit();
        return;
    }
    AcceptHitAndEndSearch();
}
