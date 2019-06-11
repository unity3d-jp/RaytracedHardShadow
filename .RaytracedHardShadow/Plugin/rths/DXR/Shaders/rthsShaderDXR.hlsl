#include "rthsCommon.h"

struct RayPayload
{
    float shadow;
    uint pass;
    uint instance_id; // instance id for first ray
};

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
    payload.pass = 0;
    payload.instance_id = 0;

    TraceRay(gRtScene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 0, 0, ray, payload);
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
    if (payload.pass == 1) {
        // ignore self shadow
        if (payload.instance_id == InstanceID()) {
            IgnoreHit();
        }
    }
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload : SV_RayPayload, in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes)
{
    if (payload.pass == 0) {
        payload.pass += 1;
        payload.instance_id = InstanceID();

        int rt_flags = RaytraceFlags();
        int ray_flags = 0;
        if ((rt_flags & RTFLAG_IGNORE_SELF_SHADOW) != 0)
            ray_flags = RAY_FLAG_FORCE_NON_OPAQUE; // calling any hit shader require non-opaque flag
        else
            ray_flags = RAY_FLAG_FORCE_OPAQUE & RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
        ray_flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

        int li;
        for (li = 0; li < LightCount(); ++li) {
            LightData light = GetLight(li);

            if (light.light_type == LIGHT_TYPE_DIRECTIONAL) {
                // directional light
                RayDesc ray;
                ray.Origin = HitPosition();
                ray.Direction = -light.direction.xyz;
                ray.TMin = 0.0f;
                ray.TMax = CameraFarPlane();
                TraceRay(gRtScene, ray_flags, 0xFF, 0, 0, 0, ray, payload);
            }
            else if (light.light_type == LIGHT_TYPE_SPOT) {
                // spot light
                float3 pos = HitPosition();
                float3 dir = normalize(light.position - pos);
                float distance = length(light.position - pos);
                if (angle_between(-dir, light.direction) * 2.0f <= light.spot_angle && distance < light.range) {
                    RayDesc ray;
                    ray.Origin = pos;
                    ray.Direction = dir;
                    ray.TMin = 0.0f;
                    ray.TMax = distance;
                    TraceRay(gRtScene, ray_flags, 0xFF, 0, 0, 0, ray, payload);
                }
            }
            else if (light.light_type == LIGHT_TYPE_POINT) {
                // point light
                float3 pos = HitPosition();
                float3 dir = normalize(light.position - pos);
                float distance = length(light.position - pos);

                if (distance < light.range) {
                    RayDesc ray;
                    ray.Origin = pos;
                    ray.Direction = dir;
                    ray.TMin = 0.0f;
                    ray.TMax = distance;
                    TraceRay(gRtScene, ray_flags, 0xFF, 0, 0, 0, ray, payload);
                }
            }
            else if (light.light_type == LIGHT_TYPE_REVERSE_POINT) {
                // reverse point light
                float3 pos = HitPosition();
                float3 dir = normalize(light.position - pos);
                float distance = length(light.position - pos);

                if (distance < light.range) {
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
