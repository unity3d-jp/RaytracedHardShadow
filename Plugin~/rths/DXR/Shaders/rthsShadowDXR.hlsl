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
    RF_CULL_BACK_FACES      = 0x00000001,
    RF_FLIP_CASTER_FACES    = 0x00000002,
    RF_IGNORE_SELF_SHADOW   = 0x00000004,
    RF_KEEP_SELF_DROP_SHADOW= 0x00000008,
};

enum INSTANCE_FLAG
{
    IF_VISIBLE_FROM_CAMERAS = 0x01,
    IF_VISIBLE_FROM_LIGHTS  = 0x02,
    IF_RECEIVE_SHADOWS      = 0x04,
};

struct CameraData
{
    float4x4 view;
    float4x4 proj;
    float4 position;
    float near_plane;
    float far_plane;
    uint layer_mask_cpu;
    uint layer_mask_gpu;
};

struct LightData
{
    uint light_type;
    uint layer_mask_cpu;
    uint layer_mask_gpu;
    uint1 pad1;

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
    uint instance_flags;
};


// slot 0
RWTexture2D<float> g_output : register(u0);

// slot 1
RaytracingAccelerationStructure g_TLAS : register(t0);
StructuredBuffer<InstanceData> g_instance_data : register(t1);
ConstantBuffer<SceneData> g_scene_data : register(b0);

// slot 2
Texture2D<float> g_prev_result : register(t2);


float3 CameraPosition() { return g_scene_data.camera.position.xyz; }
float3 CameraRight()    { return g_scene_data.camera.view[0].xyz; }
float3 CameraUp()       { return g_scene_data.camera.view[1].xyz; }
float3 CameraForward()  { return -g_scene_data.camera.view[2].xyz; }
float CameraFocalLength()   { return abs(g_scene_data.camera.proj[1][1]); }
float CameraNearPlane()     { return g_scene_data.camera.near_plane; }
float CameraFarPlane() { return g_scene_data.camera.far_plane; }
uint CameraLayerMask() { return g_scene_data.camera.layer_mask_gpu; }

int   RenderFlags()         { return g_scene_data.render_flags; }
float ShadowRayOffset()     { return g_scene_data.shadow_ray_offset; }
float SelfShadowThreshold() { return g_scene_data.self_shadow_threshold; }

int LightCount() { return g_scene_data.light_count; }
LightData GetLight(int i) { return g_scene_data.lights[i]; }

float3 HitPosition() { return WorldRayOrigin() + WorldRayDirection() * (RayTCurrent() - ShadowRayOffset()); }

uint InstanceFlags() { return g_instance_data[InstanceID()].instance_flags; }

// a & b must be normalized
float angle_between(float3 a, float3 b) { return acos(clamp(dot(a, b), 0, 1)); }




struct RayPayload
{
    float shadow;
    uint instance_id;     // instance id for first ray
};

RayDesc GetCameraRay(float2 offset = 0.0f)
{
    uint2 screen_idx = DispatchRaysIndex().xy;
    uint2 screen_dim = DispatchRaysDimensions().xy;

    float aspect_ratio = (float)screen_dim.x / (float)screen_dim.y;
    float2 screen_pos = ((float2(screen_idx) + offset + 0.5f) / float2(screen_dim)) * 2.0f - 1.0f;
    screen_pos.x *= aspect_ratio;

    RayDesc ray;
    ray.Origin = CameraPosition();
    ray.Direction = normalize(
        CameraRight() * screen_pos.x +
        CameraUp() * screen_pos.y +
        CameraForward() * CameraFocalLength());
    ray.TMin = CameraNearPlane(); // 
    ray.TMax = CameraFarPlane();  // todo: correct this
    return ray;
}

RayPayload ShootCameraRay(float2 offset = 0.0f)
{
    RayPayload payload;
    payload.shadow = 0.0;

    RayDesc ray = GetCameraRay(offset);
    int render_flags = RenderFlags();
    int ray_flags = RAY_FLAG_FORCE_OPAQUE;
    if (render_flags & RF_CULL_BACK_FACES)
        ray_flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

    TraceRay(g_TLAS, ray_flags, CameraLayerMask(), 0, 0, 0, ray, payload);
    return payload;
}

float SampleDifferential(int2 idx, out float center, out float diff)
{
    int2 dim;
    g_prev_result.GetDimensions(dim.x, dim.y);

    center = g_prev_result[idx].x;
    diff = 0.0f;

    // 4 samples for now. an option for more samples may be needed. it can make an unignorable difference (both quality and speed).
    diff += abs(g_prev_result[clamp(idx + int2(-1, 0), int2(0, 0), dim - 1)].x - center);
    diff += abs(g_prev_result[clamp(idx + int2( 1, 0), int2(0, 0), dim - 1)].x - center);
    diff += abs(g_prev_result[clamp(idx + int2( 0,-1), int2(0, 0), dim - 1)].x - center);
    diff += abs(g_prev_result[clamp(idx + int2( 0, 1), int2(0, 0), dim - 1)].x - center);
    //diff += abs(g_prev_result[clamp(idx + int2(-1,-1), int2(0, 0), dim - 1)].x - center);
    //diff += abs(g_prev_result[clamp(idx + int2( 1,-1), int2(0, 0), dim - 1)].x - center);
    //diff += abs(g_prev_result[clamp(idx + int2( 1, 1), int2(0, 0), dim - 1)].x - center);
    //diff += abs(g_prev_result[clamp(idx + int2(-1, 1), int2(0, 0), dim - 1)].x - center);
    return diff;
}


[shader("raygeneration")]
void RayGenDefault()
{
    uint2 screen_idx = DispatchRaysIndex().xy;
    RayPayload payload = ShootCameraRay();
    g_output[screen_idx] = payload.shadow;
}

[shader("raygeneration")]
void RayGenAdaptiveSampling()
{
    uint2 screen_idx = DispatchRaysIndex().xy;
    uint2 cur_dim = DispatchRaysDimensions().xy;
    uint2 pre_dim; g_prev_result.GetDimensions(pre_dim.x, pre_dim.y);
    int2 pre_idx = (int2)((float2)screen_idx * ((float2)pre_dim / (float2)cur_dim));

    float center, diff;
    SampleDifferential(pre_idx, center, diff);

    if (diff == 0) {
        g_output[screen_idx] = center;
    }
    else {
        RayPayload payload = ShootCameraRay();
        g_output[screen_idx] = payload.shadow;
    }
}

[shader("raygeneration")]
void RayGenAntialiasing()
{
    uint2 idx = DispatchRaysIndex().xy;

    float center, diff;
    SampleDifferential(idx, center, diff);

    if (diff == 0) {
        g_output[idx] = center;
    }
    else {
        float total = center;

        // todo: make offset values shader parameter
        const int N = 4;
        const float d = 0.333f;
        float2 offsets[N] = {
            float2(d, d), float2(-d, d), float2(-d, -d), float2(d, -d)
        };
        for (int i = 0; i < N; ++i)
            total += ShootCameraRay(offsets[i]).shadow;

        g_output[idx] = total / (N + 1);
    }
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
    if (render_flags & RF_CULL_BACK_FACES) {
        if (render_flags & RF_FLIP_CASTER_FACES)
            ray_flags |= RAY_FLAG_CULL_FRONT_FACING_TRIANGLES;
        else
            ray_flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;
    }
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
            TraceRay(g_TLAS, ray_flags, light.layer_mask_gpu, 1, 0, 1, ray, payload);
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
                TraceRay(g_TLAS, ray_flags, light.layer_mask_gpu, 1, 0, 1, ray, payload);
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
                TraceRay(g_TLAS, ray_flags, light.layer_mask_gpu, 1, 0, 1, ray, payload);
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
                TraceRay(g_TLAS, ray_flags, light.layer_mask_gpu, 1, 0, 1, ray, payload);
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
