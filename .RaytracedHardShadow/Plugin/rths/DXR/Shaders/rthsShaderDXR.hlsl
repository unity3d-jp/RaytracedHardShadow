#include "rthsCommon.h"

struct RayPayload
{
    float shadow;
    int recursion;
    int light_index;
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
        for (li = 0; li < LightCount(); ++li) {
            LightData light = GetLight(li);

            if (light.light_type == LIGHT_TYPE_DIRECTIONAL) {
                // directional light
                RayDesc ray;
                ray.Origin = HitPosition();
                ray.Direction = -light.direction.xyz;
                ray.TMin = 0.0f;
                ray.TMax = CameraFarPlane();
                TraceRay(gRtScene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 0, 0, 0, ray, payload);
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
                    TraceRay(gRtScene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 0, 0, 0, ray, payload);
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
                    TraceRay(gRtScene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 0, 0, 0, ray, payload);
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
                    TraceRay(gRtScene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 0, 0, 0, ray, payload);
                }
            }
            ++payload.light_index;
        }
    }
    else {
         // nothing to do for now
    }
}
