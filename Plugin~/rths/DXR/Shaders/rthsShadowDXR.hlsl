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
    IF_RECEIVE_SHADOWS  = 0x01,
    IF_SHADOWS_ONLY     = 0x02,
    IF_CAST_SHADOWS     = 0x04,
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
    uint layer_count;
    uint light_count;
    float shadow_ray_offset;
    float self_shadow_threshold;
    float3 pad;

    CameraData camera;
    LightData lights[kMaxLights];
};

struct InstanceData
{
    uint instance_flags;
    uint layer_mask;
};


// slot 0
RWTexture2D<float> g_outputf : register(u0);
RWTexture2D<uint> g_outputi : register(u1);

// slot 1
RaytracingAccelerationStructure g_tlas0 : register(t0);
RaytracingAccelerationStructure g_tlas1 : register(t1);
RaytracingAccelerationStructure g_tlas2 : register(t2);
RaytracingAccelerationStructure g_tlas3 : register(t3);
StructuredBuffer<InstanceData> g_instance_data : register(t4);
ConstantBuffer<SceneData> g_scene_data : register(b0);

// slot 2
Texture2D<float> g_prev_resultf : register(t5);
Texture2D<uint> g_prev_resulti : register(t6);


float3 CameraPosition() { return g_scene_data.camera.position.xyz; }
float3 CameraRight()    { return g_scene_data.camera.view[0].xyz; }
float3 CameraUp()       { return g_scene_data.camera.view[1].xyz; }
float3 CameraForward()  { return -g_scene_data.camera.view[2].xyz; }
float CameraFocalLength()   { return abs(g_scene_data.camera.proj[1][1]); }
float CameraNearPlane()     { return g_scene_data.camera.near_plane; }
float CameraFarPlane() { return g_scene_data.camera.far_plane; }
uint CameraLayerMask() { return g_scene_data.camera.layer_mask_gpu; }

uint  RenderFlags()         { return g_scene_data.render_flags; }
float ShadowRayOffset()     { return g_scene_data.shadow_ray_offset; }
float SelfShadowThreshold() { return g_scene_data.self_shadow_threshold; }

uint LayerCount() { return g_scene_data.layer_count; }
int LightCount() { return g_scene_data.light_count; }
LightData GetLight(int i) { return g_scene_data.lights[i]; }

float3 HitPosition() { return WorldRayOrigin() + WorldRayDirection() * (RayTCurrent() - ShadowRayOffset()); }

uint InstanceFlags() { return g_instance_data[InstanceID()].instance_flags; }
uint InstanceLayerMask() { return g_instance_data[InstanceID()].layer_mask; }

// a & b must be normalized
float angle_between(float3 a, float3 b) { return acos(clamp(dot(a, b), 0, 1)); }
uint shift_mask(uint mask, uint shift) { return (mask >> shift) & 0xff; }


struct CameraPayload
{
    float shadow;
    uint light_bits;
    float t;
    uint instance_id; // instance id for first ray
};
struct LightPayload
{
    uint hit;
    uint instance_id; // instance id for first ray
};

void init(inout CameraPayload a)
{
    a.shadow = 0.0;
    a.light_bits = 0;
    a.t = 1e+100;
}
void init(inout LightPayload a, in CameraPayload b)
{
    a.hit = ~0;
    a.instance_id = b.instance_id;
}
void select(inout CameraPayload a, in CameraPayload b)
{
    if (b.t < a.t)
        a = b;
}

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

CameraPayload ShootCameraRay(float2 offset = 0.0f)
{
    CameraPayload payload;
    init(payload);

    RayDesc ray = GetCameraRay(offset);
    uint render_flags = RenderFlags();
    uint ray_flags = 0;
    if (render_flags & RF_CULL_BACK_FACES)
        ray_flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

    uint layer_mask = CameraLayerMask();
    TraceRay(g_tlas0, ray_flags, shift_mask(layer_mask, 0), 0, 0, 0, ray, payload);

    uint layer_count = LayerCount();
    if (layer_count >= 8) {
        CameraPayload tmp;
        init(tmp);
        TraceRay(g_tlas1, ray_flags, shift_mask(layer_mask, 8), 0, 0, 0, ray, tmp);
        select(payload, tmp);

        if (layer_count >= 16) {
            init(tmp);
            TraceRay(g_tlas2, ray_flags, shift_mask(layer_mask, 16), 0, 0, 0, ray, tmp);
            select(payload, tmp);

            if (layer_count >= 24) {
                init(tmp);
                TraceRay(g_tlas3, ray_flags, shift_mask(layer_mask, 24), 0, 0, 0, ray, tmp);
                select(payload, tmp);
            }
        }
    }
    return payload;
}

float SampleDifferential(int2 idx, out float center, out float diff)
{
    int2 dim;
    g_prev_resultf.GetDimensions(dim.x, dim.y);

    center = g_prev_resultf[idx].x;
    diff = 0.0f;

    // 4 samples for now. an option for more samples may be needed. it can make an unignorable difference (both quality and speed).
    diff += abs(g_prev_resultf[clamp(idx + int2(-1, 0), int2(0, 0), dim - 1)].x - center);
    diff += abs(g_prev_resultf[clamp(idx + int2( 1, 0), int2(0, 0), dim - 1)].x - center);
    diff += abs(g_prev_resultf[clamp(idx + int2( 0,-1), int2(0, 0), dim - 1)].x - center);
    diff += abs(g_prev_resultf[clamp(idx + int2( 0, 1), int2(0, 0), dim - 1)].x - center);
    //diff += abs(g_prev_resultf[clamp(idx + int2(-1,-1), int2(0, 0), dim - 1)].x - center);
    //diff += abs(g_prev_resultf[clamp(idx + int2( 1,-1), int2(0, 0), dim - 1)].x - center);
    //diff += abs(g_prev_resultf[clamp(idx + int2( 1, 1), int2(0, 0), dim - 1)].x - center);
    //diff += abs(g_prev_resultf[clamp(idx + int2(-1, 1), int2(0, 0), dim - 1)].x - center);
    return diff;
}


[shader("raygeneration")]
void RayGenDefault()
{
    uint2 screen_idx = DispatchRaysIndex().xy;
    CameraPayload payload = ShootCameraRay();
    g_outputf[screen_idx] = payload.shadow;
}

[shader("raygeneration")]
void RayGenAdaptiveSampling()
{
    uint2 screen_idx = DispatchRaysIndex().xy;
    uint2 cur_dim = DispatchRaysDimensions().xy;
    uint2 pre_dim; g_prev_resultf.GetDimensions(pre_dim.x, pre_dim.y);
    int2 pre_idx = (int2)((float2)screen_idx * ((float2)pre_dim / (float2)cur_dim));

    float center, diff;
    SampleDifferential(pre_idx, center, diff);

    if (diff == 0) {
        g_outputf[screen_idx] = center;
    }
    else {
        CameraPayload payload = ShootCameraRay();
        g_outputf[screen_idx] = payload.shadow;
    }
}

[shader("raygeneration")]
void RayGenAntialiasing()
{
    uint2 idx = DispatchRaysIndex().xy;

    float center, diff;
    SampleDifferential(idx, center, diff);

    if (diff == 0) {
        g_outputf[idx] = center;
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

        g_outputf[idx] = total / (N + 1);
    }
}


[shader("miss")]
void MissCamera(inout CameraPayload payload : SV_RayPayload)
{
    // nothing todo here
}

[shader("anyhit")]
void AnyHitCamera(inout CameraPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    // ignore hit if the object is marked 'shadow only'
    uint instance_flags = InstanceFlags();
    if (instance_flags & IF_SHADOWS_ONLY) {
        IgnoreHit();
        return;
    }
}

bool ShootShadowRay(uint flags, uint mask, in RayDesc ray, inout CameraPayload payload)
{
    LightPayload lp;
    init(lp, payload);
    TraceRay(g_tlas0, flags, shift_mask(mask, 0), 1, 0, 1, ray, lp);
    bool hit = lp.hit;

    uint layer_count = LayerCount();
    if (layer_count >= 8) {
        lp.hit = ~0;
        TraceRay(g_tlas1, flags, shift_mask(mask, 8), 1, 0, 1, ray, lp);
        hit = hit || lp.hit;

        if (layer_count >= 16) {
            lp.hit = ~0;
            TraceRay(g_tlas2, flags, shift_mask(mask, 16), 1, 0, 1, ray, lp);
            hit = hit || lp.hit;

            if (layer_count >= 24) {
                lp.hit = ~0;
                TraceRay(g_tlas3, flags, shift_mask(mask, 24), 1, 0, 1, ray, lp);
                hit = hit || lp.hit;
            }
        }
    }
    return hit;
}

[shader("closesthit")]
void ClosestHitCamera(inout CameraPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    payload.t = RayTCurrent();
    payload.instance_id = InstanceID();

    // shoot shadow ray (hit position -> light)

    uint instance_flags = InstanceFlags();
    uint instance_layer_mask = InstanceLayerMask();
    float ls = 1.0f / LightCount();

    uint render_flags = RenderFlags();
    uint ray_flags = 0;
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
        if ((instance_layer_mask & light.layer_mask_gpu) == 0)
            continue;

        uint mask = (instance_flags & IF_RECEIVE_SHADOWS) == 0 ? 0 :
            light.layer_mask_gpu & CameraLayerMask();
        bool hit = false;

        if (light.light_type == LT_DIRECTIONAL) {
            // directional light
            RayDesc ray;
            ray.Origin = HitPosition();
            ray.Direction = -light.direction.xyz;
            ray.TMin = 0.0f;
            ray.TMax = CameraFarPlane();
            hit = ShootShadowRay(ray_flags, mask, ray, payload);
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
                hit = ShootShadowRay(ray_flags, mask, ray, payload);
            }
            else
                continue;
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
                hit = ShootShadowRay(ray_flags, mask, ray, payload);
            }
            else
                continue;
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
                hit = ShootShadowRay(ray_flags, mask, ray, payload);
            }
            else
                continue;
        }

        if (!hit) {
            payload.light_bits |= 0x1 << li;
            payload.shadow += ls;
        }
    }
}


[shader("miss")]
void MissLight(inout LightPayload payload : SV_RayPayload)
{
    payload.hit = 0;
}

[shader("anyhit")]
void AnyHitLight(inout LightPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    // ignore the object if it doesn't marked cast shadows.
    uint instance_flags = InstanceFlags();
    if ((instance_flags & IF_CAST_SHADOWS) == 0) {
        IgnoreHit();
        return;
    }

    uint render_flags = RenderFlags();
    if (render_flags & RF_IGNORE_SELF_SHADOW) {
        // this condition means:
        // - always ignore zero-distance shadow
        //   (comparing instance ID is not enough because in some cases meshes are separated but seamlessly continuous. e.g. head and body)
        // - always ignore shadow cast by self back faces (relevanet only when 'cull back faces' is disabled)
        // - ignore non-zero-distance self shadow if 'keep self drop shadow' is disabled
        if (RayTCurrent() < SelfShadowThreshold() ||
            (payload.instance_id == InstanceID() && ((RenderFlags() & RF_KEEP_SELF_DROP_SHADOW) == 0 || HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)))
        {
            IgnoreHit();
            return;
        }
    }

    AcceptHitAndEndSearch();
}
