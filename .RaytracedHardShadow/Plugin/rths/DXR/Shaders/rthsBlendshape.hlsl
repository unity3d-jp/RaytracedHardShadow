#include "rthsCommonDeform.h"

RWStructuredBuffer<float4> g_dst_vertices : register(u0); // src & dst
StructuredBuffer<float3>   g_point_delta : register(t1);
StructuredBuffer<int>      g_point_offsets : register(t2);
StructuredBuffer<float>    g_point_weights : register(t3);
ConstantBuffer<MeshInfo>   g_mesh_info : register(b0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint vid = tid.x;

    float4 result = g_dst_vertices[vid];
    int num_blendshapes = g_mesh_info.num_blendshapes;
    for (int bsi = 0; bsi < num_blendshapes; ++bsi) {
        float weight = g_point_weights[bsi];
        if (weight != 0.0f) {
            result.xyz += g_point_delta[g_point_offsets[bsi] + vid] * weight;
        }
    }
    g_dst_vertices[vid] = float4(result.xyz, 1.0f);
}
