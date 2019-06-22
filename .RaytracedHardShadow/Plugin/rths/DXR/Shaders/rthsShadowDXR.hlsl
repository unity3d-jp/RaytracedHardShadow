#include "rthsCommonDXR.h"

struct RayPayload
{
    float shadow;
    uint pass;
    uint instance_id;     // 
    uint primitive_index; // instance & primitive id for first ray
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
    ray.TMin = CameraNearPlane();
    ray.TMax = CameraFarPlane();

    RayPayload payload;
    payload.shadow = 0.0;
    payload.pass = 0;
    payload.instance_id = 0;

    int render_flags = RenderFlags();
    int ray_flags = RAY_FLAG_FORCE_OPAQUE;
    if (render_flags & RF_CULL_BACK_FACE)
        ray_flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

    TraceRay(gRtScene, ray_flags, 0xFF, 0, 0, 0, ray, payload);
    gOutput[screen_idx.xy] = payload.shadow;
}

[shader("miss")]
void Miss(inout RayPayload payload : SV_RayPayload)
{
    if (payload.pass == 1) {
        payload.shadow += (1.0f / LightCount());
    }
}

[shader("anyhit")]
void AnyHit(inout RayPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    // this function is called only when ignore self shadow flag is enabled (RTFLAG_IGNORE_SELF_SHADOW) and payload.pass==1

    if (payload.instance_id == InstanceID()) {
        int render_flags = RenderFlags();
        if ((render_flags & RF_KEEP_SELF_DROP_SHADOW) == 0 || (payload.primitive_index == PrimitiveIndex() || RayTCurrent() < SelfShadowThreshold())) {
            // ignore self shadow
            IgnoreHit();
            return;
        }
    }
    AcceptHitAndEndSearch();
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    if (payload.pass == 0) {
        payload.pass += 1;
        payload.instance_id = InstanceID();
        payload.primitive_index = PrimitiveIndex();

        // shoot shadow ray (hit position -> light)

        int render_flags = RenderFlags();
        int ray_flags = 0;
        if (render_flags & RF_CULL_BACK_FACE)
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
                TraceRay(gRtScene, ray_flags, 0xFF, 0, 0, 0, ray, payload);
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
                    TraceRay(gRtScene, ray_flags, 0xFF, 0, 0, 0, ray, payload);
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
                    TraceRay(gRtScene, ray_flags, 0xFF, 0, 0, 0, ray, payload);
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
                    TraceRay(gRtScene, ray_flags, 0xFF, 0, 0, 0, ray, payload);
                }
            }
        }
    }
    else {
        // nothing to do for now
    }
}
